// RUN: %ucc -c -o %t %s

main()
{
	for(int i =0;;);
	for(int x[] = { 1, 2, 3 };;);
}
