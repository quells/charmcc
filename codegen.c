#include "charmcc.h"

static int depth;

/*
Generate a function prologue.
Pushes the frame pointer onto the stack, sets it to the stack pointer, then allocates some stack buffer.
The amount of buffer is 4 + 4*reg_count, since each register takes up 4 bytes.
Registers are saved starting with r0 at [fp, #-8] and extending downwards.
*/
static void gen_prologue(int reg_count) {
    int buf = 4 + 4*reg_count;
    printf(
        "  push  {fp}\n"
        "  add   fp, sp, #0\n"
        "  sub   sp, sp, #%d\n", buf);
    for (int i = 0; i < reg_count; i++) {
        printf("  str   r%d, [fp, #-%d]\n", i, 8 + 4*i);
    }
}

/*
Generate a function epilogue.
Restores the stack to the beginning of the frame, pops the previous frame pointer, and returns to the caller.
*/
static void gen_epilogue(void) {
    printf(
        "  add   sp, fp, #0\n"
        "  pop   {fp}\n"
        "  bx    lr\n");
}

static void push(void) {
    printf("  push  {r0}\n");
    depth++;
}

static void pop(char *arg) {
    printf("  pop   {%s}\n", arg);
    depth--;
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

    return contains(node->lhs, kind) || contains(node->rhs, kind);
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

    error("invalid expression");
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
    printf("\ndiv:\n");
    gen_prologue(4);
    printf(
        // check for divide by zero
        // @TODO: jump to some sort of panic routine
        "  cmp   r1, #0\n"
        "  beq   div_end\n"
        // variables
        "  mov   r0, #0\n"         // quotient
        "  ldr   r1, [fp, #-8]\n"  // dividend / remainder
        "  ldr   r2, [fp, #-12]\n" // divisor
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
        "  ldr   r2, [fp, #-16]\n"
        "  ldr   r3, [fp, #-20]\n");
    gen_epilogue();
}

void codegen(Node *node) {
    printf(
        ".global main\n\n"
        "main:\n"
        "  push  {fp, lr}\n"
        "  add   fp, sp, #4\n");
    gen_expr(node);
    printf(
        "  sub   sp, fp, #4\n"
        "  pop   {fp, lr}\n"
        "  bx    lr\n");

    assert(depth == 0);

    if (contains(node, ND_DIV)) {
        gen_div();
    }
}
