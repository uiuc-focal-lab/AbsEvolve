#include <clam/clam.h>
/*
  hardware integer division program, by Manna
  returns q==A//B
  */

extern void abort(void);
extern unsigned int __VERIFIER_nondet_uint(void);
extern void abort(void);

int main() {
    unsigned int A, B;
    long long r, d, p, q;
    A = __VERIFIER_nondet_uint();
    B = __VERIFIER_nondet_uint();
    __CRAB_assume(B >= 1);

    r = A;
    d = B;
    p = 1;
    q = 0;

    while (1) {
        __CRAB_assert(q == 0);
        __CRAB_assert(r == A);
        __CRAB_assert(d == B * p);
        if (!(r >= d)) break;

        d = 2 * d;
        p = 2 * p;
    }

    while (1) {
        __CRAB_assert(A == q*B + r);
        __CRAB_assert(d == B*p);

        if (!(p != 1)) break;

        d = d / 2;
        p = p / 2;
        if (r >= d) {
            r = r - d;
            q = q + p;
        }
    }

    __CRAB_assert(A == d*q + r);
    __CRAB_assert(B == d);    
    return 0;
}
