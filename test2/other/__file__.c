// RUN: %ucc -o %t %s
// RUN: %t | grep 'in q'
// RUN: %t | grep 'overridden, in hi'
// RUN: %t | grep 'in main (2 = i)'

#include <stdio.h>

void
q()
{
	printf("in %s\n", __func__);
	{
		char *__func__ = "hi";
		printf("overridden, in %s\n", __func__);
	}
}

int
main()
{
	printf("in %s (2 = %c)\n", __func__, __func__[2]);
	q();
}
