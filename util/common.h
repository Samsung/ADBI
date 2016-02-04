#ifndef COMMON_H
#define COMMON_H

#if 1
/*
 * assert corrupts the stack, so it's impossible to debug with gdb after a fail
 */
#undef assert
#define assert adbi_assure
#endif

/* ADBI uses only one thread.  Still, many functions support calls from multiple threads by using thread local storage
 * instead of global variables, etc.  Use of thread local storage is expensive, so we disable the __thread keyword and
 * simply use regular static or global variables. */
#if 1
#define __thread
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "likely.h"
#include "logger.h"

/* Macro for marking unused parameters. */
#define UNUSED(x) (void)(x)

/* Maximum length of absolute file path. */
#define MAX_PATH 256

/* Memory allocation. */
void * adbi_malloc(size_t size);
void * adbi_realloc(void * ptr, size_t size);

/* Function for internal error notifications. */
void adbi_bug_(
    const char * file, int line, const char * function,
    const char * fmt, ...)
__attribute__((noreturn));
#define adbi_bug(...)               \
    adbi_bug_(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);

#define adbi_bug_unrechable()       \
    adbi_bug_(__FILE__, __LINE__, __FUNCTION__, "Unreachable code reached.");

#define adbi_assure(cond)           \
    if (unlikely(!(cond)))          \
        adbi_bug_(__FILE__, __LINE__, __FUNCTION__, "False: %s", # cond);

#define packed_struct struct __attribute__((__packed__))

typedef uint32_t id_t;

typedef unsigned int refcnt_t;

#include "arch.h"

#define UNIMPLEMENTED()     warning("Call to unimplemented function %s.", __FUNCTION__);

#define ADBI_INJECTABLE_NAME "#adbi"


#include "util/human.h"

#endif
