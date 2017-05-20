#ifndef WHERE_H
#define WHERE_H

#include "compiler.h"

struct loc
{
	unsigned line, chr;
};

typedef struct where
{
	const char *fname, *line_str;
	unsigned line;
	unsigned short chr, len;
	unsigned char is_sysh;
	/* space optimisation: move fname + is_sysh to a table of files */
} where;
#define WHERE_INIT(fnam, lstr, n, c) { fnam, lstr, n, c, 0 }

#define WHERE_BUF_SIZ 128
const char *where_str(const struct where *w);
const char *where_str_r(char buf[ucc_static_param WHERE_BUF_SIZ], const struct where *w);

void where_current(where *);

int where_equal(where *, where *);

const where *default_where(const where *w);

#endif
