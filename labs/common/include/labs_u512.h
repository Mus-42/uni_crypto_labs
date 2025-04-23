#ifndef CRPT_LABS_CMMN_U512
#define CRPT_LABS_CMMN_U512

#include <stdint.h>

#define U512_DIGITS 16

typedef struct {
    uint32_t d[U512_DIGITS]; 
} U512;

extern U512 U512_ZERO;
extern U512 U512_ONE;
extern U512 U512_TWO;
extern U512 U512_MINUS_ONE;

void u512_print_hex(const U512* num);
U512 u512_from_u64(uint64_t num);

int u512_cmp(const U512* a, const U512* b);

U512 u512_shl(const U512* num, uint32_t ammount);
U512 u512_shr(const U512* num, uint32_t ammount);

U512 u512_bitnot(const U512* num);

uint32_t u512_clz(const U512* num);
uint32_t u512_ctrz(const U512* num);


U512 u512_add(const U512* a, const U512* b);
U512 u512_sub(const U512* a, const U512* b);
U512 u512_mul(const U512* a, const U512* b);
U512 u512_div(const U512* a, const U512* b);
U512 u512_mod(const U512* a, const U512* b);
U512 u512_pow(const U512* num, const U512* power);

typedef struct {
    U512 r;
    U512 q;
} DivResult;

DivResult u512_div_rem(const U512* a, const U512* b);

U512 u512_add_mod(const U512* a, const U512* b, const U512* mod);
U512 u512_mul_mod(const U512* a, const U512* b, const U512* mod);
U512 u512_pow_mod(const U512* num, const U512* power, const U512* mod);

#endif
