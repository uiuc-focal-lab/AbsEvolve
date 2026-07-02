#include <clam/clam.h>
/* extended Euclid's algorithm */
extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    int x, y;
    long long a, b, p, q, r, s;
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
        if (!(b != 0))
            break;
        long long c, k;
        c = a;
        k = 0;

        while (1) {
            if (!(c >= b))
                break;
            long long d, v;
            d = 1;
            v = b;

            while (1) {
                __CRAB_assert(a == y * r + x * p);
                __CRAB_assert(b == x * q + y * s);
                __CRAB_assert(a == k * b + c);
                __CRAB_assert(v == b * d);

                if (!(c >= 2 * v))
                    break;
                d = 2 * d;
                v = 2 * v;
            }
            c = c - v;
            k = k + d;
        }

        a = b;
        b = c;
        long long temp;
        temp = p;
        p = q;
        q = temp - q * k;
        temp = r;
        r = s;
        s = temp - s * k;
    }
    __CRAB_assert(p*x - q*x + r*y - s*y  == a);
    return 0;
}
