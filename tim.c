#include <stdio.h>
#include <stddef.h>

#undef __attribute__

struct foo1 { char s[80]; };
struct foo2 { char s[80]; }  __attribute__ ((aligned (64)));
struct bar1 { struct foo1 f; int i; };
struct bar2 { struct foo2 f; int i; };
#define P(arg) printf("sizeof(" #arg ") = %u\n", (unsigned)sizeof(arg))

#define offsetof(S, m) (long)&((S*)0)->m


#define JOIN_(x, y) x ## y
#define JOIN(x, y) JOIN_(x, y)

#define ASSERT_ALIGNED(exp, t) _Static_assert(exp == __builtin_has_attribute(struct t, aligned),"")
ASSERT_ALIGNED(0, foo1);
ASSERT_ALIGNED(1, foo2);
ASSERT_ALIGNED(0, bar1);
ASSERT_ALIGNED(0, bar2);


int main(void)
{
  P(struct foo1);
  P(struct foo2);
  P(struct bar1); printf("offset=%u\n", (unsigned)offsetof(struct bar1, i));
  P(struct bar2); printf("offset=%u\n", (unsigned)offsetof(struct bar2, i));
  return 0;
}
