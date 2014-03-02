struct A
{
	int x[8];
};

main()
{
	struct A a = { 1, 2, 3, 4, 5, 6, 7, 8 };
	struct A b = a;

	printf("%d %d %d %d %d %d %d %d\n",
			a.x[0], a.x[1], a.x[2], a.x[3],
			a.x[4], a.x[5], a.x[6], a.x[7]);

	printf("%d %d %d %d %d %d %d %d\n",
			b.x[0], b.x[1], b.x[2], b.x[3],
			b.x[4], b.x[5], b.x[6], b.x[7]);
}
