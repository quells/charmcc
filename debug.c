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

void debug_fn(Obj *fn) {
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

void debug_ast(Obj *prog) {
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (fn->is_function) {
            debug_fn(fn);
        }
    }
}

void debug_ir(IR *prog) {
    int globals = 0;
    for (StringList *str = prog->vars; str; str = str->next) {
        printf(".global var %s\n", str->s);
        globals++;
    }
    for (StringList *str = prog->funcs; str; str = str->next) {
        printf(".global fn %s\n", str->s);
        globals++;
    }

    for (IRInstruction *inst = prog->instructions; inst; inst = inst->next) {
        switch (inst->kind) {
        case IR_LABEL:
            printf("%s:\n", inst->target);
            break;

        case IR_ALLOC:
            printf("  alloc %d\n", inst->imm);
            break;
        case IR_STORE:
            printf("  store r%d [#%d]\n", inst->r, inst->imm);
            break;
        case IR_LOAD:
            printf("  load r%d [#%d]\n", inst->r, inst->imm);
            break;
        case IR_ASSIGN:
            printf("  assign r%d #%d\n", inst->r, inst->imm);
            break;
        case IR_PUSH:
            printf("  push r%d\n", inst->r);
            break;
        case IR_POP:
            printf("  pop r%d\n", inst->r);
            break;
        case IR_PROLOGUE:
            printf("  prologue\n");
            break;
        case IR_EPILOGUE:
            printf("  epilogue\n");
            break;
        case IR_RETURN:
            printf("  return\n");
            break;

        case IR_NEG:
            printf("  r%d <- -r%d\n", inst->r, inst->r);
            break;
        case IR_ADD:
            printf("  r%d <- r%d + r%d\n", inst->r, inst->a, inst->b);
            break;
        case IR_SUB:
            printf("  r%d <- r%d - r%d\n", inst->r, inst->a, inst->b);
            break;
        case IR_MUL:
            printf("  r%d <- r%d * r%d\n", inst->r, inst->a, inst->b);
            break;
        case IR_DIV:
            printf("  r%d <- r%d / r%d\n", inst->r, inst->a, inst->b);
            break;

        case IR_CMP:
            printf("  cmp r%d, r%d\n", inst->a, inst->b);
            break;
        case IR_EQ:
            printf("  r%d <- eq ? 1 : 0\n", inst->r);
            break;
        case IR_NEQ:
            printf("  r%d <- eq ? 0 : 1\n", inst->r);
            break;
        case IR_LT:
            printf("  r%d <- lt ? 1 : 0\n", inst->r);
            break;
        case IR_LTE:
            printf("  r%d <- lte ? 1 : 0\n", inst->r);
            break;

        case IR_BRANCH:
            printf("  b %s\n", inst->target);
            break;
        case IR_BRANCH_EQ:
            printf("  beq %s\n", inst->target);
            break;
        case IR_BRANCH_NEQ:
            printf("  bne %s\n", inst->target);
            break;
        case IR_BRANCH_LT:
            printf("  bl %s\n", inst->target);
            break;
        case IR_BRANCH_LTE:
            printf("  ble %s\n", inst->target);
            break;
        }
    }
}
