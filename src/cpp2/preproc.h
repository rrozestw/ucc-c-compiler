#ifndef PREPROC_H
#define PREPROC_H

struct macro
{
	char *spel;
	char *replace;
};

struct file_stack
{
	FILE *file;
	char *fname;
	int line_no;
	int is_sysh;
};

enum lineinfo
{
	LINEINFO_START_OF_FILE = 1 << 1,
	LINEINFO_RETURN_TO_FILE = 1 << 2,
	LINEINFO_SYSHEADER = 1 << 3
	/* LINEINFO_TREAT_AS_C = 1 << 4 - not used: extern "C" */
};

extern struct file_stack file_stack[];
extern int file_stack_idx;

void preprocess(void);
void preproc_push(FILE *f, const char *fname, int is_sysh);
int preproc_in_include(void);

void preproc_emit_line_info(int lineno, const char *fname, enum lineinfo);

#endif
