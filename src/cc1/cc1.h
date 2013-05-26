#ifndef CC1_H
#define CC1_H

/*#define FANCY_STACK_INIT 1*/
#define ASM_INLINE_FNAME "__asm__"

enum section_type
{
	SECTION_TEXT,
	SECTION_DATA,
	SECTION_BSS,
	NUM_SECTIONS
};

enum warning
{
	WARN_NONE                     = 0,
	WARN_ARG_MISMATCH             = 1 << 0,
	WARN_ARRAY_COMMA              = 1 << 1,
	WARN_ASSIGN_MISMATCH          = 1 << 2,
	WARN_COMPARE_MISMATCH         = 1 << 3,
	WARN_RETURN_TYPE              = 1 << 4,
	WARN_SIGN_COMPARE             = 1 << 5,
	WARN_EXTERN_ASSUME            = 1 << 6,
	WARN_IMPLICIT_FUNC            = 1 << 7,
	WARN_IMPLICIT_INT             = 1 << 8,
	WARN_VOID_ARITH               = 1 << 9,
	WARN_MIXED_CODE_DECLS         = 1 << 10,
	WARN_TEST_BOOL                = 1 << 11,
	WARN_LOSS_PRECISION           = 1 << 12,

	WARN_OPT_POSSIBLE             = 1 << 13,
	WARN_SWITCH_ENUM              = 1 << 14,
	WARN_ENUM_CMP                 = 1 << 15,
	WARN_INCOMPLETE_USE           = 1 << 16,
	WARN_UNUSED_EXPR              = 1 << 17,
	WARN_TEST_ASSIGN              = 1 << 18,
	WARN_READ_BEFORE_WRITE        = 1 << 19,
	WARN_SYM_NEVER_WRITTEN        = 1 << 20,
	WARN_SYM_NEVER_READ           = 1 << 21,
	WARN_DEAD_CODE                = 1 << 22,
	WARN_PREDECL_ENUM             = 1 << 23,
	WARN_OMITTED_PARAM_TYPES      = 1 << 24,
	WARN_RETURN_UNDEF             = 1 << 25,
	WARN_PAD                      = 1 << 26,

	/* TODO */
	/*
	WARN_FORMAT                   = 1 << 23,
	WARN_INT_TO_PTR               = 1 << 24,
	WARN_PTR_ARITH                = 1 << 25,
	WARN_SHADOW                   = 1 << 26,
	WARN_UNINITIALISED            = 1 << 27,
	WARN_UNUSED_PARAM             = 1 << 28,
	WARN_UNUSED_VAL               = 1 << 29,
	WARN_UNUSED_VAR               = 1 << 30,
	WARN_ARRAY_BOUNDS             = 1 << 31,
	*/
};

enum fopt
{
	FOPT_NONE                  = 0,
	FOPT_ENABLE_ASM            = 1 << 0,
	FOPT_CONST_FOLD            = 1 << 1,
	FOPT_ENGLISH               = 1 << 2,
	FOPT_SHOW_LINE             = 1 << 3,
	FOPT_PIC                   = 1 << 4,
	FOPT_PIC_PCREL             = 1 << 5,
	FOPT_BUILTIN               = 1 << 6,
	FOPT_MS_EXTENSIONS         = 1 << 7,
	FOPT_PLAN9_EXTENSIONS      = 1 << 8,
	FOPT_TAG_ANON_STRUCT_EXT   = FOPT_MS_EXTENSIONS | FOPT_PLAN9_EXTENSIONS,
	FOPT_LEADING_UNDERSCORE    = 1 << 9,
	FOPT_TRAPV                 = 1 << 10,
};

enum cc1_backend
{
	BACKEND_ASM,
	BACKEND_PRINT,
	BACKEND_STYLE,
};

extern enum fopt fopt_mode;
extern enum cc1_backend cc1_backend;

extern enum cc1_std
{
	STD_C89, /* comparable with < */
	STD_C90,
	STD_C99
} cc1_std;

void cc1_warn_atv(struct where *where, int die, int show_line, enum warning w, const char *fmt, va_list l);
void cc1_warn_at( struct where *where, int die, int show_line, enum warning w, const char *fmt, ...) ucc_printflike(5, 6);

extern FILE *cc_out[NUM_SECTIONS];
extern FILE *cc1_out;

extern int cc1_max_errors;

extern int cc1_m32; /* 32bit mode or 64? */
extern int cc1_mstack_align; /* 2^n */

#endif
