#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        if (*p == '+' || *p == '-') {
            cur = cur->next = new_token(TK_RESERVED, p, p+1);
            p++;
            continue;
        }

        error_at(p, "invalid token");
    }

    cur = cur->next = new_token(TK_EOF, p, p);
    return head.next;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        error("%s: invalid number of arguments\n", argv[0]);
    }

    current_input = argv[1];
    Token *tok = tokenize();

    printf(".global main\n\n");
    printf("main:\n");

    // The first token must be a number ...
    printf("  MOV   r0, #%d\n", get_number(tok));
    tok = tok->next;

    // ... followed by either `+ <number>` or `- <number>`.
    while (tok->kind != TK_EOF) {
        if (equal(tok, "+")) {
            printf("  ADD   r0, r0, #%d\n", get_number(tok->next));
            tok = tok->next->next;
            continue;
        }

        tok = skip(tok, "-");
        printf("  SUB   r0, r0, #%d\n", get_number(tok));
        tok = tok->next;
    }

    printf("  BX    lr\n");

    return 0;
}
