#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

/* umask */
#include <sys/types.h>
#include <sys/stat.h>

#include "cfg.h"

#include "ucc.h"
#include "ucc_ext.h"
#include "ucc_lib.h"
#include "../util/alloc.h"
#include "../util/dynarray.h"
#include "../util/util.h"
#include "../util/platform.h"
#include "../util/tmpfile.h"
#include "str.h"
#include "warning.h"

enum mode
{
	mode_preproc,
	mode_compile,
	mode_assemb,
	mode_link
};
#define MODE_ARG_CH(m) ("ESc\0"[m])

struct
{
	enum mode assume;
	int nostdlib;
	int nostartfiles;
} gopts;


#define FILE_UNINIT -2
struct cc_file
{
	struct fd_name_pair
	{
		char *fname;
		int fd;
	} in;

	struct fd_name_pair preproc, compile, assemb;

	struct fd_name_pair out;

	int preproc_asm;
};
#define FILE_IN_MODE(f)        \
((f)->preproc.fname ? mode_preproc : \
 (f)->compile.fname ? mode_compile : \
 (f)->assemb.fname  ? mode_assemb  : \
 mode_link)

static char **remove_these;
static int unlink_tmps = 1;
static int gdebug = 0, generated_temp_obj = 0;
const char *argv0;
char *wrapper;
int fsystem_cpp;

static void unlink_files(void)
{
	int i;
	for(i = 0; remove_these[i]; i++){
		if(unlink_tmps)
			remove(remove_these[i]);
		free(remove_these[i]);
	}
	free(remove_these);
}

static void tmpfilenam(struct fd_name_pair *pair)
{
	char *path;
	int fd = tmpfile_prefix_out("ucc.", &path);

	if(fd == -1)
		die("tmpfile(%s):", path);

	if(!remove_these) /* only register once */
		atexit(unlink_files);

	dynarray_add(&remove_these, path);

	pair->fname = path;
	pair->fd = fd;
}

static void create_file(struct cc_file *file, enum mode mode, char *in)
{
	char *ext;

	file->in.fname = in;
	file->in.fd = FILE_UNINIT;

	switch(gopts.assume){
		case mode_preproc:
			goto preproc;
		case mode_compile:
			goto compile;
		case mode_assemb:
			goto assemb;
		case mode_link:
			goto assume_obj;
	}

#define FILL_WITH_TMP(x)         \
			if(!file->x.fname){        \
				tmpfilenam(&file->x);    \
				if(mode == mode_ ## x){  \
					file->out = file->x;   \
					return;                \
				}                        \
			}

	ext = strrchr(in, '.');
	if(ext && ext[1] && !ext[2]){
		switch(ext[1]){
preproc:
			case 'c':
				FILL_WITH_TMP(preproc);
compile:
			case 'i':
				FILL_WITH_TMP(compile);
				goto after_compile;
assemb:
			case 'S':
				file->preproc_asm = 1;
				FILL_WITH_TMP(preproc); /* preprocess .S assembly files by default */
after_compile:
			case 's':
				if(NEED_DSYM && !file->assemb.fname)
					generated_temp_obj = 1;

				FILL_WITH_TMP(assemb);
				file->out = file->assemb;
				break;

			default:
			assume_obj:
				fprintf(stderr, "assuming \"%s\" is object-file\n", in);
			case 'o':
			case 'a':
				/* else assume it's already an object file */
				file->out = file->in;
		}
	}else{
		if(!strcmp(in, "-"))
			goto preproc;
		goto assume_obj;
	}
}

static void gen_obj_file(struct cc_file *file, char **args[4], enum mode mode)
{
	char *in = file->in.fname;

	if(file->preproc.fname){
		/* if we're preprocessing, but not cc1'ing, but we are as'ing,
		 * it's an assembly language file */
		if(file->preproc_asm)
			dynarray_add(&args[mode_preproc], ustrdup("-D__ASSEMBLER__=1"));

		preproc(in, file->preproc.fname, args[mode_preproc]);

		in = file->preproc.fname;
	}

	if(mode == mode_preproc)
		return;

	if(file->compile.fname){
		compile(in, file->compile.fname, args[mode_compile]);

		in = file->compile.fname;
	}

	if(mode == mode_compile)
		return;

	if(file->assemb.fname){
		assemble(in, file->assemb.fname, args[mode_assemb]);
	}
}

static void rename_files(struct cc_file *files, int nfiles, char *output, enum mode mode)
{
	const char mode_ch = MODE_ARG_CH(mode);
	int i;

	for(i = 0; i < nfiles; i++){
		/* path/to/input.c -> input.[os]
		 * directory names all trimmed */
		char *new;

		if(mode < FILE_IN_MODE(&files[i])){
			fprintf(stderr, "input \"%s\" unused with -%c present\n",
					files[i].in.fname, mode_ch);
			continue;
		}

		if(output){
			if(mode == mode_preproc){
				/* append to current */
				if(files[i].preproc.fname)
					cat(files[i].preproc.fname, output, i);
				continue;
			}else if(mode == mode_compile && !strcmp(output, "-")){
				/* -S -o- */
				if(files[i].compile.fname)
					cat(files[i].compile.fname, NULL, 0);
				continue;
			}

			new = ustrdup(output);
		}else{
			switch(mode){
				case mode_link:
					die("mode_link for multiple files");

				case mode_preproc:
					if(files[i].preproc.fname)
						cat(files[i].preproc.fname, NULL, 0);
					continue;

				case mode_compile:
				case mode_assemb:
				{
					int len;
					const char *base = strrchr(files[i].in.fname, '/');
					if(!base++)
						base = files[i].in.fname;

					new = ustrdup(base);
					len = strlen(new);
					if(len > 2 && new[len - 2] == '.')
						new[len - 1] = mode == mode_compile ? 's' : 'o';
					/* else stick with what we were given */
					break;
				}
			}
		}

		rename_or_move(files[i].out.fname, new);

		free(new);
	}
}

static void process_files(enum mode mode, char **inputs, char *output, char **args[4], char *backend)
{
	const int ninputs = dynarray_count(inputs);
	int i;
	struct cc_file *files;
	char **links = NULL;

	files = umalloc(ninputs * sizeof *files);

	/* crt must come first */
	if(!gopts.nostartfiles)
		dynarray_add_array(&links, ld_crt_args());


	if(backend){
		dynarray_add(&args[mode_compile], ustrprintf("-emit=%s", backend));
	}

	for(i = 0; i < ninputs; i++){
		create_file(&files[i], mode, inputs[i]);

		gen_obj_file(&files[i], args, mode);

		dynarray_add(&links, ustrdup(files[i].out.fname));
	}

	if(mode == mode_link){
		/* An object file's unresolved symbols must
		 * be _later_ in the linker's argv array.
		 * crt, user files, then stdlib
		 */
		if(!gopts.nostdlib)
			/* ld_crt_args() refers to static memory */
			dynarray_add_array(&links, ld_stdlib_args());

		if(!output)
			output = "a.out";

		link_all(links, output, args[mode_link]);

		if(NEED_DSYM && gdebug && generated_temp_obj){
			/* only need dsym if we use temporary .o files */
			dsym(output);
		}
	}else{
		rename_files(files, ninputs, output, mode);
	}

	dynarray_free(char **, links, free);

	for(i = 0; i < ninputs; i++){
		close(files[i].in.fd);
		close(files[i].preproc.fd);
		close(files[i].compile.fd);
		close(files[i].assemb.fd);
	}

	free(files);
}

ucc_dead
void die(const char *fmt, ...)
{
	const int len = strlen(fmt);
	const int err = len > 0 && fmt[len - 1] == ':';
	va_list l;

	fprintf(stderr, "%s: ", argv0);

	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);

	if(err)
		fprintf(stderr, " %s", strerror(errno));

	fputc('\n', stderr);

	exit(1);
}

void ice(const char *f, int line, const char *fn, const char *fmt, ...)
{
	va_list l;
	fprintf(stderr, "ICE: %s:%d:%s: ", f, line, fn);
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
	abort();
}

static void add_cfg_args(char ***par, const char *args)
{
	dynarray_add_tmparray(par, strsplit(args, " "));
}

static void print_search_dirs_and_exit(
		char **includes, const char **isystems, int stdinc)
{
	char **i;
	const char **ki;
	const char *sep = "";

	/* empty - no runtime libraries by default, just ld's defaults */
	printf("libraries:\n");

	printf("include: ");

	if(stdinc){
		printf("%s", UCC_INC);

		if(*UCC_INC)
			sep = ":";
	}

	for(i = includes; i && *i; i++, sep = ":")
		printf("%s%s", sep, *i + /*-I*/2);

	for(ki = isystems; ki && *ki; ki++, sep = ":")
		printf("%s%s", sep, *ki);

	putchar('\n');

	exit(0);
}

static void pass_warning(char **args[4], const char *arg)
{
	enum warning_owner owner;

	assert(!strncmp(arg, "-W", 2));
	owner = warning_owner(arg + 2);

	switch((int)owner){
		case W_OWNER_CPP:
			dynarray_add(&args[mode_preproc], ustrdup(arg));
			break;
		case W_OWNER_CC1:
		default: /* give to cc1 by default - it has the -Werror=unknown-warning logic */
			dynarray_add(&args[mode_compile], ustrdup(arg));
			break;
		case W_OWNER_CPP | W_OWNER_CC1:
			dynarray_add(&args[mode_preproc], ustrdup(arg));
			dynarray_add(&args[mode_compile], ustrdup(arg));
			break;
	}
}

int main(int argc, char **argv)
{
	enum mode mode = mode_link;
	int i, syntax_only = 0;
	int stdinc = 1;
	int print_search_dirs = 0;
	char **includes = NULL;
	char **inputs = NULL;
	char **args[4] = { 0 };
	char *output = NULL;
	char *backend = NULL;
	const char **isystems = NULL;

	umask(0077); /* prevent reading of the temporary files we create */

	gopts.assume = -1;
	argv0 = argv[0];

	if(argc <= 1){
		fprintf(stderr, "Usage: %s [options] input(s)\n", *argv);
		return 1;
	}

	/* we don't want the initial temporary fname "/tmp/tmp.xyz" tracked
	 * or showing up in error messages
	 */
	dynarray_add(&args[mode_compile], ustrdup("-fno-track-initial-fname"));

	/* bring in CPPFLAGS and CFLAGS */
	add_cfg_args(&args[mode_compile], UCC_CFLAGS);
	add_cfg_args(&args[mode_preproc], UCC_CPPFLAGS);


	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "--")){
			while(++i < argc)
				dynarray_add(&inputs, argv[i]);
			break;

		}else if(*argv[i] == '-'){
			int found = 0;
			char *arg = argv[i];

			switch(arg[1]){
#define ADD_ARG(to) dynarray_add(&args[to], ustrdup(arg))

				case 'W':
				{
					/* check for W%c, */
					char c;
					if(sscanf(arg, "-W%c", &c) == 1 && arg[3] == ','){
						/* split by commas */
						char **entries = strsplit(arg + 4, ",");

						switch(c){
#define MAP(c, to) case c: dynarray_add_tmparray(&args[to], entries); break
							MAP('p', mode_preproc);
							MAP('c', mode_compile);
							MAP('a', mode_assemb);
							MAP('l', mode_link);
#undef MAP
							default:
								fprintf(stderr, "argument \"%s\" assumed to be for cc1\n", arg);
								dynarray_add_tmparray(&args[mode_compile], entries);
						}
						continue;
					}

					/* else pass to cc1 and possibly cpp */
					pass_warning(args, arg);
					continue;
				}

				case 'f':
					/* fopts for ucc: */
					if(!strcmp(arg, "-fsyntax-only")){
						syntax_only = 1;
						continue;
					}
					if(!strcmp(argv[i], "-fsystem-cpp")){
						fsystem_cpp = 1;
						continue;
					}

					/* pull out some that cpp wants too: */
					if(!strcmp(argv[i], "-ffreestanding")
					|| !strncmp(argv[i], "-fmessage-length=", 17))
					{
						/* preproc gets this too */
						ADD_ARG(mode_preproc);
					}

					/* pass the rest onto cc1 */
					ADD_ARG(mode_compile);
					continue;

				case 'w':
					if(argv[i][2])
						goto word; /* -wabc... */
					ADD_ARG(mode_preproc); /* -w */
				case 'm':
					ADD_ARG(mode_compile);
					continue;

				case 'D':
				case 'U':
					found = 1;
				case 'P':
arg_cpp:
					ADD_ARG(mode_preproc);
					if(!strcmp(argv[i] + 1, "MM"))
						mode = mode_preproc; /* cc -MM *.c stops after preproc */

					if(found){
						if(!arg[2]){
							/* allow a space, e.g. "-D" "arg" */
							if(!(arg = argv[++i]))
								goto missing_arg;
							ADD_ARG(mode_preproc);
						}
					}
					continue;
				case 'I':
					if(arg[2]){
						dynarray_add(&includes, ustrdup(arg));
					}else{
						if(!argv[++i])
							die("-I needs an argument");

						dynarray_add(&includes, ustrprintf("-I%s", argv[i]));
					}
					continue;

				case 'l':
				case 'L':
arg_ld:
					ADD_ARG(mode_link);
					continue;

#define CHECK_1() if(argv[i][2]) goto unrec;
				case 'E': CHECK_1(); mode = mode_preproc; continue;
				case 'S': CHECK_1(); mode = mode_compile; continue;
				case 'c': CHECK_1(); mode = mode_assemb;  continue;

				case 'o':
					if(argv[i][2]){
						output = argv[i] + 2;
					}else{
						output = argv[++i];
						if(!output)
							goto missing_arg;
					}
					continue;

				case 'O':
					ADD_ARG(mode_compile);
					ADD_ARG(mode_preproc); /* __OPTIMIZE__, etc */
					continue;

				case 'g':
					/* debug */
					if(argv[i][2])
						die("-g... unexpected");
					ADD_ARG(mode_compile);
					gdebug = 1;
					/* don't pass to the assembler */
					continue;

				case 'M':
				case 'd':
				case 'C': /* -C and -CC */
					goto arg_cpp;

				case 'x':
				{
					const char *arg;
					/* prevent implicit assumption of source */
					if(argv[i][2])
						arg = argv[i] + 2;
					else if(!argv[++i])
						goto missing_arg;
					else
						arg = argv[i];

					/* TODO: "asm-with-cpp"? */
					/* TODO: order-sensitive -x */
					if(!strcmp(arg, "c"))
						gopts.assume = mode_preproc;
					else if(!strcmp(arg, "cpp-output"))
						gopts.assume = mode_compile;
					else if(!strcmp(arg, "asm"))
						gopts.assume = mode_assemb;
					else if(!strcmp(arg, "none"))
						gopts.assume = -1; /* reset */
					else
						die("-x accepts \"c\", \"cpp-output\", \"asm\" "
								"or \"none\", not \"%s\"", arg);
					continue;
				}

				case '\0':
					/* "-" aka stdin */
					goto input;

word:
				default:
					if(!strcmp(argv[i], "-s"))
						goto arg_ld;

					if(!strncmp(argv[i], "-std=", 5) || !strcmp(argv[i], "-ansi")){
						ADD_ARG(mode_compile);
						ADD_ARG(mode_preproc);
					}
					else if(!strcmp(argv[i], "-pedantic"))
						ADD_ARG(mode_compile);
					else if(!strcmp(argv[i], "-nostdlib"))
						gopts.nostdlib = 1;
					else if(!strcmp(argv[i], "-nostartfiles"))
						gopts.nostartfiles = 1;
					else if(!strcmp(argv[i], "-nostdinc"))
						stdinc = 0;
					else if(!strcmp(argv[i], "-###"))
						ucc_ext_cmds_show(1), ucc_ext_cmds_noop(1);
					else if(!strcmp(argv[i], "-v"))
						ucc_ext_cmds_show(1);
					else if(!strncmp(argv[i], "-emit", 5)){
						switch(argv[i][5]){
							case '=':
								backend = argv[i] + 6;
								break;
							case '\0':
								if(++i == argc)
									goto missing_arg;

								backend = argv[i];
								break;
							default:
								goto unrec;
						}
					}
					else if(!strcmp(argv[i], "-wrapper")){
						/* -wrapper echo,-n etc */
						wrapper = argv[++i];
						if(!wrapper)
							goto missing_arg;
					}
					else if(!strcmp(argv[i], "-trigraphs"))
						ADD_ARG(mode_preproc);
					else if(!strcmp(argv[i], "-digraphs"))
						ADD_ARG(mode_preproc);
					else if(!strcmp(argv[i], "-save-temps"))
						unlink_tmps = 0;
					else if(!strcmp(argv[i], "-isystem")){
						const char *sysinc = argv[++i];
						if(!sysinc)
							goto missing_arg;
						dynarray_add(&isystems, sysinc);
					}
					else if(!strcmp(argv[i], "-print-search-dirs"))
						print_search_dirs = 1;
					else
						break;

					continue;
			}

unrec:
			die("unrecognised option \"%s\"", argv[i]);
missing_arg:
			die("need argument for %s", argv[i - 1]);
		}else{
input:
			dynarray_add(&inputs, argv[i]);
		}
	}

	if(print_search_dirs){
		print_search_dirs_and_exit(includes, isystems, stdinc);
	}


	{
		const int ninputs = dynarray_count(inputs);

		if(output && ninputs > 1 && (mode == mode_compile || mode == mode_assemb))
			die("can't specify '-o' with '-%c' and an output", MODE_ARG_CH(mode));

		if(syntax_only){
			if(output || mode != mode_link)
				die("-%c specified in syntax-only mode",
						output ? 'o' : MODE_ARG_CH(mode));

			mode = mode_compile;
			output = "/dev/null";
		}

		if(ninputs == 0)
			die("no inputs");
	}


	if(output && mode == mode_preproc && !strcmp(output, "-"))
		output = NULL;
	/* other case is -S, which is handled in rename_files */

	if(backend){
		/* -emit=... stops early */
		mode = mode_compile;
		if(!output)
			output = "-";
	}

	/* default include paths */
	if(stdinc){
		char *const dup = ustrdup(UCC_INC);
		char *p;

		for(p = strtok(dup, ":"); p; p = strtok(NULL, ":")){
			char *inc = ustrprintf("-I%s", p);
			dynarray_add(&args[mode_preproc], inc);

			/* cc1 needs include paths so it knows system headers
			 * (for warning suppression) */
			dynarray_add(&args[mode_compile], ustrdup(inc));
		}

		free(dup);
	}
	if(isystems){
		const char **i;
		for(i = isystems; *i; i++){
			/* cpp doesn't care if it's system or not */
			dynarray_add(&args[mode_preproc], ustrprintf("-I%s", *i));
			/* cc1 only wants system - can use -I too */
			dynarray_add(&args[mode_compile], ustrprintf("-I%s", *i));
		}

		dynarray_free(const char **, isystems, NULL);
	}

	/* custom include paths */
	if(includes)
		dynarray_add_tmparray(&args[mode_preproc], includes);

	/* got arguments, a mode, and files to link */
	process_files(mode, inputs, output, args, backend);

	for(i = 0; i < 4; i++)
		dynarray_free(char **, args[i], free);
	dynarray_free(char **, inputs, NULL);

	return 0;
}
