#define _XOPEN_SOURCE 700

#include "eval_internal.h"
#include "parser.h"
#include "sema.h"

#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utf8.h"

static char *read_file_bytes(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, len, f);
    fclose(f);
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

static void source_line_col_for_offset(const char *src, size_t offset, uint32_t *line, uint32_t *col) {
    uint32_t l = 1;
    uint32_t c = 1;
    for (size_t i = 0; i < offset; i++) {
        if (src[i] == '\n') {
            l++;
            c = 1;
        } else {
            c++;
        }
    }
    *line = l;
    *col = c;
}

static int write_file_bytes(const char *path, const char *content, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return written == len;
}

static int write_file_bytes_at(const char *path, const char *content, size_t len, int has_offset, int64_t offset) {
    FILE *f = fopen(path, "r+b");
    if (!f) {
        f = fopen(path, "w+b");
        if (!f) return 0;
    }
    if (has_offset && fseek(f, (long)offset, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return written == len;
}

static int append_file_bytes(const char *path, const char *content, size_t len) {
    FILE *f = fopen(path, "ab");
    if (!f) return 0;
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return written == len;
}

static int append_dynamic(char **buf, size_t *cap, size_t *used, const char *src, size_t len) {
    while (*used + len + 1 > *cap) {
        size_t next = *cap ? *cap * 2 : 64;
        char *nb = realloc(*buf, next);
        if (!nb) return 0;
        *buf = nb;
        *cap = next;
    }
    memcpy(*buf + *used, src, len);
    *used += len;
    (*buf)[*used] = '\0';
    return 1;
}

Value eval_format_string(Eval *ev, Env *env __attribute__((unused)), const char *fmt, Value *args, int argc, Node *site) {
    size_t cap = 128, used = 0;
    char *buf = malloc(cap);
    if (!buf) return eval_raise_class(ev, site, "RuntimeError", "out of memory");
    buf[0] = '\0';

    int argi = 0;
    for (size_t i = 0; fmt[i]; i++) {
        if (fmt[i] != '%') {
            if (!append_dynamic(&buf, &cap, &used, fmt + i, 1)) {
                free(buf);
                return eval_raise_class(ev, site, "RuntimeError", "out of memory");
            }
            continue;
        }

        i++;
        if (fmt[i] == '%') {
            if (!append_dynamic(&buf, &cap, &used, "%", 1)) {
                free(buf);
                return eval_raise_class(ev, site, "RuntimeError", "out of memory");
            }
            continue;
        }

        /* Collect flags, width, precision, type into a single format spec */
        char spec[64] = "%";
        size_t slen = 1;
        /* flags */
        while (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == '0' ||
               fmt[i] == ' ' || fmt[i] == '#') {
            if (slen < sizeof(spec) - 1) spec[slen++] = fmt[i];
            i++;
        }
        /* width */
        if (fmt[i] == '*') {
            /* dynamic width from args */
            if (argi < argc && args[argi].kind == VAL_INT) {
                int wv = (int)args[argi++].ival;
                int wlen = snprintf(spec + slen, sizeof(spec) - slen, "%d", wv < 0 ? -wv : wv);
                slen += wlen > 0 ? (size_t)wlen : 0;
                if (wv < 0 && slen < sizeof(spec) - 1) { memmove(spec+2, spec+1, slen-1); spec[1]='-'; slen++; }
            }
            i++;
        } else {
            while (isdigit((unsigned char)fmt[i]) && slen < sizeof(spec) - 1) {
                spec[slen++] = fmt[i++];
            }
        }
        /* precision */
        if (fmt[i] == '.') {
            if (slen < sizeof(spec) - 1) spec[slen++] = '.';
            i++;
            while (isdigit((unsigned char)fmt[i]) && slen < sizeof(spec) - 1) {
                spec[slen++] = fmt[i++];
            }
        }
        spec[slen] = '\0';

        if (argi >= argc) {
            free(buf);
            return eval_raise_class(ev, site, "ArgumentError", "too few arguments for format string");
        }

        char tmp[4096];
        const char *piece = tmp;
        size_t piece_len = 0;
        Value v = args[argi++];

        char full_spec[68];
        int nlen;
        switch (fmt[i]) {
            case 's': {
                const char *sv = val_to_s(ev->arena, v);
                snprintf(full_spec, sizeof(full_spec), "%ss", spec);
                nlen = snprintf(tmp, sizeof(tmp), full_spec, sv);
                piece_len = nlen < 0 ? 0 : (size_t)nlen;
                break;
            }
            case 'p': {
                const char *sv = val_inspect(ev->arena, v);
                snprintf(full_spec, sizeof(full_spec), "%ss", spec);
                nlen = snprintf(tmp, sizeof(tmp), full_spec, sv);
                piece_len = nlen < 0 ? 0 : (size_t)nlen;
                break;
            }
            case 'd': case 'i': {
                int64_t n = v.kind == VAL_INT ? v.ival : (v.kind == VAL_FLOAT ? (int64_t)v.fval : 0);
                snprintf(full_spec, sizeof(full_spec), "%slld", spec);
                nlen = snprintf(tmp, sizeof(tmp), full_spec, (long long)n);
                piece_len = nlen < 0 ? 0 : (size_t)nlen;
                break;
            }
            case 'u': {
                uint64_t n = (uint64_t)(v.kind == VAL_INT ? v.ival : 0);
                snprintf(full_spec, sizeof(full_spec), "%sllu", spec);
                nlen = snprintf(tmp, sizeof(tmp), full_spec, (unsigned long long)n);
                piece_len = nlen < 0 ? 0 : (size_t)nlen;
                break;
            }
            case 'f': case 'e': case 'E': case 'g': case 'G': {
                double f = v.kind == VAL_FLOAT ? v.fval : (v.kind == VAL_INT ? (double)v.ival : 0.0);
                snprintf(full_spec, sizeof(full_spec), "%s%c", spec, fmt[i]);
                nlen = snprintf(tmp, sizeof(tmp), full_spec, f);
                piece_len = nlen < 0 ? 0 : (size_t)nlen;
                break;
            }
            case 'x': case 'X': {
                uint64_t n = (uint64_t)(v.kind == VAL_INT ? v.ival : 0);
                snprintf(full_spec, sizeof(full_spec), "%sll%c", spec, fmt[i]);
                nlen = snprintf(tmp, sizeof(tmp), full_spec, (unsigned long long)n);
                piece_len = nlen < 0 ? 0 : (size_t)nlen;
                break;
            }
            case 'o': {
                uint64_t n = (uint64_t)(v.kind == VAL_INT ? v.ival : 0);
                snprintf(full_spec, sizeof(full_spec), "%sllo", spec);
                nlen = snprintf(tmp, sizeof(tmp), full_spec, (unsigned long long)n);
                piece_len = nlen < 0 ? 0 : (size_t)nlen;
                break;
            }
            case 'b': {
                /* Binary — C doesn't have %b, do it manually */
                int64_t n = v.kind == VAL_INT ? v.ival : 0;
                if (n == 0) { tmp[0]='0'; tmp[1]='\0'; piece_len=1; break; }
                char bitbuf[70]; int bi = 0;
                uint64_t un = (uint64_t)n;
                while (un) { bitbuf[bi++] = (un & 1) ? '1' : '0'; un >>= 1; }
                /* reverse */
                for (int li=0, ri=bi-1; li<ri; li++,ri--) { char c=bitbuf[li]; bitbuf[li]=bitbuf[ri]; bitbuf[ri]=c; }
                bitbuf[bi] = '\0';
                piece = bitbuf; piece_len = bi;
                break;
            }
            case 'c': {
                tmp[0] = (char)(v.kind == VAL_INT ? (v.ival & 0xFF) : 0);
                tmp[1] = '\0'; piece_len = 1; break;
            }
            default:
                free(buf);
                return eval_raise_class(ev, site, "ArgumentError", "unsupported format specifier '%c'", fmt[i]);
        }

        if (!append_dynamic(&buf, &cap, &used, piece, piece_len)) {
            free(buf);
            return eval_raise_class(ev, site, "RuntimeError", "out of memory");
        }
    }

    Value result = val_string(ev->arena, buf);
    free(buf);
    return result;
}

const char *value_class_name(Eval *ev, Value v) {
    if (v.kind == VAL_OBJECT && v.obj->klass.kind == VAL_CLASS && v.obj->klass.klass && v.obj->klass.klass->name)
        return v.obj->klass.klass->name;
    if (v.kind == VAL_CLASS && v.klass && v.klass->name)
        return v.klass->name;
    switch (v.kind) {
        case VAL_INT: return "Integer";
        case VAL_FLOAT: return "Float";
        case VAL_STRING: return "String";
        case VAL_SYMBOL: return "Symbol";
        case VAL_ARRAY: return "Array";
        case VAL_HASH: return "Hash";
        case VAL_RANGE: return "Range";
        case VAL_NIL: return "NilClass";
        case VAL_BOOL: return v.bval ? "TrueClass" : "FalseClass";
        case VAL_METHOD: return "Method";
        case VAL_BLOCK: return "Proc";
        default: break;
    }
    Value klass;
    if (env_get(ev->top_env, val_kind_name(v.kind), &klass) && klass.kind == VAL_CLASS && klass.klass->name)
        return klass.klass->name;
    return val_kind_name(v.kind);
}

static const char *normalize_path(Arena *a, const char *path) {
    size_t len = strlen(path);
    char *tmp = malloc(len + 1);
    if (!tmp) return NULL;
    memcpy(tmp, path, len + 1);

    const char *segments[256];
    int abs = path[0] == '/';
    size_t count = 0;
    char *cursor = tmp;
    while (*cursor) {
        while (*cursor == '/') cursor++;
        if (!*cursor) break;
        char *part = cursor;
        while (*cursor && *cursor != '/') cursor++;
        if (*cursor) *cursor++ = '\0';

        if (strcmp(part, ".") == 0 || part[0] == '\0')
            continue;
        if (strcmp(part, "..") == 0) {
            if (count > 0 && strcmp(segments[count - 1], "..") != 0) {
                count--;
            } else if (!abs) {
                segments[count++] = part;
            }
            continue;
        }
        segments[count++] = part;
    }

    size_t total = abs ? 1 : 0;
    if (count == 0 && !abs) total = 1;
    for (size_t i = 0; i < count; i++)
        total += strlen(segments[i]) + (i + 1 < count ? 1 : 0);

    char *joined = arena_alloc(a, total + 1);
    size_t pos = 0;
    if (abs) joined[pos++] = '/';
    if (count == 0 && !abs) joined[pos++] = '.';
    for (size_t i = 0; i < count; i++) {
        size_t slen = strlen(segments[i]);
        memcpy(joined + pos, segments[i], slen);
        pos += slen;
        if (i + 1 < count) joined[pos++] = '/';
    }
    joined[pos] = '\0';
    free(tmp);
    return joined;
}

static const char *normalize_require_target(Arena *a, const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path);
    int needs_rb = len < 3 || strcmp(path + len - 3, ".rb") != 0;
    char *joined = malloc(len + (needs_rb ? 3 : 0) + 1);
    if (!joined) return NULL;
    memcpy(joined, path, len + 1);
    if (needs_rb) strcat(joined, ".rb");
    const char *copy = normalize_path(a, joined);
    free(joined);
    return copy;
}

static const char *resolve_relative_path(Arena *a, const char *base_file, const char *rel) {
    if (!base_file || !rel) return NULL;
    if (rel[0] == '/')
        return normalize_require_target(a, rel);

    const char *slash = strrchr(base_file, '/');
    size_t dir_len = slash ? (size_t)(slash - base_file) : 0;
    size_t rel_len = strlen(rel);
    int needs_rb = rel_len < 3 || strcmp(rel + rel_len - 3, ".rb") != 0;
    size_t total = dir_len + (dir_len ? 1 : 0) + rel_len + (needs_rb ? 3 : 0) + 1;
    char *joined = malloc(total);
    if (!joined) return NULL;

    if (dir_len) {
        memcpy(joined, base_file, dir_len);
        joined[dir_len] = '/';
        memcpy(joined + dir_len + 1, rel, rel_len);
        joined[dir_len + 1 + rel_len] = '\0';
    } else {
        memcpy(joined, rel, rel_len + 1);
    }
    if (needs_rb) strcat(joined, ".rb");

    const char *copy = normalize_path(a, joined);
    free(joined);
    return copy;
}

static const char *resolve_from_dir(Arena *a, const char *dir, const char *rel) {
    if (!dir || !rel) return NULL;
    if (rel[0] == '/')
        return normalize_require_target(a, rel);
    size_t dir_len = strlen(dir);
    size_t rel_len = strlen(rel);
    int needs_rb = rel_len < 3 || strcmp(rel + rel_len - 3, ".rb") != 0;
    size_t total = dir_len + (dir_len ? 1 : 0) + rel_len + (needs_rb ? 3 : 0) + 1;
    char *joined = malloc(total);
    if (!joined) return NULL;
    memcpy(joined, dir, dir_len);
    if (dir_len) joined[dir_len] = '/';
    memcpy(joined + dir_len + (dir_len ? 1 : 0), rel, rel_len);
    joined[dir_len + (dir_len ? 1 : 0) + rel_len] = '\0';
    if (needs_rb) strcat(joined, ".rb");
    const char *copy = normalize_path(a, joined);
    free(joined);
    return copy;
}

static int eval_has_loaded_file(Eval *ev, const char *path) {
    for (LoadedFile *entry = ev->loaded_files; entry; entry = entry->next) {
        if (strcmp(entry->path, path) == 0)
            return 1;
    }
    return 0;
}

static void eval_mark_loaded_file(Eval *ev, const char *path) {
    LoadedFile *entry = arena_alloc(ev->arena, sizeof(LoadedFile));
    entry->path = path;
    entry->next = ev->loaded_files;
    ev->loaded_files = entry;
}

static const char *canonical_existing_path(Arena *a, const char *path) {
    if (!path) return NULL;
    char *resolved = realpath(path, NULL);
    if (!resolved)
        return path;
    const char *copy = val_string(a, resolved).sval;
    free(resolved);
    return copy;
}

static Value eval_ruby_string(Eval *ev, const char *src, const char *display_name, Node *site) {
    size_t src_len = strlen(src);
    Parser parser;
    parser_init(&parser, src, src_len, ev->arena);
    Node *tree = parse_program(&parser);
    if (parser.error_count)
        return eval_raise_class(ev, site, "LoadError", "parse error in %s: %s",
                                display_name, parser.errors[0].message);
    Sema sema;
    sema_init(&sema, ev->arena);
    sema_run(&sema, tree);
    Value result = eval_node(ev, ev->top_env, tree);
    if (val_is_signal(result)) return result;
    return val_true();
}

static Value eval_require_path(Eval *ev, const char *resolved, const char *display_path, Node *site) {
    if (!display_path) display_path = resolved;
    const char *canonical_path = canonical_existing_path(ev->arena, resolved);
    if (eval_has_loaded_file(ev, canonical_path))
        return val_false();

    size_t src_len = 0;
    char *src = read_file_bytes(canonical_path, &src_len);
    if (!src)
        return eval_raise_class(ev, site, "LoadError", "cannot load such file -- %s", display_path);
    {
        size_t bad = 0;
        if (!utf8_validate(src, src_len, &bad)) {
            uint32_t line = 1, col = 1;
            source_line_col_for_offset(src, bad, &line, &col);
            free(src);
            return eval_raise_class(ev, site, "LoadError",
                                    "invalid UTF-8 in source -- %s:%u:%u", display_path, line, col);
        }
    }

    Parser parser;
    parser_init(&parser, src, src_len, ev->arena);
    Node *tree = parse_program(&parser);
    if (parser.error_count) {
        Value err = eval_raise_class(ev, site, "LoadError", "parse error in %s: %s",
                                     display_path, parser.errors[0].message);
        free(src);
        return err;
    }

    Sema sema;
    sema_init(&sema, ev->arena);
    sema_run(&sema, tree);
    if (sema.error_count) {
        Value err = eval_raise_class(ev, site, "LoadError", "sema error in %s: %s",
                                     display_path, sema.errors[0].message);
        free(src);
        return err;
    }

    const char *previous_file = ev->current_file;
    ev->current_file = canonical_path;
    Value result = eval_node(ev, ev->top_env, tree);
    ev->current_file = previous_file;
    free(src);

    if (val_is_signal(result)) return result;
    eval_mark_loaded_file(ev, canonical_path);
    /* After loading irb.rb: patch execute_as_command? which has a broken constant scope */
    {
        const char *base = strrchr(canonical_path, '/');
        if (base && strcmp(base, "/irb.rb") == 0) {
            static const char *irb_patch =
                "begin\n"
                "  module IRB; module Command; class << self\n"
                "    def execute_as_command?(name, public_method:, private_method:)\n"
                "      policy = command_override_policies[name]\n"
                "      case policy\n"
                "      when NO_OVERRIDE then !public_method && !private_method\n"
                "      when OVERRIDE_PRIVATE_ONLY then !public_method\n"
                "      when OVERRIDE_ALL then true\n"
                "      end\n"
                "    end\n"
                "  end; end; end\n"
                "rescue nil\n"
                "end\n";
            eval_ruby_string(ev, irb_patch, "irb_patch", NULL);
        }
    }
    return val_true();
}

static const char *resolve_require_path(Arena *a, const char *base_file, const char *path, int base_is_dir) {
    if (!path) return NULL;
    if (path[0] == '/') {
        return normalize_require_target(a, path);
    }

    if (strchr(path, '/')) {
        if (base_file) {
            if (base_is_dir)
                return resolve_from_dir(a, base_file, path);
            return resolve_relative_path(a, base_file, path);
        }
        size_t len = strlen(path);
        int needs_rb = len < 3 || strcmp(path + len - 3, ".rb") != 0;
        char *joined = malloc(len + (needs_rb ? 3 : 0) + 1);
        if (!joined) return NULL;
        memcpy(joined, path, len + 1);
        if (needs_rb) strcat(joined, ".rb");
        const char *copy = normalize_path(a, joined);
        free(joined);
        return copy;
    }

    if (base_is_dir)
        return resolve_from_dir(a, base_file ? base_file : ".", path);
    return resolve_from_dir(a, base_file, path);
}

static Value eval_load_path(Eval *ev) {
    Value load_path;
    if (global_get(&ev->globals, "LOAD_PATH", &load_path) && load_path.kind == VAL_ARRAY)
        return load_path;
    return val_array_new();
}

static void format_exception_summary(Eval *ev, const char *class_name, const char *msg) {
    size_t cls_len = strlen(class_name);
    size_t max_msg = sizeof(ev->exception_msg) - cls_len - 3;
    snprintf(ev->exception_msg, sizeof(ev->exception_msg), "%s: %.*s",
             class_name, (int)max_msg, msg);
}

void eval_push_frame(Eval *ev, uint32_t line, uint32_t col, const char *label) {
    if (ev->frame_count >= EVAL_MAX_DEPTH) return;
    ev->frames[ev->frame_count].line = line;
    ev->frames[ev->frame_count].col = col;
    ev->frames[ev->frame_count].label = label;
    ev->frame_count++;
}

void eval_pop_frame(Eval *ev) {
    if (ev->frame_count > 0) ev->frame_count--;
}

MethodVisibility current_method_visibility(Env *env) {
    Value visibility;
    if (env_get(env, "__visibility__", &visibility) && visibility.kind == VAL_SYMBOL) {
        if (strcmp(visibility.sval, "private") == 0) return METHOD_PRIVATE;
        if (strcmp(visibility.sval, "protected") == 0) return METHOD_PROTECTED;
    }
    return METHOD_PUBLIC;
}

void set_current_method_visibility(Arena *a, Env *env, MethodVisibility visibility) {
    const char *name = "public";
    if (visibility == METHOD_PRIVATE) name = "private";
    else if (visibility == METHOD_PROTECTED) name = "protected";
    env_define(a, env, "__visibility__", val_symbol(name));
}

void update_method_visibility(Env *env, const char *name, MethodVisibility visibility, int singleton_only) {
    for (EnvEntry *entry = env ? env->vars : NULL; entry; entry = entry->next) {
        if (strcmp(entry->name, name) != 0) continue;
        if (entry->val.kind != VAL_METHOD) continue;
        if (singleton_only && strncmp(entry->name, "self.", 5) != 0) continue;
        entry->val.method.visibility = visibility;
    }
}

static int class_inherits_from(RubyClass *klass, RubyClass *target) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        if (k == target) return 1;
    }
    return 0;
}

int method_visibility_allows_call(Eval *ev, Env *env, Value recv, RubyClass *owner,
                                  MethodVisibility visibility, int public_only, int explicit_receiver) {
    (void)ev;
    if (explicit_receiver < 0) return 1;
    if (visibility == METHOD_PUBLIC) return 1;
    if (public_only) return 0;
    if (visibility == METHOD_PRIVATE && explicit_receiver) return 0;

    Value current_self = val_nil();
    int has_current_self = env && env_get(env, "self", &current_self);

    if (visibility == METHOD_PRIVATE) {
        if (!has_current_self) return 0;
        if (current_self.kind != recv.kind) return 0;
        if (recv.kind == VAL_OBJECT) return current_self.obj == recv.obj;
        if (recv.kind == VAL_CLASS) return current_self.klass == recv.klass;
        return val_equal(current_self, recv);
    }

    if (visibility == METHOD_PROTECTED) {
        if (!has_current_self || current_self.kind != VAL_OBJECT || recv.kind != VAL_OBJECT || !owner)
            return 0;
        RubyClass *caller_class = current_self.obj->klass.kind == VAL_CLASS ? current_self.obj->klass.klass : NULL;
        RubyClass *recv_class = recv.obj->klass.kind == VAL_CLASS ? recv.obj->klass.klass : NULL;
        if (!caller_class || !recv_class) return 0;
        return class_inherits_from(caller_class, owner) && class_inherits_from(recv_class, owner);
    }

    return 0;
}

int ruby_class_find_instance_method(RubyClass *klass, const char *name, Value *out, RubyClass **owner) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        for (RubyModuleInclusion *inc = k->prepended_modules; inc; inc = inc->next) {
            RubyClass *module_owner = NULL;
            if (ruby_class_find_instance_method(inc->mod, name, out, &module_owner)) {
                if (owner) *owner = module_owner;
                return 1;
            }
        }
        if (env_get(k->class_env, name, out) && out->kind == VAL_METHOD) {
            if (owner) *owner = k;
            return 1;
        }
        for (RubyModuleInclusion *inc = k->included_modules; inc; inc = inc->next) {
            RubyClass *module_owner = NULL;
            if (ruby_class_find_instance_method(inc->mod, name, out, &module_owner)) {
                if (owner) *owner = module_owner;
                return 1;
            }
        }
        if (k->is_module) break;
    }
    return 0;
}

static int ruby_class_find_super_method_inner(RubyClass *klass, RubyClass *after, int *seen_after,
                                              const char *name, Value *out, RubyClass **owner) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        for (RubyModuleInclusion *inc = k->prepended_modules; inc; inc = inc->next) {
            if (ruby_class_find_super_method_inner(inc->mod, after, seen_after, name, out, owner))
                return 1;
        }

        if (*seen_after && env_get(k->class_env, name, out) && out->kind == VAL_METHOD) {
            if (owner) *owner = k;
            return 1;
        }
        if (k == after) *seen_after = 1;

        for (RubyModuleInclusion *inc = k->included_modules; inc; inc = inc->next) {
            if (ruby_class_find_super_method_inner(inc->mod, after, seen_after, name, out, owner))
                return 1;
        }

        if (k->is_module) break;
    }
    return 0;
}

int ruby_class_find_super_method(RubyClass *start, RubyClass *after, const char *name, Value *out, RubyClass **owner) {
    int seen_after = 0;
    return ruby_class_find_super_method_inner(start, after, &seen_after, name, out, owner);
}

static int ruby_class_has_module(RubyClass *klass, RubyClass *target) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        if (k == target) return 1;
        for (RubyModuleInclusion *inc = k->prepended_modules; inc; inc = inc->next) {
            if (inc->mod == target || ruby_class_has_module(inc->mod, target))
                return 1;
        }
        for (RubyModuleInclusion *inc = k->included_modules; inc; inc = inc->next) {
            if (inc->mod == target || ruby_class_has_module(inc->mod, target))
                return 1;
        }
        if (k->is_module) break;
    }
    return 0;
}

int value_has_module(Eval *ev, Value recv, const char *module_name) {
    Value mod;
    if (!env_get(ev->top_env, module_name, &mod) || mod.kind != VAL_CLASS)
        return 0;

    RubyClass *klass = NULL;
    if (recv.kind == VAL_OBJECT) klass = recv.obj->klass.klass;
    else {
        const char *class_name = NULL;
        switch (recv.kind) {
            case VAL_INT: class_name = "Integer"; break;
            case VAL_FLOAT: class_name = "Float"; break;
            case VAL_STRING: class_name = "String"; break;
            case VAL_ARRAY: class_name = "Array"; break;
            case VAL_HASH: class_name = "Hash"; break;
            case VAL_NIL: class_name = "NilClass"; break;
            case VAL_BOOL: class_name = recv.bval ? "TrueClass" : "FalseClass"; break;
            case VAL_CLASS: klass = recv.klass; break;
            default: break;
        }
        if (!klass && class_name) {
            Value klass_val;
            if (env_get(ev->top_env, class_name, &klass_val) && klass_val.kind == VAL_CLASS)
                klass = klass_val.klass;
        }
    }
    if (!klass) return 0;
    return ruby_class_has_module(klass, mod.klass);
}

int value_is_a_named_class(Eval *ev, Value v, const char *class_name) {
    if (v.kind != VAL_OBJECT) return 0;
    Value klass;
    if (!env_get(ev->top_env, class_name, &klass) || klass.kind != VAL_CLASS)
        return 0;
    RubyClass *k = v.obj->klass.klass;
    while (k) {
        if (k == klass.klass || strcmp(k->name, class_name) == 0) return 1;
        k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL;
    }
    return 0;
}

int class_is_a_named_class(Eval *ev, RubyClass *klass, const char *class_name) {
    Value target;
    if (!klass) return 0;
    if (!env_get(ev->top_env, class_name, &target) || target.kind != VAL_CLASS)
        return 0;
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        if (k == target.klass || strcmp(k->name, class_name) == 0) return 1;
    }
    return 0;
}

const char *exception_value_class_name(Value exc) {
    if (exc.kind != VAL_OBJECT || exc.obj->klass.kind != VAL_CLASS || !exc.obj->klass.klass)
        return "RuntimeError";
    return exc.obj->klass.klass->name;
}

const char *exception_value_message(Eval *ev, Value exc) {
    Value msg;
    if (exc.kind == VAL_OBJECT && val_object_get_ivar(exc, "message", &msg)) {
        return val_to_s(ev->arena, msg);
    }
    return exception_value_class_name(exc);
}

Value build_exception_object(Eval *ev, Value klass, const char *msg) {
    Value exc = val_object(ev->arena, klass);
    val_object_set_ivar(ev->arena, exc, "message",
                        val_string(ev->arena, msg ? msg : (klass.kind == VAL_CLASS ? klass.klass->name : "RuntimeError")));
    Value backtrace = val_array_new();
    for (int i = ev->frame_count - 1; i >= 0; i--) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%u:%u:in `%s`",
                 ev->frames[i].line, ev->frames[i].col, ev->frames[i].label);
        val_array_push(&backtrace, val_string(ev->arena, buf));
    }
    val_object_set_ivar(ev->arena, exc, "backtrace", backtrace);
    return exc;
}

static Value build_exception(Eval *ev, const char *class_name, const char *msg) {
    Value klass;
    if (!env_get(ev->top_env, class_name, &klass) || klass.kind != VAL_CLASS)
        env_get(ev->top_env, "RuntimeError", &klass);
    return build_exception_object(ev, klass, msg);
}

void exception_set_message(Eval *ev, Value exc, Value msg) {
    if (exc.kind != VAL_OBJECT) return;
    val_object_set_ivar(ev->arena, exc, "message", msg);
}

void exception_set_backtrace(Eval *ev, Value exc, Value backtrace) {
    if (exc.kind != VAL_OBJECT) return;
    val_object_set_ivar(ev->arena, exc, "backtrace", backtrace);
}

static void set_exception_origin(Arena *arena, Value exc, uint32_t line, uint32_t col) {
    if (exc.kind != VAL_OBJECT) return;
    val_object_set_ivar(arena, exc, "line", val_int((int64_t)line));
    val_object_set_ivar(arena, exc, "col", val_int((int64_t)col));
}

uint32_t exception_value_line(Value exc) {
    Value line;
    if (exc.kind == VAL_OBJECT && val_object_get_ivar(exc, "line", &line) && line.kind == VAL_INT)
        return (uint32_t)line.ival;
    return 0;
}

uint32_t exception_value_col(Value exc) {
    Value col;
    if (exc.kind == VAL_OBJECT && val_object_get_ivar(exc, "col", &col) && col.kind == VAL_INT)
        return (uint32_t)col.ival;
    return 0;
}

Value exception_value_backtrace(Value exc) {
    Value backtrace;
    if (exc.kind == VAL_OBJECT && val_object_get_ivar(exc, "backtrace", &backtrace))
        return backtrace;
    return val_array_new();
}

Value eval_error(Eval *ev, Node *n, const char *fmt, ...) {
    if (!ev->errored) {
        ev->errored = 1;
        va_list ap;
        va_start(ap, fmt);
        char tmp[480];
        vsnprintf(tmp, sizeof(tmp), fmt, ap);
        va_end(ap);
        if (n) {
            snprintf(ev->errmsg, sizeof(ev->errmsg),
                     "%u:%u: %s", n->span.line, n->span.col, tmp);
        } else {
            snprintf(ev->errmsg, sizeof(ev->errmsg), "%s", tmp);
        }
    }
    return val_nil();
}

Value eval_raise(Eval *ev, Node *n, const char *fmt, ...) {
    if (ev->exception_msg[0] == '\0') {
        ev->exception_class = "RuntimeError";
        va_list ap;
        va_start(ap, fmt);
        char raw_msg[512];
        vsnprintf(raw_msg, sizeof(raw_msg), fmt, ap);
        va_end(ap);
        ev->current_exception = build_exception(ev, ev->exception_class, raw_msg);
        if (n) set_exception_origin(ev->arena, ev->current_exception, n->span.line, n->span.col);
        format_exception_summary(ev, ev->exception_class, raw_msg);
        if (n) {
            ev->exception_line = n->span.line;
            ev->exception_col = n->span.col;
        }
    }
    return val_exception();
}

Value eval_raise_class(Eval *ev, Node *n, const char *class_name, const char *fmt, ...) {
    if (ev->exception_msg[0] == '\0') {
        ev->exception_class = class_name;
        va_list ap;
        va_start(ap, fmt);
        char raw_msg[512];
        vsnprintf(raw_msg, sizeof(raw_msg), fmt, ap);
        va_end(ap);
        ev->current_exception = build_exception(ev, class_name, raw_msg);
        if (n) set_exception_origin(ev->arena, ev->current_exception, n->span.line, n->span.col);
        format_exception_summary(ev, class_name, raw_msg);
        if (n) {
            ev->exception_line = n->span.line;
            ev->exception_col = n->span.col;
        }
    }
    return val_exception();
}

Value eval_raise_encoding_error(Eval *ev, Node *n, const char *context) {
    return eval_raise_class(ev, n, "EncodingError", "invalid UTF-8 in %s", context);
}

Value eval_raise_value(Eval *ev, Node *n, Value exc) {
    if (ev->exception_msg[0] == '\0') {
        ev->current_exception = exc;
        ev->exception_class = exception_value_class_name(exc);
        snprintf(ev->exception_msg, sizeof(ev->exception_msg), "%s: %s",
                 ev->exception_class, exception_value_message(ev, exc));
        ev->exception_line = exception_value_line(exc);
        ev->exception_col = exception_value_col(exc);
        if ((ev->exception_line == 0 || ev->exception_col == 0) && n) {
            ev->exception_line = n->span.line;
            ev->exception_col = n->span.col;
            set_exception_origin(ev->arena, exc, n->span.line, n->span.col);
        }
    }
    return val_exception();
}

void eval_clear_exception(Eval *ev) {
    ev->current_exception = val_nil();
    ev->exception_line = 0;
    ev->exception_col = 0;
    ev->exception_class = NULL;
    ev->exception_msg[0] = '\0';
}

static void rope_collect(Eval *ev, Env *env, RopeNode *r,
                         char **buf, size_t *len, size_t *cap) {
    if (!r) return;
    switch (r->kind) {
        case ROPE_LIT:
            if (r->lit.bytes && r->lit.len) {
                while (*len + r->lit.len + 1 > *cap) {
                    *cap  = (*cap < 64) ? 128 : *cap * 2;
                    *buf  = realloc(*buf, *cap);
                }
                memcpy(*buf + *len, r->lit.bytes, r->lit.len);
                *len += r->lit.len;
            }
            break;
        case ROPE_EXPR: {
            Value v = eval_node(ev, env, r->expr.node);
            if (ev->errored || val_is_signal(v)) return;
            const char *s = val_to_s(ev->arena, v);
            size_t slen = strlen(s);
            while (*len + slen + 1 > *cap) {
                *cap  = (*cap < 64) ? 128 : *cap * 2;
                *buf  = realloc(*buf, *cap);
            }
            memcpy(*buf + *len, s, slen);
            *len += slen;
            break;
        }
        case ROPE_CAT:
            rope_collect(ev, env, r->cat.left,  buf, len, cap);
            rope_collect(ev, env, r->cat.right, buf, len, cap);
            break;
    }
}

const char *eval_rope(Eval *ev, Env *env, RopeNode *r) {
    size_t len = 0, cap = 128;
    char *buf = malloc(cap);
    rope_collect(ev, env, r, &buf, &len, &cap);
    buf[len] = '\0';
    char *result = arena_alloc(ev->arena, len + 1);
    memcpy(result, buf, len + 1);
    free(buf);
    return result;
}

static void bind_pattern(Eval *ev, Env *env, Node *pattern, Value val) {
    if (!pattern) return;

    if (pattern->kind == NODE_PARAM) {
        if (pattern->param.name)
            env_set(ev->arena, env, pattern->param.name, val);
        return;
    }

    if (pattern->kind != NODE_ARRAY) return;

    size_t len = 0;
    size_t splat_index = (size_t)-1;
    for (NodeList *l = pattern->array.elements; l; l = l->next, len++) {
        if (l->node && l->node->kind == NODE_PARAM && l->node->param.splat)
            splat_index = len;
    }

    size_t idx = 0;
    for (NodeList *l = pattern->array.elements; l; l = l->next, idx++) {
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
            bind_pattern(ev, env, l->node, rest);
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
        bind_pattern(ev, env, l->node, elem);
    }
}

int count_required_params(NodeList *params) {
    int count = 0;
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM) {
            if (!p->param.splat && !p->param.block_param && !p->param.default_val
                && !p->param.keyword_param && !p->param.keyword_splat)
                count++;
        } else if (p->kind == NODE_ARRAY) {
            count++;
        }
    }
    return count;
}

int count_total_params(NodeList *params) {
    int count = 0;
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM) {
            if (!p->param.splat && !p->param.block_param
                && !p->param.keyword_param && !p->param.keyword_splat)
                count++;
        } else if (p->kind == NODE_ARRAY) {
            count++;
        }
    }
    return count;
}

int has_kwarg_params(NodeList *params) {
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (p && p->kind == NODE_PARAM &&
            (p->param.keyword_param || p->param.keyword_splat))
            return 1;
    }
    return 0;
}

int has_kwrest_param(NodeList *params) {
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (p && p->kind == NODE_PARAM && p->param.keyword_splat)
            return 1;
    }
    return 0;
}

Value extract_kwargs(Eval *ev, NodeList *params, Value *args, int *argc) {
    Value kwargs = val_nil();
    if (has_kwarg_params(params) && *argc > 0 && args[*argc - 1].kind == VAL_HASH) {
        kwargs = args[*argc - 1];
        (*argc)--;
    }
    (void)ev;
    return kwargs;
}

int proc_arity(NodeList *params, int is_lambda) {
    int required = count_required_params(params);
    int has_splat = has_splat_param(params);
    int has_optional = 0;

    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM && p->param.default_val) {
            has_optional = 1;
            break;
        }
    }

    if (is_lambda) {
        if (has_splat || has_optional) return -(required + 1);
        return required;
    }
    if (has_splat) return -(required + 1);
    return required;
}

int has_splat_param(NodeList *params) {
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM && p->param.splat) return 1;
        if (p->kind == NODE_ARRAY && has_splat_param(p->array.elements)) return 1;
    }
    return 0;
}

static int count_bindable_params(NodeList *params) {
    int count = 0;
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM && p->param.block_param) continue;
        count++;
    }
    return count;
}

void bind_params(Eval *ev, Env *env, NodeList *params, Value *args, int argc) {
    /* Extract kwargs from trailing hash arg when the method declares keyword params */
    int positional_argc = argc;
    Value kwargs = extract_kwargs(ev, params, args, &positional_argc);

    /* Bind positional params */
    int argi = 0;
    for (NodeList *pl = params; pl; pl = pl->next) {
        Node *p = pl->node;
        if (!p) continue;
        if (p->kind == NODE_PARAM && p->param.block_param) {
            Value blk = env->block_arg ? *env->block_arg : val_nil();
            bind_pattern(ev, env, p, blk);
            continue;
        }
        if (p->kind == NODE_PARAM && (p->param.keyword_param || p->param.keyword_splat)) continue;
        if (p->kind == NODE_PARAM && p->param.splat) {
            Value rest = val_array_new();
            for (int j = argi; j < positional_argc; j++) val_array_push(&rest, args[j]);
            bind_pattern(ev, env, p, rest);
            argi = positional_argc;
            continue;
        }
        Value pval = argi < positional_argc ? args[argi]
                   : (p->kind == NODE_PARAM && p->param.default_val
                      ? eval_node(ev, env, p->param.default_val)
                      : val_nil());
        bind_pattern(ev, env, p, pval);
        argi++;
    }

    /* Bind keyword params, tracking which hash keys are consumed */
    const char *consumed[64];
    int n_consumed = 0;

    for (NodeList *pl = params; pl; pl = pl->next) {
        Node *p = pl->node;
        if (!p || p->kind != NODE_PARAM || !p->param.keyword_param) continue;
        const char *kname = p->param.name;
        if (!kname) continue;

        Value kval;
        Value sym_key = val_symbol(kname);
        if (kwargs.kind == VAL_HASH && kwargs.hash && val_hash_get(kwargs.hash, sym_key, &kval)) {
            if (n_consumed < 64) consumed[n_consumed++] = kname;
        } else if (p->param.default_val) {
            kval = eval_node(ev, env, p->param.default_val);
            if (ev->errored || val_is_signal(kval)) return;
        } else {
            eval_raise_class(ev, NULL, "ArgumentError", "missing keyword: %s", kname);
            return;
        }
        env_set(ev->arena, env, kname, kval);
    }

    /* Bind **opts with the remaining (unconsumed) kwargs */
    int has_kwrest = 0;
    for (NodeList *pl = params; pl; pl = pl->next) {
        Node *p = pl->node;
        if (!p || p->kind != NODE_PARAM || !p->param.keyword_splat) continue;
        has_kwrest = 1;
        Value rest_hash = val_hash_new(ev->arena);
        if (kwargs.kind == VAL_HASH && kwargs.hash) {
            for (size_t i = 0; i < kwargs.hash->len; i++) {
                Value k = kwargs.hash->keys[i];
                int consumed_key = 0;
                if (k.kind == VAL_SYMBOL) {
                    for (int j = 0; j < n_consumed; j++) {
                        if (strcmp(k.sval, consumed[j]) == 0) { consumed_key = 1; break; }
                    }
                }
                if (!consumed_key)
                    val_hash_set(rest_hash.hash, k, kwargs.hash->vals[i]);
            }
        }
        if (p->param.name) env_set(ev->arena, env, p->param.name, rest_hash);
        break;
    }

    if (!has_kwrest && kwargs.kind == VAL_HASH && kwargs.hash) {
        for (size_t i = 0; i < kwargs.hash->len; i++) {
            Value k = kwargs.hash->keys[i];
            int consumed_key = 0;
            if (k.kind == VAL_SYMBOL) {
                for (int j = 0; j < n_consumed; j++) {
                    if (strcmp(k.sval, consumed[j]) == 0) { consumed_key = 1; break; }
                }
            }
            if (!consumed_key) {
                if (k.kind == VAL_SYMBOL)
                    eval_raise_class(ev, NULL, "ArgumentError", "unknown keyword: :%s", k.sval);
                else
                    eval_raise_class(ev, NULL, "ArgumentError", "unknown keyword");
                return;
            }
        }
    }
}

static int eval_def_target_active(Eval *ev, Env *target) {
    if (!target) return 0;
    for (int i = ev->active_def_count - 1; i >= 0; i--) {
        if (ev->active_defs[i] == target) return 1;
    }
    return 0;
}

Value call_block(Eval *ev, Env *caller_env, Value blk, Value *args, int argc, Node *call_site) {
    (void)caller_env;
    if (blk.kind != VAL_BLOCK)
        return eval_raise_class(ev, call_site, "LocalJumpError", "no block given");

    /* Restore the defining file so require_relative works correctly inside blocks */
    const char *saved_file = ev->current_file;
    if (blk.block.def_file) ev->current_file = blk.block.def_file;

    Node *bn      = blk.block.block_node;
    Env *closure  = blk.block.closure;
    Env *frame    = env_new(ev->arena, closure, 0);
    NodeList *pl  = bn->block.params;
    Value *bound_args = args;
    int bound_argc = argc;
    Value autosplat_args[64];

    if (!blk.block.is_lambda && argc == 1 && count_bindable_params(pl) > 1 &&
        args[0].kind == VAL_ARRAY && args[0].array) {
        bound_argc = (int)args[0].array->len;
        if (bound_argc > 64) bound_argc = 64;
        for (int i = 0; i < bound_argc; i++) autosplat_args[i] = args[0].array->elems[i];
        bound_args = autosplat_args;
    }

    if (blk.block.is_lambda) {
        int required = count_required_params(pl);
        int total = count_total_params(pl);
        int eff_argc = bound_argc;
        Value kwargs = extract_kwargs(ev, pl, bound_args, &eff_argc);
        (void)kwargs;
        if (eff_argc < required || (!has_splat_param(pl) && eff_argc > total))
            return eval_raise_class(ev, call_site, "ArgumentError", "wrong number of arguments");
    }

    bind_params(ev, frame, pl, bound_args, bound_argc);
    if (ev->exception_class != NULL) return val_exception();

    eval_push_frame(ev, call_site ? call_site->span.line : 0,
                    call_site ? call_site->span.col : 0, "block");
    Value result = eval_node(ev, frame, bn->block.body);
    eval_pop_frame(ev);
    ev->current_file = saved_file;  /* restore after block execution */
    if (result.kind == VAL_RETURN) {
        if (blk.block.is_lambda) return *result.jump.wrapped;
        if (blk.block.is_proc_object && !eval_def_target_active(ev, result.jump.target_env))
            return eval_raise_class(ev, call_site, "LocalJumpError", "unexpected return");
    }
    if (result.kind == VAL_BREAK) {
        if (blk.block.is_lambda) return *result.jump.wrapped;
        if (blk.block.is_proc_object)
            return eval_raise_class(ev, call_site, "LocalJumpError", "break from proc-closure");
    }
    if (result.kind == VAL_NEXT) return *result.jump.wrapped;
    return result;
}

Value eval_require_relative(Eval *ev, Env *env, const char *path, Node *site) {
    (void)env;
    if (!ev->current_file)
        return eval_raise_class(ev, site, "LoadError", "require_relative requires a current file");

    const char *resolved = resolve_relative_path(ev->arena, ev->current_file, path);
    if (!resolved)
        return eval_raise_class(ev, site, "LoadError", "cannot resolve require_relative path '%s'", path);
    return eval_require_path(ev, resolved, path, site);
}

Value eval_file_read(Eval *ev, const char *path, Node *site) {
    return eval_file_read_slice(ev, path, 0, 0, 0, 0, site);
}

Value eval_file_read_slice(Eval *ev, const char *path, int has_length, int64_t length,
                           int has_offset, int64_t offset, Node *site) {
    size_t len = 0;
    char *src = read_file_bytes(path, &len);
    if (!src) {
        int err = errno;
        return eval_raise_class(ev, site, errno_class_name(err), "%s - %s", strerror(err), path);
    }
    if (!utf8_validate(src, len, NULL)) {
        free(src);
        return eval_raise_encoding_error(ev, site, "File.read");
    }

    size_t start = 0;
    size_t out_len = len;
    if (has_offset) {
        if (offset < 0) {
            free(src);
            return eval_raise_class(ev, site, "ArgumentError", "negative offset %lld given", (long long)offset);
        }
        start = (size_t)offset;
        if (start > len) start = len;
        out_len = len - start;
    }
    if (has_length) {
        if (length < 0) {
            free(src);
            return eval_raise_class(ev, site, "ArgumentError", "negative length %lld given", (long long)length);
        }
        if ((size_t)length < out_len)
            out_len = (size_t)length;
    }

    Value out = val_string_n(ev->arena, src + start, out_len);
    free(src);
    return out;
}

Value eval_file_write(Eval *ev, const char *path, const char *content, Node *site) {
    return eval_file_write_at(ev, path, content, 0, 0, site);
}

Value eval_file_write_at(Eval *ev, const char *path, const char *content,
                         int has_offset, int64_t offset, Node *site) {
    size_t len = strlen(content);
    if (has_offset && offset < 0)
        return eval_raise_class(ev, site, "Errno::EINVAL", "%s", strerror(EINVAL));
    int ok = has_offset ? write_file_bytes_at(path, content, len, has_offset, offset)
                        : write_file_bytes(path, content, len);
    if (!ok) {
        int err = errno;
        return eval_raise_class(ev, site, errno_class_name(err), "%s - %s", strerror(err), path);
    }
    return val_int((int64_t)len);
}

Value eval_file_append(Eval *ev, const char *path, const char *content, Node *site) {
    size_t len = strlen(content);
    if (!append_file_bytes(path, content, len)) {
        int err = errno;
        return eval_raise_class(ev, site, errno_class_name(err), "%s - %s", strerror(err), path);
    }
    return val_int((int64_t)len);
}

Value eval_file_exist(Eval *ev, const char *path) {
    struct stat st;
    (void)ev;
    return val_bool(stat(path, &st) == 0);
}

Value eval_file_delete(Eval *ev, const char *path, Node *site) {
    if (remove(path) != 0) {
        int err = errno;
        return eval_raise_class(ev, site, errno_class_name(err), "%s - %s", strerror(err), path);
    }
    return val_int(1);
}

Value eval_file_touch_mode(Eval *ev, const char *path, const char *mode, Node *site) {
    const char *fmode = NULL;
    if (mode[0] == 'r') {
        if (!val_truthy(eval_file_exist(ev, path))) {
            int err = errno;
            return eval_raise_class(ev, site, errno_class_name(err), "%s - %s", strerror(err), path);
        }
        return val_nil();
    }
    if (mode[0] == 'w') fmode = strchr(mode, 'b') ? "wb" : "wb";
    else if (mode[0] == 'a') fmode = strchr(mode, 'b') ? "ab" : "ab";
    else return eval_raise_class(ev, site, "ArgumentError", "unsupported File.open mode -- %s", mode);

    FILE *f = fopen(path, fmode);
    if (!f) {
        int err = errno;
        return eval_raise_class(ev, site, errno_class_name(err), "%s - %s", strerror(err), path);
    }
    fclose(f);
    return val_nil();
}

Value eval_require(Eval *ev, Env *env, const char *path, Node *site) {
    (void)env;
    const char *resolved = NULL;

    if (strcmp(path, "singleton") == 0 || strcmp(path, "singleton.rb") == 0)
        return val_true();
    if (strcmp(path, "prism") == 0 || strcmp(path, "prism.rb") == 0) {
        static const char *prism_shim =
"module Prism\n"
"  class Source\n"
"    def initialize(src); @source = src.to_s; end\n"
"    def source; @source.empty? ? \" \" : @source; end\n"
"    def lines; source.lines; end\n"
"  end\n"
"  class Location\n"
"    attr_accessor :start_line, :end_line, :start_column, :end_column, :start_offset, :end_offset\n"
"    def initialize(sl=1,el=1,sc=0,ec=0,so=0,eo=0)\n"
"      @start_line=sl; @end_line=el; @start_column=sc; @end_column=ec\n"
"      @start_offset=so; @end_offset=eo\n"
"    end\n"
"  end\n"
"  class Token\n"
"    attr_accessor :type, :location, :value\n"
"    def initialize(type, location, value=nil)\n"
"      @type=type; @location=location; @value=value\n"
"    end\n"
"  end\n"
"  class StubNode\n"
"    def accept(v); end\n"
"    def statements; self; end\n"
"    def body; []; end\n"
"    def last; nil; end\n"
"  end\n"
"  class ParseLexResult\n"
"    attr_reader :value, :source\n"
"    def initialize(code)\n"
"      @source = Source.new(code)\n"
"      @value = [StubNode.new, []]\n"
"    end\n"
"    def success?; true; end\n"
"    def errors; []; end\n"
"    def warnings; []; end\n"
"  end\n"
"  class ParseResult\n"
"    attr_reader :source\n"
"    def initialize(code)\n"
"      @source = Source.new(code)\n"
"      @node = StubNode.new\n"
"    end\n"
"    def value; @node; end\n"
"    def success?; true; end\n"
"    def errors; []; end\n"
"    def warnings; []; end\n"
"  end\n"
"  class LexResult\n"
"    def initialize(code)\n"
"      @code = code.to_s\n"
"      @open_count = _count_opens(@code)\n"
"    end\n"
"    def success?; @open_count <= 0; end\n"
"    def continuable?; @open_count > 0; end\n"
"    def errors; []; end\n"
"    def _count_opens(code)\n"
"      # Count unmatched block openers vs end closers\n"
"      depth = 0\n"
"      in_string = false\n"
"      in_comment = false\n"
"      tokens = code.scan(/['\"]|#|\\bdo\\b|\\bbegin\\b|\\bif\\b|\\bunless\\b|\\bwhile\\b|\\buntil\\b|\\bfor\\b|\\bclass\\b|\\bmodule\\b|\\bdef\\b|\\bcase\\b|\\bend\\b|\\n/)\n"
"      i = 0\n"
"      while i < tokens.size\n"
"        tok = tokens[i]\n"
"        if in_comment\n"
"          in_comment = false if tok == \"\\n\"\n"
"        elsif in_string\n"
"          in_string = false if tok == in_string\n"
"        elsif tok == '\"' || tok == \"'\"\n"
"          in_string = tok\n"
"        elsif tok == '#'\n"
"          in_comment = true\n"
"        elsif tok == 'end'\n"
"          depth -= 1\n"
"        elsif tok =~ /\\A(begin|class|module|def|do)\\z/\n"
"          depth += 1\n"
"        elsif tok =~ /\\A(if|unless|while|until|for|case)\\z/\n"
"          # Only count as block opener if it appears to be a statement form\n"
"          # (not a modifier form like 'x if cond')\n"
"          depth += 1\n"
"        end\n"
"        i += 1\n"
"      end\n"
"      depth\n"
"    end\n"
"  end\n"
"  class Visitor\n"
"    def visit(node); node&.accept(self); end\n"
"    def method_missing(name, *args); nil; end\n"
"  end\n"
"  def self.parse_lex(code, **opts); ParseLexResult.new(code.to_s); end\n"
"  def self.lex(code, **opts); LexResult.new(code.to_s); end\n"
"  def self.parse(code, **opts); ParseResult.new(code.to_s); end\n"
"end\n"
/* Patch execute_as_command? — constant lookup in the original method body
   fails due to closure scope; redefine with identical logic in fresh scope */
;
        return eval_ruby_string(ev, prism_shim, "prism_shim", site);
    }
    if (strcmp(path, "reline") == 0 || strcmp(path, "reline.rb") == 0)
        return val_true();
    if (strcmp(path, "pathname") == 0 || strcmp(path, "pathname.rb") == 0)
        return val_true();
    if (strcmp(path, "io/console") == 0 || strcmp(path, "io/console.rb") == 0)
        return val_true();
    if (strcmp(path, "io/console/size") == 0 || strcmp(path, "io/console/size.rb") == 0)
        return val_true();
    if (strcmp(path, "shellwords") == 0 || strcmp(path, "shellwords.rb") == 0)
        return val_true();
    if (strcmp(path, "stringio") == 0 || strcmp(path, "stringio.rb") == 0)
        return val_true();
    if (strcmp(path, "open3") == 0 || strcmp(path, "open3.rb") == 0)
        return val_true();
    if (strcmp(path, "pp") == 0 || strcmp(path, "pp.rb") == 0) {
        static const char *pp_shim =
"class PP\n"
"  def initialize(out = $stdout, width = 79, colorize: false)\n"
"    @out = out; @width = width\n"
"  end\n"
"  def guard_inspect_key; yield; end\n"
"  def pp(obj)\n"
"    @out << obj.inspect if @out.respond_to?(:<<)\n"
"  end\n"
"  def flush; end\n"
"  def self.pp(obj, out = $stdout, width = 79)\n"
"    out << obj.inspect\n"
"    out << \"\\n\"\n"
"    obj\n"
"  end\n"
"end\n";
        return eval_ruby_string(ev, pp_shim, "pp_shim", site);
    }
    if (strcmp(path, "color_printer") == 0 || strcmp(path, "color_printer.rb") == 0)
        return val_true();
    if (strcmp(path, "json") == 0 || strcmp(path, "json.rb") == 0)
        return val_true();
    if (strcmp(path, "cgi") == 0 || strcmp(path, "cgi.rb") == 0)
        return val_true();
    if (strcmp(path, "yaml") == 0 || strcmp(path, "yaml.rb") == 0)
        return val_true();
    if (strcmp(path, "set") == 0 || strcmp(path, "set.rb") == 0)
        return val_true();
    if (strcmp(path, "uri") == 0 || strcmp(path, "uri.rb") == 0)
        return val_true();
    if (strcmp(path, "date") == 0 || strcmp(path, "date.rb") == 0)
        return val_true();
    if (strcmp(path, "logger") == 0 || strcmp(path, "logger.rb") == 0)
        return val_true();
    if (strcmp(path, "digest") == 0 || strcmp(path, "digest.rb") == 0)
        return val_true();
    if (strcmp(path, "base64") == 0 || strcmp(path, "base64.rb") == 0)
        return val_true();
    if (strcmp(path, "observer") == 0 || strcmp(path, "observer.rb") == 0)
        return val_true();
    if (strcmp(path, "forwardable") == 0 || strcmp(path, "forwardable.rb") == 0)
        return val_true();
    if (strcmp(path, "ostruct") == 0 || strcmp(path, "ostruct.rb") == 0) {
        static const char *ostruct_shim =
"class OpenStruct\n"
"  def initialize(hash = {})\n"
"    @table = hash.to_h\n"
"  end\n"
"  def [](key)\n"
"    @table[key.to_sym]\n"
"  end\n"
"  def []=(key, val)\n"
"    @table[key.to_sym] = val\n"
"    self\n"
"  end\n"
"  def to_h; @table.dup; end\n"
"  def respond_to_missing?(name, include_private = false)\n"
"    @table.key?(name.to_s.chomp('=').to_sym) || super\n"
"  end\n"
"  def method_missing(name, *args)\n"
"    n = name.to_s\n"
"    if n.end_with?('=')\n"
"      @table[n.chomp('=').to_sym] = args[0]\n"
"    else\n"
"      @table[name]\n"
"    end\n"
"  end\n"
"  def inspect\n"
"    parts = @table.map{|k,v| \"#{k}=#{v.inspect}\"}.join(\", \")\n"
"    \"#<OpenStruct #{parts}>\"\n"
"  end\n"
"  def to_s; inspect; end\n"
"end\n";
        return eval_ruby_string(ev, ostruct_shim, "ostruct_shim", site);
    }
    if (strcmp(path, "tmpdir") == 0 || strcmp(path, "tmpdir.rb") == 0)
        return val_true();
    if (strcmp(path, "tempfile") == 0 || strcmp(path, "tempfile.rb") == 0)
        return val_true();

    if (ev->current_file)
        resolved = resolve_require_path(ev->arena, ev->current_file, path, 0);
    if (resolved) {
        FILE *f = fopen(resolved, "rb");
        if (f) {
            fclose(f);
            return eval_require_path(ev, resolved, resolved, site);
        }
    }

    Value load_path = eval_load_path(ev);
    if (load_path.kind == VAL_ARRAY) {
        for (size_t i = 0; i < load_path.array->len; i++) {
            Value entry = load_path.array->elems[i];
            if (entry.kind != VAL_STRING) continue;
            resolved = resolve_require_path(ev->arena, entry.sval, path, 1);
            if (!resolved) continue;
            FILE *f = fopen(resolved, "rb");
            if (f) {
                fclose(f);
                return eval_require_path(ev, resolved, resolved, site);
            }
        }
    }

    return eval_raise_class(ev, site, "LoadError", "cannot load such file -- %s", path);
}
