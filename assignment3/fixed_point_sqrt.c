#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define FIXED_POINT_SCALE 8
#define TOLERANCE (1 << (FIXED_POINT_SCALE / 2))

static inline unsigned int fixed_point_divide(unsigned int a, unsigned int b)
{
    return (a << FIXED_POINT_SCALE) / b;
}

static inline unsigned int fixed_point_sqrt(unsigned int x)
{
    unsigned int r = x > 1 ? (x >> 1) : 1;
    unsigned int r_new;
    while (1) {
        if (r == 0)
            return r;
        unsigned int div = fixed_point_divide(x, r);
        r_new = (r + div) >> 1;
        unsigned int diff = r_new - r;
        if (diff < TOLERANCE) {
            return r_new;
        }
        r = r_new;
    }
}

double fixed_point_to_double(unsigned int x, int scale)
{
    return (double)x / (1 << scale);
}


int main()
{
    double max_diff = 0;
    double sum_diff = 0;
    double std_diff = 0;
    double *diff_arr = malloc(65535 * sizeof(double));
    for (int a = 1; a < 65536; a++) {
        float sqrt_2 = sqrt(a);
        unsigned int approx_sqrt_2 = fixed_point_sqrt(a * (1 << FIXED_POINT_SCALE));
        double approx_sqrt_2_double = fixed_point_to_double(approx_sqrt_2, FIXED_POINT_SCALE);
        double diff = fabs(sqrt_2 - approx_sqrt_2_double);
        diff_arr[a - 1] = diff;
        sum_diff += diff;
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    double mean_diff = sum_diff / 65535;
    for (int i = 0; i < 65535; i++) {
        std_diff += pow(diff_arr[i] - mean_diff, 2);
    }
    std_diff = sqrt(std_diff / 65535);
    printf("%f, %f, %f\n", max_diff, mean_diff, std_diff);
}