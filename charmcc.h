#define _POSIX_C_SOURCE 200809L
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
Token *tokenize(char *input);

/*----------
== Parser ==
----------*/

typedef enum {
    ND_ADD,    // +
    ND_SUB,    // -
    ND_MUL,    // *
    ND_DIV,    // /
    ND_NEG,    // unary -
    ND_EQ,     // ==
    ND_NEQ,    // !=
    ND_LT,     // <
    ND_LTE,    // <=
    ND_NUM,    // Integer
    ND_ASSIGN, // =
    ND_EXPR_STMT,
    ND_VAR,
} NodeKind;

typedef struct Node Node;

// Local variable
typedef struct Obj Obj;
struct Obj {
    Obj *next;
    char *name;
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
    Node *lhs;
    Node *rhs;
    Obj *var; // Only if kind == ND_VAR
    int val;  // Only if kind == ND_NUM
};

Function *parse(Token *tok);

/*------------
== Code Gen ==
------------*/

void codegen(Function *prog);
