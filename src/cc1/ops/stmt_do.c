#include <stdlib.h>

#include "ops.h"
#include "stmt_do.h"

const char *str_stmt_do()
{
	return "do";
}

void fold_stmt_do(stmt *s)
{
	fold_stmt_while(s);
}

void gen_stmt_do(stmt *s)
{
	char *begin = asm_label_flow("do_start");

	asm_label(begin);
	gen_stmt(s->lhs);

	asm_label(s->lbl_continue);
	gen_expr(s->expr, s->symtab);
	asm_pop(s->expr->tree_type, ASM_REG_A);
	ASM_TEST(s->expr->tree_type, ASM_REG_A);
	asm_jmp_if_zero(1, begin);

	asm_label(s->lbl_break);

	free(begin);
}

void mutate_stmt_do(stmt *s)
{
	s->f_passable = while_passable;
}
