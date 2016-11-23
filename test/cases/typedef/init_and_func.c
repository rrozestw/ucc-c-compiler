// RUN: %check -e %s

typedef int f(void) // CHECK: error: typedef storage on function
{
	return 3;
}

typedef char c = 3; // CHECK: error: initialised typedef

main()
{
	int *p = (__typeof(*p))0; // can't check here - we think p is used uninit

	(void)p;

	for(int _Alignas(long) x = 0; x; x++); // CHECK: !/warn/

	return 0;
}
