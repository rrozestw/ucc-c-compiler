#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../util/alloc.h"

#include "../type.h"
#include "../type_nav.h"
#include "../type_is.h"
#include "../pack.h"

#include "../cc1.h" /* cc1_mstack_align */

#include "val.h"
#include "out.h"
#include "virt.h"
#include "ctx.h"
#include "asm.h"
#include "impl.h"

#define TODO() \
	fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, __func__)

void out_flush_volatile(out_ctx *octx, out_val *val)
{
	out_val_consume(octx, v_to_reg(octx, val));
}

int v_is_const_reg(out_val *v)
{
	return v->type == V_REG
		&& impl_reg_frame_const(&v->bits.regoff.reg);
}

out_val *v_to_stack_mem(out_ctx *octx, out_val *vp, long stack_pos)
{
	out_val *store = v_new_sp3(octx, NULL, vp->t, stack_pos);

	vp = v_to(octx, vp, TO_CONST | TO_REG);

	out_val_retain(octx, vp);

	out_store(octx, store, vp);

	return vp;
}

void v_reg_to_stack(
		out_ctx *octx,
		const struct vreg *vr,
		type *ty, long where)
{
	out_val *reg = v_new_reg(octx, NULL, ty, vr);
	out_flush_volatile(octx,
			v_to_stack_mem(octx, reg, -where));
}

static int v_in(out_val *vp, enum vto to)
{
	switch(vp->type){
		case V_FLAG:
			break;

		case V_CONST_I:
		case V_CONST_F:
			return !!(to & TO_CONST);

		case V_REG:
			return (to & TO_REG) && vp->bits.regoff.offset == 0;

		case V_REG_SAVE:
		case V_LBL:
			return !!(to & TO_MEM);
	}

	return 0;
}

static out_val *v_save_reg(out_ctx *octx, out_val *vp)
{
	assert(vp->type == V_REG && "not reg");

	out_comment("register spill:");

	v_alloc_stack(octx, type_size(vp->t, NULL), "save reg");

	return v_to_stack_mem(
			octx, vp,
			-octx->stack_sz);
}

out_val *v_to(out_ctx *octx, out_val *vp, enum vto loc)
{
	if(v_in(vp, loc))
		return vp;

	/* TO_CONST can't be done - it should already be const,
	 * or another option should be chosen */

	/* go for register first */
	if(loc & TO_REG){
		return v_to_reg(octx, vp);
	}

	if(loc & TO_MEM){
		vp = v_to_reg(octx, vp);
		return v_save_reg(octx, vp);
	}

	assert(0 && "can't satisfy v_to");
}

static ucc_wur out_val *v_freeup_regp(out_ctx *octx, out_val *vp)
{
	struct vreg r;
	int got_reg;

	assert(vp->type == V_REG && "not reg");

	/* attempt to save to a register first */
	got_reg = v_unused_reg(octx, 0, vp->bits.regoff.reg.is_float, &r);

	if(got_reg){
		/* move 'vp' into the fresh reg */
		impl_reg_cp(octx, vp, &r);

		/* change vp's register to 'r', so that vp's original register is free */
		vp->bits.regoff.reg = r;

		return vp;

	}else{
		/* no free registers, save this one to the stack and mutate vp */
		return v_save_reg(octx, vp);
		/* vp is now a stack value */
	}
}

static out_val *v_find_reg(out_ctx *octx, const struct vreg *reg)
{
	out_val_list *i;

	for(i = octx->val_head; i; i = i->next){
		out_val *v = &i->val;
		if(v->type == V_REG && vreg_eq(&v->bits.regoff.reg, reg))
			return v;
	}

	return NULL;
}

void v_freeup_reg(out_ctx *octx, const struct vreg *r)
{
	out_val *v = v_find_reg(octx, r);

	if(v)
		out_val_consume(octx, v_freeup_regp(octx, v));
}

int v_unused_reg(
		out_ctx *octx,
		int stack_as_backup, int fp,
		struct vreg *out)
{
	unsigned char used[sizeof(octx->reserved_regs)];
	out_val_list *it;
	out_val *first;
	int i;

	memcpy(used, octx->reserved_regs, sizeof used);
	first = NULL;

	for(it = octx->val_head; it; it = it->next){
		out_val *this = &it->val;
		if(this->retains
		&& this->type == V_REG
		&& this->bits.regoff.reg.is_float == fp)
		{
			if(!first)
				first = this;
			used[impl_reg_to_scratch(&this->bits.regoff.reg)] = 1;
		}
	}

	for(i = 0; i < (fp ? N_SCRATCH_REGS_F : N_SCRATCH_REGS_I); i++)
		if(!used[i]){
			impl_scratch_to_reg(i, out);
			out->is_float = fp;
			return 1;
		}

	if(stack_as_backup){
		/* no free regs, move `first` to the stack and claim its reg */
		*out = first->bits.regoff.reg;

		out_val_consume(octx, v_freeup_regp(octx, first));

		return 1;
	}
	return 0;
}

out_val *v_to_reg_given(
		out_ctx *octx, out_val *from,
		const struct vreg *given)
{
	return impl_load(octx, from, given);
}

out_val *v_to_reg_out(out_ctx *octx, out_val *conv, struct vreg *out)
{
	if(conv->type != V_REG){
		struct vreg chosen;
		if(!out)
			out = &chosen;

		/* get a register */
		v_unused_reg(octx, 1, type_is_floating(conv->t), out);

		/* load into register */
		return v_to_reg_given(octx, conv, out);

	}else{
		if(out)
			memcpy_safe(out, &conv->bits.regoff.reg);

		return conv;
	}
}

out_val *v_to_reg(out_ctx *octx, out_val *conv)
{
	return v_to_reg_out(octx, conv, NULL);
}

static int val_present(out_val *v, out_val **ignores)
{
	out_val **i;
	for(i = ignores; i && *i; i++)
		if(v == *i)
			return 1;
	return 0;
}

void v_save_regs(out_ctx *octx, type *func_ty, out_val *ignores[])
{
	/* save all registers except callee save */
	out_val_list *l;

	/* go backwards in case the list is added to */
	for(l = octx->val_tail; l; l = l->prev){
		out_val *v = &l->val;
		int save = 0;

		if(v->retains == 0)
			continue;

		switch(v->type){
			case V_REG_SAVE:
			case V_REG:
				if(val_present(v, ignores)){
					/* don't save */
				}else if(!impl_reg_savable(&v->bits.regoff.reg)){
					/* don't save stack references */
					if(fopt_mode & FOPT_VERBOSE_ASM)
						out_comment("not saving const-reg %d", v->bits.regoff.reg.idx);

				}else if(func_ty
				&& impl_reg_is_callee_save(&v->bits.regoff.reg, func_ty))
				{
					/* only comment for non-const regs */
					out_comment("not saving reg %d - callee save",
							v->bits.regoff.reg.idx);

				}else{
					out_comment("saving register %d", v->bits.regoff.reg.idx);
					save = 1;
				}
				break;

			case V_CONST_I:
			case V_CONST_F:
			case V_LBL:
				save = 0;
				break;

			case V_FLAG:
				assert(v->retains == 1 && "v_save_regs(): retained v");
				v = v_to_reg(octx, v);
				save = 1;
		}

		if(save)
			v_save_reg(octx, v);
	}
}

void v_stack_adj(out_ctx *octx, unsigned amt, int sub)
{
	out_op(
			octx, sub ? op_minus : op_plus,
			out_new_l(
				octx,
				type_nav_btype(cc1_type_nav, type_intptr_t),
				amt),
			v_new_sp(octx, NULL));
}

unsigned v_alloc_stack2(
		out_ctx *octx,
		const unsigned sz_initial, int noop, const char *desc)
{
	unsigned sz_rounded = sz_initial;

	if(sz_initial){
		/* must be a multiple of mstack_align.
		 * assume stack_sz is aligned, and just
		 * align what we add to it
		 */
		sz_rounded = pack_to_align(sz_initial, cc1_mstack_align);

		/* if it changed, we need to realign the stack */
		if(!noop || sz_rounded != sz_initial){
			unsigned to_alloc;

			if(!noop){
				to_alloc = sz_rounded; /* the whole hog */
			}else{
				/* the extra we need to align by */
				to_alloc = sz_rounded - sz_initial;
			}

			if(fopt_mode & FOPT_VERBOSE_ASM){
				out_comment("stack alignment for %s (%u -> %u)",
						desc, octx->stack_sz, octx->stack_sz + sz_rounded);
				out_comment("alloc_n by %u (-> %u), padding with %u",
						sz_initial, octx->stack_sz + sz_initial,
						sz_rounded - sz_initial);
			}

			v_stack_adj(octx, to_alloc, 1);
		}

		octx->stack_sz += sz_rounded;
	}

	return sz_rounded;
}

unsigned v_alloc_stack_n(out_ctx *octx, unsigned sz, const char *desc)
{
	return v_alloc_stack2(octx, sz, 1, desc);
}

unsigned v_alloc_stack(out_ctx *octx, unsigned sz, const char *desc)
{
	return v_alloc_stack2(octx, sz, 0, desc);
}

unsigned v_stack_align(out_ctx *octx, unsigned const align, int force_mask)
{
	if(force_mask || (octx->stack_sz & (align - 1))){
		type *const ty = type_nav_btype(cc1_type_nav, type_intptr_t);
		const unsigned new_sz = pack_to_align(octx->stack_sz, align);
		const unsigned added = new_sz - octx->stack_sz;
		out_val *sp = v_new_sp(octx, NULL);

		sp = out_op(
				octx, op_minus,
				sp,
				out_new_l(octx, ty, added));

		octx->stack_sz = new_sz;

		if(force_mask){
			sp = out_op(octx, op_and, sp, out_new_l(octx, ty, align - 1));
		}
		out_comment("stack aligned to %u bytes", align);
		return added;
	}
	return 0;
}

void v_dealloc_stack(out_ctx *octx, unsigned sz)
{
	/* callers should've snapshotted the stack previously
	 * and be calling us with said snapshot value
	 */
	assert((sz & (cc1_mstack_align - 1)) == 0
			&& "can't dealloc by a non-stack-align amount");

	v_stack_adj(octx, sz, 0);

	octx->stack_sz -= sz;
}

enum flag_cmp v_inv_cmp(enum flag_cmp cmp, int invert_eq)
{
	switch(cmp){
#define OPPOSITE2(from, to)    \
		case flag_ ## from:        \
			return flag_ ## to; \

#define OPPOSITE(from, to) \
		OPPOSITE2(from, to);   \
		OPPOSITE2(to, from)

		OPPOSITE(le, gt);
		OPPOSITE(lt, ge);
		OPPOSITE(overflow, no_overflow);

		/*OPPOSITE(z, nz);
		OPPOSITE(nz, z);*/
#undef OPPOSITE
#undef OPPOSITE2

		case flag_eq:
		case flag_ne:
			if(invert_eq)
				return (cmp == flag_eq ? flag_ne : flag_eq);
			return cmp;
	}
	assert(0 && "invalid op");
}
