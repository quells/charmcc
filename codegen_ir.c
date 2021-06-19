#include "charmcc.h"

IRInstruction *new_inst(IRKind kind) {
    IRInstruction *inst = calloc(1, sizeof(IRInstruction));
    inst->kind = kind;
    return inst;
}

static int depth;
static Obj *current_fn;

static void gen_expr(Node *node);

static int count(void) {
    static int i = 1;
    return i++;
}

static void push(int r) {
    printf("  push  {r%d}\n", r);
    depth++;
}

static void pop(char *arg) {
    printf("  pop   {%s}\n", arg);
    depth--;
}

static void load(Type *type, int r) {
    if (type->kind == TY_ARRAY) {
        // cannot load an array into a register
        // references to the array are pointers to the first element
        return;
    }

    printf("  ldr   r%d, [r%d]\n", r, r);
}

static void store(void) {
    pop("r1");
    printf("  str   r0, [r1]\n");
}

/*
Round up `n` to the nearest multiple of `align`.
For example, align_to(5, 8) == 8 and align_to(11, 8) == 16.
*/
static int align_to(int n, int align) {
    return (n + align - 1) / align * align;
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

    return contains(node->lhs, kind)
        || contains(node->rhs, kind)
        || contains(node->body, kind)
        || contains(node->next, kind)
        || contains(node->condition, kind)
        || contains(node->consequence, kind)
        || contains(node->alternative, kind)
        || contains(node->initialize, kind)
        || contains(node->increment, kind);
}

// Compute absolute address of a node.
// It's an error if a given node does not reside in memory.
static void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR:
        if (node->var->is_local) {
            printf("  sub   r0, fp, #%d\n", node->var->offset);
        } else {
            printf("  ldr   r0, __addr_%s\n", node->var->name);
        }
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    default:
        break;
    }

    error_tok(node->repr, "not an lvalue");
}

static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("  mov   r0, #%d\n", node->val);
        return;
    case ND_NEG:
        assert(node->lhs);
        gen_expr(node->lhs);
        printf("  neg   r0, r0\n");
        return;
    case ND_VAR:
        gen_addr(node);
        assert(node->type);
        load(node->type, 0);
        return;
    case ND_ADDR:
        assert(node->lhs);
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        assert(node->lhs);
        gen_expr(node->lhs);
        assert(node->type);
        load(node->type, 0);
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push(0);
        gen_expr(node->rhs);
        store();
        return;
    case ND_FN_CALL: {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push(0);
            nargs++;
        }

        assert(nargs <= 4);
        char reg[3] = {'r', '_', '\0'};
        for (int i = nargs - 1; i >= 0; i--) {
            reg[1] = '0' + i;
            pop(reg);
        }

        printf("  bl    %s\n", node->func);
        return;
    }
    default:
        break;
    }

    gen_expr(node->rhs);
    push(0);
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
        printf("  bl    __div\n");
        return;
    case ND_EQ:
    case ND_NEQ:
    case ND_LT:
    case ND_LTE:
        printf("  cmp   r0, r1\n"); // carry set if rhs - lhs is positive
        switch (node->kind) {
        case ND_EQ:
            printf(
                "  moveq r0, #1\n"
                "  movne r0, #0\n");
            break;
        case ND_NEQ:
            printf(
                "  movne r0, #1\n"
                "  moveq r0, #0\n");
            break;
        case ND_LT:
            printf(
                "  movlt r0, #1\n"
                "  movge r0, #0\n");
            break;
        case ND_LTE:
            printf(
                "  movle r0, #1\n"
                "  movgt r0, #0\n");
            break;
        default:
            break;
        }
        return;
    default:
        break;
    }

    error_tok(node->repr, "invalid expression");
}

static int assign_offsets(Obj *prog) {
    int global_vars = 0;
    for (Obj *obj = prog; obj; obj = obj->next) {
        if (!obj->is_function) {
            global_vars++;
            continue;
        }

        Obj *fn = obj;
        int lvar_offset = PTR_SIZE;
        for (Obj *var = fn->locals; var; var = var->next) {
            lvar_offset += var->type->size;
            var->offset = lvar_offset;
        }
        fn->stack_size = align_to(lvar_offset, 16);
    }
    return global_vars;
}

static void gen_stmt(Node *node) {
    switch (node->kind) {
    case ND_IF: {
        int c = count();
        gen_expr(node->condition);
        printf("  cmp   r0, #0\n");
        printf("  beq   %s.if.else.%d\n", current_fn->name, c);
        gen_stmt(node->consequence);
        printf("  b     %s.if.end.%d\n", current_fn->name, c);
        printf("%s.if.else.%d:\n", current_fn->name, c);
        if (node->alternative) {
            gen_stmt(node->alternative);
        }
        printf("%s.if.end.%d:\n", current_fn->name, c);
        return;
    }
    case ND_LOOP: {
        int c = count();
        if (node->initialize) {
            gen_stmt(node->initialize);
        }
        printf("%s.loop.begin.%d:\n", current_fn->name, c);
        if (node->condition) {
            gen_expr(node->condition);
            printf("  cmp   r0, #0\n");
            printf("  beq   %s.loop.end.%d\n", current_fn->name, c);
        }
        gen_stmt(node->consequence);
        if (node->increment) {
            gen_expr(node->increment);
        }
        printf("  b     %s.loop.begin.%d\n", current_fn->name, c);
        printf("%s.loop.end.%d:\n", current_fn->name, c);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next) {
            gen_stmt(n);
        }
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("  b     %s.return\n", current_fn->name);
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    default:
        break;
    }

    error_tok(node->repr, "invalid statement");
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
    printf("__div:\n"
        "  push  {fp, lr}\n"
        "  add   fp, sp, #4\n");
    printf(
        // check for divide by zero
        // @TODO: jump to some sort of panic routine
        "  cmp   r1, #0\n"
        "  beq   __div_end\n"
        "  push  {r0, r1}\n"
        // variables
        "  mov   r0, #0\n"         // quotient
        "  pop   {r1, r2}\n"       // dividend / remainder, divisor
        "  mov   r3, #1\n"         // bit field
        "__div_shift:\n"
        // shift divisor left until it exceeds dividend
        // the bit field will be shifted by one less
        "  cmp   r2, r1\n"
        "  lslls r2, r2, #1\n"
        "  lslls r3, r3, #1\n"
        "  bls   __div_shift\n"
        "__div_sub:\n"
        // cmp sets the carry flag if r1 - r2 is positive, which is weird
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
        "  bcc   __div_sub\n");
    printf(
        "__div_end:\n"
        "  sub   sp, fp, #4\n"
        "  pop   {fp, pc}\n");
}

IRInstruction *gen_fn(Obj *fn) {
    current_fn = fn;
    IRInstruction head = {};
    IRInstruction *cur = &head;

    cur = cur->next = new_inst(IR_LABEL);
    cur->target = fn->name;

    cur = cur->next = new_inst(IR_PROLOGUE);

    if (fn->stack_size > 0) {
        cur = cur->next = new_inst(IR_ALLOC);
        cur->imm = fn->stack_size;
    }

    cur = cur->next = new_inst(IR_LABEL);
    cur->target = malloc((strlen(fn->name) + 8) * sizeof(char));
    sprintf(cur->target, "%s.return", fn->name);

    cur = cur->next = new_inst(IR_EPILOGUE);
    cur = cur->next = new_inst(IR_RETURN);

    return head.next;

    // printf(
    //     "%s:\n"
    //     "  push  {fp, lr}\n"
    //     "  add   fp, sp, #4\n"
    //     "  sub   sp, sp, #%d\n",
    //     fn->name,
    //     fn->stack_size);

    // if (fn->params) {
    //     // Save passed-by-register arguments to stack
    //     // str   r0, [fp, #-8]
    //     int i = 0;
    //     for (Obj *var = fn->params; var; var = var->next) {
    //         assert(i < 4);
    //         printf("  str   r%d, [fp, #-%d]\n", i++, var->offset);
    //     }
    // }

    // gen_stmt(fn->body);
    // assert(depth == 0);

    // printf(
    //     "%s.return:\n"
    //     "  sub   sp, fp, #4\n"
    //     "  pop   {fp, pc}\n\n",
    //     fn->name);
    
    // return contains(fn->body, ND_DIV);
}

// Create a linked list of intermediate representation instructions from AST.
IR *codegen_ir(Obj *ast) {
    IR *prog = calloc(1, sizeof(IR));
    
    IRInstruction head = {};
    IRInstruction *cur = &head;

    // Declare global variables
    int has_global_vars = assign_offsets(ast);
    if (has_global_vars) {
        for (Obj *obj = ast; obj; obj = obj->next) {
            if (!obj->is_function) {
                prog->vars = append_str(prog->vars, obj->name);
            }
        }
    }

    // Declare functions
    for (Obj *obj = ast; obj; obj = obj->next) {
        if (obj->is_function) {
            prog->funcs = append_str(prog->funcs, obj->name);
        }
    }

    // Generate functions
    for (Obj *obj = ast; obj; obj = obj->next) {
        if (obj->is_function) {
            cur = cur->next = gen_fn(obj);
        }
    }

    prog->instructions = head.next;
    return prog;
}

void free_ir_instruction(IRInstruction *inst) {
    if (inst == NULL) return;
    free_ir_instruction(inst->next);
    free(inst);
}

void free_ir(IR *prog) {
    free_string_list(prog->vars);
    free_string_list(prog->funcs);
    free_ir_instruction(prog->instructions);
    free(prog);
}
