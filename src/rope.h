#ifndef STONED_ROPE_H
#define STONED_ROPE_H

#include "arena.h"
#include <stddef.h>

/* Forward — ast.h includes us, so we can't include ast.h back */
struct Node;

typedef enum {
    ROPE_LIT,   /* static bytes, known at parse time */
    ROPE_EXPR,  /* interpolated expression — resolved at runtime */
    ROPE_CAT,   /* concatenation of two rope nodes */
} RopeKind;

typedef struct RopeNode {
    RopeKind kind;
    union {
        struct { const char *bytes; size_t len; }           lit;
        struct { struct Node *node; }                       expr;
        struct { struct RopeNode *left; struct RopeNode *right; } cat;
    };
} RopeNode;

RopeNode   *rope_lit(Arena *a, const char *bytes, size_t len);
RopeNode   *rope_expr(Arena *a, struct Node *node);
RopeNode   *rope_cat(Arena *a, RopeNode *left, RopeNode *right);

/* Flatten all ROPE_LIT segments into a C string.
   Returns NULL if any ROPE_EXPR nodes are present. */
const char *rope_flatten(Arena *a, RopeNode *r);

/* Returns 1 if the rope contains no ROPE_EXPR nodes */
int         rope_is_static(RopeNode *r);

#endif
