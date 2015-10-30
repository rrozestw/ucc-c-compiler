typedef void fn();
typedef fn *fnp;

fnp f()
{
	printf("f()\n");
	return (fnp)f;
}

main()
{
	((fnp)((fnp)((fnp)((fnp)f()())())())())();
	printf("done main\n");
}
