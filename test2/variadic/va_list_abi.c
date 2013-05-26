// RUN: %ucc -c -o %t.o %s
// below ensures we link with the system libs
// RUN: cc -o %t %t.o
// RUN: %t | %output_check 'hi 5 hello' 'yo'

#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_arg __builtin_va_arg
#define va_end __builtin_va_end

final_greeting(char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	vprintf(fmt, l); // tests ABI compatability of __builtin_va_list
	va_end(l);
	write(1, "yo\n", 3);
}

main()
{
	final_greeting("hi %d %s\n", 5, "hello");
}
