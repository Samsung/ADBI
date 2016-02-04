#ifndef JUMP_H_
#define JUMP_H_

#include "process/process.h"
#include "tree.h"

void jump_install(process_t * process, address_t from, address_t to);
void jump_uninstall(process_t * process, address_t from);

static inline address_t jump_get(process_t * process, address_t where) {
    return (address_t) tree_get(&process->jumps, (tree_key_t) where);
}

#endif
