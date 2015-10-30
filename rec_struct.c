int printf();

typedef struct A A;
struct A
{
	A (*fn)();
};

A f()
{
	printf("f()\n");
	static A a = { .fn = f };
	return a;
}

typedef struct B B;
struct B
{
	B *(*fn)();
};

B *g()
{
	printf("g()\n");
	static B a = { .fn = g };
	return &a;
}

int main()
{
	f().fn().fn().fn().fn();
	g()->fn()->fn()->fn()->fn();
}
