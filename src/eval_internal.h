#ifndef STONED_EVAL_INTERNAL_H
#define STONED_EVAL_INTERNAL_H

#include "eval.h"
#include "regex.h"
#include "rope.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static inline const char *errno_class_name(int err) {
    switch (err) {
        case ENOENT: return "Errno::ENOENT";
        case EACCES: return "Errno::EACCES";
        case EPERM:  return "Errno::EPERM";
        case EEXIST: return "Errno::EEXIST";
        case EBADF:  return "Errno::EBADF";
        case EINVAL: return "Errno::EINVAL";
        case ESPIPE: return "Errno::ESPIPE";
        default:     return "IOError";
    }
}

typedef struct {
    FILE *fp;
    int   owns_fp;
    int   is_pipe;   /* 1 if opened via popen — use pclose to close */
} NativeFile;

typedef struct {
    int64_t sec;
    long    nsec;
} NativeTime;

typedef struct {
    struct Env *env;
    const char *file;
    int64_t     line;
} NativeBinding;

static inline NativeFile *alloc_native_file(Arena *a, FILE *fp, int owns_fp) {
    NativeFile *nf = arena_alloc(a, sizeof(NativeFile));
    nf->fp = fp;
    nf->owns_fp = owns_fp;
    return nf;
}

static inline NativeTime *alloc_native_time(Arena *a, int64_t sec, long nsec) {
    NativeTime *nt = arena_alloc(a, sizeof(NativeTime));
    nt->sec = sec;
    nt->nsec = nsec;
    return nt;
}

static inline NativeBinding *alloc_native_binding(Arena *a, struct Env *env,
                                                  const char *file, int64_t line) {
    NativeBinding *nb = arena_alloc(a, sizeof(NativeBinding));
    nb->env = env;
    nb->file = file;
    nb->line = line;
    return nb;
}

Value eval_error(Eval *ev, Node *n, const char *fmt, ...);
Value eval_raise(Eval *ev, Node *n, const char *fmt, ...);
Value eval_raise_class(Eval *ev, Node *n, const char *class_name, const char *fmt, ...);
Value eval_raise_value(Eval *ev, Node *n, Value exc);
void eval_clear_exception(Eval *ev);
Value eval_raise_encoding_error(Eval *ev, Node *n, const char *context);
int value_is_a_named_class(Eval *ev, Value v, const char *class_name);
int class_is_a_named_class(Eval *ev, RubyClass *klass, const char *class_name);
const char *exception_value_class_name(Value exc);
const char *exception_value_message(Eval *ev, Value exc);
uint32_t exception_value_line(Value exc);
uint32_t exception_value_col(Value exc);
Value exception_value_backtrace(Value exc);
Value build_exception_object(Eval *ev, Value klass, const char *msg);
void exception_set_message(Eval *ev, Value exc, Value msg);
void exception_set_backtrace(Eval *ev, Value exc, Value backtrace);
void eval_push_frame(Eval *ev, uint32_t line, uint32_t col, const char *label);
void eval_pop_frame(Eval *ev);
void bind_params(Eval *ev, Env *env, NodeList *params, Value *args, int argc);
int count_required_params(NodeList *params);
int count_total_params(NodeList *params);
int proc_arity(NodeList *params, int is_lambda);
int has_splat_param(NodeList *params);
int has_kwarg_params(NodeList *params);
int has_kwrest_param(NodeList *params);
Value extract_kwargs(Eval *ev, NodeList *params, Value *args, int *argc);
MethodVisibility current_method_visibility(Env *env);
int is_module_function_mode(Env *env);
Value eval_ruby_string(Eval *ev, const char *src, const char *display_name, Node *site);
void set_current_method_visibility(Arena *a, Env *env, MethodVisibility visibility);
void update_method_visibility(Env *env, const char *name, MethodVisibility visibility, int singleton_only);
int method_visibility_allows_call(Eval *ev, Env *env, Value recv, RubyClass *owner,
                                  MethodVisibility visibility, int public_only, int explicit_receiver);
const char *eval_rope(Eval *ev, Env *env, RopeNode *r);
Value call_block(Eval *ev, Env *caller_env, Value blk, Value *args, int argc, Node *call_site);
Value eval_require(Eval *ev, Env *env, const char *path, Node *site);
Value eval_require_relative(Eval *ev, Env *env, const char *path, Node *site);
Value eval_load(Eval *ev, const char *path, Node *site);
Value eval_file_read(Eval *ev, const char *path, Node *site);
Value eval_file_read_slice(Eval *ev, const char *path, int has_length, int64_t length,
                           int has_offset, int64_t offset, Node *site);
Value eval_file_write(Eval *ev, const char *path, const char *content, Node *site);
Value eval_file_write_at(Eval *ev, const char *path, const char *content,
                         int has_offset, int64_t offset, Node *site);
Value eval_file_append(Eval *ev, const char *path, const char *content, Node *site);
Value eval_file_exist(Eval *ev, const char *path);
Value eval_file_delete(Eval *ev, const char *path, Node *site);
Value eval_file_touch_mode(Eval *ev, const char *path, const char *mode, Node *site);
Value eval_format_string(Eval *ev, Env *env, const char *fmt, Value *args, int argc, Node *site);
const char *value_class_name(Eval *ev, Value v);
int builtin_method_arity(const char *mname);
int value_has_module(Eval *ev, Value recv, const char *module_name);
int val_is_a(Value v, Value klass_arg);
int val_responds_to(Eval *ev, Value recv, const char *name, int include_private);
int ruby_class_find_instance_method(RubyClass *klass, const char *name, Value *out, RubyClass **owner);
int ruby_class_find_super_method(RubyClass *start, RubyClass *after, const char *name, Value *out, RubyClass **owner);
int ruby_class_find_class_method(RubyClass *klass, const char *name, Value *out, RubyClass **owner);
int ruby_class_find_class_super_method(RubyClass *start, RubyClass *after, const char *name, Value *out, RubyClass **owner);
Value call_method_value(Eval *ev, Env *env, Value recv, Value method, RubyClass *owner,
                        const char *name, Value *args, int argc, Value *blk, Node *site);
int method_object_arity(Value method_obj);
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
int dispatch_range(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out);
int dispatch_nil(Eval *ev, Env *env, Value recv, const char *name, Node *site, Value *out);
int dispatch_bool(Eval *ev, Value recv, const char *name, Node *site, Value *out);
int dispatch_class(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out, int public_only, int explicit_receiver);
int dispatch_object(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                    Value *blk, Node *site, Value *out, int public_only, int explicit_receiver);
int sym_in_array(Value *arr, const char *sym_name);
void collect_own_instance_methods(Env *class_env, Value *arr, int vis_mask);
void collect_all_instance_methods(RubyClass *klass, Value *arr, int vis_mask,
                                  RubyClass **visited, int *nv);
int primitive_class_responds_to_name(const char *klass_name, const char *name);
const char *primitive_class_methods_for_class(const char *klass_name);
const char *primitive_instance_methods_for_class(const char *klass_name);
int primitive_class_method_responds_to_name(const char *klass_name, const char *name);
Value make_bound_method_proc(Eval *ev, Value receiver, const char *method_name, int forced_arity);
int value_is_regexp(Value v);
Value regexp_search_value(Eval *ev, Value regexp, Value string, int return_index, Node *site);
Value extract_named_groups(Arena *a, const char *pattern, size_t ncaps);
Value build_match_data(Eval *ev, Value regexp, Value string, RegexMatch match);

static inline int flow_signal_out(Value v, Value *out) {
    if (v.kind == VAL_BREAK) {
        *out = *v.jump.wrapped;
        return 1;
    }
    if (v.kind == VAL_RETURN || v.kind == VAL_EXCEPTION || v.kind == VAL_THROW) {
        *out = v;
        return 1;
    }
    return 0;
}

#endif
