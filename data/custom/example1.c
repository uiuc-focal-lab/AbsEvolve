#include "clam/clam.h"

int main() {
  int x = 100;
  int y = 150;
  
  while (y <= 600)
  {
    x = x + y;
    y = 2*y;
  }

  __CRAB_assert (x <= y);
  return 0;
}