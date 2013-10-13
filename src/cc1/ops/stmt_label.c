#include "ops.h"
#include "stmt_label.h"

const char *str_stmt_label()
{
	return "label";
}

void fold_stmt_label(stmt *s, stmt_fold_ctx_block *ctx)
{
	dynmap_set(
			ctx->func_ctx->gotos,
			s->bits.label,
			bb_label);

	fold_stmt(s->lhs, ctx); /* compound */
}

basic_blk *gen_stmt_label(stmt *s, basic_blk *bb)
{
	//out_label(bb, s->expr->bits.ident.spel);
	bb = gen_stmt(s->lhs, bb); /* the code-part of the compound statement */

	return bb;
}

basic_blk *style_stmt_label(stmt *s, basic_blk *bb)
{
	stylef("\n%s: ", s->expr->bits.ident.spel);
	bb = gen_stmt(s->lhs, bb);
	return bb;
}

int label_passable(stmt *s)
{
	return fold_passable(s->lhs);
}

void mutate_stmt_label(stmt *s)
{
	s->f_passable = label_passable;
}
