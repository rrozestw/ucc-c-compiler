#include "ops.h"
#include "expr_assign_compound.h"

const char *str_expr_assign_compound()
{
	return "compound-assignment";
}

void fold_expr_assign_compound(expr *e, symtable *stab)
{
	const char *const desc = "compound assignment";
#define lvalue e->lhs

	fold_inc_writes_if_sym(lvalue, stab);

	fold_expr_nodecay(e->lhs, stab);
	FOLD_EXPR(e->rhs, stab);

	fold_check_expr(e->lhs, FOLD_CHK_NO_ST_UN, desc);
	fold_check_expr(e->rhs, FOLD_CHK_NO_ST_UN, desc);

	/* skip the addr we inserted */
	if(!expr_must_lvalue(lvalue, desc)){
		/* prevent ICE from type_size(vla), etc */
		e->tree_type = lvalue->tree_type;
		return;
	}

	expr_assign_const_check(lvalue, &e->where);

	fold_check_restrict(lvalue, e->rhs, desc, &e->where);

	UCC_ASSERT(op_can_compound(e->bits.compoundop.op), "non-compound op in compound expr");

	/*expr_promote_int_if_smaller(&e->lhs, stab);
	 * lhs int promotion is handled in code-gen */
	expr_promote_int_if_smaller(&e->rhs, stab);

	{
		type *tlhs, *trhs;
		type *resolved = op_required_promotion(
				e->bits.compoundop.op, lvalue, e->rhs,
				&e->where, desc,
				&tlhs, &trhs);

		if(tlhs){
			/* must cast the lvalue, then down cast once the operation is done
			 * special handling for expr_kind(e->lhs, cast) is done in the gen-code
			 */
			e->bits.compoundop.upcast_ty = tlhs;

		}else if(trhs){
			fold_insert_casts(trhs, &e->rhs, stab);
		}

		e->tree_type = lvalue->tree_type;

		(void)resolved;
		/*type_free_1(resolved); XXX: memleak */
	}

	/* type check is done in op_required_promotion() */
#undef lvalue
}

const out_val *gen_expr_assign_compound(const expr *e, out_ctx *octx)
{
	/* int += float
	 * lea int, cast up to float, add, cast down to int, store
	 */
	const out_val *saved_post = NULL, *addr_lhs, *rhs, *lhs, *result;

	addr_lhs = gen_expr(e->lhs, octx);

	out_val_retain(octx, addr_lhs); /* 2 */

	if(e->assign_is_post){
		out_val_retain(octx, addr_lhs); /* 3 */
		saved_post = out_deref(octx, addr_lhs); /* addr_lhs=2, saved_post=1 */
	}

	/* delay the dereference until after generating rhs.
	 * this is fine, += etc aren't sequence points
	 */

	rhs = gen_expr(e->rhs, octx);

	/* here's the delayed dereference */
	lhs = out_deref(octx, addr_lhs); /* addr_lhs=1 */
	if(e->bits.compoundop.upcast_ty)
		lhs = out_cast(octx, lhs, e->bits.compoundop.upcast_ty, /*normalise_bool:*/1);

	result = out_op(octx, e->bits.compoundop.op, lhs, rhs);
	gen_op_trapv(e->tree_type, &result, octx, e->bits.compoundop.op);

	if(e->bits.compoundop.upcast_ty) /* need to cast back down to store */
		result = out_cast(octx, result, e->tree_type, /*normalise_bool:*/1);

	if(!saved_post)
		out_val_retain(octx, result);
	out_store(octx, addr_lhs, result);

	if(!saved_post)
		return result;
	return saved_post;
}

void dump_expr_assign_compound(const expr *e, dump *ctx)
{
	dump_desc_expr_newline(ctx, "compound assignment", e, 0);

	dump_printf_indent(ctx, 0, " %s%s=",
			e->assign_is_post ? "post-assignment " : "",
			op_to_str(e->bits.compoundop.op));

	if(e->bits.compoundop.upcast_ty){
		dump_printf_indent(ctx, 0, " upcast='%s'",
				type_to_str(e->bits.compoundop.upcast_ty));
	}

	dump_printf_indent(ctx, 0, "\n");

	dump_inc(ctx);
	dump_expr(e->lhs, ctx);
	dump_expr(e->rhs, ctx);
	dump_dec(ctx);
}

void mutate_expr_assign_compound(expr *e)
{
	e->freestanding = 1;
}

expr *expr_new_assign_compound(expr *to, expr *from, enum op_type op)
{
	expr *e = expr_new_wrapper(assign_compound);

	e->lhs = to;
	e->rhs = from;
	e->bits.compoundop.op = op;

	return e;
}

const out_val *gen_expr_style_assign_compound(const expr *e, out_ctx *octx)
{
	IGNORE_PRINTGEN(gen_expr(e->lhs->lhs, octx));
	stylef(" %s= ", op_to_str(e->bits.compoundop.op));
	IGNORE_PRINTGEN(gen_expr(e->rhs, octx));
	return NULL;
}
