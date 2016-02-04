#ifndef _STRINGBUF_H
#define _STRINGBUF_H

#define STRINGBUF_SIZE 0x1000

/* Write formatted text into a temporary string buffer.  The returned buffer must not be freed -- it is a thread local
 * variable.  The maximum length of a single output string is STRINGBUF_SIZE.  If a string does not fit, it is trimmed
 * and its last 3 chars are dots ("...").
 *
 * This function is meant to be used for creation of short strings for temporary use.  The strings do not need to be
 * freed, so it can be used to achieve clean code (in comparison to the usual sequence), i.e.:
 *
 *  the usual approach:
 *      char * tmp[20];
 *      sprintf(tmp, "foo = %i", foo);
 *      ...use tmp...
 *
 *  can be replaced by:
 *      char * tmp = stringbuf_printf("foo = %i", foo);
 *      ...use tmp...
 *
 * Pointers returned by this function point into an internal buffer.  Each call to stringbuf_printf allocates some space
 * in the buffer, writes the output text there and returns the pointer to the beginning of the text.  Allocation is
 * trivial -- the function remembers where the last returned string ended and starts writing the next one just after
 * that location.  If there is not enough space remaining in the buffer, it starts writing at the beginning of the
 * buffer (so in fact this is a special case of a circular buffer).  This means that every string returned by this
 * function will eventually be overwritten.  However, it is possible to generate many short strings and use them
 * simultaneously (as long as their total length is less than STRINGBUF_SIZE).
 */
__attribute__((format(printf, 1, 2)))
const char * stringbuf_printf(char * fmt, ...);

#endif