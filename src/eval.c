#include "eval_internal.h"

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
        if (env_get(env, "self", &self) && self.kind == VAL_OBJECT)
            val_object_set_ivar(ev->arena, self, target->sval, val);
        else
            global_set(ev->arena, &ev->globals, target->sval, val);
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
                return eval_error(ev, node, "undefined local variable '%s'", node->sval);
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
                return eval_error(ev, node, "uninitialized constant '%s'", node->sval);
            return v;
        }

        case NODE_ASSIGN: {
            Value val = eval_node(ev, env, node->assign.value);
            CHECK(val);
            assign_target(ev, env, node->assign.target, val);
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
            return eval_error(ev, node, "undefined unary operator '%s'", op);
        }

        case NODE_CALL:
            return eval_call(ev, env, node);

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
                if (result.kind == VAL_BREAK) return *result.wrapped;
                if (result.kind == VAL_RETURN) return result;
                if (result.kind == VAL_NEXT) {
                    result = val_nil();
                    continue;
                }
                if (result.kind == VAL_EXCEPTION) return result;
            }
            return result;
        }

        case NODE_BEGIN: {
            Value result = eval_node(ev, env, node->begin_stmt.body);

            if (result.kind == VAL_EXCEPTION && node->begin_stmt.rescues) {
                for (NodeList *l = node->begin_stmt.rescues; l; l = l->next) {
                    Node *rescue_clause = l->node;
                    if (!rescue_clause || rescue_clause->kind != NODE_RESCUE) continue;
                    if (rescue_clause->rescue_clause.exception_class) {
                        Value rescue_class = eval_node(ev, env, rescue_clause->rescue_clause.exception_class);
                        CHECK(rescue_class);
                    if (!exception_is_a(ev, rescue_class))
                            continue;
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
                return val_return(ev->arena, v);
            }
            return val_return(ev->arena, val_nil());

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
                return eval_error(ev, node, "super called outside of instance method");

            Value cur_class_val;
            if (!env_get(env, "__class__", &cur_class_val) || cur_class_val.kind != VAL_CLASS)
                return eval_error(ev, node, "super called outside of instance method");

            Value method_name_val;
            if (!env_get(env, "__method__", &method_name_val))
                return eval_error(ev, node, "super called outside of instance method");
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
                Env *method_env = env_new(ev->arena, method.method.closure, 1);
                env_set(ev->arena, method_env, "self", self);
                env_set(ev->arena, method_env, "__method__", val_symbol(method_name));
                Value sc_val; sc_val.kind = VAL_CLASS; sc_val.klass = owner;
                env_set(ev->arena, method_env, "__class__", sc_val);

                Value *blk = NULL;
                for (Env *sc = env; sc; sc = sc->parent) {
                    if (sc->block_arg) { blk = sc->block_arg; break; }
                    if (sc->is_def) break;
                }
                if (blk) method_env->block_arg = blk;

                bind_params(ev, method_env, method.method.def_node->def.params, super_args, super_argc);

                ev->call_depth++;
                Value result = eval_node(ev, method_env, method.method.def_node->def.body);
                ev->call_depth--;
                if (result.kind == VAL_RETURN) result = *result.wrapped;
                else if (val_is_signal(result)) return result;
                return result;
            }
            return eval_error(ev, node, "super: no superclass method '%s'", method_name);
        }

        case NODE_DEF:
            if (node->def.recv) {
                Value recv = eval_node(ev, env, node->def.recv);
                CHECK(recv);
                if (recv.kind != VAL_CLASS)
                    return eval_error(ev, node, "can only define singleton methods on a class");
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
                        return eval_error(ev, node, "superclass must be a class");
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
    ev->current_file = current_file;

    static const char *builtins[] = {
        "Object", "BasicObject", "Numeric",
        "Integer", "Float", "String", "Symbol",
        "Array", "Hash", "NilClass", "TrueClass", "FalseClass",
        "Class", "Module", "Method", "Proc",
        "Exception", "StandardError", "RuntimeError",
        "ArgumentError", "TypeError", "NoMethodError",
        "ZeroDivisionError", "LocalJumpError", "KeyError", "LoadError",
        NULL
    };
    for (int i = 0; builtins[i]; i++) {
        Value klass = val_class(arena, builtins[i], val_nil());
        klass.klass->class_env = env_new(arena, ev->top_env, 1);
        env_define(arena, ev->top_env, builtins[i], klass);
    }

    Value exception, standard_error, runtime_error, argument_error, type_error, no_method_error;
    Value zero_division_error, local_jump_error, key_error, load_error;
    if (env_get(ev->top_env, "Exception", &exception) && exception.kind == VAL_CLASS &&
        env_get(ev->top_env, "StandardError", &standard_error) && standard_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "RuntimeError", &runtime_error) && runtime_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "ArgumentError", &argument_error) && argument_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "TypeError", &type_error) && type_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "NoMethodError", &no_method_error) && no_method_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "ZeroDivisionError", &zero_division_error) && zero_division_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "LocalJumpError", &local_jump_error) && local_jump_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "KeyError", &key_error) && key_error.kind == VAL_CLASS &&
        env_get(ev->top_env, "LoadError", &load_error) && load_error.kind == VAL_CLASS) {
        standard_error.klass->superclass = exception;
        runtime_error.klass->superclass = standard_error;
        argument_error.klass->superclass = standard_error;
        type_error.klass->superclass = standard_error;
        no_method_error.klass->superclass = standard_error;
        zero_division_error.klass->superclass = standard_error;
        local_jump_error.klass->superclass = standard_error;
        key_error.klass->superclass = standard_error;
        load_error.klass->superclass = standard_error;
    }
}
