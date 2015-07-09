typedef struct A {
	char buf[32];
} A;

A f()
{
	return (A){ .buf = 3 };
}

main()
{
	f();
}
