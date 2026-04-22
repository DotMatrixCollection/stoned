#include "eval_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void format_exception_summary(Eval *ev, const char *class_name, const char *msg) {
    size_t cls_len = strlen(class_name);
    size_t max_msg = sizeof(ev->exception_msg) - cls_len - 3;
    snprintf(ev->exception_msg, sizeof(ev->exception_msg), "%s: %.*s",
             class_name, (int)max_msg, msg);
}

int value_is_a_named_class(Eval *ev, Value v, const char *class_name) {
    if (v.kind != VAL_OBJECT) return 0;
    Value klass;
    if (!env_get(ev->top_env, class_name, &klass) || klass.kind != VAL_CLASS)
        return 0;
    RubyClass *k = v.obj->klass.klass;
    while (k) {
        if (k == klass.klass || strcmp(k->name, class_name) == 0) return 1;
        k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL;
    }
    return 0;
}

const char *exception_value_class_name(Value exc) {
    if (exc.kind != VAL_OBJECT || exc.obj->klass.kind != VAL_CLASS || !exc.obj->klass.klass)
        return "RuntimeError";
    return exc.obj->klass.klass->name;
}

const char *exception_value_message(Eval *ev, Value exc) {
    Value msg;
    if (exc.kind == VAL_OBJECT && val_object_get_ivar(exc, "message", &msg)) {
        return val_to_s(ev->arena, msg);
    }
    return exception_value_class_name(exc);
}

static Value build_exception(Eval *ev, const char *class_name, const char *msg) {
    Value klass;
    if (!env_get(ev->top_env, class_name, &klass) || klass.kind != VAL_CLASS) {
        env_get(ev->top_env, "RuntimeError", &klass);
    }
    Value exc = val_object(ev->arena, klass);
    val_object_set_ivar(ev->arena, exc, "message", val_string(ev->arena, msg ? msg : class_name));
    return exc;
}

static void set_exception_origin(Arena *arena, Value exc, uint32_t line, uint32_t col) {
    if (exc.kind != VAL_OBJECT) return;
    val_object_set_ivar(arena, exc, "line", val_int((int64_t)line));
    val_object_set_ivar(arena, exc, "col", val_int((int64_t)col));
}

uint32_t exception_value_line(Value exc) {
    Value line;
    if (exc.kind == VAL_OBJECT && val_object_get_ivar(exc, "line", &line) && line.kind == VAL_INT)
        return (uint32_t)line.ival;
    return 0;
}

uint32_t exception_value_col(Value exc) {
    Value col;
    if (exc.kind == VAL_OBJECT && val_object_get_ivar(exc, "col", &col) && col.kind == VAL_INT)
        return (uint32_t)col.ival;
    return 0;
}

Value eval_error(Eval *ev, Node *n, const char *fmt, ...) {
    if (!ev->errored) {
        ev->errored = 1;
        va_list ap;
        va_start(ap, fmt);
        char tmp[480];
        vsnprintf(tmp, sizeof(tmp), fmt, ap);
        va_end(ap);
        if (n) {
            snprintf(ev->errmsg, sizeof(ev->errmsg),
                     "%u:%u: %s", n->span.line, n->span.col, tmp);
        } else {
            snprintf(ev->errmsg, sizeof(ev->errmsg), "%s", tmp);
        }
    }
    return val_nil();
}

Value eval_raise(Eval *ev, Node *n, const char *fmt, ...) {
    if (ev->exception_msg[0] == '\0') {
        ev->exception_class = "RuntimeError";
        va_list ap;
        va_start(ap, fmt);
        char raw_msg[512];
        vsnprintf(raw_msg, sizeof(raw_msg), fmt, ap);
        va_end(ap);
        ev->current_exception = build_exception(ev, ev->exception_class, raw_msg);
        if (n) set_exception_origin(ev->arena, ev->current_exception, n->span.line, n->span.col);
        format_exception_summary(ev, ev->exception_class, raw_msg);
        if (n) {
            ev->exception_line = n->span.line;
            ev->exception_col = n->span.col;
        }
    }
    return val_exception();
}

Value eval_raise_class(Eval *ev, Node *n, const char *class_name, const char *fmt, ...) {
    if (ev->exception_msg[0] == '\0') {
        ev->exception_class = class_name;
        va_list ap;
        va_start(ap, fmt);
        char raw_msg[512];
        vsnprintf(raw_msg, sizeof(raw_msg), fmt, ap);
        va_end(ap);
        ev->current_exception = build_exception(ev, class_name, raw_msg);
        if (n) set_exception_origin(ev->arena, ev->current_exception, n->span.line, n->span.col);
        format_exception_summary(ev, class_name, raw_msg);
        if (n) {
            ev->exception_line = n->span.line;
            ev->exception_col = n->span.col;
        }
    }
    return val_exception();
}

Value eval_raise_value(Eval *ev, Node *n, Value exc) {
    if (ev->exception_msg[0] == '\0') {
        ev->current_exception = exc;
        ev->exception_class = exception_value_class_name(exc);
        snprintf(ev->exception_msg, sizeof(ev->exception_msg), "%s: %s",
                 ev->exception_class, exception_value_message(ev, exc));
        ev->exception_line = exception_value_line(exc);
        ev->exception_col = exception_value_col(exc);
        if ((ev->exception_line == 0 || ev->exception_col == 0) && n) {
            ev->exception_line = n->span.line;
            ev->exception_col = n->span.col;
            set_exception_origin(ev->arena, exc, n->span.line, n->span.col);
        }
    }
    return val_exception();
}

void eval_clear_exception(Eval *ev) {
    ev->current_exception = val_nil();
    ev->exception_line = 0;
    ev->exception_col = 0;
    ev->exception_class = NULL;
    ev->exception_msg[0] = '\0';
}

static void rope_collect(Eval *ev, Env *env, RopeNode *r,
                         char **buf, size_t *len, size_t *cap) {
    if (!r) return;
    switch (r->kind) {
        case ROPE_LIT:
            if (r->lit.bytes && r->lit.len) {
                while (*len + r->lit.len + 1 > *cap) {
                    *cap  = (*cap < 64) ? 128 : *cap * 2;
                    *buf  = realloc(*buf, *cap);
                }
                memcpy(*buf + *len, r->lit.bytes, r->lit.len);
                *len += r->lit.len;
            }
            break;
        case ROPE_EXPR: {
            Value v = eval_node(ev, env, r->expr.node);
            if (ev->errored || val_is_signal(v)) return;
            const char *s = val_to_s(ev->arena, v);
            size_t slen = strlen(s);
            while (*len + slen + 1 > *cap) {
                *cap  = (*cap < 64) ? 128 : *cap * 2;
                *buf  = realloc(*buf, *cap);
            }
            memcpy(*buf + *len, s, slen);
            *len += slen;
            break;
        }
        case ROPE_CAT:
            rope_collect(ev, env, r->cat.left,  buf, len, cap);
            rope_collect(ev, env, r->cat.right, buf, len, cap);
            break;
    }
}

const char *eval_rope(Eval *ev, Env *env, RopeNode *r) {
    size_t len = 0, cap = 128;
    char *buf = malloc(cap);
    rope_collect(ev, env, r, &buf, &len, &cap);
    buf[len] = '\0';
    char *result = arena_alloc(ev->arena, len + 1);
    memcpy(result, buf, len + 1);
    free(buf);
    return result;
}

Value call_block(Eval *ev, Value blk, Value *args, int argc, Node *call_site) {
    if (blk.kind != VAL_BLOCK)
        return eval_error(ev, call_site, "no block given");

    Node *bn      = blk.block.block_node;
    Env *closure  = blk.block.closure;
    Env *frame    = env_new(ev->arena, closure, 0);
    NodeList *pl  = bn->block.params;

    for (int i = 0; pl && i < argc; i++, pl = pl->next) {
        Node *p = pl->node;
        if (p && p->kind == NODE_PARAM && p->param.name)
            env_set(ev->arena, frame, p->param.name, args[i]);
    }

    Value result = eval_node(ev, frame, bn->block.body);
    if (result.kind == VAL_NEXT) return *result.wrapped;
    return result;
}
