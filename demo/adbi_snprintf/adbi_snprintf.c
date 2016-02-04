#include <adbicpy.h>
#include <io.h>
#include <division.h>
#include <varargs.h>
#include <inj.h>

LOCAL size_t strlen(const char * text) {
    const char * end = text;
    while (*end)
        ++end;
    return end - text;
}

LOCAL void adbi_write_unlocked(char **str, ssize_t *size, const char * text, ssize_t count) {
	if (*size > 0) {
		size_t copy = *size < count ? *size : count;
		__adbicpy(*str, text, copy);
		*str += copy;
	}

	*size -= count;
}

LOCAL void adbi_write_dec_unlocked(char **str, ssize_t *size, unsigned long long val) {
    static char buf[25];    /*  2 ** 64 in decimal fits into 20 bits  */
    char * p = buf + 25;
    if (val) {
        while (val) {
            unsigned long long next_val = div10l(val);
            unsigned long long rest = val - next_val * 10;
            *(--p) = ('0' + rest);
            val = next_val;
        }
        adbi_write_unlocked(str, size, p, (buf + 25) - p);
    } else {
        /* val is zero, this is a special case */
        adbi_write_unlocked(str, size, "0", 1);
    }
}

LOCAL void adbi_write_signed_dec_unlocked(char **str, ssize_t *size, signed long long val) {
    static char buf[25];    /*  2 ** 64 in decimal fits into 20 bits  */
    char * p = buf + 25;
    
    int minus = (val < 0);
    
    if (minus) {
        val = -val;
    }
    
    if (val) {
        while (val) {
            unsigned long long next_val = div10l(val);
            unsigned long long rest = val - next_val * 10;
            *(--p) = ('0' + rest);
            val = next_val;
        }
        
        if (minus) {
            /* prepend the minus sign */
            *--p = '-';
        }
        
        adbi_write_unlocked(str, size, p, (buf + 25) - p);
    } else {
        /* val is zero, this is a special case */
        adbi_write_unlocked(str, size, "0", 1);
    }
}

LOCAL void adbi_write_oct_unlocked(char **str, ssize_t *size, unsigned long long val) {
    static char buf[25];    /*  2 ** 64 in decimal fits into 20 bits  */
    char * p = buf + 25;
    
    if (val) {
        /* insert decimal digits */
        for (; val; val /= 8) {
            int digit = (int)(val % 8);
            *--p = ('0' + digit);
        }
        adbi_write_unlocked(str, size, p, (buf + 25) - p);
    } else {
        /* special case: val is zero */
        adbi_write_unlocked(str, size, "0", 1);
    }
}

LOCAL void adbi_write_hex_unlocked(char **str, ssize_t *size, unsigned long long val) {
    static const char hexdig[] = "0123456789abcdef";
    static char buf[25];    /*  2 ** 64 in hex fits into 20 bits  */
    char * p = buf + 25;
    
    if (val) {
        for (; val; val /= 16) {
            int digit = (int)(val % 16);
            *--p = (hexdig[digit]);
        }
        adbi_write_unlocked(str, size, p, (buf + 25) - p);
    } else {
        /* special case: val is zero */
        adbi_write_unlocked(str, size, "0", 1);
    }
}

LOCAL void adbi_write_char_unlocked(char **str, ssize_t *size, const unsigned char val) {
    if ((val >= 32) && (val < 127)) {
        /* regular char */
        adbi_write_unlocked(str, size, (char *) &val, 1);
    } else {
        /* non-printable char */
        static char buf[2];
        static char hexdig[] = "0123456789abcdef";
        buf[0] = hexdig[(val >> 4)];
        buf[1] = hexdig[val & 0xf];
        adbi_write_unlocked(str, size, buf, 2);
    }
}

GLOBAL int adbi_snprintf(char *str, size_t size, const char *fmt, ...) {
	size_t limit = size - 1;
	ssize_t ssize = (ssize_t) size;
    va_list ap;
    va_start(ap, fmt);
    
    while (1) {
        const char * start = fmt;
        
        /* continue until the next percent char */
        while ((*fmt) && (*fmt != '%'))
            ++fmt;
            
        if (fmt > start) {
            /* copy bytes from start to fmt directly to the output */
            adbi_write_unlocked(&str, &ssize, start, fmt - start);
        }
        
        if (!(*fmt)) {
            /* that's it - we've reached the end of the string */
            break;
        }

        ++fmt;
        
        /* do the conversion */
        {
            int length = 3;
            
            while (*fmt) {
                switch (*fmt++) {
                
                    case 'l':
                        ++length;
                        break;
                        
                    case 'h':
                        --length;
                        break;
                        
                    case 's': {
                            /* string */
                            char * text = va_arg(ap, char *);
                            adbi_write_unlocked(&str, &ssize, text, strlen(text));
                            goto done;
                        }
                        break;
                        
                    case 'p': {
                            /* pointer */
                            void * ptr = va_arg(ap, void *);
                            adbi_write_hex_unlocked(&str, &ssize, (unsigned long long) ptr);
                            goto done;
                        }
                        break;
                        
                    case 'x':
                    case 'X':
                    case 'o':
                    case 'u': {
                            /* unsigned integer */
                            
                            /* obtain the value */
                            unsigned long long value;
                            if (length >= 5) {
                                /* long long (quad) - 8B */
                                value = va_arg(ap, unsigned long long);
                            } else {
                                /* (no modifier) or long - 4B */
                                value = (unsigned long long) va_arg(ap, unsigned int);
                            }
                            
                            /* do the printing */
                            switch (*fmt) {
                                case 'o':
                                    adbi_write_oct_unlocked(&str, &ssize, value);
                                    break;
                                case 'u':
                                    adbi_write_dec_unlocked(&str, &ssize, value);
                                    break;
                                default:
                                    adbi_write_hex_unlocked(&str, &ssize, value);
                                    break;
                            }
                            
                            goto done;
                        }
                        break;
                        
                    case 'i':
                    case 'd': {
                            /* signed integer */
                            
                            /* obtain the value */
                            signed long long value;
                            if (length >= 5) {
                                /* long long (quad) - 8B */
                                value = va_arg(ap, signed long long);
                            } else {
                                /* (no modifier) or long - 4B */
                                value = (signed long long) va_arg(ap, signed int);
                            }
                            
                            adbi_write_signed_dec_unlocked(&str, &ssize, value);
                            goto done;
                        }
                        break;
                        
                    case 'c': {
                            /* char */
                            adbi_write_char_unlocked(&str, &ssize, va_arg(ap, int));
                            goto done;
                        }
                        
                    case 'e':
                    case 'E':
                    case 'f':
                    case 'F':
                    case 'g':
                    case 'G':
                    case 'a':
                    case 'A': {
                            /* float or double */
                            unsigned long long int zzz = va_arg(ap, unsigned long long int);
                            (void) zzz;
                            adbi_write_unlocked(&str, &ssize, "<float>", 7);
                            goto done;
                        }
                }
            }
        done: ;
        }
    }
    
    va_end(ap);

	*str = '\0';

	return limit - ssize;
}

INIT() {}

EXPORT(adbi_snprintf);
