struct A
{
	int a, b, c;
};

struct A f(void)
#ifdef IMPL
{
	return (struct A){ 1, 2, 3 };
}
#else
;

main()
{
	struct A a = f();
	return a.a + a.b + a.c;
}
#endif