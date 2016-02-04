#include "injectable.h"

#include "process/segment.h"
#include "process/process.h"
#include "process/thread.h"
#include "process/list.h"

#include "tree.h"
#include "util/human.h"

static tree_t injectables;
static tree_t libraries;
static tree_t bindings;

/* Next unique injectable id to assign. */
static unsigned int next_iid = 1;

static injectable_t * injectable_create(injfile_t * injfile, const char * filename) {
    injectable_t * injectable = adbi_malloc(sizeof(injectable_t));
    
    if (!filename) {
        /* built-in, loaded from memory */
        injectable->filename = adbi_malloc(strlen(injfile->name) + 16);
        sprintf((char *) injectable->filename, "<built-in %s>", injfile->name);
        injectable->builtin = true;
    } else {
        /* loaded from disk */
        injectable->filename = strdup(filename);
        injectable->builtin = false;
    }
    
    injectable->references = 0;
    injectable->id = next_iid++;
    injectable->injfile = injfile;
    
    tree_insert(&injectables, injectable->id, injectable);
    if (injectable_is_library(injectable)) {
        tree_insert(&libraries, injectable->id, injectable);
    } else {
        tree_insert(&bindings, injectable->id, injectable);
    }
    
    info("Loaded injectable %s.", str_injectable(injectable));
    return injectable;
}

static void injectable_free(injectable_t * injectable) {
    assert(injectable->references == 0);
    
    info("Freed injectable %s.", str_injectable(injectable));
    
    tree_remove(&injectables, injectable->id);
    if (injectable_is_library(injectable)) {
        tree_remove(&libraries, injectable->id);
    } else {
        tree_remove(&bindings, injectable->id);
    }
    
    if (injectable->builtin) {
        /* built-in injectable, do not unload */
    } else {
        injfile_unload(injectable->injfile);
    }
}

injectable_t * injectable_init_builtin(void * buffer, size_t size) {
    injfile_t * injfile = injfile_init(buffer, size);
    if (!injfile) {
        critical("Error initializing built-in injectable.");
        return NULL;
    }
    return injectable_create(injfile, NULL);
}

const injectable_t * injectable_get_binding(const char * path) {
    TREE_ITER(&bindings, node) {
        injectable_t * inj = node->val;
        if (strcmp(inj->injfile->name, path) == 0)
            return inj;
    }
    return NULL;
}

const injectable_t * injectable_get_library(const char * name) {
    TREE_ITER(&libraries, node) {
        injectable_t * inj = node->val;
        if (strcmp(inj->injfile->name, name) == 0)
            return inj;
    }
    return NULL;
}

const injectable_t * injectable_get(unsigned int iid) {
    return tree_get(&injectables, iid);
}

static const injectable_t * injectable_get_by_file(const char * filename) {
    TREE_ITER(&injectables, node) {
        const injectable_t * injectable = node->val;
        if ((!injectable->builtin) && (strcmp(injectable->filename, filename) == 0))
            return injectable;
    }
    return NULL;
}

const injectable_t * injectable_load(const char * filename, const char ** msg) {

    const injectable_t * injectable = injectable_get_by_file(filename);
    
    if (injectable) {
        error("Refusing to load injectable from %s -- it's already loaded (%s).",
              filename, str_injectable(injectable));
        *msg = "already loaded";
        return NULL;
    }
    
    injfile_t * injfile = injfile_load(filename);
    if (!injfile) {
        error("Error loading injectable from %s.", filename);
        *msg = "i/o error or file malformed";
        return NULL;
    }
    
    if (!injfile_is_library(injfile)) {
        injectable = injectable_get_binding(injfile->name);
        if (injectable) {
            error("Refusing to load injectable from %s -- it's bound to '%s', but injectable %s already binds to it.",
                  filename, injfile->name, str_injectable(injectable));
            *msg = "invalid binding";
            injfile_unload(injfile);
            return NULL;
        }
    }
    
    /* Create the injectable. */
    injectable = injectable_create(injfile, filename);
    
    /* Load the injectable into processes if necessary. */
    void callback(process_t * process) {
        process_attach_injectable(process, injectable);
    }
    process_iter(callback);
    
    return injectable;
}



bool injectable_unload(unsigned int iid, const char ** msg) {
    injectable_t * injectable = tree_get(&injectables, iid);
    if (!injectable) {
        if (msg) *msg = "no such injectable";
        return false;
    }
    if (injectable->builtin) {
        if (msg) *msg = "built-in injectables can't be unloaded";
        return false;
    }
    if (injectable_is_library(injectable) && injectable->references) {
        if (msg) *msg = "injectable in use by other injectable(s), can't unload";
        return false;
    }
    
    /* Remove the injectable from all processes. */
    void uninject_process(process_t * process) {
        process_detach_injectable(process, injectable);
    }
    process_iter(uninject_process);
    
    /* Free the injectable. */
    injectable_free(injectable);
    return true;
}

offset_t injectable_get_symbol(const injectable_t * injectable, const char * name) {
    assert(injectable);
    return injfile_get_adbi(injectable->injfile, name);
}

offset_t injectable_get_exported_symbol(const injectable_t * injectable, const char * name) {
    return injfile_get_export(injectable->injfile, name);
}

injectable_t * adbi_injectable;
injectable_t * adbi_mmap_injectable;
injectable_t * adbi_munmap_injectable;

bool injectable_init() {
    injectable_t * adbi_injectable_get(void);
    injectable_t * adbi_mmap_injectable_get(void);
    injectable_t * adbi_munmap_injectable_get(void);
    
    adbi_injectable = adbi_injectable_get();
    adbi_mmap_injectable = adbi_mmap_injectable_get();
    adbi_munmap_injectable = adbi_munmap_injectable_get();
    
    return adbi_injectable && adbi_mmap_injectable && adbi_munmap_injectable;
}

void injectable_cleanup() {
    injectable_t * injectable;
    while ((injectable = tree_get_any_val(&injectables))) {
        injectable_free(injectable);
    }
    assert(tree_empty(&injectables));
    assert(tree_empty(&libraries));
    assert(tree_empty(&bindings));
}

bool injectable_is_library(const injectable_t * injectable) {
    return injfile_is_library(injectable->injfile);
}

void injectable_iter(injectable_callback_t callback) {
    TREE_ITER(&injectables, node) {
        callback(node->val);
    }
}

/* XXX: symbol resolution should be changed and consider to move it to injection.c.
 * Reimplement using symbol hash table, add support for tracepoint injectable imports/exports,
 * add checking if tracepoint injectable can be loaded into process. Consider support for
 * weak symbols and priority (symbols from tracepoint injectables will have precedence over
 * symbols from library injectable).
 */
static tree_t injectable_get_dependencies(const injectable_t * injectable, injectable_symbol_callback_t unresolved_import_callback) {
    tree_t res = NULL;
    INJECTABLE_ITER_IMPORTS(injectable, import) {
        bool found_export = false;

        TREE_ITER(&libraries, node) {
            injectable_t * inj = (injectable_t *) node->val;
            if (0 < injfile_get_export(inj->injfile, import->name)) {
                found_export = true;
                tree_insert(&res, inj->id, inj);
                break;
            }
        }
        if (!found_export && unresolved_import_callback)
            unresolved_import_callback(injectable, import);
    }

    return res;
}

void injectable_iter_dependencies(const injectable_t * injectable, injectable_callback_t callback,
        injectable_symbol_callback_t unresolved_import_callback) {
    tree_t tree = injectable_get_dependencies(injectable, unresolved_import_callback);
    TREE_ITER(&tree, node) {
        injectable_t * inj = node->val;
        injectable_iter_dependencies(inj, callback, unresolved_import_callback);
        if (callback)
            callback(inj);
    }
    tree_clear(&tree);
}

void injectable_iter_exports(const injectable_t * injectable, injfile_symbol_callback_t callback) {
    injfile_iter_exports(injectable->injfile, callback);
}

void injectable_iter_imports(const injectable_t * injectable, injfile_symbol_callback_t callback) {
    injfile_iter_imports(injectable->injfile, callback);
}

void injectable_iter_adbi(const injectable_t * injectable, injfile_symbol_callback_t callback) {
    injfile_iter_adbi(injectable->injfile, callback);
}

void injectable_iter_tracepoints(const injectable_t * injectable, injfile_tpoint_callback_t callback) {
    injfile_iter_tpoints(injectable->injfile, callback);
}

