#include <clam/clam.h>
/* algorithm searching for a divisor for factorization, by Knuth */

#include <limits.h>

extern void abort(void);
extern void abort(void);

extern double sqrt(double);

int main() {
    unsigned n, a;
    unsigned r, k, q, d, s, t;
    n = __VERIFIER_nondet_uint();
    a = __VERIFIER_nondet_uint();
    __CRAB_assume(n < UINT_MAX/8);
    __CRAB_assume(a > 2);

    d = a;
    r = n % d;
    t = 0;
    k = n % (d - 2);
    q = 4 * (n / (d - 2) - n / d);
    s = sqrt(n);

    while (1) {
        __CRAB_assert(d * d * q - 2 * q * d - 4 * r * d + 4 * k * d + 8 * r == 8 * n);
        __CRAB_assert(k * t == t * t);
        __CRAB_assert(d * d * q - 2 * d * q - 4 * d * r + 4 * d * t + 4 * a * k - 4 * a * t - 8 * n + 8 * r == 0);
        __CRAB_assert(d * k - d * t - a * k + a * t == 0);

        if (!((s >= d) && (r != 0))) break;

        if (2 * r  + q < k) {
            t = r;
            r = 2 * r - k + q + d + 2;
            k = t;
            q = q + 4;
            d = d + 2;
        } else if ((2 * r  + q >= k) && (2 * r + q < d + k + 2)) {
            t = r;
            r = 2 * r - k + q;
            k = t;
            d = d + 2;
        } else if ((2 * r  + q >= k) && (2 * r + q >= d + k + 2) && (2 * r + q < 2 * d + k + 4)) {
            t = r;
            r = 2 * r - k + q - d - 2;
            k = t;
            q = q - 4;
            d = d + 2;
        } else { /* ((2*r-k+q>=0)&&(2*r-k+q>=2*d+4)) */
            t = r;
            r = 2 * r - k + q - 2 * d - 4;
            k = t;
            q = q - 8;
            d = d + 2;
        }
    }

    //postconds ? cannot be obtained with DIG (Syminfer)
    return 0;
}
