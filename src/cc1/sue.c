#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../util/alloc.h"
#include "../util/util.h"
#include "../util/dynarray.h"

#include "num.h"
#include "sue.h"
#include "cc1.h"
#include "cc1_where.h"
#include "expr.h"
#include "decl.h"
#include "type_is.h"

static void sue_set_spel(struct_union_enum_st *sue, char *spel)
{
	if(!spel){
		int len = 6 + 6 + 3 + WHERE_BUF_SIZ + 1 + 1;
		spel = umalloc(len);
		snprintf(spel, len, "<anon %s @ %s>",
				sue_str(sue), where_str(&sue->where));
	}

	free(sue->spel);
	sue->spel = spel;
}

void enum_vals_add(
		sue_member ***pmembers,
		where *w,
		char *sp, expr *e,
		attribute *attr)
{
	enum_member *emem = umalloc(sizeof *emem);
	sue_member *mem = umalloc(sizeof *mem);

	if(!e)
		e = (expr *)-1;

	emem->spel = sp;
	emem->val  = e;
	emem->attr = RETAIN(attr);
	memcpy_safe(&emem->where, w);

	mem->enum_member = emem;

	dynarray_add(pmembers, mem);
}

int enum_nentries(struct_union_enum_st *e)
{
	return dynarray_count(e->members);
}

void sue_incomplete_chk(struct_union_enum_st *st, const where *w)
{
	if(!sue_complete(st)){
		char buf[WHERE_BUF_SIZ];

		die_at(w, "%s %s is incomplete\n%s: note: forward declared here",
				sue_str(st), st->spel, where_str_r(buf, &st->where));
	}

	UCC_ASSERT(st->foldprog == SUE_FOLDED_FULLY, "sizeof unfolded sue");
	if(st->primitive == type_enum)
		UCC_ASSERT(st->size > 0, "zero-sized enum");
}

unsigned sue_size(struct_union_enum_st *st, const where *w)
{
	sue_incomplete_chk(st, w);

	return st->size; /* can be zero */
}

unsigned sue_align(struct_union_enum_st *st, const where *w)
{
	sue_incomplete_chk(st, w);

	return st->align;
}

enum sue_szkind sue_sizekind(struct_union_enum_st *sue)
{
	sue_member **mi;

	if(!sue->members)
		return SUE_EMPTY;

	if(sue->primitive == type_enum)
		return SUE_NORMAL;

	for(mi = sue->members; mi && *mi; mi++){
		decl *d = (*mi)->struct_member;
		struct_union_enum_st *subsue;

		if(d->spel) /* not anon-bitfield, must have size */
			return SUE_NORMAL;

		if((subsue = type_is_s_or_u(d->ref))
		&& sue_sizekind(subsue) == SUE_NORMAL)
		{
			return SUE_NORMAL;
		}
	}

	return SUE_NONAMED;
}

struct_union_enum_st *sue_find_this_scope(symtable *stab, const char *spel)
{
	struct_union_enum_st **i;
	for(i = stab->sues; i && *i; i++){
		struct_union_enum_st *st = *i;

		if(st->spel && !strcmp(st->spel, spel))
			return st;
	}
	return NULL;
}

struct_union_enum_st *sue_find_descend(
		symtable *stab, const char *spel, int *descended)
{
	if(descended)
		*descended = 0;

	for(; stab; stab = stab->parent){
		struct_union_enum_st *sue = sue_find_this_scope(stab, spel);
		if(sue)
			return sue;
		if(descended)
			*descended = 1;
	}

	return NULL;
}

static void sue_get_decls(sue_member **mems, sue_member ***pds)
{
	for(; mems && *mems; mems++){
		decl *d = (*mems)->struct_member;

		if(d->spel){
			dynarray_add(pds, *mems);
		}else{
			/* either an anonymous struct/union OR a bitfield */
			struct_union_enum_st *sub = type_is_s_or_u(d->ref);

			if(sub)
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

sue_member *sue_member_from_decl(decl *d)
{
	sue_member *sm = umalloc(sizeof *sm);
	sm->struct_member = d;
	return sm;
}

struct_union_enum_st *sue_decl(
		symtable *stab, char *spel,
		sue_member **members, enum type_primitive prim,
		int got_membs, int is_declaration, int pre_parse, where *w)
{
	struct_union_enum_st *sue;
	int new = 0;
	int descended;

	if(spel && stab && (sue = sue_find_descend(stab, spel, &descended))){
		char wbuf[WHERE_BUF_SIZ];

		/* redef checks */
		if(sue->primitive != prim){
			if(descended)
				goto new_type;
				/* struct A;
				 * f()
				 * {
				 *   union A { ... }; <--- new type
				 * }
				 */

			die_at(NULL, "trying to redefine %s as %s\n"
					"%s: note: from here",
					sue_str(sue),
					type_primitive_to_str(prim),
					where_str_r(wbuf, &sue->where));
		}

		if(got_membs){
			if(descended){
				/* struct A {}; f(){ struct A {}; } */
				goto new_type;
			}

			/* check we don't have two definitions */
			if(sue->got_membs){
				die_at(NULL, "can't redefine %s %s's members\n"
						"%s: note: from here",
						sue_str(sue), sue->spel,
						where_str_r(wbuf, &sue->where));
			}
		}

#if 0
		if(is_complete && !sue->complete){
			/* we've completed a sue - need a new type
			 * with a link back to its forward-decl
			 * otherwise we could get:
			 * struct A;
			 * f(struct A *p){ return p->i; } // BAD
			 * struct A { int i; }; // complete from now on
			 */
			goto new_type;

			note - this would complicate things massively
			instead we just fold functions after we parse them,
			then move on
		}
#endif

		/* struct A;
		 * f()
		 * {
		 *   struct A; <-- new type ONLY IF it's a declaration, i.e.
		 *   struct A a; <-- this alone wouldn't be a new type
		 * }
		 */
		if(is_declaration && descended)
			goto new_type;

	}else{
new_type:
		sue = umalloc(sizeof *sue);
		sue->primitive = prim;

		new = 1;

		if(w)
			memcpy_safe(&sue->where, w);
		else
			where_cc1_current(&sue->where);
	}

	if(members){
		if(prim == type_enum){
			/* enum member duplicate check is done in fold_sym,
			 * same as identifiers
			 */

		}else{
			sue_member **decls = NULL;
			int i;

			sue_get_decls(members, &decls);

			qsort(decls,
					dynarray_count(decls), sizeof *decls,
					decl_spel_cmp);

			for(i = 0; decls && decls[i]; i++){
				decl *d2, *d = decls[i]->struct_member;

				if(d->bits.var.init.dinit)
					die_at(&d->where, "%s member %s is initialised",
							sue_str(sue), d->spel);

				if(decls[i + 1]
				&& (d2 = decls[i + 1]->struct_member,
					!strcmp(d->spel, d2->spel)))
				{
					char buf[WHERE_BUF_SIZ];

					die_at(&d2->where, "duplicate member %s (from %s)",
							d->spel, where_str_r(buf, &d->where));
				}
			}

			dynarray_free(sue_member **, decls, NULL);
		}
	}

	sue->anon = !spel;
	if(got_membs)
		sue->got_membs = 1;
	/* completeness checks done above */

	sue_set_spel(sue, spel);

	if(members){
		UCC_ASSERT(!sue->members,
				"redef of struct/union should've been caught");
		sue->members = members;
	}

	if(new){
		if(!pre_parse && prim == type_enum && !sue->got_membs)
			cc1_warn_at(w, predecl_enum,
					"forward-declaration of enum %s", sue->spel);

		if(stab)
			symtab_add_sue(stab, sue);
	}

	return sue;
}

sue_member *sue_drop(struct_union_enum_st *sue, sue_member **pos)
{
	sue_member *ret = *pos;

	const size_t n = sue_nmembers(sue);
	size_t i = pos - sue->members;

	for(; i < n - 1; i++)
		sue->members[i] = sue->members[i + 1];
	sue->members[i] = NULL;

	return ret;
}

static void *sue_member_find(
		struct_union_enum_st *sue, const char *spel, unsigned *extra_off,
		struct_union_enum_st **pin)
{
	sue_member **mi;

	if(pin)
		*pin = NULL;

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

			}else if((sub = type_is_s_or_u(d->ref))){
				/* C11 anonymous struct/union */
				decl *dsub = NULL;
				decl *tdef;
				const int allow_tag = fopt_mode & FOPT_TAG_ANON_STRUCT_EXT;

				/* don't check spel - <anon struct ...> etc */
				if(!(allow_tag || sub->anon))
					continue;

				if((fopt_mode & FOPT_PLAN9_EXTENSIONS)
				&& (tdef = type_is_tdef(d->ref))
				&& !strcmp(tdef->spel, spel))
				{
					dsub = tdef;
				}

				if(!dsub)
					dsub = sue_member_find(sub, spel, extra_off, pin);

				if(dsub){
					if(pin)
						*pin = sub;
					*extra_off += d->bits.var.struct_offset;
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
				enum_member *emem = sue_member_find(e, spel,
						NULL /* safe - is enum */, NULL);

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

int enum_has_value(struct_union_enum_st *en, integral_t val)
{
	sue_member **i;

	UCC_ASSERT(en->primitive == type_enum, "enum");

	for(i = en->members; i && *i; i++){
		enum_member *ent = (*i)->enum_member;
		integral_t ent_i = const_fold_val_i(ent->val);

		if(val == ent_i)
			return 1;
	}
	return 0;
}

decl *struct_union_member_find_sue(struct_union_enum_st *in, struct_union_enum_st *needle)
{
	sue_member **i;

	UCC_ASSERT(in->primitive != type_enum, "enum");

	for(i = in->members; i && *i; i++){
		decl *d = (*i)->struct_member;
		struct_union_enum_st *s = type_is_s_or_u(d->ref);

		if(s == needle)
			return d;
	}

	return NULL;
}

decl *struct_union_member_find(
		struct_union_enum_st *sue,
		const char *spel, unsigned *extra_off,
		struct_union_enum_st **pin)
{
	return sue_member_find(sue, spel, extra_off, pin);
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
