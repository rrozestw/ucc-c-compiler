// RUN: %check %s
main()
{
	int test(void), test2(void);

	if(test())
		lbl: __attribute__((unused)) // CHECK: !/warn/
			if(test2())
				lbl2: __attribute((noreturn)) // CHECK: /warning: ignoring attribute "noreturn" on label/
					f(); // CHECK: ^/warning: unused label 'lbl2'/
}
