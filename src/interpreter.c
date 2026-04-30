#include "interpreter.h"
#include <math.h>

/* ===== Env ===== */

Env *env_new(Env *parent) {
    Env *e = (Env*)sb_xcalloc(1, sizeof(Env));
    e->parent = parent;
    return e;
}

void env_free(Env *e) {
    if (!e) return;
    EnvEntry *cur = e->head;
    while (cur) {
        EnvEntry *next = cur->next;
        free(cur->name);
        value_free(&cur->value);
        free(cur);
        cur = next;
    }
    free(e);
}

void env_define(Env *e, const char *name, Value v, bool is_const) {
    /* shadow if already defined in same scope */
    for (EnvEntry *c = e->head; c; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            value_free(&c->value);
            c->value = v;
            c->is_const = is_const;
            return;
        }
    }
    EnvEntry *ne = (EnvEntry*)sb_xcalloc(1, sizeof(EnvEntry));
    ne->name = sb_strdup(name);
    ne->value = v;
    ne->is_const = is_const;
    ne->next = e->head;
    e->head = ne;
}

static EnvEntry *env_lookup(Env *e, const char *name) {
    for (Env *cur = e; cur; cur = cur->parent) {
        for (EnvEntry *c = cur->head; c; c = c->next) {
            if (strcmp(c->name, name) == 0) return c;
        }
    }
    return NULL;
}

Value env_get(Env *e, const char *name, bool *found) {
    EnvEntry *en = env_lookup(e, name);
    if (!en) { if (found) *found = false; return v_null(); }
    if (found) *found = true;
    return value_copy(&en->value);
}

bool env_assign(Env *e, const char *name, Value v) {
    EnvEntry *en = env_lookup(e, name);
    if (!en) return false;
    if (en->is_const) return false;
    value_free(&en->value);
    en->value = v;
    return true;
}

/* ===== Runtime errors ===== */

static void rt_error(Interp *I, int line, const char *msg) {
    fprintf(stderr, "%s:%d: runtime error: %s\n",
            I->filename ? I->filename : "<input>", line, msg);
    exit(1);
}

/* ===== Builtins ===== */

static Value builtin_println(Interp *I, int argc, Value *argv) {
    SB_UNUSED(I);
    for (int i = 0; i < argc; i++) {
        if (i) fputc(' ', stdout);
        char *s = value_to_cstring(&argv[i]);
        fputs(s, stdout);
        free(s);
    }
    fputc('\n', stdout);
    return v_null();
}

static Value builtin_print(Interp *I, int argc, Value *argv) {
    SB_UNUSED(I);
    for (int i = 0; i < argc; i++) {
        char *s = value_to_cstring(&argv[i]);
        fputs(s, stdout);
        free(s);
    }
    return v_null();
}

static Value builtin_readline(Interp *I, int argc, Value *argv) {
    SB_UNUSED(I); SB_UNUSED(argc); SB_UNUSED(argv);
    size_t cap = 64, len = 0;
    char *buf = (char*)sb_xmalloc(cap);
    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (len + 1 >= cap) { cap *= 2; buf = (char*)sb_xrealloc(buf, cap); }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    return v_string_take(buf);
}

/* Math */
static Value math_sqrt(Interp *I, int argc, Value *argv) {
    SB_UNUSED(I); if (argc < 1) return v_float(0);
    double x = (argv[0].type == V_INT) ? (double)argv[0].as.i : argv[0].as.f;
    return v_float(sqrt(x));
}
static Value math_abs(Interp *I, int argc, Value *argv) {
    SB_UNUSED(I); if (argc < 1) return v_int(0);
    if (argv[0].type == V_INT) return v_int(llabs(argv[0].as.i));
    return v_float(fabs(argv[0].as.f));
}
static Value math_pow(Interp *I, int argc, Value *argv) {
    SB_UNUSED(I); if (argc < 2) return v_float(0);
    double a = (argv[0].type == V_INT) ? (double)argv[0].as.i : argv[0].as.f;
    double b = (argv[1].type == V_INT) ? (double)argv[1].as.i : argv[1].as.f;
    return v_float(pow(a, b));
}

/* Strings */
static Value strings_length(Interp *I, int argc, Value *argv) {
    SB_UNUSED(I); if (argc < 1 || argv[0].type != V_STRING) return v_int(0);
    return v_int((long long)strlen(argv[0].as.s));
}
static Value strings_concat(Interp *I, int argc, Value *argv) {
    SB_UNUSED(I);
    size_t total = 1;
    for (int i = 0; i < argc; i++) {
        char *t = value_to_cstring(&argv[i]); total += strlen(t); free(t);
    }
    char *out = (char*)sb_xmalloc(total);
    out[0] = '\0';
    for (int i = 0; i < argc; i++) {
        char *t = value_to_cstring(&argv[i]); strcat(out, t); free(t);
    }
    return v_string_take(out);
}

/* ===== forward decls ===== */
static Value eval(Interp *I, Env *env, Node *n);
static void  exec(Interp *I, Env *env, Node *n);
static Value call_value(Interp *I, Value callee, int argc, Value *argv, int line);

/* ===== Type coercion helpers ===== */

static double to_num(const Value *v) {
    switch (v->type) {
        case V_INT: return (double)v->as.i;
        case V_FLOAT: return v->as.f;
        case V_BOOL: return v->as.b ? 1.0 : 0.0;
        case V_CHAR: return (double)(unsigned char)v->as.c;
        default: return 0.0;
    }
}
static long long to_int(const Value *v) {
    switch (v->type) {
        case V_INT: return v->as.i;
        case V_FLOAT: return (long long)v->as.f;
        case V_BOOL: return v->as.b ? 1 : 0;
        case V_CHAR: return (long long)(unsigned char)v->as.c;
        default: return 0;
    }
}
static bool is_numeric(const Value *v) {
    return v->type == V_INT || v->type == V_FLOAT || v->type == V_BOOL || v->type == V_CHAR;
}
static bool both_int(const Value *a, const Value *b) {
    return (a->type == V_INT || a->type == V_BOOL || a->type == V_CHAR) &&
           (b->type == V_INT || b->type == V_BOOL || b->type == V_CHAR);
}

static Value bin_arith(Interp *I, OpKind op, Value a, Value b, int line) {
    /* string concat for + */
    if (op == OP_ADD && (a.type == V_STRING || b.type == V_STRING)) {
        char *sa = value_to_cstring(&a);
        char *sb = value_to_cstring(&b);
        size_t n = strlen(sa) + strlen(sb) + 1;
        char *r = (char*)sb_xmalloc(n);
        snprintf(r, n, "%s%s", sa, sb);
        free(sa); free(sb);
        value_free(&a); value_free(&b);
        return v_string_take(r);
    }
    if (!is_numeric(&a) || !is_numeric(&b)) {
        value_free(&a); value_free(&b);
        rt_error(I, line, "arithmetic on non-numeric value");
    }
    if (both_int(&a, &b) && op != OP_DIV) {
        long long x = to_int(&a), y = to_int(&b), r = 0;
        switch (op) {
            case OP_ADD: r = x + y; break;
            case OP_SUB: r = x - y; break;
            case OP_MUL: r = x * y; break;
            case OP_MOD: if (y == 0) rt_error(I, line, "modulo by zero"); r = x % y; break;
            default: break;
        }
        return v_int(r);
    }
    if (both_int(&a, &b) && op == OP_DIV) {
        long long x = to_int(&a), y = to_int(&b);
        if (y == 0) rt_error(I, line, "division by zero");
        return v_int(x / y);
    }
    double x = to_num(&a), y = to_num(&b), r = 0;
    switch (op) {
        case OP_ADD: r = x + y; break;
        case OP_SUB: r = x - y; break;
        case OP_MUL: r = x * y; break;
        case OP_DIV: if (y == 0.0) rt_error(I, line, "division by zero"); r = x / y; break;
        case OP_MOD: if (y == 0.0) rt_error(I, line, "modulo by zero"); r = fmod(x, y); break;
        default: break;
    }
    return v_float(r);
}

static int value_cmp(const Value *a, const Value *b) {
    if (a->type == V_STRING && b->type == V_STRING)
        return strcmp(a->as.s ? a->as.s : "", b->as.s ? b->as.s : "");
    double x = to_num(a), y = to_num(b);
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static bool value_eq(const Value *a, const Value *b) {
    if (a->type == V_NULL || b->type == V_NULL) return a->type == b->type;
    if (a->type == V_STRING || b->type == V_STRING) {
        if (a->type != b->type) return false;
        return strcmp(a->as.s ? a->as.s : "", b->as.s ? b->as.s : "") == 0;
    }
    return value_cmp(a, b) == 0;
}

/* ===== Member resolution (Console.WriteLine etc.) ===== */

/* Try to evaluate a member chain as an instance lookup (struct fields).
   Returns true on success and writes result to *out (caller owns).
   On false, no allocation has been made. */
static bool try_resolve_struct_member(Interp *I, Env *env, Node *member_expr, Value *out) {
    /* Evaluate the object subexpression */
    Node *obj = member_expr->as.member.object;
    Value v;
    if (obj->kind == EX_MEMBER) {
        if (!try_resolve_struct_member(I, env, obj, &v)) {
            /* Try as namespace path first via existing flow */
            return false;
        }
    } else if (obj->kind == EX_IDENT) {
        bool found = false;
        v = env_get(env, obj->as.ident.name, &found);
        if (!found) return false;
    } else {
        /* generic eval */
        v = eval(I, env, obj);
    }
    if (v.type != V_STRUCT) { value_free(&v); return false; }
    StructFieldV *f = struct_find_field(v.as.st, member_expr->as.member.name);
    if (!f) {
        char msg[256];
        snprintf(msg, sizeof msg, "struct '%s' has no field '%s'",
                 v.as.st->type_name ? v.as.st->type_name : "?",
                 member_expr->as.member.name);
        value_free(&v);
        rt_error(I, member_expr->line, msg);
    }
    *out = value_copy(f->value);
    value_free(&v);
    return true;
}

static Value resolve_member(Interp *I, Env *env, Node *member_expr) {
    /* First: build dotted path "A.B.C" if the chain bottoms in an identifier
       and try a namespace lookup. */
    char path[512]; path[0] = '\0';
    Node *cur = member_expr;
    Node *stack[64]; int sp = 0;
    while (cur && cur->kind == EX_MEMBER) {
        if (sp >= 64) rt_error(I, member_expr->line, "member path too deep");
        stack[sp++] = cur;
        cur = cur->as.member.object;
    }
    if (cur && cur->kind == EX_IDENT) {
        snprintf(path, sizeof path, "%s", cur->as.ident.name);
        for (int i = sp - 1; i >= 0; i--) {
            size_t len = strlen(path);
            snprintf(path + len, sizeof(path) - len, ".%s", stack[i]->as.member.name);
        }
        bool found = false;
        Value v = env_get(env, path, &found);
        if (found) return v;
    }
    /* Fallback: struct-instance field access */
    Value out;
    if (try_resolve_struct_member(I, env, member_expr, &out)) return out;
    char msg[256];
    if (path[0]) snprintf(msg, sizeof msg, "unknown name '%s'", path);
    else snprintf(msg, sizeof msg, "unsupported member access");
    rt_error(I, member_expr->line, msg);
    return v_null();
}

/* ===== Eval / Exec ===== */

static Value eval(Interp *I, Env *env, Node *n) {
    if (!n) return v_null();
    switch (n->kind) {
        case EX_INT: return v_int(n->as.i);
        case EX_FLOAT: return v_float(n->as.f);
        case EX_STRING: return v_string(n->as.s);
        case EX_CHAR: return v_char((char)n->as.i);
        case EX_BOOL: return v_bool(n->as.b);
        case EX_NULL: return v_null();
        case EX_IDENT: {
            bool found = false;
            Value v = env_get(env, n->as.ident.name, &found);
            if (!found) {
                char msg[256];
                snprintf(msg, sizeof msg, "undefined variable '%s'", n->as.ident.name);
                rt_error(I, n->line, msg);
            }
            return v;
        }
        case EX_BINARY: {
            Value a = eval(I, env, n->as.binary.lhs);
            Value b = eval(I, env, n->as.binary.rhs);
            switch (n->as.binary.op) {
                case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
                    return bin_arith(I, n->as.binary.op, a, b, n->line);
                case OP_EQ: { bool r = value_eq(&a, &b); value_free(&a); value_free(&b); return v_bool(r); }
                case OP_NEQ:{ bool r = !value_eq(&a, &b); value_free(&a); value_free(&b); return v_bool(r); }
                case OP_LT: { int c = value_cmp(&a, &b); value_free(&a); value_free(&b); return v_bool(c < 0); }
                case OP_GT: { int c = value_cmp(&a, &b); value_free(&a); value_free(&b); return v_bool(c > 0); }
                case OP_LE: { int c = value_cmp(&a, &b); value_free(&a); value_free(&b); return v_bool(c <= 0); }
                case OP_GE: { int c = value_cmp(&a, &b); value_free(&a); value_free(&b); return v_bool(c >= 0); }
                default: rt_error(I, n->line, "unsupported binary op");
            }
            return v_null();
        }
        case EX_LOGICAL: {
            Value a = eval(I, env, n->as.logical.lhs);
            bool ta = value_truthy(&a);
            if (n->as.logical.op == OP_AND) {
                if (!ta) { value_free(&a); return v_bool(false); }
                value_free(&a);
                Value b = eval(I, env, n->as.logical.rhs);
                bool tb = value_truthy(&b); value_free(&b);
                return v_bool(tb);
            } else {
                if (ta) { value_free(&a); return v_bool(true); }
                value_free(&a);
                Value b = eval(I, env, n->as.logical.rhs);
                bool tb = value_truthy(&b); value_free(&b);
                return v_bool(tb);
            }
        }
        case EX_UNARY: {
            Value v = eval(I, env, n->as.unary.operand);
            switch (n->as.unary.op) {
                case OP_NEG:
                    if (v.type == V_FLOAT) { Value r = v_float(-v.as.f); value_free(&v); return r; }
                    { long long x = to_int(&v); value_free(&v); return v_int(-x); }
                case OP_POS: return v;
                case OP_NOT: { bool t = value_truthy(&v); value_free(&v); return v_bool(!t); }
                default: rt_error(I, n->line, "unsupported unary op");
            }
            return v_null();
        }
        case EX_POSTFIX: {
            Node *t = n->as.postfix.operand;
            if (t->kind != EX_IDENT) rt_error(I, n->line, "++/-- requires variable");
            bool found = false;
            Value v = env_get(env, t->as.ident.name, &found);
            if (!found) rt_error(I, n->line, "undefined variable in ++/--");
            Value old = value_copy(&v);
            long long iv = to_int(&v);
            if (n->as.postfix.op == OP_INC) iv++; else iv--;
            value_free(&v);
            Value newv;
            if (old.type == V_FLOAT) newv = v_float((double)iv);
            else newv = v_int(iv);
            if (!env_assign(env, t->as.ident.name, value_copy(&newv))) {
                value_free(&newv);
                rt_error(I, n->line, "cannot assign to const");
            }
            value_free(&newv);
            return old;
        }
        case EX_ASSIGN: {
            Node *t = n->as.assign.target;
            if (t->kind == EX_MEMBER) {
                /* Struct field assignment: obj.field = value (single level: obj must be ident
                   or chain of struct fields). */
                Value rhs = eval(I, env, n->as.assign.value);
                /* Evaluate the object subexpression to get a struct (refcounted). */
                Value obj;
                Node *objn = t->as.member.object;
                if (objn->kind == EX_IDENT) {
                    bool found = false;
                    obj = env_get(env, objn->as.ident.name, &found);
                    if (!found) { value_free(&rhs); rt_error(I, n->line, "undefined variable in assignment"); }
                } else {
                    obj = eval(I, env, objn);
                }
                if (obj.type != V_STRUCT) {
                    value_free(&obj); value_free(&rhs);
                    rt_error(I, n->line, "field assignment requires struct value");
                }
                StructFieldV *f = struct_find_field(obj.as.st, t->as.member.name);
                if (!f) {
                    char msg[256];
                    snprintf(msg, sizeof msg, "struct '%s' has no field '%s'",
                             obj.as.st->type_name ? obj.as.st->type_name : "?",
                             t->as.member.name);
                    value_free(&obj); value_free(&rhs);
                    rt_error(I, n->line, msg);
                }
                Value to_store;
                if (n->as.assign.is_compound) {
                    Value cur = value_copy(f->value);
                    to_store = bin_arith(I, n->as.assign.compound, cur, rhs, n->line);
                } else {
                    to_store = rhs;
                }
                Value ret = value_copy(&to_store);
                value_free(f->value);
                *f->value = to_store;
                value_free(&obj);
                return ret;
            }
            if (t->kind != EX_IDENT) rt_error(I, n->line, "assignment target must be a variable");
            Value rhs = eval(I, env, n->as.assign.value);
            Value to_store;
            if (n->as.assign.is_compound) {
                bool found = false;
                Value cur = env_get(env, t->as.ident.name, &found);
                if (!found) rt_error(I, n->line, "undefined variable");
                to_store = bin_arith(I, n->as.assign.compound, cur, rhs, n->line);
            } else {
                to_store = rhs;
            }
            Value ret = value_copy(&to_store);
            if (!env_assign(env, t->as.ident.name, to_store)) {
                value_free(&to_store);
                value_free(&ret);
                rt_error(I, n->line, "undefined or const variable in assignment");
            }
            return ret;
        }
        case EX_MEMBER: {
            return resolve_member(I, env, n);
        }
        case EX_CALL: {
            Value callee;
            if (n->as.call.callee->kind == EX_MEMBER)
                callee = resolve_member(I, env, n->as.call.callee);
            else
                callee = eval(I, env, n->as.call.callee);
            int argc = (int)n->as.call.args.count;
            Value *argv = argc ? (Value*)sb_xcalloc(argc, sizeof(Value)) : NULL;
            for (int i = 0; i < argc; i++)
                argv[i] = eval(I, env, n->as.call.args.items[i]);
            Value r = call_value(I, callee, argc, argv, n->line);
            for (int i = 0; i < argc; i++) value_free(&argv[i]);
            free(argv);
            value_free(&callee);
            return r;
        }
        default:
            rt_error(I, n->line, "unsupported expression");
    }
    return v_null();
}

static Value call_value(Interp *I, Value callee, int argc, Value *argv, int line) {
    if (callee.type == V_BUILTIN) {
        return callee.as.builtin(I, argc, argv);
    }
    if (callee.type == V_FUNC) {
        Node *fn = callee.as.func.decl;
        Env *env = env_new(callee.as.func.closure);
        size_t expected = fn->as.func.param_count;
        if ((size_t)argc != expected) {
            char msg[128];
            snprintf(msg, sizeof msg, "function '%s' expects %zu arg(s), got %d",
                     fn->as.func.name, expected, argc);
            env_free(env);
            rt_error(I, line, msg);
        }
        for (size_t i = 0; i < expected; i++) {
            env_define(env, fn->as.func.params[i].name, value_copy(&argv[i]),
                       fn->as.func.params[i].type.is_const);
        }
        exec(I, env, fn->as.func.body);
        Value r = v_null();
        if (I->return_flag) {
            r = I->return_value;
            I->return_value = v_null();
            I->return_flag = 0;
        }
        env_free(env);
        return r;
    }
    rt_error(I, line, "value is not callable");
    return v_null();
}

static void exec(Interp *I, Env *env, Node *n) {
    if (!n) return;
    if (I->return_flag || I->break_flag || I->continue_flag) return;
    switch (n->kind) {
        case ST_EXPR: {
            if (n->as.expr_stmt.expr) {
                Value v = eval(I, env, n->as.expr_stmt.expr);
                value_free(&v);
            }
            break;
        }
        case ST_BLOCK: {
            Env *child = env_new(env);
            for (size_t i = 0; i < n->as.block.stmts.count; i++) {
                exec(I, child, n->as.block.stmts.items[i]);
                if (I->return_flag || I->break_flag || I->continue_flag) break;
            }
            env_free(child);
            break;
        }
        case ST_VAR_DECL: {
            Value v = v_null();
            Node *init = n->as.var_decl.init;
            if (init && init->kind == EX_STRUCT_LIT) {
                /* Construct a struct instance from the brace initializer.
                   The variable's declared type must name a known struct. */
                const char *tname = n->as.var_decl.type.type_name;
                bool found = false;
                Value def = env_get(env, tname ? tname : "", &found);
                if (!found || def.type != V_STRUCT_DEF) {
                    value_free(&def);
                    char msg[256];
                    snprintf(msg, sizeof msg,
                             "brace initializer requires a struct type ('%s' is not a struct)",
                             tname ? tname : "?");
                    rt_error(I, n->line, msg);
                }
                Node *sd = def.as.sdef.decl;
                size_t fc = sd->as.struct_decl.field_count;
                size_t given = init->as.struct_lit.values.count;
                if (given > fc) {
                    value_free(&def);
                    rt_error(I, n->line, "too many initializers for struct");
                }
                v = v_struct_new(sd->as.struct_decl.name, fc);
                for (size_t i = 0; i < fc; i++) {
                    v.as.st->fields[i].name = sb_strdup(sd->as.struct_decl.fields[i].name);
                    if (i < given) {
                        Value fv = eval(I, env, init->as.struct_lit.values.items[i]);
                        value_free(v.as.st->fields[i].value);
                        *v.as.st->fields[i].value = fv;
                    }
                }
                value_free(&def);
            } else if (init) {
                v = eval(I, env, init);
            } else {
                /* Default-construct if the declared type is a struct. */
                const char *tname = n->as.var_decl.type.type_name;
                if (tname) {
                    bool found = false;
                    Value def = env_get(env, tname, &found);
                    if (found && def.type == V_STRUCT_DEF) {
                        Node *sd = def.as.sdef.decl;
                        size_t fc = sd->as.struct_decl.field_count;
                        v = v_struct_new(sd->as.struct_decl.name, fc);
                        for (size_t i = 0; i < fc; i++) {
                            v.as.st->fields[i].name = sb_strdup(sd->as.struct_decl.fields[i].name);
                        }
                    }
                    value_free(&def);
                }
            }
            env_define(env, n->as.var_decl.name, v, n->as.var_decl.type.is_const);
            break;
        }
        case ST_IF: {
            Value c = eval(I, env, n->as.if_stmt.cond);
            bool t = value_truthy(&c);
            value_free(&c);
            if (t) exec(I, env, n->as.if_stmt.then_branch);
            else if (n->as.if_stmt.else_branch) exec(I, env, n->as.if_stmt.else_branch);
            break;
        }
        case ST_WHILE: {
            while (1) {
                Value c = eval(I, env, n->as.while_stmt.cond);
                bool t = value_truthy(&c); value_free(&c);
                if (!t) break;
                exec(I, env, n->as.while_stmt.body);
                if (I->return_flag) return;
                if (I->break_flag) { I->break_flag = 0; break; }
                if (I->continue_flag) { I->continue_flag = 0; }
            }
            break;
        }
        case ST_FOR: {
            Env *fenv = env_new(env);
            if (n->as.for_stmt.init) exec(I, fenv, n->as.for_stmt.init);
            while (1) {
                if (n->as.for_stmt.cond) {
                    Value c = eval(I, fenv, n->as.for_stmt.cond);
                    bool t = value_truthy(&c); value_free(&c);
                    if (!t) break;
                }
                exec(I, fenv, n->as.for_stmt.body);
                if (I->return_flag) { env_free(fenv); return; }
                if (I->break_flag) { I->break_flag = 0; break; }
                if (I->continue_flag) { I->continue_flag = 0; }
                if (n->as.for_stmt.post) {
                    Value p = eval(I, fenv, n->as.for_stmt.post);
                    value_free(&p);
                }
            }
            env_free(fenv);
            break;
        }
        case ST_RETURN: {
            Value v = v_null();
            if (n->as.ret.value) v = eval(I, env, n->as.ret.value);
            I->return_value = v;
            I->return_flag = 1;
            break;
        }
        case ST_BREAK: I->break_flag = 1; break;
        case ST_CONTINUE: I->continue_flag = 1; break;
        case ST_FUNC_DECL: {
            Value v = {0};
            v.type = V_FUNC;
            v.as.func.decl = n;
            v.as.func.closure = env;
            env_define(env, n->as.func.name, v, true);
            break;
        }
        case ST_MODULE: /* nothing at runtime for now */ break;
        case ST_STRUCT_DECL: {
            env_define(env, n->as.struct_decl.name, v_struct_def(n), true);
            break;
        }
        case ST_ENUM_DECL: {
            env_define(env, n->as.enum_decl.name, v_namespace(n->as.enum_decl.name), true);
            long long next = 0;
            char path[256];
            for (size_t i = 0; i < n->as.enum_decl.count; i++) {
                EnumMember *m = &n->as.enum_decl.members[i];
                long long val = m->has_value ? m->value : next;
                next = val + 1;
                snprintf(path, sizeof path, "%s.%s", n->as.enum_decl.name, m->name);
                env_define(env, path, v_int(val), true);
                /* also expose unqualified name for C-style access */
                env_define(env, m->name, v_int(val), true);
            }
            break;
        }
        default: {
            /* expression at top level (shouldn't happen via parser) */
            Value v = eval(I, env, n);
            value_free(&v);
        }
    }
}

/* ===== Init / Run ===== */

void interp_init(Interp *I, const char *filename) {
    memset(I, 0, sizeof(*I));
    I->filename = filename;
    I->globals = env_new(NULL);

    /* Built-in namespaces (registered as dotted names for member resolution).
       We register both the short and the System.-prefixed forms so that both
       'Console.WriteLine(...)' and 'System.Console.WriteLine(...)' work. */
    env_define(I->globals, "Console.WriteLine", v_builtin(builtin_println), true);
    env_define(I->globals, "Console.Write",     v_builtin(builtin_print),   true);
    env_define(I->globals, "Console.ReadLine",  v_builtin(builtin_readline),true);
    env_define(I->globals, "Console",           v_namespace("Console"),     true);

    env_define(I->globals, "System",                    v_namespace("System"),         true);
    env_define(I->globals, "System.Console",            v_namespace("System.Console"), true);
    env_define(I->globals, "System.Console.WriteLine", v_builtin(builtin_println),     true);
    env_define(I->globals, "System.Console.Write",     v_builtin(builtin_print),       true);
    env_define(I->globals, "System.Console.ReadLine",  v_builtin(builtin_readline),    true);

    env_define(I->globals, "Math.sqrt", v_builtin(math_sqrt), true);
    env_define(I->globals, "Math.abs",  v_builtin(math_abs),  true);
    env_define(I->globals, "Math.pow",  v_builtin(math_pow),  true);
    env_define(I->globals, "Math",      v_namespace("Math"),  true);

    env_define(I->globals, "System.Math",      v_namespace("System.Math"), true);
    env_define(I->globals, "System.Math.sqrt", v_builtin(math_sqrt),       true);
    env_define(I->globals, "System.Math.abs",  v_builtin(math_abs),        true);
    env_define(I->globals, "System.Math.pow",  v_builtin(math_pow),        true);

    env_define(I->globals, "Strings.length", v_builtin(strings_length), true);
    env_define(I->globals, "Strings.concat", v_builtin(strings_concat), true);
    env_define(I->globals, "Strings",        v_namespace("Strings"),    true);

    env_define(I->globals, "System.Strings",        v_namespace("System.Strings"), true);
    env_define(I->globals, "System.Strings.length", v_builtin(strings_length),     true);
    env_define(I->globals, "System.Strings.concat", v_builtin(strings_concat),     true);

    /* Top-level convenience */
    env_define(I->globals, "println", v_builtin(builtin_println), true);
    env_define(I->globals, "print",   v_builtin(builtin_print),   true);
}

void interp_dispose(Interp *I) {
    value_free(&I->return_value);
    env_free(I->globals);
    I->globals = NULL;
}

int interp_run(Interp *I, Node *program) {
    /* First pass: hoist function, struct and enum declarations into globals */
    for (size_t i = 0; i < program->as.block.stmts.count; i++) {
        Node *s = program->as.block.stmts.items[i];
        if (s->kind == ST_FUNC_DECL) {
            Value v = {0};
            v.type = V_FUNC;
            v.as.func.decl = s;
            v.as.func.closure = I->globals;
            env_define(I->globals, s->as.func.name, v, true);
        } else if (s->kind == ST_STRUCT_DECL || s->kind == ST_ENUM_DECL) {
            exec(I, I->globals, s);
        }
    }
    /* Execute top-level statements (skip already-hoisted decls). */
    for (size_t i = 0; i < program->as.block.stmts.count; i++) {
        Node *s = program->as.block.stmts.items[i];
        if (s->kind == ST_FUNC_DECL || s->kind == ST_STRUCT_DECL || s->kind == ST_ENUM_DECL) continue;
        exec(I, I->globals, s);
        if (I->return_flag) break;
    }
    /* If a 'main' function exists, call it. */
    bool found = false;
    Value mainfn = env_get(I->globals, "main", &found);
    int exit_code = 0;
    if (found && (mainfn.type == V_FUNC || mainfn.type == V_BUILTIN)) {
        Value r = call_value(I, mainfn, 0, NULL, 0);
        if (r.type == V_INT) exit_code = (int)r.as.i;
        value_free(&r);
    }
    value_free(&mainfn);
    return exit_code;
}
