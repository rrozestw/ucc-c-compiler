#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "../defs.h"
#include "ops.h"
#include "expr_op.h"
#include "../out/lbl.h"
#include "../out/asm.h"
#include "../type_is.h"
#include "../type_nav.h"

/*
 * usual arithmetic conversions:
 *   mul, div, mod,
 *   plus, minus,
 *   lt, gt le, ge, eq, ne,
 *   and, xor, or
 *
 * integer promotions:
 *   + - ~ << >>
 */

#define BOOLEAN_TYPE type_int

const char *str_expr_op()
{
	return "op";
}

#define addr_multiply(i, addr_type)  \
do{                                  \
	type *next = type_next(addr_type); \
	sintegral_t step = 1;              \
                                     \
	if(next)                           \
		step = type_size(next, NULL);    \
                                     \
	i *= step;                         \
}while(0)

static void const_op_num_fp(
		expr *e, consty *k,
		const consty *lhs, const consty *rhs)
{
	/* float const-op */
	floating_t fp_r;

	assert(lhs->type == CONST_NUM && (!rhs || rhs->type == CONST_NUM));

	fp_r = const_op_exec_fp(
			lhs->bits.num.val.f,
			rhs ? &rhs->bits.num.val.f : NULL,
			e->bits.op.op);

	k->type = CONST_NUM;

	/* both relational and normal ops between floats are not constant */
	if(!k->nonstandard_const)
		k->nonstandard_const = e;

	if(op_returns_bool(e->bits.op.op)){
		k->bits.num.val.i = fp_r; /* convert to bool */

	}else{
		const btype *ty = type_get_type(e->tree_type);

		UCC_ASSERT(ty, "no float type for float op?");

		k->bits.num.val.f = fp_r;

		switch(ty->primitive){
			case type_float:   k->bits.num.suffix = VAL_FLOAT;   break;
			case type_double:  k->bits.num.suffix = VAL_DOUBLE;  break;
			case type_ldouble: k->bits.num.suffix = VAL_LDOUBLE; break;
			default: ICE("bad float");
		}
	}
}

typedef struct
{
	int is_lbl;
	union
	{
		integral_t i;
		const char *lbl;
	} bits;
} collapsed_consty;

static void collapse_const(collapsed_consty *out, const consty *in)
{
	switch(in->type){
		case CONST_NO:
			assert(0);

		case CONST_NUM:
			out->is_lbl = 0;
			out->bits.i = in->bits.num.val.i;
			break;

		case CONST_STRK:
			out->is_lbl = 1;
			out->bits.lbl = in->bits.str->lit->lbl;
			break;

		case CONST_NEED_ADDR:
		case CONST_ADDR:
			if(in->bits.addr.is_lbl){
				out->is_lbl = 1;
				out->bits.lbl = in->bits.addr.bits.lbl;
			}else{
				out->is_lbl = 0;
				out->bits.i = in->bits.addr.bits.memaddr;
			}
			break;
	}
}

static void const_op_num_int(
		expr *e, consty *k,
		const consty *lhs, const consty *rhs)
{
	const char *err = NULL;
	int is_signed;
	collapsed_consty l, r;

	/* the op is signed if an operand is, not the result,
	 * e.g. u_a < u_b produces a bool (signed) */
	is_signed = type_is_signed(e->lhs->tree_type) ||
		(e->rhs ? type_is_signed(e->rhs->tree_type) : 0);

	collapse_const(&l, lhs);
	if(rhs){
		type *ptr;
		int ptr_r = 0;

		collapse_const(&r, rhs);

		/* need to apply pointer arithmetic */
		if(l.is_lbl + r.is_lbl < 2 /* at least one side is a number */
		&& (e->bits.op.op == op_plus || e->bits.op.op == op_minus) /* +/- */
		&& ((ptr = type_is_ptr(e->lhs->tree_type))
			|| (ptr_r = 1, ptr = type_is_ptr(e->rhs->tree_type))))
		{
			unsigned step = type_size(ptr, &e->where);

			assert(!(ptr_r ? &l : &r)->is_lbl);

			*(ptr_r ? &l.bits.i : &r.bits.i) *= step;
		}
	}else{
		memset(&r, 0, sizeof r);
	}

	CONST_FOLD_LEAF(k);
	switch(l.is_lbl + r.is_lbl){
		default:
			assert(0);

		case 1:
		{
			collapsed_consty *num_side = NULL;
			if(lhs->type == CONST_NUM)
				num_side = &l;
			else if(rhs)
				num_side = &r;

			/* label and num - only + and -, or comparison */
			switch(e->bits.op.op){
				case op_not:
					/* !&lbl */
					assert(!rhs);
					k->type = CONST_NUM;
					k->bits.num.val.i = 0;
					break;

				case op_eq:
				case op_ne:
					assert(num_side && "binary two labels shouldn't be here");
					if(num_side->bits.i == 0){
						/* &x == 0, etc */
						k->type = CONST_NUM;
						k->bits.num.val.i = (e->bits.op.op != op_eq);
						break;
					}
					/* fall */

				default:
					/* ~&lbl, 5 == &lbl, 6 > &lbl etc */
					k->type = CONST_NO;
					break;

				case op_plus:
				case op_minus:
					if(!rhs){
						/* unary +/- on label */
						k->type = CONST_NO;
						break;
					}

					memcpy_safe(k, num_side == &l ? rhs : lhs);
					if(e->bits.op.op == op_plus)
						k->offset += num_side->bits.i;
					else if(e->bits.op.op == op_minus)
						k->offset -= num_side->bits.i;
					break;
			}
			break;
		}

		case 0:
		{
			integral_t int_r;

			int_r = const_op_exec(
					l.bits.i, rhs ? &r.bits.i : NULL,
					e->bits.op.op, is_signed, &err);

			if(err){
				warn_at(&e->where, "%s", err);
				k->type = CONST_NO;
			}else{
				k->type = CONST_NUM;
				k->bits.num.val.i = int_r;
			}
			break;
		}

		case 2:
			switch(e->bits.op.op){
				case op_not:
					assert(0 && "binary not?");
				case op_unknown:
					assert(0);
				default:
					k->type = CONST_NO;
					break;

				case op_orsc:
				case op_andsc:
					k->type = CONST_NUM;
					k->bits.num.val.i = 1;
					break;

				case op_eq:
				case op_ne:
				{
					int same = !strcmp(l.bits.lbl, r.bits.lbl);
					k->type = CONST_NUM;

					k->bits.num.val.i = ((e->bits.op.op == op_eq) == same);
					break;
				}
			}
			break;
	}
}

static void const_op_num(
		expr *e, consty *k,
		const consty *lhs, const consty *rhs)
{
	int fp[2] = {
		type_is_floating(e->lhs->tree_type)
	};

	if(rhs){
		fp[1] = type_is_floating(e->rhs->tree_type);

		UCC_ASSERT(!(fp[0] ^ fp[1]),
				"one float and one non-float?");
	}

	if(fp[0])
		const_op_num_fp(e, k, lhs, rhs);
	else
		const_op_num_int(e, k, lhs, rhs);
}

static void const_shortcircuit(
		expr *e, consty *k,
		const consty *lhs,
		const consty *rhs)
{
	collapsed_consty clhs;
	int truth;

	if(lhs->type == CONST_NO)
		return;
	assert(rhs->type == CONST_NO);

	collapse_const(&clhs, lhs);
	truth = clhs.is_lbl ? 1 : clhs.bits.i;

	if(e->bits.op.op == op_andsc){
		if(truth){
			/* &lbl && [not-constant] */
		}else{
			/* 0 && [not-constant] */
			CONST_FOLD_LEAF(k);
			k->type = CONST_NUM;
			k->bits.num.val.i = 0;
		}
	}else if(e->bits.op.op == op_orsc){
		if(truth){
			/* &lbl || [not-constant] */
			CONST_FOLD_LEAF(k);
			k->type = CONST_NUM;
			k->bits.num.val.i = 1;
		}else{
			/* 0 || [not-constant] */
		}
	}else{
		assert(0);
	}
}

static void fold_const_expr_op(expr *e, consty *k)
{
	consty lhs, rhs;

	memset(k, 0, sizeof *k);

	const_fold(e->lhs, &lhs);
	if(e->rhs){
		const_fold(e->rhs, &rhs);
	}else{
		memset(&rhs, 0, sizeof rhs);
		rhs.type = CONST_NUM;
	}

	if(!CONST_AT_COMPILE_TIME(lhs.type) /* catch need_addr */
	|| !CONST_AT_COMPILE_TIME(rhs.type))
	{
		/* allow shortcircuit */
		k->type = CONST_NO;
		if(e->bits.op.op == op_andsc || e->bits.op.op == op_orsc)
			const_shortcircuit(e, k, &lhs, &rhs);
		return;
	}

	const_op_num(e, k, &lhs, e->rhs ? &rhs : NULL);

	if(!k->nonstandard_const
	&& lhs.type != CONST_NO /* otherwise it's uninitialised */
	&& rhs.type != CONST_NO)
	{
		k->nonstandard_const = lhs.nonstandard_const
			? lhs.nonstandard_const : rhs.nonstandard_const;
	}
}

static void expr_promote_if_smaller(expr **pe, symtable *stab, int do_float)
{
	expr *e = *pe;
	type *to;

	if(type_is_floating(e->tree_type) && !do_float)
		return;

	if(type_is_promotable(e->tree_type, &to)){
		expr *cast;

		UCC_ASSERT(!type_is(e->tree_type, type_ptr),
				"invalid promotion for pointer");

		/* if(type_primitive_size(e->tree_type->type->primitive) >= type_primitive_size(to))
		 *   return;
		 *
		 * insert down-casts too - the tree_type of the expression is still important
		 */

		cast = expr_new_cast(e, to, 1);

		fold_expr_cast_descend(cast, stab, 0);

		*pe = cast;
	}
}

static void expr_promote_int_if_smaller(expr **pe, symtable *stab)
{
	expr_promote_if_smaller(pe, stab, 0);
}

void expr_promote_default(expr **pe, symtable *stab)
{
	expr_promote_if_smaller(pe, stab, 1);
}

type *op_required_promotion(
		enum op_type op,
		expr *lhs, expr *rhs,
		where *w,
		type **plhs, type **prhs,
		const char *op_desc)
{
	type *resolved = NULL;
	type *const tlhs = lhs->tree_type, *const trhs = rhs->tree_type;
	int floating_lhs;

	*plhs = *prhs = NULL;

	if((floating_lhs = type_is_floating(tlhs))
			!= type_is_floating(trhs))
	{
		/* cast _to_ the floating type */
		type *res = floating_lhs ? (*prhs = tlhs) : (*plhs = trhs);

		resolved = op_returns_bool(op)
			? type_nav_btype(cc1_type_nav, BOOLEAN_TYPE)
			: res;

		goto fin;
		/* else we pick the largest floating or integral type */
	}

#if 0
	If either operand is a pointer:
		relation: both must be pointers

	if both operands are pointers:
		must be '-'

	exactly one pointer:
		must be + or -
#endif

	if(type_is_void(tlhs) || type_is_void(trhs))
		die_at(w, "use of void expression");

	{
		const int l_ptr = !!type_is(tlhs, type_ptr);
		const int r_ptr = !!type_is(trhs, type_ptr);

		if(l_ptr && r_ptr){
			char buf[TYPE_STATIC_BUFSIZ];

			if(op == op_minus){
				/* don't allow void * */
				switch(type_cmp(tlhs, trhs, 0)){
					case TYPE_CONVERTIBLE_IMPLICIT:
					case TYPE_CONVERTIBLE_EXPLICIT:
					case TYPE_NOT_EQUAL:
						die_at(w, "subtraction of distinct pointer types %s and %s",
								type_to_str(tlhs), type_to_str_r(buf, trhs));
					case TYPE_QUAL_ADD:
					case TYPE_QUAL_SUB:
					case TYPE_QUAL_POINTED_ADD:
					case TYPE_QUAL_POINTED_SUB:
					case TYPE_QUAL_NESTED_CHANGE:
					case TYPE_EQUAL:
					case TYPE_EQUAL_TYPEDEF:
						break;
				}

				resolved = type_nav_btype(cc1_type_nav, type_intptr_t);

			}else if(op_returns_bool(op)){
ptr_relation:
				if(op_is_comparison(op)){
					if(fold_type_chk_warn(tlhs, trhs, w,
							l_ptr && r_ptr
							? "comparison lacks a cast"
							: "comparison between pointer and integer"))
					{
						/* not equal - ptr vs int */
						*(l_ptr ? prhs : plhs) = type_nav_btype(cc1_type_nav, type_intptr_t);
					}
				}

				resolved = type_nav_btype(cc1_type_nav, BOOLEAN_TYPE);

			}else{
				die_at(w, "operation between two pointers must be relational or subtraction");
			}

			goto fin;

		}else if(l_ptr || r_ptr){
			/* + or - */

			/* cmp between pointer and integer - missing cast */
			if(op_returns_bool(op))
				goto ptr_relation;

			switch(op){
				default:
					die_at(w, "operation between pointer and integer must be + or -");

				case op_minus:
					if(!l_ptr)
						die_at(w, "subtraction of pointer from integer");

				case op_plus:
					break;
			}

			resolved = l_ptr ? tlhs : trhs;

			/* FIXME: promote to unsigned */
			*(l_ptr ? prhs : plhs) = type_nav_btype(cc1_type_nav, type_intptr_t);

			/* + or -, check if we can */
			{
				type *const next = type_next(resolved);

				if(!type_is_complete(next)){
					if(type_is_void(next)){
						warn_at(w, "arithmetic on void pointer");
					}else{
						die_at(w, "arithmetic on pointer to incomplete type %s",
								type_to_str(next));
					}
					/* TODO: note: type declared at resolved->where */
				}else if(type_is(next, type_func)){
					warn_at(w, "arithmetic on function pointer '%s'",
							type_to_str(resolved));
				}
			}

			goto fin;
		}
	}

#if 0
	If both operands have the same type, then no further conversion is needed.

	Otherwise, if both operands have signed integer types or both have unsigned
	integer types, the operand with the type of lesser integer conversion rank
	is converted to the type of the operand with greater rank.

	Otherwise, if the operand that has unsigned integer type has rank greater
	or equal to the rank of the type of the other operand, then the operand
	with signed integer type is converted to the type of the operand with
	unsigned integer type.

	Otherwise, if the type of the operand with signed integer type can
	represent all of the values of the type of the operand with unsigned
	integer type, then the operand with unsigned integer type is converted to
	the type of the operand with signed integer type.

	Otherwise, both operands are converted to the unsigned integer type
	corresponding to the type of the operand with signed integer type.
#endif

	{
		type *tlarger = NULL;

		if(op == op_shiftl || op == op_shiftr){
			/* fine with any parameter sizes
			 * don't need to match. resolves to lhs,
			 * or int if lhs is smaller (done before this function)
			 */

			UCC_ASSERT(
					type_size(tlhs, &lhs->where)
						>= type_primitive_size(type_int),
					"shift operand should have been promoted");

			resolved = tlhs;

		}else if(op == op_andsc || op == op_orsc){
			/* no promotion */
			resolved = type_nav_btype(cc1_type_nav, BOOLEAN_TYPE);

		}else{
			const int l_unsigned = !type_is_signed(tlhs),
			          r_unsigned = !type_is_signed(trhs);

			const int l_sz = type_size(tlhs, &lhs->where),
			          r_sz = type_size(trhs, &rhs->where);

			if(l_unsigned == r_unsigned){
				if(l_sz != r_sz){
					const int l_larger = l_sz > r_sz;

					fold_type_chk_warn(
							tlhs, trhs,
							w, op_desc);

					*(l_larger ? prhs : plhs) = (l_larger ? tlhs : trhs);

					tlarger = l_larger ? tlhs : trhs;

				}else{
					/* default to either */
					tlarger = tlhs;
				}

			}else if(l_unsigned ? l_sz >= r_sz : r_sz >= l_sz){
				if(l_unsigned)
					tlarger = *prhs = tlhs;
				else
					tlarger = *plhs = trhs;

			}else if(l_unsigned ? r_sz > l_sz : l_sz > r_sz){
				/* can the signed type represent all of the unsigned type's values?
				 * this is true if signed_type > unsigned_type
				 * - convert unsigned to signed type */

				if(l_unsigned)
					tlarger = *plhs = trhs;
				else
					tlarger = *prhs = tlhs;

			}else{
				/* else convert both to (unsigned)signed_type */
				type *signed_t = l_unsigned ? trhs : tlhs;

				tlarger = *plhs = *prhs = type_sign(signed_t, 0);
			}

			/* if we have a _comparison_, convert to bool */
			resolved = op_returns_bool(op)
				? type_nav_btype(cc1_type_nav, BOOLEAN_TYPE)
				: tlarger;
		}
	}

fin:
	UCC_ASSERT(resolved, "no decl from type promotion");

	return resolved; /* XXX: memleak in some cases */
}

type *op_promote_types(
		enum op_type op,
		expr **plhs, expr **prhs,
		where *w, symtable *stab,
		const char *op_desc)
{
	type *tlhs, *trhs;
	type *resolved;

	resolved = op_required_promotion(
			op, *plhs, *prhs, w,
			&tlhs, &trhs, op_desc);

	if(tlhs)
		fold_insert_casts(tlhs, plhs, stab);

	if(trhs)
		fold_insert_casts(trhs, prhs, stab);

	return resolved;
}

static expr *expr_is_array_cast(expr *e)
{
	expr *array = e;

	if(expr_kind(array, cast))
		array = array->expr;

	if(type_is(array->tree_type, type_array))
		return array;

	return NULL;
}

int fold_check_bounds(expr *e, int chk_one_past_end)
{
	/* this could be in expr_deref, but it catches more in expr_op */
	expr *array;
	int lhs = 0;

	/* check bounds */
	if(e->bits.op.op != op_plus && e->bits.op.op != op_minus)
		return 0;

	array = expr_is_array_cast(e->lhs);
	if(array)
		lhs = 1;
	else
		array = expr_is_array_cast(e->rhs);

	if(array && !type_is_incomplete_array(array->tree_type)){
		consty k;

		const_fold(lhs ? e->rhs : e->lhs, &k);

		if(k.type == CONST_NUM){
			const size_t sz = type_array_len(array->tree_type);

			UCC_ASSERT(K_INTEGRAL(k.bits.num),
					"fp index?");

#define idx k.bits.num
			if(e->bits.op.op == op_minus)
				idx.val.i = -idx.val.i;

			/* index is allowed to be one past the end, i.e. idx.val == sz */
			if((sintegral_t)idx.val.i < 0
			|| (chk_one_past_end ? idx.val.i > sz : idx.val.i == sz))
			{
				/* XXX: note */
				char buf[WHERE_BUF_SIZ];

				warn_at(&e->where,
						"index %" NUMERIC_FMT_D " out of bounds of array, size %ld\n"
						"%s: note: array declared here",
						idx.val.i, (long)sz,
						where_str_r(buf, type_loc(array->tree_type)));
				return 1;
			}
#undef idx
		}
	}

	return 0;
}

static int op_unsigned_cmp_check(expr *e)
{
	switch(e->bits.op.op){
			int lhs;
		/*case op_gt:*/
		case op_ge:
		case op_lt:
		case op_le:
			if((lhs = !type_is_signed(e->lhs->tree_type))
			||        !type_is_signed(e->rhs->tree_type))
			{
				consty k;

				const_fold(lhs ? e->rhs : e->lhs, &k);

				if(k.type == CONST_NUM && K_INTEGRAL(k.bits.num)){
					const int v = k.bits.num.val.i;

					if(v <= 0){
						warn_at(&e->where,
								"comparison of unsigned expression %s %d is always %s",
								op_to_str(e->bits.op.op), v,
								e->bits.op.op == op_lt || e->bits.op.op == op_le ? "false" : "true");
						return 1;
					}
				}
			}

		default:
			return 0;
	}
}

static int msg_if_precedence(expr *sub, where *w,
		enum op_type binary, int (*test)(enum op_type))
{
	sub = expr_skip_casts(sub);

	if(expr_kind(sub, op)
	&& sub->rhs /* don't warn for (1 << -5) : (-5) is a unary op */
	&& !sub->in_parens
	&& sub->bits.op.op != binary
	&& (test ? (*test)(sub->bits.op.op) : 1))
	{
		/* ==, !=, <, ... */
		warn_at(w, "%s has higher precedence than %s",
				op_to_str(sub->bits.op.op), op_to_str(binary));
		return 1;
	}
	return 0;
}

static int op_check_precedence(expr *e)
{
	switch(e->bits.op.op){
		case op_or:
		case op_and:
			return msg_if_precedence(e->lhs, &e->where, e->bits.op.op, op_is_comparison)
				||   msg_if_precedence(e->rhs, &e->where, e->bits.op.op, op_is_comparison);
			break;

		case op_andsc:
		case op_orsc:
			return msg_if_precedence(e->lhs, &e->where, e->bits.op.op, op_is_shortcircuit)
				||   msg_if_precedence(e->rhs, &e->where, e->bits.op.op, op_is_shortcircuit);
			break;

		case op_shiftl:
		case op_shiftr:
			return msg_if_precedence(e->lhs, &e->where, e->bits.op.op, NULL)
				|| msg_if_precedence(e->rhs, &e->where, e->bits.op.op, NULL);
			break;

		default:
			return 0;
	}
}

static int str_cmp_check(expr *e)
{
	if(op_is_comparison(e->bits.op.op)){
		consty kl, kr;

		const_fold(e->lhs, &kl);
		const_fold(e->rhs, &kr);

		if(kl.type == CONST_STRK || kr.type == CONST_STRK){
			warn_at(&e->rhs->where, "comparison with string literal is undefined");
			return 1;
		}
	}
	return 0;
}

static int op_shift_check(expr *e)
{
	switch(e->bits.op.op){
		case op_shiftl:
		case op_shiftr:
		{
			const unsigned ty_sz = CHAR_BIT * type_size(e->lhs->tree_type, &e->lhs->where);
			int undefined = 0;
			consty lhs, rhs;

			const_fold(e->lhs, &lhs);
			const_fold(e->rhs, &rhs);

			if(type_is_signed(e->rhs->tree_type)
			&& (sintegral_t)rhs.bits.num.val.i < 0)
			{
				warn_at(&e->rhs->where, "shift count is negative (%"
						NUMERIC_FMT_D ")", (sintegral_t)rhs.bits.num.val.i);

				undefined = 1;
			}else if(rhs.bits.num.val.i >= ty_sz){
				warn_at(&e->rhs->where, "shift count >= width of %s (%u)",
						type_to_str(e->lhs->tree_type), ty_sz);

				undefined = 1;
			}

			if(undefined){
				consty k;

				memset(&k, 0, sizeof k);

				if(lhs.type == CONST_NUM){
					k.type = CONST_NUM;
					k.bits.num.val.i = 0;
				}else{
					k.type = CONST_NO;
				}

				expr_set_const(e, &k);
			}

			return undefined; /* aka, warned */
		}
		default:
			return 0;
	}
}

static int op_float_check(expr *e)
{
	type *tl = e->lhs->tree_type,
	         *tr = e->rhs->tree_type;

	if((type_is_floating(tl) || type_is_floating(tr))
	&& !op_can_float(e->bits.op.op))
	{
		char buf[TYPE_STATIC_BUFSIZ];

		/* TODO: factor to a error-continuing function */
		fold_had_error = 1;
		warn_at_print_error(&e->where,
				"binary %s between '%s' and '%s'",
				op_to_str(e->bits.op.op),
				type_to_str_r(buf, tl),
				type_to_str(       tr));

		return 1;
	}

	return 0;
}

void expr_check_sign(const char *desc,
		expr *lhs, expr *rhs, where *w)
{
	consty kl, kr;

	const_fold(lhs, &kl);
	const_fold(rhs, &kr);

	/* don't bother for literals */
	if(kl.type != CONST_NUM
	&& kr.type != CONST_NUM
	&& type_is_scalar(lhs->tree_type) && type_is_scalar(rhs->tree_type)
	&& type_is_signed(lhs->tree_type) != type_is_signed(rhs->tree_type))
	{
		warn_at(w, "signed and unsigned types in '%s'", desc);
	}
}

void fold_expr_op(expr *e, symtable *stab)
{
	const char *op_desc = e->bits.op.array_notation
		? "subscript"
		: op_to_str(e->bits.op.op);

	UCC_ASSERT(e->bits.op.op != op_unknown, "unknown op in expression at %s",
			where_str(&e->where));

	FOLD_EXPR(e->lhs, stab);
	fold_check_expr(e->lhs, FOLD_CHK_NO_ST_UN, op_desc);

	if(e->rhs){
		FOLD_EXPR(e->rhs, stab);
		fold_check_expr(e->rhs, FOLD_CHK_NO_ST_UN, op_desc);

		if(op_float_check(e)){
			/* short circuit - TODO: error expr */
			e->tree_type = type_nav_btype(cc1_type_nav, type_int);
			return;
		}

		/* no-op if float */
		expr_promote_int_if_smaller(&e->lhs, stab);
		expr_promote_int_if_smaller(&e->rhs, stab);

		/* must check signs before casting */
		if(op_is_comparison(e->bits.op.op)){
			expr_check_sign(
					op_desc,
					e->lhs,
					e->rhs,
					&e->where);
		}

		e->tree_type = op_promote_types(e->bits.op.op,
				&e->lhs, &e->rhs, &e->where, stab, op_desc);

		(void)(
				fold_check_bounds(e, 1) ||
				op_check_precedence(e) ||
				op_unsigned_cmp_check(e) ||
				op_shift_check(e) ||
				str_cmp_check(e));

	}else{
		/* (except unary-not) can only have operations on integers,
		 * promote to signed int
		 */
		const char *op_desc = op_to_str(e->bits.op.op);

		expr_promote_int_if_smaller(&e->lhs, stab);

		switch(e->bits.op.op){
			default:
				ICE("bad unary op %s", op_desc);

			case op_not:
				fold_check_expr(e->lhs,
						FOLD_CHK_NO_ST_UN,
						op_desc);

				e->tree_type = type_nav_btype(cc1_type_nav, type_int);
				break;

			case op_plus:
			case op_minus:
			case op_bnot:
				fold_check_expr(
						e->lhs,
						(e->bits.op.op == op_bnot ? FOLD_CHK_INTEGRAL : 0)
							| FOLD_CHK_NO_ST_UN,
						op_desc);

				e->tree_type = e->lhs->tree_type;
				break;
		}
	}
}

const out_val *gen_expr_str_op(expr *e, out_ctx *octx)
{
	idt_printf("op: %s\n", op_to_str(e->bits.op.op));
	gen_str_indent++;

#define PRINT_IF(hs) if(e->hs) print_expr(e->hs)
	PRINT_IF(lhs);
	PRINT_IF(rhs);
#undef PRINT_IF

	gen_str_indent--;

	UNUSED_OCTX();
}

static const out_val *op_shortcircuit(expr *e, out_ctx *octx)
{
	out_blk *blk_rhs, *blk_empty, *landing;
	const out_val *lhs;

	blk_rhs = out_blk_new(octx, "shortcircuit_a");
	blk_empty = out_blk_new(octx, "shortcircuit_b");
	landing = out_blk_new(octx, "shortcircuit_landing");

	lhs = gen_expr(e->lhs, octx);
	lhs = out_normalise(octx, lhs);

	out_ctrl_branch(
			octx,
			lhs,
			e->bits.op.op == op_andsc ? blk_rhs : blk_empty,
			e->bits.op.op == op_andsc ? blk_empty : blk_rhs);

	out_current_blk(octx, blk_rhs);
	{
		const out_val *rhs = gen_expr(e->rhs, octx);
		rhs = out_normalise(octx, rhs);

		out_ctrl_transfer(octx, landing, rhs, &blk_rhs);
	}

	out_current_blk(octx, blk_empty);
	{
		out_ctrl_transfer(
				octx,
				landing,
				out_new_l(
					octx,
					type_nav_btype(cc1_type_nav, BOOLEAN_TYPE),
					e->bits.op.op == op_orsc ? 1 : 0),
				&blk_empty);

	}

	out_current_blk(octx, landing);
	{
		const out_val *merged = out_ctrl_merge(octx, blk_empty, blk_rhs);

		return merged;
	}
}

const out_val *gen_expr_op(expr *e, out_ctx *octx)
{
	const out_val *lhs, *rhs, *eval;

	switch(e->bits.op.op){
		case op_orsc:
		case op_andsc:
			return op_shortcircuit(e, octx);

		case op_unknown:
			ICE("asm_operate: unknown operator got through");

		default:
			break;
	}

	lhs = gen_expr(e->lhs, octx);

	if(!e->rhs)
		return out_op_unary(octx, e->bits.op.op, lhs);

	rhs = gen_expr(e->rhs, octx);

	eval = out_op(octx, e->bits.op.op, lhs, rhs);

	/* make sure we get the pointer, for example 2+(int *)p
	 * or the int, e.g. (int *)a && (int *)b -> int */
	eval = out_change_type(octx, eval, e->tree_type);

	if(fopt_mode & FOPT_TRAPV
	&& type_is_integral(e->tree_type)
	&& type_is_signed(e->tree_type))
	{
		out_blk *land = out_blk_new(octx, "trapv_end");
		out_blk *blk_undef = out_blk_new(octx, "travp_bad");

		out_ctrl_branch(octx,
				out_new_overflow(octx, &eval),
				blk_undef,
				land);

		out_current_blk(octx, blk_undef);
		out_ctrl_end_undefined(octx);

		out_current_blk(octx, land);
	}

	return eval;
}

void mutate_expr_op(expr *e)
{
	e->f_const_fold = fold_const_expr_op;
}

expr *expr_new_op(enum op_type op)
{
	expr *e = expr_new_wrapper(op);
	e->bits.op.op = op;
	return e;
}

expr *expr_new_op2(enum op_type o, expr *l, expr *r)
{
	expr *e = expr_new_op(o);
	e->lhs = l, e->rhs = r;
	return e;
}

const out_val *gen_expr_style_op(expr *e, out_ctx *octx)
{
	if(e->rhs){
		const char *post;
		char middle[16];

		if(e->bits.op.array_notation){
			strcpy(middle, "[");
			post = "]";
		}else{
			snprintf(middle, sizeof middle, " %s ", op_to_str(e->bits.op.op));
			post = "";
		}

		IGNORE_PRINTGEN(gen_expr(e->lhs, octx));
		stylef("%s", middle);
		IGNORE_PRINTGEN(gen_expr(e->rhs, octx));
		stylef("%s", post);

	}else{
		stylef("%s ", op_to_str(e->bits.op.op));
		IGNORE_PRINTGEN(gen_expr(e->lhs, octx));
	}
	return NULL;
}
