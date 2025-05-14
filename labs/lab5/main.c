#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <labs_random.h>

#define MAX_FACTORS 16

typedef struct {
    uint32_t factors[MAX_FACTORS];
} PrimeFactors;

static uint32_t factorize(PrimeFactors* factors, uint32_t n) {
    assert(n > 0);
    memset(factors, 0, sizeof(PrimeFactors));
    uint32_t i = 0;
    if (n % 2 == 0) {
        factors->factors[i++] = 2;
        while (n % 2 == 0) n /= 2;
    }
    for (uint32_t j = 3; j * j <= n; j += 2) {
        if (n % j != 0) continue;
        factors->factors[i++] = j;
        while (n % j == 0) n /= j; 
    }
    if (n != 1) {
        factors->factors[i++] = n;
    }
    return i;
}

// integer pow(a, e) mod p
static uint32_t ipow_mod(uint32_t a, uint32_t e, uint32_t p) {
    uint64_t ret = 1;
    uint64_t b = a;
    while (e != 0) {
        if (e & 1) {
            ret *= b;
            ret %= p;
        }
        e >>= 1;
        b *= b;
        b %= p;
    }
    return (uint32_t)ret;
}

// integer ceil(sqrt(n))
static uint32_t isqrt(uint32_t n) {
    if (n == 0) return 0;
    uint32_t r = 0;
    uint32_t i = 15;
    while (i < 16) {
        uint32_t t = r | 1 << i;
        if (t * t < n)
            r = t;
        i -= 1;
    }
    return r + 1;
}

// GF(p)* = <g>
static uint32_t find_generator(uint32_t p) { 
    PrimeFactors f;
    uint32_t n = p - 1;
    uint32_t c = factorize(&f, n);
    for (uint32_t g = 2; g < p; g++) {
        bool is_generator = true;
        for (uint32_t i = 0; i < c; i++) {
            if (ipow_mod(g, n / f.factors[i], p) == 1) {
                is_generator = false;
                break;
            }
        }
        if (is_generator) 
            return g;
    }
    assert(false);
}

static uint32_t inv_mod(uint32_t a, uint32_t p) {
    // works only when p - prime 
    // a^(p-1) = 1 mod p  (euler's theorem)
    // a^(p-2) = a^-1 mod p
    return ipow_mod(a, p-2, p);
}

#define DL_TABLE_SIZE (1<<16)
#define DL_TABLE_MASK (DL_TABLE_SIZE-1)
static uint32_t dl_table[DL_TABLE_SIZE];
static uint32_t dl_values[DL_TABLE_SIZE];

static void table_put(uint32_t a, uint32_t v) {
    uint32_t i = a & DL_TABLE_MASK;
    while (dl_table[i] != 0 && dl_table[i] != a) {
        i += 1;
        i &= DL_TABLE_MASK;
    }
    dl_table[i] = a;
    dl_values[i] = v;
}

static uint32_t table_get(uint32_t a) {
    uint32_t i = a & DL_TABLE_MASK;
    while (dl_table[i] != 0) {
        if (dl_table[i] == a) 
            return dl_values[i];
        i += 1;
        i &= DL_TABLE_MASK;
    }
    return 0;
}

static uint32_t discrete_log(uint32_t a, uint32_t g, uint32_t p) {
    assert(a != 0 && a < p);
    uint32_t n = p - 1;
    uint32_t m = isqrt(n);
    assert(m < DL_TABLE_SIZE);
    memset(dl_table, 0, sizeof(dl_table));
    // baby step
    uint32_t t = 1;
    for (uint32_t i = 0; i < m; i++) {
        table_put(t, i);
        t *= g;
        t %= p;
    }
    // s = g^-m
    uint32_t gm = ipow_mod(g, m, p);
    uint64_t s = inv_mod(gm, p);
    assert(gm * s % p == 1);
    // giant step
    uint64_t x = a;
    for (uint32_t i = 0; i < m; i++) {
        uint32_t j = table_get((uint32_t)x);
        if (j != 0) {
            return j + m * i;
        }
        x *= s;
        x %= p;
    }
    assert(false);
}

static void show_find_generator() {
    printf("GF(%d)* = <%d>\n", 41, find_generator(41));
    printf("GF(%d)* = <%d>\n", 43, find_generator(43));
    printf("GF(%d)* = <%d>\n", 499, find_generator(499));
    printf("GF(%d)* = <%d>\n", 997, find_generator(997));
    printf("GF(%d)* = <%d>\n", 1000003, find_generator(1000003));
}

static void show_discrete_log() {
    uint32_t p = 1000003;
    uint32_t g = find_generator(p);
    printf("GF(%d)* = <%d>\n", 1000003, find_generator(1000003));
    uint32_t l;
    printf("log(%d) = %d\n", 42426, l=discrete_log(42426, g, p));
    printf("%d^%d = %d\n", g, l, ipow_mod(g, l, p));
    printf("log(%d) = %d\n", 1834, l=discrete_log(1834, g, p));
    printf("%d^%d = %d\n", g, l, ipow_mod(g, l, p));
    printf("log(%d) = %d\n", 123456, l=discrete_log(123456, g, p));
    printf("%d^%d = %d\n", g, l, ipow_mod(g, l, p));
}

static void show_diffie_hellman() {
    // TODO time based initialization 
    uint32_t rng = 42;

    uint32_t p = 1000003;
    uint32_t g = find_generator(p);
    // printf("GF(%d)* = <%d>\n", p, g);
    // generating private keys:
    uint32_t a = xorshift_next(&rng) % p;
    uint32_t b = xorshift_next(&rng) % p;

    printf("Alice generated priveate key %d\n", a);
    printf("Bob generated priveate key %d\n", b);

    // public keys:
    uint32_t ga = ipow_mod(g, a, p);
    uint32_t gb = ipow_mod(g, b, p);
    
    // Alice sends ga to Bob
    // Bob sends gb to Alice 
    printf("Alice send %d\n", ga);
    printf("Bob send %d\n", gb);

    // shared key
    uint32_t key_alice = ipow_mod(gb, a, p);
    uint32_t key_bob = ipow_mod(ga, b, p);

    assert(key_alice == key_bob);

    printf("Alice's key: %d\n", key_alice);
    printf("Bob's key: %d\n", key_bob);
}

int main() {
    // show_find_generator();
    // show_discrete_log();
    // show_diffie_hellman();

    // uint32_t p = 1000003;
    // uint32_t g = find_generator(p);
    // printf("GF(%d)* = <%d>\n", p, g);
    // printf("%d = %d\n", 49, isqrt(49));
    // printf("%d = %d\n", 48, isqrt(48));
    // printf("%d = %d\n", 50, isqrt(50));
    // printf("%d^-1 = %d\n", 42, inv_mod(42, p));
    // printf("%d^-1 = %d\n", 26, inv_mod(26, p));
}
