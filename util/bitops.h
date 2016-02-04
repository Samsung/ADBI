/* Bit manipulation functions.
 *
 * Note: Many of the functions defined here use approaches from the excellent "Bit Twiddling Hacks" website by
 * Sean Eron Anderson. */

#ifndef BITOPS_H_
#define BITOPS_H_

typedef uint32_t bitops_t;
#define bitops_bits ((int) (sizeof(bitops_t) * 8))

/* Extract bits low:high from value. */
static inline bitops_t bits(bitops_t value, unsigned int low, unsigned int high) {
    assert((low < bitops_bits) && (high < bitops_bits) && (high >= low));
    int bit_count = (1 + high - low) & (bitops_bits - 1);
    bitops_t mask = (1 << (bit_count)) - 1;
    return (value >> low) & mask;
}

/* Extract bit n from value. */
static inline bitops_t bit(bitops_t value, int n) {
    return bits(value, n, n);
}

/* Sign extend given value, msb is the current most significant bit. */
static inline bitops_t sign_extend(bitops_t value, unsigned int msb) {
    assert(msb > 0);
    bitops_t mask = 1U << (msb - 1);
    value = value & ((1U << msb) - 1);
    return (value ^ mask) - mask;
}

/* Count the number of bits set in value. */
static inline bitops_t bits_count(bitops_t value) {
    bitops_t c;
    for (c = 0; value; c++) {
        value &= value - 1;     // clear the least significant bit set
    }
    return c;
}

/* Get the most significant bit set. */
static inline bitops_t get_msb(bitops_t value) {
    bitops_t c = 0;
    while (value >>= 1)
        ++c;
    return c;
}

/* Shift the bits of the given value into the bits inside the mask.
 *
 * Example:
 *  mask    10110011
 *
 *            [abcde]
 *  val        10110
 *
 *         [a0bc00de]
 *  result  10010010
 */
static inline bitops_t get_bits_by_mask(bitops_t val, bitops_t mask) {
    bitops_t ret = 0;
    int sh;
    for (sh = bitops_bits; sh >= 0; --sh) {
        if (mask & (1 << sh)) {
            ret <<= 1;
            ret |= (val & (1 << sh)) ? 1 : 0;
        }
    }
    return ret;
}

/* Collect the bits in the mask and construct a single value. This is the inverse of get_bits_by_mask. */
static inline bitops_t set_bits_by_mask(bitops_t val, bitops_t mask) {
    bitops_t ret = 0;
    int sh;
    for (sh = 0; val; ++sh) {
        if (mask & (1 << sh)) {
            /* the bit is set */
            ret |= (val & 1) << sh;
            val >>= 1;
        }
    }
    return ret;
}

/* Create a bit mask with the given range of bits lit. */
static inline bitops_t bit_mask(int low, int high) {
    assert(low <= high);
    return ((1 << (high - low + 1)) - 1) << low;
}

#endif
