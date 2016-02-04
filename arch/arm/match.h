#ifndef MATCH_H_
#define MATCH_H_

#ifndef NDEBUG
#include <string.h>
#endif

#if 0

/* NOTE: for literal strings strlen(str) == sizeof(str) - 1 */
#define match_get_bit(pattern, n)           \
    ((n) < (sizeof(pattern) - 1) ?          \
     (pattern)[sizeof(pattern) - (n) - 2] :  \
     '-')

#else

#define match_get_bit(pattern, n)           \
    ((n) < (strlen(pattern)) ?              \
     (pattern)[strlen(pattern) - (n) - 1] :  \
     '-')

#endif

#define match_mask_bit(pattern, n) \
    (((uint32_t) ((match_get_bit(pattern, n)) != '-' ? 1 : 0)) << (n))

#define match_mask(pattern)         \
    (match_mask_bit(pattern, 0) |   \
     match_mask_bit(pattern, 1)  |   \
     match_mask_bit(pattern, 2)  |   \
     match_mask_bit(pattern, 3)  |   \
     match_mask_bit(pattern, 4)  |   \
     match_mask_bit(pattern, 5)  |   \
     match_mask_bit(pattern, 6)  |   \
     match_mask_bit(pattern, 7)  |   \
     match_mask_bit(pattern, 8)  |   \
     match_mask_bit(pattern, 9)  |   \
     match_mask_bit(pattern, 10) |   \
     match_mask_bit(pattern, 11) |   \
     match_mask_bit(pattern, 12) |   \
     match_mask_bit(pattern, 13) |   \
     match_mask_bit(pattern, 14) |   \
     match_mask_bit(pattern, 15) |   \
     match_mask_bit(pattern, 16) |   \
     match_mask_bit(pattern, 17) |   \
     match_mask_bit(pattern, 18) |   \
     match_mask_bit(pattern, 19) |   \
     match_mask_bit(pattern, 20) |   \
     match_mask_bit(pattern, 21) |   \
     match_mask_bit(pattern, 22) |   \
     match_mask_bit(pattern, 23) |   \
     match_mask_bit(pattern, 24) |   \
     match_mask_bit(pattern, 25) |   \
     match_mask_bit(pattern, 26) |   \
     match_mask_bit(pattern, 27) |   \
     match_mask_bit(pattern, 28) |   \
     match_mask_bit(pattern, 29) |   \
     match_mask_bit(pattern, 30) |   \
     match_mask_bit(pattern, 31))

#define match_val_bit(pattern, n) \
    (((uint32_t) ((match_get_bit(pattern, n)) == '1' ? 1 : 0)) << (n))

#define match_val(pattern)          \
    (match_val_bit(pattern, 0)  |   \
     match_val_bit(pattern, 1)   |   \
     match_val_bit(pattern, 2)   |   \
     match_val_bit(pattern, 3)   |   \
     match_val_bit(pattern, 4)   |   \
     match_val_bit(pattern, 5)   |   \
     match_val_bit(pattern, 6)   |   \
     match_val_bit(pattern, 7)   |   \
     match_val_bit(pattern, 8)   |   \
     match_val_bit(pattern, 9)   |   \
     match_val_bit(pattern, 10)  |   \
     match_val_bit(pattern, 11)  |   \
     match_val_bit(pattern, 12)  |   \
     match_val_bit(pattern, 13)  |   \
     match_val_bit(pattern, 14)  |   \
     match_val_bit(pattern, 15)  |   \
     match_val_bit(pattern, 16)  |   \
     match_val_bit(pattern, 17)  |   \
     match_val_bit(pattern, 18)  |   \
     match_val_bit(pattern, 19)  |   \
     match_val_bit(pattern, 20)  |   \
     match_val_bit(pattern, 21)  |   \
     match_val_bit(pattern, 22)  |   \
     match_val_bit(pattern, 23)  |   \
     match_val_bit(pattern, 24)  |   \
     match_val_bit(pattern, 25)  |   \
     match_val_bit(pattern, 26)  |   \
     match_val_bit(pattern, 27)  |   \
     match_val_bit(pattern, 28)  |   \
     match_val_bit(pattern, 29)  |   \
     match_val_bit(pattern, 30)  |   \
     match_val_bit(pattern, 31))

#ifndef DEBUG_MATCH

#define match(value, pattern) \
    (((value) & match_mask(pattern)) == match_val(pattern))

#else

static inline int match(insn_t value, const char * pattern) {
    insn_t mask = match_mask(pattern);
    insn_t testval = match_val(pattern);
    assert((strlen(pattern) & 0x7) == 0);
    return (value & mask) == testval;
}

#endif

#endif /* MATCH_H_ */
