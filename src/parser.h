#ifndef STONED_PARSER_H
#define STONED_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    const char *message;
    uint32_t    line;
    uint32_t    col;
} ParseError;

#define MAX_ERRORS 64

typedef struct {
    Lexer      lexer;
    Arena     *arena;
    ParseError errors[MAX_ERRORS];
    int        error_count;
    int        panic;           /* 1 = in panic mode, skip to sync point */
    int        allow_command_arg_commas;
    int        command_arg_depth;
    int        stop_at_param_pipe;
} Parser;

void  parser_init(Parser *p, const char *src, size_t len, Arena *arena);
Node *parse_program(Parser *p);

#endif
