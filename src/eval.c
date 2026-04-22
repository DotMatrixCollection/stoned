#include "eval_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(v) do { if (ev->errored || val_is_signal(v)) return (v); } while(0)

static void assign_lvar(Eval *ev, Env *env, const char *name, Value val) {
    if (!env_update(env, name, val))
        env_set(ev->arena, env, name, val);
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
            Node *target = node->assign.target;
            if (target->kind == NODE_LVAR)
                assign_lvar(ev, env, target->sval, val);
            else if (target->kind == NODE_IVAR) {
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
                if (ev->errored) return val_nil();
                if (result.kind == VAL_BREAK) return *result.wrapped;
                if (result.kind == VAL_RETURN) return result;
                if (result.kind == VAL_NEXT) {
                    result = val_nil();
                    continue;
                }
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

            RubyClass *search = cur_class_val.klass->superclass.kind == VAL_CLASS
                                ? cur_class_val.klass->superclass.klass : NULL;

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

            while (search) {
                Value method;
                if (env_get(search->class_env, method_name, &method) && method.kind == VAL_METHOD) {
                    Env *method_env = env_new(ev->arena, method.method.closure, 1);
                    env_set(ev->arena, method_env, "self", self);
                    Value sc_val; sc_val.kind = VAL_CLASS; sc_val.klass = search;
                    env_set(ev->arena, method_env, "__method__", val_symbol(method_name));
                    env_set(ev->arena, method_env, "__class__", sc_val);

                    Value *blk = NULL;
                    for (Env *sc = env; sc; sc = sc->parent) {
                        if (sc->block_arg) { blk = sc->block_arg; break; }
                        if (sc->is_def) break;
                    }
                    if (blk) method_env->block_arg = blk;

                    NodeList *params = method.method.def_node->def.params;
                    for (int i = 0; i < super_argc && params; i++, params = params->next)
                        env_set(ev->arena, method_env, params->node->param.name, super_args[i]);

                    ev->call_depth++;
                    Value result = eval_node(ev, method_env, method.method.def_node->def.body);
                    ev->call_depth--;
                    if (ev->errored) return val_nil();
                    if (result.kind == VAL_RETURN) result = *result.wrapped;
                    return result;
                }
                search = search->superclass.kind == VAL_CLASS ? search->superclass.klass : NULL;
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
                env_define(ev->arena, recv.klass->class_env, key, val_method(node, ev->top_env));
            } else {
                env_define(ev->arena, env, node->def.name, val_method(node, ev->top_env));
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
            if (node->klass.body)
                eval_node(ev, klass.klass->class_env, node->klass.body);
            if (ev->errored) return val_nil();

            if (!reopen)
                env_define(ev->arena, ev->top_env, node->klass.name, klass);
            return klass;
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

void eval_init(Eval *ev, Arena *arena, FILE *out) {
    memset(ev, 0, sizeof(*ev));
    ev->arena   = arena;
    ev->out     = out;
    ev->top_env = env_new(arena, NULL, 1);

    static const char *builtins[] = {
        "Object", "BasicObject", "Numeric",
        "Integer", "Float", "String", "Symbol",
        "Array", "Hash", "NilClass", "TrueClass", "FalseClass",
        "Class", "Module", "Method", "Proc",
        NULL
    };
    for (int i = 0; builtins[i]; i++) {
        Value klass = val_class(arena, builtins[i], val_nil());
        klass.klass->class_env = env_new(arena, ev->top_env, 1);
        env_define(arena, ev->top_env, builtins[i], klass);
    }
}
