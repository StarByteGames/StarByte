#ifndef STARBYTE_INTERPRETER_H
#define STARBYTE_INTERPRETER_H

#include "ast.h"
#include "value.h"
#include <setjmp.h>

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

typedef struct ExcFrame {
    jmp_buf env;
    struct ExcFrame *prev;
} ExcFrame;

typedef struct Interp {
    Env *globals;
    const char *filename;

    /* control flow flags */
    int return_flag;
    int break_flag;
    int continue_flag;
    Value return_value;

    /* exception state */
    ExcFrame *exc_stack;
    Value     exc_value;
    int       exc_line;

    /* Garbage-collected buffer list (intrusive). */
    struct Buffer *gc_head;
    size_t gc_count;

    /* Synthesized AST nodes (e.g. lambda function shells) that the
       interpreter owns and frees on shutdown. */
    Node **synth_nodes;
    size_t synth_node_count;
    size_t synth_node_cap;

    /* Active coroutine, if any. NULL when running on the main thread.
       Set by co_resume / cleared on suspend or completion. */
    struct Coroutine *current_co;
} Interp;

struct Coroutine;
void  interp_track_synth_node(Interp *I, Node *n);

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
