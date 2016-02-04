#include <stdlib.h>
#include <string.h>

#include "payload.h"

enum payload_type {
    PAYLOAD_TYPE_TERMINATOR = 0x00,
    PAYLOAD_TYPE_U32 = 0x10,
    PAYLOAD_TYPE_I32 = 0x11,
    PAYLOAD_TYPE_U64 = 0x20,
    PAYLOAD_TYPE_I64 = 0x21,
    PAYLOAD_TYPE_STR = 0x80,
};

typedef packed_struct payload_element_info_t {
    uint8_t  type;      /* payload_type encoded as unsigned byte                    */
    uint8_t  reserved;  /* must be zero, reserved for future use                    */
    uint16_t name_size; /* length of the name string, including the null terminator */
    uint32_t data_size; /* data size */
} payload_element_info_t;

/***********************************************************************************************************************
 * Construction, destruction, reset
 **********************************************************************************************************************/

payload_buffer_t * payload_create() {
    payload_buffer_t * result = adbi_malloc(sizeof(payload_buffer_t));
    result->allocated = 64;
    result->size = 0;
    result->buf = adbi_malloc(result->allocated);
    return result;
}

void payload_reset(payload_buffer_t * pb) {
    pb->size = 0;
}

void payload_free(payload_buffer_t * pb) {
    free(pb->buf);
    free(pb);
}


/***********************************************************************************************************************
 * Payload construction
 **********************************************************************************************************************/

static void payload_append_bytes(payload_buffer_t * pb,
                                 const void * data,
                                 size_t count) {
                                 
    size_t allocate = pb->allocated;
    
    while (count + pb->size > allocate) {
        allocate *= 2;
    }
    
    if (allocate != pb->allocated) {
        /* Reallocate */
        pb->buf = adbi_realloc(pb->buf, allocate);
        pb->allocated = allocate;
    }
    
    memcpy(pb->buf + pb->size, data, count);
    pb->size += count;
    
}

static void payload_add_element(payload_buffer_t * pb,
                                enum payload_type type,
                                const char * name,
                                const void * data,
                                size_t count) {
                                
    payload_element_info_t info;
    
    info.type = (uint8_t) type;
    info.reserved = 0;
    info.name_size = strlen(name) + 1; /* We store the terminator too. */
    info.data_size = count;
    
    payload_append_bytes(pb, &info, sizeof(payload_element_info_t));
    payload_append_bytes(pb, name, info.name_size);
    payload_append_bytes(pb, data, info.data_size);
}

#define payload_put(type, type_enum, postfix)                                           \
    void payload_put_ ## postfix (payload_buffer_t * pb, const char * name, type val) { \
        payload_add_element(pb, (type_enum), name, &val, sizeof(type));                 \
    }

payload_put(uint64_t, PAYLOAD_TYPE_U64, u64)
payload_put(int64_t, PAYLOAD_TYPE_I64, i64)
payload_put(uint32_t, PAYLOAD_TYPE_U32, u32)
payload_put(int32_t, PAYLOAD_TYPE_I32, i32)

void payload_put_str(payload_buffer_t * pb, const char * name, const char * val) {
    payload_add_element(pb,
                        PAYLOAD_TYPE_STR,
                        name,
                        val ? val : "",
                        val ? strlen(val) + 1 : 1);
}

void payload_put_term(payload_buffer_t * pb) {
    payload_add_element(pb, PAYLOAD_TYPE_TERMINATOR, "", NULL, 0);
}

/***********************************************************************************************************************
 * Helper functions
 **********************************************************************************************************************/

static enum payload_type payload_element_get_type(const payload_element_info_t * e) {
    return (enum payload_type) e->type;
}

static size_t payload_element_get_size(const payload_element_info_t * e) {
    return e->name_size + e->data_size + sizeof(payload_element_info_t);
}

static const char * payload_element_get_name(const payload_element_info_t * e) {
    const char * where = (const char *) e;
    where += sizeof(payload_element_info_t);
    return where;
}

static const void * payload_element_get_data(const payload_element_info_t * e) {
    const char * where = (const char *) e;
    where += sizeof(payload_element_info_t);
    where += e->name_size;
    return (const void *)(where);
}

static const payload_element_info_t * payload_element_get_next(const payload_element_info_t * e) {
    const char * where = (const char *) e;
    where += payload_element_get_size(e);
    return (const payload_element_info_t *) where;
}


/***********************************************************************************************************************
 * Checking payload correctness
 **********************************************************************************************************************/

/* Check if the size bytes at element represent a correct payload element. */
static bool payload_check_element(const payload_element_info_t * element, size_t size) {

    const char * name;
    
    if (size < sizeof(payload_element_info_t)) {
        /* Header doesn't fit. */
        return false;
    }
    
    if (size < payload_element_get_size(element)) {
        /* Payload doesn't fit. */
        return false;
    }
    
    if (element->name_size == 0) {
        /* Name size must be at least 1. */
        return false;
    }
    
    name = payload_element_get_name(element);
    
    if (name[element->name_size - 1]) {
        /* Name is not null terminated. */
        return false;
    }
    
    if (strlen(name) + 1 != element->name_size) {
        /* Null in the middle of the name string. */
        return false;
    }
    
    switch (element->type) {
        case PAYLOAD_TYPE_TERMINATOR:
            return element->data_size == 0;
        case PAYLOAD_TYPE_I32:
            return element->data_size == sizeof(int32_t);
        case PAYLOAD_TYPE_U32:
            return element->data_size == sizeof(uint32_t);
        case PAYLOAD_TYPE_I64:
            return element->data_size == sizeof(int64_t);
        case PAYLOAD_TYPE_U64:
            return element->data_size == sizeof(uint64_t);
        case PAYLOAD_TYPE_STR:
            return ((const char *) payload_element_get_data(element))[element->data_size - 1] == 0;
        default:
            return false;
    }
    
}

/* Check if the size bytes at buf represent a correct payload buffer. */
bool payload_check(const char * buf, size_t size) {
    const payload_element_info_t * element = (const payload_element_info_t *) buf;
    
    while (1) {
        if (!payload_check_element(element, size))
            return false;
            
        assert(size >= payload_element_get_size(element));
        
        size -= payload_element_get_size(element);
        
        if (element->type == PAYLOAD_TYPE_TERMINATOR) {
            return (size == 0);
        }
        
        element = payload_element_get_next(element);
    }
    
    adbi_bug_unrechable();
    return false;
}


/**********************************************************************************************************************
 * Reading fields
 **********************************************************************************************************************/

static const payload_element_info_t * payload_find(const void * buf, const char * name) {

    const payload_element_info_t * element = (payload_element_info_t *) buf;
    
    while (element->type != PAYLOAD_TYPE_TERMINATOR) {
    
        if (strcmp(name, payload_element_get_name(element)) == 0) {
            return element;
        } else {
            element = payload_element_get_next(element);
        }
    }
    
    return NULL;
}

static const payload_element_info_t * payload_find_typed(const void * buf, const char * name,
        enum payload_type type) {
        
    const payload_element_info_t * element;
    
    if (!(element = payload_find(buf, name)))
        return NULL;
        
    if (payload_element_get_type(element) != type)
        return NULL;
        
    return element;
}

/**********************************************************************************************************************/

#define payload_get(type, type_enum, postfix)                                       \
    const type * payload_get_ ## postfix(const void * buf, const char * name) {     \
        const payload_element_info_t * element;                                     \
        if (!(element = payload_find_typed(buf, name, (type_enum))))                \
            return NULL;                                                            \
        else                                                                        \
            return (const type *) payload_element_get_data(element);                \
    }

payload_get(uint64_t, PAYLOAD_TYPE_U64, u64)
payload_get(int64_t, PAYLOAD_TYPE_I64, i64)
payload_get(uint32_t, PAYLOAD_TYPE_U32, u32)
payload_get(int32_t, PAYLOAD_TYPE_I32, i32)
payload_get(char, PAYLOAD_TYPE_STR, str)
