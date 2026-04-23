#include "eval_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (ev->errored || val_is_signal(v)) return (v); } while(0)

static int val_responds_to(Eval *ev, Value recv, const char *name, int include_private);

static void copy_module_methods(Eval *ev, Env *target, RubyClass *mod, int singleton_prefix) {
    if (!mod || !target) return;
    for (EnvEntry *entry = mod->class_env ? mod->class_env->vars : NULL; entry; entry = entry->next) {
        if (strcmp(entry->name, "self") == 0) continue;
        if (entry->val.kind != VAL_METHOD) continue;

        const char *name = entry->name;
        if (strncmp(name, "self.", 5) == 0) continue;

        if (singleton_prefix) {
            size_t nlen = strlen(name);
            char *key = arena_alloc(ev->arena, nlen + 6);
            memcpy(key, "self.", 5);
            memcpy(key + 5, name, nlen + 1);
            env_define(ev->arena, target, key, entry->val);
        } else {
            env_define(ev->arena, target, name, entry->val);
        }
    }
}

static Value builtin_extend(Eval *ev, Value self, Value *args, int argc, Node *site) {
    if (self.kind != VAL_CLASS && self.kind != VAL_OBJECT)
        return eval_raise_class(ev, site, "TypeError", "extend requires an object");

    for (int i = 0; i < argc; i++) {
        if (args[i].kind != VAL_CLASS || !args[i].klass->is_module)
            return eval_raise_class(ev, site, "TypeError", "extend requires a Module");
    }

    if (self.kind == VAL_CLASS) {
        for (int i = 0; i < argc; i++)
            copy_module_methods(ev, self.klass->class_env, args[i].klass, 1);
    } else {
        if (!self.obj->singleton_env)
            self.obj->singleton_env = env_new(ev->arena, NULL, 1);
        for (int i = 0; i < argc; i++)
            copy_module_methods(ev, self.obj->singleton_env, args[i].klass, 0);
    }
    return self;
}

static void define_attr_reader(Eval *ev, Value klass, const char *attr) {
    if (klass.kind != VAL_CLASS) return;
    Arena *a = ev->arena;

    Node *ivar_node = arena_alloc(a, sizeof(Node));
    memset(ivar_node, 0, sizeof(Node));
    ivar_node->kind = NODE_IVAR;
    ivar_node->sval = attr;

    NodeList *stmts = arena_alloc(a, sizeof(NodeList));
    stmts->node = ivar_node;
    stmts->next = NULL;

    Node *body = arena_alloc(a, sizeof(Node));
    memset(body, 0, sizeof(Node));
    body->kind = NODE_BODY;
    body->body.stmts = stmts;

    Node *def = arena_alloc(a, sizeof(Node));
    memset(def, 0, sizeof(Node));
    def->kind = NODE_DEF;
    def->def.name = attr;
    def->def.body = body;

    env_define(a, klass.klass->class_env, attr, val_method(def, ev->top_env, METHOD_PUBLIC));
}

static void define_attr_writer(Eval *ev, Value klass, const char *attr) {
    if (klass.kind != VAL_CLASS) return;
    Arena *a = ev->arena;
    size_t alen = strlen(attr);

    char *method_name = arena_alloc(a, alen + 2);
    memcpy(method_name, attr, alen);
    method_name[alen] = '=';
    method_name[alen + 1] = '\0';

    Node *param = arena_alloc(a, sizeof(Node));
    memset(param, 0, sizeof(Node));
    param->kind = NODE_PARAM;
    param->param.name = "value";

    NodeList *params = arena_alloc(a, sizeof(NodeList));
    params->node = param;
    params->next = NULL;

    Node *ivar_target = arena_alloc(a, sizeof(Node));
    memset(ivar_target, 0, sizeof(Node));
    ivar_target->kind = NODE_IVAR;
    ivar_target->sval = attr;

    Node *value_node = arena_alloc(a, sizeof(Node));
    memset(value_node, 0, sizeof(Node));
    value_node->kind = NODE_LVAR;
    value_node->sval = "value";

    Node *assign = arena_alloc(a, sizeof(Node));
    memset(assign, 0, sizeof(Node));
    assign->kind = NODE_ASSIGN;
    assign->assign.target = ivar_target;
    assign->assign.value = value_node;

    NodeList *stmts = arena_alloc(a, sizeof(NodeList));
    stmts->node = assign;
    stmts->next = NULL;

    Node *body = arena_alloc(a, sizeof(Node));
    memset(body, 0, sizeof(Node));
    body->kind = NODE_BODY;
    body->body.stmts = stmts;

    Node *def = arena_alloc(a, sizeof(Node));
    memset(def, 0, sizeof(Node));
    def->kind = NODE_DEF;
    def->def.name = method_name;
    def->def.params = params;
    def->def.body = body;

    env_define(a, klass.klass->class_env, method_name, val_method(def, ev->top_env, METHOD_PUBLIC));
}

static const char *prim_class_name(Value v) {
    switch (v.kind) {
        case VAL_INT:    return "Integer";
        case VAL_FLOAT:  return "Float";
        case VAL_STRING: return "String";
        case VAL_SYMBOL: return "Symbol";
        case VAL_ARRAY:  return "Array";
        case VAL_HASH:   return "Hash";
        case VAL_RANGE:  return "Range";
        case VAL_NIL:    return "NilClass";
        case VAL_BOOL:   return v.bval ? "TrueClass" : "FalseClass";
        case VAL_CLASS:  return "Class";
        case VAL_METHOD: return "Method";
        case VAL_BLOCK:  return "Proc";
        case VAL_OBJECT: return v.obj->klass.klass ? v.obj->klass.klass->name : "Object";
        default:         return "Object";
    }
}

static Value dispatch_dynamic_send(Eval *ev, Env *env, Value recv, const char *dispatch_name,
                                   Value *args, int argc, Value *blk, Node *site, int public_only) {
    if (argc < 1)
        return eval_raise_class(ev, site, "ArgumentError", "%s requires a method name", dispatch_name);
    const char *mname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING)
                        ? args[0].sval : NULL;
    if (!mname)
        return eval_raise_class(ev, site, "TypeError", "%s: method name must be a symbol or string", dispatch_name);
    if (public_only && !val_responds_to(ev, recv, mname, 0) && val_responds_to(ev, recv, mname, 1))
        return eval_raise_class(ev, site, "NoMethodError", "undefined method '%s' for %s", mname, val_kind_name(recv.kind));
    int explicit_receiver = public_only ? 0 : -1;
    return dispatch_method(ev, env, recv, mname, args + 1, argc - 1, blk, site, public_only, explicit_receiver);
}

int val_is_a(Value v, Value klass_arg) {
    if (klass_arg.kind != VAL_CLASS) return 0;
    const char *kname = klass_arg.klass->name;
    if (strcmp(kname, "Object") == 0 || strcmp(kname, "BasicObject") == 0) return 1;
    if (v.kind == VAL_CLASS) {
        if (strcmp(kname, "Module") == 0) return 1;
        if (strcmp(kname, "Class") == 0) return !v.klass->is_module;
        return 0;
    }
    if (v.kind == VAL_OBJECT) {
        RubyClass *k = v.obj->klass.klass;
        while (k) {
            if (strcmp(k->name, kname) == 0) return 1;
            k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL;
        }
        return 0;
    }
    if (strcmp(kname, "Numeric") == 0)
        return v.kind == VAL_INT || v.kind == VAL_FLOAT;
    if (strcmp(kname, "Enumerable") == 0)
        return v.kind == VAL_RANGE;
    return strcmp(prim_class_name(v), kname) == 0;
}

static Value val_class_of(Eval *ev, Value v) {
    if (v.kind == VAL_OBJECT) return v.obj->klass;
    const char *kname = prim_class_name(v);
    if (v.kind == VAL_CLASS && v.klass->is_module) kname = "Module";
    Value klass;
    if (env_get(ev->top_env, kname, &klass) && klass.kind == VAL_CLASS) return klass;
    Value stub = val_class(ev->arena, kname, val_nil());
    stub.klass->class_env = env_new(ev->arena, ev->top_env, 1);
    return stub;
}

static int method_visible_for_respond_to(Value method, int include_private) {
    return method.kind == VAL_METHOD && (include_private || method.method.visibility == METHOD_PUBLIC);
}

static int builtin_primitive_responds_to(Value recv, const char *name) {
    static const char *int_methods[] = {
        "to_s", "to_f", "to_i", "to_int", "to_r", "abs", "abs2", "even?", "odd?", "zero?",
        "nonzero?", "positive?", "negative?", "integer?", "ceil", "floor", "round",
        "truncate", "succ", "next", "pred", "chr", "gcd", "lcm", "pow", "divmod",
        "digits", "between?", "clamp", "times", "upto", "downto", "step", NULL
    };
    static const char *float_methods[] = {
        "to_s", "to_f", "to_i", "truncate", "to_r", "abs", "abs2", "zero?", "nonzero?",
        "positive?", "negative?", "integer?", "nan?", "finite?", "infinite?", "ceil",
        "floor", "round", "divmod", "between?", "clamp", "step", NULL
    };
    static const char *str_methods[] = {
        "to_s", "to_i", "to_f", "to_sym", "length", "size", "empty?", "upcase",
        "downcase", "strip", "chars", "include?", "start_with?", "end_with?", "split",
        "each_char", "reverse", "next", "succ", "replace", "inspect", "chomp", "chop",
        "lstrip", "rstrip", "capitalize", "swapcase", "ljust", "rjust", "center", "ord",
        "hex", "oct", "bytes", "<<", "index", "rindex", "[]", "slice", "lines",
        "each_line", "tr", "count", "delete", "squeeze", "scan", "sub", "gsub", "*", NULL
    };
    static const char *arr_methods[] = {
        "length", "size", "count", "empty?", "first", "last", "push", "append", "pop",
        "shift", "unshift", "prepend", "reverse", "to_s", "inspect", "join", "include?",
        "each", "each_with_index", "map", "collect", "select", "filter", "reject",
        "reduce", "inject", "any?", "all?", "none?", "min", "max", "sum", "flatten",
        "uniq", "sort", "compact", "zip", NULL
    };
    static const char *hash_methods[] = {
        "[]", "[]=", "fetch", "has_key?", "key?", "include?", "member?", "has_value?",
        "value?", "delete", "keys", "values", "length", "size", "count", "empty?",
        "to_s", "inspect", "to_a", "merge", "merge!", "update", "each", "each_pair",
        "each_key", "each_value", "map", "collect", "select", "filter", "reject", "any?",
        "all?", "min_by", "max_by", "sort_by", "flat_map", "reduce", "inject", "store",
        "clear", "dup", "nil?", NULL
    };
    static const char *proc_methods[] = {
        "call", "[]", "lambda?", "arity", "to_s", "inspect", NULL
    };
    static const char *range_methods[] = {
        "begin", "first", "end", "last", "exclude_end?",
        "include?", "member?", "cover?", "===",
        "each", "each_with_index", "to_a", "entries",
        "size", "count", "length", "min", "max", "step",
        "map", "collect", "select", "filter", "reject",
        "to_s", "inspect", NULL
    };
    static const char *nil_methods[] = {
        "nil?", "to_s", "inspect", NULL
    };
    static const char *bool_methods[] = {
        "to_s", "inspect", "!", "nil?", NULL
    };

    const char **methods = NULL;
    if (recv.kind == VAL_INT) methods = int_methods;
    else if (recv.kind == VAL_FLOAT) methods = float_methods;
    else if (recv.kind == VAL_STRING) methods = str_methods;
    else if (recv.kind == VAL_ARRAY) methods = arr_methods;
    else if (recv.kind == VAL_HASH) methods = hash_methods;
    else if (recv.kind == VAL_RANGE) methods = range_methods;
    else if (recv.kind == VAL_BLOCK) methods = proc_methods;
    else if (recv.kind == VAL_NIL) methods = nil_methods;
    else if (recv.kind == VAL_BOOL) methods = bool_methods;
    if (!methods) return 0;

    for (int i = 0; methods[i]; i++) {
        if (strcmp(name, methods[i]) == 0) return 1;
    }
    return 0;
}

static int val_responds_to(Eval *ev, Value recv, const char *name, int include_private) {
    if (strcmp(name, "is_a?") == 0 || strcmp(name, "kind_of?") == 0 ||
        strcmp(name, "instance_of?") == 0 || strcmp(name, "class") == 0 ||
        strcmp(name, "nil?") == 0 || strcmp(name, "respond_to?") == 0 ||
        strcmp(name, "send") == 0 || strcmp(name, "__send__") == 0 ||
        strcmp(name, "public_send") == 0 ||
        strcmp(name, "freeze") == 0 || strcmp(name, "frozen?") == 0 ||
        strcmp(name, "object_id") == 0 || strcmp(name, "to_s") == 0 ||
        strcmp(name, "inspect") == 0 || strcmp(name, "==") == 0 ||
        strcmp(name, "!=") == 0 || strcmp(name, "equal?") == 0)
        return 1;

    if (recv.kind == VAL_OBJECT) {
        Value m;
        if (recv.obj->singleton_env && env_get(recv.obj->singleton_env, name, &m) &&
            method_visible_for_respond_to(m, include_private))
            return 1;
        if (ruby_class_find_instance_method(recv.obj->klass.klass, name, &m, NULL))
            return method_visible_for_respond_to(m, include_private);
        return 0;
    }

    if (recv.kind == VAL_CLASS) {
        if (strcmp(name, "new") == 0) return 1;
        size_t nlen = strlen(name);
        char *key = arena_alloc(ev->arena, nlen + 6);
        memcpy(key, "self.", 5);
        memcpy(key + 5, name, nlen + 1);
        RubyClass *k = recv.klass;
        while (k) {
            Value m;
            if (env_get(k->class_env, key, &m) && method_visible_for_respond_to(m, include_private))
                return 1;
            k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL;
        }
        return 0;
    }

    if (builtin_primitive_responds_to(recv, name)) return 1;

    Value klass = val_class_of(ev, recv);
    if (klass.kind == VAL_CLASS) {
        Value method;
        if (ruby_class_find_instance_method(klass.klass, name, &method, NULL))
            return method_visible_for_respond_to(method, include_private);
    }
    return 0;
}

Value call_method_value(Eval *ev, Env *env, Value recv, Value method, RubyClass *owner,
                        const char *name, Value *args, int argc, Value *blk, Node *site) {
    (void)env;
    NodeList *params = method.method.def_node->def.params;
    int required = count_required_params(params);
    int has_splat = has_splat_param(params);
    int total = count_total_params(params);
    if ((argc < required) || (!has_splat && argc > total))
        return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");

    Env *method_env = env_new(ev->arena, method.method.closure, 1);
    env_set(ev->arena, method_env, "self", recv);
    env_set(ev->arena, method_env, "__method__", val_symbol(name));
    Value owner_val = recv.kind == VAL_OBJECT ? recv.obj->klass : val_nil();
    if (owner) {
        owner_val.kind = VAL_CLASS;
        owner_val.klass = owner;
    }
    env_set(ev->arena, method_env, "__class__", owner_val);
    if (blk) method_env->block_arg = blk;
    bind_params(ev, method_env, params, args, argc);
    ev->call_depth++;
    if (ev->active_def_count < EVAL_MAX_DEPTH)
        ev->active_defs[ev->active_def_count++] = method_env;
    eval_push_frame(ev, site ? site->span.line : 0, site ? site->span.col : 0, name);
    Value result = eval_node(ev, method_env, method.method.def_node->def.body);
    eval_pop_frame(ev);
    if (ev->active_def_count > 0) ev->active_def_count--;
    ev->call_depth--;
    if (result.kind == VAL_RETURN && result.jump.target_env == method_env) return *result.jump.wrapped;
    return result;
}

static Value dispatch_respond_to_missing(Eval *ev, Env *env, Value recv, const char *name,
                                         int include_private, Node *site) {
    if (recv.kind == VAL_OBJECT) {
        RubyClass *owner = NULL;
        Value method;
        if (ruby_class_find_instance_method(recv.obj->klass.klass, "respond_to_missing?", &method, &owner)) {
            Value args[2];
            args[0] = val_symbol(name);
            args[1] = val_bool(include_private);
            Value result = call_method_value(ev, env, recv, method, owner, "respond_to_missing?", args, 2, NULL, site);
            if (val_is_signal(result)) return result;
            return val_bool(val_truthy(result));
        }
    } else if (recv.kind == VAL_CLASS) {
        size_t nlen = strlen("respond_to_missing?");
        char *key = arena_alloc(ev->arena, nlen + 6);
        memcpy(key, "self.", 5);
        memcpy(key + 5, "respond_to_missing?", nlen + 1);
        for (RubyClass *k = recv.klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
            Value method;
            if (env_get(k->class_env, key, &method) && method.kind == VAL_METHOD) {
                Value args[2];
                args[0] = val_symbol(name);
                args[1] = val_bool(include_private);
                Value result = call_method_value(ev, env, recv, method, k, "respond_to_missing?", args, 2, NULL, site);
                if (val_is_signal(result)) return result;
                return val_bool(val_truthy(result));
            }
        }
    } else {
        Value klass = val_class_of(ev, recv);
        if (klass.kind == VAL_CLASS) {
            RubyClass *owner = NULL;
            Value method;
            if (ruby_class_find_instance_method(klass.klass, "respond_to_missing?", &method, &owner)) {
                Value args[2];
                args[0] = val_symbol(name);
                args[1] = val_bool(include_private);
                Value result = call_method_value(ev, env, recv, method, owner, "respond_to_missing?", args, 2, NULL, site);
                if (val_is_signal(result)) return result;
                return val_bool(val_truthy(result));
            }
        }
    }
    return val_false();
}

static Value dispatch_method_missing(Eval *ev, Env *env, Value recv, const char *name,
                                     Value *args, int argc, Value *blk, Node *site) {
    if (recv.kind == VAL_OBJECT) {
        RubyClass *owner = NULL;
        Value method;
        if (ruby_class_find_instance_method(recv.obj->klass.klass, "method_missing", &method, &owner)) {
            Value mm_args[65];
            mm_args[0] = val_symbol(name);
            for (int i = 0; i < argc && i < 64; i++) mm_args[i + 1] = args[i];
            return call_method_value(ev, env, recv, method, owner, "method_missing", mm_args, argc + 1, blk, site);
        }
    } else if (recv.kind == VAL_CLASS) {
        char *key = arena_alloc(ev->arena, strlen("method_missing") + 6);
        memcpy(key, "self.method_missing", strlen("self.method_missing") + 1);
        for (RubyClass *k = recv.klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
            Value method;
            if (env_get(k->class_env, key, &method) && method.kind == VAL_METHOD) {
                Value mm_args[65];
                mm_args[0] = val_symbol(name);
                for (int i = 0; i < argc && i < 64; i++) mm_args[i + 1] = args[i];
                return call_method_value(ev, env, recv, method, k, "method_missing", mm_args, argc + 1, blk, site);
            }
        }
    } else {
        Value klass = val_class_of(ev, recv);
        if (klass.kind == VAL_CLASS) {
            RubyClass *owner = NULL;
            Value method;
            if (ruby_class_find_instance_method(klass.klass, "method_missing", &method, &owner)) {
                Value mm_args[65];
                mm_args[0] = val_symbol(name);
                for (int i = 0; i < argc && i < 64; i++) mm_args[i + 1] = args[i];
                return call_method_value(ev, env, recv, method, owner, "method_missing", mm_args, argc + 1, blk, site);
            }
        }
    }
    return eval_raise_class(ev, site, "NoMethodError", "undefined method '%s' for %s", name, val_kind_name(recv.kind));
}

static Value builtin_kernel(Eval *ev, Env *env, const char *name,
                            Value *args, int argc, Value *blk, Node *site) {
    if (strcmp(name, "puts") == 0) {
        if (argc == 0) {
            fprintf(ev->out, "\n");
            return val_nil();
        }
        for (int i = 0; i < argc; i++) {
            if (args[i].kind == VAL_ARRAY) {
                for (size_t j = 0; j < args[i].array->len; j++)
                    fprintf(ev->out, "%s\n", val_to_s(ev->arena, args[i].array->elems[j]));
            } else {
                fprintf(ev->out, "%s\n", val_to_s(ev->arena, args[i]));
            }
        }
        return val_nil();
    }
    if (strcmp(name, "print") == 0) {
        for (int i = 0; i < argc; i++)
            fprintf(ev->out, "%s", val_to_s(ev->arena, args[i]));
        return val_nil();
    }
    if (strcmp(name, "p") == 0) {
        for (int i = 0; i < argc; i++)
            fprintf(ev->out, "%s\n", val_inspect(ev->arena, args[i]));
        if (argc == 1) return args[0];
        Value arr = val_array_new();
        for (int i = 0; i < argc; i++) val_array_push(&arr, args[i]);
        return arr;
    }
    if (strcmp(name, "raise") == 0) {
        const char *class_name = "RuntimeError";
        const char *msg = "RuntimeError";
        if (argc == 0 && ev->rescue_context.kind == VAL_OBJECT &&
            value_is_a_named_class(ev, ev->rescue_context, "Exception")) {
            return eval_raise_value(ev, site, ev->rescue_context);
        } else if (argc >= 1 && args[0].kind == VAL_OBJECT && value_is_a_named_class(ev, args[0], "Exception")) {
            return eval_raise_value(ev, site, args[0]);
        } else if (argc >= 1 && args[0].kind == VAL_CLASS) {
            class_name = args[0].klass->name;
            msg = class_name;
            if (argc >= 2)
                msg = val_to_s(ev->arena, args[1]);
        } else if (argc >= 1) {
            msg = val_to_s(ev->arena, args[0]);
        }
        return eval_raise_class(ev, site, class_name, "%s", msg);
    }
    if (strcmp(name, "proc") == 0) {
        if (!blk) return eval_raise_class(ev, site, "ArgumentError", "proc requires a block");
        return val_proc(blk->block.block_node, blk->block.closure);
    }
    if (strcmp(name, "lambda") == 0) {
        if (!blk) return eval_raise_class(ev, site, "ArgumentError", "lambda requires a block");
        return val_lambda(blk->block.block_node, blk->block.closure);
    }
    if (strcmp(name, "rand") == 0) {
        if (argc == 0) return val_float((double)rand() / RAND_MAX);
        int64_t n = argc > 0 ? args[0].ival : 1;
        if (n <= 0) return val_int(0);
        return val_int((int64_t)(rand() % n));
    }
    if (strcmp(name, "exit") == 0) {
        int code = argc > 0 ? (int)args[0].ival : 0;
        exit(code);
    }
    if (strcmp(name, "attr_reader") == 0 || strcmp(name, "attr_writer") == 0 ||
        strcmp(name, "attr_accessor") == 0) {
        Value self;
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "TypeError", "%s must be called in a class body", name);
        for (int i = 0; i < argc; i++) {
            const char *attr = (args[i].kind == VAL_SYMBOL || args[i].kind == VAL_STRING)
                               ? args[i].sval : NULL;
            if (!attr) continue;
            if (strcmp(name, "attr_reader") == 0 || strcmp(name, "attr_accessor") == 0)
                define_attr_reader(ev, self, attr);
            if (strcmp(name, "attr_writer") == 0 || strcmp(name, "attr_accessor") == 0)
                define_attr_writer(ev, self, attr);
        }
        return val_nil();
    }
    if (strcmp(name, "include") == 0) {
        Value self;
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "TypeError", "include must be called in a class or module body");
        for (int i = 0; i < argc; i++) {
            if (args[i].kind != VAL_CLASS || !args[i].klass->is_module)
                return eval_raise_class(ev, site, "TypeError", "include requires a Module");
            RubyModuleInclusion *inc = arena_alloc(ev->arena, sizeof(RubyModuleInclusion));
            inc->mod = args[i].klass;
            inc->next = self.klass->included_modules;
            self.klass->included_modules = inc;
        }
        return val_nil();
    }
    if (strcmp(name, "prepend") == 0) {
        Value self;
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "TypeError", "prepend must be called in a class or module body");
        for (int i = 0; i < argc; i++) {
            if (args[i].kind != VAL_CLASS || !args[i].klass->is_module)
                return eval_raise_class(ev, site, "TypeError", "prepend requires a Module");
            RubyModuleInclusion *inc = arena_alloc(ev->arena, sizeof(RubyModuleInclusion));
            inc->mod = args[i].klass;
            inc->next = self.klass->prepended_modules;
            self.klass->prepended_modules = inc;
        }
        return val_nil();
    }
    if (strcmp(name, "extend") == 0) {
        Value self;
        if (!env_get(env, "self", &self))
            return eval_raise_class(ev, site, "TypeError", "extend requires an object");
        return builtin_extend(ev, self, args, argc, site);
    }
    if (strcmp(name, "require_relative") == 0) {
        if (argc < 1)
            return eval_raise_class(ev, site, "ArgumentError", "require_relative requires a path");
        const char *path = args[0].kind == VAL_STRING ? args[0].sval : NULL;
        if (!path)
            return eval_raise_class(ev, site, "TypeError", "require_relative path must be a String");
        return eval_require_relative(ev, env, path, site);
    }
    if (strcmp(name, "require") == 0) {
        if (argc < 1)
            return eval_raise_class(ev, site, "ArgumentError", "require requires a path");
        const char *path = args[0].kind == VAL_STRING ? args[0].sval : NULL;
        if (!path)
            return eval_raise_class(ev, site, "TypeError", "require path must be a String");
        return eval_require(ev, env, path, site);
    }
    if (strcmp(name, "public") == 0 || strcmp(name, "private") == 0 || strcmp(name, "protected") == 0) {
        Value self;
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "TypeError", "%s must be called in a class or module body", name);
        MethodVisibility visibility = METHOD_PUBLIC;
        if (strcmp(name, "private") == 0) visibility = METHOD_PRIVATE;
        if (strcmp(name, "protected") == 0) visibility = METHOD_PROTECTED;
        if (argc == 0) {
            set_current_method_visibility(ev->arena, env, visibility);
            return val_nil();
        }
        for (int i = 0; i < argc; i++) {
            const char *mname = (args[i].kind == VAL_SYMBOL || args[i].kind == VAL_STRING) ? args[i].sval : NULL;
            if (!mname) continue;
            update_method_visibility(env, mname, visibility, 0);
            size_t nlen = strlen(mname);
            char *key = arena_alloc(ev->arena, nlen + 6);
            memcpy(key, "self.", 5);
            memcpy(key + 5, mname, nlen + 1);
            update_method_visibility(env, key, visibility, 1);
        }
        return val_nil();
    }
    if (strcmp(name, "private_class_method") == 0 ||
        strcmp(name, "public_class_method") == 0 ||
        strcmp(name, "protected_class_method") == 0) {
        Value self;
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "TypeError", "%s must be called in a class or module body", name);
        if (argc < 1)
            return eval_raise_class(ev, site, "ArgumentError", "%s requires at least one method name", name);

        MethodVisibility visibility = METHOD_PUBLIC;
        if (strcmp(name, "private_class_method") == 0) visibility = METHOD_PRIVATE;
        if (strcmp(name, "protected_class_method") == 0) visibility = METHOD_PROTECTED;

        for (int i = 0; i < argc; i++) {
            const char *mname = (args[i].kind == VAL_SYMBOL || args[i].kind == VAL_STRING) ? args[i].sval : NULL;
            if (!mname) continue;
            size_t nlen = strlen(mname);
            char *key = arena_alloc(ev->arena, nlen + 6);
            memcpy(key, "self.", 5);
            memcpy(key + 5, mname, nlen + 1);
            update_method_visibility(env, key, visibility, 1);
        }
        return val_nil();
    }
    (void)blk;
    return val_nil();
}

static Value make_symbol_proc(Eval *ev, const char *method_name);

Value dispatch_method(Eval *ev, Env *env __attribute__((unused)), Value recv,
                      const char *name, Value *args, int argc,
                      Value *blk, Node *site, int public_only, int explicit_receiver);

Value dispatch_method(Eval *ev, Env *env __attribute__((unused)), Value recv,
                      const char *name, Value *args, int argc,
                      Value *blk, Node *site, int public_only, int explicit_receiver) {
    Value out;

    if (strcmp(name, "nil?") == 0) {
        if (argc != 0) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        return val_bool(recv.kind == VAL_NIL);
    }
    if (strcmp(name, "is_a?") == 0 || strcmp(name, "kind_of?") == 0) {
        if (argc != 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        return val_bool(val_is_a(recv, args[0]));
    }
    if (strcmp(name, "instance_of?") == 0) {
        if (argc != 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        if (args[0].kind != VAL_CLASS) return val_false();
        if (recv.kind == VAL_OBJECT)
            return val_bool(recv.obj->klass.klass == args[0].klass);
        if (recv.kind == VAL_CLASS)
            return val_bool(strcmp(recv.klass->is_module ? "Module" : "Class", args[0].klass->name) == 0);
        return val_bool(strcmp(prim_class_name(recv), args[0].klass->name) == 0);
    }
    if (strcmp(name, "class") == 0) {
        if (argc != 0) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        return val_class_of(ev, recv);
    }
    if (strcmp(name, "respond_to?") == 0) {
        if (argc < 1 || argc > 2) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        int include_private = argc >= 2 && val_truthy(args[1]);
        const char *mname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING)
                            ? args[0].sval : NULL;
        if (!mname) return val_false();
        if (val_responds_to(ev, recv, mname, include_private)) return val_true();
        return dispatch_respond_to_missing(ev, env, recv, mname, include_private, site);
    }
    if (strcmp(name, "extend") == 0)
        return builtin_extend(ev, recv, args, argc, site);
    if (strcmp(name, "equal?") == 0) {
        if (argc < 1) return val_false();
        if (recv.kind == VAL_OBJECT && args[0].kind == VAL_OBJECT)
            return val_bool(recv.obj == args[0].obj);
        return val_bool(val_equal(recv, args[0]));
    }
    if (strcmp(name, "freeze") == 0) return recv;
    if (strcmp(name, "frozen?") == 0) return val_false();
    if (strcmp(name, "itself") == 0) return recv;
    if (strcmp(name, "object_id") == 0) {
        if (recv.kind == VAL_OBJECT) return val_int((int64_t)(uintptr_t)recv.obj);
        if (recv.kind == VAL_INT) return val_int(recv.ival * 2 + 1);
        return val_int((int64_t)(uintptr_t)recv.sval);
    }
    if (strcmp(name, "==") == 0) {
        if (argc < 1) return val_false();
        return val_bool(val_equal(recv, args[0]));
    }
    if (strcmp(name, "!=") == 0) {
        if (argc < 1) return val_true();
        return val_bool(!val_equal(recv, args[0]));
    }
    if (strcmp(name, "===") == 0 && recv.kind != VAL_CLASS && recv.kind != VAL_RANGE) {
        if (argc < 1) return val_false();
        return val_bool(val_equal(recv, args[0]));
    }
    if (recv.kind == VAL_SYMBOL) {
        if (strcmp(name, "to_s") == 0 || strcmp(name, "id2name") == 0)
            return val_string(ev->arena, recv.sval ? recv.sval : "");
        if (strcmp(name, "inspect") == 0) {
            const char *s = recv.sval ? recv.sval : "";
            size_t n = strlen(s);
            char *buf = arena_alloc(ev->arena, n + 2);
            buf[0] = ':';
            memcpy(buf + 1, s, n + 1);
            return val_string(ev->arena, buf);
        }
        if (strcmp(name, "to_sym") == 0) return recv;
        if (strcmp(name, "to_proc") == 0) return make_symbol_proc(ev, recv.sval ? recv.sval : "");
        if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0)
            return val_int((int64_t)(recv.sval ? strlen(recv.sval) : 0));
    }
    if (strcmp(name, "send") == 0 || strcmp(name, "__send__") == 0)
        return dispatch_dynamic_send(ev, env, recv, name, args, argc, blk, site, 0);
    if (strcmp(name, "public_send") == 0)
        return dispatch_dynamic_send(ev, env, recv, name, args, argc, blk, site, 1);
    if (recv.kind == VAL_BLOCK) {
        if (strcmp(name, "call") == 0 || strcmp(name, "[]") == 0)
            return call_block(ev, env, recv, args, argc, site);
        if (strcmp(name, "lambda?") == 0)
            return val_bool(recv.block.is_lambda);
        if (strcmp(name, "arity") == 0)
            return val_int(proc_arity(recv.block.block_node->block.params, recv.block.is_lambda));
        if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0)
            return val_string(ev->arena, recv.block.is_lambda ? "#<Proc:lambda>" : "#<Proc>");
    }

    if (dispatch_integer(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (dispatch_float(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (dispatch_string(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (recv.kind == VAL_ARRAY && dispatch_array(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (recv.kind == VAL_HASH && dispatch_hash(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (recv.kind == VAL_RANGE && dispatch_range(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (recv.kind == VAL_NIL && dispatch_nil(ev, recv, name, site, &out)) return out;
    if (dispatch_bool(ev, recv, name, site, &out)) return out;

    if (recv.kind != VAL_OBJECT && recv.kind != VAL_CLASS) {
        Value klass = val_class_of(ev, recv);
        if (klass.kind == VAL_CLASS) {
            RubyClass *owner = NULL;
            Value method;
            if (ruby_class_find_instance_method(klass.klass, name, &method, &owner)) {
                if (method_visibility_allows_call(ev, env, recv, owner, method.method.visibility,
                                                  public_only, explicit_receiver)) {
                    Value result = call_method_value(ev, env, recv, method, owner, name, args, argc, blk, site);
                    if (val_is_signal(result)) return result;
                    return result;
                }
            }
        }
    }

    if (dispatch_class(ev, env, recv, name, args, argc, blk, site, &out, public_only, explicit_receiver)) return out;
    if (dispatch_object(ev, env, recv, name, args, argc, blk, site, &out, public_only, explicit_receiver)) return out;

    if (strcmp(name, "method_missing") != 0)
        return dispatch_method_missing(ev, env, recv, name, args, argc, blk, site);
    return eval_raise_class(ev, site, "NoMethodError", "undefined method '%s' for %s", name, val_kind_name(recv.kind));
}

Value eval_binop(Eval *ev, Env *env, Node *node) {
    const char *op = node->binop.op;

    if (strcmp(op, "&&") == 0) {
        Value left = eval_node(ev, env, node->binop.left);
        CHECK(left);
        if (!val_truthy(left)) return left;
        return eval_node(ev, env, node->binop.right);
    }
    if (strcmp(op, "||") == 0) {
        Value left = eval_node(ev, env, node->binop.left);
        CHECK(left);
        if (val_truthy(left)) return left;
        return eval_node(ev, env, node->binop.right);
    }

    Value left = eval_node(ev, env, node->binop.left);
    CHECK(left);
    Value right = eval_node(ev, env, node->binop.right);
    CHECK(right);

        if (strcmp(op, "+") == 0 && left.kind == VAL_STRING) {
        if (right.kind != VAL_STRING)
            return eval_raise_class(ev, node, "TypeError", "String can only be concatenated with String");
        size_t la = strlen(left.sval), lb = strlen(right.sval);
        char *buf = arena_alloc(ev->arena, la + lb + 1);
        memcpy(buf, left.sval, la);
        memcpy(buf + la, right.sval, lb + 1);
        return val_string(ev->arena, buf);
    }

    if (strcmp(op, "+") == 0 && left.kind == VAL_ARRAY && right.kind == VAL_ARRAY) {
        Value result = val_array_new();
        for (size_t i = 0; i < left.array->len; i++) val_array_push(&result, left.array->elems[i]);
        for (size_t i = 0; i < right.array->len; i++) val_array_push(&result, right.array->elems[i]);
        return result;
    }
    if (strcmp(op, "<<") == 0 && left.kind == VAL_ARRAY) {
        val_array_push(&left, right);
        return left;
    }

    if (strcmp(op, "==") == 0) return val_bool(val_equal(left, right));
    if (strcmp(op, "!=") == 0) return val_bool(!val_equal(left, right));

    double lf = 0;
    double rf = 0;
    int both_int = (left.kind == VAL_INT && right.kind == VAL_INT);
    if (left.kind == VAL_INT) lf = (double)left.ival;
    else if (left.kind == VAL_FLOAT) lf = left.fval;
    if (right.kind == VAL_INT) rf = (double)right.ival;
    else if (right.kind == VAL_FLOAT) rf = right.fval;

    if (left.kind == VAL_INT || left.kind == VAL_FLOAT) {
        if (strcmp(op, "+") == 0) return both_int ? val_int(left.ival + right.ival) : val_float(lf + rf);
        if (strcmp(op, "-") == 0) return both_int ? val_int(left.ival - right.ival) : val_float(lf - rf);
        if (strcmp(op, "*") == 0) {
            if (left.kind == VAL_INT && right.kind == VAL_STRING)
                return dispatch_method(ev, env, right, "*", &left, 1, NULL, node, 0, 1);
            return both_int ? val_int(left.ival * right.ival) : val_float(lf * rf);
        }
        if (strcmp(op, "/") == 0) {
            if (both_int) {
                if (right.ival == 0) return eval_raise_class(ev, node, "ZeroDivisionError", "divided by 0");
                return val_int(left.ival / right.ival);
            }
            return val_float(lf / rf);  /* IEEE 754: x/0.0 = Inf or NaN, not an error */
        }
        if (strcmp(op, "%") == 0) {
            if (both_int) {
                if (right.ival == 0) return eval_raise_class(ev, node, "ZeroDivisionError", "divided by 0");
                return val_int(left.ival % right.ival);
            }
            return val_float(fmod(lf, rf));
        }
        if (strcmp(op, "**") == 0) {
            return both_int && right.ival >= 0
                ? val_int((int64_t)pow((double)left.ival, (double)right.ival))
                : val_float(pow(lf, rf));
        }
        if (strcmp(op, "<") == 0) return val_bool(lf < rf);
        if (strcmp(op, "<=") == 0) return val_bool(lf <= rf);
        if (strcmp(op, ">") == 0) return val_bool(lf > rf);
        if (strcmp(op, ">=") == 0) return val_bool(lf >= rf);
        if (strcmp(op, "<=>") == 0) return val_int(lf < rf ? -1 : lf > rf ? 1 : 0);
        if (both_int) {
            if (strcmp(op, "&") == 0) return val_int(left.ival & right.ival);
            if (strcmp(op, "|") == 0) return val_int(left.ival | right.ival);
            if (strcmp(op, "^") == 0) return val_int(left.ival ^ right.ival);
            if (strcmp(op, "<<") == 0) return val_int(left.ival << right.ival);
            if (strcmp(op, ">>") == 0) return val_int(left.ival >> right.ival);
        }
    }

    if (left.kind == VAL_STRING && right.kind == VAL_STRING) {
        if (strcmp(op, "<=>") == 0) {
            int r = strcmp(left.sval, right.sval);
            return val_int(r < 0 ? -1 : r > 0 ? 1 : 0);
        }
        if (strcmp(op, "<") == 0) return val_bool(strcmp(left.sval, right.sval) < 0);
        if (strcmp(op, "<=") == 0) return val_bool(strcmp(left.sval, right.sval) <= 0);
        if (strcmp(op, ">") == 0) return val_bool(strcmp(left.sval, right.sval) > 0);
        if (strcmp(op, ">=") == 0) return val_bool(strcmp(left.sval, right.sval) >= 0);
        if (strcmp(op, "*") == 0) return dispatch_method(ev, env, left, "*", &right, 1, NULL, node, 0, 1);
    }

    Value op_result = dispatch_method(ev, env, left, op, &right, 1, NULL, node, 0, 1);
    if (!ev->errored && !val_is_signal(op_result))
        return op_result;
    if (ev->errored) {
        ev->errored = 0;
        return eval_raise_class(ev, node, "NoMethodError", "undefined operator '%s' for %s", op, val_kind_name(left.kind));
    }
    return op_result;
}

static Value make_symbol_proc(Eval *ev, const char *method_name) {
    Arena *a = ev->arena;
    Span s = {0, 0, 0};
    Node *recv_p = node_new(a, NODE_PARAM, s);
    recv_p->param.name = "__sym_recv__";
    Node *rest_p = node_new(a, NODE_PARAM, s);
    rest_p->param.splat = 1;
    rest_p->param.name = "__sym_rest__";
    NodeList *params = nodelist_append(a, NULL, recv_p);
    params = nodelist_append(a, params, rest_p);
    Node *rest_var = node_new(a, NODE_LVAR, s);
    rest_var->sval = "__sym_rest__";
    Node *splat_arg = node_new(a, NODE_UNOP, s);
    splat_arg->unop.op = "*";
    splat_arg->unop.operand = rest_var;
    Node *recv_var = node_new(a, NODE_LVAR, s);
    recv_var->sval = "__sym_recv__";
    Node *call_node = node_new(a, NODE_CALL, s);
    call_node->call.recv = recv_var;
    call_node->call.method = method_name;
    call_node->call.args = nodelist_append(a, NULL, splat_arg);
    call_node->call.block = NULL;
    Node *body = node_new(a, NODE_BODY, s);
    body->body.stmts = nodelist_append(a, NULL, call_node);
    Node *block_node = node_new(a, NODE_BLOCK, s);
    block_node->block.params = params;
    block_node->block.body = body;
    return val_proc(block_node, ev->top_env);
}

Value eval_call(Eval *ev, Env *env, Node *node) {
    if (ev->call_depth > EVAL_MAX_DEPTH)
        return eval_raise_class(ev, node, "SystemStackError", "stack level too deep");

    Value args[64];
    int argc = 0;
    Node *block_pass_node = NULL;
    for (NodeList *l = node->call.args; l && argc < 64; l = l->next) {
        if (!l->node) continue;
        if (l->node->kind == NODE_BLOCK_PASS) { block_pass_node = l->node; continue; }
        if (l->node->kind == NODE_UNOP && strcmp(l->node->unop.op, "*") == 0) {
            Value splat = eval_node(ev, env, l->node->unop.operand);
            CHECK(splat);
            if (splat.kind == VAL_ARRAY) {
                for (size_t i = 0; i < splat.array->len && argc < 64; i++)
                    args[argc++] = splat.array->elems[i];
            } else {
                args[argc++] = splat;
            }
            continue;
        }
        Value a = eval_node(ev, env, l->node);
        CHECK(a);
        args[argc++] = a;
    }

    Value blk_val;
    Value *blk = NULL;
    if (node->call.block) {
        blk_val = val_block(node->call.block, env);
        blk = &blk_val;
    } else if (block_pass_node) {
        Value bp = eval_node(ev, env, block_pass_node->block_pass.expr);
        if (val_is_signal(bp)) return bp;
        if (bp.kind == VAL_SYMBOL) {
            blk_val = make_symbol_proc(ev, bp.sval);
            blk = &blk_val;
        } else if (bp.kind == VAL_BLOCK) {
            blk_val = bp;
            blk = &blk_val;
        }
        /* nil/false block pass → no block */
    }

    if (!node->call.recv) {
        const char *name = node->call.method;

        if (strcmp(name, "yield") == 0) {
            Value *block_arg = NULL;
            for (Env *sc = env; sc; sc = sc->parent) {
                if (sc->block_arg) { block_arg = sc->block_arg; break; }
                if (sc->is_def) break;
            }
            if (!block_arg) return eval_raise_class(ev, node, "LocalJumpError", "no block given (yield)");
            return call_block(ev, env, *block_arg, args, argc, node);
        }
        if (strcmp(name, "block_given?") == 0) {
            for (Env *sc = env; sc; sc = sc->parent) {
                if (sc->block_arg) return val_true();
                if (sc->is_def) break;
            }
            return val_false();
        }

        static const char *kernel_names[] = {
            "puts", "print", "p", "raise", "proc", "lambda", "rand", "exit", "include", "prepend", "extend",
            "require", "require_relative", "public", "private", "protected",
            "private_class_method", "public_class_method", "protected_class_method",
            "attr_reader", "attr_writer", "attr_accessor", NULL
        };
        for (int i = 0; kernel_names[i]; i++) {
            if (strcmp(name, kernel_names[i]) == 0)
                return builtin_kernel(ev, env, name, args, argc, blk, node);
        }

        Value fn;
        if (env_get(env, name, &fn) && fn.kind == VAL_METHOD)
            goto call_method;

        Value self;
        if (env_get(env, "self", &self) && self.kind == VAL_OBJECT) {
            if (self.obj->singleton_env && env_get(self.obj->singleton_env, name, &fn) && fn.kind == VAL_METHOD) {
                Env *method_env = env_new(ev->arena, fn.method.closure, 1);
                env_set(ev->arena, method_env, "self", self);
                env_set(ev->arena, method_env, "__method__", val_symbol(name));
                Value klass_val = self.obj->klass;
                env_set(ev->arena, method_env, "__class__", klass_val);
                if (blk) method_env->block_arg = blk;
                bind_params(ev, method_env, fn.method.def_node->def.params, args, argc);
                ev->call_depth++;
                if (ev->active_def_count < EVAL_MAX_DEPTH)
                    ev->active_defs[ev->active_def_count++] = method_env;
                eval_push_frame(ev, node->span.line, node->span.col, name);
                Value result = eval_node(ev, method_env, fn.method.def_node->def.body);
                eval_pop_frame(ev);
                if (ev->active_def_count > 0) ev->active_def_count--;
                ev->call_depth--;
                if (result.kind == VAL_RETURN && result.jump.target_env == method_env) result = *result.jump.wrapped;
                else if (val_is_signal(result)) return result;
                return result;
            }
            RubyClass *owner = NULL;
            if (ruby_class_find_instance_method(self.obj->klass.klass, name, &fn, &owner)) {
                Env *method_env = env_new(ev->arena, fn.method.closure, 1);
                env_set(ev->arena, method_env, "self", self);
                env_set(ev->arena, method_env, "__method__", val_symbol(name));
                Value klass_val; klass_val.kind = VAL_CLASS; klass_val.klass = owner;
                env_set(ev->arena, method_env, "__class__", klass_val);
                if (blk) method_env->block_arg = blk;
                bind_params(ev, method_env, fn.method.def_node->def.params, args, argc);
                ev->call_depth++;
                if (ev->active_def_count < EVAL_MAX_DEPTH)
                    ev->active_defs[ev->active_def_count++] = method_env;
                eval_push_frame(ev, node->span.line, node->span.col, name);
                Value result = eval_node(ev, method_env, fn.method.def_node->def.body);
                eval_pop_frame(ev);
                if (ev->active_def_count > 0) ev->active_def_count--;
                ev->call_depth--;
                if (result.kind == VAL_RETURN && result.jump.target_env == method_env) result = *result.jump.wrapped;
                else if (val_is_signal(result)) return result;
                return result;
            }
        }

        if (env_get(env, "self", &self) && self.kind == VAL_CLASS) {
            size_t nlen = strlen(name);
            char *key = arena_alloc(ev->arena, nlen + 6);
            memcpy(key, "self.", 5);
            memcpy(key + 5, name, nlen + 1);
            RubyClass *cklass = self.klass;
            while (cklass) {
                if (env_get(cklass->class_env, key, &fn) && fn.kind == VAL_METHOD) {
                    Env *method_env = env_new(ev->arena, fn.method.closure, 1);
                    env_set(ev->arena, method_env, "self", self);
                    env_set(ev->arena, method_env, "__method__", val_symbol(name));
                    Value kv; kv.kind = VAL_CLASS; kv.klass = cklass;
                    env_set(ev->arena, method_env, "__class__", kv);
                    if (blk) method_env->block_arg = blk;
                    bind_params(ev, method_env, fn.method.def_node->def.params, args, argc);
                    ev->call_depth++;
                    if (ev->active_def_count < EVAL_MAX_DEPTH)
                        ev->active_defs[ev->active_def_count++] = method_env;
                    eval_push_frame(ev, node->span.line, node->span.col, name);
                    Value result = eval_node(ev, method_env, fn.method.def_node->def.body);
                    eval_pop_frame(ev);
                    if (ev->active_def_count > 0) ev->active_def_count--;
                    ev->call_depth--;
                    if (result.kind == VAL_RETURN && result.jump.target_env == method_env) result = *result.jump.wrapped;
                    else if (val_is_signal(result)) return result;
                    return result;
                }
                cklass = cklass->superclass.kind == VAL_CLASS ? cklass->superclass.klass : NULL;
            }
        }

        if (env_get(env, "self", &self) && self.kind != VAL_OBJECT && self.kind != VAL_CLASS) {
            Value result = dispatch_method(ev, env, self, name, args, argc, blk, node, 0, 0);
            if (!ev->errored)
                return result;
            ev->errored = 0;
        }

        if (!env_get(env, name, &fn))
            return eval_raise_class(ev, node, "NoMethodError", "undefined method '%s'", name);
        if (fn.kind != VAL_METHOD)
            return eval_raise_class(ev, node, "TypeError", "'%s' is not a method", name);

call_method:
        if (!env_get(env, name, &fn))
            return eval_raise_class(ev, node, "NoMethodError", "undefined method '%s'", name);
        Node *def = fn.method.def_node;
        Env *closure = fn.method.closure;
        Env *frame = env_new(ev->arena, closure, 1);
        if (blk) frame->block_arg = blk;

        bind_params(ev, frame, def->def.params, args, argc);

        ev->call_depth++;
        if (ev->active_def_count < EVAL_MAX_DEPTH)
            ev->active_defs[ev->active_def_count++] = frame;
        eval_push_frame(ev, node->span.line, node->span.col, name);
        Value result = eval_node(ev, frame, def->def.body);
        eval_pop_frame(ev);
        if (ev->active_def_count > 0) ev->active_def_count--;
        ev->call_depth--;
        if (result.kind == VAL_RETURN && result.jump.target_env == frame) return *result.jump.wrapped;
        if (val_is_signal(result)) return result;
        return result;
    }

    Value recv = eval_node(ev, env, node->call.recv);
    CHECK(recv);

    if (recv.kind == VAL_ARRAY) {
        if (strcmp(node->call.method, "[]") == 0) {
            if (argc < 1) return eval_raise_class(ev, node, "ArgumentError", "wrong number of args for []");
            int64_t idx = args[0].ival;
            if (idx < 0) idx = (int64_t)recv.array->len + idx;
            if (idx < 0 || (size_t)idx >= recv.array->len) return val_nil();
            return recv.array->elems[idx];
        }
        if (strcmp(node->call.method, "[]=") == 0) {
            if (argc < 2) return eval_raise_class(ev, node, "ArgumentError", "wrong number of args for []=");
            int64_t idx = args[0].ival;
            if (idx < 0) idx = (int64_t)recv.array->len + idx;
            if (idx >= 0 && (size_t)idx < recv.array->len)
                recv.array->elems[idx] = args[1];
            return args[1];
        }
    }

    return dispatch_method(ev, env, recv, node->call.method, args, argc, blk, node, 0, 1);
}
