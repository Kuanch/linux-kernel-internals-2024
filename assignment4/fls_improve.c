#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

static int fls(unsigned int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

static int __ilog2_u32(uint32_t n)
{
	return fls(n) - 1;
}

int i_sqrt(int x)
{
    if (x <= 1) /* Assume x is always positive */
        return x;

    int z = 0;
    for (int m = 1UL << ((31 - __builtin_clz(x)) & ~1UL); m; m >>= 2) {
        int b = z + m;
        z >>= 1;
        if (x >= b)
            x -= b, z += m;               
    }
    return z;
}

int main()
{
    int x = 16;
    printf("sqrt(%d) = %d\n", x, i_sqrt(x));
    assert(i_sqrt(x) == sqrt(x));

	int y = 0x1234 * 0x1234;;
    printf("sqrt(%d) = %d\n", y, i_sqrt(y));
    assert(i_sqrt(y) == sqrt(y));
	
	return 0;
}