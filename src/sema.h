#ifndef STONED_SEMA_H
#define STONED_SEMA_H

#include "ast.h"
#include "parser.h"  /* reuse ParseError / MAX_ERRORS */

typedef struct LocalEntry {
    const char        *name;
    struct LocalEntry *next;
} LocalEntry;

typedef struct Scope {
    struct Scope *parent;
    int           is_def;   /* 1 = hard boundary: locals don't escape into here */
    LocalEntry   *locals;
} Scope;

typedef struct {
    Arena      *arena;
    Scope      *scope;
    int         loop_depth;   /* while/until nesting */
    int         block_depth;  /* do/{ block nesting */
    int         def_depth;    /* def nesting */
    int         lambda_depth; /* lambda body nesting */
    int         proc_depth;   /* proc/proc.new body nesting */
    int         rescue_depth; /* rescue body nesting */
    ParseError  errors[MAX_ERRORS];
    int         error_count;
} Sema;

void sema_init(Sema *s, Arena *arena);
void sema_run(Sema *s, Node *program);

#endif
