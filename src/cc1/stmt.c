#include <stdio.h>
#include <stdarg.h>

#include "../util/util.h"
#include "../util/alloc.h"
#include "data_structs.h"
#include "stmt.h"

stmt_flow *stmt_flow_new()
{
	stmt_flow *t = umalloc(sizeof *t);
	return t;
}

void stmt_mutate(stmt *s, func_fold_stmt *f_fold, func_gen_stmt *f_gen, func_str_stmt *f_str)
{
	s->f_fold = f_fold;
	s->f_gen  = f_gen;
	s->f_str  = f_str;
}

stmt *stmt_new(func_fold_stmt *f_fold, func_gen_stmt *f_gen, func_str_stmt *f_str, symtable *stab)
{
	stmt *s = umalloc(sizeof *s);
	where_new(&s->where);

	stmt_mutate(s, f_fold, f_gen, f_str);

	UCC_ASSERT(stab, "no symtable for stmtement");
	s->symtab = stab;

	return s;
}

