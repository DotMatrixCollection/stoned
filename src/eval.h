#ifndef STONED_EVAL_H
#define STONED_EVAL_H

#include "value.h"
#include "env.h"
#include "ast.h"
#include <stdio.h>

#define EVAL_MAX_DEPTH 512

typedef struct {
    uint32_t line;
    uint32_t col;
    const char *label;
} EvalFrame;

typedef struct LoadedFile {
    const char *path;
    struct LoadedFile *next;
} LoadedFile;

/* at_exit handler list — singly linked, prepend on register, run in LIFO order */
typedef struct AtExitHandler {
    Value                  blk;
    struct AtExitHandler  *next;
} AtExitHandler;

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
    EvalFrame    frames[EVAL_MAX_DEPTH];
    int          frame_count;
    Env         *active_defs[EVAL_MAX_DEPTH];
    int          active_def_count;
    const char  *current_file;
    const char  *exec_path;
    const char  *runtime_root;
    LoadedFile  *loaded_files;
    AtExitHandler *at_exit_handlers;
} Eval;

void  eval_init(Eval *ev, Arena *arena, FILE *out, const char *current_file, const char *exec_path,
                int script_argc, char **script_argv);
Value eval_node(Eval *ev, Env *env, Node *node);

#endif
