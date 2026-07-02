#include "clam/clam.h"
int main() {
  int k = nd_int();
  int n = nd_int();
  int k1 = nd_int();
  int n1 = nd_int();
  __CRAB_assume (k >= n);
  __CRAB_assume (k1 >= n1);
  
  int x = k;
  int y = n;
  int a = k1 + 1;
  int b = n1 + 1;
  
  x = x + y;
  y = 2*y;
  a = a + b;
  b = 2*b;
  __CRAB_assert (x >= y);
  __CRAB_assert (a >= b);
  return 0;
}
