#include "env.h"
#include <string.h>

Env *env_new(Arena *a, Env *parent, int is_def) {
    Env *e    = arena_alloc(a, sizeof(Env));
    e->parent = parent;
    e->is_def = is_def;
    e->vars   = NULL;
    e->block_arg = NULL;
    return e;
}

Env *env_nearest_def(Env *env) {
    for (Env *sc = env; sc; sc = sc->parent) {
        if (sc->is_def) return sc;
    }
    return NULL;
}

void env_set(Arena *a, Env *env, const char *name, Value val) {
    /* Update in-place if already exists anywhere in accessible chain */
    if (env_update(env, name, val)) return;
    /* Otherwise create in the current frame */
    EnvEntry *e = arena_alloc(a, sizeof(EnvEntry));
    e->name     = name;
    e->val      = val;
    e->next     = env->vars;
    env->vars   = e;
}

int env_get(Env *env, const char *name, Value *out) {
    for (Env *sc = env; sc; sc = sc->parent) {
        for (EnvEntry *e = sc->vars; e; e = e->next) {
            if (strcmp(e->name, name) == 0) {
                *out = e->val;
                return 1;
            }
        }
    }
    return 0;
}

int env_update(Env *env, const char *name, Value val) {
    for (Env *sc = env; sc; sc = sc->parent) {
        for (EnvEntry *e = sc->vars; e; e = e->next) {
            if (strcmp(e->name, name) == 0) {
                e->val = val;
                return 1;
            }
        }
        if (sc->is_def) break;  /* don't cross def boundaries for assignment */
    }
    return 0;
}

void env_define(Arena *a, Env *env, const char *name, Value val) {
    for (EnvEntry *e = env->vars; e; e = e->next) {
        if (strcmp(e->name, name) == 0) { e->val = val; return; }
    }
    EnvEntry *e = arena_alloc(a, sizeof(EnvEntry));
    e->name = name;
    e->val  = val;
    e->next = env->vars;
    env->vars = e;
}

void global_set(Arena *a, GlobalTable *gt, const char *name, Value val) {
    for (GlobalEntry *e = gt->head; e; e = e->next) {
        if (strcmp(e->name, name) == 0) { e->val = val; return; }
    }
    GlobalEntry *e = arena_alloc(a, sizeof(GlobalEntry));
    e->name = name;
    e->val  = val;
    e->next = gt->head;
    gt->head = e;
}

int global_get(GlobalTable *gt, const char *name, Value *out) {
    for (GlobalEntry *e = gt->head; e; e = e->next) {
        if (strcmp(e->name, name) == 0) { *out = e->val; return 1; }
    }
    return 0;
}
