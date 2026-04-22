#ifndef STONED_EVAL_INTERNAL_H
#define STONED_EVAL_INTERNAL_H

#include "eval.h"
#include "rope.h"

Value eval_error(Eval *ev, Node *n, const char *fmt, ...);
Value eval_raise(Eval *ev, Node *n, const char *fmt, ...);
Value eval_raise_class(Eval *ev, Node *n, const char *class_name, const char *fmt, ...);
Value eval_raise_value(Eval *ev, Node *n, Value exc);
void eval_clear_exception(Eval *ev);
int value_is_a_named_class(Eval *ev, Value v, const char *class_name);
const char *exception_value_class_name(Value exc);
const char *exception_value_message(Eval *ev, Value exc);
uint32_t exception_value_line(Value exc);
uint32_t exception_value_col(Value exc);
Value exception_value_backtrace(Value exc);
void eval_push_frame(Eval *ev, uint32_t line, uint32_t col, const char *label);
void eval_pop_frame(Eval *ev);
void bind_params(Eval *ev, Env *env, NodeList *params, Value *args, int argc);
int count_required_params(NodeList *params);
int has_splat_param(NodeList *params);
MethodVisibility current_method_visibility(Env *env);
void set_current_method_visibility(Arena *a, Env *env, MethodVisibility visibility);
void update_method_visibility(Env *env, const char *name, MethodVisibility visibility, int singleton_only);
int method_visibility_allows_call(Eval *ev, Env *env, Value recv, RubyClass *owner,
                                  MethodVisibility visibility, int public_only, int explicit_receiver);
const char *eval_rope(Eval *ev, Env *env, RopeNode *r);
Value call_block(Eval *ev, Value blk, Value *args, int argc, Node *call_site);
Value eval_require(Eval *ev, Env *env, const char *path, Node *site);
Value eval_require_relative(Eval *ev, Env *env, const char *path, Node *site);
int ruby_class_find_instance_method(RubyClass *klass, const char *name, Value *out, RubyClass **owner);
int ruby_class_find_super_method(RubyClass *start, RubyClass *after, const char *name, Value *out, RubyClass **owner);
Value dispatch_method(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                      Value *blk, Node *site, int public_only, int explicit_receiver);
Value eval_binop(Eval *ev, Env *env, Node *node);
Value eval_call(Eval *ev, Env *env, Node *node);
int dispatch_integer(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                     Value *blk, Node *site, Value *out);
int dispatch_float(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out);
int dispatch_string(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                    Value *blk, Node *site, Value *out);
int dispatch_array(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out);
int dispatch_hash(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                  Value *blk, Node *site, Value *out);
int dispatch_nil(Eval *ev, Value recv, const char *name, Node *site, Value *out);
int dispatch_bool(Eval *ev, Value recv, const char *name, Node *site, Value *out);
int dispatch_class(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out, int public_only, int explicit_receiver);
int dispatch_object(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                    Value *blk, Node *site, Value *out, int public_only, int explicit_receiver);

static inline int flow_signal_out(Value v, Value *out) {
    if (v.kind == VAL_BREAK) {
        *out = *v.wrapped;
        return 1;
    }
    if (v.kind == VAL_RETURN || v.kind == VAL_EXCEPTION) {
        *out = v;
        return 1;
    }
    return 0;
}

#endif
