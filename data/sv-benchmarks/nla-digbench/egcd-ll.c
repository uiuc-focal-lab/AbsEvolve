#include <clam/clam.h>
/* extended Euclid's algorithm */
extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    long long a, b, p, q, r, s;
    int x, y;
    x = __VERIFIER_nondet_int();
    y = __VERIFIER_nondet_int();
    __CRAB_assume(x >= 1);
    __CRAB_assume(y >= 1);

    a = x;
    b = y;
    p = 1;
    q = 0;
    r = 0;
    s = 1;

    while (1) {
        __CRAB_assert(1 == p * s - r * q);
        __CRAB_assert(a == y * r + x * p);
        __CRAB_assert(b == x * q + y * s);

        if (!(a != b))
            break;

        if (a > b) {
            a = a - b;
            p = p - q;
            r = r - s;
        } else {
            b = b - a;
            q = q - p;
            s = s - r;
        }
    }
    
    __CRAB_assert(a - b == 0);    
    __CRAB_assert(p*x + r*y - b == 0);
    __CRAB_assert(q*r - p*s + 1 == 0);
    __CRAB_assert(q*x + s*y - b == 0);
    return 0;
}
