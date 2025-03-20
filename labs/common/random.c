#include <stdint.h>

uint32_t xorshift_next(uint32_t* state) {
    uint32_t v = *state;

    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;

    *state = v;
    return v;
}
