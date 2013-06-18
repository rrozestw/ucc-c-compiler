// RUN: %ucc -o %t %s
// RUN: %t | %output_check '[0] = hi 5' '[1] = yo 2'
// RUN: %t; [ $? -eq 8 ]
struct A
{
	int n;
	// pad of 4
	struct Ent
	{
		char *nam;
		int type;
	} ents[];
};

print(struct A *p)
{
	for(int i = 0; i < p->n; i++)
		printf("[%d] = %s %d\n", i, p->ents[i].nam, p->ents[i].type);
}

main()
{
	static struct A x = {
		2,
		{
			{ "hi", 5 },
			{ "yo", 2 },
		}
	};

	print(&x);

	return sizeof(x);
}