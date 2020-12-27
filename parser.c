#include "charmcc.h"

// All local variable instances found during parsing.
Obj *locals;

static Node *new_node(NodeKind kind, Token *repr) {
    Node *node = calloc(1, sizeof(Node));

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

static Obj *new_lvar(char *name, Type *type) {
    Obj *var = calloc(1, sizeof(Obj));

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

static Node *new_num(int val, Token *repr) {
    Node *node = new_node(ND_NUM, repr);
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

static void create_param_lvars(Type *param) {
    if (param) {
        create_param_lvars(param->next);
        new_lvar(get_ident(param->name), param);
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
static Type *declarator(Token **rest, Token *tok, Type *type);
static Node *declaration(Token **rest, Token *tok);
static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// typespec :: "int"
static Type *typespec(Token **rest, Token *tok) {
    *rest = skip(tok, "int");
    return ty_int;
}

// func-params :: param ("," param)*
// param       :: typespec declarator
static Type *func_params(Token **rest, Token *tok, Type *type) {
    Type head = {};
    Type *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        Type *base_type = typespec(&tok, tok);
        Type *t = declarator(&tok, tok, base_type);
        cur = cur->next = copy_type(t);
    }

    type = func_type(type);
    type->params = head.next;
    *rest = tok->next;
    return type;
}

// type-suffix :: "(" func-params
//              | "[" num "]" type-suffix
//              | nothing
static Type *type_suffix(Token **rest, Token *tok, Type *type) {
    if (equal(tok, "(")) {
        return func_params(rest, tok->next, type);
    }

    if (equal(tok, "[")) {
        int size = get_number(tok->next);
        tok = skip(tok->next->next, "]");
        type = type_suffix(rest, tok, type);
        return array_of(type, size);
    }

    *rest = tok;
    return type;
}

// declarator :: "*"* ident type-suffix
static Type *declarator(Token **rest, Token *tok, Type *type) {
    while (consume(&tok, tok, "*")) {
        type = pointer_to(type);
    }

    if (tok->kind != TK_IDENT) {
        error_tok(tok, "expected a variable name");
    }

    type = type_suffix(rest, tok->next, type);
    type->name = tok;
    return type;
}

// declaration :: typespec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **rest, Token *tok) {
    Type *base_type = typespec(&tok, tok);

    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!equal(tok, ";")) {
        if (i++ > 0) {
            tok = skip(tok, ",");
        }

        Type *type = declarator(&tok, tok, base_type);
        Obj *var = new_lvar(get_ident(type->name), type);

        if (!equal(tok, "=")) {
            continue;
        }

        Node *lhs = new_var(var, type->name);
        Node *rhs = assign(&tok, tok->next);
        Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
        cur = cur->next = new_unary(ND_EXPR_STMT, node, tok);
    }

    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

// compound-stmt :: (declaration | stmt)* "}"
static Node *compound_stmt(Token **rest, Token *tok) {
    Node *node = new_node(ND_BLOCK, tok);

    Node head = {};
    Node *cur = &head;
    while (!equal(tok, "}")) {
        if (equal(tok, "int")) {
            cur = cur->next = declaration(&tok, tok);
        } else {
            cur = cur->next = stmt(&tok, tok);
        }
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

    rhs = new_binary(ND_MUL, rhs, new_num(lhs->type->base->size, tok), tok);
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
        rhs = new_binary(ND_MUL, rhs, new_num(lhs->type->base->size, tok), tok);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->type = lhs->type;
        return node;
    }

    if (lhs->type->base && rhs->type->base) {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->type = ty_int;
        return new_binary(ND_DIV, node, new_num(lhs->type->base->size, tok), tok);
    }

    error_tok(tok, "invalid operands");
    return NULL;
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
//        | postfix
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

    return postfix(rest, tok);
}

// postfix = primary ("[" expr "]")*
static Node *postfix(Token **rest, Token *tok) {
    Node *node = primary(&tok, tok);

    while (equal(tok, "[")) {
        // x[y] is sugar for *(x + y)
        Token *start = tok;
        Node *idx = expr(&tok, tok->next);
        tok = skip(tok, "]");
        node = new_unary(ND_DEREF, new_add(node, idx, start), start);
    }

    *rest = tok;
    return node;
}

// fn-call :: ident "(" (assign, ("," assign)*)? ")"
static Node *fn_call(Token **rest, Token *tok) {
    Token *start = tok;
    tok = tok->next->next;

    Node head = {};
    Node *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        cur = cur->next = assign(&tok, tok);
    }

    *rest = skip(tok, ")");

    Node *node = new_node(ND_FN_CALL, start);
    node->func = strndup(start->loc, start->len);
    node->args = head.next;
    return node;
}

// primary :: "(" expr ")"
//          | "sizeof" unary
//          | ident fn-args?
//          | num
static Node *primary(Token **rest, Token *tok) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if (equal(tok, "sizeof")) {
        Node *node = unary(rest, tok->next);
        add_type(node);
        Node *size = new_num(node->type->size, tok);
        free_node(node);
        return size;
    }

    if (tok->kind == TK_IDENT) {
        if (equal(tok->next, "(")) {
            return fn_call(rest, tok);
        }

        Obj *var = find_var(tok);
        if (!var) {
            error_tok(tok, "undefined variable");
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

// function-definition :: stmt*
static Function *function(Token **rest, Token *tok) {
    Type *type = typespec(&tok, tok);
    type = declarator(&tok, tok, type);

    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    fn->name = get_ident(type->name);
    fn->type = type;
    create_param_lvars(type->params);
    fn->params = locals;

    #if DEBUG_ALLOCS
    fprintf(stderr, "alloc func  %p %s\n", fn, fn->name);
    #endif

    tok = skip(tok, "{");
    fn->body = compound_stmt(rest, tok);
    fn->locals = locals;
    return fn;
}

// program :: function-definition*
Function *parse(Token *tok) {
    Function head = {};
    Function *cur = &head;

    while (tok->kind != TK_EOF) {
        cur = cur->next = function(&tok, tok);
    }

    return head.next;
}

void free_obj(Obj *o) {
    if (o == NULL) return;

    free_obj(o->next);

    #if DEBUG_ALLOCS
    fprintf(stderr, "free  obj   %p ", o);
    switch (o->type->kind) {
    case TY_INT:
        fprintf(stderr, "int");
        break;
    case TY_PTR: {
        fprintf(stderr, "ptr");
        break;
    }
    case TY_FUNC:
        fprintf(stderr, "func");
        break;
    }
    fprintf(stderr, " %s\n", o->name);
    #endif

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

    free_node(n->args);
    free_node(n->body);

    #if DEBUG_ALLOCS
    fprintf(stderr, "free  node  %p ", n);
    switch (n->kind) {
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
        fprintf(stderr, "fn call %s\n", n->func);
        break;
    }
    #endif
    
    if (n->func != NULL) free(n->func);

    free(n);
}

void free_function(Function *f) {
    if (f == NULL) return;

    free_function(f->next);
    free_type(f->type);
    free_node(f->body);
    free_obj(f->locals);

    #if DEBUG_ALLOCS
    fprintf(stderr, "free  func  %p %s\n", f, f->name);
    #endif

    free(f->name);
    free(f);
}

void free_ast(Function *prog) {
    free_function(prog);
}
