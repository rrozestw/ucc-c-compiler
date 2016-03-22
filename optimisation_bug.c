static inline sum(int i)
{
	if(i <= 0)
		return 0;
	return i + sum(i-1);
}

__attribute__((always_inline))
static inline f(int i)
{
	return sum(i) >= 365;
}

main()
{
	for(int i = 0; i < 365; i++)
		if(f(i)){
			printf("%d\n", i);
			break;
		}
}
