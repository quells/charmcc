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

static void debug_node(Node *n) {
    switch (n->kind) {
    case ND_ADD:
        debug_binop("+", n);
        return;
    case ND_SUB:
        debug_binop("-", n);
        return;
    case ND_DIV:
        debug_binop("/", n);
        return;
    case ND_NEG:
        debug_unop("-", n);
        return;
    case ND_ADDR:
        debug_unop("addr", n);
        return;
    case ND_DEREF:
        debug_unop("ptr", n);
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
        printf("(let %s ", n->lhs->var->name);
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
            printf("? ");
        }
        debug_node(n->consequence);
        if (n->increment) {
            debug_node(n->increment);
        }
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
        printf("%s", n->var->name);
        return;
    }
}

void debug_ast(Function *prog) {
    if (prog->locals != NULL) {
        printf("Local variables:\n");
        for (Obj *l = prog->locals; l; l = l->next) {
            printf("  %s\n", l->name);
        }
        printf("\n");
    }

    debug_nodes(prog->body);
    printf("\n");
}
