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
"#include <stddef.h>\n"
"\n"
"typedef enum { SB_NULL, SB_INT, SB_FLOAT, SB_BOOL, SB_CHAR, SB_STRING, SB_STRUCT, SB_BUFFER } sb_type;\n"
"\n"
"struct sb_struct;\n"
"struct sb_buffer;\n"
"\n"
"typedef struct {\n"
"    sb_type t;\n"
"    union {\n"
"        long long i;\n"
"        double    f;\n"
"        int       b;\n"
"        char      c;\n"
"        char     *s;\n"
"        struct sb_struct *st;\n"
"        struct sb_buffer *bf;\n"
"    } v;\n"
"} sb_value;\n"
"\n"
"typedef struct sb_field { char *name; sb_value v; } sb_field;\n"
"typedef struct sb_struct {\n"
"    char *type_name;\n"
"    int   n;\n"
"    sb_field *f;\n"
"} sb_struct;\n"
"\n"
"typedef struct sb_buffer {\n"
"    size_t n;\n"
"    sb_value *items;\n"
"    int gc_managed;\n"
"    int freed;\n"
"    int gc_mark;\n"
"    struct sb_buffer *gc_next;\n"
"} sb_buffer;\n"
"\n"
"static sb_buffer *sb_gc_head = NULL;\n"
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
"        case SB_STRUCT:return v.v.st!=0;\n"
"        case SB_BUFFER:return v.v.bf!=0 && !v.v.bf->freed;\n"
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
"        case SB_STRUCT: {\n"
"            sb_struct *st = v.v.st;\n"
"            size_t cap = 64, len = 0;\n"
"            char *out = sb_xmalloc(cap);\n"
"            #define SBAPP(s) do { const char *_s=(s); size_t _l=strlen(_s); if(len+_l+1>cap){cap=(len+_l+1)*2; out=realloc(out,cap); if(!out) sb_oom();} memcpy(out+len,_s,_l); len+=_l; out[len]='\\0'; } while(0)\n"
"            SBAPP(st && st->type_name ? st->type_name : \"struct\");\n"
"            SBAPP(\"{\");\n"
"            if (st) {\n"
"                for (int i = 0; i < st->n; i++) {\n"
"                    if (i) SBAPP(\", \");\n"
"                    SBAPP(st->f[i].name ? st->f[i].name : \"?\");\n"
"                    SBAPP(\"=\");\n"
"                    char *fs = sb_to_cstr(st->f[i].v); SBAPP(fs); free(fs);\n"
"                }\n"
"            }\n"
"            SBAPP(\"}\");\n"
"            #undef SBAPP\n"
"            return out;\n"
"        }\n"
"        case SB_BUFFER: {\n"
"            sb_buffer *b = v.v.bf;\n"
"            if (!b) return sb_strdup_(\"<buffer:null>\");\n"
"            if (b->freed) return sb_strdup_(\"<buffer:freed>\");\n"
"            size_t cap=32,len=0;\n"
"            char *out=sb_xmalloc(cap);\n"
"            #define SBAPP2(s) do { const char *_s=(s); size_t _l=strlen(_s); if(len+_l+1>cap){cap=(len+_l+1)*2; out=realloc(out,cap); if(!out) sb_oom();} memcpy(out+len,_s,_l); len+=_l; out[len]='\\0'; } while(0)\n"
"            SBAPP2(\"[\");\n"
"            for (size_t i=0;i<b->n;i++){\n"
"                if (i) SBAPP2(\", \");\n"
"                char *fs=sb_to_cstr(b->items[i]); SBAPP2(fs); free(fs);\n"
"            }\n"
"            SBAPP2(\"]\");\n"
"            #undef SBAPP2\n"
"            return out;\n"
"        }\n"
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
"/* ----- memory management ----- */\n"
"\n"
"static sb_buffer *sb_buffer_new(size_t n){\n"
"    sb_buffer *b = sb_xmalloc(sizeof(sb_buffer));\n"
"    b->n = n;\n"
"    b->items = n ? sb_xmalloc(sizeof(sb_value)*n) : NULL;\n"
"    for (size_t i=0;i<n;i++) b->items[i] = sb_null();\n"
"    b->gc_managed = 0;\n"
"    b->freed = 0;\n"
"    b->gc_mark = 0;\n"
"    b->gc_next = NULL;\n"
"    return b;\n"
"}\n"
"static void sb_buffer_free_contents(sb_buffer *b){\n"
"    if (!b || b->freed) return;\n"
"    if (b->items){ free(b->items); b->items = NULL; }\n"
"    b->n = 0;\n"
"    b->freed = 1;\n"
"}\n"
"static sb_value sb_alloc_(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    sb_value v = (argc>0)?va_arg(ap,sb_value):sb_int(0);\n"
"    va_end(ap);\n"
"    long long n = sb_to_int(v); if (n<0) n=0;\n"
"    sb_buffer *b = sb_buffer_new((size_t)n);\n"
"    sb_value r; r.t=SB_BUFFER; r.v.bf=b; return r;\n"
"}\n"
"static sb_value sb_free_(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    sb_value v = (argc>0)?va_arg(ap,sb_value):sb_null();\n"
"    va_end(ap);\n"
"    if (v.t!=SB_BUFFER || !v.v.bf) sb_die(\"free() expects a buffer\");\n"
"    if (v.v.bf->gc_managed) sb_die(\"cannot free() a GC-managed buffer; use gc_collect()\");\n"
"    sb_buffer_free_contents(v.v.bf);\n"
"    free(v.v.bf);\n"
"    return sb_null();\n"
"}\n"
"static sb_value sb_gc_alloc_(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    sb_value v = (argc>0)?va_arg(ap,sb_value):sb_int(0);\n"
"    va_end(ap);\n"
"    long long n = sb_to_int(v); if (n<0) n=0;\n"
"    sb_buffer *b = sb_buffer_new((size_t)n);\n"
"    b->gc_managed = 1;\n"
"    b->gc_next = sb_gc_head;\n"
"    sb_gc_head = b;\n"
"    sb_value r; r.t=SB_BUFFER; r.v.bf=b; return r;\n"
"}\n"
"/* The codegen GC has no roots to walk -- variables are real C locals.\n"
"   gc_collect() therefore frees every GC-managed buffer that is currently\n"
"   in the GC list and not flagged via gc_mark. We provide gc_collect()\n"
"   anyway so programs are portable between the interpreter and the\n"
"   native backend; users who want manual control should use alloc/free. */\n"
"static sb_value sb_gc_collect_(int argc, ...){\n"
"    (void)argc;\n"
"    long long freed = 0;\n"
"    sb_buffer *prev=NULL, *cur=sb_gc_head;\n"
"    while (cur){\n"
"        sb_buffer *next = cur->gc_next;\n"
"        if (!cur->gc_mark){\n"
"            if (prev) prev->gc_next = next; else sb_gc_head = next;\n"
"            sb_buffer_free_contents(cur);\n"
"            free(cur);\n"
"            freed++;\n"
"        } else {\n"
"            cur->gc_mark = 0;\n"
"            prev = cur;\n"
"        }\n"
"        cur = next;\n"
"    }\n"
"    return sb_int(freed);\n"
"}\n"
"static sb_value sb_len_(int argc, ...){\n"
"    va_list ap; va_start(ap,argc);\n"
"    sb_value v = (argc>0)?va_arg(ap,sb_value):sb_null();\n"
"    va_end(ap);\n"
"    if (v.t==SB_BUFFER && v.v.bf) return sb_int((long long)v.v.bf->n);\n"
"    if (v.t==SB_STRING) return sb_int((long long)strlen(v.v.s?v.v.s:\"\"));\n"
"    return sb_int(0);\n"
"}\n"
"static sb_value sb_index_get(sb_value b, sb_value idx){\n"
"    if (b.t!=SB_BUFFER || !b.v.bf) sb_die(\"index target is not a buffer\");\n"
"    if (b.v.bf->freed) sb_die(\"buffer has been freed\");\n"
"    long long i = sb_to_int(idx);\n"
"    if (i<0 || (size_t)i>=b.v.bf->n) sb_die(\"buffer index out of bounds\");\n"
"    return b.v.bf->items[i];\n"
"}\n"
"static sb_value sb_index_set(sb_value b, sb_value idx, sb_value val){\n"
"    if (b.t!=SB_BUFFER || !b.v.bf) sb_die(\"index target is not a buffer\");\n"
"    if (b.v.bf->freed) sb_die(\"buffer has been freed\");\n"
"    long long i = sb_to_int(idx);\n"
"    if (i<0 || (size_t)i>=b.v.bf->n) sb_die(\"buffer index out of bounds\");\n"
"    b.v.bf->items[i] = val;\n"
"    return val;\n"
"}\n"
"\n"
"/* ----- struct helpers ----- */\n"
"\n"
"static sb_value sb_struct_new(const char *type_name, int n, ...) {\n"
"    sb_struct *s = sb_xmalloc(sizeof(sb_struct));\n"
"    s->type_name = sb_strdup_(type_name?type_name:\"\");\n"
"    s->n = n;\n"
"    s->f = n ? sb_xmalloc(sizeof(sb_field)*(size_t)n) : NULL;\n"
"    va_list ap; va_start(ap, n);\n"
"    for (int i = 0; i < n; i++) {\n"
"        const char *fn = va_arg(ap, const char*);\n"
"        sb_value     fv = va_arg(ap, sb_value);\n"
"        s->f[i].name = sb_strdup_(fn?fn:\"\");\n"
"        s->f[i].v    = fv;\n"
"    }\n"
"    va_end(ap);\n"
"    sb_value r; r.t = SB_STRUCT; r.v.st = s; return r;\n"
"}\n"
"static sb_value sb_struct_get(sb_value sv, const char *name) {\n"
"    if (sv.t != SB_STRUCT || !sv.v.st) sb_die(\"field access on non-struct value\");\n"
"    for (int i = 0; i < sv.v.st->n; i++)\n"
"        if (strcmp(sv.v.st->f[i].name, name)==0) return sv.v.st->f[i].v;\n"
"    sb_die(\"unknown struct field\");\n"
"    return sb_null();\n"
"}\n"
"static sb_value sb_struct_set(sb_value sv, const char *name, sb_value val) {\n"
"    if (sv.t != SB_STRUCT || !sv.v.st) sb_die(\"field assign on non-struct value\");\n"
"    for (int i = 0; i < sv.v.st->n; i++)\n"
"        if (strcmp(sv.v.st->f[i].name, name)==0) { sv.v.st->f[i].v = val; return val; }\n"
"    sb_die(\"unknown struct field\");\n"
"    return sb_null();\n"
"}\n"
"\n"
"/* ----- class / object dispatch ----- */\n"
"\n"
"typedef sb_value (*sb_method_fn)(sb_value, int, sb_value*);\n"
"typedef struct { const char *name; sb_method_fn fn; } sb_method_entry;\n"
"typedef struct {\n"
"    const char *name;\n"
"    const char *parent;            /* may be NULL */\n"
"    int n_methods;\n"
"    const sb_method_entry *methods;\n"
"    sb_method_fn ctor;             /* may be NULL */\n"
"    sb_value (*alloc_init)(void);  /* allocate fresh instance + run field defaults */\n"
"} sb_class_entry;\n"
"\n"
"static const sb_class_entry *sb_class_table = NULL;\n"
"static int sb_class_table_n = 0;\n"
"\n"
"static const sb_class_entry *sb_find_class(const char *name) {\n"
"    if (!name) return NULL;\n"
"    for (int i = 0; i < sb_class_table_n; i++)\n"
"        if (strcmp(sb_class_table[i].name, name)==0) return &sb_class_table[i];\n"
"    return NULL;\n"
"}\n"
"static sb_method_fn sb_lookup_method_from(const char *cls, const char *method) {\n"
"    const sb_class_entry *e = sb_find_class(cls);\n"
"    while (e) {\n"
"        for (int i = 0; i < e->n_methods; i++)\n"
"            if (strcmp(e->methods[i].name, method)==0) return e->methods[i].fn;\n"
"        e = e->parent ? sb_find_class(e->parent) : NULL;\n"
"    }\n"
"    return NULL;\n"
"}\n"
"static sb_value sb_dispatch(sb_value obj, const char *method, int argc, ...) {\n"
"    if (obj.t != SB_STRUCT || !obj.v.st) sb_die(\"method call on non-object value\");\n"
"    sb_method_fn fn = sb_lookup_method_from(obj.v.st->type_name, method);\n"
"    if (!fn) sb_die(\"unknown method\");\n"
"    sb_value *argv = argc ? sb_xmalloc(sizeof(sb_value)*(size_t)argc) : NULL;\n"
"    va_list ap; va_start(ap, argc);\n"
"    for (int i = 0; i < argc; i++) argv[i] = va_arg(ap, sb_value);\n"
"    va_end(ap);\n"
"    sb_value r = fn(obj, argc, argv);\n"
"    free(argv);\n"
"    return r;\n"
"}\n"
"static sb_value sb_dispatch_from(sb_value obj, const char *cls_start, const char *method, int argc, ...) {\n"
"    sb_method_fn fn = sb_lookup_method_from(cls_start, method);\n"
"    if (!fn) sb_die(\"unknown method (super)\");\n"
"    sb_value *argv = argc ? sb_xmalloc(sizeof(sb_value)*(size_t)argc) : NULL;\n"
"    va_list ap; va_start(ap, argc);\n"
"    for (int i = 0; i < argc; i++) argv[i] = va_arg(ap, sb_value);\n"
"    va_end(ap);\n"
"    sb_value r = fn(obj, argc, argv);\n"
"    free(argv);\n"
"    return r;\n"
"}\n"
"static sb_value sb_call_ctor_chain(sb_value this_, const char *cls, int argc, ...) {\n"
"    const sb_class_entry *e = sb_find_class(cls);\n"
"    sb_method_fn ctor = NULL;\n"
"    while (e) { if (e->ctor) { ctor = e->ctor; break; } e = e->parent ? sb_find_class(e->parent) : NULL; }\n"
"    if (!ctor) {\n"
"        if (argc != 0) sb_die(\"no parent constructor accepts arguments\");\n"
"        return sb_null();\n"
"    }\n"
"    sb_value *argv = argc ? sb_xmalloc(sizeof(sb_value)*(size_t)argc) : NULL;\n"
"    va_list ap; va_start(ap, argc);\n"
"    for (int i = 0; i < argc; i++) argv[i] = va_arg(ap, sb_value);\n"
"    va_end(ap);\n"
"    sb_value r = ctor(this_, argc, argv); (void)r;\n"
"    free(argv);\n"
"    return sb_null();\n"
"}\n"
"static sb_value sb_new(const char *cls, int argc, ...) {\n"
"    const sb_class_entry *e = sb_find_class(cls);\n"
"    if (!e) sb_die(\"unknown class\");\n"
"    sb_value obj = e->alloc_init();\n"
"    const sb_class_entry *ce = e;\n"
"    sb_method_fn ctor = NULL;\n"
"    while (ce) { if (ce->ctor) { ctor = ce->ctor; break; } ce = ce->parent ? sb_find_class(ce->parent) : NULL; }\n"
"    if (ctor) {\n"
"        sb_value *argv = argc ? sb_xmalloc(sizeof(sb_value)*(size_t)argc) : NULL;\n"
"        va_list ap; va_start(ap, argc);\n"
"        for (int i = 0; i < argc; i++) argv[i] = va_arg(ap, sb_value);\n"
"        va_end(ap);\n"
"        sb_value r = ctor(obj, argc, argv); (void)r;\n"
"        free(argv);\n"
"    } else if (argc != 0) {\n"
"        sb_die(\"class has no constructor but was called with arguments\");\n"
"    }\n"
"    return obj;\n"
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

    /* tracked declarations for struct/enum support */
    Node      **structs;        /* ST_STRUCT_DECL nodes */
    size_t      struct_count;
    /* class declarations */
    Node      **classes;        /* ST_CLASS_DECL nodes */
    size_t      class_count;
    /* interface declarations (for conformance checks) */
    Node      **ifaces;
    size_t      iface_count;
    /* current class name while emitting a method body (for super), or NULL */
    const char *cur_class;
    /* flat enum-member table for dotted/unqualified resolution */
    struct {
        char       *enum_name;  /* "Color" */
        char       *member;     /* "RED"   */
        long long   value;
    } *enums;
    size_t      enum_count;
} Cg;

static Node *find_struct_decl(Cg *g, const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < g->struct_count; i++) {
        if (strcmp(g->structs[i]->as.struct_decl.name, name) == 0)
            return g->structs[i];
    }
    return NULL;
}

static Node *find_class_decl(Cg *g, const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < g->class_count; i++) {
        if (strcmp(g->classes[i]->as.class_decl.name, name) == 0)
            return g->classes[i];
    }
    return NULL;
}

static Node *find_iface_decl(Cg *g, const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < g->iface_count; i++) {
        if (strcmp(g->ifaces[i]->as.iface_decl.name, name) == 0)
            return g->ifaces[i];
    }
    return NULL;
}

/* Find a method by name walking the class chain. Returns the FUNC_DECL
   (skipping the constructor pseudo-method) or NULL. */
static Node *find_method_in_chain(Cg *g, Node *cls, const char *name) {
    for (Node *c = cls; c; c = c->as.class_decl.base_name ? find_class_decl(g, c->as.class_decl.base_name) : NULL) {
        int ci = c->as.class_decl.ctor_index;
        for (size_t i = 0; i < c->as.class_decl.method_count; i++) {
            if ((int)i == ci) continue;
            if (strcmp(c->as.class_decl.methods[i]->as.func.name, name) == 0)
                return c->as.class_decl.methods[i];
        }
    }
    return NULL;
}

/* Look up an enum member by full dotted name "Enum.Member" or by bare "Member".
   Returns true and writes value on success. */
static bool find_enum_dotted(Cg *g, const char *enum_name, const char *member, long long *out) {
    for (size_t i = 0; i < g->enum_count; i++) {
        if (strcmp(g->enums[i].member, member) != 0) continue;
        if (enum_name && strcmp(g->enums[i].enum_name, enum_name) != 0) continue;
        *out = g->enums[i].value;
        return true;
    }
    return false;
}

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
    /* memory */
    if (!strcmp(dotted, "alloc")      || !strcmp(dotted, "Memory.alloc")      || !strcmp(dotted, "System.Memory.alloc"))      return "sb_alloc_";
    if (!strcmp(dotted, "free")       || !strcmp(dotted, "Memory.free")       || !strcmp(dotted, "System.Memory.free"))       return "sb_free_";
    if (!strcmp(dotted, "gc_alloc")   || !strcmp(dotted, "Memory.gcAlloc")    || !strcmp(dotted, "System.Memory.gcAlloc"))    return "sb_gc_alloc_";
    if (!strcmp(dotted, "gc_collect") || !strcmp(dotted, "Memory.gcCollect")  || !strcmp(dotted, "System.Memory.gcCollect"))  return "sb_gc_collect_";
    if (!strcmp(dotted, "len")        || !strcmp(dotted, "Memory.length")     || !strcmp(dotted, "System.Memory.length"))     return "sb_len_";
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

    /* super(args) -- call parent constructor on 'this' */
    if (callee->kind == EX_IDENT && strcmp(callee->as.ident.name, "super") == 0) {
        if (!g->cur_class) cg_die("'super' used outside a class method", n->line);
        Node *cd = find_class_decl(g, g->cur_class);
        const char *parent = (cd && cd->as.class_decl.base_name) ? cd->as.class_decl.base_name : NULL;
        if (!parent) cg_die("'super(...)' in class with no base class", n->line);
        fprintf(g->out, "sb_call_ctor_chain(this, ");
        cg_emit_c_string(g->out, parent);
        fprintf(g->out, ", %d", argc);
        for (int i = 0; i < argc; i++) {
            fputs(", ", g->out);
            cg_expr(g, n->as.call.args.items[i]);
        }
        fputc(')', g->out);
        return;
    }

    /* super.method(args) */
    if (callee->kind == EX_MEMBER
        && callee->as.member.object
        && callee->as.member.object->kind == EX_IDENT
        && strcmp(callee->as.member.object->as.ident.name, "super") == 0)
    {
        if (!g->cur_class) cg_die("'super' used outside a class method", n->line);
        Node *cd = find_class_decl(g, g->cur_class);
        const char *parent = (cd && cd->as.class_decl.base_name) ? cd->as.class_decl.base_name : NULL;
        if (!parent) cg_die("'super.<method>' in class with no base class", n->line);
        fputs("sb_dispatch_from(this, ", g->out);
        cg_emit_c_string(g->out, parent);
        fputs(", ", g->out);
        cg_emit_c_string(g->out, callee->as.member.name);
        fprintf(g->out, ", %d", argc);
        for (int i = 0; i < argc; i++) {
            fputs(", ", g->out);
            cg_expr(g, n->as.call.args.items[i]);
        }
        fputc(')', g->out);
        return;
    }

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
            if (callee->kind == EX_IDENT) {
                /* Class instantiation: ClassName(args) or new ClassName(args) */
                if (find_class_decl(g, dotted)) {
                    fputs("sb_new(", g->out);
                    cg_emit_c_string(g->out, dotted);
                    fprintf(g->out, ", %d", argc);
                    for (int i = 0; i < argc; i++) {
                        fputs(", ", g->out);
                        cg_expr(g, n->as.call.args.items[i]);
                    }
                    fputc(')', g->out);
                    free(dotted);
                    return;
                }
                /* user-defined function */
                fprintf(g->out, "sb_fn_%s(", dotted);
                for (int i = 0; i < argc; i++) {
                    if (i) fputs(", ", g->out);
                    cg_expr(g, n->as.call.args.items[i]);
                }
                fputc(')', g->out);
                free(dotted);
                return;
            }
            /* EX_MEMBER chain that wasn't a builtin -- treat as method dispatch */
            free(dotted);
        }
        if (callee->kind == EX_MEMBER) {
            fputs("sb_dispatch(", g->out);
            cg_expr(g, callee->as.member.object);
            fputs(", ", g->out);
            cg_emit_c_string(g->out, callee->as.member.name);
            fprintf(g->out, ", %d", argc);
            for (int i = 0; i < argc; i++) {
                fputs(", ", g->out);
                cg_expr(g, n->as.call.args.items[i]);
            }
            fputc(')', g->out);
            return;
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
        case EX_IDENT: {
            long long ev;
            if (find_enum_dotted(g, NULL, n->as.ident.name, &ev)) {
                fprintf(g->out, "sb_int(%lldLL)", ev);
            } else {
                fprintf(g->out, "%s", n->as.ident.name);
            }
            break;
        }
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
            if (t->kind == EX_INDEX) {
                if (n->as.assign.is_compound) {
                    const char *fn = binop_fn(n->as.assign.compound);
                    if (!fn) cg_die("unsupported compound op", n->line);
                    fputs("sb_index_set(", g->out);
                    cg_expr(g, t->as.index_expr.object);
                    fputs(", ", g->out);
                    cg_expr(g, t->as.index_expr.index);
                    fprintf(g->out, ", %s(sb_index_get(", fn);
                    cg_expr(g, t->as.index_expr.object);
                    fputs(", ", g->out);
                    cg_expr(g, t->as.index_expr.index);
                    fputs("), ", g->out);
                    cg_expr(g, n->as.assign.value);
                    fputs("))", g->out);
                } else {
                    fputs("sb_index_set(", g->out);
                    cg_expr(g, t->as.index_expr.object);
                    fputs(", ", g->out);
                    cg_expr(g, t->as.index_expr.index);
                    fputs(", ", g->out);
                    cg_expr(g, n->as.assign.value);
                    fputc(')', g->out);
                }
                break;
            }
            if (t->kind == EX_MEMBER) {
                /* Struct field assignment: emit sb_struct_set(obj, "name", value)
                   (with sb_<op>(...) for compound). */
                if (n->as.assign.is_compound) {
                    const char *fn = binop_fn(n->as.assign.compound);
                    if (!fn) cg_die("unsupported compound op", n->line);
                    fputs("sb_struct_set(", g->out);
                    cg_expr(g, t->as.member.object);
                    fputs(", ", g->out);
                    cg_emit_c_string(g->out, t->as.member.name);
                    fprintf(g->out, ", %s(sb_struct_get(", fn);
                    cg_expr(g, t->as.member.object);
                    fputs(", ", g->out);
                    cg_emit_c_string(g->out, t->as.member.name);
                    fputs("), ", g->out);
                    cg_expr(g, n->as.assign.value);
                    fputs("))", g->out);
                } else {
                    fputs("sb_struct_set(", g->out);
                    cg_expr(g, t->as.member.object);
                    fputs(", ", g->out);
                    cg_emit_c_string(g->out, t->as.member.name);
                    fputs(", ", g->out);
                    cg_expr(g, n->as.assign.value);
                    fputc(')', g->out);
                }
                break;
            }
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
        case EX_INDEX:
            fputs("sb_index_get(", g->out);
            cg_expr(g, n->as.index_expr.object);
            fputs(", ", g->out);
            cg_expr(g, n->as.index_expr.index);
            fputc(')', g->out);
            break;
        case EX_MEMBER: {
            /* enum dotted access: Color.RED -> sb_int(N) */
            if (n->as.member.object && n->as.member.object->kind == EX_IDENT) {
                long long ev;
                if (find_enum_dotted(g, n->as.member.object->as.ident.name,
                                     n->as.member.name, &ev)) {
                    fprintf(g->out, "sb_int(%lldLL)", ev);
                    break;
                }
            }
            /* struct field read */
            fputs("sb_struct_get(", g->out);
            cg_expr(g, n->as.member.object);
            fputs(", ", g->out);
            cg_emit_c_string(g->out, n->as.member.name);
            fputc(')', g->out);
            break;
        }
        case EX_STRUCT_LIT:
            /* should be consumed by ST_VAR_DECL with a known struct type */
            cg_die("brace initializer requires a struct-typed declaration", n->line);
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
        case ST_VAR_DECL: {
            cg_indent(g);
            Node *init = n->as.var_decl.init;
            const char *tname = n->as.var_decl.type.type_name;
            Node *sd = find_struct_decl(g, tname);
            if (init && init->kind == EX_STRUCT_LIT) {
                if (!sd) cg_die("brace initializer requires a known struct type", n->line);
                size_t fc = sd->as.struct_decl.field_count;
                size_t given = init->as.struct_lit.values.count;
                if (given > fc) cg_die("too many initializers for struct", n->line);
                fprintf(g->out, "sb_value %s = sb_struct_new(", n->as.var_decl.name);
                cg_emit_c_string(g->out, sd->as.struct_decl.name);
                fprintf(g->out, ", %zu", fc);
                for (size_t i = 0; i < fc; i++) {
                    fputs(", ", g->out);
                    cg_emit_c_string(g->out, sd->as.struct_decl.fields[i].name);
                    fputs(", ", g->out);
                    if (i < given) cg_expr(g, init->as.struct_lit.values.items[i]);
                    else fputs("sb_null()", g->out);
                }
                fputs(");\n", g->out);
            } else if (!init && sd) {
                /* default-construct struct: all fields null */
                size_t fc = sd->as.struct_decl.field_count;
                fprintf(g->out, "sb_value %s = sb_struct_new(", n->as.var_decl.name);
                cg_emit_c_string(g->out, sd->as.struct_decl.name);
                fprintf(g->out, ", %zu", fc);
                for (size_t i = 0; i < fc; i++) {
                    fputs(", ", g->out);
                    cg_emit_c_string(g->out, sd->as.struct_decl.fields[i].name);
                    fputs(", sb_null()", g->out);
                }
                fputs(");\n", g->out);
            } else {
                fprintf(g->out, "sb_value %s = ", n->as.var_decl.name);
                if (init) cg_expr(g, init);
                else fputs("sb_null()", g->out);
                fputs(";\n", g->out);
            }
            break;
        }
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
        case ST_CLASS_DECL:
        case ST_INTERFACE_DECL:
            /* All emission happens at top level via codegen_emit_c. */
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

/* ---------- class / interface emission ---------- */

/* Walk the inheritance chain (root first) collecting fields by name in
   declaration order. Caller frees the returned arrays. */
static void collect_class_fields(Cg *g, Node *cls, char ***names_out, size_t *count_out) {
    /* build chain root..cls */
    Node *chain[64]; int depth = 0;
    for (Node *c = cls; c && depth < 64; ) {
        chain[depth++] = c;
        c = c->as.class_decl.base_name ? find_class_decl(g, c->as.class_decl.base_name) : NULL;
    }
    char **names = NULL; size_t n = 0, cap = 0;
    for (int d = depth - 1; d >= 0; d--) {
        Node *c = chain[d];
        for (size_t i = 0; i < c->as.class_decl.field_count; i++) {
            const char *fn = c->as.class_decl.fields[i].name;
            if (n == cap) { cap = cap ? cap * 2 : 8; names = (char**)sb_xrealloc(names, cap * sizeof(char*)); }
            names[n++] = sb_strdup(fn);
        }
    }
    *names_out = names; *count_out = n;
}

static void cg_emit_method_signature(Cg *g, const char *cls, const char *method) {
    fprintf(g->out, "static sb_value sb_method_%s_%s(sb_value this, int argc, sb_value *argv)",
            cls, method);
}
static void cg_emit_ctor_signature(Cg *g, const char *cls) {
    fprintf(g->out, "static sb_value sb_ctor_%s(sb_value this, int argc, sb_value *argv)", cls);
}

/* Emit body of a method/ctor: bind params from argv, then emit statements. */
static void cg_emit_method_body(Cg *g, Node *fn) {
    fputs(" {\n", g->out);
    fputs("    (void)this; (void)argc; (void)argv;\n", g->out);
    for (size_t i = 0; i < fn->as.func.param_count; i++) {
        fprintf(g->out, "    sb_value %s = (argc > %zu) ? argv[%zu] : sb_null();\n",
                fn->as.func.params[i].name, i, i);
    }
    g->indent = 1;
    if (fn->as.func.body && fn->as.func.body->kind == ST_BLOCK) {
        for (size_t i = 0; i < fn->as.func.body->as.block.stmts.count; i++)
            cg_stmt(g, fn->as.func.body->as.block.stmts.items[i]);
    }
    cg_indent(g); fputs("return sb_null();\n", g->out);
    g->indent = 0;
    fputs("}\n\n", g->out);
}

static void cg_emit_class_alloc(Cg *g, Node *cls) {
    char **fnames = NULL; size_t fcount = 0;
    collect_class_fields(g, cls, &fnames, &fcount);
    fprintf(g->out, "static sb_value sb_alloc_%s(void) {\n", cls->as.class_decl.name);
    fputs("    return sb_struct_new(", g->out);
    cg_emit_c_string(g->out, cls->as.class_decl.name);
    fprintf(g->out, ", %d", (int)fcount);
    for (size_t i = 0; i < fcount; i++) {
        fputs(", ", g->out);
        cg_emit_c_string(g->out, fnames[i]);
        fputs(", sb_null()", g->out);
    }
    fputs(");\n}\n", g->out);
    for (size_t i = 0; i < fcount; i++) free(fnames[i]);
    free(fnames);
}

static void cg_emit_class_init(Cg *g, Node *cls) {
    fprintf(g->out, "static void sb_init_%s(sb_value this) {\n", cls->as.class_decl.name);
    fputs("    (void)this;\n", g->out);
    if (cls->as.class_decl.base_name && find_class_decl(g, cls->as.class_decl.base_name)) {
        fprintf(g->out, "    sb_init_%s(this);\n", cls->as.class_decl.base_name);
    }
    g->cur_class = cls->as.class_decl.name;
    for (size_t i = 0; i < cls->as.class_decl.field_count; i++) {
        ClassField *cf = &cls->as.class_decl.fields[i];
        if (!cf->init) continue;
        fputs("    sb_struct_set(this, ", g->out);
        cg_emit_c_string(g->out, cf->name);
        fputs(", ", g->out);
        cg_expr(g, cf->init);
        fputs(");\n", g->out);
    }
    g->cur_class = NULL;
    fputs("}\n", g->out);
    fprintf(g->out, "static sb_value sb_alloc_init_%s(void) {\n", cls->as.class_decl.name);
    fprintf(g->out, "    sb_value o = sb_alloc_%s();\n", cls->as.class_decl.name);
    fprintf(g->out, "    sb_init_%s(o);\n", cls->as.class_decl.name);
    fputs("    return o;\n}\n\n", g->out);
}

static void cg_emit_class_methods(Cg *g, Node *cls) {
    const char *cname = cls->as.class_decl.name;
    int ci = cls->as.class_decl.ctor_index;
    g->cur_class = cname;
    for (size_t i = 0; i < cls->as.class_decl.method_count; i++) {
        Node *m = cls->as.class_decl.methods[i];
        if ((int)i == ci) {
            cg_emit_ctor_signature(g, cname);
        } else {
            cg_emit_method_signature(g, cname, m->as.func.name);
        }
        cg_emit_method_body(g, m);
    }
    g->cur_class = NULL;
}

/* Emit static method table + class entry for one class. */
static void cg_emit_class_tables(Cg *g, Node *cls) {
    const char *cname = cls->as.class_decl.name;
    int ci = cls->as.class_decl.ctor_index;
    fprintf(g->out, "static const sb_method_entry sb_methods_%s[] = {\n", cname);
    bool any = false;
    for (size_t i = 0; i < cls->as.class_decl.method_count; i++) {
        if ((int)i == ci) continue;
        Node *m = cls->as.class_decl.methods[i];
        fputs("    { ", g->out);
        cg_emit_c_string(g->out, m->as.func.name);
        fprintf(g->out, ", sb_method_%s_%s },\n", cname, m->as.func.name);
        any = true;
    }
    if (!any) fputs("    { 0, 0 }\n", g->out);
    fputs("};\n", g->out);
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

    /* Collect struct, enum, class, interface declarations (top-level only). */
    for (size_t i = 0; i < program->as.block.stmts.count; i++) {
        Node *s = program->as.block.stmts.items[i];
        if (s->kind == ST_STRUCT_DECL) {
            g.structs = (Node**)sb_xrealloc(g.structs, (g.struct_count + 1) * sizeof(Node*));
            g.structs[g.struct_count++] = s;
        } else if (s->kind == ST_CLASS_DECL) {
            g.classes = (Node**)sb_xrealloc(g.classes, (g.class_count + 1) * sizeof(Node*));
            g.classes[g.class_count++] = s;
        } else if (s->kind == ST_INTERFACE_DECL) {
            g.ifaces = (Node**)sb_xrealloc(g.ifaces, (g.iface_count + 1) * sizeof(Node*));
            g.ifaces[g.iface_count++] = s;
        } else if (s->kind == ST_ENUM_DECL) {
            long long next = 0;
            for (size_t k = 0; k < s->as.enum_decl.count; k++) {
                EnumMember *m = &s->as.enum_decl.members[k];
                long long val = m->has_value ? m->value : next;
                next = val + 1;
                g.enums = sb_xrealloc(g.enums, (g.enum_count + 1) * sizeof(*g.enums));
                g.enums[g.enum_count].enum_name = sb_strdup(s->as.enum_decl.name);
                g.enums[g.enum_count].member    = sb_strdup(m->name);
                g.enums[g.enum_count].value     = val;
                g.enum_count++;
            }
        }
    }

    /* Validate base classes and interface conformance (mirrors interpreter). */
    for (size_t i = 0; i < g.class_count; i++) {
        Node *c = g.classes[i];
        if (c->as.class_decl.base_name && !find_class_decl(&g, c->as.class_decl.base_name)) {
            fprintf(stderr, "starbyte codegen: unknown base class '%s' for class '%s' (line %d)\n",
                    c->as.class_decl.base_name, c->as.class_decl.name, c->line);
            fclose(f); free(g.structs); free(g.classes); free(g.ifaces); return 1;
        }
        for (size_t k = 0; k < c->as.class_decl.interface_count; k++) {
            const char *in = c->as.class_decl.interface_names[k];
            Node *id = find_iface_decl(&g, in);
            if (!id) {
                fprintf(stderr, "starbyte codegen: unknown interface '%s' for class '%s' (line %d)\n",
                        in, c->as.class_decl.name, c->line);
                fclose(f); free(g.structs); free(g.classes); free(g.ifaces); return 1;
            }
            for (size_t m = 0; m < id->as.iface_decl.method_count; m++) {
                const char *mn = id->as.iface_decl.methods[m].name;
                if (!find_method_in_chain(&g, c, mn)) {
                    fprintf(stderr, "starbyte codegen: class '%s' does not implement method '%s' from interface '%s' (line %d)\n",
                            c->as.class_decl.name, mn, in, c->line);
                    fclose(f); free(g.structs); free(g.classes); free(g.ifaces); return 1;
                }
            }
        }
    }

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

    /* ---- class emission ---- */
    if (g.class_count > 0) {
        /* forward declare alloc/init/method/ctor symbols */
        for (size_t i = 0; i < g.class_count; i++) {
            Node *c = g.classes[i];
            const char *cn = c->as.class_decl.name;
            fprintf(f, "static sb_value sb_alloc_%s(void);\n", cn);
            fprintf(f, "static void sb_init_%s(sb_value);\n", cn);
            fprintf(f, "static sb_value sb_alloc_init_%s(void);\n", cn);
            int ci = c->as.class_decl.ctor_index;
            for (size_t k = 0; k < c->as.class_decl.method_count; k++) {
                Node *m = c->as.class_decl.methods[k];
                if ((int)k == ci) {
                    fprintf(f, "static sb_value sb_ctor_%s(sb_value, int, sb_value*);\n", cn);
                } else {
                    fprintf(f, "static sb_value sb_method_%s_%s(sb_value, int, sb_value*);\n",
                            cn, m->as.func.name);
                }
            }
        }
        fputs("\n", f);
        /* alloc + init bodies */
        for (size_t i = 0; i < g.class_count; i++) {
            cg_emit_class_alloc(&g, g.classes[i]);
            cg_emit_class_init(&g, g.classes[i]);
        }
        /* method bodies */
        for (size_t i = 0; i < g.class_count; i++) {
            cg_emit_class_methods(&g, g.classes[i]);
        }
        /* method tables */
        for (size_t i = 0; i < g.class_count; i++) {
            cg_emit_class_tables(&g, g.classes[i]);
        }
        /* class table */
        fputs("static const sb_class_entry sb_class_table_arr[] = {\n", f);
        for (size_t i = 0; i < g.class_count; i++) {
            Node *c = g.classes[i];
            const char *cn = c->as.class_decl.name;
            int ci = c->as.class_decl.ctor_index;
            int nmeth = (int)c->as.class_decl.method_count - (ci >= 0 ? 1 : 0);
            fputs("    { ", f);
            cg_emit_c_string(f, cn);
            fputs(", ", f);
            if (c->as.class_decl.base_name)
                cg_emit_c_string(f, c->as.class_decl.base_name);
            else
                fputs("NULL", f);
            fprintf(f, ", %d, sb_methods_%s, ", nmeth, cn);
            if (ci >= 0) fprintf(f, "sb_ctor_%s", cn);
            else         fputs("NULL", f);
            fprintf(f, ", sb_alloc_init_%s },\n", cn);
        }
        fputs("};\n\n", f);
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
    if (g.class_count > 0) {
        fputs("    sb_class_table = sb_class_table_arr;\n", f);
        fputs("    sb_class_table_n = (int)(sizeof(sb_class_table_arr)/sizeof(sb_class_table_arr[0]));\n", f);
    }
    fputs("    sb_top_level();\n", f);
    if (g.has_main) {
        fputs("    sb_value r = sb_fn_main();\n", f);
        fputs("    if (r.t == SB_INT) return (int)r.v.i;\n", f);
    }
    fputs("    return 0;\n", f);
    fputs("}\n", f);

    fclose(f);

    free(g.structs);
    free(g.classes);
    free(g.ifaces);
    for (size_t i = 0; i < g.enum_count; i++) {
        free(g.enums[i].enum_name);
        free(g.enums[i].member);
    }
    free(g.enums);
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
