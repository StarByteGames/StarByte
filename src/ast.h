#ifndef STARBYTE_AST_H
#define STARBYTE_AST_H

#include "common.h"

typedef enum {
    /* Expressions */
    EX_INT, EX_FLOAT, EX_STRING, EX_CHAR, EX_BOOL, EX_NULL,
    EX_IDENT,
    EX_BINARY,
    EX_UNARY,
    EX_ASSIGN,         /* target = value (target may be ident) */
    EX_CALL,           /* callee(args...) */
    EX_MEMBER,         /* obj.name (used for Console.WriteLine etc.) */
    EX_POSTFIX,        /* x++ x-- */
    EX_LOGICAL,        /* && || (short-circuit) */
    EX_STRUCT_LIT,     /* { e1, e2, ... } brace initializer */
    EX_INDEX,          /* obj[idx] -- buffer indexing */
    EX_LAMBDA,         /* func(params) { body }  or  func(params) => expr */

    /* Statements */
    ST_EXPR,
    ST_VAR_DECL,       /* type ident (= expr)?; */
    ST_BLOCK,
    ST_IF,
    ST_WHILE,
    ST_FOR,
    ST_RETURN,
    ST_BREAK,
    ST_CONTINUE,
    ST_FUNC_DECL,
    ST_MODULE,         /* module a.b.c; (parsed, ignored at runtime) */
    ST_STRUCT_DECL,    /* struct Name { type field; ... }; */
    ST_ENUM_DECL,      /* enum Name { A, B = 3, C }; */
    ST_CLASS_DECL,     /* class Name [: Base[, IFoo, ...]] { fields/methods } */
    ST_INTERFACE_DECL, /* interface Name { method signatures; } */
    ST_TRY,            /* try {..} catch ([T] name) {..} [finally {..}] */
    ST_THROW           /* throw expr; */
} NodeKind;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR, OP_NOT, OP_NEG, OP_POS,
    OP_INC, OP_DEC
} OpKind;

typedef struct Node Node;

typedef struct {
    Node **items;
    size_t count, cap;
} NodeList;

void nodelist_push(NodeList *l, Node *n);
void nodelist_free(NodeList *l);

typedef struct {
    char *type_name;   /* "int","float","char","bool","string","void", or user */
    bool is_const;
} TypeRef;

typedef struct {
    TypeRef type;
    char *name;
} Param;

typedef struct {
    TypeRef type;
    char *name;
} StructField;

typedef struct {
    char     *name;
    long long value;
    bool      has_value;
} EnumMember;

/* Class members: a field is a TypeRef + name + optional default expression.
   A method is just a Node* (ST_FUNC_DECL). */
typedef struct {
    TypeRef type;
    char   *name;
    Node   *init;       /* optional default value, may be NULL */
} ClassField;

/* Interface method signature: return type, name, params (no body). */
typedef struct {
    TypeRef ret_type;
    char   *name;
    Param  *params;
    size_t  param_count;
} InterfaceMethod;

struct Node {
    NodeKind kind;
    int line;
    union {
        long long  i;
        double     f;
        char      *s;       /* owns */
        bool       b;

        struct { char *name; } ident;

        struct { OpKind op; Node *lhs, *rhs; } binary;
        struct { OpKind op; Node *operand; } unary;
        struct { OpKind op; Node *operand; } postfix;
        struct { Node *target; Node *value; OpKind compound; bool is_compound; } assign;

        struct { Node *callee; NodeList args; } call;
        struct { Node *object; char *name; } member;
        struct { Node *object; Node *index; } index_expr;
        struct { OpKind op; Node *lhs, *rhs; } logical;

        struct { Node *expr; } expr_stmt;

        struct {
            TypeRef type;
            char *name;
            Node *init;       /* may be NULL */
        } var_decl;

        struct { NodeList stmts; } block;

        struct { Node *cond; Node *then_branch; Node *else_branch; } if_stmt;
        struct { Node *cond; Node *body; } while_stmt;
        struct { Node *init; Node *cond; Node *post; Node *body; } for_stmt;
        struct { Node *value; } ret;

        struct {
            TypeRef ret_type;
            char *name;
            Param *params;
            size_t param_count;
            Node *body;       /* block */
        } func;

        /* Lambda: anonymous function expression. Owns its params and body
           the same way ST_FUNC_DECL does. `id` is a unique zero-based
           index assigned by the parser; the native backend uses it to
           generate stable C symbol names (sb_lambda_<id>). */
        struct {
            Param *params;
            size_t param_count;
            Node *body;       /* block */
            int   id;
        } lambda;

        struct { char *name; } module_stmt;

        struct { NodeList values; } struct_lit;

        struct {
            char         *name;
            StructField  *fields;
            size_t        field_count;
        } struct_decl;

        struct {
            char        *name;
            EnumMember  *members;
            size_t       count;
        } enum_decl;

        struct {
            char         *name;
            char         *base_name;       /* may be NULL */
            char        **interface_names; /* array of strdup'd names, may be NULL */
            size_t        interface_count;
            ClassField   *fields;
            size_t        field_count;
            Node        **methods;         /* array of ST_FUNC_DECL */
            size_t        method_count;
            int           ctor_index;      /* index into methods (-1 if none) */
        } class_decl;

        struct {
            char            *name;
            char           **base_names;   /* parent interfaces, may be NULL */
            size_t           base_count;
            InterfaceMethod *methods;
            size_t           method_count;
        } iface_decl;

        struct {
            Node *body;          /* ST_BLOCK */
            char *catch_name;    /* may be NULL if no catch */
            Node *catch_body;    /* ST_BLOCK, may be NULL */
            Node *finally_body;  /* ST_BLOCK, may be NULL */
        } try_stmt;

        struct { Node *value; } throw_stmt;
    } as;
};

Node *node_new(NodeKind k, int line);
void  node_free(Node *n);

#endif
