#include <clam/clam.h>
/*
  A division algorithm, by Kaldewaij
  returns A//B
*/

#include <limits.h>

extern void abort(void);
extern void abort(void);

int main() {
  unsigned A, B;
  unsigned q, r, b;
    A = __VERIFIER_nondet_uint();
    B = __VERIFIER_nondet_uint();
    __CRAB_assume(B < UINT_MAX/2);
    __CRAB_assume(B >= 1);

    q = 0;
    r = A;
    b = B;

    while (1) {
        if (!(r >= b)) break;
        b = 2 * b;
    }

    while (1) {
        __CRAB_assert(A == q * b + r);
        if (!(b != B)) break;

        q = 2 * q;
        b = b / 2;
        if (r >= b) {
            q = q + 1;
            r = r - b;
        }
    }

    __CRAB_assert(A == q * b + r);
    return 0;
}
