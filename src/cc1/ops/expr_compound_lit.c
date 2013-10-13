#include "ops.h"
#include "expr_compound_lit.h"
#include "../out/asm.h"
#include "../out/lbl.h"
#include "../decl_init.h"

#include "../stmt_ctx.h"

const char *str_expr_compound_lit(void)
{
	return "compound-lit";
}

void fold_expr_compound_lit(expr *e, symtable *stab)
{
	decl *d = e->bits.complit.decl;

	if(e->code)
		return; /* being called from fold_gen_init_assignment_base */

	/* must be set before the recursive fold_gen_init_assignment_base */
	e->tree_type = d->ref;

	if(!stab->parent){
		d->spel = out_label_data_store(0);
		d->store = store_static;
	}

	e->bits.complit.sym = sym_new_stab(
			stab, d, stab->parent ? sym_local : sym_global);

	/* fold the initialiser */
	UCC_ASSERT(d->init, "no init for comp.literal");

	decl_init_brace_up_fold(d, stab);

	/*
	 * update the type, for example if an array type has been completed
	 * this is done before folds, for array bounds checks
	 */
	e->tree_type = d->ref;

	if(stab->parent){
		/* create the code for assignemnts
		 *
		 * - we must create a nested scope,
		 *   otherwise any other decls in stab's scope will
		 *   be generated twice - once for the scope we're nested in (stab),
		 *   and again on our call to gen_stmt() in our gen function
		 */
		e->code = stmt_new_wrapper(code, symtab_new(stab));
		decl_init_create_assignments_base(d->init, d->ref, e, e->code);

		fold_stmt_code(e->code);
	}else{
		fold_decl_global_init(d, stab);
	}
}

static basic_blk *gen_expr_compound_lit_code(expr *e, basic_blk *bb)
{
	if(!e->expr_comp_lit_cgen){
		ICW("compound literal code-gen needs basic-block check");

		e->expr_comp_lit_cgen = 1;

		UCC_ASSERT(e->code->symtab->parent,
				"global compound initialiser tried for code");

		bb = gen_stmt(e->code, bb);
	}

	return bb;
}

basic_blk *gen_expr_compound_lit(expr *e, basic_blk *bb)
{
	/* allow (int){2}, but not (struct...){...} */
	fold_check_expr(e, FOLD_CHK_NO_ST_UN, "compound literal");

	bb = gen_expr_compound_lit_code(e, bb);

	out_push_sym_val(bb, e->bits.complit.sym);

	return bb;
}

static basic_blk *lea_expr_compound_lit(expr *e, basic_blk *bb)
{
	bb = gen_expr_compound_lit_code(e, bb);

	out_push_sym(bb, e->bits.complit.sym);

	return bb;
}

static void const_expr_compound_lit(expr *e, consty *k)
{
	decl *d = e->bits.complit.decl;

	if(decl_init_is_const(d->init, NULL)){
		k->type = CONST_ADDR_OR_NEED(d);
		k->bits.addr.is_lbl = 1;
		k->bits.addr.bits.lbl = d->spel;
		k->offset = 0;
	}else{
		k->type = CONST_NO;
	}
}

basic_blk *gen_expr_str_compound_lit(expr *e, basic_blk *bb)
{
	decl *const d = e->bits.complit.decl;

	if(e->op)
		goto out;

	e->op = 1;
	{
		idt_printf("(%s){\n", decl_to_str(d));

		gen_str_indent++;
		print_decl(d,
				PDECL_NONE         |
				PDECL_INDENT       |
				PDECL_NEWLINE      |
				PDECL_SYM_OFFSET   |
				PDECL_FUNC_DESCEND |
				PDECL_PINIT        |
				PDECL_SIZE         |
				PDECL_ATTR);
		gen_str_indent--;

		idt_printf("}\n");
		idt_printf("init code:\n");
		print_stmt(e->code);
	}
	e->op = 0;

out:
	return bb;
}

basic_blk *gen_expr_style_compound_lit(expr *e, basic_blk *bb)
{
	stylef("(%s)", type_ref_to_str(e->bits.complit.decl->ref));
	gen_style_dinit(e->bits.complit.decl->init);
	return bb;
}

void mutate_expr_compound_lit(expr *e)
{
	e->f_lea = lea_expr_compound_lit;
	e->f_const_fold = const_expr_compound_lit;
}

static decl *compound_lit_decl(type_ref *t, decl_init *init)
{
	decl *d = decl_new();

	d->ref = t;
	d->init = init;

	return d;
}

void expr_compound_lit_from_cast(expr *e, decl_init *init)
{
	e->bits.complit.decl = compound_lit_decl(e->bits.tref /* from cast */, init);

	expr_mutate_wrapper(e, compound_lit);
}

expr *expr_new_compound_lit(type_ref *t, decl_init *init)
{
	expr *e = expr_new_wrapper(compound_lit);
	e->bits.complit.decl = compound_lit_decl(t, init);
	return e;
}
