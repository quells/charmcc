#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PTR_SIZE 4

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

// Local variable
typedef struct Obj Obj;
struct Obj {
    Obj *next;
    char *name;
    Type *type;
    int offset; // Offset from frame pointer
};

typedef struct Function Function;
struct Function {
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

Function *parse(Token *tok);
void free_ast(Function *prog);

/*----------
Type Checker
----------*/

typedef enum {
    TY_INT,
    TY_PTR,
} TypeKind;

struct Type {
    TypeKind kind;
    Type *base;
    Token *name;
};

extern Type *ty_int;

bool is_integer(Type *type);
Type *pointer_to(Type *base);
void add_type(Node *node);
void free_type(Type *type);

/*------------
== Code Gen ==
------------*/

void codegen(Function *prog);

void debug_ast(Function *prog);
