struct __attribute__((aligned(64))) Aligned
{
	int b;
};

struct A
{
	int a;
	struct Aligned b;
} a = { 1, 2 };
