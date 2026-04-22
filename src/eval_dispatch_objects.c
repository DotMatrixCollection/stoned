#include "eval_internal.h"

#include <string.h>

int dispatch_class(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out) {
    (void)env; (void)site;
    if (recv.kind != VAL_CLASS) return 0;
    if (strcmp(name, "new") == 0) {
        Value obj = val_object(ev->arena, recv);
        RubyClass *klass = recv.klass;
        while (klass) {
            Value init_method;
            if (env_get(klass->class_env, "initialize", &init_method) && init_method.kind == VAL_METHOD) {
                Env *method_env = env_new(ev->arena, init_method.method.closure, 1);
                env_set(ev->arena, method_env, "self", obj);
                env_set(ev->arena, method_env, "__method__", val_symbol("initialize"));
                Value klass_val; klass_val.kind = VAL_CLASS; klass_val.klass = klass;
                env_set(ev->arena, method_env, "__class__", klass_val);
                if (blk) method_env->block_arg = blk;
                NodeList *params = init_method.method.def_node->def.params;
                for (int i = 0; i < argc && params; i++, params = params->next)
                    env_set(ev->arena, method_env, params->node->param.name, args[i]);
                ev->call_depth++;
                Value result = eval_node(ev, method_env, init_method.method.def_node->def.body);
                ev->call_depth--;
                if (ev->errored) { *out = val_nil(); return 1; }
                (void)result;
                break;
            }
            klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
        }
        *out = obj;
        return 1;
    }
    size_t nlen = strlen(name);
    char *key = arena_alloc(ev->arena, nlen + 6);
    memcpy(key, "self.", 5);
    memcpy(key + 5, name, nlen + 1);
    RubyClass *cklass = recv.klass;
    while (cklass) {
        Value cm;
        if (env_get(cklass->class_env, key, &cm) && cm.kind == VAL_METHOD) {
            Env *method_env = env_new(ev->arena, cm.method.closure, 1);
            env_set(ev->arena, method_env, "self", recv);
            env_set(ev->arena, method_env, "__method__", val_symbol(name));
            Value kv; kv.kind = VAL_CLASS; kv.klass = cklass;
            env_set(ev->arena, method_env, "__class__", kv);
            if (blk) method_env->block_arg = blk;
            NodeList *params = cm.method.def_node->def.params;
            for (int i = 0; i < argc && params; i++, params = params->next)
                env_set(ev->arena, method_env, params->node->param.name, args[i]);
            ev->call_depth++;
            Value result = eval_node(ev, method_env, cm.method.def_node->def.body);
            ev->call_depth--;
            if (ev->errored) { *out = val_nil(); return 1; }
            if (result.kind == VAL_RETURN) result = *result.wrapped;
            *out = result;
            return 1;
        }
        cklass = cklass->superclass.kind == VAL_CLASS ? cklass->superclass.klass : NULL;
    }
    return 0;
}

int dispatch_object(Eval *ev, Value recv, const char *name, Value *args, int argc,
                    Value *blk, Value *out) {
    if (recv.kind != VAL_OBJECT) return 0;
    RubyClass *klass = recv.obj->klass.klass;
    while (klass) {
        Value method;
        if (env_get(klass->class_env, name, &method) && method.kind == VAL_METHOD) {
            Env *method_env = env_new(ev->arena, method.method.closure, 1);
            env_set(ev->arena, method_env, "self", recv);
            env_set(ev->arena, method_env, "__method__", val_symbol(name));
            Value klass_val; klass_val.kind = VAL_CLASS; klass_val.klass = klass;
            env_set(ev->arena, method_env, "__class__", klass_val);
            if (blk) method_env->block_arg = blk;
            NodeList *params = method.method.def_node->def.params;
            for (int i = 0; i < argc && params; i++, params = params->next)
                env_set(ev->arena, method_env, params->node->param.name, args[i]);
            ev->call_depth++;
            Value result = eval_node(ev, method_env, method.method.def_node->def.body);
            ev->call_depth--;
            if (ev->errored) { *out = val_nil(); return 1; }
            if (result.kind == VAL_RETURN) result = *result.wrapped;
            *out = result;
            return 1;
        }
        klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
    }
    return 0;
}
