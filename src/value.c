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

Value value_copy(const Value *v) {
    Value r = *v;
    if (v->type == V_STRING && v->as.s) r.as.s = sb_strdup(v->as.s);
    return r;
}

void value_free(Value *v) {
    if (!v) return;
    if (v->type == V_STRING && v->as.s) { free(v->as.s); v->as.s = NULL; }
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
    }
    return "?";
}
