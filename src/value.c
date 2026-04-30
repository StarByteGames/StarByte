#include "value.h"
#include "ast.h"

Value v_null(void) { Value v = {0}; v.type = V_NULL; return v; }
Value v_int(long long i) { Value v = {0}; v.type = V_INT; v.as.i = i; return v; }
Value v_float(double f) { Value v = {0}; v.type = V_FLOAT; v.as.f = f; return v; }
Value v_bool(bool b) { Value v = {0}; v.type = V_BOOL; v.as.b = b; return v; }
Value v_char(char c) { Value v = {0}; v.type = V_CHAR; v.as.c = c; return v; }
Value v_string(const char *s) { Value v = {0}; v.type = V_STRING; v.as.s = sb_strdup(s ? s : ""); return v; }
Value v_string_take(char *s) { Value v = {0}; v.type = V_STRING; v.as.s = s; return v; }
Value v_builtin(BuiltinFn fn) { Value v = {0}; v.type = V_BUILTIN; v.as.builtin = fn; return v; }
Value v_namespace(const char *name) { Value v = {0}; v.type = V_NAMESPACE; v.as.ns.name = name; return v; }

Value v_struct_def(struct Node *decl) { Value v = {0}; v.type = V_STRUCT_DEF; v.as.sdef.decl = decl; return v; }

/* ===== Classes / objects ===== */

static ClassDef *classdef_retain(ClassDef *c) { if (c) c->refcount++; return c; }
static void classdef_release(ClassDef *c) {
    if (!c) return;
    if (--c->refcount > 0) return;
    if (c->parent) classdef_release(c->parent);
    free(c);
}

Value v_class(struct Node *decl, ClassDef *parent) {
    ClassDef *c = (ClassDef*)sb_xcalloc(1, sizeof(ClassDef));
    c->refcount = 1;
    c->decl = decl;
    c->parent = parent ? classdef_retain(parent) : NULL;
    Value v = {0}; v.type = V_CLASS; v.as.cls = c; return v;
}

/* gather fields by walking parent chain (parent fields first). */
static void collect_fields(ClassDef *c, ClassField ***out, size_t *count, size_t *cap) {
    if (!c) return;
    if (c->parent) collect_fields(c->parent, out, count, cap);
    Node *d = c->decl;
    for (size_t i = 0; i < d->as.class_decl.field_count; i++) {
        if (*count == *cap) {
            *cap = (*cap) ? (*cap) * 2 : 4;
            *out = (ClassField**)sb_xrealloc(*out, (*cap) * sizeof(ClassField*));
        }
        (*out)[(*count)++] = &d->as.class_decl.fields[i];
    }
}

Value v_object(ClassDef *cls) {
    ObjectInstance *o = (ObjectInstance*)sb_xcalloc(1, sizeof(ObjectInstance));
    o->refcount = 1;
    o->cls = classdef_retain(cls);
    ClassField **flist = NULL; size_t fcount = 0, fcap = 0;
    collect_fields(cls, &flist, &fcount, &fcap);
    o->field_count = fcount;
    o->fields = fcount ? (StructFieldV*)sb_xcalloc(fcount, sizeof(StructFieldV)) : NULL;
    for (size_t i = 0; i < fcount; i++) {
        o->fields[i].name = sb_strdup(flist[i]->name);
        o->fields[i].value = (Value*)sb_xcalloc(1, sizeof(Value));
        o->fields[i].value->type = V_NULL;
    }
    free(flist);
    Value v = {0}; v.type = V_OBJECT; v.as.obj = o; return v;
}

Value v_super(ObjectInstance *obj, ClassDef *from) {
    Value v = {0}; v.type = V_SUPER;
    if (obj) obj->refcount++;
    v.as.sup.obj = obj;
    v.as.sup.from = classdef_retain(from);
    return v;
}

Value v_interface(struct Node *decl) { Value v = {0}; v.type = V_INTERFACE; v.as.iface.decl = decl; return v; }

/* ===== Buffers (manual + GC) ===== */

Buffer *buffer_new(size_t len) {
    Buffer *b = (Buffer*)sb_xcalloc(1, sizeof(Buffer));
    b->refcount = 1;
    b->len = len;
    b->items = len ? (Value*)sb_xcalloc(len, sizeof(Value)) : NULL;
    for (size_t i = 0; i < len; i++) b->items[i].type = V_NULL;
    b->gc_managed = false;
    b->freed = false;
    b->gc_mark = 0;
    b->gc_next = NULL;
    return b;
}

void buffer_retain(Buffer *b) { if (b) b->refcount++; }

void buffer_free_contents(Buffer *b) {
    if (!b || b->freed) return;
    if (b->items) {
        for (size_t i = 0; i < b->len; i++) value_free(&b->items[i]);
        free(b->items);
        b->items = NULL;
    }
    b->len = 0;
    b->freed = true;
}

void buffer_release(Buffer *b) {
    if (!b) return;
    if (--b->refcount > 0) return;
    /* GC-managed buffers are owned by the GC; don't truly free here. */
    if (b->gc_managed) return;
    buffer_free_contents(b);
    free(b);
}

Value v_buffer(Buffer *b) {
    Value v = {0};
    v.type = V_BUFFER;
    v.as.buf = b;
    return v;
}

StructFieldV *object_find_field(ObjectInstance *obj, const char *name) {
    if (!obj) return NULL;
    for (size_t i = 0; i < obj->field_count; i++) {
        if (obj->fields[i].name && strcmp(obj->fields[i].name, name) == 0)
            return &obj->fields[i];
    }
    return NULL;
}

struct Node *class_find_method(ClassDef *cls, const char *name, ClassDef **owner_out) {
    for (ClassDef *c = cls; c; c = c->parent) {
        Node *d = c->decl;
        for (size_t i = 0; i < d->as.class_decl.method_count; i++) {
            Node *m = d->as.class_decl.methods[i];
            if (m->as.func.name && strcmp(m->as.func.name, name) == 0) {
                if (owner_out) *owner_out = c;
                return m;
            }
        }
    }
    return NULL;
}

static void object_release(ObjectInstance *o) {
    if (!o) return;
    if (--o->refcount > 0) return;
    for (size_t i = 0; i < o->field_count; i++) {
        free(o->fields[i].name);
        if (o->fields[i].value) {
            value_free(o->fields[i].value);
            free(o->fields[i].value);
        }
    }
    free(o->fields);
    classdef_release(o->cls);
    free(o);
}

Value v_struct_new(const char *type_name, size_t field_count) {
    Value v = {0};
    v.type = V_STRUCT;
    StructInstance *st = (StructInstance*)sb_xcalloc(1, sizeof(StructInstance));
    st->refcount = 1;
    st->type_name = sb_strdup(type_name ? type_name : "");
    st->field_count = field_count;
    st->fields = field_count
        ? (StructFieldV*)sb_xcalloc(field_count, sizeof(StructFieldV))
        : NULL;
    for (size_t i = 0; i < field_count; i++) {
        st->fields[i].name = NULL;
        st->fields[i].value = (Value*)sb_xcalloc(1, sizeof(Value));
        st->fields[i].value->type = V_NULL;
    }
    v.as.st = st;
    return v;
}

StructFieldV *struct_find_field(StructInstance *st, const char *name) {
    if (!st) return NULL;
    for (size_t i = 0; i < st->field_count; i++) {
        if (st->fields[i].name && strcmp(st->fields[i].name, name) == 0)
            return &st->fields[i];
    }
    return NULL;
}

static void struct_release(StructInstance *st) {
    if (!st) return;
    if (--st->refcount > 0) return;
    for (size_t i = 0; i < st->field_count; i++) {
        free(st->fields[i].name);
        if (st->fields[i].value) {
            value_free(st->fields[i].value);
            free(st->fields[i].value);
        }
    }
    free(st->fields);
    free(st->type_name);
    free(st);
}

Value value_copy(const Value *v) {
    Value r = *v;
    if (v->type == V_STRING && v->as.s) r.as.s = sb_strdup(v->as.s);
    else if (v->type == V_STRUCT && v->as.st) v->as.st->refcount++;
    else if (v->type == V_CLASS && v->as.cls) v->as.cls->refcount++;
    else if (v->type == V_OBJECT && v->as.obj) v->as.obj->refcount++;
    else if (v->type == V_SUPER) {
        if (v->as.sup.obj) v->as.sup.obj->refcount++;
        if (v->as.sup.from) v->as.sup.from->refcount++;
    }
    else if (v->type == V_BUFFER && v->as.buf) v->as.buf->refcount++;
    return r;
}

void value_free(Value *v) {
    if (!v) return;
    if (v->type == V_STRING && v->as.s) { free(v->as.s); v->as.s = NULL; }
    else if (v->type == V_STRUCT && v->as.st) { struct_release(v->as.st); v->as.st = NULL; }
    else if (v->type == V_CLASS && v->as.cls) { classdef_release(v->as.cls); v->as.cls = NULL; }
    else if (v->type == V_OBJECT && v->as.obj) { object_release(v->as.obj); v->as.obj = NULL; }
    else if (v->type == V_SUPER) {
        if (v->as.sup.obj) object_release(v->as.sup.obj);
        if (v->as.sup.from) classdef_release(v->as.sup.from);
        v->as.sup.obj = NULL; v->as.sup.from = NULL;
    }
    else if (v->type == V_BUFFER && v->as.buf) { buffer_release(v->as.buf); v->as.buf = NULL; }
    v->type = V_NULL;
}

bool value_truthy(const Value *v) {
    switch (v->type) {
        case V_NULL: return false;
        case V_BOOL: return v->as.b;
        case V_INT: return v->as.i != 0;
        case V_FLOAT: return v->as.f != 0.0;
        case V_CHAR: return v->as.c != 0;
        case V_STRING: return v->as.s && v->as.s[0];
        default: return true;
    }
}

char *value_to_cstring(const Value *v) {
    char buf[64];
    switch (v->type) {
        case V_NULL: return sb_strdup("null");
        case V_BOOL: return sb_strdup(v->as.b ? "true" : "false");
        case V_INT: snprintf(buf, sizeof buf, "%lld", v->as.i); return sb_strdup(buf);
        case V_FLOAT: {
            snprintf(buf, sizeof buf, "%g", v->as.f);
            return sb_strdup(buf);
        }
        case V_CHAR: { char b[2] = {v->as.c, 0}; return sb_strdup(b); }
        case V_STRING: return sb_strdup(v->as.s ? v->as.s : "");
        case V_FUNC: return sb_strdup("<func>");
        case V_BUILTIN: return sb_strdup("<builtin>");
        case V_NAMESPACE: return sb_strdup(v->as.ns.name ? v->as.ns.name : "<namespace>");
        case V_STRUCT_DEF: return sb_strdup("<struct-def>");
        case V_CLASS: return sb_strdup("<class>");
        case V_INTERFACE: return sb_strdup("<interface>");
        case V_BUFFER: {
            Buffer *b = v->as.buf;
            if (!b) return sb_strdup("<buffer:null>");
            if (b->freed) return sb_strdup("<buffer:freed>");
            size_t cap = 32, len = 0;
            char *out = (char*)sb_xmalloc(cap);
            #define APPB(s) do { \
                const char *_s = (s); size_t _l = strlen(_s); \
                if (len + _l + 1 > cap) { cap = (len + _l + 1) * 2; out = (char*)sb_xrealloc(out, cap); } \
                memcpy(out + len, _s, _l); len += _l; out[len] = '\0'; \
            } while (0)
            APPB("[");
            for (size_t i = 0; i < b->len; i++) {
                if (i) APPB(", ");
                char *fs = value_to_cstring(&b->items[i]);
                APPB(fs);
                free(fs);
            }
            APPB("]");
            #undef APPB
            return out;
        }
        case V_SUPER: return sb_strdup("<super>");
        case V_OBJECT: {
            ObjectInstance *o = v->as.obj;
            const char *tn = (o && o->cls && o->cls->decl) ? o->cls->decl->as.class_decl.name : "object";
            size_t cap = 64, len = 0;
            char *out = (char*)sb_xmalloc(cap);
            #define APP(s) do { \
                const char *_s = (s); size_t _l = strlen(_s); \
                if (len + _l + 1 > cap) { cap = (len + _l + 1) * 2; out = (char*)sb_xrealloc(out, cap); } \
                memcpy(out + len, _s, _l); len += _l; out[len] = '\0'; \
            } while (0)
            APP(tn ? tn : "object");
            APP("{");
            if (o) {
                for (size_t i = 0; i < o->field_count; i++) {
                    if (i) APP(", ");
                    APP(o->fields[i].name ? o->fields[i].name : "?");
                    APP("=");
                    char *fs = value_to_cstring(o->fields[i].value);
                    APP(fs);
                    free(fs);
                }
            }
            APP("}");
            #undef APP
            return out;
        }
        case V_STRUCT: {
            StructInstance *st = v->as.st;
            size_t cap = 64, len = 0;
            char *out = (char*)sb_xmalloc(cap);
            #define APP(s) do { \
                const char *_s = (s); size_t _l = strlen(_s); \
                if (len + _l + 1 > cap) { cap = (len + _l + 1) * 2; out = (char*)sb_xrealloc(out, cap); } \
                memcpy(out + len, _s, _l); len += _l; out[len] = '\0'; \
            } while (0)
            APP(st && st->type_name ? st->type_name : "struct");
            APP("{");
            if (st) {
                for (size_t i = 0; i < st->field_count; i++) {
                    if (i) APP(", ");
                    APP(st->fields[i].name ? st->fields[i].name : "?");
                    APP("=");
                    char *fs = value_to_cstring(st->fields[i].value);
                    APP(fs);
                    free(fs);
                }
            }
            APP("}");
            #undef APP
            return out;
        }
    }
    return sb_strdup("");
}

const char *value_type_name(ValueType t) {
    switch (t) {
        case V_NULL: return "null";
        case V_INT: return "int";
        case V_FLOAT: return "float";
        case V_BOOL: return "bool";
        case V_CHAR: return "char";
        case V_STRING: return "string";
        case V_FUNC: return "func";
        case V_BUILTIN: return "builtin";
        case V_NAMESPACE: return "namespace";
        case V_STRUCT: return "struct";
        case V_STRUCT_DEF: return "struct-def";
        case V_CLASS: return "class";
        case V_OBJECT: return "object";
        case V_SUPER: return "super";
        case V_INTERFACE: return "interface";
        case V_BUFFER: return "buffer";
    }
    return "?";
}
