#include <clam/clam.h>
extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    int k, y, x, c;
    k = __VERIFIER_nondet_int();

    y = 0;
    x = 0;
    c = 0;

    while (1) {
        __CRAB_assert(4*x - y*y*y*y - 2*y*y*y - y*y == 0);

        if (!(c < k))
            break;

        c = c + 1;
        y = y + 1;
        x = y * y * y + x;
    }
    __CRAB_assert(k*y - (y*y) == 0);
    __CRAB_assert(4*x - y*y*y*y - 2*y*y*y - y*y == 0);
    return 0;
}
