// RUN: %asmcheck %s

struct A
{
	int i, j;
} ar[] = {
	[3] = {
		1, 2
	}
};
