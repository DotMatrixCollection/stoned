#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arena.h"
#include "parser.h"
#include "sema.h"
#include "eval_internal.h"
#include "utf8.h"

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

int main(int argc, char **argv) {
    const char *src;
    size_t src_len;
    char *file_buf = NULL;

    if (argc >= 2) {
        file_buf = read_file(argv[1], &src_len);
        if (!file_buf) return 1;
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
        return 1;
    }

    Eval eval;
    eval_init(&eval, &arena, stdout, argc >= 2 ? argv[1] : NULL);
    Value result = eval_node(&eval, eval.top_env, tree);

    if (eval.errored) {
        fprintf(stderr, "error: %s\n", eval.errmsg);
        arena_free(&arena);
        free(file_buf);
        return 1;
    }
    if (result.kind == VAL_EXCEPTION) {
        fprintf(stderr, "error: %u:%u: %s\n",
                eval.exception_line, eval.exception_col, eval.exception_msg);
        Value backtrace = exception_value_backtrace(eval.current_exception);
        if (backtrace.kind == VAL_ARRAY) {
            for (size_t i = 0; i < backtrace.array->len; i++) {
                fprintf(stderr, "  from %s\n", val_to_s(&arena, backtrace.array->elems[i]));
            }
        }
        arena_free(&arena);
        free(file_buf);
        return 1;
    }

    arena_free(&arena);
    free(file_buf);
    return parser.error_count ? 1 : 0;
}
