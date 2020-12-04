#include "charmcc.h"

static int depth;

static void gen_expr(Node *node);

static int count(void) {
    static int i = 1;
    return i++;
}

static void push(void) {
    printf("  push  {r0}\n");
    depth++;
}

static void pop(char *arg) {
    printf("  pop   {%s}\n", arg);
    depth--;
}

/*
Round up `n` to the nearest multiple of `align`.
For example, align_to(5, 8) == 8 and align_to(11, 8) == 16.
*/
static int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}

// Check if the AST contains a node of kind.
// Returns 1 if found, 0 otherwise.
static int contains(Node *node, NodeKind kind) {
    if (node == NULL) {
        return 0;
    }

    if (node->kind == kind) {
        return 1;
    }

    return contains(node->lhs, kind)
        || contains(node->rhs, kind)
        || contains(node->body, kind)
        || contains(node->next, kind)
        || contains(node->condition, kind)
        || contains(node->consequence, kind)
        || contains(node->alternative, kind)
        || contains(node->initialize, kind)
        || contains(node->increment, kind);
}

// Compute absolute address of a node.
// It's an error if a given node does not reside in memory.
static void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR:
        printf("  sub   r0, fp, #%d\n", node->var->offset);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    default:
        break;
    }

    error_tok(node->repr, "not an lvalue");
}

static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("  mov   r0, #%d\n", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        printf("  neg   r0, r0\n");
        return;
    case ND_VAR:
        gen_addr(node);
        printf("  ldr   r0, [r0]\n");
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        printf("  ldr   r0, [r0]\n");
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("r1");
        printf("  str   r0, [r1]\n");
        return;
    case ND_FN_CALL: {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            nargs++;
        }

        assert(nargs <= 4);
        char reg[3] = {'r', '_', '\0'};
        for (int i = nargs - 1; i >= 0; i--) {
            reg[1] = '0' + i;
            pop(reg);
        }

        printf("  bl    %s\n", node->func);
        return;
    }
    default:
        break;
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("r1");

    switch (node->kind) {
    case ND_ADD:
        printf("  add   r0, r0, r1\n");
        return;
    case ND_SUB:
        printf("  sub   r0, r0, r1\n");
        return;
    case ND_MUL:
        printf("  mul   r0, r0, r1\n");
        return;
    case ND_DIV:
        printf("  bl    div\n");
        return;
    case ND_EQ:
    case ND_NEQ:
    case ND_LT:
    case ND_LTE:
        printf("  cmp   r0, r1\n"); // carry set if rhs - lhs is positive
        switch (node->kind) {
        case ND_EQ:
            printf(
                "  moveq r0, #1\n"
                "  movne r0, #0\n");
            break;
        case ND_NEQ:
            printf(
                "  movne r0, #1\n"
                "  moveq r0, #0\n");
            break;
        case ND_LT:
            printf(
                "  movlt r0, #1\n"
                "  movge r0, #0\n");
            break;
        case ND_LTE:
            printf(
                "  movle r0, #1\n"
                "  movgt r0, #0\n");
            break;
        default:
            break;
        }
        return;
    default:
        break;
    }

    error_tok(node->repr, "invalid expression");
}

static void assign_lvar_offsets(Function *prog) {
    int offset = 4;
    for (Obj *var = prog->locals; var; var = var->next) {
        offset += 4;
        var->offset = offset;
    }
    prog->stack_size = align_to(offset, 16);
}

static void gen_stmt(Node *node) {
    switch (node->kind) {
    case ND_IF: {
        int c = count();
        gen_expr(node->condition);
        printf("  cmp   r0, #0\n");
        printf("  beq   main.if.else.%d\n", c);
        gen_stmt(node->consequence);
        printf("  b     main.if.end.%d\n", c);
        printf("main.if.else.%d:\n", c);
        if (node->alternative) {
            gen_stmt(node->alternative);
        }
        printf("main.if.end.%d:\n", c);
        return;
    }
    case ND_LOOP: {
        int c = count();
        if (node->initialize) {
            gen_stmt(node->initialize);
        }
        printf("main.loop.begin.%d:\n", c);
        if (node->condition) {
            gen_expr(node->condition);
            printf("  cmp   r0, #0\n");
            printf("  beq   main.loop.end.%d\n", c);
        }
        gen_stmt(node->consequence);
        if (node->increment) {
            gen_expr(node->increment);
        }
        printf("  b     main.loop.begin.%d\n", c);
        printf("main.loop.end.%d:\n", c);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next) {
            gen_stmt(n);
        }
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("  b     main.return\n");
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    default:
        break;
    }

    error_tok(node->repr, "invalid statement");
}

/*
Generate a subroutine for integer division.

Inputs:
  r0 : dividend
  r1 : divisor

Outputs:
  r0 : quotient
  r1 : remainder

@PERF:
  Check divisor for powers of two and use shifts instead.

References:
  https://www.virag.si/2010/02/simple-division-algorithm-for-arm-assembler/
  http://www.tofla.iconbar.com/tofla/arm/arm02/index.htm
*/
static void gen_div(void) {
    printf("\ndiv:\n"
        "  push  {fp, lr}\n"
        "  add   fp, sp, #4\n");
    printf(
        // check for divide by zero
        // @TODO: jump to some sort of panic routine
        "  cmp   r1, #0\n"
        "  beq   div_end\n"
        "  push  {r0, r1}\n"
        // variables
        "  mov   r0, #0\n"         // quotient
        "  pop   {r1, r2}\n"       // dividend / remainder, divisor
        "  mov   r3, #1\n"         // bit field
        "div_shift:\n"
        // shift divisor left until it exceeds dividend
        // the bit field will be shifted by one less
        "  cmp   r2, r1\n"
        "  lslls r2, r2, #1\n"
        "  lslls r3, r3, #1\n"
        "  bls   div_shift\n"
        "div_sub:\n"
        // cmp sets the carry flag if r1 - r2 is positive, which is weird
        "  cmp   r1, r2\n"
        // subtract divisor from the remainder if it was smaller
        // this also sets the carry flag since the result is positive
        "  subcs r1, r1, r2\n"
        // add bit field to the quotient if the divisor was smaller
        "  addcs r0, r0, r3\n"
        // shift bit field right, setting the carry flag if it underflows
        "  lsrs  r3, r3, #1\n"
        // shift divisor right if bit field has not underflowed
        "  lsrcc r2, r2, #1\n"
        // loop if bit field has not underflowed
        "  bcc   div_sub\n");
    printf(
        "div_end:\n"
        "  sub   sp, fp, #4\n"
        "  pop   {fp, pc}\n");
}

void codegen(Function *prog) {
    assign_lvar_offsets(prog);

    printf(
        ".global main\n\n"
        "main:\n"
        "  push  {fp, lr}\n"
        "  add   fp, sp, #4\n"
        "  sub   sp, sp, #%d\n", prog->stack_size);

    gen_stmt(prog->body);
    assert(depth == 0);

    printf(
        "main.return:\n"
        "  sub   sp, fp, #4\n"
        "  pop   {fp, pc}\n");

    if (contains(prog->body, ND_DIV)) {
        gen_div();
    }
}
