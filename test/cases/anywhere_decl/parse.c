// RUN: %ucc -fsyntax-only %s

main()
{
	{
		int i;
	}

	int a;
	{
		int a;
	}

	5;

	//int a;

	int x;
}
