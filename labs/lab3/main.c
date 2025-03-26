#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdalign.h>
#include <labs_random.h>

static const uint32_t K[] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

static const uint32_t INITIAL_H[] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
};

static const uint8_t PADD_BUF[] = {
    128, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

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

void sha256_display_hash(const Sha256Hash* h) {
    for (size_t i = 0; i < 8; i++) {
        printf("%08X", h->h[i]);
    }
    putchar('\n');
}

// 5.3.3
void sha256_init(Sha256State* state) {
    state->length = 0;
    state->current_word = 0;
    memcpy(&state->hash.h, INITIAL_H, sizeof(state->hash.h));
}

// 4.1.2
#define ROTR_U32(x, n) ((x) >> (n) | (x) << (32 - (n)))

static uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t sha256_big_sigma_0(uint32_t x) {
    return ROTR_U32(x, 2) ^ ROTR_U32(x, 13) ^ ROTR_U32(x, 22);
}

static uint32_t sha256_big_sigma_1(uint32_t x) {
    return ROTR_U32(x, 6) ^ ROTR_U32(x, 11) ^ ROTR_U32(x, 25);
}

static uint32_t sha256_small_sigma_0(uint32_t x) {
    return ROTR_U32(x, 7) ^ ROTR_U32(x, 18) ^ x >> 3;
}

static uint32_t sha256_small_sigma_1(uint32_t x) {
    return ROTR_U32(x, 17) ^ ROTR_U32(x, 19) ^ x >> 10;
}

// 6.2.2
static void sha256_process_block(Sha256Hash* hash, const Sha256Block* block) {
    // a b c d e f g h
    // 0 1 2 3 4 5 6 7
    alignas(32) uint32_t h[8];
    memcpy(h, &hash->h, sizeof(hash->h));
    alignas(32) uint32_t w[64];
    memcpy(w, &block->w, sizeof(block->w));
    for (size_t t = 16; t < 64; t++) {
        w[t] = sha256_small_sigma_1(w[t-2]) + w[t-7] + sha256_small_sigma_0(w[t-15]) + w[t-16];
    }

    for (size_t t = 0; t < 64; t++) {
        uint32_t t1 = h[7] + sha256_big_sigma_1(h[4]) + sha256_ch(h[4], h[5], h[6]) + K[t] + w[t];
        uint32_t t2 = sha256_big_sigma_0(h[0]) + sha256_maj(h[0], h[1], h[2]);
        h[7] = h[6];
        h[6] = h[5];
        h[5] = h[4];
        h[4] = h[3] + t1;
        h[3] = h[2];
        h[2] = h[1];
        h[1] = h[0];
        h[0] = t1 + t2;
    }

    for (size_t t = 0; t < 8; t++) {
        hash->h[t] += h[t];
    }
}

void sha256_accumulate_hash(Sha256State* state, size_t bytes_len, const char* bytes) {
    uint32_t w = state->current_word;
    uint64_t len = state->length;
    for (size_t i = 0; i < bytes_len; i++) {
        w <<= 8;
        w |= (uint8_t)bytes[i];
        if ((len + i) % 4 == 3) {
            size_t k = (len + i) / 4 % 16;
            state->current_block.w[k] = w;
            w = 0;
        }
        if ((len + i) % 64 == 63) {
            sha256_process_block(&state->hash, &state->current_block);
        }
    }
    state->length += bytes_len;
    state->current_word = w;
}

// 5.1.1
Sha256Hash sha256_finish(Sha256State* state) {
    size_t len = state->length;
    size_t j = 1 + (119 - len) % 64;
    sha256_accumulate_hash(state, j, (const char*)&PADD_BUF);
    uint8_t len_buf[8];
    len *= 8;
    for (size_t i = 7; i < 8; i--) {
        len_buf[i] = (uint8_t)(len & 0xFF);
        len >>= 8;
    }
    sha256_accumulate_hash(state, 8, (const char*)&len_buf);
    assert(state->length % 64 == 0);
    return state->hash;
}

// lab tasks

static uint32_t u32_popcount_dumm(uint32_t w) {
    uint32_t count = 0;
    while (w != 0) {
        w &= w - 1;
        count += 1;
    }
    return count;
}

#define READ_BUF_SIZE (1<<12)
#define TASK1_USAGE "usage: lab3 <filename>\n"

void task1(const char* filename) {
    if (filename == NULL) {
        printf(TASK1_USAGE);
        return;
    }
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        printf(TASK1_USAGE);
        return;
    }
    Sha256State state;
    sha256_init(&state);
    char buf[READ_BUF_SIZE];
    while (!feof(f)) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n == 0) break;
        sha256_accumulate_hash(&state, n, buf);
    }
    fclose(f);
    Sha256Hash hash = sha256_finish(&state);
    sha256_display_hash(&hash);
}

#define PANGRAMS_COUNT 5

static const char* PANGRAMS[PANGRAMS_COUNT] = {
    "Pack my box with five dozen liquor jugs",
    "The quick brown fox jumps over the lazy dog",
    "The five boxing wizards jump quickly",
    "Sphinx of black quartz, judge my vow",
    "Glib jocks quiz nymph to vex dwarf",
};

void task2() {
    char buf[64];
    Sha256State state;
    for (size_t i = 0; i < PANGRAMS_COUNT; i++) {
        size_t len = strlen(PANGRAMS[i]);
        strncpy(buf, PANGRAMS[i], sizeof(buf));
        buf[4] ^= 1; // change single bit

        sha256_init(&state);
        sha256_accumulate_hash(&state, len, PANGRAMS[i]);
        Sha256Hash pangram_hash = sha256_finish(&state);
        sha256_init(&state);
        sha256_accumulate_hash(&state, len, buf);
        Sha256Hash buf_hash = sha256_finish(&state);

        uint32_t bits_flipped = 0;
        for (size_t i = 0; i < 8; i++) {
            bits_flipped += u32_popcount_dumm(pangram_hash.h[i] ^ buf_hash.h[i]);
        }
        
        printf("# %zu\npangram = %s\nbuf = %s\n", i, PANGRAMS[i], buf);
        printf("pangram hash:\n");
        sha256_display_hash(&pangram_hash);
        printf("buf hash:\n");
        sha256_display_hash(&buf_hash);
        printf("bits flipped: %d\n\n\n", bits_flipped);
    }
}

#define K_MIN 5
#define K_MAX 32
#define K_TARGET 40
#define K_TESTS 100

uint64_t collisions_bitset[(((size_t)1<<K_MAX)+63)>>6];

typedef struct {
    uint64_t key;
    uint32_t seed;
} SeedTableEntry;

#define SEED_TABLE_SIZE (1<<20)
SeedTableEntry collision_seeds[SEED_TABLE_SIZE];

double time_now() {
    struct timespec t = {0};
    int ret = clock_gettime(CLOCK_MONOTONIC, &t);
    assert(ret == 0);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

void task3_1() {
    uint32_t buf[16];
    uint32_t rng_state = 42;
    Sha256State state;

    for (size_t k = K_MIN; k <= K_MAX; k++) {
        double start = time_now();
        size_t total_iterations = 0;
        for (size_t i = 0; i < K_TESTS; i++) {
            size_t bitset_size = (((size_t)1<<k)+63)>>6;
            memset(collisions_bitset, 0, sizeof(uint64_t) * bitset_size);
            for (;;) {
                for (size_t j = 0; j < 16; j++) {
                    buf[j] = xorshift_next(&rng_state);
                }
                total_iterations += 1;

                sha256_init(&state);
                sha256_accumulate_hash(&state, sizeof(buf), (const char*)buf);
                Sha256Hash hash = sha256_finish(&state);
                size_t index = hash.h[0] >> (32 - k);
                uint64_t mask = (uint64_t)1 << (index & 63);
                if ((collisions_bitset[index >> 6] & mask) != 0) {
                    // found collision
                    break;
                }
                collisions_bitset[index >> 6] |= mask;
            }
        }
        double end = time_now();
        printf("first %zu bit collision found in %lfs (%zu iterations)\n", k, (end - start)/(double)K_TESTS, total_iterations);
    }
}

void task3_2() {
    uint32_t buf[16];
    uint32_t rng_state = 42;
    Sha256State state;

    double start = time_now();
    size_t total_iterations = 0;
    
    uint32_t seed_a = 0;
    uint32_t seed_b = 0;

    for (size_t i = 0; i < K_TESTS; i++) {
        memset(collision_seeds, 0, sizeof(collision_seeds));
        for (;;) {
            uint32_t seed = rng_state;
            for (size_t j = 0; j < 16; j++) {
                buf[j] = xorshift_next(&rng_state);
            }
            total_iterations += 1;

            sha256_init(&state);
            sha256_accumulate_hash(&state, sizeof(buf), (const char*)buf);
            Sha256Hash hash = sha256_finish(&state);
            uint64_t key = ((uint64_t)hash.h[0] << 32 | hash.h[1]) >> (64 - K_TARGET);
            size_t index = key % SEED_TABLE_SIZE;
            if (collision_seeds[index].seed != 0 && collision_seeds[index].key == key) {
                seed_a = seed;
                seed_b = collision_seeds[index].seed;
                break;
            }
            collision_seeds[index].key = key;
            collision_seeds[index].seed = seed;
        }
    }
    double end = time_now();
    printf("first %d bit collision found in %lfs (%zu iterations)\nseed_a = %u, seed_b = %u\n", K_TARGET, (end - start), total_iterations, seed_a, seed_b);

    for (size_t j = 0; j < 16; j++) {
        buf[j] = xorshift_next(&seed_a);
    }
    sha256_init(&state);
    sha256_accumulate_hash(&state, sizeof(buf), (const char*)buf);
    Sha256Hash hash_a = sha256_finish(&state);
    for (size_t j = 0; j < 16; j++) {
        buf[j] = xorshift_next(&seed_b);
    }
    printf("hash_a:\n");
    sha256_display_hash(&hash_a);
    sha256_init(&state);
    sha256_accumulate_hash(&state, sizeof(buf), (const char*)buf);
    Sha256Hash hash_b = sha256_finish(&state);
    printf("hash_b:\n");
    sha256_display_hash(&hash_b);
}

int main(int argc, const char** argv) {
    //task1(argv[1]);
    //task2();
    // TODO plot 3.1 stats as time(k) somehow
    task3_1();
    //task3_2();
}
