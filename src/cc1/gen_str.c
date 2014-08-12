#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "../util/util.h"
#include "../util/platform.h"
#include "../util/dynarray.h"

#include "macros.h"

#include "sym.h"
#include "cc1.h"
#include "sue.h"
#include "expr.h"
#include "stmt.h"
#include "type_is.h"
#include "gen_str.h"
#include "str.h"
#include "const.h"
#include "decl_init.h"
#include "funcargs.h"
#include "out/asm.h" /* cc*_out */
#include "gen_asm.h" /* IGNORE_PRINTGEN */

#define ENGLISH_PRINT_ARGLIST

#define PRINT_IF(x, sub, fn) \
	if(x->sub){ \
		idt_printf(#sub ":\n"); \
		gen_str_indent++; \
		fn(x->sub); \
		gen_str_indent--; \
	}

int gen_str_indent = 0;

FILE *gen_file(void)
{
	return cc1_out;
}

void idt_print()
{
	int i;

	for(i = gen_str_indent; i > 0; i--)
		fputs("  ", cc1_out);
}

void idt_printf(const char *fmt, ...)
{
	va_list l;

	idt_print();

	va_start(l, fmt);
	vfprintf(cc1_out, fmt, l);
	va_end(l);
}

static void print_expr_val(expr *e)
{
	consty k;

	const_fold(e, &k);

	UCC_ASSERT(k.type == CONST_NUM, "val expected");
	UCC_ASSERT((k.bits.num.suffix & VAL_UNSIGNED) == 0, "TODO: unsigned");

	if(K_INTEGRAL(k.bits.num))
		fprintf(cc1_out, NUMERIC_FMT_D, k.bits.num.val.i);
	else
		fprintf(cc1_out, NUMERIC_FMT_LD, k.bits.num.val.f);
}

static void print_decl_init(decl_init *di)
{
	switch(di->type){
		case decl_init_scalar:
			idt_printf("scalar:\n");
			gen_str_indent++;
			print_expr(di->bits.expr);
			gen_str_indent--;
			break;

		case decl_init_copy:
			ICE("copy in print");
			break;

		case decl_init_brace:
		{
			decl_init *s;
			int i;

			idt_printf("brace\n");

			gen_str_indent++;
			for(i = 0; (s = di->bits.ar.inits[i]); i++){
				if(s == DYNARRAY_NULL){
					idt_printf("[%d] = <zero init>\n", i);
				}else if(s->type == decl_init_copy){
					idt_printf("[%d] = copy from range_store[%ld]\n",
							i, (long)DECL_INIT_COPY_IDX(s, di));
				}else{
					const int need_brace = s->type == decl_init_brace;

					/* ->member not printed */
#ifdef DINIT_WITH_STRUCT
					if(s->spel)
						idt_printf(".%s", s->spel);
					else
#endif
						idt_printf("[%d]", i);

					fprintf(cc1_out, " = %s\n", need_brace ? "{" : "");

					gen_str_indent++;
					print_decl_init(s);
					gen_str_indent--;

					if(need_brace)
						idt_printf("}\n");
				}
			}
			gen_str_indent--;

			if(di->bits.ar.range_inits){
				struct init_cpy *icpy;

				idt_printf("range store:\n");
				gen_str_indent++;

				for(i = 0; (icpy = di->bits.ar.range_inits[i]); i++){
					idt_printf("store[%d]:\n", i);
					gen_str_indent++;
					print_decl_init(icpy->range_init);
					gen_str_indent--;
					if(icpy->first_instance){
						idt_printf("first expr:\n");
						gen_str_indent++;
						print_expr(icpy->first_instance);
						gen_str_indent--;
					}
				}
				gen_str_indent--;
			}
		}
	}
}

static void print_type_eng(type *ref)
{
	if(!ref)
		return;

	print_type_eng(ref->ref);

	switch(ref->type){
		case type_auto:
			ICE("__auto_type");

		case type_cast:
			if(ref->bits.cast.is_signed_cast)
				fprintf(cc1_out, "%s ", ref->bits.cast.signed_true ? "signed" : "unsigned");
			else
				fprintf(cc1_out, "%s", type_qual_to_str(ref->bits.cast.qual, 1));
			break;

		case type_ptr:
			fprintf(cc1_out, "pointer to ");
			break;

		case type_block:
			fprintf(cc1_out, "block returning ");
			break;

		case type_func:
		{
#ifdef ENGLISH_PRINT_ARGLIST
			funcargs *fargs = ref->bits.func.args;
			decl **iter;
#endif

			fputs("function", cc1_out);

#ifdef ENGLISH_PRINT_ARGLIST
			fputc('(', cc1_out);
			if(fargs->arglist){

				for(iter = fargs->arglist; iter && *iter; iter++){
					print_decl(*iter, PDECL_NONE);
					if(iter[1])
						fputs(", ", cc1_out);
				}

				if(fargs->variadic)
					fputs("variadic", cc1_out);

			}else{
				fprintf(cc1_out, "taking %s arguments", fargs->args_void ? "no" : "unspecified");
			}
			fputc(')', cc1_out);
#endif
			fputs(" returning ", cc1_out);

			break;
		}

		case type_array:
			fputs("array[", cc1_out);
			if(ref->bits.array.size)
				print_expr_val(ref->bits.array.size);
			fputs("] of ", cc1_out);
			break;

		case type_btype:
			fprintf(cc1_out, "%s", btype_to_str(ref->bits.type));
			break;

		case type_tdef:
		case type_attr:
			ICE("TODO");
		case type_where:
			break;
	}
}

static void print_decl_eng(decl *d)
{
	if(d->spel)
		fprintf(cc1_out, "\"%s\": ", d->spel);

	print_type_eng(d->ref);
}

void print_type(type *ref, decl *d)
{
	char buf[TYPE_STATIC_BUFSIZ];

	fprintf(cc1_out, "%s",
			type_to_str_r_spel(buf, ref, d ? d->spel : NULL));

	if(ref->type == type_attr){
		attribute *da;
		for(da = ref->bits.attr; da; da = da->next){
			fprintf(cc1_out, " __attribute__((%s))",
					attribute_to_str(da));
		}
	}
}

static void print_attribute(attribute *da)
{
	for(; da; da = da->next){
		idt_printf("__attribute__((%s))\n", attribute_to_str(da));

		gen_str_indent++;
		switch(da->type){
			case attr_section:
				idt_printf("section \"%s\"\n", da->bits.section);
				break;
			case attr_nonnull:
			{
				unsigned long l = da->bits.nonnull_args;

				idt_printf("nonnull: ");
				if(l == ~0UL){
					fprintf(cc1_out, "all");
				}else{
					const char *sep = "";
					int i;

					for(i = 0; i <= 32; i++)
						if(l & (1 << i)){
							fprintf(cc1_out, "%s%d", sep, i);
							sep = ", ";
						}
				}

				fputc('\n', cc1_out);
				break;
			}

			default:
				break;
		}
		gen_str_indent--;
	}
}

static void print_type_attr(type *r)
{
	enum attribute_type i;

	for(i = 0; i < attr_LAST; i++){
		attribute *da;
		if((da = type_attr_present(r, i)))
			print_attribute(da);
	}
}

void print_decl(decl *d, enum pdeclargs mode)
{
	if(mode & PDECL_INDENT)
		idt_print();

	if(d->store)
		fprintf(cc1_out, "%s ", decl_store_to_str(d->store));

	if(fopt_mode & FOPT_ENGLISH){
		print_decl_eng(d);
	}else{
		print_type(d->ref, d);
	}

	if(mode & PDECL_SYM_OFFSET){
		if(d->sym){
			const int off = d->sym->type == sym_arg
				? d->sym->loc.arg_offset
				: (int)d->sym->loc.stack_pos;

			fprintf(cc1_out, " (sym %s, pos = %d)",
					sym_to_str(d->sym->type), off);
		}else{
			fprintf(cc1_out, " (no sym)");
		}
	}

	if(mode & PDECL_SIZE && !type_is(d->ref, type_func)){
		if(type_is_complete(d->ref)){
			const int sz = decl_size(d);
			fprintf(cc1_out, " size %d bytes. %d platform-word(s)", sz, sz / platform_word_size());
		}else{
			fprintf(cc1_out, " incomplete decl");
		}
	}

	if(mode & PDECL_NEWLINE)
		fputc('\n', cc1_out);

	if(!type_is(d->ref, type_func) && d->bits.var.init && mode & PDECL_PINIT){
		gen_str_indent++;
		print_decl_init(d->bits.var.init);
		gen_str_indent--;
	}

	if(mode & PDECL_ATTR){
		gen_str_indent++;
		if(!type_is(d->ref, type_func) && d->bits.var.align)
			idt_printf("[align={as_int=%d, resolved=%d}]\n",
					d->bits.var.align->as_int, d->bits.var.align->resolved);
		print_attribute(d->attr);
		print_type_attr(d->ref);
		gen_str_indent--;
	}

	if((mode & PDECL_FUNC_DESCEND) && DECL_HAS_FUNC_CODE(d)){
		decl **iter;

		gen_str_indent++;

		for(iter = d->bits.func.code->symtab->decls; iter && *iter; iter++){
			sym *s = (*iter)->sym;
			if(s)
				idt_printf("offset of %s = %d\n", (*iter)->spel, s->loc.stack_pos);
		}

		idt_printf("function stack space %d\n",
				d->bits.func.code->symtab->auto_total_size);

		print_stmt(d->bits.func.code);

		gen_str_indent--;
	}
}

void print_expr(expr *e)
{
	idt_printf("expr: %s\n", e->f_str());
	if(e->tree_type){ /* might be a label */
		idt_printf("tree_type: ");
		gen_str_indent++;
		print_type(e->tree_type, NULL);
		gen_str_indent--;
		fputc('\n', cc1_out);
	}
	gen_str_indent++;
	if(e->f_gen)
		IGNORE_PRINTGEN(e->f_gen(e, NULL));
	else
		idt_printf("builtin/%s::%s\n", e->f_str(), e->expr->bits.ident.spel);
	gen_str_indent--;
}

static void print_struct(struct_union_enum_st *sue)
{
	sue_member **iter;

	if(!sue_complete(sue)){
		idt_printf("incomplete %s %s\n", sue_str(sue), sue->spel);
		return;
	}

	idt_printf("%s %s (size %d):\n", sue_str(sue), sue->spel, sue_size(sue, &sue->where));

	gen_str_indent++;
	for(iter = sue->members; iter && *iter; iter++){
		decl *d = (*iter)->struct_member;

		idt_printf("decl %s:\n", d->spel ? d->spel : "<anon>");
		gen_str_indent++;
		print_decl(d, PDECL_INDENT | PDECL_NEWLINE | PDECL_ATTR);

		if(!type_is(d->ref, type_func)){
#define SHOW_FIELD(nam) idt_printf("." #nam " = %u\n", d->bits.var.nam)
			SHOW_FIELD(struct_offset);

			if(d->bits.var.field_width){
				integral_t v = const_fold_val_i(d->bits.var.field_width);

				gen_str_indent++;

				idt_printf(".field_width = %" NUMERIC_FMT_D "\n", v);

				SHOW_FIELD(struct_offset_bitfield);

				gen_str_indent--;
			}
		}

		gen_str_indent--;
	}
	gen_str_indent--;
}

static void print_enum(struct_union_enum_st *et)
{
	sue_member **mi;

	idt_printf("enum %s:\n", et->spel);

	gen_str_indent++;
	for(mi = et->members; *mi; mi++){
		enum_member *m = (*mi)->enum_member;

		idt_printf("member %s = %" NUMERIC_FMT_D "\n", m->spel, (integral_t)m->val->bits.num.val.i);
	}
	gen_str_indent--;
}

static void print_sues_static_asserts(symtable *stab)
{
	struct_union_enum_st **sit;
	static_assert **stati;
	int nl = 0;

	for(sit = stab->sues; sit && *sit; sit++){
		struct_union_enum_st *sue = *sit;
		(sue->primitive == type_enum ? print_enum : print_struct)(sue);
		nl = 1;
	}

	for(stati = stab->static_asserts; stati && *stati; stati++){
		static_assert *sa = *stati;

		idt_printf("static assertion: %s\n", sa->s);
		gen_str_indent++;
		print_expr(sa->e);
		gen_str_indent--;

		nl = 1;
	}

	if(nl)
		fputc('\n', cc1_out);
}

static void print_stmt_flow(stmt_flow *t)
{
	idt_printf("for parts:\n");

	gen_str_indent++;
	PRINT_IF(t, for_init,      print_expr);
	PRINT_IF(t, for_while,     print_expr);
	PRINT_IF(t, for_inc,       print_expr);
	gen_str_indent--;
}

void print_stmt(stmt *t)
{
	idt_printf("statement: %s\n", t->f_str());

	if(t->flow){
		gen_str_indent++;
		print_stmt_flow(t->flow);
		gen_str_indent--;
	}

	PRINT_IF(t, expr, print_expr);
	PRINT_IF(t, lhs,  print_stmt);
	PRINT_IF(t, rhs,  print_stmt);
	PRINT_IF(t, rhs,  print_stmt);

	if(stmt_kind(t, code)){
		idt_printf("structs/unions/enums:\n");
		gen_str_indent++;
		print_sues_static_asserts(t->symtab);
		gen_str_indent--;

		if(t->symtab){
			decl **iter;

			idt_printf("stack space %d\n", t->symtab->auto_total_size);
			idt_printf("decls:\n");

			for(iter = t->symtab->decls; iter && *iter; iter++){
				decl *d = *iter;

				gen_str_indent++;
				print_decl(d, PDECL_INDENT
						| PDECL_NEWLINE
						| PDECL_SYM_OFFSET
						| PDECL_ATTR
						| PDECL_PINIT);
				gen_str_indent--;
			}
		}

		if(t->bits.code.stmts){
			stmt **iter;

			idt_printf("code:\n");

			for(iter = t->bits.code.stmts; *iter; iter++){
				gen_str_indent++;
				print_stmt(*iter);
				gen_str_indent--;
			}
		}
	}
}

void gen_str(symtable_global *symtab)
{
	decl **diter;

	print_sues_static_asserts(&symtab->stab);

	for(diter = symtab->stab.decls; diter && *diter; diter++){
		decl *const d = *diter;

		print_decl(d, PDECL_INDENT
				| PDECL_NEWLINE
				| PDECL_FUNC_DESCEND
				| PDECL_SIZE
				| PDECL_PINIT
				| PDECL_ATTR);

		if(gen_str_indent != 0)
			fprintf(stderr, "indent (%d) not reset after %s\n",
					gen_str_indent, d->spel);
	}
}
