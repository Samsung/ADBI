#ifndef ASYNCIO_H
#define ASYNCIO_H

bool comm_init(void);
void comm_cleanup(void);

void comm_handle_io(void);

#endif
