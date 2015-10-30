#include <stdio.h>
 
int main(int argc, char * argv[]) {
  (void)argc;
  (void)argv;
 
  int x = 1;
 
  while(x > 0)
  {
    x++;
  }
 
  printf("x is: %d\n", x);
 
  return 0;
}