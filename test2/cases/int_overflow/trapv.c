// RUN: %ucc -ftrapv -DCALL=trapv -o %t %s
// RUN: %t; [ $? -ne 0 ]
// RUN: %ucc -ftrapv -DCALL=notrapv -o %t %s
// RUN: %t

#define INT_MAX 2147483647

trapv(int i)
{
	return i + 1;
}

notrapv(unsigned i)
{
	return i + 1;
}

main()
{
	int max = INT_MAX;
	CALL(max);
	return 0;
}
