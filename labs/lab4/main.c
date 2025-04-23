#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <labs_random.h>
#include <tommath.h>



#define MP_DIGITS_ROUND_UP(BITS) (BITS + MP_DIGIT_BIT - 1) / MP_DIGIT_BIT
#define PRIME_BITS 512
#define PRIME_MP_DIGITS MP_DIGITS_ROUND_UP(PRIME_BITS)
#define MILLER_RABIN_BASE_BITS 512
#define MILLER_RABIN_BASE_MP_DIGITS MP_DIGITS_ROUND_UP(MILLER_RABIN_BASE_BITS)
#define MILLER_RABIN_ROUNDS 100
#define MODULUS_BITS 1024

#define PRIMALITY_TEST_PRINT_INFO true


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


// TODO
// mp_err mp_mask_n_bits(const mp_int* a, int n) {
//     if (a->used < MP_DIGITS_ROUND_UP) 
//     return MP_OKAY;
// }

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
        if ((err = mp_rand(&base, MILLER_RABIN_BASE_MP_DIGITS)) != MP_OKAY)
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

mp_err pick_large_prime(int digits, int test_rounds, mp_int* prime) {
    mp_err err;
    while (true) {
        if ((err = mp_rand(prime, digits)) != MP_OKAY)
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
} RSAState;

mp_err rsa_init(RSAState* s) {
    return mp_init_multi(&s->p, &s->q, &s->e, &s->d, &s->n, NULL);
}

void rsa_clear(RSAState* s) {
    mp_clear_multi(&s->p, &s->q, &s->e, &s->d, &s->n, NULL);
}

mp_err rsa_pick_key(RSAState* s) {
    mp_err err;
    mp_int phi, t;
    
    // pick p and q
    if ((err = pick_large_prime(PRIME_MP_DIGITS, MILLER_RABIN_ROUNDS, &s->p)) != MP_OKAY)
        return err; 
    if ((err = pick_large_prime(PRIME_MP_DIGITS, MILLER_RABIN_ROUNDS, &s->q)) != MP_OKAY)
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
    // m = (c)^d mod n
    return mp_exptmod(c, &s->d, &s->n, m);
}

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
        // if ((err = mp_init_u64(&a, i)) != MP_OKAY) {
        //     goto CLEANUP;
        // }
        // for (uint64_t j = 2; j < i; j++) {
        //     if ((err = mp_init_u64(&b, j)) != MP_OKAY) {
        //         goto CLEANUP;
        //     }
        //     bool result;
        //     miller_rabin_test(&a, &b, &result);
        //     if (!result) {
        //         is_prime = false;
        //         break;
        //     }
        // }
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
    if ((err = pick_large_prime(PRIME_MP_DIGITS, MILLER_RABIN_ROUNDS, &prime)) != MP_OKAY)
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
        goto CLEANUP;
    
    mp_int m, c;
    
    if ((err = mp_init_multi(&m, &c, NULL)) != MP_OKAY) 
        goto CLEANUP;

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

    mp_clear_multi(&m, &c, NULL);
CLEANUP:
    rsa_clear(&state);
    return err;
}

int main(void) {
    mp_err err;
    // if ((err = show_primes_up_to_1000()) != MP_OKAY) 
    //     goto PRINT_ERR;
    // if ((err = show_pick_large_prime()) != MP_OKAY) 
    //     goto PRINT_ERR;
    if ((err = show_rsa_demo()) != MP_OKAY)
        goto PRINT_ERR;
    return EXIT_SUCCESS;
PRINT_ERR:
    printf("ERR: %s\n", mp_error_to_string(err));
    return EXIT_FAILURE;
}
