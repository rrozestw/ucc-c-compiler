global;
f(int a, int b)
{
	int i = a + b;
	global = i;
}

main()
{
	f(3, 4);
}
