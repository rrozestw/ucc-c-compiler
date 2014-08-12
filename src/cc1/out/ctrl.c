#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "../../util/dynarray.h"

#include "forwards.h"
#include "blk.h"

#include "out.h" /* this file defs */
#include "asm.h"
#include "val.h"
#include "impl.h"
#include "ctx.h"
#include "virt.h"

void out_ctrl_branch(
		out_ctx *octx,
		const out_val *cond,
		out_blk *if_true, out_blk *if_false)
{
	impl_branch(octx, cond, if_true, if_false);
	out_val_consume(octx, cond);

	out_current_blk(octx, if_true);
}

void out_ctrl_end_ret(out_ctx *octx, const out_val *ret, type *ty)
{
	if(ret)
		impl_to_retreg(octx, ret, ty);
	out_ctrl_transfer(octx, octx->epilogue_blk, NULL, NULL);
	octx->current_blk = NULL;
}

void out_ctrl_end_undefined(out_ctx *octx)
{
	impl_undefined(octx);
	octx->current_blk = NULL;
}

const out_val *out_ctrl_merge(out_ctx *octx, out_blk *from_a, out_blk *from_b)
{
	out_blk *const saved_current_blk = octx->current_blk;
	type *ty;
	struct vreg merge_reg;

	assert(from_a->phi_val && from_b->phi_val);

	ty = from_a->phi_val->t;

	/* need them both in a register */
	out_current_blk(octx, from_a);
	{
		/* apply_offset - prevent merge_reg being a base pointer */
		const out_val *regged =
			v_reg_apply_offset(octx,
					v_to_reg_out(
						octx,
						from_a->phi_val,
						&merge_reg));

		/* apply_offset may move to another reg */
		memcpy_safe(&merge_reg, &regged->bits.regoff.reg);

		out_flush_volatile(octx, regged);
	}
	out_current_blk(octx, from_b);
	{
		const out_val *regged = v_to_reg_given(octx, from_b->phi_val, &merge_reg);
		out_flush_volatile(octx, regged);
	}

	out_current_blk(octx, saved_current_blk);

	return v_new_reg(octx, NULL, ty, &merge_reg);
}

void out_current_blk(out_ctx *octx, out_blk *new_blk)
{
	octx->last_used_blk = new_blk;

	octx->current_blk = new_blk;
}

void out_ctrl_transfer(out_ctx *octx, out_blk *to,
		const out_val *phi /* optional */, out_blk **mergee)
{
	out_blk *from = octx->current_blk;

	assert(!!phi == !!mergee);

	if(mergee)
		*mergee = from;

	/* always add a merge_pred - this means we can generate
	 * terminating branches before their merge block, even though
	 * they don't merge to it (instead of generating them after ret)
	 */
	dynarray_add(&to->merge_preds, from ? from : octx->last_used_blk);

	if(!from){
		/* we're transferring from unreachable code, ignore */
		if(phi)
			out_val_consume(octx, phi);
		return;
	}

	assert(!from->phi_val);
	from->phi_val = phi;

	assert(from->type == BLK_UNINIT);
	from->type = BLK_NEXT_BLOCK;
	from->bits.next = to;

	octx->current_blk = NULL;
}

void out_ctrl_transfer_make_current(out_ctx *octx, out_blk *to)
{
	out_ctrl_transfer(octx, to, NULL, NULL);
	out_current_blk(octx, to);
}

void out_ctrl_transfer_exp(out_ctx *octx, const out_val *addr)
{
	assert(addr->retains == 1); /* don't want this changing under us */

	impl_jmp_expr(octx, addr); /* must jump now, while we have octx */

	if(octx->current_blk){
		octx->current_blk->type = BLK_NEXT_EXPR;
		octx->current_blk->bits.exp = addr;
	}

	octx->current_blk = NULL;
}
