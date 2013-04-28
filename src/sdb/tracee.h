#ifndef TRACEE_H
#define TRACEE_H

#include <sys/types.h>
#include "breakpoint.h"

typedef struct tracee
{
	pid_t pid;
	enum
	{
		TRACEE_BREAK,
		TRACEE_SIGNALED,
		TRACEE_EXITED,
		TRACEE_KILLED,
	} event;

	union
	{
		int sig;
		int exit_code;
		bkpt *bkpt;
	} evt;

	bkpt **bkpts;

	int attached_to; /* did we attach or create? - used at exit */

} tracee;

void tracee_traceme(void);

pid_t tracee_create(tracee *t);
int   tracee_attach(tracee *t, pid_t);
int   tracee_leave( tracee *t);

void  tracee_wait(tracee *t, reg_t *p_ip);

void  tracee_kill(tracee *t, int sig);
void  tracee_cont(tracee *t, int sig); /* continue with signal */
int   tracee_alive(tracee *t);

void  tracee_continue(tracee *t);
void  tracee_step(tracee *t);

int tracee_break(tracee *t, addr_t);

int tracee_get_reg(tracee *t, enum pseudo_reg r, reg_t *p);
int tracee_set_reg(tracee *t, enum pseudo_reg r, const reg_t v);

#endif
