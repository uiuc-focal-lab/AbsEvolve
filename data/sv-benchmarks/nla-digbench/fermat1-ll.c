#include <clam/clam.h>
/* program computing a divisor for factorisation, by Knuth 4.5.4 Alg C ? */

extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    int A, R;
    long long u, v, r;
    A = __VERIFIER_nondet_int();
    R = __VERIFIER_nondet_int();
    __CRAB_assume((((long long) R - 1) * ((long long) R - 1)) < A);
    //__CRAB_assume(A <= R * R);
    __CRAB_assume(A % 2 == 1);

    u = ((long long) 2 * R) + 1;
    v = 1;
    r = ((long long) R * R) - A;


    while (1) {
        __CRAB_assert(4*(A+r) == u*u - v*v - 2*u + 2*v);
        if (!(r != 0))
            break;

        while (1) {
	    __CRAB_assert(4*(A+r) == u*u - v*v - 2*u + 2*v);
            if (!(r > 0))
                break;
            r = r - v;
            v = v + 2;
        }

        while (1) {
	    __CRAB_assert(4*(A+r) == u*u - v*v - 2*u + 2*v);
            if (!(r < 0))
                break;
            r = r + u;
            u = u + 2;
        }
    }

    __CRAB_assert(((long long) 4*A) == u*u - v*v - 2*u + 2*v);
    return 0;
}
