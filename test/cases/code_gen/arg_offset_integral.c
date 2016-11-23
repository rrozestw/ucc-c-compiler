// RUN: %ucc -o %t %s
// RUN: %t | %output_check '1793'

f(a, b, c, d, e, f, g, h)
{
	return
		  (a << 0)
		+ (b << 1)
		+ (c << 2)
		+ (d << 3)
		+ (e << 4)
		+ (f << 5)
		+ (g << 6)
		+ (h << 7)
		;
}

main()
{
	printf("%d\n", f(1, 2, 3, 4, 5, 6, 7, 8));
}
