#include <stdio.h>
#include <stdint.h>

void divmod_10(uint32_t in, uint32_t *div, uint32_t *mod)
{
    uint32_t x = (in | 1) - (in >> 2); /* div = in/10 ==> div = 0.75*in/8 */
    uint32_t q = (x >> 4) + x;
    x = q;
    q = (q >> 8) + x;
    q = (q >> 8) + x;
    q = (q >> 8) + x;
    q = (q >> 8) + x;

    *div = (q >> 3);
    *mod = in - ((q & ~0x7) + (*div << 1));
}

int main()
{
    uint32_t in = 102;
    uint32_t div, mod;
    divmod_10(in, &div, &mod);
    printf("%d / 10 = %d, %d %% 10 = %d\n", in, div, in, mod);
    return 0;
}