// RUN: %check %s
// RUN: %layout_check %s

int g[2] = { 1, 2, 3 }; // CHECK: warning: excess initialiser for 'int[2]'

struct A
{
	int i, j;
} as[2] = {
	{ 1, 2 },
	{ 3, 4 },
	{ 5, 6 } // CHECK: warning: excess initialiser for 'struct A[2]'
};

main()
{
	int x[5] = { 1, 2 };    // CHECK: !/warn/
	int k[] = { 1, 2, 3 };  // CHECK: !/warn/
	char y[2] = { 1, 2, 3, 4 }; // CHECK: warning: excess initialiser for 'char[2]'
}
