#define _XOPEN_SOURCE 700

#include "eval_internal.h"
#include "parser.h"
#include "sema.h"

#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
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

        /* Named reference: %{key} — look up key in the hash argument */
        if (fmt[i] == '{') {
            const char *kstart = fmt + i + 1;
            const char *kend = strchr(kstart, '}');
            if (!kend) {
                free(buf);
                return eval_raise_class(ev, site, "ArgumentError", "malformed named reference: unclosed '{'");
            }
            size_t klen = (size_t)(kend - kstart);
            char key[256];
            if (klen >= sizeof(key)) klen = sizeof(key) - 1;
            memcpy(key, kstart, klen); key[klen] = '\0';
            /* The single argument must be a hash */
            Value hash_arg = (argc == 1 && args[0].kind == VAL_HASH) ? args[0] : val_nil();
            char *key_copy = arena_alloc(ev->arena, klen + 1);
            memcpy(key_copy, key, klen + 1);
            Value sym_key = val_symbol(key_copy);
            Value str_key = val_string(ev->arena, key);
            Value val = val_nil();
            if (hash_arg.kind == VAL_HASH) {
                if (!val_hash_get(hash_arg.hash, sym_key, &val))
                    val_hash_get(hash_arg.hash, str_key, &val);
            }
            const char *sv = val_to_s(ev->arena, val);
            if (!append_dynamic(&buf, &cap, &used, sv, strlen(sv))) {
                free(buf); return eval_raise_class(ev, site, "RuntimeError", "out of memory");
            }
            i = (size_t)(kend - fmt);  /* skip to '}' */
            continue;
        }

        /* Named reference with type: %<key>type — look up key, then format with type */
        if (fmt[i] == '<') {
            const char *kstart = fmt + i + 1;
            const char *kend = strchr(kstart, '>');
            if (!kend) {
                free(buf); return eval_raise_class(ev, site, "ArgumentError", "malformed named reference: unclosed '<'");
            }
            size_t klen = (size_t)(kend - kstart);
            char key[256]; if (klen >= sizeof(key)) klen = sizeof(key) - 1;
            memcpy(key, kstart, klen); key[klen] = '\0';
            char *key_copy = arena_alloc(ev->arena, klen + 1);
            memcpy(key_copy, key, klen + 1);
            Value hash_arg = (argc == 1 && args[0].kind == VAL_HASH) ? args[0] : val_nil();
            Value sym_key = val_symbol(key_copy);
            Value str_key = val_string(ev->arena, key);
            Value val = val_nil();
            if (hash_arg.kind == VAL_HASH) {
                if (!val_hash_get(hash_arg.hash, sym_key, &val))
                    val_hash_get(hash_arg.hash, str_key, &val);
            }
            i = (size_t)(kend - fmt) + 1; /* now at type char */
            /* Now parse the type specifier and format the value */
            Value fake_args[1] = { val };
            /* Build a mini format string from the type char */
            char mini_fmt[4] = { '%', fmt[i], '\0', '\0' };
            /* i stays at type char position; outer for-loop will do i++ */
            Value mini_result = eval_format_string(ev, env, mini_fmt, fake_args, 1, site);
            if (val_is_signal(mini_result)) { free(buf); return mini_result; }
            const char *sv = mini_result.kind == VAL_STRING ? mini_result.sval : val_to_s(ev->arena, val);
            if (!append_dynamic(&buf, &cap, &used, sv, strlen(sv))) {
                free(buf); return eval_raise_class(ev, site, "RuntimeError", "out of memory");
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
                /* Binary — C doesn't have %b, implement with width/pad support */
                int64_t n = v.kind == VAL_INT ? v.ival : 0;
                char bitbuf[70]; int bi = 0;
                if (n == 0) { bitbuf[bi++] = '0'; }
                else {
                    uint64_t un = (uint64_t)n;
                    while (un) { bitbuf[bi++] = (un & 1) ? '1' : '0'; un >>= 1; }
                    for (int li=0, ri=bi-1; li<ri; li++,ri--) { char c=bitbuf[li]; bitbuf[li]=bitbuf[ri]; bitbuf[ri]=c; }
                }
                bitbuf[bi] = '\0';
                /* Extract width and padding char from spec */
                int width = 0; char pad_char = ' '; int left_align = 0;
                const char *sp = spec + 1; /* skip leading % */
                if (*sp == '-') { left_align = 1; sp++; }
                if (*sp == '0') { pad_char = '0'; sp++; }
                while (*sp >= '0' && *sp <= '9') { width = width * 10 + (*sp - '0'); sp++; }
                if (width > 0 && bi < width) {
                    int pad = width - bi;
                    if (left_align) {
                        memcpy(tmp, bitbuf, (size_t)bi);
                        memset(tmp + bi, ' ', (size_t)pad);
                        tmp[bi + pad] = '\0';
                    } else {
                        memset(tmp, pad_char, (size_t)pad);
                        memcpy(tmp + pad, bitbuf, (size_t)bi + 1);
                    }
                    piece = tmp; piece_len = (size_t)width;
                } else {
                    piece = bitbuf; piece_len = (size_t)bi;
                }
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

Value eval_ruby_string(Eval *ev, const char *src, const char *display_name, Node *site) {
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

static Value eval_ruby_string_parts(Eval *ev, const char **parts, size_t part_count,
                                    const char *display_name, Node *site) {
    size_t cap = 0;
    size_t used = 0;
    char *src = NULL;
    for (size_t i = 0; i < part_count; i++) {
        if (!append_dynamic(&src, &cap, &used, parts[i], strlen(parts[i]))) {
            free(src);
            return eval_raise_class(ev, site, "RuntimeError", "out of memory");
        }
    }
    Value result = eval_ruby_string(ev, src ? src : "", display_name, site);
    free(src);
    return result;
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
    /* After loading irb.rb: patch execute_as_command? and inspector */
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
                "end\n"
                /* Replace the pp/stream inspector with a simple inspect-based one.
                   Our string value semantics can't support the streaming ColorPrinter
                   path (ColorPrinter.pp writes to @out ivar, caller's `out` local
                   doesn't see the mutation). A plain v.inspect proc sidesteps this. */
                "begin\n"
                "  _si = IRB::Inspector.new(proc { |v, colorize: true| v.inspect })\n"
                "  IRB::Inspector::INSPECTORS[true]           = _si\n"
                "  IRB::Inspector::INSPECTORS[:pp]            = _si\n"
                "  IRB::Inspector::INSPECTORS[:pretty_inspect] = _si\n"
                "  IRB::Inspector::INSPECTORS['true']         = _si\n"
                "  IRB::Inspector::INSPECTORS['pp']           = _si\n"
                "rescue => _e\n"
                "end\n"
                /* RubyVM stub — birb references keep_script_lines around the REPL loop */
                "begin\n"
                "  module RubyVM\n"
                "    def self.keep_script_lines; false; end\n"
                "    def self.keep_script_lines=(v); v; end\n"
                "  end\n"
                "rescue => _e\n"
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
        if (strcmp(visibility.sval, "module_function") == 0) return METHOD_PRIVATE;
    }
    return METHOD_PUBLIC;
}

int is_module_function_mode(Env *env) {
    Value visibility;
    return env_get(env, "__visibility__", &visibility) &&
           visibility.kind == VAL_SYMBOL &&
           strcmp(visibility.sval, "module_function") == 0;
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
        if (env_get_own(k->class_env, name, out)) {
            if (out->kind == VAL_METHOD) {
                if (owner) *owner = k;
                return 1;
            }
            if (out->kind == VAL_UNDEF_METHOD)
                return 1; /* blocked — caller must check kind */
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

static int ruby_class_get_direct_class_method(RubyClass *klass, const char *name, Value *out) {
    if (!klass || !klass->class_env || !name) return 0;
    for (EnvEntry *entry = klass->class_env->vars; entry; entry = entry->next) {
        if (!entry->name || strncmp(entry->name, "self.", 5) != 0) continue;
        if (strcmp(entry->name + 5, name) == 0 && entry->val.kind == VAL_METHOD) {
            *out = entry->val;
            return 1;
        }
    }
    return 0;
}

int ruby_class_find_class_method(RubyClass *klass, const char *name, Value *out, RubyClass **owner) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        if (ruby_class_get_direct_class_method(k, name, out)) {
            if (owner) *owner = k;
            return 1;
        }
        for (RubyModuleInclusion *inc = k->extended_modules; inc; inc = inc->next) {
            RubyClass *module_owner = NULL;
            if (ruby_class_find_instance_method(inc->mod, name, out, &module_owner)) {
                if (owner) *owner = module_owner;
                return 1;
            }
        }
    }
    return 0;
}

static int ruby_class_find_class_super_method_inner(RubyClass *klass, RubyClass *after,
                                                    int *seen_after, const char *name,
                                                    Value *out, RubyClass **owner) {
    for (RubyClass *k = klass; k; k = k->superclass.kind == VAL_CLASS ? k->superclass.klass : NULL) {
        if (*seen_after && ruby_class_get_direct_class_method(k, name, out)) {
            if (owner) *owner = k;
            return 1;
        }
        if (k == after) *seen_after = 1;

        for (RubyModuleInclusion *inc = k->extended_modules; inc; inc = inc->next) {
            if (*seen_after) {
                RubyClass *module_owner = NULL;
                if (ruby_class_find_instance_method(inc->mod, name, out, &module_owner)) {
                    if (owner) *owner = module_owner;
                    return 1;
                }
            } else if (ruby_class_find_super_method_inner(inc->mod, after, seen_after, name, out, owner)) {
                return 1;
            }
        }
    }
    return 0;
}

int ruby_class_find_class_super_method(RubyClass *start, RubyClass *after, const char *name, Value *out, RubyClass **owner) {
    int seen_after = 0;
    return ruby_class_find_class_super_method_inner(start, after, &seen_after, name, out, owner);
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
        /* Set __cause__ if inside a rescue block (rescue_context holds the caught exception) */
        if (ev->rescue_context.kind == VAL_OBJECT)
            val_object_set_ivar(ev->arena, ev->current_exception, "__cause__", ev->rescue_context);
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
        /* Set __cause__ if inside a rescue block */
        if (ev->rescue_context.kind == VAL_OBJECT && exc.kind == VAL_OBJECT) {
            Value existing_cause = val_nil();
            if (!val_object_get_ivar(exc, "__cause__", &existing_cause) || existing_cause.kind == VAL_NIL)
                val_object_set_ivar(ev->arena, exc, "__cause__", ev->rescue_context);
        }
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
            /* For objects, dispatch Ruby-defined to_s before falling back. */
            const char *s;
            if (v.kind == VAL_OBJECT || v.kind == VAL_CLASS) {
                Value ts = dispatch_method(ev, env, v, "to_s", NULL, 0, NULL, NULL, 0, 1);
                if (!val_is_signal(ts) && ts.kind == VAL_STRING && ts.sval)
                    s = ts.sval;
                else
                    s = val_to_s(ev->arena, v);
            } else {
                s = val_to_s(ev->arena, v);
            }
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
            env_define(ev->arena, env, pattern->param.name, val);
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
            /* Required positional: not splat, not block, not keyword (any), not keyword-splat,
               no default. Keyword params (bar:) are validated separately by bind_params. */
            if (!p->param.splat && !p->param.block_param && !p->param.default_val
                && !p->param.keyword_splat && !p->param.keyword_param)
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
        env_define(ev->arena, env, kname, kval);
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
        if (p->param.name) env_define(ev->arena, env, p->param.name, rest_hash);
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
    Value closure_self = val_nil();
    if (env_get(closure, "self", &closure_self))
        env_define(ev->arena, frame, "self", closure_self);
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
    if (strcmp(path, "reline") == 0 || strcmp(path, "reline.rb") == 0) {
        static const char *reline_shim =
"module Reline\n"
"  HISTORY = []\n"
"  DEFAULT_DIALOG_CONTEXT = []\n"
"  def self.get_screen_size; [24, 80]; end\n"
"  def self.readmultiline(prompt = '', add_hist = false)\n"
"    $stdout.print(prompt) if prompt && !prompt.empty?\n"
"    line = $stdin.gets\n"
"    return nil if line.nil?\n"
"    yield(line) if block_given?\n"
"    line\n"
"  end\n"
"  def self.input; $stdin; end\n"
"  def self.output; $stdout; end\n"
"  def self.input=(v); v; end\n"
"  def self.output=(v); v; end\n"
"  def self.completion_proc; nil; end\n"
"  def self.completion_proc=(v); v; end\n"
"  def self.completion_append_character; nil; end\n"
"  def self.completion_append_character=(v); v; end\n"
"  def self.basic_word_break_characters; \" \\t\\n\\\"'`><=;|&{\"; end\n"
"  def self.basic_word_break_characters=(v); v; end\n"
"  def self.completer_quote_characters; '\"\\'' ; end\n"
"  def self.completer_quote_characters=(v); v; end\n"
"  def self.output_modifier_proc; nil; end\n"
"  def self.output_modifier_proc=(v); v; end\n"
"  def self.prompt_proc; nil; end\n"
"  def self.prompt_proc=(v); v; end\n"
"  def self.auto_indent_proc; nil; end\n"
"  def self.auto_indent_proc=(v); v; end\n"
"  def self.autocompletion; false; end\n"
"  def self.autocompletion=(v); v; end\n"
"  def self.add_dialog_proc(*a); nil; end\n"
"  def self.dig_perfect_match_proc; nil; end\n"
"  def self.dig_perfect_match_proc=(v); v; end\n"
"  def self.delete_text; nil; end\n"
"  def self.ungetc(c); nil; end\n"
"  def self.encoding_system_needs; Encoding.find('UTF-8'); end\n"
"  module IOGate\n"
"    def self.in_pasting?; false; end\n"
"    def self.prep; nil; end\n"
"    def self.deprep(old); nil; end\n"
"    def self.set_winch_handler(&blk); nil; end\n"
"    def self.ttyname; nil; end\n"
"    CursorPos = Struct.new(:x, :y)\n"
"  end\n"
"  module Unicode\n"
"    def self.escape_for_print(str); str.to_s; end\n"
"    def self.calculate_width(str, ambiguous_double_width = false)\n"
"      str.to_s.length\n"
"    end\n"
"    def self.split_by_width(str, max_width, encoding = nil)\n"
"      s = str.to_s\n"
"      return [[s], 0] if s.length <= max_width\n"
"      [[s[0, max_width], s[max_width..]], 0]\n"
"    end\n"
"  end\n"
"  CursorPos = Struct.new(:x, :y)\n"
"  Config = Class.new\n"
"end\n";
        return eval_ruby_string(ev, reline_shim, "reline_shim", site);
    }
    if (strcmp(path, "pathname") == 0 || strcmp(path, "pathname.rb") == 0)
        return val_true();
    if (strcmp(path, "io/console") == 0 || strcmp(path, "io/console.rb") == 0)
        return val_true();
    if (strcmp(path, "io/console/size") == 0 || strcmp(path, "io/console/size.rb") == 0)
        return val_true();
    if (strcmp(path, "shellwords") == 0 || strcmp(path, "shellwords.rb") == 0)
        return val_true();
    if (strcmp(path, "stringio") == 0 || strcmp(path, "stringio.rb") == 0) {
        static const char *sio_shim =
"class StringIO\n"
"  def initialize(str = '', mode = 'r+')\n"
"    @buf = str.to_s.dup\n"
"    @pos = (mode.include?('a') ? @buf.length : 0)\n"
"    @mode = mode\n"
"  end\n"
"  def string; @buf; end\n"
"  def string=(s); @buf = s.to_s; @pos = 0; end\n"
"  def pos; @pos; end\n"
"  def tell; @pos; end\n"
"  def pos=(n); @pos = n.to_i; end\n"
"  def seek(n, whence = 0)\n"
"    @pos = case whence\n"
"           when 0 then n\n"
"           when 1 then @pos + n\n"
"           when 2 then @buf.length + n\n"
"           else n\n"
"           end\n"
"    @pos = 0 if @pos < 0\n"
"    0\n"
"  end\n"
"  def rewind; @pos = 0; 0; end\n"
"  def eof?; @pos >= @buf.length; end\n"
"  def eof; eof?; end\n"
"  def read(n = nil)\n"
"    return nil if eof? && !n.nil?\n"
"    if n.nil?\n"
"      s = @buf[@pos..] || ''\n"
"      @pos = @buf.length\n"
"      s\n"
"    else\n"
"      s = @buf[@pos, n] || ''\n"
"      @pos += s.length\n"
"      s.empty? ? nil : s\n"
"    end\n"
"  end\n"
"  def gets(sep = $/, limit = nil)\n"
"    return nil if eof?\n"
"    rest = @buf[@pos..] || ''\n"
"    if sep.nil?\n"
"      line = rest\n"
"    elsif sep == ''\n"
"      idx = rest.index(\"\\n\\n\")\n"
"      line = idx ? rest[0, idx + 2] : rest\n"
"    else\n"
"      idx = rest.index(sep)\n"
"      line = idx ? rest[0, idx + sep.length] : rest\n"
"    end\n"
"    line = line[0, limit] if limit\n"
"    @pos += line.length\n"
"    line\n"
"  end\n"
"  def readline(sep = $/)\n"
"    s = gets(sep)\n"
"    raise EOFError, 'end of file reached' if s.nil?\n"
"    s\n"
"  end\n"
"  def readlines(sep = $/)\n"
"    lines = []\n"
"    while (line = gets(sep)); lines << line; end\n"
"    lines\n"
"  end\n"
"  def each_line(sep = $/, &blk)\n"
"    return to_enum(:each_line, sep) unless block_given?\n"
"    while (line = gets(sep)); blk.call(line); end\n"
"    self\n"
"  end\n"
"  alias each each_line\n"
"  alias lines readlines\n"
"  def getc\n"
"    return nil if eof?\n"
"    c = @buf[@pos]\n"
"    @pos += 1\n"
"    c\n"
"  end\n"
"  def readchar\n"
"    c = getc\n"
"    raise EOFError, 'end of file reached' if c.nil?\n"
"    c\n"
"  end\n"
"  def ungetc(c); @pos -= 1 if @pos > 0; end\n"
"  def write(s); t = s.to_s; @buf[@pos, t.length] = t; @pos += t.length; t.length; end\n"
"  def print(*args); args.each { |a| write(a.to_s) }; nil; end\n"
"  def puts(*args)\n"
"    if args.empty?; write(\"\\n\")\n"
"    else; args.each { |a| s = a.to_s; write(s); write(\"\\n\") unless s.end_with?(\"\\n\") }; end\n"
"    nil\n"
"  end\n"
"  def <<(s); write(s.to_s); self; end\n"
"  def truncate(n = 0); @buf = @buf[0, n] || ''; @pos = [@pos, @buf.length].min; 0; end\n"
"  def size; @buf.size; end\n"
"  def length; @buf.length; end\n"
"  def to_s; @buf; end\n"
"  def inspect; \"#<StringIO>\"; end\n"
"  def flush; self; end\n"
"  def sync; true; end\n"
"  def sync=(v); v; end\n"
"  def tty?; false; end\n"
"  def isatty; false; end\n"
"  def closed?; false; end\n"
"  def close; end\n"
"  def fileno; raise IOError, 'StringIO has no fileno'; end\n"
"  def external_encoding; Encoding::UTF_8 rescue nil; end\n"
"  def internal_encoding; nil; end\n"
"  def set_encoding(enc, *); self; end\n"
"end\n";
        return eval_ruby_string(ev, sio_shim, "stringio_shim", site);
    }
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
    if (strcmp(path, "json") == 0 || strcmp(path, "json.rb") == 0 ||
        strcmp(path, "json/common") == 0 || strcmp(path, "json/pure") == 0) {
        static const char *json_shim =
"module JSON\n"
"  class ParseError < StandardError; end\n"
"  class GeneratorError < StandardError; end\n"
"  class Parser\n"
"    def initialize(str)\n"
"      @s = str.to_s\n"
"      @p = 0\n"
"    end\n"
"    def parse\n"
"      ws; v = val; ws\n"
"      raise JSON::ParseError, \"trailing garbage at #{@p}\" unless @p >= @s.length\n"
"      v\n"
"    end\n"
"    private\n"
"    def ws; @p += 1 while @p < @s.length && \" \\t\\n\\r\".include?(@s[@p]); end\n"
"    def val\n"
"      ws\n"
"      c = @s[@p]\n"
"      case c\n"
"      when '{' then obj\n"
"      when '[' then arr\n"
"      when '\"' then str\n"
"      when 't' then eat('true'); true\n"
"      when 'f' then eat('false'); false\n"
"      when 'n' then eat('null'); nil\n"
"      when '-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' then num\n"
"      else raise JSON::ParseError, \"unexpected #{c.inspect} at #{@p}\"\n"
"      end\n"
"    end\n"
"    def eat(s)\n"
"      raise JSON::ParseError, \"expected #{s.inspect}\" unless @s[@p, s.length] == s\n"
"      @p += s.length\n"
"    end\n"
"    def obj\n"
"      @p += 1; ws\n"
"      h = {}\n"
"      unless @s[@p] == '}'\n"
"        loop do\n"
"          ws; raise JSON::ParseError, 'expected string key' unless @s[@p] == '\"'\n"
"          k = str; ws\n"
"          raise JSON::ParseError, \"expected ':'\" unless @s[@p] == ':'\n"
"          @p += 1\n"
"          h[k] = val; ws\n"
"          break if @s[@p] == '}'\n"
"          raise JSON::ParseError, \"expected ',' or '}'\" unless @s[@p] == ','\n"
"          @p += 1\n"
"        end\n"
"      end\n"
"      @p += 1; h\n"
"    end\n"
"    def arr\n"
"      @p += 1; ws\n"
"      a = []\n"
"      unless @s[@p] == ']'\n"
"        loop do\n"
"          a << val; ws\n"
"          break if @s[@p] == ']'\n"
"          raise JSON::ParseError, \"expected ',' or ']'\" unless @s[@p] == ','\n"
"          @p += 1\n"
"        end\n"
"      end\n"
"      @p += 1; a\n"
"    end\n"
"    def str\n"
"      @p += 1\n"
"      result = ''\n"
"      while @p < @s.length && @s[@p] != '\"'\n"
"        if @s[@p] == '\\\\'\n"
"          @p += 1\n"
"          case @s[@p]\n"
"          when '\"' then result += '\"'\n"
"          when '\\\\' then result += '\\\\'\n"
"          when '/' then result += '/'\n"
"          when 'n' then result += \"\\n\"\n"
"          when 'r' then result += \"\\r\"\n"
"          when 't' then result += \"\\t\"\n"
"          when 'b' then result += \"\\b\"\n"
"          when 'f' then result += \"\\f\"\n"
"          when 'u'\n"
"            code = @s[@p+1, 4].to_i(16)\n"
"            result += code.chr rescue '?'\n"
"            @p += 4\n"
"          else result += @s[@p]\n"
"          end\n"
"        else\n"
"          result += @s[@p]\n"
"        end\n"
"        @p += 1\n"
"      end\n"
"      @p += 1\n"
"      result\n"
"    end\n"
"    def num\n"
"      s = @p\n"
"      @p += 1 if @s[@p] == '-'\n"
"      @p += 1 while @p < @s.length && @s[@p] >= '0' && @s[@p] <= '9'\n"
"      is_float = false\n"
"      if @p < @s.length && @s[@p] == '.'\n"
"        is_float = true; @p += 1\n"
"        @p += 1 while @p < @s.length && @s[@p] >= '0' && @s[@p] <= '9'\n"
"      end\n"
"      if @p < @s.length && (@s[@p] == 'e' || @s[@p] == 'E')\n"
"        is_float = true; @p += 1\n"
"        @p += 1 if @p < @s.length && (@s[@p] == '+' || @s[@p] == '-')\n"
"        @p += 1 while @p < @s.length && @s[@p] >= '0' && @s[@p] <= '9'\n"
"      end\n"
"      t = @s[s, @p - s]\n"
"      is_float ? t.to_f : t.to_i\n"
"    end\n"
"  end\n"
"  def self.parse(str, opts = {})\n"
"    Parser.new(str).parse\n"
"  end\n"
"  def self.[](str)\n"
"    Parser.new(str).parse\n"
"  end\n"
"  def self.generate(obj, opts = {})\n"
"    _gen(obj)\n"
"  end\n"
"  def self.dump(obj, opts = {})\n"
"    generate(obj, opts)\n"
"  end\n"
"  def self.load(str, opts = {})\n"
"    parse(str, opts)\n"
"  end\n"
"  def self.pretty_generate(obj, opts = {})\n"
"    _pretty(obj, opts[:indent] || '  ', 0)\n"
"  end\n"
"  def self._gen(o)\n"
"    case o\n"
"    when Hash\n"
"      pairs = o.map { |k, v| _gen(k.to_s) + ':' + _gen(v) }\n"
"      '{' + pairs.join(',') + '}'\n"
"    when Array\n"
"      '[' + o.map { |v| _gen(v) }.join(',') + ']'\n"
"    when String\n"
"      '\"' + o.gsub('\\\\', '\\\\\\\\').gsub('\"', '\\\\\"').gsub(\"\\n\", '\\\\n').gsub(\"\\r\", '\\\\r').gsub(\"\\t\", '\\\\t') + '\"'\n"
"    when Integer, Float\n"
"      o.to_s\n"
"    when true, false\n"
"      o.to_s\n"
"    when nil\n"
"      'null'\n"
"    else\n"
"      _gen(o.to_s)\n"
"    end\n"
"  end\n"
"  def self._pretty(o, indent, depth)\n"
"    pad = indent * depth\n"
"    inner = indent * (depth + 1)\n"
"    case o\n"
"    when Hash\n"
"      return '{}' if o.empty?\n"
"      pairs = o.map { |k, v| inner + _gen(k.to_s) + ': ' + _pretty(v, indent, depth + 1) }\n"
"      \"{\\n\" + pairs.join(\",\\n\") + \"\\n\" + pad + \"}\"\n"
"    when Array\n"
"      return '[]' if o.empty?\n"
"      items = o.map { |v| inner + _pretty(v, indent, depth + 1) }\n"
"      \"[\\n\" + items.join(\",\\n\") + \"\\n\" + pad + \"]\"\n"
"    else\n"
"      _gen(o)\n"
"    end\n"
"  end\n"
"end\n";
        return eval_ruby_string(ev, json_shim, "json_shim", site);
    }
    if (strcmp(path, "cgi") == 0 || strcmp(path, "cgi.rb") == 0)
        return val_true();
    if (strcmp(path, "yaml") == 0 || strcmp(path, "yaml.rb") == 0)
        return val_true();
    if (strcmp(path, "set") == 0 || strcmp(path, "set.rb") == 0) {
        static const char *set_shim =
"class Set\n"
"  include Enumerable\n"
"  def initialize(enum = nil)\n"
"    @h = {}\n"
"    enum.each { |v| @h[v] = true } if enum\n"
"  end\n"
"  def add(v); @h[v] = true; self; end\n"
"  alias << add\n"
"  alias add? add\n"
"  def delete(v); @h.delete(v); self; end\n"
"  alias delete? delete\n"
"  def include?(v); @h.key?(v); end\n"
"  alias member? include?\n"
"  alias === include?\n"
"  def size; @h.size; end\n"
"  alias length size\n"
"  def empty?; @h.empty?; end\n"
"  def each(&blk)\n"
"    return to_enum(:each) unless block_given?\n"
"    @h.each_key(&blk)\n"
"    self\n"
"  end\n"
"  def to_a; @h.keys; end\n"
"  alias entries to_a\n"
"  def to_set; self; end\n"
"  def merge(other); other.each { |v| add(v) }; self; end\n"
"  def intersection(other)\n"
"    result = Set.new\n"
"    each { |v| result.add(v) if other.include?(v) }\n"
"    result\n"
"  end\n"
"  alias & intersection\n"
"  def union(other)\n"
"    result = Set.new(to_a)\n"
"    other.each { |v| result.add(v) }\n"
"    result\n"
"  end\n"
"  alias | union\n"
"  alias + union\n"
"  def difference(other)\n"
"    result = Set.new\n"
"    each { |v| result.add(v) unless other.include?(v) }\n"
"    result\n"
"  end\n"
"  alias - difference\n"
"  def subset?(other); all? { |v| other.include?(v) }; end\n"
"  alias <= subset?\n"
"  def superset?(other); other.subset?(self); end\n"
"  alias >= superset?\n"
"  def proper_subset?(other); subset?(other) && size < other.size; end\n"
"  alias < proper_subset?\n"
"  def proper_superset?(other); other.proper_subset?(self); end\n"
"  alias > proper_superset?\n"
"  def ==(other)\n"
"    return false unless other.is_a?(Set)\n"
"    size == other.size && subset?(other)\n"
"  end\n"
"  def dup; Set.new(to_a); end\n"
"  def clear; @h.clear; self; end\n"
"  def select!(&blk); @h.delete_if { |k, _| !blk.call(k) }; self; end\n"
"  alias filter! select!\n"
"  def reject!(&blk); @h.delete_if { |k, _| blk.call(k) }; self; end\n"
"  def collect!(&blk)\n"
"    new_h = {}\n"
"    @h.each_key { |v| new_h[blk.call(v)] = true }\n"
"    @h = new_h\n"
"    self\n"
"  end\n"
"  alias map! collect!\n"
"  def flatten(n = nil); Set.new(to_a.flatten(n)); end\n"
"  def flatten!; @h = {}.tap { |h| to_a.flatten.each { |v| h[v] = true } }; self; end\n"
"  def inspect; \"#<Set: {#{to_a.map(&:inspect).join(', ')}}>\"; end\n"
"  def to_s; inspect; end\n"
"  def classify(&blk)\n"
"    result = {}\n"
"    each { |v| k = blk.call(v); (result[k] ||= Set.new).add(v) }\n"
"    result\n"
"  end\n"
"  def divide(&blk)\n"
"    if blk.arity == 1\n"
"      classify(&blk).values.to_set\n"
"    else\n"
"      r = Set.new\n"
"      todo = to_a.dup\n"
"      until todo.empty?\n"
"        v = todo.shift\n"
"        g = Set.new([v])\n"
"        todo.each { |w| g.add(w) if blk.call(v, w) }\n"
"        r.add(g)\n"
"      end\n"
"      r\n"
"    end\n"
"  end\n"
"end\n"
"class Array\n"
"  def to_set; Set.new(self); end\n"
"end\n";
        return eval_ruby_string(ev, set_shim, "set_shim", site);
    }
    if (strcmp(path, "uri") == 0 || strcmp(path, "uri.rb") == 0)
        return val_true();
    if (strcmp(path, "date") == 0 || strcmp(path, "date.rb") == 0) {
        static const char *date_shim =
"class Date\n"
"  include Comparable\n"
"  def initialize(y = -4712, m = 1, d = 1)\n"
"    y, m, d = y.to_i, m.to_i, d.to_i\n"
"    a = (14 - m) / 12\n"
"    y2 = y + 4800 - a\n"
"    m2 = m + 12 * a - 3\n"
"    @jd = d + (153 * m2 + 2) / 5 + 365 * y2 + y2 / 4 - y2 / 100 + y2 / 400 - 32045\n"
"  end\n"
"  def self._from_jd(jdn)\n"
"    a = jdn + 32044\n"
"    b = (4 * a + 3) / 146097\n"
"    c = a - (146097 * b) / 4\n"
"    d2 = (4 * c + 3) / 1461\n"
"    e = c - (1461 * d2) / 4\n"
"    m = (5 * e + 2) / 153\n"
"    d = e - (153 * m + 2) / 5 + 1\n"
"    mo = m + 3 - 12 * (m / 10)\n"
"    y = 100 * b + d2 - 4800 + m / 10\n"
"    new(y, mo, d)\n"
"  end\n"
"  def _ymd\n"
"    a = @jd + 32044\n"
"    b = (4 * a + 3) / 146097\n"
"    c = a - (146097 * b) / 4\n"
"    d2 = (4 * c + 3) / 1461\n"
"    e = c - (1461 * d2) / 4\n"
"    m = (5 * e + 2) / 153\n"
"    d = e - (153 * m + 2) / 5 + 1\n"
"    mo = m + 3 - 12 * (m / 10)\n"
"    y = 100 * b + d2 - 4800 + m / 10\n"
"    [y, mo, d]\n"
"  end\n"
"  def year;  _ymd[0]; end\n"
"  def month; _ymd[1]; end\n"
"  def mon;   _ymd[1]; end\n"
"  def day;   _ymd[2]; end\n"
"  def mday;  _ymd[2]; end\n"
"  def jd;    @jd; end\n"
"  def +(n);  Date._from_jd(@jd + n.to_i); end\n"
"  def -(other)\n"
"    other.is_a?(Date) ? @jd - other.jd : Date._from_jd(@jd - other.to_i)\n"
"  end\n"
"  def <=>(other); return nil unless other.is_a?(Date); @jd <=> other.jd; end\n"
"  def to_s\n"
"    y, m, d = _ymd\n"
"    format('%04d-%02d-%02d', y, m, d)\n"
"  end\n"
"  def inspect; \"#<Date: #{to_s}>\"; end\n"
"  def strftime(fmt = '%Y-%m-%d')\n"
"    y, m, d = _ymd\n"
"    wday_val = (@jd + 1) % 7\n"
"    SDAYS  = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat']\n"
"    DAYS   = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday']\n"
"    SMONS  = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec']\n"
"    MONTHS = ['January','February','March','April','May','June','July','August','September','October','November','December']\n"
"    fmt.gsub(/%[-]?[A-Za-z%]/) do |code|\n"
"      case code\n"
"      when '%Y' then format('%04d', y)\n"
"      when '%y' then format('%02d', y % 100)\n"
"      when '%m' then format('%02d', m)\n"
"      when '%-m' then m.to_s\n"
"      when '%d' then format('%02d', d)\n"
"      when '%-d' then d.to_s\n"
"      when '%e' then format('%2d', d)\n"
"      when '%j' then yday.to_s\n"
"      when '%A' then DAYS[wday_val]\n"
"      when '%a' then SDAYS[wday_val]\n"
"      when '%B' then MONTHS[m-1]\n"
"      when '%b', '%h' then SMONS[m-1]\n"
"      when '%n' then \"\\n\"\n"
"      when '%t' then \"\\t\"\n"
"      when '%%' then '%'\n"
"      else code\n"
"      end\n"
"    end\n"
"  end\n"
"  def yday\n"
"    y, m, d = _ymd\n"
"    base = Date.new(y, 1, 1)\n"
"    (@jd - base.jd) + 1\n"
"  end\n"
"  def wday; (@jd + 1) % 7; end\n"
"  def next_day(n = 1); self + n; end\n"
"  def prev_day(n = 1); self - n; end\n"
"  alias next next_day\n"
"  alias succ next_day\n"
"  def leap?\n"
"    y = year\n"
"    (y % 400 == 0) || (y % 100 != 0 && y % 4 == 0)\n"
"  end\n"
"  def self.today\n"
"    t = Time.now\n"
"    new(t.year, t.month, t.day)\n"
"  end\n"
"  def self.parse(str)\n"
"    if str =~ /^(\\d{4})-(\\d{1,2})-(\\d{1,2})/\n"
"      new($1.to_i, $2.to_i, $3.to_i)\n"
"    elsif str =~ /^(\\d{1,2})\\/(\\d{1,2})\\/(\\d{4})/\n"
"      m1, m2, y = $1.to_i, $2.to_i, $3.to_i\n"
"      m1 > 12 ? new(y, m2, m1) : new(y, m1, m2)\n"
"    elsif str =~ /^(\\d{4})(\\d{2})(\\d{2})$/\n"
"      new($1.to_i, $2.to_i, $3.to_i)\n"
"    else\n"
"      raise ArgumentError, \"invalid date: #{str.inspect}\"\n"
"    end\n"
"  end\n"
"  def self._leap?(y)\n"
"    (y % 400 == 0) || (y % 100 != 0 && y % 4 == 0)\n"
"  end\n"
"  def self.valid_date?(y, m, d)\n"
"    y, m, d = y.to_i, m.to_i, d.to_i\n"
"    m >= 1 && m <= 12 && d >= 1 && d <= 31\n"
"  end\n"
"end\n";
        return eval_ruby_string(ev, date_shim, "date_shim", site);
    }
    if (strcmp(path, "thread") == 0 || strcmp(path, "thread.rb") == 0) {
        static const char *thread_shim =
"class Mutex\n"
"  def initialize; @locked = false; end\n"
"  def lock; @locked = true; self; end\n"
"  def unlock; @locked = false; self; end\n"
"  def locked?; @locked; end\n"
"  def try_lock; return false if @locked; @locked = true; true; end\n"
"  def synchronize\n"
"    lock\n"
"    begin; yield; ensure; unlock; end\n"
"  end\n"
"end\n"
"class Queue\n"
"  def initialize; @arr = []; end\n"
"  def push(v); @arr << v; self; end\n"
"  alias enq push\n"
"  alias << push\n"
"  def pop(non_block = false)\n"
"    raise ThreadError, 'queue empty' if non_block && @arr.empty?\n"
"    @arr.shift\n"
"  end\n"
"  alias deq pop\n"
"  alias shift pop\n"
"  def size; @arr.size; end\n"
"  alias length size\n"
"  def empty?; @arr.empty?; end\n"
"  def clear; @arr.clear; self; end\n"
"  def num_waiting; 0; end\n"
"  def close; self; end\n"
"end\n"
"class SizedQueue < Queue\n"
"  def initialize(max); super(); @max = max; end\n"
"  def max; @max; end\n"
"end\n";
        return eval_ruby_string(ev, thread_shim, "thread_shim", site);
    }
    if (strcmp(path, "logger") == 0 || strcmp(path, "logger.rb") == 0)
        return val_true();
    if (strcmp(path, "digest") == 0 || strcmp(path, "digest.rb") == 0)
        return val_true();
    if (strcmp(path, "base64") == 0 || strcmp(path, "base64.rb") == 0)
        return val_true();
    if (strcmp(path, "observer") == 0 || strcmp(path, "observer.rb") == 0)
        return val_true();
    if (strcmp(path, "forwardable") == 0 || strcmp(path, "forwardable.rb") == 0) {
        static const char *forwardable_shim =
"module Forwardable\n"
"  def def_delegator(accessor, method, alias_name = method)\n"
"    acc = accessor.to_s\n"
"    meth = method.to_s\n"
"    ali = alias_name.to_s\n"
"    if acc.start_with?('@')\n"
"      define_method(ali) { |*args, &blk| instance_variable_get(acc).send(meth, *args, &blk) }\n"
"    else\n"
"      define_method(ali) { |*args, &blk| send(acc).send(meth, *args, &blk) }\n"
"    end\n"
"  end\n"
"  def def_delegators(accessor, *methods)\n"
"    methods.each { |m| def_delegator(accessor, m) }\n"
"  end\n"
"  alias delegate def_delegator\n"
"  def self.included(base)\n"
"    base.extend(Forwardable)\n"
"  end\n"
"end\n"
"module SingleForwardable\n"
"  def def_delegator(accessor, method, alias_name = method)\n"
"    acc = accessor.to_s; meth = method.to_s; ali = alias_name.to_s\n"
"    define_singleton_method(ali) { |*args, &blk| send(acc).send(meth, *args, &blk) }\n"
"  end\n"
"  def def_delegators(accessor, *methods)\n"
"    methods.each { |m| def_delegator(accessor, m) }\n"
"  end\n"
"end\n";
        return eval_ruby_string(ev, forwardable_shim, "forwardable_shim", site);
    }
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
    if (strcmp(path, "tempfile") == 0 || strcmp(path, "tempfile.rb") == 0) {
        static const char *tempfile_shim =
"class Tempfile\n"
"  @@_tf_counter = 0\n"
"  attr_reader :path\n"
"  def initialize(prefix = 'tempfile', tmpdir = '/tmp', suffix: '')\n"
"    prefix = prefix.is_a?(Array) ? prefix[0].to_s : prefix.to_s\n"
"    sfx    = prefix.is_a?(Array) ? (prefix[1] || '').to_s : suffix.to_s\n"
"    @@_tf_counter += 1\n"
"    pid = Process.pid rescue 0\n"
"    @path = \"#{tmpdir}/#{prefix}#{pid}-#{@@_tf_counter}#{sfx}\"\n"
"    @closed = false\n"
"    @unlinked = false\n"
"    @file = File.open(@path, 'w+')\n"
"  end\n"
"  def self.new(prefix = 'tempfile', *rest, **opts)\n"
"    tf = super\n"
"    if block_given?\n"
"      begin; yield tf; ensure; tf.close; tf.unlink; end\n"
"    else\n"
"      tf\n"
"    end\n"
"  end\n"
"  def self.create(prefix = 'tempfile', tmpdir = '/tmp', **opts)\n"
"    tf = new(prefix, tmpdir)\n"
"    begin\n"
"      yield tf\n"
"    ensure\n"
"      tf.close rescue nil\n"
"      File.unlink(tf.path) rescue nil\n"
"    end\n"
"  end\n"
"  def self.open(prefix = 'tempfile', *rest, **opts)\n"
"    tf = new(prefix, *rest, **opts)\n"
"    if block_given?\n"
"      begin; yield tf; ensure; tf.close; tf.unlink; end\n"
"    else\n"
"      tf\n"
"    end\n"
"  end\n"
"  def write(s); @file.write(s) if @file; end\n"
"  def read(*args); @file ? @file.read(*args) : ''; end\n"
"  def puts(*args); @file.puts(*args) if @file; end\n"
"  def print(*args); @file.print(*args) if @file; end\n"
"  def flush; @file.flush if @file; self; end\n"
"  def rewind; @file.rewind if @file; end\n"
"  def seek(n, whence = 0); @file.seek(n, whence) if @file; end\n"
"  def close\n"
"    return if @closed\n"
"    @file.close rescue nil\n"
"    @closed = true\n"
"  end\n"
"  def close!\n"
"    close\n"
"    unlink\n"
"  end\n"
"  def unlink\n"
"    return if @unlinked\n"
"    File.unlink(@path) rescue nil\n"
"    @unlinked = true\n"
"  end\n"
"  alias delete unlink\n"
"  def closed?; @closed; end\n"
"  def size; @file ? @file.size : 0; end\n"
"  def length; size; end\n"
"  def to_s; @path; end\n"
"  def inspect; \"#<Tempfile:#{@path}>\"; end\n"
"  def fileno; @file ? @file.fileno : nil; end\n"
"  def eof?; @file ? @file.eof? : true; end\n"
"  def gets(*args); @file ? @file.gets(*args) : nil; end\n"
"  def readline(*args); @file ? @file.readline(*args) : (raise EOFError, 'end of file reached'); end\n"
"  def readlines(*args); @file ? @file.readlines(*args) : []; end\n"
"  def each_line(*args, &blk); @file ? @file.each_line(*args, &blk) : self; end\n"
"  alias each each_line\n"
"  def getc; @file ? @file.getc : nil; end\n"
"  def getbyte; @file ? @file.getbyte : nil; end\n"
"  def pos; @file ? @file.tell : 0; end\n"
"  def tell; @file ? @file.tell : 0; end\n"
"  def truncate(len); @file ? @file.truncate(len) : 0; end\n"
"end\n";
        return eval_ruby_string(ev, tempfile_shim, "tempfile_shim", site);
    }
    if (strcmp(path, "etc") == 0 || strcmp(path, "etc.rb") == 0) {
        static const char *etc_shim =
"module Etc\n"
"  def self.sysconfdir; '/etc'; end\n"
"  def self.nprocessors\n"
"    n = ENV['NPROCESSORS'] || ENV['NUMBER_OF_PROCESSORS']\n"
"    n ? n.to_i : 1\n"
"  end\n"
"  def self.getlogin; ENV['USER'] || ENV['LOGNAME'] || 'user'; end\n"
"  def self.getpwuid(uid = nil)\n"
"    pw = Object.new\n"
"    def pw.name; ENV['USER'] || 'user'; end\n"
"    def pw.dir; ENV['HOME'] || '/tmp'; end\n"
"    pw\n"
"  end\n"
"  def self.getpwnam(name)\n"
"    pw = Object.new\n"
"    def pw.name; ENV['USER'] || 'user'; end\n"
"    def pw.dir; ENV['HOME'] || '/tmp'; end\n"
"    pw\n"
"  end\n"
"end\n";
        return eval_ruby_string(ev, etc_shim, "etc_shim", site);
    }
    if (strcmp(path, "socket") == 0 || strcmp(path, "socket.rb") == 0)
        return val_true();
    if (strcmp(path, "fileutils") == 0 || strcmp(path, "fileutils.rb") == 0)
        return val_true();
    if (strcmp(path, "timeout") == 0 || strcmp(path, "timeout.rb") == 0) {
        static const char *timeout_shim =
"module Timeout\n"
"  class Error < RuntimeError; end\n"
"  def self.timeout(sec, klass = nil, &block)\n"
"    block.call\n"
"  end\n"
"end\n";
        return eval_ruby_string(ev, timeout_shim, "timeout_shim", site);
    }
    if (strcmp(path, "monitor") == 0 || strcmp(path, "monitor.rb") == 0) {
        static const char *monitor_shim =
"class Monitor\n"
"  def synchronize; yield; end\n"
"  def mon_enter; end\n"
"  def mon_exit; end\n"
"  def try_enter; true; end\n"
"end\n"
"module MonitorMixin\n"
"  def self.included(base)\n"
"    base.instance_eval { def new_cond; Object.new; end }\n"
"  end\n"
"  def initialize(*args)\n"
"    mon_initialize\n"
"  end\n"
"  def mon_initialize; end\n"
"  def synchronize; yield; end\n"
"  def mon_enter; end\n"
"  def mon_exit; end\n"
"  def try_enter; true; end\n"
"  def new_cond; Object.new; end\n"
"end\n";
        return eval_ruby_string(ev, monitor_shim, "monitor_shim", site);
    }
    if (strcmp(path, "zlib") == 0 || strcmp(path, "zlib.rb") == 0) {
        static const char *zlib_shim =
"module Zlib\n"
"  BEST_COMPRESSION = 9\n"
"  BEST_SPEED = 1\n"
"  DEFAULT_COMPRESSION = -1\n"
"  NO_COMPRESSION = 0\n"
"  def self.deflate(str, level = DEFAULT_COMPRESSION); str.to_s; end\n"
"  def self.inflate(str); str.to_s; end\n"
"  def self.adler32(str = '', adler = 1); adler.to_i; end\n"
"  def self.crc32(str = '', crc = 0); crc.to_i; end\n"
"  class Error < StandardError; end\n"
"  class GzipError < Error; end\n"
"  class GzipWriter\n"
"    def initialize(io, level = nil, strategy = nil, **opts); @io = io; @buf = ''; end\n"
"    def write(s); @buf << s.to_s; @io.write(s.to_s) if @io.respond_to?(:write); s.to_s.length; end\n"
"    def <<(s); write(s); self; end\n"
"    def flush; self; end\n"
"    def close; self; end\n"
"    def finish; @buf; end\n"
"    def self.open(path, level = nil, **opts, &blk)\n"
"      f = File.open(path, 'wb')\n"
"      w = new(f, level)\n"
"      blk ? (blk.call(w); w.close; nil) : w\n"
"    end\n"
"  end\n"
"  class GzipReader\n"
"    def initialize(io, **opts); @io = io; end\n"
"    def read(n = nil); n ? @io.read(n) : @io.read; end\n"
"    def each_line(&blk); @io.each_line(&blk); end\n"
"    def close; @io.close if @io.respond_to?(:close); self; end\n"
"    def self.open(path, **opts, &blk)\n"
"      f = File.open(path, 'rb')\n"
"      r = new(f)\n"
"      blk ? (result = blk.call(r); r.close; result) : r\n"
"    end\n"
"  end\n"
"end\n";
        return eval_ruby_string(ev, zlib_shim, "zlib_shim", site);
    }
    if (strcmp(path, "strscan") == 0 || strcmp(path, "strscan.rb") == 0) {
        static const char *strscan_shim =
"class StringScanner\n"
"  def initialize(str)\n"
"    @str = str.to_s\n"
"    @pos = 0\n"
"  end\n"
"  def string; @str; end\n"
"  def pos; @pos; end\n"
"  def pos=(n); @pos = n.to_i; end\n"
"  def eos?; @pos >= @str.length; end\n"
"  def reset; @pos = 0; self; end\n"
"  def rest; @str[@pos..]; end\n"
"  def rest_size; @str.length - @pos; end\n"
"  def peek(len); @str[@pos, len] || ''; end\n"
"  def getch\n"
"    return nil if eos?\n"
"    c = @str[@pos]; @pos += 1; c\n"
"  end\n"
"  def get_byte; getch; end\n"
"  def scan(pattern)\n"
"    m = @str[@pos..].match(Regexp.new('\\A(?:' + pattern.source + ')'))\n"
"    return nil unless m\n"
"    @matched = m[0]; @pos += @matched.length; @matched\n"
"  end\n"
"  def scan_until(pattern)\n"
"    m = @str[@pos..].match(pattern)\n"
"    return nil unless m\n"
"    end_pos = @pos + m.end(0)\n"
"    @matched = @str[@pos...end_pos]; @pos = end_pos; @matched\n"
"  end\n"
"  def skip(pattern)\n"
"    m = @str[@pos..].match(Regexp.new('\\A(?:' + pattern.source + ')'))\n"
"    return nil unless m\n"
"    @matched = m[0]; @pos += @matched.length; @matched.length\n"
"  end\n"
"  def check(pattern)\n"
"    m = @str[@pos..].match(Regexp.new('\\A(?:' + pattern.source + ')'))\n"
"    m ? m[0] : nil\n"
"  end\n"
"  def matched; @matched; end\n"
"  def matched?; !@matched.nil?; end\n"
"  def matched_size; @matched ? @matched.length : nil; end\n"
"  def pre_match; @pos - (@matched || '').length >= 0 ? @str[0...(@pos - (@matched || '').length)] : nil; end\n"
"  def post_match; @str[@pos..]; end\n"
"  def [](n); @matched; end\n"
"  def inspect; \"#<StringScanner #{@pos}/#{@str.length}>\"; end\n"
"end\n";
        return eval_ruby_string(ev, strscan_shim, "strscan_shim", site);
    }
    if (strcmp(path, "rubygems/dependency") == 0 ||
        strcmp(path, "rubygems/dependency.rb") == 0 ||
        strcmp(path, "rubygems/deprecate") == 0 ||
        strcmp(path, "rubygems/deprecate.rb") == 0 ||
        strcmp(path, "rubygems/platform") == 0 ||
        strcmp(path, "rubygems/platform.rb") == 0 ||
        strcmp(path, "rubygems/specification") == 0 ||
        strcmp(path, "rubygems/specification.rb") == 0 ||
        strcmp(path, "rubygems/stub_specification") == 0 ||
        strcmp(path, "rubygems/stub_specification.rb") == 0 ||
        strcmp(path, "rubygems/name_tuple") == 0 ||
        strcmp(path, "rubygems/name_tuple.rb") == 0 ||
        strcmp(path, "rubygems/source") == 0 ||
        strcmp(path, "rubygems/source.rb") == 0 ||
        strcmp(path, "rubygems/user_interaction") == 0 ||
        strcmp(path, "rubygems/user_interaction.rb") == 0 ||
        strcmp(path, "rubygems/command") == 0 ||
        strcmp(path, "rubygems/command.rb") == 0) {
        return eval_require(ev, env, "rubygems", site);
    }
    if (strcmp(path, "rubygems") == 0 || strcmp(path, "rubygems.rb") == 0) {
        static const char *rubygems_shim_part1 =
"module Gem\n"
"  VERSION = \"3.6.9\"\n"
"  RUBYGEMS_VERSION = \"3.6.9\"\n"
"\n"
"  module Deprecate; end\n"
"\n"
"  class Version\n"
"    include Comparable\n"
"    def initialize(v); @version = v.to_s; end\n"
"    def to_s; @version; end\n"
"    def segments\n"
"      @version.split(\".\").map { |s| s =~ /\\A\\d+\\z/ ? s.to_i : s }\n"
"    end\n"
"    def <=>(other)\n"
"      segments <=> Gem::Version.new(other.to_s).segments\n"
"    end\n"
"    def inspect; \"#<Gem::Version \\\"#{@version}\\\">\"; end\n"
"    def prerelease?; @version =~ /[a-zA-Z]/; end\n"
"    def release\n"
"      Gem::Version.new(@version.split(\".\").reject { |s| s =~ /[a-zA-Z]/ }.join(\".\"))\n"
"    end\n"
"  end\n"
"\n"
"  class Requirement\n"
"    OPS = { \"=\" => :==, \"!=\" => :!=, \">\" => :>, \"<\" => :<, \">=\" => :>=, \"<=\" => :<=, \"~>\" => :=~ }.freeze\n"
"    def initialize(*reqs)\n"
"      raw = reqs.flatten.map(&:to_s)\n"
"      raw = ['>= 0'] if raw.empty?\n"
"      @requirements = raw.map do |req|\n"
"        if req =~ /\\A\\s*(=|!=|>=|<=|>|<|~>)\\s*(.+)\\z/\n"
"          [$1, Gem::Version.new($2)]\n"
"        else\n"
"          ['=', Gem::Version.new(req)]\n"
"        end\n"
"      end\n"
"    end\n"
"    attr_reader :requirements\n"
"    def satisfied_by?(version)\n"
"      version = Gem::Version.new(version.to_s)\n"
"      @requirements.all? do |op, other|\n"
"        if op == '='\n"
"          (version <=> other) == 0\n"
"        elsif op == '!='\n"
"          (version <=> other) != 0\n"
"        elsif op == '>'\n"
"          version > other\n"
"        elsif op == '<'\n"
"          version < other\n"
"        elsif op == '>='\n"
"          version >= other\n"
"        elsif op == '<='\n"
"          version <= other\n"
"        elsif op == '~>'\n"
"          segs = other.to_s.split('.').map(&:to_i)\n"
"          lower = other\n"
"          if segs.length >= 2\n"
"            upper_segs = segs[0..-2].dup\n"
"            upper_segs[-1] = upper_segs[-1] + 1\n"
"            upper = Gem::Version.new(upper_segs.join('.'))\n"
"            version >= lower && version < upper\n"
"          else\n"
"            version >= lower\n"
"          end\n"
"        else\n"
"          true\n"
"        end\n"
"      end\n"
"    end\n"
"    def requirements_list; @requirements.map { |op, v| \"#{op} #{v}\" }; end\n"
"    def to_s; requirements_list.join(\", \"); end\n"
"    def inspect; \"Gem::Requirement.new(#{requirements_list.map(&:inspect).join(\", \")})\"; end\n"
"    def self.new(*reqs); super(*reqs); end\n"
"    def self.default; new(\">= 0\"); end\n"
"    def none?; @requirements == [['>=', Gem::Version.new('0')]]; end\n"
"    def ==(other); other.is_a?(Gem::Requirement) && requirements_list == other.requirements_list; end\n"
"  end\n"
"\n"
"  class Dependency\n"
"    attr_reader :name, :requirement, :type\n"
"    def initialize(name, *reqs)\n"
"      type = reqs.last.is_a?(Symbol) ? reqs.pop : :runtime\n"
"      @name = name.to_s\n"
;
        static const char *rubygems_shim_part2 =
"      @type = type\n"
"      @requirement = Gem::Requirement.new(*reqs)\n"
"    end\n"
"    def runtime?; @type == :runtime; end\n"
"    def development?; @type == :development; end\n"
"    def requirements_list; @requirement.requirements_list; end\n"
"    def matches_spec?(spec)\n"
"      spec && spec.name == name && requirement.satisfied_by?(spec.version)\n"
"    end\n"
"    def to_s; \"#{name} (#{requirement})\"; end\n"
"    def inspect; \"#<Gem::Dependency name=\\\"#{name}\\\" requirements=\\\"#{requirement}\\\">\"; end\n"
"  end\n"
"\n"
"  class Source\n"
"    attr_accessor :uri\n"
"    def initialize(uri = nil); @uri = uri; end\n"
"  end\n"
"  class Source::Installed < Source; end\n"
"  class Source::Local < Source; end\n"
"  class Source::Lock < Source; end\n"
"  class Source::SpecificFile < Source; end\n"
"  class Source::Git < Source; end\n"
"  class Source::Vendor < Source; end\n"
"\n"
"  class Specification\n"
"    CURRENT_SPEC_VERSION = 4\n"
"    @@default_value = {\n"
"      authors: [], email: [], require_paths: ['lib'], files: [], executables: [],\n"
"      runtime_dependencies: [], development_dependencies: []\n"
"    }\n"
"    @@array_attributes = @@default_value.select { |_, v| v.is_a?(Array) }.keys\n"
"    attr_accessor :name, :version, :summary, :description, :authors,\n"
"                  :email, :homepage, :license, :require_paths,\n"
"                  :files, :executables, :bindir, :metadata,\n"
"                  :required_ruby_version, :required_rubygems_version,\n"
"                  :loaded_from, :platform, :activated\n"
"    def initialize\n"
"      @name = \"\"\n"
"      @version = Gem::Version.new(\"0.0.0\")\n"
"      @summary = \"\"\n"
"      @description = \"\"\n"
"      @authors = []\n"
"      @email = []\n"
"      @homepage = \"\"\n"
"      @license = nil\n"
"      @licenses = []\n"
"      @require_paths = [\"lib\"]\n"
"      @files = []\n"
"      @executables = []\n"
"      @bindir = \"bin\"\n"
"      @metadata = {}\n"
"      @runtime_dependencies = []\n"
"      @development_dependencies = []\n"
"      @required_ruby_version = Gem::Requirement.default\n"
"      @required_rubygems_version = Gem::Requirement.default\n"
"      @loaded_from = nil\n"
"      @platform = Gem::Platform::RUBY\n"
"      @activated = false\n"
"      yield self if block_given?\n"
"      Gem::Specification._loading_spec = self\n"
"    end\n"
"    def add_runtime_dependency(name, *reqs)\n"
;
        static const char *rubygems_shim_part3 =
"      @runtime_dependencies << Gem::Dependency.new(name, *reqs, :runtime)\n"
"    end\n"
"    alias add_dependency add_runtime_dependency\n"
"    def add_development_dependency(name, *reqs)\n"
"      @development_dependencies << Gem::Dependency.new(name, *reqs, :development)\n"
"    end\n"
"    def runtime_dependencies; @runtime_dependencies; end\n"
"    def development_dependencies; @development_dependencies; end\n"
"    def dependencies; @runtime_dependencies + @development_dependencies; end\n"
"    def licenses; @licenses; end\n"
"    def license=(val); @license = val; @licenses = [val]; end\n"
"    def full_name; \"#{name}-#{version}\"; end\n"
"    def base_dir; Gem.home; end\n"
"    def gem_dir; File.join(Gem.home, \"gems\", full_name); end\n"
"    def full_gem_path; gem_dir; end\n"
"    def lib_dirs_glob\n"
"      require_paths.map { |rp| File.join(gem_dir, rp) }\n"
"    end\n"
"    def full_require_paths; lib_dirs_glob; end\n"
"    def load_paths; full_require_paths; end\n"
"    def source; @source ||= Gem::Source.new(\"https://rubygems.org\"); end\n"
"    def source=(value); @source = value; end\n"
"    def extensions_dir; File.join(base_dir, \"extensions\"); end\n"
"    def extension_dir; File.join(extensions_dir, full_name); end\n"
"    def default_gem?; false; end\n"
"    def ignored?; false; end\n"
"    def missing_extensions?; false; end\n"
"    def validate_for_resolution; true; end\n"
"    def to_s; \"#{name}-#{version}\"; end\n"
"    def inspect; \"#<Gem::Specification name=\\\"#{name}\\\" version=\\\"#{version}\\\">\"; end\n"
"    def self._load_gemspec(path)\n"
"      spec = new\n"
"      begin\n"
"        load path\n"
"      rescue Exception\n"
"        nil\n"
"      end\n"
"      spec\n"
"    end\n"
"    def self._scan_gemspecs\n"
"      return @_all if defined?(@_all) && @_all\n"
"      @_all = []\n"
"      Gem.path.each do |gem_dir|\n"
"        spec_dir = File.join(gem_dir, \"specifications\")\n"
"        next unless Dir.exist?(spec_dir)\n"
"        Dir.glob(File.join(spec_dir, \"*.gemspec\")).each do |f|\n"
"          begin\n"
"            # Gemspecs call Gem::Specification.new { |s| ... } which returns spec.\n"
"            # Capture via a sentinel around load.\n"
"            @_loading_spec = nil\n"
"            load f\n"
"            @_all << @_loading_spec if @_loading_spec && !@_loading_spec.name.to_s.empty?\n"
"          rescue Exception\n"
"            # skip malformed gemspecs\n"
"          end\n"
"        end\n"
;
        static const char *rubygems_shim_part4 =
"      end\n"
"      @_loading_spec = nil\n"
"      @_all\n"
"    end\n"
"    def self.all\n"
"      _scan_gemspecs\n"
"    end\n"
"    def self.all_by_name\n"
"      all.each_with_object({}) { |s, h| (h[s.name] ||= []) << s }\n"
"    end\n"
"    def self._normalize_requirements(reqs)\n"
"      flat = reqs.flatten\n"
"      flat.empty? ? Gem::Requirement.default : Gem::Requirement.new(flat)\n"
"    end\n"
"    def self.find_by_name(name, *reqs)\n"
"      requirements = _normalize_requirements(reqs)\n"
"      matches = all.select { |s| s.name == name.to_s && requirements.satisfied_by?(s.version) }\n"
"      matches.sort_by { |s| s.version }.last\n"
"    end\n"
"    def self.find_all_by_name(name, *reqs)\n"
"      requirements = _normalize_requirements(reqs)\n"
"      all.select { |s| s.name == name.to_s && requirements.satisfied_by?(s.version) }\n"
"    end\n"
"    def self.each(&blk)\n"
"      all.each(&blk)\n"
"    end\n"
"    def self.any?(&blk)\n"
"      all.any?(&blk)\n"
"    end\n"
"    def self._load(str); new; end\n"
"    def self._loading_spec; @_loading_spec; end\n"
"    def self._loading_spec=(s); @_loading_spec = s; end\n"
"    def self.reset!\n"
"      @_all = nil\n"
"      @_loading_spec = nil\n"
"    end\n"
"    def self.reset\n"
"      reset!\n"
"    end\n"
"  end\n"
"\n"
"  BasicSpecification = Specification\n"
"  StubSpecification = Specification\n"
"\n"
"  class Platform\n"
"    RUBY = \"ruby\"\n"
"    CURRENT = RUBY_PLATFORM\n"
"    attr_accessor :cpu, :os, :version\n"
"    def initialize(arch)\n"
"      @arch = arch.to_s\n"
"      parts = @arch.split('-', 3)\n"
"      @cpu = parts[0]\n"
"      @os = parts[1]\n"
"      @version = parts[2]\n"
"    end\n"
"    def to_s; @arch; end\n"
"    def inspect; \"Gem::Platform.new(\\\"#{@arch}\\\")\"; end\n"
"    def ==(other); @arch == other.to_s; end\n"
"    alias === ==\n"
"    def self.local; new(CURRENT); end\n"
"  end\n"
"\n"
"  class NameTuple\n"
"    attr_reader :name, :version, :platform\n"
"    def initialize(name, version, platform = Gem::Platform::RUBY)\n"
"      @name = name.to_s\n"
"      @version = Gem::Version.new(version.to_s)\n"
"      @platform = platform.is_a?(Gem::Platform) ? platform.to_s : platform.to_s\n"
"    end\n"
"    def spec_name\n"
"      suffix = platform == Gem::Platform::RUBY ? '' : \"-#{platform}\"\n"
"      \"#{name}-#{version}#{suffix}.gemspec\"\n"
"    end\n"
"    def full_name\n"
"      spec_name.sub(/\\.gemspec\\z/, '')\n"
"    end\n"
"    def lock_name\n"
"      platform == Gem::Platform::RUBY ? \"#{name} (#{version})\" : \"#{name} (#{version}-#{platform})\"\n"
"    end\n"
"  end\n"
"\n"
"  class Command\n"
"    class << self\n"
"      attr_accessor :build_args\n"
"    end\n"
"  end\n"
"\n"
"  module DefaultUserInteraction\n"
"    class << self\n"
"      attr_accessor :ui\n"
"    end\n"
"  end\n"
"\n"
"  module Resolver\n"
"    class VendorSpecification; end\n"
;
        static const char *rubygems_shim_part5 =
"    class ActivationRequest; end\n"
"    class APISet\n"
"      class GemParser; end\n"
"    end\n"
"  end\n"
"\n"
"  class SafeMarshal\n"
"    def self.safe_load(obj); obj; end\n"
"  end\n"
"\n"
"  class ConfigFile\n"
"    DEFAULT_BACKTRACE = false\n"
"    def initialize(args = []); @data = {}; end\n"
"    def [](key); @data[key]; end\n"
"    def []=(key, val); @data[key] = val; end\n"
"    def backtrace; DEFAULT_BACKTRACE; end\n"
"    def verbose; false; end\n"
"  end\n"
"\n"
"  class LoadError < ::LoadError\n"
"    attr_accessor :name, :requirement\n"
"  end\n"
"  class MissingSpecError < Gem::LoadError\n"
"    def initialize(name, req = nil)\n"
"      @name = name.to_s\n"
"      @requirement = req\n"
"      super(\"Could not find gem '#{@name}'\")\n"
"    end\n"
"  end\n"
"  class MissingSpecVersionError < Gem::MissingSpecError\n"
"    def initialize(msg = nil)\n"
"      @message = msg.to_s if msg\n"
"    end\n"
"    def message\n"
"      @message || super\n"
"    end\n"
"    def to_s\n"
"      message\n"
"    end\n"
"  end\n"
"  class GemNotFoundException < Gem::LoadError; end\n"
"  class Exception < ::RuntimeError; end\n"
"\n"
"  class << self\n"
"    def home\n"
"      @home ||= ENV[\"GEM_HOME\"] || File.join(ENV[\"HOME\"] || \"/tmp\", \".gem\", \"ruby\", RUBY_VERSION)\n"
"    end\n"
"    def dir; home; end\n"
"    def path\n"
"      @path ||= begin\n"
"        paths = [home]\n"
"        gp = ENV[\"GEM_PATH\"]\n"
"        if gp && !gp.empty?\n"
"          paths += gp.split(File::PATH_SEPARATOR).reject { |p| p.empty? }\n"
"        end\n"
"        paths.uniq\n"
"      end\n"
"    end\n"
"    def default_dir\n"
"      begin\n"
"        File.join(RbConfig::CONFIG[\"rubylibdir\"] || \"\", \"..\", \"..\", \"gems\", RUBY_VERSION)\n"
"      rescue NameError\n"
"        File.join(home, \"..\", \"gems\", RUBY_VERSION)\n"
"      end\n"
"    end\n"
"    def rubygems_version\n"
"      Gem::Version.new(VERSION)\n"
"    end\n"
"    def loaded_specs\n"
"      @loaded_specs ||= {}\n"
"    end\n"
"    def ruby_engine\n"
"      defined?(RUBY_ENGINE) ? RUBY_ENGINE : \"ruby\"\n"
"    end\n"
"    def find_files(glob); []; end\n"
"    def find_files_from_load_path(glob); []; end\n"
"    def available?(name, *reqs)\n"
"      spec = Gem::Specification.find_by_name(name.to_s, reqs)\n"
"      spec ? true : false\n"
"    end\n"
"    def ruby; RbConfig.ruby; end\n"
"    def user_home\n"
"      ENV[\"HOME\"] || \"/tmp\"\n"
"    end\n"
"    def bindir\n"
"      File.join(home, \"bin\")\n"
"    end\n"
"    def win_platform?; false; end\n"
"    def java_platform?; false; end\n"
"    def platforms; [Gem::Platform::RUBY]; end\n"
"    def configuration\n"
"      @configuration ||= Gem::ConfigFile.new([])\n"
"    end\n"
"    def suffixes; [\"\", \".rb\"]; end\n"
;
        static const char *rubygems_shim_part6 =
"    def datadir(name)\n"
"      File.join(home, \"gems\", name, \"data\")\n"
"    end\n"
"    def bin_path(name, exec_name = nil, *args)\n"
"      spec = Gem::Specification.find_by_name(name.to_s, args)\n"
"      unless spec\n"
"        raise Gem::MissingSpecError, \"Could not find gem '#{name}'\"\n"
"      end\n"
"      exec_name = exec_name || name.to_s\n"
"      bindir = spec.bindir || \"bin\"\n"
"      candidate = File.join(spec.gem_dir, bindir, exec_name)\n"
"      if File.exist?(candidate)\n"
"        return candidate\n"
"      end\n"
"      wrapper = File.join(home, \"bin\", exec_name)\n"
"      if File.exist?(wrapper)\n"
"        return wrapper\n"
"      end\n"
"      raise Gem::Exception, \"can't find executable #{exec_name} for gem #{name}\"\n"
"    end\n"
"    def activate_bin_path(name, exec_name, *args)\n"
"      spec = Gem::Specification.find_by_name(name.to_s, args)\n"
"      unless spec\n"
"        raise Gem::MissingSpecError, \"Could not find gem '#{name}'\"\n"
"      end\n"
"      Gem.loaded_specs[name.to_s] = spec\n"
"      bin_path(name, exec_name, args)\n"
"    end\n"
"    def suffix_pattern\n"
"      /(?:\\.rb)?\\z/\n"
"    end\n"
"    def gem_path; path; end\n"
"    def use_paths(home_dir, paths_arr = nil)\n"
"      @home = home_dir\n"
"      @path = paths_arr ? paths_arr : [home_dir]\n"
"    end\n"
"    def clear_paths\n"
"      @home = nil\n"
"      @path = nil\n"
"    end\n"
"    def spec_cache_dir\n"
"      File.join(home, \"specs\")\n"
"    end\n"
"    def read_binary(path)\n"
"      File.binread(path)\n"
"    end\n"
"    def marshal_version\n"
"      \"#{Marshal::MAJOR_VERSION}.#{Marshal::MINOR_VERSION}\"\n"
"    end\n"
"    def load_safe_marshal\n"
"      true\n"
"    end\n"
"    def source_index; nil; end\n"
"    def sources\n"
"      @sources ||= [\"https://rubygems.org\"]\n"
"    end\n"
"    def post_install_hooks; @post_install_hooks ||= []; end\n"
"    def post_uninstall_hooks; @post_uninstall_hooks ||= []; end\n"
"    def pre_install_hooks; @pre_install_hooks ||= []; end\n"
"    def pre_uninstall_hooks; @pre_uninstall_hooks ||= []; end\n"
"    def done_installing_hooks; @done_installing_hooks ||= []; end\n"
"    def post_reset_hooks; @post_reset_hooks ||= []; end\n"
"  end\n"
"end\n"
"\n"
"def gem(name, *requirements)\n"
"  spec = Gem::Specification.find_by_name(name.to_s, requirements)\n"
"  if spec\n"
"    spec.require_paths.each do |rp|\n"
"      lib = File.join(spec.gem_dir, rp)\n"
"      $LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)\n"
"    end\n"
"    Gem.loaded_specs[name.to_s] = spec\n"
"    true\n"
"  else\n"
"    req = requirements.empty? ? nil : Gem::Requirement.new(requirements)\n"
"    matching_name = Gem::Specification.find_all_by_name(name.to_s)\n"
"    if req && !matching_name.empty?\n"
"      raise Gem::MissingSpecVersionError, \"Could not find gem '#{name}' with requirement '#{req}'\"\n"
"    end\n"
"    err = Gem::MissingSpecError.new\n"
"    err.instance_variable_set(:@message, \"Could not find gem '#{name}'\")\n"
"    raise err\n"
"  end\n"
"end\n"
"\n"
"# Extend $LOAD_PATH with all installed gem lib dirs when rubygems is loaded.\n"
"Gem.path.each do |gem_root|\n"
"  gems_dir = File.join(gem_root, \"gems\")\n"
"  next unless Dir.exist?(gems_dir)\n"
"  Dir.children(gems_dir).each do |entry|\n"
"    lib = File.join(gems_dir, entry, \"lib\")\n"
"    $LOAD_PATH << lib if Dir.exist?(lib) && !$LOAD_PATH.include?(lib)\n"
"  end\n"
"end\n"
;
        const char *rubygems_shim_parts[6];
        rubygems_shim_parts[0] = rubygems_shim_part1;
        rubygems_shim_parts[1] = rubygems_shim_part2;
        rubygems_shim_parts[2] = rubygems_shim_part3;
        rubygems_shim_parts[3] = rubygems_shim_part4;
        rubygems_shim_parts[4] = rubygems_shim_part5;
        rubygems_shim_parts[5] = rubygems_shim_part6;
        return eval_ruby_string_parts(ev, rubygems_shim_parts,
                                      sizeof(rubygems_shim_parts) / sizeof(rubygems_shim_parts[0]),
                                      "rubygems_shim", site);
    }

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

Value eval_load(Eval *ev, const char *path, Node *site) {
    if (!path || path[0] == '\0')
        return eval_raise_class(ev, site, "LoadError", "load path is empty");

    /* Expand relative paths relative to cwd */
    char resolved_buf[PATH_MAX * 2];
    if (path[0] != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            int n = snprintf(resolved_buf, sizeof(resolved_buf), "%s/%s", cwd, path);
            if (n > 0 && n < (int)sizeof(resolved_buf))
                path = resolved_buf;
        }
    }

    size_t src_len = 0;
    char *src = read_file_bytes(path, &src_len);
    if (!src)
        return eval_raise_class(ev, site, "LoadError", "cannot load such file -- %s", path);

    {
        size_t bad = 0;
        if (!utf8_validate(src, src_len, &bad)) {
            free(src);
            return eval_raise_class(ev, site, "LoadError", "invalid UTF-8 in source -- %s", path);
        }
    }

    Parser parser;
    parser_init(&parser, src, src_len, ev->arena);
    Node *tree = parse_program(&parser);
    if (parser.error_count) {
        Value err = eval_raise_class(ev, site, "LoadError", "parse error in %s: %s",
                                     path, parser.errors[0].message);
        free(src);
        return err;
    }

    Sema sema;
    sema_init(&sema, ev->arena);
    sema_run(&sema, tree);
    if (sema.error_count) {
        Value err = eval_raise_class(ev, site, "LoadError", "sema error in %s: %s",
                                     path, sema.errors[0].message);
        free(src);
        return err;
    }

    const char *previous_file = ev->current_file;
    ev->current_file = path;
    Value result = eval_node(ev, ev->top_env, tree);
    ev->current_file = previous_file;
    free(src);

    if (val_is_signal(result)) return result;
    return val_true();
}
