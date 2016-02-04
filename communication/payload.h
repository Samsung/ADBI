#ifndef PAYLOAD_H_
#define PAYLOAD_H_

#include <stdlib.h>

/* Payload API
 *
 * The payload API allows construction and decoding of simple data structures.
 * It was designed to be simple to process, but still flexible and extendible.
 *
 * Payloads created by this API consist of entries. A payload can have an
 * arbitrary number of entries (including zero). Each entry has a name, a type
 * and a value.
 *
 * Names of entries are simple, null-terminated strings. Their maximum length
 * is about 64K (2 ** 16 - 2). Names must be unique in a payload. However, this
 * API does not check for duplicates (duplicates cause undefined behavior).
 *
 * Currently, the following types of values can be stored in a payload:
 *      * unsigned 32-bit integers (u32);
 *      * signed 32-bit integers (i32);
 *      * strings (length limited to about 4G) (str);
 *
 * Example of payload construction
 *      // Create the buffer
 *      payload_buffer_t * b = payload_create();
 *
 *      // Insert unsigned 32-bit int named foo with value 42
 *      payload_put_u32(b, "foo", 42);
 *
 *      // Insert string named bar with value "spam and eggs"
 *      payload_put_str(b, "bar", "spam and eggs");
 *
 *      // Terminate the buffer
 *      payload_put_term(b);
 *
 *      // The payload is now complete and can be saved or sent out.
 *      write(fd, b->buf, b->size);
 *
 *      // Free the buffer
 *      payload_free(b);
 *
 * Example of payload decoding
 *      // The size must be obtained from some other source
 *      size_t size = 123;
 *      void * buf = malloc(size);
 *
 *      // read data from network or file
 *      read(fd, buf, size);    // TODO: add error checking, etc...
 *
 *      // check if data is correct
 *      if (!payload_check(buf, size)) {
 *          // payload is malformed -- complain, crash or panic
 *      } else {
 *          // payload is ok -- we can read data safely
 *          const uint32_t * foo = payload_get_u32(buf, "foo");
 *          const char * bar = payload_get_u32(buf, "bar");
 *
 *          // foo and bar may be NULL
 *          if (!foo || !bar) {
 *              // error: foo or bar is missing in the payload
 *          }
 *
 *          // Use the read values.
 *          printf("foo: %u, bar: %s", (unsigned int) *foo, bar);
 *      }
 *
 *      free(buf);
 *
 */

typedef struct payload_buffer_t {
    char * buf;         /* Pointer to the memory buffer */
    size_t allocated;   /* Amount of allocated memory */
    size_t size;        /* Amount of memory used */
} payload_buffer_t;

/* Create a new resizable buffer for use with payload_put_* functions. */
payload_buffer_t * payload_create();

/* Free up a buffer after using payload_put_* functions. */
void payload_free(payload_buffer_t * pb);

/* Reset a buffer. After using this function, the buffer structure can be
 * reused to construct another payload packet. No memory is freed or allocated
 * during this call. */
void payload_reset(payload_buffer_t * pb);

/* The payload_put_x functions all insert a single data value into the buffer.
 * The internal memory buffer in the pb structure is extended if necessary.
 *
 * The functions don't check for duplicated entries (i.e. entries with the same
 * name).
 *
 * The payload_put_term inserts a special end marker into the payload buffer.
 * No other entries can be added after this marker to the buffer.
 */
void payload_put_u64(payload_buffer_t * pb, const char * name, uint64_t val);
void payload_put_i64(payload_buffer_t * pb, const char * name, int64_t val);
void payload_put_u32(payload_buffer_t * pb, const char * name, uint32_t val);
void payload_put_i32(payload_buffer_t * pb, const char * name, int32_t val);
void payload_put_str(payload_buffer_t * pb, const char * name, const char * val);
void payload_put_term(payload_buffer_t * pb);

/* Check if the size bytes at buf represents a correct payload buffer. */
bool payload_check(const char * buf, size_t size);

/* The payload_get_x functions read a single value of data from the buffer.
 *
 * The functions return a pointer which points to the memory address inside the
 * buffer. This means, that freeing buf will cause all pointers returned by
 * these functions to become invalid.
 *
 * If the buffer has no entry, which matches the given name, or the matching
 * entry has a different type, NULL is returned.
 *
 * The functions require the buffer to represent a correct payload. Passing
 * an invalid payload to the functions can (and probably will) cause crashes.
 * For this reason, it is suggested to check the buffer using the payload_check
 * function before calling any of these functions (especially if the buffer
 * may contain invalid data, e.g. data received from the network or read from
 * the disk).
 */
const uint64_t * payload_get_u64(const void * buf, const char * name);
const int64_t * payload_get_i64(const void * buf, const char * name);
const uint32_t * payload_get_u32(const void * buf, const char * name);
const int32_t * payload_get_i32(const void * buf, const char * name);
const char * payload_get_str(const void * buf, const char * name);

#endif
