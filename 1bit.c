typedef unsigned long size_t;

enum { false, true };
typedef _Bool bool;

typedef struct A
{
	size_t a : 63;
	bool b : 1;
} A;

int printf(const char *, ...)
	__attribute__((format(printf, 1, 2)));

void init(A *p, size_t n, bool x)
{
	p->a = n;
	p->b = x;
}

int main()
{
	A a;

	init(&a, 10, false);

	printf("%zu %s\n", a.a, a.b ? "true" : "false");

	a.b = !a.b;

	printf("%zu %s\n", a.a, a.b ? "true" : "false");

	a.a++;

	printf("%zu %s\n", a.a, a.b ? "true" : "false");

	a.b = true;
	printf("%zu %s\n", a.a, a.b ? "true" : "false");
}
