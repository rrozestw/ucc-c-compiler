// RUN: %ucc -c -o %t %s

__attribute((always_inline))
int f(int i)
{
a:
	g(&i);
	if(i == 7)
		goto a;
	return 3 + i;
}

main()
{
	return f(2);
}
