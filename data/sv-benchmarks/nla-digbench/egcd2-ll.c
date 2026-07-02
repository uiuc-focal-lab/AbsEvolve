#include <clam/clam.h>
/* extended Euclid's algorithm */
extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    int x, y;
    long long a, b, p, q, r, s, c, k, xy, yy;
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
    c = 0;
    k = 0;
    xy = (long long) x * y;
    yy = (long long) y * y;
    __CRAB_assume(xy < 2147483647);
    __CRAB_assume(yy < 2147483647);

    while (1) {
        if (!(b != 0))
            break;
        c = a;
        k = 0;

        while (1) {
            __CRAB_assert(a == k * b + c);
            __CRAB_assert(a == y*r + x*p);
            __CRAB_assert(b == x * q + y * s);
	    __CRAB_assert(q*xy + s*yy - q*x - b*y - s*y + b == 0);
            if (!(c >= b))
                break;
            c = c - b;
            k = k + 1;
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
    

    __CRAB_assert(q*x + s*y == 0);
    __CRAB_assert(p*x + r*y == a);
    return a;
}
