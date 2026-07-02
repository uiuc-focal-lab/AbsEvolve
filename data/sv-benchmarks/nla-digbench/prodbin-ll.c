#include <clam/clam.h>
/* shift_add algorithm for computing the 
   product of two natural numbers
*/
extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    int a, b;
    long long x, y, z;

    a = __VERIFIER_nondet_int();
    b = __VERIFIER_nondet_int();
    __CRAB_assume(b >= 1);

    x = a;
    y = b;
    z = 0;

    while (1) {
        __CRAB_assert(z + x * y == (long long) a * b);
        if (!(y != 0))
            break;

        if (y % 2 == 1) {
            z = z + x;
            y = y - 1;
        }
        x = 2 * x;
        y = y / 2;
    }
    __CRAB_assert(z == (long long) a * b);
    
    return 0;
}
