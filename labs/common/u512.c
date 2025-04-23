#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <labs_u512.h>

U512 U512_ZERO = {0};
U512 U512_ONE = (U512){ .d = { 1 }};
U512 U512_TWO = (U512){ .d = { 2 }};
U512 U512_MINUS_ONE = (U512){ .d = { 
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
}};

void u512_print_hex(const U512* num) {
    for (size_t i = 0; i < U512_DIGITS; i++) {
        printf("%08x", num->d[U512_DIGITS - 1 - i]);
    }
    putchar('\n');
}

U512 u512_from_u64(uint64_t num) {
    U512 ret = {0};
    ret.d[0] = num & 0xFFFFFFFF;
    ret.d[1] = num >> 32;
    return ret;
}

static int bigint_cmp(const uint32_t* a, size_t a_len, const uint32_t* b, size_t b_len) {
    // TODO handle different lenght
    assert(a_len == b_len);

    for (size_t i = 0; i < a_len; i++) {
        uint32_t da = a[a_len - 1 - i];
        uint32_t db = b[b_len - 1 - i];
        if (da != db) {
            if (da < db) return -1;
            else return 1;
        }
    }
    return 0;
}

int u512_cmp(const U512* a, const U512* b) {
    return bigint_cmp(&a->d[0], U512_DIGITS, &b->d[0], U512_DIGITS);
}

static uint32_t bigint_inplace_shl(uint32_t* digits, size_t len, uint32_t ammount) {
    if (ammount > 31) {
        uint32_t shift_positions = ammount / 32;
        uint32_t fill_count = shift_positions * sizeof(uint32_t);
        uint32_t move_count = (len - shift_positions) * sizeof(uint32_t);
        memmove((char*)&digits[shift_positions], (char*)&digits[0], move_count);
        memset((char*)&digits[0], 0, fill_count);
        ammount &= 31;
    }
    if (ammount == 0) return 0;

    uint32_t carry = 0;
    for (size_t i = 0; i < len; i++) {
        uint32_t d = digits[i];
        digits[i] = d << ammount | carry;
        carry = d >> (32 - ammount);
    }
    return carry;
}

U512 u512_shl(const U512* num, uint32_t ammount) {
    U512 ret = {0};
    memcpy(&ret, num, sizeof(U512));
    bigint_inplace_shl(&ret.d[0], U512_DIGITS, ammount);
    return ret;
}

static uint32_t bigint_inplace_shr(uint32_t* digits, size_t len, uint32_t ammount) {
    if (ammount > 31) {
        uint32_t shift_positions = ammount / 32;
        uint32_t fill_count = shift_positions * sizeof(uint32_t);
        uint32_t move_count = (U512_DIGITS - shift_positions) * sizeof(uint32_t);
        memmove((char*)&digits[0], (char*)&digits[shift_positions], move_count);
        memset((char*)&digits[move_count], 0, fill_count);
        ammount &= 31;
    }
    if (ammount == 0) return 0;
    uint32_t carry = 0;
    for (size_t i = 0; i < len ; i++) {
        size_t j = len - 1 - i;
        uint32_t d = digits[j];
        digits[j] = d >> ammount | carry;
        carry = d << (32 - ammount);
    }
    return carry;
}

U512 u512_shr(const U512* num, uint32_t ammount) {
    U512 ret = {0};
    memcpy(&ret, num, sizeof(U512));
    bigint_inplace_shr(&ret.d[0], U512_DIGITS, ammount);
    return ret;
}

static void bigint_inplace_bitnot(uint32_t* digits, size_t len) {
    for (size_t i = 0; i < len; i++) {
        digits[i] = ~digits[i];
    }
}

U512 u512_bitnot(const U512* num) {
    U512 ret = {0};
    memcpy(&ret, num, sizeof(U512));
    bigint_inplace_bitnot(&ret.d[0], U512_DIGITS);
    return ret;
}

static uint32_t u32_popcnt_dumb(uint32_t num) {
    uint32_t cnt = 0;
    while (num != 0) {
        num &= num - 1;
        cnt += 1;
    }
    return cnt;
}

static uint32_t u32_ctrz(uint32_t num) {
    if (num == 0) return 32;
    return u32_popcnt_dumb(num ^ (num - 1)) - 1;
}

static uint32_t u32_clz_dumb(uint32_t num) {
    for (size_t i = 0; i < 32; i++) {
        size_t j = 31 - i;
        if ((num >> j & 1) != 0) {
            return i;
        }
    }
    return 32;
}

uint32_t u512_ctrz(const U512* num) {
    uint32_t count = 0;
    for (size_t i = 0; i < U512_DIGITS; i++) {
        if (num->d[i] != 0) {
            count += u32_ctrz(num->d[i]);
            break;
        }
        count += 32;
    }
    return count;
}

static uint32_t bigint_clz(const uint32_t* digits, size_t len) {
    uint32_t count = 0;
    for (size_t i = 0; i < len; i++) {
        size_t j = len - 1 - i;
        if (digits[j] != 0) {
            count += u32_clz_dumb(digits[j]);
            break;
        }
        count += 32;
    }
    return count;
}

uint32_t u512_clz(const U512* num) {
    return bigint_clz(&num->d[0], U512_DIGITS);
}

static uint32_t bigint_add_impl(uint32_t* a, const uint32_t* b) {
    uint64_t carry = 0;
    for (size_t i = 0; i < U512_DIGITS; i++) {
        carry += (uint64_t)a[i] + (uint64_t)b[i];
        a[i] = carry & 0xFFFFFFFF;
        carry >>= 32;
    }
    return carry;
}

typedef struct {
    U512 num;
    uint32_t carry;
} AddImplResult;

static AddImplResult u512_add_impl(const U512* a, const U512* b) {
    AddImplResult ret = {0};
    uint64_t carry = 0;
    for (size_t i = 0; i < U512_DIGITS; i++) {
        carry += (uint64_t)a->d[i] + (uint64_t)b->d[i];
        ret.num.d[i] = carry & 0xFFFFFFFF;
        carry >>= 32;
    }
    ret.carry = carry;
    return ret;
}

DivResult u512_div_rem(const U512* a, const U512* b) {
    U512 r = {0};
    U512 q = {0};
    size_t n = 512 - u512_clz(a);
    for (size_t i = 0; i < n; i++) {
        size_t j = n - 1 - i;
        r = u512_shl(&r, 1);
        r.d[0] |= a->d[j / 32] >> (j & 31) & 1;
        if (u512_cmp(&r, b) >= 0) {
            r = u512_sub(&r, b);
            q.d[j / 32] |= 1 << (j & 31);
        }
    }
    return (DivResult){ .r = r, .q = q };
}

// r_out_len >= denom_len
// q_out_len >= num_len
static void bigint_div_rem(
    const uint32_t* num,
    size_t num_len,
    const uint32_t* denom,
    size_t denom_len,
    uint32_t* q_out,
    size_t q_out_len,
    uint32_t* r_out,
    size_t r_out_len
) {
    assert(r_out_len >= denom_len && q_out_len > num_len);

    memset((void*)r_out, 0, r_out_len);
    memset((void*)q_out, 0, q_out_len);

    size_t bitwidth = 32 * num_len;
    size_t n = bitwidth - bigint_clz(num, num_len);
    for (size_t i = 0; i < n; i++) {
        size_t j = n - 1 - i;
        bigint_inplace_shl(r_out, r_out_len, 1);
        r_out[0] |= num[j / 32] >> (j & 31) & 1;
        if (bigint_cmp(r_out, r_out_len, denom, denom_len) >= 0) {
            //r = u512_sub(&r, b);
            q_out[j / 32] |= 1 << (j & 31);
        }
    }
}

U512 u512_add(const U512* a, const U512* b) {
    AddImplResult ret = u512_add_impl(a, b);
    return ret.num;
}

U512 u512_sub(const U512* a, const U512* b) {
    U512 not_b = u512_bitnot(b);
    U512 minus_b = u512_add(&not_b, &U512_ONE);
    return u512_add(a, &minus_b);
}

// TODO mul & pow by modulo 2^32
U512 u512_mul(const U512* a, const U512* b);
U512 u512_pow(const U512* num, const U512* power);

U512 u512_div(const U512* a, const U512* b) {
    return u512_div_rem(a, b).q;
}
U512 u512_mod(const U512* a, const U512* b) {
    return u512_div_rem(a, b).r;
}

// TODO fix overflow on 512th bit
// for now works only for 511-bit m
U512 u512_mul_mod(const U512* a, const U512* b, const U512* m) {
    U512 ret = {0};
    U512 ca = {0};
    memcpy(&ca, a, sizeof(U512));
    ca = u512_mod(&ca, m);

    size_t n = 512 - u512_clz(b);
    for (size_t i = 0; i < n; i++) {
        if ((b->d[i / 32] >> (i & 31) & 1) != 0) {
            U512 t1 = u512_add(&ret, &ca);
            ret = u512_mod(&t1, m);
        }
        U512 t2 = u512_shl(&ca, 1);
        ca = u512_mod(&t2, m);
    }

    return ret;
}

U512 u512_pow_mod(const U512* num, const U512* power, const U512* m) {
    U512 ret = U512_ONE;
    U512 ca = {0};
    memcpy(&ca, num, sizeof(U512));

    size_t n = 512 - u512_clz(power);
    for (size_t i = 0; i < n; i++) {
        if ((power->d[i / 32] >> (i & 31) & 1) != 0) {
            ret = u512_mul_mod(&ret, &ca, m);
        }
        ca = u512_mul_mod(&ca, &ca, m);
    }

    return ret;
}
