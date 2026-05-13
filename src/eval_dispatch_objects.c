#define _POSIX_C_SOURCE 200809L

#include "eval_internal.h"
#include "utf8.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *file_fopen_mode(const char *mode) {
    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) return "rb";
    if (strcmp(mode, "r+") == 0 || strcmp(mode, "rb+") == 0 || strcmp(mode, "r+b") == 0) return "rb+";
    if (strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) return "wb";
    if (strcmp(mode, "w+") == 0 || strcmp(mode, "wb+") == 0 || strcmp(mode, "w+b") == 0) return "wb+";
    if (strcmp(mode, "a") == 0 || strcmp(mode, "ab") == 0) return "ab";
    if (strcmp(mode, "a+") == 0 || strcmp(mode, "ab+") == 0 || strcmp(mode, "a+b") == 0) return "ab+";
    return NULL;
}

static int mode_has_plus(const char *mode)  { return mode && strchr(mode, '+') != NULL; }
static int mode_is_read(const char *mode)   { return mode && (mode[0] == 'r' || mode_has_plus(mode)); }
static int mode_is_write(const char *mode)  { return mode && (mode[0] == 'w' || mode_has_plus(mode)); }
static int mode_is_append(const char *mode) { return mode && mode[0] == 'a'; }
static int mode_is_binary(const char *mode) { return mode && strchr(mode, 'b') != NULL; }
static int mode_allows_read(const char *mode){ return mode_is_read(mode); }
static int mode_allows_write(const char *mode){ return mode_is_write(mode) || mode_is_append(mode); }

static NativeFile *native_file(Value recv) {
    return recv.kind == VAL_OBJECT ? (NativeFile *)recv.obj->native : NULL;
}

static Value invalid_file_object(Eval *ev, Node *site) {
    return eval_raise_class(ev, site, "IOError", "invalid File object");
}

static Value closed_file_error(Eval *ev, Node *site) {
    return eval_raise_class(ev, site, "IOError", "closed stream");
}

static int ensure_open_native_file(Eval *ev __attribute__((unused)),
                                   Value recv,
                                   Node *site __attribute__((unused)),
                                   NativeFile **out) {
    NativeFile *nf = native_file(recv);
    if (!nf || !nf->fp) {
        *out = NULL;
        return 0;
    }
    *out = nf;
    return 1;
}

static int stream_sync_enabled(Value recv) {
    Value sync = val_false();
    return val_object_get_ivar(recv, "sync", &sync) && val_truthy(sync);
}

static Value maybe_flush_stream(Eval *ev, Value recv, NativeFile *nf, Node *site) {
    if (!stream_sync_enabled(recv))
        return val_nil();
    if (fflush(nf->fp) != 0)
        return eval_raise_class(ev, site, "IOError", "cannot flush file");
    return val_nil();
}

static Value wrong_arg_count(Eval *ev, Node *site, int given, int expected) {
    return eval_raise_class(ev, site, "ArgumentError",
                            "wrong number of arguments (given %d, expected %d)",
                            given, expected);
}

static const char *join_write_args(Eval *ev, Value *args, int argc) {
    size_t total = 1;
    for (int i = 0; i < argc; i++)
        total += strlen(val_to_s(ev->arena, args[i]));
    char *buf = arena_alloc(ev->arena, total);
    buf[0] = '\0';
    for (int i = 0; i < argc; i++)
        strcat(buf, val_to_s(ev->arena, args[i]));
    return buf;
}

static const char *infer_fd_mode(Eval *ev, int64_t fd_num, Node *site) {
    int flags = fcntl((int)fd_num, F_GETFL);
    if (flags < 0) {
        int err = errno;
        Value raised = eval_raise_class(ev, site, errno_class_name(err), "%s", strerror(err));
        (void)raised;
        return NULL;
    }
    switch (flags & O_ACCMODE) {
        case O_RDONLY: return "r";
        case O_WRONLY: return "w";
        case O_RDWR:   return "r+";
        default: {
            Value raised = eval_raise_class(ev, site, "Errno::EINVAL", "%s", strerror(EINVAL));
            (void)raised;
            return NULL;
        }
    }
}

static Value implicit_integer_conversion_error(Eval *ev, Value v, Node *site) {
    if (v.kind == VAL_NIL)
        return eval_raise_class(ev, site, "TypeError", "no implicit conversion from nil to integer");
    if (v.kind == VAL_BOOL)
        return eval_raise_class(ev, site, "TypeError", "no implicit conversion of %s into Integer",
                                v.bval ? "true" : "false");
    return eval_raise_class(ev, site, "TypeError", "no implicit conversion of %s into Integer",
                            value_class_name(ev, v));
}

static Value implicit_string_conversion_error(Eval *ev, Value v, Node *site) {
    if (v.kind == VAL_NIL)
        return eval_raise_class(ev, site, "TypeError", "no implicit conversion of nil into String");
    if (v.kind == VAL_BOOL)
        return eval_raise_class(ev, site, "TypeError", "no implicit conversion of %s into String",
                                v.bval ? "true" : "false");
    return eval_raise_class(ev, site, "TypeError", "no implicit conversion of %s into String",
                            value_class_name(ev, v));
}

static Value file_open_stream(Eval *ev, const char *path, const char *mode, Node *site) {
    const char *fmode = file_fopen_mode(mode);
    if (!fmode)
        return eval_raise_class(ev, site, "ArgumentError", "invalid access mode %s", mode);

    FILE *fp = fopen(path, fmode);
    if (!fp) {
        int err = errno;
        return eval_raise_class(ev, site, errno_class_name(err), "%s - %s", strerror(err), path);
    }

    NativeFile *nf = alloc_native_file(ev->arena, fp, 1);

    Value wrapper = val_nil();
    wrapper.kind = VAL_OBJECT;
    wrapper.obj = (RubyObject *)nf;
    return wrapper;
}

static Value file_close_stream(Eval *ev, Value recv, Node *site) {
    NativeFile *nf = native_file(recv);
    if (!nf || !nf->fp) return val_nil();
    if (nf->owns_fp && fclose(nf->fp) != 0) {
        nf->fp = NULL;
        return eval_raise_class(ev, site, "IOError", "cannot close file");
    }
    nf->fp = NULL;
    return val_nil();
}

static Value io_open_fd(Eval *ev, int64_t fd_num, const char *mode, Node *site) {
    if (fd_num < 0)
        return eval_raise_class(ev, site, errno_class_name(EBADF), "%s", strerror(EBADF));
    if (!mode) {
        mode = infer_fd_mode(ev, fd_num, site);
        if (!mode)
            return val_exception();
    }
    const char *fmode = file_fopen_mode(mode);
    if (!fmode)
        return eval_raise_class(ev, site, "ArgumentError", "invalid access mode %s", mode);

    int dup_fd = dup((int)fd_num);
    if (dup_fd < 0) {
        int err = errno;
        return eval_raise_class(ev, site, errno_class_name(err), "%s - %lld", strerror(err), (long long)fd_num);
    }

    FILE *fp = fdopen(dup_fd, fmode);
    if (!fp) {
        int err = errno;
        close(dup_fd);
        return eval_raise_class(ev, site, errno_class_name(err), "%s - %lld", strerror(err), (long long)fd_num);
    }

    Value wrapper = val_nil();
    wrapper.kind = VAL_OBJECT;
    wrapper.obj = (RubyObject *)alloc_native_file(ev->arena, fp, 1);
    return wrapper;
}

static Value file_read_stream(Eval *ev, Value recv, const char *mode, const char *context, Node *site) {
    if (!mode_allows_read(mode))
        return eval_raise_class(ev, site, "IOError", "not opened for reading");

    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf)
        return eval_raise_class(ev, site, "RuntimeError", "out of memory");

    int ch;
    while ((ch = fgetc(nf->fp)) != EOF) {
        if (len + 1 >= cap) {
            size_t next = cap * 2;
            char *nb = realloc(buf, next);
            if (!nb) {
                free(buf);
                return eval_raise_class(ev, site, "RuntimeError", "out of memory");
            }
            buf = nb;
            cap = next;
        }
        buf[len++] = (char)ch;
    }

    if (ferror(nf->fp)) {
        free(buf);
        clearerr(nf->fp);
        return eval_raise_class(ev, site, "IOError", "cannot read file");
    }

    buf[len] = '\0';
    if (!mode_is_binary(mode) && !utf8_validate(buf, len, NULL)) {
        free(buf);
        return eval_raise_encoding_error(ev, site, context);
    }

    Value out = val_string_n(ev->arena, buf, len);
    free(buf);
    return out;
}

static Value file_gets_stream(Eval *ev, Value recv, const char *mode, const char *context,
                              Value *args, int argc, Node *site) {
    if (argc > 1)
        return eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
    if (!mode_allows_read(mode))
        return eval_raise_class(ev, site, "IOError", "not opened for reading");

    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    const char *sep = "\n";
    size_t sep_len = 1;
    int read_all = 0;
    int paragraph = 0;
    if (argc == 1) {
        if (args[0].kind == VAL_NIL) {
            read_all = 1;
        } else if (args[0].kind == VAL_STRING) {
            sep = args[0].sval;
            sep_len = strlen(sep);
            if (sep_len == 0) paragraph = 1;
        } else {
            return eval_raise_class(ev, site, "TypeError", "%s separator must be a String or nil", context);
        }
    }

    size_t cap = 128;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf)
        return eval_raise_class(ev, site, "RuntimeError", "out of memory");

    int ch;
    while ((ch = fgetc(nf->fp)) != EOF) {
        if (len + 2 >= cap) {
            size_t next = cap * 2;
            char *nb = realloc(buf, next);
            if (!nb) {
                free(buf);
                return eval_raise_class(ev, site, "RuntimeError", "out of memory");
            }
            buf = nb;
            cap = next;
        }
        buf[len++] = (char)ch;

        if (read_all) continue;

        if (paragraph) {
            if (len >= 2 && buf[len - 1] == '\n' && buf[len - 2] == '\n')
                break;
            continue;
        }

        if (len >= sep_len && memcmp(buf + len - sep_len, sep, sep_len) == 0)
            break;
    }

    if (ferror(nf->fp)) {
        free(buf);
        clearerr(nf->fp);
        return eval_raise_class(ev, site, "IOError", "cannot read file");
    }

    if (len == 0) {
        free(buf);
        return val_nil();
    }

    buf[len] = '\0';
    if (!mode_is_binary(mode) && !utf8_validate(buf, len, NULL)) {
        free(buf);
        return eval_raise_encoding_error(ev, site, context);
    }

    Value out = val_string_n(ev->arena, buf, len);
    free(buf);
    return out;
}

static Value file_write_stream(Eval *ev, Value recv, const char *mode, const char *content, size_t len, Node *site) {
    if (!mode_allows_write(mode))
        return eval_raise_class(ev, site, "IOError", "not opened for writing");

    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    if (len && fwrite(content, 1, len, nf->fp) != len)
        return eval_raise_class(ev, site, "IOError", "cannot write file");

    Value flushed = maybe_flush_stream(ev, recv, nf, site);
    if (val_is_signal(flushed))
        return flushed;

    return val_int((int64_t)len);
}

static Value file_tell_stream(Eval *ev, Value recv, Node *site) {
    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    long pos = ftell(nf->fp);
    if (pos < 0) {
        int err = errno;
        clearerr(nf->fp);
        return eval_raise_class(ev, site, errno_class_name(err), "%s", strerror(err));
    }
    return val_int((int64_t)pos);
}

static Value file_seek_stream(Eval *ev, Value recv, Value *args, int argc, Node *site) {
    if (argc < 1 || argc > 2)
        return eval_raise_class(ev, site, "ArgumentError",
                                "wrong number of arguments (given %d, expected 1..2)", argc);
    if (args[0].kind != VAL_INT)
        return implicit_integer_conversion_error(ev, args[0], site);

    int whence = SEEK_SET;
    if (argc == 2) {
        if (args[1].kind != VAL_INT)
            return implicit_integer_conversion_error(ev, args[1], site);
        if (args[1].ival == 0) whence = SEEK_SET;
        else if (args[1].ival == 1) whence = SEEK_CUR;
        else if (args[1].ival == 2) whence = SEEK_END;
        else return eval_raise_class(ev, site, "Errno::EINVAL", "%s", strerror(EINVAL));
    }

    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    if (fseek(nf->fp, (long)args[0].ival, whence) != 0) {
        int err = errno;
        clearerr(nf->fp);
        return eval_raise_class(ev, site, errno_class_name(err), "%s", strerror(err));
    }
    clearerr(nf->fp);
    return val_int(0);
}

static Value file_rewind_stream(Eval *ev, Value recv, Node *site) {
    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);
    rewind(nf->fp);
    clearerr(nf->fp);
    return val_int(0);
}

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

static int object_is_named_class(Value recv, const char *name) {
    return recv.kind == VAL_OBJECT &&
           recv.obj->klass.kind == VAL_CLASS &&
           strcmp(recv.obj->klass.klass->name, name) == 0;
}

int value_is_regexp(Value v) {
    return object_is_named_class(v, "Regexp");
}

static Value md_group_str(Arena *a, const char *s, long beg_i, long end_i,
                           Value *cap_beg, Value *cap_end, int64_t ncaps, int64_t idx) {
    if (idx == 0)
        return val_string_n(a, s + beg_i, end_i >= beg_i ? (size_t)(end_i - beg_i) : 0);
    if (cap_beg && idx >= 1 && idx <= ncaps) {
        long gb = cap_beg->array->elems[idx-1].ival;
        long ge = cap_end->array->elems[idx-1].ival;
        return (gb < 0) ? val_nil() : val_string_n(a, s + gb, ge > gb ? (size_t)(ge - gb) : 0);
    }
    return val_nil();
}

static Value build_match_data(Eval *ev, Value regexp, Value string, RegexMatch match) {
    Value md_class;
    Value obj;

    if (!env_get(ev->top_env, "MatchData", &md_class) || md_class.kind != VAL_CLASS)
        return val_nil();
    obj = val_object(ev->arena, md_class);
    val_object_set_ivar(ev->arena, obj, "__regexp__", regexp);
    val_object_set_ivar(ev->arena, obj, "__string__", string);
    val_object_set_ivar(ev->arena, obj, "__beg__", val_int(match.beg));
    val_object_set_ivar(ev->arena, obj, "__end__", val_int(match.end));
    val_object_set_ivar(ev->arena, obj, "__ncaps__", val_int((int64_t)match.capture_count));
    if (match.capture_count > 0 && match.cap_beg && match.cap_end) {
        Value beg_arr = val_array_new();
        Value end_arr = val_array_new();
        for (size_t i = 0; i < match.capture_count; i++) {
            val_array_push(&beg_arr, val_int(match.cap_beg[i]));
            val_array_push(&end_arr, val_int(match.cap_end[i]));
        }
        val_object_set_ivar(ev->arena, obj, "__cap_beg__", beg_arr);
        val_object_set_ivar(ev->arena, obj, "__cap_end__", end_arr);
    }
    return obj;
}

Value regexp_search_value(Eval *ev, Value regexp, Value string, int return_index, Node *site) {
    Regex *compiled;
    RegexMatch match = {0, 0, 0, NULL, NULL};
    Value source;
    RegexError err = {0};
    RegexStatus status;

    if (!value_is_regexp(regexp))
        return eval_raise_class(ev, site, "TypeError", "expected Regexp");
    if (string.kind != VAL_STRING)
        return eval_raise_class(ev, site, "TypeError", "expected String");

    compiled = regexp.obj->native;
    if (!compiled) {
        if (!val_object_get_ivar(regexp, "source", &source) || source.kind != VAL_STRING)
            return eval_raise_class(ev, site, "RuntimeError", "invalid Regexp object");
        status = regex_compile(ev->arena, source.sval, 0, &compiled, &err);
        if (status != REGEX_OK)
            return eval_raise_class(ev, site, "RegexpError", "%s", err.message[0] ? err.message : "regexp compile failed");
        regexp.obj->native = compiled;
    }

    status = regex_search(compiled, string.sval, strlen(string.sval), 0, &match);
    if (status == REGEX_MISMATCH)
        return val_nil();
    if (status != REGEX_OK)
        return eval_raise_class(ev, site, "RuntimeError", "regexp search failed");
    Value result = return_index ? val_int(match.beg) : build_match_data(ev, regexp, string, match);
    regex_match_free(&match);
    return result;
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
    if (recv.klass->is_module && recv.klass->class_env) {
        Value const_val;
        if (env_get(recv.klass->class_env, name, &const_val) && const_val.kind == VAL_CLASS) {
            *out = const_val;
            return 1;
        }
    }
    if (strcmp(name, "new") == 0 && strcmp(recv.klass->name, "IO") == 0) {
        if (argc < 1 || argc > 2) {
            *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
        } else {
            if (args[0].kind != VAL_INT) {
                *out = implicit_integer_conversion_error(ev, args[0], site);
                return 1;
            }
            Value mode = val_nil();
            const char *mode_cstr = NULL;
            if (argc >= 2 && args[1].kind != VAL_NIL) {
                mode = args[1];
            } else {
                mode_cstr = infer_fd_mode(ev, args[0].ival, site);
                if (!mode_cstr) {
                    *out = val_exception();
                    return 1;
                }
                mode = val_string(ev->arena, mode_cstr);
            }
            if (mode.kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, mode, site);
                return 1;
            }
            Value opened = io_open_fd(ev, args[0].ival, mode.sval, site);
            if (val_is_signal(opened)) {
                *out = opened;
                return 1;
            }
            Value io_obj = val_object(ev->arena, recv);
            val_object_set_ivar(ev->arena, io_obj, "__fd_num__", args[0]);
            val_object_set_ivar(ev->arena, io_obj, "mode", mode);
            val_object_set_ivar(ev->arena, io_obj, "closed", val_false());
            val_object_set_ivar(ev->arena, io_obj, "sync", val_false());
            io_obj.obj->native = opened.obj;
            *out = io_obj;
        }
        return 1;
    }
    if (strcmp(recv.klass->name, "File") == 0) {
        if (strcmp(name, "read") == 0) {
            if (argc < 1) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File.read requires a path");
            } else if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
            } else {
                *out = eval_file_read(ev, args[0].sval, site);
            }
            return 1;
        }
        if (strcmp(name, "write") == 0) {
            if (argc < 2) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File.write requires a path and content");
            } else if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
            } else {
                *out = eval_file_write(ev, args[0].sval, val_to_s(ev->arena, args[1]), site);
            }
            return 1;
        }
        if (strcmp(name, "delete") == 0) {
            if (argc < 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
            } else {
                Value count = val_int(0);
                for (int i = 0; i < argc; i++) {
                    if (args[i].kind != VAL_STRING) {
                        *out = implicit_string_conversion_error(ev, args[i], site);
                        return 1;
                    }
                    Value deleted = eval_file_delete(ev, args[i].sval, site);
                    if (val_is_signal(deleted)) {
                        *out = deleted;
                        return 1;
                    }
                    count = val_int(count.ival + deleted.ival);
                }
                *out = count;
            }
            return 1;
        }
        if (strcmp(name, "exist?") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
            } else if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
            } else {
                *out = eval_file_exist(ev, args[0].sval);
            }
            return 1;
        }
        if (strcmp(name, "open") == 0) {
            if (argc < 1) {
                *out = eval_raise_class(ev, site, "ArgumentError", "File.open requires a path");
            } else if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
            } else {
                Value mode = (argc >= 2 && args[1].kind != VAL_NIL) ? args[1] : val_string(ev->arena, "r");
                if (mode.kind != VAL_STRING) {
                    *out = implicit_string_conversion_error(ev, mode, site);
                    return 1;
                }
                Value opened = file_open_stream(ev, args[0].sval, mode.sval, site);
                if (val_is_signal(opened)) {
                    *out = opened;
                    return 1;
                }
                Value file_obj = val_object(ev->arena, recv);
                val_object_set_ivar(ev->arena, file_obj, "path", args[0]);
                val_object_set_ivar(ev->arena, file_obj, "mode", mode);
                val_object_set_ivar(ev->arena, file_obj, "closed", val_false());
                val_object_set_ivar(ev->arena, file_obj, "sync", val_false());
                file_obj.obj->native = opened.obj;
                if (blk) {
                    Value result = call_block(ev, env, *blk, &file_obj, 1, site);
                    Value closed_result = file_close_stream(ev, file_obj, site);
                    val_object_set_ivar(ev->arena, file_obj, "closed", val_true());
                    if (val_is_signal(closed_result)) {
                        *out = closed_result;
                        return 1;
                    }
                    if (result.kind == VAL_BREAK)
                        result = *result.jump.wrapped;
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
    if ((strcmp(name, "escape") == 0 || strcmp(name, "quote") == 0) &&
        strcmp(recv.klass->name, "Regexp") == 0) {
        if (argc < 1 || args[0].kind != VAL_STRING) {
            *out = eval_raise_class(ev, site, "TypeError", "Regexp.escape requires a String");
            return 1;
        }
        const char *s = args[0].sval ? args[0].sval : "";
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen * 2 + 1);
        size_t j = 0;
        for (size_t i = 0; i < slen; i++) {
            char c = s[i];
            if (c == '\\' || c == '.' || c == '|' || c == '^' || c == '$' ||
                c == '?' || c == '*' || c == '+' || c == '(' || c == ')' ||
                c == '[' || c == ']' || c == '{' || c == '}' || c == '#')
                buf[j++] = '\\';
            buf[j++] = c;
        }
        buf[j] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "new") == 0) {
        if (strcmp(recv.klass->name, "Regexp") == 0) {
            Value obj;
            Regex *compiled = NULL;
            RegexError err = {0};
            unsigned int options = 0;
            if (argc < 1 || argc > 2) {
                *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = eval_raise_class(ev, site, "TypeError", "Regexp.new pattern must be a String");
                return 1;
            }
            if (argc >= 2 && args[1].kind == VAL_INT)
                options = (unsigned int)args[1].ival;
            if (regex_compile(ev->arena, args[0].sval, options, &compiled, &err) != REGEX_OK) {
                *out = eval_raise_class(ev, site, "RegexpError", "%s", err.message[0] ? err.message : "regexp compile failed");
                return 1;
            }
            obj = val_object(ev->arena, recv);
            obj.obj->native = compiled;
            val_object_set_ivar(ev->arena, obj, "source", args[0]);
            val_object_set_ivar(ev->arena, obj, "__options__", val_int((int64_t)options));
            *out = obj;
            return 1;
        }
        if (strcmp(recv.klass->name, "Array") == 0) {
            if (argc > 2) {
                *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                return 1;
            }
            if (argc == 0) {
                *out = val_array_new();
                return 1;
            }
            if (args[0].kind != VAL_INT) {
                *out = eval_raise_class(ev, site, "TypeError", "Array.new size must be an Integer");
                return 1;
            }
            if (args[0].ival < 0) {
                *out = eval_raise_class(ev, site, "ArgumentError", "negative array size");
                return 1;
            }
            Value arr = val_array_new();
            for (int64_t i = 0; i < args[0].ival; i++) {
                Value elem = argc > 1 ? args[1] : val_nil();
                if (blk) {
                    Value idx = val_int(i);
                    elem = call_block(ev, env, *blk, &idx, 1, site);
                    if (val_is_signal(elem)) { *out = elem; return 1; }
                }
                val_array_push(&arr, elem);
            }
            *out = arr;
            return 1;
        }
        if (strcmp(recv.klass->name, "Hash") == 0) {
            Value default_value = argc > 0 ? args[0] : val_nil();
            Value default_proc = blk ? *blk : val_nil();
            *out = val_hash_new_with_defaults(ev->arena, default_value, default_proc);
            return 1;
        }
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

    if (strcmp(name, "alias_method") == 0) {
        if (argc != 2) {
            *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
            return 1;
        }
        const char *new_name = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        const char *old_name = (args[1].kind == VAL_SYMBOL || args[1].kind == VAL_STRING) ? args[1].sval : NULL;
        if (!new_name || !old_name) {
            *out = eval_raise_class(ev, site, "TypeError", "expected Symbol or String");
            return 1;
        }
        Value method;
        if (!ruby_class_find_instance_method(recv.klass, old_name, &method, NULL)) {
            *out = eval_raise_class(ev, site, "NameError",
                                    "undefined method '%s' for alias_method", old_name);
            return 1;
        }
        env_define(ev->arena, recv.klass->class_env, new_name, method);
        *out = val_symbol(new_name);
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
            if (cm.method.visibility == METHOD_PROTECTED) {
                *out = eval_raise_class(ev, site, "NoMethodError",
                                        "protected method '%s' called for an instance of %s",
                                        name, value_class_name(ev, recv));
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

        if (strcmp(kname, "Regexp") == 0) {
            if (strcmp(name, "source") == 0) {
                Value source;
                if (!val_object_get_ivar(recv, "source", &source))
                    source = val_string(ev->arena, "");
                *out = source;
                return 1;
            }
            if (strcmp(name, "inspect") == 0 || strcmp(name, "to_s") == 0) {
                Value source;
                size_t n;
                char *buf;
                if (!val_object_get_ivar(recv, "source", &source) || source.kind != VAL_STRING)
                    source = val_string(ev->arena, "");
                n = strlen(source.sval);
                buf = arena_alloc(ev->arena, n + 3);
                buf[0] = '/';
                memcpy(buf + 1, source.sval, n);
                buf[n + 1] = '/';
                buf[n + 2] = '\0';
                *out = val_string(ev->arena, buf);
                return 1;
            }
            if (strcmp(name, "options") == 0) {
                Value opts;
                *out = (val_object_get_ivar(recv, "__options__", &opts) && opts.kind == VAL_INT)
                       ? opts : val_int(0);
                return 1;
            }
            if (strcmp(name, "match") == 0) {
                if (argc < 1) {
                    *out = eval_raise_class(ev, site, "ArgumentError", "Regexp#match requires an argument");
                    return 1;
                }
                Value md = regexp_search_value(ev, recv, args[0], 0, site);
                if (ev->errored) { *out = md; return 1; }
                if (blk && md.kind != VAL_NIL) {
                    Value r = call_block(ev, env, *blk, &md, 1, site);
                    if (ev->errored || val_is_signal(r)) { *out = r; return 1; }
                    *out = r;
                    return 1;
                }
                *out = md;
                return 1;
            }
            if (strcmp(name, "match?") == 0) {
                if (argc < 1) {
                    *out = eval_raise_class(ev, site, "ArgumentError", "Regexp#match? requires an argument");
                    return 1;
                }
                Value md = regexp_search_value(ev, recv, args[0], 0, site);
                if (ev->errored) { *out = md; return 1; }
                *out = val_bool(md.kind != VAL_NIL);
                return 1;
            }
            if (strcmp(name, "=~") == 0) {
                if (argc < 1) {
                    *out = val_nil();
                    return 1;
                }
                *out = regexp_search_value(ev, recv, args[0], 1, site);
                return 1;
            }
            if (strcmp(name, "===") == 0) {
                if (argc < 1) {
                    *out = val_false();
                    return 1;
                }
                Value md = regexp_search_value(ev, recv, args[0], 0, site);
                if (ev->errored) { *out = md; return 1; }
                *out = val_bool(md.kind != VAL_NIL);
                return 1;
            }
        }

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

        if (strcmp(kname, "MatchData") == 0) {
            Value string, regexp, beg_v, end_v, ncaps_v, cap_beg_v, cap_end_v;
            long beg_i, end_i;
            int64_t ncaps = 0;

            if (!val_object_get_ivar(recv, "__string__", &string))
                string = val_string(ev->arena, "");
            if (!val_object_get_ivar(recv, "__regexp__", &regexp))
                regexp = val_nil();
            if (!val_object_get_ivar(recv, "__beg__", &beg_v) || beg_v.kind != VAL_INT)
                beg_v = val_int(0);
            if (!val_object_get_ivar(recv, "__end__", &end_v) || end_v.kind != VAL_INT)
                end_v = val_int(0);
            if (!val_object_get_ivar(recv, "__ncaps__", &ncaps_v) || ncaps_v.kind != VAL_INT)
                ncaps_v = val_int(0);
            beg_i  = beg_v.ival;
            end_i  = end_v.ival;
            ncaps  = ncaps_v.ival;
            int has_caps = val_object_get_ivar(recv, "__cap_beg__", &cap_beg_v) &&
                           val_object_get_ivar(recv, "__cap_end__", &cap_end_v) &&
                           cap_beg_v.kind == VAL_ARRAY && cap_end_v.kind == VAL_ARRAY;

            /* Helper: string for group idx (0 = overall, 1..n = captures) */
#define MD_GROUP_STR(idx) md_group_str(ev->arena, string.sval, beg_i, end_i, \
                                       has_caps ? &cap_beg_v : NULL, \
                                       has_caps ? &cap_end_v : NULL, ncaps, (idx))

            if (strcmp(name, "to_s") == 0) {
                *out = val_string_n(ev->arena, string.sval + beg_i, end_i >= beg_i ? (size_t)(end_i - beg_i) : 0);
                return 1;
            }
            if (strcmp(name, "inspect") == 0) {
                Value matched = val_string_n(ev->arena, string.sval + beg_i, end_i >= beg_i ? (size_t)(end_i - beg_i) : 0);
                const char *body = val_inspect(ev->arena, matched);
                size_t n = strlen(body);
                char *buf = arena_alloc(ev->arena, n + 14);
                memcpy(buf, "#<MatchData ", 12);
                memcpy(buf + 12, body, n);
                buf[12 + n] = '>'; buf[13 + n] = '\0';
                *out = val_string(ev->arena, buf);
                return 1;
            }
            if (strcmp(name, "string") == 0)  { *out = string;  return 1; }
            if (strcmp(name, "regexp") == 0)  { *out = regexp;  return 1; }
            if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0) {
                *out = val_int(ncaps + 1);
                return 1;
            }
            if (strcmp(name, "captures") == 0) {
                Value arr = val_array_new();
                for (int64_t i = 1; i <= ncaps; i++)
                    val_array_push(&arr, MD_GROUP_STR(i));
                *out = arr;
                return 1;
            }
            if (strcmp(name, "[]") == 0) {
                if (argc < 1) {
                    *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                } else if (args[0].kind != VAL_INT) {
                    *out = eval_raise_class(ev, site, "TypeError", "MatchData#[] index must be an Integer");
                } else {
                    int64_t idx = args[0].ival;
                    if (idx < 0) idx += ncaps + 1;
                    if (idx < 0 || idx > ncaps) *out = val_nil();
                    else *out = MD_GROUP_STR(idx);
                }
                return 1;
            }
            if (strcmp(name, "begin") == 0) {
                int64_t idx = (argc >= 1 && args[0].kind == VAL_INT) ? args[0].ival : 0;
                if (idx == 0) { *out = val_int(beg_i); }
                else if (has_caps && idx >= 1 && idx <= ncaps) {
                    long gb = cap_beg_v.array->elems[idx-1].ival;
                    *out = (gb < 0) ? val_nil() : val_int(gb);
                } else { *out = val_nil(); }
                return 1;
            }
            if (strcmp(name, "end") == 0) {
                int64_t idx = (argc >= 1 && args[0].kind == VAL_INT) ? args[0].ival : 0;
                if (idx == 0) { *out = val_int(end_i); }
                else if (has_caps && idx >= 1 && idx <= ncaps) {
                    long ge = cap_end_v.array->elems[idx-1].ival;
                    *out = (ge < 0) ? val_nil() : val_int(ge);
                } else { *out = val_nil(); }
                return 1;
            }
            if (strcmp(name, "pre_match") == 0) {
                *out = val_string_n(ev->arena, string.sval, beg_i > 0 ? (size_t)beg_i : 0);
                return 1;
            }
            if (strcmp(name, "post_match") == 0) {
                size_t slen = strlen(string.sval);
                size_t off = end_i > 0 ? (size_t)end_i : 0;
                if (off > slen) off = slen;
                *out = val_string_n(ev->arena, string.sval + off, slen - off);
                return 1;
            }
#undef MD_GROUP_STR
        }
    }

    if (recv.obj->klass.kind == VAL_CLASS && strcmp(recv.obj->klass.klass->name, "IO") == 0) {
        Value mode = val_string(ev->arena, "r");
        Value closed;
        Value fd_num_val = val_int(-1);
        if (strcmp(name, "close") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            Value closed_result = file_close_stream(ev, recv, site);
            val_object_set_ivar(ev->arena, recv, "closed", val_true());
            *out = closed_result;
            return 1;
        }
        if (strcmp(name, "closed?") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            if (val_object_get_ivar(recv, "closed", &closed)) *out = closed;
            else *out = val_false();
            return 1;
        }
        if (val_object_get_ivar(recv, "closed", &closed) && val_truthy(closed) &&
            strcmp(name, "closed?") != 0 && strcmp(name, "close") != 0) {
            *out = closed_file_error(ev, site);
            return 1;
        }
        if (val_object_get_ivar(recv, "mode", &mode) && mode.kind != VAL_STRING) {
            *out = eval_raise_class(ev, site, "IOError", "invalid IO object");
            return 1;
        }
        NativeFile *nf = NULL;
        if (!ensure_open_native_file(ev, recv, site, &nf)) {
            *out = eval_raise_class(ev, site, "IOError", "invalid IO object");
            return 1;
        }
        FILE *stream = nf->fp;
        if (val_object_get_ivar(recv, "__fd_num__", &fd_num_val) && fd_num_val.kind != VAL_INT) {
            *out = eval_raise_class(ev, site, "IOError", "invalid IO object");
            return 1;
        }
        int64_t fd_num = fd_num_val.kind == VAL_INT ? fd_num_val.ival : -1;

        if (strcmp(name, "puts") == 0) {
            if (!mode_allows_write(mode.sval)) {
                *out = eval_raise_class(ev, site, "IOError", "not opened for writing");
                return 1;
            }
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
            Value flushed = maybe_flush_stream(ev, recv, nf, site);
            if (val_is_signal(flushed)) {
                *out = flushed;
                return 1;
            }
            *out = val_nil();
            return 1;
        }
        if (strcmp(name, "print") == 0) {
            if (!mode_allows_write(mode.sval)) {
                *out = eval_raise_class(ev, site, "IOError", "not opened for writing");
                return 1;
            }
            for (int i = 0; i < argc; i++)
                fprintf(stream, "%s", val_to_s(ev->arena, args[i]));
            Value flushed = maybe_flush_stream(ev, recv, nf, site);
            if (val_is_signal(flushed)) {
                *out = flushed;
                return 1;
            }
            *out = val_nil();
            return 1;
        }
        if (strcmp(name, "write") == 0) {
            const char *content = join_write_args(ev, args, argc);
            *out = file_write_stream(ev, recv, mode.sval, content, strlen(content), site);
            return 1;
        }
        if (strcmp(name, "<<") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            if (!mode_allows_write(mode.sval)) {
                *out = eval_raise_class(ev, site, "IOError", "not opened for writing");
                return 1;
            }
            fprintf(stream, "%s", val_to_s(ev->arena, args[0]));
            Value flushed = maybe_flush_stream(ev, recv, nf, site);
            if (val_is_signal(flushed)) {
                *out = flushed;
                return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "flush") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            fflush(stream);
            *out = recv;
            return 1;
        }
        if (strcmp(name, "sync") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            Value sync = val_false();
            if (val_object_get_ivar(recv, "sync", &sync))
                *out = sync;
            else
                *out = val_false();
            return 1;
        }
        if (strcmp(name, "sync=") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            Value sync = val_bool(val_truthy(args[0]));
            val_object_set_ivar(ev->arena, recv, "sync", sync);
            *out = sync;
            return 1;
        }
        if (strcmp(name, "fileno") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            if (fd_num >= 0) *out = val_int(fd_num);
            else *out = val_int(fileno(stream));
            return 1;
        }
        if (strcmp(name, "isatty") == 0 || strcmp(name, "tty?") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = val_false();
            return 1;
        }
        if (strcmp(name, "gets") == 0) {
            *out = file_gets_stream(ev, recv, mode.sval, "IO#gets", args, argc, site);
            return 1;
        }
        if (strcmp(name, "read") == 0) {
            *out = file_read_stream(ev, recv, mode.sval, "IO#read", site);
            return 1;
        }
        if (strcmp(name, "tell") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_tell_stream(ev, recv, site);
            return 1;
        }
        if (strcmp(name, "seek") == 0) {
            *out = file_seek_stream(ev, recv, args, argc, site);
            return 1;
        }
        if (strcmp(name, "rewind") == 0) {
            if (argc != 0) {
                *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                return 1;
            }
            *out = file_rewind_stream(ev, recv, site);
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
        if (strcmp(name, "path") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = path;
            return 1;
        }
        if (strcmp(name, "mode") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = mode;
            return 1;
        }
        if (val_object_get_ivar(recv, "closed", &closed) && val_truthy(closed) &&
            strcmp(name, "closed?") != 0 && strcmp(name, "close") != 0) {
            *out = closed_file_error(ev, site);
            return 1;
        }
        if (strcmp(name, "sync") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            Value sync = val_false();
            if (val_object_get_ivar(recv, "sync", &sync))
                *out = sync;
            else
                *out = val_false();
            return 1;
        }
        if (strcmp(name, "sync=") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            Value sync = val_bool(val_truthy(args[0]));
            val_object_set_ivar(ev->arena, recv, "sync", sync);
            *out = sync;
            return 1;
        }
        if (strcmp(name, "flush") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            NativeFile *nf = NULL;
            if (!ensure_open_native_file(ev, recv, site, &nf)) {
                *out = invalid_file_object(ev, site);
                return 1;
            }
            fflush(nf->fp);
            *out = recv;
            return 1;
        }
        if (strcmp(name, "fileno") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            NativeFile *nf = NULL;
            if (!ensure_open_native_file(ev, recv, site, &nf)) {
                *out = invalid_file_object(ev, site);
                return 1;
            }
            *out = val_int(fileno(nf->fp));
            return 1;
        }
        if (strcmp(name, "isatty") == 0 || strcmp(name, "tty?") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            NativeFile *nf = NULL;
            if (!ensure_open_native_file(ev, recv, site, &nf)) {
                *out = invalid_file_object(ev, site);
                return 1;
            }
            *out = val_bool(isatty(fileno(nf->fp)));
            return 1;
        }
        if (strcmp(name, "close") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            Value closed_result = file_close_stream(ev, recv, site);
            val_object_set_ivar(ev->arena, recv, "closed", val_true());
            *out = closed_result;
            return 1;
        }
        if (strcmp(name, "closed?") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
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
            } else {
                *out = file_read_stream(ev, recv, mode.sval, "File#read", site);
            }
            return 1;
        }
        if (strcmp(name, "gets") == 0) {
            *out = file_gets_stream(ev, recv, mode.sval, "File#gets", args, argc, site);
            return 1;
        }
        if (strcmp(name, "write") == 0) {
            const char *content = join_write_args(ev, args, argc);
            *out = file_write_stream(ev, recv, mode.sval, content, strlen(content), site);
            return 1;
        }
        if (strcmp(name, "<<") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            const char *content = val_to_s(ev->arena, args[0]);
            Value wrote = file_write_stream(ev, recv, mode.sval, content, strlen(content), site);
            if (val_is_signal(wrote)) {
                *out = wrote;
                return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "print") == 0) {
            size_t total = 1;
            for (int i = 0; i < argc; i++)
                total += strlen(val_to_s(ev->arena, args[i]));
            char *buf = arena_alloc(ev->arena, total);
            buf[0] = '\0';
            for (int i = 0; i < argc; i++)
                strcat(buf, val_to_s(ev->arena, args[i]));
            Value wrote = file_write_stream(ev, recv, mode.sval, buf, strlen(buf), site);
            if (val_is_signal(wrote)) {
                *out = wrote;
                return 1;
            }
            *out = val_nil();
            return 1;
        }
        if (strcmp(name, "puts") == 0) {
            if (argc == 0) {
                Value wrote = file_write_stream(ev, recv, mode.sval, "\n", 1, site);
                if (val_is_signal(wrote)) {
                    *out = wrote;
                    return 1;
                }
                *out = val_nil();
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
            Value wrote = file_write_stream(ev, recv, mode.sval, buf, (size_t)len, site);
            if (val_is_signal(wrote)) {
                *out = wrote;
                return 1;
            }
            *out = val_nil();
            return 1;
        }
        if (strcmp(name, "tell") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_tell_stream(ev, recv, site);
            return 1;
        }
        if (strcmp(name, "seek") == 0) {
            *out = file_seek_stream(ev, recv, args, argc, site);
            return 1;
        }
        if (strcmp(name, "rewind") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_rewind_stream(ev, recv, site);
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
                                               public_only, explicit_receiver)) {
                if (method.method.visibility == METHOD_PROTECTED) {
                    *out = eval_raise_class(ev, site, "NoMethodError",
                                            "protected method '%s' called for an instance of %s",
                                            name, value_class_name(ev, recv));
                    return 1;
                }
                return 0;
            }
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
                                               public_only, explicit_receiver)) {
                if (method.method.visibility == METHOD_PROTECTED) {
                    *out = eval_raise_class(ev, site, "NoMethodError",
                                            "protected method '%s' called for an instance of %s",
                                            name, value_class_name(ev, recv));
                    return 1;
                }
                return 0;
            }
            Value result = call_method_value(ev, env, recv, method, owner, name, args, argc, blk, site);
            if (val_is_signal(result)) { *out = result; return 1; }
            *out = result;
            return 1;
        }
        klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
    }
    return 0;
}
