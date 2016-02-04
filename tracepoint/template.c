#include <string.h>

#include "template.h"

template_instance_t * template_instatiate(const template_t * t) {
    template_instance_t * result = adbi_malloc(sizeof(template_instance_t));
    
    result->size = t->bindata.size;
    result->data = adbi_malloc(t->bindata.size);
    
    memcpy(result->data, t->bindata.data, t->bindata.size);
    
    return result;
}

void template_instance_free(template_instance_t * i) {
    free(i->data);
    free(i);
}


static void template_insert_bits(template_instance_t * instance,
                                 offset_t offset,
                                 const void * data,
                                 const void * mask,
                                 size_t count) {
                                 
    const unsigned char * d = data;
    const unsigned char * m = mask;
    unsigned char * p = instance->data + offset;
    
    assert(offset >= 0);
    assert((offset_t) instance->size > offset);
    assert((offset_t) instance->size >= offset + (offset_t) count);
    
    while (count) {
        *p = (*p & ~*m) | (*d & *m);
        --count;
        ++p;
        ++m;
        ++d;
    }
    
}

static void template_insert_bytes(template_instance_t * i,
                                  offset_t offset, const void * data, size_t count) {
    assert((offset_t) i->size > offset);
    assert((offset_t) i->size >= offset + (offset_t) count);
    memcpy(i->data + offset, data, count);
}

void template_insert_u16(template_instance_t * instance, offset_t offset, uint16_t value) {
    template_insert_bytes(instance, offset, &value, sizeof(uint16_t));
}

void template_insert_u16_bits(template_instance_t * instance, offset_t offset, uint16_t value,
                              uint16_t mask) {
    template_insert_bits(instance, offset, &value, &mask, sizeof(uint16_t));
}

void template_insert_u32(template_instance_t * instance, offset_t offset, uint32_t value) {
    template_insert_bytes(instance, offset, &value, sizeof(uint32_t));
}

void template_insert_u32_bits(template_instance_t * instance, offset_t offset, uint32_t value,
                              uint32_t mask) {
    template_insert_bits(instance, offset, &value, &mask, sizeof(uint32_t));
}

void template_insert_u64(template_instance_t * instance, offset_t offset, uint64_t value) {
    template_insert_bytes(instance, offset, &value, sizeof(uint64_t));
}

void template_insert_u64_bits(template_instance_t * instance, offset_t offset, uint64_t value,
                              uint64_t mask) {
    template_insert_bits(instance, offset, &value, &mask, sizeof(uint64_t));
}
