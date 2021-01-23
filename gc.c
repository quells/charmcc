#include "charmcc.h"

void *allocate(GC *gc, size_t size) {
    void *obj = calloc(1, size);
    GC *next = calloc(1, sizeof(GC));
    next->obj = obj;

    if (gc->tail == NULL) {
        gc->next = next;
    } else {
        gc->tail->next = next;
    }
    gc->tail = next;

    return obj;
}

void cleanup(GC *gc) {
    if (gc == NULL) return;
    if (gc->obj != NULL) {
        free(gc->obj);
    }
    if (gc->next != NULL) {
        cleanup(gc->next);
        free(gc->next);
    }
}
