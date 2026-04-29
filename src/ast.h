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
    ST_MODULE          /* module a.b.c; (parsed, ignored at runtime) */
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

        struct { char *name; } module_stmt;
    } as;
};

Node *node_new(NodeKind k, int line);
void  node_free(Node *n);

#endif
