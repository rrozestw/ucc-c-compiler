// RUN: %ucc -o %t %s
// RUN: %t | %stdoutcheck %s
// RUN: %check %s

float f = -1; // CHECK: !/warning:.*standard/
int i = -10.0f; // CHECK: !/warning:.*standard/

main()
{
	printf("%.1f\n", f); // STDOUT: -1.0
	printf("%d\n", i); // STDOUT-NEXT: -10
}
