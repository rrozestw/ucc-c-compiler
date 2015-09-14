// ./ucc -S -o- -O tim.c

inline f(int a, int b)
{
	return a<<b;
}

main()
{
	return f(5, 32);
}
