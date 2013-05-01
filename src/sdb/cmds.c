#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>

#include "util.h"
#include "../util/dynarray.h"
#define ARGC(argv) dynarray_count((void **)argv)

#include "arch.h"
#include "tracee.h"
#include "cmds.h"
#include "signal.h"

#define strcmp_lim(a, k) strncmp(a, k, strlen(a))

#define ARG_CHECK(exp) (ARGC(argv) exp) || (argv[1] && !strcmp(argv[1], HELP_ARG))
#define NO_ARGS()             \
	if(ARG_CHECK(!= 1)){        \
		warn("Usage: %s", *argv); \
		return DISPATCH_REPROMPT; \
	}
#define HELP_ARG "-h"

enum { DISPATCH_REPROMPT = 0, DISPATCH_WAIT = 1 };

static int
c_kill(tracee *child, char **argv)
{
	NO_ARGS();

	tracee_kill(child, SIGKILL);
	return DISPATCH_WAIT;
}

int
c_quit(tracee *child, char **argv)
{
	NO_ARGS();

	sdb_exit(child);
}

static int
c_detach(tracee *child, char **argv)
{
	NO_ARGS();

	if(tracee_detach(child))
		warn("detach(%d):", TRACEE_PID(child));

	return DISPATCH_REPROMPT;
}

static int
c_break(tracee *child, char **argv)
{
	if(ARG_CHECK(> 2)){
		warn("Usage: %s [addr]", *argv);
		return DISPATCH_REPROMPT;
	}

	addr_t addr;
	if(argv[1]){
		if(sscanf(argv[1], "0x%lx", &addr) != 1){
			warn("%s isn't an address", argv[1]);
			return DISPATCH_REPROMPT;
		}
	}else if(tracee_get_reg(child, ARCH_REG_IP, &addr)){
		warn("read register ip:");
		return DISPATCH_REPROMPT;
	}

	if(tracee_break(child, addr))
		warn("break:");

	return DISPATCH_REPROMPT;
}

enum examine_type
{
#define TYPE(ch, fmt, nam, cast) nam,
#include "examine_type.def"
#undef TYPE
};

static int
examine_parse_fmt(char *fmt,
		unsigned *pcount, unsigned *psize,
		enum examine_type *p_ty)
{
	/* defaults */
	*pcount = 1, *psize = sizeof(int);
	*p_ty = x_hex;

	if(!fmt)
		return 1;

	char *end;
	if(isdigit(*fmt)){
		errno = 0;
		*pcount = strtol(fmt, &end, 0);
		if(errno)
			return 0;
	}else{
		end = fmt;
	}

	/* type */
	switch(*end){
		default:
			break; /* no type given */
		case '\0': goto done;
#define TYPE(ch, fmt, ty, cast) case ch: *p_ty = ty; end++; break;
#include "examine_type.def"
#undef TYPE
	}

	/* size */
	switch(*end){
		default:
			break; /* no size given */
		case '\0': goto done;
#define SIZE(ch, n) case ch: *psize = n; end++; break
		SIZE('b', 1);
		SIZE('h', 2);
		SIZE('w', 4);
		SIZE('g', 8);
#undef SIZE
	}

	if(*end){
		warn("extra characters at end of format: \"%s\"", end);
		return 0;
	}

done:
	return 1;
}

static int
c_examine(tracee *child, char **argv)
{
	char *fmt = strchr(argv[0], '/');
	if(fmt)
		*fmt++ = '\0';

	if(ARG_CHECK(!= 2)){
		warn("Usage: %s[/fmt] [addr]", *argv);
		warn("  fmt = <count><type><size>");
		warn("  size: Byte, Half, Word, Giant");
		warn("  type: Octal, heX, Decimal, Unsigned, t(binary)");
		warn("        Float, Address, Instruction, Char, String");
		return DISPATCH_REPROMPT;
	}

	errno = 0;
	addr_t addr = strtol(argv[1], NULL, 0 /* auto base */);
	if(errno){
		warn("%s: invalid number '%s' (%s)", argv[1], strerror(errno));
		return DISPATCH_REPROMPT;
	}

	unsigned count, size;
	enum examine_type ty;
	if(!examine_parse_fmt(fmt, &count, &size, &ty))
		return DISPATCH_REPROMPT;

	for(; count > 0; count--){
		word_t val;
		if(arch_mem_read(TRACEE_PID(child), addr, &val)){
			warn("read memory at 0x%x: %s", addr, strerror(errno));
			break;
		}
		printf("0x%lx: ", addr);

		/* we read a full word - trim */
		switch(size){
			case 1: val &= UCHAR_MAX; break;
			case 2: val &= USHRT_MAX; break;
			case 4: val &= UINT_MAX ; break;
			case 8: val &= ULONG_MAX; break;
		}

		switch(ty){
#define TYPE(ch, fmt, ty, cast) case ty: printf(fmt "\n", *(cast *)&val); break;
#include "examine_type.def"
#undef TYPE
		}
		addr += size;
	}

	return DISPATCH_REPROMPT;
}

static int
c_help(tracee *, char **argv);

static int
c_regs_read(tracee *t, char **argv)
{
	NO_ARGS();

	const char **r;
	for(r = arch_reg_names(); *r; r++){
		int i = arch_reg_offset(*r);

		assert(i != -1);

		reg_t v;
		if(arch_reg_read(TRACEE_PID(t), i, &v))
			warn("read reg \"%s\":", *r);
		else
			printf("%s = 0x%lx\n", *r, v);
	}

	return DISPATCH_REPROMPT;
}

static int c_cont(tracee *t, char **argv)
{
	NO_ARGS();

	tracee_continue(t);

	return DISPATCH_WAIT;
}

static int c_step(tracee *t, char **argv)
{
	NO_ARGS();

	tracee_step(t);

	return DISPATCH_WAIT;
}

static int c_reg_read(tracee *t, char **argv)
{
	if(ARG_CHECK(!= 2)){
		warn("Usage: %s register", *argv);
		return DISPATCH_REPROMPT;
	}

	int i = arch_reg_offset(argv[1]);
	if(i == -1){
		warn("unknown register \"%s\"", argv[1]);
		return DISPATCH_REPROMPT;
	}

	reg_t v;
	if(arch_reg_read(TRACEE_PID(t), i, &v)){
		warn("reg read:");
		return DISPATCH_REPROMPT;
	}

	printf("0x%lx\n", v);

	return DISPATCH_REPROMPT;
}

static int c_reg_write(tracee *t, char **argv)
{
	if(ARG_CHECK(!= 3)){
		warn("Usage: %s register value", *argv);
		return DISPATCH_REPROMPT;
	}

	int i = arch_reg_offset(argv[1]);
	if(i == -1){
		warn("unknown register \"%s\"", argv[1]);
		return DISPATCH_REPROMPT;
	}

	reg_t v;
	if(sscanf(argv[2], "%lu", &v) != 1){
		warn("%s isn't a register value", argv[2]);
		return DISPATCH_REPROMPT;
	}

	if(arch_reg_write(TRACEE_PID(t), i, v))
		warn("reg write:");

	return DISPATCH_REPROMPT;
}

static int
c_sig(tracee *child, char **argv)
{
	/* e.g. sig TRAP USR1 stop pass
	 *      sig INT send
	 */
	if(ARG_CHECK(< 2)){
usage:
		warn("Usage: %s send SIG\n"
				 "       %s cont SIG\n"
				 "       %s print|stop|pass... SIG...",
				 *argv, *argv, *argv);
		return DISPATCH_REPROMPT;
	}

	const int argc = ARGC(argv);
	if(!strcmp_lim(argv[1], "send") || !strcmp_lim(argv[1], "continue")){
		if(argc != 3)
			goto usage;

		int sig = sig_parse(argv[2]);
		if(sig == -1){
			warn("bad signal to send, \"%s\"", argv[2]);
			goto out;
		}

		(*argv[1] == 'c' ? tracee_cont : tracee_kill)(child, sig);
		return DISPATCH_WAIT;

	}else{
		handle_mask mask_add = 0, mask_rm = 0;

		int i;
		for(i = 1; i < argc; i++){
			int no;

			no = !strncmp(argv[i], "no", 2);

#define CHECK(str, en)              \
			if(!strcmp_lim(argv[i], str)) \
				*(no ? &mask_rm : &mask_add) |= HANDLE_ ## en

			/**/ CHECK("print", PRINT);
			else CHECK("stop",  STOP);
			else CHECK("pass",  HEREDITARY);
			else break;
#undef CHECK
		}

		if(i == argc){
			warn("no signals to handle");
			goto usage;
		}

		for(; i < argc; i++){
			int sig = sig_parse(argv[i]);
			if(sig == -1){
				warn("couldn't parse \"%s\"", argv[i]);
				goto out;
			}

			sig_handle_mask_set(
					sig,
					(sig_handle_mask(sig) | mask_add) & ~mask_rm);
		}
	}

out:
	return DISPATCH_REPROMPT;
}

static const struct dispatch
{
	const char *s;
	int (*f)(tracee *, char **);
	int need_alive;
} cmds[] = {
	{ "quit",   c_quit,           0 },
	{ "help",   c_help,           0 },
	{ "break",  c_break,          1 }, /* TODO: lazy */
	{ "x",      c_examine,        1 },
	{ "rall",   c_regs_read,      1 },
	{ "rr"  ,   c_reg_read,       1 },
	{ "rw"  ,   c_reg_write,      1 },
	{ "kill",   c_kill,           1 },
	{ "cont",   c_cont,           1 },
	{ "step",   c_step,           1 },
	{ "signal", c_sig,            1 }, /* TODO: lazy */
	{ "detach", c_detach,         1 },

	{ NULL }
};

static int
c_help(tracee *child, char **argv)
{
	if(ARG_CHECK(!= 2)){
		printf("available commands:\n");
		for(int i = 0; cmds[i].s; i++)
			printf("  %s\n", cmds[i].s);
	}else{
		/* two args - "help cmd" */
		cmd_dispatch(child, (char *[]){ argv[1], HELP_ARG, NULL });
	}

	return DISPATCH_REPROMPT;
}

int
cmd_dispatch(tracee *child, char **inp)
{
	/* TODO: parse cmd, tab completion, shortened recognition (e.g. "disas") */
	char *slash = strchr(inp[0], '/');
	const size_t len = slash ? (unsigned)(slash - inp[0]) : strlen(*inp);
	const struct dispatch *found = NULL;
	int ret = DISPATCH_REPROMPT;

	if(len == 0)
		return DISPATCH_REPROMPT;

	for(int i = 0; cmds[i].s; i++)
		if(!strncmp(cmds[i].s, inp[0], len)){
			if(found){
				warn("ambiguous command \"%s\"", inp[0]);
				warn("first two: \"%s\" and \"%s\"", cmds[i].s, found->s);
				return DISPATCH_REPROMPT;
			}
			found = &cmds[i];
		}

	if(found){
		// FIXME: remove alive
		if(found->need_alive && !tracee_alive(child))
			printf("child isn't running, can't \"%s\"\n", found->s);
		else
			ret = found->f(child, inp);
	}else{
		printf("command \"%s\" not found\n", inp[0]);
	}

	return ret;
}
