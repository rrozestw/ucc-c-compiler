/*
 * Autotab v003.
 * Program to detect the tabbing style of a text file, and report
 * it as a Vim command to set up the tabstop, shiftwidth and expandtab
 * parameters.
 *
 * Copyright 2012
 * Kaz Kylheku <kaz@kylheku.com>
 * Vancouver, Canada
 *
 * To use this, compile to an executable called "autotab".
 * Then in the .vimrc file, add something like this:
 *
 *   :au BufRead * execute 'set' system("autotab < " . bufname("%"))
 *
 * Or, better still, with this (all joined to one line):
 *
 *   :au BufReadPost * if bufname("%") != "" |
 *     execute 'set' system("autotab < " . bufname("%")) | endif
 */

#ifndef __UCC__
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <stdarg.h>
#  include <stddef.h>
#  include <ctype.h>
#  include <math.h>
#else
#  include <stdarg.h>
#  define EOF -1
typedef unsigned long size_t;
typedef long ptrdiff_t;
typedef struct __FILE FILE;
extern FILE *stderr, *stdout, *stdin;
enum
{
	EXIT_FAILURE = 1
};

extern void *realloc(void *, size_t);
extern void *malloc(size_t);
extern void free(void *);

extern char *strstr(char *, char *);
extern char *strchr(char *, char);
extern int strcmp(char *, char *);
extern int strncmp(char *, char *, size_t);
extern size_t strlen(char *);
extern size_t strspn(char *, char *);
extern size_t strcspn(char *, char *);

extern int printf(char *, ...);
extern int fprintf(FILE *, char *, ...);
extern int vfprintf(FILE *, char *, va_list);

extern int getc(FILE *);

extern long labs(long);

typedef int ch_fn(int);
extern ch_fn isspace, iscntrl, isalnum;

#endif


#define MAX_SNARF_LINES 5000 /* only sample so many lines from the input */

#define MIN(A, B) ((A) < (B) ? (A) : (B))

const char *name = "autotab";

typedef struct line {
  struct line *next;
  char *str;
} line_t;

typedef struct {
  char *base;
  char *next;
  size_t size;
} buffer_t;

int debug_enabled;

static void oops(char *fmt, ...)
{
  va_list vl;
  va_start (vl, fmt);
  fprintf(stderr, "%s: ", name);
  vfprintf(stderr, fmt, vl);
  va_end (vl);
}

static void debug(char *fmt, ...)
{
  if (debug_enabled) {
    va_list vl;
    va_start (vl, fmt);
    fprintf(stdout, "%s: ", name);
    vfprintf(stdout, fmt, vl);
    va_end (vl);
  }
}

static int grow_buffer(buffer_t *buf)
{
  char *base = buf->base, *next = buf->next;
  size_t size = buf->size;
  size_t new_size = (size == 0) ? 512 : size * 2;
  ptrdiff_t delta = base ? next - base : 0;
  char *new_base;

  if (base != 0 && next < base + size)
    return 1;

  if (new_size < size) {
    oops("overflow due to excessively long line\n");
    return 0;
  }

  new_base = realloc(base, new_size);

  if (new_base == 0) {
    oops("out of memory\n");
    return 0;
  }

  buf->base = new_base;
  buf->next = new_base + delta;
  buf->size = new_size;
  return 1;
}

static int add_char(buffer_t *buf, int ch)
{
  if (!grow_buffer(buf))
    return 0;
  *buf->next++ = ch;
  return 1;
}

static void trim_buffer(buffer_t *buf)
{
  char *base = buf->base, *next = buf->next;
  ptrdiff_t delta = base ? next - base : 0;
  char *new_base = delta ? realloc(base, delta) : 0;

  if (new_base != 0) {
    buf->base = new_base;
    buf->next = new_base + delta;
    buf->size = delta;
  }
}

static void discard_buffer(buffer_t *buf)
{
  static buffer_t blank = { 0 };
  free(buf->base);
  *buf = blank;
}

static char *extract_buffer(buffer_t *buf)
{
  static buffer_t blank = { 0 };
  char *str = buf->base;
  *buf = blank;
  return str;
}

size_t buffer_size(buffer_t *buf)
{
  if (buf->base == 0)
    return 0;
  return buf->next - buf->base;
}

static int buffer_blank(buffer_t *buf)
{
  return buf->base == 0;
}

static char *snarf_line(FILE *stream)
{
  buffer_t buf = { 0 };

  for (;;) {
    int ch = getc(stream);

    /* EOF must cause null pointer return,
       not a pointer to an empty string. */
    if (ch == EOF && buffer_blank(&buf))
      break;

    if (ch == EOF || ch == '\n') {
      if (!add_char(&buf, 0))
        goto oops_out;
      break;
    }

    add_char(&buf, ch);
  }

  trim_buffer(&buf);
  return extract_buffer(&buf);

oops_out:
  discard_buffer(&buf);
  return 0;
}

static line_t *push_line(char *str, line_t *lines)
{
  line_t *new_line = malloc(sizeof *new_line);

  if (new_line != 0) {
    new_line->next = lines;
    new_line->str = str;
    return new_line;
  }

  return 0;
}

static line_t *nreverse_lines(line_t *lines)
{
  line_t *new_list = 0, *next;

  while (lines != 0) {
    next = lines->next;
    lines->next = new_list;
    new_list = lines;
    lines = next;
  }

  return new_list;
}


static line_t *snarf_lines(FILE *stream)
{
  line_t *list = 0;
  int i;

  for (i = 0; i < MAX_SNARF_LINES; i++) {
    char *str = snarf_line(stream);
    line_t *new_list;

    if (str == 0)
      break;

    if ((new_list = push_line(str, list)) == 0) {
      oops("out of memory\n");
      free(str);
      break;
    }

    list = new_list;
  }

  return nreverse_lines(list);
}

static void free_lines(line_t *lines)
{
  line_t *next;

  while (lines != 0) {
    next = lines->next;
    free(lines->str);
    free(lines);
    lines = next;
  }
}

static int fgrep(line_t *list, const char *pattern)
{
  while (list != 0 && strstr(list->str, pattern) == 0)
    list = list->next;
  return list != 0;
}

#define SPACE  " "
#define LETAB  "."
#define INTAB  "-"
#define OPGRP  "("
#define CLGRP  ")"
#define TOKEN  "x"
#define PUNCT  "|"

#define ANYSP  SPACE LETAB INTAB
#define NONSP  OPGRP CLGRP TOKEN PUNCT

int munge_char(int ch)
{
  if (isspace(ch) || iscntrl(ch))
    return SPACE[0];
  if (strchr("([{<", ch))
    return OPGRP[0];
  if (strchr(")]}>", ch))
    return CLGRP[0];
  if (isalnum(ch) || ch == '_')
    return TOKEN[0];
  return PUNCT[0];
}

/*
 * This function expands leading tabs into one or more '.' characters
 * representing spaces, inner tabs into '-' characters, punctuation characters
 * into '|', and non-puncutation into 'x'. The resulting data is used
 * as the basis for some pattern matching to determine alignment between
 * lines.
 */
static char *tab_munge_line(const char *str, int tabsize)
{
  buffer_t buf = { 0 };
  int col = 0;
  int leading_tabs = 1;
  size_t after_last_nonspace = 0;
  char *ret;

  if (!grow_buffer(&buf))
    goto oops_out;

  for (;; str++) {
    switch (*str) {
    case 0:
      if (!add_char(&buf, 0))
        goto oops_out;
      break;
    case '\t':
      do {
        if (!add_char(&buf, leading_tabs ? LETAB[0] : INTAB[0]))
          goto oops_out;
      } while ((++col % tabsize) != 0);
      continue;
    default:
      leading_tabs = 0;
      {
        if (!add_char(&buf, munge_char(*str)))
          goto oops_out;
        col++;
        after_last_nonspace = buffer_size(&buf);
      }
      continue;
    }
    break;
  }

  trim_buffer(&buf);
  ret = extract_buffer(&buf);
  ret[after_last_nonspace] = 0;
  return ret;

oops_out:
  discard_buffer(&buf);
  return 0;
}

static line_t *filter_lines(line_t *lines, int arg,
                            char *(*func)(const char *, int))
{
  line_t *list = 0;

  for (; lines != 0; lines = lines->next) {
    char *str = func(lines->str, arg);
    line_t *new_list;

    if (str == 0)
      break;

    if ((new_list = push_line(str, list)) == 0) {
      oops("out of memory\n");
      free(str);
      break;
    }

    list = new_list;
  }

  return nreverse_lines(list);
}

static line_t *tab_munge(line_t *lines, int tabsize)
{
  return filter_lines(lines, tabsize, tab_munge_line);
}

static int smatch(const char *str, const char *bag0, ...)
{
  const char *bag = bag0;
  va_list vl;
  int match = 1;

  va_start (vl, bag0);

  while (bag != 0) {
    if (*str == 0 || strchr(bag, *str) == 0) {
      match = 0;
      break;
    }
    bag = va_arg (vl, const char *);
    str++;
  }

  va_end (vl);

  return match;
}

static long compute_alignment_score(line_t *lines, int tabsize, int shiftwidth)
{
  line_t *next;
  long lineno0, lineno1;
  long score = 0;

#define ALIGN_DBG(STR0_PTR) \
  do { \
    debug("code %s:%d\n", __FILE__, __LINE__); \
    debug("lines: %ld,%ld\n", lineno0, lineno1); \
    debug("line0: %s\n", lines->str); \
    debug("line1: %s\n", next->str); \
    debug("pos:   %*s\n", (int) ((STR0_PTR) - lines->str) + 1, "^"); \
  } while (0)

  /* Loop over pairs of lines. After each iteration, the
     second one of the pair becomes the first line in the next pair.
     The pair of lines are not always consecutive. */
  for (next = lines ? lines->next : 0, lineno0 = 1, lineno1 = lineno0 + 1;
       lines && next;
       lines = next, next = next ? next->next : 0, lineno0 = lineno1++)
  {
    char *str0 = lines->str;
    size_t len0 = strlen(str0);
    long tnd0 = strspn(str0, LETAB);    /* leading space generated by tabs */
    long ind0 = strspn(str0, ANYSP);    /* indentation */
    long int0 = strcspn(str0, INTAB);   /* position of first inner tab */

    if (len0 == ind0) {
      /* First of the two lines is blank, or pure indentation. Next! */
      continue;
    }

    /* This inner loop just bails if it reaches the end of its body,
       but if it is continued, it scans to choose a different second
       line while maintaining the same first line, but not too far!
       The reason for this is that alignment sometimes occurs between
       lines which are not consecutive. There can be blank lines,
       or "out of band" things like preprocessor directives in the
       C language. If we can't find a viable next line within a
       few lines, we skip the whole junk, and continue with a new pair. */
    for (; next && lineno1 - lineno0 <= 4; next = next->next, lineno1++)
    {
      char *str1 = next->str;
      size_t len1 = strlen(str1);
      long tnd1 = strspn(str1, LETAB);
      long ind1 = strspn(str1, ANYSP);
      long int1 = strcspn(str1, INTAB);

      if (len1 == ind1) {
        /* Second line is blank or pure indentation; choose
           the next line to be used as second. */
        continue;
      }

      /* There is usually no alignment when the next line de-indents. */
      if (ind1 < ind0) {
        if (ind1 == 0 || ind0 - ind1 > shiftwidth) {
          /* If the de-indent is by more than a shiftwidth, or goes
             all the way to column zero, choose another second line. */
          continue;
        }
        /* Else skip to next pair. */
        break;
      }

      /* If second line is indented more, but has less tabbed indentation,
         that's probably indicates too small tab size, or odd formatting.
         Better not look for alignment with this pair.*/
      if (tnd1 < tnd0)
        break;

      /* Second line indents beyond first line. There are couple
         of interesting cases here, when the first line opens
         an indented group. */
      if (ind1 >= len0) {
        if (len0 && strchr(OPGRP, str0[len0-1]) &&
            ind1 != tnd1 && ind0 != tnd0) {
          /* First line ends with opening parenthesis, bracket or brace. */
          if (ind1 == len0 && ind0 < len0 - 1) {
            score += 2;
            ALIGN_DBG(str0 + ind1);
          }
          if (ind1 == len0 - 1 + shiftwidth) {
            score += 4;
            ALIGN_DBG(str0 + ind1);
          }
        }
        break;
      }

      if (tnd0 != tnd1) {
        long fsp0 = ind0 + strcspn(str0 + ind0, ANYSP);
        long fit0 = fsp0 + strspn(str0 + fsp0, ANYSP);

        /* If lines differ in leading tabs, then we do leading align check. */
        if (ind0 % shiftwidth == 0 && ind1 == ind0 + shiftwidth) {
          /* An indentation from a shiftwidth-aligned position by
             one shiftwidth contributes a small score. */
          /* ALIGN_DBG(str0 + ind0); */
          score += 1;
        }
        if (ind0 == ind1) {
          /* Small score if lines are indented the same, but
             with different combination of tabs and spaces;
             this sometimes occurs when code is edited by different
             people who agree on the indentation, but disagree
             on the use of hard tabs versus spaces. */
          ALIGN_DBG(str0 + ind1);
          score += 2;
        }
        if (ind0 != ind1 && ind1 != tnd1 &&
            !(ind1 - ind0 <= shiftwidth && ind1 % shiftwidth == 0)) {
          /* If indent of second line is different from first,
             does not consist of all tabs, and is not a multiple of the
             shiftwidth that is within a shiftwidth of the prior line,
             it may be a leading alignment. */
          if (len0 && strchr(OPGRP, str0[len0 - 1])) {
            /* Any indentation in the line following a line which
               ends with group opener is probably just indentation,
               with no alignment. */
            break;
          }
          if (ind1 == fit0) {
            /* Indented line aligned with space-delimited item,
               which is the first such item past the indentation.
               This is weak if it occurs within a shiftwidth. */
            ALIGN_DBG(str0 + fit0);
            score += (ind1 - ind0) > shiftwidth ? 6 : 2;
          }
          if (strchr(OPGRP, str0[ind1 - 1]) && (strchr(NONSP, str0[ind1]))) {
            /* Indented line aligned with nonspace following opening
               bracket or brace. */
            ALIGN_DBG(str0 + ind1);
            score += (ind1 - ind0) > shiftwidth ? 6 : 2;
          }
        }
        /* Now look for inner alignments due to offsets caused by
           leading tabs, or inner tabs. */
        str0 += ind1;
        str1 += ind1;
      } else if (int0 == len0 && int1 == len1) {
        /* If lines do not differ in leading tabs, and have no internal
           tabs, we are done. */
        break;
      } else {
        /* Lines do not differ in leading tabs, but have internal tabs;
           so we can advance to the first discrepancy in inner tabbing,
           and look for alignments after that. */
        str0 += ind1;
        str1 += ind1;

        for (;;) {
          {
            size_t itcsp0 = strcspn(str0, INTAB);
            size_t itcsp1 = strcspn(str1, INTAB);

            str0 += MIN(itcsp0, itcsp1);
            str1 += MIN(itcsp0, itcsp1);

            if (itcsp0 != itcsp1)
              break;
          }

          {
            size_t itsp0 = strspn(str0, INTAB);
            size_t itsp1 = strspn(str1, INTAB);

            str0 += MIN(itsp0, itsp1);
            str1 += MIN(itsp0, itsp1);

            if (itsp0 != itsp1 || itsp0 == 0)
              break;
          }
          score += 2;
          ALIGN_DBG(str0);
        }
      }

      for (; *str0 && *str1; str0++, str1++) {
        if (str0[0] == INTAB[0] && strchr(NONSP, str0[1])) {
          /* Alignment of non-space elements by inner tabs is strong indicator. */
          if (str1[0] == INTAB[0] && strchr(NONSP, str1[1])) {
            ALIGN_DBG(str0);
            score += 4;
            break;
          }
        } else if (!strncmp(str0, INTAB SPACE, 2)) {
          /* Alignment of spaces with leading tab is a weak indicator. */
          if (!strncmp(str1, INTAB SPACE, 2)) {
            ALIGN_DBG(str0);
            score += 2;
            break;
          }
        } else if (smatch(str0, SPACE, SPACE, NONSP, (const char *) 0)) {
          /* Alignment of material preceded by two or more blanks
            (achieved by either tabs or spaces). */
          if (smatch(str1, SPACE INTAB, SPACE INTAB, NONSP, (const char *) 0)) {
            ALIGN_DBG(str0);
            score += 4;
            break;
          }
        }
      }

      break;
    }
  }

  return score;
#undef ALIGN_DBG
}

static int determine_shiftwidth(line_t *lines_in, int tabsize, int munged)
{
  line_t *lines = (munged) ? lines_in : tab_munge(lines_in, tabsize);
  long max_hist = 0, indent_hist[9] = { 0 }, zerocol_hist[9] = { 0 };
  int i, shiftwidth = 0;

  if (lines == 0)
    return 0;

  for (; lines && lines->next; lines = lines->next) {
    char *str0 = lines->str;
    char *str1 = lines->next->str;
    long ind0 = strspn(str0, ANYSP);
    long ind1 = strspn(str1, ANYSP);
    long move = labs(ind1 - ind0);

    /* Empty lines tell us nothing. */
    if (strlen(str0 + ind0) == 0 || strlen(str1 + ind1) == 0)
      continue;

    /* Only a move by an amount which is a divisor of the starting
       position is nesting-level indentation.
       This criterion filters out a lot of indentation which
       exists for alignment with something in the prior line,
       rather than for increased block nesting */
    if (move >= 2 && move <= 8) {
      if (ind0 && ind1) {
        if (ind0 % move == 0)
          indent_hist[move]++;
      } else if (ind0 < ind1) {
        /* Indentation out of column zero tabulated separately.
           Consider only if second line goes beyond previous line,
           or if the indentation does not suspiciously look like alignment. */
        if (strlen(str0) < ind1 ||
            !(strchr(ANYSP, str0[ind1 - 1]) != 0 &&
              strchr(ANYSP, str0[ind1]) == 0))
          zerocol_hist[move]++;
      }
    }
  }

  for (i = 2; i <= 8; i++) {
    debug("hist[%d] = %ld\n", i, indent_hist[i]);
    if (indent_hist[i] >= max_hist) {
      max_hist = indent_hist[i];
      shiftwidth = i;
    }
  }

  /* The indent_history turned up nothing; maybe the file
     only has one level of indentation, with no deeper nesting. */
  if (max_hist == 0) {
    for (i = 2; i <= 8; i++) {
      debug("zerocol[%d] = %ld\n", i, zerocol_hist[i]);
      if (zerocol_hist[i] >= max_hist) {
        max_hist = zerocol_hist[i];
        shiftwidth = i;
      }
    }
  }

  free_lines(munged ? 0 : lines);
  return shiftwidth;
}

static int determine_tabsize(line_t *lines)
{
  int tabsize;
  long best_score = -1;
  int best_tabsize = 0;

  for (tabsize = 2; tabsize <= 8; tabsize++) {
    line_t *lines_retabbed = tab_munge(lines, tabsize);
    long score;
    int shiftwidth;

    if (lines_retabbed == 0)
      return 0;

    shiftwidth = determine_shiftwidth(lines_retabbed, tabsize, 1);

    score = compute_alignment_score(lines_retabbed, tabsize, shiftwidth);

    /* Scores for common tab sizes get a boost; 8 a bit more than 4. */
    switch (tabsize) {
    case 4: score += 1 + score / 16; break;
    case 8: score += 1 + score / 8; break;
    }

    if (score >= best_score) {
      best_tabsize = tabsize;
      best_score = score;
    }

    free_lines(lines_retabbed);
    debug("score[%d] = %ld\n", tabsize, score);
  }

  return best_tabsize;
}

#if 0
static int compute_alignment_histogram(long (*array)[128], line_t *lines)
{
  line_t iter;

  for (iter = lines; iter != 0; iter = iter->next) {
    char *str = iter->str;
    size_t pos = 0;

    for (;;) {
      size_t skipnonblanks = strcspn(str, ANYSP);
      size_t skipblanks = strspn(str, ANYSP);

      pos += skipnonblanks + skipblanks;

      if (str[pos] == 0 || pos >= 128)
        break;

      if (skipblanks >= 2)
        array[pos]++;
    }
  }
}
#endif

int main(int argc, char **argv)
{
  line_t *lines;
  int tabsize = 8, expandtabs = 1, shiftwidth = 8;
  int ret = EXIT_FAILURE;

  if (argc > 1 && !strcmp(argv[1], "-d"))
    debug_enabled = 1;

  if ((lines = snarf_lines(stdin)) == 0)
    goto out_default;

  if (fgrep(lines, "\t")) {
    expandtabs = 0;
    if ((tabsize = determine_tabsize(lines)) == 0)
      goto out;
  }

  if ((shiftwidth = determine_shiftwidth(lines, tabsize, 0)) == 0)
    goto out;

out_default:
  printf("tabstop=%d shiftwidth=%d %sexpandtab\n", tabsize, shiftwidth,
         expandtabs ? "" : "no");
  ret = 0;

out:
  free_lines(lines);
  return ret;
}
