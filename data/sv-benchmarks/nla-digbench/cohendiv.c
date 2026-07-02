#include <clam/clam.h>
/*
  Cohen's integer division
  returns x % y
  http://www.cs.upc.edu/~erodri/webpage/polynomial_invariants/cohendiv.htm
*/
extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    int x, y, q, r, a, b;

    x = __VERIFIER_nondet_int();
    y = __VERIFIER_nondet_int();

    __CRAB_assume(y >= 1);

    q = 0;
    r = x;
    a = 0;
    b = 0;

    while (1) {
	__CRAB_assert(b == y*a);
	__CRAB_assert(x == q*y + r);
    
	if (!(r >= y))
	    break;
	a = 1;
	b = y;

	while (1) {            
	    __CRAB_assert(b == y*a);
	    __CRAB_assert(x == q*y + r);
	    __CRAB_assert(r >= 0);

	    if (!(r >= 2 * b))
		break;
	    
	    __CRAB_assert(r >= 2 * y * a);
	    
	    a = 2 * a;
	    b = 2 * b;
	}
	r = r - b;
	q = q + a;
    }
    
    __CRAB_assert(x == q*y + r);
    return 0;
}
