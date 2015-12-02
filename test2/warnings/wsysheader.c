// RUN: %check --prefix=sysh %s -Wsystem-headers -Weverything -Wc,-I/system
// RUN: %check --prefix=norm %s                  -Weverything -Wc,-I/system

# 5 "/system/sys.h"
// ^ the above 5 is volatile to change

f(); // CHECK-sysh: /warning/
// CHECK-norm: ^ !/warning/

# 2 "src.c"

int main(void)
{
	return 0;
}
