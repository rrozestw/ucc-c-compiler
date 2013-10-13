#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "defs.h"
#include "../util/util.h"
#include "data_structs.h"
#include "cc1.h"
#include "fold.h"
#include "fold_sym.h"
#include "sym.h"
#include "../util/platform.h"
#include "const.h"
#include "../util/alloc.h"
#include "../util/dynarray.h"
#include "../util/dynmap.h"
#include "sue.h"
#include "decl.h"
#include "decl_init.h"
#include "pack.h"
#include "funcargs.h"
#include "out/lbl.h"

#include "stmt_ctx.h"

decl     *curdecl_func;
type_ref *curdecl_ref_func_called; /* for funcargs-local labels and return type-checking */
int fold_had_error;

void fold_insert_casts(type_ref *tlhs, expr **prhs, symtable *stab)
{
	/* insert a cast: rhs -> lhs */
	expr *cast = expr_new_cast(tlhs, 1);

	cast->expr = *prhs;
	*prhs = cast;

	/* need to fold the cast again - mainly for "loss of precision" warning */
	fold_expr_cast_descend(cast, stab, 0);
}

int fold_type_chk_warn(
		type_ref *lhs, type_ref *rhs,
		where *w, const char *desc)
{
	int error = 1;

	switch(type_ref_cmp(lhs, rhs, 0)){
		case TYPE_CONVERTIBLE_IMPLICIT:
			return 1;
		case TYPE_EQUAL:
			break;

		case TYPE_QUAL_LOSS:
		case TYPE_CONVERTIBLE_EXPLICIT:
			error = 0;

		case TYPE_NOT_EQUAL:
		{
			char buf[TYPE_REF_STATIC_BUFSIZ];
			char wbuf[WHERE_BUF_SIZ];

			(error ? warn_at_print_error : warn_at)(
					w,
					"mismatching types, %s:\n%s: note: '%s' vs '%s'",
					desc, where_str_r(wbuf, w),
					type_ref_to_str_r(buf, lhs),
					type_ref_to_str(       rhs));

			if(error){
				fold_had_error = 1;
				return 0; /* don't cast - we don't want duplicate errors */
			}
			return 1; /* need cast */
		}
	}
	return 0;
}

void fold_type_chk_and_cast(
		type_ref *lhs, expr **prhs,
		symtable *stab, where *w,
		const char *desc)
{
#if 0
	int strict = 0;

	/* stronger checks for blocks, functions and (non-void) pointers */
	if(type_ref_is_nonvoid_ptr(a)
	&& type_ref_is_nonvoid_ptr(b))
	{
		strict = 1;
	}
	else
	if(type_ref_is(a, type_ref_block)
	|| type_ref_is(b, type_ref_block)
	|| type_ref_is(a, type_ref_func)
	|| type_ref_is(b, type_ref_func))
	{
		strict = 1;
	}
#endif

	if(fold_type_chk_warn(lhs, (*prhs)->tree_type, w, desc))
		fold_insert_casts(lhs, prhs, stab);
}

void fold_check_restrict(expr *lhs, expr *rhs, const char *desc, where *w)
{
	/* restrict operation checks */
	const enum type_qualifier ql = type_ref_qual(lhs->tree_type),
				                    qr = type_ref_qual(rhs->tree_type);

	if((ql & qual_restrict) && (qr & qual_restrict))
		warn_at(w, "restrict pointers in %s", desc);
}

sym *fold_inc_writes_if_sym(expr *e, symtable *stab)
{
	if(expr_kind(e, identifier)){
		sym *sym = symtab_search(stab, e->bits.ident.spel);

		if(sym){
			sym->nwrites++;
			return sym;
		}
	}

	return NULL;
}

void fold_expr(expr *e, symtable *stab)
{
	if(e->tree_type)
		return;

	EOF_WHERE(&e->where, e->f_fold(e, stab));

	UCC_ASSERT(e->tree_type, "no tree_type after fold (%s)", e->f_str());
}

static expr *fold_expr_lval2rval(expr *e, symtable *stab)
{
	fold_expr(e, stab);

	if(expr_is_lvalue(e)){
		e = expr_new_cast_rval(e);
		fold_expr_cast_descend(e, stab, 0);
	}

	return e;
}

expr *fold_expr_decay(expr *e, symtable *stab)
{
	/* perform array decay and pointer decay */
	type_ref *r;
	expr *imp_cast = NULL;

	e = fold_expr_lval2rval(e, stab);

	r = e->tree_type;

	EOF_WHERE(&e->where,
			type_ref *decayed = type_ref_decay(r);

			if(decayed != r)
				imp_cast = expr_new_cast(decayed, 1);
		);

	if(imp_cast){
		imp_cast->expr = e;
		fold_expr_cast_descend(imp_cast, stab, 0);
		e = imp_cast;
	}

	return e;
}

static void fold_calling_conv(type_ref *r)
{
	enum calling_conv conv;
	decl_attr *found;

	/* don't currently check for a second one... */
	found = type_attr_present(r, attr_call_conv);

	if(found){
		conv = found->bits.conv;
	}else{
		static int init = 0;
		static enum calling_conv def_conv;

		if(!init){
			init = 1;

			if(platform_32bit()){
				def_conv = conv_cdecl;
			}else{
				/* 64-bit - MS or SYSV */
				if(platform_sys() == PLATFORM_CYGWIN)
					def_conv = conv_x64_ms;
				else
					def_conv = conv_x64_sysv;
			}
		}

		conv = def_conv;
	}

	type_ref_funcargs(r)->conv = conv;
}

void fold_type_ref(type_ref *r, type_ref *parent, symtable *stab)
{
	enum type_qualifier q_to_check = qual_none;

	if(!r || r->folded)
		return;

	r->folded = 1;

	switch(r->type){
		case type_ref_array:
			if(type_ref_is(r->ref, type_ref_func))
				die_at(&r->where, "array of functions");

			if(r->bits.array.size){
				consty k;

				FOLD_EXPR(r->bits.array.size, stab);
				const_fold(r->bits.array.size, &k);

				if(k.type != CONST_NUM)
					die_at(&r->where, "not a constant for array size");
				else if(K_FLOATING(k.bits.num))
					die_at(&r->where, "not an integral array size");
				else if((sintegral_t)k.bits.num.val.i < 0)
					die_at(&r->where, "negative array size");
				/* allow zero length arrays */
			}
			break;

		case type_ref_func:
			if(type_ref_is(r->ref, type_ref_func))
				die_at(&r->where, "function returning a function");

			if(type_ref_is(parent, type_ref_ptr)
			&& (type_ref_qual(parent) & qual_restrict))
			{
				die_at(&r->where, "restrict qualified function pointer");
			}

			symtab_fold_decls_sues(r->bits.func.arg_scope);
			fold_funcargs(r->bits.func.args, r->bits.func.arg_scope, r);
			fold_calling_conv(r);
			break;

		case type_ref_block:
			if(!type_ref_is(r->ref, type_ref_func))
				die_at(&r->where, "invalid block pointer - function required (got %s)",
						type_ref_to_str(r->ref));

			/*q_to_check = r->bits.block.qual; - allowed */
			break;

		case type_ref_cast:
			if(!r->bits.cast.is_signed_cast)
				q_to_check = type_ref_qual(r);
			break;

		case type_ref_ptr:
			/*q_to_check = r->bits.qual; - allowed */
			break;

		case type_ref_type:
			if(stab->are_params){
				/* check if we're a new struct/union decl */
				struct_union_enum_st *sue = type_ref_is_s_or_u(r);

				if(sue){
					struct_union_enum_st *above = sue_find_descend(
							stab->parent, sue->spel, NULL);

					if(!above){
						warn_at(&r->where,
								"declaration of '%s %s' only visible inside function",
								sue_str(sue), sue->spel);
					}
				}
			}
			break;

		case type_ref_tdef:
		{
			expr *p_expr = r->bits.tdef.type_of;

			/* q_to_check = TODO */
			fold_expr_no_decay(p_expr, stab);

			if(r->bits.tdef.decl)
				fold_decl(r->bits.tdef.decl, stab, NULL);

			break;
		}
	}

	/*
	 * now we've folded, check for restrict
	 * since typedef int *intptr; intptr restrict a; is valid
	 */
	if(q_to_check & qual_restrict)
		warn_at(&r->where, "restrict on non-pointer type '%s'", type_ref_to_str(r));

	fold_type_ref(r->ref, r, stab);
}

static int fold_align(int al, int min, int max, where *w)
{
	/* allow zero */
	if(al & (al - 1))
		die_at(w, "alignment %d isn't a power of 2", al);

	UCC_ASSERT(al > 0, "zero align");
	if(al < min)
		die_at(w,
				"can't reduce alignment (%d -> %d)",
				min, al);

	if(al > max)
		max = al;
	return max;
}

static void fold_func_attr(decl *d)
{
	funcargs *fa = type_ref_funcargs(d->ref);

	if(decl_attr_present(d, attr_sentinel) && !fa->variadic)
		warn_at(&d->where, "variadic function required for sentinel check");
}

static void fold_decl_add_sym(decl *d, symtable *stab)
{
	/* must be before fold*, since sym lookups are done */
	if(d->sym){
		/* arg */
		UCC_ASSERT(d->sym->type != sym_local || !d->spel /* anon sym, e.g. strk */,
				"sym (type %d) \"%s\" given symbol too early",
				d->sym->type, d->spel);
	}else{
		enum sym_type ty;

		if(stab->are_params)
			ty = sym_arg;
		else
			ty = !stab->parent || decl_store_static_or_extern(d->store)
				? sym_global : sym_local;

		d->sym = sym_new(d, ty);
	}
}

void fold_decl(decl *d, symtable *stab, stmt **pinit_code)
{
#define inits (*pinit_code)
	/* this is called from wherever we can define a
	 * struct/union/enum,
	 * e.g. a code-block (explicit or implicit),
	 *      global scope
	 * and an if/switch/while statement: if((struct A { int i; } *)0)...
	 * an argument list/type_ref::func: f(struct A { int i, j; } *p, ...)
	 */

	decl_attr *attrib = NULL;
	int can_align = 1;

	if(d->folded)
		return;
	d->folded = 1;

	fold_type_ref(d->ref, NULL, stab);

	if(d->spel)
		fold_decl_add_sym(d, stab);

#if 0
	/* if we have a type and it's incomplete, error */
	no - only on use
	if(!type_ref_is_complete(d->ref))
		die_at(&d->where, "use of incomplete type - %s (%s)", d->spel, decl_to_str(d));
#endif

	if(d->field_width){
		consty k;

		FOLD_EXPR(d->field_width, stab);
		const_fold(d->field_width, &k);

		if(k.type != CONST_NUM)
			die_at(&d->where, "constant expression required for field width");
		if(K_FLOATING(k.bits.num))
			die_at(&d->where, "integral expression required for field width");

		if((sintegral_t)k.bits.num.val.i < 0)
			die_at(&d->where, "field width must be positive");

		if(k.bits.num.val.i == 0){
			/* allow anonymous 0-width bitfields
			 * we align the next bitfield to a boundary
			 */
			if(d->spel)
				die_at(&d->where,
						"none-anonymous bitfield \"%s\" with 0-width",
						d->spel);
		}else{
			const unsigned max = CHAR_BIT * type_ref_size(d->ref, &d->where);
			if(k.bits.num.val.i > max){
				die_at(&d->where,
						"bitfield too large for \"%s\" (%u bits)",
						decl_to_str(d), max);
			}
		}

		if(!type_ref_is_integral(d->ref))
			die_at(&d->where, "field width on non-integral field %s",
					decl_to_str(d));

		/* FIXME: only warn if "int" specified,
		 * i.e. detect explicit signed/unsigned */
		if(k.bits.num.val.i == 1 && type_ref_is_signed(d->ref))
			warn_at(&d->where, "1-bit signed field \"%s\" takes values -1 and 0",
					decl_to_str(d));

		can_align = 0;
	}

	/* allow:
	 *   register int (*f)();
	 * disallow:
	 *   register int   f();
	 *   register int  *f();
	 */
	if(DECL_IS_FUNC(d)){
		switch(d->store & STORE_MASK_STORE){
			case store_register:
			case store_auto:
				die_at(&d->where, "%s storage for function", decl_store_to_str(d->store));
		}

		if(stab->parent){
			if(d->func_code)
				die_at(&d->func_code->where, "nested function %s", d->spel);
			else if((d->store & STORE_MASK_STORE) == store_static)
				die_at(&d->where, "block-scoped function cannot have static storage");
		}

		can_align = 0;

		fold_func_attr(d);

	}else if((d->store & STORE_MASK_EXTRA) == store_inline){
		warn_at(&d->where, "inline on non-function");
	}

	if(d->align || (attrib = decl_attr_present(d, attr_aligned))){
		const int tal = type_ref_align(d->ref, &d->where);

		struct decl_align *i;
		int max_al = 0;

		if((d->store & STORE_MASK_STORE) == store_register)
			can_align = 0;

		if(!can_align)
			die_at(&d->where, "can't align %s", decl_to_str(d));

		for(i = d->align; i; i = i->next){
			int al;

			if(i->as_int){
				consty k;

				const_fold(
						FOLD_EXPR(i->bits.align_intk, stab),
						&k);

				if(k.type != CONST_NUM)
					die_at(&d->where, "alignment must be a constant");
				if(K_FLOATING(k.bits.num))
					die_at(&d->where, "non-integral alignment");

				al = k.bits.num.val.i;
			}else{
				type_ref *ty = i->bits.align_ty;
				UCC_ASSERT(ty, "no type");
				fold_type_ref(ty, NULL, stab);
				al = type_ref_align(ty, &d->where);
			}

			if(al == 0)
				al = decl_size(d);
			max_al = fold_align(al, tal, max_al, &d->where);
		}

		if(attrib){
			unsigned long al;

			if(attrib->bits.align){
				consty k;

				FOLD_EXPR(attrib->bits.align, stab);
				const_fold(attrib->bits.align, &k);

				if(k.type != CONST_NUM || !K_INTEGRAL(k.bits.num))
					die_at(&attrib->where, "aligned attribute not reducible to integer constant");
				al = k.bits.num.val.i;
			}else{
				al = platform_align_max();
			}

			max_al = fold_align(al, tal, max_al, &attrib->where);
			if(!d->align)
				d->align = umalloc(sizeof *d->align);
		}

		d->align->resolved = max_al;
	}

	if(d->init){
		const int is_static_init =
			(d->store & STORE_MASK_STORE) == store_static || !stab->parent;

		if((d->store & STORE_MASK_STORE) == store_extern){
			/* allow for globals - remove extern since it's a definition */
			if(stab->parent){
				die_at(&d->where, "externs can't be initialised");
			}else{
				warn_at(&d->where, "extern initialisation");
				d->store &= ~store_extern;
			}
		}

		/* don't generate for anonymous symbols
		 * they're done elsewhere (e.g. compound literals and strings)
		 */
		if(d->spel){
			/* this creates the below s->inits array */
			if(is_static_init){
				fold_decl_global_init(d, stab);

			}else if(pinit_code){
				EOF_WHERE(&d->where,
						if(!inits)
							inits = stmt_new_wrapper(code, symtab_new(stab));

						decl_init_brace_up_fold(d, inits->symtab);
						decl_init_create_assignments_base(d->init,
							d->ref, expr_new_identifier(d->spel),
							inits);
					);
				/* folded elsewhere */
			}else{
				ICE("fold_decl(%s) with no pinit_code?", d->spel);
			}
		}
	}

	/* name static decls */
	if(stab->parent
	&& (d->store & STORE_MASK_STORE) == store_static
	&& d->spel)
	{
			d->spel_asm = out_label_static_local(curdecl_func->spel, d->spel);
	}
#undef inits
}

void fold_decl_global_init(decl *d, symtable *stab)
{
	if(!d->init)
		return;

	EOF_WHERE(&d->where,
		/* this completes the array, if any */
		decl_init_brace_up_fold(d, stab);
	);

	if(!decl_init_is_const(d->init, stab)){
		die_at(&d->init->where, "%s %s initialiser not constant",
				stab->parent ? "static" : "global",
				decl_init_to_str(d->init->type));
	}
}

static void stmt_fctx_start(stmt_fold_ctx_function *fctx, basic_blk **bb_start, basic_blk **bb_end)
{
	*bb_start = bb_new("fn_start");
	*bb_end   = bb_new("fn_end");

	memset(fctx, 0, sizeof *fctx);
	fctx->gotos = dynmap_new((dynmap_cmp_f *)strcmp);
}

static void stmt_fctx_free(stmt_fold_ctx_function *fctx)
{
	dynmap_free(fctx->gotos);
}

static void fold_func(decl *func_decl)
{
	if(func_decl->func_code){
		struct
		{
			char *extra;
			where *where;
		} the_return = { NULL, NULL };
		symtable *const arg_symtab = DECL_FUNC_ARG_SYMTAB(func_decl);

		if(func_decl->store & store_inline
		&& (func_decl->store & STORE_MASK_STORE) == store_default)
		{
			warn_at(&func_decl->where,
					"pure inline function will not have code emitted "
					"(missing \"static\" or \"extern\")");
		}

		if(func_decl->ref->type != type_ref_func)
			warn_at(&func_decl->where,
					"typedef function implementation is an extension");

		{
			type_ref *fref = type_ref_is(func_decl->ref, type_ref_func);
			UCC_ASSERT(fref, "not a func");
			curdecl_ref_func_called = type_ref_func_call(fref, NULL);
			curdecl_func = func_decl;
		}

		{
			decl **i;
			for(i = arg_symtab->decls; i && *i; i++)
				if(!(*i)->spel)
					die_at(&func_decl->where, "argument %ld in \"%s\" is unnamed",
							i - arg_symtab->decls + 1, func_decl->spel);
		}

		{
			stmt_fold_ctx_function fctx;
			stmt_fold_ctx_block bctx = { 0 };

			basic_blk *b_start, *b_end;

			bctx.func_ctx = &fctx;

			stmt_fctx_start(&fctx, &b_start, &b_end);
			fold_stmt(func_decl->func_code, &bctx);
			stmt_fctx_free(&fctx);
		}

		/* now decls are folded, layout both parameters and local variables */
		symtab_layout_decls(arg_symtab, 0);

		if(decl_attr_present(func_decl, attr_noreturn)){
			if(!type_ref_is_void(curdecl_ref_func_called)){
				cc1_warn_at(&func_decl->where, 0, WARN_RETURN_UNDEF,
						"function \"%s\" marked no-return has a non-void return value",
						func_decl->spel);
			}


			if(fold_passable(func_decl->func_code)){
				/* if we reach the end, it's bad */
				the_return.extra = "implicitly ";
				the_return.where = &func_decl->where;
			}else{
				stmt *ret = NULL;

				stmt_walk(func_decl->func_code, stmt_walk_first_return, NULL, &ret);

				if(ret){
					/* obviously returns */
					the_return.extra = "";
					the_return.where = &ret->where;
				}
			}

			if(the_return.extra){
				cc1_warn_at(the_return.where, 0, WARN_RETURN_UNDEF,
						"function \"%s\" marked no-return %sreturns",
						func_decl->spel, the_return.extra);
			}

		}else if(!type_ref_is_void(curdecl_ref_func_called)){
			/* non-void func - check it doesn't return */
			if(fold_passable(func_decl->func_code)){
				cc1_warn_at(&func_decl->where, 0, WARN_RETURN_UNDEF,
						"control reaches end of non-void function %s",
						func_decl->spel);
			}
		}

		curdecl_ref_func_called = NULL;
		curdecl_func = NULL;
	}
}

void fold_decl_global(decl *d, symtable *stab)
{
	switch((enum decl_storage)(d->store & STORE_MASK_STORE)){
		case store_extern:
		case store_default:
		case store_static:
			break;

		case store_inline: /* allowed, but not accessible via STORE_MASK_STORE */
			ICE("inline");
		case store_typedef: /* global typedef */
			break;

		case store_auto:
		case store_register:
			die_at(&d->where, "invalid storage class %s on global scoped %s",
					decl_store_to_str(d->store),
					DECL_IS_FUNC(d) ? "function" : "variable");
	}

	fold_decl(d, stab, NULL);

	if(DECL_IS_FUNC(d)){
		UCC_ASSERT(!d->init, "function has init?");
		fold_func(d);
	}
}

void fold_check_expr(expr *e, enum fold_chk chk, const char *desc)
{
	if(!e)
		return;

	UCC_ASSERT(e->tree_type, "no tree type");

	/* fatal ones first */

	if((chk & FOLD_CHK_ALLOW_VOID) == 0 && type_ref_is_void(e->tree_type))
		die_at(&e->where, "%s requires non-void expression", desc);

	if(chk & FOLD_CHK_NO_ST_UN){
		struct_union_enum_st *sue;

		if((sue = type_ref_is_s_or_u(e->tree_type))){
			die_at(&e->where, "%s involved in %s",
					sue_str(sue), desc);
		}
	}

	if(chk & FOLD_CHK_NO_BITFIELD){
		if(e && expr_kind(e, struct)){
			decl *d = e->bits.struct_mem.d;

			if(d->field_width)
				die_at(&e->where, "bitfield in %s", desc);
		}
	}

	if(chk & FOLD_CHK_INTEGRAL){
		if(type_ref_is_floating(e->tree_type)){
			die_at(&e->where, "%s requires an integral expression (not \"%s\")",
					desc, type_ref_to_str(e->tree_type));
		}
	}

	if(!e->in_parens && expr_kind(e, assign))
		cc1_warn_at(&e->where, 0,WARN_TEST_ASSIGN, "assignment in %s", desc);

	if(chk & FOLD_CHK_BOOL){
		if(!type_ref_is_bool(e->tree_type)){
			cc1_warn_at(&e->where, 0, WARN_TEST_BOOL,
					"testing a non-boolean expression (%s), in %s",
					type_ref_to_str(e->tree_type), desc);
		}

		if(expr_kind(e, addr)){
			cc1_warn_at(&e->where, 0, WARN_TEST_BOOL/*FIXME*/,
					"an address is always true");
		}
	}

	if(chk & FOLD_CHK_CONST_I){
		consty k;
		const_fold(e, &k);

		if(k.type != CONST_NUM || !K_INTEGRAL(k.bits.num))
			die_at(&e->where, "integral constant expected for %s", desc);
	}
}

#ifdef SYMTAB_DEBUG
void print_stab(symtable *st, int current, where *w)
{
	decl **i;

	if(st->parent)
		print_stab(st->parent, 0, NULL);

	if(current)
		fprintf(stderr, "[34m");

	fprintf(stderr, "\ttable %p, children %d, vars %d, parent: %p",
			(void *)st,
			dynarray_count(st->children),
			dynarray_count(st->decls),
			(void *)st->parent);

	if(current)
		fprintf(stderr, "[m%s%s", w ? " at " : "", w ? where_str(w) : "");

	fputc('\n', stderr);

	for(i = st->decls; i && *i; i++)
		fprintf(stderr, "\t\tdecl %s\n", (*i)->spel);
}
#endif

void fold_stmt(stmt *t, stmt_fold_ctx_block *ctx)
{
	UCC_ASSERT(t->symtab->parent, "symtab has no parent");

#ifdef SYMTAB_DEBUG
	if(stmt_kind(t, code)){
		fprintf(stderr, "fold-code, symtab:\n");
		PRINT_STAB(t, 1);
	}
#endif

	t->f_fold(t, ctx);
}

void fold_stmt_and_add_to_curswitch(stmt *t, stmt_fold_ctx_block *ctx)
{
	fold_stmt(t->lhs, ctx); /* compound */

	if(!ctx->curswitch)
		die_at(&t->where, "%s not inside switch", t->f_str());

	dynarray_add(&ctx->curswitch->bits.switch_cases, t);

	/* we are compound, copy some attributes */
	t->kills_below_code = t->lhs->kills_below_code;
	/* TODO: copy ->freestanding? */
}

void fold_funcargs(funcargs *fargs, symtable *stab, type_ref *from)
{
	decl_attr *da;
	unsigned long nonnulls = 0;

	/* check nonnull corresponds to a pointer arg */
	if((da = type_attr_present(from, attr_nonnull)))
		nonnulls = da->bits.nonnull_args;

	if(fargs->arglist){
		/* check for unnamed params and extern/static specs */
		int i;

		for(i = 0; fargs->arglist[i]; i++){
			decl *const d = fargs->arglist[i];

			/* fold before for array checks, etc */
			fold_decl(d, stab, NULL);

			/* convert any array definitions and functions to pointers */
			EOF_WHERE(&d->where,
				/* must be before the decl is folded (since fold checks this) */
				if(decl_conv_array_func_to_ptr(d))
					fold_type_ref(d->ref, NULL, stab); /* refold if we converted */
			);

			if(decl_store_static_or_extern(d->store)){
				die_at(&fargs->where, "function argument %d is static or extern", i + 1);
			}

			/* ensure ptr */
			if((nonnulls & (1 << i))
			&& !type_ref_is(d->ref, type_ref_ptr)
			&& !type_ref_is(d->ref, type_ref_block))
			{
				warn_at(&fargs->arglist[i]->where, "nonnull attribute applied to non-pointer argument '%s'",
						type_ref_to_str(d->ref));
			}
		}

		if(i == 0 && nonnulls)
			warn_at(&fargs->where, "nonnull attribute applied to function with no arguments");
		else if(nonnulls != ~0UL && nonnulls & -(1 << i))
			warn_at(&fargs->where, "nonnull attributes above argument index %d ignored", i + 1);
	}else if(nonnulls){
		warn_at(&fargs->where, "nonnull attribute on parameterless function");
	}
}

int fold_passable_yes(stmt *s)
{ (void)s; return 1; }

int fold_passable_no(stmt *s)
{ (void)s; return 0; }

int fold_passable(stmt *s)
{
	return s->f_passable(s);
}

void fold_merge_tenatives(symtable *stab)
{
	decl **const globs = stab->decls;

	int i;
	/* go in reverse */
	for(i = dynarray_count(globs) - 1; i >= 0; i--){
		decl *d = globs[i];
		decl *init = NULL;

		/* functions are checked in fold_sym,
		 * where we compare local definitions too */
		if(d->proto_flag || !d->spel || DECL_IS_FUNC(d))
			continue;

		switch((enum decl_storage)(d->store & STORE_MASK_STORE)){
			case store_typedef:
			case store_extern:
				continue;
			default:
				break;
		}

		/* check for a single init between all prototypes */
		for(; d; d = d->proto){
			d->proto_flag = 1;
			if(d->init){
				if(init){
					char wbuf[WHERE_BUF_SIZ];
					die_at(&init->where, "multiple definitions of \"%s\"\n"
							"%s: note: other definition here", init->spel,
							where_str_r(wbuf, &d->where));
				}
				init = d;
			}else if(DECL_IS_ARRAY(d) && type_ref_is_complete(d->ref)){
				init = d;
				decl_default_init(d, stab);
			}
		}
		if(!init){
			/* no initialiser, give the last declared one a default init */
			d = globs[i];

			decl_default_init(d, stab);

			if(DECL_IS_ARRAY(d)){
				warn_at(&d->where,
						"tenative array definition assumed to have one element");
			}

			cc1_warn_at(&d->where, 0, WARN_TENATIVE_INIT,
					"default-initialising tenative definition of \"%s\"",
					d->spel);
		}
	}
}
