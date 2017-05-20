#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include "../util/util.h"
#include "../util/alloc.h"
#include "../util/escape.h"
#include "str.h"
#include "macros.h"
#include "cc1_where.h"
#include "warn.h"

void escape_string(char *old_str, size_t *plen)
{
	char *new_str = umalloc(*plen);
	size_t i, new_i;

	/* "parse" into another string */

	for(i = new_i = 0; i < *plen; i++){
		char add;

		if(old_str[i] == '\\'){
			where loc;
			char *end;
			int warn;

			where_cc1_current(&loc);
			loc.chr += i;

			add = read_char_single(old_str + i, &end, &warn);

			UCC_ASSERT(end, "bad escape?");

			i = (end - old_str) - 1;

			switch(warn){
				case 0:
					break;
				case ERANGE:
					warn_at_print_error(&loc,
							"escape sequence out of range (larger than 0xff)");
					break;
				case EINVAL:
					cc1_warn_at(&loc, escape_char, "invalid escape character");
					break;
			}

		}else{
			add = old_str[i];
		}

		new_str[new_i++] = add;
	}

	memcpy(old_str, new_str, new_i);
	*plen = new_i;
	free(new_str);
}

char *str_add_escape(const char *s, const size_t len)
{
	size_t nlen = 0, i;
	char *new, *p;

	for(i = 0; i < len; i++)
		if(s[i] == '\\' || s[i] == '"')
			nlen += 3;
		else if(!isprint(s[i]))
			nlen += 4;
		else
			nlen++;

	p = new = umalloc(nlen + 1);

	for(i = 0; i < len; i++)
		if(s[i] == '\\' || s[i] == '"'){
			*p++ = '\\';
			*p++ = s[i];
		}else if(!isprint(s[i])){
			/* cast to unsigned char so we don't try printing
			 * some massive sign extended negative number */
			int n = sprintf(p, "\\%03o", (unsigned char)s[i]);
			UCC_ASSERT(n <= 4, "sprintf(octal), length %d > 4", n);
			p += n;
		}else{
			*p++ = s[i];
		}

	return new;
}

int literal_print(FILE *f, const char *s, size_t len)
{
	char *literal = str_add_escape(s, len);
	int r = fprintf(f, "%s", literal);
	free(literal);
	return r;
}
