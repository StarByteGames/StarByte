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
    V_NAMESPACE,
    V_STRUCT,        /* instance */
    V_STRUCT_DEF,    /* type/blueprint */
    V_CLASS,         /* class definition */
    V_OBJECT,        /* class instance */
    V_SUPER,         /* bound super reference (this + lookup_from class) */
    V_INTERFACE      /* interface definition */
} ValueType;

struct Node;
struct Interp;
struct Env;

typedef struct Value Value;
typedef Value (*BuiltinFn)(struct Interp *I, int argc, Value *argv);

typedef struct StructField_v {
    char *name;
    Value *value;        /* heap-allocated so refs can mutate */
} StructFieldV;

typedef struct StructInstance {
    int refcount;
    char *type_name;
    size_t field_count;
    StructFieldV *fields;
} StructInstance;

typedef struct ClassDef {
    int refcount;                /* lifetime tied to V_CLASS values */
    struct Node *decl;           /* ST_CLASS_DECL (not owned) */
    struct ClassDef *parent;     /* may be NULL (refcount tracked) */
} ClassDef;

typedef struct ObjectInstance {
    int refcount;
    ClassDef *cls;               /* refcount held while instance lives */
    size_t field_count;          /* total including inherited */
    StructFieldV *fields;        /* parent fields first, then own */
} ObjectInstance;

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
        StructInstance *st;          /* refcounted */
        struct {
            struct Node *decl;       /* ST_STRUCT_DECL (not owned) */
        } sdef;
        ClassDef       *cls;         /* V_CLASS, refcounted */
        ObjectInstance *obj;         /* V_OBJECT, refcounted */
        struct {
            ObjectInstance *obj;     /* receiver, refcount held */
            ClassDef       *from;    /* class to start method lookup at */
        } sup;                       /* V_SUPER */
        struct {
            struct Node *decl;       /* ST_INTERFACE_DECL (not owned) */
        } iface;
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
Value v_struct_def(struct Node *decl);
Value v_struct_new(const char *type_name, size_t field_count);
StructFieldV *struct_find_field(StructInstance *st, const char *name);

/* Class / object helpers */
Value v_class(struct Node *decl, ClassDef *parent);   /* parent may be NULL; takes ref */
Value v_object(ClassDef *cls);                        /* allocates fields, takes ref of cls */
Value v_super(ObjectInstance *obj, ClassDef *from);   /* takes refs */
Value v_interface(struct Node *decl);
StructFieldV *object_find_field(ObjectInstance *obj, const char *name);
struct Node *class_find_method(ClassDef *cls, const char *name, ClassDef **owner_out);

Value value_copy(const Value *v);
void  value_free(Value *v);

bool  value_truthy(const Value *v);
char *value_to_cstring(const Value *v); /* malloc'd */
const char *value_type_name(ValueType t);

#endif
