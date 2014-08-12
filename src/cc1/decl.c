#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../util/util.h"
#include "../util/alloc.h"
#include "../util/platform.h"
#include "../util/dynarray.h"

#include "cc1_where.h"

#include "macros.h"
#include "sue.h"
#include "const.h"
#include "cc1.h"
#include "fold.h"
#include "funcargs.h"
#include "defs.h"

#include "type_is.h"

decl *decl_new_w(const where *w)
{
	decl *d = umalloc(sizeof *d);
	memcpy_safe(&d->where, w);
	return d;
}

decl *decl_new()
{
	where wtmp;
	where_cc1_current(&wtmp);
	return decl_new_w(&wtmp);
}

decl *decl_new_ty_sp(type *ty, char *sp)
{
	decl *d = decl_new();
	d->ref = ty;
	d->spel = sp;
	return d;
}

void decl_replace_with(decl *to, decl *from)
{
	/* XXX: memleak of .ref */
	memcpy_safe(&to->where, &from->where);
	to->ref      = from->ref;
	to->attr = RETAIN(from->attr);
	to->spel_asm = from->spel_asm;
	/* no point copying bitfield stuff */
	memcpy_safe(&to->bits, &from->bits);
}

const char *decl_asm_spel(decl *d)
{
	if(!d->spel_asm){
		/* apply underscore prefixes, name mangling, etc */
		type *rf = type_is(d->ref, type_func);
		char *pre, suff[8];

		pre = fopt_mode & FOPT_LEADING_UNDERSCORE ? "_" : "";
		*suff = '\0';

		if(rf){
			funcargs *fa = type_funcargs(rf);

			switch(fa->conv){
				case conv_fastcall:
					pre = "@";

				case conv_stdcall:
					snprintf(suff, sizeof suff,
							"@%d",
							dynarray_count(fa->arglist) * platform_word_size());

				case conv_x64_sysv:
				case conv_x64_ms:
				case conv_cdecl:
					break;
			}
		}

		if(*pre || *suff)
			d->spel_asm = ustrprintf(
					"%s%s%s", pre, d->spel, suff);


		if(!d->spel_asm)
			d->spel_asm = d->spel;
	}

	return d->spel_asm;
}

void decl_free(decl *d)
{
	if(!d)
		return;

	/* expr_free(d->field_width); XXX: leak */

	free(d);
}

const char *decl_store_to_str(const enum decl_storage s)
{
	static char buf[16]; /* "inline register" is the longest - just a fit */

	if(s & STORE_MASK_EXTRA){
		*buf = '\0';

		if((s & STORE_MASK_EXTRA) == store_inline)
			strcpy(buf, "inline ");

		strcpy(buf + strlen(buf), decl_store_to_str(s & STORE_MASK_STORE));
		return buf;
	}

	switch(s){
		case store_inline:
			ICE("inline");
		case store_default:
			return "";
		CASE_STR_PREFIX(store, auto);
		CASE_STR_PREFIX(store, static);
		CASE_STR_PREFIX(store, extern);
		CASE_STR_PREFIX(store, register);
		CASE_STR_PREFIX(store, typedef);
	}
	return NULL;
}

unsigned decl_size(decl *d)
{
	if(type_is_void(d->ref))
		die_at(&d->where, "%s is void", d->spel);

	if(!type_is(d->ref, type_func) && d->bits.var.field_width)
		die_at(&d->where, "can't take size of a bitfield");

	return type_size(d->ref, &d->where);
}

unsigned decl_align(decl *d)
{
	unsigned al = 0;

	if(!type_is(d->ref, type_func) && d->bits.var.align)
		al = d->bits.var.align->resolved;

	return al ? al : type_align(d->ref, &d->where);
}

enum type_cmp decl_cmp(decl *a, decl *b, enum type_cmp_opts opts)
{
	enum type_cmp cmp = type_cmp(a->ref, b->ref, opts);
	enum decl_storage sa = a->store & STORE_MASK_STORE,
	                  sb = b->store & STORE_MASK_STORE;

	if(cmp & TYPE_EQUAL_ANY && sa != sb){
		/* types are equal but there's a store mismatch
		 * only return convertible if it's a typedef or static mismatch
		 */
#define STORE_INCOMPAT(st) ((st) == store_typedef || (st) == store_static)

		if(STORE_INCOMPAT(sa) || STORE_INCOMPAT(sb))
			return TYPE_CONVERTIBLE_IMPLICIT;
	}

	return cmp;
}

int decl_conv_array_func_to_ptr(decl *d)
{
	type *old = d->ref;

	d->ref = type_decay(d->ref);

	return old != d->ref;
}

type *decl_is_decayed_array(decl *d)
{
	return type_is_decayed_array(d->ref);
}

int decl_store_static_or_extern(enum decl_storage s)
{
	switch((enum decl_storage)(s & STORE_MASK_STORE)){
		case store_static:
		case store_extern:
			return 1;
		default:
			return 0;
	}
}

enum linkage decl_linkage(decl *d)
{
	/* global scoped or extern */
	switch((enum decl_storage)(d->store & STORE_MASK_STORE)){
		case store_extern: return linkage_external;
		case store_static: return linkage_internal;

		case store_register:
		case store_auto:
		case store_typedef:
			return linkage_none;

		case store_inline:
			ICE("bad store");

		case store_default:
			break;
	}

	/* either global non-static or local */
	return d->sym && d->sym->type == sym_global
		? linkage_external
		: linkage_none;
}

int decl_store_duration_is_static(decl *d)
{
	return decl_store_static_or_extern(d->store)
		|| (d->sym && d->sym->type == sym_global);
}

const char *decl_to_str_r(char buf[DECL_STATIC_BUFSIZ], decl *d)
{
	char *bufp = buf;

	if(d->store)
		bufp += snprintf(bufp, DECL_STATIC_BUFSIZ, "%s ", decl_store_to_str(d->store));

	type_to_str_r_spel(bufp, d->ref, d->spel);

	return buf;
}

const char *decl_to_str(decl *d)
{
	static char buf[DECL_STATIC_BUFSIZ];
	return decl_to_str_r(buf, d);
}
