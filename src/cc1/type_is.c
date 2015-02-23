#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "../util/where.h"
#include "../util/util.h"
#include "../util/platform.h"
#include "../util/dynarray.h"

#include "expr.h"
#include "sue.h"
#include "type.h"
#include "type_nav.h"
#include "decl.h"
#include "const.h"
#include "funcargs.h"

#include "type_is.h"

type *type_next_1(type *r)
{
	if(r->type == type_tdef){
		/* typedef - jump to its typeof */
		struct type_tdef *tdef = &r->bits.tdef;
		decl *preferred = tdef->decl;

		r = preferred ? preferred->ref : tdef->type_of->tree_type;

		UCC_ASSERT(r, "unfolded typeof()");

		return r;
	}

	return r->ref;
}

enum type_skippage
{
	STOP_AT_TDEF = 1 << 0,
	STOP_AT_CAST = 1 << 1,
	STOP_AT_QUAL_CASTS = 1 << 2,
	STOP_AT_ATTR = 1 << 3,
	STOP_AT_WHERE = 1 << 4,
};
static type *type_skip(type *t, enum type_skippage skippage)
{
	while(t){
		switch(t->type){
			case type_tdef:
				if(skippage & STOP_AT_TDEF)
					goto fin;
				break;
			case type_cast:
				if(skippage & STOP_AT_CAST)
					goto fin;
				if(skippage & STOP_AT_QUAL_CASTS)
					goto fin;
				break;
			case type_attr:
				if(skippage & STOP_AT_ATTR)
					goto fin;
				break;
			case type_where:
				if(skippage & STOP_AT_WHERE)
					goto fin;
				break;
			default:
				goto fin;
		}
		t = type_next_1(t);
	}

fin:
	return t;
}

type *type_skip_all(type *t)
{
	return type_skip(t, 0);
}

type *type_skip_non_tdefs(type *t)
{
	return type_skip(t, STOP_AT_TDEF);
}

type *type_skip_non_casts(type *t)
{
	return type_skip(t, STOP_AT_CAST);
}

type *type_skip_wheres(type *t)
{
	return type_skip(t, ~0 & ~STOP_AT_WHERE);
}

type *type_skip_tdefs(type *t)
{
	return type_skip(t, ~STOP_AT_TDEF & ~STOP_AT_WHERE & ~STOP_AT_ATTR);
}

type *type_skip_non_tdefs_consts(type *t)
{
	return type_skip(t, STOP_AT_TDEF | STOP_AT_QUAL_CASTS);
}

type *type_skip_non_wheres(type *t)
{
	return type_skip(t, STOP_AT_WHERE);
}

type *type_skip_non_attr(type *t)
{
	return type_skip(t, STOP_AT_ATTR);
}

decl *type_is_tdef(type *t)
{
	t = type_skip_non_tdefs(t);

	if(t && t->type == type_tdef)
		return t->bits.tdef.decl;

	return NULL;
}

type *type_next(type *r)
{
	if(!r)
		return NULL;

	switch(r->type){
		case type_auto:
			ICE("__auto_type");

		case type_btype:
			return NULL;

		case type_tdef:
		case type_cast:
		case type_attr:
		case type_where:
			return type_next(type_skip_all(r));

		case type_ptr:
		case type_block:
		case type_func:
		case type_array:
			return r->ref;
	}

	ucc_unreach(NULL);
}

type *type_is(type *r, enum type_kind t)
{
	r = type_skip_all(r);

	if(!r || r->type != t)
		return NULL;

	return r;
}

type *type_is_primitive(type *r, enum type_primitive p)
{
	r = type_is(r, type_btype);

	/* extra checks for a type */
	if(r && (p == type_unknown || r->bits.type->primitive == p))
		return r;

	return NULL;
}

type *type_is_primitive_anysign(type *ty, enum type_primitive p)
{
	enum type_primitive a, b;

	ty = type_is(ty, type_btype);

	if(!ty)
		return NULL;

	if(p == type_unknown)
		return ty;

	a = p;
	b = ty->bits.type->primitive;

	if(TYPE_PRIMITIVE_IS_CHAR(a))
		a = type_nchar;
	else
		a = type_primitive_is_signed(a, 0) ? TYPE_PRIMITIVE_TO_UNSIGNED(a) : a;

	if(TYPE_PRIMITIVE_IS_CHAR(b))
		b = type_nchar;
	else
		b = type_primitive_is_signed(b, 0) ? TYPE_PRIMITIVE_TO_UNSIGNED(b) : b;

	return a == b ? ty : NULL;
}

type *type_is_ptr(type *r)
{
	r = type_is(r, type_ptr);
	return r ? r->ref : NULL;
}

type *type_is_ptr_or_block(type *r)
{
	type *t = type_is_ptr(r);
	if(t)
		return t;

	r = type_is(r, type_block);
	if(r)
		return type_next(r);
	return NULL;
}

type *type_is_array(type *r)
{
	r = type_is(r, type_array);
	return r ? r->ref : NULL;
}

type *type_is_scalar(type *r)
{
	if(type_is_s_or_u(r) || type_is_array(r))
		return NULL;
	return r;
}

type *type_is_func_or_block(type *r)
{
	type *t = type_is(r, type_func);
	if(t)
		return t;

	t = type_is(r, type_block);
	if(t){
		t = type_skip_all(type_next(t));
		UCC_ASSERT(t->type == type_func,
				"block->next not func?");
		return t;
	}

	return NULL;
}

const btype *type_get_type(type *t)
{
	t = type_skip_all(t);
	return t && t->type == type_btype ? t->bits.type : NULL;
}

enum type_primitive type_get_primitive(type *t)
{
	const btype *bt = type_get_type(t);

	return bt ? bt->primitive : type_unknown;
}

int type_is_bool(type *r)
{
	if(type_is(r, type_ptr))
		return 1;

	r = type_is(r, type_btype);

	if(!r)
		return 0;

	return type_is_integral(r);
}

int type_is_fptr(type *r)
{
	return !!type_is(type_is_ptr(r), type_func);
}

int type_is_nonfptr(type *r)
{
	if((r = type_is_ptr(r)))
		return !type_is(r, type_func);

	return 0; /* not a ptr */
}

int type_is_void_ptr(type *r)
{
	return !!type_is_primitive(type_is_ptr(r), type_void);
}

int type_is_nonvoid_ptr(type *r)
{
	if((r = type_is_ptr(r)))
		return !type_is_primitive(r, type_void);
	return 0;
}

int type_is_integral(type *r)
{
	r = type_is(r, type_btype);

	if(!r)
		return 0;

	switch(r->bits.type->primitive){
		case type_int:   case type_uint:
		case type_nchar: case type_schar: case type_uchar:
		case type__Bool:
		case type_short: case type_ushort:
		case type_long:  case type_ulong:
		case type_llong: case type_ullong:
		case type___int128: case type_u__int128:
		case type_enum:
			return 1;

		case type_unknown:
		case type_void:
		case type_struct:
		case type_union:
		case type_float:
		case type_double:
		case type_ldouble:
			break;
	}

	return 0;
}

int type_is_arith(type *t)
{
	t = type_is(t, type_btype);
	if(!t)
		return 0;
	return type_is_integral(t) || type_is_floating(t);
}

int type_is_complete(type *r)
{
	/* decl is "void" or incomplete-struct or array[] */
	r = type_skip_all(r);

	switch(r->type){
		case type_auto:
			ICE("__auto_type");

		case type_btype:
		{
			const btype *t = r->bits.type;

			switch(t->primitive){
				case type_void:
					return 0;
				case type_struct:
				case type_union:
				case type_enum:
					return sue_complete(t->sue);

				default:break;
			}

			break;
		}

		case type_array:
			return (r->bits.array.is_vla || r->bits.array.size)
				&& type_is_complete(r->ref);

		case type_func:
		case type_ptr:
		case type_block:
			break;

		case type_tdef:
		case type_attr:
		case type_cast:
		case type_where:
			ICE("should've been skipped");
	}


	return 1;
}

type *type_is_vla(type *ty, enum vla_kind kind)
{
	for(ty = type_is(ty, type_array);
	    ty;
	    ty = ty->ref)
	{
		if(ty->bits.array.is_vla)
			return ty;

		if(kind == VLA_TOP_DIMENSION)
			break;
	}

	return NULL;
}

int type_is_variably_modified_vla(type *const ty, int *vla)
{
	type *ti;

	if(vla)
		*vla = 0;

	/* need to check all the way down to the btype */
	for(ti = ty; ti; ti = type_next(ti)){
		type *test = type_is(ti, type_array);

		if(test && test->bits.array.is_vla){
			if(vla && ti == ty)
				*vla = 1;
			return 1;
		}
	}

	return 0;
}

int type_is_variably_modified(type *ty)
{
	return type_is_variably_modified_vla(ty, NULL);
}

int type_is_incomplete_array(type *r)
{
	if((r = type_is(r, type_array)))
		return !r->bits.array.size;

	return 0;
}

type *type_complete_array(type *r, expr *sz)
{
	r = type_is(r, type_array);

	UCC_ASSERT(r, "not an array");

	r = type_array_of(r->ref, sz);

	return r;
}

struct_union_enum_st *type_is_s_or_u_or_e(type *r)
{
	type *test = type_is(r, type_btype);

	if(!test)
		return NULL;

	/* see comment in type_is_enum() */
	return test->bits.type->sue; /* NULL if not s/u/e */
}

struct_union_enum_st *type_is_enum(type *t)
{
	/* this depends heavily on type_is_s_or_u_or_e returning regardless of .primitive */
	struct_union_enum_st *sue = type_is_s_or_u_or_e(t);
	return sue && sue->primitive == type_enum ? sue : NULL;
}

struct_union_enum_st *type_is_s_or_u(type *r)
{
	struct_union_enum_st *sue = type_is_s_or_u_or_e(r);
	if(sue && sue->primitive != type_enum)
		return sue;
	return NULL;
}

type *type_func_call(type *fp, funcargs **pfuncargs)
{
	fp = type_skip_all(fp);
	switch(fp->type){
		case type_ptr:
		case type_block:
			fp = type_is(fp->ref, type_func);
			UCC_ASSERT(fp, "not a func for fcall");
			/* fall */

		case type_func:
			if(pfuncargs)
				*pfuncargs = fp->bits.func.args;
			fp = fp->ref;
			UCC_ASSERT(fp, "no ref for func");
			break;

		default:
			ICE("can't func-deref non func-ptr/block ref (%d)", fp->type);
	}

	return fp;
}

int type_decayable(type *r)
{
	switch(type_skip_all(r)->type){
		case type_array:
		case type_func:
			return 1;
		default:
			return 0;
	}
}

static type *type_keep_w_attr(type *t, where *loc, attribute **attr)
{
	attribute **i;

	for(i = attr; i && *i; i++)
		t = type_attributed(t, RETAIN(*i));

	if(loc && !type_has_loc(t))
		t = type_at_where(t, loc);

	return t;
}

type *type_decay(type *const ty)
{
	/* f(int x[][5]) decays to f(int (*x)[5]), not f(int **x) */
	where *loc = NULL;
	attribute **attr = NULL;
	type *test;
	type *ret = ty;

	for(test = ty; test; test = type_next_1(test)){
		switch(test->type){
			case type_auto:
				ICE("__auto_type");

			case type_where:
				if(!loc)
					loc = &test->bits.where;
				break;

			case type_attr:
				dynarray_add(&attr, test->bits.attr);
				break;

			case type_cast:
			case type_tdef:
				/* skip */
				break;

			case type_btype:
			case type_ptr:
			case type_block:
				/* nothing to decay */
				goto out;

			case type_array:
				ret = type_keep_w_attr(
						type_decayed_ptr_to(test->ref, test),
						loc, attr);
				goto out;

			case type_func:
				ret = type_keep_w_attr(
						type_ptr_to(test),
						loc, attr);
				goto out;
		}
	}

out:
	dynarray_free(attribute **, attr, NULL);
	return ret;
}

int type_is_void(type *r)
{
	return !!type_is_primitive(r, type_void);
}

int type_is_signed(type *r)
{
	/* need to take casts into account */
	while(r)
		switch(r->type){
			case type_btype:
				return btype_is_signed(r->bits.type);

			case type_ptr:
				/* "unspecified" */
				return 1;

			default:
				r = type_next_1(r);
		}

	return 0;
}

int type_is_floating(type *r)
{
	r = type_is(r, type_btype);

	if(!r)
		return 0;

	return type_floating(r->bits.type->primitive);
}

enum type_qualifier type_qual(const type *r)
{
	/* stop at the first pointer or type, collecting from type_cast quals */

	if(!r)
		return qual_none;

	switch(r->type){
		case type_btype:
		case type_auto:
		case type_func:
		case type_array:
			return qual_none;

		case type_where:
		case type_attr:
			return type_qual(r->ref);

		case type_cast:
			/* descend */
			return r->bits.cast.qual | type_qual(r->ref);

		case type_ptr:
		case type_block:
			return qual_none; /* no descend */

		case type_tdef:
			return type_qual(r->bits.tdef.type_of->tree_type);
	}

	ucc_unreach(qual_none);
}

enum type_primitive type_primitive(type *ty)
{
	ty = type_is_primitive(ty, type_unknown);
	UCC_ASSERT(ty, "not primitive?");
	return ty->bits.type->primitive;
}

funcargs *type_funcargs(type *r)
{
	type *test;

	r = type_skip_all(r);

	if((test = type_is(r, type_ptr))
	|| (test = type_is(r, type_block)))
	{
		r = type_skip_all(test->ref); /* jump down past the (*)() */
	}

	UCC_ASSERT(r && r->type == type_func,
			"not a function type - %s",
			type_kind_to_str(r->type));

	return r->bits.func.args;
}

int type_is_callable(type *r)
{
	type *test;

	r = type_skip_all(r);

	if((test = type_is(r, type_ptr)) || (test = type_is(r, type_block)))
		return !!type_is(test->ref, type_func);

	return 0;
}

int type_is_const(type *const ty)
{
	/* const int x[3] - const, despite being array->const->int */
	type *nonarray = type_is_array(ty);
	if(!nonarray)
		nonarray = ty;

	return !!(type_qual(nonarray) & qual_const);
}

unsigned type_array_len(type *r)
{
	r = type_is(r, type_array);

	UCC_ASSERT(r, "not an array");
	UCC_ASSERT(r->bits.array.size, "array len of []");

	return const_fold_val_i(r->bits.array.size);
}

int type_is_promotable(type *const t, type **pto)
{
	type *test;
	if((test = type_is_primitive(t, type_unknown))){
		static unsigned sz_int, sz_double;
		const int fp = type_floating(test->bits.type->primitive);
		unsigned rsz;

		if(!sz_int){
			sz_int = type_primitive_size(type_int);
			sz_double = type_primitive_size(type_double);
		}

		rsz = type_size(test, type_loc(t)); /* may be enum-int */

		if(rsz < (fp ? sz_double : sz_int)){
			*pto = type_nav_btype(cc1_type_nav, fp ? type_double : type_int);
			return 1;
		}
	}

	return 0;
}

int type_is_variadic_func(type *r)
{
	return (r = type_is(r, type_func)) && r->bits.func.args->variadic;
}

int type_is_autotype(type *t)
{
	t = type_skip_all(t);
	return t && t->type == type_auto;
}

type *type_is_decayed_array(type *r)
{
	if((r = type_is(r, type_ptr)))
		return r->bits.ptr.decayed_from;
	return NULL;
}
