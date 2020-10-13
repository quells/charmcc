#include "charmcc.h"

// Input string
static char *current_input;

// Reports an error and exits.
void error(char *fmt, ...) {
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

void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->loc, fmt, ap);
}

// Consumes the current token if it matches `s`.
bool equal(Token *tok, char *s) {
    return strlen(s) == tok->len &&
           !strncmp(tok->loc, s, tok->len);
}

// Ensure that the current token is `s`.
Token *skip(Token *tok, char *s) {
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

static bool startswith(char *p, char *q) {
    return strncmp(p, q, strlen(q)) == 0;
}

// Is `c` a valid head character for an identifier?
static bool is_ident_head(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

// Is `c` a valid tail character for an identifier?
static bool is_ident_tail(char c) {
    return is_ident_head(c) || ('0' <= c && c <= '9');
}

static bool is_keyword(Token *tok) {
    static char *kw[] = {"return", "if", "else", "for"};
    for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
        if (equal(tok, kw[i])) {
            return true;
        }
    }
    return false;
}

static void convert_keywords(Token *tok) {
    for (Token *t = tok; t->kind != TK_EOF; t = t->next) {
        if (is_keyword(t)) {
            t->kind = TK_RESERVED;
        }
    }
}

// Create linked list of tokens from program source.
Token *tokenize(char *p) {
    current_input = p;
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

        // Identifier or keyword
        if (is_ident_head(*p)) {
            char *start = p;
            do {
                p++;
            } while (is_ident_tail(*p));
            cur = cur->next = new_token(TK_IDENT, start, p);
            continue;
        }

        // Punctuation
        if (startswith(p, "==") || startswith(p, "!=") ||
            startswith(p, "<=") || startswith(p, ">=")) {
            cur = cur->next = new_token(TK_RESERVED, p, p+2);
            p += 2;
            continue;
        }
        if (ispunct(*p)) {
            cur = cur->next = new_token(TK_RESERVED, p, p+1);
            p++;
            continue;
        }

        error_at(p, "invalid token");
    }

    cur = cur->next = new_token(TK_EOF, p, p);
    convert_keywords(head.next);
    return head.next;
}
