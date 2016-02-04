#ifndef SPAWN_H_
#define SPAWN_H_

#include <sys/types.h>

bool spawn_is_spawned(pid_t pid);
void spawn_died(pid_t);
pid_t spawn_process(const char * const * argv);

#endif /* SPAWN_H_ */
