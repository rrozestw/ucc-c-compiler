#ifndef BLK_H
#define BLK_H

#include "forwards.h"
#include "../../util/where.h"

struct out_blk
{
	/* all blocks: */
	struct where locn;
	const char *desc;
	char *lbl;
	char **insns;

	out_blk **merge_preds;
	int flush_in_prog;

#define BLK_IS_MERGE(b) ( \
		b->merge_preds        \
		&& b->merge_preds[0]  \
		&& b->merge_preds[1])

	enum
	{
		BLK_UNINIT,
		BLK_TERMINAL, /* a ->nothing */
		BLK_NEXT_BLOCK, /* a ->b */
		BLK_NEXT_EXPR, /* a ->`expr` */
		BLK_COND, /* a ? ->b : ->c */
	} type;

	/* phi terminators: */
	const out_val *phi_val;

	union
	{
		/* nothing */

		/* ucond jump */
		out_blk *next;

		/* expr jump */
		const out_val *exp;

		/* cond */
		struct blk_cond
		{
			char *insn;
			out_blk *if_0_blk, *if_1_blk;
			char unlikely;
		} cond;
	} bits;
};

ucc_nonnull()
out_blk *out_blk_new_lbl(out_ctx *, const char *lbl, where *);

void blk_flushall(out_ctx *octx, out_blk *first, char *end_dbg_lbl);

void blk_terminate_condjmp(
		out_ctx *octx, char *condinsn,
		out_blk *bpass, out_blk *bfail,
		int unlikely);

void blk_terminate_jmp(out_blk *, char *jmpinsn);
void blk_terminate_undef(out_blk *);

#define blk_add_insn(blk, insn) \
	dynarray_add(&(blk)->insns, (insn))

#endif
