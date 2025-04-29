#ifndef LAB3_SHA256_INCLUDE
#define LAB3_SHA256_INCLUDE

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t w[16];
} Sha256Block;

typedef struct {
    uint32_t h[8];
} Sha256Hash;

typedef struct {
    Sha256Hash hash;
    Sha256Block current_block;
    uint64_t length; // in _bytes_
    uint32_t current_word;
} Sha256State;

void sha256_init(Sha256State* state);
void sha256_display_hash(const Sha256Hash* h);
void sha256_accumulate_hash(Sha256State* state, size_t bytes_len, const char* bytes);
Sha256Hash sha256_finish(Sha256State* state);

#endif//LAB3_SHA256_INCLUDE
