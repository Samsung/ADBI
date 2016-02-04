#ifndef _INJECTABLE_H_
#define _INJECTABLE_H_

#include "process/process.h"
#include "process/segment.h"
#include "injfile.h"

struct injectable_t {
    /* unique id */
    unsigned int id;
    
    /* inj file name */
    const char * filename;
    
    /* injectable file */
    injfile_t * injfile;
    
    /* reference count */
    refcnt_t references;
    
    /* is the injectable built-in? */
    bool builtin;
};

typedef struct injectable_t injectable_t;

injectable_t * injectable_init_builtin(void * buffer, size_t size);
const injectable_t * injectable_load(const char * filename, const char ** msg);

bool injectable_unload(unsigned int iid, const char ** msg);

bool injectable_is_library(const injectable_t * injectable);

const injectable_t * injectable_get(unsigned int iid);
const injectable_t * injectable_get_library(const char * name);
const injectable_t * injectable_get_binding(const char * path);

offset_t injectable_get_symbol(const injectable_t * injectable, const char * name);
offset_t injectable_get_exported_symbol(const injectable_t * injectable, const char * name);

void injections_init(thread_t * thread, segment_t * segment);
void injections_gone(thread_t * thread, segment_t * segment);

extern injectable_t * adbi_injectable;
extern injectable_t * adbi_mmap_injectable;
extern injectable_t * adbi_munmap_injectable;

bool injectable_init();
void injectable_cleanup();

typedef void (injectable_callback_t)(const injectable_t * injectable);
void injectable_iter(injectable_callback_t callback);

typedef void (injectable_symbol_callback_t)(const injectable_t * injectable, const injfile_symbol_t * symbol);
void injectable_iter_dependencies(const injectable_t * injectable, injectable_callback_t callback,
		injectable_symbol_callback_t unresolved_import_callback);

typedef void (injectable_iter_fn_t)(const injectable_t * injectable, injfile_symbol_callback_t callback);

injectable_iter_fn_t injectable_iter_exports;
injectable_iter_fn_t injectable_iter_imports;
injectable_iter_fn_t injectable_iter_adbi;

void injectable_iter_tracepoints(const injectable_t * injectable, injfile_tpoint_callback_t callback);

#define INJECTABLE_ITER_IMPORTS(injectable, symbol)     INJFILE_ITER_IMPORTS(injectable->injfile, symbol)
#define INJECTABLE_ITER_EXPORTS(injectable, symbol)     INJFILE_ITER_EXPORTS(injectable->injfile, symbol)
#define INJECTABLE_ITER_ADBI(injectable, symbol)        INJFILE_ITER_ADBI(injectable->injfile, symbol)
#define INJECTABLE_ITER_TPOINTS(injectable, tpoint)     INJFILE_ITER_TPOINTS(injectable->injfile, tpoint)

const char * str_injectable(const injectable_t * injectable);

#endif
