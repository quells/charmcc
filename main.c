#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------
== Lexer ==
---------*/

typedef enum {
    TK_RESERVED, // Keywords or punctuation
    TK_NUM,      // Numeric literals
    TK_EOF,      // End-of-file
} TokenKind;

// Token type
typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int val;    // If kind is TK_NUM, the value is stored here
    char *loc;  // Token location in input
    int len;    // Token length
};

// Input string
static char *current_input;

// Reports an error an exits.
static void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Reports an error location and exits.
static void verror_at(char *loc, char *fmt, va_list ap) {
    int pos = loc - current_input;
    fprintf(stderr, "%s\n", current_input);
    fprintf(stderr, "%*s", pos, ""); // print spaces for pos
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

static void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

static void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->loc, fmt, ap);
}

// Consumes the current token if it matches `s`.
static bool equal(Token *tok, char *s) {
    return strlen(s) == tok->len &&
           !strncmp(tok->loc, s, tok->len);
}

// Ensure that the current token is `s`.
static Token *skip(Token *tok, char *s) {
    if (!equal(tok, s)) {
        error_tok(tok, "expected '%s'", s);
    }
    return tok->next;
}

// Ensure that the current token is TK_NUM.
static int get_number(Token *tok) {
    if (tok->kind != TK_NUM) {
        error_tok(tok, "expected a number");
    }
    return tok->val;
}

// Create a token.
static Token *new_token(TokenKind kind, char *start, char *end) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->loc = start;
    tok->len = end - start;
    return tok;
}

// Create linked list of tokens from program source.
static Token *tokenize(void) {
    char *p = current_input;
    Token head = {};
    Token *cur = &head;

    while (*p) {
        // Skip whitespace
        if (isspace(*p)) {
            p++;
            continue;
        }

        // Numeric literal
        if (isdigit(*p)) {
            cur = cur->next = new_token(TK_NUM, p, p);
            char *q = p;
            cur->val = strtoul(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        // Punctuation
        if (ispunct(*p)) {
            cur = cur->next = new_token(TK_RESERVED, p, p+1);
            p++;
            continue;
        }

        error_at(p, "invalid token");
    }

    cur = cur->next = new_token(TK_EOF, p, p);
    return head.next;
}

/*----------
== Parser ==
----------*/

typedef enum {
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_NUM, // Integer
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    int val;  // Only if kind == ND_NUM
};

static Node *new_node(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_num(int val) {
    Node *node = new_node(ND_NUM);
    node->val = val;
    return node;
}

static Node *expr(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// expr :: mul ("+" mul | "-" mul)*
static Node *expr(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        if (equal(tok, "+")) {
            node = new_binary(ND_ADD, node, mul(&tok, tok->next));
            continue;
        }

        if (equal(tok, "-")) {
            node = new_binary(ND_SUB, node, mul(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
}

// mul :: primary ("*" primary | "/" primary)*
static Node *mul(Token **rest, Token *tok) {
    Node *node = primary(&tok, tok);

    for (;;) {
        if (equal(tok, "*")) {
            node = new_binary(ND_MUL, node, primary(&tok, tok->next));
            continue;
        }

        if (equal(tok, "/")) {
            node = new_binary(ND_DIV, node, primary(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
}

// primary :: "(" expr ")" | num
static Node *primary(Token **rest, Token *tok) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val);
        *rest = tok->next;
        return node;
    }

    error_tok(tok, "expected an expression");
}

/*------------
== Code Gen ==
------------*/

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
    if (node->kind == ND_NUM) {
        printf("  mov   r0, #%d\n", node->val);
        return;
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
        // cmp sets the carry flag if the r1 - r2 is positive, which is weird
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

/*--------
== Main ==
--------*/

int main(int argc, char **argv) {
    if (argc != 2) {
        error("%s: invalid number of arguments\n", argv[0]);
    }

    current_input = argv[1];
    Token *tok = tokenize();
    Node *node = expr(&tok, tok);

    if (tok->kind != TK_EOF) {
        error_tok(tok, "extra token");
    }

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

    return 0;
}
