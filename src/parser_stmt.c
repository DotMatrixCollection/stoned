#include "parser_internal.h"

#include <string.h>

static Node *parse_expr_list(Parser *p, Node *first, int elem_min_bp, int assignment_target);

static const char *method_name_from_token(Parser *p, Token tok) {
    switch (tok.kind) {
        case TOK_IDENT:
        case TOK_CONST:
        case TOK_SYMBOL:
            return tok.sval;
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_STAR2: return "**";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_EQ2: return "==";
        case TOK_EQ3: return "===";
        case TOK_NEQ: return "!=";
        case TOK_LT: return "<";
        case TOK_LEQ: return "<=";
        case TOK_GT: return ">";
        case TOK_GEQ: return ">=";
        case TOK_SPACESHIP: return "<=>";
        case TOK_LSHIFT: return "<<";
        case TOK_RSHIFT: return ">>";
        case TOK_LBRACKET:
            if (match(p, TOK_RBRACKET)) return "[]";
            if (match(p, TOK_EQ)) {
                expect(p, TOK_RBRACKET, "expected ']'");
                return "[]=";
            }
            error(p, "expected ']' or ']=' after '[' in method name", tok.line, tok.col);
            return NULL;
        default:
            return NULL;
    }
}

/* Attach rescue/ensure clauses to an already-parsed body, producing a NODE_BEGIN.
   Called for both explicit begin...end and implicit method-level rescue. */
static Node *wrap_rescue_ensure(Parser *p, Span s, Node *body) {
    Node *n = node_new(p->arena, NODE_BEGIN, s);
    n->begin_stmt.body = body;
    while (match(p, TOK_RESCUE)) {
        Node *rc = node_new(p->arena, NODE_RESCUE, s);
        if (!check(p, TOK_ARROW) && !check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) &&
            !check(p, TOK_END) && !check(p, TOK_ENSURE) && !check(p, TOK_ELSE)) {
            do {
                Node *exc = parse_expr(p, 0);
                rc->rescue_clause.exception_classes =
                    nodelist_append(p->arena, rc->rescue_clause.exception_classes, exc);
            } while (match(p, TOK_COMMA));
        }
        if (match(p, TOK_ARROW)) {
            Token var_tok = advance(p);
            if (var_tok.kind != TOK_IDENT) {
                error(p, "expected exception variable name after '=>'", var_tok.line, var_tok.col);
                return NULL;
            }
            rc->rescue_clause.exception_var = var_tok.sval;
        }
        skip_terminators(p);
        rc->rescue_clause.body = parse_body(p, 0);
        n->begin_stmt.rescues = nodelist_append(p->arena, n->begin_stmt.rescues, rc);
    }
    if (match(p, TOK_ELSE)) {
        skip_terminators(p);
        n->begin_stmt.else_body = parse_body(p, 0);
    }
    if (match(p, TOK_ENSURE)) {
        skip_terminators(p);
        n->begin_stmt.ensure_body = parse_body(p, 0);
    }
    return n;
}

static Node *parse_assignment_target_elem(Parser *p) {
    Token t = peek(p);
    Span s = tok_span(t);

    if (t.kind == TOK_LPAREN) {
        advance(p);
        Node *group = parse_assignment_target_elem(p);
        if (check(p, TOK_COMMA))
            group = parse_expr_list(p, group, 7, 1);
        expect(p, TOK_RPAREN, "expected ')'");
        return group;
    }

    if (t.kind == TOK_STAR) {
        advance(p);
        Token name_tok = advance(p);
        if (name_tok.kind != TOK_IDENT) {
            error(p, "expected identifier after '*'", name_tok.line, name_tok.col);
            return NULL;
        }
        Node *param = node_new(p->arena, NODE_PARAM, s);
        param->param.splat = 1;
        param->param.name = name_tok.sval;
        return param;
    }

    return parse_expr(p, 7);
}

static Node *parse_expr_list(Parser *p, Node *first, int elem_min_bp, int assignment_target) {
    if (!first) return NULL;
    if (!check(p, TOK_COMMA)) return first;

    Node *list = node_new(p->arena, NODE_ARRAY, first->span);
    NodeList *elems = NULL;
    elems = nodelist_append(p->arena, elems, first);
    while (match(p, TOK_COMMA)) {
        Node *elem = assignment_target ? parse_assignment_target_elem(p) : parse_expr(p, elem_min_bp);
        if (elem) elems = nodelist_append(p->arena, elems, elem);
    }
    list->array.elements = elems;
    return list;
}

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
        skip_terminators(p);
        Node *body = parse_body(p, 0);
        Node *n = wrap_rescue_ensure(p, s, body);
        if (!n) return NULL;
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

        const char *def_name = method_name_from_token(p, name_tok);
        if (!def_name) {
            error(p, "expected method name after 'def'", name_tok.line, name_tok.col);
            return NULL;
        }
        if ((name_tok.kind == TOK_IDENT || name_tok.kind == TOK_CONST) &&
            (check(p, TOK_QUESTION) || check(p, TOK_BANG))) {
            Token suffix = advance(p);
            size_t nlen = strlen(def_name);
            char *buf = arena_alloc(p->arena, nlen + 2);
            memcpy(buf, def_name, nlen);
            buf[nlen] = suffix.kind == TOK_QUESTION ? '?' : '!';
            buf[nlen + 1] = '\0';
            def_name = buf;
        }
        n->def.name = def_name;

        if (match(p, TOK_LPAREN)) {
            n->def.params = parse_params(p);
            expect(p, TOK_RPAREN, "expected ')'");
        }

        if (match(p, TOK_EQ)) {
            /* Endless method: def foo = expr */
            Node *expr = parse_expr(p, 0);
            Node *body = node_new(p->arena, NODE_BODY, s);
            body->body.stmts = expr ? nodelist_append(p->arena, NULL, expr) : NULL;
            n->def.body = body;
            return n;
        }

        skip_terminators(p);
        Node *def_body = parse_body(p, 0);
        if (check(p, TOK_RESCUE) || check(p, TOK_ENSURE)) {
            def_body = wrap_rescue_ensure(p, s, def_body);
            if (!def_body) return NULL;
        }
        n->def.body = def_body;
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    if (t.kind == TOK_ALIAS) {
        advance(p);
        Token new_tok = advance(p);
        const char *new_name = method_name_from_token(p, new_tok);
        if (!new_name) {
            error(p, "expected new method name after 'alias'", new_tok.line, new_tok.col);
            return NULL;
        }
        if ((new_tok.kind == TOK_IDENT || new_tok.kind == TOK_CONST) &&
            (check(p, TOK_QUESTION) || check(p, TOK_BANG))) {
            Token suffix = advance(p);
            size_t nlen = strlen(new_name);
            char *buf = arena_alloc(p->arena, nlen + 2);
            memcpy(buf, new_name, nlen);
            buf[nlen] = suffix.kind == TOK_QUESTION ? '?' : '!';
            buf[nlen + 1] = '\0';
            new_name = buf;
        }

        Token old_tok = advance(p);
        const char *old_name = method_name_from_token(p, old_tok);
        if (!old_name) {
            error(p, "expected existing method name after alias target", old_tok.line, old_tok.col);
            return NULL;
        }
        if ((old_tok.kind == TOK_IDENT || old_tok.kind == TOK_CONST) &&
            (check(p, TOK_QUESTION) || check(p, TOK_BANG))) {
            Token suffix = advance(p);
            size_t nlen = strlen(old_name);
            char *buf = arena_alloc(p->arena, nlen + 2);
            memcpy(buf, old_name, nlen);
            buf[nlen] = suffix.kind == TOK_QUESTION ? '?' : '!';
            buf[nlen + 1] = '\0';
            old_name = buf;
        }

        Node *n = node_new(p->arena, NODE_ALIAS, s);
        n->alias_stmt.new_name = new_name;
        n->alias_stmt.old_name = old_name;
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

    if (t.kind == TOK_CASE) {
        advance(p);
        Node *n = node_new(p->arena, NODE_CASE, s);
        /* Optional subject — absent if next token is a terminator */
        if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF))
            n->case_stmt.subject = parse_expr(p, 0);
        skip_terminators(p);

        NodeList *whens = NULL;
        while (check(p, TOK_WHEN)) {
            Span ws = tok_span(peek(p));
            advance(p);
            Node *w = node_new(p->arena, NODE_WHEN, ws);
            NodeList *patterns = NULL;
            Node *pat = parse_expr(p, 0);
            if (pat) patterns = nodelist_append(p->arena, patterns, pat);
            while (match(p, TOK_COMMA)) {
                skip_terminators(p);
                Node *more = parse_expr(p, 0);
                if (more) patterns = nodelist_append(p->arena, patterns, more);
            }
            w->when_clause.patterns = patterns;
            if (!match(p, TOK_THEN)) skip_terminators(p);
            w->when_clause.body = parse_body(p, 0);
            whens = nodelist_append(p->arena, whens, w);
        }
        n->case_stmt.whens = whens;

        if (match(p, TOK_ELSE)) {
            skip_terminators(p);
            n->case_stmt.else_body = parse_body(p, 0);
        }
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    if (t.kind == TOK_MODULE) {
        advance(p);
        Token name_tok = advance(p);
        if (name_tok.kind != TOK_CONST) {
            error(p, "expected module name after 'module'", name_tok.line, name_tok.col);
            return NULL;
        }
        Node *n = node_new(p->arena, NODE_MODULE, s);
        n->klass.name = name_tok.sval;
        skip_terminators(p);
        n->klass.body = parse_body(p, 0);
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    if (t.kind == TOK_STAR) {
        Node *lhs = parse_assignment_target_elem(p);
        if (check(p, TOK_COMMA))
            lhs = parse_expr_list(p, lhs, 7, 1);
        if (!match(p, TOK_EQ)) {
            Token t2 = peek(p);
            error(p, "expected '=' after assignment target", t2.line, t2.col);
            return NULL;
        }
        Node *rhs_first = parse_expr(p, 0);
        Node *rhs = parse_expr_list(p, rhs_first, 0, 0);
        Node *n = node_new(p->arena, NODE_ASSIGN, s);
        n->assign.target = lhs;
        n->assign.value = rhs;
        return n;
    }

    Node *expr = parse_expr(p, 0);
    if (!expr) return NULL;

    if (check(p, TOK_COMMA)) {
        Node *lhs = parse_expr_list(p, expr, 7, 1);
        if (!match(p, TOK_EQ)) {
            Token t2 = peek(p);
            error(p, "expected '=' after multiple assignment target", t2.line, t2.col);
            return NULL;
        }
        Node *rhs_first = parse_expr(p, 0);
        Node *rhs = parse_expr_list(p, rhs_first, 0, 0);
        Node *n = node_new(p->arena, NODE_ASSIGN, s);
        n->assign.target = lhs;
        n->assign.value = rhs;
        return n;
    }

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
            t.kind == TOK_ELSIF || t.kind == TOK_ENSURE || t.kind == TOK_RESCUE ||
            t.kind == TOK_WHEN)
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
