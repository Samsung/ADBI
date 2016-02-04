/* Division support for injectables.
 *
 * ARM processors don't have hardware support for division.  For this reason, division must be implemented in software.
 * Normally, the GCC compiler handles this problem.  Whenever a division operation is required, GCC inserts a call to
 * a function, which replaces the division operation.  This and other helper functions are implemented in libgcc, a
 * library (usually static) distributed together with GCC.  The compiler and linker take care about linking with this
 * file automatically.  This library should even be included in the library when linking with the -nostdlib and similar
 * switches.  (However, this depends on the configuration.)  The libgcc library has its own dependencies as well, which
 * we can not supply.  For this reason, we need our own division algorithms.
 */
#ifndef DIVISION_H_
#define DIVISION_H_

static inline unsigned long long int llsr(unsigned long long int val, unsigned int shift) {
    union {
        unsigned long long int ll;
        unsigned int w[2];
    } u;
    
    u.ll = val;
    u.w[0] >>= shift;
    u.w[0] |= u.w[1] << (32 - shift);
    u.w[1] >>= shift;
    
    return u.ll;
}

/* Divide two unsigned 32-bit integers.  Behavior when divisor is zero is undefined.  Result is rounded towards zero. */
static unsigned int div(unsigned int dividend, unsigned int divisor) {

    /* Find (log base 2) + 1 of the number, (0b01 -> shift = 2; 0b01010 -> shift == 5) */
    int shift;
    for (shift = 0; shift < 32; ++shift) {
        unsigned int d = 1 << shift;
        if (d > divisor)
            break;
    }
    
    /* divisor is between
            db = (1 << (shift - 1))     and     da = (1 << shift)
       so the quotient will be between
            a = dividend / da           and     b = dividend / db
    */
    unsigned int a = shift < 32 ? dividend >> shift : 0xffffffff;
    unsigned int b = dividend >> (shift - 1);
    
    /* quoutient is between a and b -- we'll perform a binary search */
    unsigned long long int quoutient = (b >> 1) + (a >> 1);
    
    /* binary search */
    while (b > a) {
        unsigned int q = quoutient * divisor;
        
        if (q > dividend) {
            b = quoutient - 1;
        } else { /* q <= dividend */
            unsigned int rest = dividend - q;
            if (rest < divisor) {
                /* bingo! */
                return quoutient;
            } else {
                a = quoutient + 1;
            }
        }
        quoutient = (b >> 1) + (a >> 1);
    }
    
    quoutient = a;
    return quoutient;
}

/* Divide two unsigned 64-bit integers.  Behavior when divisor is zero is undefined.  Result is rounded towards zero. */
static unsigned long long int divl(unsigned long long int dividend, unsigned long long int divisor) {

    /* Find (log base 2) + 1 of the number, (0b01 -> shift = 2; 0b01010 -> shift == 5) */
    int shift;
    for (shift = 0; shift < 64; ++shift) {
        unsigned long long int d = 1 << shift;
        if (d > divisor)
            break;
    }
    
    /* divisor is between
            db = (1 << (shift - 1))     and     da = (1 << shift)
       so the quotient will be between
            a = dividend / da           and     b = dividend / db
    */
    unsigned long long int a = shift < 64 ? llsr(dividend, shift) : 0xffffffffffffffff;
    unsigned long long int b = llsr(dividend, shift - 1);
    
    /* quoutient is between a and b -- we'll perform a binary search */
    unsigned long long int quoutient = (b >> 1) + (a >> 1);
    
    /* binary search */
    while (b > a) {
        unsigned long long int q = quoutient * divisor;
        
        if (q > dividend) {
            b = quoutient - 1;
        } else { /* q <= dividend */
            unsigned long long int rest = dividend - q;
            if (rest < divisor) {
                /* bingo! */
                return quoutient;
            } else {
                a = quoutient + 1;
            }
        }
        quoutient = (b >> 1) + (a >> 1);
    }
    
    quoutient = a;
    return quoutient;
}

/* Fast division by 10. */
#ifdef __aarch64__

static unsigned int div10(unsigned int v) {
    return ((unsigned long long) v * 0xcccccccd) >> 35;
}

#else

static __attribute__((naked)) unsigned int div10(unsigned int v) {
    /* Instead of dividing by 10, we multiply the dividend by (0.1 * 2 ^ (32 + 3)).  The result will be 64-bit wide.
     * The first 32-bits of the result will be the quotient multiplied by (2 ^ 3).  Shifting this value right by 3 bits
     * will give us the final quotient.
     */
    __asm("ldr   r1, =0xcccccccd    \n"
          "umull r1, r0, r1, r0     \n"
          "lsr   r0, #3             \n"
          "bx lr                    \n");
}

#endif

/* Long division by 10. */
static unsigned long long int div10l(unsigned long long int v) {

    /* It's a kind of magic.  We achieve 64-bit (long) division by dividing the two 32-bit halfs of the number 64-bit
     * number.  The first (most significant) half can produce a rest when dividing, which has to be carried over to the
     * second half.  The rest_add table contains values added to the second half after dividing depending on the rest
     * from the first division.  This allows evaluation of a result which is almost correct -- it can be either the
     * expected result, or the expected result plus one.  The error can be easily detected and corrected.
     */
    
    /* one dream */
    static unsigned long long int rest_add[] = {
        0x00000000, 0x1999999a, 0x33333334, 0x4ccccccd, 0x66666667,
        0x80000001, 0x9999999a, 0xb3333334, 0xcccccccd, 0xe6666667
    };
    
    /* one soul */
    unsigned long long int a = div10((unsigned int)(v >> 32));
    unsigned long long int b = div10((unsigned int)(v & 0xffffffff));
    
    /* one prize */
    int ri = (v >> 32) - a * 10;
    
    /* one goal */
    unsigned long long int ret = (a << 32) + b + rest_add[ri];
    
    /* one golden glance */
    if (ret * 10L > v) {
        //printf("OGG %llu %llu\n", ret * 10, v);
        --ret;
    }
    
    /* of what should be */
    return ret;
}

/* Signed variants. */

static signed long long int divsl(signed long long int dividend, signed long long int divisor) {
    if (dividend < 0) {
        if (divisor < 0)
            return (signed long long int) divl(-dividend, -divisor);
        else
            return -((signed long long int) divl(-dividend, divisor));
    } else {
        if (divisor < 0)
            return -((signed long long int) divl(dividend, -divisor));
        else
            return (signed long long int) divl(dividend, divisor);
    }
}

static signed int div10s(signed int v) {
    if (v < 0)
        return -((signed int) div10(-v));
    else
        return (signed int) div10(v);
}

static signed long long int div10sl(signed long long int v) {
    if (v < 0)
        return -((signed long long int) div10l(-v));
    else
        return (signed long long int) div10l(v);
}


#endif /* DIVISION_H_ */
