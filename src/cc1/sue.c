#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../util/alloc.h"
#include "../util/util.h"
#include "../util/dynarray.h"
#include "../util/printu.h"
#include "data_structs.h"
#include "sue.h"
#include "cc1.h"

static void sue_set_spel(struct_union_enum_st *sue, char *spel)
{
	if(!spel){
		int len = 6 + 6 + 3 + WHERE_BUF_SIZ + 1 + 1;
		spel = umalloc(len);
		snprintu(spel, len, "<anon %s @ %W>",
				sue_str(sue), &sue->where);
	}

	free(sue->spel);
	sue->spel = spel;
}

void enum_vals_add(sue_member ***pmembers, char *sp, expr *e)
{
	enum_member *emem = umalloc(sizeof *emem);
	sue_member *mem = umalloc(sizeof *mem);

	if(!e)
		e = (expr *)-1;

	emem->spel = sp;
	emem->val  = e;

	mem->enum_member = emem;

	dynarray_add((void ***)pmembers, mem);
}

int enum_nentries(struct_union_enum_st *e)
{
	int n = 0;
	sue_member **i;
	for(i = e->members; *i; i++, n++);
	return n;
}

int sue_size(struct_union_enum_st *st, const where *w)
{
	if(sue_incomplete(st))
		DIE_AT(w, "%s %s is incomplete", sue_str(st), st->spel);

	if(st->size)
		return st->size;

	if(st->primitive == type_enum)
		return st->size = type_primitive_size(type_int);

	ICE("%s of unfolded sue", __func__);
	return -1;
}

struct_union_enum_st *sue_find(symtable *stab, const char *spel)
{
	for(; stab; stab = stab->parent){
		struct_union_enum_st **i;

		for(i = stab->sues; i && *i; i++){
			struct_union_enum_st *st = *i;

			if(st->spel && !strcmp(st->spel, spel))
				return st;
		}
	}
	return NULL;
}

static void sue_get_decls(sue_member **mems, sue_member ***pds)
{
	for(; *mems; mems++){
		decl *d = (*mems)->struct_member;

		if(d->spel){
			dynarray_add((void ***)pds, *mems);
		}else{
			struct_union_enum_st *sub = type_ref_is_s_or_u(d->ref);

			sue_get_decls(sub->members, pds);
		}
	}
}

static int decl_spel_cmp(const void *pa, const void *pb)
{
	const sue_member *a = *(sue_member *const *)pa,
	                 *b = *(sue_member *const *)pb;

	return strcmp(a->struct_member->spel, b->struct_member->spel);
}

struct_union_enum_st *sue_add(symtable *const stab, char *spel, sue_member **members, enum type_primitive prim)
{
	struct_union_enum_st *sue;
	int new = 0;

	if(spel && (sue = sue_find(stab, spel))){
		/* redef checks */
		if(sue->primitive != prim)
			DIE_AT(NULL, "trying to redefine %s as %s (from %W)",
					sue_str(sue),
					type_primitive_to_str(prim),
					&sue->where);

		if(members && !sue_incomplete(sue))
			DIE_AT(NULL, "can't redefine %s %s's members (defined at %W)",
					sue_str(sue), sue->spel, &sue->where);

	}else{
		sue = umalloc(sizeof *sue);
		sue->primitive = prim;

		new = 1;

		where_new(&sue->where);
	}

	if(members){
		if(prim == type_enum){
			/* check for duplicates */
			int i;

			for(i = 0; members[i]; i++){
				const char *const spel = members[i]->enum_member->spel;

				struct_union_enum_st *e_sue;
				enum_member *e_mem;

				enum_member_search(&e_mem, &e_sue, stab, spel);

				if(e_mem)
					DIE_AT(NULL, "redeclaration of enumerator %s\n%W: from here", spel, &e_sue->where);
			}

		}else{
			sue_member **decls = NULL;
			int i;

			sue_get_decls(members, &decls);

			qsort(decls,
					dynarray_count((void **)decls), sizeof *decls,
					decl_spel_cmp);

			for(i = 0; decls[i]; i++){
				decl *d2, *d = decls[i]->struct_member;

				if(d->init)
					DIE_AT(&d->where, "%s member %s is initialised",
							sue_str(sue), d->spel);

				if(decls[i + 1]
				&& (d2 = decls[i + 1]->struct_member,
					!strcmp(d->spel, d2->spel)))
				{
					DIE_AT(&d2->where, "duplicate member %s\n%W: from here",
							d->spel, &d->where);
				}
			}

			dynarray_free((void ***)&decls, NULL);
		}
	}

	sue->anon = !spel;

	sue_set_spel(sue, spel);

	if(members){
		UCC_ASSERT(!sue->members, "redef of struct/union should've been caught");
		sue->members = members;
	}

	if(new)
		dynarray_add((void ***)&stab->sues, sue);

	return sue;
}

static void *sue_member_find(struct_union_enum_st *sue, const char *spel, unsigned *extra_off)
{
	sue_member **mi;

	for(mi = sue->members; mi && *mi; mi++){
		if(sue->primitive == type_enum){
			enum_member *em = (*mi)->enum_member;

			if(!strcmp(spel, em->spel))
				return em;

		}else{
			struct_union_enum_st *sub;
			decl *d = (*mi)->struct_member;
			char *sp = d->spel;

			if(sp){
				if(!strcmp(sp, spel))
					return d;

			}else if((sub = type_ref_is_s_or_u(d->ref))){
				/* C11 anonymous struct/union */
				decl *dsub = NULL;
				decl *tdef;
				const int allow_tag = fopt_mode & FOPT_TAG_ANON_STRUCT_EXT;

				/* don't check spel - <anon struct ...> etc */
				if(!(allow_tag || sub->anon))
					continue;

				if((fopt_mode & FOPT_PLAN9_EXTENSIONS)
				&& (tdef = type_ref_is_tdef(d->ref))
				&& !strcmp(tdef->spel, spel))
				{
					dsub = tdef;
				}

				if(!dsub)
					dsub = sue_member_find(sub, spel, extra_off);

				if(dsub){
					*extra_off += d->struct_offset;
					return dsub;
				}
			}
		}
	}

	return NULL;
}


void enum_member_search(enum_member **pm, struct_union_enum_st **psue, symtable *stab, const char *spel)
{
	for(; stab; stab = stab->parent){
		struct_union_enum_st **i;

		for(i = stab->sues; i && *i; i++){
			struct_union_enum_st *e = *i;

			if(e->primitive == type_enum){
				enum_member *emem = sue_member_find(e, spel, NULL /* safe - is enum */);

				if(emem){
					*pm = emem;
					*psue = e;
					return;
				}
			}
		}
	}

	*pm = NULL;
	*psue = NULL;
}

decl *struct_union_member_find_sue(struct_union_enum_st *in, struct_union_enum_st *needle)
{
	sue_member **i;

	UCC_ASSERT(in->primitive != type_enum, "enum");

	for(i = in->members; i && *i; i++){
		decl *d = (*i)->struct_member;
		struct_union_enum_st *s = type_ref_is_s_or_u(d->ref);

		if(s == needle)
			return d;
	}

	return NULL;
}

decl *struct_union_member_find(struct_union_enum_st *sue, const char *spel, unsigned *extra_off)
{
	return sue_member_find(sue, spel, extra_off);
}

decl *struct_union_member_at_idx(struct_union_enum_st *sue, int idx)
{
	int n = sue_nmembers(sue);
	/*
	 * not needed since there are checks in decl init code,
	 * but in case this is used elsewhere...
	 */
	if(idx >= n)
		return NULL;

	return sue->members[idx]->struct_member;
}

int struct_union_member_idx(struct_union_enum_st *sue, decl *member)
{
	int i;
	for(i = 0; sue->members[i]; i++)
		if(sue->members[i]->struct_member == member)
			return i;
	return -1;
}
