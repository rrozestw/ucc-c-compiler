// RUN: %ucc -o %t %s
// RUN: %t | %stdoutcheck %s

// STDOUT: 46
// STDOUT-NEXT: -5

f()
{
	_Static_assert(__builtin_constant_p((char)5678), "cast not a constant");
	return (char)5678;
}

g()
{
	// this checks sign extension when casting
	return (char)-5;
}

main()
{
	printf("%d\n", f());
	printf("%d\n", g());
}
