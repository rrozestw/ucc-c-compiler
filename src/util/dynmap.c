#include <stdlib.h>
#include <stdarg.h>

#include "dynmap.h"
#include "alloc.h"

typedef struct pair pair;

struct dynmap
{
	dynmap_cmp_f *cmp;
	struct pair
	{
		void *key, *value;
		pair *next;
	} *pairs, *last_pair;
	/* last_pair keeps the keys in the order they are added */
};

dynmap *
dynmap_new(dynmap_cmp_f cmp)
{
	dynmap *m = umalloc(sizeof *m);
	m->cmp = cmp;
	return m;
}

void
dynmap_free(dynmap *map)
{
	pair *p, *q;

	for(p = map->pairs; p; q = p->next, free(p), p = q);

	free(map);
}

static pair *
dynmap_nochk_pair(dynmap *map, void *key)
{
	pair *i;

	for(i = map->pairs; i; i = i->next)
		if(!map->cmp(i->key, key))
			return i;

	return NULL;
}

void *
dynmap_nochk_get(dynmap *map, void *key)
{
	pair *i;

	i = dynmap_nochk_pair(map, key);
	if(i)
		return i->value;

	return NULL;
}

void
dynmap_nochk_set(dynmap *map, void *key, void *val)
{
	pair *p = dynmap_nochk_pair(map, key);

	if(p){
		p->value = val;
	}else{
		p = umalloc(sizeof *p);
		p->key   = key;
		p->value = val;

		if(map->last_pair)
			map->last_pair->next = p;
		else
			map->pairs = p;

		map->last_pair = p;
	}
}

static pair *
dynmap_nochk_idx(dynmap *map, int i)
{
	pair *p;

	for(p = map->pairs; p && i > 0; p = p->next, i--);

	return p;
}

void *
dynmap_nochk_key(dynmap *map, int i)
{
	pair *p = dynmap_nochk_idx(map, i);
	return p ? p->key : NULL;
}

void *
dynmap_nochk_value(dynmap *map, int i)
{
	pair *p = dynmap_nochk_idx(map, i);
	return p ? p->value : NULL;
}
