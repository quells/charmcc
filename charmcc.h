#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PTR_SIZE 4
#define DEBUG_ALLOCS 0

// A linked list where the head keeps track of the tail for fast append.
typedef struct StringList StringList;
struct StringList {
    StringList *next;
    StringList *tail;
    char *s;
};

static StringList *append_str(StringList *head, char *str) {
    StringList *next = calloc(1, sizeof(StringList));
    next->s = str;

    if (head == NULL) {
        head = next;
        return head;
    }

    if (head->tail == NULL) {
        head->next = next;
    } else {
        head->tail->next = next;
    }
    head->tail = next;

    return head;
}

static void free_string_list(StringList *list) {
    if (list == NULL) return;
    free_string_list(list->next);
    free(list);
}

/*---------
== Lexer ==
---------*/

typedef enum {
    TK_IDENT,    // Identifiers
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

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *tokenize(char *input);
void free_tokens(Token *tok);

/*---------------------
== Memory Management ==
---------------------*/

typedef struct MemManager MemManager;
struct MemManager {
    MemManager *next;
    MemManager *tail;
    void *obj;
};

MemManager *new_memmanager();
void register_obj(MemManager *mm, void *obj);
void *allocate(MemManager *mm, size_t size);
void cleanup(MemManager *mm);

/*----------
== Parser ==
----------*/

typedef enum {
    ND_ADD,    // +
    ND_SUB,    // -
    ND_MUL,    // *
    ND_DIV,    // /
    ND_NEG,    // unary -
    ND_ADDR,   // unary &
    ND_DEREF,  // unary *
    ND_EQ,     // ==
    ND_NEQ,    // !=
    ND_LT,     // <
    ND_LTE,    // <=
    ND_NUM,    // Integer

    ND_ASSIGN, // =
    ND_IF,     // if
    ND_LOOP,   // for, while
    ND_RETURN, // return

    ND_BLOCK,
    ND_EXPR_STMT,
    ND_VAR,
    ND_FN_CALL,
} NodeKind;

typedef struct Type Type;
typedef struct Node Node;

// Variable or function
typedef struct Obj Obj;
struct Obj {
    Obj *next;
    char *name;
    Type *type;
    bool is_local;
    bool is_function;

    // Variable
    int offset; // Offset from frame pointer

    // Function
    Obj *params;
    Node *body;
    Obj *locals;
    int stack_size;
};

struct Node {
    NodeKind kind;
    Node *next;
    Type *type;
    Token *repr;

    Node *lhs;
    Node *rhs;

    // Only if kind == ND_IF || ND_LOOP
    Node *condition;
    Node *consequence;
    Node *alternative;
    Node *initialize;
    Node *increment;

    // Only if kind == ND_FN_CALL
    char *func;
    Node *args;

    Node *body; // Only if kind == ND_BLOCK
    Obj *var;   // Only if kind == ND_VAR
    int val;    // Only if kind == ND_NUM
};

Obj *parse(Token *tok, MemManager *mm);

/*----------
Type Checker
----------*/

typedef enum {
    TY_INT,
    TY_PTR,
    TY_ARRAY,
    TY_FUNC,
} TypeKind;

struct Type {
    TypeKind kind;

    // sizeof() value
    int size;

    Token *name;

    // Only if kind == TY_PTR or TY_ARRAY
    Type *base;

    // Only if kind == TY_ARRAY
    int array_len;

    // Only if kind == TY_FUNC
    Type *return_type;
    Type *params;
    Type *next;
};

extern Type *ty_int;

bool is_integer(Type *type);
Type *copy_type(Type *type, MemManager *mm);
Type *pointer_to(Type *base, MemManager *mm);
Type *func_type(Type *return_type, MemManager *mm);
Type *array_of(Type *base, int size, MemManager *mm);
void add_type(Node *node, MemManager *mm);

/*------------
== Code Gen ==
------------*/

typedef enum {
    IR_LABEL,

    IR_ALLOC,
    IR_STORE,
    IR_LOAD,
    IR_ASSIGN,
    IR_PUSH,
    IR_POP,
    IR_PROLOGUE,
    IR_EPILOGUE,
    IR_RETURN,

    IR_NEG,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,

    IR_CMP,
    IR_EQ,
    IR_NEQ,
    IR_LT,
    IR_LTE,

    IR_BRANCH,
    IR_BRANCH_EQ,
    IR_BRANCH_NEQ,
    IR_BRANCH_LT,
    IR_BRANCH_LTE
} IRKind;

typedef struct IRInstruction IRInstruction;
struct IRInstruction {
    IRKind kind;
    IRInstruction *next;

    char *target;
    int r, a, b, imm;
};

typedef struct IR IR;
struct IR {
    StringList *vars;
    StringList *funcs;
    IRInstruction *instructions;
};

IR *codegen_ir(Obj *prog);
void codegen_32(IR *prog);
void free_ir(IR *prog);

void debug_ast(Obj *prog);
void debug_ir(IR *prog);
