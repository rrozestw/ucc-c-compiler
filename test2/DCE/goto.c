// RUN: %ucc -g -o %t %s

main()
{
	goto a;
	if(0){
		int i = 5;
		int j = 2;

		g(); // not undefined - unreachable & DCE'd
	}
	a:;
}
