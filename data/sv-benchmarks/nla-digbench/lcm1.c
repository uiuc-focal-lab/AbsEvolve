#include <clam/clam.h>
/*
 * algorithm for computing simultaneously the GCD and the LCM,
 * by Sankaranarayanan
 */

extern void abort(void);
extern unsigned __VERIFIER_nondet_uint(void);
extern void abort(void);

int main() {
    unsigned a, b;
    unsigned x, y, u, v;
    a = __VERIFIER_nondet_uint();
    b = __VERIFIER_nondet_uint();
    __CRAB_assume(a >= 1);  //infinite loop if remove
    __CRAB_assume(b >= 1);

    __CRAB_assume(a <= 65535);
    __CRAB_assume(b <= 65535);

    x = a;
    y = b;
    u = b;
    v = 0;

    while (1) {
        __CRAB_assert(x*u + y*v == a*b);
        if (!(x != y))
            break;

        while (1) {
	    __CRAB_assert(x*u + y*v == a*b);
            if (!(x > y))
                break;
            x = x - y;
            v = v + u;
        }

        while (1) {
	    __CRAB_assert(x*u + y*v == a*b);
            if (!(x < y))
                break;
            y = y - x;
            u = u + v;
        }
    }

    __CRAB_assert(u*y + v*y == a*b);
    __CRAB_assert(x == y);

    //x == gcd(a,b)
    //u + v == lcm(a,b)
    return 0;
}
