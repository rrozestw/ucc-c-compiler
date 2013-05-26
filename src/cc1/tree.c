#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../util/alloc.h"
#include "../util/util.h"
#include "data_structs.h"
#include "macros.h"
#include "sym.h"
#include "../util/platform.h"
#include "sue.h"
#include "decl.h"
#include "cc1.h"

const where *eof_where = NULL;

intval *intval_new(long v)
{
	intval *iv = umalloc(sizeof *iv);
	iv->val = v;
	return iv;
}

void where_new(struct where *w)
{
	extern const char *current_fname;
	extern int parse_finished;

	if(parse_finished){
eof_w:
		if(eof_where){
			memcpy(w, eof_where, sizeof *w);
		}else if(current_fname){
			/* still parsing, at EOF */
			goto final;
		}else{
			ICE("where_new() after buffer eof");
		}

	}else{
		extern int current_line, current_chr;
		extern const char *current_fname, *current_line_str;
		extern int current_fname_used, current_line_str_used;

		if(!current_fname || !current_line_str)
			goto eof_w;

final:
		w->line  = current_line;
		w->chr   = current_chr;
		w->fname = current_fname;
		w->line_str = current_line_str;

		current_fname_used = 1;
		current_line_str_used = 1;
	}
}

int intval_cmp(const intval *a, const intval *b)
{
	const long la = a->val, lb = b->val;

	if(la > lb)
		return 1;
	if(la < lb)
		return -1;
	return 0;
}

static type *type_new_primitive1(enum type_primitive p)
{
	type *t = umalloc(sizeof *t);
	where_new(&t->where);
	t->primitive = p;
	t->is_signed = 1;
	return t;
}

const type *type_new_primitive_sue(enum type_primitive p, struct_union_enum_st *s)
{
	type *t = type_new_primitive1(p);
	t->sue = s;
	return t;
}

const type *type_new_primitive_signed(enum type_primitive p, int sig)
{
	type *t = type_new_primitive1(p);
	t->is_signed = sig;
	return t;
}

const type *type_new_primitive(enum type_primitive p)
{
	return type_new_primitive1(p);
}

unsigned type_primitive_size(enum type_primitive tp)
{
	switch(tp){
		case type_char:
		case type__Bool:
		case type_void:
			return 1;

		case type_short:
			return 2;

		case type_int:
		case type_float:
			return 4;

		case type_long:
		case type_double:
			/* 4 on 32-bit */
			if(cc1_m32)
				return 4;
			return 8;

		case type_llong:
			ICW("TODO: long long");
			return 16;

		case type_ldouble:
			/* 80-bit float */
			ICW("TODO: long double");
			return 10; /* FIXME: 32-bit? */

		case type_union:
		case type_struct:
		case type_enum:
			ICE("sue size");

		case type_unknown:
			break;
	}

	ICE("type %s in %s()",
			type_primitive_to_str(tp), __func__);
	return -1;
}

unsigned type_size(const type *t, where const *from)
{
	if(t->sue)
		return sue_size(t->sue, from);

	return type_primitive_size(t->primitive);
}

int type_qual_equal(enum type_qualifier a, enum type_qualifier b)
{
 return (a | qual_restrict) == (b | qual_restrict);
}

int type_equal(const type *a, const type *b, enum type_cmp mode)
{
	if((mode & TYPE_CMP_ALLOW_SIGNED_UNSIGNED) == 0
	&& a->is_signed != b->is_signed)
	{
		return 0;
	}

	if(a->sue != b->sue)
		return 0;

	return mode & TYPE_CMP_EXACT ? a->primitive == b->primitive : 1;
}

const char *op_to_str(const enum op_type o)
{
	switch(o){
		case op_multiply: return "*";
		case op_divide:   return "/";
		case op_plus:     return "+";
		case op_minus:    return "-";
		case op_modulus:  return "%";
		case op_eq:       return "==";
		case op_ne:       return "!=";
		case op_le:       return "<=";
		case op_lt:       return "<";
		case op_ge:       return ">=";
		case op_gt:       return ">";
		case op_or:       return "|";
		case op_xor:      return "^";
		case op_and:      return "&";
		case op_orsc:     return "||";
		case op_andsc:    return "&&";
		case op_not:      return "!";
		case op_bnot:     return "~";
		case op_shiftl:   return "<<";
		case op_shiftr:   return ">>";
		CASE_STR_PREFIX(op, unknown);
	}
	return NULL;
}

const char *type_primitive_to_str(const enum type_primitive p)
{
	switch(p){
		CASE_STR_PREFIX(type, void);
		CASE_STR_PREFIX(type, char);
		CASE_STR_PREFIX(type, short);
		CASE_STR_PREFIX(type, int);
		CASE_STR_PREFIX(type, long);
		CASE_STR_PREFIX(type, float);
		CASE_STR_PREFIX(type, double);
		CASE_STR_PREFIX(type, _Bool);

		case type_llong:   return "long long";
		case type_ldouble: return "long double";

		CASE_STR_PREFIX(type, struct);
		CASE_STR_PREFIX(type, union);
		CASE_STR_PREFIX(type, enum);

		CASE_STR_PREFIX(type, unknown);
	}
	return NULL;
}

const char *type_qual_to_str(const enum type_qualifier qual, int trailing_space)
{
	static char buf[32];
	/* trailing space is purposeful */
	snprintf(buf, sizeof buf, "%s%s%s%s",
		qual & qual_const    ? "const"    : "",
		qual & qual_volatile ? "volatile" : "",
		qual & qual_restrict ? "restrict" : "",
		qual && trailing_space ? " " : "");
	return buf;
}

int op_can_compound(enum op_type o)
{
	switch(o){
		case op_plus:
		case op_minus:
		case op_multiply:
		case op_divide:
		case op_modulus:
		case op_not:
		case op_bnot:
		case op_and:
		case op_or:
		case op_xor:
		case op_shiftl:
		case op_shiftr:
			return 1;
		default:
			break;
	}
	return 0;
}

int op_is_comparison(enum op_type o)
{
	switch(o){
		case op_eq:
		case op_ne:
		case op_le:
		case op_lt:
		case op_ge:
		case op_gt:
			return 1;
		default:
			break;
	}
	return 0;
}

int op_is_shortcircuit(enum op_type o)
{
	switch(o){
		case op_andsc:
		case op_orsc:
			return 1;
		default:
			return 0;
	}
}

int op_is_relational(enum op_type o)
{
	return op_is_comparison(o) || op_is_shortcircuit(o);
}

const char *type_to_str(const type *t)
{
#define BUF_SIZE (sizeof(buf) - (bufp - buf))
	static char buf[TYPE_STATIC_BUFSIZ];
	char *bufp = buf;

	if(!t->is_signed) bufp += snprintf(bufp, BUF_SIZE, "unsigned ");

	if(t->sue){
		snprintf(bufp, BUF_SIZE, "%s%s %s",
				sue_incomplete(t->sue) ? "incomplete-" : "",
				sue_str(t->sue),
				t->sue->spel);

	}else{
		switch(t->primitive){
			case type_void:
			case type__Bool:
			case type_char:
			case type_short:
			case type_int:
			case type_long:
			case type_float:
			case type_double:
			case type_llong:
			case type_ldouble:
				snprintf(bufp, BUF_SIZE, "%s",
						type_primitive_to_str(t->primitive));
				break;

			case type_unknown:
				ICE("unknown type primitive (%W)", &t->where);
			case type_enum:
			case type_struct:
			case type_union:
				ICE("struct/union/enum without ->sue");
		}
	}

	return buf;
}
