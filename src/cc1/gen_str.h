#ifndef GEN_STR_H
#define GEN_STR_H

enum pdeclargs
{
	PDECL_NONE          = 0,
	PDECL_INDENT        = 1 << 0,
	PDECL_NEWLINE       = 1 << 1,
	PDECL_SYM_OFFSET    = 1 << 2,
	PDECL_FUNC_DESCEND  = 1 << 3,
	PDECL_PISDEF        = 1 << 4,
	PDECL_PINIT         = 1 << 5,
	PDECL_SIZE          = 1 << 6,
	PDECL_ATTR          = 1 << 7
};

void idt_printf(const char *fmt, ...) ucc_printflike(1, 2);
void idt_print(void);

void print_decl(decl *d, enum pdeclargs);
void print_type_ref(type_ref *ref, decl *d);

void print_stmt(stmt *t);
void print_expr(expr *e);

void gen_str(symtable_global *);

extern int gen_str_indent;

#endif
