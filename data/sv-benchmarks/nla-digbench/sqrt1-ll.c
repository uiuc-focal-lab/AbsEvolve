#include <clam/clam.h>
/* Compute the floor of the square root of a natural number */

extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);


int main() {
    int n;
    long long a, s, t;
    n = __VERIFIER_nondet_int();

    a = 0;
    s = 1;
    t = 1;

    while (1) {
        __CRAB_assert(t == 2*a + 1);
        __CRAB_assert(s == (a + 1) * (a + 1));
	__CRAB_assert(t*t - 4*s + 2*t + 1 == 0);
        // the above 2 should be equiv to 

        if (!(s <= n))
            break;

        a = a + 1;
        t = t + 2;
        s = s + t;
    }
    
    __CRAB_assert(t == 2 * a + 1);
    __CRAB_assert(s == (a + 1) * (a + 1));
    __CRAB_assert(t*t - 4*s + 2*t + 1 == 0);

    return 0;
}
