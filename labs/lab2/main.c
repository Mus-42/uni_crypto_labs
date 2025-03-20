#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <labs_random.h>

#define MAX_AES_ROUNDS 32

// 128-bit block for state | key
//
// w[0] w[1] w[2] w[3]
//
// 0    4    8    12
// 1    5    9    13
// 2    6   10    14
// 3    7   11    15

typedef struct {
    uint32_t w[4];
} Block;

Block block_from_bytes(const char* bytes) {
    Block block = {0};
    const unsigned char* ubytes = (const unsigned char*)bytes;
    for (size_t i = 0; i < 4; i++) {
        // usigned char allows to break pointer aliasing rules
        unsigned char* b = (unsigned char*)&block.w[i];
        b[0] = ubytes[i*4 + 0];
        b[1] = ubytes[i*4 + 1];
        b[2] = ubytes[i*4 + 2];
        b[3] = ubytes[i*4 + 3];
    }

    return block;
}

static uint32_t u32_swap_bytes(uint32_t w) {
    return w >> 24 | (w >> 8 & 0xFF00) | (w << 8 & 0xFF0000) | (w << 24 & 0xFF000000);
}

Block block_from_words_ne(const uint32_t* w) { 
    Block block = {0};
    for (size_t i = 0; i < 4; i++) {
        block.w[i] = u32_swap_bytes(w[i]);
    }
    return block;
}

Block block_from_words_be(const uint32_t* w) { 
    Block block = {0};
    for (size_t i = 0; i < 4; i++) {
        block.w[i] = w[i];
    }
    return block;
}

static void display_block(Block* block) {
    for (size_t i = 0; i < 4; i++) {
        printf("%08X", u32_swap_bytes(block->w[i]));
    }
    putchar('\n');
}

// in GF(2^8) with polynomial x^8 + x^4 + x^3 + x + 1

uint8_t ff_mult_dumm(uint8_t a, uint8_t b) {
    const unsigned poly = (unsigned)0b100011011; // x^8 + x^4 + x^3 + x + 1
    unsigned value = 0;
    // multiply
    for (unsigned i = 0; i < 8; i++) {
        unsigned mask = (unsigned)0 - (b >> i & 1);
        value ^= (unsigned)a << i & mask;
    }
    // take module 
    for (unsigned i = 6; i < 8; i--) {
        unsigned mask = (unsigned)0 - (value >> (i + 8) & 1);
        value ^= poly << i & mask;
    }
    return value;
}

uint8_t ff_mult_tbl[256][256];
uint8_t ff_inv[256] = {0};
uint8_t sbox[256];
uint8_t sbox_inv[256];
uint32_t rcon[MAX_AES_ROUNDS+1];

static void init_ff() {
    for (size_t i = 0; i < 256; i++) {
        for (size_t j = i; j < 256; j++) {
            uint8_t prod = ff_mult_dumm(i, j);

            ff_mult_tbl[i][j] = prod;
            ff_mult_tbl[j][i] = prod;

            if (prod == 1) {
                ff_inv[i] = j;
                ff_inv[j] = i;
            }
        }
    }
}

static void init_sbox() {
    for (size_t i = 0; i < 256; i++) {
        unsigned b = ff_inv[i];
        unsigned raw_v = 0x63 ^ b ^ (b << 1) ^ (b << 2) ^ (b << 3) ^ (b << 4);
        raw_v ^= (raw_v >> 8);
        sbox[i] = raw_v;
        sbox_inv[sbox[i]] = i;
    }
}

static void init_rcon() {
    uint8_t x_i = 1;
    for (size_t i = 0; i <= MAX_AES_ROUNDS; i++) {
        rcon[i] = (uint32_t)x_i << 24;
        x_i = ff_mult_tbl[0x02][x_i];
    }
}

static void init_tables() {
    init_ff();
    init_sbox();
    init_rcon();
}

static void display_table(const uint8_t* table) {
    for (size_t i = 0; i < 256; i++) {
        printf("%02X ", table[i]);
        if ((i & 0xF) == 0xF) {
            putchar('\n');
        }
    }
}

static void display_tables() {
    puts("ff_inv:");
    display_table(ff_inv);
    puts("\nsbox:");
    display_table(sbox);
    puts("\nsbox_inv:");
    display_table(sbox_inv);
    puts("\n\nrcon:");
    for (size_t i = 0; i <= MAX_AES_ROUNDS; i++) {
        printf("%08X\n", rcon[i]);
    }
}

uint32_t sub_word(uint32_t w) {
    unsigned char* b = (unsigned char*)&w;
    b[0] = sbox[b[0]];
    b[1] = sbox[b[1]];
    b[2] = sbox[b[2]];
    b[3] = sbox[b[3]];
    return w;
}

void sub_bytes(Block* state) {
    for (size_t i = 0; i < 4; i++) {
        // usigned char allows to break pointer aliasing rules
        unsigned char* b = (unsigned char*)&state->w[i];
        b[0] = sbox[b[0]];
        b[1] = sbox[b[1]];
        b[2] = sbox[b[2]];
        b[3] = sbox[b[3]];
    }
}

void inv_sub_bytes(Block* state) {
    for (size_t i = 0; i < 4; i++) {
        // usigned char allows to break pointer aliasing rules
        unsigned char* b = (unsigned char*)&state->w[i];
        b[0] = sbox_inv[b[0]];
        b[1] = sbox_inv[b[1]];
        b[2] = sbox_inv[b[2]];
        b[3] = sbox_inv[b[3]];
    }
}

void add_round_key(Block* state, const Block* key) {
    state->w[0] ^= key->w[0];
    state->w[1] ^= key->w[1];
    state->w[2] ^= key->w[2];
    state->w[3] ^= key->w[3];
}

void inv_add_round_key(Block* state, const Block* key) {
    add_round_key(state, key);
}

void mix_columns(Block* state) {
    for (size_t i = 0; i < 4; i++) {
        uint32_t b_word = 0;
        unsigned char* a = (unsigned char*)&state->w[i];
        unsigned char* b = (unsigned char*)&b_word;
        b[0] = ff_mult_tbl[0x02][a[0]] ^ ff_mult_tbl[0x03][a[1]] ^ a[2] ^ a[3];
        b[1] = ff_mult_tbl[0x02][a[1]] ^ ff_mult_tbl[0x03][a[2]] ^ a[3] ^ a[0];
        b[2] = ff_mult_tbl[0x02][a[2]] ^ ff_mult_tbl[0x03][a[3]] ^ a[0] ^ a[1];
        b[3] = ff_mult_tbl[0x02][a[3]] ^ ff_mult_tbl[0x03][a[0]] ^ a[1] ^ a[2];
        state->w[i] = b_word;
    }
}

void inv_mix_columns(Block* state) {
    for (size_t i = 0; i < 4; i++) {
        uint32_t b_word = 0;
        unsigned char* a = (unsigned char*)&state->w[i];
        unsigned char* b = (unsigned char*)&b_word;
        b[0] = ff_mult_tbl[0x0e][a[0]] ^ ff_mult_tbl[0x0b][a[1]] ^ ff_mult_tbl[0x0d][a[2]] ^ ff_mult_tbl[0x09][a[3]];
        b[1] = ff_mult_tbl[0x09][a[0]] ^ ff_mult_tbl[0x0e][a[1]] ^ ff_mult_tbl[0x0b][a[2]] ^ ff_mult_tbl[0x0d][a[3]];
        b[2] = ff_mult_tbl[0x0d][a[0]] ^ ff_mult_tbl[0x09][a[1]] ^ ff_mult_tbl[0x0e][a[2]] ^ ff_mult_tbl[0x0b][a[3]];
        b[3] = ff_mult_tbl[0x0b][a[0]] ^ ff_mult_tbl[0x0d][a[1]] ^ ff_mult_tbl[0x09][a[2]] ^ ff_mult_tbl[0x0e][a[3]];
        state->w[i] = b_word;
    }
}

// 0    4    8    12
// 1    5    9    13
// 2    6   10    14
// 3    7   11    15

void shift_rows(Block* b) {
    unsigned s[4];
    for (size_t i = 1; i < 4; i++) {
        s[0] = ((unsigned char*)&b->w[0])[i];
        s[1] = ((unsigned char*)&b->w[1])[i];
        s[2] = ((unsigned char*)&b->w[2])[i];
        s[3] = ((unsigned char*)&b->w[3])[i];
        ((unsigned char*)&b->w[0])[i] = s[(0 + i) % 4];
        ((unsigned char*)&b->w[1])[i] = s[(1 + i) % 4];
        ((unsigned char*)&b->w[2])[i] = s[(2 + i) % 4];
        ((unsigned char*)&b->w[3])[i] = s[(3 + i) % 4];
    }
}

void inv_shift_rows(Block* b) {
    unsigned s[4];
    for (size_t i = 1; i < 4; i++) {
        s[0] = ((unsigned char*)&b->w[0])[i];
        s[1] = ((unsigned char*)&b->w[1])[i];
        s[2] = ((unsigned char*)&b->w[2])[i];
        s[3] = ((unsigned char*)&b->w[3])[i];
        ((unsigned char*)&b->w[0])[i] = s[(4 - i) % 4];
        ((unsigned char*)&b->w[1])[i] = s[(5 - i) % 4];
        ((unsigned char*)&b->w[2])[i] = s[(6 - i) % 4];
        ((unsigned char*)&b->w[3])[i] = s[(7 - i) % 4];
    }
}

static uint32_t rot_word(uint32_t w) {
    return w << 8 | (w >> 24 & 0xFF);
}

void key_expansion(const uint32_t* key, size_t key_len, Block* round_keys, size_t rounds) {
    uint32_t w[4 * (MAX_AES_ROUNDS + 1)];
    memcpy(w, key, 4 * key_len);
    for (size_t i = key_len; i <= 4 * rounds; i++) {
        uint32_t t = w[i-1];
        if (i % key_len == 0) {
            t = sub_word(rot_word(t));
            t ^= rcon[i / key_len - 1];
        } else if (key_len > 6 && i % key_len == 4) {
            t = sub_word(t);
        }
        w[i] = t ^ w[i - key_len];
    }
    for (size_t i = 0; i <= MAX_AES_ROUNDS; i++) {
        round_keys[i] = block_from_words_ne(&w[i * 4]);
    }
}

void cipher_block(Block* state, const Block* round_keys, size_t rounds) {
    add_round_key(state, &round_keys[0]);
    for (size_t i = 1; i < rounds; i++) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, &round_keys[i]);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, &round_keys[rounds]);
}

void decipher_block(Block* state, const Block* round_keys, size_t rounds) {
    add_round_key(state, &round_keys[rounds]);
    for (size_t i = rounds-1; i > 0; i--) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, &round_keys[i]);
        inv_mix_columns(state);
    }
    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, &round_keys[0]);
}

static void show_test_vectors() {
    // KEYS

    //const uint32_t example_key[8] = {
    //    0x603DEB10,
    //    0x15CA71BE,
    //    0x2B73AEF0,
    //    0x857D7781,
    //    0x1F352C07,
    //    0x3B6108D7,
    //    0x2D9810A3,
    //    0x0914DFF4,
    //};
    //const uint32_t example_key[4] = {
    //    0x00010203,
    //    0x04050607,
    //    0x08090a0b,
    //    0x0c0d0e0f,
    //};
    //const uint32_t example_key[4] = {
    //    0x2B7E1516,
    //    0x28AED2A6,
    //    0xABF71588,
    //    0x09CF4F3C,
    //};
    const uint32_t example_key[8] = {
        0x00010203,
        0x04050607,
        0x08090A0B,
        0x0C0D0E0F,
        0x10111213,
        0x14151617,
        0x18191A1B,
        0x1C1D1E1F,
    };

    // INPUTS

    //const uint32_t example_input[4] = {
    //    0x3243F6A8,
    //    0x885A308D,
    //    0x313198A2,
    //    0xE0370734,
    //};
    const uint32_t example_input[4] = {
        0x00112233,
        0x44556677,
        0x8899AABB,
        0xCCDDEEFF,
    };

    Block round_keys[MAX_AES_ROUNDS+1];
    key_expansion(example_key, 8, round_keys, MAX_AES_ROUNDS);
    //for (size_t i = 0; i <= MAX_AES_ROUNDS; i++) {
    //    display_block(&round_keys[i]);
    //}
    

    Block state = block_from_words_ne(example_input);
    printf("Initial: ");
    display_block(&state);

    cipher_block(&state, round_keys, 14);
    printf("Encrypted: ");
    display_block(&state);

    decipher_block(&state, round_keys, 14);
    printf("Decrypted: ");
    display_block(&state);

}

static uint32_t u32_popcount_dumm(uint32_t w) {
    uint32_t count = 0;
    while (w != 0) {
        w &= w - 1;
        count += 1;
    }
    return count;
}

static uint32_t block_diff_bits_count(const Block* a, const Block* b) {
    uint32_t count = 0;
    for (size_t i = 0; i < 4; i++) {
        count += u32_popcount_dumm(a->w[i] ^ b->w[i]);
    }
    return count;
}

#define NUM_TESTS 100000
#define ROUNDS_MIN 1
#define ROUNDS_MAX 18
#define ROUNDS_STEP 1

void lab_task() {
    uint32_t random_state = 42;
    uint32_t buf[8];
    Block round_keys[MAX_AES_ROUNDS+1];
    
    for (size_t round_count = ROUNDS_MIN; round_count <= ROUNDS_MAX; round_count += ROUNDS_STEP) {
        double flip_average_a = 0.;
        double flip_average_b = 0.;
        for (size_t t = 0; t < NUM_TESTS; t++) {
            // random state
            for (size_t i = 0; i < 4; i++) buf[i] = xorshift_next(&random_state);
            Block state_orig = block_from_words_ne(buf);
            // random key
            uint32_t key[4];
            for (size_t i = 0; i < 4; i++) key[i] = xorshift_next(&random_state);

            // trivial case
            Block state_trivial = state_orig;
            key_expansion(key, 4, round_keys, round_count);
            cipher_block(&state_trivial, round_keys, round_count);

            // a) flip single state bit
            Block state_a = state_orig;
            uint32_t aflip_at = xorshift_next(&random_state) & 127;
            state_a.w[aflip_at >> 5] ^= 1 << (aflip_at & 31);
            cipher_block(&state_a, round_keys, round_count);

            // b) flip single key bit
            Block state_b = state_orig;
            uint32_t bflip_at = xorshift_next(&random_state) & 127;
            key[bflip_at >> 5] ^= 1 << (bflip_at & 31);
            key_expansion(key, 4, round_keys, round_count);
            cipher_block(&state_b, round_keys, round_count);

            flip_average_a += (double)block_diff_bits_count(&state_trivial, &state_a);
            flip_average_b += (double)block_diff_bits_count(&state_trivial, &state_b);
        }
        flip_average_a /= (double)NUM_TESTS;
        flip_average_b /= (double)NUM_TESTS;
            
        printf("\n\n round count = %zu\n", round_count);
        printf("a) av. flipped %.2lf/128 = %.8lf%%\n", flip_average_a, flip_average_a * (100. / 128.));
        printf("b) av. flipped %.2lf/128 = %.8lf%%\n", flip_average_b, flip_average_b * (100. / 128.));
    }
}

int main(int argc, const char** argv) {
    init_tables();
    //display_tables();
    //show_test_vectors();
    lab_task();
}
