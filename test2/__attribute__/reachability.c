// RUN: %ucc -c %s 2>&1 | grep .; [ $? -eq 0 ]

abort() __attribute__((__noreturn__));

main()
{
	abort();
	// shouldn't warn about main not returning a value
}
