#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "injfile.h"

#include "util/human.h"

struct injfile_t * injfile_init(void * ptr, size_t bytes) {
    /* This function will return ptr casted to (injfile_t *), but first it will perform a few checks and fix the
     * pointers to strings and lists. */
    struct injfile_t * inj = (struct injfile_t *)(ptr);
    
    /* Check if the file has a complete header. */
    if (bytes < sizeof(struct injfile_t))
        return NULL;
        
    /* Check magic. */
    if (strncmp(inj->magic, "adbi3inj", 8) != 0)
        return NULL;

    /* Check version. */
    if (inj->version != 0x0210)
        return NULL;

    /* Check code size */
    if (inj->code_offset + inj->code_size > bytes)
        return NULL;

    /* TODO: Perform remaining checks. */

    /* Create pointers from offsets. */
#define fix_ptr(x) inj->x = (inj->x ## _offset) ? ((void *) (((char *) inj) + (inj->x ## _offset))) : NULL;
    fix_ptr(code);
    fix_ptr(name);
    fix_ptr(comment);
    fix_ptr(adbi);
    fix_ptr(imports);
    fix_ptr(exports);
    fix_ptr(tpoints);
    fix_ptr(strings);
    fix_ptr(lines);
#undef fix_ptr
    
    return inj;
}

struct injfile_t * injfile_load(const char * path) {
    struct injfile_t * ret = NULL;
    void * buf = NULL;
    FILE * f = NULL;
    long size;
    
    if (!(f = fopen(path, "rb"))) {
        error("Error opening %s for reading: %s.", path, strerror(errno));
        goto out;
    }
    
    if (fseek(f, 0, SEEK_END) != 0) {
        error("Error seeking end of %s: %s.", path, strerror(errno));
        goto out;
    }
    
    if ((size = ftell(f)) < 0) {
        error("Error reading size of %s: %s.", path, strerror(errno));
        goto out;
    }
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        error("Error seeking start of %s: %s.", path, strerror(errno));
        goto out;
    }
    
    if (!(buf = malloc(size))) {
        error("Error allocating memory for %s (%li bytes).", path, size);
        goto out;
    }
    
    size_t got = fread(buf, 1, size, f);
    
    if ((long) got != size) {
        if (feof(f))
            error("Error reading %s (%li bytes): unexpected end of file (got %zi bytes).", path, size, got);
        else if (ferror(f))
            error("Error reading %s (%li bytes): %s.", path, size, strerror(ferror(f)));
            
        goto out;
    }
    
    ret = injfile_init(buf, size);
    
out:
    if (f)
        fclose(f);
    if (!ret)
        free(buf);
    return ret;
}

void injfile_unload(struct injfile_t * injfile) {
    free(injfile);
}

void injfile_iter_tpoints(const struct injfile_t * injfile, injfile_tpoint_callback_t callback) {
    const struct injfile_tracepoint_t * tpoint;
    if (injfile->tpoints)
        for (tpoint = injfile->tpoints; tpoint->address; ++tpoint) {
            callback(tpoint->address, tpoint->handler_fn);
        }
}

static void injfile_iter_symbols(const struct injfile_symbol_t * symbol, injfile_symbol_callback_t callback) {
    if (symbol)
        for (; symbol->name[0]; ++symbol) {
            callback(symbol->name, symbol->offset);
        }
}

void injfile_iter_imports(const struct injfile_t * injfile, injfile_symbol_callback_t callback) {
    injfile_iter_symbols(injfile->imports, callback);
}

void injfile_iter_exports(const struct injfile_t * injfile, injfile_symbol_callback_t callback) {
    injfile_iter_symbols(injfile->exports, callback);
}

void injfile_iter_adbi(const struct injfile_t * injfile, injfile_symbol_callback_t callback) {
    injfile_iter_symbols(injfile->adbi, callback);
}

static offset_t injfile_get_symbol(const struct injfile_symbol_t * symbol, const char * name) {
    if (symbol)
        for (; symbol->name[0]; ++symbol) {
            if (strncmp(name, symbol->name, 28) == 0)
                return symbol->offset;
        }
    return -1;
}

offset_t injfile_get_import(const struct injfile_t * injfile, const char * name) {
    return injfile_get_symbol(injfile->imports, name);
}

offset_t injfile_get_export(const struct injfile_t * injfile, const char * name) {
    return injfile_get_symbol(injfile->exports, name);
}

offset_t injfile_get_adbi(const struct injfile_t * injfile, const char * name) {
    assert(injfile);
    return injfile_get_symbol(injfile->adbi, name);
}

/* Check if the inj file represents a library injectable. */
bool injfile_is_library(const struct injfile_t * injfile) {
    return (injfile->flags & INJFILE_FLAG_LIBRARY);
}

static const char * injfile_get_string(const struct injfile_t * injfile, uint32_t offset) {
    if (!injfile->strings) {
        /* no strings */
        return "<error>";
    }
    return injfile->strings + offset;
}

bool injfile_addr2line(const struct injfile_t * injfile, offset_t offset, const char ** filename,
                       uint32_t * line) {
    if (!injfile->lines) {
        /* no line information at all */
        return false;
    }
    
    struct injfile_lineinfo_t * li;
    
    for (li = injfile->lines; (li->addr | li->file | li->line); ++li) {
        if (li->addr > offset) {
            /* previous entry is the one we are looking for */
            break;
        }
    }
    
    if (!(li->addr | li->file | li->line) || (li->addr > offset)) {
        /* reached first element with address greater than given offset or last element */
        if (li == injfile->lines) {
            /* li has no previous element */
            return false;
        }
        
        --li;
        if (line)
            *line = li->line;
        if (filename)
            *filename = injfile_get_string(injfile, li->file);
            
        return true;
    }
    
    return false;
}
