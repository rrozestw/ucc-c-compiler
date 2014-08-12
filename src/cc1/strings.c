#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "strings.h"

#include "../util/dynmap.h"
#include "../util/alloc.h"

#include "out/lbl.h"

struct string_key
{
	char *str;
	int is_wide;
};

static int strings_key_eq(void *a, void *b)
{
	const struct string_key *ka = a, *kb = b;

	if(ka->is_wide != kb->is_wide)
		return 1;
	return strcmp(ka->str, kb->str);
}

static unsigned strings_hash(const void *p)
{
	const struct string_key *k = p;

	return dynmap_strhash(k->str);
}

stringlit *strings_lookup(
		dynmap **plit_tbl, char *s, size_t len, int wide)
{
	stringlit *lit;
	dynmap *lit_tbl;
	struct string_key key = { s, wide };

	if(!*plit_tbl)
		*plit_tbl = dynmap_new(strings_key_eq, strings_hash);
	lit_tbl = *plit_tbl;

	lit = dynmap_get(struct string_key *, stringlit *, lit_tbl, &key);

	if(!lit){
		struct string_key *alloc_key;
		stringlit *prev;

		lit = umalloc(sizeof *lit);
		lit->str = s;
		lit->len = len;
		lit->wide = wide;
		/* create the label immediately - used in const folding */
		lit->lbl = out_label_data_store(wide ? STORE_P_WCHAR : STORE_P_CHAR);

		alloc_key = umalloc(sizeof *alloc_key);
		*alloc_key = key;
		prev = dynmap_set(struct string_key *, stringlit *, lit_tbl, alloc_key, lit);
		assert(!prev);
	}

	return lit;
}

void stringlit_use(stringlit *s)
{
	s->use_cnt++;
}

int stringlit_empty(const stringlit *str)
{
	switch(str->len){
		case 0:
			return 1;
		case 1:
			return *str->str == '\0';
	}
	return 0;
}
