#include "ops.h"
#include "expr_assign.h"

const char *str_expr_assign()
{
	return "assign";
}

int expr_is_lvalue(expr *e, enum lvalue_opts opts)
{
	/*
	 * valid lvaluess:
	 *
	 *   x              = 5; // non-func identifier
	 *   *(expr)        = 5; // dereference
	 *   struct.member  = 5; // struct
	 *   struct->member = 5; // struct
	 *
	 * also can't be const, checked in fold_assign (since we allow const inits)
	 *
	 * order is important
	 */

	/* _lvalue_ addressing makes an exception for this */
	if(decl_is_func(e->tree_type))
		return 0;

	if(expr_kind(e, deref))
		return 1;

	if(expr_kind(e, struct))
		return 1;

	if(decl_is_array(e->tree_type))
		return 0;

	if(expr_kind(e, identifier))
		return e->tree_type->func_code ? opts & LVAL_ALLOW_FUNC : 1;

	return 0;
}

void fold_expr_assign(expr *e, symtable *stab)
{
	fold_inc_writes_if_sym(e->lhs, stab);

	fold_expr(e->lhs, stab);
	fold_expr(e->rhs, stab);

	if(expr_kind(e->lhs, identifier))
		e->lhs->sym->nreads--; /* cancel the read that fold_ident thinks it got */

	if(decl_is_void(e->rhs->tree_type))
		DIE_AT(&e->where, "assignment from void expression");

	if(!expr_is_lvalue(e->lhs, 0)){
		DIE_AT(&e->lhs->where, "not an lvalue (%s%s%s)",
				e->lhs->f_str(),
				expr_kind(e->lhs, op) ? " - " : "",
				expr_kind(e->lhs, op) ? op_to_str(e->lhs->op) : ""
			);
	}

	if(!e->assign_is_init && decl_is_const(e->lhs->tree_type))
		DIE_AT(&e->where, "can't modify const expression %s", e->lhs->f_str());


	e->tree_type = decl_copy(e->lhs->tree_type);

	/* type check */
	{
		char bufto[DECL_STATIC_BUFSIZ], buffrom[DECL_STATIC_BUFSIZ];

		fold_decl_equal(e->lhs->tree_type, e->rhs->tree_type,
				&e->where, WARN_ASSIGN_MISMATCH,
				"%s type mismatch: %s <-- %s",
				e->assign_is_init ? "initialisation" : "assignment",
				decl_to_str_r(bufto,   e->lhs->tree_type),
				decl_to_str_r(buffrom, e->rhs->tree_type));
	}


	/* do the typecheck after the equal-check, since the typecheck inserts casts */
	fold_insert_casts(e->lhs->tree_type, &e->rhs, stab, &e->where, "assignment");
}

void gen_expr_assign(expr *e, symtable *stab)
{
	/*if(decl_is_struct_or_union(e->tree_type))*/
	fold_disallow_st_un(e, "copy (TODO)"); /* yes this is meant to be in gen */

	UCC_ASSERT(!e->assign_is_post, "assign_is_post set for non-compound assign");

	/* optimisation: do this first, since rhs might also be a store */
	gen_expr(e->rhs, stab);
	lea_expr(e->lhs, stab);
	out_swap();

	out_store();
}

void gen_expr_str_assign(expr *e, symtable *stab)
{
	(void)stab;
	idt_printf("assignment, expr:\n");
	idt_printf("assign to:\n");
	gen_str_indent++;
	print_expr(e->lhs);
	gen_str_indent--;
	idt_printf("assign from:\n");
	gen_str_indent++;
	print_expr(e->rhs);
	gen_str_indent--;
}

void mutate_expr_assign(expr *e)
{
	e->freestanding = 1;
}

expr *expr_new_assign(expr *to, expr *from)
{
	expr *ass = expr_new_wrapper(assign);

	ass->lhs = to;
	ass->rhs = from;

	return ass;
}

void gen_expr_style_assign(expr *e, symtable *stab)
{ (void)e; (void)stab; /* TODO */ }
