#ifndef _INJFILE_H_
#define _INJFILE_H_

#include <stdint.h>

#define INJFILE_FLAG_LIBRARY 0x1

typedef int32_t  inj_offset_t;
typedef uint32_t inj_address_t;

struct __attribute__((packed)) injfile_symbol_t {
    char name[28];
    inj_offset_t offset;
};

struct __attribute__((packed)) injfile_tracepoint_t {
    inj_address_t address;
    inj_offset_t handler_fn;
};

struct __attribute__((packed)) injfile_lineinfo_t {
    inj_offset_t addr;
    uint32_t file;
    uint32_t line;
};

struct __attribute__((packed)) injfile_t {
    char magic[8];
    uint16_t version;
    uint16_t flags;
    uint32_t code_size;
#define ptr_field(type, name) union { uint64_t name ## _offset; type * name; }
    ptr_field(uint8_t, code);
    ptr_field(const char, name);
    ptr_field(const char, comment);
    ptr_field(struct injfile_symbol_t, adbi);
    ptr_field(struct injfile_symbol_t, imports);
    ptr_field(struct injfile_symbol_t, exports);
    ptr_field(struct injfile_tracepoint_t, tpoints);
    ptr_field(const char, strings);
    ptr_field(struct injfile_lineinfo_t, lines);
#undef ptr_field
};

typedef struct injfile_symbol_t injfile_symbol_t;
typedef struct injfile_tracepoint_t injfile_tpoint_t;
typedef struct injfile_t injfile_t;

injfile_t * injfile_init(void * ptr, size_t bytes);
injfile_t * injfile_load(const char * filename);
void injfile_unload(injfile_t * injfile);

typedef void (injfile_tpoint_callback_t)(address_t tpoint_addr, offset_t handler_offset);
typedef void (injfile_symbol_callback_t)(const char * name, offset_t offset);

void injfile_iter_tpoints(const injfile_t * injfile, injfile_tpoint_callback_t callback);
void injfile_iter_imports(const injfile_t * injfile, injfile_symbol_callback_t callback);
void injfile_iter_exports(const injfile_t * injfile, injfile_symbol_callback_t callback);
void injfile_iter_adbi(const injfile_t * injfile, injfile_symbol_callback_t callback);

offset_t injfile_get_import(const injfile_t * injfile, const char * name);
offset_t injfile_get_export(const injfile_t * injfile, const char * name);
offset_t injfile_get_adbi(const injfile_t * injfile, const char * name);

static inline const injfile_symbol_t * injfile_symbol_iter_first(const injfile_symbol_t * list) {
    return list;
}

static inline const injfile_symbol_t * injfile_symbol_iter_next(const injfile_symbol_t * current) {
    ++current;
    return current->name[0] ? current : NULL;
}

#define INJFILE_ITER_IMPORTS(injfile, symbol)                                           \
    for (const injfile_symbol_t * symbol = injfile_symbol_iter_first(injfile->imports); \
            symbol;                                                                     \
            symbol = injfile_symbol_iter_next(symbol))

#define INJFILE_ITER_EXPORTS(injfile, symbol)                                           \
    for (const injfile_symbol_t * symbol = injfile_symbol_iter_first(injfile->exports); \
            symbol;                                                                     \
            symbol = injfile_symbol_iter_next(symbol))

#define INJFILE_ITER_ADBI(injfile, symbol)                                              \
    for (const injfile_symbol_t * symbol = injfile_symbol_iter_first(injfile->adbi);    \
            symbol;                                                                     \
            symbol = injfile_symbol_iter_next(symbol))


static inline const injfile_tpoint_t * injfile_tpoint_iter_first(const injfile_tpoint_t * list) {
    return list;
}

static inline const injfile_tpoint_t * injfile_tpoint_iter_next(const injfile_tpoint_t * current) {
    ++current;
    return current->address ? current : NULL;
}

#define INJFILE_ITER_TPOINTS(injfile, tpoint)                                                   \
    for (const injfile_tpoint_t * tpoint = injfile_tpoint_iter_first(injfile->tpoints);         \
            tpoint;                                                                             \
            tpoint = injfile_tpoint_iter_next(tpoint))

bool injfile_is_library(const struct injfile_t * injfile);

typedef address_t (injfile_import_resolver_fn)(const char * symbol);

bool injfile_addr2line(const struct injfile_t * injfile, offset_t offset, const char ** filename,
                       uint32_t * line);

#endif /* _INJFILE_H_ */
