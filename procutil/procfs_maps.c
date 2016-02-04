#include <ctype.h>
#include "process/segment.h"

/* Verify that the given line represents a valid /proc/PID/maps line. */
static void procfs_maps_format_check(const char * line) {

    static bool maps_verified = false;
    
    const char * buf = line;
    
    if (likely(maps_verified)) {
        /* We already checked the format previously, there is no need to do it
         * again, because all lines in all /proc/PID/maps files share the same
         * format. */
        return;
    }
    
#define procfs_maps_format_check(cond) {                \
        if (!(cond)) {                                  \
            goto no;                                    \
        } else {                                        \
            ++buf;                                      \
        }                                               \
    }
    
#define procfs_maps_format_check_dec() {                    \
        procfs_maps_format_check(isdigit(*buf));            \
        while (isdigit(*buf))                               \
            ++buf;                                          \
    }
    
#define procfs_maps_format_check_hex(n) {                   \
        int i;                                              \
        for (i = 0; i < (n); ++i)                           \
            procfs_maps_format_check(isxdigit(*buf))        \
        }
    
#define procfs_maps_format_check_hex2(n) {                  \
        int i;                                              \
        for (i = 0; i < (n); ++i)                           \
            procfs_maps_format_check(isxdigit(*buf))        \
        while (isxdigit(*buf))                              \
            ++buf;                                          \
    }

    /* address range */
    procfs_maps_format_check_hex2(0)
    procfs_maps_format_check(*buf == '-')
    procfs_maps_format_check_hex2(0)
    
    /* space */
    procfs_maps_format_check(*buf == ' ')
    
    /* flags */
    procfs_maps_format_check((*buf == '-') || (*buf == 'r'))
    procfs_maps_format_check((*buf == '-') || (*buf == 'w'))
    procfs_maps_format_check((*buf == '-') || (*buf == 'x'))
    procfs_maps_format_check((*buf == 'p') || (*buf == 's'))
    
    /* space */
    procfs_maps_format_check(*buf == ' ')
    
    /* offset in hex */
    procfs_maps_format_check_hex2(8)
    
    /* space */
    procfs_maps_format_check(*buf == ' ')
    
    /* device */
    procfs_maps_format_check_hex2(2)
    procfs_maps_format_check(*buf == ':')
    procfs_maps_format_check_hex2(2)
    
    /* space */
    procfs_maps_format_check(*buf == ' ')
    
    /* decimal number */
    procfs_maps_format_check_dec()
    
#undef procfs_maps_format_check
#undef procfs_maps_format_check_hex
#undef procfs_maps_format_check_dec
    
    maps_verified = true;
    debug("Note: /proc/PID/maps format verified.");
    return;
    
no:
    fatal("Error: /proc/PID/maps format incorrect.");
    exit(EXIT_FAILURE);
}

/* Parse the given /proc/PID/maps line and write result into segment */
void procfs_maps_parse_segment(char * buf, segment_t * segment) {

    /* TODO: Don't call procfs_maps_format_check every time.  Check the format at startup, by looking at
     * /proc/self/maps
     */
    procfs_maps_format_check(buf);
    
    segment->state = SEGMENT_STATE_NEW;
    
    segment->start  = (address_t) strtoll(buf,  &buf, 16);
    segment->end    = (address_t) strtoll(buf + 1,  &buf, 16);
    
    segment->flags = 0;
    segment->flags |= buf[1] == 'r' ? SEGMENT_READABLE : 0;
    segment->flags |= buf[2] == 'w' ? SEGMENT_WRITEABLE : 0;
    segment->flags |= buf[3] == 'x' ? SEGMENT_EXECUTABLE : 0;
    segment->flags |= buf[4] == 's' ? SEGMENT_SHARED : 0;

    /* skip flags */
    buf += 6;

    segment->offset = (address_t) strtoll(buf, &buf, 16);
    
    /* Skip space after offset, before device numbers */
    ++buf;

    /* Skip device numbers */
    while (*buf != ' ')
    	++buf;
    
    /* Skip space after device numbers, before fd */
    ++buf;

    /* Skip fd. */
    while (isdigit(*buf))
        ++buf;
        
    /* Skip space and tabs (but not new line) to reach filename or end of
     * string. We're sure the last char is null or new line. */
    while (*buf == ' ')
        ++buf;
        
    if (*buf == '\n') {
        segment->filename = NULL;
    } else if (*buf == '\0') {
        /* Line did not fit into buffer. */
        free(segment);
        segment = NULL;
    } else {
        char * t;
        
        /* This is a mapped file. */
        segment->filename = buf;
        
        /* The string is terminated by a new line, but we don't want it in the file name. */
        for (t = segment->filename; *t != '\n'; ++t);
        *t = '\0';
    }
    
}

