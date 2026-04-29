#ifndef STARBYTE_INTERPRETER_H
#define STARBYTE_INTERPRETER_H

#include "ast.h"
#include "value.h"

typedef struct EnvEntry {
    char *name;
    Value value;
    bool  is_const;
    struct EnvEntry *next;
} EnvEntry;

typedef struct Env {
    struct Env *parent;
    EnvEntry *head;
} Env;

typedef struct Interp {
    Env *globals;
    const char *filename;

    /* control flow flags */
    int return_flag;
    int break_flag;
    int continue_flag;
    Value return_value;
} Interp;

void  interp_init(Interp *I, const char *filename);
void  interp_dispose(Interp *I);
int   interp_run(Interp *I, Node *program);

/* env helpers */
Env  *env_new(Env *parent);
void  env_free(Env *e);
void  env_define(Env *e, const char *name, Value v, bool is_const);
Value env_get(Env *e, const char *name, bool *found);
bool  env_assign(Env *e, const char *name, Value v); /* false if not found / const */

#endif
