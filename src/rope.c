#include "rope.h"
#include <string.h>

RopeNode *rope_lit(Arena *a, const char *bytes, size_t len) {
    RopeNode *r = arena_alloc(a, sizeof(RopeNode));
    r->kind      = ROPE_LIT;
    r->lit.bytes = bytes;
    r->lit.len   = len;
    return r;
}

RopeNode *rope_expr(Arena *a, struct Node *node) {
    RopeNode *r  = arena_alloc(a, sizeof(RopeNode));
    r->kind      = ROPE_EXPR;
    r->expr.node = node;
    return r;
}

RopeNode *rope_cat(Arena *a, RopeNode *left, RopeNode *right) {
    RopeNode *r  = arena_alloc(a, sizeof(RopeNode));
    r->kind      = ROPE_CAT;
    r->cat.left  = left;
    r->cat.right = right;
    return r;
}

int rope_is_static(RopeNode *r) {
    if (!r) return 1;
    switch (r->kind) {
        case ROPE_LIT:  return 1;
        case ROPE_EXPR: return 0;
        case ROPE_CAT:  return rope_is_static(r->cat.left) &&
                               rope_is_static(r->cat.right);
    }
    return 0;
}

/* Measure total byte length of a static rope */
static size_t rope_measure(RopeNode *r) {
    if (!r) return 0;
    switch (r->kind) {
        case ROPE_LIT:  return r->lit.len;
        case ROPE_EXPR: return 0;
        case ROPE_CAT:  return rope_measure(r->cat.left) +
                               rope_measure(r->cat.right);
    }
    return 0;
}

static void rope_write(RopeNode *r, char *buf, size_t *pos) {
    if (!r) return;
    switch (r->kind) {
        case ROPE_LIT:
            memcpy(buf + *pos, r->lit.bytes, r->lit.len);
            *pos += r->lit.len;
            break;
        case ROPE_EXPR:
            break;
        case ROPE_CAT:
            rope_write(r->cat.left,  buf, pos);
            rope_write(r->cat.right, buf, pos);
            break;
    }
}

const char *rope_flatten(Arena *a, RopeNode *r) {
    if (!rope_is_static(r)) return NULL;
    size_t len = rope_measure(r);
    char  *buf = arena_alloc(a, len + 1);
    size_t pos = 0;
    rope_write(r, buf, &pos);
    buf[len] = '\0';
    return buf;
}
