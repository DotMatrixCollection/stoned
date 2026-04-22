#include "parser_internal.h"

#include <string.h>

void error(Parser *p, const char *msg, uint32_t line, uint32_t col) {
    if (p->error_count < MAX_ERRORS) {
        p->errors[p->error_count].message = msg;
        p->errors[p->error_count].line = line;
        p->errors[p->error_count].col = col;
        p->error_count++;
    }
    p->panic = 1;
}

void sync(Parser *p) {
    p->panic = 0;
    while (1) {
        Token t = lexer_peek(&p->lexer);
        switch (t.kind) {
            case TOK_EOF:
            case TOK_NEWLINE:
            case TOK_SEMICOLON:
            case TOK_DEF:
            case TOK_CLASS:
            case TOK_MODULE:
            case TOK_END:
                return;
            default:
                lexer_consume(&p->lexer);
        }
    }
}

Token peek(Parser *p) { return lexer_peek(&p->lexer); }
Token advance(Parser *p) { return lexer_next(&p->lexer); }
int check(Parser *p, TokenKind k) { return peek(p).kind == k; }

int match(Parser *p, TokenKind k) {
    if (check(p, k)) {
        advance(p);
        return 1;
    }
    return 0;
}

Token expect(Parser *p, TokenKind k, const char *msg) {
    Token t = peek(p);
    if (t.kind == k) return advance(p);
    error(p, msg, t.line, t.col);
    return t;
}

void skip_terminators(Parser *p) {
    while (check(p, TOK_NEWLINE) || check(p, TOK_SEMICOLON))
        advance(p);
}

Span tok_span(Token t) {
    Span s;
    s.line = t.line;
    s.col = t.col;
    s.len = t.len;
    return s;
}

void parser_init(Parser *p, const char *src, size_t len, Arena *arena) {
    memset(p, 0, sizeof(*p));
    lexer_init(&p->lexer, src, len, arena);
    p->arena = arena;
    p->allow_command_arg_commas = 1;
}

Node *parse_program(Parser *p) {
    Span s = {1, 1, 0};
    Node *n = node_new(p->arena, NODE_PROGRAM, s);
    NodeList *stmts = NULL;

    skip_terminators(p);
    while (!check(p, TOK_EOF)) {
        Node *stmt = parse_stmt(p);
        if (p->panic) {
            sync(p);
            continue;
        }
        if (stmt) stmts = nodelist_append(p->arena, stmts, stmt);
        skip_terminators(p);
    }

    n->body.stmts = stmts;
    return n;
}
