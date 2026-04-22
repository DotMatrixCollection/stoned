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
} Eval;

void  eval_init(Eval *ev, Arena *arena, FILE *out);
Value eval_node(Eval *ev, Env *env, Node *node);

#endif
