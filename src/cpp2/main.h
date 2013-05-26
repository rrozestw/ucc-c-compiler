#ifndef MAIN_H
#define MAIN_H

void dirname_push(char *d);
char *dirname_pop(void);

extern const char *current_fname;

extern char **cd_stack;

extern char cpp_time[16], cpp_date[16];

extern int option_line_info;

extern const char *current_fname;
extern int current_line;


#define CPP_X(f, ...)      \
	do{                      \
		current_line--;        \
		f(NULL, __VA_ARGS__);  \
		current_line++;        \
	}while(0)

#define CPP_WARN(...) CPP_X(WARN_AT, __VA_ARGS__)
#define CPP_DIE(... ) CPP_X(DIE_AT,  __VA_ARGS__)

#endif
