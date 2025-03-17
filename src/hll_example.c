
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "hll.h"

int main() {
    // Initialize two HyperLogLog counters
    HyperLogLog* hll1 = hll_init(14, 12345, false, 0, 0);
    HyperLogLog* hll2 = hll_init(14, 12345, false, 0, 0);

    if (!hll1 || !hll2) {
        fprintf(stderr, "Failed to initialize HyperLogLog counters\n");
        return EXIT_FAILURE;
    }

    // Add elements to the counters
    const uint8_t a[] = "a";
    const uint8_t b[] = "b";
    hll_add(hll1, a, sizeof(a) - 1);
    hll_add(hll2, b, sizeof(b) - 1);

    // Merge hll2 into hll1
    if (!hll_merge(hll1, hll2)) {
        fprintf(stderr, "Failed to merge HyperLogLog counters\n");
        return EXIT_FAILURE;
    }

    // Get and print the cardinality of the merged counter
    uint64_t cardinality = hll_cardinality(hll1);
    printf("Estimated cardinality: %lu\n", cardinality);

    // Free the HyperLogLog structures
    hll_free(hll1);
    hll_free(hll2);

    return EXIT_SUCCESS;
}
