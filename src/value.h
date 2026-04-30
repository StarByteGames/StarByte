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
    V_INTERFACE,     /* interface definition */
    V_BUFFER         /* heap-allocated dynamic array of Values */
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

/* A Buffer is a heap-allocated array of Value slots used for both manual
   memory management (alloc/free) and the optional garbage collector
   (gc_alloc/gc_collect). Manually-allocated buffers participate in normal
   reference counting as well, so passing one around still works; calling
   free() drops the contents and marks the buffer as freed. GC-managed
   buffers ignore refcount-driven freeing and are collected by the GC. */
typedef struct Buffer {
    int     refcount;
    size_t  len;
    Value  *items;       /* len Value slots */
    bool    gc_managed;  /* true: lifetime owned by GC */
    bool    freed;       /* true after explicit free() (manual mode) */
    int     gc_mark;     /* used during mark/sweep */
    struct Buffer *gc_next; /* intrusive list head in Interp */
} Buffer;

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
        Buffer *buf;                 /* V_BUFFER */
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

/* Buffer helpers (memory management). */
Buffer *buffer_new(size_t len);          /* refcount 1, manual */
void    buffer_retain(Buffer *b);
void    buffer_release(Buffer *b);       /* drops refcount; frees if 0 and not GC */
void    buffer_free_contents(Buffer *b); /* used by free() and GC sweep */
Value   v_buffer(Buffer *b);             /* takes existing ref */
StructFieldV *object_find_field(ObjectInstance *obj, const char *name);
struct Node *class_find_method(ClassDef *cls, const char *name, ClassDef **owner_out);

Value value_copy(const Value *v);
void  value_free(Value *v);

bool  value_truthy(const Value *v);
char *value_to_cstring(const Value *v); /* malloc'd */
const char *value_type_name(ValueType t);

#endif
