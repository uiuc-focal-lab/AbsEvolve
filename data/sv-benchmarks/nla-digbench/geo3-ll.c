#include <clam/clam.h>
/* 
Geometric Series
computes x = sum(z^k)[k=0..k-1], y = z^(k-1)
*/

extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);
int main() {
    int z, a, k;
    unsigned long long x, y, c;
    long long az;
    z = __VERIFIER_nondet_int();
    a = __VERIFIER_nondet_int();
    k = __VERIFIER_nondet_int();

    x = a;
    y = 1;
    c = 1;
    az = (long long) a * z;

    while (1) {
        __CRAB_assert(z*x - x + a - az*y == 0);

        if (!(c < k))
            break;

        c = c + 1;
        x = x * z + a;
        y = y * z;
    }
    __CRAB_assert(z*x - x + a - az*y == 0);
    return x;
}
