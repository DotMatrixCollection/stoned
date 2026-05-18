#define _POSIX_C_SOURCE 200809L
#include "eval_internal.h"
#include "parser.h"
#include "sema.h"

#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

extern char **environ;

#define CHECK(v) do { if (ev->errored || val_is_signal(v)) return (v); } while(0)

static Value val_class_of(Eval *ev, Value v);

/* Store last child exit code in a global; the Ruby prelude wraps it as $?. */
static void set_child_status(Eval *ev, Env *env __attribute__((unused)),
                             int exit_code, Node *site __attribute__((unused))) {
    global_set(ev->arena, &ev->globals, "__child_exit__", val_int((int64_t)exit_code));
}

static void apply_child_env_overrides(Eval *ev, Value overrides) {
    if (overrides.kind != VAL_HASH)
        return;
    for (size_t i = 0; i < overrides.hash->len; i++) {
        const char *key = val_to_s(ev->arena, overrides.hash->keys[i]);
        if (!key || key[0] == '\0')
            continue;
        if (overrides.hash->vals[i].kind == VAL_NIL) {
            unsetenv(key);
            continue;
        }
        const char *value = val_to_s(ev->arena, overrides.hash->vals[i]);
        setenv(key, value ? value : "", 1);
    }
}

static Value run_child_process(Eval *ev, Env *env, Value overrides, Value *args, int argc,
                               Node *site, int return_pid) {
    int arg_index = 0;
    if (argc > 0 && args[0].kind == VAL_HASH) {
        overrides = args[0];
        arg_index = 1;
    }
    if (argc - arg_index <= 0) {
        if (return_pid)
            return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments (given 0, expected 1+)");
        return val_false();
    }

    int shell_form = (argc - arg_index == 1);
    char **argv = NULL;
    if (!shell_form) {
        argv = arena_alloc(ev->arena, (size_t)(argc - arg_index + 1) * sizeof(char *));
        for (int i = arg_index; i < argc; i++)
            argv[i - arg_index] = (char *)val_to_s(ev->arena, args[i]);
        argv[argc - arg_index] = NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (return_pid)
            return eval_raise_class(ev, site, "SystemCallError", "%s", strerror(errno));
        set_child_status(ev, env, -1, site);
        return val_nil();
    }
    if (pid == 0) {
        apply_child_env_overrides(ev, overrides);
        if (shell_form) {
            const char *cmd = val_to_s(ev->arena, args[arg_index]);
            execlp("sh", "sh", "-c", cmd ? cmd : "", (char *)NULL);
        } else {
            execvp(argv[0], argv);
        }
        _exit(127);
    }

    if (return_pid)
        return val_int((int64_t)pid);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        set_child_status(ev, env, -1, site);
        return val_nil();
    }
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    set_child_status(ev, env, exit_code, site);
    return exit_code == 0 ? val_true() : val_false();
}

static NativeBinding *native_binding(Value v) {
    if (v.kind != VAL_OBJECT || v.obj->klass.kind != VAL_CLASS ||
        strcmp(v.obj->klass.klass->name, "Binding") != 0)
        return NULL;
    return (NativeBinding *)v.obj->native;
}

static int is_local_identifier_name(const char *name) {
    if (!name || !name[0] || strcmp(name, "self") == 0)
        return 0;
    if (!((name[0] >= 'a' && name[0] <= 'z') || name[0] == '_'))
        return 0;
    for (const char *p = name + 1; *p; p++) {
        if (!( (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '_'))
            return 0;
    }
    return 1;
}

static Value eval_string_in_context(Eval *ev, Env *caller_env, const char *src, Env *target_env,
                                    const char *file, int64_t line, Node *site) {
    if (!src)
        src = "";

    size_t src_len = strlen(src);
    size_t local_prelude_len = 0;
    for (EnvEntry *entry = target_env ? target_env->vars : NULL; entry; entry = entry->next) {
        if (!is_local_identifier_name(entry->name))
            continue;
        local_prelude_len += strlen(entry->name) * 2 + 2;
    }
    if (local_prelude_len > 0)
        local_prelude_len += 10; /* begin;...;end; */
    size_t prefix_len = line > 1 ? (size_t)(line - 1) : 0;
    char *shifted = arena_alloc(ev->arena, prefix_len + local_prelude_len + src_len + 1);
    for (size_t i = 0; i < prefix_len; i++)
        shifted[i] = '\n';
    size_t pos = prefix_len;
    if (local_prelude_len > 0) {
        memcpy(shifted + pos, "begin;", 6);
        pos += 6;
        for (EnvEntry *entry = target_env ? target_env->vars : NULL; entry; entry = entry->next) {
            size_t nlen;
            if (!is_local_identifier_name(entry->name))
                continue;
            nlen = strlen(entry->name);
            memcpy(shifted + pos, entry->name, nlen);
            pos += nlen;
            shifted[pos++] = '=';
            memcpy(shifted + pos, entry->name, nlen);
            pos += nlen;
            shifted[pos++] = ';';
        }
        memcpy(shifted + pos, "end;", 4);
        pos += 4;
    }
    memcpy(shifted + pos, src, src_len + 1);

    Parser parser;
    parser_init(&parser, shifted, prefix_len + pos - prefix_len + src_len, ev->arena);
    Node *tree = parse_program(&parser);
    if (parser.error_count > 0)
        return eval_raise_class(ev, site, "SyntaxError", "%s", parser.errors[0].message);

    Sema sema;
    sema_init(&sema, ev->arena);
    sema_run(&sema, tree);
    if (sema.error_count > 0)
        return eval_raise_class(ev, site, "SyntaxError", "%s", sema.errors[0].message);

    const char *previous_file = ev->current_file;
    ev->current_file = file ? file : previous_file;
    Value result = eval_node(ev, target_env ? target_env : caller_env, tree);
    ev->current_file = previous_file;
    return result;
}

static Env *value_singleton_env(Value recv) {
    switch (recv.kind) {
        case VAL_OBJECT: return recv.obj->singleton_env;
        case VAL_ARRAY:  return recv.array->singleton_env;
        case VAL_HASH:   return recv.hash->singleton_env;
        default:         return NULL;
    }
}

static Env **value_singleton_env_slot(Value recv) {
    switch (recv.kind) {
        case VAL_OBJECT: return &recv.obj->singleton_env;
        case VAL_ARRAY:  return &recv.array->singleton_env;
        case VAL_HASH:   return &recv.hash->singleton_env;
        default:         return NULL;
    }
}

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
    if (self.kind != VAL_CLASS && !value_singleton_env_slot(self))
        return eval_raise_class(ev, site, "TypeError", "extend requires an object");

    for (int i = 0; i < argc; i++) {
        if (args[i].kind != VAL_CLASS || !args[i].klass->is_module)
            return eval_raise_class(ev, site, "TypeError", "extend requires a Module");
    }

    if (self.kind == VAL_CLASS) {
        for (int i = 0; i < argc; i++)
            copy_module_methods(ev, self.klass->class_env, args[i].klass, 1);
    } else {
        Env **slot = value_singleton_env_slot(self);
        if (!*slot) *slot = env_new(ev->arena, NULL, 1);
        for (int i = 0; i < argc; i++)
            copy_module_methods(ev, *slot, args[i].klass, 0);
    }
    return self;
}

static void define_attr_reader_in_env(Eval *ev, Env *target_env,
                                      const char *method_name, const char *attr) {
    if (!target_env) return;
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
    def->def.name = method_name;
    def->def.body = body;

    env_define(a, target_env, method_name, val_method(def, ev->top_env, METHOD_PUBLIC, ev->current_file));
}

static void define_attr_reader(Eval *ev, Value klass, const char *attr) {
    if (klass.kind != VAL_CLASS) return;
    define_attr_reader_in_env(ev, klass.klass->class_env, attr, attr);
}

static void define_attr_writer_in_env(Eval *ev, Env *target_env,
                                      const char *method_name, const char *attr) {
    if (!target_env) return;
    Arena *a = ev->arena;

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

    env_define(a, target_env, method_name, val_method(def, ev->top_env, METHOD_PUBLIC, ev->current_file));
}

static void define_attr_writer(Eval *ev, Value klass, const char *attr) {
    if (klass.kind != VAL_CLASS) return;
    Arena *a = ev->arena;
    size_t alen = strlen(attr);
    char *method_name = arena_alloc(a, alen + 2);
    memcpy(method_name, attr, alen);
    method_name[alen] = '=';
    method_name[alen + 1] = '\0';
    define_attr_writer_in_env(ev, klass.klass->class_env, method_name, attr);
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
    if (public_only && !val_responds_to(ev, recv, mname, 0) && val_responds_to(ev, recv, mname, 1)) {
        Value method = val_nil();
        RubyClass *owner = NULL;
        int found = 0;
        Env *singleton_env = value_singleton_env(recv);
        if (singleton_env)
            found = env_get(singleton_env, mname, &method) && method.kind == VAL_METHOD;
        if (!found && recv.kind == VAL_OBJECT) {
            if (recv.obj->klass.kind == VAL_CLASS)
                found = ruby_class_find_instance_method(recv.obj->klass.klass, mname, &method, &owner);
        } else if (recv.kind == VAL_CLASS) {
            size_t nlen = strlen(mname);
            char *key = arena_alloc(ev->arena, nlen + 6);
            memcpy(key, "self.", 5);
            memcpy(key + 5, mname, nlen + 1);
            for (RubyClass *k = recv.klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
                if (env_get(k->class_env, key, &method) && method.kind == VAL_METHOD) { found = 1; break; }
            }
        } else {
            Value klass = val_class_of(ev, recv);
            if (klass.kind == VAL_CLASS)
                found = ruby_class_find_instance_method(klass.klass, mname, &method, &owner);
        }
        if (found && method.kind == VAL_METHOD && method.method.visibility == METHOD_PROTECTED)
            return eval_raise_class(ev, site, "NoMethodError",
                                    "protected method '%s' called for an instance of %s",
                                    mname, value_class_name(ev, recv));
        return eval_raise_class(ev, site, "NoMethodError", "undefined method '%s' for %s", mname, val_kind_name(recv.kind));
    }
    int explicit_receiver = public_only ? 0 : -1;
    return dispatch_method(ev, env, recv, mname, args + 1, argc - 1, blk, site, public_only, explicit_receiver);
}

static int class_includes_module(RubyClass *k, const char *mod_name) {
    for (RubyModuleInclusion *m = k->included_modules; m; m = m->next)
        if (strcmp(m->mod->name, mod_name) == 0) return 1;
    for (RubyModuleInclusion *m = k->prepended_modules; m; m = m->next)
        if (strcmp(m->mod->name, mod_name) == 0) return 1;
    return 0;
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
            if (class_includes_module(k, kname)) return 1;
            k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL;
        }
        return 0;
    }
    if (strcmp(kname, "Numeric") == 0)
        return v.kind == VAL_INT || v.kind == VAL_FLOAT;
    if (strcmp(kname, "Comparable") == 0)
        return v.kind == VAL_INT || v.kind == VAL_FLOAT || v.kind == VAL_STRING || v.kind == VAL_SYMBOL;
    if (strcmp(kname, "Enumerable") == 0)
        return v.kind == VAL_RANGE || v.kind == VAL_ARRAY || v.kind == VAL_HASH;
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

static int env_get_local_value(Env *env, const char *name, Value *out) {
    if (!env) return 0;
    for (EnvEntry *entry = env->vars; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            *out = entry->val;
            return 1;
        }
    }
    return 0;
}

static const char *find_instance_alias_name(RubyClass *klass, const char *name) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        if (!k->class_env) continue;
        Value alias_target = val_nil();
        if (env_get_local_value(k->class_env, name, &alias_target) &&
            (alias_target.kind == VAL_SYMBOL || alias_target.kind == VAL_STRING) &&
            alias_target.sval && alias_target.sval[0] != '\0')
            return alias_target.sval;
        if (k->is_module) break;
    }
    return NULL;
}

static int builtin_primitive_responds_to(Value recv, const char *name) {
    static const char *int_methods[] = {
        "to_s", "to_f", "to_i", "to_int", "to_r", "abs", "abs2", "even?", "odd?", "zero?",
        "nonzero?", "positive?", "negative?", "integer?", "ceil", "floor", "round",
        "truncate", "succ", "next", "pred", "chr", "gcd", "lcm", "pow", "divmod",
        "digits", "between?", "clamp", "times", "upto", "downto", "step",
        "+", "-", "*", "/", "%", "**", "<", "<=", ">", ">=", "<=>",
        "<<", ">>", "&", "|", "^", "~", "-@", "[]", NULL
    };
    static const char *float_methods[] = {
        "to_s", "to_f", "to_i", "truncate", "to_r", "abs", "abs2", "zero?", "nonzero?",
        "positive?", "negative?", "integer?", "nan?", "finite?", "infinite?", "ceil",
        "floor", "round", "divmod", "between?", "clamp", "step",
        "+", "-", "*", "/", "%", "**", "<", "<=", ">", ">=", "<=>", NULL
    };
    static const char *str_methods[] = {
        "to_s", "to_str", "to_i", "to_f", "to_sym", "to_r", "to_c", "length", "size", "empty?",
        "upcase", "upcase!", "downcase", "downcase!", "strip", "strip!", "chars", "include?",
        "start_with?", "end_with?", "split", "each_char", "reverse", "reverse!", "next", "succ",
        "replace", "inspect", "chomp", "chomp!", "chop", "chop!", "lstrip", "rstrip", "lstrip!",
        "rstrip!", "capitalize", "swapcase", "ljust", "rjust", "center", "ord", "hex", "oct",
        "bytes", "bytesize", "<<", "index", "rindex", "[]", "[]=", "slice", "slice!", "lines",
        "each_line", "tr", "tr!", "count", "delete", "delete!", "squeeze", "squeeze!", "scan",
        "sub", "sub!", "gsub", "gsub!", "match", "match?", "=~", "*", "+", "encoding",
        "encode", "force_encoding", "b", "freeze", "frozen?", "dup", "clone",
        "delete_prefix", "delete_suffix", "insert", "prepend", "concat", "format",
        "unpack", "unpack1", "setbyte", "getbyte", "byteslice", "hash",
        NULL
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
        "to_s", "inspect", "to_a", "to_h", "merge", "merge!", "update", "each", "each_pair",
        "each_key", "each_value", "each_with_object", "map", "collect", "select", "filter",
        "reject", "any?", "all?", "none?", "find", "detect",
        "min_by", "max_by", "sort_by", "sort", "flat_map", "reduce", "inject", "store",
        "clear", "dup", "nil?", "freeze", "frozen?", "transform_values", "transform_keys",
        "transform_values!", "transform_keys!", "filter_map", "slice", "except",
        "invert", "key", "assoc", "rassoc", "values_at", "fetch_values",
        "group_by", "tally", "flat_map", "zip", "each_with_index", "min", "max",
        "sum", "reduce", "inject", "first", "take", "drop",
        NULL
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
        "nil?", "to_s", "to_str", "to_a", "to_h", "to_i", "to_f", "to_r", "to_c",
        "inspect", "freeze", "frozen?", "dup", "class", "is_a?", "kind_of?",
        "respond_to?", "send", "==", "!=", "!", "hash", "object_id",
        NULL
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

int val_responds_to(Eval *ev, Value recv, const char *name, int include_private) {
    if (strcmp(name, "is_a?") == 0 || strcmp(name, "kind_of?") == 0 ||
        strcmp(name, "instance_of?") == 0 || strcmp(name, "class") == 0 ||
        strcmp(name, "nil?") == 0 || strcmp(name, "respond_to?") == 0 ||
        strcmp(name, "send") == 0 || strcmp(name, "__send__") == 0 ||
        strcmp(name, "public_send") == 0 ||
        strcmp(name, "freeze") == 0 || strcmp(name, "frozen?") == 0 ||
        strcmp(name, "object_id") == 0 || strcmp(name, "to_s") == 0 ||
        strcmp(name, "inspect") == 0 || strcmp(name, "==") == 0 ||
        strcmp(name, "!=") == 0 || strcmp(name, "equal?") == 0 ||
        strcmp(name, "method") == 0 || strcmp(name, "tap") == 0 ||
        strcmp(name, "then") == 0 || strcmp(name, "yield_self") == 0)
        return 1;
    /* Kernel functions available on every object */
    {
        static const char *kernel[] = {
            "puts", "print", "p", "pp", "warn", "require", "require_relative",
            "raise", "fail", "exit", "exit!", "abort", "lambda", "proc", "loop", "rand", "srand", "open",
            "__method__", "__dir__", "__FILE__", "__LINE__",
            "sleep", "catch", "throw", "trap", "at_exit", "printf", "sprintf", "format",
            "system", "spawn", "wait", "waitpid", "`", "load",
            NULL
        };
        for (int ki = 0; kernel[ki]; ki++) {
            if (strcmp(name, kernel[ki]) == 0) return 1;
        }
    }

    Env *singleton_env = value_singleton_env(recv);
    if (singleton_env) {
        Value m;
        if (env_get(singleton_env, name, &m) &&
            method_visible_for_respond_to(m, include_private))
            return 1;
    }

    if (recv.kind == VAL_OBJECT) {
        if (find_instance_alias_name(recv.obj->klass.klass, name))
            return 1;
        Value m;
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
    /* NODE_BLOCK (from define_singleton_method) uses block.params/body; NODE_DEF uses def.params/body */
    int is_block_node = (method.method.def_node->kind == NODE_BLOCK);
    NodeList *params = is_block_node ? method.method.def_node->block.params
                                     : method.method.def_node->def.params;
    int required = count_required_params(params);
    int has_splat = has_splat_param(params);
    int total = count_total_params(params);
    /* When the method declares keyword params, the trailing hash arg is kwargs,
       not a positional argument — don't count it against the positional arity. */
    int effective_argc = argc;
    Value kwargs = extract_kwargs(ev, params, args, &effective_argc);
    (void)kwargs;
    if ((effective_argc < required) || (!has_splat && effective_argc > total))
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
    if (ev->exception_class != NULL) return val_exception();
    const char *saved_file = ev->current_file;
    if (method.method.def_file) ev->current_file = method.method.def_file;
    ev->call_depth++;
    if (ev->active_def_count < EVAL_MAX_DEPTH)
        ev->active_defs[ev->active_def_count++] = method_env;
    eval_push_frame(ev, site ? site->span.line : 0, site ? site->span.col : 0, name);
    Node *body = is_block_node ? method.method.def_node->block.body
                               : method.method.def_node->def.body;
    Value result = eval_node(ev, method_env, body);
    eval_pop_frame(ev);
    if (ev->active_def_count > 0) ev->active_def_count--;
    ev->call_depth--;
    ev->current_file = saved_file;
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
    return eval_raise_class(ev, site, "NoMethodError", "undefined method '%s' for %s", name, prim_class_name(recv));
}

static const char *dispatch_to_s(Eval *ev, Env *env, Value v, Node *site) {
    if (v.kind == VAL_OBJECT) {
        Value s = dispatch_method(ev, env, v, "to_s", NULL, 0, NULL, site, 0, 1);
        if (!ev->errored && s.kind == VAL_STRING) return s.sval;
        ev->errored = 0; ev->exception_class = NULL;
    }
    return val_to_s(ev->arena, v);
}

static const char *dispatch_inspect(Eval *ev, Env *env, Value v, Node *site) {
    if (v.kind == VAL_OBJECT) {
        Value s = dispatch_method(ev, env, v, "inspect", NULL, 0, NULL, site, 0, 1);
        if (!ev->errored && s.kind == VAL_STRING) return s.sval;
        ev->errored = 0; ev->exception_class = NULL;
    }
    if (v.kind == VAL_ARRAY) {
        size_t n = v.array->len;
        if (n == 0) return "[]";
        const char **parts = arena_alloc(ev->arena, n * sizeof(char *));
        size_t total = 2;
        for (size_t i = 0; i < n; i++) {
            parts[i] = dispatch_inspect(ev, env, v.array->elems[i], site);
            total += strlen(parts[i]) + (i < n - 1 ? 2 : 0);
        }
        char *buf = arena_alloc(ev->arena, total + 1);
        size_t j = 0; buf[j++] = '[';
        for (size_t i = 0; i < n; i++) {
            size_t plen = strlen(parts[i]);
            memcpy(buf + j, parts[i], plen); j += plen;
            if (i < n - 1) { buf[j++] = ','; buf[j++] = ' '; }
        }
        buf[j++] = ']'; buf[j] = '\0';
        return buf;
    }
    if (v.kind == VAL_HASH) {
        RubyHash *h = v.hash;
        if (h->len == 0) return "{}";
        const char **ks = arena_alloc(ev->arena, h->len * sizeof(char *));
        const char **vs = arena_alloc(ev->arena, h->len * sizeof(char *));
        size_t total = 2;
        for (size_t i = 0; i < h->len; i++) {
            ks[i] = dispatch_inspect(ev, env, h->keys[i], site);
            vs[i] = dispatch_inspect(ev, env, h->vals[i], site);
            total += strlen(ks[i]) + 2 + strlen(vs[i]) + (i < h->len - 1 ? 2 : 0);
        }
        char *buf = arena_alloc(ev->arena, total + 1);
        size_t j = 0; buf[j++] = '{';
        for (size_t i = 0; i < h->len; i++) {
            size_t klen = strlen(ks[i]), vlen = strlen(vs[i]);
            memcpy(buf + j, ks[i], klen); j += klen;
            buf[j++] = '='; buf[j++] = '>';
            memcpy(buf + j, vs[i], vlen); j += vlen;
            if (i < h->len - 1) { buf[j++] = ','; buf[j++] = ' '; }
        }
        buf[j++] = '}'; buf[j] = '\0';
        return buf;
    }
    return val_inspect(ev->arena, v);
}

Value builtin_kernel(Eval *ev, Env *env, const char *name,
                            Value *args, int argc, Value *blk, Node *site) {
    Value stdout_obj = val_nil();
    int have_stdout = global_get(&ev->globals, "stdout", &stdout_obj);
    Value stderr_obj = val_nil();
    int have_stderr = global_get(&ev->globals, "stderr", &stderr_obj);

    if (strcmp(name, "__FILE__") == 0) {
        return ev->current_file ? val_string(ev->arena, ev->current_file) : val_string(ev->arena, "(eval)");
    }
    if (strcmp(name, "__LINE__") == 0) {
        return val_int(site ? site->span.line : 0);
    }

    if (strcmp(name, "__method__") == 0) {
        if (argc != 0)
            return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        Value m = val_nil();
        env_get(env, "__method__", &m);
        return m;
    }

    if (strcmp(name, "__dir__") == 0) {
        if (argc != 0)
            return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        if (!ev->current_file)
            return val_nil();
        const char *slash = strrchr(ev->current_file, '/');
        if (!slash)
            return val_string(ev->arena, ".");
        if (slash == ev->current_file)
            return val_string(ev->arena, "/");
        return val_string_n(ev->arena, ev->current_file, (size_t)(slash - ev->current_file));
    }

    if (strcmp(name, "caller") == 0 || strcmp(name, "caller_locations") == 0) {
        int skip = (argc > 0 && args[0].kind == VAL_INT) ? (int)args[0].ival : 1;
        Value arr = val_array_new();
        for (int fi = ev->frame_count - 1 - skip; fi >= 0; fi--) {
            char buf[512];
            snprintf(buf, sizeof(buf), "%u:%u:in `%s'",
                     ev->frames[fi].line, ev->frames[fi].col, ev->frames[fi].label);
            val_array_push(&arr, val_string(ev->arena, buf));
        }
        return arr;
    }
    if (strcmp(name, "binding") == 0) {
        if (argc != 0)
            return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        Value binding_class;
        if (!env_get(ev->top_env, "Binding", &binding_class) || binding_class.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "NameError", "uninitialized constant Binding");
        Value obj = val_object(ev->arena, binding_class);
        obj.obj->native = alloc_native_binding(ev->arena, env, ev->current_file,
                                               site ? (int64_t)site->span.line : 1);
        return obj;
    }

    if (strcmp(name, "eval") == 0) {
        if (argc < 1 || argc > 4)
            return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        if (args[0].kind != VAL_STRING)
            return eval_raise_class(ev, site, "TypeError", "eval requires a String");

        Env *target_env = env;
        const char *file = ev->current_file;
        int64_t line = 1;
        if (argc >= 2 && args[1].kind != VAL_NIL) {
            NativeBinding *binding = native_binding(args[1]);
            if (!binding)
                return eval_raise_class(ev, site, "TypeError", "wrong argument type %s (expected Binding)",
                                        value_class_name(ev, args[1]));
            target_env = binding->env;
            file = binding->file ? binding->file : file;
            line = binding->line > 0 ? binding->line : 1;
        }
        if (argc >= 3 && args[2].kind != VAL_NIL) {
            if (args[2].kind != VAL_STRING && args[2].kind != VAL_SYMBOL)
                return eval_raise_class(ev, site, "TypeError", "eval filename must be a String");
            file = args[2].sval;
        }
        if (argc >= 4 && args[3].kind != VAL_NIL) {
            if (args[3].kind != VAL_INT)
                return eval_raise_class(ev, site, "TypeError", "eval line number must be an Integer");
            line = args[3].ival;
        }
        return eval_string_in_context(ev, env, args[0].sval, target_env, file, line, site);
    }

    if (have_stdout && (strcmp(name, "puts") == 0 || strcmp(name, "print") == 0 ||
                        strcmp(name, "p") == 0 || strcmp(name, "pp") == 0)) {
        if (strcmp(name, "puts") == 0) {
            if (argc == 0) {
                Value nl = val_string(ev->arena, "\n");
                return dispatch_method(ev, env, stdout_obj, "write", &nl, 1, NULL, site, 0, 1);
            }
            for (int i = 0; i < argc; i++) {
                if (args[i].kind == VAL_ARRAY) {
                    for (size_t j = 0; j < args[i].array->len; j++) {
                        const char *s = dispatch_to_s(ev, env, args[i].array->elems[j], site);
                        size_t len = strlen(s);
                        size_t extra = (len == 0 || s[len - 1] != '\n') ? 1 : 0;
                        char *buf = arena_alloc(ev->arena, len + 2);
                        memcpy(buf, s, len);
                        if (extra) buf[len] = '\n';
                        buf[len + extra] = '\0';
                        Value line = val_string(ev->arena, buf);
                        Value out = dispatch_method(ev, env, stdout_obj, "write", &line, 1, NULL, site, 0, 1);
                        if (val_is_signal(out)) return out;
                    }
                } else {
                    const char *s = dispatch_to_s(ev, env, args[i], site);
                    size_t len = strlen(s);
                    /* Don't add extra \n if string already ends in \n */
                    size_t extra = (len == 0 || s[len - 1] != '\n') ? 1 : 0;
                    char *buf = arena_alloc(ev->arena, len + 2);
                    memcpy(buf, s, len);
                    if (extra) buf[len] = '\n';
                    buf[len + extra] = '\0';
                    Value line = val_string(ev->arena, buf);
                    Value out = dispatch_method(ev, env, stdout_obj, "write", &line, 1, NULL, site, 0, 1);
                    if (val_is_signal(out)) return out;
                }
            }
            return val_nil();
        }
        if (strcmp(name, "print") == 0) {
            for (int i = 0; i < argc; i++) {
                Value str = val_string(ev->arena, dispatch_to_s(ev, env, args[i], site));
                Value out = dispatch_method(ev, env, stdout_obj, "write", &str, 1, NULL, site, 0, 1);
                if (val_is_signal(out)) return out;
            }
            return val_nil();
        }
        /* p/pp: dispatch inspect for objects, fall back to val_inspect */
        for (int i = 0; i < argc; i++) {
            const char *s;
            s = dispatch_inspect(ev, env, args[i], site);
            size_t len = strlen(s);
            char *buf = arena_alloc(ev->arena, len + 2);
            memcpy(buf, s, len);
            buf[len] = '\n';
            buf[len + 1] = '\0';
            Value line = val_string(ev->arena, buf);
            Value out = dispatch_method(ev, env, stdout_obj, "write", &line, 1, NULL, site, 0, 1);
            if (val_is_signal(out)) return out;
        }
        if (argc == 1) return args[0];
        Value arr = val_array_new();
        for (int i = 0; i < argc; i++) val_array_push(&arr, args[i]);
        return arr;
    }

    if (strcmp(name, "catch") == 0) {
        const char *tag = (argc > 0 && (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING))
                          ? args[0].sval : NULL;
        if (!blk) return val_nil();
        Value result = call_block(ev, env, *blk, args, argc, site);
        if (result.kind == VAL_THROW) {
            if (!tag || !result.throw_sig.tag || strcmp(tag, result.throw_sig.tag) == 0) {
                return result.throw_sig.value ? *result.throw_sig.value : val_nil();
            }
        }
        if (val_is_signal(result) && result.kind != VAL_THROW) return result;
        return result;
    }
    if (strcmp(name, "throw") == 0) {
        const char *tag = (argc > 0 && (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING))
                          ? args[0].sval : NULL;
        Value inner = (argc > 1) ? args[1] : val_nil();
        Value *valptr = arena_alloc(ev->arena, sizeof(Value));
        *valptr = inner;
        Value signal; memset(&signal, 0, sizeof(signal));
        signal.kind = VAL_THROW;
        signal.throw_sig.tag = tag;
        signal.throw_sig.value = valptr;
        return signal;
    }
    if (strcmp(name, "method") == 0) {
        /* Bare method(:name) — look up on self */
        if (argc < 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        Value self = val_nil();
        env_get(env, "self", &self);
        return dispatch_method(ev, env, self, "method", args, argc, blk, site, 0, 1);
    }
    if (strcmp(name, "trap") == 0) {
        return val_string(ev->arena, "DEFAULT");
    }
    if (strcmp(name, "at_exit") == 0) {
        if (blk) {
            AtExitHandler *h = arena_alloc(ev->arena, sizeof(AtExitHandler));
            h->blk  = *blk;
            h->next = ev->at_exit_handlers;
            ev->at_exit_handlers = h;
        }
        return val_nil();
    }
    if (strcmp(name, "sleep") == 0) {
        return val_int(argc > 0 && args[0].kind == VAL_INT ? args[0].ival :
                       argc > 0 && args[0].kind == VAL_FLOAT ? (int64_t)args[0].fval : 0);
    }
    if (strcmp(name, "`") == 0) {
        if (argc == 1 && args[0].kind == VAL_STRING) {
            const char *cmd = args[0].sval;
            FILE *fp = popen(cmd, "r");
            if (!fp) {
                set_child_status(ev, env, -1, site);
                return val_string(ev->arena, "");
            }
            char buf[4096]; size_t total = 0;
            char *out = arena_alloc(ev->arena, 1);
            out[0] = '\0';
            while (fgets(buf, sizeof(buf), fp)) {
                size_t n = strlen(buf);
                char *next = arena_alloc(ev->arena, total + n + 1);
                memcpy(next, out, total);
                memcpy(next + total, buf, n + 1);
                out = next; total += n;
            }
            int wstatus = pclose(fp);
            int exit_code = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
            set_child_status(ev, env, exit_code, site);
            return val_string(ev->arena, out);
        }
        set_child_status(ev, env, 0, site);
        return val_string(ev->arena, "");
    }
    if (strcmp(name, "system") == 0) {
        return run_child_process(ev, env, val_nil(), args, argc, site, 0);
    }
    if (strcmp(name, "spawn") == 0) {
        return run_child_process(ev, env, val_nil(), args, argc, site, 1);
    }
    if (strcmp(name, "wait") == 0 || strcmp(name, "waitpid") == 0) {
        pid_t pid = argc > 0 && args[0].kind == VAL_INT ? (pid_t)args[0].ival : -1;
        int status = 0;
        pid_t result = waitpid(pid, &status, 0);
        if (result < 0) return eval_raise_class(ev, site, "SystemCallError", "%s", strerror(errno));
        return val_int((int64_t)result);
    }

    if (strcmp(name, "puts") == 0) {
        if (argc == 0) { fprintf(ev->out, "\n"); return val_nil(); }
        for (int i = 0; i < argc; i++) {
            if (args[i].kind == VAL_ARRAY) {
                for (size_t j = 0; j < args[i].array->len; j++) {
                    const char *s2 = dispatch_to_s(ev, env, args[i].array->elems[j], site);
                    size_t slen2 = strlen(s2);
                    if (slen2 > 0 && s2[slen2 - 1] == '\n') fprintf(ev->out, "%s", s2);
                    else fprintf(ev->out, "%s\n", s2);
                }
            } else {
                const char *s2 = dispatch_to_s(ev, env, args[i], site);
                size_t slen2 = strlen(s2);
                if (slen2 > 0 && s2[slen2 - 1] == '\n') fprintf(ev->out, "%s", s2);
                else fprintf(ev->out, "%s\n", s2);
            }
        }
        return val_nil();
    }
    if (strcmp(name, "print") == 0) {
        for (int i = 0; i < argc; i++)
            fprintf(ev->out, "%s", dispatch_to_s(ev, env, args[i], site));
        return val_nil();
    }
    if (strcmp(name, "p") == 0 || strcmp(name, "pp") == 0) {
        for (int i = 0; i < argc; i++) {
            const char *s;
            s = dispatch_inspect(ev, env, args[i], site);
            fprintf(ev->out, "%s\n", s);
        }
        if (argc == 1) return args[0];
        Value arr = val_array_new();
        for (int i = 0; i < argc; i++) val_array_push(&arr, args[i]);
        return arr;
    }
    if (strcmp(name, "warn") == 0) {
        if (have_stderr) {
            if (argc == 0) {
                Value nl = val_string(ev->arena, "\n");
                return dispatch_method(ev, env, stderr_obj, "write", &nl, 1, NULL, site, 0, 1);
            }
            for (int i = 0; i < argc; i++) {
                if (args[i].kind == VAL_ARRAY) {
                    for (size_t j = 0; j < args[i].array->len; j++) {
                        const char *s = val_to_s(ev->arena, args[i].array->elems[j]);
                        size_t len = strlen(s);
                        char *buf = arena_alloc(ev->arena, len + 2);
                        memcpy(buf, s, len);
                        buf[len] = '\n';
                        buf[len + 1] = '\0';
                        Value line = val_string(ev->arena, buf);
                        Value out = dispatch_method(ev, env, stderr_obj, "write", &line, 1, NULL, site, 0, 1);
                        if (val_is_signal(out)) return out;
                    }
                } else {
                    const char *s = val_to_s(ev->arena, args[i]);
                    size_t len = strlen(s);
                    char *buf = arena_alloc(ev->arena, len + 2);
                    memcpy(buf, s, len);
                    buf[len] = '\n';
                    buf[len + 1] = '\0';
                    Value line = val_string(ev->arena, buf);
                    Value out = dispatch_method(ev, env, stderr_obj, "write", &line, 1, NULL, site, 0, 1);
                    if (val_is_signal(out)) return out;
                }
            }
            return val_nil();
        }
        if (argc == 0) {
            fprintf(stderr, "\n");
            return val_nil();
        }
        for (int i = 0; i < argc; i++) {
            if (args[i].kind == VAL_ARRAY) {
                for (size_t j = 0; j < args[i].array->len; j++)
                    fprintf(stderr, "%s\n", val_to_s(ev->arena, args[i].array->elems[j]));
            } else {
                fprintf(stderr, "%s\n", val_to_s(ev->arena, args[i]));
            }
        }
        return val_nil();
    }
    if (strcmp(name, "printf") == 0) {
        /* printf([io,] format, *args) — write formatted string to stdout */
        if (argc < 1) return val_nil();
        int fmt_idx = 0;
        /* Check if first arg is an IO object (skip it) */
        if (args[0].kind == VAL_OBJECT) fmt_idx = 1;
        if (fmt_idx >= argc) return val_nil();
        const char *fmt = args[fmt_idx].kind == VAL_STRING ? args[fmt_idx].sval : val_to_s(ev->arena, args[fmt_idx]);
        Value result = eval_format_string(ev, env, fmt, args + fmt_idx + 1, argc - fmt_idx - 1, site);
        if (val_is_signal(result)) return result;
        if (result.kind == VAL_STRING)
            fprintf(ev->out, "%s", result.sval);
        return val_nil();
    }
    if (strcmp(name, "format") == 0 || strcmp(name, "sprintf") == 0) {
        if (argc < 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        const char *fmt = val_to_s(ev->arena, args[0]);
        return eval_format_string(ev, env, fmt, args + 1, argc - 1, site);
    }
    if (strcmp(name, "Integer") == 0) {
        if (argc < 1 || argc > 2) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        Value v = args[0];
        int explicit_base = (argc == 2 && args[1].kind == VAL_INT) ? (int)args[1].ival : 0;
        if (v.kind == VAL_INT) return v;
        if (v.kind == VAL_FLOAT && explicit_base == 0) return val_int((int64_t)v.fval);
        if (v.kind == VAL_STRING) {
            const char *s = v.sval ? v.sval : "";
            while (*s == ' ' || *s == '\t') s++;
            int base = explicit_base;
            if (base == 0) {
                if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
                else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) { base = 2;  s += 2; }
                else if (s[0] == '0' && (s[1] == 'o' || s[1] == 'O')) { base = 8;  s += 2; }
                else if (s[0] == '0' && s[1] >= '0' && s[1] <= '7')   { base = 8; }
                else base = 10;
            } else {
                /* explicit base: strip optional prefix if it matches */
                if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
                else if (base == 2  && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) s += 2;
                else if (base == 8  && s[0] == '0' && (s[1] == 'o' || s[1] == 'O')) s += 2;
            }
            char *end = NULL;
            int64_t result = (int64_t)strtoll(s, &end, base);
            /* Validate the whole string was consumed (skip trailing spaces) */
            while (end && (*end == ' ' || *end == '\t')) end++;
            if (!end || *end != '\0')
                return eval_raise_class(ev, site, "ArgumentError",
                    "invalid value for Integer(): \"%s\"", v.sval ? v.sval : "");
            return val_int(result);
        }
        if (!val_responds_to(ev, v, "to_i", 1))
            return eval_raise_class(ev, site, "TypeError", "can't convert %s into Integer", val_kind_name(v.kind));
        Value converted = dispatch_method(ev, env, v, "to_i", NULL, 0, NULL, site, 0, -1);
        if (val_is_signal(converted)) return converted;
        if (converted.kind == VAL_INT) return converted;
        return eval_raise_class(ev, site, "TypeError", "can't convert %s into Integer", val_kind_name(v.kind));
    }
    if (strcmp(name, "Float") == 0) {
        if (argc != 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        Value v = args[0];
        if (v.kind == VAL_FLOAT) return v;
        if (v.kind == VAL_INT) return val_float((double)v.ival);
        if (v.kind == VAL_STRING) {
            const char *s = v.sval ? v.sval : "";
            while (*s == ' ' || *s == '\t') s++;
            char *end = NULL;
            double fval = strtod(s, &end);
            while (end && (*end == ' ' || *end == '\t')) end++;
            if (!end || *end != '\0' || end == s)
                return eval_raise_class(ev, site, "ArgumentError",
                    "invalid value for Float(): \"%s\"", v.sval ? v.sval : "");
            return val_float(fval);
        }
        if (!val_responds_to(ev, v, "to_f", 1))
            return eval_raise_class(ev, site, "TypeError", "can't convert %s into Float", val_kind_name(v.kind));
        Value converted = dispatch_method(ev, env, v, "to_f", NULL, 0, NULL, site, 0, -1);
        if (val_is_signal(converted)) return converted;
        if (converted.kind == VAL_FLOAT) return converted;
        if (converted.kind == VAL_INT) return val_float((double)converted.ival);
        return eval_raise_class(ev, site, "TypeError", "can't convert %s into Float", val_kind_name(v.kind));
    }
    if (strcmp(name, "String") == 0) {
        if (argc != 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        if (args[0].kind == VAL_STRING) return args[0];
        return val_string(ev->arena, val_to_s(ev->arena, args[0]));
    }
    if (strcmp(name, "Array") == 0) {
        if (argc != 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        Value v = args[0];
        if (v.kind == VAL_ARRAY) return v;
        if (v.kind == VAL_NIL) return val_array_new();
        if (!val_responds_to(ev, v, "to_a", 1)) {
            Value arr = val_array_new();
            val_array_push(&arr, v);
            return arr;
        }
        Value converted = dispatch_method(ev, env, v, "to_a", NULL, 0, NULL, site, 0, -1);
        if (val_is_signal(converted)) return converted;
        if (converted.kind == VAL_ARRAY) return converted;
        Value arr = val_array_new();
        val_array_push(&arr, v);
        return arr;
    }
    if (strcmp(name, "Hash") == 0) {
        if (argc != 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        Value v = args[0];
        if (v.kind == VAL_HASH) return v;
        if (v.kind == VAL_NIL) return val_hash_new(ev->arena);
        if (v.kind == VAL_ARRAY && v.array && v.array->len == 0) return val_hash_new(ev->arena);
        /* Try to_hash */
        Value converted = dispatch_method(ev, env, v, "to_hash", NULL, 0, NULL, site, 0, -1);
        if (!val_is_signal(converted) && converted.kind == VAL_HASH) return converted;
        ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
        return eval_raise_class(ev, site, "TypeError", "no implicit conversion of %s into Hash", value_class_name(ev, v));
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
            /* Route through ClassName.new(args) so user initialize is called */
            Value new_args = argc >= 2 ? args[1] : val_nil();
            Value exc_obj = dispatch_method(ev, env, args[0], "new",
                                            argc >= 2 ? &args[1] : NULL,
                                            argc >= 2 ? argc - 1 : 0,
                                            NULL, site, 0, 1);
            if (!val_is_signal(exc_obj) && exc_obj.kind == VAL_OBJECT &&
                value_is_a_named_class(ev, exc_obj, "Exception"))
                return eval_raise_value(ev, site, exc_obj);
            (void)new_args;
            msg = argc >= 2 ? val_to_s(ev->arena, args[1]) : class_name;
        } else if (argc >= 1) {
            msg = val_to_s(ev->arena, args[0]);
        }
        return eval_raise_class(ev, site, class_name, "%s", msg);
    }
    if (strcmp(name, "proc") == 0) {
        if (!blk) return eval_raise_class(ev, site, "ArgumentError", "proc requires a block");
        Value p = val_proc(blk->block.block_node, blk->block.closure);
        p.block.def_file = ev->current_file;
        return p;
    }
    if (strcmp(name, "lambda") == 0) {
        if (!blk) return eval_raise_class(ev, site, "ArgumentError", "lambda requires a block");
        Value l = val_lambda(blk->block.block_node, blk->block.closure);
        l.block.def_file = ev->current_file;
        return l;
    }
    if (strcmp(name, "loop") == 0) {
        if (!blk) return eval_raise_class(ev, site, "LocalJumpError", "no block given (loop)");
        for (;;) {
            Value r = call_block(ev, env, *blk, NULL, 0, site);
            if (r.kind == VAL_EXCEPTION) {
                if (ev->current_exception.kind == VAL_OBJECT &&
                    value_is_a_named_class(ev, ev->current_exception, "StopIteration")) {
                    ev->exception_class = NULL;
                    ev->current_exception = val_nil();
                    return val_nil();
                }
                return r;
            }
            if (r.kind == VAL_BREAK)
                return r.jump.wrapped ? *r.jump.wrapped : val_nil();
            if (r.kind == VAL_RETURN) return r;
        }
    }
    if (strcmp(name, "rand") == 0) {
        double r01 = (double)rand() / ((double)RAND_MAX + 1.0);
        if (argc == 0) return val_float(r01);
        if (args[0].kind == VAL_FLOAT) {
            double n = args[0].fval;
            return n <= 0.0 ? val_float(r01) : val_float(r01 * n);
        }
        if (args[0].kind == VAL_RANGE) {
            RubyRange *rng = args[0].range;
            int64_t lo = rng->begin_val.kind == VAL_INT ? rng->begin_val.ival : 0;
            int64_t hi = rng->end_val.kind == VAL_INT ? rng->end_val.ival :
                         rng->end_val.kind == VAL_NIL ? lo + 1000000 : (int64_t)rng->end_val.fval;
            if (rng->exclusive) hi--;
            if (hi < lo) return val_nil();
            return val_int(lo + (int64_t)(r01 * (double)(hi - lo + 1)));
        }
        int64_t n = args[0].kind == VAL_INT ? args[0].ival : 1;
        if (n <= 0) return val_float(r01);
        return val_int((int64_t)(r01 * (double)n));
    }
    if (strcmp(name, "open") == 0) {
        /* Kernel#open — opens a file like File.open */
        if (argc < 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        const char *path = val_to_s(ev->arena, args[0]);
        const char *mode = argc >= 2 ? val_to_s(ev->arena, args[1]) : "r";
        Value file_class;
        if (!env_get(ev->top_env, "File", &file_class) || file_class.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "NameError", "uninitialized constant File");
        Value file_args[2] = { args[0], argc >= 2 ? args[1] : val_string(ev->arena, "r") };
        return dispatch_method(ev, env, file_class, "open", file_args, argc >= 2 ? 2 : 1, blk, site, 0, 1);
        (void)path; (void)mode;
    }
    if (strcmp(name, "srand") == 0) {
        unsigned int seed = argc > 0 && args[0].kind == VAL_INT
                            ? (unsigned int)args[0].ival
                            : (unsigned int)time(NULL);
        srand(seed);
        return val_int((int64_t)seed);
    }
    if (strcmp(name, "exit") == 0 || strcmp(name, "exit!") == 0) {
        int code = 0;
        if (argc > 0) {
            if (args[0].kind == VAL_BOOL) code = args[0].bval ? 0 : 1;
            else if (args[0].kind == VAL_INT) code = (int)args[0].ival;
        }
        if (strcmp(name, "exit!") == 0) {
            /* exit! bypasses rescue and at_exit — call C exit directly */
            exit(code);
        }
        /* Raise SystemExit so it can be rescued; main.c extracts the status */
        Value klass;
        if (!env_get(ev->top_env, "SystemExit", &klass) || klass.kind != VAL_CLASS)
            exit(code);
        Value exc = build_exception_object(ev, klass, code == 0 ? "exit" : "exit");
        val_object_set_ivar(ev->arena, exc, "status", val_int((int64_t)code));
        return eval_raise_value(ev, site, exc);
    }
    if (strcmp(name, "abort") == 0) {
        if (argc > 0 && args[0].kind == VAL_STRING)
            fprintf(stderr, "%s\n", args[0].sval);
        Value klass;
        if (!env_get(ev->top_env, "SystemExit", &klass) || klass.kind != VAL_CLASS)
            exit(1);
        Value exc = build_exception_object(ev, klass, "abort");
        val_object_set_ivar(ev->arena, exc, "status", val_int(1));
        return eval_raise_value(ev, site, exc);
    }
    if (strcmp(name, "attr_reader") == 0 || strcmp(name, "attr_writer") == 0 ||
        strcmp(name, "attr_accessor") == 0) {
        Value self;
        Value singleton_target = val_nil();
        int in_singleton_class = env_get(env, "__singleton_target__", &singleton_target);
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "TypeError", "%s must be called in a class body", name);
        for (int i = 0; i < argc; i++) {
            const char *attr = (args[i].kind == VAL_SYMBOL || args[i].kind == VAL_STRING)
                               ? args[i].sval : NULL;
            if (!attr) continue;
            if (in_singleton_class && singleton_target.kind == VAL_CLASS) {
                size_t alen = strlen(attr);
                char *reader_name = arena_alloc(ev->arena, alen + 6);
                memcpy(reader_name, "self.", 5);
                memcpy(reader_name + 5, attr, alen + 1);
                char *writer_name = arena_alloc(ev->arena, alen + 7);
                memcpy(writer_name, "self.", 5);
                memcpy(writer_name + 5, attr, alen);
                writer_name[alen + 5] = '=';
                writer_name[alen + 6] = '\0';
                if (strcmp(name, "attr_reader") == 0 || strcmp(name, "attr_accessor") == 0)
                    define_attr_reader_in_env(ev, singleton_target.klass->class_env, reader_name, attr);
                if (strcmp(name, "attr_writer") == 0 || strcmp(name, "attr_accessor") == 0)
                    define_attr_writer_in_env(ev, singleton_target.klass->class_env, writer_name, attr);
            } else {
                if (strcmp(name, "attr_reader") == 0 || strcmp(name, "attr_accessor") == 0)
                    define_attr_reader(ev, self, attr);
                if (strcmp(name, "attr_writer") == 0 || strcmp(name, "attr_accessor") == 0)
                    define_attr_writer(ev, self, attr);
            }
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
            /* Call Module.included(base) hook if defined */
            {
                size_t ilen = strlen("included");
                char *ikey = arena_alloc(ev->arena, ilen + 6);
                memcpy(ikey, "self.", 5); memcpy(ikey + 5, "included", ilen + 1);
                Value cm;
                if (env_get(args[i].klass->class_env, ikey, &cm) && cm.kind == VAL_METHOD) {
                    call_method_value(ev, env, args[i], cm, args[i].klass, "included", &self, 1, NULL, site);
                }
            }
            if (strcmp(args[i].klass->name, "Singleton") == 0) {
                Node *body = node_new(ev->arena, NODE_BODY, site ? site->span : (Span){0, 0, 0});
                body->body.stmts = NULL;
                Node *def = node_new(ev->arena, NODE_DEF, site ? site->span : (Span){0, 0, 0});
                def->def.name = "instance";
                def->def.body = body;
                env_define(ev->arena, self.klass->class_env, "self.instance",
                           val_method(def, ev->top_env, METHOD_PUBLIC, ev->current_file));
            }
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
    if (strcmp(name, "load") == 0) {
        if (argc < 1)
            return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments (given 0, expected 1+)");
        if (args[0].kind != VAL_STRING)
            return eval_raise_class(ev, site, "TypeError", "no implicit conversion of %s into String",
                                    value_class_name(ev, args[0]));
        return eval_load(ev, args[0].sval, site);
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
    if (strcmp(name, "alias_method") == 0) {
        Value self;
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "TypeError", "alias_method must be called in a class or module body");
        if (argc != 2)
            return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        const char *new_name = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        const char *old_name = (args[1].kind == VAL_SYMBOL || args[1].kind == VAL_STRING) ? args[1].sval : NULL;
        if (!new_name || !old_name)
            return eval_raise_class(ev, site, "TypeError", "expected Symbol or String");
        Value method;
        if (!ruby_class_find_instance_method(self.klass, old_name, &method, NULL))
            return eval_raise_class(ev, site, "NameError", "undefined method '%s' for alias_method", old_name);
        env_define(ev->arena, self.klass->class_env, new_name, method);
        return val_symbol(new_name);
    }
    if (strcmp(name, "module_function") == 0) {
        Value self;
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS || !self.klass->is_module)
            return eval_raise_class(ev, site, "TypeError", "module_function must be called in a module body");
        if (argc == 0)
            return val_nil(); /* no-arg form sets module_function mode for subsequent defs — stub */
        for (int i = 0; i < argc; i++) {
            const char *mname = (args[i].kind == VAL_SYMBOL || args[i].kind == VAL_STRING) ? args[i].sval : NULL;
            if (!mname) continue;
            Value method;
            if (!ruby_class_find_instance_method(self.klass, mname, &method, NULL))
                return eval_raise_class(ev, site, "NameError", "undefined method '%s' for module_function", mname);
            size_t nlen = strlen(mname);
            char *key = arena_alloc(ev->arena, nlen + 6);
            memcpy(key, "self.", 5);
            memcpy(key + 5, mname, nlen + 1);
            env_define(ev->arena, self.klass->class_env, key, method);
            update_method_visibility(self.klass->class_env, mname, METHOD_PRIVATE, 0);
        }
        return val_nil();
    }
    if (strcmp(name, "autoload") == 0) {
        Value self;
        if (!env_get(env, "self", &self) || self.kind != VAL_CLASS)
            return eval_raise_class(ev, site, "TypeError", "autoload must be called in a class or module body");
        if (argc != 2)
            return eval_raise_class(ev, site, "ArgumentError",
                                    "wrong number of arguments (given %d, expected 2)", argc);
        const char *path = NULL;
        if (args[1].kind == VAL_STRING || args[1].kind == VAL_SYMBOL)
            path = args[1].sval;
        if (!path)
            return eval_raise_class(ev, site, "TypeError", "autoload path must be a String");
        return eval_require(ev, env, path, site);
    }
    if (strcmp(name, "deprecate_constant") == 0 || strcmp(name, "private_constant") == 0 ||
        strcmp(name, "public_constant") == 0 || strcmp(name, "using") == 0) {
        (void)blk;
        return val_nil();
    }
    (void)blk;
    return val_nil();
}

static Value make_symbol_proc(Eval *ev, const char *method_name);

Value make_bound_method_proc(Eval *ev, Value receiver, const char *method_name) {
    Arena *a = ev->arena;
    Span s = {0, 0, 0};
    Env *closure = env_new(a, ev->top_env, 0);
    env_define(a, closure, "__bound_recv__", receiver);
    Node *rest_p = node_new(a, NODE_PARAM, s);
    rest_p->param.splat = 1;
    rest_p->param.name = "__bound_args__";
    NodeList *params = nodelist_append(a, NULL, rest_p);
    Node *recv_var = node_new(a, NODE_LVAR, s);
    recv_var->sval = "__bound_recv__";
    Node *args_var = node_new(a, NODE_LVAR, s);
    args_var->sval = "__bound_args__";
    Node *splat_arg = node_new(a, NODE_UNOP, s);
    splat_arg->unop.op = "*";
    splat_arg->unop.operand = args_var;
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
    return val_lambda(block_node, closure);
}

Value dispatch_method(Eval *ev, Env *env __attribute__((unused)), Value recv,
                      const char *name, Value *args, int argc,
                      Value *blk, Node *site, int public_only, int explicit_receiver);

Value dispatch_method(Eval *ev, Env *env __attribute__((unused)), Value recv,
                      const char *name, Value *args, int argc,
                      Value *blk, Node *site, int public_only, int explicit_receiver) {
    Value out;
    Env *singleton_env = value_singleton_env(recv);

    if (singleton_env) {
        Value method;
        RubyClass *owner = recv.kind == VAL_OBJECT && recv.obj->klass.kind == VAL_CLASS
                         ? recv.obj->klass.klass : NULL;
        if (env_get(singleton_env, name, &method) && method.kind == VAL_METHOD) {
            if (!method_visibility_allows_call(ev, env, recv, owner, method.method.visibility,
                                               public_only, explicit_receiver)) {
                if (method.method.visibility == METHOD_PROTECTED)
                    return eval_raise_class(ev, site, "NoMethodError",
                                            "protected method '%s' called for an instance of %s",
                                            name, value_class_name(ev, recv));
            } else {
                return call_method_value(ev, env, recv, method, owner, name, args, argc, blk, site);
            }
        }
    }

    if (recv.kind == VAL_OBJECT && recv.obj->klass.kind == VAL_CLASS) {
        const char *alias_target = find_instance_alias_name(recv.obj->klass.klass, name);
        if (alias_target && strcmp(alias_target, name) != 0)
            return dispatch_method(ev, env, recv, alias_target, args, argc, blk, site, public_only, explicit_receiver);
    }

    if (strcmp(name, "nil?") == 0) {
        if (argc != 0) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        return val_bool(recv.kind == VAL_NIL);
    }
    if (recv.kind == VAL_OBJECT && recv.obj->klass.kind == VAL_CLASS &&
        strcmp(recv.obj->klass.klass->name, "Pathname") == 0) {
        Value path = val_nil();
        val_object_get_ivar(recv, "path", &path);
        const char *p = path.kind == VAL_STRING ? path.sval : "";
        if (strcmp(name, "to_s") == 0) return val_string(ev->arena, p);
        if (strcmp(name, "dirname") == 0) {
            const char *slash = strrchr(p, '/');
            const char *dir = ".";
            if (slash) {
                if (slash == p) dir = "/";
                else {
                    size_t dlen = (size_t)(slash - p);
                    char *buf = arena_alloc(ev->arena, dlen + 1);
                    memcpy(buf, p, dlen);
                    buf[dlen] = '\0';
                    dir = buf;
                }
            }
            Value pathname_class;
            if (!env_get(ev->top_env, "Pathname", &pathname_class) || pathname_class.kind != VAL_CLASS)
                return val_string(ev->arena, dir);
            Value obj = val_object(ev->arena, pathname_class);
            val_object_set_ivar(ev->arena, obj, "path", val_string(ev->arena, dir));
            return obj;
        }
        if (strcmp(name, "exist?") == 0) {
            struct stat st;
            return val_bool(stat(p, &st) == 0);
        }
        if (strcmp(name, "writable?") == 0)
            return val_bool(access(p, W_OK) == 0);
        if (strcmp(name, "stat") == 0) {
            struct stat st;
            if (stat(p, &st) != 0)
                return eval_raise_class(ev, site, errno_class_name(errno), "%s - %s", strerror(errno), p);
            Value stat_class = val_class(ev->arena, "Pathname::Stat", val_nil());
            Value stat_obj = val_object(ev->arena, stat_class);
            val_object_set_ivar(ev->arena, stat_obj, "mode", val_int((int64_t)st.st_mode));
            return stat_obj;
        }
        if (strcmp(name, "chmod") == 0) {
            if (argc != 1 || args[0].kind != VAL_INT)
                return eval_raise_class(ev, site, "TypeError", "Pathname#chmod requires an Integer");
            if (chmod(p, (mode_t)args[0].ival) != 0)
                return eval_raise_class(ev, site, errno_class_name(errno), "%s - %s", strerror(errno), p);
            return val_int(0);
        }
    }
    if (recv.kind == VAL_OBJECT && recv.obj->klass.kind == VAL_CLASS &&
        (strcmp(name, "inspect") == 0 || strcmp(name, "to_s") == 0) &&
        !value_is_a_named_class(ev, recv, "Exception") &&
        !value_is_a_named_class(ev, recv, "Encoding") &&
        !value_is_a_named_class(ev, recv, "Struct") &&
        strcmp(recv.obj->klass.klass->name, "Regexp") != 0 &&
        strcmp(recv.obj->klass.klass->name, "MatchData") != 0 &&
        strcmp(recv.obj->klass.klass->name, "Time") != 0) {
        Value custom = val_nil();
        if (!ruby_class_find_instance_method(recv.obj->klass.klass, name, &custom, NULL)) {
            const char *klass_name = recv.obj->klass.klass->name ? recv.obj->klass.klass->name : "Object";
            char buf[128];
            snprintf(buf, sizeof(buf), "#<%s:%p>", klass_name, (void *)recv.obj);
            return val_string(ev->arena, buf);
        }
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
        /* Pointer/identity equality — same object in memory */
        if (argc < 1) return val_false();
        if (recv.kind != args[0].kind) return val_false();
        switch (recv.kind) {
            case VAL_OBJECT: return val_bool(recv.obj     == args[0].obj);
            case VAL_BLOCK:  return val_bool(recv.block.block_node == args[0].block.block_node);
            case VAL_STRING: return val_bool(recv.sval    == args[0].sval);
            case VAL_ARRAY:  return val_bool(recv.array   == args[0].array);
            case VAL_HASH:   return val_bool(recv.hash    == args[0].hash);
            case VAL_CLASS:  return val_bool(recv.klass   == args[0].klass);
            default:         return val_bool(val_equal(recv, args[0]));
        }
    }
    if (strcmp(name, "eql?") == 0) {
        if (argc < 1) return val_false();
        /* eql? requires same type and value — differs from == for numeric cross-type */
        if (recv.kind != args[0].kind) return val_false();
        return val_bool(val_equal(recv, args[0]));
    }
    if (strcmp(name, "freeze") == 0) {
        if (recv.kind == VAL_OBJECT) recv.obj->frozen = 1;
        else if (recv.kind == VAL_ARRAY) recv.array->frozen = 1;
        else if (recv.kind == VAL_HASH)  recv.hash->frozen = 1;
        else recv.frozen = 1;
        return recv;
    }
    if (strcmp(name, "frozen?") == 0) {
        if (recv.kind == VAL_OBJECT) return val_bool(recv.obj->frozen);
        if (recv.kind == VAL_ARRAY)  return val_bool(recv.array->frozen);
        if (recv.kind == VAL_HASH)   return val_bool(recv.hash->frozen);
        /* integers, floats, symbols, nil, true, false are always frozen */
        if (recv.kind == VAL_INT || recv.kind == VAL_FLOAT ||
            recv.kind == VAL_SYMBOL || recv.kind == VAL_NIL || recv.kind == VAL_BOOL)
            return val_true();
        return val_bool(recv.frozen);
    }
    if (strcmp(name, "dup") == 0 || strcmp(name, "clone") == 0) {
        int is_clone = (strcmp(name, "clone") == 0);
        if (recv.kind == VAL_OBJECT) {
            Value copy = val_object(ev->arena, recv.obj->klass);
            for (IVarEntry *iv = recv.obj->ivars; iv; iv = iv->next)
                val_object_set_ivar(ev->arena, copy, iv->name, iv->val);
            copy.obj->native = recv.obj->native;
            if (is_clone && recv.obj->frozen) copy.obj->frozen = 1;
            return copy;
        }
        if (recv.kind == VAL_HASH) {
            Value copy = val_hash_new_with_defaults(ev->arena, recv.hash->default_value, recv.hash->default_proc);
            for (size_t i = 0; i < recv.hash->len; i++)
                val_hash_set(copy.hash, recv.hash->keys[i], recv.hash->vals[i]);
            return copy;
        }
        if (recv.kind == VAL_ARRAY) {
            Value copy = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++)
                val_array_push(&copy, recv.array->elems[i]);
            return copy;
        }
        /* String: dup returns unfrozen copy; clone preserves frozen */
        if (recv.kind == VAL_STRING) {
            Value copy = val_string(ev->arena, recv.sval);
            if (is_clone) copy.frozen = recv.frozen;
            /* else dup: unfrozen */
            return copy;
        }
        /* Other primitives: return self */
        return recv;
    }
    if (strcmp(name, "itself") == 0) return recv;
    if (recv.kind == VAL_HASH && strcmp(name, "to_hash") == 0) return recv;
    if (recv.kind == VAL_OBJECT && recv.obj->klass.kind == VAL_CLASS &&
        strcmp(recv.obj->klass.klass->name, "Thread::Mutex") == 0 &&
        strcmp(name, "synchronize") == 0) {
        if (!blk) return recv;
        return call_block(ev, env, *blk, NULL, 0, site);
    }
    if (strcmp(name, "instance_variable_get") == 0) {
        if (argc < 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        const char *raw = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!raw) return eval_raise_class(ev, site, "TypeError", "instance_variable_get requires a Symbol or String");
        const char *ivar = raw[0] == '@' ? raw + 1 : raw; /* strip leading @ */
        if (recv.kind == VAL_OBJECT) {
            Value v;
            return val_object_get_ivar(recv, ivar, &v) ? v : val_nil();
        }
        if (recv.kind == VAL_CLASS && recv.klass && recv.klass->class_env) {
            size_t nlen = strlen(ivar);
            char *key = arena_alloc(ev->arena, nlen + 2);
            key[0] = '@'; memcpy(key + 1, ivar, nlen + 1);
            Value v;
            return env_get(recv.klass->class_env, key, &v) ? v : val_nil();
        }
        return val_nil();
    }
    if (strcmp(name, "instance_variable_set") == 0) {
        if (argc < 2) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        const char *raw = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!raw) return eval_raise_class(ev, site, "TypeError", "instance_variable_set requires a Symbol or String");
        const char *ivar = raw[0] == '@' ? raw + 1 : raw;
        if (recv.kind == VAL_OBJECT) {
            if (recv.obj->frozen) {
                const char *kname = recv.obj->klass.kind == VAL_CLASS ? recv.obj->klass.klass->name : "Object";
                return eval_raise_class(ev, site, "FrozenError", "can't modify frozen %s: %s", kname, ivar);
            }
            val_object_set_ivar(ev->arena, recv, ivar, args[1]);
            return args[1];
        }
        if (recv.kind == VAL_CLASS && recv.klass && recv.klass->class_env) {
            size_t nlen = strlen(ivar);
            char *key = arena_alloc(ev->arena, nlen + 2);
            key[0] = '@'; memcpy(key + 1, ivar, nlen + 1);
            env_define(ev->arena, recv.klass->class_env, key, args[1]);
            return args[1];
        }
        return val_nil();
    }
    if (strcmp(name, "instance_variable_defined?") == 0) {
        if (argc < 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        const char *raw = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!raw) return val_false();
        const char *ivar = raw[0] == '@' ? raw + 1 : raw;
        if (recv.kind == VAL_OBJECT) { Value v; return val_bool(val_object_get_ivar(recv, ivar, &v)); }
        if (recv.kind == VAL_CLASS && recv.klass && recv.klass->class_env) {
            size_t nlen = strlen(ivar);
            char *key = arena_alloc(ev->arena, nlen + 2);
            key[0] = '@'; memcpy(key + 1, ivar, nlen + 1);
            Value v;
            return val_bool(env_get(recv.klass->class_env, key, &v));
        }
        return val_false();
    }
    if (strcmp(name, "instance_variables") == 0) {
        Value arr = val_array_new();
        if (recv.kind == VAL_OBJECT) {
            for (IVarEntry *iv = recv.obj->ivars; iv; iv = iv->next) {
                if (iv->name[0] != '_') { /* skip internal __ fields */
                    /* prepend @ to match Ruby convention */
                    size_t nlen = strlen(iv->name);
                    char *sym = arena_alloc(ev->arena, nlen + 2);
                    sym[0] = '@';
                    memcpy(sym + 1, iv->name, nlen + 1);
                    val_array_push(&arr, val_symbol(sym));
                }
            }
        }
        return arr;
    }
    if (strcmp(name, "singleton_class") == 0) {
        /* Return a synthetic singleton class object */
        Value sc_klass;
        if (!env_get(ev->top_env, "Class", &sc_klass)) sc_klass = val_class_of(ev, recv);
        /* For now return the actual class — proper singleton_class is complex */
        return val_class_of(ev, recv);
    }
    if (strcmp(name, "singleton_methods") == 0) {
        /* Return methods defined in the singleton env */
        Value arr = val_array_new();
        Env *singleton_env = value_singleton_env(recv);
        if (singleton_env) {
            for (EnvEntry *e = singleton_env->vars; e; e = e->next) {
                if (e->val.kind == VAL_METHOD)
                    val_array_push(&arr, val_symbol(e->name));
            }
        }
        return arr;
    }
    if (strcmp(name, "define_singleton_method") == 0) {
        if (argc < 1 || !blk) { return val_nil(); }
        const char *mname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!mname) return val_nil();
        Env **slot = value_singleton_env_slot(recv);
        if (slot) {
            if (!*slot) *slot = env_new(ev->arena, NULL, 1);
            env_define(ev->arena, *slot, mname, val_method(blk->block.block_node, blk->block.closure, METHOD_PUBLIC, blk->block.def_file));
        }
        return val_symbol(mname);
    }
    if (strcmp(name, "tap") == 0) {
        if (!blk) return eval_raise_class(ev, site, "LocalJumpError", "no block given");
        Value result = call_block(ev, env, *blk, &recv, 1, site);
        if (val_is_signal(result)) return result;
        return recv;
    }
    if (strcmp(name, "then") == 0 || strcmp(name, "yield_self") == 0) {
        if (!blk) return eval_raise_class(ev, site, "LocalJumpError", "no block given");
        return call_block(ev, env, *blk, &recv, 1, site);
    }
    if (strcmp(name, "instance_eval") == 0 ||
        strcmp(name, "class_eval") == 0 ||
        strcmp(name, "module_eval") == 0) {
        if (blk) {
            /* Block form: evaluate block with self = receiver */
            Env *ieval_env = env_new(ev->arena, blk->block.closure, 0);
            env_define(ev->arena, ieval_env, "self", recv);
            Value bargs[1] = { recv };
            Value ieval_block = *blk;
            ieval_block.block.closure = ieval_env;
            return call_block(ev, ieval_env, ieval_block, bargs, 1, site);
        }
        if (argc >= 1 && args[0].kind == VAL_STRING) {
            /* String form: eval string with self = receiver */
            Env *ieval_env = env_new(ev->arena, env, 0);
            env_define(ev->arena, ieval_env, "self", recv);
            const char *file = argc >= 2 && args[1].kind == VAL_STRING ? args[1].sval : ev->current_file;
            int64_t line = argc >= 3 && args[2].kind == VAL_INT ? args[2].ival : 1;
            return eval_string_in_context(ev, env, args[0].sval, ieval_env, file, line, site);
        }
        return val_nil();
    }
    if (strcmp(name, "instance_exec") == 0) {
        if (!blk) return eval_raise_class(ev, site, "LocalJumpError", "no block given (instance_exec)");
        /* Like instance_eval block form but passes args to the block instead of recv */
        Env *ieval_env = env_new(ev->arena, blk->block.closure, 0);
        env_define(ev->arena, ieval_env, "self", recv);
        Value ieval_block = *blk;
        ieval_block.block.closure = ieval_env;
        return call_block(ev, ieval_env, ieval_block, args, argc, site);
    }
    if (strcmp(name, "object_id") == 0) {
        if (recv.kind == VAL_OBJECT) return val_int((int64_t)(uintptr_t)recv.obj);
        if (recv.kind == VAL_INT) return val_int(recv.ival * 2 + 1);
        return val_int((int64_t)(uintptr_t)recv.sval);
    }
    if (strcmp(name, "==") == 0) {
        if (argc < 1) return val_false();
        if (recv.kind == VAL_OBJECT) {
            Value m; RubyClass *owner;
            if (ruby_class_find_instance_method(recv.obj->klass.klass, "==", &m, &owner))
                return call_method_value(ev, env, recv, m, owner, "==", args, argc, blk, site);
            Value disp_out;
            if (dispatch_object(ev, env, recv, "==", args, argc, blk, site, &disp_out, public_only, explicit_receiver))
                return disp_out;
        }
        return val_bool(val_equal(recv, args[0]));
    }
    if (strcmp(name, "!=") == 0) {
        if (argc < 1) return val_true();
        if (recv.kind == VAL_OBJECT) {
            Value m; RubyClass *owner;
            if (ruby_class_find_instance_method(recv.obj->klass.klass, "!=", &m, &owner))
                return call_method_value(ev, env, recv, m, owner, "!=", args, argc, blk, site);
            Value disp_out;
            if (dispatch_object(ev, env, recv, "!=", args, argc, blk, site, &disp_out, public_only, explicit_receiver))
                return disp_out;
            /* Fall back to negating == rather than pointer identity */
            Value eq = dispatch_method(ev, env, recv, "==", args, argc, blk, site, public_only, explicit_receiver);
            if (!val_is_signal(eq)) return val_bool(!val_truthy(eq));
            ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
        }
        return val_bool(!val_equal(recv, args[0]));
    }
    if (strcmp(name, "===") == 0 && recv.kind != VAL_CLASS && recv.kind != VAL_RANGE) {
        if (argc < 1) return val_false();
        if (recv.kind == VAL_OBJECT) {
            Value m; RubyClass *owner;
            if (ruby_class_find_instance_method(recv.obj->klass.klass, "===", &m, &owner))
                return call_method_value(ev, env, recv, m, owner, "===", args, argc, blk, site);
            Value disp_out;
            if (dispatch_object(ev, env, recv, "===", args, argc, blk, site, &disp_out, public_only, explicit_receiver))
                return disp_out;
        }
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

    /* Object#methods / public_methods / private_methods / protected_methods */
    if (strcmp(name, "methods") == 0 || strcmp(name, "public_methods") == 0 ||
        strcmp(name, "private_methods") == 0 || strcmp(name, "protected_methods") == 0) {
        int include_super = (argc == 0) || val_truthy(args[0]);
        int vis_mask;
        if (strcmp(name, "public_methods") == 0) vis_mask = 1;
        else if (strcmp(name, "private_methods") == 0) vis_mask = 4;
        else if (strcmp(name, "protected_methods") == 0) vis_mask = 2;
        else vis_mask = 3; /* methods: public + protected */
        Value arr = val_array_new();
        if (singleton_env) {
            for (EnvEntry *e = singleton_env->vars; e; e = e->next) {
                if (e->val.kind != VAL_METHOD) continue;
                MethodVisibility vis = e->val.method.visibility;
                int match = ((vis_mask & 1) && vis == METHOD_PUBLIC) ||
                            ((vis_mask & 2) && vis == METHOD_PROTECTED) ||
                            ((vis_mask & 4) && vis == METHOD_PRIVATE);
                if (match && !sym_in_array(&arr, e->name))
                    val_array_push(&arr, val_symbol(e->name));
            }
        }
        /* For VAL_CLASS, class methods live in klass->class_env keyed as "self.name" */
        if (recv.kind == VAL_CLASS && recv.klass) {
            RubyClass *kc = recv.klass;
            while (kc) {
                Env *ce = kc->class_env;
                for (EnvEntry *e = ce ? ce->vars : NULL; e; e = e->next) {
                    if (e->val.kind != VAL_METHOD) continue;
                    /* Class method entries are keyed "self.name" */
                    const char *mname = e->name;
                    if (strncmp(mname, "self.", 5) == 0) mname += 5;
                    else continue; /* skip instance method entries */
                    MethodVisibility vis = e->val.method.visibility;
                    int match = ((vis_mask & 1) && vis == METHOD_PUBLIC) ||
                                ((vis_mask & 2) && vis == METHOD_PROTECTED) ||
                                ((vis_mask & 4) && vis == METHOD_PRIVATE);
                    if (match && !sym_in_array(&arr, mname))
                        val_array_push(&arr, val_symbol(mname));
                }
                if (!include_super) break;
                kc = (kc->superclass.kind == VAL_CLASS) ? kc->superclass.klass : NULL;
            }
        }
        if (recv.kind == VAL_OBJECT) {
            if (recv.obj->klass.kind == VAL_CLASS) {
                RubyClass *k = recv.obj->klass.klass;
                if (include_super) {
                    RubyClass *visited[256]; int nv = 0;
                    collect_all_instance_methods(k, &arr, vis_mask, visited, &nv);
                } else {
                    collect_own_instance_methods(k->class_env, &arr, vis_mask);
                }
            }
        }
        /* Add hardcoded built-in public methods (from Object/BasicObject) when inheriting */
        if (include_super && (vis_mask & 1)) {
            static const char *obj_builtins[] = {
                "class", "nil?", "is_a?", "kind_of?", "instance_of?", "respond_to?",
                "equal?", "==", "!=", "freeze", "frozen?", "itself", "tap", "then", "yield_self", "object_id",
            "instance_eval", "class_eval", "module_eval",
                "send", "__send__", "public_send", "extend", "methods", "public_methods",
                "private_methods", "protected_methods", "method", "inspect", "to_s", NULL
            };
            for (int i = 0; obj_builtins[i]; i++) {
                if (!sym_in_array(&arr, obj_builtins[i]))
                    val_array_push(&arr, val_symbol(obj_builtins[i]));
            }
        }
        return arr;
    }

    /* Object#method */
    if (strcmp(name, "method") == 0) {
        if (argc < 1) return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        const char *mname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!mname) return eval_raise_class(ev, site, "TypeError", "expected Symbol or String");
        /* val_responds_to now includes kernel functions — no longer raises for them */
        if (!val_responds_to(ev, recv, mname, 1))
            return eval_raise_class(ev, site, "NameError", "undefined method '%s' for class '%s'", mname, prim_class_name(recv));
        Value method_val = val_nil();
        if (singleton_env) {
            Value m;
            if (env_get(singleton_env, mname, &m) && m.kind == VAL_METHOD)
                method_val = m;
        }
        if (recv.kind == VAL_OBJECT) {
            if (method_val.kind == VAL_NIL && recv.obj->klass.kind == VAL_CLASS) {
                Value m; RubyClass *owner = NULL;
                if (ruby_class_find_instance_method(recv.obj->klass.klass, mname, &m, &owner))
                    method_val = m;
            }
        }
        Value m_klass;
        if (!env_get(ev->top_env, "Method", &m_klass) || m_klass.kind != VAL_CLASS)
            return val_nil();
        Value obj = val_object(ev->arena, m_klass);
        val_object_set_ivar(ev->arena, obj, "__receiver__", recv);
        val_object_set_ivar(ev->arena, obj, "__method_name__", val_string(ev->arena, mname));
        val_object_set_ivar(ev->arena, obj, "__method__", method_val);
        return obj;
    }

    if (recv.kind == VAL_BLOCK) {
        if (strcmp(name, "call") == 0 || strcmp(name, "[]") == 0)
            return call_block(ev, env, recv, args, argc, site);
        if (strcmp(name, "lambda?") == 0)
            return val_bool(recv.block.is_lambda);
        if (strcmp(name, "arity") == 0)
            return val_int(proc_arity(recv.block.block_node->block.params, recv.block.is_lambda));
        if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0)
            return val_string(ev->arena, recv.block.is_lambda ? "#<Proc:lambda>" : "#<Proc>");
        if (strcmp(name, "parameters") == 0) {
            /* Return parameter info from the block's params */
            Value arr = val_array_new();
            NodeList *params = recv.block.block_node ? recv.block.block_node->block.params : NULL;
            for (NodeList *pl = params; pl; pl = pl->next) {
                if (!pl->node || pl->node->kind != NODE_PARAM) continue;
                Value pair = val_array_new();
                const char *ptype = pl->node->param.splat ? "rest" :
                                    pl->node->param.block_param ? "block" :
                                    pl->node->param.keyword_splat ? "keyrest" :
                                    pl->node->param.keyword_param ? "key" :
                                    pl->node->param.default_val ? "opt" : "req";
                val_array_push(&pair, val_symbol(ptype));
                if (pl->node->param.name)
                    val_array_push(&pair, val_symbol(pl->node->param.name));
                val_array_push(&arr, pair);
            }
            return arr;
        }
        /* curry is implemented in the Ruby prelude (eval_support.c) */
    }

    if (strcmp(name, "inspect") == 0 && recv.kind != VAL_OBJECT && recv.kind != VAL_CLASS)
        return val_string(ev->arena, val_inspect(ev->arena, recv));

    if (dispatch_integer(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (dispatch_float(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (dispatch_string(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    /* Symbol methods — delegate string-like operations, return symbol for case/upcase/downcase */
    if (recv.kind == VAL_SYMBOL) {
        if (strcmp(name, "upcase") == 0 || strcmp(name, "downcase") == 0 ||
            strcmp(name, "capitalize") == 0 || strcmp(name, "swapcase") == 0) {
            Value as_str = val_string(ev->arena, recv.sval);
            Value r = dispatch_method(ev, env, as_str, name, args, argc, blk, site, 0, 1);
            if (!val_is_signal(r) && r.kind == VAL_STRING)
                return val_symbol(r.sval);
            return r;
        }
        if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0 ||
            strcmp(name, "empty?") == 0 || strcmp(name, "match") == 0 ||
            strcmp(name, "match?") == 0 || strcmp(name, "start_with?") == 0 ||
            strcmp(name, "end_with?") == 0 || strcmp(name, "include?") == 0 ||
            strcmp(name, "encoding") == 0 || strcmp(name, "bytes") == 0 ||
            strcmp(name, "chars") == 0 || strcmp(name, "succ") == 0 ||
            strcmp(name, "next") == 0 || strcmp(name, "[]") == 0) {
            Value as_str = val_string(ev->arena, recv.sval);
            return dispatch_method(ev, env, as_str, name, args, argc, blk, site, 0, 1);
        }
        if (strcmp(name, "<=>") == 0 || strcmp(name, "==") == 0) {
            if (argc < 1) return val_nil();
            if (args[0].kind != VAL_SYMBOL) return strcmp(name, "<=>") == 0 ? val_nil() : val_false();
            int cmp = strcmp(recv.sval, args[0].sval);
            if (strcmp(name, "<=>") == 0) return val_int(cmp < 0 ? -1 : cmp > 0 ? 1 : 0);
            return val_bool(cmp == 0);
        }
    }
    if (recv.kind == VAL_ARRAY && dispatch_array(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (recv.kind == VAL_HASH && dispatch_hash(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    if (recv.kind == VAL_RANGE && dispatch_range(ev, env, recv, name, args, argc, blk, site, &out)) return out;
    /* Nil receiver calling kernel function (e.g. Method#call on a Kernel method) */
    if (recv.kind == VAL_NIL) {
        static const char *kern_nil[] = {
            "puts", "print", "p", "pp", "warn", "require", "require_relative", "raise",
            "lambda", "proc", "loop", "rand", "srand", "open", "exit", "exit!", "abort", "format", "sprintf", "printf",
            "sleep", "catch", "throw", "trap", "at_exit", "`",
            "system", "spawn", "wait", "waitpid", "load", NULL
        };
        for (int ki = 0; kern_nil[ki]; ki++) {
            if (strcmp(name, kern_nil[ki]) == 0)
                return builtin_kernel(ev, env, name, args, argc, blk, site);
        }
    }
    if (recv.kind == VAL_NIL && dispatch_nil(ev, recv, name, site, &out)) return out;
    if (dispatch_bool(ev, recv, name, site, &out)) return out;

    /* Boolean comparison methods that need args */
    if (recv.kind == VAL_BOOL) {
        if (strcmp(name, "==") == 0 || strcmp(name, "equal?") == 0) {
            return val_bool(argc > 0 && args[0].kind == VAL_BOOL && args[0].bval == recv.bval);
        }
        if (strcmp(name, "<=>") == 0) {
            if (argc > 0 && args[0].kind == VAL_BOOL) {
                int a = recv.bval ? 1 : 0, b = args[0].bval ? 1 : 0;
                return val_int(a < b ? -1 : a > b ? 1 : 0);
            }
            return val_nil();
        }
        if (strcmp(name, "&") == 0) return val_bool(recv.bval && argc > 0 && val_truthy(args[0]));
        if (strcmp(name, "|") == 0) return val_bool(recv.bval || (argc > 0 && val_truthy(args[0])));
        if (strcmp(name, "^") == 0) return val_bool(recv.bval ^ (argc > 0 && val_truthy(args[0])));
    }

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
                if (method.method.visibility == METHOD_PROTECTED)
                    return eval_raise_class(ev, site, "NoMethodError",
                                            "protected method '%s' called for an instance of %s",
                                            name, value_class_name(ev, recv));
            }
        }
    }

    if (dispatch_class(ev, env, recv, name, args, argc, blk, site, &out, public_only, explicit_receiver)) return out;
    if (dispatch_object(ev, env, recv, name, args, argc, blk, site, &out, public_only, explicit_receiver)) return out;

    if (strcmp(name, "method_missing") != 0)
        return dispatch_method_missing(ev, env, recv, name, args, argc, blk, site);
    return eval_raise_class(ev, site, "NoMethodError", "undefined method '%s' for %s", name, prim_class_name(recv));
}

Value eval_binop(Eval *ev, Env *env, Node *node) {
    const char *op = node->binop.op;

    if (strcmp(op, "&&") == 0 || strcmp(op, "and") == 0) {
        Value left = eval_node(ev, env, node->binop.left);
        CHECK(left);
        if (!val_truthy(left)) return left;
        return eval_node(ev, env, node->binop.right);
    }
    if (strcmp(op, "||") == 0 || strcmp(op, "or") == 0) {
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
        Value rhs = right;
        if (rhs.kind != VAL_STRING) {
            /* Try implicit to_str coercion */
            Value coerced = dispatch_method(ev, env, rhs, "to_str", NULL, 0, NULL, node, 0, -1);
            if (!val_is_signal(coerced) && coerced.kind == VAL_STRING) {
                rhs = coerced;
            } else {
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
                return eval_raise_class(ev, node, "TypeError", "no implicit conversion of %s into String",
                                        value_class_name(ev, right));
            }
        }
        size_t la = strlen(left.sval), lb = strlen(rhs.sval);
        char *buf = arena_alloc(ev->arena, la + lb + 1);
        memcpy(buf, left.sval, la);
        memcpy(buf + la, rhs.sval, lb + 1);
        return val_string(ev->arena, buf);
    }

    if (strcmp(op, "+") == 0 && left.kind == VAL_ARRAY && right.kind == VAL_ARRAY) {
        Value result = val_array_new();
        for (size_t i = 0; i < left.array->len; i++) val_array_push(&result, left.array->elems[i]);
        for (size_t i = 0; i < right.array->len; i++) val_array_push(&result, right.array->elems[i]);
        return result;
    }
    if (strcmp(op, "<<") == 0 && left.kind == VAL_ARRAY) {
        if (left.array->frozen)
            return eval_raise_class(ev, node, "FrozenError", "can't modify frozen Array");
        val_array_push(&left, right);
        return left;
    }

    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
        Value argv[1];
        argv[0] = right;
        return dispatch_method(ev, env, left, op, argv, 1, NULL, node, 0, 1);
    }

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
                /* Ruby modulo: result has same sign as divisor */
                int64_t r = left.ival % right.ival;
                if (r != 0 && (r < 0) != (right.ival < 0)) r += right.ival;
                return val_int(r);
            }
            /* Ruby fmod: result has same sign as divisor */
            double r = fmod(lf, rf);
            if (r != 0.0 && (r < 0) != (rf < 0)) r += rf;
            return val_float(r);
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
    if (!ev->errored && !val_is_signal(op_result)) {
        /* For mutating operators (<<, concat) on strings, update the receiver variable */
        if (op_result.kind == VAL_STRING &&
            (strcmp(op, "<<") == 0 || strcmp(op, "concat") == 0) &&
            node->binop.left) {
            Node *ln = node->binop.left;
            if (ln->kind == NODE_LVAR)
                env_set(ev->arena, env, ln->sval, op_result);
            else if (ln->kind == NODE_IVAR) {
                Value self;
                if (env_get(env, "self", &self) && self.kind == VAL_OBJECT)
                    val_object_set_ivar(ev->arena, self, ln->sval, op_result);
            } else if (ln->kind == NODE_GVAR)
                global_set(ev->arena, &ev->globals, ln->sval, op_result);
        }
        return op_result;
    }
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
    return val_lambda(block_node, ev->top_env);
}

Value eval_call(Eval *ev, Env *env, Node *node) {
    if (ev->call_depth > EVAL_MAX_DEPTH)
        return eval_raise_class(ev, node, "SystemStackError", "stack level too deep");

    Value args[64];
    int argc = 0;
    Node *block_pass_node = NULL;
    Value kwargs = val_nil();
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
        if (l->node->kind == NODE_HASH && l->node->hash.keyword_style) {
            Value khash = eval_node(ev, env, l->node);
            CHECK(khash);
            if (khash.kind == VAL_HASH) {
                if (kwargs.kind != VAL_HASH) kwargs = val_hash_new(ev->arena);
                for (size_t i = 0; i < khash.hash->len; i++)
                    val_hash_set(kwargs.hash, khash.hash->keys[i], khash.hash->vals[i]);
            } else {
                args[argc++] = khash;
            }
            continue;
        }
        if (l->node->kind == NODE_UNOP && strcmp(l->node->unop.op, "**") == 0) {
            Value dsplat = eval_node(ev, env, l->node->unop.operand);
            CHECK(dsplat);
            if (dsplat.kind == VAL_HASH) {
                if (kwargs.kind != VAL_HASH) kwargs = val_hash_new(ev->arena);
                for (size_t i = 0; i < dsplat.hash->len; i++)
                    val_hash_set(kwargs.hash, dsplat.hash->keys[i], dsplat.hash->vals[i]);
            } else if (argc < 64) {
                args[argc++] = dsplat;
            }
            continue;
        }
        Value a = eval_node(ev, env, l->node);
        CHECK(a);
        args[argc++] = a;
    }
    if (kwargs.kind == VAL_HASH && argc < 64)
        args[argc++] = kwargs;

    Value blk_val;
    Value *blk = NULL;
    if (node->call.block) {
        blk_val = val_block(node->call.block, env);
        blk_val.block.def_file = ev->current_file;
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
        } else if (bp.kind == VAL_OBJECT && bp.obj->klass.kind == VAL_CLASS &&
                   strcmp(bp.obj->klass.klass->name, "Method") == 0) {
            Value recv_iv, mname_iv;
            if (val_object_get_ivar(bp, "__receiver__", &recv_iv) &&
                val_object_get_ivar(bp, "__method_name__", &mname_iv)) {
                blk_val = make_bound_method_proc(ev, recv_iv, mname_iv.sval);
                blk = &blk_val;
            }
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
            "puts", "print", "p", "pp", "warn", "Integer", "Float", "String", "Array", "Hash", "format", "sprintf", "printf", "raise", "proc", "lambda", "loop", "rand", "srand", "open", "exit", "exit!", "abort", "include", "prepend", "extend",
            "require", "require_relative", "public", "private", "protected",
            "private_class_method", "public_class_method", "protected_class_method",
            "attr_reader", "attr_writer", "attr_accessor", "alias_method", "module_function", "autoload",
            "deprecate_constant", "private_constant", "public_constant",
            "__dir__", "__method__", "__FILE__", "__LINE__", "__callee__", "binding", "eval", "`",
            "trap", "at_exit", "sleep", "catch", "throw", "method", "caller", "caller_locations",
            "system", "spawn", "wait", "waitpid", "load", NULL
        };
        for (int i = 0; kernel_names[i]; i++) {
            if (strcmp(name, kernel_names[i]) == 0)
                return builtin_kernel(ev, env, name, args, argc, blk, node);
        }

        Value self;
        if (!env_get(env, "self", &self)) self = val_nil();

        /* Check if self has an instance method with this name — it takes
           priority over top-level defs (e.g. instance_eval DSL patterns). */
        if (self.kind == VAL_OBJECT && self.obj->klass.kind == VAL_CLASS) {
            Value m; RubyClass *owner;
            if (ruby_class_find_instance_method(self.obj->klass.klass, name, &m, &owner))
                return dispatch_method(ev, env, self, name, args, argc, blk, node, 0, 0);
        }

        Value fn;
        /* Check kernel-mangled name first (def Foo() when Foo is also a class constant) */
        {
            size_t nlen = strlen(name);
            char *mkey = arena_alloc(ev->arena, nlen + 9);
            memcpy(mkey, "__kern__", 8);
            memcpy(mkey + 8, name, nlen + 1);
            if (env_get(ev->top_env, mkey, &fn) && fn.kind == VAL_METHOD)
                goto call_method;
        }
        if (env_get(env, name, &fn) && fn.kind == VAL_METHOD)
            goto call_method;

        if (self.kind != VAL_NIL) {
            return dispatch_method(ev, env, self, name, args, argc, blk, node, 0, 0);
        }

        /*
         * Fallbacks below are retained for environments that somehow lack self,
         * but ordinary bare calls should already have returned through
         * dispatch_method(self, ...).
         */
        if (env_get(env, "self", &self) && self.kind == VAL_OBJECT) {
            if (self.obj->singleton_env && env_get(self.obj->singleton_env, name, &fn) && fn.kind == VAL_METHOD) {
                Env *method_env = env_new(ev->arena, fn.method.closure, 1);
                env_set(ev->arena, method_env, "self", self);
                env_set(ev->arena, method_env, "__method__", val_symbol(name));
                Value klass_val = self.obj->klass;
                env_set(ev->arena, method_env, "__class__", klass_val);
                if (blk) method_env->block_arg = blk;
                bind_params(ev, method_env, fn.method.def_node->def.params, args, argc);
                if (ev->exception_class != NULL) return val_exception();
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
                if (ev->exception_class != NULL) return val_exception();
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
                    if (ev->exception_class != NULL) return val_exception();
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
        env_set(ev->arena, frame, "__method__", val_symbol(name));
        if (blk) frame->block_arg = blk;

        bind_params(ev, frame, def->def.params, args, argc);
        if (ev->exception_class != NULL) return val_exception();
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
    if (node->call.safe_nav && recv.kind == VAL_NIL)
        return val_nil();

    if (recv.kind == VAL_ARRAY) {
        if (strcmp(node->call.method, "[]") == 0) {
            if (argc < 1) return eval_raise_class(ev, node, "ArgumentError", "wrong number of args for []");
            if (args[0].kind == VAL_RANGE) {
                RubyRange *r = args[0].range;
                int64_t begin = 0;
                int64_t end = (int64_t)recv.array->len;
                if (r->begin_val.kind == VAL_INT) {
                    begin = r->begin_val.ival;
                    if (begin < 0) begin += (int64_t)recv.array->len;
                } else if (r->begin_val.kind != VAL_NIL) {
                    return val_nil();
                }
                if (r->end_val.kind == VAL_INT) {
                    end = r->end_val.ival;
                    if (end < 0) end += (int64_t)recv.array->len;
                    if (!r->exclusive) end++;
                } else if (r->end_val.kind != VAL_NIL) {
                    return val_nil();
                }
                if (begin < 0) begin = 0;
                if (end < begin) end = begin;
                if ((size_t)begin > recv.array->len) return val_nil();
                if ((size_t)end > recv.array->len) end = (int64_t)recv.array->len;
                Value slice = val_array_new();
                for (int64_t i = begin; i < end; i++)
                    val_array_push(&slice, recv.array->elems[i]);
                return slice;
            }
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

    Value result = dispatch_method(ev, env, recv, node->call.method, args, argc, blk, node, 0, 1);

    /* For mutating methods on LVAR/IVAR/GVAR receivers, update the binding. */
    if (!val_is_signal(result) &&
        (result.kind == VAL_STRING || result.kind == VAL_ARRAY || result.kind == VAL_HASH) &&
        (strcmp(node->call.method, "<<") == 0 ||
         strcmp(node->call.method, "concat") == 0 ||
         strcmp(node->call.method, "replace") == 0 ||
         strcmp(node->call.method, "prepend") == 0 ||
         strcmp(node->call.method, "insert") == 0 ||
         strcmp(node->call.method, "force_encoding") == 0 ||
         strcmp(node->call.method, "freeze") == 0 ||
         strcmp(node->call.method, "upcase!") == 0 ||
         strcmp(node->call.method, "downcase!") == 0 ||
         strcmp(node->call.method, "strip!") == 0 ||
         strcmp(node->call.method, "lstrip!") == 0 ||
         strcmp(node->call.method, "rstrip!") == 0 ||
         strcmp(node->call.method, "chomp!") == 0 ||
         strcmp(node->call.method, "chop!") == 0 ||
         strcmp(node->call.method, "reverse!") == 0 ||
         strcmp(node->call.method, "capitalize!") == 0 ||
         strcmp(node->call.method, "swapcase!") == 0 ||
         strcmp(node->call.method, "gsub!") == 0 ||
         strcmp(node->call.method, "sub!") == 0) &&
        node->call.recv) {
        Node *recv_node = node->call.recv;
        if (recv_node->kind == NODE_LVAR)
            env_set(ev->arena, env, recv_node->sval, result);
        else if (recv_node->kind == NODE_IVAR) {
            Value self;
            if (env_get(env, "self", &self) && self.kind == VAL_OBJECT)
                val_object_set_ivar(ev->arena, self, recv_node->sval, result);
        } else if (recv_node->kind == NODE_GVAR)
            global_set(ev->arena, &ev->globals, recv_node->sval, result);
    }

    return result;
}
