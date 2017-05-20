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
 * warn: ERANGE, EINVAL
 * err: EILSEQ (e.g. '\')
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
 */
char escape_char_1(
		char *start, char **const end,
		int *const warn, int one_byte_limit)
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
		int *const overflow,
		int applylimit)
	ucc_nonnull();

#define isoct(x) ('0' <= (x) && (x) < '8')

#endif
