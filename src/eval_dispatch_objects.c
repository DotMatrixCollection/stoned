#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "eval_internal.h"
#include "utf8.h"
#include <math.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
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

static size_t regexp_union_piece_length(Value v) {
    if (v.kind == VAL_OBJECT && v.obj->klass.kind == VAL_CLASS &&
        strcmp(v.obj->klass.klass->name, "Regexp") == 0) {
        Value source = val_nil();
        if (val_object_get_ivar(v, "source", &source) && source.kind == VAL_STRING)
            return strlen(source.sval ? source.sval : "");
    }
    const char *s = (v.kind == VAL_STRING || v.kind == VAL_SYMBOL) ? v.sval : NULL;
    if (!s) return strlen(val_kind_name(v.kind));
    size_t len = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == '.' || *p == '|' || *p == '^' || *p == '$' ||
            *p == '?' || *p == '*' || *p == '+' || *p == '(' || *p == ')' ||
            *p == '[' || *p == ']' || *p == '{' || *p == '}' || *p == '#')
            len++;
        len++;
    }
    return len;
}

static char *append_regexp_union_piece(char *dst, Value v) {
    if (v.kind == VAL_OBJECT && v.obj->klass.kind == VAL_CLASS &&
        strcmp(v.obj->klass.klass->name, "Regexp") == 0) {
        Value source = val_nil();
        if (val_object_get_ivar(v, "source", &source) && source.kind == VAL_STRING) {
            const char *s = source.sval ? source.sval : "";
            size_t len = strlen(s);
            memcpy(dst, s, len);
            return dst + len;
        }
    }
    const char *s = (v.kind == VAL_STRING || v.kind == VAL_SYMBOL) ? v.sval : NULL;
    if (!s) s = val_kind_name(v.kind);
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == '.' || *p == '|' || *p == '^' || *p == '$' ||
            *p == '?' || *p == '*' || *p == '+' || *p == '(' || *p == ')' ||
            *p == '[' || *p == ']' || *p == '{' || *p == '}' || *p == '#')
            *dst++ = '\\';
        *dst++ = *p;
    }
    return dst;
}

static Value default_console_winsize(Arena *arena) {
    Value size = val_array_new();
    val_array_push(&size, val_int(24));
    val_array_push(&size, val_int(80));
    return size;
}

static Value stream_winsize(Eval *ev, FILE *stream) {
    if (!stream)
        return default_console_winsize(ev->arena);

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    int fd = fileno(stream);
    if (fd >= 0 && ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        Value size = val_array_new();
        val_array_push(&size, val_int((int64_t)ws.ws_row));
        val_array_push(&size, val_int((int64_t)ws.ws_col));
        return size;
    }
    return default_console_winsize(ev->arena);
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

static int class_includes_module_name(RubyClass *klass, const char *name) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        for (RubyModuleInclusion *m = k->included_modules; m; m = m->next)
            if (strcmp(m->mod->name, name) == 0) return 1;
        for (RubyModuleInclusion *m = k->prepended_modules; m; m = m->next)
            if (strcmp(m->mod->name, name) == 0) return 1;
        if (k->is_module) break;
    }
    return 0;
}

static const char *path_expand_tilde(const char *path, char *buf, size_t buf_size) {
    if (!path)
        return "";
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home || home[0] == '\0') home = "/";
        snprintf(buf, buf_size, "%s%s", home, path + 1);
        return buf;
    }
    return path;
}

static int build_absolute_path(Eval *ev, const char *path, const char *base,
                               int expand_tilde, char *out, size_t out_size, Node *site) {
    char path_buf[PATH_MAX * 2];
    char base_buf[PATH_MAX * 2];
    const char *use_path = path ? path : "";
    const char *use_base = base;
    if (expand_tilde)
        use_path = path_expand_tilde(use_path, path_buf, sizeof(path_buf));
    if (use_base && expand_tilde)
        use_base = path_expand_tilde(use_base, base_buf, sizeof(base_buf));

    if (use_path[0] == '/') {
        snprintf(out, out_size, "%s", use_path);
        return 1;
    }

    char cwd[PATH_MAX];
    const char *resolved_base = use_base;
    if (!resolved_base || resolved_base[0] == '\0') {
        resolved_base = getcwd(cwd, sizeof(cwd));
        if (!resolved_base)
            return eval_raise_class(ev, site, errno_class_name(errno), "%s", strerror(errno)), 0;
    } else if (resolved_base[0] != '/') {
        char cwd2[PATH_MAX];
        const char *c = getcwd(cwd2, sizeof(cwd2));
        if (!c)
            return eval_raise_class(ev, site, errno_class_name(errno), "%s", strerror(errno)), 0;
        snprintf(out, out_size, "%s/%s", c, resolved_base);
        resolved_base = out;
    }

    snprintf(out, out_size, "%s/%s", resolved_base, use_path);
    return 1;
}

static Value lexical_normalize_path(Eval *ev, const char *abs_path) {
    int is_abs = (abs_path[0] == '/');
    size_t wlen = strlen(abs_path);
    char *work = arena_alloc(ev->arena, wlen + 1);
    memcpy(work, abs_path, wlen + 1);
    const char **comps = arena_alloc(ev->arena, sizeof(const char *) * (wlen / 2 + 4));
    int ncomps = 0;
    char *save_ptr = NULL;
    char *tok = strtok_r(work, "/", &save_ptr);
    while (tok) {
        if (strcmp(tok, ".") == 0) {
            /* skip */
        } else if (strcmp(tok, "..") == 0) {
            if (ncomps > 0) ncomps--;
        } else {
            comps[ncomps++] = tok;
        }
        tok = strtok_r(NULL, "/", &save_ptr);
    }
    char *result = arena_alloc(ev->arena, wlen + 4);
    size_t pos = 0;
    if (is_abs) result[pos++] = '/';
    for (int i = 0; i < ncomps; i++) {
        if (i > 0) result[pos++] = '/';
        size_t clen = strlen(comps[i]);
        memcpy(result + pos, comps[i], clen);
        pos += clen;
    }
    if (pos == 0) result[pos++] = '/';
    result[pos] = '\0';
    return val_string(ev->arena, result);
}

static Value build_time_value(Eval *ev, int64_t sec, long nsec) {
    Value time_class;
    if (!env_get(ev->top_env, "Time", &time_class) || time_class.kind != VAL_CLASS)
        return val_nil();
    Value obj = val_object(ev->arena, time_class);
    obj.obj->native = alloc_native_time(ev->arena, sec, nsec);
    return obj;
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

static Value file_read_stream_with_length(Eval *ev, Value recv, const char *mode, const char *context,
                                          Value *args, int argc, Node *site) {
    if (argc > 1)
        return eval_raise_class(ev, site, "ArgumentError",
                                "wrong number of arguments (given %d, expected 0..1)", argc);
    if (!mode_allows_read(mode))
        return eval_raise_class(ev, site, "IOError", "not opened for reading");

    if (argc == 0 || args[0].kind == VAL_NIL)
        return file_read_stream(ev, recv, mode, context, site);

    if (args[0].kind != VAL_INT)
        return implicit_integer_conversion_error(ev, args[0], site);
    if (args[0].ival < 0)
        return eval_raise_class(ev, site, "ArgumentError", "negative length %lld given",
                                (long long)args[0].ival);
    if (args[0].ival == 0)
        return val_string(ev->arena, "");

    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    size_t want = (size_t)args[0].ival;
    char *buf = malloc(want + 1);
    if (!buf)
        return eval_raise_class(ev, site, "RuntimeError", "out of memory");

    size_t got = fread(buf, 1, want, nf->fp);
    if (got == 0 && feof(nf->fp)) {
        free(buf);
        return val_nil();
    }
    if (ferror(nf->fp)) {
        free(buf);
        clearerr(nf->fp);
        return eval_raise_class(ev, site, "IOError", "cannot read file");
    }

    buf[got] = '\0';
    if (!mode_is_binary(mode) && !utf8_validate(buf, got, NULL)) {
        free(buf);
        return eval_raise_encoding_error(ev, site, context);
    }

    Value out = val_string_n(ev->arena, buf, got);
    free(buf);
    return out;
}

static Value file_gets_stream(Eval *ev, Value recv, const char *mode, const char *context,
                              Value *args, int argc, Node *site) {
    if (argc > 2)
        return eval_raise_class(ev, site, "ArgumentError",
                                "wrong number of arguments (given %d, expected 0..2)", argc);
    if (!mode_allows_read(mode))
        return eval_raise_class(ev, site, "IOError", "not opened for reading");

    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    const char *sep = "\n";
    size_t sep_len = 1;
    int read_all = 0;
    int paragraph = 0;
    int has_limit = 0;
    int64_t limit = 0;
    if (argc >= 1) {
        if (args[0].kind == VAL_INT) {
            has_limit = 1;
            limit = args[0].ival;
        } else if (args[0].kind == VAL_NIL) {
            read_all = 1;
        } else if (args[0].kind == VAL_STRING) {
            sep = args[0].sval;
            sep_len = strlen(sep);
            if (sep_len == 0) paragraph = 1;
        } else {
            return implicit_string_conversion_error(ev, args[0], site);
        }
    }
    if (argc == 2) {
        if (args[1].kind != VAL_INT)
            return implicit_integer_conversion_error(ev, args[1], site);
        has_limit = 1;
        limit = args[1].ival;
    }
    if (has_limit && limit == 0)
        return val_string(ev->arena, "");
    if (has_limit && limit < 0)
        has_limit = 0;

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
        if (has_limit && len >= (size_t)limit)
            break;

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

    if (paragraph) {
        size_t only_newlines = 1;
        for (size_t i = 0; i < len; i++) {
            if (buf[i] != '\n') {
                only_newlines = 0;
                break;
            }
        }
        if (only_newlines) {
            free(buf);
            return val_nil();
        }
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

static Value file_eof_stream(Eval *ev, Value recv, Node *site) {
    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    int ch = fgetc(nf->fp);
    if (ch == EOF) {
        if (ferror(nf->fp)) {
            clearerr(nf->fp);
            return eval_raise_class(ev, site, "IOError", "cannot read file");
        }
        return val_true();
    }

    ungetc(ch, nf->fp);
    return val_false();
}

static Value file_readline_stream(Eval *ev, Value recv, const char *mode, const char *context,
                                  Value *args, int argc, Node *site) {
    Value line = file_gets_stream(ev, recv, mode, context, args, argc, site);
    if (val_is_signal(line))
        return line;
    if (line.kind == VAL_NIL)
        return eval_raise_class(ev, site, "EOFError", "end of file reached");
    return line;
}

static Value file_getc_stream(Eval *ev, Value recv, const char *mode, const char *context, Node *site) {
    if (!mode_allows_read(mode))
        return eval_raise_class(ev, site, "IOError", "not opened for reading");

    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    int ch = fgetc(nf->fp);
    if (ch == EOF) {
        if (ferror(nf->fp)) {
            clearerr(nf->fp);
            return eval_raise_class(ev, site, "IOError", "cannot read file");
        }
        return val_nil();
    }

    char buf[4];
    buf[0] = (char)ch;
    size_t width = 1;
    if (!mode_is_binary(mode) && ((unsigned char)buf[0]) >= 0x80) {
        unsigned char c0 = (unsigned char)buf[0];
        if (c0 >= 0xC2 && c0 <= 0xDF) width = 2;
        else if (c0 >= 0xE0 && c0 <= 0xEF) width = 3;
        else if (c0 >= 0xF0 && c0 <= 0xF4) width = 4;
        else return eval_raise_encoding_error(ev, site, context);

        for (size_t i = 1; i < width; i++) {
            int next = fgetc(nf->fp);
            if (next == EOF) {
                if (ferror(nf->fp)) {
                    clearerr(nf->fp);
                    return eval_raise_class(ev, site, "IOError", "cannot read file");
                }
                return eval_raise_encoding_error(ev, site, context);
            }
            buf[i] = (char)next;
        }

        if (!utf8_decode_one(buf, width, NULL, NULL))
            return eval_raise_encoding_error(ev, site, context);
    }

    return val_string_n(ev->arena, buf, width);
}

static Value file_readchar_stream(Eval *ev, Value recv, const char *mode, const char *context, Node *site) {
    Value ch = file_getc_stream(ev, recv, mode, context, site);
    if (val_is_signal(ch))
        return ch;
    if (ch.kind == VAL_NIL)
        return eval_raise_class(ev, site, "EOFError", "end of file reached");
    return ch;
}

static Value file_getbyte_stream(Eval *ev, Value recv, const char *mode, Node *site) {
    if (!mode_allows_read(mode))
        return eval_raise_class(ev, site, "IOError", "not opened for reading");

    NativeFile *nf = NULL;
    if (!ensure_open_native_file(ev, recv, site, &nf))
        return invalid_file_object(ev, site);

    int ch = fgetc(nf->fp);
    if (ch == EOF) {
        if (ferror(nf->fp)) {
            clearerr(nf->fp);
            return eval_raise_class(ev, site, "IOError", "cannot read file");
        }
        return val_nil();
    }

    return val_int((unsigned char)ch);
}

static Value file_readbyte_stream(Eval *ev, Value recv, const char *mode, Node *site) {
    Value byte = file_getbyte_stream(ev, recv, mode, site);
    if (val_is_signal(byte))
        return byte;
    if (byte.kind == VAL_NIL)
        return eval_raise_class(ev, site, "EOFError", "end of file reached");
    return byte;
}

static Value file_readlines_stream(Eval *ev, Value recv, const char *mode, const char *context,
                                   Value *args, int argc, Node *site) {
    if (argc > 2)
        return eval_raise_class(ev, site, "ArgumentError",
                                "wrong number of arguments (given %d, expected 0..2)", argc);
    if (!mode_allows_read(mode))
        return eval_raise_class(ev, site, "IOError", "not opened for reading");

    Value lines = val_array_new();
    while (1) {
        Value line = file_gets_stream(ev, recv, mode, context, args, argc, site);
        if (val_is_signal(line))
            return line;
        if (line.kind == VAL_NIL)
            break;
        val_array_push(&lines, line);
    }
    return lines;
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

/* Extract named capture group names from a regexp pattern in order.
   Returns a VAL_ARRAY of strings (or nil for unnamed groups). */
static Value extract_named_groups(Arena *a, const char *pattern, size_t ncaps) {
    Value arr = val_array_new();
    if (!pattern || ncaps == 0) return arr;

    /* Pre-fill with nil */
    for (size_t i = 0; i < ncaps; i++)
        val_array_push(&arr, val_nil());

    /* Walk the pattern scanning for (?<name> or (?'name' */
    size_t cap_idx = 0;
    for (const char *p = pattern; *p; p++) {
        if (*p == '\\') { p++; continue; }
        if (*p == '[') {
            p++;
            while (*p && !(*p == ']' && *(p-1) != '\\')) p++;
            continue;
        }
        if (*p == '(' && *(p+1) == '?') {
            const char *q = p + 2;
            char close = 0;
            if (*q == '<' && *(q+1) != '=' && *(q+1) != '!') { q++; close = '>'; }
            else if (*q == '\'') { q++; close = '\''; }
            else { /* non-capturing or other — skip, don't count */ continue; }

            const char *name_start = q;
            while (*q && *q != close) q++;
            if (*q == close && q > name_start) {
                if (cap_idx < ncaps) {
                    char *name = arena_alloc(a, (size_t)(q - name_start) + 1);
                    memcpy(name, name_start, (size_t)(q - name_start));
                    name[q - name_start] = '\0';
                    arr.array->elems[cap_idx] = val_string(a, name);
                }
                cap_idx++;
                p = q;
            }
            continue;
        }
        if (*p == '(') cap_idx++;
    }
    return arr;
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

        /* Named capture group names, in group order */
        Value source;
        const char *pat = NULL;
        if (val_object_get_ivar(regexp, "source", &source) && source.kind == VAL_STRING)
            pat = source.sval;
        Value names = extract_named_groups(ev->arena, pat, match.capture_count);
        val_object_set_ivar(ev->arena, obj, "__cap_names__", names);
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

    static const char *cap_keys[] = {"1","2","3","4","5","6","7","8","9"};

    status = regex_search(compiled, string.sval, strlen(string.sval), 0, &match);
    if (status == REGEX_MISMATCH) {
        global_set(ev->arena, &ev->globals, "~", val_nil());
        for (int i = 0; i < 9; i++)
            global_set(ev->arena, &ev->globals, cap_keys[i], val_nil());
        return val_nil();
    }
    if (status != REGEX_OK)
        return eval_raise_class(ev, site, "RuntimeError", "regexp search failed");

    /* build MatchData only if needed ($~ or match method); =~ also needs it for $~ */
    Value md = build_match_data(ev, regexp, string, match);
    global_set(ev->arena, &ev->globals, "~", md);

    /* set $1..$9 from capture groups */
    const char *str = string.sval;
    for (size_t i = 0; i < 9; i++) {
        Value cap;
        if (i < match.capture_count && match.cap_beg && match.cap_end && match.cap_beg[i] >= 0) {
            size_t start = (size_t)match.cap_beg[i];
            size_t len   = (size_t)(match.cap_end[i] - match.cap_beg[i]);
            cap = val_string_n(ev->arena, str + start, len);
        } else {
            cap = val_nil();
        }
        global_set(ev->arena, &ev->globals, cap_keys[i], cap);
    }

    Value result = return_index ? val_int(match.beg) : md;
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

static int singleton_class_method_lookup(Eval *ev, Env *env, Value recv, const char *name,
                                         Value *out) {
    Value singleton_target = val_nil();
    if (!env_get(env, "__singleton_target__", &singleton_target))
        return 0;
    if (singleton_target.kind != recv.kind)
        return 0;
    if (singleton_target.kind == VAL_CLASS && singleton_target.klass != recv.klass)
        return 0;
    if (singleton_target.kind == VAL_OBJECT && singleton_target.obj != recv.obj)
        return 0;
    if (singleton_target.kind == VAL_ARRAY && singleton_target.array != recv.array)
        return 0;
    if (singleton_target.kind == VAL_HASH && singleton_target.hash != recv.hash)
        return 0;

    if (singleton_target.kind == VAL_CLASS) {
        size_t nlen = strlen(name);
        char *key = arena_alloc(ev->arena, nlen + 6);
        memcpy(key, "self.", 5);
        memcpy(key + 5, name, nlen + 1);
        return env_get(singleton_target.klass->class_env, key, out) && out->kind == VAL_METHOD;
    }

    Env *singleton_env = NULL;
    if (singleton_target.kind == VAL_OBJECT) singleton_env = singleton_target.obj->singleton_env;
    else if (singleton_target.kind == VAL_ARRAY) singleton_env = singleton_target.array->singleton_env;
    else if (singleton_target.kind == VAL_HASH) singleton_env = singleton_target.hash->singleton_env;
    return singleton_env && env_get(singleton_env, name, out) && out->kind == VAL_METHOD;
}

static const char *primitive_methods_for_class(const char *klass_name) {
    /* Returns a comma-sep list of primitive methods; used for instance_methods reflection */
    if (strcmp(klass_name, "Integer") == 0 || strcmp(klass_name, "Numeric") == 0)
        return "to_s,to_i,to_f,to_r,to_c,inspect,+,-,*,/,%,**,<,<=,>,>=,<=>,==,!=,abs,divmod,gcd,lcm,pow,digits,chr,succ,pred,next,times,upto,downto,step,zero?,nonzero?,positive?,negative?,odd?,even?,integer?,between?,clamp,floor,ceil,round,truncate,fdiv,remainder,gcd,lcm,bit_length,size,[]";
    if (strcmp(klass_name, "Float") == 0)
        return "to_s,to_i,to_f,to_r,inspect,+,-,*,/,%,**,<,<=,>,>=,<=>,==,abs,divmod,floor,ceil,round,truncate,nan?,infinite?,finite?,zero?,positive?,negative?,between?,clamp";
    if (strcmp(klass_name, "String") == 0)
        return "to_s,to_i,to_f,to_sym,to_str,length,size,empty?,upcase,downcase,capitalize,swapcase,strip,lstrip,rstrip,chomp,chop,chars,bytes,lines,split,join,include?,start_with?,end_with?,index,rindex,[],[]=,slice,replace,reverse,center,ljust,rjust,count,delete,squeeze,tr,scan,sub,gsub,match,match?,=~,ord,hex,oct,succ,next,encode,encoding,freeze,frozen?,dup,clone,inspect,<<,+,*,each_line,each_char,each_byte,insert,delete_prefix,delete_suffix,b,unicode_normalize,force_encoding,valid_encoding?,ascii_only?,bytesize";
    if (strcmp(klass_name, "Symbol") == 0)
        return "to_s,to_sym,to_proc,id2name,inspect,length,size,upcase,downcase,capitalize,match,match?,=~,[]";
    if (strcmp(klass_name, "Array") == 0)
        return "length,size,count,empty?,first,last,push,pop,shift,unshift,append,prepend,<<,+,-,&,|,*,flatten,compact,uniq,sort,sort_by,reverse,map,collect,select,filter,reject,each,each_with_index,each_with_object,each_slice,each_cons,flat_map,collect_concat,inject,reduce,zip,product,combination,permutation,sample,shuffle,include?,index,find_index,rindex,to_a,join,min,max,min_by,max_by,minmax,minmax_by,sum,any?,all?,none?,count,tally,group_by,chunk,chunk_while,slice_when,rotate,take,take_while,drop,drop_while,flatten,flatten!,inspect,to_s,freeze,frozen?,dup,clone,pack,[]";
    if (strcmp(klass_name, "Hash") == 0)
        return "keys,values,length,size,empty?,has_key?,has_value?,key?,value?,include?,member?,fetch,merge,merge!,update,delete,each,each_pair,each_key,each_value,map,select,filter,reject,any?,all?,none?,count,sum,flat_map,find,detect,min_by,max_by,sort_by,group_by,each_with_object,transform_keys,transform_values,transform_keys!,transform_values!,to_a,to_h,invert,inspect,to_s,freeze,frozen?,dup,clone,[],[]=";
    return NULL;
}

static int primitive_unbound_method_name(const char *name) {
    static const char *names[] = {
        "inspect", "to_s", "class",
        "methods", "public_methods", "private_methods", "protected_methods",
        "instance_variables", "method", "public_method", "constants",
        NULL
    };
    for (int i = 0; names[i]; i++) {
        if (strcmp(name, names[i]) == 0)
            return 1;
    }
    return 0;
}

int dispatch_class(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out, int public_only, int explicit_receiver) {
    if (recv.kind != VAL_CLASS) return 0;

    /* Kernel.method forwards to top-level builtin_kernel */
    if (strcmp(recv.klass->name, "Kernel") == 0) {
        /* instance_method(:name) — return UnboundMethod wrapping the named method */
        if (strcmp(name, "instance_method") == 0) {
            if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
            const char *mname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
            if (!mname) { *out = val_nil(); return 1; }
            Value ubm_klass;
            if (!env_get(ev->top_env, "UnboundMethod", &ubm_klass) || ubm_klass.kind != VAL_CLASS)
                { *out = val_nil(); return 1; }
            Value ubm = val_object(ev->arena, ubm_klass);
            val_object_set_ivar(ev->arena, ubm, "__klass__", recv);
            val_object_set_ivar(ev->arena, ubm, "__method_name__", val_string(ev->arena, mname));
            val_object_set_ivar(ev->arena, ubm, "__method__", val_nil()); /* native marker */
            *out = ubm; return 1;
        }
        extern Value builtin_kernel(Eval *ev, Env *env, const char *name,
                                    Value *args, int argc, Value *blk, Node *site);
        *out = builtin_kernel(ev, env, name, args, argc, blk, site);
        return 1;
    }

    if (strcmp(recv.klass->name, "Struct") == 0 && strcmp(name, "new") == 0) {
        static int struct_counter = 0;
        char anon_name[64];
        snprintf(anon_name, sizeof(anon_name), "Struct::Anonymous%d", ++struct_counter);
        Value klass = val_class(ev->arena, anon_name, recv);
        klass.klass->class_env = env_new(ev->arena, recv.klass->class_env, 1);

        Value members = val_array_new();
        for (int i = 0; i < argc; i++) {
            const char *member = (args[i].kind == VAL_SYMBOL || args[i].kind == VAL_STRING) ? args[i].sval : NULL;
            if (!member) {
                *out = eval_raise_class(ev, site, "TypeError", "Struct member name must be a Symbol or String");
                return 1;
            }
            val_array_push(&members, val_string(ev->arena, member));

            Arena *a = ev->arena;
            Node *ivar_node = arena_alloc(a, sizeof(Node));
            memset(ivar_node, 0, sizeof(Node));
            ivar_node->kind = NODE_IVAR;
            ivar_node->sval = member;

            NodeList *stmts = arena_alloc(a, sizeof(NodeList));
            stmts->node = ivar_node;
            stmts->next = NULL;

            Node *body = arena_alloc(a, sizeof(Node));
            memset(body, 0, sizeof(Node));
            body->kind = NODE_BODY;
            body->body.stmts = stmts;

            Node *def = arena_alloc(a, sizeof(Node));
            memset(def, 0, sizeof(Node));
            def->kind = NODE_DEF;
            def->def.name = member;
            def->def.body = body;

            env_define(a, klass.klass->class_env, member, val_method(def, ev->top_env, METHOD_PUBLIC));
        }

        env_define(ev->arena, klass.klass->class_env, "__struct_members__", members);

        /* Evaluate optional block in context of the new Struct class */
        if (blk && blk->kind == VAL_BLOCK && blk->block.block_node) {
            env_set(ev->arena, klass.klass->class_env, "self", klass);
            env_set(ev->arena, klass.klass->class_env, "__class__", klass);
            env_set(ev->arena, klass.klass->class_env, "__singleton_target__", val_nil());
            set_current_method_visibility(ev->arena, klass.klass->class_env, METHOD_PUBLIC);
            Node *blk_body = blk->block.block_node->block.body;
            if (blk_body) eval_node(ev, klass.klass->class_env, blk_body);
        }

        *out = klass;
        return 1;
    }
    if (strcmp(recv.klass->name, "Encoding") == 0 && strcmp(name, "find") == 0) {
        if (argc < 1 || args[0].kind != VAL_STRING) {
            *out = eval_raise_class(ev, site, "ArgumentError", "Encoding.find requires a String");
            return 1;
        }
        const char *enc_name = args[0].sval;
        Value enc_obj = val_nil();
        env_get(recv.klass->class_env, enc_name, &enc_obj);
        if (enc_obj.kind == VAL_NIL) {
            /* Return UTF-8 as a safe default for unknown encodings */
            env_get(recv.klass->class_env, "UTF_8", &enc_obj);
        }
        *out = enc_obj;
        return 1;
    }
    if (strcmp(recv.klass->name, "Shellwords") == 0 && strcmp(name, "split") == 0) {
        if (argc != 1 || args[0].kind != VAL_STRING) {
            *out = eval_raise_class(ev, site, "ArgumentError", "Shellwords.split requires a String");
            return 1;
        }
        const char *s = args[0].sval;
        Value arr = val_array_new();
        while (*s) {
            while (*s == ' ' || *s == '\t') s++;
            if (!*s) break;
            char buf[4096]; size_t blen = 0;
            char quote = 0;
            if (*s == '"' || *s == '\'') { quote = *s++; }
            while (*s && (quote ? (*s != quote) : (*s != ' ' && *s != '\t'))) {
                if (!quote && *s == '\\' && s[1]) { s++; }
                if (blen < sizeof(buf) - 1) buf[blen++] = *s;
                s++;
            }
            if (quote && *s == quote) s++;
            buf[blen] = '\0';
            val_array_push(&arr, val_string(ev->arena, buf));
        }
        *out = arr;
        return 1;
    }
    if (strcmp(recv.klass->name, "Pathname") == 0 && strcmp(name, "new") == 0) {
        if (argc != 1 || (args[0].kind != VAL_STRING && args[0].kind != VAL_SYMBOL)) {
            *out = eval_raise_class(ev, site, "TypeError", "Pathname.new requires a String");
            return 1;
        }
        Value obj = val_object(ev->arena, recv);
        val_object_set_ivar(ev->arena, obj, "path", val_string(ev->arena, args[0].sval));
        *out = obj;
        return 1;
    }
    size_t name_len = strlen(name);
    if (recv.klass->class_env && argc == 1 && name_len > 1 &&
        name[0] >= 'A' && name[0] <= 'Z' && name[name_len - 1] == '=') {
        char *const_name = arena_alloc(ev->arena, name_len);
        memcpy(const_name, name, name_len - 1);
        const_name[name_len - 1] = '\0';
        env_define(ev->arena, recv.klass->class_env, const_name, args[0]);
        size_t full_len = strlen(recv.klass->name) + 2 + (name_len - 1);
        char *full_name = arena_alloc(ev->arena, full_len + 1);
        memcpy(full_name, recv.klass->name, strlen(recv.klass->name));
        memcpy(full_name + strlen(recv.klass->name), "::", 2);
        memcpy(full_name + strlen(recv.klass->name) + 2, const_name, name_len);
        env_define(ev->arena, ev->top_env, full_name, args[0]);
        *out = args[0];
        return 1;
    }
    if (strcmp(recv.klass->name, "Math") == 0) {
        double arg = (argc > 0) ? (args[0].kind == VAL_FLOAT ? args[0].fval :
                     args[0].kind == VAL_INT ? (double)args[0].ival : 0.0) : 0.0;
        double arg2 = (argc > 1) ? (args[1].kind == VAL_FLOAT ? args[1].fval :
                      args[1].kind == VAL_INT ? (double)args[1].ival : 0.0) : 0.0;
        if (strcmp(name, "sqrt") == 0) { *out = val_float(sqrt(arg)); return 1; }
        if (strcmp(name, "cbrt") == 0) { *out = val_float(cbrt(arg)); return 1; }
        if (strcmp(name, "sin") == 0)  { *out = val_float(sin(arg));  return 1; }
        if (strcmp(name, "cos") == 0)  { *out = val_float(cos(arg));  return 1; }
        if (strcmp(name, "tan") == 0)  { *out = val_float(tan(arg));  return 1; }
        if (strcmp(name, "asin") == 0) { *out = val_float(asin(arg)); return 1; }
        if (strcmp(name, "acos") == 0) { *out = val_float(acos(arg)); return 1; }
        if (strcmp(name, "atan") == 0) { *out = val_float(atan(arg)); return 1; }
        if (strcmp(name, "atan2") == 0){ *out = val_float(atan2(arg, arg2)); return 1; }
        if (strcmp(name, "exp") == 0)  { *out = val_float(exp(arg));  return 1; }
        if (strcmp(name, "log") == 0)  { *out = val_float(argc > 1 ? log(arg)/log(arg2) : log(arg)); return 1; }
        if (strcmp(name, "log2") == 0) { *out = val_float(log2(arg)); return 1; }
        if (strcmp(name, "log10") == 0){ *out = val_float(log10(arg));return 1; }
        if (strcmp(name, "hypot") == 0){ *out = val_float(hypot(arg, arg2)); return 1; }
        if (strcmp(name, "pow") == 0)  { *out = val_float(pow(arg, arg2));  return 1; }
        if (strcmp(name, "floor") == 0){ *out = val_float(floor(arg)); return 1; }
        if (strcmp(name, "ceil") == 0) { *out = val_float(ceil(arg));  return 1; }
        if (strcmp(name, "ldexp") == 0){ *out = val_float(ldexp(arg, (int)arg2)); return 1; }
        *out = val_nil(); return 1;
    }
    if (strcmp(recv.klass->name, "Reline") == 0 ||
        strcmp(recv.klass->name, "Reline::Unicode") == 0) {
        if (strcmp(name, "get_screen_size") == 0) {
            Value arr = val_array_new();
            val_array_push(&arr, val_int(24));
            val_array_push(&arr, val_int(80));
            *out = arr; return 1;
        }
        if (strcmp(name, "calculate_width") == 0) {
            /* Return display width (ASCII assumption: byte length) */
            if (argc < 1 || args[0].kind != VAL_STRING) { *out = val_int(0); return 1; }
            *out = val_int((int64_t)strlen(args[0].sval)); return 1;
        }
        if (strcmp(name, "split_by_width") == 0) {
            /* Return [[line_without_newline, ""], false] */
            if (argc < 1 || args[0].kind != VAL_STRING) {
                Value r = val_array_new();
                Value inner = val_array_new();
                val_array_push(&inner, val_string(ev->arena, ""));
                val_array_push(&r, inner); val_array_push(&r, val_false());
                *out = r; return 1;
            }
            const char *line = args[0].sval;
            int64_t width = (argc >= 2 && args[1].kind == VAL_INT) ? args[1].ival : 80;
            /* Simple split: if line is short enough, return as single chunk */
            size_t llen = strlen(line);
            Value lines_arr = val_array_new();
            if ((int64_t)llen <= width) {
                /* Trim trailing newline for display */
                size_t dlen = llen;
                while (dlen > 0 && (line[dlen-1] == '\n' || line[dlen-1] == '\r')) dlen--;
                char *trimmed = arena_alloc(ev->arena, dlen + 1);
                memcpy(trimmed, line, dlen); trimmed[dlen] = '\0';
                val_array_push(&lines_arr, val_string(ev->arena, trimmed));
                val_array_push(&lines_arr, val_string(ev->arena, ""));
            } else {
                /* Split into chunks of `width` */
                size_t pos = 0;
                while (pos < llen) {
                    size_t take = (llen - pos) < (size_t)width ? (llen - pos) : (size_t)width;
                    char *chunk = arena_alloc(ev->arena, take + 1);
                    memcpy(chunk, line + pos, take); chunk[take] = '\0';
                    val_array_push(&lines_arr, val_string(ev->arena, chunk));
                    pos += take;
                }
                val_array_push(&lines_arr, val_string(ev->arena, ""));
            }
            Value result = val_array_new();
            val_array_push(&result, lines_arr);
            val_array_push(&result, val_false());
            *out = result; return 1;
        }
        if (strcmp(name, "ambiguous_width") == 0) { *out = val_int(1); return 1; }
        if (strcmp(name, "get_last_line") == 0 || strcmp(name, "clear_screen") == 0)
            { *out = val_nil(); return 1; }
        *out = val_nil(); return 1;
    }
    if (strcmp(recv.klass->name, "Thread") == 0) {
        if (strcmp(name, "current") == 0) {
            /* Return a singleton Thread object representing the main thread */
            Value thread_obj = val_nil();
            if (!global_get(&ev->globals, "__main_thread__", &thread_obj) ||
                thread_obj.kind != VAL_OBJECT) {
                thread_obj = val_object(ev->arena, recv);
                val_object_set_ivar(ev->arena, thread_obj, "name", val_string(ev->arena, "main"));
                global_set(ev->arena, &ev->globals, "__main_thread__", thread_obj);
            }
            *out = thread_obj;
            return 1;
        }
        if (strcmp(name, "main") == 0) {
            Value thread_obj = val_nil();
            if (!global_get(&ev->globals, "__main_thread__", &thread_obj) ||
                thread_obj.kind != VAL_OBJECT) {
                thread_obj = val_object(ev->arena, recv);
                val_object_set_ivar(ev->arena, thread_obj, "name", val_string(ev->arena, "main"));
                global_set(ev->arena, &ev->globals, "__main_thread__", thread_obj);
            }
            *out = thread_obj;
            return 1;
        }
        *out = val_nil();
        return 1;
    }
    if (strcmp(recv.klass->name, "Process") == 0 && strcmp(name, "pid") == 0) {
        if (argc != 0) {
            *out = wrong_arg_count(ev, site, argc, 0);
            return 1;
        }
        *out = val_int((int64_t)getpid());
        return 1;
    }
    if (strcmp(name, "deprecate_constant") == 0 || strcmp(name, "private_constant") == 0 ||
        strcmp(name, "public_constant") == 0 || strcmp(name, "using") == 0 ||
        strcmp(name, "refine") == 0) {
        *out = val_nil();
        return 1;
    }
    if (strcmp(name, "autoload") == 0) {
        if (argc != 2) {
            *out = eval_raise_class(ev, site, "ArgumentError",
                                    "wrong number of arguments (given %d, expected 2)", argc);
            return 1;
        }
        const char *path = NULL;
        if (args[1].kind == VAL_STRING || args[1].kind == VAL_SYMBOL)
            path = args[1].sval;
        if (!path) {
            *out = eval_raise_class(ev, site, "TypeError", "autoload path must be a String");
            return 1;
        }
        *out = eval_require(ev, env, path, site);
        return 1;
    }
    if (strcmp(name, "===") == 0) {
        if (argc < 1) { *out = val_false(); return 1; }
        *out = val_bool(val_is_a(args[0], recv));
        return 1;
    }
    if (strcmp(name, "define_method") == 0) {
        if (argc < 1 || argc > 2) {
            *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
            return 1;
        }
        const char *mname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!mname) {
            *out = eval_raise_class(ev, site, "TypeError", "expected Symbol or String");
            return 1;
        }

        Value method_proc = val_nil();
        if (argc == 2)
            method_proc = args[1];
        else if (blk)
            method_proc = *blk;
        if (method_proc.kind != VAL_BLOCK) {
            *out = eval_raise_class(ev, site, "ArgumentError", "define_method requires a block or Proc");
            return 1;
        }

        Node *def = node_new(ev->arena, NODE_DEF, site ? site->span : (Span){0, 0, 0});
        def->def.name = mname;
        def->def.params = method_proc.block.block_node ? method_proc.block.block_node->block.params : NULL;
        def->def.body = method_proc.block.block_node ? method_proc.block.block_node->block.body : NULL;
        Value method = val_method(def, method_proc.block.closure, current_method_visibility(env));

        Value singleton_target = val_nil();
        if (env_get(env, "__singleton_target__", &singleton_target) &&
            singleton_target.kind == VAL_CLASS && singleton_target.klass == recv.klass) {
            size_t nlen = strlen(mname);
            char *key = arena_alloc(ev->arena, nlen + 6);
            memcpy(key, "self.", 5);
            memcpy(key + 5, mname, nlen + 1);
            env_define(ev->arena, recv.klass->class_env, key, method);
        } else {
            env_define(ev->arena, recv.klass->class_env, mname, method);
        }
        *out = val_symbol(mname);
        return 1;
    }
    if (recv.klass->is_module && recv.klass->class_env) {
        Value const_val;
        if (env_get(recv.klass->class_env, name, &const_val) && const_val.kind != VAL_METHOD) {
            /* Only return the constant if there's no class method with this name */
            size_t nlen = strlen(name);
            char *class_key = arena_alloc(ev->arena, nlen + 6);
            memcpy(class_key, "self.", 5);
            memcpy(class_key + 5, name, nlen + 1);
            Value cm;
            if (!env_get(recv.klass->class_env, class_key, &cm) || cm.kind != VAL_METHOD) {
                *out = const_val;
                return 1;
            }
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
    if (strcmp(name, "open") == 0 && strcmp(recv.klass->name, "IO") == 0) {
        if (argc < 1 || argc > 3) {
            *out = argc < 1
                 ? wrong_arg_count(ev, site, argc, 1)
                 : eval_raise_class(ev, site, "ArgumentError",
                                    "wrong number of arguments (given %d, expected 1..3)", argc);
            return 1;
        }
        if (args[0].kind != VAL_INT) {
            *out = implicit_integer_conversion_error(ev, args[0], site);
            return 1;
        }

        Value mode = val_nil();
        Value options = val_nil();
        const char *mode_cstr = NULL;

        if (argc >= 2 && args[1].kind != VAL_NIL) {
            if (args[1].kind == VAL_STRING) mode = args[1];
            else if (args[1].kind == VAL_HASH) options = args[1];
            else {
                *out = implicit_string_conversion_error(ev, args[1], site);
                return 1;
            }
        }
        if (argc >= 3 && args[2].kind != VAL_NIL) {
            if (args[2].kind != VAL_HASH) {
                *out = eval_raise_class(ev, site, "TypeError", "no implicit conversion into Hash");
                return 1;
            }
            options = args[2];
        }
        if (mode.kind == VAL_NIL) {
            mode_cstr = infer_fd_mode(ev, args[0].ival, site);
            if (!mode_cstr) {
                *out = val_exception();
                return 1;
            }
            mode = val_string(ev->arena, mode_cstr);
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

        if (options.kind == VAL_HASH) {
            Value enc = val_nil();
            if (val_hash_get(options.hash, val_symbol("external_encoding"), &enc))
                val_object_set_ivar(ev->arena, io_obj, "external_encoding", enc);
            if (val_hash_get(options.hash, val_symbol("internal_encoding"), &enc))
                val_object_set_ivar(ev->arena, io_obj, "internal_encoding", enc);
        }

        if (blk) {
            Value result = call_block(ev, env, *blk, &io_obj, 1, site);
            Value closed_result = file_close_stream(ev, io_obj, site);
            val_object_set_ivar(ev->arena, io_obj, "closed", val_true());
            if (val_is_signal(closed_result)) {
                *out = closed_result;
                return 1;
            }
            if (result.kind == VAL_BREAK)
                result = *result.jump.wrapped;
            *out = result;
        } else {
            *out = io_obj;
        }
        return 1;
    }
    if (strcmp(name, "console_size") == 0 && strcmp(recv.klass->name, "IO") == 0) {
        if (argc != 0) {
            *out = wrong_arg_count(ev, site, argc, 0);
            return 1;
        }
        Value stdout_value = val_nil();
        if (env_get(ev->top_env, "STDOUT", &stdout_value) &&
            stdout_value.kind == VAL_OBJECT &&
            stdout_value.obj->klass.kind == VAL_CLASS &&
            strcmp(stdout_value.obj->klass.klass->name, "IO") == 0) {
            NativeFile *stdout_nf = NULL;
            if (ensure_open_native_file(ev, stdout_value, site, &stdout_nf)) {
                *out = stream_winsize(ev, stdout_nf->fp);
                return 1;
            }
        }
        *out = default_console_winsize(ev->arena);
        return 1;
    }
    if (strcmp(recv.klass->name, "Dir") == 0) {
        if (strcmp(name, "pwd") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd))) {
                *out = eval_raise_class(ev, site, errno_class_name(errno), "%s", strerror(errno));
                return 1;
            }
            *out = val_string(ev->arena, cwd);
            return 1;
        }
        if (strcmp(name, "mkdir") == 0) {
            if (argc < 1 || argc > 2) {
                *out = argc < 1
                     ? wrong_arg_count(ev, site, argc, 1)
                     : eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 1..2)", argc);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            int64_t mode = 0777;
            if (argc == 2 && args[1].kind != VAL_NIL) {
                if (args[1].kind != VAL_INT) {
                    *out = implicit_integer_conversion_error(ev, args[1], site);
                    return 1;
                }
                mode = args[1].ival;
            }
            if (mkdir(args[0].sval, (mode_t)mode) != 0) {
                *out = eval_raise_class(ev, site, errno_class_name(errno), "%s - %s",
                                        strerror(errno), args[0].sval);
                return 1;
            }
            *out = val_int(0);
            return 1;
        }
        if (strcmp(name, "chdir") == 0) {
            if (argc > 1) {
                *out = eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 0..1)", argc);
                return 1;
            }
            const char *path = NULL;
            char home_buf[PATH_MAX];
            if (argc == 0 || args[0].kind == VAL_NIL) {
                const char *home = getenv("HOME");
                if (!home || home[0] == '\0') {
                    *out = eval_raise_class(ev, site, "ArgumentError", "HOME not set");
                    return 1;
                }
                snprintf(home_buf, sizeof(home_buf), "%s", home);
                path = home_buf;
            } else if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            } else {
                path = args[0].sval;
            }

            char old_cwd[PATH_MAX];
            if (!getcwd(old_cwd, sizeof(old_cwd))) {
                *out = eval_raise_class(ev, site, errno_class_name(errno), "%s", strerror(errno));
                return 1;
            }
            if (chdir(path) != 0) {
                *out = eval_raise_class(ev, site, errno_class_name(errno), "%s - %s",
                                        strerror(errno), path);
                return 1;
            }

            if (blk) {
                Value result = call_block(ev, env, *blk, NULL, 0, site);
                int restore_err = 0;
                int restore_errno = 0;
                if (chdir(old_cwd) != 0) {
                    restore_err = 1;
                    restore_errno = errno;
                }
                if (restore_err) {
                    *out = eval_raise_class(ev, site, errno_class_name(restore_errno), "%s - %s",
                                            strerror(restore_errno), old_cwd);
                    return 1;
                }
                if (result.kind == VAL_BREAK)
                    result = *result.jump.wrapped;
                *out = result;
                return 1;
            }

            *out = val_int(0);
            return 1;
        }
    }
    if (strcmp(recv.klass->name, "File") == 0) {
        if (strcmp(name, "basename") == 0) {
            if (argc < 1 || argc > 2) {
                *out = eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 1..2)", argc);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            const char *ext = NULL;
            if (argc >= 2 && args[1].kind != VAL_NIL) {
                if (args[1].kind != VAL_STRING) {
                    *out = implicit_string_conversion_error(ev, args[1], site);
                    return 1;
                }
                ext = args[1].sval;
            }
            const char *path = args[0].sval ? args[0].sval : "";
            if (path[0] == '\0') { *out = val_string(ev->arena, ""); return 1; }
            /* strip trailing slashes, but keep root */
            size_t plen = strlen(path);
            size_t end = plen;
            while (end > 1 && path[end - 1] == '/') end--;
            /* find last slash */
            size_t start = 0;
            for (size_t i = 0; i < end; i++)
                if (path[i] == '/') start = i + 1;
            size_t blen = end - start;
            /* root path ("/" or pure slashes) is its own basename */
            if (blen == 0) { *out = val_string(ev->arena, "/"); return 1; }
            /* apply ext suffix stripping */
            if (ext && ext[0] != '\0') {
                if (strcmp(ext, ".*") == 0) {
                    for (size_t i = 1; i < blen; i++)
                        if (path[start + i] == '.') blen = i;
                } else {
                    size_t elen = strlen(ext);
                    if (blen > elen &&
                        memcmp(path + start + blen - elen, ext, elen) == 0)
                        blen -= elen;
                }
            }
            char *buf = arena_alloc(ev->arena, blen + 1);
            memcpy(buf, path + start, blen);
            buf[blen] = '\0';
            *out = val_string(ev->arena, buf);
            return 1;
        }
        if (strcmp(name, "dirname") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            const char *path = args[0].sval ? args[0].sval : "";
            if (path[0] == '\0') { *out = val_string(ev->arena, "."); return 1; }
            size_t plen = strlen(path);
            size_t end = plen;
            while (end > 1 && path[end - 1] == '/') end--;
            int last_slash = -1;
            for (size_t i = 0; i < end; i++)
                if (path[i] == '/') last_slash = (int)i;
            if (last_slash < 0) { *out = val_string(ev->arena, "."); return 1; }
            if (last_slash == 0) { *out = val_string(ev->arena, "/"); return 1; }
            size_t dlen = (size_t)last_slash;
            while (dlen > 1 && path[dlen - 1] == '/') dlen--;
            char *buf = arena_alloc(ev->arena, dlen + 1);
            memcpy(buf, path, dlen);
            buf[dlen] = '\0';
            *out = val_string(ev->arena, buf);
            return 1;
        }
        if (strcmp(name, "extname") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            const char *path = args[0].sval ? args[0].sval : "";
            /* get basename bounds */
            size_t plen = strlen(path);
            size_t end = plen;
            while (end > 1 && path[end - 1] == '/') end--;
            size_t start = 0;
            for (size_t i = 0; i < end; i++)
                if (path[i] == '/') start = i + 1;
            size_t blen = end - start;
            /* last dot in basename, skipping a leading dot */
            int dot = -1;
            for (size_t i = 1; i < blen; i++)
                if (path[start + i] == '.') dot = (int)i;
            if (dot < 0) { *out = val_string(ev->arena, ""); return 1; }
            size_t elen = blen - (size_t)dot;
            char *buf = arena_alloc(ev->arena, elen + 1);
            memcpy(buf, path + start + dot, elen);
            buf[elen] = '\0';
            *out = val_string(ev->arena, buf);
            return 1;
        }
        if (strcmp(name, "join") == 0) {
            if (argc == 0) { *out = val_string(ev->arena, ""); return 1; }
            /* compute upper bound */
            size_t total = 2;
            for (int i = 0; i < argc; i++) {
                const char *s = val_to_s(ev->arena, args[i]);
                total += strlen(s) + 1;
            }
            char *buf = arena_alloc(ev->arena, total);
            size_t pos = 0;
            for (int i = 0; i < argc; i++) {
                const char *s = val_to_s(ev->arena, args[i]);
                size_t slen = strlen(s);
                if (i == 0) {
                    memcpy(buf, s, slen);
                    pos = slen;
                } else {
                    /* strip trailing slashes from accumulated result */
                    while (pos > 0 && buf[pos - 1] == '/') pos--;
                    /* strip leading slashes from this segment */
                    size_t skip = 0;
                    while (skip < slen && s[skip] == '/') skip++;
                    buf[pos++] = '/';
                    memcpy(buf + pos, s + skip, slen - skip);
                    pos += slen - skip;
                }
            }
            buf[pos] = '\0';
            *out = val_string(ev->arena, buf);
            return 1;
        }
        if (strcmp(name, "split") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            const char *sp = args[0].sval ? args[0].sval : "";
            /* dirname */
            Value dir_val;
            if (sp[0] == '\0') {
                dir_val = val_string(ev->arena, ".");
            } else {
                size_t splen = strlen(sp);
                size_t send = splen;
                while (send > 1 && sp[send - 1] == '/') send--;
                int slast = -1;
                for (size_t i = 0; i < send; i++)
                    if (sp[i] == '/') slast = (int)i;
                if (slast < 0) {
                    dir_val = val_string(ev->arena, ".");
                } else if (slast == 0) {
                    dir_val = val_string(ev->arena, "/");
                } else {
                    size_t dlen = (size_t)slast;
                    while (dlen > 1 && sp[dlen - 1] == '/') dlen--;
                    char *db = arena_alloc(ev->arena, dlen + 1);
                    memcpy(db, sp, dlen);
                    db[dlen] = '\0';
                    dir_val = val_string(ev->arena, db);
                }
            }
            /* basename (no ext stripping) */
            Value base_val;
            if (sp[0] == '\0') {
                base_val = val_string(ev->arena, "");
            } else {
                size_t splen = strlen(sp);
                size_t send = splen;
                while (send > 1 && sp[send - 1] == '/') send--;
                size_t sstart = 0;
                for (size_t i = 0; i < send; i++)
                    if (sp[i] == '/') sstart = i + 1;
                size_t blen = send - sstart;
                char *bb = arena_alloc(ev->arena, blen + 1);
                memcpy(bb, sp + sstart, blen);
                bb[blen] = '\0';
                base_val = val_string(ev->arena, bb);
            }
            Value pair = val_array_new();
            val_array_push(&pair, dir_val);
            val_array_push(&pair, base_val);
            *out = pair;
            return 1;
        }
        if (strcmp(name, "expand_path") == 0) {
            if (argc < 1 || argc > 2) {
                *out = eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 1..2)", argc);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            const char *base = NULL;
            if (argc >= 2 && args[1].kind != VAL_NIL) {
                if (args[1].kind != VAL_STRING) {
                    *out = implicit_string_conversion_error(ev, args[1], site);
                    return 1;
                }
                base = args[1].sval;
            }
            char abs_buf[PATH_MAX * 2];
            if (!build_absolute_path(ev, args[0].sval ? args[0].sval : "", base, 1, abs_buf, sizeof(abs_buf), site)) {
                *out = val_exception();
                return 1;
            }
            *out = lexical_normalize_path(ev, abs_buf);
            return 1;
        }
        if (strcmp(name, "absolute_path") == 0) {
            if (argc < 1 || argc > 2) {
                *out = eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 1..2)", argc);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            const char *base = NULL;
            if (argc >= 2 && args[1].kind != VAL_NIL) {
                if (args[1].kind != VAL_STRING) {
                    *out = implicit_string_conversion_error(ev, args[1], site);
                    return 1;
                }
                base = args[1].sval;
            }
            char abs_buf[PATH_MAX * 2];
            if (!build_absolute_path(ev, args[0].sval ? args[0].sval : "", base, 0, abs_buf, sizeof(abs_buf), site)) {
                *out = val_exception();
                return 1;
            }
            *out = lexical_normalize_path(ev, abs_buf);
            return 1;
        }
        if (strcmp(name, "realpath") == 0) {
            if (argc < 1 || argc > 2) {
                *out = eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 1..2)", argc);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            const char *base = NULL;
            if (argc >= 2 && args[1].kind != VAL_NIL) {
                if (args[1].kind != VAL_STRING) {
                    *out = implicit_string_conversion_error(ev, args[1], site);
                    return 1;
                }
                base = args[1].sval;
            }
            char abs_buf[PATH_MAX * 2];
            if (!build_absolute_path(ev, args[0].sval ? args[0].sval : "", base, 0, abs_buf, sizeof(abs_buf), site)) {
                *out = val_exception();
                return 1;
            }
            char resolved[PATH_MAX];
            if (!realpath(abs_buf, resolved)) {
                *out = eval_raise_class(ev, site, errno_class_name(errno), "%s - %s",
                                        strerror(errno), abs_buf);
                return 1;
            }
            *out = val_string(ev->arena, resolved);
            return 1;
        }
        if (strcmp(name, "read") == 0) {
            if (argc < 1 || argc > 3) {
                *out = argc < 1
                     ? wrong_arg_count(ev, site, argc, 1)
                     : eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 0..2)", argc - 1);
            } else if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
            } else {
                int has_length = 0;
                int64_t length = 0;
                int has_offset = 0;
                int64_t offset = 0;
                if (argc >= 2 && args[1].kind != VAL_NIL) {
                    if (args[1].kind != VAL_INT) {
                        *out = implicit_integer_conversion_error(ev, args[1], site);
                        return 1;
                    }
                    has_length = 1;
                    length = args[1].ival;
                }
                if (argc >= 3) {
                    if (args[2].kind != VAL_INT) {
                        *out = implicit_integer_conversion_error(ev, args[2], site);
                        return 1;
                    }
                    has_offset = 1;
                    offset = args[2].ival;
                }
                *out = eval_file_read_slice(ev, args[0].sval, has_length, length, has_offset, offset, site);
            }
            return 1;
        }
        if (strcmp(name, "write") == 0) {
            if (argc < 2 || argc > 3) {
                *out = argc < 2
                     ? eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 2..3)", argc)
                     : eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 2..3)", argc);
            } else if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
            } else {
                int has_offset = 0;
                int64_t offset = 0;
                if (argc >= 3 && args[2].kind != VAL_NIL) {
                    if (args[2].kind != VAL_INT) {
                        *out = implicit_integer_conversion_error(ev, args[2], site);
                        return 1;
                    }
                    has_offset = 1;
                    offset = args[2].ival;
                }
                *out = eval_file_write_at(ev, args[0].sval, val_to_s(ev->arena, args[1]),
                                          has_offset, offset, site);
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
        if (strcmp(name, "directory?") == 0 || strcmp(name, "file?") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            struct stat st;
            if (stat(args[0].sval, &st) != 0) {
                *out = val_false();
                return 1;
            }
            *out = val_bool(strcmp(name, "directory?") == 0 ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode));
            return 1;
        }
        if (strcmp(name, "readable?") == 0 || strcmp(name, "writable?") == 0 || strcmp(name, "executable?") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            int mode = R_OK;
            if (strcmp(name, "writable?") == 0) mode = W_OK;
            else if (strcmp(name, "executable?") == 0) mode = X_OK;
            *out = val_bool(access(args[0].sval, mode) == 0);
            return 1;
        }
        if (strcmp(name, "mtime") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
                return 1;
            }
            struct stat st;
            if (stat(args[0].sval, &st) != 0) {
                *out = eval_raise_class(ev, site, errno_class_name(errno), "%s - %s",
                                        strerror(errno), args[0].sval);
                return 1;
            }
            *out = build_time_value(ev, (int64_t)st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
            return 1;
        }
        if (strcmp(name, "open") == 0) {
            if (argc < 1 || argc > 3) {
                *out = argc < 1
                     ? wrong_arg_count(ev, site, argc, 1)
                     : eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 1..3)", argc);
            } else if (args[0].kind != VAL_STRING) {
                *out = implicit_string_conversion_error(ev, args[0], site);
            } else {
                Value mode = (argc >= 2 && args[1].kind != VAL_NIL) ? args[1] : val_string(ev->arena, "r");
                if (mode.kind != VAL_STRING) {
                    *out = implicit_string_conversion_error(ev, mode, site);
                    return 1;
                }
                if (argc >= 3 && args[2].kind != VAL_NIL && args[2].kind != VAL_INT) {
                    *out = implicit_integer_conversion_error(ev, args[2], site);
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
    if (strcmp(name, "union") == 0 && strcmp(recv.klass->name, "Regexp") == 0) {
        Value *items = args;
        int item_count = argc;
        if (argc == 1 && args[0].kind == VAL_ARRAY) {
            items = args[0].array->elems;
            item_count = (int)args[0].array->len;
        }
        size_t total = 1;
        for (int i = 0; i < item_count; i++) {
            total += regexp_union_piece_length(items[i]);
            if (i + 1 < item_count) total++;
        }
        char *pattern = arena_alloc(ev->arena, total);
        char *cursor = pattern;
        for (int i = 0; i < item_count; i++) {
            if (i > 0) *cursor++ = '|';
            cursor = append_regexp_union_piece(cursor, items[i]);
        }
        *cursor = '\0';

        Regex *compiled = NULL;
        RegexError err = {0};
        if (regex_compile(ev->arena, pattern, 0, &compiled, &err) != REGEX_OK) {
            *out = eval_raise_class(ev, site, "RegexpError", "invalid regexp: %s", err.message);
            return 1;
        }
        Value obj = val_object(ev->arena, recv);
        obj.obj->native = compiled;
        val_object_set_ivar(ev->arena, obj, "source", val_string(ev->arena, pattern));
        *out = obj;
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
        Value struct_members = val_nil();
        if (class_is_a_named_class(ev, recv.klass, "Struct") &&
            recv.klass->class_env &&
            env_get(recv.klass->class_env, "__struct_members__", &struct_members) &&
            struct_members.kind == VAL_ARRAY) {
            if (argc > (int)struct_members.array->len) {
                *out = eval_raise_class(ev, site, "ArgumentError", "struct size differs");
                return 1;
            }
            Value obj = val_object(ev->arena, recv);
            for (size_t i = 0; i < struct_members.array->len; i++) {
                Value member = struct_members.array->elems[i];
                if (member.kind != VAL_STRING) continue;
                Value val = i < (size_t)argc ? args[i] : val_nil();
                val_object_set_ivar(ev->arena, obj, member.sval, val);
            }
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
    if (strcmp(name, "instance") == 0 && argc == 0 &&
        class_includes_module_name(recv.klass, "Singleton")) {
        Value existing = val_nil();
        if (env_get(recv.klass->class_env, "__singleton_instance__", &existing))
            { *out = existing; return 1; }
        Value obj = val_object(ev->arena, recv);
        env_define(ev->arena, recv.klass->class_env, "__singleton_instance__", obj);
        *out = obj;
        return 1;
    }
    /* Class comparison operators: A < B (A is strict subclass of B) */
    if ((strcmp(name, "<") == 0 || strcmp(name, "<=") == 0 ||
         strcmp(name, ">") == 0 || strcmp(name, ">=") == 0 ||
         strcmp(name, "<=>") == 0) && recv.kind == VAL_CLASS) {
        if (argc < 1 || args[0].kind != VAL_CLASS) { *out = val_nil(); return 1; }
        RubyClass *a = recv.klass, *b = args[0].klass;
        if (a == b) {
            /* equal */
            if (strcmp(name, "<") == 0 || strcmp(name, ">") == 0) *out = val_false();
            else if (strcmp(name, "<=>") == 0) *out = val_int(0);
            else *out = val_true();
            return 1;
        }
        /* Check if a is a subclass of b */
        int a_sub_b = 0;
        for (RubyClass *k = a->superclass.kind == VAL_CLASS ? a->superclass.klass : NULL; k;
             k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
            if (k == b) { a_sub_b = 1; break; }
        }
        int b_sub_a = 0;
        for (RubyClass *k = b->superclass.kind == VAL_CLASS ? b->superclass.klass : NULL; k;
             k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
            if (k == a) { b_sub_a = 1; break; }
        }
        if (strcmp(name, "<") == 0)  { *out = val_bool(a_sub_b); return 1; }
        if (strcmp(name, "<=") == 0) { *out = val_bool(a_sub_b || a == b); return 1; }
        if (strcmp(name, ">") == 0)  { *out = val_bool(b_sub_a); return 1; }
        if (strcmp(name, ">=") == 0) { *out = val_bool(b_sub_a || a == b); return 1; }
        if (strcmp(name, "<=>") == 0) {
            *out = a_sub_b ? val_int(-1) : b_sub_a ? val_int(1) : val_nil();
            return 1;
        }
    }

    /* Struct subclass class methods: members */
    if (strcmp(name, "members") == 0 &&
        class_is_a_named_class(ev, recv.klass, "Struct") &&
        recv.klass->class_env) {
        Value sm = val_nil();
        if (env_get(recv.klass->class_env, "__struct_members__", &sm) && sm.kind == VAL_ARRAY) {
            Value syms = val_array_new();
            for (size_t i = 0; i < sm.array->len; i++) {
                Value s = sm.array->elems[i];
                val_array_push(&syms, val_symbol(s.kind == VAL_STRING ? s.sval : "?"));
            }
            *out = syms;
        } else { *out = val_array_new(); }
        return 1;
    }

    if (strcmp(name, "class_variables") == 0) {
        Value arr = val_array_new();
        if (recv.klass->class_env) {
            for (EnvEntry *e = recv.klass->class_env->vars; e; e = e->next) {
                if (e->name && e->name[0] == '@' && e->name[1] == '@')
                    val_array_push(&arr, val_symbol(e->name));
            }
        }
        *out = arr; return 1;
    }
    if (strcmp(name, "const_defined?") == 0) {
        if (argc < 1) { *out = val_false(); return 1; }
        const char *cname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!cname) { *out = val_false(); return 1; }
        Value v;
        *out = val_bool(recv.klass->class_env && env_get(recv.klass->class_env, cname, &v));
        return 1;
    }
    if (strcmp(name, "const_get") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        const char *cname = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
        if (!cname) { *out = val_nil(); return 1; }
        Value v = val_nil();
        if (recv.klass->class_env) env_get(recv.klass->class_env, cname, &v);
        *out = v; return 1;
    }
    /* ---- Class reflection ---- */

    if (strcmp(name, "superclass") == 0) {
        *out = recv.klass->superclass.kind == VAL_CLASS ? recv.klass->superclass : val_nil();
        return 1;
    }

    if (strcmp(name, "name") == 0 || strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0) {
        *out = val_string(ev->arena, recv.klass->name);
        return 1;
    }

    if (strcmp(name, "constants") == 0) {
        Value arr = val_array_new();
        if (strcmp(recv.klass->name, "Module") == 0) {
            for (EnvEntry *entry = ev->top_env ? ev->top_env->vars : NULL; entry; entry = entry->next) {
                if (!entry->name || !entry->name[0]) continue;
                if (!(entry->name[0] >= 'A' && entry->name[0] <= 'Z')) continue;
                if (strstr(entry->name, "::")) continue;
                if (entry->val.kind == VAL_METHOD) continue;
                val_array_push(&arr, val_symbol(entry->name));
            }
            *out = arr;
            return 1;
        }
        for (RubyClass *k = recv.klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
            for (EnvEntry *entry = k->class_env ? k->class_env->vars : NULL; entry; entry = entry->next) {
                if (!entry->name || !entry->name[0]) continue;
                if (!(entry->name[0] >= 'A' && entry->name[0] <= 'Z')) continue;
                if (strstr(entry->name, "::")) continue;
                if (entry->val.kind == VAL_METHOD) continue;
                int seen = 0;
                for (size_t i = 0; i < arr.array->len; i++) {
                    Value existing = arr.array->elems[i];
                    if (existing.kind == VAL_SYMBOL && strcmp(existing.sval, entry->name) == 0) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen)
                    val_array_push(&arr, val_symbol(entry->name));
            }
            if (k->is_module) break;
        }
        *out = arr;
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
    if (strcmp(name, "include?") == 0) {
        if (argc < 1 || args[0].kind != VAL_CLASS)
            { *out = eval_raise_class(ev, site, "TypeError", "Class#include? requires a module"); return 1; }
        const char *mname = args[0].klass->name;
        RubyClass *k = recv.klass;
        while (k) {
            for (RubyModuleInclusion *m = k->included_modules; m; m = m->next)
                if (strcmp(m->mod->name, mname) == 0) { *out = val_true(); return 1; }
            for (RubyModuleInclusion *m = k->prepended_modules; m; m = m->next)
                if (strcmp(m->mod->name, mname) == 0) { *out = val_true(); return 1; }
            k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL;
        }
        *out = val_false(); return 1;
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
        /* Add primitive methods for known builtin classes */
        if ((vis_mask & 1) && strcmp(name, "private_instance_methods") != 0) {
            const char *prim_list = primitive_methods_for_class(recv.klass->name);
            if (prim_list) {
                const char *p = prim_list;
                while (*p) {
                    const char *end = strchr(p, ','); size_t len = end ? (size_t)(end-p) : strlen(p);
                    if (len < 128) {
                        char *mname = arena_alloc(ev->arena, len + 1);
                        memcpy(mname, p, len); mname[len] = '\0';
                        int dup = 0;
                        for (size_t ai = 0; ai < arr.array->len; ai++)
                            if (arr.array->elems[ai].kind == VAL_SYMBOL && strcmp(arr.array->elems[ai].sval, mname) == 0) { dup = 1; break; }
                        if (!dup) val_array_push(&arr, val_symbol(mname));
                    }
                    if (!end) break; p = end + 1;
                }
            }
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
        if (!singleton_class_method_lookup(ev, env, recv, mname, &method) &&
            !ruby_class_find_instance_method(recv.klass, mname, &method, &owner)) {
            *out = val_false();
            return 1;
        }
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
        /* Check if it's a primitive method for this class */
        const char *prim_list = primitive_methods_for_class(recv.klass->name);
        int is_primitive = primitive_unbound_method_name(mname);
        if (!is_primitive && prim_list) {
            /* scan comma-separated list */
            const char *p = prim_list;
            while (*p) {
                const char *end = strchr(p, ','); size_t len = end ? (size_t)(end-p) : strlen(p);
                if (strlen(mname) == len && strncmp(mname, p, len) == 0) { is_primitive = 1; break; }
                if (!end) break; p = end + 1;
            }
        }
        if (!ruby_class_find_instance_method(recv.klass, mname, &method_val, &owner) && !is_primitive) {
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

        if (strcmp(kname, "Binding") == 0) {
            NativeBinding *binding = (NativeBinding *)recv.obj->native;
            if (strcmp(name, "local_variable_set") == 0) {
                if (argc != 2) {
                    *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                    return 1;
                }
                const char *var = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
                if (!var) {
                    *out = eval_raise_class(ev, site, "TypeError",
                                            "local_variable_set requires a Symbol or String");
                    return 1;
                }
                if (binding && binding->env) {
                    if (!env_update(binding->env, var, args[1]))
                        env_set(ev->arena, binding->env, var, args[1]);
                }
                *out = args[1];
                return 1;
            }
            if (strcmp(name, "local_variable_get") == 0) {
                if (argc != 1) {
                    *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                    return 1;
                }
                const char *var = (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) ? args[0].sval : NULL;
                Value value;
                if (!var) {
                    *out = eval_raise_class(ev, site, "TypeError",
                                            "local_variable_get requires a Symbol or String");
                    return 1;
                }
                if (!binding || !binding->env || !env_get(binding->env, var, &value)) {
                    *out = eval_raise_class(ev, site, "NameError",
                                            "local variable '%s' is not defined for Binding", var);
                    return 1;
                }
                *out = value;
                return 1;
            }
            if (strcmp(name, "local_variables") == 0) {
                Value arr = val_array_new();
                Env *scan = binding ? binding->env : NULL;
                while (scan) {
                    for (EnvEntry *e = scan->vars; e; e = e->next) {
                        const char *n2 = e->name;
                        size_t nlen = strlen(n2);
                        if (nlen == 0 || n2[0] == '_' && n2[1] == '_') continue;
                        if (n2[0] != '@' && n2[0] != '$' && n2[0] != ':' &&
                            strcmp(n2, "self") != 0 && strcmp(n2, "true") != 0 &&
                            strcmp(n2, "false") != 0 && strcmp(n2, "nil") != 0 &&
                            n2[0] >= 'a' && n2[0] <= 'z') {
                            val_array_push(&arr, val_symbol(n2));
                        }
                    }
                    if (scan->is_def) break;
                    scan = scan->parent;
                }
                *out = arr;
                return 1;
            }
            if (strcmp(name, "source_location") == 0) {
                if (argc != 0) {
                    *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                    return 1;
                }
                Value loc = val_array_new();
                val_array_push(&loc, binding && binding->file ? val_string(ev->arena, binding->file) : val_nil());
                val_array_push(&loc, val_int(binding ? binding->line : 1));
                *out = loc;
                return 1;
            }
        }

        if (strcmp(kname, "Time") == 0) {
            NativeTime *nt = (NativeTime *)recv.obj->native;
            if (!nt) {
                *out = eval_raise_class(ev, site, "RuntimeError", "invalid Time object");
                return 1;
            }
            if (strcmp(name, "to_i") == 0) {
                *out = val_int(nt->sec);
                return 1;
            }
            if (strcmp(name, "==") == 0 || strcmp(name, "!=") == 0 || strcmp(name, "<=>") == 0) {
                if (argc != 1) {
                    *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                    return 1;
                }
                if (!(args[0].kind == VAL_OBJECT && args[0].obj->klass.kind == VAL_CLASS &&
                      strcmp(args[0].obj->klass.klass->name, "Time") == 0 && args[0].obj->native)) {
                    *out = strcmp(name, "!=") == 0 ? val_true()
                         : strcmp(name, "==") == 0 ? val_false()
                         : val_nil();
                    return 1;
                }
                NativeTime *other = (NativeTime *)args[0].obj->native;
                int cmp = 0;
                if (nt->sec < other->sec) cmp = -1;
                else if (nt->sec > other->sec) cmp = 1;
                else if (nt->nsec < other->nsec) cmp = -1;
                else if (nt->nsec > other->nsec) cmp = 1;
                if (strcmp(name, "<=>") == 0) {
                    *out = val_int(cmp);
                    return 1;
                }
                *out = strcmp(name, "==") == 0 ? val_bool(cmp == 0) : val_bool(cmp != 0);
                return 1;
            }
            if (strcmp(name, "inspect") == 0 || strcmp(name, "to_s") == 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Time(%lld.%09ld)",
                         (long long)nt->sec, nt->nsec);
                *out = val_string(ev->arena, buf);
                return 1;
            }
        }

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
            if (strcmp(name, "call") == 0 || strcmp(name, "[]") == 0 ||
                strcmp(name, "bind_call") == 0) {
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
            if (strcmp(name, "bind_call") == 0) {
                if (argc < 1) {
                    *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
                    return 1;
                }
                Value method_name_v;
                if (!val_object_get_ivar(recv, "__method_name__", &method_name_v)) {
                    *out = val_nil();
                    return 1;
                }
                *out = dispatch_method(ev, env, args[0], method_name_v.sval, args + 1, argc - 1,
                                       blk, site, 0, -1);
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
                    return 1;
                }
                if (args[0].kind == VAL_INT) {
                    int64_t idx = args[0].ival;
                    if (idx < 0) idx += ncaps + 1;
                    if (idx < 0 || idx > ncaps) *out = val_nil();
                    else *out = MD_GROUP_STR(idx);
                    return 1;
                }
                /* Named capture: symbol or string key */
                if (args[0].kind == VAL_SYMBOL || args[0].kind == VAL_STRING) {
                    const char *key = args[0].sval;
                    Value names_v;
                    if (val_object_get_ivar(recv, "__cap_names__", &names_v) &&
                        names_v.kind == VAL_ARRAY) {
                        for (size_t ni = 0; ni < names_v.array->len; ni++) {
                            Value nm = names_v.array->elems[ni];
                            if (nm.kind == VAL_STRING && strcmp(nm.sval, key) == 0) {
                                *out = MD_GROUP_STR((int64_t)(ni + 1));
                                return 1;
                            }
                        }
                    }
                    *out = val_nil();
                    return 1;
                }
                *out = eval_raise_class(ev, site, "TypeError", "MatchData#[] index must be an Integer or Symbol");
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
            if (strcmp(name, "to_a") == 0 || strcmp(name, "captures") == 0) {
                Value arr = val_array_new();
                int start = strcmp(name, "captures") == 0 ? 1 : 0;
                int total = (int)ncaps + 1;
                for (int i = start; i < total; i++) val_array_push(&arr, MD_GROUP_STR(i));
                *out = arr; return 1;
            }
            if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0)
                { *out = val_int(ncaps + 1); return 1; }
            if (strcmp(name, "string") == 0) { *out = string; return 1; }
            if (strcmp(name, "regexp") == 0) { *out = regexp; return 1; }
            if (strcmp(name, "named_captures") == 0)
                { *out = val_hash_new(ev->arena); return 1; } /* stub: no named capture support yet */
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
        if (strcmp(name, "to_i") == 0 || strcmp(name, "to_int") == 0) {
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
            *out = val_bool(isatty(fileno(stream)));
            return 1;
        }
        if (strcmp(name, "winsize") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = stream_winsize(ev, stream);
            return 1;
        }
        if (strcmp(name, "external_encoding") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            if (!val_object_get_ivar(recv, "external_encoding", out))
                *out = val_nil();
            return 1;
        }
        if (strcmp(name, "internal_encoding") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            if (!val_object_get_ivar(recv, "internal_encoding", out))
                *out = val_nil();
            return 1;
        }
        if (strcmp(name, "wait_readable") == 0) {
            if (argc > 1) {
                *out = eval_raise_class(ev, site, "ArgumentError",
                                        "wrong number of arguments (given %d, expected 0..1)", argc);
                return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "ungetc") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            int ch = EOF;
            if (args[0].kind == VAL_INT) ch = (unsigned char)args[0].ival;
            else if (args[0].kind == VAL_STRING && args[0].sval && args[0].sval[0] != '\0')
                ch = (unsigned char)args[0].sval[0];
            else {
                *out = eval_raise_class(ev, site, "TypeError", "IO#ungetc requires an Integer or String");
                return 1;
            }
            if (ungetc(ch, stream) == EOF) {
                *out = eval_raise_class(ev, site, "IOError", "ungetc failed");
                return 1;
            }
            *out = val_nil();
            return 1;
        }
        if (strcmp(name, "raw") == 0 || strcmp(name, "cooked") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            if (!blk) {
                *out = recv;
                return 1;
            }
            Value result = call_block(ev, env, *blk, NULL, 0, site);
            if (ev->errored || val_is_signal(result)) {
                *out = result;
                return 1;
            }
            *out = result;
            return 1;
        }
        if (strcmp(name, "raw!") == 0 || strcmp(name, "cooked!") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "gets") == 0) {
            *out = file_gets_stream(ev, recv, mode.sval, "IO#gets", args, argc, site);
            return 1;
        }
        if (strcmp(name, "readline") == 0) {
            *out = file_readline_stream(ev, recv, mode.sval, "IO#readline", args, argc, site);
            return 1;
        }
        if (strcmp(name, "getc") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_getc_stream(ev, recv, mode.sval, "IO#getc", site);
            return 1;
        }
        if (strcmp(name, "readchar") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_readchar_stream(ev, recv, mode.sval, "IO#readchar", site);
            return 1;
        }
        if (strcmp(name, "getbyte") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_getbyte_stream(ev, recv, mode.sval, site);
            return 1;
        }
        if (strcmp(name, "readbyte") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_readbyte_stream(ev, recv, mode.sval, site);
            return 1;
        }
        if (strcmp(name, "readlines") == 0) {
            *out = file_readlines_stream(ev, recv, mode.sval, "IO#readlines", args, argc, site);
            return 1;
        }
        if (strcmp(name, "each_byte") == 0) {
            if (!blk) {
                *out = eval_raise_class(ev, site, "LocalJumpError", "IO#each_byte requires a block");
                return 1;
            }
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            while (1) {
                Value byte = file_getbyte_stream(ev, recv, mode.sval, site);
                if (val_is_signal(byte)) {
                    *out = byte;
                    return 1;
                }
                if (byte.kind == VAL_NIL)
                    break;
                Value r = call_block(ev, env, *blk, &byte, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "each_char") == 0) {
            if (!blk) {
                *out = eval_raise_class(ev, site, "LocalJumpError", "IO#each_char requires a block");
                return 1;
            }
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            while (1) {
                Value ch = file_getc_stream(ev, recv, mode.sval, "IO#each_char", site);
                if (val_is_signal(ch)) {
                    *out = ch;
                    return 1;
                }
                if (ch.kind == VAL_NIL)
                    break;
                Value r = call_block(ev, env, *blk, &ch, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "each_line") == 0) {
            if (!blk) {
                *out = eval_raise_class(ev, site, "LocalJumpError", "IO#each_line requires a block");
                return 1;
            }
            while (1) {
                Value line = file_gets_stream(ev, recv, mode.sval, "IO#each_line", args, argc, site);
                if (val_is_signal(line)) {
                    *out = line;
                    return 1;
                }
                if (line.kind == VAL_NIL)
                    break;
                Value r = call_block(ev, env, *blk, &line, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "eof?") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_eof_stream(ev, recv, site);
            return 1;
        }
        if (strcmp(name, "read") == 0) {
            *out = file_read_stream_with_length(ev, recv, mode.sval, "IO#read", args, argc, site);
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
        if (strcmp(name, "pos") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_tell_stream(ev, recv, site);
            return 1;
        }
        if (strcmp(name, "pos=") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            Value positioned = file_seek_stream(ev, recv, args, 1, site);
            if (val_is_signal(positioned)) {
                *out = positioned;
                return 1;
            }
            *out = args[0];
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
            *out = file_read_stream_with_length(ev, recv, mode.sval, "File#read", args, argc, site);
            return 1;
        }
        if (strcmp(name, "gets") == 0) {
            *out = file_gets_stream(ev, recv, mode.sval, "File#gets", args, argc, site);
            return 1;
        }
        if (strcmp(name, "readline") == 0) {
            *out = file_readline_stream(ev, recv, mode.sval, "File#readline", args, argc, site);
            return 1;
        }
        if (strcmp(name, "getc") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_getc_stream(ev, recv, mode.sval, "File#getc", site);
            return 1;
        }
        if (strcmp(name, "readchar") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_readchar_stream(ev, recv, mode.sval, "File#readchar", site);
            return 1;
        }
        if (strcmp(name, "getbyte") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_getbyte_stream(ev, recv, mode.sval, site);
            return 1;
        }
        if (strcmp(name, "readbyte") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_readbyte_stream(ev, recv, mode.sval, site);
            return 1;
        }
        if (strcmp(name, "readlines") == 0) {
            *out = file_readlines_stream(ev, recv, mode.sval, "File#readlines", args, argc, site);
            return 1;
        }
        if (strcmp(name, "each_byte") == 0) {
            if (!blk) {
                *out = eval_raise_class(ev, site, "LocalJumpError", "File#each_byte requires a block");
                return 1;
            }
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            while (1) {
                Value byte = file_getbyte_stream(ev, recv, mode.sval, site);
                if (val_is_signal(byte)) {
                    *out = byte;
                    return 1;
                }
                if (byte.kind == VAL_NIL)
                    break;
                Value r = call_block(ev, env, *blk, &byte, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "each_char") == 0) {
            if (!blk) {
                *out = eval_raise_class(ev, site, "LocalJumpError", "File#each_char requires a block");
                return 1;
            }
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            while (1) {
                Value ch = file_getc_stream(ev, recv, mode.sval, "File#each_char", site);
                if (val_is_signal(ch)) {
                    *out = ch;
                    return 1;
                }
                if (ch.kind == VAL_NIL)
                    break;
                Value r = call_block(ev, env, *blk, &ch, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "each_line") == 0) {
            if (!blk) {
                *out = eval_raise_class(ev, site, "LocalJumpError", "File#each_line requires a block");
                return 1;
            }
            while (1) {
                Value line = file_gets_stream(ev, recv, mode.sval, "File#each_line", args, argc, site);
                if (val_is_signal(line)) {
                    *out = line;
                    return 1;
                }
                if (line.kind == VAL_NIL)
                    break;
                Value r = call_block(ev, env, *blk, &line, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
            return 1;
        }
        if (strcmp(name, "eof?") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_eof_stream(ev, recv, site);
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
        if (strcmp(name, "pos") == 0) {
            if (argc != 0) {
                *out = wrong_arg_count(ev, site, argc, 0);
                return 1;
            }
            *out = file_tell_stream(ev, recv, site);
            return 1;
        }
        if (strcmp(name, "pos=") == 0) {
            if (argc != 1) {
                *out = wrong_arg_count(ev, site, argc, 1);
                return 1;
            }
            Value positioned = file_seek_stream(ev, recv, args, 1, site);
            if (val_is_signal(positioned)) {
                *out = positioned;
                return 1;
            }
            *out = args[0];
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
