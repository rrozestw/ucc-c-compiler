// RUN: %ocheck 0 %s
// shouldn't clobber the pointer with the sub operation
f(int *p)
{
	--*p;
}

main()
{
	int i = 5;
	f(&i);
	if(i != 4)
		abort();
	return 0;
}
