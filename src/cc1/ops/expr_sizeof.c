#include "ops.h"
#include "expr_sizeof.h"
#include "../sue.h"
#include "../out/asm.h"

#define SIZEOF_WHAT(e) ((e)->expr ? (e)->expr->tree_type : (e)->bits.size_of.of_type)
#define SIZEOF_SIZE(e)  (e)->bits.size_of.sz

#define sizeof_this tref

const char *str_expr_sizeof()
{
	return "sizeof/typeof";
}

void fold_expr_sizeof(expr *e, symtable *stab)
{
	type_ref *chosen;

	if(e->expr)
		FOLD_EXPR_NO_DECAY(e->expr, stab);
	else
		fold_type_ref(e->bits.size_of.of_type, NULL, stab);

	chosen = SIZEOF_WHAT(e);

#ifdef FIELD_WIDTH_TODO
	if(chosen->field_width){
		if(e->expr_is_typeof){
			WARN_AT(&e->where, "typeof applied to a bit-field - using underlying type");

			chosen->field_width = NULL;
			/* not a memleak
			 * should still be referenced from the decl we copied,
			 * or the struct type, etc
			 */
		}else{
			DIE_AT(&e->where, "sizeof applied to a bit-field");
		}
	}
#endif

	switch(e->what_of){
		case what_typeof:
			e->tree_type = chosen;
			break;

		case what_sizeof:
		{
			/* check for sizeof array parameter */
			if(type_ref_is_decayed_array(chosen)){
				char ar_buf[TYPE_REF_STATIC_BUFSIZ];

				WARN_AT(&e->where, "array parameter size is sizeof(%s), not sizeof(%s)",
						type_ref_to_str(chosen),
						type_ref_to_str_r_show_decayed(ar_buf, chosen));
			}
		} /* fall */

		case what_alignof:
		{
			const char *us = e->what_of == what_alignof ? "alignof" : "sizeof";
			struct_union_enum_st *sue;
			int set = 0; /* need this, since .bits can't be relied upon to be 0 */

			if(!type_ref_is_complete(chosen))
				DIE_AT(&e->where, "%s incomplete type %s", us, type_ref_to_str(chosen));

			if((sue = type_ref_is_s_or_u(chosen)) && sue_incomplete(sue))
				DIE_AT(&e->where, "%s %s", us, type_ref_to_str(chosen));

			if(e->what_of == what_alignof && e->expr){
				decl *d = NULL;

				if(expr_kind(e->expr, identifier))
					d = e->expr->bits.ident.sym->decl;
				else if(expr_kind(e->expr, struct))
					d = e->expr->bits.struct_mem.d;

				if(d)
					SIZEOF_SIZE(e) = decl_align(d), set = 1;
			}

			if(!set)
				SIZEOF_SIZE(e) = (e->what_of == what_sizeof
						? type_ref_size : type_ref_align)(
							SIZEOF_WHAT(e), &e->where);

			{
				type *t;
				e->tree_type = type_ref_new_type(t = type_new_primitive(type_long));
				/* size_t */
				t->is_signed = 0;
			}
			break;
		}
	}
}

void const_expr_sizeof(expr *e, consty *k)
{
	UCC_ASSERT(e->tree_type, "const_fold on sizeof before fold");
	k->bits.iv.val = SIZEOF_SIZE(e);
	k->bits.iv.suffix = VAL_UNSIGNED | VAL_LONG;
	k->type = CONST_VAL;
}

void gen_expr_sizeof(expr *e, symtable *stab)
{
	type_ref *r = SIZEOF_WHAT(e);
	(void)stab;

	out_push_i(e->tree_type, SIZEOF_SIZE(e));

	out_comment("sizeof %s%s", e->expr ? "" : "type ", type_ref_to_str(r));
}

void gen_expr_str_sizeof(expr *e, symtable *stab)
{
	(void)stab;
	if(e->expr){
		idt_printf("sizeof expr:\n");
		print_expr(e->expr);
	}else{
		idt_printf("sizeof %s\n", type_ref_to_str(e->bits.size_of.of_type));
	}

	if(e->what_of == what_sizeof)
		idt_printf("size = %ld\n", SIZEOF_SIZE(e));
}

void mutate_expr_sizeof(expr *e)
{
	e->f_const_fold = const_expr_sizeof;
}

expr *expr_new_sizeof_type(type_ref *t, enum what_of what_of)
{
	expr *e = expr_new_wrapper(sizeof);
	e->bits.size_of.of_type = t;
	e->what_of = what_of;
	return e;
}

expr *expr_new_sizeof_expr(expr *sizeof_this, enum what_of what_of)
{
	expr *e = expr_new_wrapper(sizeof);
	e->expr = sizeof_this;
	e->what_of = what_of;
	return e;
}

void gen_expr_style_sizeof(expr *e, symtable *stab)
{ (void)e; (void)stab; /* TODO */ }
