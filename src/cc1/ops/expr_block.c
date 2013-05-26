#include "ops.h"
#include "expr_block.h"
#include "../out/lbl.h"
#include "../../util/dynarray.h"

const char *str_expr_block(void)
{
	return "block";
}

void fold_expr_block(expr *e, symtable *stab)
{
	/* add e->bits.block_args to symtable */
	symtab_add_args(e->code->symtab, e->bits.block_args, "block-function");

	/* prevent access to nested vars */
	e->code->symtab->parent = symtab_root(e->code->symtab);

	UCC_ASSERT(stmt_kind(e->code, code), "!code for block");
	fold_stmt(e->code);

	/*
	 * TODO:
	 * search e->code for expr_identifier,
	 * and it it's not in e->code->symtab or lower,
	 * add it as a block argument
	 *
	 * int i;
	 * ^{
	 *   i = 5;
	 * }();
	 */

	/*
	 * search for a return
	 * if none: void
	 * else the type of the first one we find
	 */

	if(e->bits.tref){
		/* just the return _type_, not (^)() qualified */
		e->tree_type = e->bits.tref;

	}else{
		stmt *r = NULL;

		stmt_walk(e->code, stmt_walk_first_return, NULL, &r);

		if(r && r->expr){
			e->tree_type = r->expr->tree_type;
		}else{
			e->tree_type = type_ref_cached_VOID();
		}
	}

	/* copied the type, now make it a (^)() */
	e->tree_type = type_ref_new_block(
			type_ref_new_func(e->tree_type, e->bits.block_args),
			qual_const
			);

	/* add the function to the global scope */
	{
		decl *df = decl_new();

		df->spel = out_label_block(curdecl_func->spel);
		e->bits.block_sym = sym_new_stab(symtab_root(stab), df, sym_global);

		df->is_definition = 1; /* necessary for code-gen */
		df->func_code = e->code;
		df->ref = e->tree_type;

		fold_decl(df, stab); /* funcarg folding + typedef/struct lookup, etc */
	}
}

void gen_expr_block(expr *e)
{
	out_push_lbl(e->bits.block_sym->decl->spel, 1);
}

void gen_expr_str_block(expr *e)
{
	idt_printf("block, type: %s, code:\n", type_ref_to_str(e->tree_type));
	gen_str_indent++;
	print_stmt(e->code);
	gen_str_indent--;
}

void gen_expr_style_block(expr *e)
{
	stylef("^%s", type_ref_to_str(e->tree_type));
	gen_stmt(e->code);
}

void mutate_expr_block(expr *e)
{
	(void)e;
}

expr *expr_new_block(type_ref *rt, funcargs *args, stmt *code)
{
	expr *e = expr_new_wrapper(block);
	e->bits.block_args = args;
	e->code = code;
	e->bits.tref = rt; /* return type if not null */
	return e;
}
