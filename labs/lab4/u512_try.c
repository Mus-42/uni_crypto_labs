#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <labs_random.h>
#include <labs_u512.h>
#include <tommath.h>

bool is_pseudo_prime_miller_rabin(const U512* num, const U512* base) {
    // even
    if ((num->d[0] & 1) == 0) {
        return u512_cmp(num, &U512_TWO) == 0;
    }
    // <= 1
    if (u512_cmp(num, &U512_ONE) <= 0) 
        return false;

    // n-1 = 2^s * d
    U512 n1 = u512_add(num, &U512_MINUS_ONE);
    //U512 n1 = u512_sub(num, &U512_ONE);
    uint32_t s = u512_ctrz(&n1);
    U512 d = u512_shr(&n1, s);
    U512 x = u512_pow_mod(base, &d, num);

    //printf("%u %d %u ", d.d[0], s, num->d[0]);
    //u512_print_hex(&x);
    if (u512_cmp(&x, &U512_ONE) == 0) return true; 

    for (size_t i = 0; i < s; i++) {
        U512 y = u512_mul_mod(&x, &x, num);
        if (u512_cmp(&y, &U512_ONE) == 0 && u512_cmp(&x, &U512_ONE) != 0 && u512_cmp(&x, &n1) != 0)
            return false;
        x = y;
    }

    //printf("almost there!\n");
    //u512_print_hex(&x);

    //return true;
    return u512_cmp(&x, &U512_ONE) == 0;
}

// e - public exponent
// n - public modulus
// m - message
U512 rsa_encrypt(const U512* e, const U512* n, const U512* m) {
    // c = m^e mod n 
    return u512_pow_mod(m, e, n);
}

// d - private exponent
// n - public modulus
// c - ciphertext 
U512 rsa_decrypt(const U512* d, const U512* n, const U512* c) {
    // m = (c)^d mod n
    return u512_pow_mod(c, d, n);
}
