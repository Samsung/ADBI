#ifndef TEMPLATE_H_
#define TEMPLATE_H_

#include <stdio.h>
#include <stdint.h>
#include "util/bindata.h"

#include "template_field.h"

typedef struct template_field_t {
    template_field_id_t field;
    offset_t offset;
} template_field_t;

typedef struct template_t {
    bindata_t bindata;
    template_field_t * fields;
    const char * name;
    offset_t literal_pool;
} template_t;

extern template_t handler_template_arm;
extern template_t handler_template_thumb;

typedef bindata_t template_instance_t;

void template_insert_data(template_instance_t * instance, offset_t offset, const void * data, size_t count);

void template_insert_u16(template_instance_t * instance, offset_t offset, uint16_t value);
void template_insert_u32(template_instance_t * instance, offset_t offset, uint32_t value);
void template_insert_u64(template_instance_t * instance, offset_t offset, uint64_t value);

void template_insert_u16_bits(template_instance_t * instance, offset_t offset, uint16_t value, uint16_t mask);
void template_insert_u32_bits(template_instance_t * instance, offset_t offset, uint32_t value, uint32_t mask);
void template_insert_u64_bits(template_instance_t * instance, offset_t offset, uint64_t value, uint64_t mask);

template_instance_t * template_instatiate(const template_t * t);
void template_instance_free(template_instance_t * i);

const template_t * template_select(insn_t insn, insn_kind_t kind);

template_instance_t * template_get_handler(const template_t * template, address_t trampoline_address, address_t insn_address,
        address_t handler_address, insn_t insn, insn_kind_t insn_kind);

bool template_need_return_jump(const template_t * template);
insn_kind_t template_get_template_kind(const template_t * template);

typedef void (*template_return_callback_t)(address_t from, address_t to);

void template_iter_return_address(address_t pc, insn_t insn, insn_kind_t kind, const template_t * template,
        address_t trampoline_addr, template_return_callback_t callback);

#endif
