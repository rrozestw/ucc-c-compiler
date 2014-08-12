#include "ops.h"
#include "expr_comma.h"
#include "../type_is.h"

const char *str_expr_comma()
{
	return "comma";
}

static void fold_const_expr_comma(expr *e, consty *k)
{
	consty klhs;

	const_fold(e->lhs, &klhs);
	const_fold(e->rhs, k);

	/* klhs.nonstandard_const || k->nonstandard_const
	 * ^ doesn't matter - comma expressions are nonstandard-const anyway
	 */
	k->nonstandard_const = e;

	if(!CONST_AT_COMPILE_TIME(klhs.type))
		k->type = CONST_NO;
}

static const out_val *comma_lea(expr *e, out_ctx *octx)
{
	out_val_consume(octx, gen_maybe_struct_expr(e->lhs, octx));
	return lea_expr(e->rhs, octx);
}

void fold_expr_comma(expr *e, symtable *stab)
{
	FOLD_EXPR(e->lhs, stab);
	fold_check_expr(
			e->lhs,
			FOLD_CHK_ALLOW_VOID,
			"comma-expr");

	FOLD_EXPR(e->rhs, stab);
	fold_check_expr(
			e->rhs,
			FOLD_CHK_ALLOW_VOID,
			"comma-expr");

	e->tree_type = e->rhs->tree_type;

	if(!e->lhs->freestanding && !type_is_void(e->lhs->tree_type))
		warn_at(&e->lhs->where, "left hand side of comma is unused");

	e->freestanding = e->rhs->freestanding;

	if(e->rhs->f_lea){
		e->f_lea = comma_lea;
		e->lvalue_internal = 1;
	}
}

const out_val *gen_expr_comma(expr *e, out_ctx *octx)
{
	/* attempt lea, don't want full struct dereference */
	out_val_consume(octx, gen_maybe_struct_expr(e->lhs, octx));

	return gen_expr(e->rhs, octx);
}

const out_val *gen_expr_str_comma(expr *e, out_ctx *octx)
{
	idt_printf("comma expression\n");
	idt_printf("comma lhs:\n");
	gen_str_indent++;
	print_expr(e->lhs);
	gen_str_indent--;
	idt_printf("comma rhs:\n");
	gen_str_indent++;
	print_expr(e->rhs);
	gen_str_indent--;
	UNUSED_OCTX();
}

expr *expr_new_comma2(expr *lhs, expr *rhs)
{
	expr *e = expr_new_comma();
	e->lhs = lhs, e->rhs = rhs;
	return e;
}

void mutate_expr_comma(expr *e)
{
	e->f_const_fold = fold_const_expr_comma;
}

const out_val *gen_expr_style_comma(expr *e, out_ctx *octx)
{
	IGNORE_PRINTGEN(gen_expr(e->lhs, octx));
	stylef(", ");
	return gen_expr(e->rhs, octx);
}
