#ifndef BINDATA_H
#define BINDATA_H

#include <stdlib.h>

struct bindata_t {
    size_t size;
    unsigned char * data;
};

typedef struct bindata_t bindata_t;

#endif
