#include <stdlib.h>
#include <string.h>

#include "ops.h"
#include "stmt_switch.h"
#include "../sue.h"
#include "../../util/alloc.h"
#include "../../util/dynarray.h"

const char *str_stmt_switch()
{
	return "switch";
}

void fold_switch_dups(stmt *sw)
{
	typedef int (*qsort_f)(const void *, const void *);

	int n = dynarray_count((void **)sw->codes);
	struct
	{
		intval start, end;
		stmt *cse;
	} *const vals = malloc(n * sizeof *vals);

	stmt **titer, *def = NULL;
	int i;

	/* gather all switch values */
	for(i = 0, titer = sw->codes; titer && *titer; titer++){
		stmt *cse = *titer;

		if(cse->expr->expr_is_default){
			if(def){
				char buf[WHERE_BUF_SIZ];

				DIE_AT(&cse->where, "duplicate default statement (from %s)",
						where_str_r(buf, &def->where));
			}
			def = cse;
			n--;
			continue;
		}

		vals[i].cse = cse;

		const_fold_need_val(cse->expr, &vals[i].start);

		if(stmt_kind(cse, case_range))
			const_fold_need_val(cse->expr2, &vals[i].end);
		else
			memcpy(&vals[i].end, &vals[i].start, sizeof vals[i].end);

		i++;
	}

	/* sort vals for comparison */
	qsort(vals, n, sizeof(*vals), (qsort_f)intval_cmp); /* struct layout guarantees this */

	for(i = 1; i < n; i++){
		const long last_prev  = vals[i-1].end.val;
		const long first_this = vals[i].start.val;

		if(last_prev >= first_this){
			char buf[WHERE_BUF_SIZ];
			const int overlap = vals[i  ].end.val != vals[i  ].start.val
				               || vals[i-1].end.val != vals[i-1].start.val;

			DIE_AT(&vals[i-1].cse->where, "%s case statements %s %ld (from %s)",
					overlap ? "overlapping" : "duplicate",
					overlap ? "starting at" : "for",
					vals[i].start.val,
					where_str_r(buf, &vals[i].cse->where));
		}
	}

	free(vals);
}

void fold_switch_enum(stmt *sw, type *enum_type)
{
	const int nents = enum_nentries(enum_type->sue);
	stmt **titer;
	char *const marks = umalloc(nents * sizeof *marks);
	int midx;

	/* for each case/default/case_range... */
	for(titer = sw->codes; titer && *titer; titer++){
		stmt *cse = *titer;
		int v, w;
		intval iv;

		if(cse->expr->expr_is_default)
			goto ret;

		const_fold_need_val(cse->expr, &iv);
		v = iv.val;

		if(stmt_kind(cse, case_range)){
			const_fold_need_val(cse->expr2, &iv);
			w = iv.val;
		}else{
			w = v;
		}

		for(; v <= w; v++){
			sue_member **mi;
			for(midx = 0, mi = enum_type->sue->members; *mi; midx++, mi++){
				enum_member *m = (*mi)->enum_member;

				const_fold_need_val(m->val, &iv);

				if(v == iv.val)
					marks[midx]++;
			}
		}
	}

	for(midx = 0; midx < nents; midx++)
		if(!marks[midx])
			cc1_warn_at(&sw->where, 0, 1, WARN_SWITCH_ENUM, "enum %s::%s not handled in switch",
					enum_type->sue->anon ? "" : enum_type->sue->spel,
					enum_type->sue->members[midx]->enum_member->spel);

ret:
	free(marks);
}

void fold_stmt_switch(stmt *s)
{
	type *typ;
	symtable *test_symtab = fold_stmt_test_init_expr(s, "switch");

	s->lbl_break = asm_label_flow("switch");

	fold_expr(s->expr, test_symtab);

	fold_need_expr(s->expr, "switch", 0);

	OPT_CHECK(s->expr, "constant expression in switch");

	/* this folds sub-statements,
	 * causing case: and default: to add themselves to ->parent->codes,
	 * i.e. s->codes
	 */
	fold_stmt(s->lhs);

	/* check for dups */
	fold_switch_dups(s);

	/* check for an enum */
	typ = s->expr->tree_type->type;
	if(typ->primitive == type_enum){
		UCC_ASSERT(typ->sue, "no enum for enum type");
		fold_switch_enum(s, typ);
	}
}

void gen_stmt_switch(stmt *s)
{
	stmt **titer, *tdefault;
	int is_unsigned = !s->expr->tree_type->type->is_signed;

	tdefault = NULL;

	gen_expr(s->expr, s->symtab);
	asm_temp(1, "pop rax ; switch on this");

	for(titer = s->codes; titer && *titer; titer++){
		stmt *cse = *titer;

		UCC_ASSERT(cse->expr->expr_is_default || !(cse->expr->bits.iv.suffix & VAL_UNSIGNED), "don's handle unsigned yet");

		if(stmt_kind(cse, case_range)){
			char *skip = asm_label_code("range_skip");
			intval min, max;

			const_fold_need_val(cse->expr,  &min);
			const_fold_need_val(cse->expr2, &max);

			/* TODO: proper signed/unsiged format */
			asm_temp(1, "cmp rax, %ld", min.val);
			asm_temp(1, "j%s %s", is_unsigned ? "b" : "l", skip);
			asm_temp(1, "cmp rax, %ld", max.val);
			asm_temp(1, "j%se %s", is_unsigned ? "b" : "l", cse->expr->spel);
			asm_label(skip);
			free(skip);
		}else if(cse->expr->expr_is_default){
			tdefault = cse;
		}else{
			/* FIXME: address-of, etc? */
			intval iv;

			const_fold_need_val(cse->expr, &iv);

			asm_temp(1, "cmp rax, %ld", iv.val);
			asm_temp(1, "je %s", cse->expr->spel);
		}
	}

	if(tdefault)
		asm_temp(1, "jmp %s", tdefault->expr->spel);
	else
		asm_temp(1, "jmp %s", s->lbl_break);

	gen_stmt(s->lhs); /* the actual code inside the switch */

	asm_label(s->lbl_break);
}

int switch_passable(stmt *s)
{
	/* this is nowhere near perfect
	 * if we have a default case, passable s->lhs
	 * otherwise, we are passable unless the switch is a full covered enum
	 */
	return fold_passable(s->lhs);
}

void mutate_stmt_switch(stmt *s)
{
	s->f_passable = switch_passable;
}
