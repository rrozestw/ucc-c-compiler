typedef int FILE;
typedef unsigned long long size_t;
#define EOF -1

FILE *fmemopen(void *buf, size_t size, const char *mode);

main()
{
	char buf[123] = { 0 };
	FILE *w = fmemopen(buf, sizeof buf, "w");

	fprintf(w, "hello there %s\n", "timothy");
	fclose(w);

	FILE *r = fmemopen(buf, strlen(buf), "r");
	while(int c = fgetc(r), c != EOF)
		printf("read: '%c'\n", c);
}
