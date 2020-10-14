#include "charmcc.h"

Type *ty_int = &(Type){TY_INT};

bool is_integer(Type *ty) {
    return ty->kind == TY_INT;
}

Type *pointer_to(Type *base) {
    Type *type = calloc(1, sizeof(Type));
    type->kind = TY_PTR;
    type->base = base;
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
    case ND_VAR:
    case ND_NUM:
        node->type = ty_int;
        return;
    case ND_ADDR:
        node->type = pointer_to(node->lhs->type);
        return;
    case ND_DEREF:
        if (node->lhs->type->kind == TY_PTR) {
            node->type = node->lhs->type->base;
        } else{
            node->type = ty_int;
        }
        return;
    }
}
