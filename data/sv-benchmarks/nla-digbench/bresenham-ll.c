#include <clam/clam.h>
/*
  Bresenham's line drawing algorithm 
  from Srivastava et al.'s paper From Program Verification to Program Synthesis in POPL '10 
*/
extern void abort(void);
extern int __VERIFIER_nondet_int(void);
extern void abort(void);

int main() {
    int X, Y;
    long long x, y, v, xy, yx;
    X = __VERIFIER_nondet_int();
    Y = __VERIFIER_nondet_int();
    v = ((long long) 2 * Y) - X;         // cast required to avoid int overflow
    y = 0;
    x = 0;

    while (1) {
        yx = (long long) Y*x;
        xy = (long long) X*y;
	__CRAB_assert( 2*yx - 2*xy - X + (long long) 2*Y - v == 0);
        if (!(x <= X))
            break;
        // out[x] = y

        if (v < 0) {
            v = v + (long long) 2 * Y;
        } else {
            v = v + 2 * ((long long) Y - X);
            y++;
        }
        x++;
    }
    xy = (long long) x*y;
    yx = (long long) Y*x;
    __CRAB_assert(2*yx - 2*xy - X + (long long) 2*Y - v + 2*y == 0);

    return 0;
}
