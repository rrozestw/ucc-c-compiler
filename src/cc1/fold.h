#ifndef FOLD_H
#define FOLD_H

/* basic folding */
void fold_decl_global(decl *d, symtable *stab);
void fold_decl_global_init(decl *d, symtable *stab);

void fold_merge_tenatives(symtable *stab);

void fold_decl(decl *d, symtable *stab, stmt **pinit_code);

void fold_type_ref(type_ref *r, type_ref *parent, symtable *stab);

void fold_check_restrict(expr *lhs, expr *rhs, const char *desc, where *w);

void fold_funcargs(funcargs *fargs, symtable *stab, type_ref *from);

#define fold_stmt_in_switch(t) fold_stmt((t)->lhs) /* compound */

/* cast insertion */
void fold_insert_casts(type_ref *tlhs, expr **prhs, symtable *stab);

int fold_type_chk_warn(
		type_ref *lhs, type_ref *rhs,
		where *w, const char *desc);

void fold_type_chk_and_cast(
		type_ref *lhs, expr **prhs,
		symtable *stab, where *w,
		const char *desc);

/* struct / enum / integral checking */
enum fold_chk
{
	FOLD_CHK_NO_ST_UN    = 1 << 0, /* e.g. struct A + ... */
	FOLD_CHK_NO_BITFIELD = 1 << 1, /* e.g. &, sizeof */
	FOLD_CHK_BOOL        = 1 << 2, /* e.g. if(...) */
	FOLD_CHK_INTEGRAL    = 1 << 3, /* e.g. switch(...) */
	FOLD_CHK_ALLOW_VOID  = 1 << 4,
	FOLD_CHK_CONST_I     = 1 << 5, /* e.g. case (...): */
};
void fold_check_expr(expr *e, enum fold_chk, const char *desc);

/* expression + statement folding */
expr *fold_expr_decay(expr *e, symtable *stab) ucc_wur;
void fold_expr(expr *e, symtable *stab);
#define FOLD_EXPR(e, stab) ((e) = fold_expr_decay((e), (stab)))
#define fold_expr_no_decay fold_expr

void fold_stmt(stmt *t);

sym *fold_inc_writes_if_sym(expr *e, symtable *stab);

/* flow */
int fold_passable(stmt *s);
int fold_passable_yes(stmt *s);
int fold_passable_no( stmt *s);

extern decl *curdecl_func;
extern type_ref *curdecl_ref_func_called;
extern int fold_had_error;

#ifdef SYMTAB_DEBUG
#  define PRINT_STAB(st, cur) print_stab(st->symtab, cur, &st->where)
void print_stab(symtable *st, int current, where *w);
#endif

#endif
