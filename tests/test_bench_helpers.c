// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "../diagnostics/bench/bench_args.h"
#include "../diagnostics/bench/gemm_inputs.h"

int main(void)
{
	unsigned long long value = 0;
	assert(bench_positive_count("20000", ULLONG_MAX, &value) && value == 20000);
	assert(bench_positive_count("30", INT_MAX, &value) && value == 30);
	assert(bench_positive_count("0x100", ULLONG_MAX, &value) && value == 256);
	assert(bench_positive_count("18446744073709551615", ULLONG_MAX, &value) && value == ULLONG_MAX);
	const char *invalid[] = {"", "0", "-1", "+1", " 1", "1x", "18446744073709551616"};
	for (unsigned int i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		assert(!bench_positive_count(invalid[i], ULLONG_MAX, &value));
	assert(!bench_positive_count("2147483648", INT_MAX, &value));
	assert(!bench_positive_count(NULL, ULLONG_MAX, &value));

	for (uint32_t seed = 1; seed <= 3; seed++) {
		float rows[256][256];
		for (unsigned int row = 0; row < 256; row++)
			for (unsigned int col = 0; col < 256; col++) {
				float input = gemm_input(row, col, seed);
				assert(input >= -1.0f && input < 1.0f);
				assert(input * 16 == (int)(input * 16));
				rows[row][col] = input;
			}
		/* The old row*17 % 17 pattern fails the first assertion. */
		for (unsigned int first = 0; first < 256; first++)
			for (unsigned int second = first + 1; second < 256; second++) {
				assert(memcmp(rows[first], rows[second], sizeof(rows[first])));
				bool distinct = false;
				for (unsigned int row = 0; row < 256; row++)
					distinct |= rows[row][first] != rows[row][second];
				assert(distinct);
			}
		/* Independently form a small GEMM. A swapped output row must change
		 * the result, rather than hiding behind identical reference rows. */
		float output[32][24] = {{0}};
		for (unsigned int row = 0; row < 32; row++)
			for (unsigned int col = 0; col < 24; col++)
				for (unsigned int inner = 0; inner < 48; inner++)
					output[row][col] += gemm_input(row, inner, seed) *
						gemm_input(inner, col, seed ^ 0xa5a5a5a5u);
		for (unsigned int row = 1; row < 32; row++)
			assert(memcmp(output[0], output[row], sizeof(output[0])));
	}
	puts("benchmark arguments and nondegenerate GEMM inputs passed");
	return 0;
}
