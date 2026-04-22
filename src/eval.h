#ifndef STONED_EVAL_H
#define STONED_EVAL_H

#include "value.h"
#include "env.h"
#include "ast.h"
#include <stdio.h>

#define EVAL_MAX_DEPTH 512

typedef struct {
    Arena       *arena;
    GlobalTable  globals;
    Env         *top_env;   /* top-level environment */
    FILE        *out;
    int          call_depth;
    int          errored;
    char         errmsg[512];
    Value        current_exception;
    Value        rescue_context;
    uint32_t     exception_line;
    uint32_t     exception_col;
    const char  *exception_class;
    char         exception_msg[512];
} Eval;

void  eval_init(Eval *ev, Arena *arena, FILE *out);
Value eval_node(Eval *ev, Env *env, Node *node);

#endif
