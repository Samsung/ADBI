#include "process/process.h"
#include "process/segment.h"
#include "jump.h"

void jump_install(process_t * process, address_t from, address_t to) {
    debug("Installing jump in process %s: %s -> %s.", str_process(process), str_address(process, from),
          str_address(process, to));
    tree_insert(&process->jumps, (tree_key_t) from, (void *) to);
}

void jump_uninstall(process_t * process, address_t from) {
    debug("Removing jump in process %s at address %s.", str_process(process), str_address(process, from));
    tree_remove(&process->jumps, from);
}
