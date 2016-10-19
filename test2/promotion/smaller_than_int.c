// RUN: %check %s

main()
{
	unsigned short a = 1, b = 2;

	return a * b; // CHECK: warning: operands promoted to int for '*'
}
