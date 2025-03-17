#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "hll.h"
#include "../lib/murmur2.h"

/* Node structure for sparse representation */
typedef struct Node {
    struct Node* next;
    uint64_t index;
    uint8_t fsb;
} Node;

/* HyperLogLog structure definition */
struct HyperLogLog {
    uint8_t* registers;           /* Densely encoded registers */
    unsigned short p;             /* 2^p = number of registers */
    uint64_t* histogram;          /* Register histogram */
    uint64_t seed;                /* MurmurHash64A seed */
    uint64_t size;                /* Number of registers */
    uint64_t cache;               /* Cached cardinality estimate */
    uint64_t added;               /* Number of elements added */
    bool isCached;                /* If the cache is up to date */
    bool isSparse;                /* If sparse encoding is currently in use */

    /* Fields used for sparse representation */
    Node* sparseRegisterList;     /* Linked list of registers */
    Node* sparseRegisterBuffer;   /* Temporary buffer of nodes to be added */
    Node* nodeCache;              /* The last sparse register accessed using get() */
    uint64_t bufferSize;          /* Number of elements in the temporary buffer */
    uint64_t listSize;            /* Number of elements in the linked list */
    uint64_t maxBufferSize;       /* Max number of elements for the temporary buffer */
    uint64_t maxListSize;         /* Max number of nodes in the sparse list */
};

/* Get register m in dense representation */
static inline uint64_t getDenseRegister(uint64_t m, uint8_t* regs)
{
    uint64_t nBits = 6*m + 6;
    uint64_t bytePos = nBits/8 - 1;
    uint8_t leftByte = regs[bytePos];
    uint8_t rightByte = regs[bytePos + 1];
    uint8_t nrb = (uint8_t) (nBits % 8);
    uint8_t reg;

    leftByte <<= nrb; /* Move left bits into high order spots */
    rightByte >>= (8 - nrb); /* Move rights bits into the low order spots */
    reg = leftByte | rightByte; /* OR the result to get the register */
    reg &= 63; /* Get rid of the 2 extra bits */

    return (uint64_t) reg;
}

/* Set register m to n in dense representation */
static inline void setDenseRegister(uint64_t m, uint8_t n, uint8_t* regs)
{
    uint64_t nBits = 6*m + 6;
    uint64_t bytePos = nBits/8 - 1;
    uint8_t nrb = (uint8_t) (nBits % 8);
    uint8_t nlb = 6 - nrb;
    uint8_t leftByte = regs[bytePos];
    uint8_t rightByte = regs[bytePos + 1];

    leftByte >>= nlb;
    leftByte <<= nlb;
    rightByte <<= nrb;
    rightByte >>= nrb;
    leftByte |= (n >> nrb); /* Set the new left bits */
    rightByte |= (n << (8 - nrb)); /* Set the new right bits */
    regs[bytePos] = leftByte;
    regs[bytePos + 1] = rightByte;
}

/* Compares two sparse register nodes */
int compareNodes(const void* a, const void* b) {
    int result = -1;
    Node* A = (Node*) a;
    Node* B = (Node*) b;

    if (A->index == B->index) {
        if (A->fsb > B->fsb) {
            result = 1;
        } else if (A->fsb < B->fsb) {
            result = -1;
        } else {
            result = 0;
        }
    } else if (A->index > B->index) {
        result = 1;
    }

    return result;
}

/* Updates the register list using the items in the buffer */
void flushRegisterBuffer(HyperLogLog* self)
{
    uint64_t i;
    Node* node;
    Node* current = self->sparseRegisterList;
    Node* next = NULL;
    Node* prev = NULL;

    qsort(self->sparseRegisterBuffer, self->bufferSize, sizeof(Node), compareNodes);

    for (i = 0; i < self->bufferSize; i++) {
        /* Create the new node from the current item in the buffer */
        node = (Node*)malloc(sizeof(Node));
        node->fsb = self->sparseRegisterBuffer[i].fsb;
        node->index = self->sparseRegisterBuffer[i].index;
        node->next = NULL;

        /* If head doesn't exist then set it */
        if (self->sparseRegisterList == NULL) {
            self->sparseRegisterList = node;
            self->histogram[0]--;
            self->histogram[(uint8_t)node->fsb]++;
            self->listSize += 1;
            prev = node;
            continue;
        }

        /* Since both lists are sorted try to use the last node */
        if (prev != NULL) {
            current = prev;
        } else {
            current = self->sparseRegisterList;
        }

        while (current != NULL) {
            // Are we updating an existing node?
            if (current->index == node->index) {
                if (current->fsb < node->fsb) {
                    self->histogram[current->fsb]--;
                    self->histogram[node->fsb]++;
                    current->fsb = node->fsb;
                }

                prev = current;

                /* We don't need the new node */
                free(node);
                break;
            }

            // Are we creating a new head?
            if (current->index > node->index) {
                node->next = current;
                self->histogram[0]--;
                self->histogram[(uint8_t)node->fsb]++;
                self->sparseRegisterList = node;
                self->listSize += 1;
                prev = node;
                break;
            }

            // Are we creating a new tail?
            if (current->next == NULL) {
                current->next = node;
                self->histogram[0]--;
                self->histogram[(uint8_t)node->fsb]++;
                self->listSize += 1;
                prev = node;
                break;
            }

            // Are inserting between nodes?
            if (current->next->index > node->index) {
                node->next = current->next;
                current->next = node;
                self->histogram[0]--;
                self->histogram[(uint8_t)node->fsb]++;
                self->listSize += 1;
                prev = node;
                break;
            }

            next = current->next;
            current = next;
        }
    }

    self->bufferSize = 0;
}

/* Transforms a HyperLogLog from sparse to dense representation */
void transformToDense(HyperLogLog* self) {
    uint64_t bytes = (self->size*6)/8 + 1;
    self->registers = (uint8_t*)calloc(bytes, sizeof(uint8_t));

    if (self->registers == NULL) {
        /* Handle error */
        return;
    }

    flushRegisterBuffer(self);

    Node* next = NULL;
    Node* current = self->sparseRegisterList;

    while (current != NULL) {
        setDenseRegister(current->index, current->fsb, self->registers);
        next = current->next;
        current = next;
    }

    current = self->sparseRegisterList;

    while (current != NULL) {
        next = current;
        current = current->next;

        if (next != NULL) {
            free(next);
            next = NULL;
        }
    }

    if (self->sparseRegisterBuffer != NULL) {
        free(self->sparseRegisterBuffer);
        self->sparseRegisterBuffer = NULL;
    }

    self->sparseRegisterList = NULL;
    self->nodeCache = NULL;
    self->isSparse = 0;
}

/* Gets the register value at the specified index in sparse representation */
static inline uint64_t getSparseRegister(HyperLogLog* self, uint64_t index)
{
    Node* current = NULL;

    if (self->bufferSize > 0) {
        flushRegisterBuffer(self);
    }

    current = self->sparseRegisterList;

    /* Can we used the cache? */
    if (self->nodeCache != NULL && self->nodeCache->index <= index) {
        current = self->nodeCache;
    }

    while (current != NULL) {
        if (current->index > index) {
            return 0;
        } else if (current->index == index) {
            self->nodeCache = current;
            return current->fsb;
        }

        current = current->next;
    }

    return 0;
}

/* Sets a sparse register */
static inline void setSparseRegister(HyperLogLog* self, uint64_t index, uint8_t fsb)
{
    /* Add an element to the buffer if there is room */
    if (self->bufferSize < self->maxBufferSize) {
        self->sparseRegisterBuffer[self->bufferSize].index = index;
        self->sparseRegisterBuffer[self->bufferSize].fsb = fsb;
        self->bufferSize++;
    }
    /* Otherwise flush the buffer and then add */
    else {
        flushRegisterBuffer(self);
        self->bufferSize = 1;
        self->sparseRegisterBuffer[0].index = index;
        self->sparseRegisterBuffer[0].fsb = fsb;
    }
}

/* Set a HyperLogLog register */
static inline bool setRegister(HyperLogLog* self, uint64_t index, uint8_t newFsb) {
    self->added++; /* Increment method call counter */

    if (self->isSparse) {
        setSparseRegister(self, index, newFsb);

        /* Switch to dense representation? */
        if (self->listSize >= self->maxListSize) {
            transformToDense(self);
        }

        self->isCached = 0;
        return true;
    } else {
        uint64_t fsb = getDenseRegister(index, self->registers);

        if (newFsb > fsb) {
            setDenseRegister(index, (uint8_t)newFsb, self->registers);
            self->histogram[newFsb] += 1; /* Increment the new count */
            self->isCached = 0;

            if (self->histogram[fsb] == 0) {
                self->histogram[0] -= 1;
            } else {
                self->histogram[fsb] -= 1;
            }

            return true;
        }
    }

    return false;
}

/* Counts leading zeros in an unsigned 64bit integer */
uint8_t clz(uint64_t x) {
    uint8_t shift;

    static uint8_t const zeroes[] = {
        64, 63, 62, 62, 61, 61, 61, 61,
        60, 60, 60, 60, 60, 60, 60, 60,
        59, 59, 59, 59, 59, 59, 59, 59,
        59, 59, 59, 59, 59, 59, 59, 59,
        58, 58, 58, 58, 58, 58, 58, 58,
        58, 58, 58, 58, 58, 58, 58, 58,
        58, 58, 58, 58, 58, 58, 58, 58,
        58, 58, 58, 58, 58, 58, 58, 58,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56
    };

    /* Do a binary search to find which byte contains the first set bit */
    if (x >= (1UL << 32UL)) {
        if (x >= (1UL << 48UL)) {
            shift = (x >= (1UL << 56UL)) ? 56 : 48;
        } else {
            shift = (x >= (1UL << 40UL)) ? 40 : 32;
        }
    } else {
        if (x >= (1U << 16U)) {
            shift = (x >= (1U << 24U)) ? 24 : 16;
        } else {
            shift = (x >= (1U << 8U)) ? 8 : 0;
        }
    }

    /* Get the byte containing the first set bit */
    uint8_t fsbByte = (uint8_t)(x >> shift);

    /* Look up the leading zero count for (x >> shift) using byte. Subtract
     * the bit shift to get the leading zero count for x */
    return zeroes[fsbByte] - shift;
}

/* Helper functions for cardinality estimation */
double sigma(double x) {
    if (x == 1.0) {
        return INFINITY;
    }

    double zPrime;
    double y = 1.0;
    double z = x;

    do {
        x *= x;
        zPrime = z;
        z += x*y;
        y += y;
    } while(z != zPrime);

    return z;
}

double tau(double x) {
    if (x == 0.0 || x == 1.0) {
        return 0.0;
    }

    double zPrime;
    double y = 1.0;
    double z = 1 - x;

    do {
        x = sqrt(x);
        zPrime = z;
        y *= 0.5;
        z -= pow(1 - x, 2)*y;
    } while(zPrime != z);

    return z/3;
}

/* Check if a register index is valid */
bool isValidIndex(uint64_t index, uint64_t size)
{
    return index < size;
}

/* Public API Implementation */

/* Create a new HyperLogLog */
HyperLogLog* hll_init(unsigned short p, uint64_t seed, bool sparse,
                    uint64_t maxSparseListSize, uint64_t maxSparseBufferSize)
{
    HyperLogLog* hll = (HyperLogLog*)malloc(sizeof(HyperLogLog));

    if (!hll) return NULL;

    hll->p = p;
    hll->seed = seed;
    hll->added = 0;
    hll->cache = 0;
    hll->isCached = 0;
    hll->listSize = 0;
    hll->size = 1UL << p;
    hll->histogram = (uint64_t*)calloc(65, sizeof(uint64_t));

    if (!hll->histogram) {
        free(hll);
        return NULL;
    }

    hll->histogram[0] = hll->size; /* Set the zeroes count */
    hll->nodeCache = NULL;
    hll->registers = NULL;
    hll->sparseRegisterList = NULL;
    hll->sparseRegisterBuffer = NULL;
    hll->bufferSize = 0;

    if (sparse) {
        hll->isSparse = 1;

        if (maxSparseListSize > 0) {
            hll->maxListSize = maxSparseListSize;
        } else {
            uint64_t defaultSize = hll->size/4;
            uint64_t maxDefaultSize = 1 << 20;

            if (maxDefaultSize < defaultSize) {
                hll->maxListSize = maxDefaultSize;
            } else if (defaultSize <= 4) {
                hll->maxListSize = 2;
            } else {
                hll->maxListSize = defaultSize;
            }
        }

        if (maxSparseBufferSize > 0) {
            hll->maxBufferSize = maxSparseBufferSize;
        } else {
            uint64_t defaultSize = hll->maxListSize/2;
            uint64_t maxDefaultSize = 200000;

            if (maxDefaultSize < defaultSize) {
                hll->maxBufferSize = maxDefaultSize;
            } else {
                hll->maxBufferSize = defaultSize;
            }
        }

        hll->sparseRegisterBuffer = (Node*)malloc(sizeof(Node) * hll->maxBufferSize);

        if (!hll->sparseRegisterBuffer) {
            free(hll->histogram);
            free(hll);
            return NULL;
        }
    } else {
        uint64_t bytes = (hll->size*6)/8 + 1;
        hll->registers = (uint8_t*)calloc(bytes, sizeof(uint8_t));

        if (!hll->registers) {
            free(hll->histogram);
            free(hll);
            return NULL;
        }

        hll->isSparse = 0;
    }

    return hll;
}

/* Free a HyperLogLog */
void hll_free(HyperLogLog* hll)
{
    if (!hll) return;

    free(hll->histogram);

    if (hll->registers) {
        free(hll->registers);
    }

    if (hll->isSparse) {
        Node* next = NULL;
        Node* current = hll->sparseRegisterList;

        while (current != NULL) {
            next = current->next;
            free(current);
            current = next;
        }

        if (hll->sparseRegisterBuffer) {
            free(hll->sparseRegisterBuffer);
        }
    }

    free(hll);
}

/* Add an element to a HyperLogLog */
bool hll_add(HyperLogLog* hll, const uint8_t* data, uint64_t dataLen)
{
    uint64_t hash, index, newFsb;

    hash = MurmurHash64A((void*)data, dataLen, hll->seed);

    index = (hash >> (64 - hll->p)); /* Use the first p bits as an index */
    newFsb = hash << hll->p; /* Remove the first p bits */
    newFsb = clz(newFsb) + 1; /* Find the first set bit in the remaining bits */

    return setRegister(hll, index, (uint8_t)newFsb);
}

/* Get cardinality estimate */
uint64_t hll_cardinality(HyperLogLog* hll)
{
    if (hll->isCached) {
        return hll->cache;
    } else if (hll->isSparse && hll->bufferSize > 0) {
        flushRegisterBuffer(hll);
    }

    double alpha = 0.7213475;
    double m = (double)hll->size;
    double z = m * tau((m - (double)hll->histogram[hll->p + 1])/m);

    uint64_t k;
    for (k = 64 - hll->p; k >= 1; --k) {
        z += hll->histogram[k];
        z *= 0.5;
    }

    z += m * sigma((double)hll->histogram[0]/m);
    uint64_t estimate = (uint64_t)round(alpha * m * (m/z));

    hll->cache = estimate;
    hll->isCached = 1;

    return estimate;
}

/* Merge another HyperLogLog into the current one */
bool hll_merge(HyperLogLog* dest, HyperLogLog* src)
{
    if (src->size != dest->size) {
        return false;
    }

    dest->isCached = 0;

    for (uint64_t i = 0; i < dest->size; i++) {
        uint64_t newVal;
        uint64_t oldVal;

        if (dest->isSparse) {
            oldVal = getSparseRegister(dest, i);
        } else {
            oldVal = getDenseRegister(i, dest->registers);
        }

        if (src->isSparse) {
            newVal = getSparseRegister(src, i);
        } else {
            newVal = getDenseRegister(i, src->registers);
        }

        if (oldVal < newVal) {
            setRegister(dest, i, (uint8_t)newVal);
        }
    }

    return true;
}

/* Get a Murmur64A hash of data */
uint64_t hll_hash(HyperLogLog* hll, const uint8_t* data, uint64_t dataLen)
{
    return MurmurHash64A((void*)data, dataLen, hll->seed);
}

/* Get register value by index */
uint64_t hll_get_register(HyperLogLog* hll, uint64_t index)
{
    if (!isValidIndex(index, hll->size)) return 0;

    if (hll->isSparse) {
        return getSparseRegister(hll, index);
    } else {
        return getDenseRegister(index, hll->registers);
    }
}

/* Get the number of registers */
uint64_t hll_size(HyperLogLog* hll)
{
    return hll->size;
}

/* Get the seed value */
uint64_t hll_seed(HyperLogLog* hll)
{
    return hll->seed;
}
