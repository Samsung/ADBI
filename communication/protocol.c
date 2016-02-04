#include <ctype.h>
#include <stdarg.h>
#include <byteswap.h>
#include <dirent.h>

#include "process/list.h"
#include "process/process.h"
#include "process/segment.h"

#include "procutil/mem.h"

#include "injection/inject.h"

#include "injectable/injectable.h"

#include "configuration/state.h"

#include "tracepoint/jump.h"
#include "tracepoint/template.h"

#include "util/signal.h"

#include "communication.h"
#include "payload.h"
#include "protocol.h"

/******************************************************************************/

/* Only one request packet can be handled at a time, so the functions in this
 * file all share one buffer for response packet construction. */
static payload_buffer_t * payload_buffer;

/******************************************************************************/

/* Converts a packet type string to a 4-byte ID. */
static uint32_t packet_id(const char * type) {
    uint32_t result = 0;
    uint8_t * str = (uint8_t *) type;
    int i;
    
    assert(strlen((const char *) str) == 4);
    
    for (i = 0; i < 4; ++i) {
        result <<= 8;
        result |= (uint32_t) str[i];
    }
    return result;
}

/******************************************************************************/

packet_t * packet_create(const packet_header_t * head) {
    packet_t * packet = adbi_malloc(sizeof(packet_t));
    memcpy(&packet->head, head, sizeof(packet_header_t));
    packet->payload = adbi_malloc(head->length);
    return packet;
}

void packet_free(packet_t * packet) {
    free(packet->payload);
    free(packet);
}

/******************************************************************************/

/* Create a response packet to the given request. The returned packet's payload
 * is equal to the global packet_buffer value. The returned object is a static
 * local, so it does not require freeing. Before sending out the packet, the
 * function appends a terminator field to the payload. */
static const packet_t * respond(const packet_t * request, const char * type) {
    static packet_t response;
    
    payload_put_term(payload_buffer);
    
    response.head.type = packet_id(type);
    response.head.length = payload_buffer->size;
    response.head.seq = request->head.seq;
    response.payload = payload_buffer->buf;
    
    assert(payload_check(payload_buffer->buf, payload_buffer->size));
    return &response;
}

/* Wrapper function for creating a response packet. Response packets can
 * contain a msg field with a human-readable message. This function allows
 * simple construction of a packet with a msg field using a printf-like
 * syntax. */
static const packet_t * say(const packet_t * request, const char * type, const char * msgfmt, ...) {

    char msg[256];
    
    va_list args;
    va_start(args, msgfmt);
    
    /* TODO: Range check / check required buffer size. */
    vsnprintf(msg, 256, msgfmt, args);
    payload_put_str(payload_buffer, "msg", msg);
    va_end(args);
    
    return respond(request, type);
}

/******************************************************************************/

/* Helper macros for use in the handle_XXXX functions. */

/* Response construction */
#define say_OKAY(...)   return say(request, "OKAY", __VA_ARGS__)
#define say_FAIL(...)   return say(request, "FAIL", __VA_ARGS__)
#define say_MALF(...)   return say(request, "MALF", __VA_ARGS__)
#define say_USUP(...)   return say(request, "USUP", __VA_ARGS__)
#define say_unimplemented()     say_USUP("Not implemented.")

/******************************************************************************/

/* Reading fields */
#define read_ptr(type, var, name)                               \
    if (!(var = payload_get_ ## type (request->payload, name))) \
        say_MALF("Field missing: '%s'.", name);

#define read_deref(ctype, type, var, name) {        \
        const ctype * var ## _ptr;                  \
        read_ptr(type, var ## _ptr, name);          \
        var = * var ## _ptr;                        \
    }

#define read_u64x(var, name) read_deref(uint64_t, u64, var, name)
#define read_i64x(var, name) read_deref(uint64_t, u64, var, name)
#define read_u32x(var, name) read_deref(uint32_t, u32, var, name)
#define read_i32x(var, name) read_deref(uint32_t, u32, var, name)
#define read_strx(var, name) read_ptr(str, var, name)

#define read_u64(var) read_deref(uint64_t, u64, var, # var)
#define read_i64(var) read_deref(uint64_t, u64, var, # var)
#define read_u32(var) read_deref(uint32_t, u32, var, # var)
#define read_i32(var) read_deref(uint32_t, u32, var, # var)
#define read_str(var) read_ptr(str, var, # var)

/* Adding fields */
#define write_u64x(name, val) payload_put_u64(payload_buffer, name, val);
#define write_i64x(name, val) payload_put_i64(payload_buffer, name, val);
#define write_u32x(name, val) payload_put_u32(payload_buffer, name, val);
#define write_i32x(name, val) payload_put_i32(payload_buffer, name, val);
#define write_strx(name, val) payload_put_str(payload_buffer, name, val);

#define write_u64(var) payload_put_u64(payload_buffer, # var, var);
#define write_i64(var) payload_put_i64(payload_buffer, # var, var);
#define write_u32(var) payload_put_u32(payload_buffer, # var, var);
#define write_i32(var) payload_put_i32(payload_buffer, # var, var);
#define write_str(var) payload_put_str(payload_buffer, # var, var);

/******************************************************************************/

/* Each of the following functions handles one packet type (i.e. handle_XXXX
 * handles XXXX). The functions return a response packet, usually constructed
 * by one of the helper macros say_YYYY.
 */

/***********************************************************************************************************************
 * Process control
 **********************************************************************************************************************/

static const packet_t * handle_ATTC(const packet_t * request) {
    uint32_t pid;
    read_u32(pid);
    
    if (process_attach(pid)) {
        say_OKAY("Attached %u.", pid);
    } else {
        say_FAIL("Error attaching %u.", pid);
    }
}

static const packet_t * handle_DETC(const packet_t * request) {
    uint32_t pid;
    process_t * process;
    
    read_u32(pid);
    
    if (!(process = process_get(pid)))
        say_FAIL("Process %u not attached.", pid);

    process_detach(process);
    process_put(process);
    
    say_OKAY("Detached from process %d.", pid);
}

static const packet_t * handle_SPWN(const packet_t * request) {
    uint32_t i, argc;
    const char ** argv;
    const process_t * process;
    
    read_u32(argc);
    
    argv = adbi_malloc((argc + 1) * sizeof(const char *));
    for (i = 0; i < argc; ++i) {
        char name[16];
        adbi_assure(snprintf(name, 16, "argv[%u]", (unsigned int) i) < 16);
        read_strx(argv[i], name);
    }
    argv[argc] = NULL;
    
    if ((process = process_spawn(argv))) {
        say_OKAY("Spawned %u.", (unsigned int) process->pid);
    } else {
        say_FAIL("Error spawning process.");
    }
    
}

static const packet_t * handle_KILL(const packet_t * request) {
    uint32_t pid;
    read_u32(pid);
    
    process_t * process;
    
    if (!(process = process_get(pid)))
        say_FAIL("Process %u not attached.", pid);
        
    process_kill(process);
    process_put(process);
    
    say_OKAY("Process %u killed.", pid);
}

static const packet_t * handle_PROC(const packet_t * request) {

    uint32_t procc = 0;
    
    void callback(process_t * process) {
        char name[16];
        adbi_assure(snprintf(name, 16, "procv[%u]", (unsigned int) procc) < 16);
        write_u32x(name, process->pid);
        ++procc;
    }
    
    /* Report about processes. */
    process_iter(callback);
    
    /* Store amount of processes. */
    write_u32(procc);
    
    /* Return a nice message. */
    say_OKAY("Tracing %u process%s.", (unsigned int) procc, procc ? "" : "es");
}

/***********************************************************************************************************************
 * Injectable control
 **********************************************************************************************************************/

/* Injectable load. */
static const packet_t * handle_INJL(const packet_t * request) {
    const injectable_t * injectable;
    bool tstate;
    
    const char * path;
    const char * whynot;
    
    if ((tstate = state_tracing()))
        state_tracing_set(false);

    read_str(path);
    if ((injectable = injectable_load(path, &whynot))) {
        say_OKAY("Loaded injectable %u (%s) from %s.", injectable->id, injectable->injfile->name, path);
    } else {
        say_FAIL("Error loading injectable from %s: %s.", path, whynot);
    }

    if (tstate)
        state_tracing_set(true);
}

/* Injectable unload. */
static const packet_t * handle_INJU(const packet_t * request) {
    uint32_t iid;
    bool tstate;
    
    read_u32(iid);
    const char * whynot;
    
    if ((tstate = state_tracing()))
        state_tracing_set(false);

    bool res = injectable_unload(iid, &whynot);

    if (tstate)
        state_tracing_set(true);

    if (res){
        say_OKAY("Injectable %u unloaded.", iid);
    } else {
        say_FAIL("Error unloading injectable %u: %s.", iid, whynot);
    }
}

/* Injectable list. */
static const packet_t * handle_INJQ(const packet_t * request) {
    uint32_t injc = 0;
    void callback(const injectable_t * injectable) {
        char name[16];
        adbi_assure(snprintf(name, 16, "injid[%u]", (unsigned int) injc) < 16);
        write_u32x(name, injectable->id);
        adbi_assure(snprintf(name, 16, "injfn[%u]", (unsigned int) injc) < 16);
        write_strx(name, injectable->filename);
        adbi_assure(snprintf(name, 16, "injtp[%u]", (unsigned int) injc) < 16);
        write_strx(name, injectable_is_library(injectable) ? "library" : "handler");
        adbi_assure(snprintf(name, 16, "injnm[%u]", (unsigned int) injc) < 16);
        write_strx(name, injectable->injfile->name);
        adbi_assure(snprintf(name, 16, "injrc[%u]", (unsigned int) injc) < 16);
        write_u32x(name, injectable->references);
        adbi_assure(snprintf(name, 16, "injcm[%u]", (unsigned int) injc) < 16);
        write_strx(name, injectable->injfile->comment);
        ++injc;
    }
    injectable_iter(callback);
    write_u32(injc);
    say_OKAY("%u injectable%s loaded.", injc, injc == 1 ? "" : "s");
}

/* Injectable tracepoints. */
static const packet_t * handle_INJT(const packet_t * request) {
    uint32_t iid;
    read_u32(iid);
    
    const injectable_t * injectable = injectable_get(iid);
    if (!injectable) {
        say_FAIL("No such injectable: %u.", iid);
    }
    
    uint32_t tptc = 0;
    
    void callback(address_t addr, offset_t handler) {
        char name[16];
        adbi_assure(snprintf(name, 16, "tpta[%u]", (unsigned int) tptc) < 16);
        write_u32x(name, addr);
        adbi_assure(snprintf(name, 16, "tpth[%u]", (unsigned int) tptc) < 16);
        write_i32x(name, handler);
        ++tptc;
    }
    
    injectable_iter_tracepoints(injectable, callback);
    write_u32(tptc);
    
    if (tptc) {
        say_OKAY("%u tracepoint%s defined.", tptc, tptc == 1 ? "" : "s");
    } else {
        say_OKAY("No tracepoints defined.");
    }
}

/* Injectable symbols. */
static const packet_t * handle_INJ_(const packet_t * request, injectable_iter_fn_t fn) {
    uint32_t iid;
    read_u32(iid);
    
    const injectable_t * injectable = injectable_get(iid);
    if (!injectable) {
        say_FAIL("No such injectable: %u.", iid);
    }
    
    uint32_t symc = 0;
    
    void callback(const char * symbol, offset_t offset) {
        char name[16];
        adbi_assure(snprintf(name, 16, "symnm[%u]", (unsigned int) symc) < 16);
        write_strx(name, symbol);
        adbi_assure(snprintf(name, 16, "symad[%u]", (unsigned int) symc) < 16);
        write_u32x(name, (uint32_t) offset);
        ++symc;
    }
    
    fn(injectable, callback);
    write_u32(symc);
    
    if (symc) {
        say_OKAY("%u symbol%s defined.", symc, symc == 1 ? "" : "s");
    } else {
        say_OKAY("No symbols defined.");
    }
    
}

/* Injectable exports. */
static const packet_t * handle_INJE(const packet_t * request) {
    return handle_INJ_(request, injectable_iter_exports);
}

/* Injectable imports. */
static const packet_t * handle_INJI(const packet_t * request) {
    return handle_INJ_(request, injectable_iter_imports);
}

/* Injectable ADBI symbols. */
static const packet_t * handle_INJA(const packet_t * request) {
    return handle_INJ_(request, injectable_iter_adbi);
}

/***********************************************************************************************************************
 * ADBI server control
 **********************************************************************************************************************/

/* Set log level */
static const packet_t * handle_LLEV(const packet_t * request) {
    uint32_t loglevel;
    read_u32(loglevel);
    logger_level = loglevel;
    say_OKAY("Log level set to %i.", logger_level);
}

/* Start tracing. */
static const packet_t * handle_STRT(const packet_t * request) {
    if (state_tracing())
        say_FAIL("Tracees are already running.");
    state_tracing_set(true);
    say_OKAY("All inferiors are now running.");
}

/* Stop tracing. */
static const packet_t * handle_STOP(const packet_t * request) {
    if (!state_tracing())
        say_FAIL("Tracees are already stopped.");
    state_tracing_set(false);
    say_OKAY("All inferiors are now stopped.");
}

/* Ping (succeeds always) */
static const packet_t * handle_PING(const packet_t * request) {
    say_OKAY("Ping-pong.");
}

/* Quit request */
static const packet_t * handle_QUIT(const packet_t * request) {
    ++signal_quit;
    say_OKAY("ADBI Server quitting.");
}

/* Directory listing. */
static const packet_t * handle_LDIR(const packet_t * request) {
    const char * path;
    read_str(path);
    
    DIR * dir = opendir(path);
    
    if (!dir)
        say_FAIL("No such directory: %s.", path);
        
    uint32_t entc = 0;
    struct dirent * entry;
    
    while ((entry = readdir(dir))) {
        char name[16];
        adbi_assure(snprintf(name, 16, "entv[%u]", (unsigned int) entc) < 16);
        write_strx(name, entry->d_name);
        
        /* check if this is a directory */
        adbi_assure(snprintf(name, 16, "entd[%u]", (unsigned int) entc) < 16);
        write_u32x(name, entry->d_type == DT_DIR);
        
        ++entc;
    }
    
    write_u32(entc);
    closedir(dir);
    say_OKAY("Found %i elements.", entc);
}

/***********************************************************************************************************************
 * Process diagnostics
 **********************************************************************************************************************/

/* Address explanation. */
static const packet_t * handle_ADDR(const packet_t * request) {
    uint32_t pid;
    uint64_t address;
    process_t * process;
    const char * info;
    
    read_u32(pid);
    read_u64(address);
    
    if (!(process = process_get(pid)))
        say_FAIL("Not attached to %u.", (unsigned int) pid);
        
    info = str_address(process, address);
    
    write_u64(address);
    write_str(info);
    process_put(process);
    say_OKAY("Address %s.", info);
}

/* Process memory dump. */
static const packet_t * handle_MEMD(const packet_t * request) {
    uint32_t pid;
    uint64_t address;
    uint32_t size;
    process_t * process;
    uint32_t * data;
    bool is_running;
    
    read_u32(pid);
    read_u64(address);
    read_u32(size);
    
    size &= ~(0x3);
    if (size > 1024 * 10)
        size = 1024 * 10;
        
    if (!(process = process_get(pid)))
        say_FAIL("Not attached to %u.", (unsigned int) pid);
        

    data = adbi_malloc(size);

    if ((is_running = process_is_running(process)))
        process_stop(process);

    size = mem_read(thread_any_stopped(process), address, size, data);

    if (is_running)
        process_continue(process);

    size &= ~(0x3);
    size /= sizeof(size);
    
    for (typeof(size) i = 0; i < size; ++i) {
        char name[16];
        adbi_assure(snprintf(name, 16, "word[%u]", i) < 16);
        write_u32x(name, data[i]);
    }
    
    write_u64(address);
    write_u32(size);
    process_put(process);
    free(data);
    say_OKAY("Dumped %u bytes.", size);
}

/* Process memory map. */
static const packet_t * handle_MAPS(const packet_t * request) {
    uint32_t pid;
    process_t * process;
    
    uint32_t segc = 0;
    
    void report(const char * type, address_t lo, address_t hi,
                const char * file, offset_t offset) {
        char name[16];
        
        adbi_assure(snprintf(name, 16, "seglo[%u]", (unsigned int) segc) < 16);
        write_u64x(name, lo);
        
        adbi_assure(snprintf(name, 16, "seghi[%u]", (unsigned int) segc) < 16);
        write_u64x(name, hi);
        
        adbi_assure(snprintf(name, 16, "segtype[%u]", (unsigned int) segc) < 16);
        write_strx(name, type);
        
        adbi_assure(snprintf(name, 16, "segfile[%u]", (unsigned int) segc) < 16);
        write_strx(name, file ? file : "");
        
        adbi_assure(snprintf(name, 16, "segoff[%u]", (unsigned int) segc) < 16);
        write_u32x(name, file ? offset : 0);
        
        ++segc;
    }
    
    void callback_segment(segment_t * segment) {
        report("natural", segment->start, segment->end, segment->filename, segment->offset);
        if (segment->trampolines) {
            char name[128];
            snprintf(name, 128, "<%s - %s>",
                     segment->filename,
                     segment->injection->injectable->filename);
            report("trampoline", segment->trampolines,
                   segment->trampolines + segment->trampolines_size,
                   name, 0);
        }
    }
    
    void callback_injection(injection_t * injection) {
        report("injection", injection->address,
               injection->address + injection->injectable->injfile->code_size,
               injection->injectable->filename, 0);
    }
    
    read_u32(pid);
    if (!(process = process_get(pid)))
        say_FAIL("Not attached to %u.", (unsigned int) pid);
        
    segment_iter(process, callback_segment);
    injection_iter(process, callback_injection);
    
    write_u32(segc);
    process_put(process);
    say_OKAY("Process %u has %u segment%s.", pid, segc, segc == 1 ? "" : "s");
}

/******************************************************************************/

void protocol_cleanup() {
    if (payload_buffer)
        payload_free(payload_buffer);
}

bool protocol_init() {
    payload_buffer = payload_create();
    debug("Response payload buffer created.");
    return payload_buffer;
}

/**********************************************************************************************************************/

/* Translate the packet type represented as uint into a human readable string. */
static const char * packet_str(uint32_t packet_type) {
    static union {
        uint32_t i;
        char s[5];
    } t;
    t.i = bswap_32(packet_type);
    t.s[4] = '\0';
    for (int i = 0; i < 4; ++i) {
        if (!isprint(t.s[i]))
            t.s[i] = '.';
        else if (isspace(t.s[i]))
            t.s[i] = ' ';
    }
    return t.s;
}

/* Main request packet handling function. Takes a request packet as parameter
 * and returns a response packet. The function always checks the packet payload
 * for correctness. If the packet is correct, it calls the appropriate
 * handle_XXXX function.
 */
const packet_t * handle_packet(const packet_t * request) {

    payload_reset(payload_buffer);
    
    if (!payload_check(request->payload, request->head.length)) {
        warning("Malformed packet received.");
        say_MALF("Packet payload malformed.");
    }
    
#define call_handler(what)                          \
    if (request->head.type == packet_id(# what)) {  \
        info("Received %s packet.", # what);        \
        return handle_ ## what(request);            \
    }
    
    /* process control */
    call_handler(ATTC)
    call_handler(DETC)
    call_handler(SPWN)
    call_handler(KILL)
    call_handler(PROC)
    
    /* injectables control */
    call_handler(INJL)  /* load         */
    call_handler(INJU)  /* unload       */
    call_handler(INJQ)  /* query        */
    
    /* injectable information */
    call_handler(INJE)  /* exports      */
    call_handler(INJI)  /* imports      */
    call_handler(INJA)  /* adbi         */
    call_handler(INJT)  /* tracepoints  */
    
    /* adbiserver control */
    call_handler(LLEV)
    call_handler(STRT)
    call_handler(STOP)
    call_handler(QUIT)
    
    /* process diagnostics */
    call_handler(ADDR)
    call_handler(MEMD)
    call_handler(MAPS)
    
    /* helper requests */
    call_handler(LDIR)
    call_handler(PING)
    
#undef call_handler
    
    info("Received unsupported packet type: %s.", packet_str(request->head.type));
    say_USUP("Unsupported packet type: %s.", packet_str(request->head.type));
}
