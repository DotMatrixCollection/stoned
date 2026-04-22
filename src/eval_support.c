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

void eval_push_frame(Eval *ev, uint32_t line, uint32_t col, const char *label) {
    if (ev->frame_count >= EVAL_MAX_DEPTH) return;
    ev->frames[ev->frame_count].line = line;
    ev->frames[ev->frame_count].col = col;
    ev->frames[ev->frame_count].label = label;
    ev->frame_count++;
}

void eval_pop_frame(Eval *ev) {
    if (ev->frame_count > 0) ev->frame_count--;
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
    Value backtrace = val_array_new();
    for (int i = ev->frame_count - 1; i >= 0; i--) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%u:%u:in `%s`",
                 ev->frames[i].line, ev->frames[i].col, ev->frames[i].label);
        val_array_push(&backtrace, val_string(ev->arena, buf));
    }
    val_object_set_ivar(ev->arena, exc, "backtrace", backtrace);
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

Value exception_value_backtrace(Value exc) {
    Value backtrace;
    if (exc.kind == VAL_OBJECT && val_object_get_ivar(exc, "backtrace", &backtrace))
        return backtrace;
    return val_array_new();
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

static void bind_pattern(Eval *ev, Env *env, Node *pattern, Value val) {
    if (!pattern) return;

    if (pattern->kind == NODE_PARAM) {
        if (pattern->param.name)
            env_set(ev->arena, env, pattern->param.name, val);
        return;
    }

    if (pattern->kind != NODE_ARRAY) return;

    size_t len = 0;
    size_t splat_index = (size_t)-1;
    for (NodeList *l = pattern->array.elements; l; l = l->next, len++) {
        if (l->node && l->node->kind == NODE_PARAM && l->node->param.splat)
            splat_index = len;
    }

    size_t idx = 0;
    for (NodeList *l = pattern->array.elements; l; l = l->next, idx++) {
        if (l->node && l->node->kind == NODE_PARAM && l->node->param.splat) {
            Value rest = val_array_new();
            size_t tail_count = len - idx - 1;
            size_t available = 0;
            if (val.kind == VAL_ARRAY && val.array)
                available = val.array->len;
            else if (idx == 0)
                available = 1;

            size_t rest_end = available > tail_count ? available - tail_count : 0;
            for (size_t j = idx; j < rest_end; j++) {
                Value elem = val_nil();
                if (val.kind == VAL_ARRAY && val.array && j < val.array->len)
                    elem = val.array->elems[j];
                else if (j == 0 && val.kind != VAL_ARRAY)
                    elem = val;
                val_array_push(&rest, elem);
            }
            bind_pattern(ev, env, l->node, rest);
            continue;
        }

        Value elem = val_nil();
        if (splat_index != (size_t)-1 && idx > splat_index) {
            size_t tail_offset = len - idx;
            if (val.kind == VAL_ARRAY && val.array && val.array->len >= tail_offset)
                elem = val.array->elems[val.array->len - tail_offset];
        } else if (val.kind == VAL_ARRAY && val.array && idx < val.array->len) {
            elem = val.array->elems[idx];
        } else if (idx == 0 && val.kind != VAL_ARRAY) {
            elem = val;
        }
        bind_pattern(ev, env, l->node, elem);
    }
}

int count_required_params(NodeList *params) {
    int count = 0;
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM) {
            if (!p->param.splat && !p->param.block_param)
                count++;
        } else if (p->kind == NODE_ARRAY) {
            count++;
        }
    }
    return count;
}

int has_splat_param(NodeList *params) {
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM && p->param.splat) return 1;
        if (p->kind == NODE_ARRAY && has_splat_param(p->array.elements)) return 1;
    }
    return 0;
}

void bind_params(Eval *ev, Env *env, NodeList *params, Value *args, int argc) {
    int pi = 0;
    for (NodeList *pl = params; pl; pl = pl->next, pi++) {
        Node *p = pl->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM && p->param.splat) {
            Value rest = val_array_new();
            for (int j = pi; j < argc; j++) val_array_push(&rest, args[j]);
            bind_pattern(ev, env, p, rest);
            break;
        }

        Value pval = pi < argc ? args[pi]
                   : (p->kind == NODE_PARAM && p->param.default_val
                      ? eval_node(ev, env, p->param.default_val)
                      : val_nil());
        bind_pattern(ev, env, p, pval);
    }
}

Value call_block(Eval *ev, Value blk, Value *args, int argc, Node *call_site) {
    if (blk.kind != VAL_BLOCK)
        return eval_raise_class(ev, call_site, "LocalJumpError", "no block given");

    Node *bn      = blk.block.block_node;
    Env *closure  = blk.block.closure;
    Env *frame    = env_new(ev->arena, closure, 0);
    NodeList *pl  = bn->block.params;

    if (blk.block.is_lambda && !has_splat_param(pl) && argc != count_required_params(pl))
        return eval_raise_class(ev, call_site, "ArgumentError", "wrong number of arguments");

    bind_params(ev, frame, pl, args, argc);

    eval_push_frame(ev, call_site ? call_site->span.line : 0,
                    call_site ? call_site->span.col : 0, "block");
    Value result = eval_node(ev, frame, bn->block.body);
    eval_pop_frame(ev);
    if (blk.block.is_lambda && result.kind == VAL_RETURN) return *result.wrapped;
    if (result.kind == VAL_NEXT) return *result.wrapped;
    return result;
}
