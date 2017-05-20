#ifndef ESCAPE_H
#define ESCAPE_H

#include "compiler.h"

enum base
{
	BIN, OCT, DEC, HEX
};

const char *base_to_str(enum base);

/* convert a (wide?) character to an int
 * is_wide dictates whether
 *   'abc' -> (('a' * 256) + 'b') * 256 + 'c'
 * or
 *   L'abc' -> L'c'
 *
 * warn: EINVAL ('\z'), E2BIG ('abcde')
 * err: EILSEQ (e.g. '\'), ERANGE ('\x100')
 *
 * *warn and *err must be initialised by the caller
 */
int escape_char(
		char *start,
		/*nullable*/char *limit,
		char **end,
		int is_wide,
		/*nullable*/int *multi,
		int *warn,
		int *err)
	ucc_nonnull((1, 3, 6, 7));

/* convert a single character-as-string to 'char'
 * e.g.
 * "\xffg" -> 255, end="g"
 *
 * is_wide dictates whether '\x100' overflows
 * (if not, wchar_t max is checked)
 *
 * *warn and *err must be initialised by the caller
 */
int escape_char_1(
		char *start, char **const end,
		int is_wide,
		int *const warn,
		int *const err)
	ucc_nonnull();

/*
 * Used for parsing integers, given a base
 * e.g.
 * 0x5315156813,
 * '\xff'
 * '\321'
 * "\1234" <-- applylimit prevents over-reading
 * '\1234' <-- !applylimit => error/overflow
 *
 * *eptr = NULL, on error */
unsigned long long char_seq_to_ullong(
		char *s,
		char **eptr,
		enum base mode,
		int *const overflow)
	ucc_nonnull();

#define isoct(x) ('0' <= (x) && (x) < '8')

#endif
