#include <stdio.h>


#define BITS_PER_LONG 32
#define FIXED_POINT_SCALE 8

#ifndef _ASM_GENERIC_BITOPS___FLS_H_
#define _ASM_GENERIC_BITOPS___FLS_H_
static unsigned long __fls(unsigned long word)
{
	int num = BITS_PER_LONG - 1;

#if BITS_PER_LONG == 64
	if (!(word & (~0ul << 32))) {
		num -= 32;
		word <<= 32;
	}
#endif
	if (!(word & (~0ul << (BITS_PER_LONG-16)))) {
		num -= 16;
		word <<= 16;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-8)))) {
		num -= 8;
		word <<= 8;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-4)))) {
		num -= 4;
		word <<= 4;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-2)))) {
		num -= 2;
		word <<= 2;
	}
	if (!(word & (~0ul << (BITS_PER_LONG-1))))
		num -= 1;
	return num;
}
#endif /* _ASM_GENERIC_BITOPS___FLS_H_ */


static unsigned int fixed_point_divide(unsigned int a, unsigned int b)
{
    unsigned int result;
    int a_ls = __fls(a);
    int b_ls = __fls(b);
    if ((b > a || b_ls - a_ls >= FIXED_POINT_SCALE) && a_ls + FIXED_POINT_SCALE < 32)
        result = (a << FIXED_POINT_SCALE) / b;
    else
        result = (a / b) << FIXED_POINT_SCALE;
    return result;
}

int main()
{
    double a = 123;
    double b = 45;
    printf("%f\n", a / b);                                   // 2.733333

    unsigned int c = a * (1 << FIXED_POINT_SCALE);
    unsigned int d = b * (1 << FIXED_POINT_SCALE);
    double r1 = c / d;
    double r2 = fixed_point_divide(c, d);
    printf("%f\n", (double) r1);                             // 2.000000
    printf("%f\n", (double) r2 / (1 << FIXED_POINT_SCALE));  // 2.730469
}