#include "charmcc.h"

// All local variable instances found during parsing.
Obj *locals;

static Node *new_node(NodeKind kind, Token *repr) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->repr = repr;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *repr) {
    Node *node = new_node(kind, repr);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *repr) {
    Node *node = new_node(kind, repr);
    node->lhs = expr;
    return node;
}

static Node *new_var(Obj *var, Token *repr) {
    Node *node = new_node(ND_VAR, repr);
    node->var = var;
    return node;
}

static Obj *new_lvar(char *name) {
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;
    var->next = locals;
    locals = var;
    return var;
}

static Node *new_num(int val, Token *repr) {
    Node *node = new_node(ND_NUM, repr);
    node->val = val;
    return node;
}

static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// Find local variable by name.
static Obj *find_var(Token *tok) {
    for (Obj *var = locals; var; var = var->next) {
        if (strlen(var->name) == tok->len &&
            !strncmp(tok->loc, var->name, tok->len)) {
            return var;
        }
    }
    return NULL;
}

// compound-stmt :: stmt* "}"
static Node *compound_stmt(Token **rest, Token *tok) {
    Node *node = new_node(ND_BLOCK, tok);

    Node head = {};
    Node *cur = &head;
    while (!equal(tok, "}")) {
        cur = cur->next = stmt(&tok, tok);
        add_type(cur);
    }

    node->body = head.next;
    *rest = tok->next;
    return node;
}

// stmt :: "return" expr ";"
//       | "if" "(" expr ")" stmt ("else" stmt)?
//       | "for" "(" expr-stmt expr? ";" expr? ")" stmt
//       | "while" "(" expr ")" stmt
//       | "{" compound-stmt
//       | expr-stmt
static Node *stmt(Token **rest, Token *tok) {
    if (equal(tok, "return")) {
        Node *node = new_node(ND_RETURN, tok);
        node->lhs = expr(&tok, tok->next);
        *rest = skip(tok, ";");
        return node;
    }

    if (equal(tok, "if")) {
        Node *node = new_node(ND_IF, tok);
        tok = skip(tok->next, "(");
        node->condition = expr(&tok, tok);
        tok = skip(tok, ")");
        node->consequence = stmt(&tok, tok);
        if (equal(tok, "else")) {
            node->alternative = stmt(&tok, tok->next);
        }
        *rest = tok;
        return node;
    }

    if (equal(tok, "for")) {
        Node *node = new_node(ND_LOOP, tok);
        tok = skip(tok->next, "(");

        node->initialize = expr_stmt(&tok, tok);

        if (!equal(tok, ";")) {
            node->condition = expr(&tok, tok);
        }
        tok = skip(tok, ";");

        if (!equal(tok, ")")) {
            node->increment = expr(&tok, tok);
        }
        tok = skip(tok, ")");

        node->consequence = stmt(rest, tok);
        return node;
    }

    if (equal(tok, "while")) {
        Node *node = new_node(ND_LOOP, tok);
        tok = skip(tok->next, "(");
        node->condition = expr(&tok, tok);
        tok = skip(tok, ")");
        node->consequence = stmt(rest, tok);
        return node;
    }

    if (equal(tok, "{")) {
        return compound_stmt(rest, tok->next);
    }

    return expr_stmt(rest, tok);
}

// expr-stmt :: expr? ";"
static Node *expr_stmt(Token **rest, Token *tok) {
    if (equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }

    Node *node = new_node(ND_EXPR_STMT, tok);
    node->lhs = expr(&tok, tok);
    *rest = skip(tok, ";");
    return node;
}

// expr :: assign
static Node *expr(Token **rest, Token *tok) {
    return assign(rest, tok);
}

// assign :: equality ("=" assign)?
static Node *assign(Token **rest, Token *tok) {
    Node *node = equality(&tok, tok);
    if (equal(tok, "=")) {
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next), tok);
    }
    *rest = tok;
    return node;
}

// equality :: relational ("==" relational | "!=" relational)*
static Node *equality(Token **rest, Token *tok) {
    Node *node = relational(&tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next), start);
            continue;
        }

        if (equal(tok, "!=")) {
            node = new_binary(ND_NEQ, node, relational(&tok, tok->next), start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// relational :: add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **rest, Token *tok) {
    Node *node = add(&tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "<")) {
            node = new_binary(ND_LT, node, add(&tok, tok->next), start);
            continue;
        }

        if (equal(tok, "<=")) {
            node = new_binary(ND_LTE, node, add(&tok, tok->next), start);
            continue;
        }

        if (equal(tok, ">")) {
            node = new_binary(ND_LT, add(&tok, tok->next), node, start);
            continue;
        }

        if (equal(tok, ">=")) {
            node = new_binary(ND_LTE, add(&tok, tok->next), node, start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

/*
`+` performs pointer arithmetic if the arguments include a pointer.

Moves the pointer by the size of the elements, not bytes.
p+n :: p + sizeof(*p)*n
*/
static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_binary(ND_ADD, lhs, rhs, tok);
    }

    if (lhs->type->base && rhs->type->base) {
        error_tok(tok, "invalid operands");
    }

    if (!lhs->type->base && rhs->type->base) {
        Node *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    rhs = new_binary(ND_MUL, rhs, new_num(PTR_SIZE, tok), tok);
    return new_binary(ND_ADD, lhs, rhs, tok);
}

/*
`-` performs pointer arithmetic if the arguments include a pointer.

Moves the pointer by the size of the elements, not bytes.
p-n :: p - sizeof(*p)*n

The distance between two pointers, in units of the size of the elements.
p-q :: (p<--->q) / sizeof(*p)
*/
static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_binary(ND_SUB, lhs, rhs, tok);
    }

    if (lhs->type->base && is_integer(rhs->type)) {
        rhs = new_binary(ND_MUL, rhs, new_num(PTR_SIZE, tok), tok);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->type = lhs->type;
        return node;
    }

    if (lhs->type->base && rhs->type->base) {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->type = ty_int;
        return new_binary(ND_DIV, node, new_num(PTR_SIZE, tok), tok);
    }

    error_tok(tok, "invalid operands");
}

// add :: mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "+")) {
            node = new_add(node, mul(&tok, tok->next), start);
            continue;
        }

        if (equal(tok, "-")) {
            node = new_sub(node, mul(&tok, tok->next), start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// mul :: unary ("*" unary | "/" unary)*
static Node *mul(Token **rest, Token *tok) {
    Node *node = unary(&tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "*")) {
            node = new_binary(ND_MUL, node, unary(&tok, tok->next), start);
            continue;
        }

        if (equal(tok, "/")) {
            node = new_binary(ND_DIV, node, unary(&tok, tok->next), start);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// unary :: ("+" | "-" | "&" | "*") unary
//        | primary
static Node *unary(Token **rest, Token *tok) {
    if (equal(tok, "+")) {
        return unary(rest, tok->next);
    }

    if (equal(tok, "-")) {
        return new_unary(ND_NEG, unary(rest, tok->next), tok);
    }

    if (equal(tok, "&")) {
        return new_unary(ND_ADDR, unary(rest, tok->next), tok);
    }

    if (equal(tok, "*")) {
        return new_unary(ND_DEREF, unary(rest, tok->next), tok);
    }

    return primary(rest, tok);
}

// primary :: "(" expr ")"
//          | ident
//          | num
static Node *primary(Token **rest, Token *tok) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if (tok->kind == TK_IDENT) {
        Obj *var = find_var(tok);
        if (!var) {
            var = new_lvar(strndup(tok->loc, tok->len));
        }
        *rest = tok->next;
        return new_var(var, tok);
    }

    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val, tok);
        *rest = tok->next;
        return node;
    }

    error_tok(tok, "expected an expression");
    return NULL;
}

// program :: stmt*
Function *parse(Token *tok) {
    tok = skip(tok, "{");

    Function *prog = calloc(1, sizeof(Function));
    prog->body = compound_stmt(&tok, tok);
    prog->locals = locals;
    return prog;
}

void free_obj(Obj *o) {
    if (o == NULL) return;

    free_obj(o->next);
    free(o->name);
    free(o);
}

void free_node(Node *n) {
    if (n == NULL) return;

    free_node(n->next);
    free_type(n->type);

    free_node(n->lhs);
    free_node(n->rhs);

    free_node(n->condition);
    free_node(n->consequence);
    free_node(n->alternative);
    free_node(n->initialize);
    free_node(n->increment);

    free_node(n->body);

    free(n);
}

void free_ast(Function *prog) {
    free_obj(prog->locals);
    free_node(prog->body);
    free(prog);
}
