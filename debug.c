#include "charmcc.h"

static void debug_node(Node *n);

static void debug_nodes(Node *nodes) {
    for (Node *n = nodes; n; n = n->next) {
        debug_node(n);
    }
}

static void debug_binop(char *op, Node *n) {
    printf("(%s ", op);
    debug_node(n->lhs);
    printf(", ");
    debug_node(n->rhs);
    printf(")");
}

static void debug_unop(char *op, Node *n) {
    printf("(%s ", op);
    debug_node(n->lhs);
    printf(")");
}

static void debug_type(Type *t) {
    if (t == NULL) return;

    if (t->kind == TY_PTR) {
        printf("*");
        debug_type(t->base);
    }
}

static void debug_node(Node *n) {
    switch (n->kind) {
    case ND_ADD:
        debug_binop("+", n);
        return;
    case ND_SUB:
        debug_binop("-", n);
        return;
    case ND_MUL:
        debug_binop("*", n);
        return;
    case ND_DIV:
        debug_binop("/", n);
        return;
    case ND_NEG:
        debug_unop("-", n);
        return;
    case ND_ADDR:
        printf("(addr ");
        debug_node(n->lhs);
        printf(")");
        return;
    case ND_DEREF:
        printf("(deref ");
        debug_node(n->lhs);
        printf(")");
        return;
    case ND_EQ:
        debug_binop("==", n);
        return;
    case ND_NEQ:
        debug_binop("!=", n);
        return;
    case ND_LT:
        debug_binop("<", n);
        return;
    case ND_LTE:
        debug_binop("<=", n);
        return;
    case ND_NUM:
        printf("%d", n->val);
        return;

    case ND_ASSIGN:
        printf("(let ");
        debug_node(n->lhs);
        printf(" ");
        debug_node(n->rhs);
        printf(")");
        return;
    case ND_IF:
        printf("(if ");
        debug_node(n->condition);
        printf(" ");
        debug_node(n->consequence);
        if (n->alternative) {
            printf(" : ");
            debug_node(n->alternative);
        }
        printf("); ");
        return;
    case ND_LOOP:
        printf("(loop ");
        if (n->initialize) {
            debug_node(n->initialize);
        }
        if (n->condition) {
            debug_node(n->condition);
        }
        if (n->increment) {
            debug_node(n->increment);
        }
        debug_node(n->consequence);
        printf("); ");
        return;
    case ND_RETURN:
        debug_unop("return", n);
        return;

    case ND_BLOCK:
        printf("{ ");
        debug_nodes(n->body);
        printf(" }");
        return;
    case ND_EXPR_STMT:
        debug_node(n->lhs);
        printf("; ");
        return;
    case ND_VAR:
        if (n->var->type->kind == TY_PTR) {
            debug_type(n->var->type);
        }
        printf("%s", n->var->name);
        return;
    case ND_FN_CALL:
        printf("(call %s", n->func);
        for (Node *arg = n->args; arg; arg = arg->next) {
            printf(" ");
            debug_node(arg);
        }
        printf(")");
        return;
    }

    error_tok(n->repr, "unhandled node");
}

void debug_fn(Function *fn) {
    if (fn->locals != NULL) {
        printf("%s local variables:", fn->name);
        for (Obj *l = fn->locals; l; l = l->next) {
            printf("  %s", l->name);
        }
        printf("\n");
    }
    printf("%s :: ", fn->name);

    debug_nodes(fn->body);
    printf("\n");
}

void debug_ast(Function *prog) {
    for (Function *fn = prog; fn; fn = fn->next) {
        debug_fn(fn);
    }
}
