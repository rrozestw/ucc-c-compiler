#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "ops.h"
#include "expr_funcall.h"
#include "../../util/dynarray.h"
#include "../../util/platform.h"
#include "../../util/alloc.h"
#include "../funcargs.h"

const char *str_expr_funcall()
{
	return "funcall";
}

int const_fold_expr_funcall(expr *e)
{
	(void)e;
	return 1; /* could extend to have int x() const; */
}

enum printf_attr
{
	printf_attr_long = 1 << 0
};

static void format_check_printf_1(char fmt, type_ref *tt,
		where *w, enum printf_attr attr)
{
	int allow_long = 0;

	switch(fmt){
		enum type_primitive prim;

		case 's': prim = type_char; goto ptr;
		case 'p': prim = type_void; goto ptr;
		case 'n': prim = type_int;  goto ptr;
ptr:
			tt = type_ref_is_type(type_ref_is_ptr(tt), prim);
			if(!tt){
				WARN_AT(w, "format %%%c expects '%s *' argument",
						fmt, type_primitive_to_str(prim));
			}
			break;

		case 'x':
		case 'X':
		case 'u':
		case 'o':
			/* unsigned ... */
		case '*':
		case 'c':
		case 'd':
		case 'i':
			allow_long = 1;
			if(!type_ref_is_integral(tt))
				WARN_AT(w, "format %%%c expects integral argument", fmt);
			if((attr & printf_attr_long) && !type_ref_is_type(tt, type_long))
				WARN_AT(w, "format %%l%c expects long argument", fmt);
			break;

		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G':
		case 'a':
		case 'A':
			if(!type_ref_is_floating(tt))
				WARN_AT(w, "format %%d expects double argument");
			break;

		default:
			WARN_AT(w, "unknown conversion character 0x%x", fmt);
	}

	if(!allow_long && attr & printf_attr_long)
		WARN_AT(w, "format %%%c expects a long", fmt);
}

static void format_check_printf_str(
		expr **args,
		const char *fmt, const int len,
		int var_arg, where *w)
{
	int n_arg = 0;
	int i;

	for(i = 0; i < len && fmt[i];){
		if(fmt[i++] == '%'){
			int fin;
			enum printf_attr attr;
			expr *e;

			if(fmt[i] == '%'){
				i++;
				continue;
			}

recheck:
			attr = 0;
			fin = 0;
			do switch(fmt[i]){
				 case 'l':
					/* TODO: multiple check.. */
					attr |= printf_attr_long;

				case '1': case '2': case '3':
				case '4': case '5': case '6':
				case '7': case '8': case '9':

				case '0': case '#': case '-':
				case ' ': case '+': case '.':

				case 'h': case 'L':
					i++;
					break;
				default:
					fin = 1;
			}while(!fin);

			e = args[var_arg + n_arg++];

			if(!e){
				WARN_AT(w, "too few arguments for format (%%%c)", fmt[i]);
				break;
			}

			format_check_printf_1(fmt[i], e->tree_type, &e->where, attr);
			if(fmt[i] == '*'){
				i++;
				goto recheck;
			}
		}
	}

	if((!fmt[i] || i == len) && args[var_arg + n_arg])
		WARN_AT(w, "too many arguments for format");
}

static void format_check_printf(
		expr *str_arg,
		expr **args,
		unsigned var_arg,
		where *w)
{
	stringval *fmt_str;
	consty k;

	const_fold(str_arg, &k);

	switch(k.type){
		case CONST_NO:
		case CONST_NEED_ADDR:
			/* check for the common case printf(x?"":"", ...) */
			if(expr_kind(str_arg, if)){
				format_check_printf(str_arg->lhs, args, var_arg, w);
				format_check_printf(str_arg->rhs, args, var_arg, w);
				return;
			}

			WARN_AT(w, "format argument isn't constant");
			return;

		case CONST_VAL:
			if(k.bits.iv.val == 0)
				return; /* printf(NULL, ...) */
			/* fall */

		case CONST_ADDR:
			WARN_AT(w, "format argument isn't a string constant");
			return;

		case CONST_STRK:
			fmt_str = k.bits.str;
			break;
	}

	{
		const char *fmt = fmt_str->str;
		const int   len = fmt_str->len;

		if(k.offset >= len)
			WARN_AT(w, "undefined printf-format argument");
		else
			format_check_printf_str(args, fmt + k.offset, len, var_arg, w);
	}
}

static void format_check(where *w, type_ref *ref, expr **args, const int variadic)
{
	decl_attr *attr = type_attr_present(ref, attr_format);
	unsigned n, fmt_arg, var_arg;

	if(!attr)
		return;

	if(!variadic)
		DIE_AT(w, "variadic function required for format check");

	fmt_arg = attr->attr_extra.format.fmt_arg;
	var_arg = attr->attr_extra.format.var_arg;

	n = dynarray_count((void **)args);

	if(fmt_arg >= n)
		DIE_AT(w, "format argument out of bounds (%d >= %d)", fmt_arg, n);
	if(var_arg > n)
		DIE_AT(w, "variadic argument out of bounds (%d >= %d)", var_arg, n);
	if(var_arg <= fmt_arg)
		DIE_AT(w, "variadic argument %s format argument", var_arg == fmt_arg ? "at" : "before");

	switch(attr->attr_extra.format.fmt_func){
		case attr_fmt_printf:
			format_check_printf(args[fmt_arg], args, var_arg, w);
			break;

		case attr_fmt_scanf:
			ICW("scanf check");
			break;
	}
}

#define ATTR_WARN(w, ...) do{ WARN_AT(w, __VA_ARGS__); return; }while(0)

static void sentinel_check(where *w, type_ref *ref, expr **args,
		const int variadic, const int nstdargs)
{
	decl_attr *attr = type_attr_present(ref, attr_sentinel);
	int i, nvs;
	expr *sentinel;

	if(!attr)
		return;

	if(!variadic)
		ATTR_WARN(w, "variadic function required for sentinel check");

	i = attr->attr_extra.sentinel;
	nvs = dynarray_count((void **)args) - nstdargs;

	if(nvs == 0)
		ATTR_WARN(w, "not enough variadic arguments for a sentinel");

	UCC_ASSERT(nvs >= 0, "too few args");

	if(i >= nvs)
		ATTR_WARN(w, "sentinel index is not a variadic argument");

	sentinel = args[(nstdargs + nvs - 1) - i];

	if(!expr_is_null_ptr(sentinel, 0))
		ATTR_WARN(&sentinel->where, "sentinel argument expected (got %s)",
				type_ref_to_str(sentinel->tree_type));

}

static void static_array_check(
		decl *arg_decl, expr *arg_expr)
{
	/* if ty_func is x[static %d], check counts */
	type_ref *ty_expr = arg_expr->tree_type;
	type_ref *ty_decl = decl_is_decayed_array(arg_decl);
	consty k_decl;

	if(!ty_decl || !ty_decl->bits.ptr.is_static)
		return;

	if(expr_is_null_ptr(arg_expr, 1 /* int */)){
		WARN_AT(&arg_expr->where, "passing null-pointer where array expected");
		return;
	}

	const_fold(ty_decl->bits.ptr.size, &k_decl);

	if(!(ty_expr = type_ref_is_decayed_array(ty_expr))){
		WARN_AT(&arg_expr->where,
				(k_decl.type == CONST_VAL) ?
				"array of size >= %ld expected for parameter" :
				"array expected for parameter", k_decl.bits.iv.val);
		return;
	}

	/* ty_expr is the type_ref_ptr, decayed from array */
	{
		consty k_arg;

		const_fold(ty_expr->bits.ptr.size, &k_arg);

		if(k_decl.type == CONST_VAL && k_arg.bits.iv.val < k_decl.bits.iv.val)
			WARN_AT(&arg_expr->where,
					"array of size %ld passed where size %ld needed",
					k_arg.bits.iv.val, k_decl.bits.iv.val);
	}
}

void fold_expr_funcall(expr *e, symtable *stab)
{
	type_ref *type_func;
	funcargs *args_from_decl;
	char *sp = NULL;
	int count_decl = 0;

#if 0
	if(func_is_asm(sp)){
		expr *arg1;
		const char *str;
		int i;

		if(!e->funcargs || e->funcargs[1] || !expr_kind(e->funcargs[0], addr))
			DIE_AT(&e->where, "invalid __asm__ arguments");

		arg1 = e->funcargs[0];
		str = arg1->data_store->data.str;
		for(i = 0; i < arg1->array_store->len - 1; i++){
			char ch = str[i];
			if(!isprint(ch) && !isspace(ch))
invalid:
				DIE_AT(&arg1->where, "invalid __asm__ string (character 0x%x at index %d, %d / %d)",
						ch, i, i + 1, arg1->array_store->len);
		}

		if(str[i])
			goto invalid;

		/* TODO: allow a long return, e.g. __asm__(("movq $5, %rax")) */
		e->tree_type = decl_new_void();
		return;
		ICE("TODO: __asm__");
	}
#endif


	if(expr_kind(e->expr, identifier) && (sp = e->expr->bits.ident.spel)){
		/* check for implicit function */
		if(!(e->expr->bits.ident.sym = symtab_search(stab, sp))){
			funcargs *args = funcargs_new();
			decl *df;

			funcargs_empty(args); /* set up the funcargs as if it's "x()" - i.e. any args */

			type_func = type_ref_new_func(type_ref_new_type(type_new_primitive(type_int)), args);

			cc1_warn_at(&e->where, 0, 1, WARN_IMPLICIT_FUNC, "implicit declaration of function \"%s\"", sp);

			df = decl_new();
			df->ref = type_func;
			df->spel = e->expr->bits.ident.spel;
			df->is_definition = 1; /* needed since it's a local var */

			/* not declared - generate a sym ourselves */
			e->expr->bits.ident.sym = SYMTAB_ADD(stab, df, sym_local);
		}
	}

	FOLD_EXPR(e->expr, stab);
	type_func = e->expr->tree_type;

	if(!type_ref_is_callable(type_func)){
		DIE_AT(&e->expr->where, "%s-expression (type '%s') not callable",
				e->expr->f_str(), type_ref_to_str(type_func));
	}

	if(expr_kind(e->expr, deref)
	&& type_ref_is(type_ref_is_ptr(expr_deref_what(e->expr)->tree_type), type_ref_func)){
		/* XXX: memleak */
		/* (*f)() - dereffing to a function, then calling - remove the deref */
		e->expr = expr_deref_what(e->expr);
	}

	e->tree_type = type_ref_func_call(type_func, &args_from_decl);

	/* func count comparison, only if the func has arg-decls, or the func is f(void) */
	UCC_ASSERT(args_from_decl, "no funcargs for decl %s", sp);


	/* this block is purely count checking */
	if(args_from_decl->arglist || args_from_decl->args_void){
		const int count_arg  = dynarray_count((void **)e->funcargs);

		count_decl = dynarray_count((void **)args_from_decl->arglist);

		if(count_decl != count_arg && (args_from_decl->variadic ? count_arg < count_decl : 1)){
			DIE_AT(&e->where, "too %s arguments to function %s (got %d, need %d)",
					count_arg > count_decl ? "many" : "few",
					sp, count_arg, count_decl);
		}
	}else if(args_from_decl->args_void_implicit && e->funcargs){
		WARN_AT(&e->where, "too many arguments to implicitly (void)-function");
	}

	/* this block folds the args and type-checks */
	if(e->funcargs){
		unsigned long nonnulls = 0;
		char *const desc = ustrprintf("function argument to %s", sp);
		int i, j;
		decl_attr *da;

		if((da = type_attr_present(type_func, attr_nonnull)))
			nonnulls = da->attr_extra.nonnull_args;

		for(i = j = 0; e->funcargs[i]; i++){
			decl *decl_arg;
			expr *arg = FOLD_EXPR(e->funcargs[i], stab);

			fold_need_expr(arg, desc, 0);
			fold_disallow_st_un(arg, desc);

			if(args_from_decl->arglist && (decl_arg = args_from_decl->arglist[j])){
				const int eq = fold_type_ref_equal(
						decl_arg->ref, arg->tree_type, &arg->where,
						WARN_ARG_MISMATCH, 0,
						"mismatching argument %d to %s (%Q <-- %R)",
						i, sp, decl_arg, arg->tree_type);

				if(!eq){
					fold_insert_casts(decl_arg->ref, &e->funcargs[i],
							stab, &arg->where, desc);

					arg = e->funcargs[i];
				}

				/* f(int [static 5]) check */
				static_array_check(decl_arg, arg);

				j++;
			}

			if((nonnulls & (1 << i)) && expr_is_null_ptr(arg, 1))
				WARN_AT(&arg->where, "null passed where non-null required (arg %d)", i + 1);
		}

		free(desc);
	}

	/* each arg needs casting up to int size, if smaller */
	if(e->funcargs){
		int i;
		for(i = 0; e->funcargs[i]; i++)
			expr_promote_int_if_smaller(&e->funcargs[i], stab);
	}

	fold_disallow_st_un(e, "return");

	/* attr */
	{
		type_ref *r = e->expr->tree_type;

		format_check(&e->where, r, e->funcargs, args_from_decl->variadic);
		sentinel_check(&e->where, r, e->funcargs, args_from_decl->variadic, count_decl);
	}

	/* check the subexp tree type to get the funcall decl_attrs */
	if(type_attr_present(e->expr->tree_type, attr_warn_unused))
		e->freestanding = 0; /* needs use */
}

void gen_expr_funcall(expr *e, symtable *stab)
{
	if(0){
		out_comment("start manual __asm__");
		ICE("same");
#if 0
		fprintf(cc_out[SECTION_TEXT], "%s\n", e->funcargs[0]->data_store->data.str);
#endif
		out_comment("end manual __asm__");
	}else{
		/* continue with normal funcall */
		int nargs = 0;

		gen_expr(e->expr, stab);

		if(e->funcargs){
			expr **aiter;

			for(aiter = e->funcargs; *aiter; aiter++, nargs++);

			for(aiter--; aiter >= e->funcargs; aiter--){
				expr *earg = *aiter;

				/* should be of size int or larger (for integral types)
				 * or double (for floating types)
				 */
				gen_expr(earg, stab);
			}
		}

		out_call(nargs, e->tree_type, e->expr->tree_type);
	}
}

void gen_expr_str_funcall(expr *e, symtable *stab)
{
	expr **iter;

	(void)stab;

	idt_printf("funcall, calling:\n");

	gen_str_indent++;
	print_expr(e->expr);
	gen_str_indent--;

	if(e->funcargs){
		int i;
		idt_printf("args:\n");
		gen_str_indent++;
		for(i = 1, iter = e->funcargs; *iter; iter++, i++){
			idt_printf("arg %d:\n", i);
			gen_str_indent++;
			print_expr(*iter);
			gen_str_indent--;
		}
		gen_str_indent--;
	}else{
		idt_printf("no args\n");
	}
}

void mutate_expr_funcall(expr *e)
{
	(void)e;
}

int expr_func_passable(expr *e)
{
	/* need to check the sub-expr, i.e. the function */
	return !type_attr_present(e->expr->tree_type, attr_noreturn);
}

expr *expr_new_funcall()
{
	expr *e = expr_new_wrapper(funcall);
	e->freestanding = 1;
	return e;
}

void gen_expr_style_funcall(expr *e, symtable *stab)
{ (void)e; (void)stab; /* TODO */ }
