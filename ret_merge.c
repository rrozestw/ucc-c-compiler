_Noreturn void exit(int);
void g(int);

int f(int p)
{
	(p == 5 ? exit : g)(2);
	return 7;
}
