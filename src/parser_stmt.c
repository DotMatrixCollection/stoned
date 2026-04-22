#include "parser_internal.h"

#include <string.h>

Node *parse_stmt(Parser *p) {
    Token t = peek(p);
    Span s = tok_span(t);

    if (t.kind == TOK_IF || t.kind == TOK_UNLESS) {
        advance(p);
        NodeKind kind = (t.kind == TOK_IF) ? NODE_IF : NODE_UNLESS;
        Node *cond = parse_expr(p, 0);
        if (!match(p, TOK_THEN)) skip_terminators(p);
        Node *then_body = parse_body(p, 0);
        Node *else_body = NULL;

        while (check(p, TOK_ELSIF)) {
            advance(p);
            Node *elsif_cond = parse_expr(p, 0);
            if (!match(p, TOK_THEN)) skip_terminators(p);
            Node *elsif_body = parse_body(p, 0);
            Node *elsif_node = node_new(p->arena, NODE_IF, s);
            elsif_node->cond.cond = elsif_cond;
            elsif_node->cond.then_body = elsif_body;
            elsif_node->cond.else_body = NULL;
            if (!else_body) {
                else_body = elsif_node;
            } else {
                Node *last = else_body;
                while (last->cond.else_body) last = last->cond.else_body;
                last->cond.else_body = elsif_node;
            }
        }
        if (match(p, TOK_ELSE)) {
            skip_terminators(p);
            Node *else_blk = parse_body(p, 0);
            if (!else_body) {
                else_body = else_blk;
            } else {
                Node *last = else_body;
                while (last->kind == NODE_IF && last->cond.else_body)
                    last = last->cond.else_body;
                last->cond.else_body = else_blk;
            }
        }
        expect(p, TOK_END, "expected 'end'");
        Node *n = node_new(p->arena, kind, s);
        n->cond.cond = cond;
        n->cond.then_body = then_body;
        n->cond.else_body = else_body;
        return n;
    }

    if (t.kind == TOK_WHILE || t.kind == TOK_UNTIL) {
        advance(p);
        NodeKind kind = (t.kind == TOK_WHILE) ? NODE_WHILE : NODE_UNTIL;
        Node *cond = parse_expr(p, 0);
        if (!match(p, TOK_DO)) skip_terminators(p);
        Node *body = parse_body(p, 0);
        expect(p, TOK_END, "expected 'end'");
        Node *n = node_new(p->arena, kind, s);
        n->loop.cond = cond;
        n->loop.body = body;
        return n;
    }

    if (t.kind == TOK_BEGIN) {
        advance(p);
        Node *n = node_new(p->arena, NODE_BEGIN, s);
        skip_terminators(p);
        n->begin_stmt.body = parse_body(p, 0);
        while (match(p, TOK_RESCUE)) {
            Node *rescue_clause = node_new(p->arena, NODE_RESCUE, s);
            if (!check(p, TOK_ARROW) && !check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) &&
                !check(p, TOK_END) && !check(p, TOK_ENSURE)) {
                rescue_clause->rescue_clause.exception_class = parse_expr(p, 0);
            }
            if (match(p, TOK_ARROW)) {
                Token var_tok = advance(p);
                if (var_tok.kind != TOK_IDENT) {
                    error(p, "expected exception variable name after '=>'", var_tok.line, var_tok.col);
                    return NULL;
                }
                rescue_clause->rescue_clause.exception_var = var_tok.sval;
            }
            skip_terminators(p);
            rescue_clause->rescue_clause.body = parse_body(p, 0);
            n->begin_stmt.rescues =
                nodelist_append(p->arena, n->begin_stmt.rescues, rescue_clause);
        }
        if (match(p, TOK_ENSURE)) {
            skip_terminators(p);
            n->begin_stmt.ensure_body = parse_body(p, 0);
        }
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    if (t.kind == TOK_DEF) {
        advance(p);
        Token name_tok = advance(p);
        Node *n = node_new(p->arena, NODE_DEF, s);
        n->def.recv = NULL;

        if (check(p, TOK_DOT)) {
            Node *recv_node = NULL;
            if (name_tok.kind == TOK_SELF) {
                recv_node = node_new(p->arena, NODE_SELF, tok_span(name_tok));
            } else if (name_tok.kind == TOK_IDENT || name_tok.kind == TOK_CONST) {
                recv_node = node_new(p->arena, NODE_LVAR, tok_span(name_tok));
                recv_node->sval = name_tok.sval;
            } else {
                error(p, "unexpected receiver in def", name_tok.line, name_tok.col);
                return NULL;
            }
            n->def.recv = recv_node;
            advance(p);
            name_tok = advance(p);
        }

        if (name_tok.kind != TOK_IDENT && name_tok.kind != TOK_CONST) {
            error(p, "expected method name after 'def'", name_tok.line, name_tok.col);
            return NULL;
        }
        if (check(p, TOK_QUESTION) || check(p, TOK_BANG)) {
            Token suffix = advance(p);
            size_t nlen = strlen(name_tok.sval);
            char *buf = arena_alloc(p->arena, nlen + 2);
            memcpy(buf, name_tok.sval, nlen);
            buf[nlen] = suffix.kind == TOK_QUESTION ? '?' : '!';
            buf[nlen + 1] = '\0';
            name_tok.sval = buf;
        }
        n->def.name = name_tok.sval;

        if (match(p, TOK_LPAREN)) {
            n->def.params = parse_params(p);
            expect(p, TOK_RPAREN, "expected ')'");
        }

        skip_terminators(p);
        n->def.body = parse_body(p, 0);
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    if (t.kind == TOK_CLASS) {
        advance(p);
        Token name_tok = advance(p);
        if (name_tok.kind != TOK_CONST) {
            error(p, "expected class name after 'class'", name_tok.line, name_tok.col);
            return NULL;
        }
        Node *n = node_new(p->arena, NODE_CLASS, s);
        n->klass.name = name_tok.sval;
        n->klass.superclass = NULL;

        if (match(p, TOK_LT)) {
            Token super_tok = advance(p);
            if (super_tok.kind != TOK_CONST) {
                error(p, "expected superclass name after '<'", super_tok.line, super_tok.col);
                return NULL;
            }
            n->klass.superclass = node_new(p->arena, NODE_CONST, tok_span(super_tok));
            n->klass.superclass->sval = super_tok.sval;
        }

        skip_terminators(p);
        n->klass.body = parse_body(p, 0);
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    Node *expr = parse_expr(p, 0);
    if (!expr) return NULL;

    t = peek(p);
    if (t.kind == TOK_IF || t.kind == TOK_UNLESS) {
        advance(p);
        NodeKind kind = (t.kind == TOK_IF) ? NODE_IF : NODE_UNLESS;
        Node *cond = parse_expr(p, 0);
        Node *n = node_new(p->arena, kind, s);
        n->cond.cond = cond;
        n->cond.then_body = expr;
        n->cond.else_body = NULL;
        return n;
    }
    if (t.kind == TOK_WHILE || t.kind == TOK_UNTIL) {
        advance(p);
        NodeKind kind = (t.kind == TOK_WHILE) ? NODE_WHILE : NODE_UNTIL;
        Node *cond = parse_expr(p, 0);
        Node *n = node_new(p->arena, kind, s);
        n->loop.cond = cond;
        n->loop.body = expr;
        return n;
    }

    return expr;
}

Node *parse_body(Parser *p, int stop_at_rbrace) {
    Span s = tok_span(peek(p));
    Node *n = node_new(p->arena, NODE_BODY, s);
    NodeList *stmts = NULL;

    while (1) {
        skip_terminators(p);
        Token t = peek(p);
        if (t.kind == TOK_EOF || t.kind == TOK_END || t.kind == TOK_ELSE ||
            t.kind == TOK_ELSIF || t.kind == TOK_ENSURE || t.kind == TOK_RESCUE)
            break;
        if (stop_at_rbrace && t.kind == TOK_RBRACE)
            break;

        Node *stmt = parse_stmt(p);
        if (p->panic) {
            sync(p);
            continue;
        }
        if (stmt) stmts = nodelist_append(p->arena, stmts, stmt);
    }

    n->body.stmts = stmts;
    return n;
}
