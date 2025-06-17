#include <stdbool.h>
#include "pll_table.h"

bool get_pll_parameters_binary(float target_freq, uint8_t *fb_divider, uint8_t *refdiv,
                               uint8_t *postdiv1, uint8_t *postdiv2, float *actual_freq) 
{
    if (target_freq < pll_table[0].freq) {
        return false;
    }

    int left = 0;
    int right = PLL_TABLE_SIZE - 1;
    int result = -1; // Index of the closest lower frequency

    // Binary search for the largest frequency <= target_freq
    while (left <= right) {
        int mid = left + (right - left) / 2; // Avoid overflow
        float mid_freq = pll_table[mid].freq;

        if (mid_freq <= target_freq) {
            result = mid; // Candidate for largest <= target_freq
            left = mid + 1; // Search for a larger frequency
        } else {
            right = mid - 1; // Search for a smaller frequency
        }
    }

    *fb_divider = pll_table[result].fb_divider;
    *refdiv = pll_table[result].refdiv;
    *postdiv1 = pll_table[result].postdiv1;
    *postdiv2 = pll_table[result].postdiv2;
    *actual_freq = pll_table[result].freq;

    return true;
}