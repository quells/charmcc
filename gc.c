#include "charmcc.h"

void *allocate(GC *gc, size_t size) {
    void *ptr = calloc(1, size);
    GC *next = calloc(1, sizeof(GC));
    next->next = gc;
    next->obj = ptr;
    gc = next;
    return ptr;
}

void cleanup(GC *gc) {
    if (gc == NULL) return;
    cleanup(gc->next);
    free(gc->obj);
}
