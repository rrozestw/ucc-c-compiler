#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "../util/util.h"
#include "../util/dynarray.h"
#include "data_structs.h"
#include "cc1.h"
#include "macros.h"
#include "../util/platform.h"
#include "sym.h"
#include "gen_asm.h"
#include "../util/util.h"
#include "const.h"
#include "tree.h"
#include "out/out.h"
#include "out/lbl.h"
#include "out/asm.h"

char *curfunc_lblfin; /* extern */

void gen_expr(expr *e, symtable *stab)
{
	consty k;

	const_fold(e, &k);

	if(k.type == CONST_VAL) /* TODO: -O0 skips this */
		out_push_iv(e->tree_type, &k.bits.iv);
	else
		EOF_WHERE(&e->where, e->f_gen(e, stab));
}

void lea_expr(expr *e, symtable *stab)
{
	UCC_ASSERT(e->f_lea, "invalid store expression %s (no f_store())", e->f_str());

	e->f_lea(e, stab);
}

void gen_stmt(stmt *t)
{
	EOF_WHERE(&t->where, t->f_gen(t));
}

void static_addr(expr *e)
{
	consty k;

	memset(&k, 0, sizeof k);

	const_fold(e, &k);

	switch(k.type){
		case CONST_NEED_ADDR:
		case CONST_NO:
			ICE("non-constant expr-%s const=%d%s",
					e->f_str(),
					k.type,
					k.type == CONST_NEED_ADDR ? " (needs addr)" : "");
			break;

		case CONST_VAL:
			asm_declare_partial("%ld", k.bits.iv.val);
			break;

		case CONST_ADDR:
			if(k.bits.addr.is_lbl)
				asm_declare_partial("%s", k.bits.addr.bits.lbl);
			else
				asm_declare_partial("%d", k.bits.addr.bits.memaddr);
			break;

		case CONST_STRK:
			asm_declare_partial("%s", k.bits.str->lbl);
			break;
	}

	/* offset in bytes, no mul needed */
	if(k.offset)
		asm_declare_partial(" + %ld", k.offset);
}

#ifdef FANCY_STACK_INIT
#define ITER_DECLS(i) for(i = df->func_code->symtab->decls; i && *i; i++)

void gen_func_stack(decl *df, const int offset)
{
	int use_sub = 1, clever = 0;
	decl **iter;

	ITER_DECLS(iter)
		if((*iter)->init){
			clever = 1;
			break;
		}

	if(use_sub){
		asm_output_new(asm_out_type_sub,
				asm_operand_new_reg(NULL, ASM_REG_SP),
				asm_operand_new_val(offset));
	}else{
		ITER_DECLS(iter){
			decl *d = *iter;
			if(d->init && d->init->type != decl_init_scalar){
				ICW("TODO: stack gen or expr for %s init", decl_to_str(d));
			}
		}
	}
}
#else
#endif

void gen_asm_global(decl *d)
{
	decl_attr *sec;

	if(!d->is_definition)
		return;

	if((sec = decl_has_attr(d, attr_section))){
		ICW("%W: TODO: section attribute \"%s\" on %s",
				&d->attr->where,
				sec->attr_extra.section, d->spel);
	}

	/* order of the if matters */
	if(DECL_IS_FUNC(d) || type_ref_is(d->ref, type_ref_block)){
		/* check .func_code, since it could be a block */
		int nargs = 0;
		decl **aiter;
		const char *sp;

		if(!d->func_code)
			return;

		for(aiter = d->func_code->symtab->decls; aiter && *aiter; aiter++)
			if((*aiter)->sym->type == sym_arg)
				nargs++;

		sp = decl_asm_spel(d);

		out_label(sp);

		out_func_prologue(
				d->func_code->symtab->auto_total_size,
				nargs,
				decl_is_variadic(d));

		curfunc_lblfin = out_label_code(sp);

		gen_stmt(d->func_code);

		out_label(curfunc_lblfin);

		out_func_epilogue();

		free(curfunc_lblfin);

	}else{
		/* asm takes care of .bss vs .data, etc */
		asm_declare_decl_init(cc_out[SECTION_DATA], d);
	}
}

static void gen_gasm(char *asm_str)
{
	fprintf(cc_out[SECTION_TEXT], "%s\n", asm_str);
}

void gen_asm(symtable_global *globs)
{
	decl **diter;
	struct symtable_gasm **iasm = globs->gasms;

	for(diter = globs->stab.decls; diter && *diter; diter++){
		decl *d = *diter;

		while(iasm && d == (*iasm)->before){
			gen_gasm((*iasm)->asm_str);

			if(!*++iasm)
				iasm = NULL;
		}

		/* inline_only aren't currently inlined */
		if(!d->is_definition)
			continue;

		if(d->inline_only){
			/* emit an extern for it anyway */
			asm_predeclare_extern(d);
			continue;
		}

		switch(d->store & STORE_MASK_STORE){
			case store_inline:
			case store_auto:
			case store_register:
			case store_typedef:
				ICE("%s storage on global %s",
						decl_store_to_str(d->store),
						decl_to_str(d));

			case store_static:
				break;

			case store_extern:
				if(!DECL_IS_FUNC(d) || !d->func_code)
					break;
				/* else extern func with definition */

			case store_default:
				asm_predeclare_global(d);
		}

		gen_asm_global(d);

		UCC_ASSERT(out_vcount() == 0, "non empty vstack after global gen");
	}

	for(; iasm && *iasm; ++iasm)
		gen_gasm((*iasm)->asm_str);
}
