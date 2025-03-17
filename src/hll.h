#ifndef HLL_H
#define HLL_H

#include <stdint.h>
#include <stdbool.h>

#define HLL_VERSION "1.0.0"

/* HyperLogLog structure */
typedef struct HyperLogLog HyperLogLog;

/* Creates a new HyperLogLog with 2^p registers */
HyperLogLog* hll_init(unsigned short p, uint64_t seed, bool sparse,
                    uint64_t maxSparseListSize, uint64_t maxSparseBufferSize);

/* Frees the memory used by a HyperLogLog */
void hll_free(HyperLogLog* hll);

/* Adds an element to the HyperLogLog */
bool hll_add(HyperLogLog* hll, const uint8_t* data, uint64_t dataLen);

/* Gets the cardinality estimate */
uint64_t hll_cardinality(HyperLogLog* hll);

/* Merges another HyperLogLog into the current one */
bool hll_merge(HyperLogLog* dest, HyperLogLog* src);

/* Gets a Murmur64A hash of data */
uint64_t hll_hash(HyperLogLog* hll, const uint8_t* data, uint64_t dataLen);

/* Gets the value of a register */
uint64_t hll_get_register(HyperLogLog* hll, uint64_t index);

/* Gets the number of registers */
uint64_t hll_size(HyperLogLog* hll);

/* Gets the seed value */
uint64_t hll_seed(HyperLogLog* hll);

/* Helper functions */
uint8_t clz(uint64_t x);
double sigma(double x);
double tau(double x);
bool isValidIndex(uint64_t index, uint64_t size);

#endif /* HLL_H */
