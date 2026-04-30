#include "value.h"

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
    return r;
}

void value_free(Value *v) {
    if (!v) return;
    if (v->type == V_STRING && v->as.s) { free(v->as.s); v->as.s = NULL; }
    else if (v->type == V_STRUCT && v->as.st) { struct_release(v->as.st); v->as.st = NULL; }
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
    }
    return "?";
}
