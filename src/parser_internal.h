#ifndef STONED_PARSER_INTERNAL_H
#define STONED_PARSER_INTERNAL_H

#include "parser.h"

void error(Parser *p, const char *msg, uint32_t line, uint32_t col);
void sync(Parser *p);

Token peek(Parser *p);
Token advance(Parser *p);
int check(Parser *p, TokenKind k);
int match(Parser *p, TokenKind k);
Token expect(Parser *p, TokenKind k, const char *msg);
void skip_terminators(Parser *p);
Span tok_span(Token t);
static inline int token_adjacent(Token left, Token right) {
    return left.line == right.line && right.col == left.col + left.len;
}

Node *parse_stmt(Parser *p);
Node *parse_expr(Parser *p, int min_bp);
Node *parse_primary(Parser *p);
Node *parse_body(Parser *p, int stop_at_rbrace);
Node *wrap_rescue_ensure(Parser *p, Span s, Node *body);
NodeList *parse_args(Parser *p);
NodeList *parse_params(Parser *p);
NodeList *parse_params_unparen(Parser *p, uint32_t def_line);
Node *parse_block(Parser *p);

#endif
