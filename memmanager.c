#include "charmcc.h"

/*
A linked list where the head keeps track of the tail for fast append.
Each element of the list is a pointer which needs to be freed before the program can exit.
*/
MemManager *new_memmanager() {
    MemManager *mm = calloc(1, sizeof(MemManager));
    return mm;
}

void register_obj(MemManager *mm, void *obj) {
    MemManager *next = calloc(1, sizeof(MemManager));
    next->obj = obj;

    if (mm->tail == NULL) {
        // first registration
        mm->next = next;
    } else {
        // every other registration
        mm->tail->next = next;
    }
    mm->tail = next;
}

void *allocate(MemManager *mm, size_t size) {
    void *obj = calloc(1, size);
    register_obj(mm, obj);
    return obj;
}

/*
Frees each element of the list and the root MemManager.
*/
void cleanup(MemManager *mm) {
    if (mm == NULL) return;
    if (mm->obj != NULL) {
        #if DEBUG_ALLOCS
        printf("free %p\n", mm->obj);
        #endif
        free(mm->obj);
        #if DEBUG_ALLOCS
        mm->obj = NULL;
        #endif
    }
    if (mm->next != NULL) {
        cleanup(mm->next);
        free(mm->next);
        #if DEBUG_ALLOCS
        mm->next = NULL;
        #endif
    }
    if (mm->tail != NULL) {
        // free root
        free(mm);
        #if DEBUG_ALLOCS
        mm->tail = NULL;
        #endif
    }
}
