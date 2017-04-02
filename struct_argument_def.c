int f(
		struct f {int i;} a,
		int (*b)(struct f {double j;}))
{
	struct f lol;
	lol.i = 2;
}

int h(
		int (*b)(/* nested scope */struct f {double j;}))
{
	struct f lol; // incomplete
}

//int g(struct g {int i;} a, int (* b)(struct g {double j;} c), int (* c)(struct g {double *k;}))

int g(
		struct g {int i;} a,
		struct g {float p;} aa) // error: redef
{
}

int main(void){ return 0; }
