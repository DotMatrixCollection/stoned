#include "eval_internal.h"
#include "utf8.h"

#include <stdio.h>
#include <string.h>

static Value exception_arg_message(Eval *ev, Value recv, Value *args, int argc, int *ok, Node *site) {
    if (argc > 1) {
        *ok = 0;
        return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
    }
    *ok = 1;
    if (argc == 0 || args[0].kind == VAL_NIL)
        return val_string(ev->arena, recv.kind == VAL_CLASS ? recv.klass->name : exception_value_class_name(recv));
    return val_string(ev->arena, val_to_s(ev->arena, args[0]));
}

int sym_in_array(Value *arr, const char *sym_name) {
    for (size_t i = 0; i < arr->array->len; i++) {
        if (arr->array->elems[i].kind == VAL_SYMBOL &&
            strcmp(arr->array->elems[i].sval, sym_name) == 0) return 1;
    }
    return 0;
}

void collect_own_instance_methods(Env *class_env, Value *arr, int vis_mask) {
    for (EnvEntry *entry = class_env ? class_env->vars : NULL; entry; entry = entry->next) {
        if (entry->val.kind != VAL_METHOD) continue;
        if (strncmp(entry->name, "self.", 5) == 0) continue;
        MethodVisibility vis = entry->val.method.visibility;
        int match = ((vis_mask & 1) && vis == METHOD_PUBLIC) ||
                    ((vis_mask & 2) && vis == METHOD_PROTECTED) ||
                    ((vis_mask & 4) && vis == METHOD_PRIVATE);
        if (match && !sym_in_array(arr, entry->name))
            val_array_push(arr, val_symbol(entry->name));
    }
}

void collect_all_instance_methods(RubyClass *klass, Value *arr, int vis_mask,
                                         RubyClass **visited, int *nv) {
    if (!klass) return;
    for (int i = 0; i < *nv; i++) if (visited[i] == klass) return;
    visited[(*nv)++] = klass;
    for (RubyModuleInclusion *inc = klass->prepended_modules; inc; inc = inc->next)
        collect_all_instance_methods(inc->mod, arr, vis_mask, visited, nv);
    collect_own_instance_methods(klass->class_env, arr, vis_mask);
    for (RubyModuleInclusion *inc = klass->included_modules; inc; inc = inc->next)
        collect_all_instance_methods(inc->mod, arr, vis_mask, visited, nv);
    if (!klass->is_module && klass->superclass.kind == VAL_CLASS)
        collect_all_instance_methods(klass->superclass.klass, arr, vis_mask, visited, nv);
}

static void collect_class_ancestors(RubyClass *klass, Value *arr, RubyClass **visited, int *nv) {
    if (!klass) return;
    for (int i = 0; i < *nv; i++) if (visited[i] == klass) return;
    visited[(*nv)++] = klass;
    for (RubyModuleInclusion *inc = klass->prepended_modules; inc; inc = inc->next)
        collect_class_ancestors(inc->mod, arr, visited, nv);
    Value kv; kv.kind = VAL_CLASS; kv.klass = klass;
    val_array_push(arr, kv);
    for (RubyModuleInclusion *inc = klass->included_modules; inc; inc = inc->next)
        collect_class_ancestors(inc->mod, arr, visited, nv);
    if (!klass->is_module && klass->superclass.kind == VAL_CLASS)
        collect_class_ancestors(klass->superclass.klass, arr, visited, nv);
}

int dispatch_class(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out, int public_only, int explicit_receiver) {
    if (recv.kind != VAL_CLASS) return 0;
    if (strcmp(name, "===") == 0) {
        if (argc < 1) { *out = val_false(); return 1; }
        *out = val_bool(val_is_a(args[0], recv));
        return 1;
    }
    if (strcmp(recv.klass->name, "File") == 0) {
        if (strcmp(name, "read") == 0) {
            if (argc < 1) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File.read requires a path");
            } else if (args[0].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "File.read path must be a String");
            } else {
                *out = eval_file_read(ev, args[0].sval, site);
            }
            return 1;
        }
        if (strcmp(name, "write") == 0) {
            if (argc < 2) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File.write requires a path and content");
            } else if (args[0].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "File.write path must be a String");
            } else if (args[1].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "File.write content must be a String");
            } else {
                *out = eval_file_write(ev, args[0].sval, args[1].sval, site);
            }
            return 1;
        }
        if (strcmp(name, "delete") == 0) {
            if (argc < 1) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File.delete requires a path");
            } else if (args[0].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "File.delete path must be a String");
            } else {
                *out = eval_file_delete(ev, args[0].sval, site);
            }
            return 1;
        }
        if (strcmp(name, "exist?") == 0) {
            if (argc < 1) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File.exist? requires a path");
            } else if (args[0].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "File.exist? path must be a String");
            } else {
                *out = eval_file_exist(ev, args[0].sval);
            }
            return 1;
        }
        if (strcmp(name, "open") == 0) {
            if (argc < 1) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File.open requires a path");
            } else if (args[0].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "File.open path must be a String");
            } else {
                Value mode = argc >= 2 ? args[1] : val_string(ev->arena, "r");
                if (mode.kind != VAL_STRING) {
                    *out = eval_raise_class(ev, site, "TypeError", "File.open mode must be a String");
                    return 1;
                }
                Value touched = eval_file_touch_mode(ev, args[0].sval, mode.sval, site);
                if (val_is_signal(touched)) {
                    *out = touched;
                    return 1;
                }
                Value file_obj = val_object(ev->arena, recv);
                val_object_set_ivar(ev->arena, file_obj, "path", args[0]);
                val_object_set_ivar(ev->arena, file_obj, "mode", mode);
                val_object_set_ivar(ev->arena, file_obj, "closed", val_false());
                if (blk) {
                    Value result = call_block(ev, env, *blk, &file_obj, 1, site);
                    val_object_set_ivar(ev->arena, file_obj, "closed", val_true());
                    *out = result;
                } else {
                    *out = file_obj;
                }
            }
            return 1;
        }
    }
    if (strcmp(recv.klass->name, "Proc") == 0 && strcmp(name, "new") == 0) {
        if (!blk) {
            *out = eval_raise_class(ev, site, "ArgumentError", "Proc.new requires a block");
        } else {
            *out = val_proc(blk->block.block_node, blk->block.closure);
        }
        return 1;
    }
    if (strcmp(name, "new") == 0) {
        if (class_is_a_named_class(ev, recv.klass, "Exception")) {
            int ok = 1;
            Value message = exception_arg_message(ev, recv, args, argc, &ok, site);
            if (!ok) { *out = message; return 1; }
            Value obj = build_exception_object(ev, recv, message.sval);
            *out = obj;
            return 1;
        }
        Value obj = val_object(ev->arena, recv);
        RubyClass *klass = recv.klass;
        while (klass) {
            Value init_method;
            RubyClass *owner = NULL;
            if (ruby_class_find_instance_method(klass, "initialize", &init_method, &owner)) {
                Env *method_env = env_new(ev->arena, init_method.method.closure, 1);
                env_set(ev->arena, method_env, "self", obj);
                env_set(ev->arena, method_env, "__method__", val_symbol("initialize"));
                Value klass_val; klass_val.kind = VAL_CLASS; klass_val.klass = owner;
                env_set(ev->arena, method_env, "__class__", klass_val);
                if (blk) method_env->block_arg = blk;
                bind_params(ev, method_env, init_method.method.def_node->def.params, args, argc);
                ev->call_depth++;
                if (ev->active_def_count < EVAL_MAX_DEPTH)
                    ev->active_defs[ev->active_def_count++] = method_env;
                eval_push_frame(ev, site ? site->span.line : 0, site ? site->span.col : 0, "initialize");
                Value result = eval_node(ev, method_env, init_method.method.def_node->def.body);
                eval_pop_frame(ev);
                if (ev->active_def_count > 0) ev->active_def_count--;
                ev->call_depth--;
                if (result.kind == VAL_RETURN && result.jump.target_env == method_env)
                    result = *result.jump.wrapped;
                if (val_is_signal(result)) { *out = result; return 1; }
                (void)result;
                break;
            }
            klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
        }
        *out = obj;
        return 1;
    }
    /* ---- Class reflection ---- */

    if (strcmp(name, "superclass") == 0) {
        *out = recv.klass->superclass.kind == VAL_CLASS ? recv.klass->superclass : val_nil();
        return 1;
    }

    if (strcmp(name, "name") == 0) {
        *out = val_string(ev->arena, recv.klass->name);
        return 1;
    }

    if (strcmp(name, "ancestors") == 0) {
        Value arr = val_array_new();
        RubyClass *visited[256]; int nv = 0;
        collect_class_ancestors(recv.klass, &arr, visited, &nv);
        *out = arr;
        return 1;
    }

    if (strcmp(name, "instance_methods") == 0 ||
        strcmp(name, "public_instance_methods") == 0 ||
        strcmp(name, "private_instance_methods") == 0 ||
        strcmp(name, "protected_instance_methods") == 0) {
        int include_super = (argc == 0) || val_truthy(args[0]);
        int vis_mask;
        if (strcmp(name, "public_instance_methods") == 0) vis_mask = 1;
        else if (strcmp(name, "private_instance_methods") == 0) vis_mask = 4;
        else if (strcmp(name, "protected_instance_methods") == 0) vis_mask = 2;
        else vis_mask = 3; /* public + protected */
        Value arr = val_array_new();
        if (include_super) {
            RubyClass *visited[256]; int nv = 0;
            collect_all_instance_methods(recv.klass, &arr, vis_mask, visited, &nv);
        } else {
            collect_own_instance_methods(recv.klass->class_env, &arr, vis_mask);
        }
        *out = arr;
        return 1;
    }

    if (strcmp(name, "method_defined?") == 0 ||
        strcmp(name, "public_method_defined?") == 0 ||
        strcmp(name, "private_method_defined?") == 0 ||
        strcmp(name, "protected_method_defined?") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        const char *mname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!mname) { *out = eval_raise_class(ev, site, "TypeError", "expected Symbol or String"); return 1; }
        Value method; RubyClass *owner = NULL;
        if (!ruby_class_find_instance_method(recv.klass, mname, &method, &owner)) { *out = val_false(); return 1; }
        MethodVisibility vis = method.method.visibility;
        int match = 0;
        if (strcmp(name, "method_defined?") == 0) match = (vis == METHOD_PUBLIC || vis == METHOD_PROTECTED);
        else if (strcmp(name, "public_method_defined?") == 0) match = (vis == METHOD_PUBLIC);
        else if (strcmp(name, "private_method_defined?") == 0) match = (vis == METHOD_PRIVATE);
        else match = (vis == METHOD_PROTECTED);
        *out = val_bool(match);
        return 1;
    }

    if (strcmp(name, "instance_method") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        const char *mname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!mname) { *out = eval_raise_class(ev, site, "TypeError", "expected Symbol or String"); return 1; }
        Value method_val; RubyClass *owner = NULL;
        if (!ruby_class_find_instance_method(recv.klass, mname, &method_val, &owner)) {
            *out = eval_raise_class(ev, site, "NameError", "undefined method '%s' for class '%s'", mname, recv.klass->name);
            return 1;
        }
        Value ubm_klass;
        if (!env_get(ev->top_env, "UnboundMethod", &ubm_klass) || ubm_klass.kind != VAL_CLASS) { *out = val_nil(); return 1; }
        Value obj = val_object(ev->arena, ubm_klass);
        val_object_set_ivar(ev->arena, obj, "__klass__", recv);
        val_object_set_ivar(ev->arena, obj, "__method_name__", val_string(ev->arena, mname));
        val_object_set_ivar(ev->arena, obj, "__method__", method_val);
        *out = obj;
        return 1;
    }

    /* ---- end class reflection ---- */

    size_t nlen = strlen(name);
    char *key = arena_alloc(ev->arena, nlen + 6);
    memcpy(key, "self.", 5);
    memcpy(key + 5, name, nlen + 1);
    RubyClass *cklass = recv.klass;
    while (cklass) {
        Value cm;
        if (env_get(cklass->class_env, key, &cm) && cm.kind == VAL_METHOD) {
            if (method_visibility_allows_call(ev, env, recv, cklass, cm.method.visibility,
                                              public_only, explicit_receiver)) {
                Value result = call_method_value(ev, env, recv, cm, cklass, name, args, argc, blk, site);
                if (val_is_signal(result)) { *out = result; return 1; }
                *out = result;
                return 1;
            }
        }
        cklass = cklass->superclass.kind == VAL_CLASS ? cklass->superclass.klass : NULL;
    }
    return 0;
}

Value make_bound_method_proc(Eval *ev, Value receiver, const char *method_name);

int dispatch_object(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                    Value *blk, Node *site, Value *out, int public_only, int explicit_receiver) {
    if (recv.kind != VAL_OBJECT) return 0;

    /* Method and UnboundMethod objects */
    if (recv.obj->klass.kind == VAL_CLASS) {
        const char *kname = recv.obj->klass.klass->name;

        if (strcmp(kname, "Method") == 0) {
            if (strcmp(name, "call") == 0 || strcmp(name, "[]") == 0) {
                Value receiver, method_name_v;
                if (!val_object_get_ivar(recv, "__receiver__", &receiver) ||
                    !val_object_get_ivar(recv, "__method_name__", &method_name_v)) {
                    *out = eval_raise_class(ev, site, "RuntimeError", "invalid Method object");
                    return 1;
                }
                /* bypass visibility — Method#call always allowed */
                *out = dispatch_method(ev, env, receiver, method_name_v.sval, args, argc, blk, site, 0, -1);
                return 1;
            }
            if (strcmp(name, "arity") == 0) {
                Value method_val;
                if (val_object_get_ivar(recv, "__method__", &method_val) && method_val.kind == VAL_METHOD)
                    *out = val_int(proc_arity(method_val.method.def_node->def.params, 1));
                else
                    *out = val_int(-1);
                return 1;
            }
            if (strcmp(name, "to_proc") == 0) {
                Value receiver, method_name_v;
                if (!val_object_get_ivar(recv, "__receiver__", &receiver) ||
                    !val_object_get_ivar(recv, "__method_name__", &method_name_v)) {
                    *out = val_nil(); return 1;
                }
                *out = make_bound_method_proc(ev, receiver, method_name_v.sval);
                return 1;
            }
            return 0;
        }

        if (strcmp(kname, "UnboundMethod") == 0) {
            if (strcmp(name, "arity") == 0) {
                Value method_val;
                if (val_object_get_ivar(recv, "__method__", &method_val) && method_val.kind == VAL_METHOD)
                    *out = val_int(proc_arity(method_val.method.def_node->def.params, 1));
                else
                    *out = val_int(-1);
                return 1;
            }
            if (strcmp(name, "bind") == 0) {
                if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
                Value method_name_v, method_val;
                if (!val_object_get_ivar(recv, "__method_name__", &method_name_v) ||
                    !val_object_get_ivar(recv, "__method__", &method_val)) {
                    *out = val_nil(); return 1;
                }
                Value m_klass;
                if (!env_get(ev->top_env, "Method", &m_klass) || m_klass.kind != VAL_CLASS) { *out = val_nil(); return 1; }
                Value obj = val_object(ev->arena, m_klass);
                val_object_set_ivar(ev->arena, obj, "__receiver__", args[0]);
                val_object_set_ivar(ev->arena, obj, "__method_name__", method_name_v);
                val_object_set_ivar(ev->arena, obj, "__method__", method_val);
                *out = obj;
                return 1;
            }
            return 0;
        }
    }

    if (recv.obj->klass.kind == VAL_CLASS && strcmp(recv.obj->klass.klass->name, "IO") == 0) {
        Value fd_val;
        if (!val_object_get_ivar(recv, "__fd__", &fd_val) || fd_val.kind != VAL_STRING) {
            *out = eval_raise_class(ev, site, "IOError", "invalid IO object");
            return 1;
        }
        int is_stdin = strcmp(fd_val.sval, "stdin") == 0;
        FILE *stream = is_stdin ? stdin
                     : strcmp(fd_val.sval, "stderr") == 0 ? stderr
                     : ev->out;

        if (strcmp(name, "puts") == 0) {
            if (argc == 0) {
                fprintf(stream, "\n");
            } else {
                for (int i = 0; i < argc; i++) {
                    if (args[i].kind == VAL_ARRAY) {
                        for (size_t j = 0; j < args[i].array->len; j++)
                            fprintf(stream, "%s\n", val_to_s(ev->arena, args[i].array->elems[j]));
                    } else {
                        fprintf(stream, "%s\n", val_to_s(ev->arena, args[i]));
                    }
                }
            }
            *out = val_nil();
            return 1;
        }
        if (strcmp(name, "print") == 0) {
            for (int i = 0; i < argc; i++)
                fprintf(stream, "%s", val_to_s(ev->arena, args[i]));
            *out = val_nil();
            return 1;
        }
        if (strcmp(name, "write") == 0) {
            if (argc < 1 || args[0].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "IO#write requires a String");
                return 1;
            }
            size_t len = strlen(args[0].sval);
            fwrite(args[0].sval, 1, len, stream);
            *out = val_int((int64_t)len);
            return 1;
        }
        if (strcmp(name, "<<") == 0) {
            if (argc >= 1)
                fprintf(stream, "%s", val_to_s(ev->arena, args[0]));
            *out = recv;
            return 1;
        }
        if (strcmp(name, "flush") == 0) {
            fflush(stream);
            *out = recv;
            return 1;
        }
        if (strcmp(name, "sync") == 0) {
            *out = val_true();
            return 1;
        }
        if (strcmp(name, "sync=") == 0) {
            *out = argc > 0 ? args[0] : val_nil();
            return 1;
        }
        if (strcmp(name, "fileno") == 0) {
            *out = val_int(is_stdin ? 0 : strcmp(fd_val.sval, "stderr") == 0 ? 2 : 1);
            return 1;
        }
        if (strcmp(name, "isatty") == 0 || strcmp(name, "tty?") == 0) {
            *out = val_false();
            return 1;
        }
        if (is_stdin && strcmp(name, "gets") == 0) {
            char buf[4096];
            if (!fgets(buf, sizeof(buf), stdin)) {
                *out = val_nil();
                return 1;
            }
            if (!utf8_validate(buf, strlen(buf), NULL)) {
                *out = eval_raise_encoding_error(ev, site, "$stdin.gets");
                return 1;
            }
            *out = val_string(ev->arena, buf);
            return 1;
        }
        if (is_stdin && strcmp(name, "read") == 0) {
            size_t cap = 65536, len = 0;
            char *buf = arena_alloc(ev->arena, cap);
            int c;
            while ((c = fgetc(stdin)) != EOF) {
                if (len + 2 >= cap) {
                    char *nb = arena_alloc(ev->arena, cap * 2);
                    memcpy(nb, buf, len);
                    buf = nb;
                    cap *= 2;
                }
                buf[len++] = (char)c;
            }
            buf[len] = '\0';
            if (!utf8_validate(buf, len, NULL)) {
                *out = eval_raise_encoding_error(ev, site, "$stdin.read");
                return 1;
            }
            *out = val_string(ev->arena, buf);
            return 1;
        }
        return 0;
    }
    if (recv.obj->klass.kind == VAL_CLASS && strcmp(recv.obj->klass.klass->name, "File") == 0) {
        Value path, mode, closed;
        if (!val_object_get_ivar(recv, "path", &path) || path.kind != VAL_STRING) {
            *out = eval_raise_class(ev, site, "LoadError", "invalid File object");
            return 1;
        }
        if (!val_object_get_ivar(recv, "mode", &mode) || mode.kind != VAL_STRING) {
            *out = eval_raise_class(ev, site, "LoadError", "invalid File object");
            return 1;
        }
        if (val_object_get_ivar(recv, "closed", &closed) && val_truthy(closed) &&
            strcmp(name, "closed?") != 0 && strcmp(name, "close") != 0) {
            *out = eval_raise_class(ev, site, "LoadError", "closed file");
            return 1;
        }
        if (strcmp(name, "path") == 0) {
            *out = path;
            return 1;
        }
        if (strcmp(name, "mode") == 0) {
            *out = mode;
            return 1;
        }
        if (strcmp(name, "close") == 0) {
            val_object_set_ivar(ev->arena, recv, "closed", val_true());
            *out = val_nil();
            return 1;
        }
        if (strcmp(name, "closed?") == 0) {
            Value closed;
            if (val_object_get_ivar(recv, "closed", &closed)) {
                *out = closed;
            } else {
                *out = val_false();
            }
            return 1;
        }
        if (strcmp(name, "read") == 0) {
            if (argc != 0) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File#read takes no arguments");
            } else if (strcmp(mode.sval, "w") == 0 || strcmp(mode.sval, "a") == 0) {
                *out = eval_raise_class(ev, site, "LoadError", "not opened for reading");
            } else {
                *out = eval_file_read(ev, path.sval, site);
            }
            return 1;
        }
        if (strcmp(name, "write") == 0) {
            if (argc < 1) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File#write requires content");
            } else if (args[0].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "File#write content must be a String");
            } else if (strcmp(mode.sval, "r") == 0) {
                *out = eval_raise_class(ev, site, "LoadError", "not opened for writing");
            } else {
                *out = eval_file_append(ev, path.sval, args[0].sval, site);
            }
            return 1;
        }
        if (strcmp(name, "print") == 0) {
            if (strcmp(mode.sval, "r") == 0) {
                *out = eval_raise_class(ev, site, "LoadError", "not opened for writing");
                return 1;
            }
            size_t total = 1;
            for (int i = 0; i < argc; i++)
                total += strlen(val_to_s(ev->arena, args[i]));
            char *buf = arena_alloc(ev->arena, total);
            buf[0] = '\0';
            for (int i = 0; i < argc; i++)
                strcat(buf, val_to_s(ev->arena, args[i]));
            *out = eval_file_append(ev, path.sval, buf, site);
            return 1;
        }
        if (strcmp(name, "puts") == 0) {
            if (strcmp(mode.sval, "r") == 0) {
                *out = eval_raise_class(ev, site, "LoadError", "not opened for writing");
                return 1;
            }
            if (argc == 0) {
                *out = eval_file_append(ev, path.sval, "\n", site);
                return 1;
            }
            FILE *scratch = tmpfile();
            if (!scratch) {
                *out = eval_raise_class(ev, site, "LoadError", "cannot write file -- %s", path.sval);
                return 1;
            }
            for (int i = 0; i < argc; i++) {
                if (args[i].kind == VAL_ARRAY) {
                    for (size_t j = 0; j < args[i].array->len; j++)
                        fprintf(scratch, "%s\n", val_to_s(ev->arena, args[i].array->elems[j]));
                } else {
                    fprintf(scratch, "%s\n", val_to_s(ev->arena, args[i]));
                }
            }
            long len = ftell(scratch);
            rewind(scratch);
            char *buf = arena_alloc(ev->arena, (size_t)len + 1);
            fread(buf, 1, (size_t)len, scratch);
            buf[len] = '\0';
            fclose(scratch);
            *out = eval_file_append(ev, path.sval, buf, site);
            return 1;
        }
    }
    if (value_is_a_named_class(ev, recv, "Exception")) {
        if (strcmp(name, "message") == 0 || strcmp(name, "to_s") == 0) {
            *out = val_string(ev->arena, exception_value_message(ev, recv));
            return 1;
        }
        if (strcmp(name, "backtrace") == 0) {
            *out = exception_value_backtrace(recv);
            return 1;
        }
        if (strcmp(name, "inspect") == 0) {
            const char *klass = exception_value_class_name(recv);
            const char *msg = exception_value_message(ev, recv);
            size_t len = strlen(klass) + strlen(msg) + 5;
            char *buf = arena_alloc(ev->arena, len);
            snprintf(buf, len, "%s: %s", klass, msg);
            *out = val_string(ev->arena, buf);
            return 1;
        }
        if (strcmp(name, "exception") == 0) {
            int ok = 1;
            if (argc == 0 || (argc == 1 && val_equal(recv, args[0]))) {
                *out = recv;
                return 1;
            }
            Value message = exception_arg_message(ev, recv, args, argc, &ok, site);
            if (!ok) { *out = message; return 1; }
            Value klass;
            klass.kind = VAL_CLASS;
            klass.klass = recv.obj->klass.klass;
            Value copy = build_exception_object(ev, klass, message.sval);
            exception_set_backtrace(ev, copy, exception_value_backtrace(recv));
            Value line, col;
            if (val_object_get_ivar(recv, "line", &line)) val_object_set_ivar(ev->arena, copy, "line", line);
            if (val_object_get_ivar(recv, "col", &col)) val_object_set_ivar(ev->arena, copy, "col", col);
            *out = copy;
            return 1;
        }
        if (strcmp(name, "set_backtrace") == 0) {
            if (argc < 1) {
                *out = eval_raise_class(ev, site, "ArgumentError", "set_backtrace requires an argument");
                return 1;
            }
            if (args[0].kind != VAL_ARRAY && args[0].kind != VAL_NIL) {
                *out = eval_raise_class(ev, site, "TypeError", "set_backtrace requires an Array or nil");
                return 1;
            }
            exception_set_backtrace(ev, recv, args[0]);
            *out = args[0];
            return 1;
        }
    }
    if (recv.obj->singleton_env) {
        Value method;
        if (env_get(recv.obj->singleton_env, name, &method) && method.kind == VAL_METHOD) {
            RubyClass *owner = recv.obj->klass.kind == VAL_CLASS ? recv.obj->klass.klass : NULL;
            if (!method_visibility_allows_call(ev, env, recv, owner, method.method.visibility,
                                               public_only, explicit_receiver))
                return 0;
            Value result = call_method_value(ev, env, recv, method, owner, name, args, argc, blk, site);
            if (val_is_signal(result)) { *out = result; return 1; }
            *out = result;
            return 1;
        }
    }
    RubyClass *klass = recv.obj->klass.klass;
    while (klass) {
        Value method;
        RubyClass *owner = NULL;
        if (ruby_class_find_instance_method(klass, name, &method, &owner)) {
            if (!method_visibility_allows_call(ev, env, recv, owner, method.method.visibility,
                                               public_only, explicit_receiver))
                return 0;
            Value result = call_method_value(ev, env, recv, method, owner, name, args, argc, blk, site);
            if (val_is_signal(result)) { *out = result; return 1; }
            *out = result;
            return 1;
        }
        klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
    }
    return 0;
}
