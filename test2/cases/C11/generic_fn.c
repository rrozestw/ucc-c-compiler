// RUN: %ocheck 0 %s
funcs()
{
	if(_Generic((0, funcs), int (*)(): 0, char(*)(): 1) != 0)
		abort();
}

main()
{
	funcs();
	return 0;
}
