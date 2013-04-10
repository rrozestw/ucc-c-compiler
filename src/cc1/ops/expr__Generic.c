#include "ops.h"
#include "expr__Generic.h"
#include "../../util/printu.h"

const char *str_expr__Generic()
{
	return "_Generic";
}

void fold_expr__Generic(expr *e, symtable *stab)
{
	struct generic_lbl **i, *def;

	def = NULL;

	FOLD_EXPR(e->expr, stab);

	for(i = e->bits.generic.list; i && *i; i++){
		const int flags = DECL_CMP_EXACT_MATCH;
		struct generic_lbl **j, *l = *i;

		FOLD_EXPR(l->e, stab);

		for(j = i + 1; *j; j++){
			type_ref *m = (*j)->t;

			/* duplicate default checked below */
			if(m && type_ref_equal(m, l->t, flags))
				DIE_AT(&m->where, "duplicate type in _Generic: %s", type_ref_to_str(l->t));
		}


		if(l->t){
			fold_type_ref(l->t, NULL, stab);

			if(type_ref_equal(e->expr->tree_type, l->t, flags)){
				UCC_ASSERT(!e->bits.generic.chosen, "already chosen expr for _Generic");
				e->bits.generic.chosen = l;
			}
		}else{
			if(def)
				DIE_AT(&def->e->where, "second default for _Generic");
			def = l;
		}
	}


	if(!e->bits.generic.chosen){
		if(def)
			e->bits.generic.chosen = def;
		else
			DIE_AT(&e->where, "no type satisfying %s", type_ref_to_str(e->expr->tree_type));
	}

	e->tree_type = e->bits.generic.chosen->e->tree_type;
}

void gen_expr__Generic(expr *e, symtable *stab)
{
	gen_expr(e->bits.generic.chosen->e, stab);
}

void gen_expr_str__Generic(expr *e, symtable *stab)
{
	struct generic_lbl **i;

	(void)stab;

	idt_printf("_Generic expr:\n");
	gen_str_indent++;
	print_expr(e->expr);
	gen_str_indent--;

	idt_printf("_Generic choices:\n");
	gen_str_indent++;
	for(i = e->bits.generic.list; i && *i; i++){
		struct generic_lbl *l = *i;

		if(e->bits.generic.chosen == l)
			idt_printf("[Chosen]\n");

		if(l->t){
			idt_printf("type: ");
			gen_str_indent++;
			printu("%R", l->t);
			gen_str_indent--;
			fprintf(cc1_out, "\n");
		}else{
			idt_printf("default:\n");
		}
		idt_printf("expr:\n");
		gen_str_indent++;
		print_expr(l->e);
		gen_str_indent--;
	}
	gen_str_indent--;
}

void const_expr__Generic(expr *e, consty *k)
{
	/* we're const if our chosen expr is */
	UCC_ASSERT(e->bits.generic.chosen, "_Generic const check before fold");

	const_fold(e->bits.generic.chosen->e, k);
}

void mutate_expr__Generic(expr *e)
{
	e->f_const_fold = const_expr__Generic;
}

expr *expr_new__Generic(expr *test, struct generic_lbl **lbls)
{
	expr *e = expr_new_wrapper(_Generic);
	e->expr = test;
	e->bits.generic.list = lbls;
	return e;
}

void gen_expr_style__Generic(expr *e, symtable *stab)
{ (void)e; (void)stab; /* TODO */ }
