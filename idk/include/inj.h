#ifndef _ADBI_INJ_H_
#define _ADBI_INJ_H_

/* Boolean support. */
#define bool    _Bool
#define true    1
#define false   0

/* Attributes of GLOBAL functions. */
#define GLOBAL_ATTR __attribute__((noclone, noinline, section(".adbi")))

/* Marks a function to be externally callable (by ADBI server or by another injectable).
 *
 * Usage:
 *    GLOBAL_ATTR
 void my_function() { ... };
 *
 * The macro expands to a set of attributes, which guarantee that the function will appear in the result injectable
 * after linking.
 */
#define GLOBAL  GLOBAL_ATTR

/* Marks function local (not visible externally), this is the oposite of GLOBAL. */
#define LOCAL   static

/* Defines an imported function.  The function will be dynamically linked after the injectable is loaded.  The imported
 * function must already exist (and be exported) in one of the injectables already loaded.
 *
 * Usage:
 *      IMPORT(<function name>, <return type>, <args>);
 *
 * Example:
 *      IMPORT(foo, int, int a, char b)     // imports int foo(int a, char b);
 *      IMPORT(bar, void, void)             // imports void bar(void);
 *
 */
#ifdef __aarch64__

#define IMPORT(function, rettype, ...)                                      \
    asm(".type "# function ", %function             \n"                     \
        ".type __import$"# function ", %function    \n"                     \
        ".weak __import$"# function "               \n"                     \
        ".align 2                                   \n"                     \
        "__import$" # function ":                   \n"                     \
        # function ":                               \n"                     \
        "   ldr x16, 1f		                        \n"                     \
        "   br x16                                  \n"                     \
        "1:	.dword	0xbaadcafebaadf00d	            \n"                     \
    );                                                                      \
    __attribute__((noinline)) rettype function(__VA_ARGS__)

#else

#define IMPORT(function, rettype, ...)                                      \
    static __attribute__((naked, noinline))                                 \
    rettype function(__VA_ARGS__) {                                         \
        asm("   ldr pc, 1f         \n"                                      \
            "1: .word  0xbaadf00d  \n");                                    \
    }                                                                       \
    void __import$ ## function() __attribute__((weak, alias(# function)));

#endif

/* Enables exporting of a function.  The exported function must be GLOBAL.
 *
 * Usage:
 *      GLOBAL void foo(int bar) { ...foo definition... }
 *      EXPORT(foo);
 */
#define EXPORT(function) void __export$ ## function() __attribute__((weak, alias(# function)))

/* Enables visibility of a function to ADBI server.  The exported function must be GLOBAL.
 * Usage:
 *      GLOBAL void foo(int bar) { ...foo definition... }
 *      ADBI(foo);
 */
#define ADBI(function) void __adbi$ ## function() __attribute__((weak, alias(# function)))

/* Defines code to be executed just after the injectable is loaded.  When this function gets called, all imports are
 * already resolved.  The function should return zero on successful initialization, non-zero on error.  If the
 * initialization function fails, the injectable is unloaded immediately.
 *
 * There are also zero, one and two parameter versions of injectable initialization function where first is PID (TID)
 * of current running thread and second is TGID (PID).
 *
 * Usage:
 *      INIT() {
 *          ... initialization code ...
 *      }
 *
 *      INIT(int pid) {
 *          ... initialization code ...
 *      }
 *
 *      INIT(int pid, int tgid) {
 *          ... initialization code ...
 *      }
 *
 */
#define INIT0() GLOBAL_ATTR int __adbi$init(void)
#define INIT1(pid) GLOBAL_ATTR int __adbi$init(pid)
#define INIT2(pid, tgid) GLOBAL_ATTR int __adbi$init(pid, tgid)

#define GET_INIT_MACRO(_0, _1, _2, NAME, ...) NAME
#define INIT(...) GET_INIT_MACRO(_0, ##__VA_ARGS__, INIT2, INIT1, INIT0)(__VA_ARGS__)

/* Defines a handler function to be launched when execution reaches the given address. The address should be a hex
 * value, 8 chars long, representing the offset inside the binary file.
 *
 * Usage:
 *      HANDLER(00001000) {
 *         ...process the hit...
 *      }
 */
#define HANDLER(address) GLOBAL_ATTR void __handler$ ## address(void)

/* This is the entry point of the injectable.  All this function does is jumping directly to the initialization
 * function.  This function is marked as the ELF entry address.  Just like all other GLOBAL functions, it is placed in
 * the .adbi section.  This way we make sure the .adbi section is not removed during section garbage collection (GLOBAL
 * functions may be unreferenced locally, so they could be removed -- however, the linker can only remove whole
 * sections and it can't remove the section which contains the entry address). */
#ifdef __aarch64__
__attribute__((section(".adbi"))) void __adbi$entry(int pid, int tgid) {
    asm("b __adbi$init");   /* jump directly, without even touching the stack. */
}

#else /* __aarch64__ */

__attribute__((section(".adbi"), naked)) void __adbi$entry(int pid, int tgid) {
    asm("b __adbi$init");   /* jump directly, without even touching the stack. */
}
#endif /* __aarch64__ */

/* Defines code to be executed just after the new process was spawned by traced process. Function is executed by
 * newly created main thread of new process.
 *
 * There is zero and one argument versions of function. First parameter if used is PID of newly spawned process
 * and also is PID (TID) of its main thread.
 *
 * Usage:
 *      NEW_PROCESS() {
 *          ... new process handler code ...
 *      }
 *
 *      NEW_PROCESS(int pid) {
 *          ... new process handler code ...
 *      }
 */
#define NEW_PROCESS0() GLOBAL_ATTR int __adbi$new_process(void)
#define NEW_PROCESS1(pid) GLOBAL_ATTR int __adbi$new_process(pid)

#define GET_NEW_PROCESS_MACRO(_0, _1, NAME, ...) NAME
#define NEW_PROCESS(...) GET_NEW_PROCESS_MACRO(_0, ##__VA_ARGS__, NEW_PROCESS1, NEW_PROCESS0)(__VA_ARGS__)

/* Defines code to be executed just after the new thread was created by one of traced processes. Function is
 * executed by newly created thread.
 *
 * There is two, one and zero argument versions of function. Like in INIT handler first parameter is PID (TID)
 * and second is TGID (PID) of newly created thread.
 *
 * Usage:
 *      NEW_THREAD() {
 *          ... new thread handler code ...
 *      }
 *
 *      NEW_THREAD(int pid) {
 *          ... new thread handler code ...
 *      }
 *
 *      NEW_THREAD(int pid, int tgid) {
 *          ... new thread handler code ...
 *      }
 */
#define NEW_THREAD0() GLOBAL_ATTR int __adbi$new_thread(void)
#define NEW_THREAD1(pid) GLOBAL_ATTR int __adbi$new_thread(pid)
#define NEW_THREAD2(pid, tgid) GLOBAL_ATTR int __adbi$new_thread(pid, tgid)

#define GET_NEW_THREAD_MACRO(_0, _1, _2, NAME, ...) NAME
#define NEW_THREAD(...) GET_NEW_THREAD_MACRO(_0, ##__VA_ARGS__, NEW_THREAD2, NEW_THREAD1, NEW_THREAD0)(__VA_ARGS__)

/* Define code to be executed just after detach from process. Code will be run with all injections already unloaded,
 * so after this function no other handler will be executed in process (except we attach to i again). Please note that
 * this is not process exit handler but injection detach handler. Code will not be executed on normal process exit.
 * Code can be run in any thread of detaching process.
 *
 * There is zero and one argument versions of function. First parameter if used is TGID (PID) of process being detached.
 *
 * Usage:
 *      EXIT() {
 *          ... detach handler code ...
 *      }
 *
 *      EXIT(int pid) {
 *          ... detach handler code ...
 *      }
 */

#define EXIT0() GLOBAL_ATTR int __adbi$exit(void)
#define EXIT1(pid) GLOBAL_ATTR int __adbi$exit(pid)

#define GET_EXIT_MACRO(_0, _1, NAME, ...) NAME
#define EXIT(...) GET_EXIT_MACRO(_0, ##__VA_ARGS__, EXIT1, EXIT0)(__VA_ARGS__)

#endif
