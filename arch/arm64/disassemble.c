#include "process/process.h"
#include "process/thread.h"
#include "tracepoint/patch.h"
#include "tracepoint/template.h"

#include "tracepoint/tracepoint.h"

#include "capstone.h"
#include "disarm.h"

static __thread csh cshandle = 0;

#define DISARM64_BUFSIZE    (256)
static __thread char disarm64_buf[DISARM64_BUFSIZE];

static bool open_capstone() {
    cs_err res = cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &cshandle);
    if (res != CS_ERR_OK)
        error("Error opening Capstone engine: %s", cs_strerror(res));

    return (res == CS_ERR_OK);
}

static const char * disarm64(insn_t insn, address_t pc) {
    cs_insn *csi;
    size_t count;
    if (!cshandle && !open_capstone())
        return "<capstone open failed>";

    count = cs_disasm(cshandle, (const uint8_t *) &insn, sizeof(insn), pc, 1, &csi);
    if (count == 1) {
        snprintf(disarm64_buf, DISARM64_BUFSIZE-2, "%s\t%s", csi->mnemonic, csi->op_str);
        cs_free(csi, 1);
    } else {
        cs_err err = cs_errno(cshandle);
        if (err != CS_ERR_OK) {
            error("Capstone disassembler failed: %s", cs_strerror(err));
            return "<capstone error>";
        }
        return "<unrecognised>";
    }

    return disarm64_buf;
}

/* Disassemble the given instruction of the given kind and return a string representing the instruction. The result
 * string may additionally include comments and more detailed information (like exact branch destination addresses).
 * Warning: the returned string is a thread-local variable. */
const char * arm64_disassemble_extended(insn_t insn, insn_kind_t kind, address_t pc) {
    switch (kind) {
    case INSN_KIND_T32_16:
        return disthumba(insn, pc);
    case INSN_KIND_T32_32:
        return disthumb2a(insn, pc);
    case INSN_KIND_A32:
        return disarma(insn, pc);
    case INSN_KIND_A64:
        return disarm64(insn, pc);
    default:
        adbi_bug_unrechable();
        return "<undefined>";
    }
}

/* Disassemble the given instruction of the given kind and return a string representing the instruction.
 * Warning: the returned string is a thread-local variable. */
const char * arm64_disassemble(insn_t insn, insn_kind_t kind) {
    char * disassembly = (char *) arm64_disassemble_extended(insn, kind, 0);
    char * d;

    for (d = disassembly; *d; ++d) {
        if (*d == '\t') {
            /* replace tabs by spaces */
            *d = ' ';
        }

        if (*d == ';') {
            /* discard comments */
            *d = '\0';
            break;
        }
    }

    /* trim spaces on the right */
    for (--d; (d > disassembly) && *d == ' '; --d) *d = '\0';

    return disassembly;
}



void arm64_disassemble_handler(thread_t * thread, tracepoint_t * tracepoint, void * code) {
    struct process_t * process = thread->process;
    insn_t insn;
    offset_t offset = 0;
    address_t base = tracepoint->trampoline;

    int is_patched(size_t bytes) {
        for (offset_t x = 0; x < (offset_t) bytes; ++x) {
            const char * a = ((const char *) code) + x;
            const char * b = ((const char *) tracepoint->template->bindata.data) + offset + x;
            if (*a != *b)
                return 1;
        }
        return 0;
    }
    

    address_t get_return_jump(address_t from) {
        return (address_t) tree_get(&process->jumps, (tree_key_t) from);
    }

    debug("Trampoline (%zu bytes) in process %s for handler at %p created from template %s.",
          tracepoint->template->bindata.size, str_process(process), (void *) tracepoint->handler,
          tracepoint->template->name);

    if (process->mode32) {
        while (offset < tracepoint->template->literal_pool) {
            if (tracepoint->insn_kind == INSN_KIND_A32) {
                /* ARM */
                insn = *((uint32_t *) code);
                debug(" %5lx:  %s %08x   %s",
                      base + offset, is_patched(4) ? "*" : " ",
                      insn, arm64_disassemble_extended(insn, INSN_KIND_A32, base + offset));
                offset += 4;
                code += 4;
            } else {
                /* Thumb */
                uint16_t val = *((uint16_t *) code);
                insn = val;

                if (is_t32_32_halfword(insn)) {
                    /* 32 bit */
                    uint32_t val = *((uint32_t *) code);
                    insn = t32_swap_halfwords(val);

                    debug(" %5lx:  %s %04x %04x  %s",
                          base + offset, is_patched(4) ? "*" : " ",
                          t32_first_halfowrd(insn), t32_last_halfowrd(insn),
                          arm64_disassemble_extended(insn, INSN_KIND_T32_32, base + offset));

                    offset += 4;
                    code += 4;
                } else {
                    /* 16 bit */
                    debug(" %5lx:  %s %04x       %s",
                          base + offset, is_patched(4) ? "*" : " ",
                          insn, arm64_disassemble_extended(insn, INSN_KIND_T32_16, base + offset));
                    offset += 2;
                    code += 2;
                }
            }
        }
    } else {
        cs_insn *csi;
        size_t i = 0;
        size_t count = cs_disasm(cshandle, (uint8_t *) code, tracepoint->template->literal_pool, base, 0, &csi);

        int is_relative_jump_return(offset_t off) {
            template_field_t * f;
            for (f = tracepoint->template->fields; f->field; ++f) {
                if (f->offset > off)
                    return 0;
                if (f->offset == off) {
                    if ((f->field == TF_HANDLER_RETURN) ||
                            (f->field == TF_HANDLER_RETURN_TO_IMM26) ||
                            (f->field == TF_HANDLER_RETURN_TO_IMM19) ||
                            (f->field == TF_HANDLER_RETURN_TO_IMM14))
                        return 1;
                    else
                        return 0;
                }
            }

            return 0;
        }

        while (offset < tracepoint->template->literal_pool) {
            insn = *((uint32_t *) code);
            if ((i < count) && (base + offset) == csi[i].address) {
                debug(" %5lx:  %s %08x   %s\t%s", base + offset,
                        is_relative_jump_return(offset) ? "r" : (is_patched(4) ? "*" : " "),
                            insn, csi[i].mnemonic, csi[i].op_str);
                ++i;
            } else {
                address_t ret = get_return_jump(base + offset);
                if (ret)
                    debug(" %5lx:  j %08x   jump to %s", base + offset, insn, str_address(process, ret));
                else
                    debug(" %5lx:  %s %08x", base + offset, is_patched(4) ? "*" : " ", insn);

            }
            offset += 4;
            code += 4;
        }
        cs_free(csi, count);
    }

    if (offset < (offset_t) tracepoint->template->bindata.size)
                debug("   -   -   -   -   -   -   -   -   -   -   -   -   -   -");

    /* If there's anything left in the buffer, dump it as raw data (.byte). */
    if (process->mode32) {
        while (offset < (offset_t) tracepoint->template->bindata.size) {
            uint32_t v = *((uint32_t *) code);

            const char * addr = str_address(process, v);

            if (addr) {
                debug(" %5lx:  %s %08x   %s", base + offset, is_patched(4) ? "*" : " ", v, addr);
            } else {
                debug(" %5lx:  %s %08x   .word %08x", base + offset, is_patched(4) ? "*" : " ", v, v);
            }

            offset += 4;
            code += 4;
        }
    } else {
        while (offset < (offset_t) tracepoint->template->bindata.size) {
            uint64_t v = *((uint64_t *) code);

            debug(" %5lx:  %s %s", base + offset, is_patched(4) ? "*" : " ", str_address(process, v));

            offset += 8;
            code += 8;
        }
    }

    debug("Traced instruction:");
    debug(" %5lx:    %08x   %s", tracepoint->address, tracepoint->insn,
            arm64_disassemble_extended(tracepoint->insn, tracepoint->insn_kind, tracepoint->address));
}
