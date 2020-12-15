#include "charmcc.h"

Type *ty_int = &(Type){TY_INT};

void free_type(Type *t) {
    if (t == NULL) return;

    free_type(t->return_type);

    switch (t->kind) {
        case TY_INT:
            break;
        case TY_PTR:
            #if DEBUG_ALLOCS
            fprintf(stderr, "free  ptrty %p\n", t);
            #endif
            free_type(t->base);
            free(t);
            break;
        case TY_FUNC:
            #if DEBUG_ALLOCS
            fprintf(stderr, "free  fn ty %p\n", t);
            #endif
            free_type(t->base);
            free(t);
            break;
        default:
            // ignore previously freed types
            break;
    }
}

bool is_integer(Type *type) {
    return type->kind == TY_INT;
}

Type *copy_type(Type *type) {
    Type *copy = calloc(1, sizeof(Type));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc copy of type %p\n", type);
    #endif

    *copy = *type;
    return copy;
}

Type *pointer_to(Type *base) {
    Type *type = calloc(1, sizeof(Type));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc ptrty %p\n", type);
    #endif

    type->kind = TY_PTR;
    type->base = base;
    return type;
}

Type *func_type(Type *return_type) {
    Type *type = calloc(1, sizeof(Type));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc fn ty %p\n", type);
    #endif

    type->kind = TY_FUNC;
    type->return_type = return_type;
    return type;
}

void add_type(Node *node) {
    if (!node || node->type) {
        return;
    }

    add_type(node->lhs);
    add_type(node->rhs);
    add_type(node->condition);
    add_type(node->consequence);
    add_type(node->alternative);
    add_type(node->initialize);
    add_type(node->increment);

    for (Node *n = node->body; n; n = n->next) {
        add_type(n);
    }

    switch (node->kind) {
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_NEG:
    case ND_ASSIGN:
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
        node->type = pointer_to(node->lhs->type);
        return;
    case ND_DEREF:
        if (node->lhs->type->kind != TY_PTR) {
            error_tok(node->repr, "invalid pointer dereference");
        }
        node->type = node->lhs->type->base;
        return;
    default:
        break;
    }
}
