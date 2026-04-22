#include "eval_internal.h"
#include "parser.h"
#include "sema.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file_bytes(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, len, f);
    fclose(f);
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

static const char *normalize_path(Arena *a, const char *path) {
    size_t len = strlen(path);
    char *tmp = malloc(len + 1);
    if (!tmp) return NULL;
    memcpy(tmp, path, len + 1);

    const char *segments[256];
    int abs = path[0] == '/';
    size_t count = 0;
    char *cursor = tmp;
    while (*cursor) {
        while (*cursor == '/') cursor++;
        if (!*cursor) break;
        char *part = cursor;
        while (*cursor && *cursor != '/') cursor++;
        if (*cursor) *cursor++ = '\0';

        if (strcmp(part, ".") == 0 || part[0] == '\0')
            continue;
        if (strcmp(part, "..") == 0) {
            if (count > 0 && strcmp(segments[count - 1], "..") != 0) {
                count--;
            } else if (!abs) {
                segments[count++] = part;
            }
            continue;
        }
        segments[count++] = part;
    }

    size_t total = abs ? 1 : 0;
    if (count == 0 && !abs) total = 1;
    for (size_t i = 0; i < count; i++)
        total += strlen(segments[i]) + (i + 1 < count ? 1 : 0);

    char *joined = arena_alloc(a, total + 1);
    size_t pos = 0;
    if (abs) joined[pos++] = '/';
    if (count == 0 && !abs) joined[pos++] = '.';
    for (size_t i = 0; i < count; i++) {
        size_t slen = strlen(segments[i]);
        memcpy(joined + pos, segments[i], slen);
        pos += slen;
        if (i + 1 < count) joined[pos++] = '/';
    }
    joined[pos] = '\0';
    free(tmp);
    return joined;
}

static const char *resolve_relative_path(Arena *a, const char *base_file, const char *rel) {
    if (!base_file || !rel) return NULL;

    const char *slash = strrchr(base_file, '/');
    size_t dir_len = slash ? (size_t)(slash - base_file) : 0;
    size_t rel_len = strlen(rel);
    int needs_rb = rel_len < 3 || strcmp(rel + rel_len - 3, ".rb") != 0;
    size_t total = dir_len + (dir_len ? 1 : 0) + rel_len + (needs_rb ? 3 : 0) + 1;
    char *joined = malloc(total);
    if (!joined) return NULL;

    if (dir_len) {
        memcpy(joined, base_file, dir_len);
        joined[dir_len] = '/';
        memcpy(joined + dir_len + 1, rel, rel_len);
        joined[dir_len + 1 + rel_len] = '\0';
    } else {
        memcpy(joined, rel, rel_len + 1);
    }
    if (needs_rb) strcat(joined, ".rb");

    const char *copy = normalize_path(a, joined);
    free(joined);
    return copy;
}

static int eval_has_loaded_file(Eval *ev, const char *path) {
    for (LoadedFile *entry = ev->loaded_files; entry; entry = entry->next) {
        if (strcmp(entry->path, path) == 0)
            return 1;
    }
    return 0;
}

static void eval_mark_loaded_file(Eval *ev, const char *path) {
    LoadedFile *entry = arena_alloc(ev->arena, sizeof(LoadedFile));
    entry->path = path;
    entry->next = ev->loaded_files;
    ev->loaded_files = entry;
}

static Value eval_require_path(Eval *ev, const char *resolved, Node *site) {
    if (eval_has_loaded_file(ev, resolved))
        return val_false();

    size_t src_len = 0;
    char *src = read_file_bytes(resolved, &src_len);
    if (!src)
        return eval_raise_class(ev, site, "LoadError", "cannot load such file -- %s", resolved);

    Parser parser;
    parser_init(&parser, src, src_len, ev->arena);
    Node *tree = parse_program(&parser);
    if (parser.error_count) {
        Value err = eval_raise_class(ev, site, "LoadError", "parse error in %s: %s",
                                     resolved, parser.errors[0].message);
        free(src);
        return err;
    }

    Sema sema;
    sema_init(&sema, ev->arena);
    sema_run(&sema, tree);
    if (sema.error_count) {
        Value err = eval_raise_class(ev, site, "LoadError", "sema error in %s: %s",
                                     resolved, sema.errors[0].message);
        free(src);
        return err;
    }

    const char *previous_file = ev->current_file;
    ev->current_file = resolved;
    Value result = eval_node(ev, ev->top_env, tree);
    ev->current_file = previous_file;
    free(src);

    if (val_is_signal(result)) return result;
    eval_mark_loaded_file(ev, resolved);
    return val_true();
}

static const char *resolve_require_path(Arena *a, const char *base_file, const char *path, int current_dir) {
    if (!path) return NULL;
    if (strchr(path, '/')) {
        if (base_file)
            return resolve_relative_path(a, base_file, path);
        size_t len = strlen(path);
        int needs_rb = len < 3 || strcmp(path + len - 3, ".rb") != 0;
        char *joined = malloc(len + (needs_rb ? 3 : 0) + 1);
        if (!joined) return NULL;
        memcpy(joined, path, len + 1);
        if (needs_rb) strcat(joined, ".rb");
        const char *copy = normalize_path(a, joined);
        free(joined);
        return copy;
    }

    const char *base = current_dir ? "./entry.rb" : base_file;
    return resolve_relative_path(a, base, path);
}

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

MethodVisibility current_method_visibility(Env *env) {
    Value visibility;
    if (env_get(env, "__visibility__", &visibility) && visibility.kind == VAL_SYMBOL) {
        if (strcmp(visibility.sval, "private") == 0) return METHOD_PRIVATE;
        if (strcmp(visibility.sval, "protected") == 0) return METHOD_PROTECTED;
    }
    return METHOD_PUBLIC;
}

void set_current_method_visibility(Arena *a, Env *env, MethodVisibility visibility) {
    const char *name = "public";
    if (visibility == METHOD_PRIVATE) name = "private";
    else if (visibility == METHOD_PROTECTED) name = "protected";
    env_define(a, env, "__visibility__", val_symbol(name));
}

void update_method_visibility(Env *env, const char *name, MethodVisibility visibility, int singleton_only) {
    for (EnvEntry *entry = env ? env->vars : NULL; entry; entry = entry->next) {
        if (strcmp(entry->name, name) != 0) continue;
        if (entry->val.kind != VAL_METHOD) continue;
        if (singleton_only && strncmp(entry->name, "self.", 5) != 0) continue;
        entry->val.method.visibility = visibility;
    }
}

static int class_inherits_from(RubyClass *klass, RubyClass *target) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        if (k == target) return 1;
    }
    return 0;
}

int method_visibility_allows_call(Eval *ev, Env *env, Value recv, RubyClass *owner,
                                  MethodVisibility visibility, int public_only, int explicit_receiver) {
    (void)ev;
    if (explicit_receiver < 0) return 1;
    if (visibility == METHOD_PUBLIC) return 1;
    if (public_only) return 0;
    if (visibility == METHOD_PRIVATE && explicit_receiver) return 0;

    Value current_self = val_nil();
    int has_current_self = env && env_get(env, "self", &current_self);

    if (visibility == METHOD_PRIVATE) {
        if (!has_current_self) return 0;
        if (current_self.kind != recv.kind) return 0;
        if (recv.kind == VAL_OBJECT) return current_self.obj == recv.obj;
        if (recv.kind == VAL_CLASS) return current_self.klass == recv.klass;
        return val_equal(current_self, recv);
    }

    if (visibility == METHOD_PROTECTED) {
        if (!has_current_self || current_self.kind != VAL_OBJECT || recv.kind != VAL_OBJECT || !owner)
            return 0;
        RubyClass *caller_class = current_self.obj->klass.kind == VAL_CLASS ? current_self.obj->klass.klass : NULL;
        RubyClass *recv_class = recv.obj->klass.kind == VAL_CLASS ? recv.obj->klass.klass : NULL;
        if (!caller_class || !recv_class) return 0;
        return class_inherits_from(caller_class, owner) && class_inherits_from(recv_class, owner);
    }

    return 0;
}

int ruby_class_find_instance_method(RubyClass *klass, const char *name, Value *out, RubyClass **owner) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        for (RubyModuleInclusion *inc = k->prepended_modules; inc; inc = inc->next) {
            RubyClass *module_owner = NULL;
            if (ruby_class_find_instance_method(inc->mod, name, out, &module_owner)) {
                if (owner) *owner = module_owner;
                return 1;
            }
        }
        if (env_get(k->class_env, name, out) && out->kind == VAL_METHOD) {
            if (owner) *owner = k;
            return 1;
        }
        for (RubyModuleInclusion *inc = k->included_modules; inc; inc = inc->next) {
            RubyClass *module_owner = NULL;
            if (ruby_class_find_instance_method(inc->mod, name, out, &module_owner)) {
                if (owner) *owner = module_owner;
                return 1;
            }
        }
        if (k->is_module) break;
    }
    return 0;
}

static int ruby_class_find_super_method_inner(RubyClass *klass, RubyClass *after, int *seen_after,
                                              const char *name, Value *out, RubyClass **owner) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        for (RubyModuleInclusion *inc = k->prepended_modules; inc; inc = inc->next) {
            if (ruby_class_find_super_method_inner(inc->mod, after, seen_after, name, out, owner))
                return 1;
        }

        if (*seen_after && env_get(k->class_env, name, out) && out->kind == VAL_METHOD) {
            if (owner) *owner = k;
            return 1;
        }
        if (k == after) *seen_after = 1;

        for (RubyModuleInclusion *inc = k->included_modules; inc; inc = inc->next) {
            if (ruby_class_find_super_method_inner(inc->mod, after, seen_after, name, out, owner))
                return 1;
        }

        if (k->is_module) break;
    }
    return 0;
}

int ruby_class_find_super_method(RubyClass *start, RubyClass *after, const char *name, Value *out, RubyClass **owner) {
    int seen_after = 0;
    return ruby_class_find_super_method_inner(start, after, &seen_after, name, out, owner);
}

static int ruby_class_has_module(RubyClass *klass, RubyClass *target) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        if (k == target) return 1;
        for (RubyModuleInclusion *inc = k->prepended_modules; inc; inc = inc->next) {
            if (inc->mod == target || ruby_class_has_module(inc->mod, target))
                return 1;
        }
        for (RubyModuleInclusion *inc = k->included_modules; inc; inc = inc->next) {
            if (inc->mod == target || ruby_class_has_module(inc->mod, target))
                return 1;
        }
        if (k->is_module) break;
    }
    return 0;
}

int value_has_module(Eval *ev, Value recv, const char *module_name) {
    Value mod;
    if (!env_get(ev->top_env, module_name, &mod) || mod.kind != VAL_CLASS)
        return 0;

    RubyClass *klass = NULL;
    if (recv.kind == VAL_OBJECT) klass = recv.obj->klass.klass;
    else {
        const char *class_name = NULL;
        switch (recv.kind) {
            case VAL_INT: class_name = "Integer"; break;
            case VAL_FLOAT: class_name = "Float"; break;
            case VAL_STRING: class_name = "String"; break;
            case VAL_ARRAY: class_name = "Array"; break;
            case VAL_HASH: class_name = "Hash"; break;
            case VAL_NIL: class_name = "NilClass"; break;
            case VAL_BOOL: class_name = recv.bval ? "TrueClass" : "FalseClass"; break;
            case VAL_CLASS: klass = recv.klass; break;
            default: break;
        }
        if (!klass && class_name) {
            Value klass_val;
            if (env_get(ev->top_env, class_name, &klass_val) && klass_val.kind == VAL_CLASS)
                klass = klass_val.klass;
        }
    }
    if (!klass) return 0;
    return ruby_class_has_module(klass, mod.klass);
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

Value eval_require_relative(Eval *ev, Env *env, const char *path, Node *site) {
    (void)env;
    if (!ev->current_file)
        return eval_raise_class(ev, site, "LoadError", "require_relative requires a current file");

    const char *resolved = resolve_relative_path(ev->arena, ev->current_file, path);
    if (!resolved)
        return eval_raise_class(ev, site, "LoadError", "cannot resolve require_relative path '%s'", path);
    return eval_require_path(ev, resolved, site);
}

Value eval_require(Eval *ev, Env *env, const char *path, Node *site) {
    (void)env;
    const char *resolved = NULL;

    if (ev->current_file)
        resolved = resolve_require_path(ev->arena, ev->current_file, path, 0);
    if (resolved) {
        FILE *f = fopen(resolved, "rb");
        if (f) {
            fclose(f);
            return eval_require_path(ev, resolved, site);
        }
    }

    resolved = resolve_require_path(ev->arena, NULL, path, 1);
    if (resolved) {
        FILE *f = fopen(resolved, "rb");
        if (f) {
            fclose(f);
            return eval_require_path(ev, resolved, site);
        }
    }

    return eval_raise_class(ev, site, "LoadError", "cannot load such file -- %s", path);
}
