#include <stddef.h>

#include "../type_nav.h"
#include "../funcargs.h"

#include "out.h"
#include "val.h" /* for .t */

#define BUILTIN_USE_LIBC 1
#define LIBC_BYTE_LIMIT 16

static void out_memcpy_single(
		out_ctx *octx,
		const out_val **dst, const out_val **src)
{
	type *t1 = type_nav_btype(cc1_type_nav, type_intptr_t);

	out_val_retain(octx, *dst);
	out_val_retain(octx, *src);
	out_store(octx, *dst, out_deref(octx, *src));

	*dst = out_op(octx, op_plus, *dst, out_new_l(octx, t1, 1));
	*src = out_op(octx, op_plus, *src, out_new_l(octx, t1, 1));
}

static const out_val *out_memcpy_libc(
		out_ctx *octx,
		const out_val *dest, const out_val *src,
		unsigned long nbytes)
{
	type *voidty = type_nav_btype(cc1_type_nav, type_void);
	funcargs *fargs = funcargs_new();
	type *fnty_noptr = type_func_of(voidty, fargs, NULL);
	type *fnty_ptr = type_ptr_to(fnty_noptr);
	char *mangled = "_memcpy"; // FIXME: func_mangle("memcpy", fnty_noptr);

	const out_val *fn = out_new_lbl(octx, fnty_ptr, mangled, 0);
	const out_val *args[4] = { 0 };

	args[0] = dest;
	args[1] = src;

	args[2] = out_new_l(
			octx,
			type_nav_btype(cc1_type_nav, type_intptr_t),
			nbytes);

	return out_call(octx, fn, args, fnty_ptr);
}

static const out_val *out_memcpy_manual(
		out_ctx *octx,
		const out_val *dest, const out_val *src,
		unsigned long nbytes)
{
	size_t i = nbytes;
	type *tptr;
	unsigned tptr_sz;

	if(i > 0){
		tptr = type_ptr_to(type_nav_MAX_FOR(cc1_type_nav, nbytes));
		tptr_sz = type_size(tptr, NULL);
	}

	while(i > 0){
		/* as many copies as we can */
		dest = out_change_type(octx, dest, tptr);
		src = out_change_type(octx, src, tptr);

		while(i >= tptr_sz){
			i -= tptr_sz;
			out_memcpy_single(octx, &dest, &src);
		}

		if(i > 0){
			tptr_sz /= 2;
			tptr = type_ptr_to(type_nav_MAX_FOR(cc1_type_nav, tptr_sz));
		}
	}

	out_val_release(octx, src);
	return out_op(
			octx, op_minus,
			dest, out_new_l(octx, dest->t, nbytes));
}

const out_val *out_memcpy(
		out_ctx *octx,
		const out_val *dest, const out_val *src,
		unsigned long nbytes)
{
#warning TODO: merge this
	if(BUILTIN_USE_LIBC && nbytes > LIBC_BYTE_LIMIT){
		return out_memcpy_libc(octx, dest, src, nbytes);
	}else{
		return out_memcpy_manual(octx, dest, src, nbytes);
	}
}

static const out_val *out_memset_libc(
		out_ctx *octx,
		const out_val *dest,
		unsigned byte,
		unsigned long nbytes)
{
#warning TODO: merge call int() funcs
	type *voidty = type_nav_btype(cc1_type_nav, type_void);
	funcargs *fargs = funcargs_new();
	type *fnty_noptr = type_func_of(voidty, fargs, NULL);
	type *fnty_ptr = type_ptr_to(fnty_noptr);
	char *mangled = "_memset"; // FIXME: func_mangle("memset", fnty_noptr);

	const out_val *fn = out_new_lbl(octx, fnty_ptr, mangled, 0);
	const out_val *args[4] = { 0 };

	args[0] = dest;
	args[1] = out_new_l(octx,
			type_nav_btype(cc1_type_nav, type_uchar),
			byte);
	args[2] = out_new_l(octx,
			type_nav_btype(cc1_type_nav, type_intptr_t),
			nbytes);

	return out_call(octx, fn, args, fnty_ptr);
}

static const out_val *out_memset_manual(
		out_ctx *octx,
		const out_val *dest,
		unsigned byte,
		unsigned long bytes)
{
	size_t n, rem;
	unsigned i;
	type *tzero = type_nav_MAX_FOR(cc1_type_nav, bytes);

	type *textra, *textrap;
	const out_val *v_ptr = dest;
	const out_val *v_byte = out_new_l(octx,
			type_nav_btype(cc1_type_nav, type_uchar),
			byte);

	if(!tzero)
		tzero = type_nav_btype(cc1_type_nav, type_nchar);

	n   = bytes / type_size(tzero, NULL);
	rem = bytes % type_size(tzero, NULL);

	if((textra = rem ? type_nav_MAX_FOR(cc1_type_nav, rem) : NULL))
		textrap = type_ptr_to(textra);

	v_ptr = out_change_type(octx, v_ptr, type_ptr_to(tzero));

#ifdef MEMSET_VERBOSE
	out_comment("memset(%s, %d, %lu), using ptr<%s>, %lu steps",
			e->expr->f_str(),
			e->bits.builtin_memset.ch,
			e->bits.builtin_memset.len,
			type_to_str(tzero), n);
#endif

	for(i = 0; i < n; i++){
		const out_val *v_inc;

		/* *p = 0 */
		out_val_retain(octx, v_ptr);
		out_store(octx, v_ptr, out_val_retain(octx, v_byte));

		/* p++ (copied pointer) */
		v_inc = out_new_l(octx, type_nav_btype(cc1_type_nav, type_intptr_t), 1);

		v_ptr = out_op(octx, op_plus, v_ptr, v_inc);

		if(rem){
			/* need to zero a little more */
			v_ptr = out_change_type(octx, v_ptr, textrap);

			out_val_retain(octx, v_ptr);
			out_store(octx, v_ptr, out_val_retain(octx, v_byte));
		}
	}

	out_val_release(octx, v_byte);

	return out_op(
			octx, op_minus,
			v_ptr,
			out_new_l(
				octx,
				type_nav_btype(cc1_type_nav, type_intptr_t),
				bytes));
}

ucc_wur const out_val *out_memset(
		out_ctx *octx,
		const out_val *dest,
		unsigned byte,
		unsigned long nbytes)
{
	if(BUILTIN_USE_LIBC && nbytes > LIBC_BYTE_LIMIT){
		return out_memset_libc(octx, dest, byte, nbytes);
	}else{
		return out_memset_manual(octx, dest, byte, nbytes);
	}
}
