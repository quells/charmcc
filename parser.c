#include "charmcc.h"

// All local variable instances found during parsing.
Obj *locals;

static Node *new_node(NodeKind kind, Token *repr, GC *gc) {
    Node *node = allocate(gc, sizeof(Node));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc node  %p ", node);
    switch (kind) {
    case ND_ADD:
        fprintf(stderr, "add\n");
        break;
    case ND_SUB:
        fprintf(stderr, "sub\n");
        break;
    case ND_MUL:
        fprintf(stderr, "mul\n");
        break;
    case ND_DIV:
        fprintf(stderr, "div\n");
        break;
    case ND_NEG:
        fprintf(stderr, "neg\n");
        break;
    case ND_ADDR:
        fprintf(stderr, "addr\n");
        break;
    case ND_DEREF:
        fprintf(stderr, "deref\n");
        break;
    case ND_EQ:
        fprintf(stderr, "eq\n");
        break;
    case ND_NEQ:
        fprintf(stderr, "neq\n");
        break;
    case ND_LT:
        fprintf(stderr, "lt\n");
        break;
    case ND_LTE:
        fprintf(stderr, "lte\n");
        break;
    case ND_NUM:
        fprintf(stderr, "num\n");
        break;
    case ND_ASSIGN:
        fprintf(stderr, "assign\n");
        break;
    case ND_IF:
        fprintf(stderr, "if\n");
        break;
    case ND_LOOP:
        fprintf(stderr, "loop\n");
        break;
    case ND_RETURN:
        fprintf(stderr, "return\n");
        break;
    case ND_BLOCK:
        fprintf(stderr, "block\n");
        break;
    case ND_EXPR_STMT:
        fprintf(stderr, "expr stmt\n");
        break;
    case ND_VAR:
        fprintf(stderr, "var\n");
        break;
    case ND_FN_CALL:
        fprintf(stderr, "fn call\n");
        break;
    }
    #endif

    node->kind = kind;
    node->repr = repr;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *repr, GC *gc) {
    Node *node = new_node(kind, repr, gc);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *repr, GC *gc) {
    Node *node = new_node(kind, repr, gc);
    node->lhs = expr;
    return node;
}

static Node *new_var(Obj *var, Token *repr, GC *gc) {
    Node *node = new_node(ND_VAR, repr, gc);
    node->var = var;
    return node;
}

static Obj *new_lvar(char *name, Type *type, GC *gc) {
    Obj *var = allocate(gc, sizeof(Obj));

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc obj   %p ", var);
    switch (type->kind) {
    case TY_INT:
        fprintf(stderr, "int\n");
        break;
    case TY_PTR:
        fprintf(stderr, "ptr\n");
        break;
    case TY_FUNC:
        fprintf(stderr, "func\n");
        break;
    case TY_ARRAY:
        fprintf(stderr, "array\n");
        break;
    }
    #endif

    var->name = name;
    var->type = type;
    var->next = locals;
    locals = var;
    return var;
}

static Node *new_num(int val, Token *repr, GC *gc) {
    Node *node = new_node(ND_NUM, repr, gc);
    node->val = val;
    return node;
}

static int get_number(Token *tok) {
    if (tok->kind != TK_NUM) {
        error_tok(tok, "expected a number");
    }
    return tok->val;
}

static char *get_ident(Token *tok) {
    if (tok->kind != TK_IDENT) {
        error_tok(tok, "expected an identifier");
    }
    return strndup(tok->loc, tok->len);
}

static void create_param_lvars(Type *param, GC *gc) {
    if (param) {
        create_param_lvars(param->next, gc);
        new_lvar(get_ident(param->name), param, gc);
    }
}

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

static Type *typespec(Token **rest, Token *tok);
static Type *declarator(Token **rest, Token *tok, Type *type, GC *gc);
static Node *declaration(Token **rest, Token *tok, GC *gc);
static Node *compound_stmt(Token **rest, Token *tok, GC *gc);
static Node *stmt(Token **rest, Token *tok, GC *gc);
static Node *expr_stmt(Token **rest, Token *tok, GC *gc);
static Node *expr(Token **rest, Token *tok, GC *gc);
static Node *assign(Token **rest, Token *tok, GC *gc);
static Node *equality(Token **rest, Token *tok, GC *gc);
static Node *relational(Token **rest, Token *tok, GC *gc);
static Node *add(Token **rest, Token *tok, GC *gc);
static Node *mul(Token **rest, Token *tok, GC *gc);
static Node *postfix(Token **rest, Token *tok, GC *gc);
static Node *unary(Token **rest, Token *tok, GC *gc);
static Node *primary(Token **rest, Token *tok, GC *gc);

// typespec :: "int"
static Type *typespec(Token **rest, Token *tok) {
    *rest = skip(tok, "int");
    return ty_int;
}

// func-params :: param ("," param)*
// param       :: typespec declarator
static Type *func_params(Token **rest, Token *tok, Type *type, GC *gc) {
    Type head = {};
    Type *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        Type *base_type = typespec(&tok, tok);
        Type *t = declarator(&tok, tok, base_type, gc);
        cur = cur->next = copy_type(t, gc);
    }

    type = func_type(type, gc);
    type->params = head.next;
    *rest = tok->next;
    return type;
}

// type-suffix :: "(" func-params
//              | "[" num "]" type-suffix
//              | nothing
static Type *type_suffix(Token **rest, Token *tok, Type *type, GC *gc) {
    if (equal(tok, "(")) {
        return func_params(rest, tok->next, type, gc);
    }

    if (equal(tok, "[")) {
        int size = get_number(tok->next);
        tok = skip(tok->next->next, "]");
        type = type_suffix(rest, tok, type, gc);
        return array_of(type, size, gc);
    }

    *rest = tok;
    return type;
}

// declarator :: "*"* ident type-suffix
static Type *declarator(Token **rest, Token *tok, Type *type, GC *gc) {
    while (consume(&tok, tok, "*")) {
        type = pointer_to(type, gc);
    }

    if (tok->kind != TK_IDENT) {
        error_tok(tok, "expected a variable name");
    }

    type = type_suffix(rest, tok->next, type, gc);
    type->name = tok;
    return type;
}

// declaration :: typespec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **rest, Token *tok, GC *gc) {
    Type *base_type = typespec(&tok, tok);

    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!equal(tok, ";")) {
        if (i++ > 0) {
            tok = skip(tok, ",");
        }

        Type *type = declarator(&tok, tok, base_type, gc);
        Obj *var = new_lvar(get_ident(type->name), type, gc);

        if (!equal(tok, "=")) {
            continue;
        }

        Node *lhs = new_var(var, type->name, gc);
        Node *rhs = assign(&tok, tok->next, gc);
        Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok, gc);
        cur = cur->next = new_unary(ND_EXPR_STMT, node, tok, gc);
    }

    Node *node = new_node(ND_BLOCK, tok, gc);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

// compound-stmt :: (declaration | stmt)* "}"
static Node *compound_stmt(Token **rest, Token *tok, GC *gc) {
    Node *node = new_node(ND_BLOCK, tok, gc);

    Node head = {};
    Node *cur = &head;
    while (!equal(tok, "}")) {
        if (equal(tok, "int")) {
            cur = cur->next = declaration(&tok, tok, gc);
        } else {
            cur = cur->next = stmt(&tok, tok, gc);
        }
        add_type(cur, gc);
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
static Node *stmt(Token **rest, Token *tok, GC *gc) {
    if (equal(tok, "return")) {
        Node *node = new_node(ND_RETURN, tok, gc);
        node->lhs = expr(&tok, tok->next, gc);
        *rest = skip(tok, ";");
        return node;
    }

    if (equal(tok, "if")) {
        Node *node = new_node(ND_IF, tok, gc);
        tok = skip(tok->next, "(");
        node->condition = expr(&tok, tok, gc);
        tok = skip(tok, ")");
        node->consequence = stmt(&tok, tok, gc);
        if (equal(tok, "else")) {
            node->alternative = stmt(&tok, tok->next, gc);
        }
        *rest = tok;
        return node;
    }

    if (equal(tok, "for")) {
        Node *node = new_node(ND_LOOP, tok, gc);
        tok = skip(tok->next, "(");

        node->initialize = expr_stmt(&tok, tok, gc);

        if (!equal(tok, ";")) {
            node->condition = expr(&tok, tok, gc);
        }
        tok = skip(tok, ";");

        if (!equal(tok, ")")) {
            node->increment = expr(&tok, tok, gc);
        }
        tok = skip(tok, ")");

        node->consequence = stmt(rest, tok, gc);
        return node;
    }

    if (equal(tok, "while")) {
        Node *node = new_node(ND_LOOP, tok, gc);
        tok = skip(tok->next, "(");
        node->condition = expr(&tok, tok, gc);
        tok = skip(tok, ")");
        node->consequence = stmt(rest, tok, gc);
        return node;
    }

    if (equal(tok, "{")) {
        return compound_stmt(rest, tok->next, gc);
    }

    return expr_stmt(rest, tok, gc);
}

// expr-stmt :: expr? ";"
static Node *expr_stmt(Token **rest, Token *tok, GC *gc) {
    if (equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok, gc);
    }

    Node *node = new_node(ND_EXPR_STMT, tok, gc);
    node->lhs = expr(&tok, tok, gc);
    *rest = skip(tok, ";");
    return node;
}

// expr :: assign
static Node *expr(Token **rest, Token *tok, GC *gc) {
    return assign(rest, tok, gc);
}

// assign :: equality ("=" assign)?
static Node *assign(Token **rest, Token *tok, GC *gc) {
    Node *node = equality(&tok, tok, gc);
    if (equal(tok, "=")) {
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next, gc), tok, gc);
    }
    *rest = tok;
    return node;
}

// equality :: relational ("==" relational | "!=" relational)*
static Node *equality(Token **rest, Token *tok, GC *gc) {
    Node *node = relational(&tok, tok, gc);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next, gc), start, gc);
            continue;
        }

        if (equal(tok, "!=")) {
            node = new_binary(ND_NEQ, node, relational(&tok, tok->next, gc), start, gc);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// relational :: add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **rest, Token *tok, GC *gc) {
    Node *node = add(&tok, tok, gc);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "<")) {
            node = new_binary(ND_LT, node, add(&tok, tok->next, gc), start, gc);
            continue;
        }

        if (equal(tok, "<=")) {
            node = new_binary(ND_LTE, node, add(&tok, tok->next, gc), start, gc);
            continue;
        }

        if (equal(tok, ">")) {
            node = new_binary(ND_LT, add(&tok, tok->next, gc), node, start, gc);
            continue;
        }

        if (equal(tok, ">=")) {
            node = new_binary(ND_LTE, add(&tok, tok->next, gc), node, start, gc);
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
static Node *new_add(Node *lhs, Node *rhs, Token *tok, GC *gc) {
    add_type(lhs, gc);
    add_type(rhs, gc);

    if (is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_binary(ND_ADD, lhs, rhs, tok, gc);
    }

    if (lhs->type->base && rhs->type->base) {
        error_tok(tok, "invalid operands");
    }

    if (!lhs->type->base && rhs->type->base) {
        Node *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    rhs = new_binary(ND_MUL, rhs, new_num(lhs->type->base->size, tok, gc), tok, gc);
    return new_binary(ND_ADD, lhs, rhs, tok, gc);
}

/*
`-` performs pointer arithmetic if the arguments include a pointer.

Moves the pointer by the size of the elements, not bytes.
p-n :: p - sizeof(*p)*n

The distance between two pointers, in units of the size of the elements.
p-q :: (p<--->q) / sizeof(*p)
*/
static Node *new_sub(Node *lhs, Node *rhs, Token *tok, GC *gc) {
    add_type(lhs, gc);
    add_type(rhs, gc);

    if (is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_binary(ND_SUB, lhs, rhs, tok, gc);
    }

    if (lhs->type->base && is_integer(rhs->type)) {
        rhs = new_binary(ND_MUL, rhs, new_num(lhs->type->base->size, tok, gc), tok, gc);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok, gc);
        node->type = lhs->type;
        return node;
    }

    if (lhs->type->base && rhs->type->base) {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok, gc);
        node->type = ty_int;
        return new_binary(ND_DIV, node, new_num(lhs->type->base->size, tok, gc), tok, gc);
    }

    error_tok(tok, "invalid operands");
    return NULL;
}

// add :: mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok, GC *gc) {
    Node *node = mul(&tok, tok, gc);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "+")) {
            node = new_add(node, mul(&tok, tok->next, gc), start, gc);
            continue;
        }

        if (equal(tok, "-")) {
            node = new_sub(node, mul(&tok, tok->next, gc), start, gc);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// mul :: unary ("*" unary | "/" unary)*
static Node *mul(Token **rest, Token *tok, GC *gc) {
    Node *node = unary(&tok, tok, gc);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "*")) {
            node = new_binary(ND_MUL, node, unary(&tok, tok->next, gc), start, gc);
            continue;
        }

        if (equal(tok, "/")) {
            node = new_binary(ND_DIV, node, unary(&tok, tok->next, gc), start, gc);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// unary :: ("+" | "-" | "&" | "*") unary
//        | postfix
static Node *unary(Token **rest, Token *tok, GC *gc) {
    if (equal(tok, "+")) {
        return unary(rest, tok->next, gc);
    }

    if (equal(tok, "-")) {
        return new_unary(ND_NEG, unary(rest, tok->next, gc), tok, gc);
    }

    if (equal(tok, "&")) {
        return new_unary(ND_ADDR, unary(rest, tok->next, gc), tok, gc);
    }

    if (equal(tok, "*")) {
        return new_unary(ND_DEREF, unary(rest, tok->next, gc), tok, gc);
    }

    return postfix(rest, tok, gc);
}

// postfix = primary ("[" expr "]")*
static Node *postfix(Token **rest, Token *tok, GC *gc) {
    Node *node = primary(&tok, tok, gc);

    while (equal(tok, "[")) {
        // x[y] is sugar for *(x + y)
        Token *start = tok;
        Node *idx = expr(&tok, tok->next, gc);
        tok = skip(tok, "]");
        node = new_unary(ND_DEREF, new_add(node, idx, start, gc), start, gc);
    }

    *rest = tok;
    return node;
}

// fn-call :: ident "(" (assign, ("," assign)*)? ")"
static Node *fn_call(Token **rest, Token *tok, GC *gc) {
    Token *start = tok;
    tok = tok->next->next;

    Node head = {};
    Node *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        cur = cur->next = assign(&tok, tok, gc);
    }

    *rest = skip(tok, ")");

    Node *node = new_node(ND_FN_CALL, start, gc);
    node->func = strndup(start->loc, start->len);
    node->args = head.next;
    return node;
}

// primary :: "(" expr ")"
//          | "sizeof" unary
//          | ident fn-args?
//          | num
static Node *primary(Token **rest, Token *tok, GC *gc) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next, gc);
        *rest = skip(tok, ")");
        return node;
    }

    if (equal(tok, "sizeof")) {
        Node *node = unary(rest, tok->next, gc);
        add_type(node, gc);
        return new_num(node->type->size, tok, gc);
    }

    if (tok->kind == TK_IDENT) {
        if (equal(tok->next, "(")) {
            return fn_call(rest, tok, gc);
        }

        Obj *var = find_var(tok);
        if (!var) {
            error_tok(tok, "undefined variable");
        }
        *rest = tok->next;
        return new_var(var, tok, gc);
    }

    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val, tok, gc);
        *rest = tok->next;
        return node;
    }

    error_tok(tok, "expected an expression");
    return NULL;
}

// function-definition :: stmt*
static Function *function(Token **rest, Token *tok, GC *gc) {
    Type *type = typespec(&tok, tok);
    type = declarator(&tok, tok, type, gc);

    locals = NULL;

    Function *fn = allocate(gc, sizeof(Function));
    fn->name = get_ident(type->name);
    fn->type = type;
    create_param_lvars(type->params, gc);
    fn->params = locals;

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc func  %p %s\n", fn, fn->name);
    #endif

    tok = skip(tok, "{");
    fn->body = compound_stmt(rest, tok, gc);
    fn->locals = locals;
    return fn;
}

// program :: function-definition*
Function *parse(Token *tok, GC *gc) {
    Function head = {};
    Function *cur = &head;

    while (tok->kind != TK_EOF) {
        cur = cur->next = function(&tok, tok, gc);
    }

    return head.next;
}
