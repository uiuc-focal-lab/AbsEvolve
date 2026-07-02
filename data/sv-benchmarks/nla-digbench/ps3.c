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
        __CRAB_assert(6*x - 2*y*y*y - 3*y*y - y == 0);

        if (!(c < k))
            break;

        c = c + 1;
        y = y + 1;
        x = y * y + x;
    }
    __CRAB_assert(6*x - 2*y*y*y - 3*y*y - y == 0);
    return 0;
}
