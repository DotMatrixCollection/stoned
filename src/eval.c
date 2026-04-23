#include "eval_internal.h"
#include "parser.h"
#include "sema.h"

#include <stdio.h>
#include <string.h>

#define CHECK(v) do { if (ev->errored || val_is_signal(v)) return (v); } while(0)

static void assign_lvar(Eval *ev, Env *env, const char *name, Value val) {
    if (!env_update(env, name, val))
        env_set(ev->arena, env, name, val);
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
        if (env_get(env, "self", &self) && self.kind == VAL_OBJECT) {
            if (self.obj->frozen) {
                const char *kname = self.obj->klass.kind == VAL_CLASS ? self.obj->klass.klass->name : "Object";
                eval_raise_class(ev, target, "FrozenError", "can't modify frozen %s", kname);
                return;
            }
            val_object_set_ivar(ev->arena, self, target->sval, val);
        } else {
            global_set(ev->arena, &ev->globals, target->sval, val);
        }
    } else if (target->kind == NODE_GVAR) {
        global_set(ev->arena, &ev->globals, target->sval, val);
    } else if (target->kind == NODE_CONST) {
        env_set(ev->arena, ev->top_env, target->sval, val);
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
            return env_get(ev->top_env, node->sval, out);
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
            Value v;
            return env_get(ev->top_env, node->sval, &v) ? "constant" : NULL;
        }
        case NODE_SELF:
            return "self";
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

        case NODE_ROPE: {
            const char *s = eval_rope(ev, env, node->interp.rope);
            if (ev->errored) return val_nil();
            return val_string(ev->arena, s);
        }

        case NODE_LVAR: {
            Value v;
            if (!env_get(env, node->sval, &v))
                return eval_raise_class(ev, node, "NameError", "undefined local variable '%s'", node->sval);
            return v;
        }
        case NODE_IVAR: {
            Value self;
            if (env_get(env, "self", &self) && self.kind == VAL_OBJECT) {
                Value v;
                if (!val_object_get_ivar(self, node->sval, &v)) return val_nil();
                return v;
            }
            Value v;
            if (!global_get(&ev->globals, node->sval, &v)) return val_nil();
            return v;
        }
        case NODE_GVAR: {
            Value v;
            if (!global_get(&ev->globals, node->sval, &v)) return val_nil();
            return v;
        }
        case NODE_CONST: {
            Value v;
            if (!env_get(ev->top_env, node->sval, &v))
                return eval_raise_class(ev, node, "NameError", "uninitialized constant '%s'", node->sval);
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
            else if (target->kind == NODE_IVAR) {
                Value self;
                if (env_get(env, "self", &self) && self.kind == VAL_OBJECT)
                    val_object_set_ivar(ev->arena, self, target->sval, val);
                else
                    global_set(ev->arena, &ev->globals, target->sval, val);
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
            }
            if (strcmp(op, "+") == 0) return operand;
            if (strcmp(op, "~") == 0 && operand.kind == VAL_INT)
                return val_int(~operand.ival);
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
                Value cond = eval_node(ev, env, node->loop.cond);
                CHECK(cond);
                int cont = (node->kind == NODE_WHILE) ? val_truthy(cond) : !val_truthy(cond);
                if (!cont) break;
                result = eval_node(ev, env, node->loop.body);
                if (result.kind == VAL_BREAK) return *result.jump.wrapped;
                if (result.kind == VAL_RETURN) return result;
                if (result.kind == VAL_NEXT) {
                    result = val_nil();
                    continue;
                }
                if (result.kind == VAL_EXCEPTION) return result;
            }
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
            retry_begin:
            result = eval_node(ev, env, node->begin_stmt.body);

            if (result.kind == VAL_EXCEPTION && node->begin_stmt.rescues) {
                for (NodeList *l = node->begin_stmt.rescues; l; l = l->next) {
                    Node *rescue_clause = l->node;
                    if (!rescue_clause || rescue_clause->kind != NODE_RESCUE) continue;
                    if (rescue_clause->rescue_clause.exception_classes) {
                        int matched = 0;
                        for (NodeList *cl = rescue_clause->rescue_clause.exception_classes; cl; cl = cl->next) {
                            Value rescue_class = eval_node(ev, env, cl->node);
                            CHECK(rescue_class);
                            if (exception_is_a(ev, rescue_class)) { matched = 1; break; }
                        }
                        if (!matched) continue;
                    }
                    Value rescued_exc = ev->current_exception;
                    Value previous_rescue = ev->rescue_context;
                    eval_clear_exception(ev);
                    ev->rescue_context = rescued_exc;
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
                        if (p->kind != NODE_PARAM || !p->param.name || p->param.block_param) continue;
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
                if (recv.kind != VAL_CLASS)
                    return eval_raise_class(ev, node, "TypeError", "can only define singleton methods on a class");
                size_t nlen = strlen(node->def.name);
                char *key = arena_alloc(ev->arena, nlen + 6);
                memcpy(key, "self.", 5);
                memcpy(key + 5, node->def.name, nlen + 1);
                env_define(ev->arena, recv.klass->class_env, key, val_method(node, ev->top_env, METHOD_PUBLIC));
            } else {
                env_define(ev->arena, env, node->def.name,
                           val_method(node, ev->top_env, current_method_visibility(env)));
            }
            return val_nil();

        case NODE_ALIAS: {
            Value method = val_nil();
            int found = 0;

            Value self = val_nil();
            if (env_get(env, "self", &self) && self.kind == VAL_CLASS && self.klass->class_env == env) {
                found = ruby_class_find_instance_method(self.klass, node->alias_stmt.old_name, &method, NULL);
                if (found) env_define(ev->arena, self.klass->class_env, node->alias_stmt.new_name, method);
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
            Value existing;
            int reopen = env_get(ev->top_env, node->klass.name, &existing) &&
                         existing.kind == VAL_CLASS;

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
                    if (strcmp(node->klass.name, "Object") != 0 &&
                        env_get(ev->top_env, "Object", &object_val) &&
                        object_val.kind == VAL_CLASS)
                        superclass = object_val;
                }
                klass = val_class(ev->arena, node->klass.name, superclass);
                klass.klass->class_env = env_new(ev->arena, ev->top_env, 1);
            }

            env_set(ev->arena, klass.klass->class_env, "self", klass);
            set_current_method_visibility(ev->arena, klass.klass->class_env, METHOD_PUBLIC);
            if (node->klass.body) {
                Value body_result = eval_node(ev, klass.klass->class_env, node->klass.body);
                if (val_is_signal(body_result)) return body_result;
            }

            if (!reopen)
                env_define(ev->arena, ev->top_env, node->klass.name, klass);
            return klass;
        }

        case NODE_MODULE: {
            Value existing;
            int reopen = env_get(ev->top_env, node->klass.name, &existing) &&
                         existing.kind == VAL_CLASS;

            Value mod;
            if (reopen) {
                mod = existing;
            } else {
                mod = val_class(ev->arena, node->klass.name, val_nil());
                mod.klass->class_env = env_new(ev->arena, ev->top_env, 1);
                mod.klass->is_module = 1;
            }

            env_set(ev->arena, mod.klass->class_env, "self", mod);
            set_current_method_visibility(ev->arena, mod.klass->class_env, METHOD_PUBLIC);
            if (node->klass.body) {
                Value body_result = eval_node(ev, mod.klass->class_env, node->klass.body);
                if (val_is_signal(body_result)) return body_result;
            }

            if (!reopen)
                env_define(ev->arena, ev->top_env, node->klass.name, mod);
            return mod;
        }

        case NODE_RANGE: {
            Value begin = eval_node(ev, env, node->range.begin);
            CHECK(begin);
            Value end = eval_node(ev, env, node->range.end);
            CHECK(end);
            return val_range(ev->arena, begin, end, node->range.exclusive);
        }

        case NODE_ARRAY: {
            Value arr = val_array_new();
            for (NodeList *l = node->array.elements; l; l = l->next) {
                Value elem = eval_node(ev, env, l->node);
                CHECK(elem);
                val_array_push(&arr, elem);
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

void eval_init(Eval *ev, Arena *arena, FILE *out, const char *current_file) {
    memset(ev, 0, sizeof(*ev));
    ev->arena   = arena;
    ev->out     = out;
    ev->top_env = env_new(arena, NULL, 1);
    ev->active_defs[0] = ev->top_env;
    ev->active_def_count = 1;
    ev->current_file = current_file;

    Value load_path = val_array_new();
    val_array_push(&load_path, val_string(arena, "."));
    if (current_file) {
        const char *slash = strrchr(current_file, '/');
        if (slash) {
            size_t len = (size_t)(slash - current_file);
            val_array_push(&load_path, val_string_n(arena, current_file, len));
        }
    }
    global_set(arena, &ev->globals, "LOAD_PATH", load_path);

    static const char *builtins[] = {
        "Object", "BasicObject", "Numeric",
        "Integer", "Float", "String", "Symbol",
        "Array", "Hash", "Range", "NilClass", "TrueClass", "FalseClass", "IO", "File",
        "Class", "Module", "Method", "UnboundMethod", "Proc", "Comparable", "Enumerable",
        "Exception", "StandardError", "RuntimeError",
        "ArgumentError", "TypeError", "NameError", "NoMethodError",
        "ZeroDivisionError", "LocalJumpError", "KeyError", "LoadError", "StopIteration",
        "SystemStackError", "IOError", "EncodingError", "FrozenError",
        NULL
    };
    for (int i = 0; builtins[i]; i++) {
        Value klass = val_class(arena, builtins[i], val_nil());
        klass.klass->class_env = env_new(arena, ev->top_env, 1);
        if (strcmp(builtins[i], "Comparable") == 0 || strcmp(builtins[i], "Enumerable") == 0)
            klass.klass->is_module = 1;
        env_define(arena, ev->top_env, builtins[i], klass);
    }

    Value exception, standard_error, runtime_error, argument_error, type_error, name_error, no_method_error;
    Value zero_division_error, local_jump_error, key_error, load_error, stop_iteration, system_stack_error, io_error, encoding_error, frozen_error;
    if (env_get(ev->top_env, "Exception", &exception) && exception.kind == VAL_CLASS &&
        env_get(ev->top_env, "StandardError", &standard_error) && standard_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "RuntimeError", &runtime_error) && runtime_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "ArgumentError", &argument_error) && argument_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "TypeError", &type_error) && type_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "NameError", &name_error) && name_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "NoMethodError", &no_method_error) && no_method_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "ZeroDivisionError", &zero_division_error) && zero_division_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "LocalJumpError", &local_jump_error) && local_jump_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "KeyError", &key_error) && key_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "LoadError", &load_error) && load_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "StopIteration", &stop_iteration) && stop_iteration.kind == VAL_CLASS &&
        env_get(ev->top_env, "SystemStackError", &system_stack_error) && system_stack_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "IOError", &io_error) && io_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "EncodingError", &encoding_error) && encoding_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "FrozenError", &frozen_error) && frozen_error.kind == VAL_CLASS) {
        standard_error.klass->superclass = exception;
        runtime_error.klass->superclass = standard_error;
        argument_error.klass->superclass = standard_error;
        type_error.klass->superclass = standard_error;
        name_error.klass->superclass = standard_error;
        no_method_error.klass->superclass = standard_error;
        zero_division_error.klass->superclass = standard_error;
        local_jump_error.klass->superclass = standard_error;
        key_error.klass->superclass = standard_error;
        load_error.klass->superclass = standard_error;
        stop_iteration.klass->superclass = standard_error;
        system_stack_error.klass->superclass = standard_error;
        io_error.klass->superclass = standard_error;
        encoding_error.klass->superclass = standard_error;
        frozen_error.klass->superclass = runtime_error;
        no_method_error.klass->superclass = name_error;
    }

    Value io_class;
    if (env_get(ev->top_env, "IO", &io_class) && io_class.kind == VAL_CLASS) {
        static const char *fds[] = { "stdout", "stderr", "stdin", NULL };
        static const char *consts[] = { "STDOUT", "STDERR", "STDIN", NULL };
        for (int i = 0; fds[i]; i++) {
            Value obj = val_object(arena, io_class);
            val_object_set_ivar(arena, obj, "__fd__", val_string(arena, fds[i]));
            global_set(arena, &ev->globals, fds[i], obj);
            env_define(arena, ev->top_env, consts[i], obj);
        }
    }

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
        "  def first\n"
        "    each do |x|\n"
        "      return x\n"
        "    end\n"
        "    nil\n"
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
        "    started = args.length > 0\n"
        "    acc = args[0]\n"
        "    each do |x|\n"
        "      if started\n"
        "        acc = yield(acc, x)\n"
        "      else\n"
        "        acc = x\n"
        "        started = true\n"
        "      end\n"
        "    end\n"
        "    acc\n"
        "  end\n"
        "\n"
        "  def inject(*args)\n"
        "    reduce(*args) { |acc, x| yield acc, x }\n"
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
        "  def tally\n"
        "    result = {}\n"
        "    each do |x|\n"
        "      n = result[x]\n"
        "      if n == nil\n"
        "        result[x] = 1\n"
        "      else\n"
        "        result[x] = n + 1\n"
        "      end\n"
        "    end\n"
        "    result\n"
        "  end\n"
        "end\n"
        "\n";

    static const char *prelude_core =
        "class Integer\n"
        "  include Comparable\n"
        "end\n"
        "class Float\n"
        "  include Comparable\n"
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
        "end\n";

    size_t prelude_len =
        strlen(prelude_comparable) + strlen(prelude_enumerable) + strlen(prelude_core) + 1;
    char *prelude = arena_alloc(arena, prelude_len);
    prelude[0] = '\0';
    strcat(prelude, prelude_comparable);
    strcat(prelude, prelude_enumerable);
    strcat(prelude, prelude_core);

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
