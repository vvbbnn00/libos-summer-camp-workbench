#ifndef OBJPOOL_H
#define OBJPOOL_H

#include "spinlock.h"
#include "bitmap.h"

struct objpool {
    void* pool;
    bitmap_t* bitmap;
    size_t objsize;
    size_t num;
    size_t count;
    spinlock_t lock;
};

#define OBJPOOL_ALLOC(NAME, TYPE, N) \
    TYPE _##NAME##_array[N];\
    BITMAP_ALLOC(_##NAME##_array_bitmap, N);\
    struct objpool NAME = {\
        .pool = _##NAME##_array,\
        .bitmap = _##NAME##_array_bitmap,\
        .objsize = sizeof(TYPE),\
        .num = N,\
        .lock = SPINLOCK_INITVAL,\
    }

void objpool_init(struct objpool *objpool);
void* objpool_alloc(struct objpool *objpool);
void objpool_free(struct objpool *objpool, void* obj);

#endif