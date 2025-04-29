#include "lab3_sha256.h"
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <tommath.h>

#include <labs_random.h>

#define MP_DIGITS_ROUND_UP(BITS) (BITS + MP_DIGIT_BIT - 1) / MP_DIGIT_BIT
#define PRIME_BITS 512
#define MILLER_RABIN_BASE_BITS 512
#define MILLER_RABIN_ROUNDS 100
#define MODULUS_BITS 1024

#define PRIMALITY_TEST_PRINT_INFO false 

// MILLER RABIN

// a - number
// b - base
mp_err miller_rabin_test(const mp_int *a, const mp_int *b, bool *result) {
    mp_int n1, y, r;
    mp_err err = MP_OKAY;

    // sanity check: b > 1
    if (mp_cmp_d(b, 1uL) != MP_GT)
        return MP_VAL;

    // n1 = a - 1
    if ((err = mp_init_copy(&n1, a)) != MP_OKAY)
        return err;
    if ((err = mp_sub_d(&n1, 1uL, &n1)) != MP_OKAY)
        goto LBL_ERR1;

    // n1 = 2^s * r
    if ((err = mp_init_copy(&r, &n1)) != MP_OKAY)
        goto LBL_ERR1;
    int s = mp_cnt_lsb(&r);
    if ((err = mp_div_2d(&r, s, &r, NULL)) != MP_OKAY)
        goto LBL_ERR2;

    // y = b^r mod a 
    if ((err = mp_init(&y)) != MP_OKAY)
        goto LBL_ERR2;
    if ((err = mp_exptmod(b, &r, a, &y)) != MP_OKAY)
        goto LBL_END;


    // if y == 1 or y == n1 -> no sense in test, go to end
    if ((mp_cmp_d(&y, 1uL) == MP_EQ) || (mp_cmp(&y, &n1) == MP_EQ)) {
        *result = true;
        goto LBL_END;
    }

    // while j <= s-1 and y != n1
    for (int j = 1; (j <= (s - 1)) && (mp_cmp(&y, &n1) != MP_EQ); j++) {
        if ((err = mp_sqrmod(&y, a, &y)) != MP_OKAY)
            goto LBL_END;

        // if y == 1 then composite
        if (mp_cmp_d(&y, 1uL) == MP_EQ) {
            *result = false;
            goto LBL_END;
        }
    }

    // if y != n1 then composite
    if (mp_cmp(&y, &n1) != MP_EQ) {
        *result = false;
        goto LBL_END;
    }

    // probably prime now
    *result = true;
    
    // cleanup
LBL_END:
    mp_clear(&y);
LBL_ERR2:
    mp_clear(&r);
LBL_ERR1:
    mp_clear(&n1);
    return err;
}


mp_err mp_rand_int_bits(mp_int* a, int bits) {
    mp_err err;
    if ((err = mp_rand(a, MP_DIGITS_ROUND_UP(bits))) != MP_OKAY)
        return err;
    if (MP_DIGITS_ROUND_UP(bits) != a->used || a->used < 0)
        return MP_ERR;
    // set all above high bit to 0
    a->dp[MP_DIGITS_ROUND_UP(bits) - 1] &= (1ull << ((bits - 1) % MP_DIGIT_BIT)) - 1;
    // set high bit to 1
    a->dp[MP_DIGITS_ROUND_UP(bits) - 1] |= 1ull << ((bits - 1) % MP_DIGIT_BIT);
    return err;
}

mp_err miller_rabin_test_rounds(const mp_int *a, int rounds, bool *result) {
    mp_int base, a2;
    mp_err err;
    
    int cmp_2 = mp_cmp_d(a, 2uL);
    if (cmp_2 != MP_GT) {
        // if 2 -> prime
        // if < 2 -> not prime
        *result = cmp_2 == MP_EQ;
        return MP_OKAY;
    }

    if ((err = mp_init(&base)) != MP_OKAY) 
        return err;

    // a2 = a - 2
    if ((err = mp_init_copy(&a2, a)) != MP_OKAY) 
        goto LBL_ERR1;
    if ((err = mp_sub_d(&a2, 2, &a2)) != MP_OKAY)
        goto CLEANUP;

    for (int i = 0; i < rounds; i++) {
        bool test_result;
        // base = (rand()) % (a - 2) + 2
        if ((err = mp_rand_int_bits(&base, MILLER_RABIN_BASE_BITS)) != MP_OKAY)
            goto CLEANUP;
        if ((err = mp_mod(&base, &a2, &base)) != MP_OKAY)
            goto CLEANUP;
        if ((err = mp_add_d(&base, 2, &base)) != MP_OKAY)
            goto CLEANUP;

        if (PRIMALITY_TEST_PRINT_INFO) {
            printf("testing base: ");
            if ((err = mp_fwrite(&base, 10, stdout)) != MP_OKAY)
                goto CLEANUP;
            putchar('\n');
        }
        // now base is random integer in range [2, a-1]
    
        if ((err = miller_rabin_test(a, &base, &test_result)) != MP_OKAY)
            goto CLEANUP;


        if (!test_result) {
            *result = false;
            goto CLEANUP;
        }
    }

    *result = true;
CLEANUP:
    mp_clear(&a2);
LBL_ERR1:
    mp_clear(&base);
    return err;
}

// message 

#define MSG_BUF_BYTES (MODULUS_BITS / 8)
// MSG & OEAP parts
#define MSG_BUF_OAEP_BYTES 16
#define MSG_BUF_MSG_BYTES (MSG_BUF_BYTES - MSG_BUF_OAEP_BYTES)

typedef struct {
    char buf[MSG_BUF_BYTES];
} MsgBuf;


// OAEP
// (RFC 8017)

mp_err msg_init(MsgBuf* msg_buf, const char* msg) {
    size_t len = strlen(msg);
    if (len > MSG_BUF_MSG_BYTES)
        return MP_VAL;

    memset(&msg_buf->buf[len], 0, MSG_BUF_BYTES-len);
    memcpy(&msg_buf->buf[0], msg, len);

    return MP_OKAY;
}

#define SPLIT_N (MSG_BUF_BYTES / 2)

void msg_print_buf(const MsgBuf* msg_buf) {
    for (size_t i = 0; i < MSG_BUF_BYTES; i++) {
        printf("%02x", (unsigned char)msg_buf->buf[i]);

        if (i % SPLIT_N == SPLIT_N - 1) {
            putchar('\n');
        }
    }
    for (size_t i = 0; i < MSG_BUF_BYTES; i++) {
        if (isprint(msg_buf->buf[i])) {
            printf(" %c", msg_buf->buf[i]);
        } else {
            printf(" .");
        }

        if (i % SPLIT_N == SPLIT_N - 1) {
            putchar('\n');
        }
    }
    putchar('\n');
}

uint32_t get_rng_seed() {
    struct timespec t = {0};
    int ret = clock_gettime(CLOCK_MONOTONIC, &t);
    assert(ret == 0);
    return (uint32_t)t.tv_nsec;
}

// #define OAEP_HASH_LEN (16 * 4) // in bytes
#define OAEP_HASH_LEN 16  // in bytes

// https://www.rfc-editor.org/rfc/rfc8017#appendix-B.2.1
// mgf1
void oaep_mgf(uint32_t seed, char* buf, size_t len) {
    char seed_buf[8];
    memcpy(&seed_buf[0], &seed, 4);
    Sha256State sha256;
    for (uint32_t i = 0; i < len; i += OAEP_HASH_LEN) {
        memcpy(&seed_buf[4], &i, 4);
        sha256_init(&sha256);
        sha256_accumulate_hash(&sha256, sizeof(seed_buf), seed_buf);
        Sha256Hash hash = sha256_finish(&sha256);
        size_t n = len - i;
        if (n > OAEP_HASH_LEN) 
            n = OAEP_HASH_LEN;
        memcpy(&buf[i], &hash, n);
    }
}


mp_err pick_large_prime(int bits, int test_rounds, mp_int* prime) {
    mp_err err;
    while (true) {
        if ((err = mp_rand_int_bits(prime, bits)) != MP_OKAY)
            return err;

        bool is_prime;
        if ((err = miller_rabin_test_rounds(prime, test_rounds, &is_prime)) != MP_OKAY)
            return err;

        if (PRIMALITY_TEST_PRINT_INFO && is_prime) {
            printf("prime: ");
            if ((err = mp_fwrite(prime, 10, stdout)) != MP_OKAY)
                break;
            printf("\n");
        }

        if (is_prime)
            break;

        if (PRIMALITY_TEST_PRINT_INFO) {
            printf("composite: ");
            if ((err = mp_fwrite(prime, 10, stdout)) != MP_OKAY)
                break;
            printf("\n");
        }
    }
    return err;
}

typedef struct {
    // primes
    mp_int p;
    mp_int q;
    // exponents
    mp_int e;
    mp_int d;
    // modulus
    mp_int n;
    // d mod p-1
    mp_int d_p;
    // d mod q-1
    mp_int d_q;
    // q^-1 mod p
    mp_int q_inv;
} RSAState;

mp_err rsa_init(RSAState* s) {
    return mp_init_multi(&s->p, &s->q, &s->e, &s->d, &s->n, &s->d_p, &s->d_q, &s->q_inv, NULL);
}

void rsa_clear(RSAState* s) {
    mp_clear_multi(&s->p, &s->q, &s->e, &s->d, &s->n, &s->d_p, &s->d_q, &s->q_inv, NULL);
}

mp_err rsa_pick_key(RSAState* s) {
    mp_err err;
    mp_int phi, t;
    
    // pick p and q
    if ((err = pick_large_prime(PRIME_BITS, MILLER_RABIN_ROUNDS, &s->p)) != MP_OKAY)
        return err; 
    if ((err = pick_large_prime(PRIME_BITS, MILLER_RABIN_ROUNDS, &s->q)) != MP_OKAY)
        return err;

    // n = p * q
    if ((err = mp_mul(&s->p, &s->q, &s->n)) != MP_OKAY)
        return err;

    // e = 2^16 + 1 (just pick fixed value)
    if ((err = mp_init_u64(&s->e, (1<<16) + 1)) != MP_OKAY)
        return err;
    
    // phi(n) = (p-1)(q-1)
    if ((err = mp_init_copy(&phi, &s->p)) != MP_OKAY)
        return err;
    if ((err = mp_init_copy(&t, &s->q)) != MP_OKAY)
        goto LBL_ERR1;
    if ((err = mp_sub_d(&t, 1, &t)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_sub_d(&phi, 1, &phi)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_mul(&phi, &t, &phi)) != MP_OKAY)
        goto CLEANUP;

    // d = e^-1 mod phi(n)
    if ((err = mp_invmod(&s->e, &phi, &s->d)) != MP_OKAY)
        goto CLEANUP;

    // d_p = d mod p - 1
    if ((err = mp_copy(&s->p, &t)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_sub_d(&t, 1, &t)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_mod(&s->d, &t, &s->d_p)) != MP_OKAY)
        goto CLEANUP;

    // d_q = d mod q - 1
    if ((err = mp_copy(&s->q, &t)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_sub_d(&t, 1, &t)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_mod(&s->d, &t, &s->d_q)) != MP_OKAY)
        goto CLEANUP;

    // q_inv = q^-1 mod p
    if ((err = mp_invmod(&s->q, &s->p, &s->q_inv)) != MP_OKAY)
        goto CLEANUP;

CLEANUP:
    mp_clear(&t);
LBL_ERR1:
    mp_clear(&phi);
    return err;
}

mp_err rsa_encrypt(const RSAState* s, const mp_int* m, mp_int* c) {
    // c = (m)^e mod n 
    return mp_exptmod(m, &s->e, &s->n, c);
}

mp_err rsa_decrypt_dumm(const RSAState* s, const mp_int* c, mp_int* m) {
    // m = (c)^d mod n
    return mp_exptmod(c, &s->d, &s->n, m);
}

mp_err rsa_decrypt(const RSAState* s, const mp_int* c, mp_int* m) {
    mp_err err;
    mp_int m1, m2, h;

    if ((err = mp_init_multi(&m1, &m2, &h, NULL)) != MP_OKAY)
        return err;
    
    // m1 = c^d_p mod p
    if ((err = mp_exptmod(c, &s->d_p, &s->p, &m1)) != MP_OKAY)
        goto CLEANUP;
    
    // m2 = c^d_q mod q
    if ((err = mp_exptmod(c, &s->d_q, &s->q, &m2)) != MP_OKAY)
        goto CLEANUP;
    
    // h = q_inv (m1 - m2) mod p
    if ((err = mp_submod(&m1, &m2, &s->p, &h)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_mulmod(&h, &s->q_inv, &s->p, &h)) != MP_OKAY)
        goto CLEANUP;
    
    // m = m2 + h q
    if ((err = mp_mulmod(&h, &s->q, &s->n, m)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_addmod(m, &m2, &s->n, m)) != MP_OKAY)
        goto CLEANUP;
    
CLEANUP:
    mp_clear_multi(&m1, &m2, &h, NULL);
    return err;
}

// OAEP

mp_err bignum_to_msg(MsgBuf* msg_buf, mp_int num) {
    mp_err err;

    return err;
}

#define DB_LEN (MSG_BUF_BYTES - OAEP_HASH_LEN - 1)

mp_err msg_oaep_encrypt(const char* msg) {
    size_t msg_len = strlen(msg);
    // too long
    if (msg_len + 2 * OAEP_HASH_LEN + 2 > MSG_BUF_BYTES)
        return MP_VAL;

    Sha256State sha256;
    sha256_init(&sha256);
    // hash of empty label
    Sha256Hash l_hash = sha256_finish(&sha256);

    uint32_t seed = get_rng_seed();

    char db[DB_LEN] = {0};

    // db = l_hash || 0* || 1 || msg
    memcpy(&db[0], &l_hash, OAEP_HASH_LEN);
    db[DB_LEN-msg_len-1] = 1;
    memcpy(&db[DB_LEN-msg_len], msg, msg_len);

    for (size_t i = 0; i < 0; i++) {
        
    }

    return MP_OKAY;
}

// SHOW

mp_err show_primes_up_to_1000() {
    mp_int a, b;
    mp_err err;
    
    if ((err = mp_init_multi(&a, &b, NULL)) != MP_OKAY) 
        return err; 

    size_t count = 0;
    for (uint64_t i = 2; i < 1000; i++) {
        bool is_prime = true;
        if ((err = mp_init_u64(&a, i)) != MP_OKAY)
            goto CLEANUP;
        if ((err = miller_rabin_test_rounds(&a, MILLER_RABIN_ROUNDS, &is_prime)) != MP_OKAY)
            goto CLEANUP;
        if (is_prime) {
            count += 1;
            printf("%zu\n", i);
        }
    }
    printf("total primes: %zu\n", count);
CLEANUP:
    mp_clear_multi(&a, &b, NULL);
    return err;
}

mp_err show_pick_large_prime() {
    mp_err err;
    mp_int prime;

    if (mp_init(&prime) != MP_OKAY) return EXIT_FAILURE;
    if ((err = pick_large_prime(PRIME_BITS, MILLER_RABIN_ROUNDS, &prime)) != MP_OKAY)
        goto CLEANUP;
    if ((err = mp_fwrite(&prime, 10, stdout)) != MP_OKAY)
        goto CLEANUP;
    putchar('\n');

CLEANUP:
    mp_clear(&prime);
    return err;
}

mp_err show_rsa_demo() {
    mp_err err;
    RSAState state;
    if ((err = rsa_init(&state)) != MP_OKAY) 
        return err; 
    if ((err = rsa_pick_key(&state)) != MP_OKAY) 
        goto LBL_ERR1;
    
    mp_int m, c;
    
    if ((err = mp_init_multi(&m, &c, NULL)) != MP_OKAY) 
        goto LBL_ERR1;

    if ((err = mp_init_u32(&m, 4242004242)) != MP_OKAY) 
        goto CLEANUP;

    printf("m = ");
    if ((err = mp_fwrite(&m, 10, stdout)) != MP_OKAY)
        goto CLEANUP;
    putchar('\n');

    if ((err = rsa_encrypt(&state, &m, &c)) != MP_OKAY) 
        goto CLEANUP;

    printf("encrypt(m) = ");
    if ((err = mp_fwrite(&c, 10, stdout)) != MP_OKAY)
        goto CLEANUP;
    putchar('\n');

    if ((err = rsa_decrypt_dumm(&state, &c, &m)) != MP_OKAY) 
        goto CLEANUP;

    printf("decrypt_dumm(c) = ");
    if ((err = mp_fwrite(&m, 10, stdout)) != MP_OKAY)
        goto CLEANUP;
    putchar('\n');

    if ((err = rsa_decrypt(&state, &c, &m)) != MP_OKAY) 
        goto CLEANUP;

    printf("decrypt(c) = ");
    if ((err = mp_fwrite(&m, 10, stdout)) != MP_OKAY)
        goto CLEANUP;
    putchar('\n');

CLEANUP:
    mp_clear_multi(&m, &c, NULL);
LBL_ERR1:
    rsa_clear(&state);
    return err;
}

mp_err show_oaep_demo() {
    mp_err err;

    MsgBuf buf;
    msg_init(&buf, "a message");
    printf("initial buf:\n");
    msg_print_buf(&buf);

    return err;
}

#ifndef LAB4_NOMAIN
int main(void) {
    mp_err err;
    // if ((err = show_primes_up_to_1000()) != MP_OKAY) 
    //     goto PRINT_ERR;
    // if ((err = show_pick_large_prime()) != MP_OKAY) 
    //     goto PRINT_ERR;
    // if ((err = show_rsa_demo()) != MP_OKAY)
    //     goto PRINT_ERR;
    if ((err = show_oaep_demo()) != MP_OKAY)
        goto PRINT_ERR;
    return EXIT_SUCCESS;
PRINT_ERR:
    printf("ERR: %s\n", mp_error_to_string(err));
    return EXIT_FAILURE;
}
#endif//LAB4_NOMAIN
