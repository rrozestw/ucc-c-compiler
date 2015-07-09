#include <stddef.h>

#include "../type_nav.h"
#include "../funcargs.h"

#include "out.h"
#include "val.h" /* for .t */

#define BUILTIN_USE_LIBC 1

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

	args[0] = out_new_l(
			octx,
			type_nav_btype(cc1_type_nav, type_intptr_t),
			nbytes);

	args[1] = dest;
	args[2] = src;

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
	if(BUILTIN_USE_LIBC){
		return out_memcpy_libc(octx, dest, src, nbytes);
	}else{
		return out_memcpy_manual(octx, dest, src, nbytes);
	}
}
