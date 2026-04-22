#ifndef STONED_ENV_H
#define STONED_ENV_H

#include "value.h"
#include "arena.h"

typedef struct EnvEntry {
    const char      *name;
    Value            val;
    struct EnvEntry *next;
} EnvEntry;

typedef struct Env {
    EnvEntry   *vars;
    struct Env *parent;
    int         is_def;     /* hard scope boundary */
    Value      *block_arg;  /* block passed to this call frame, NULL if none */
} Env;

Env  *env_new(Arena *a, Env *parent, int is_def);
void  env_set(Arena *a, Env *env, const char *name, Value val);
int   env_get(Env *env, const char *name, Value *out);
/* Update existing binding, stopping at is_def boundaries; returns 0 if not found */
int   env_update(Env *env, const char *name, Value val);
/* Define in the current frame only (for def/class — never updates parent) */
void  env_define(Arena *a, Env *env, const char *name, Value val);

/* Global variable table (flat, no scoping) */
typedef struct GlobalEntry {
    const char       *name;
    Value             val;
    struct GlobalEntry *next;
} GlobalEntry;

typedef struct GlobalTable {
    GlobalEntry *head;
} GlobalTable;

void  global_set(Arena *a, GlobalTable *gt, const char *name, Value val);
int   global_get(GlobalTable *gt, const char *name, Value *out);

#endif
