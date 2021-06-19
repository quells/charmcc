#include "charmcc.h"

void codegen_32(IR *prog) {
    error("implemente codegen_32");

    // int contains_div = 0;
    // int global_vars = assign_offsets(ast);
    // if (global_vars) {
    //     printf(".data\n.balign 4\n\n");
    //     for (Obj *obj = prog; obj; obj = obj->next) {
    //         if (!obj->is_function) {
    //             printf("__global_%s:\n", obj->name);
    //             printf("  .word 0\n");
    //         }
    //     }
    //     printf("\n");
    // }

    // printf(".text\n.balign 4\n");
    // for (Obj *obj = prog; obj; obj = obj->next) {
    //     if (obj->is_function) {
    //         printf(".global %s\n", obj->name);
    //     }
    // }
    // printf("\n");
    // for (Obj *obj = prog; obj; obj = obj->next) {
    //     if (obj->is_function) {
    //         contains_div += gen_fn(obj);
    //     }
    // }

    // if (contains_div) {
    //     gen_div();
    // }

    // if (global_vars) {
    //     if (contains_div) {
    //         printf("\n");
    //     }
    //     for (Obj *obj = prog; obj; obj = obj->next) {
    //         if (!obj->is_function) {
    //             printf("__addr_%s: .word __global_%s\n", obj->name, obj->name);
    //         }
    //     }
    //     printf("\n");
    // }
}
