#include <stdio.h>
#include <stdarg.h>
#include "human.h"

#include "process/process.h"
#include "process/thread.h"
#include "process/segment.h"

#include "injectable/injectable.h"
#include "injection/injection.h"

#include "tracepoint/tracepoint.h"

#include "stringbuf.h"

const char * str_node(const node_t * node) {
    return stringbuf_printf("(k=%lx, v=%lx) h=%d, l=%lx, p=%lx, r=%lx", node->key, (unsigned long) node->val, node->height, (unsigned long) node->links.left, (unsigned long) node->links.parent, (unsigned long) node->links.right);
}

const char * str_process(const process_t * process) {
    return stringbuf_printf("%i", process->pid);
}

const char * str_thread(const thread_t * thread) {
    return stringbuf_printf("%i:%i", thread->process->pid, thread->pid);
}

const char * str_injectable(const injectable_t * injectable) {
    return stringbuf_printf("%u:%s", injectable->id, injectable->filename);
}

const char * str_injection(const injection_t * injection) {
    return stringbuf_printf("[%u:%s]", injection->injectable->id, injection->injectable->filename);
}

const char * str_signal(int signo) {
    return stringbuf_printf("%i (%s)", signo, strsignal(signo));
}

const char * str_segment(const segment_t * segment) {
	return stringbuf_printf("%08lx-%08lx %c%c%c %08lx \"%s\"", segment->start, segment->end,
			segment_is_readable(segment) ? 'r' : '-',
			segment_is_writeable(segment) ? 'w' : '-',
			segment_is_executable(segment) ? 'x' : '-',
			segment->offset, segment->filename);
}

const char * str_tracepoint(const tracepoint_t * tracepoint) {
	return stringbuf_printf("at %p created from template %s (handler at %lx, trampoline at %lx)",
			(void *) tracepoint->address, tracepoint->template->name, tracepoint->handler,
			tracepoint->trampoline);
}

static const char * str_injection_line(const injectable_t * injectable, offset_t offset) {
    const char * file;
    uint32_t line;
    if (injfile_addr2line(injectable->injfile, offset, &file, &line)) {
        return stringbuf_printf(" ~ %s:%u", file, line);
    }
    return "";
}

const char * str_injection_offset(const injectable_t * injectable, offset_t offset) {

    /* Best known result. */
    struct {
        offset_t offset;
        enum { NONE, ADBI, EXPORT, IMPORT, TPOINT } type;
        union {
            const char * name;  /* for IMPORT, EXPORT, ADBI */
            address_t addr;     /* for TPOINT */
        };
    } best;
    best.offset = -1;
    best.type = NONE;
    best.name = '\0';
    
    /* Unfortunately, the structures in injfiles are not optimized for searching, so we'll need to iterate through
     * all symbols.  We are searching for a symbol, that is at an address lower or equal to the one we've got. */
    INJECTABLE_ITER_ADBI(injectable, symbol) {
        if ((symbol->offset <= offset) && (symbol->offset > best.offset)) {
            best.offset = symbol->offset;
            best.type = ADBI;
            best.name = symbol->name;
        }
    }
    
    INJECTABLE_ITER_EXPORTS(injectable, symbol) {
        if ((symbol->offset <= offset) && (symbol->offset > best.offset)) {
            best.offset = symbol->offset;
            best.type = EXPORT;
            best.name = symbol->name;
        }
    }
    
    INJECTABLE_ITER_IMPORTS(injectable, symbol) {
        if ((symbol->offset <= offset) && (symbol->offset > best.offset)) {
            best.offset = symbol->offset;
            best.type = IMPORT;
            best.name = symbol->name;
        }
    }
    
    INJECTABLE_ITER_TPOINTS(injectable, tpoint) {
        if ((tpoint->handler_fn <= offset) && (tpoint->handler_fn > best.offset)) {
            best.offset = tpoint->handler_fn;
            best.type = TPOINT;
            best.addr = tpoint->address;
        }
    }
    
    /* No matter what kind of result was found, we can construct a string containing the offset from the base
     * address of the symbol or handler. */
    char offset_str[16] = "";
    if (offset - best.offset) {
        snprintf(offset_str, 16, " +%lx", offset - best.offset);
    } else {
        /* We got an exact match, so there's no offset. Leave the string empty. */
    }
    
    /* Prepare a string containing the description of the symbol type */
    const char * symbol_str = NULL;
    switch (best.type) {
        case ADBI:
            symbol_str = "ADBI symbol";
            break;
        case EXPORT:
            symbol_str = "export";
            break;
        case IMPORT:
            symbol_str = "import";
            break;
        default:
            /* Result isn't a symbol. */
            symbol_str = NULL;
    }
    
    /* Finally construct the result string. */
    switch (best.type) {
        case TPOINT:
            /* Tracepoint handler. */
            return stringbuf_printf("%s:handler(%s:%5lx)%s%s", str_injectable(injectable),
                                    injectable->injfile->name, best.addr, offset_str,
                                    str_injection_line(injectable, offset));
        case ADBI:
        case EXPORT:
        case IMPORT:
            /* Symbol of some kind, symbol_str should be initialized properly. */
            return stringbuf_printf("%s:%s '%s'%s%s)", str_injectable(injectable),
                                    symbol_str, best.name, offset_str,
                                    str_injection_line(injectable, offset));
        default:
            /* No matching symbol found, print just the offset relative to injectable start. */
            return stringbuf_printf("%s:%05lx%s", str_injectable(injectable),
                                    offset, str_injection_line(injectable, offset));
    }
    
}

const char * str_address(const struct process_t * process, address_t address) {
    const segment_t * segment = segment_get(process, address);
    
    if (segment) {
        /* The address is inside a regular segment. */
        if (segment->filename) {
            /* The segment is mapped to a file. */
            const tracepoint_t * tracepoint = tracepoint_get_by_runtime_address(process, address);
            return stringbuf_printf("%p (mapped to %s:%05lx%s)", (void *) address,
                                    segment->filename, segment_addr2fo(segment, address),
                                    tracepoint ? "; the instruction is traced" : "");
                                    
        } else {
            /* The segment is anonymous. */
            return stringbuf_printf("%p (inside segment starting at %p)", (void *) address, (void *) segment->start);
        }
    }
    
    segment = segment_get_by_trampoline(process, address);
    if (segment) {
        /* Address is in a trampoline segment. */
        const tracepoint_t * tracepoint = tracepoint_get_by_trampoline_address(segment, address);
        if (!tracepoint)
            return stringbuf_printf("%p (trampoline with no tracepoint, segment probably under cloning)",
                                    (void *) address);

        return stringbuf_printf("%p (trampoline for tracepoint at %s:%05lx; %s:%05lx)", (void *) address,
                                segment->filename, segment_addr2fo(segment, tracepoint->address),
                                tracepoint->template->name, (address - tracepoint->trampoline));
    }
    
    const injection_t * injection = injection_get_by_address(process, address);
    if (injection) {
        /* The address is inside an injection.  Try to find a symbol or tracepoint handler, which contains the
         * address. */
        offset_t offset = address - injection->address;
        
        const char * d = str_injection_offset(injection->injectable, offset);
        return stringbuf_printf("%p (%s)", (void *) address, d);
    }
    
    /* The address is invalid. */
    return stringbuf_printf("%p (%s)", (void *) address, address ? "invalid" : "NULL");
}


