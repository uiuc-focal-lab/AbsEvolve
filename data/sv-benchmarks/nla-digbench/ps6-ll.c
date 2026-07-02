#include <clam/clam.h>
extern void abort(void);
extern short __VERIFIER_nondet_short(void);
extern void abort(void);

int main() {
    short k;
    long long y, x, c;
    k = __VERIFIER_nondet_short();
    __CRAB_assume(k <= 256);

    y = 0;
    x = 0;
    c = 0;

    while (1) {
        __CRAB_assert(-2*y*y*y*y*y*y - 6 * y*y*y*y*y - 5 * y*y*y*y + y*y + 12*x == 0);

        if (!(c < k))
            break;

        c = c + 1;
        y = y + 1;
        x = y * y * y * y * y + x;
    }
    
    __CRAB_assert(-2*y*y*y*y*y*y - 6 * y*y*y*y*y - 5 * y*y*y*y + y*y + 12*x == 0);
    __CRAB_assert(k*y == y*y);		      
    return 0;
}
