#include "ops.h"
#include "stmt_return.h"

const char *str_stmt_return()
{
	return "return";
}

void fold_stmt_return(stmt *s)
{
	const int void_func = type_ref_is_void(curdecl_ref_func_called);

	if(s->expr){
		FOLD_EXPR(s->expr, s->symtab);
		fold_need_expr(s->expr, "return", 0);

		fold_type_ref_equal(curdecl_ref_func_called, s->expr->tree_type,
				&s->where, WARN_RETURN_TYPE, 0,
				"mismatching return type for %s (%R <-- %R)",
				curdecl_func->spel,
				curdecl_ref_func_called, s->expr->tree_type);

		if(void_func){
			cc1_warn_at(&s->where, 0, 1, WARN_RETURN_TYPE,
					"return with a value in void function %s", curdecl_func->spel);
		}else{
			fold_insert_casts(curdecl_ref_func_called, &s->expr, s->symtab, &s->expr->where, "return");
		}

	}else if(!void_func){
		cc1_warn_at(&s->where, 0, 1, WARN_RETURN_TYPE,
				"empty return in non-void function %s", curdecl_func->spel);

	}
}

void gen_stmt_return(stmt *s)
{
	if(s->expr){
		gen_expr(s->expr, s->symtab);
		out_pop_func_ret(s->expr->tree_type);
		out_comment("return");
	}
	out_push_lbl(curfunc_lblfin, 0);
	out_jmp();
}

void mutate_stmt_return(stmt *s)
{
	s->f_passable = fold_passable_no;
}
