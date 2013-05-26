// RUN: %ucc %s -o %t
// RUN: %t
// RUN: %check %s

struct A
{
	struct A *(*f)(int);
} *ret_me(int i, int j)
{
	static struct A a;
	static int init;

	if(!init){
		init = 1;
		a.f = ret_me; // CHECK: /warning: assignment type mismatch/
	}

	printf("%d\n", i);

	return &a;
}

main()
{
	ret_me(1, 2)->f(2)->f(3)->f(4)->f(5);
	return 0;
}
