#include "division.h"
#include "net.h"

static int adbi_write_fd;
static mutex_t adbi_write_mutex;

LOCAL int adbi_write_init() {
    adbi_write_mutex = 0;
    struct sockaddr_in address;
    
    adbi_write_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int ret = get_errno(&adbi_write_fd);
    if (ret)
        goto out;
        
    address.sin_family = AF_INET;
    address.sin_port = htons(2222);
    address.sin_addr.s_addr = inet_ip(127, 0, 0, 1);
    
    ret = connect(adbi_write_fd, (const struct sockaddr *) &address, sizeof(struct sockaddr));
    ret = get_errno(&ret);
    if (ret)
        goto out;
        
out:
    if (ret) {
        adbi_write_fd = -1;
    }
    return ret;
}

LOCAL int adbi_write_exit() {
    if (adbi_write_fd == -1)
        return 0;

    return close(adbi_write_fd);
}

LOCAL size_t strlen(const char * text) {
    const char * end = text;
    while (*end)
        ++end;
    return end - text;
}

LOCAL void adbi_write_unlocked(const char * text, size_t count) {

    /* Simply write into the fd without any checking.  If we failed to open, the injectable should be unloaded, so
     * we should not reach this code anyway.  If, for some reason, this gets executed even if no file was opened,
     * nothing bad should happen, the system call will simply fail. */
    
    while (count) {
        ssize_t written = write(adbi_write_fd, text, count);
        if (written < 0) {
            int error = (get_errno(&written));
            if (error == EINTR) {
                /* We got interrupted, handler was called, we can continue now. */
                continue;
            } else {
                /* We got an error.  There's no way to report the error, so just abort. */
                return;
            }
        }
        count -= written;
        text += written;
    }
    
}

LOCAL void adbi_write_dec_unlocked(unsigned long long val) {
    static char buf[25];    /*  2 ** 64 in decimal fits into 20 bits  */
    char * p = buf + 25;
    if (val) {
        while (val) {
            unsigned long long next_val = div10l(val);
            unsigned long long rest = val - next_val * 10;
            *(--p) = ('0' + rest);
            val = next_val;
        }
        adbi_write_unlocked(p, (buf + 25) - p);
    } else {
        /* val is zero, this is a special case */
        adbi_write_unlocked("0", 1);
    }
}

LOCAL void adbi_write_signed_dec_unlocked(signed long long val) {
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
        
        adbi_write_unlocked(p, (buf + 25) - p);
    } else {
        /* val is zero, this is a special case */
        adbi_write_unlocked("0", 1);
    }
}

LOCAL void adbi_write_oct_unlocked(unsigned long long val) {
    static char buf[25];    /*  2 ** 64 in decimal fits into 20 bits  */
    char * p = buf + 25;
    
    if (val) {
        /* insert decimal digits */
        for (; val; val /= 8) {
            int digit = (int)(val % 8);
            *--p = ('0' + digit);
        }
        adbi_write_unlocked(p, (buf + 25) - p);
    } else {
        /* special case: val is zero */
        adbi_write_unlocked("0", 1);
    }
}

LOCAL void adbi_write_hex_unlocked(unsigned long long val) {
    static const char hexdig[] = "0123456789abcdef";
    static char buf[25];    /*  2 ** 64 in hex fits into 20 bits  */
    char * p = buf + 25;
    
    if (val) {
        for (; val; val /= 16) {
            int digit = (int)(val % 16);
            *--p = (hexdig[digit]);
        }
        adbi_write_unlocked(p, (buf + 25) - p);
    } else {
        /* special case: val is zero */
        adbi_write_unlocked("0", 1);
    }
}

LOCAL void adbi_write_char_unlocked(const unsigned char val) {
    if ((val >= 32) && (val < 127)) {
        /* regular char */
        adbi_write_unlocked((char *) &val, 1);
    } else {
        /* non-printable char */
        static char buf[2];
        static char hexdig[] = "0123456789abcdef";
        buf[0] = hexdig[(val >> 4)];
        buf[1] = hexdig[val & 0xf];
        adbi_write_unlocked(buf, 2);
    }
}

GLOBAL void adbi_printf(const char * fmt, ...) {
    va_list ap;
    
    mutex_lock(&adbi_write_mutex);
    
    va_start(ap, fmt);
    
    while (1) {
        const char * start = fmt;
        
        /* continue until the next percent char */
        while ((*fmt) && (*fmt != '%'))
            ++fmt;
            
        if (fmt > start) {
            /* copy bytes from start to fmt directly to the output */
            adbi_write_unlocked(start, fmt - start);
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
                            adbi_write_unlocked(text, strlen(text));
                            goto done;
                        }
                        break;
                        
                    case 'p': {
                            /* pointer */
                            void * ptr = va_arg(ap, void *);
                            adbi_write_hex_unlocked((unsigned long) ptr);
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
                            switch (*(fmt-1)) {
                                case 'o':
                                    adbi_write_oct_unlocked(value);
                                    break;
                                case 'u':
                                    adbi_write_dec_unlocked(value);
                                    break;
                                default:
                                    adbi_write_hex_unlocked(value);
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
                            
                            adbi_write_signed_dec_unlocked(value);
                            goto done;
                        }
                        break;
                        
                    case 'c': {
                            /* char */
                            adbi_write_char_unlocked(va_arg(ap, int));
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
                            adbi_write_unlocked("<float>", 7);
                            goto done;
                        }
                        
                }
                
            }
            
        done: ;
        }
        
    }
    
    va_end(ap);
    
    mutex_unlock(&adbi_write_mutex);
}

GLOBAL void adbi_writen(const char * text, size_t count) {
    /* XXX: Let's hope the thread will not die unexpectedly while holding the lock... */
    mutex_lock(&adbi_write_mutex);
    adbi_write_unlocked(text, count);
    mutex_unlock(&adbi_write_mutex);
}



GLOBAL void adbi_write(const char * text) {
    adbi_writen(text, strlen(text));
}

EXPORT(adbi_writen);
EXPORT(adbi_write);
EXPORT(adbi_printf);
