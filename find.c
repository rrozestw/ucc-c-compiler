#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dirent.h>

static void die(const char *fmt, ...)
	__attribute((format(printf, 1, 2)))
{
	va_list l;
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);

	if(*fmt && fmt[strlen(fmt)-1] == ':'){
		fputc(' ', stderr);
		perror(NULL);
	}else{
		fputc('\n', stderr);
	}

	exit(1);
}

static void find(const char *path)
{
	DIR *d = opendir(path);

	if(!d)
		die("opendir %s:", path);

	while(struct dirent *ent = readdir(d)){
		if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;

		size_t l = strlen(path) + strlen(ent->d_name) + 2;
		char buf[l];

		snprintf(buf, l, "%s/%s", path, ent->d_name);

		struct stat st;
		char *post = "";
		if(stat(buf, &st))
			die("stat %s:", buf);

		if(S_ISDIR(st.st_mode))
			post = "/";

		printf("%s%s\n", buf, post);

		if(*post)
			find(buf);
	}
	closedir(d);
}

int main(int argc, char **argv)
{
	if(argc > 1)
		for(int i = 1; i < argc; i++)
			find(argv[i]);
	else
		find(".");

	return 0;
}
