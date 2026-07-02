#include <clam/clam.h>
/* 
Geometric Series
computes x=(z-1)* sum(z^k)[k=0..k-1] , y = z^k
returns 1+x-y == 0
*/

extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);
int main() {
    int z, k;
    unsigned long long x, y, c;
    z = __VERIFIER_nondet_int();
    k = __VERIFIER_nondet_int();
    __CRAB_assume(z >= 1);
    __CRAB_assume(k >= 1);

    x = 1;
    y = z;
    c = 1;

    while (1) {
        __CRAB_assert(x*z - x - y + 1 == 0);

        if (!(c < k)) 
            break;

        c = c + 1;
        x = x * z + 1;
        y = y * z;

    }  //geo1

    x = x * (z - 1);

    __CRAB_assert(1 + x - y == 0);
    return 0;
}
