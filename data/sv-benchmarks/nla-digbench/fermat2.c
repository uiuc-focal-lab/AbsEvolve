#include <clam/clam.h>
/* program computing a divisor for factorisation, by Bressoud */

extern void abort(void);
extern double __VERIFIER_nondet_double(void);
extern void abort(void);

int main() {
    int A, R;
    int u, v, r;
    A = __VERIFIER_nondet_double();
    R = __VERIFIER_nondet_double();
    //__CRAB_assume(A >= 1);
    __CRAB_assume((R - 1) * (R - 1) < A);
    //__CRAB_assume(A <= R * R);
    __CRAB_assume(A % 2 == 1);

    u = 2 * R + 1;
    v = 1;
    r = R * R - A;

    while (1) {
        __CRAB_assert(4*(A+r) == u*u - v*v - 2*u + 2*v);
        if (!(r != 0)) break;

        if (r > 0) {
            r = r - v;
            v = v + 2;
        } else {
            r = r + u;
            u = u + 2;
        }
    }

    //return  (u - v) / 2;
    __CRAB_assert(4*A == u*u - v*v  - 2*u + 2*v);
    return 0;
}
