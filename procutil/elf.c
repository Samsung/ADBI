#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <elf.h>
#include <link.h>

#include "process/segment.h"
#include "process/process.h"
#include "process/thread.h"
#include "procutil/mem.h"
#include "injection/fncall.h"

#include "procfs.h"

static bool elf_get_ident(FILE * file, unsigned char * e_ident) {
    if (fseek(file, 0, SEEK_SET))
        return false;
    if (fread(e_ident, EI_NIDENT, 1, file) != 1)
        return false;
    return (strncmp((char *) e_ident, ELFMAG, SELFMAG) == 0);
}

static unsigned char elf_get_class(FILE * file) {
    unsigned char e_ident[EI_NIDENT];
    if (!elf_get_ident(file, e_ident))
        return '\0';
    return e_ident[EI_CLASS];
}

bool elf_is_elf64(FILE * file) {
    return (elf_get_class(file) == ELFCLASS64);
}

static bool elf32_get_elf_header(FILE * file, Elf32_Ehdr * elf_header) {
    if (fseek(file, 0, SEEK_SET))
        return false;
    if (fread(elf_header, sizeof(Elf32_Ehdr), 1, file) != 1)
        return false;
    return (strncmp((char *) &elf_header->e_ident, ELFMAG, SELFMAG) == 0);
}

static bool elf64_get_elf_header(FILE * file, Elf64_Ehdr * elf_header) {
    if (fseek(file, 0, SEEK_SET))
        return false;
    if (fread(elf_header, sizeof(Elf64_Ehdr), 1, file) != 1)
        return false;
    return (strncmp((char *) &elf_header->e_ident, ELFMAG, SELFMAG) == 0);
}

static bool elf32_get_section_header(FILE * file, const Elf32_Ehdr * elf_header, unsigned int index,
                                   Elf32_Shdr * header) {
    unsigned int offset = elf_header->e_shoff + elf_header->e_shentsize * index;
    if (fseek(file, offset, SEEK_SET))
        return false;
    return (fread(header, sizeof(Elf32_Shdr), 1, file) == 1);
}

static bool elf64_get_section_header(FILE * file, const Elf64_Ehdr * elf_header, unsigned int index,
                                   Elf64_Shdr * header) {
    unsigned int offset = elf_header->e_shoff + elf_header->e_shentsize * index;
    if (fseek(file, offset, SEEK_SET))
        return false;
    return (fread(header, sizeof(Elf64_Shdr), 1, file) == 1);
}

static char * elf32_get_string(FILE * file, const Elf32_Ehdr * elf_header, unsigned int offset) {
    Elf32_Shdr sec_head;
    if (!elf32_get_section_header(file, elf_header, elf_header->e_shstrndx, &sec_head))
        return NULL;
        
    char buf[256];
    
    offset += sec_head.sh_offset;
    if (fseek(file, offset, SEEK_SET))
        return NULL;
        
    size_t got = fread(buf, 1, 255, file);
    
    return strndup(buf, got);
}

static char * elf64_get_string(FILE * file, const Elf64_Ehdr * elf_header, unsigned int offset) {
    Elf64_Shdr sec_head;
    if (!elf64_get_section_header(file, elf_header, elf_header->e_shstrndx, &sec_head))
        return NULL;

    char buf[256];

    offset += sec_head.sh_offset;
    if (fseek(file, offset, SEEK_SET))
        return NULL;

    size_t got = fread(buf, 1, 255, file);

    return strndup(buf, got);
}

static bool elf32_get_section_header_by_name(FILE * file, const Elf32_Ehdr * elf_header, const char * name,
        Elf32_Shdr * header) {
    bool ret = false;
    for (int i = 0; (i < elf_header->e_shnum) && (!ret); ++i) {
        if (!elf32_get_section_header(file, elf_header, i, header))
            return false;
        char * section_name = elf32_get_string(file, elf_header, header->sh_name);
        ret = (strcmp(name, section_name) == 0);
        free(section_name);
    }
    return ret;
}

static bool elf64_get_section_header_by_name(FILE * file, const Elf64_Ehdr * elf_header, const char * name,
        Elf64_Shdr * header) {
    bool ret = false;
    for (int i = 0; (i < elf_header->e_shnum) && (!ret); ++i) {
        if (!elf64_get_section_header(file, elf_header, i, header))
            return false;
        char * section_name = elf64_get_string(file, elf_header, header->sh_name);
        ret = (strcmp(name, section_name) == 0);
        free(section_name);
    }
    return ret;
}

static bool elf32_get_dynamic_section(FILE * file, const Elf32_Ehdr * elf_header, Elf32_Shdr * header) {
    if (!elf32_get_section_header_by_name(file, elf_header, ".dynamic", header))
        return false;
    return header->sh_type == SHT_DYNAMIC;
}

static bool elf64_get_dynamic_section(FILE * file, const Elf64_Ehdr * elf_header, Elf64_Shdr * header) {
    if (!elf64_get_section_header_by_name(file, elf_header, ".dynamic", header))
        return false;
    return header->sh_type == SHT_DYNAMIC;
}

/* TODO: this looks so ugly, it reminds me of the Windows API... */
static bool elf32_get_dynamic_entry(FILE * file, const Elf32_Shdr * dynamic, Elf32_Sword tag,     /* in */
                                  unsigned int * idxptr,                                        /* in & out */
                                  Elf32_Word * value, unsigned int * offsetptr                  /* out */
                                 ) {
                                 
    unsigned int idx = idxptr ? *idxptr : 0;
    
    Elf32_Dyn dyn_ent;
    
    do {
        unsigned int offset = dynamic->sh_offset + idx * sizeof(Elf32_Dyn);
        
        if (fseek(file, offset, SEEK_SET))
            return false;
            
        if (fread(&dyn_ent, sizeof(Elf32_Dyn), 1, file) != 1)
            return false;
            
        if (dyn_ent.d_tag == tag) {
            /* we got it! */
            if (idxptr)
                *idxptr = idx;
            if (value)
                *value = dyn_ent.d_un.d_val;
            if (offsetptr)
                *offsetptr = offset + sizeof(Elf32_Sword);
            return true;
        }
        
        ++idx;
    } while (dyn_ent.d_tag != 0);
    
    /* not found */
    return false;
}

static bool elf64_get_dynamic_entry(FILE * file, const Elf64_Shdr * dynamic, Elf64_Sword tag,     /* in */
                                  unsigned int * idxptr,                                        /* in & out */
                                  Elf64_Word * value, unsigned int * offsetptr                  /* out */
                                 ) {

    unsigned int idx = idxptr ? *idxptr : 0;

    Elf64_Dyn dyn_ent;

    do {
        unsigned int offset = dynamic->sh_offset + idx * sizeof(Elf64_Dyn);

        if (fseek(file, offset, SEEK_SET))
            return false;

        if (fread(&dyn_ent, sizeof(Elf64_Dyn), 1, file) != 1)
            return false;

        if (dyn_ent.d_tag == tag) {
            /* we got it! */
            if (idxptr)
                *idxptr = idx;
            if (value)
                *value = dyn_ent.d_un.d_val;
            if (offsetptr)
                *offsetptr = offset + sizeof(Elf64_Sword);
            return true;
        }

        ++idx;
    } while (dyn_ent.d_tag != 0);

    /* not found */
    return false;
}

static address_t elf32_addr2fo(FILE * file, const Elf32_Ehdr * elf_header, Elf32_Addr addr) {
    for (unsigned int i = 0; i < elf_header->e_shnum; ++i) {
        Elf32_Shdr header;
        if (!elf32_get_section_header(file, elf_header, i, &header))
            return 0;
        if (!((addr >= header.sh_addr) && (addr < header.sh_addr + header.sh_size)))
            continue;
        return header.sh_offset + (addr - header.sh_addr);
    }
    return 0;
}

static address_t elf64_addr2fo(FILE * file, const Elf64_Ehdr * elf_header, Elf64_Addr addr) {
    for (unsigned int i = 0; i < elf_header->e_shnum; ++i) {
        Elf64_Shdr header;
        if (!elf64_get_section_header(file, elf_header, i, &header))
            return 0;
        if (!((addr >= header.sh_addr) && (addr < header.sh_addr + header.sh_size)))
            continue;
        return header.sh_offset + (addr - header.sh_addr);
    }
    return 0;
}

unsigned int elf_get_r_data_offset(const char * filename) {
    FILE * file = fopen(filename, "r");
    if (!file)
        return 0;
    unsigned int ret = 0;
    Elf32_Ehdr elf_header;
    Elf32_Shdr dynamic;
    if (!(elf32_get_elf_header(file, &elf_header)
            && elf32_get_dynamic_section(file, &elf_header, &dynamic)
            && elf32_get_dynamic_entry(file, &dynamic, DT_DEBUG, NULL, NULL, &ret))) {
        ret = 0;
    }
    fclose(file);
    return ret;
}

address_t elf64_get_r_debug_address(FILE * file, address_t * entry, const char * filename) {
    address_t ret;

    unsigned int idx = 0;
    Elf64_Ehdr elf_header;
    Elf64_Shdr dynamic;

    if (!elf64_get_elf_header(file, &elf_header)) {
        error("Error reading ELF header from %s.", filename);
        return (address_t) NULL;
    }
    if (!elf64_get_dynamic_section(file, &elf_header, &dynamic)) {
        error("Could not find .dynamic section in %s.", filename);
        return (address_t) NULL;
    }
    if (!elf64_get_dynamic_entry(file, &dynamic, DT_DEBUG, &idx, NULL, NULL)) {
        error("Could not find DT_DEBUG entry in .dynamic section of %s.",
                filename);
        return (address_t) NULL;
    }
    /* We've found a .dynamic section in the ELF.  Loadable sections of ELF files have load addresses assigned.  The
     * dynamic section is always loaded, so it will have a load address.  We were parsing the elf structures of the
     * executable (not a library), so this address shouldn't be relocated (libraries can be loaded at any address,
     * executables are loaded at a predefined address, usually 0x8000).  That's really nice, because we can carelessly
     * dereference the magical pointer below. */
    ret = (address_t) (dynamic.sh_addr + idx * sizeof(Elf64_Dyn)
            + offsetof(Elf64_Dyn, d_un.d_val)); /* offset of the value (d_val) inside the dynamic section entry */
    ret = (address_t) elf64_addr2fo(file, &elf_header, (Elf64_Addr) ret);


    if (entry)
        *entry = elf64_addr2fo(file, &elf_header, elf_header.e_entry);

    return ret;
}

address_t elf32_get_r_debug_address(FILE * file, address_t * entry, const char * filename) {
    address_t ret;

    unsigned int idx = 0;
    Elf32_Ehdr elf_header;
    Elf32_Shdr dynamic;

    if (!elf32_get_elf_header(file, &elf_header)) {
        error("Error reading ELF header from %s.", filename);
        return (address_t) NULL;
    }
    if (!elf32_get_dynamic_section(file, &elf_header, &dynamic)) {
        error("Could not find .dynamic section in %s.", filename);
        return (address_t) NULL;
    }
    if (!elf32_get_dynamic_entry(file, &dynamic, DT_DEBUG, &idx, NULL, NULL)) {
        error("Could not find DT_DEBUG entry in .dynamic section of %s.",
                filename);
        return (address_t) NULL;
    }
    /* We've found a .dynamic section in the ELF.  Loadable sections of ELF files have load addresses assigned.  The
     * dynamic section is always loaded, so it will have a load address.  We were parsing the elf structures of the
     * executable (not a library), so this address shouldn't be relocated (libraries can be loaded at any address,
     * executables are loaded at a predefined address, usually 0x8000).  That's really nice, because we can carelessly
     * dereference the magical pointer below. */
    ret = (address_t) (dynamic.sh_addr + idx * sizeof(Elf32_Dyn)
            + sizeof(Elf32_Sword)); /* offset of the value (d_val) inside the dynamic section entry */
    ret = (address_t) elf32_addr2fo(file, &elf_header, (Elf32_Addr) ret);


    if (entry)
        *entry = elf32_addr2fo(file, &elf_header, elf_header.e_entry);

    return ret;
}

/*struct r_debug {
    int32_t r_version;
    struct link_map * r_map;
    void (*r_brk)(void);
    int32_t r_state;
    uintptr_t r_ldbase;
};*/

#define str_elfclass(class) ((class) == ELFCLASS32   ? "32" :           \
                             (class) == ELFCLASS64   ? "64" :           \
                             (class) == ELFCLASSNONE ? "None" : "???")

static address_t get_r_debug_address(pid_t pid, address_t * entry, const char ** exefile) {
    address_t ret = NULL;
    /* find out our own executable */
    const char * filename = procfs_get_exe(pid, 0);
    
    if (!filename)
        return 0;
        
    
    FILE * file = fopen(filename, "r");
    if (!file) {
        error("Error opening main executable of %u: %s: %s.", pid, filename, strerror(errno));
        goto out;
    }
    
    unsigned char elfclass = elf_get_class(file);
    info("Main executable (class %s) of process %u is %s.", str_elfclass(elfclass), pid, filename);

    if (elfclass == ELFCLASS64)
        ret = elf64_get_r_debug_address(file, entry, filename);
    else
        ret = elf32_get_r_debug_address(file, entry, filename);

    if (entry)
        debug("Entry address of PID %u is %s:%lx.", pid, filename, (*entry));
    if (ret)
        debug("PID %u has DT_DEBUG value located at address %s:%lx.", pid, filename, ret);

out:
    if (file)
        fclose(file);
    if (exefile)
        *exefile = filename;
    return ret;
}

/* TODO: This belongs to segment.c */
void segment_iter_mapped(process_t * process, const char * executable, address_t file_offset,
                         void (*callback)(segment_t * segment, address_t address)) {
                         
    void inner(segment_t * segment) {
    
        if (!segment->filename)
            return;
            
        if (strcmp(segment->filename, executable))
            return;
            
        address_t lo = segment->offset;
        address_t hi = lo + segment->end - segment->start;
        
        if (!((lo <= file_offset) && (file_offset < hi)))
            return;
            
        /* The segment has the offset mapped. */
        address_t rtaddr = segment->start + file_offset - segment->offset;
        callback(segment, rtaddr);
        
    }
    segment_iter(process, inner);
}

static address_t elf_get_rdebug_ptr(thread_t * thread, const char * executable, address_t file_offset) {

    address_t ret = 0;
    const size_t addr_size = thread->process->mode32 ? 4 : 8;
    
    void callback(segment_t * segment, address_t address) {
    
        (void) segment;
        
        if (ret)
            return;
            
        debug("Trying to access DT_DEBUG in process %s at %p...", str_process(thread->process), (void *) address);
        
        if (mem_read(thread, address, addr_size, &ret) != addr_size) {
            ret = 0;
            return;
        }
        
    }
    
    segment_iter_mapped(thread->process, executable, file_offset, callback);
    
    if (ret)
        debug("Found DT_DEBUG in process %s, value is %p.", str_process(thread->process), (void *) ret);
        
    return ret;
}

/* Get the linker breakpoint address of a different process.  Return NULL if the address is not found. */
void * elf_get_remote_linker_breakpoint_address(thread_t * thread) {
    address_t entry;
    const char * executable;
    address_t r_debug_ptrptr = get_r_debug_address(thread->process->pid, &entry, &executable);
    address_t r_debug_ptr;
    struct r_debug _r_debug;
    
    if (!r_debug_ptrptr) {
        /* Error or statically linked binary. */
        return NULL;
    }
    
    r_debug_ptr = elf_get_rdebug_ptr(thread, executable, r_debug_ptrptr);
    
    if (r_debug_ptr) {
        goto gotit;
    }
    
    /* We got the DT_DEBUG address, but it's not initialized yet.  We need to wait for the linker to finish
     * initialization of the main executable. */
    if (!segment_translate_file_to_mem(thread->process, executable, entry, &entry)) {
        return NULL;
    }
    
    if (!segment_check_address_is_code(thread->process, entry)) {
        error("The entry address %p of process %s is invalid.", (void *) entry, str_process(thread->process));
        return NULL;
    }
    
    error("Process %s has not finished linker initialization.  "
          "Letting it run until it reaches its entry point in the executable at %s.",
          str_process(thread->process),
          str_address(thread->process, entry));
          
    if (!fncall_runtil(thread, entry)) {
        error("Error running %s until executable entry point.", str_process(thread->process));
        return NULL;
    }
    
    /* We might have missed some memory changes... */
    segment_rescan(thread);
    
    r_debug_ptr = elf_get_rdebug_ptr(thread, executable, r_debug_ptrptr);
    
    if (!r_debug_ptr) {
        error("Error reading DT_DEBUG still zero after reaching entry address in process %s.",
              str_process(thread->process));
        return NULL;
    }
    
gotit:
    if (mem_read(thread, r_debug_ptr, sizeof(_r_debug), &_r_debug) != sizeof(_r_debug)) {
        error("Error reading r_debug struct of %s.", str_process(thread->process));
        return NULL;
    }
    debug("Process %s has linker breakpoint at %s.",
          str_process(thread->process), str_address(thread->process, (address_t) _r_debug.r_brk));
    return (void *) _r_debug.r_brk;
}
