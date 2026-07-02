#include <clam/clam.h>
extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    int k;
    long long y, x, c;
    k = __VERIFIER_nondet_int();

    y = 0;
    x = 0;
    c = 0;

    while (1) {
        __CRAB_assert((y * y) - 2 * x + y == 0);

        if (!(c < k))
            break;

        c = c + 1;
        y = y + 1;
        x = y + x;
    }
    __CRAB_assert((y*y) - 2*x + y == 0);
     
    return 0;
}
