// RUN: %ocheck 10 %s

typedef struct A { long i, j, k; } A;

__attribute((always_inline))
inline A f(int k, int cond)
{
	if(cond){
		A local = { .i = 99, .k = k };
		return local;
	}
	return (A){ .i = 3, .k = k };
}

main()
{
	if(f(1, 1).i != 99)
		abort();

	if(f(1, 1).i + f(5, 1).k != 104)
		abort();

	// 3 + 7
	return f(2941, 0).i + f(7, 0).k;
}
