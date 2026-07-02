#include <clam/clam.h>
/* 
Geometric Series
computes x = sum(z^k)[k=0..k-1], y = z^(k-1)
*/

extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);


int main() {
    int z, k;
    unsigned long long x, y, c;
    z = __VERIFIER_nondet_int();
    k = __VERIFIER_nondet_int();

    x = 1;
    y = 1;
    c = 1;

    while (1) {
        __CRAB_assert(1 + x*z - x - z*y == 0);

        if (!(c < k))
            break;

        c = c + 1;
        x = x * z + 1;
        y = y * z;
    }
    __CRAB_assert(1 + x*z - x - z*y == 0);
    return 0;
}
