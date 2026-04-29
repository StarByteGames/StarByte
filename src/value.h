#ifndef STARBYTE_VALUE_H
#define STARBYTE_VALUE_H

#include "common.h"

typedef enum {
    V_NULL,
    V_INT,
    V_FLOAT,
    V_BOOL,
    V_CHAR,
    V_STRING,
    V_FUNC,
    V_BUILTIN,
    V_NAMESPACE
} ValueType;

struct Node;
struct Interp;
struct Env;

typedef struct Value Value;
typedef Value (*BuiltinFn)(struct Interp *I, int argc, Value *argv);

struct Value {
    ValueType type;
    union {
        long long i;
        double    f;
        bool      b;
        char      c;
        char     *s;          /* owned C-string */
        struct {
            struct Node *decl;       /* ST_FUNC_DECL (not owned) */
            struct Env  *closure;    /* not owned */
        } func;
        BuiltinFn builtin;
        struct {
            const char *name;
            /* members lookup is done by name in interpreter */
        } ns;
    } as;
};

Value v_null(void);
Value v_int(long long i);
Value v_float(double f);
Value v_bool(bool b);
Value v_char(char c);
Value v_string(const char *s);          /* copies */
Value v_string_take(char *s);           /* takes ownership */
Value v_builtin(BuiltinFn fn);
Value v_namespace(const char *name);

Value value_copy(const Value *v);
void  value_free(Value *v);

bool  value_truthy(const Value *v);
char *value_to_cstring(const Value *v); /* malloc'd */
const char *value_type_name(ValueType t);

#endif
