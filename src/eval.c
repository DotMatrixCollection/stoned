#include "eval_internal.h"
#include "parser.h"
#include "sema.h"
#include "version.h"

#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#define CHECK(v) do { if (ev->errored || val_is_signal(v)) return (v); } while(0)

extern char **environ;

static Env **value_singleton_env_slot(Value recv) {
    switch (recv.kind) {
        case VAL_OBJECT: return &recv.obj->singleton_env;
        case VAL_ARRAY:  return &recv.array->singleton_env;
        case VAL_HASH:   return &recv.hash->singleton_env;
        default:         return NULL;
    }
}

static void assign_lvar(Eval *ev, Env *env, const char *name, Value val) {
    if (!env_update(env, name, val))
        env_set(ev->arena, env, name, val);
}

static Value lookup_const_head(Eval *ev, Env *env, const char *name) {
    Value v;
    Value current_class;
    if (env && env_get(env, "__class__", &current_class) && current_class.kind == VAL_CLASS) {
        Value scope = current_class;
        while (scope.kind == VAL_CLASS) {
            for (RubyClass *klass = scope.klass; klass; klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL) {
                if (klass->class_env && env_get(klass->class_env, name, &v))
                    return v;
            }
            const char *scope_name = scope.klass->name;
            const char *last = scope_name ? strstr(scope_name, "::") : NULL;
            const char *scan = last;
            const char *final = NULL;
            while (scan) {
                final = scan;
                scan = strstr(scan + 2, "::");
            }
            if (!final)
                break;
            size_t parent_len = (size_t)(final - scope_name);
            char *parent_name = arena_alloc(ev->arena, parent_len + 1);
            memcpy(parent_name, scope_name, parent_len);
            parent_name[parent_len] = '\0';
            scope = lookup_const_head(ev, NULL, parent_name);
            if (scope.kind != VAL_CLASS)
                break;
        }
    }
    if (env_get(ev->top_env, name, &v))
        return v;
    /* Fallback: search top_env for qualified name ending in ::name
       (handles constants defined in class bodies of modules not yet in scope) */
    {
        size_t nlen = strlen(name);
        size_t sep_len = nlen + 2; /* :: + name */
        for (EnvEntry *e = ev->top_env->vars; e; e = e->next) {
            size_t elen = e->name ? strlen(e->name) : 0;
            if (elen > sep_len &&
                e->name[elen - sep_len] == ':' &&
                e->name[elen - sep_len + 1] == ':' &&
                strcmp(e->name + elen - nlen, name) == 0) {
                /* Check if this belongs to our class scope */
                Value current_class;
                if (env && env_get(env, "__class__", &current_class) &&
                    current_class.kind == VAL_CLASS && current_class.klass->name) {
                    /* qualified name should start with our class name */
                    size_t clen = strlen(current_class.klass->name);
                    if (elen >= clen + 2 + nlen &&
                        strncmp(e->name, current_class.klass->name, clen) == 0)
                        return e->val;
                }
            }
        }
    }
    return val_nil();
}

static int path_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

static const char *path_join2(Arena *arena, const char *left, const char *right) {
    size_t left_len = strlen(left);
    size_t right_len = strlen(right);
    int needs_slash = left_len > 0 && left[left_len - 1] != '/';
    char *joined = arena_alloc(arena, left_len + right_len + (size_t)needs_slash + 1);
    memcpy(joined, left, left_len);
    if (needs_slash) joined[left_len++] = '/';
    memcpy(joined + left_len, right, right_len + 1);
    return joined;
}

static const char *path_dirname(Arena *arena, const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) return ".";
    if (slash == path) return "/";
    return val_string_n(arena, path, (size_t)(slash - path)).sval;
}

static const char *runtime_root_from_exec(Arena *arena, const char *exec_path) {
    if (!exec_path || !*exec_path) return NULL;

    const char *exec_dir = path_dirname(arena, exec_path);
    if (path_exists(path_join2(arena, exec_dir, "rbconfig.rb")))
        return exec_dir;

    const char *prefix = path_dirname(arena, exec_dir);
    const char *ruby_root = path_join2(arena, path_join2(arena, prefix, "lib"), "ruby");
    if (path_exists(path_join2(arena, path_join2(arena, ruby_root, STONED_RUBY_VERSION), "rbconfig.rb")))
        return prefix;

    return NULL;
}

static Value lookup_const_path(Eval *ev, Env *env, const char *name) {
    const char *cursor = name;
    if (cursor[0] == ':' && cursor[1] == ':')
        cursor += 2;

    const char *sep = strstr(cursor, "::");
    if (!sep)
        return lookup_const_head(ev, env, cursor);

    size_t head_len = (size_t)(sep - cursor);
    char head[256];
    if (head_len >= sizeof(head))
        return val_nil();
    memcpy(head, cursor, head_len);
    head[head_len] = '\0';

    Value current = lookup_const_head(ev, env, head);
    if (current.kind == VAL_NIL)
        return val_nil();

    cursor = sep + 2;
    while (1) {
        sep = strstr(cursor, "::");
        size_t part_len = sep ? (size_t)(sep - cursor) : strlen(cursor);
        char part[256];
        if (part_len >= sizeof(part))
            return val_nil();
        memcpy(part, cursor, part_len);
        part[part_len] = '\0';
        if (current.kind != VAL_CLASS || !current.klass->class_env)
            return val_nil();
        {
            Value found = val_nil();
            int found_any = 0;
            for (RubyClass *klass = current.klass; klass; klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL) {
                if (klass->class_env && env_get(klass->class_env, part, &found)) {
                    found_any = 1;
                    break;
                }
            }
            if (!found_any)
                return val_nil();
            current = found;
        }
        if (!sep) break;
        cursor = sep + 2;
    }

    return current;
}

static Value lookup_const_on_class(Value current, const char *part) {
    Value found = val_nil();
    if (current.kind != VAL_CLASS)
        return val_nil();
    for (RubyClass *klass = current.klass; klass; klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL) {
        if (klass->class_env && env_get(klass->class_env, part, &found))
            return found;
    }
    return val_nil();
}

static void split_const_path(Arena *arena, const char *full_name, const char **parent_out, const char **leaf_out) {
    const char *cursor = full_name;
    if (cursor[0] == ':' && cursor[1] == ':')
        cursor += 2;
    const char *last = NULL;
    for (const char *p = cursor; (p = strstr(p, "::")); p += 2)
        last = p;
    if (!last) {
        *parent_out = NULL;
        *leaf_out = cursor;
        return;
    }
    *leaf_out = last + 2;
    size_t parent_len = (size_t)(last - cursor);
    char *parent = arena_alloc(arena, parent_len + 1);
    memcpy(parent, cursor, parent_len);
    parent[parent_len] = '\0';
    *parent_out = parent;
}

static const char *qualify_const_name(Arena *arena, const char *prefix, const char *leaf) {
    if (!prefix || !prefix[0]) return leaf;
    size_t plen = strlen(prefix);
    size_t llen = strlen(leaf);
    char *full = arena_alloc(arena, plen + 2 + llen + 1);
    memcpy(full, prefix, plen);
    memcpy(full + plen, "::", 2);
    memcpy(full + plen + 2, leaf, llen + 1);
    return full;
}

static int validate_special_global_assignment(Eval *ev, Node *target, Value val) {
    if (!target || target->kind != NODE_GVAR) return 1;
    if ((strcmp(target->sval, "stdout") == 0 || strcmp(target->sval, "stderr") == 0) &&
        !(value_is_a_named_class(ev, val, "IO") || value_is_a_named_class(ev, val, "File") ||
          val_responds_to(ev, val, "write", 1))) {
        eval_raise_class(ev, target, "TypeError", "$%s must have write method, %s given",
                         target->sval, value_class_name(ev, val));
        return 0;
    }
    return 1;
}

static void assign_target(Eval *ev, Env *env, Node *target, Value val) {
    if (!target) return;

    if (target->kind == NODE_ARRAY) {
        size_t len = 0;
        size_t splat_index = (size_t)-1;
        for (NodeList *l = target->array.elements; l; l = l->next, len++) {
            if (l->node && l->node->kind == NODE_PARAM && l->node->param.splat)
                splat_index = len;
        }

        size_t idx = 0;
        for (NodeList *l = target->array.elements; l; l = l->next, idx++) {
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
                assign_target(ev, env, l->node, rest);
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
            assign_target(ev, env, l->node, elem);
        }
        return;
    }

    if (target->kind == NODE_PARAM && target->param.name) {
        assign_lvar(ev, env, target->param.name, val);
        return;
    }

    if (target->kind == NODE_LVAR) {
        assign_lvar(ev, env, target->sval, val);
    } else if (target->kind == NODE_IVAR) {
        Value self;
        if (env_get(env, "self", &self)) {
            if (self.kind == VAL_OBJECT) {
                if (self.obj->frozen) {
                    const char *kname = self.obj->klass.kind == VAL_CLASS ? self.obj->klass.klass->name : "Object";
                    eval_raise_class(ev, target, "FrozenError", "can't modify frozen %s", kname);
                    return;
                }
                val_object_set_ivar(ev->arena, self, target->sval, val);
            } else if (self.kind == VAL_CLASS && self.klass && self.klass->class_env) {
                /* Class instance variable: store in class_env with @ prefix */
                size_t nlen = strlen(target->sval);
                char *key = arena_alloc(ev->arena, nlen + 2);
                key[0] = '@'; memcpy(key + 1, target->sval, nlen + 1);
                env_define(ev->arena, self.klass->class_env, key, val);
            } else {
                global_set(ev->arena, &ev->globals, target->sval, val);
            }
        } else {
            global_set(ev->arena, &ev->globals, target->sval, val);
        }
    } else if (target->kind == NODE_GVAR) {
        if (!validate_special_global_assignment(ev, target, val)) return;
        global_set(ev->arena, &ev->globals, target->sval, val);
    } else if (target->kind == NODE_CVAR) {
        Value self = val_nil();
        env_get(env, "self", &self);
        RubyClass *klass = NULL;
        if (self.kind == VAL_CLASS) klass = self.klass;
        else if (self.kind == VAL_OBJECT && self.obj->klass.kind == VAL_CLASS)
            klass = self.obj->klass.klass;
        /* Write to the class that already owns this cvar, else current class */
        RubyClass *owner = klass;
        for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
            Value existing;
            if (k->class_env && env_get(k->class_env, target->sval, &existing)) { owner = k; break; }
        }
        if (owner && owner->class_env)
            env_define(ev->arena, owner->class_env, target->sval, val);
        else
            global_set(ev->arena, &ev->globals, target->sval, val);
    } else if (target->kind == NODE_CONST) {
        const char *parent_name = NULL;
        const char *leaf_name = NULL;
        split_const_path(ev->arena, target->sval, &parent_name, &leaf_name);
        if (parent_name) {
            Value parent = lookup_const_path(ev, env, parent_name);
            if (parent.kind == VAL_CLASS && parent.klass->class_env) {
                env_define(ev->arena, parent.klass->class_env, leaf_name, val);
                env_define(ev->arena, ev->top_env, target->sval, val);
                return;
            }
        }
        Value self = val_nil();
        if (env_get(env, "self", &self) && self.kind == VAL_CLASS && self.klass->class_env == env) {
            env_define(ev->arena, self.klass->class_env, target->sval, val);
            const char *qualified = qualify_const_name(ev->arena, self.klass->name, target->sval);
            env_define(ev->arena, ev->top_env, qualified, val);
        } else {
            env_define(ev->arena, ev->top_env, target->sval, val);
        }
        /* If a class/module is assigned to a constant and has an anonymous name, rename it */
        if (val.kind == VAL_CLASS && val.klass) {
            const char *existing = val.klass->name;
            int is_anon = existing && (strncmp(existing, "Struct::Anonymous", 17) == 0 ||
                                        strncmp(existing, "#<Class:", 8) == 0 ||
                                        strncmp(existing, "Anonymous", 9) == 0);
            if (is_anon) {
                val.klass->name = arena_alloc(ev->arena, strlen(target->sval) + 1);
                memcpy((char *)val.klass->name, target->sval, strlen(target->sval) + 1);
            }
        }
    } else if (target->kind == NODE_CALL) {
        /* Subscript assignment: h[k] = v, obj.attr = v, etc. */
        Value recv = eval_node(ev, env, target->call.recv ? target->call.recv : target);
        if (val_is_signal(recv)) { ev->errored = 1; return; }
        /* Build []=  args: original subscript args + val */
        Value setargs[65];
        int setargc = 0;
        for (NodeList *l = target->call.args; l && setargc < 64; l = l->next) {
            if (!l->node) continue;
            Value a = eval_node(ev, env, l->node);
            if (val_is_signal(a)) { ev->errored = 1; return; }
            setargs[setargc++] = a;
        }
        setargs[setargc++] = val;
        const char *setter = target->call.method;
        /* For [] → []= */
        if (strcmp(setter, "[]") == 0) setter = "[]=";
        else {
            /* attr= — append = if not already there */
            size_t slen = strlen(setter);
            if (slen > 0 && setter[slen-1] != '=') {
                char *s = arena_alloc(ev->arena, slen + 2);
                memcpy(s, setter, slen); s[slen] = '='; s[slen+1] = '\0';
                setter = s;
            }
        }
        dispatch_method(ev, env, recv, setter, setargs, setargc, NULL, target, 0, 1);
    }
}

static int exception_is_a(Eval *ev, Value klass) {
    if (klass.kind != VAL_CLASS || ev->current_exception.kind != VAL_OBJECT) return 0;
    RubyClass *k = ev->current_exception.obj->klass.klass;
    while (k) {
        if (strcmp(k->name, klass.klass->name) == 0) return 1;
        k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL;
    }
    return 0;
}

static int defined_simple_value(Eval *ev, Env *env, Node *node, Value *out) {
    if (!node) return 0;

    switch (node->kind) {
        case NODE_NIL:
            *out = val_nil();
            return 1;
        case NODE_TRUE:
            *out = val_true();
            return 1;
        case NODE_FALSE:
            *out = val_false();
            return 1;
        case NODE_SELF:
            return env_get(env, "self", out);
        case NODE_INT:
            *out = val_int(node->ival);
            return 1;
        case NODE_FLOAT:
            *out = val_float(node->fval);
            return 1;
        case NODE_STRING:
            *out = val_string(ev->arena, node->sval);
            return 1;
        case NODE_SYMBOL:
            *out = val_symbol(node->sval);
            return 1;
        case NODE_LVAR:
            return env_get(env, node->sval, out);
        case NODE_IVAR: {
            Value self;
            if (env_get(env, "self", &self) && self.kind == VAL_OBJECT)
                return val_object_get_ivar(self, node->sval, out);
            return global_get(&ev->globals, node->sval, out);
        }
        case NODE_GVAR:
            return global_get(&ev->globals, node->sval, out);
        case NODE_CONST:
            *out = lookup_const_path(ev, env, node->sval);
            return out->kind != VAL_NIL;
        default:
            return 0;
    }
}

static int defined_method(Eval *ev, Env *env, Value recv, const char *name, Node *site) {
    Value args[2];
    args[0] = val_symbol(name);
    args[1] = val_true();
    Value result = dispatch_method(ev, env, recv, "respond_to?", args, 2, NULL, site, 0, 1);
    if (result.kind == VAL_EXCEPTION) {
        eval_clear_exception(ev);
        return 0;
    }
    return result.kind == VAL_BOOL && result.bval;
}

static const char *defined_expr(Eval *ev, Env *env, Node *node) {
    if (!node) return NULL;

    switch (node->kind) {
        case NODE_LVAR: {
            Value v;
            if (env_get(env, node->sval, &v)) return "local-variable";
            Value self;
            if (env_get(env, "self", &self) && defined_method(ev, env, self, node->sval, node))
                return "method";
            return NULL;
        }
        case NODE_IVAR: {
            Value v;
            return defined_simple_value(ev, env, node, &v) ? "instance-variable" : NULL;
        }
        case NODE_CVAR:
        case NODE_GVAR: {
            Value v;
            return defined_simple_value(ev, env, node, &v) ? "global-variable" : NULL;
        }
        case NODE_CONST: {
            Value v = lookup_const_path(ev, env, node->sval);
            return v.kind != VAL_NIL ? "constant" : NULL;
        }
        case NODE_SELF:
            return "self";
        case NODE_ASSIGN:
        case NODE_OP_ASSIGN:
            return "assignment";
        case NODE_NIL:
            return "nil";
        case NODE_TRUE:
            return "true";
        case NODE_FALSE:
            return "false";
        case NODE_INT:
        case NODE_FLOAT:
        case NODE_STRING:
        case NODE_SYMBOL:
        case NODE_ARRAY:
        case NODE_HASH:
        case NODE_RANGE:
        case NODE_ROPE:
            return "expression";
        case NODE_CALL:
            if (!node->call.recv) {
                Value self;
                if (env_get(env, "self", &self) && defined_method(ev, env, self, node->call.method, node))
                    return "method";
                return NULL;
            }
            {
                Value recv;
                if (!defined_simple_value(ev, env, node->call.recv, &recv))
                    return NULL;
                if (defined_method(ev, env, recv, node->call.method, node))
                    return "method";
                return NULL;
            }
        default:
            return "expression";
    }
}

static void fire_method_added(Eval *ev, Env *env, Value klass, const char *mname, Node *site) {
    if (klass.kind != VAL_CLASS) return;
    Value hook;
    if (env_get(klass.klass->class_env, "self.method_added", &hook) && !val_is_signal(hook)) {
        Value sym = val_symbol(mname);
        dispatch_method(ev, env, klass, "method_added", &sym, 1, NULL, site, 0, 1);
        /* ignore errors from the hook — it's informational */
        ev->current_exception = val_nil();
        ev->exception_class = NULL;
        ev->exception_msg[0] = '\0';
    }
}

Value eval_node(Eval *ev, Env *env, Node *node) {
    if (!node || ev->errored) return val_nil();

    switch (node->kind) {
        case NODE_NIL:    return val_nil();
        case NODE_TRUE:   return val_true();
        case NODE_FALSE:  return val_false();
        case NODE_SELF: {
            Value v;
            if (env_get(env, "self", &v)) return v;
            return val_nil();
        }
        case NODE_INT:    return val_int(node->ival);
        case NODE_FLOAT:  return val_float(node->fval);
        case NODE_STRING: return val_string(ev->arena, node->sval);
        case NODE_SYMBOL: return val_symbol(node->sval);

        case NODE_REGEXP: {
            Value regexp_class;
            Regex *compiled = NULL;
            RegexError err = {0};
            if (!env_get(ev->top_env, "Regexp", &regexp_class) || regexp_class.kind != VAL_CLASS)
                return eval_raise_class(ev, node, "NameError", "uninitialized constant Regexp");
            if (regex_compile(ev->arena, node->regexp_lit.pattern,
                              node->regexp_lit.options, &compiled, &err) != REGEX_OK)
                return eval_raise_class(ev, node, "RegexpError", "%s",
                                       err.message[0] ? err.message : "regexp compile failed");
            Value obj = val_object(ev->arena, regexp_class);
            obj.obj->native = compiled;
            val_object_set_ivar(ev->arena, obj, "source",
                                val_string(ev->arena, node->regexp_lit.pattern));
            val_object_set_ivar(ev->arena, obj, "__options__",
                                val_int((int64_t)node->regexp_lit.options));
            return obj;
        }

        case NODE_ROPE: {
            const char *s = eval_rope(ev, env, node->interp.rope);
            if (ev->errored) return val_nil();
            return val_string(ev->arena, s);
        }

        case NODE_LVAR: {
            Value v;
            /* If marked as local by parser but not yet assigned, Ruby returns nil (not NameError).
               But first check: if the name is not in the env at all and self has this method,
               treat it as a zero-arg method call (Ruby: bare names call self's methods). */
            if (!env_get(env, node->sval, &v)) {
                Value self = val_nil();
                env_get(env, "self", &self);
                if (self.kind == VAL_OBJECT && self.obj->klass.kind == VAL_CLASS) {
                    Value m; RubyClass *owner = NULL;
                    if (ruby_class_find_instance_method(self.obj->klass.klass, node->sval, &m, &owner))
                        return call_method_value(ev, env, self, m, owner, node->sval, NULL, 0, NULL, node);
                }
                return val_nil();
            }
            /* VAL_METHOD from def in env: call it, don't return the raw method object */
            if (v.kind == VAL_METHOD) {
                Value self = val_nil();
                env_get(env, "self", &self);
                return call_method_value(ev, env, self, v, NULL, node->sval, NULL, 0, NULL, node);
            }
            return v;
        }
        case NODE_IVAR: {
            Value self;
            if (env_get(env, "self", &self)) {
                if (self.kind == VAL_OBJECT) {
                    Value v;
                    if (!val_object_get_ivar(self, node->sval, &v)) return val_nil();
                    return v;
                }
                if (self.kind == VAL_CLASS && self.klass && self.klass->class_env) {
                    /* Class instance variable: stored in class_env with @ prefix */
                    size_t nlen = strlen(node->sval);
                    char *key = arena_alloc(ev->arena, nlen + 2);
                    key[0] = '@'; memcpy(key + 1, node->sval, nlen + 1);
                    Value v;
                    if (!env_get(self.klass->class_env, key, &v)) return val_nil();
                    return v;
                }
            }
            Value v;
            if (!global_get(&ev->globals, node->sval, &v)) return val_nil();
            return v;
        }
        case NODE_GVAR: {
            /* $? — synthesize Process::Status from last child exit code */
            if (node->sval && strcmp(node->sval, "?") == 0) {
                Value code = val_int(0);
                global_get(&ev->globals, "__child_exit__", &code);
                /* Find Process::Status class and instantiate it */
                Value proc_mod = val_nil();
                if (env_get(ev->top_env, "Process", &proc_mod) &&
                    proc_mod.kind == VAL_CLASS && proc_mod.klass->class_env) {
                    Value status_class = val_nil();
                    if (env_get(proc_mod.klass->class_env, "Status", &status_class) &&
                        status_class.kind == VAL_CLASS) {
                        Value args[1] = { code };
                        Value ps = dispatch_method(ev, env, status_class, "new", args, 1,
                                                   NULL, node, 0, 1);
                        if (!val_is_signal(ps)) return ps;
                    }
                }
                return val_nil();
            }
            Value v;
            if (!global_get(&ev->globals, node->sval, &v)) return val_nil();
            return v;
        }
        case NODE_CVAR: {
            /* Find class scope: self may be a class or an instance */
            Value self = val_nil();
            env_get(env, "self", &self);
            RubyClass *klass = NULL;
            if (self.kind == VAL_CLASS) klass = self.klass;
            else if (self.kind == VAL_OBJECT && self.obj->klass.kind == VAL_CLASS)
                klass = self.obj->klass.klass;
            while (klass) {
                Value v;
                if (klass->class_env && env_get(klass->class_env, node->sval, &v))
                    return v;
                klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
            }
            return val_nil();
        }
        case NODE_CONST: {
            Value v = lookup_const_path(ev, env, node->sval);
            if (v.kind == VAL_NIL) {
                /* Try const_missing hook on the parent module */
                const char *parent_name = NULL, *leaf_name = NULL;
                split_const_path(ev->arena, node->sval, &parent_name, &leaf_name);
                if (parent_name && leaf_name) {
                    Value parent = lookup_const_path(ev, env, parent_name);
                    if (parent.kind == VAL_CLASS) {
                        Value sym = val_symbol(leaf_name);
                        Value r = dispatch_method(ev, env, parent, "const_missing", &sym, 1, NULL, node, 0, 1);
                        if (!val_is_signal(r)) return r;
                        eval_clear_exception(ev);
                    }
                }
                return eval_raise_class(ev, node, "NameError", "uninitialized constant '%s'", node->sval);
            }
            return v;
        }
        case NODE_CONST_ACCESS: {
            Value recv = eval_node(ev, env, node->const_access.recv);
            CHECK(recv);
            Value v = lookup_const_on_class(recv, node->const_access.name);
            if (v.kind == VAL_NIL && recv.kind == VAL_CLASS) {
                /* Try Module.const_missing(:Name) hook */
                Value sym = val_symbol(node->const_access.name);
                Value r = dispatch_method(ev, env, recv, "const_missing", &sym, 1, NULL, node, 0, 1);
                if (!val_is_signal(r)) return r;
                eval_clear_exception(ev);
                return eval_raise_class(ev, node, "NameError", "uninitialized constant '%s'", node->const_access.name);
            }
            if (v.kind == VAL_NIL)
                return eval_raise_class(ev, node, "NameError", "uninitialized constant '%s'", node->const_access.name);
            return v;
        }

        case NODE_ASSIGN: {
            Value val = eval_node(ev, env, node->assign.value);
            CHECK(val);
            {
                const char *exc_before = ev->exception_class;
                assign_target(ev, env, node->assign.target, val);
                if (ev->exception_class != exc_before) return val_exception();
            }
            return val;
        }

        case NODE_OP_ASSIGN: {
            char op[8];
            const char *raw_op = node->binop.op;
            size_t oplen = strlen(raw_op);
            if (oplen > 0 && raw_op[oplen - 1] == '=') {
                memcpy(op, raw_op, oplen - 1);
                op[oplen - 1] = '\0';
            } else {
                strcpy(op, raw_op);
            }

            Node fake;
            memset(&fake, 0, sizeof(fake));
            fake.kind = NODE_BINOP;
            fake.span = node->span;
            fake.binop.op = op;
            fake.binop.left = node->binop.left;
            fake.binop.right = node->binop.right;

            Value val = eval_binop(ev, env, &fake);
            CHECK(val);

            Node *target = node->binop.left;
            if (target->kind == NODE_LVAR)
                assign_lvar(ev, env, target->sval, val);
            else if (target->kind == NODE_IVAR || target->kind == NODE_GVAR ||
                     target->kind == NODE_CVAR) {
                assign_target(ev, env, target, val);
            } else {
                assign_target(ev, env, target, val);
            }
            return val;
        }

        case NODE_BINOP:
            return eval_binop(ev, env, node);

        case NODE_UNOP: {
            Value operand = eval_node(ev, env, node->unop.operand);
            CHECK(operand);
            const char *op = node->unop.op;
            if (strcmp(op, "!") == 0 || strcmp(op, "not") == 0)
                return val_bool(!val_truthy(operand));
            if (strcmp(op, "-") == 0) {
                if (operand.kind == VAL_INT) return val_int(-operand.ival);
                if (operand.kind == VAL_FLOAT) return val_float(-operand.fval);
                /* Built-in Complex -@ */
                if (operand.kind == VAL_OBJECT && operand.obj->klass.kind == VAL_CLASS &&
                    operand.obj->klass.klass && strcmp(operand.obj->klass.klass->name, "Complex") == 0) {
                    Value cplx_class;
                    if (env_get(ev->top_env, "Complex", &cplx_class) && cplx_class.kind == VAL_CLASS) {
                        Value real_v = val_nil(), imag_v = val_nil();
                        val_object_get_ivar(operand, "real", &real_v);
                        val_object_get_ivar(operand, "imaginary", &imag_v);
                        /* Negate real and imaginary parts inline */
                        Value neg_r = (real_v.kind == VAL_INT) ? val_int(-real_v.ival)
                                    : (real_v.kind == VAL_FLOAT) ? val_float(-real_v.fval)
                                    : real_v;
                        Value neg_i = (imag_v.kind == VAL_INT) ? val_int(-imag_v.ival)
                                    : (imag_v.kind == VAL_FLOAT) ? val_float(-imag_v.fval)
                                    : imag_v;
                        Value args[2] = {neg_r, neg_i};
                        return dispatch_method(ev, env, cplx_class, "new", args, 2, NULL, node, 0, 1);
                    }
                }
                /* Try user-defined -@ method */
                Value r = dispatch_method(ev, env, operand, "-@", NULL, 0, NULL, node, 0, 1);
                if (!val_is_signal(r)) return r;
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
            }
            if (strcmp(op, "+") == 0) {
                if (operand.kind == VAL_INT || operand.kind == VAL_FLOAT) return operand;
                /* Try user-defined +@ method */
                Value r = dispatch_method(ev, env, operand, "+@", NULL, 0, NULL, node, 0, 1);
                if (!val_is_signal(r)) return r;
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
                return operand;
            }
            if (strcmp(op, "~") == 0) {
                if (operand.kind == VAL_INT) return val_int(~operand.ival);
                /* Try user-defined ~ method */
                Value r = dispatch_method(ev, env, operand, "~", NULL, 0, NULL, node, 0, 1);
                if (!val_is_signal(r)) return r;
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
            }
            return eval_raise_class(ev, node, "NoMethodError", "undefined unary operator '%s'", op);
        }

        case NODE_CALL:
            return eval_call(ev, env, node);

        case NODE_DEFINED: {
            const char *kind = defined_expr(ev, env, node->defined_expr.expr);
            return kind ? val_string(ev->arena, kind) : val_nil();
        }

        case NODE_IF:
        case NODE_UNLESS: {
            Value cond = eval_node(ev, env, node->cond.cond);
            CHECK(cond);
            int taken = (node->kind == NODE_IF) ? val_truthy(cond) : !val_truthy(cond);
            if (taken) return eval_node(ev, env, node->cond.then_body);
            if (node->cond.else_body) return eval_node(ev, env, node->cond.else_body);
            return val_nil();
        }

        case NODE_WHILE:
        case NODE_UNTIL: {
            Value result = val_nil();
            while (1) {
                if (!node->loop.post_test) {
                    Value cond = eval_node(ev, env, node->loop.cond);
                    CHECK(cond);
                    int cont = (node->kind == NODE_WHILE) ? val_truthy(cond) : !val_truthy(cond);
                    if (!cont) break;
                }
                result = eval_node(ev, env, node->loop.body);
                if (result.kind == VAL_BREAK) return *result.jump.wrapped;
                if (result.kind == VAL_RETURN) return result;
                if (result.kind == VAL_NEXT) {
                    result = val_nil();
                } else if (result.kind == VAL_EXCEPTION) {
                    return result;
                }
                if (node->loop.post_test) {
                    Value cond = eval_node(ev, env, node->loop.cond);
                    CHECK(cond);
                    int cont = (node->kind == NODE_WHILE) ? val_truthy(cond) : !val_truthy(cond);
                    if (!cont) break;
                }
            }
            return result;
        }

        case NODE_FOR: {
            Value iterable = eval_node(ev, env, node->for_loop.iterable);
            CHECK(iterable);
            Value result = val_nil();
            Node *for_target = node->for_loop.target;

/* Bind the loop variable into the enclosing env (for leaks scope like MRI). */
#define FOR_BIND(item_val) do { \
    Value _fv = (item_val); \
    assign_target(ev, env, for_target, _fv); \
} while (0)

/* Run the body once; handle control-flow signals. */
#define FOR_BODY() do { \
    result = eval_node(ev, env, node->for_loop.body); \
    if (result.kind == VAL_BREAK)     { result = *result.jump.wrapped; goto for_done; } \
    if (result.kind == VAL_RETURN)    goto for_done; \
    if (result.kind == VAL_EXCEPTION) goto for_done; \
    if (result.kind == VAL_NEXT)      result = val_nil(); \
} while (0)

            if (iterable.kind == VAL_ARRAY) {
                for (size_t i = 0; i < iterable.array->len; i++) {
                    FOR_BIND(iterable.array->elems[i]);
                    FOR_BODY();
                }
            } else if (iterable.kind == VAL_RANGE) {
                RubyRange *r = iterable.range;
                if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
                    return eval_raise_class(ev, node, "TypeError", "for loop requires an Integer range");
                int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
                for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
                    FOR_BIND(val_int(i));
                    FOR_BODY();
                }
            } else {
                /* Arbitrary iterable: lower to to_a then iterate, matching MRI's each semantics */
                Value ary = dispatch_method(ev, env, iterable, "to_a", NULL, 0, NULL, node, 0, 1);
                if (val_is_signal(ary)) return ary;
                if (ary.kind == VAL_ARRAY) {
                    for (size_t i = 0; i < ary.array->len; i++) {
                        FOR_BIND(ary.array->elems[i]);
                        FOR_BODY();
                    }
                } else {
                    return eval_raise_class(ev, node, "TypeError",
                        "no implicit conversion of %s into Array", val_kind_name(iterable.kind));
                }
            }

            for_done:
#undef FOR_BIND
#undef FOR_BODY
            return result;
        }

        case NODE_CASE: {
            Value subject = val_true(); /* caseless: each when tests truthiness */
            int has_subject = (node->case_stmt.subject != NULL);
            if (has_subject) {
                subject = eval_node(ev, env, node->case_stmt.subject);
                CHECK(subject);
            }
            for (NodeList *l = node->case_stmt.whens; l; l = l->next) {
                Node *w = l->node;
                if (!w || w->kind != NODE_WHEN) continue;
                for (NodeList *pl = w->when_clause.patterns; pl; pl = pl->next) {
                    Value pat = eval_node(ev, env, pl->node);
                    CHECK(pat);
                    int matched;
                    if (has_subject) {
                        Value result = dispatch_method(ev, env, pat, "===", &subject, 1, NULL, node, 0, 1);
                        CHECK(result);
                        matched = val_truthy(result);
                    } else {
                        matched = val_truthy(pat);
                    }
                    if (matched)
                        return eval_node(ev, env, w->when_clause.body);
                }
            }
            if (node->case_stmt.else_body)
                return eval_node(ev, env, node->case_stmt.else_body);
            return val_nil();
        }

        case NODE_RETRY:
            return val_retry();

        case NODE_BEGIN: {
            Value result;
            int rescued = 0;
            retry_begin:
            result = eval_node(ev, env, node->begin_stmt.body);

            if (result.kind == VAL_EXCEPTION && node->begin_stmt.rescues) {
                for (NodeList *l = node->begin_stmt.rescues; l; l = l->next) {
                    Node *rescue_clause = l->node;
                    if (!rescue_clause || rescue_clause->kind != NODE_RESCUE) continue;
                    if (rescue_clause->rescue_clause.exception_classes) {
                        int matched = 0;
                        for (NodeList *cl = rescue_clause->rescue_clause.exception_classes; cl; cl = cl->next) {
                            /* Save exception state: evaluating the class name may itself raise
                               (e.g. undefined constant Interrupt → NameError), which would
                               overwrite the exception we're trying to rescue. */
                            Value saved_exc = ev->current_exception;
                            const char *saved_eclass = ev->exception_class;
                            int saved_errored = ev->errored;
                            Value rescue_class = eval_node(ev, env, cl->node);
                            if (val_is_signal(rescue_class)) {
                                /* Restore original exception and skip this unresolvable class */
                                ev->current_exception = saved_exc;
                                ev->exception_class = saved_eclass;
                                ev->errored = saved_errored;
                                continue;
                            }
                            if (exception_is_a(ev, rescue_class)) { matched = 1; break; }
                        }
                        if (!matched) continue;
                    }
                    Value rescued_exc = ev->current_exception;
                    Value previous_rescue = ev->rescue_context;
                    eval_clear_exception(ev);
                    ev->rescue_context = rescued_exc;
                    rescued = 1;
                    Env *rescue_env = env;
                    if (rescue_clause->rescue_clause.exception_var) {
                        rescue_env = env_new(ev->arena, env, 0);
                        env_set(ev->arena, rescue_env, rescue_clause->rescue_clause.exception_var, rescued_exc);
                    }
                    result = eval_node(ev, rescue_env, rescue_clause->rescue_clause.body);
                    ev->rescue_context = previous_rescue;
                    if (result.kind == VAL_RETRY) goto retry_begin;
                    break;
                }
            }

            if (result.kind != VAL_EXCEPTION && !rescued && node->begin_stmt.else_body) {
                result = eval_node(ev, env, node->begin_stmt.else_body);
            }

            if (node->begin_stmt.ensure_body) {
                Value ensure_result = eval_node(ev, env, node->begin_stmt.ensure_body);
                if (val_is_signal(ensure_result))
                    return ensure_result;
            }

            return result;
        }

        case NODE_RETURN:
            if (node->jump.value) {
                Value v = eval_node(ev, env, node->jump.value);
                CHECK(v);
                return val_return(ev->arena, v, env_nearest_def(env));
            }
            return val_return(ev->arena, val_nil(), env_nearest_def(env));

        case NODE_BREAK:
            if (node->jump.value) {
                Value v = eval_node(ev, env, node->jump.value);
                CHECK(v);
                return val_break(ev->arena, v);
            }
            return val_break(ev->arena, val_nil());

        case NODE_NEXT:
            if (node->jump.value) {
                Value v = eval_node(ev, env, node->jump.value);
                CHECK(v);
                return val_next(ev->arena, v);
            }
            return val_next(ev->arena, val_nil());

        case NODE_SUPER: {
            Value self;
            if (!env_get(env, "self", &self) || self.kind != VAL_OBJECT)
                return eval_raise_class(ev, node, "NoMethodError", "super called outside of instance method");

            Value cur_class_val;
            if (!env_get(env, "__class__", &cur_class_val) || cur_class_val.kind != VAL_CLASS)
                return eval_raise_class(ev, node, "NoMethodError", "super called outside of instance method");

            Value method_name_val;
            if (!env_get(env, "__method__", &method_name_val))
                return eval_raise_class(ev, node, "NoMethodError", "super called outside of instance method");
            const char *method_name = method_name_val.sval;

            Value super_args[64];
            int super_argc = 0;
            if (!node->super_call.forward_args) {
                for (NodeList *l = node->super_call.args; l && super_argc < 64; l = l->next) {
                    Value a = eval_node(ev, env, l->node);
                    CHECK(a);
                    super_args[super_argc++] = a;
                }
            } else {
                Env *frame = env;
                Value method_val;
                if (env_get(cur_class_val.klass->class_env, method_name, &method_val) &&
                    method_val.kind == VAL_METHOD) {
                    NodeList *params = method_val.method.def_node->def.params;
                    for (; params && super_argc < 64; params = params->next) {
                        Node *p = params->node;
                        if (p->kind != NODE_PARAM || !p->param.name ||
                            p->param.block_param || p->param.keyword_param ||
                            p->param.keyword_splat) continue;
                        Value pval;
                        if (env_get(frame, p->param.name, &pval))
                            super_args[super_argc++] = pval;
                    }
                }
            }

            Value method;
            RubyClass *owner = NULL;
            if (ruby_class_find_super_method(self.obj->klass.klass, cur_class_val.klass,
                                             method_name, &method, &owner)) {
                Value *blk = NULL;
                for (Env *sc = env; sc; sc = sc->parent) {
                    if (sc->block_arg) { blk = sc->block_arg; break; }
                    if (sc->is_def) break;
                }
                Value result = call_method_value(ev, env, self, method, owner, method_name,
                                                 super_args, super_argc, blk, node);
                if (val_is_signal(result)) return result;
                return result;
            }
            return eval_raise_class(ev, node, "NoMethodError", "super: no superclass method '%s'", method_name);
        }

        case NODE_DEF:
            if (node->def.recv) {
                Value recv = eval_node(ev, env, node->def.recv);
                CHECK(recv);
                if (recv.kind == VAL_CLASS) {
                    size_t nlen = strlen(node->def.name);
                    char *key = arena_alloc(ev->arena, nlen + 6);
                    memcpy(key, "self.", 5);
                    memcpy(key + 5, node->def.name, nlen + 1);
                    env_define(ev->arena, recv.klass->class_env, key, val_method(node, ev->top_env, METHOD_PUBLIC, ev->current_file));
                } else {
                    Env **slot = value_singleton_env_slot(recv);
                    if (!slot)
                        return eval_raise_class(ev, node, "TypeError",
                                                "can only define singleton methods on heap objects");
                    if (!*slot) *slot = env_new(ev->arena, NULL, 1);
                    env_define(ev->arena, *slot, node->def.name,
                               val_method(node, ev->top_env, METHOD_PUBLIC, ev->current_file));
                }
            } else {
                Value singleton_target = val_nil();
                if (env_get(env, "__singleton_target__", &singleton_target)) {
                    if (singleton_target.kind == VAL_CLASS) {
                        size_t nlen = strlen(node->def.name);
                        char *key = arena_alloc(ev->arena, nlen + 6);
                        memcpy(key, "self.", 5);
                        memcpy(key + 5, node->def.name, nlen + 1);
                        env_define(ev->arena, singleton_target.klass->class_env, key,
                                   val_method(node, ev->top_env, current_method_visibility(env), ev->current_file));
                    } else {
                        Env **slot = value_singleton_env_slot(singleton_target);
                        if (slot) {
                            if (!*slot) *slot = env_new(ev->arena, NULL, 1);
                            env_define(ev->arena, *slot, node->def.name,
                                       val_method(node, ev->top_env, current_method_visibility(env), ev->current_file));
                        } else {
                            /* singleton_target is nil (class body) — hoist to self's class_env if in a class */
                            Value self_val = val_nil();
                            env_get(env, "self", &self_val);
                            Env *def_target = env;
                            if (self_val.kind == VAL_CLASS && self_val.klass && self_val.klass->class_env)
                                def_target = self_val.klass->class_env;
                            env_define(ev->arena, def_target, node->def.name,
                                       val_method(node, ev->top_env, current_method_visibility(env), ev->current_file));
                            if (self_val.kind == VAL_CLASS)
                                fire_method_added(ev, env, self_val, node->def.name, node);
                            /* module_function: also add self.name as a public module method */
                            if (self_val.kind == VAL_CLASS && self_val.klass &&
                                self_val.klass->is_module && is_module_function_mode(env)) {
                                size_t nlen = strlen(node->def.name);
                                char *key = arena_alloc(ev->arena, nlen + 6);
                                memcpy(key, "self.", 5);
                                memcpy(key + 5, node->def.name, nlen + 1);
                                env_define(ev->arena, def_target, key,
                                           val_method(node, ev->top_env, METHOD_PUBLIC, ev->current_file));
                            }
                        }
                    }
                } else {
                    /* If self is a class (e.g. inside class_eval or a block inside a class body),
                       register the instance method in the class's class_env rather than the
                       local block frame.  This matches Ruby: def always hoists to the nearest
                       class/module scope when one is in effect. */
                    Value self_val = val_nil();
                    env_get(env, "self", &self_val);
                    Env *def_target = env;
                    if (self_val.kind == VAL_CLASS && self_val.klass && self_val.klass->class_env)
                        def_target = self_val.klass->class_env;

                    /* Don't overwrite existing class/module constants with top-level defs.
                       In Ruby, def Foo() defines a kernel method that doesn't shadow the Foo constant. */
                    int is_const_name = node->def.name && isupper((unsigned char)node->def.name[0]);
                    Value existing_const = val_nil();
                    int would_shadow = is_const_name && def_target == env &&
                        env_get(ev->top_env, node->def.name, &existing_const) &&
                        existing_const.kind == VAL_CLASS;
                    if (!would_shadow) {
                        env_define(ev->arena, def_target, node->def.name,
                                   val_method(node, ev->top_env, current_method_visibility(env), ev->current_file));
                        if (self_val.kind == VAL_CLASS)
                            fire_method_added(ev, env, self_val, node->def.name, node);
                        /* module_function: also add self.name as a public module method */
                        if (self_val.kind == VAL_CLASS && self_val.klass &&
                            self_val.klass->is_module && is_module_function_mode(env)) {
                            size_t nlen = strlen(node->def.name);
                            char *key = arena_alloc(ev->arena, nlen + 6);
                            memcpy(key, "self.", 5);
                            memcpy(key + 5, node->def.name, nlen + 1);
                            env_define(ev->arena, def_target, key,
                                       val_method(node, ev->top_env, METHOD_PUBLIC, ev->current_file));
                        }
                    } else {
                        /* Store as private kernel method under a mangled key that doesn't shadow the constant */
                        size_t nlen = strlen(node->def.name);
                        char *mkey = arena_alloc(ev->arena, nlen + 9);
                        memcpy(mkey, "__kern__", 8);
                        memcpy(mkey + 8, node->def.name, nlen + 1);
                        env_define(ev->arena, ev->top_env, mkey,
                                   val_method(node, ev->top_env, METHOD_PRIVATE, ev->current_file));
                    }
                }
            }
            return val_nil();

        case NODE_ALIAS: {
            Value method = val_nil();
            int found = 0;

            Value self = val_nil();
            if (env_get(env, "self", &self) && self.kind == VAL_CLASS && self.klass->class_env) {
                Value singleton_target = val_nil();
                int in_singleton_class = env_get(env, "__singleton_target__", &singleton_target) &&
                                         singleton_target.kind == VAL_CLASS &&
                                         singleton_target.klass == self.klass;
                if (in_singleton_class) {
                    size_t old_len = strlen(node->alias_stmt.old_name);
                    char *old_key = arena_alloc(ev->arena, old_len + 6);
                    memcpy(old_key, "self.", 5);
                    memcpy(old_key + 5, node->alias_stmt.old_name, old_len + 1);
                    found = env_get(self.klass->class_env, old_key, &method) && method.kind == VAL_METHOD;
                    if (found) {
                        size_t new_len = strlen(node->alias_stmt.new_name);
                        char *new_key = arena_alloc(ev->arena, new_len + 6);
                        memcpy(new_key, "self.", 5);
                        memcpy(new_key + 5, node->alias_stmt.new_name, new_len + 1);
                        env_define(ev->arena, self.klass->class_env, new_key, method);
                    } else if (val_responds_to(ev, self, node->alias_stmt.old_name, 1)) {
                        size_t new_len = strlen(node->alias_stmt.new_name);
                        char *new_key = arena_alloc(ev->arena, new_len + 6);
                        memcpy(new_key, "self.", 5);
                        memcpy(new_key + 5, node->alias_stmt.new_name, new_len + 1);
                        env_define(ev->arena, self.klass->class_env, new_key, val_symbol(node->alias_stmt.old_name));
                        found = 1;
                    }
                } else {
                    found = ruby_class_find_instance_method(self.klass, node->alias_stmt.old_name, &method, NULL);
                    if (found) {
                        env_define(ev->arena, self.klass->class_env, node->alias_stmt.new_name, method);
                    } else {
                        Value probe = val_object(ev->arena, self);
                        if (val_responds_to(ev, probe, node->alias_stmt.old_name, 1)) {
                            env_define(ev->arena, self.klass->class_env, node->alias_stmt.new_name,
                                       val_symbol(node->alias_stmt.old_name));
                            found = 1;
                        } else {
                            /* Kernel/top-level function: only alias if top_env has a binding
                               or it's a known dispatchable name; refuse otherwise */
                            static const char *kernel_fns[] = {
                                "load", "require", "require_relative",
                                "puts", "print", "p", "pp", "warn",
                                "exit", "abort", "raise", "fail",
                                "lambda", "proc", "loop", "rand",
                                "format", "sprintf", "system",
                                "Integer", "Float", "String", "Array",
                                "__method__", "__dir__", "__callee__",
                                "gets", "sleep", "at_exit",
                                NULL
                            };
                            int is_kernel = 0;
                            for (int ki = 0; kernel_fns[ki]; ki++) {
                                if (strcmp(node->alias_stmt.old_name, kernel_fns[ki]) == 0) {
                                    is_kernel = 1;
                                    break;
                                }
                            }
                            if (is_kernel) { /* Create a forwarding call node */
                            Arena *a = ev->arena;
                            Node *call_node = arena_alloc(a, sizeof(Node));
                            memset(call_node, 0, sizeof(Node));
                            call_node->kind = NODE_CALL;
                            call_node->call.method = node->alias_stmt.old_name;
                            call_node->call.recv = NULL;

                            Node *rest_param = arena_alloc(a, sizeof(Node));
                            memset(rest_param, 0, sizeof(Node));
                            rest_param->kind = NODE_PARAM;
                            rest_param->param.name = "__fwd_args__";
                            rest_param->param.splat = 1;

                            Node *splat_arg = arena_alloc(a, sizeof(Node));
                            memset(splat_arg, 0, sizeof(Node));
                            splat_arg->kind = NODE_UNOP;
                            splat_arg->unop.op = "*";
                            Node *arg_ref = arena_alloc(a, sizeof(Node));
                            memset(arg_ref, 0, sizeof(Node));
                            arg_ref->kind = NODE_LVAR;
                            arg_ref->sval = "__fwd_args__";
                            splat_arg->unop.operand = arg_ref;

                            call_node->call.args = nodelist_append(a, NULL, splat_arg);

                            Node *def_node = arena_alloc(a, sizeof(Node));
                            memset(def_node, 0, sizeof(Node));
                            def_node->kind = NODE_DEF;
                            def_node->def.name = node->alias_stmt.new_name;
                            def_node->def.params = nodelist_append(a, NULL, rest_param);
                            def_node->def.body = call_node;

                            Value fwd_method = val_method(def_node, env, METHOD_PUBLIC, ev->current_file);
                            env_define(ev->arena, self.klass->class_env, node->alias_stmt.new_name, fwd_method);
                            found = 1;
                        }
                    }
                }
            }
            } else {
                found = env_get(env, node->alias_stmt.old_name, &method) && method.kind == VAL_METHOD;
                if (found) env_define(ev->arena, env, node->alias_stmt.new_name, method);
            }

            if (!found)
                return eval_raise_class(ev, node, "NameError",
                                        "undefined method '%s' for alias", node->alias_stmt.old_name);
            return val_nil();
        }

        case NODE_CLASS: {
            const char *parent_name = NULL;
            const char *leaf_name = NULL;
            split_const_path(ev->arena, node->klass.name, &parent_name, &leaf_name);
            Env *target_env = ev->top_env;
            const char *full_name = node->klass.name;
            if (parent_name) {
                Value parent = lookup_const_path(ev, env, parent_name);
                if (parent.kind != VAL_CLASS)
                    return eval_raise_class(ev, node, "NameError", "uninitialized constant '%s'", parent_name);
                target_env = parent.klass->class_env;
            } else {
                Value current_self = val_nil();
                if (env_get(env, "self", &current_self) && current_self.kind == VAL_CLASS &&
                    current_self.klass->class_env == env) {
                    target_env = current_self.klass->class_env;
                    full_name = qualify_const_name(ev->arena, current_self.klass->name, leaf_name);
                }
            }

            Value existing = val_nil();
            int reopen = 0;
            for (EnvEntry *e = target_env->vars; e; e = e->next) {
                if (strcmp(e->name, leaf_name) == 0) {
                    existing = e->val;
                    reopen = existing.kind == VAL_CLASS && !existing.klass->is_module;
                    break;
                }
            }

            Value klass;
            if (reopen) {
                klass = existing;
            } else {
                Value superclass = val_nil();
                if (node->klass.superclass) {
                    superclass = eval_node(ev, env, node->klass.superclass);
                    CHECK(superclass);
                    if (superclass.kind != VAL_CLASS && superclass.kind != VAL_NIL)
                        return eval_raise_class(ev, node, "TypeError", "superclass must be a class");
                } else {
                    /* implicit superclass is Object unless we are defining Object itself */
                    Value object_val;
                    if (strcmp(leaf_name, "Object") != 0 &&
                        env_get(ev->top_env, "Object", &object_val) &&
                        object_val.kind == VAL_CLASS)
                        superclass = object_val;
                }
                klass = val_class(ev->arena, full_name, superclass);
                klass.klass->class_env = env_new(ev->arena, target_env, 1);
                env_define(ev->arena, target_env, leaf_name, klass);
                env_define(ev->arena, ev->top_env, full_name, klass);
            }

            env_set(ev->arena, klass.klass->class_env, "self", klass);
            env_set(ev->arena, klass.klass->class_env, "__class__", klass);
            env_set(ev->arena, klass.klass->class_env, "__singleton_target__", val_nil());
            set_current_method_visibility(ev->arena, klass.klass->class_env, METHOD_PUBLIC);
            if (node->klass.body) {
                Value body_result = eval_node(ev, klass.klass->class_env, node->klass.body);
                if (val_is_signal(body_result)) return body_result;
            }

            return klass;
        }

        case NODE_SCLASS: {
            Value recv = eval_node(ev, env, node->sclass.recv);
            CHECK(recv);

            Env *target_env = NULL;
            if (recv.kind == VAL_CLASS) {
                target_env = recv.klass->class_env;
            } else if (recv.kind == VAL_OBJECT) {
                if (!recv.obj->singleton_env)
                    recv.obj->singleton_env = env_new(ev->arena, NULL, 1);
                target_env = recv.obj->singleton_env;
            } else {
                return eval_raise_class(ev, node, "TypeError", "singleton class must be opened on an object");
            }

            env_set(ev->arena, target_env, "self", recv);
            if (recv.kind == VAL_CLASS)
                env_set(ev->arena, target_env, "__class__", recv);
            Value prev_singleton_target = val_nil();
            env_get(target_env, "__singleton_target__", &prev_singleton_target);
            Value prev_visibility = val_nil();
            env_get(target_env, "__visibility__", &prev_visibility);
            env_set(ev->arena, target_env, "__singleton_target__", recv);
            set_current_method_visibility(ev->arena, target_env, METHOD_PUBLIC);
            Value body_result = recv;
            if (node->sclass.body) {
                body_result = eval_node(ev, target_env, node->sclass.body);
                if (val_is_signal(body_result)) return body_result;
            }
            /* Restore singleton target and visibility so subsequent defs go to instance methods */
            env_set(ev->arena, target_env, "__singleton_target__", prev_singleton_target);
            if (prev_visibility.kind == VAL_SYMBOL)
                env_set(ev->arena, target_env, "__visibility__", prev_visibility);
            else
                set_current_method_visibility(ev->arena, target_env, METHOD_PUBLIC);
            return body_result;
        }

        case NODE_MODULE: {
            const char *parent_name = NULL;
            const char *leaf_name = NULL;
            split_const_path(ev->arena, node->klass.name, &parent_name, &leaf_name);
            Env *target_env = ev->top_env;
            const char *full_name = node->klass.name;
            if (parent_name) {
                Value parent = lookup_const_path(ev, env, parent_name);
                if (parent.kind != VAL_CLASS)
                    return eval_raise_class(ev, node, "NameError", "uninitialized constant '%s'", parent_name);
                target_env = parent.klass->class_env;
            } else {
                Value current_self = val_nil();
                if (env_get(env, "self", &current_self) && current_self.kind == VAL_CLASS &&
                    current_self.klass->class_env == env) {
                    target_env = current_self.klass->class_env;
                    full_name = qualify_const_name(ev->arena, current_self.klass->name, leaf_name);
                }
            }

            Value existing = val_nil();
            int reopen = 0;
            for (EnvEntry *e = target_env->vars; e; e = e->next) {
                if (strcmp(e->name, leaf_name) == 0) {
                    existing = e->val;
                    reopen = existing.kind == VAL_CLASS && existing.klass->is_module;
                    break;
                }
            }

            Value mod;
            if (reopen) {
                mod = existing;
            } else {
                mod = val_class(ev->arena, full_name, val_nil());
                mod.klass->class_env = env_new(ev->arena, target_env, 1);
                mod.klass->is_module = 1;
                env_define(ev->arena, target_env, leaf_name, mod);
                env_define(ev->arena, ev->top_env, full_name, mod);
            }

            env_set(ev->arena, mod.klass->class_env, "self", mod);
            env_set(ev->arena, mod.klass->class_env, "__class__", mod);
            env_set(ev->arena, mod.klass->class_env, "__singleton_target__", val_nil());
            set_current_method_visibility(ev->arena, mod.klass->class_env, METHOD_PUBLIC);
            if (node->klass.body) {
                Value body_result = eval_node(ev, mod.klass->class_env, node->klass.body);
                if (val_is_signal(body_result)) return body_result;
            }

            return mod;
        }

        case NODE_RANGE: {
            Value begin = val_nil();
            Value end = val_nil();
            if (node->range.begin) {
                begin = eval_node(ev, env, node->range.begin);
                CHECK(begin);
            }
            if (node->range.end) {
                end = eval_node(ev, env, node->range.end);
                CHECK(end);
            }
            return val_range(ev->arena, begin, end, node->range.exclusive);
        }

        case NODE_ARRAY: {
            Value arr = val_array_new();
            for (NodeList *l = node->array.elements; l; l = l->next) {
                Node *el = l->node;
                if (el && el->kind == NODE_UNOP && strcmp(el->unop.op, "*") == 0) {
                    Value splat = eval_node(ev, env, el->unop.operand);
                    CHECK(splat);
                    if (splat.kind != VAL_ARRAY && splat.kind != VAL_NIL) {
                        Value as_arr = dispatch_method(ev, env, splat, "to_a", NULL, 0, NULL, el, 0, 1);
                        if (!ev->errored && as_arr.kind == VAL_ARRAY) splat = as_arr;
                        else { ev->errored = 0; ev->exception_class = NULL; }
                    }
                    if (splat.kind == VAL_ARRAY) {
                        for (size_t i = 0; i < splat.array->len; i++)
                            val_array_push(&arr, splat.array->elems[i]);
                    } else if (splat.kind != VAL_NIL) {
                        val_array_push(&arr, splat);
                    }
                } else {
                    Value elem = eval_node(ev, env, el);
                    CHECK(elem);
                    val_array_push(&arr, elem);
                }
            }
            return arr;
        }

        case NODE_HASH: {
            Value h = val_hash_new(ev->arena);
            for (NodeList *l = node->hash.pairs; l; l = l->next) {
                Node *pair = l->node;
                if (!pair || pair->kind != NODE_PAIR) continue;
                Value key = eval_node(ev, env, pair->pair.key);
                CHECK(key);
                Value val = eval_node(ev, env, pair->pair.value);
                CHECK(val);
                val_hash_set(h.hash, key, val);
            }
            return h;
        }

        case NODE_BODY:
        case NODE_PROGRAM: {
            Value result = val_nil();
            for (NodeList *l = node->body.stmts; l; l = l->next) {
                result = eval_node(ev, env, l->node);
                if (ev->errored || val_is_signal(result)) return result;
            }
            return result;
        }

        default:
            return eval_error(ev, node, "cannot evaluate node kind %s", node_kind_name(node->kind));
    }
}

void eval_init(Eval *ev, Arena *arena, FILE *out, const char *current_file, const char *exec_path,
               int script_argc, char **script_argv) {
    memset(ev, 0, sizeof(*ev));
    ev->arena   = arena;
    ev->out     = out;
    ev->top_env = env_new(arena, NULL, 1);
    ev->active_defs[0] = ev->top_env;
    ev->active_def_count = 1;
    ev->current_file = current_file;
    ev->exec_path = exec_path;
    ev->runtime_root = runtime_root_from_exec(arena, exec_path);

    Value load_path = val_array_new();
    val_array_push(&load_path, val_string(arena, "."));
    if (current_file) {
        const char *slash = strrchr(current_file, '/');
        if (slash) {
            size_t len = (size_t)(slash - current_file);
            val_array_push(&load_path, val_string_n(arena, current_file, len));
            const char *script_dir = val_string_n(arena, current_file, len).sval;
            const char *dir_base = strrchr(script_dir, '/');
            const char *dir_name = dir_base ? dir_base + 1 : script_dir;
            if (strcmp(dir_name, "exe") == 0) {
                const char *project_root = path_dirname(arena, script_dir);
                const char *sibling_lib = path_join2(arena, project_root, "lib");
                if (path_exists(sibling_lib)) {
                    val_array_push(&load_path, val_string(arena, sibling_lib));
                }
            }
        }
    }
    if (ev->runtime_root) {
        const char *source_rbconfig = path_join2(arena, ev->runtime_root, "rbconfig.rb");
        if (path_exists(source_rbconfig)) {
            val_array_push(&load_path, val_string(arena, ev->runtime_root));
        } else {
            const char *ruby_root = path_join2(arena, path_join2(arena, ev->runtime_root, "lib"), "ruby");
            val_array_push(&load_path, val_string(arena, path_join2(arena, ruby_root, STONED_RUBY_VERSION)));
            val_array_push(&load_path, val_string(arena, ruby_root));
        }
    }
    global_set(arena, &ev->globals, "LOAD_PATH", load_path);
    global_set(arena, &ev->globals, ":", load_path);   /* $: alias for $LOAD_PATH */
    Value loaded_features = val_array_new();
    global_set(arena, &ev->globals, "\"", loaded_features);           /* $" alias */
    global_set(arena, &ev->globals, "LOADED_FEATURES", loaded_features); /* $LOADED_FEATURES */

    static const char *builtins[] = {
        "Object", "BasicObject", "Numeric",
        "Integer", "Float", "String", "Symbol",
        "Array", "Hash", "Range", "NilClass", "TrueClass", "FalseClass", "IO", "File", "Dir", "Time", "Binding", "Struct", "Pathname",
        "Class", "Module", "Method", "UnboundMethod", "Proc", "Regexp", "MatchData", "Comparable", "Enumerable", "Kernel",
        "Thread", "Process",
        "Exception", "StandardError", "RuntimeError",
        "ArgumentError", "TypeError", "NameError", "NoMethodError", "RegexpError",
        "ZeroDivisionError", "LocalJumpError", "KeyError", "LoadError", "StopIteration", "EOFError",
        "IndexError", "SystemExit", "SystemStackError", "IOError", "EncodingError", "FrozenError",
        "SystemCallError", "SignalException", "Interrupt", "SyntaxError", "ScriptError",
        "NotImplementedError", "FiberError", "Fiber",
        NULL
    };
    for (int i = 0; builtins[i]; i++) {
        Value klass = val_class(arena, builtins[i], val_nil());
        klass.klass->class_env = env_new(arena, ev->top_env, 1);
        if (strcmp(builtins[i], "Comparable") == 0 || strcmp(builtins[i], "Enumerable") == 0 ||
            strcmp(builtins[i], "Kernel") == 0)
            klass.klass->is_module = 1;
        if (strcmp(builtins[i], "Thread") == 0 || strcmp(builtins[i], "Process") == 0)
            klass.klass->is_module = 1;
        env_define(arena, ev->top_env, builtins[i], klass);
    }

    {
        Value thread_mod;
        if (env_get(ev->top_env, "Thread", &thread_mod) && thread_mod.kind == VAL_CLASS) {
            Value mutex_class = val_class(arena, "Thread::Mutex", val_nil());
            mutex_class.klass->class_env = env_new(arena, ev->top_env, 1);
            env_define(arena, ev->top_env, "Thread::Mutex", mutex_class);
            env_define(arena, thread_mod.klass->class_env, "Mutex", mutex_class);
            /* Also expose Mutex at top level for convenience */
            env_define(arena, ev->top_env, "Mutex", mutex_class);
        }
    }

    /* Wire up the class hierarchy superclasses */
    {
        static const char *hierarchy[][2] = {
            {"Integer",   "Numeric"},  {"Float",    "Numeric"},
            {"Numeric",   "Object"},   {"String",   "Object"},
            {"Symbol",    "Object"},   {"Array",    "Object"},
            {"Hash",      "Object"},   {"Range",    "Object"},
            {"Regexp",    "Object"},   {"NilClass", "Object"},
            {"TrueClass", "Object"},   {"FalseClass","Object"},
            {"IO",        "Object"},   {"File",     "IO"},
            {"Proc",      "Object"},   {"Method",   "Object"},
            {"UnboundMethod","Object"},{"Binding",  "Object"},
            {"Exception", "Object"},   {"StandardError","Exception"},
            {"RuntimeError","StandardError"}, {"TypeError","StandardError"},
            {"ArgumentError","StandardError"},{"NameError","StandardError"},
            {"NoMethodError","NameError"},    {"StopIteration","StandardError"},
            {"RangeError","StandardError"},   {"IOError","StandardError"},
            {"ZeroDivisionError","StandardError"},
            {"NotImplementedError","StandardError"},
            {"EncodingError","StandardError"},{"FrozenError","RuntimeError"},
            {"LoadError","StandardError"},    {"SyntaxError","StandardError"},
            {"ScriptError","StandardError"},  {"NotImplementedError","ScriptError"},
            {"SystemExit","Exception"},        {"IndexError","StandardError"},
            {"KeyError","IndexError"},
            {"SignalException","Exception"},  {"Interrupt","SignalException"},
            {"BasicObject","Object"},
            {"FiberError","StandardError"},   {"Fiber","Object"},
            {NULL, NULL}
        };
        for (int hi = 0; hierarchy[hi][0]; hi++) {
            Value child, parent;
            if (env_get(ev->top_env, hierarchy[hi][0], &child) && child.kind == VAL_CLASS &&
                env_get(ev->top_env, hierarchy[hi][1], &parent) && parent.kind == VAL_CLASS) {
                child.klass->superclass = parent;
            }
        }
    }

    {
        Value argv_val = val_array_new();
        for (int i = 0; i < script_argc; i++)
            val_array_push(&argv_val, val_string(arena, script_argv[i]));
        env_define(arena, ev->top_env, "ARGV", argv_val);
    }
    env_define(arena, ev->top_env, "RUBY_ENGINE", val_string(arena, STONED_ENGINE_NAME));
    env_define(arena, ev->top_env, "RUBY_VERSION", val_string(arena, STONED_RUBY_VERSION));
    env_define(arena, ev->top_env, "RUBY_PLATFORM", val_string(arena, STONED_RUBY_PLATFORM));
    env_define(arena, ev->top_env, "RUBY_DESCRIPTION",
               val_string(arena, STONED_ENGINE_NAME " " STONED_BUILD_VERSION " (ruby " STONED_RUBY_VERSION ")"));
    env_define(arena, ev->top_env, "RUBY_ENGINE_VERSION", val_string(arena, STONED_BUILD_VERSION));
    env_define(arena, ev->top_env, "RUBY_PATCHLEVEL", val_int(-1));
    env_define(arena, ev->top_env, "RUBY_REVISION", val_string(arena, "0"));
    env_define(arena, ev->top_env, "RUBY_RELEASE_DATE", val_string(arena, STONED_RELEASE_DATE));
    env_define(arena, ev->top_env, "RUBY_COPYRIGHT", val_string(arena, "stoned - Copyright (C) 2026"));

    /* Standard Ruby global variables */
    global_set(arena, &ev->globals, "/", val_string(arena, "\n"));  /* $/ record separator */
    global_set(arena, &ev->globals, "\\", val_nil());               /* $\ output record separator */
    global_set(arena, &ev->globals, ",", val_nil());                /* $, field separator */
    global_set(arena, &ev->globals, ";", val_nil());                /* $; default split separator */
    if (current_file) {
        global_set(arena, &ev->globals, "0", val_string(arena, current_file));
        global_set(arena, &ev->globals, "PROGRAM_NAME", val_string(arena, current_file));
    }

    {
        Value marshal_mod = val_class(arena, "Marshal", val_nil());
        marshal_mod.klass->class_env = env_new(arena, ev->top_env, 1);
        marshal_mod.klass->is_module = 1;
        env_define(arena, marshal_mod.klass->class_env, "MAJOR_VERSION", val_int(4));
        env_define(arena, marshal_mod.klass->class_env, "MINOR_VERSION", val_int(8));
        env_define(arena, ev->top_env, "Marshal", marshal_mod);
    }

    {
        Value env_hash = val_hash_new(arena);
        for (char **entry = environ; entry && *entry; entry++) {
            const char *eq = strchr(*entry, '=');
            if (!eq) continue;
            size_t key_len = (size_t)(eq - *entry);
            if (key_len == 0) continue;
            char *key = arena_alloc(arena, key_len + 1);
            memcpy(key, *entry, key_len);
            key[key_len] = '\0';
            val_hash_set(env_hash.hash, val_string(arena, key), val_string(arena, eq + 1));
        }
        env_define(arena, ev->top_env, "ENV", env_hash);
    }

    {
        Value singleton_mod = val_class(arena, "Singleton", val_nil());
        singleton_mod.klass->class_env = env_new(arena, ev->top_env, 1);
        singleton_mod.klass->is_module = 1;
        env_define(arena, ev->top_env, "Singleton", singleton_mod);
    }

    {
        Value prism_mod = val_class(arena, "Prism", val_nil());
        prism_mod.klass->class_env = env_new(arena, ev->top_env, 1);
        prism_mod.klass->is_module = 1;
        env_define(arena, ev->top_env, "Prism", prism_mod);

        static const char *prism_classes[] = {
            "Visitor",
            "CallNode",
            "MatchWriteNode",
            "EmbeddedStatementsNode",
            "EmbeddedVariableNode",
            "StringNode",
            NULL
        };
        for (int i = 0; prism_classes[i]; i++) {
            const char *name = prism_classes[i];
            const char *full = qualify_const_name(arena, "Prism", name);
            Value klass = val_class(arena, full, val_nil());
            klass.klass->class_env = env_new(arena, prism_mod.klass->class_env, 1);
            env_define(arena, prism_mod.klass->class_env, name, klass);
            env_define(arena, ev->top_env, full, klass);
        }
    }

    {
        Value reline_mod = val_class(arena, "Reline", val_nil());
        reline_mod.klass->class_env = env_new(arena, ev->top_env, 1);
        reline_mod.klass->is_module = 1;
        env_define(arena, ev->top_env, "Reline", reline_mod);
        env_define(arena, reline_mod.klass->class_env, "VERSION", val_string(arena, "0.0.0"));
        env_define(arena, reline_mod.klass->class_env, "HISTORY", val_array_new());
        env_define(arena, reline_mod.klass->class_env, "DEFAULT_DIALOG_CONTEXT", val_nil());

        static const char *reline_modules[] = { "Unicode", "IOGate", NULL };
        for (int i = 0; reline_modules[i]; i++) {
            const char *name = reline_modules[i];
            const char *full = qualify_const_name(arena, "Reline", name);
            Value mod = val_class(arena, full, val_nil());
            mod.klass->class_env = env_new(arena, reline_mod.klass->class_env, 1);
            mod.klass->is_module = 1;
            env_define(arena, reline_mod.klass->class_env, name, mod);
            env_define(arena, ev->top_env, full, mod);
        }

        static const char *reline_classes[] = { "Config", "CursorPos", "DialogRenderInfo", NULL };
        for (int i = 0; reline_classes[i]; i++) {
            const char *name = reline_classes[i];
            const char *full = qualify_const_name(arena, "Reline", name);
            Value klass = val_class(arena, full, val_nil());
            klass.klass->class_env = env_new(arena, reline_mod.klass->class_env, 1);
            env_define(arena, reline_mod.klass->class_env, name, klass);
            env_define(arena, ev->top_env, full, klass);
        }
    }

    {
        Value sw_mod = val_class(arena, "Shellwords", val_nil());
        sw_mod.klass->class_env = env_new(arena, ev->top_env, 1);
        sw_mod.klass->is_module = 1;
        env_define(arena, ev->top_env, "Shellwords", sw_mod);
    }
    {
        Value sio_class = val_class(arena, "StringIO", val_nil());
        sio_class.klass->class_env = env_new(arena, ev->top_env, 1);
        env_define(arena, ev->top_env, "StringIO", sio_class);
    }
    {
        Value open3_mod = val_class(arena, "Open3", val_nil());
        open3_mod.klass->class_env = env_new(arena, ev->top_env, 1);
        open3_mod.klass->is_module = 1;
        env_define(arena, ev->top_env, "Open3", open3_mod);
    }
    {
        Value math_mod = val_class(arena, "Math", val_nil());
        math_mod.klass->class_env = env_new(arena, ev->top_env, 1);
        math_mod.klass->is_module = 1;
        env_define(arena, ev->top_env, "Math", math_mod);
        /* Math constants */
        Value pi = {0}; pi.kind = VAL_FLOAT; pi.fval = 3.14159265358979323846;
        env_define(arena, math_mod.klass->class_env, "PI", pi);
        Value e = {0}; e.kind = VAL_FLOAT; e.fval = 2.71828182845904523536;
        env_define(arena, math_mod.klass->class_env, "E", e);
    }
    {
        Value pp_class = val_class(arena, "PP", val_nil());
        pp_class.klass->class_env = env_new(arena, ev->top_env, 1);
        env_define(arena, ev->top_env, "PP", pp_class);
    }
    {
        Value enc_class = val_class(arena, "Encoding", val_nil());
        enc_class.klass->class_env = env_new(arena, ev->top_env, 1);
        /* Common encoding constants — const_key is the Ruby constant name (underscored),
           canonical_name is what .name/.to_s returns (Ruby standard dash form). */
        static const struct { const char *const_key; const char *canonical_name; } enc_table[] = {
            {"UTF_8",       "UTF-8"},
            {"US_ASCII",    "US-ASCII"},
            {"ASCII_8BIT",  "ASCII-8BIT"},
            {"BINARY",      "ASCII-8BIT"},
            {"EUC_JP",      "EUC-JP"},
            {"Windows_31J", "Windows-31J"},
            {"ISO_8859_1",  "ISO-8859-1"},
            {"UTF_16BE",    "UTF-16BE"},
            {"UTF_16LE",    "UTF-16LE"},
            {"UTF_32BE",    "UTF-32BE"},
            {"UTF_32LE",    "UTF-32LE"},
            {NULL, NULL}
        };
        for (int i = 0; enc_table[i].const_key; i++) {
            Value obj = val_object(arena, enc_class);
            val_object_set_ivar(arena, obj, "name", val_string(arena, enc_table[i].canonical_name));
            env_define(arena, enc_class.klass->class_env, enc_table[i].const_key, obj);
        }
        env_define(arena, ev->top_env, "Encoding", enc_class);
    }

    Value exception, standard_error, runtime_error, argument_error, type_error, name_error, no_method_error, regexp_error;
    Value zero_division_error, local_jump_error, key_error, load_error, stop_iteration, eof_error, system_stack_error, io_error, encoding_error, frozen_error, system_call_error;
    if (env_get(ev->top_env, "Exception", &exception) && exception.kind == VAL_CLASS &&
        env_get(ev->top_env, "StandardError", &standard_error) && standard_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "RuntimeError", &runtime_error) && runtime_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "ArgumentError", &argument_error) && argument_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "TypeError", &type_error) && type_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "NameError", &name_error) && name_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "NoMethodError", &no_method_error) && no_method_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "RegexpError", &regexp_error) && regexp_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "ZeroDivisionError", &zero_division_error) && zero_division_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "LocalJumpError", &local_jump_error) && local_jump_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "KeyError", &key_error) && key_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "LoadError", &load_error) && load_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "StopIteration", &stop_iteration) && stop_iteration.kind == VAL_CLASS &&
        env_get(ev->top_env, "EOFError", &eof_error) && eof_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "SystemStackError", &system_stack_error) && system_stack_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "IOError", &io_error) && io_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "EncodingError", &encoding_error) && encoding_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "FrozenError", &frozen_error) && frozen_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "SystemCallError", &system_call_error) && system_call_error.kind == VAL_CLASS) {
        standard_error.klass->superclass = exception;
        runtime_error.klass->superclass = standard_error;
        argument_error.klass->superclass = standard_error;
        type_error.klass->superclass = standard_error;
        name_error.klass->superclass = standard_error;
        no_method_error.klass->superclass = standard_error;
        regexp_error.klass->superclass = standard_error;
        zero_division_error.klass->superclass = standard_error;
        local_jump_error.klass->superclass = standard_error;
        key_error.klass->superclass = standard_error;
        load_error.klass->superclass = standard_error;
        stop_iteration.klass->superclass = standard_error;
        eof_error.klass->superclass = standard_error;
        system_stack_error.klass->superclass = standard_error;
        io_error.klass->superclass = standard_error;
        encoding_error.klass->superclass = standard_error;
        frozen_error.klass->superclass = runtime_error;
        no_method_error.klass->superclass = name_error;
        system_call_error.klass->superclass = standard_error;
    }

    {
        Value scerr;
        if (env_get(ev->top_env, "SystemCallError", &scerr) && scerr.kind == VAL_CLASS) {
            Value errno_mod = val_class(arena, "Errno", val_nil());
            errno_mod.klass->is_module = 1;
            errno_mod.klass->class_env = env_new(arena, ev->top_env, 1);
            env_define(arena, ev->top_env, "Errno", errno_mod);

            static const char *short_names[] = { "ENOENT", "EACCES", "EEXIST", "EBADF", "EPERM", "EINVAL", "ESPIPE", NULL };
            static const char *full_names[]  = { "Errno::ENOENT", "Errno::EACCES", "Errno::EEXIST", "Errno::EBADF", "Errno::EPERM", "Errno::EINVAL", "Errno::ESPIPE", NULL };
            for (int i = 0; short_names[i]; i++) {
                Value ec = val_class(arena, full_names[i], scerr);
                ec.klass->class_env = env_new(arena, ev->top_env, 1);
                env_define(arena, ev->top_env, full_names[i], ec);
                env_define(arena, errno_mod.klass->class_env, short_names[i], ec);
            }
        }
    }

    Value io_class;
    if (env_get(ev->top_env, "IO", &io_class) && io_class.kind == VAL_CLASS) {
        static const char *fds[] = { "stdout", "stderr", "stdin", NULL };
        static const char *consts[] = { "STDOUT", "STDERR", "STDIN", NULL };
        static FILE *streams[] = { NULL, NULL, NULL };
        streams[0] = stdout;
        streams[1] = stderr;
        streams[2] = stdin;
        static int fd_nums[] = { 1, 2, 0 };
        static const char *modes[] = { "w", "w", "r" };
        static int sync_defaults[] = { 0, 1, 0 };
        for (int i = 0; fds[i]; i++) {
            Value obj = val_object(arena, io_class);
            val_object_set_ivar(arena, obj, "__fd__", val_string(arena, fds[i]));
            val_object_set_ivar(arena, obj, "__fd_num__", val_int(fd_nums[i]));
            val_object_set_ivar(arena, obj, "mode", val_string(arena, modes[i]));
            val_object_set_ivar(arena, obj, "closed", val_false());
            val_object_set_ivar(arena, obj, "sync", val_bool(sync_defaults[i]));
            obj.obj->native = alloc_native_file(arena, streams[i], 0);
            global_set(arena, &ev->globals, fds[i], obj);
            env_define(arena, ev->top_env, consts[i], obj);
        }
        /* $> and $stdout are aliases */
        Value stdout_val;
        if (global_get(&ev->globals, "stdout", &stdout_val))
            global_set(arena, &ev->globals, ">", stdout_val);
    }
    env_define(arena, ev->top_env, "SEEK_SET", val_int(0));
    env_define(arena, ev->top_env, "SEEK_CUR", val_int(1));
    env_define(arena, ev->top_env, "SEEK_END", val_int(2));

    static const char *prelude_comparable =
        "module Comparable\n"
        "  def <(other)\n"
        "    (self <=> other) < 0\n"
        "  end\n"
        "  def <=(other)\n"
        "    (self <=> other) <= 0\n"
        "  end\n"
        "  def >(other)\n"
        "    (self <=> other) > 0\n"
        "  end\n"
        "  def >=(other)\n"
        "    (self <=> other) >= 0\n"
        "  end\n"
        "\n"
        "  def ==(other)\n"
        "    return false if other.nil?\n"
        "    (self <=> other) == 0\n"
        "  rescue\n"
        "    false\n"
        "  end\n"
        "\n"
        "  def between?(min, max)\n"
        "    self >= min && self <= max\n"
        "  end\n"
        "\n"
        "  def clamp(min, max)\n"
        "    return min if self < min\n"
        "    return max if self > max\n"
        "    self\n"
        "  end\n"
        "end\n"
        "\n";

    static const char *prelude_enumerable =
        "module Enumerable\n"
        "  def find\n"
        "    result = nil\n"
        "    each do |x|\n"
        "      if yield x\n"
        "        result = x\n"
        "        break\n"
        "      end\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def detect\n"
        "    result = nil\n"
        "    each do |x|\n"
        "      if yield x\n"
        "        result = x\n"
        "        break\n"
        "      end\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def entries\n"
        "    result = []\n"
        "    each do |x|\n"
        "      result << x\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def to_a\n"
        "    entries\n"
        "  end\n"
        "\n"
        "  def first(n = nil)\n"
        "    if n.nil?\n"
        "      each { |x| return x }\n"
        "      nil\n"
        "    else\n"
        "      result = []\n"
        "      each { |x| break if result.length >= n; result << x }\n"
        "      result\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def take(n)\n"
        "    result = []\n"
        "    each do |x|\n"
        "      break result if result.length >= n\n"
        "      result << x\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def drop(n)\n"
        "    result = []\n"
        "    i = 0\n"
        "    each do |x|\n"
        "      result << x if i >= n\n"
        "      i = i + 1\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def count\n"
        "    if block_given?\n"
        "      n = 0\n"
        "      each do |x|\n"
        "        n = n + 1 if yield x\n"
        "      end\n"
        "      return n\n"
        "    end\n"
        "    entries.length\n"
        "  end\n"
        "\n"
        "  def map\n"
        "    result = []\n"
        "    each do |x|\n"
        "      result << yield(x)\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def collect\n"
        "    map { |x| yield x }\n"
        "  end\n"
        "\n"
        "  def select\n"
        "    result = []\n"
        "    each do |x|\n"
        "      result << x if yield(x)\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def reject\n"
        "    result = []\n"
        "    each do |x|\n"
        "      result << x if !yield(x)\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def reduce(*args)\n"
        "    sym = nil\n"
        "    init = nil\n"
        "    has_init = false\n"
        "    if args.length == 2\n"
        "      init = args[0]; has_init = true; sym = args[1]\n"
        "    elsif args.length == 1 && args[0].is_a?(Symbol)\n"
        "      sym = args[0]\n"
        "    elsif args.length == 1\n"
        "      init = args[0]; has_init = true\n"
        "    end\n"
        "    started = has_init\n"
        "    acc = init\n"
        "    each do |x|\n"
        "      if started\n"
        "        acc = sym ? acc.send(sym, x) : yield(acc, x)\n"
        "      else\n"
        "        acc = x\n"
        "        started = true\n"
        "      end\n"
        "    end\n"
        "    acc\n"
        "  end\n"
        "\n"
        "  def inject(*args, &blk)\n"
        "    if blk\n"
        "      reduce(*args) { |acc, x| blk.call(acc, x) }\n"
        "    else\n"
        "      reduce(*args)\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def any?\n"
        "    each do |x|\n"
        "      return true if block_given? ? yield(x) : x\n"
        "    end\n"
        "    false\n"
        "  end\n"
        "\n"
        "  def all?\n"
        "    each do |x|\n"
        "      return false if block_given? ? !yield(x) : !x\n"
        "    end\n"
        "    true\n"
        "  end\n"
        "\n"
        "  def none?\n"
        "    each do |x|\n"
        "      return false if block_given? ? yield(x) : x\n"
        "    end\n"
        "    true\n"
        "  end\n"
        "\n"
        "  def include?(item)\n"
        "    each do |x|\n"
        "      return true if x == item\n"
        "    end\n"
        "    false\n"
        "  end\n"
        "\n"
        "  def member?(item)\n"
        "    include?(item)\n"
        "  end\n"
        "\n"
        "  def min\n"
        "    found = false\n"
        "    best = nil\n"
        "    each do |x|\n"
        "      if !found\n"
        "        best = x\n"
        "        found = true\n"
        "      elsif block_given?\n"
        "        best = x if yield(x, best) < 0\n"
        "      else\n"
        "        best = x if (x <=> best) < 0\n"
        "      end\n"
        "    end\n"
        "    found ? best : nil\n"
        "  end\n"
        "\n"
        "  def max\n"
        "    found = false\n"
        "    best = nil\n"
        "    each do |x|\n"
        "      if !found\n"
        "        best = x\n"
        "        found = true\n"
        "      elsif block_given?\n"
        "        best = x if yield(x, best) > 0\n"
        "      else\n"
        "        best = x if (x <=> best) > 0\n"
        "      end\n"
        "    end\n"
        "    found ? best : nil\n"
        "  end\n"
        "\n"
        "  def sort\n"
        "    return entries.sort unless block_given?\n"
        "    entries.sort { |a, b| yield a, b }\n"
        "  end\n"
        "\n"
        "  def sum(init = 0)\n"
        "    acc = init\n"
        "    each do |x|\n"
        "      acc = acc + (block_given? ? yield(x) : x)\n"
        "    end\n"
        "    acc\n"
        "  end\n"
        "\n"
        "  def flat_map\n"
        "    result = []\n"
        "    each do |x|\n"
        "      y = yield(x)\n"
        "      if y.is_a?(Array)\n"
        "        y.each do |v|\n"
        "          result << v\n"
        "        end\n"
        "      else\n"
        "        result << y\n"
        "      end\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def each_with_object(obj)\n"
        "    return to_enum(:each_with_object, obj) unless block_given?\n"
        "    each do |x|\n"
        "      yield x, obj\n"
        "    end\n"
        "    obj\n"
        "  end\n"
        "\n"
        "  def min_by\n"
        "    found = false\n"
        "    best = nil\n"
        "    best_key = nil\n"
        "    each do |x|\n"
        "      key = yield(x)\n"
        "      if !found || (key <=> best_key) < 0\n"
        "        best = x\n"
        "        best_key = key\n"
        "        found = true\n"
        "      end\n"
        "    end\n"
        "    found ? best : nil\n"
        "  end\n"
        "\n"
        "  def max_by\n"
        "    found = false\n"
        "    best = nil\n"
        "    best_key = nil\n"
        "    each do |x|\n"
        "      key = yield(x)\n"
        "      if !found || (key <=> best_key) > 0\n"
        "        best = x\n"
        "        best_key = key\n"
        "        found = true\n"
        "      end\n"
        "    end\n"
        "    found ? best : nil\n"
        "  end\n"
        "\n"
        "  def sort_by\n"
        "    pairs = map { |x| [yield(x), x] }\n"
        "    pairs.sort { |a, b| a[0] <=> b[0] }.map { |pair| pair[1] }\n"
        "  end\n"
        "\n"
        "  def zip(*others)\n"
        "    entries.zip(*others)\n"
        "  end\n"
        "\n"
        "  def group_by\n"
        "    result = {}\n"
        "    each do |x|\n"
        "      key = yield(x)\n"
        "      bucket = result[key]\n"
        "      if bucket == nil\n"
        "        bucket = []\n"
        "        result[key] = bucket\n"
        "      end\n"
        "      bucket << x\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def tally(hash = nil)\n"
        "    result = hash || {}\n"
        "    each do |x|\n"
        "      n = result[x]\n"
        "      result[x] = n == nil ? 1 : n + 1\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def minmax\n"
        "    mn = nil; mx = nil; found = false\n"
        "    each do |x|\n"
        "      if !found\n"
        "        mn = mx = x; found = true\n"
        "      else\n"
        "        cmp_mn = block_given? ? yield(x, mn) : (x <=> mn)\n"
        "        cmp_mx = block_given? ? yield(x, mx) : (x <=> mx)\n"
        "        mn = x if cmp_mn.is_a?(Integer) && cmp_mn < 0\n"
        "        mx = x if cmp_mx.is_a?(Integer) && cmp_mx > 0\n"
        "      end\n"
        "    end\n"
        "    [mn, mx]\n"
        "  end\n"
        "\n"
        "  def minmax_by\n"
        "    mn = nil; mx = nil; mn_key = nil; mx_key = nil; found = false\n"
        "    each do |x|\n"
        "      key = yield(x)\n"
        "      if !found\n"
        "        mn = mx = x; mn_key = mx_key = key; found = true\n"
        "      else\n"
        "        if (key <=> mn_key) < 0; mn = x; mn_key = key; end\n"
        "        if (key <=> mx_key) > 0; mx = x; mx_key = key; end\n"
        "      end\n"
        "    end\n"
        "    [mn, mx]\n"
        "  end\n"
        "\n"
        "  def partition\n"
        "    left = []; right = []\n"
        "    each do |x|\n"
        "      if yield(x); left << x; else; right << x; end\n"
        "    end\n"
        "    [left, right]\n"
        "  end\n"
        "\n"
        "  def each_with_index\n"
        "    return to_enum(:each_with_index) unless block_given?\n"
        "    i = 0\n"
        "    each do |x|\n"
        "      yield x, i\n"
        "      i = i + 1\n"
        "    end\n"
        "    self\n"
        "  end\n"
        "\n"
        "  def filter_map\n"
        "    result = []\n"
        "    each do |x|\n"
        "      v = yield(x)\n"
        "      result << v if v\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def each_slice(n)\n"
        "    return to_enum(:each_slice, n) unless block_given?\n"
        "    slice = []\n"
        "    each do |x|\n"
        "      slice << x\n"
        "      if slice.length == n\n"
        "        yield slice\n"
        "        slice = []\n"
        "      end\n"
        "    end\n"
        "    yield slice unless slice.empty?\n"
        "    nil\n"
        "  end\n"
        "\n"
        "  def each_cons(n)\n"
        "    return to_enum(:each_cons, n) unless block_given?\n"
        "    window = []\n"
        "    each do |x|\n"
        "      window << x\n"
        "      if window.length == n\n"
        "        yield window.dup\n"
        "        window.shift\n"
        "      end\n"
        "    end\n"
        "    nil\n"
        "  end\n"
        "\n"
        "  def take_while\n"
        "    result = []\n"
        "    each do |x|\n"
        "      break unless yield(x)\n"
        "      result << x\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "\n"
        "  def drop_while\n"
        "    result = []\n"
        "    dropping = true\n"
        "    each do |x|\n"
        "      if dropping && yield(x)\n"
        "        next\n"
        "      else\n"
        "        dropping = false\n"
        "        result << x\n"
        "      end\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "end\n"
        "\n";

    static const char *prelude_core =
        "class Object\n"
        "  def method_missing(name, *args)\n"
        "    raise NoMethodError, \"undefined method '#{name}' for #{self.class}\"\n"
        "  end\n"
        "  private :method_missing\n"
        "  def respond_to_missing?(name, include_private = false)\n"
        "    false\n"
        "  end\n"
        "  private :respond_to_missing?\n"
        "  def to_enum(meth = :each, *args)\n"
        "    obj = self\n"
        "    Enumerator.new do |y|\n"
        "      obj.send(meth, *args) { |*vals| y.yield(*vals) }\n"
        "    end\n"
        "  end\n"
        "  alias enum_for to_enum\n"
        "end\n"
        "class Numeric\n"
        "  include Comparable\n"
        "end\n"
        "class Integer\n"
        "  def self.sqrt(n)\n"
        "    raise ArgumentError, 'Integer#sqrt requires non-negative integer' if n < 0\n"
        "    Math.sqrt(n.to_f).floor\n"
        "  end\n"
        "end\n"
        "class Float\n"
        "  INFINITY = (1.0/0.0)\n"
        "  NAN = (0.0/0.0)\n"
        "end\n"
        "class String\n"
        "  include Comparable\n"
        "end\n"
        "class Array\n"
        "  include Enumerable\n"
        "end\n"
        "class Hash\n"
        "  include Enumerable\n"
        "end\n"
        "class Range\n"
        "  include Enumerable\n"
        "end\n"
        /* Enumerator class — position-based iteration with Lazy nested class */
        "class Enumerator\n"
        "  include Enumerable\n"
        "  class Yielder\n"
        "    def initialize; @values = []; end\n"
        "    def <<(v); @values << v; self; end\n"
        "    def yield(*args); args.each { |v| @values << v }; self; end\n"
        "    def to_a; @values.dup; end\n"
        "  end\n"
        "  def initialize(arr = nil, &blk)\n"
        "    if arr && arr.respond_to?(:to_a)\n"
        "      @arr = arr.to_a\n"
        "    elsif blk\n"
        "      yielder = Yielder.new\n"
        "      blk.call(yielder)\n"
        "      @arr = yielder.to_a\n"
        "    else\n"
        "      @arr = []\n"
        "    end\n"
        "    @pos = 0\n"
        "  end\n"
        "  def next\n"
        "    raise StopIteration, 'iteration reached an end' if @pos >= @arr.length\n"
        "    v = @arr[@pos]; @pos += 1; v\n"
        "  end\n"
        "  def peek\n"
        "    raise StopIteration, 'iteration reached an end' if @pos >= @arr.length\n"
        "    @arr[@pos]\n"
        "  end\n"
        "  def rewind; @pos = 0; self; end\n"
        "  def each(&blk)\n"
        "    if blk; @arr.each(&blk)\n"
        "    else; self\n"
        "    end\n"
        "  end\n"
        "  def size; @arr.length; end\n"
        "  def with_index(offset = 0, &blk)\n"
        "    unless blk\n"
        "      i = offset\n"
        "      return Enumerator.new(@arr.map { |x| pair = [x, i]; i += 1; pair })\n"
        "    end\n"
        "    i = offset\n"
        "    @arr.each { |x| blk.call(x, i); i += 1 }\n"
        "    self\n"
        "  end\n"
        "  def with_object(obj, &blk)\n"
        "    return Enumerator.new(@arr.map { |x| [x, obj] }) unless blk\n"
        "    @arr.each { |x| blk.call(x, obj) }\n"
        "    obj\n"
        "  end\n"
        "  def to_a; @arr.dup; end\n"
        "  def next_values\n"
        "    [self.next]\n"
        "  end\n"
        "  def peek_values\n"
        "    [self.peek]\n"
        "  end\n"
        "  class Lazy\n"
        "    def initialize(src, &blk)\n"
        "      @src = src\n"
        "      @ops = blk ? [[:select_map, blk]] : []\n"
        "    end\n"
        "    def select(&blk); lazy = Lazy.new(@src); lazy.instance_variable_set(:@ops, @ops + [[:select, blk]]); lazy; end\n"
        "    def filter(&blk); select(&blk); end\n"
        "    def map(&blk);    lazy = Lazy.new(@src); lazy.instance_variable_set(:@ops, @ops + [[:map, blk]]); lazy; end\n"
        "    def reject(&blk); lazy = Lazy.new(@src); lazy.instance_variable_set(:@ops, @ops + [[:reject, blk]]); lazy; end\n"
        "    def take(n);      lazy = Lazy.new(@src); lazy.instance_variable_set(:@ops, @ops + [[:take, n]]); lazy; end\n"
        "    def _collect(cap)\n"
        "      result = []\n"
        "      take_limit = nil\n"
        "      @ops.each { |op, arg| take_limit = arg if op == :take }\n"
        "      limit = [cap, take_limit].compact.min\n"
        "      @src.each do |x|\n"
        "        val = x\n"
        "        skip = false\n"
        "        @ops.each do |op, arg|\n"
        "          case op\n"
        "          when :select then skip = true unless arg.call(val)\n"
        "          when :reject then skip = true if arg.call(val)\n"
        "          when :map then val = arg.call(val) unless skip\n"
        "          end\n"
        "          break if skip\n"
        "        end\n"
        "        result << val unless skip\n"
        "        break if limit && result.size >= limit\n"
        "      end\n"
        "      result\n"
        "    end\n"
        "    def first(n = nil)\n"
        "      result = _collect(n || 1)\n"
        "      n.nil? ? result.first : result\n"
        "    end\n"
        "    def to_a; _collect(nil); end\n"
        "    def force; to_a; end\n"
        "  end\n"
        "end\n"
        "class Array\n"
        "  def lazy; Enumerator::Lazy.new(self); end\n"
        "end\n"
        "class Range\n"
        "  def lazy; Enumerator::Lazy.new(self); end\n"
        "end\n"
        "class Exception\n"
        "  def initialize(msg = nil)\n"
        "    if msg\n"
        "      instance_variable_set(:@__exc_message__, msg.to_s)\n"
        "      instance_variable_set(:message, msg.to_s)\n"
        "    end\n"
        "  end\n"
        "end\n"
        "class Proc\n"
        "  def >>(other)\n"
        "    first = self\n"
        "    lambda { |*args| other.call(first.call(*args)) }\n"
        "  end\n"
        "  def <<(other)\n"
        "    last = self\n"
        "    lambda { |*args| last.call(other.call(*args)) }\n"
        "  end\n"
        "  def curry(needed_arity = nil)\n"
        "    original = self\n"
        "    ar = original.arity\n"
        "    needed = needed_arity ? needed_arity.to_i : (ar < 0 ? (-ar - 1) : ar)\n"
        "    make_curried = lambda { |accum|\n"
        "      lambda { |*args|\n"
        "        na = accum + args\n"
        "        na.length >= needed ? original.call(*na) : make_curried.(na)\n"
        "      }\n"
        "    }\n"
        "    make_curried.([])\n"
        "  end\n"
        "end\n";

    static const char *prelude_file_constants =
        "class File\n"
        "  SEPARATOR = '/'\n"
        "  PATH_SEPARATOR = ':'\n"
        "  ALT_SEPARATOR = nil\n"
        "  FNM_NOESCAPE  = 0x01\n"
        "  FNM_PATHNAME  = 0x02\n"
        "  FNM_DOTMATCH  = 0x04\n"
        "  FNM_CASEFOLD  = 0x08\n"
        "  FNM_EXTGLOB   = 0x20\n"
        "end\n"
        "class IO\n"
        "  NULL = '/dev/null'\n"
        "end\n";

    static const char *prelude_rbconfig =
        "module RbConfig\n"
        "  RUBY_API_VERSION = RUBY_VERSION\n"
        "  CONFIG = {\n"
        "    'ruby_version' => RUBY_VERSION,\n"
        "    'arch' => RUBY_PLATFORM,\n"
        "    'host_os' => 'linux',\n"
        "    'MAJOR' => RUBY_VERSION.split('.')[0] || '4',\n"
        "    'MINOR' => RUBY_VERSION.split('.')[1] || '0',\n"
        "    'TEENY' => '0',\n"
        "  }\n"
        "end\n";

    static const char *prelude_marshal =
        "module Marshal\n"
        "  def self.load(src, proc = nil); {}; end\n"
        "  def self.restore(src); {}; end\n"
        "  def self.dump(obj, port = nil, limit = nil); ''; end\n"
        "end\n";

    static const char *prelude_signal =
        "module Signal\n"
        "  def self.trap(sig, *args, &blk); end\n"
        "  def self.list\n"
        "    {'HUP'=>1,'INT'=>2,'QUIT'=>3,'ILL'=>4,'TRAP'=>5,'ABRT'=>6,'IOT'=>6,\n"
        "     'BUS'=>7,'FPE'=>8,'KILL'=>9,'USR1'=>10,'SEGV'=>11,'USR2'=>12,\n"
        "     'PIPE'=>13,'ALRM'=>14,'TERM'=>15,'CHLD'=>17,'CONT'=>18,'STOP'=>19,\n"
        "     'TSTP'=>20,'TTIN'=>21,'TTOU'=>22,'WINCH'=>28}\n"
        "  end\n"
        "end\n"
        "module GC\n"
        "  def self.compact; nil; end\n"
        "  def self.start; nil; end\n"
        "  def self.stat; {}; end\n"
        "end\n"
        "module ObjectSpace\n"
        "  def self.each_object(klass = nil, &blk)\n"
        "    return Enumerator.new([]) unless blk\n"
        "    0\n"
        "  end\n"
        "  def self.count_objects; {}; end\n"
        "end\n";

    static const char *prelude_mutex_queue =
        "class Mutex\n"
        "  def initialize; @locked = false; end\n"
        "  def lock; @locked = true; self; end\n"
        "  def unlock; @locked = false; self; end\n"
        "  def locked?; @locked; end\n"
        "  def try_lock; return false if @locked; @locked = true; true; end\n"
        "  def synchronize; lock; begin; yield; ensure; unlock; end; end\n"
        "  def owned?; @locked; end\n"
        "end\n"
        "class Queue\n"
        "  def initialize; @arr = []; end\n"
        "  def push(v); @arr << v; self; end\n"
        "  alias enq push\n"
        "  alias << push\n"
        "  def pop(non_block = false)\n"
        "    raise ThreadError, 'queue empty' if non_block && @arr.empty?\n"
        "    @arr.shift\n"
        "  end\n"
        "  alias deq pop\n"
        "  alias shift pop\n"
        "  def size; @arr.size; end\n"
        "  alias length size\n"
        "  def empty?; @arr.empty?; end\n"
        "  def clear; @arr.clear; self; end\n"
        "  def num_waiting; 0; end\n"
        "  def close; self; end\n"
        "end\n"
        "class SizedQueue < Queue\n"
        "  def initialize(max); super(); @max = max; end\n"
        "  def max; @max; end\n"
        "end\n"
        "module Thread::Backtrace\n"
        "  class Location\n"
        "    def initialize(path, lineno, label)\n"
        "      @path = path; @lineno = lineno; @label = label\n"
        "    end\n"
        "    def path; @path; end\n"
        "    def absolute_path; @path; end\n"
        "    def lineno; @lineno; end\n"
        "    def label; @label; end\n"
        "    def base_label\n"
        "      l = @label.to_s\n"
        "      i = l.rindex('#') || l.rindex('.')\n"
        "      i ? l[(i + 1)..] : l\n"
        "    end\n"
        "    def to_s; \"#{@path}:#{@lineno}:in `#{@label}'\"; end\n"
        "    def inspect; to_s; end\n"
        "  end\n"
        "end\n";

    static const char *prelude_random =
        "class Random\n"
        "  def initialize(seed = nil)\n"
        "    @state = seed ? seed.to_i & 0xFFFFFFFFFFFFFFFF : (rand(0x7FFFFFFF) ^ object_id)\n"
        "    @state = 0 if @state == 0\n"
        "  end\n"
        "  def rand(n = nil)\n"
        "    @state = (@state * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF\n"
        "    r = @state >> 1\n"
        "    if n.nil?\n"
        "      r.to_f / 0x3FFFFFFFFFFFFFFF\n"
        "    elsif n.is_a?(Range)\n"
        "      lo = n.begin; hi = n.end\n"
        "      size = n.exclude_end? ? hi - lo : hi - lo + 1\n"
        "      lo + (r % size)\n"
        "    else\n"
        "      n = n.to_i\n"
        "      n <= 0 ? r.to_f / 0x3FFFFFFFFFFFFFFF : r % n\n"
        "    end\n"
        "  end\n"
        "  def bytes(n)\n"
        "    result = []\n"
        "    n.times { result << rand(256) }\n"
        "    result.pack('C*')\n"
        "  end\n"
        "  DEFAULT = new(0)\n"
        "  @default = DEFAULT\n"
        "  def self.rand(n = nil); @default.rand(n); end\n"
        "  def self.srand(seed = nil)\n"
        "    old = @default\n"
        "    @default = new(seed)\n"
        "    old.object_id\n"
        "  end\n"
        "  def self.new_seed; rand(0x7FFFFFFF); end\n"
        "end\n";

    static const char *prelude_process_status =
        "class Process\n"
        "  class Status\n"
        "    def initialize(code); @exitstatus = code.to_i; end\n"
        "    def exitstatus; @exitstatus; end\n"
        "    def success?; @exitstatus == 0; end\n"
        "    def to_i; @exitstatus; end\n"
        "    def ==(other); to_i == other.to_i; end\n"
        "    def to_s; \"pid exit #{@exitstatus}\"; end\n"
        "    def inspect; \"#<Process::Status: exit #{@exitstatus}>\"; end\n"
        "  end\n"
        "end\n";

    static const char *prelude_rational =
        "class Rational < Numeric\n"
        "  include Comparable\n"
        "  attr_reader :numerator, :denominator\n"
        "\n"
        "  def initialize(num, den = 1)\n"
        "    raise ZeroDivisionError, 'divided by 0' if den == 0\n"
        "    if num.is_a?(Float)\n"
        "      # Convert float to integer fraction via 1e7 denominator\n"
        "      scale = 10000000\n"
        "      num = (num * scale).round\n"
        "      den = den * scale\n"
        "    end\n"
        "    if den.is_a?(Float)\n"
        "      scale = 10000000\n"
        "      num = num * scale\n"
        "      den = (den * scale).round\n"
        "    end\n"
        "    if den < 0\n"
        "      num = -num\n"
        "      den = -den\n"
        "    end\n"
        "    g = num.abs.gcd(den)\n"
        "    @numerator = num / g\n"
        "    @denominator = den / g\n"
        "  end\n"
        "\n"
        "  def +(other)\n"
        "    case other\n"
        "    when Rational\n"
        "      Rational.new(@numerator * other.denominator + other.numerator * @denominator,\n"
        "                   @denominator * other.denominator)\n"
        "    when Integer\n"
        "      Rational.new(@numerator + other * @denominator, @denominator)\n"
        "    when Float\n"
        "      to_f + other\n"
        "    else\n"
        "      raise TypeError, \"#{other.class} can't be coerced into Rational\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def -(other)\n"
        "    case other\n"
        "    when Rational\n"
        "      Rational.new(@numerator * other.denominator - other.numerator * @denominator,\n"
        "                   @denominator * other.denominator)\n"
        "    when Integer\n"
        "      Rational.new(@numerator - other * @denominator, @denominator)\n"
        "    when Float\n"
        "      to_f - other\n"
        "    else\n"
        "      raise TypeError, \"#{other.class} can't be coerced into Rational\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def *(other)\n"
        "    case other\n"
        "    when Rational\n"
        "      Rational.new(@numerator * other.numerator, @denominator * other.denominator)\n"
        "    when Integer\n"
        "      Rational.new(@numerator * other, @denominator)\n"
        "    when Float\n"
        "      to_f * other\n"
        "    else\n"
        "      raise TypeError, \"#{other.class} can't be coerced into Rational\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def /(other)\n"
        "    case other\n"
        "    when Rational\n"
        "      raise ZeroDivisionError, 'divided by 0' if other.numerator == 0\n"
        "      Rational.new(@numerator * other.denominator, @denominator * other.numerator)\n"
        "    when Integer\n"
        "      raise ZeroDivisionError, 'divided by 0' if other == 0\n"
        "      Rational.new(@numerator, @denominator * other)\n"
        "    when Float\n"
        "      to_f / other\n"
        "    else\n"
        "      raise TypeError, \"#{other.class} can't be coerced into Rational\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def **(exp)\n"
        "    if exp.is_a?(Integer)\n"
        "      if exp >= 0\n"
        "        Rational.new(@numerator ** exp, @denominator ** exp)\n"
        "      else\n"
        "        Rational.new(@denominator ** (-exp), @numerator ** (-exp))\n"
        "      end\n"
        "    elsif exp.is_a?(Float)\n"
        "      to_f ** exp\n"
        "    else\n"
        "      raise TypeError, \"#{exp.class} can't be coerced into Integer\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def <=>(other)\n"
        "    case other\n"
        "    when Rational\n"
        "      (@numerator * other.denominator) <=> (other.numerator * @denominator)\n"
        "    when Integer\n"
        "      @numerator <=> (other * @denominator)\n"
        "    when Float\n"
        "      to_f <=> other\n"
        "    else\n"
        "      nil\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def ==(other)\n"
        "    case other\n"
        "    when Rational\n"
        "      @numerator == other.numerator && @denominator == other.denominator\n"
        "    when Integer\n"
        "      @denominator == 1 && @numerator == other\n"
        "    when Float\n"
        "      to_f == other\n"
        "    else\n"
        "      false\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def to_f; @numerator.to_f / @denominator.to_f; end\n"
        "  def to_i; @numerator / @denominator; end\n"
        "  def to_r; self; end\n"
        "  def frozen?; true; end\n"
        "  def to_s; \"#{@numerator}/#{@denominator}\"; end\n"
        "  def inspect; \"(#{@numerator}/#{@denominator})\"; end\n"
        "\n"
        "  def -@; Rational.new(-@numerator, @denominator); end\n"
        "  def +@; self; end\n"
        "  def abs; Rational.new(@numerator.abs, @denominator); end\n"
        "\n"
        "  def zero?; @numerator == 0; end\n"
        "  def nonzero?; @numerator == 0 ? nil : self; end\n"
        "  def positive?; @numerator > 0; end\n"
        "  def negative?; @numerator < 0; end\n"
        "  def integer?; false; end\n"
        "  def infinite?; nil; end\n"
        "  def finite?; true; end\n"
        "  def rationalize(eps = nil); self; end\n"
        "\n"
        "  def ceil(ndigits = 0)\n"
        "    if ndigits == 0\n"
        "      q, r = @numerator.divmod(@denominator)\n"
        "      r > 0 ? q + 1 : q\n"
        "    else\n"
        "      to_f.ceil(ndigits)\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def floor(ndigits = 0)\n"
        "    ndigits == 0 ? @numerator / @denominator : to_f.floor(ndigits)\n"
        "  end\n"
        "\n"
        "  def truncate(ndigits = 0)\n"
        "    if ndigits == 0\n"
        "      @numerator < 0 ? -(-@numerator / @denominator) : @numerator / @denominator\n"
        "    else\n"
        "      to_f.truncate(ndigits)\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def round(ndigits = 0)\n"
        "    if ndigits == 0\n"
        "      (@numerator * 2 + @denominator) / (@denominator * 2)\n"
        "    else\n"
        "      to_f.round(ndigits)\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def coerce(other)\n"
        "    if other.is_a?(Integer)\n"
        "      [Rational.new(other), self]\n"
        "    elsif other.is_a?(Float)\n"
        "      [other, to_f]\n"
        "    else\n"
        "      raise TypeError, \"can't coerce #{other.class} into Rational\"\n"
        "    end\n"
        "  end\n"
        "end\n"
        "\n"
        "def Rational(num, den = 1)\n"
        "  Rational.new(num, den)\n"
        "end\n"
        "\n"
        "class Complex < Numeric\n"
        "  attr_reader :real, :imaginary\n"
        "  alias imag imaginary\n"
        "\n"
        "  def initialize(real, imag = 0)\n"
        "    @real = real\n"
        "    @imaginary = imag\n"
        "  end\n"
        "\n"
        "  def +(other)\n"
        "    case other\n"
        "    when Complex\n"
        "      Complex.new(@real + other.real, @imaginary + other.imaginary)\n"
        "    when Numeric\n"
        "      Complex.new(@real + other, @imaginary)\n"
        "    else\n"
        "      raise TypeError, \"#{other.class} can't be coerced into Complex\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def -(other)\n"
        "    case other\n"
        "    when Complex\n"
        "      Complex.new(@real - other.real, @imaginary - other.imaginary)\n"
        "    when Numeric\n"
        "      Complex.new(@real - other, @imaginary)\n"
        "    else\n"
        "      raise TypeError, \"#{other.class} can't be coerced into Complex\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def *(other)\n"
        "    case other\n"
        "    when Complex\n"
        "      Complex.new(@real * other.real - @imaginary * other.imaginary,\n"
        "                  @real * other.imaginary + @imaginary * other.real)\n"
        "    when Numeric\n"
        "      Complex.new(@real * other, @imaginary * other)\n"
        "    else\n"
        "      raise TypeError, \"#{other.class} can't be coerced into Complex\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def /(other)\n"
        "    case other\n"
        "    when Complex\n"
        "      denom = (other.real * other.real + other.imaginary * other.imaginary).to_f\n"
        "      raise ZeroDivisionError, 'divided by 0' if denom == 0\n"
        "      Complex.new((@real * other.real + @imaginary * other.imaginary) / denom,\n"
        "                  (@imaginary * other.real - @real * other.imaginary) / denom)\n"
        "    when Numeric\n"
        "      raise ZeroDivisionError, 'divided by 0' if other == 0\n"
        "      Complex.new(@real.to_f / other, @imaginary.to_f / other)\n"
        "    else\n"
        "      raise TypeError, \"#{other.class} can't be coerced into Complex\"\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def ==(other)\n"
        "    case other\n"
        "    when Complex\n"
        "      @real == other.real && @imaginary == other.imaginary\n"
        "    when Numeric\n"
        "      @imaginary == 0 && @real == other\n"
        "    else\n"
        "      false\n"
        "    end\n"
        "  end\n"
        "\n"
        "  def abs; Math.sqrt(@real * @real + @imaginary * @imaginary); end\n"
        "  alias magnitude abs\n"
        "  def abs2; @real * @real + @imaginary * @imaginary; end\n"
        "  def angle; Math.atan2(@imaginary, @real); end\n"
        "  alias arg angle\n"
        "  alias phase angle\n"
        "  def conjugate; Complex.new(@real, -@imaginary); end\n"
        "  alias conj conjugate\n"
        "  def rectangular; [@real, @imaginary]; end\n"
        "  alias rect rectangular\n"
        "  def polar; [abs, angle]; end\n"
        "\n"
        "  def to_r\n"
        "    raise RangeError, \"can't convert #{inspect} into Rational\" unless @imaginary == 0\n"
        "    @real.to_r\n"
        "  end\n"
        "  def to_f\n"
        "    raise RangeError, \"can't convert #{inspect} into Float\" unless @imaginary == 0\n"
        "    @real.to_f\n"
        "  end\n"
        "  def to_i\n"
        "    raise RangeError, \"can't convert #{inspect} into Integer\" unless @imaginary == 0\n"
        "    @real.to_i\n"
        "  end\n"
        "  def to_c; self; end\n"
        "  def real?; false; end\n"
        "  def zero?; @real == 0 && @imaginary == 0; end\n"
        "  def nonzero?; zero? ? nil : self; end\n"
        "  def finite?; true; end\n"
        "  def infinite?; nil; end\n"
        "\n"
        "  def to_s\n"
        "    im = @imaginary\n"
        "    if im.is_a?(Rational)\n"
        "      im_s = im >= 0 ? \"+#{im}i\" : \"#{im}i\"\n"
        "    elsif im >= 0\n"
        "      im_s = \"+#{im}i\"\n"
        "    else\n"
        "      im_s = \"#{im}i\"\n"
        "    end\n"
        "    \"#{@real}#{im_s}\"\n"
        "  end\n"
        "  def inspect; \"(#{to_s})\"; end\n"
        "\n"
        "  def coerce(other)\n"
        "    [Complex.new(other), self]\n"
        "  end\n"
        "end\n"
        "\n"
        "def Complex(real, imag = 0)\n"
        "  Complex.new(real, imag)\n"
        "end\n"
        "\n"
        "\n";

    size_t prelude_len =
        strlen(prelude_comparable) + strlen(prelude_enumerable) + strlen(prelude_core) +
        strlen(prelude_file_constants) + strlen(prelude_rbconfig) +
        strlen(prelude_marshal) + strlen(prelude_signal) +
        strlen(prelude_process_status) + strlen(prelude_mutex_queue) +
        strlen(prelude_rational) + strlen(prelude_random) + 2;
    char *prelude = arena_alloc(arena, prelude_len);
    prelude[0] = '\0';
    strcat(prelude, prelude_comparable);
    strcat(prelude, prelude_enumerable);
    strcat(prelude, prelude_rational);
    strcat(prelude, prelude_core);
    strcat(prelude, prelude_file_constants);
    strcat(prelude, prelude_rbconfig);
    strcat(prelude, prelude_marshal);
    strcat(prelude, prelude_signal);
    strcat(prelude, prelude_process_status);
    strcat(prelude, prelude_mutex_queue);
    strcat(prelude, prelude_random);

    Parser parser;
    parser_init(&parser, prelude, strlen(prelude), arena);
    Node *tree = parse_program(&parser);
    if (!parser.error_count) {
        Sema sema;
        sema_init(&sema, arena);
        sema_run(&sema, tree);
        if (!sema.error_count)
            (void)eval_node(ev, ev->top_env, tree);
    }
}
