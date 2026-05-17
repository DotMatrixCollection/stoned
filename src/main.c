#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "arena.h"
#include "parser.h"
#include "sema.h"
#include "eval_internal.h"
#include "utf8.h"
#include "version.h"

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    *out_len = len;
    return buf;
}

static void line_col_for_offset(const char *src, size_t offset, uint32_t *line, uint32_t *col) {
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

static void write_runtime_stderr(Eval *eval, Arena *arena, const char *text) {
    Value stderr_obj = val_nil();
    if (global_get(&eval->globals, "stderr", &stderr_obj)) {
        Value saved_exception = eval->current_exception;
        Value saved_rescue = eval->rescue_context;
        uint32_t saved_line = eval->exception_line;
        uint32_t saved_col = eval->exception_col;
        const char *saved_class = eval->exception_class;
        char saved_msg[sizeof(eval->exception_msg)];
        memcpy(saved_msg, eval->exception_msg, sizeof(saved_msg));
        int saved_errored = eval->errored;

        eval->errored = 0;
        eval_clear_exception(eval);
        Value str = val_string(arena, text);
        Value out = dispatch_method(eval, eval->top_env, stderr_obj, "write", &str, 1, NULL, NULL, 0, 1);
        eval->errored = saved_errored;
        eval->current_exception = saved_exception;
        eval->rescue_context = saved_rescue;
        eval->exception_line = saved_line;
        eval->exception_col = saved_col;
        eval->exception_class = saved_class;
        memcpy(eval->exception_msg, saved_msg, sizeof(saved_msg));
        if (!val_is_signal(out)) return;
        eval_clear_exception(eval);
    }
    fputs(text, stderr);
}

static char *dup_cstr(const char *src) {
    size_t len = strlen(src);
    char *dup = malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, src, len + 1);
    return dup;
}

static char *detect_exec_path(const char *argv0) {
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len >= 0) {
        buf[len] = '\0';
        return dup_cstr(buf);
    }
    if (argv0 && strchr(argv0, '/')) return dup_cstr(argv0);
    return NULL;
}

int main(int argc, char **argv) {
    const char *src;
    size_t src_len;
    char *file_buf = NULL;
    char *exec_path = detect_exec_path(argc > 0 ? argv[0] : NULL);

    if (argc == 2 && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version"))) {
        printf("%s %s (ruby %s)\n", STONED_ENGINE_NAME, STONED_BUILD_VERSION, STONED_RUBY_VERSION);
        free(exec_path);
        return 0;
    }

    if (argc >= 2) {
        file_buf = read_file(argv[1], &src_len);
        if (!file_buf) {
            free(exec_path);
            return 1;
        }
        src = file_buf;
    } else {
        /* read stdin */
        size_t cap = 4096, len = 0;
        char *buf = malloc(cap);
        int c;
        while ((c = fgetc(stdin)) != EOF) {
            if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
            buf[len++] = (char)c;
        }
        buf[len] = '\0';
        src = buf;
        src_len = len;
        file_buf = buf;
    }

    {
        size_t bad = 0;
        if (!utf8_validate(src, src_len, &bad)) {
            uint32_t line = 1, col = 1;
            line_col_for_offset(src, bad, &line, &col);
            fprintf(stderr, "parse error:%u:%u: invalid UTF-8 in source\n", line, col);
            free(file_buf);
            free(exec_path);
            return 1;
        }
    }

    Arena  arena = arena_new();
    Parser parser;
    parser_init(&parser, src, src_len, &arena);

    Node *tree = parse_program(&parser);

    if (parser.error_count) {
        for (int i = 0; i < parser.error_count; i++) {
            fprintf(stderr, "parse error:%u:%u: %s\n",
                parser.errors[i].line,
                parser.errors[i].col,
                parser.errors[i].message);
        }
    }

    Sema sema;
    sema_init(&sema, &arena);
    sema_run(&sema, tree);

    if (sema.error_count) {
        for (int i = 0; i < sema.error_count; i++) {
            fprintf(stderr, "sema error:%u:%u: %s\n",
                sema.errors[i].line,
                sema.errors[i].col,
                sema.errors[i].message);
        }
    }

    if (parser.error_count || sema.error_count) {
        arena_free(&arena);
        free(file_buf);
        free(exec_path);
        return 1;
    }

    Eval eval;
    /* argv[2..] are script arguments exposed as ARGV inside the script */
    int script_argc = argc > 2 ? argc - 2 : 0;
    char **script_argv = argc > 2 ? argv + 2 : NULL;
    eval_init(&eval, &arena, stdout, argc >= 2 ? argv[1] : NULL, exec_path,
              script_argc, script_argv);
    Value result = eval_node(&eval, eval.top_env, tree);

    if (eval.errored) {
        char buf[2048];
        snprintf(buf, sizeof(buf), "error: %s\n", eval.errmsg);
        write_runtime_stderr(&eval, &arena, buf);
        arena_free(&arena);
        free(file_buf);
        free(exec_path);
        return 1;
    }
    if (result.kind == VAL_EXCEPTION) {
        char buf[2048];
        snprintf(buf, sizeof(buf), "error: %u:%u: %s\n",
                 eval.exception_line, eval.exception_col, eval.exception_msg);
        write_runtime_stderr(&eval, &arena, buf);
        Value backtrace = exception_value_backtrace(eval.current_exception);
        if (backtrace.kind == VAL_ARRAY) {
            for (size_t i = 0; i < backtrace.array->len; i++) {
                snprintf(buf, sizeof(buf), "  from %s\n", val_to_s(&arena, backtrace.array->elems[i]));
                write_runtime_stderr(&eval, &arena, buf);
            }
        }
        arena_free(&arena);
        free(file_buf);
        free(exec_path);
        return 1;
    }

    arena_free(&arena);
    free(file_buf);
    free(exec_path);
    return parser.error_count ? 1 : 0;
}
