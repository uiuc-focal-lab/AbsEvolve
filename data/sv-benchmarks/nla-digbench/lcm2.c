#include <clam/clam.h>
/* Algorithm for computing simultaneously the GCD and the LCM, by Dijkstra */

extern void abort(void);
extern unsigned __VERIFIER_nondet_uint(void);
extern void abort(void);

int main() {
    unsigned a, b;
    unsigned x, y, u, v;
    a = __VERIFIER_nondet_uint();
    b = __VERIFIER_nondet_uint();
    __CRAB_assume(a >= 1); //inf loop if remove
    __CRAB_assume(b >= 1);

    __CRAB_assume(a <= 65535);
    __CRAB_assume(b <= 65535);

    x = a;
    y = b;
    u = b;
    v = a;

    while (1) {
        __CRAB_assert(x*u + y*v == 2*a*b);

        if (!(x != y))
            break;

        if (x > y) {
            x = x - y;
            v = v + u;
        } else {
            y = y - x;
            u = u + v;
        }
    }

    __CRAB_assert(x*u + y*v == 2*a*b);
    // x == gcd(a,b)
    //(u + v)/2==lcm(a,b)

    return 0;
}
