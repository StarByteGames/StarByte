#include "codegen.h"
#include "common.h"
#include <errno.h>

/* ============================================================
 *  StarByte native backend (transpiles AST to C, then invokes
 *  the system C compiler).
 * ============================================================ */

/* ---------- runtime: emitted at the top of every generated file ---------- */
static const char *RUNTIME_C =
"/* --- StarByte runtime --- */\n"
"#include <stdio.h>\n"
"#include <stdlib.h>\n"
"#include <string.h>\n"
"#include <stdarg.h>\n"
"#include <math.h>\n"
"\n"
"typedef enum { SB_NULL, SB_INT, SB_FLOAT, SB_BOOL, SB_CHAR, SB_STRING } sb_type;\n"
"\n"
"typedef struct {\n"
"    sb_type t;\n"
"    union {\n"
"        long long i;\n"
"        double    f;\n"
"        int       b;\n"
"        char      c;\n"
"        char     *s;\n"
"    } v;\n"
"} sb_value;\n"
"\n"
"static void sb_oom(void){ fprintf(stderr,\"starbyte: out of memory\\n\"); exit(1); }\n"
"static void *sb_xmalloc(size_t n){ void *p=malloc(n); if(!p) sb_oom(); return p; }\n"
"static char *sb_strdup_(const char *s){ size_t n=strlen(s)+1; char *r=sb_xmalloc(n); memcpy(r,s,n); return r; }\n"
"\n"
"static sb_value sb_null(void){ sb_value v; v.t=SB_NULL; v.v.i=0; return v; }\n"
"static sb_value sb_int(long long i){ sb_value v; v.t=SB_INT; v.v.i=i; return v; }\n"
"static sb_value sb_float(double f){ sb_value v; v.t=SB_FLOAT; v.v.f=f; return v; }\n"
"static sb_value sb_bool(int b){ sb_value v; v.t=SB_BOOL; v.v.b=b?1:0; return v; }\n"
"static sb_value sb_char_(char c){ sb_value v; v.t=SB_CHAR; v.v.c=c; return v; }\n"
"static sb_value sb_string(const char *s){ sb_value v; v.t=SB_STRING; v.v.s=sb_strdup_(s?s:\"\"); return v; }\n"
"static sb_value sb_string_take(char *s){ sb_value v; v.t=SB_STRING; v.v.s=s; return v; }\n"
"\n"
"static int sb_truthy(sb_value v){\n"
"    switch(v.t){\n"
"        case SB_NULL: return 0;\n"
"        case SB_BOOL: return v.v.b;\n"
"        case SB_INT:  return v.v.i!=0;\n"
"        case SB_FLOAT:return v.v.f!=0.0;\n"
"        case SB_CHAR: return v.v.c!=0;\n"
"        case SB_STRING:return v.v.s && v.v.s[0];\n"
"    }\n"
"    return 0;\n"
"}\n"
"\n"
"static char *sb_to_cstr(sb_value v){\n"
"    char buf[64];\n"
"    switch(v.t){\n"
"        case SB_NULL: return sb_strdup_(\"null\");\n"
"        case SB_BOOL: return sb_strdup_(v.v.b?\"true\":\"false\");\n"
"        case SB_INT:  snprintf(buf,sizeof buf,\"%lld\",v.v.i); return sb_strdup_(buf);\n"
"        case SB_FLOAT:snprintf(buf,sizeof buf,\"%g\",v.v.f); return sb_strdup_(buf);\n"
"        case SB_CHAR: { char b[2]={v.v.c,0}; return sb_strdup_(b); }\n"
"        case SB_STRING:return sb_strdup_(v.v.s?v.v.s:\"\");\n"
"    }\n"
"    return sb_strdup_(\"\");\n"
"}\n"
"\n"
"static double sb_to_num(sb_value v){\n"
"    switch(v.t){\n"
"        case SB_INT: return (double)v.v.i;\n"
"        case SB_FLOAT: return v.v.f;\n"
"        case SB_BOOL: return v.v.b?1.0:0.0;\n"
"        case SB_CHAR: return (double)(unsigned char)v.v.c;\n"
"        default: return 0.0;\n"
"    }\n"
"}\n"
"static long long sb_to_int(sb_value v){\n"
"    switch(v.t){\n"
"        case SB_INT: return v.v.i;\n"
"        case SB_FLOAT: return (long long)v.v.f;\n"
"        case SB_BOOL: return v.v.b?1:0;\n"
"        case SB_CHAR: return (long long)(unsigned char)v.v.c;\n"
"        default: return 0;\n"
"    }\n"
"}\n"
"static int sb_is_num(sb_value v){\n"
"    return v.t==SB_INT||v.t==SB_FLOAT||v.t==SB_BOOL||v.t==SB_CHAR;\n"
"}\n"
"static int sb_both_int(sb_value a, sb_value b){\n"
"    return (a.t==SB_INT||a.t==SB_BOOL||a.t==SB_CHAR) &&\n"
"           (b.t==SB_INT||b.t==SB_BOOL||b.t==SB_CHAR);\n"
"}\n"
"\n"
"static void sb_die(const char *msg){ fprintf(stderr,\"runtime error: %s\\n\", msg); exit(1); }\n"
"\n"
"static sb_value sb_add(sb_value a, sb_value b){\n"
"    if (a.t==SB_STRING || b.t==SB_STRING){\n"
"        char *sa=sb_to_cstr(a), *sb_=sb_to_cstr(b);\n"
"        size_t n=strlen(sa)+strlen(sb_)+1;\n"
"        char *r=sb_xmalloc(n); snprintf(r,n,\"%s%s\",sa,sb_);\n"
"        free(sa); free(sb_);\n"
"        return sb_string_take(r);\n"
"    }\n"
"    if (!sb_is_num(a)||!sb_is_num(b)) sb_die(\"add on non-numeric\");\n"
"    if (sb_both_int(a,b)) return sb_int(sb_to_int(a)+sb_to_int(b));\n"
"    return sb_float(sb_to_num(a)+sb_to_num(b));\n"
"}\n"
"static sb_value sb_sub(sb_value a, sb_value b){\n"
"    if(!sb_is_num(a)||!sb_is_num(b)) sb_die(\"sub on non-numeric\");\n"
"    if(sb_both_int(a,b)) return sb_int(sb_to_int(a)-sb_to_int(b));\n"
"    return sb_float(sb_to_num(a)-sb_to_num(b));\n"
"}\n"
"static sb_value sb_mul(sb_value a, sb_value b){\n"
"    if(!sb_is_num(a)||!sb_is_num(b)) sb_die(\"mul on non-numeric\");\n"
"    if(sb_both_int(a,b)) return sb_int(sb_to_int(a)*sb_to_int(b));\n"
"    return sb_float(sb_to_num(a)*sb_to_num(b));\n"
"}\n"
"static sb_value sb_div(sb_value a, sb_value b){\n"
"    if(!sb_is_num(a)||!sb_is_num(b)) sb_die(\"div on non-numeric\");\n"
"    if(sb_both_int(a,b)){\n"
"        long long y=sb_to_int(b); if(y==0) sb_die(\"division by zero\");\n"
"        return sb_int(sb_to_int(a)/y);\n"
"    }\n"
"    double y=sb_to_num(b); if(y==0.0) sb_die(\"division by zero\");\n"
"    return sb_float(sb_to_num(a)/y);\n"
"}\n"
"static sb_value sb_mod(sb_value a, sb_value b){\n"
"    if(!sb_is_num(a)||!sb_is_num(b)) sb_die(\"mod on non-numeric\");\n"
"    if(sb_both_int(a,b)){\n"
"        long long y=sb_to_int(b); if(y==0) sb_die(\"modulo by zero\");\n"
"        return sb_int(sb_to_int(a)%y);\n"
"    }\n"
"    double y=sb_to_num(b); if(y==0.0) sb_die(\"modulo by zero\");\n"
"    return sb_float(fmod(sb_to_num(a),y));\n"
"}\n"
"\n"
"static int sb_cmp(sb_value a, sb_value b){\n"
"    if(a.t==SB_STRING && b.t==SB_STRING)\n"
"        return strcmp(a.v.s?a.v.s:\"\", b.v.s?b.v.s:\"\");\n"
"    double x=sb_to_num(a), y=sb_to_num(b);\n"
"    if(x<y) return -1; if(x>y) return 1; return 0;\n"
"}\n"
"static int sb_eq_v(sb_value a, sb_value b){\n"
"    if(a.t==SB_NULL || b.t==SB_NULL) return a.t==b.t;\n"
"    if(a.t==SB_STRING || b.t==SB_STRING){\n"
"        if(a.t!=b.t) return 0;\n"
"        return strcmp(a.v.s?a.v.s:\"\", b.v.s?b.v.s:\"\")==0;\n"
"    }\n"
"    return sb_cmp(a,b)==0;\n"
"}\n"
"static sb_value sb_eq(sb_value a, sb_value b){ return sb_bool(sb_eq_v(a,b)); }\n"
"static sb_value sb_neq(sb_value a, sb_value b){ return sb_bool(!sb_eq_v(a,b)); }\n"
"static sb_value sb_lt(sb_value a, sb_value b){ return sb_bool(sb_cmp(a,b)<0); }\n"
"static sb_value sb_gt(sb_value a, sb_value b){ return sb_bool(sb_cmp(a,b)>0); }\n"
"static sb_value sb_le(sb_value a, sb_value b){ return sb_bool(sb_cmp(a,b)<=0); }\n"
"static sb_value sb_ge(sb_value a, sb_value b){ return sb_bool(sb_cmp(a,b)>=0); }\n"
"\n"
"static sb_value sb_neg(sb_value a){\n"
"    if(a.t==SB_FLOAT) return sb_float(-a.v.f);\n"
"    return sb_int(-sb_to_int(a));\n"
"}\n"
"static sb_value sb_pos(sb_value a){ return a; }\n"
"static sb_value sb_not(sb_value a){ return sb_bool(!sb_truthy(a)); }\n"
"\n"
"/* postfix ++ / --  : returns OLD value, mutates *p */\n"
"static sb_value sb_postinc(sb_value *p){\n"
"    sb_value old=*p;\n"
"    long long iv=sb_to_int(*p);\n"
"    if(p->t==SB_FLOAT) *p=sb_float((double)(iv+1));\n"
"    else *p=sb_int(iv+1);\n"
"    return old;\n"
"}\n"
"static sb_value sb_postdec(sb_value *p){\n"
"    sb_value old=*p;\n"
"    long long iv=sb_to_int(*p);\n"
"    if(p->t==SB_FLOAT) *p=sb_float((double)(iv-1));\n"
"    else *p=sb_int(iv-1);\n"
"    return old;\n"
"}\n"
"\n"
"/* ----- builtins ----- */\n"
"\n"
"static sb_value sb_println(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    for(int i=0;i<argc;i++){\n"
"        if(i) fputc(' ',stdout);\n"
"        sb_value v = va_arg(ap, sb_value);\n"
"        char *s = sb_to_cstr(v); fputs(s,stdout); free(s);\n"
"    }\n"
"    va_end(ap);\n"
"    fputc('\\n',stdout);\n"
"    return sb_null();\n"
"}\n"
"static sb_value sb_print(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    for(int i=0;i<argc;i++){\n"
"        sb_value v = va_arg(ap, sb_value);\n"
"        char *s = sb_to_cstr(v); fputs(s,stdout); free(s);\n"
"    }\n"
"    va_end(ap);\n"
"    return sb_null();\n"
"}\n"
"static sb_value sb_readline(int argc, ...){\n"
"    (void)argc;\n"
"    size_t cap=64,len=0; char *buf=sb_xmalloc(cap);\n"
"    int c;\n"
"    while((c=fgetc(stdin))!=EOF && c!='\\n'){\n"
"        if(len+1>=cap){ cap*=2; buf=realloc(buf,cap); if(!buf) sb_oom(); }\n"
"        buf[len++]=(char)c;\n"
"    }\n"
"    buf[len]='\\0';\n"
"    return sb_string_take(buf);\n"
"}\n"
"\n"
"static sb_value sb_math_sqrt(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    sb_value v = (argc>0)?va_arg(ap, sb_value):sb_int(0);\n"
"    va_end(ap);\n"
"    return sb_float(sqrt(sb_to_num(v)));\n"
"}\n"
"static sb_value sb_math_abs(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    sb_value v = (argc>0)?va_arg(ap, sb_value):sb_int(0);\n"
"    va_end(ap);\n"
"    if(v.t==SB_INT) return sb_int(v.v.i<0?-v.v.i:v.v.i);\n"
"    return sb_float(fabs(sb_to_num(v)));\n"
"}\n"
"static sb_value sb_math_pow(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    sb_value a = (argc>0)?va_arg(ap, sb_value):sb_int(0);\n"
"    sb_value b = (argc>1)?va_arg(ap, sb_value):sb_int(0);\n"
"    va_end(ap);\n"
"    return sb_float(pow(sb_to_num(a), sb_to_num(b)));\n"
"}\n"
"static sb_value sb_strings_length(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    sb_value v = (argc>0)?va_arg(ap, sb_value):sb_string(\"\");\n"
"    va_end(ap);\n"
"    if(v.t!=SB_STRING) return sb_int(0);\n"
"    return sb_int((long long)strlen(v.v.s?v.v.s:\"\"));\n"
"}\n"
"static sb_value sb_strings_concat(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    /* two passes */\n"
"    size_t total=1;\n"
"    sb_value *vals = sb_xmalloc(sizeof(sb_value)*(size_t)argc);\n"
"    for(int i=0;i<argc;i++){\n"
"        vals[i] = va_arg(ap, sb_value);\n"
"        char *t = sb_to_cstr(vals[i]); total += strlen(t); free(t);\n"
"    }\n"
"    va_end(ap);\n"
"    char *out = sb_xmalloc(total); out[0]='\\0';\n"
"    for(int i=0;i<argc;i++){\n"
"        char *t = sb_to_cstr(vals[i]); strcat(out,t); free(t);\n"
"    }\n"
"    free(vals);\n"
"    return sb_string_take(out);\n"
"}\n"
"\n"
"/* --- end runtime --- */\n";

/* ============================================================
 *  Helpers
 * ============================================================ */

typedef struct {
    FILE *out;
    int   indent;
    const char *src;
    bool  has_main;
} Cg;

static void cg_indent(Cg *g) { for (int i = 0; i < g->indent; i++) fputs("    ", g->out); }
static void cg_die(const char *msg, int line) {
    fprintf(stderr, "starbyte codegen: %s (line %d)\n", msg, line);
    exit(1);
}

static void cg_emit_c_string(FILE *f, const char *s) {
    fputc('"', f);
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '\\': fputs("\\\\", f); break;
            case '"':  fputs("\\\"", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (c < 0x20) fprintf(f, "\\x%02x", c);
                else fputc((int)c, f);
        }
    }
    fputc('"', f);
}

/* dotted identifier path used for builtin/namespace calls */
static char *dotted_from_member(Node *n) {
    if (n->kind == EX_IDENT) return sb_strdup(n->as.ident.name);
    if (n->kind != EX_MEMBER) return NULL;
    char *base = dotted_from_member(n->as.member.object);
    if (!base) return NULL;
    size_t len = strlen(base) + 1 + strlen(n->as.member.name) + 1;
    char *r = (char*)sb_xmalloc(len);
    snprintf(r, len, "%s.%s", base, n->as.member.name);
    free(base);
    return r;
}

/* Map a dotted call target to a runtime C function (variadic).
   Returns NULL if not a known builtin. */
static const char *map_builtin(const char *dotted) {
    /* Console / System.Console */
    if (!strcmp(dotted, "Console.WriteLine") || !strcmp(dotted, "System.Console.WriteLine")) return "sb_println";
    if (!strcmp(dotted, "Console.Write")     || !strcmp(dotted, "System.Console.Write"))     return "sb_print";
    if (!strcmp(dotted, "Console.ReadLine")  || !strcmp(dotted, "System.Console.ReadLine"))  return "sb_readline";
    /* Math */
    if (!strcmp(dotted, "Math.sqrt")|| !strcmp(dotted, "System.Math.sqrt")) return "sb_math_sqrt";
    if (!strcmp(dotted, "Math.abs") || !strcmp(dotted, "System.Math.abs"))  return "sb_math_abs";
    if (!strcmp(dotted, "Math.pow") || !strcmp(dotted, "System.Math.pow"))  return "sb_math_pow";
    /* Strings */
    if (!strcmp(dotted, "Strings.length")|| !strcmp(dotted, "System.Strings.length")) return "sb_strings_length";
    if (!strcmp(dotted, "Strings.concat")|| !strcmp(dotted, "System.Strings.concat")) return "sb_strings_concat";
    /* globals */
    if (!strcmp(dotted, "println")) return "sb_println";
    if (!strcmp(dotted, "print"))   return "sb_print";
    return NULL;
}

/* ============================================================
 *  Expression / statement emitters
 * ============================================================ */

static void cg_expr(Cg *g, Node *n);
static void cg_stmt(Cg *g, Node *n);

static const char *binop_fn(OpKind op) {
    switch (op) {
        case OP_ADD: return "sb_add";
        case OP_SUB: return "sb_sub";
        case OP_MUL: return "sb_mul";
        case OP_DIV: return "sb_div";
        case OP_MOD: return "sb_mod";
        case OP_EQ:  return "sb_eq";
        case OP_NEQ: return "sb_neq";
        case OP_LT:  return "sb_lt";
        case OP_GT:  return "sb_gt";
        case OP_LE:  return "sb_le";
        case OP_GE:  return "sb_ge";
        default:     return NULL;
    }
}

static void cg_call(Cg *g, Node *n) {
    Node *callee = n->as.call.callee;
    int argc = (int)n->as.call.args.count;

    /* Member call (builtin or namespace) */
    if (callee->kind == EX_MEMBER || callee->kind == EX_IDENT) {
        char *dotted = dotted_from_member(callee);
        if (dotted) {
            const char *fn = map_builtin(dotted);
            if (fn) {
                fprintf(g->out, "%s(%d", fn, argc);
                for (int i = 0; i < argc; i++) {
                    fputs(", ", g->out);
                    cg_expr(g, n->as.call.args.items[i]);
                }
                fputc(')', g->out);
                free(dotted);
                return;
            }
            /* user-defined function: must be EX_IDENT */
            if (callee->kind == EX_IDENT) {
                fprintf(g->out, "sb_fn_%s(", dotted);
                for (int i = 0; i < argc; i++) {
                    if (i) fputs(", ", g->out);
                    cg_expr(g, n->as.call.args.items[i]);
                }
                fputc(')', g->out);
                free(dotted);
                return;
            }
            fprintf(stderr, "starbyte codegen: unknown function or namespace '%s' (line %d)\n",
                    dotted, n->line);
            free(dotted);
            exit(1);
        }
    }
    cg_die("unsupported call form", n->line);
}

static void cg_expr(Cg *g, Node *n) {
    if (!n) { fputs("sb_null()", g->out); return; }
    switch (n->kind) {
        case EX_INT:    fprintf(g->out, "sb_int(%lldLL)", n->as.i); break;
        case EX_FLOAT:  fprintf(g->out, "sb_float(%.17g)", n->as.f); break;
        case EX_BOOL:   fprintf(g->out, "sb_bool(%d)", n->as.b ? 1 : 0); break;
        case EX_NULL:   fputs("sb_null()", g->out); break;
        case EX_CHAR:   fprintf(g->out, "sb_char_((char)%lld)", n->as.i); break;
        case EX_STRING: fputs("sb_string(", g->out); cg_emit_c_string(g->out, n->as.s); fputc(')', g->out); break;
        case EX_IDENT:  fprintf(g->out, "%s", n->as.ident.name); break;
        case EX_BINARY: {
            const char *fn = binop_fn(n->as.binary.op);
            if (!fn) cg_die("unsupported binary op", n->line);
            fprintf(g->out, "%s(", fn);
            cg_expr(g, n->as.binary.lhs); fputs(", ", g->out);
            cg_expr(g, n->as.binary.rhs); fputc(')', g->out);
            break;
        }
        case EX_LOGICAL: {
            /* short-circuit via C && / || on sb_truthy */
            const char *cop = (n->as.logical.op == OP_AND) ? "&&" : "||";
            fputs("sb_bool((sb_truthy(", g->out);
            cg_expr(g, n->as.logical.lhs);
            fprintf(g->out, ")) %s (sb_truthy(", cop);
            cg_expr(g, n->as.logical.rhs);
            fputs(")))", g->out);
            break;
        }
        case EX_UNARY: {
            const char *fn =
                (n->as.unary.op == OP_NEG) ? "sb_neg" :
                (n->as.unary.op == OP_POS) ? "sb_pos" :
                (n->as.unary.op == OP_NOT) ? "sb_not" : NULL;
            if (!fn) cg_die("unsupported unary op", n->line);
            fprintf(g->out, "%s(", fn);
            cg_expr(g, n->as.unary.operand);
            fputc(')', g->out);
            break;
        }
        case EX_POSTFIX: {
            if (n->as.postfix.operand->kind != EX_IDENT)
                cg_die("++/-- requires variable", n->line);
            fprintf(g->out, "%s(&%s)",
                    (n->as.postfix.op == OP_INC) ? "sb_postinc" : "sb_postdec",
                    n->as.postfix.operand->as.ident.name);
            break;
        }
        case EX_ASSIGN: {
            Node *t = n->as.assign.target;
            if (t->kind != EX_IDENT) cg_die("assign target must be variable", n->line);
            if (n->as.assign.is_compound) {
                const char *fn = binop_fn(n->as.assign.compound);
                if (!fn) cg_die("unsupported compound op", n->line);
                fprintf(g->out, "(%s = %s(%s, ", t->as.ident.name, fn, t->as.ident.name);
                cg_expr(g, n->as.assign.value);
                fputs("))", g->out);
            } else {
                fprintf(g->out, "(%s = ", t->as.ident.name);
                cg_expr(g, n->as.assign.value);
                fputc(')', g->out);
            }
            break;
        }
        case EX_CALL:   cg_call(g, n); break;
        case EX_MEMBER: cg_die("bare member access not supported in native backend", n->line); break;
        case EX_STRUCT_LIT:
            cg_die("struct/brace initializers are not supported by the native backend yet (use --run)", n->line);
            break;
        default:        cg_die("unsupported expression", n->line);
    }
}

static void cg_stmt(Cg *g, Node *n) {
    if (!n) return;
    switch (n->kind) {
        case ST_EXPR:
            cg_indent(g);
            if (n->as.expr_stmt.expr) {
                fputs("(void)(", g->out);
                cg_expr(g, n->as.expr_stmt.expr);
                fputs(");\n", g->out);
            } else {
                fputs(";\n", g->out);
            }
            break;
        case ST_BLOCK: {
            cg_indent(g); fputs("{\n", g->out);
            g->indent++;
            for (size_t i = 0; i < n->as.block.stmts.count; i++)
                cg_stmt(g, n->as.block.stmts.items[i]);
            g->indent--;
            cg_indent(g); fputs("}\n", g->out);
            break;
        }
        case ST_VAR_DECL:
            cg_indent(g);
            fprintf(g->out, "sb_value %s = ", n->as.var_decl.name);
            if (n->as.var_decl.init) cg_expr(g, n->as.var_decl.init);
            else fputs("sb_null()", g->out);
            fputs(";\n", g->out);
            break;
        case ST_IF:
            cg_indent(g); fputs("if (sb_truthy(", g->out);
            cg_expr(g, n->as.if_stmt.cond);
            fputs(")) {\n", g->out);
            g->indent++; cg_stmt(g, n->as.if_stmt.then_branch); g->indent--;
            cg_indent(g); fputs("}", g->out);
            if (n->as.if_stmt.else_branch) {
                fputs(" else {\n", g->out);
                g->indent++; cg_stmt(g, n->as.if_stmt.else_branch); g->indent--;
                cg_indent(g); fputs("}\n", g->out);
            } else {
                fputc('\n', g->out);
            }
            break;
        case ST_WHILE:
            cg_indent(g); fputs("while (sb_truthy(", g->out);
            cg_expr(g, n->as.while_stmt.cond);
            fputs(")) {\n", g->out);
            g->indent++; cg_stmt(g, n->as.while_stmt.body); g->indent--;
            cg_indent(g); fputs("}\n", g->out);
            break;
        case ST_FOR: {
            /* emulate as { init; while(cond) { body; post; } } */
            cg_indent(g); fputs("{\n", g->out);
            g->indent++;
            if (n->as.for_stmt.init) cg_stmt(g, n->as.for_stmt.init);
            cg_indent(g); fputs("while (", g->out);
            if (n->as.for_stmt.cond) {
                fputs("sb_truthy(", g->out);
                cg_expr(g, n->as.for_stmt.cond);
                fputc(')', g->out);
            } else {
                fputc('1', g->out);
            }
            fputs(") {\n", g->out);
            g->indent++;
            cg_stmt(g, n->as.for_stmt.body);
            if (n->as.for_stmt.post) {
                cg_indent(g);
                fputs("(void)(", g->out);
                cg_expr(g, n->as.for_stmt.post);
                fputs(");\n", g->out);
            }
            g->indent--;
            cg_indent(g); fputs("}\n", g->out);
            g->indent--;
            cg_indent(g); fputs("}\n", g->out);
            break;
        }
        case ST_RETURN:
            cg_indent(g); fputs("return ", g->out);
            if (n->as.ret.value) cg_expr(g, n->as.ret.value);
            else fputs("sb_null()", g->out);
            fputs(";\n", g->out);
            break;
        case ST_BREAK:    cg_indent(g); fputs("break;\n", g->out); break;
        case ST_CONTINUE: cg_indent(g); fputs("continue;\n", g->out); break;
        case ST_FUNC_DECL:
            /* Definitions are emitted at top-level, not inside other blocks. */
            cg_die("nested function definitions are not supported", n->line);
            break;
        case ST_MODULE:   /* nothing in emitted C */ break;
        case ST_STRUCT_DECL:
        case ST_ENUM_DECL:
            /* not yet supported by native backend; silently skip top-level */
            break;
        default:          cg_die("unsupported statement", n->line);
    }
}

/* ============================================================
 *  Top-level emission
 * ============================================================ */

static void cg_emit_func_proto(Cg *g, Node *fn) {
    fprintf(g->out, "sb_value sb_fn_%s(", fn->as.func.name);
    if (fn->as.func.param_count == 0) fputs("void", g->out);
    else {
        for (size_t i = 0; i < fn->as.func.param_count; i++) {
            if (i) fputs(", ", g->out);
            fprintf(g->out, "sb_value %s", fn->as.func.params[i].name);
        }
    }
    fputc(')', g->out);
}

static void cg_emit_func_def(Cg *g, Node *fn) {
    cg_emit_func_proto(g, fn);
    fputs(" {\n", g->out);
    g->indent = 1;
    /* fn body is a ST_BLOCK; emit its statements directly (no extra braces) */
    if (fn->as.func.body && fn->as.func.body->kind == ST_BLOCK) {
        for (size_t i = 0; i < fn->as.func.body->as.block.stmts.count; i++)
            cg_stmt(g, fn->as.func.body->as.block.stmts.items[i]);
    }
    /* default return so non-void / void functions both compile */
    cg_indent(g); fputs("return sb_null();\n", g->out);
    g->indent = 0;
    fputs("}\n\n", g->out);
}

int codegen_emit_c(Node *program, const char *c_out_path, const char *src_filename) {
    FILE *f = fopen(c_out_path, "w");
    if (!f) {
        fprintf(stderr, "starbyte: cannot write '%s': %s\n", c_out_path, strerror(errno));
        return 1;
    }
    Cg g = {0};
    g.out = f;
    g.src = src_filename;

    fprintf(f, "/* Generated by StarByte from %s */\n",
            src_filename ? src_filename : "<input>");
    fputs(RUNTIME_C, f);
    fputs("\n", f);

    /* forward declarations + detect main */
    for (size_t i = 0; i < program->as.block.stmts.count; i++) {
        Node *s = program->as.block.stmts.items[i];
        if (s->kind == ST_FUNC_DECL) {
            cg_emit_func_proto(&g, s);
            fputs(";\n", f);
            if (strcmp(s->as.func.name, "main") == 0) g.has_main = true;
        }
    }
    fputs("\n", f);

    /* function definitions */
    for (size_t i = 0; i < program->as.block.stmts.count; i++) {
        Node *s = program->as.block.stmts.items[i];
        if (s->kind == ST_FUNC_DECL) cg_emit_func_def(&g, s);
    }

    /* top-level statements (everything that's not a func or module) */
    fputs("static void sb_top_level(void) {\n", f);
    g.indent = 1;
    for (size_t i = 0; i < program->as.block.stmts.count; i++) {
        Node *s = program->as.block.stmts.items[i];
        if (s->kind == ST_FUNC_DECL || s->kind == ST_MODULE) continue;
        cg_stmt(&g, s);
    }
    g.indent = 0;
    fputs("}\n\n", f);

    /* C entry point */
    fputs("int main(int argc, char **argv) {\n", f);
    fputs("    (void)argc; (void)argv;\n", f);
    fputs("    sb_top_level();\n", f);
    if (g.has_main) {
        fputs("    sb_value r = sb_fn_main();\n", f);
        fputs("    if (r.t == SB_INT) return (int)r.v.i;\n", f);
    }
    fputs("    return 0;\n", f);
    fputs("}\n", f);

    fclose(f);
    return 0;
}

/* ============================================================
 *  Compile generated C with the system compiler
 * ============================================================ */

static int has_space(const char *s) { for (; *s; s++) if (*s == ' ' || *s == '\t') return 1; return 0; }

int codegen_compile_c(const char *c_path, const char *exe_path, const char *cc_override) {
    const char *cc = cc_override;
    if (!cc) cc = getenv("CC");
    if (!cc || !*cc) cc = "cc";

    /* simple shell-quoting: wrap each path in single quotes, escape any single quotes */
    char cmd[4096];
    int n = snprintf(cmd, sizeof cmd,
        "%s -O2 -std=c11 -o '%s' '%s' -lm",
        cc, exe_path, c_path);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        fprintf(stderr, "starbyte: compile command too long\n");
        return 1;
    }
    /* refuse paths with single quotes to keep things sane */
    if (strchr(c_path, '\'') || strchr(exe_path, '\'')) {
        fprintf(stderr, "starbyte: paths containing single quotes are not supported\n");
        return 1;
    }
    if (has_space(cc)) { /* allow CC like "gcc -m64": leave as-is, no quoting */ }

    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "starbyte: C compiler failed (cmd: %s)\n", cmd);
        return rc;
    }
    return 0;
}
