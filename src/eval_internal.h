#ifndef STONED_EVAL_INTERNAL_H
#define STONED_EVAL_INTERNAL_H

#include "eval.h"
#include "rope.h"

Value eval_error(Eval *ev, Node *n, const char *fmt, ...);
const char *eval_rope(Eval *ev, Env *env, RopeNode *r);
Value call_block(Eval *ev, Value blk, Value *args, int argc, Node *call_site);
Value dispatch_method(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                      Value *blk, Node *site);
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
                   Value *blk, Node *site, Value *out);
int dispatch_object(Eval *ev, Value recv, const char *name, Value *args, int argc,
                    Value *blk, Value *out);

#endif
