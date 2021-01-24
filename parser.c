#include "charmcc.h"

// All local variable instances found during parsing.
Obj *locals;
Obj *globals;

static Node *new_node(NodeKind kind, Token *repr, MemManager *mm) {
    Node *node = allocate(mm, sizeof(Node));

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

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *repr, MemManager *mm) {
    Node *node = new_node(kind, repr, mm);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *repr, MemManager *mm) {
    Node *node = new_node(kind, repr, mm);
    node->lhs = expr;
    return node;
}

static Node *new_var(Obj *var, Token *repr, MemManager *mm) {
    Node *node = new_node(ND_VAR, repr, mm);
    node->var = var;
    return node;
}

static Obj *new_obj(char *name, Type *type, MemManager *mm) {
    Obj *var = allocate(mm, sizeof(Obj));

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
    return var;
}

static Obj *new_lvar(char *name, Type *type, MemManager *mm) {
    Obj *var = new_obj(name, type, mm);
    var->is_local = true;
    var->next = locals;
    locals = var;
    return var;
}

static Obj *new_gvar(char *name, Type *type, MemManager *mm) {
    Obj *var = new_obj(name, type, mm);
    var->next = globals;
    globals = var;
    return var;
}

static Node *new_num(int val, Token *repr, MemManager *mm) {
    Node *node = new_node(ND_NUM, repr, mm);
    node->val = val;
    return node;
}

static int get_number(Token *tok) {
    if (tok->kind != TK_NUM) {
        error_tok(tok, "expected a number");
    }
    return tok->val;
}

static char *get_ident(Token *tok, MemManager *mm) {
    if (tok->kind != TK_IDENT) {
        error_tok(tok, "expected an identifier");
    }
    char *copy = strndup(tok->loc, tok->len);
    register_obj(mm, copy);
    return copy;
}

static void create_param_lvars(Type *param, MemManager *mm) {
    if (param) {
        create_param_lvars(param->next, mm);
        new_lvar(get_ident(param->name, mm), param, mm);
    }
}

// Find variable by name.
static Obj *find_var(Token *tok) {
    for (Obj *var = locals; var; var = var->next) {
        if (strlen(var->name) == tok->len &&
            !strncmp(tok->loc, var->name, tok->len)) {
            return var;
        }
    }
    for (Obj *var = globals; var; var = var->next) {
        if (strlen(var->name) == tok->len &&
            !strncmp(tok->loc, var->name, tok->len)) {
            return var;
        }
    }
    return NULL;
}

static Type *typespec(Token **rest, Token *tok);
static Type *declarator(Token **rest, Token *tok, Type *type, MemManager *mm);
static Node *declaration(Token **rest, Token *tok, MemManager *mm);
static Node *compound_stmt(Token **rest, Token *tok, MemManager *mm);
static Node *stmt(Token **rest, Token *tok, MemManager *mm);
static Node *expr_stmt(Token **rest, Token *tok, MemManager *mm);
static Node *expr(Token **rest, Token *tok, MemManager *mm);
static Node *assign(Token **rest, Token *tok, MemManager *mm);
static Node *equality(Token **rest, Token *tok, MemManager *mm);
static Node *relational(Token **rest, Token *tok, MemManager *mm);
static Node *add(Token **rest, Token *tok, MemManager *mm);
static Node *mul(Token **rest, Token *tok, MemManager *mm);
static Node *postfix(Token **rest, Token *tok, MemManager *mm);
static Node *unary(Token **rest, Token *tok, MemManager *mm);
static Node *primary(Token **rest, Token *tok, MemManager *mm);

// typespec :: "int"
static Type *typespec(Token **rest, Token *tok) {
    *rest = skip(tok, "int");
    return ty_int;
}

// func-params :: param ("," param)*
// param       :: typespec declarator
static Type *func_params(Token **rest, Token *tok, Type *type, MemManager *mm) {
    Type head = {};
    Type *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        Type *base_type = typespec(&tok, tok);
        Type *t = declarator(&tok, tok, base_type, mm);
        cur = cur->next = copy_type(t, mm);
    }

    type = func_type(type, mm);
    type->params = head.next;
    *rest = tok->next;
    return type;
}

// type-suffix :: "(" func-params
//              | "[" num "]" type-suffix
//              | nothing
static Type *type_suffix(Token **rest, Token *tok, Type *type, MemManager *mm) {
    if (equal(tok, "(")) {
        return func_params(rest, tok->next, type, mm);
    }

    if (equal(tok, "[")) {
        int size = get_number(tok->next);
        tok = skip(tok->next->next, "]");
        type = type_suffix(rest, tok, type, mm);
        return array_of(type, size, mm);
    }

    *rest = tok;
    return type;
}

// declarator :: "*"* ident type-suffix
static Type *declarator(Token **rest, Token *tok, Type *type, MemManager *mm) {
    while (consume(&tok, tok, "*")) {
        type = pointer_to(type, mm);
    }

    if (tok->kind != TK_IDENT) {
        error_tok(tok, "expected a variable name");
    }

    type = type_suffix(rest, tok->next, type, mm);
    type->name = tok;
    return type;
}

// declaration :: typespec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **rest, Token *tok, MemManager *mm) {
    Type *base_type = typespec(&tok, tok);

    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!equal(tok, ";")) {
        if (i++ > 0) {
            tok = skip(tok, ",");
        }

        Type *type = declarator(&tok, tok, base_type, mm);
        Obj *var = new_lvar(get_ident(type->name, mm), type, mm);

        if (!equal(tok, "=")) {
            continue;
        }

        Node *lhs = new_var(var, type->name, mm);
        Node *rhs = assign(&tok, tok->next, mm);
        Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok, mm);
        cur = cur->next = new_unary(ND_EXPR_STMT, node, tok, mm);
    }

    Node *node = new_node(ND_BLOCK, tok, mm);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

// compound-stmt :: (declaration | stmt)* "}"
static Node *compound_stmt(Token **rest, Token *tok, MemManager *mm) {
    Node *node = new_node(ND_BLOCK, tok, mm);

    Node head = {};
    Node *cur = &head;
    while (!equal(tok, "}")) {
        if (equal(tok, "int")) {
            cur = cur->next = declaration(&tok, tok, mm);
        } else {
            cur = cur->next = stmt(&tok, tok, mm);
        }
        add_type(cur, mm);
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
static Node *stmt(Token **rest, Token *tok, MemManager *mm) {
    if (equal(tok, "return")) {
        Node *node = new_node(ND_RETURN, tok, mm);
        node->lhs = expr(&tok, tok->next, mm);
        *rest = skip(tok, ";");
        return node;
    }

    if (equal(tok, "if")) {
        Node *node = new_node(ND_IF, tok, mm);
        tok = skip(tok->next, "(");
        node->condition = expr(&tok, tok, mm);
        tok = skip(tok, ")");
        node->consequence = stmt(&tok, tok, mm);
        if (equal(tok, "else")) {
            node->alternative = stmt(&tok, tok->next, mm);
        }
        *rest = tok;
        return node;
    }

    if (equal(tok, "for")) {
        Node *node = new_node(ND_LOOP, tok, mm);
        tok = skip(tok->next, "(");

        node->initialize = expr_stmt(&tok, tok, mm);

        if (!equal(tok, ";")) {
            node->condition = expr(&tok, tok, mm);
        }
        tok = skip(tok, ";");

        if (!equal(tok, ")")) {
            node->increment = expr(&tok, tok, mm);
        }
        tok = skip(tok, ")");

        node->consequence = stmt(rest, tok, mm);
        return node;
    }

    if (equal(tok, "while")) {
        Node *node = new_node(ND_LOOP, tok, mm);
        tok = skip(tok->next, "(");
        node->condition = expr(&tok, tok, mm);
        tok = skip(tok, ")");
        node->consequence = stmt(rest, tok, mm);
        return node;
    }

    if (equal(tok, "{")) {
        return compound_stmt(rest, tok->next, mm);
    }

    return expr_stmt(rest, tok, mm);
}

// expr-stmt :: expr? ";"
static Node *expr_stmt(Token **rest, Token *tok, MemManager *mm) {
    if (equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok, mm);
    }

    Node *node = new_node(ND_EXPR_STMT, tok, mm);
    node->lhs = expr(&tok, tok, mm);
    *rest = skip(tok, ";");
    return node;
}

// expr :: assign
static Node *expr(Token **rest, Token *tok, MemManager *mm) {
    return assign(rest, tok, mm);
}

// assign :: equality ("=" assign)?
static Node *assign(Token **rest, Token *tok, MemManager *mm) {
    Node *node = equality(&tok, tok, mm);
    if (equal(tok, "=")) {
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next, mm), tok, mm);
    }
    *rest = tok;
    return node;
}

// equality :: relational ("==" relational | "!=" relational)*
static Node *equality(Token **rest, Token *tok, MemManager *mm) {
    Node *node = relational(&tok, tok, mm);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next, mm), start, mm);
            continue;
        }

        if (equal(tok, "!=")) {
            node = new_binary(ND_NEQ, node, relational(&tok, tok->next, mm), start, mm);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// relational :: add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **rest, Token *tok, MemManager *mm) {
    Node *node = add(&tok, tok, mm);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "<")) {
            node = new_binary(ND_LT, node, add(&tok, tok->next, mm), start, mm);
            continue;
        }

        if (equal(tok, "<=")) {
            node = new_binary(ND_LTE, node, add(&tok, tok->next, mm), start, mm);
            continue;
        }

        if (equal(tok, ">")) {
            node = new_binary(ND_LT, add(&tok, tok->next, mm), node, start, mm);
            continue;
        }

        if (equal(tok, ">=")) {
            node = new_binary(ND_LTE, add(&tok, tok->next, mm), node, start, mm);
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
static Node *new_add(Node *lhs, Node *rhs, Token *tok, MemManager *mm) {
    add_type(lhs, mm);
    add_type(rhs, mm);

    if (is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_binary(ND_ADD, lhs, rhs, tok, mm);
    }

    if (lhs->type->base && rhs->type->base) {
        error_tok(tok, "invalid operands");
    }

    if (!lhs->type->base && rhs->type->base) {
        Node *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    rhs = new_binary(ND_MUL, rhs, new_num(lhs->type->base->size, tok, mm), tok, mm);
    return new_binary(ND_ADD, lhs, rhs, tok, mm);
}

/*
`-` performs pointer arithmetic if the arguments include a pointer.

Moves the pointer by the size of the elements, not bytes.
p-n :: p - sizeof(*p)*n

The distance between two pointers, in units of the size of the elements.
p-q :: (p<--->q) / sizeof(*p)
*/
static Node *new_sub(Node *lhs, Node *rhs, Token *tok, MemManager *mm) {
    add_type(lhs, mm);
    add_type(rhs, mm);

    if (is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_binary(ND_SUB, lhs, rhs, tok, mm);
    }

    if (lhs->type->base && is_integer(rhs->type)) {
        rhs = new_binary(ND_MUL, rhs, new_num(lhs->type->base->size, tok, mm), tok, mm);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok, mm);
        node->type = lhs->type;
        return node;
    }

    if (lhs->type->base && rhs->type->base) {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok, mm);
        node->type = ty_int;
        return new_binary(ND_DIV, node, new_num(lhs->type->base->size, tok, mm), tok, mm);
    }

    error_tok(tok, "invalid operands");
    return NULL;
}

// add :: mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok, MemManager *mm) {
    Node *node = mul(&tok, tok, mm);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "+")) {
            node = new_add(node, mul(&tok, tok->next, mm), start, mm);
            continue;
        }

        if (equal(tok, "-")) {
            node = new_sub(node, mul(&tok, tok->next, mm), start, mm);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// mul :: unary ("*" unary | "/" unary)*
static Node *mul(Token **rest, Token *tok, MemManager *mm) {
    Node *node = unary(&tok, tok, mm);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "*")) {
            node = new_binary(ND_MUL, node, unary(&tok, tok->next, mm), start, mm);
            continue;
        }

        if (equal(tok, "/")) {
            node = new_binary(ND_DIV, node, unary(&tok, tok->next, mm), start, mm);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// unary :: ("+" | "-" | "&" | "*") unary
//        | postfix
static Node *unary(Token **rest, Token *tok, MemManager *mm) {
    if (equal(tok, "+")) {
        return unary(rest, tok->next, mm);
    }

    if (equal(tok, "-")) {
        return new_unary(ND_NEG, unary(rest, tok->next, mm), tok, mm);
    }

    if (equal(tok, "&")) {
        return new_unary(ND_ADDR, unary(rest, tok->next, mm), tok, mm);
    }

    if (equal(tok, "*")) {
        return new_unary(ND_DEREF, unary(rest, tok->next, mm), tok, mm);
    }

    return postfix(rest, tok, mm);
}

// postfix = primary ("[" expr "]")*
static Node *postfix(Token **rest, Token *tok, MemManager *mm) {
    Node *node = primary(&tok, tok, mm);

    while (equal(tok, "[")) {
        // x[y] is sugar for *(x + y)
        Token *start = tok;
        Node *idx = expr(&tok, tok->next, mm);
        tok = skip(tok, "]");
        node = new_unary(ND_DEREF, new_add(node, idx, start, mm), start, mm);
    }

    *rest = tok;
    return node;
}

// fn-call :: ident "(" (assign, ("," assign)*)? ")"
static Node *fn_call(Token **rest, Token *tok, MemManager *mm) {
    Token *start = tok;
    tok = tok->next->next;

    Node head = {};
    Node *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        cur = cur->next = assign(&tok, tok, mm);
    }

    *rest = skip(tok, ")");

    Node *node = new_node(ND_FN_CALL, start, mm);
    node->func = strndup(start->loc, start->len);
    register_obj(mm, node->func);
    node->args = head.next;
    return node;
}

// primary :: "(" expr ")"
//          | "sizeof" unary
//          | ident fn-args?
//          | num
static Node *primary(Token **rest, Token *tok, MemManager *mm) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next, mm);
        *rest = skip(tok, ")");
        return node;
    }

    if (equal(tok, "sizeof")) {
        Node *node = unary(rest, tok->next, mm);
        add_type(node, mm);
        return new_num(node->type->size, tok, mm);
    }

    if (tok->kind == TK_IDENT) {
        if (equal(tok->next, "(")) {
            return fn_call(rest, tok, mm);
        }

        Obj *var = find_var(tok);
        if (!var) {
            error_tok(tok, "undefined variable");
        }
        *rest = tok->next;
        return new_var(var, tok, mm);
    }

    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val, tok, mm);
        *rest = tok->next;
        return node;
    }

    error_tok(tok, "expected an expression");
    return NULL;
}

// function-definition :: stmt*
static Token *function(Token *tok, Type *base_type, MemManager *mm) {
    Type *type = declarator(&tok, tok, type, mm);

    Obj *fn = new_gvar(get_ident(type->name, mm), type, mm);
    fn->is_function = true;

    locals = NULL;

    create_param_lvars(type->params, mm);
    fn->params = locals;

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc func  %p %s\n", fn, fn->name);
    #endif

    tok = skip(tok, "{");
    fn->body = compound_stmt(&tok, tok, mm);
    fn->locals = locals;
    return tok;
}

static Token *global_variable(Token *tok, Type *base_type, MemManager *mm) {
    bool first = true;
    while (!consume(&tok, tok, ";")) {
        if (!first) {
            tok = skip(tok, ",");
        }
        first = false;

        Type *type = declarator(&tok, tok, base_type, mm);
        new_gvar(get_ident(type->name, mm), type, mm);
    }
    return tok;
}

static bool is_function(Token *tok, MemManager *mm) {
    if (equal(tok, ";")) return false;

    Type dummy = {};
    Type *type = declarator(&tok, tok, &dummy, mm);
    return type->kind == TY_FUNC;
}

// program :: (function-definition | global-variable)*
Obj *parse(Token *tok, MemManager *mm) {
    globals = NULL;

    while (tok->kind != TK_EOF) {
        Type *base_type = typespec(&tok, tok);

        if (is_function(tok, mm)) {
            tok = function(tok, base_type, mm);
        } else {
            tok = global_variable(tok, base_type, mm);
        }
    }

    return globals;
}
