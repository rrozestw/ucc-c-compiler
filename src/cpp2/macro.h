#ifndef MACRO_H
#define MACRO_H

/*#define EVAL_DEBUG*/

typedef struct
{
	char *nam, *val;
	enum { MACRO, FUNC, VARIADIC } type;
	char **args;
	int blue; /* being evaluated? */
	int use_cnt; /* track usage for double-eval */
} macro;

macro *macro_add(     const char *nam, const char *val);
macro *macro_add_func(const char *nam, const char *val,
		char **args, int variadic);

macro *macro_find(const char *sp);
void   macro_remove(const char *nam);

extern macro **macros;

#endif
