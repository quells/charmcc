#include "charmcc.h"

Type *ty_int = &(Type){TY_INT, 4}; // signed 32-bit integer

bool is_integer(Type *type) {
    return type->kind == TY_INT;
}

Type *copy_type(Type *type, MemManager *mm) {
    Type *copy = allocate(mm, sizeof(Type));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc copy of type %p\n", type);
    #endif

    *copy = *type;
    return copy;
}

Type *pointer_to(Type *base, MemManager *mm) {
    Type *type = allocate(mm, sizeof(Type));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc ptrty %p\n", type);
    #endif

    type->kind = TY_PTR;
    type->size = PTR_SIZE;
    type->base = base;
    return type;
}

Type *func_type(Type *return_type, MemManager *mm) {
    Type *type = allocate(mm, sizeof(Type));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc fn ty %p\n", type);
    #endif

    type->kind = TY_FUNC;
    type->return_type = return_type;
    return type;
}

Type *array_of(Type *base, int len, MemManager *mm) {
    Type *type = allocate(mm, sizeof(Type));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc array %p of %p\n", type, base);
    #endif

    type->kind = TY_ARRAY;
    type->size = base->size * len;
    type->base = base;
    type->array_len = len;
    return type;
}

void add_type(Node *node, MemManager *mm) {
    if (!node || node->type) {
        return;
    }

    add_type(node->lhs, mm);
    add_type(node->rhs, mm);
    add_type(node->condition, mm);
    add_type(node->consequence, mm);
    add_type(node->alternative, mm);
    add_type(node->initialize, mm);
    add_type(node->increment, mm);

    for (Node *n = node->body; n; n = n->next) {
        add_type(n, mm);
    }

    switch (node->kind) {
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_NEG:
        node->type = node->lhs->type;
        return;
    case ND_ASSIGN:
        if (node->lhs->type->kind == TY_ARRAY) {
            error_tok(node->lhs->repr, "not an lvalue");
        }
        node->type = node->lhs->type;
        return;
    case ND_EQ:
    case ND_NEQ:
    case ND_LT:
    case ND_LTE:
    case ND_NUM:
    case ND_FN_CALL:
        node->type = ty_int;
        return;
    case ND_VAR:
        node->type = node->var->type;
        return;
    case ND_ADDR:
        if (node->lhs->type->kind == TY_ARRAY) {
            node->type = pointer_to(node->lhs->type->base, mm);
        } else {
            node->type = pointer_to(node->lhs->type, mm);
        }
        return;
    case ND_DEREF:
        if (!node->lhs->type->base) {
            error_tok(node->repr, "invalid pointer dereference");
        }
        node->type = node->lhs->type->base;
        return;
    default:
        break;
    }
}
