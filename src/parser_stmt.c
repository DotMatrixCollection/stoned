#include "parser_internal.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Pattern matching parser                                              */
/* ------------------------------------------------------------------ */

static Node *parse_pattern(Parser *p);

static Node *parse_array_pattern(Parser *p, Span s) {
    /* '[' already consumed */
    Node *n = node_new(p->arena, NODE_PATTERN_ARRAY, s);
    n->pattern_collection.elements = NULL;
    n->pattern_collection.has_rest = 0;
    n->pattern_collection.has_pre_rest = 0;
    n->pattern_collection.rest_name = NULL;
    n->pattern_collection.pre_rest_name = NULL;
    int first = 1;
    while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
        skip_terminators(p);
        if (check(p, TOK_RBRACKET)) break;
        if (check(p, TOK_STAR)) {
            advance(p);
            const char *rname = NULL;
            if (check(p, TOK_IDENT)) {
                Token rt = advance(p);
                rname = rt.sval;
                lexer_mark_local(&p->lexer, rt.sval);
            }
            if (first) {
                /* Leading * — may be find pattern prefix */
                n->pattern_collection.has_pre_rest = 1;
                n->pattern_collection.pre_rest_name = rname;
            } else {
                n->pattern_collection.has_rest = 1;
                n->pattern_collection.rest_name = rname;
            }
            first = 0;
            if (check(p, TOK_COMMA)) { advance(p); continue; }
            break;
        }
        first = 0;
        Node *elem = parse_pattern(p);
        if (elem) n->pattern_collection.elements = nodelist_append(p->arena, n->pattern_collection.elements, elem);
        if (!match(p, TOK_COMMA)) break;
    }
    skip_terminators(p);
    match(p, TOK_RBRACKET);
    return n;
}

static Node *parse_hash_pattern(Parser *p, Span s) {
    /* '{' already consumed */
    Node *n = node_new(p->arena, NODE_PATTERN_HASH, s);
    n->pattern_collection.elements = NULL;
    n->pattern_collection.has_rest = 0;
    n->pattern_collection.rest_name = NULL;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        skip_terminators(p);
        if (check(p, TOK_RBRACE)) break;
        /* **rest or **nil */
        if (check(p, TOK_STAR2)) {
            advance(p);
            n->pattern_collection.has_rest = 1;
            if (check(p, TOK_NIL)) { advance(p); n->pattern_collection.rest_name = NULL; }
            else if (check(p, TOK_IDENT)) {
                Token rt = advance(p);
                n->pattern_collection.rest_name = rt.sval;
                lexer_mark_local(&p->lexer, rt.sval);
            }
            if (check(p, TOK_COMMA)) { advance(p); continue; }
            break;
        }
        /* key: pattern  or  key:  (shorthand binding) */
        Token key_tok = peek(p);
        const char *key_name = NULL;
        if (key_tok.kind == TOK_IDENT || key_tok.kind == TOK_CONST) {
            advance(p);
            key_name = key_tok.sval;
        } else if (key_tok.kind == TOK_SYMBOL) {
            advance(p);
            key_name = key_tok.sval;
        } else { advance(p); key_name = "?"; }
        /* Store the key as a symbol node */
        Node *key_node = node_new(p->arena, NODE_SYMBOL, tok_span(key_tok));
        key_node->sval = key_name;
        n->pattern_collection.elements = nodelist_append(p->arena, n->pattern_collection.elements, key_node);
        /* Colon consumed as part of symbol key, or match it */
        if (check(p, TOK_COLON)) advance(p);
        /* Pattern for this key: if next is comma/} it's a shorthand capture */
        Node *val_pat = NULL;
        if (!check(p, TOK_COMMA) && !check(p, TOK_RBRACE) && !check(p, TOK_NEWLINE)) {
            val_pat = parse_pattern(p);
        } else {
            /* Shorthand: {name:} binds to variable `name` */
            Node *cap = node_new(p->arena, NODE_PATTERN_CAPTURE, tok_span(key_tok));
            cap->pattern_capture.pattern = NULL;
            cap->pattern_capture.name = key_name;
            lexer_mark_local(&p->lexer, key_name);
            val_pat = cap;
        }
        n->pattern_collection.elements = nodelist_append(p->arena, n->pattern_collection.elements, val_pat);
        if (!match(p, TOK_COMMA)) break;
    }
    skip_terminators(p);
    match(p, TOK_RBRACE);
    return n;
}

static Node *parse_pattern(Parser *p) {
    skip_terminators(p);
    Token t = peek(p);
    Span s = tok_span(t);
    Node *pat = NULL;

    /* Pin operator ^var */
    if (t.kind == TOK_CARET) {
        advance(p);
        Token vt = advance(p);
        pat = node_new(p->arena, NODE_PATTERN_PIN, s);
        pat->sval = vt.sval;
    }
    /* Array pattern */
    else if (t.kind == TOK_LBRACKET) {
        advance(p);
        pat = parse_array_pattern(p, s);
    }
    /* Hash pattern */
    else if (t.kind == TOK_LBRACE) {
        advance(p);
        pat = parse_hash_pattern(p, s);
    }
    /* Nil/True/False literals */
    else if (t.kind == TOK_NIL)   { advance(p); pat = node_new(p->arena, NODE_NIL,   s); }
    else if (t.kind == TOK_TRUE)  { advance(p); pat = node_new(p->arena, NODE_TRUE,  s); }
    else if (t.kind == TOK_FALSE) { advance(p); pat = node_new(p->arena, NODE_FALSE, s); }
    /* Integer literal (possibly negative) */
    else if (t.kind == TOK_INT) {
        advance(p);
        pat = node_new(p->arena, NODE_INT, s);
        pat->ival = t.ival;
    }
    else if (t.kind == TOK_MINUS) {
        /* negative literal */
        advance(p);
        Token nt = peek(p);
        if (nt.kind == TOK_INT) {
            advance(p);
            pat = node_new(p->arena, NODE_INT, s);
            pat->ival = -nt.ival;
        } else if (nt.kind == TOK_FLOAT) {
            advance(p);
            pat = node_new(p->arena, NODE_FLOAT, s);
            pat->fval = -nt.fval;
        }
    }
    else if (t.kind == TOK_FLOAT) {
        advance(p);
        pat = node_new(p->arena, NODE_FLOAT, s);
        pat->fval = t.fval;
    }
    /* String literal */
    else if (t.kind == TOK_INTERP_BEG || t.kind == TOK_STRING) {
        pat = parse_expr(p, 0);
    }
    /* Symbol */
    else if (t.kind == TOK_SYMBOL) {
        advance(p);
        pat = node_new(p->arena, NODE_SYMBOL, s);
        pat->sval = t.sval;
    }
    /* Constant (type check) */
    else if (t.kind == TOK_CONST) {
        pat = parse_expr(p, 0);  /* handles Foo::Bar etc. */
    }
    /* Bare identifier — capture variable */
    else if (t.kind == TOK_IDENT) {
        advance(p);
        /* `_` is wildcard, others are captures */
        if (strcmp(t.sval, "_") == 0) {
            pat = node_new(p->arena, NODE_NIL, s); /* wildcard — any value matches */
            pat->kind = NODE_NIL;
            /* reuse nil node as wildcard marker by giving it a special sval */
            pat = node_new(p->arena, NODE_PATTERN_CAPTURE, s);
            pat->pattern_capture.pattern = NULL;
            pat->pattern_capture.name = NULL; /* wildcard: don't bind */
        } else {
            lexer_mark_local(&p->lexer, t.sval);
            pat = node_new(p->arena, NODE_PATTERN_CAPTURE, s);
            pat->pattern_capture.pattern = NULL;  /* match-all capture */
            pat->pattern_capture.name = t.sval;
        }
    }
    else {
        pat = parse_expr(p, 0);
    }

    /* => capture binding: `pattern => var` */
    if (pat && check(p, TOK_ARROW)) {
        advance(p);
        Token vt = advance(p);
        lexer_mark_local(&p->lexer, vt.sval);
        Node *cap = node_new(p->arena, NODE_PATTERN_CAPTURE, s);
        cap->pattern_capture.pattern = pat;
        cap->pattern_capture.name = vt.sval;
        pat = cap;
    }

    /* | alternation */
    if (pat && check(p, TOK_PIPE)) {
        advance(p);
        Node *right = parse_pattern(p);
        Node *or_node = node_new(p->arena, NODE_PATTERN_OR, s);
        or_node->pattern_or.left = pat;
        or_node->pattern_or.right = right;
        pat = or_node;
    }

    return pat;
}

static void mark_assign_targets_stmt(Parser *p, Node *target) {
    if (!target) return;
    if (target->kind == NODE_LVAR)
        lexer_mark_local(&p->lexer, target->sval);
    else if (target->kind == NODE_ARRAY)
        for (NodeList *el = target->array.elements; el; el = el->next)
            mark_assign_targets_stmt(p, el->node);
}

static Node *parse_expr_list(Parser *p, Node *first, int elem_min_bp, int assignment_target);

static Token peek_next_token_stmt(Parser *p) {
    Parser copy = *p;
    advance(&copy);
    return peek(&copy);
}

static const char *parse_const_path_name(Parser *p) {
    (void)match(p, TOK_COLON2);
    Token name_tok = advance(p);
    if (name_tok.kind != TOK_CONST) {
        error(p, "expected constant name", name_tok.line, name_tok.col);
        return NULL;
    }

    size_t len = strlen(name_tok.sval);
    char *buf = arena_alloc(p->arena, len + 1);
    memcpy(buf, name_tok.sval, len + 1);

    while (match(p, TOK_COLON2)) {
        Token part = advance(p);
        if (part.kind != TOK_CONST) {
            error(p, "expected constant name after '::'", part.line, part.col);
            return NULL;
        }
        size_t plen = strlen(part.sval);
        char *next = arena_alloc(p->arena, len + 2 + plen + 1);
        memcpy(next, buf, len);
        memcpy(next + len, "::", 2);
        memcpy(next + len + 2, part.sval, plen + 1);
        buf = next;
        len += 2 + plen;
    }

    return buf;
}

static const char *join_const_parts(Parser *p, const char **parts, int count) {
    if (count <= 0) return NULL;
    size_t len = strlen(parts[0]);
    char *buf = arena_alloc(p->arena, len + 1);
    memcpy(buf, parts[0], len + 1);

    for (int i = 1; i < count; i++) {
        size_t plen = strlen(parts[i]);
        char *next = arena_alloc(p->arena, len + 2 + plen + 1);
        memcpy(next, buf, len);
        memcpy(next + len, "::", 2);
        memcpy(next + len + 2, parts[i], plen + 1);
        buf = next;
        len += 2 + plen;
    }
    return buf;
}

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
        case TOK_AMP: return "&";
        case TOK_PIPE: return "|";
        case TOK_CARET: return "^";
        case TOK_TILDE: return "~";
        case TOK_MATCH: return "=~";
        case TOK_NMATCH: return "!~";
        case TOK_BANG: return "!";
        case TOK_LBRACKET:
            if (match(p, TOK_RBRACKET)) {
                if (match(p, TOK_EQ)) return "[]=";
                return "[]";
            }
            error(p, "expected ']' after '[' in method name", tok.line, tok.col);
            return NULL;
        /* Keywords that can legally be used as method names in Ruby */
        case TOK_ALIAS:     return "alias";
        case TOK_BEGIN:     return "begin";
        case TOK_CLASS:     return "class";
        case TOK_DEF:       return "def";
        case TOK_DO:        return "do";
        case TOK_ELSE:      return "else";
        case TOK_END:       return "end";
        case TOK_FOR:       return "for";
        case TOK_IF:        return "if";
        case TOK_IN:        return "in";
        case TOK_MODULE:    return "module";
        case TOK_NEXT:      return "next";
        case TOK_NIL:       return "nil";
        case TOK_NOT:       return "not";
        case TOK_OR:        return "or";
        case TOK_RESCUE:    return "rescue";
        case TOK_RETURN:    return "return";
        case TOK_SELF:      return "self";
        case TOK_SUPER:     return "super";
        case TOK_THEN:      return "then";
        case TOK_TRUE:      return "true";
        case TOK_FALSE:     return "false";
        case TOK_UNLESS:    return "unless";
        case TOK_UNTIL:     return "until";
        case TOK_WHEN:      return "when";
        case TOK_WHILE:     return "while";
        case TOK_YIELD:     return "yield";
        default:
            return NULL;
    }
}

/* Attach rescue/ensure clauses to an already-parsed body, producing a NODE_BEGIN.
   Called for both explicit begin...end and implicit method-level rescue. */
Node *wrap_rescue_ensure(Parser *p, Span s, Node *body) {
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
            lexer_mark_local(&p->lexer, var_tok.sval);
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
        if (assignment_target &&
            (check(p, TOK_EQ) || check(p, TOK_NEWLINE) || check(p, TOK_SEMICOLON) ||
             check(p, TOK_RPAREN) || check(p, TOK_EOF) || check(p, TOK_IN)))
            break;
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
        /* Raise command_arg_depth so 'do' is not consumed as a block by parse_expr */
        p->command_arg_depth++;
        Node *cond = parse_expr(p, 0);
        p->command_arg_depth--;
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
        t = peek(p);
        if (t.kind == TOK_WHILE || t.kind == TOK_UNTIL) {
            advance(p);
            NodeKind kind = (t.kind == TOK_WHILE) ? NODE_WHILE : NODE_UNTIL;
            Node *cond = parse_expr(p, 0);
            Node *loop = node_new(p->arena, kind, s);
            loop->loop.cond = cond;
            loop->loop.body = n;
            loop->loop.post_test = 1;
            return loop;
        }
        return n;
    }

    if (t.kind == TOK_DEF) {
        advance(p);
        Token name_tok = advance(p);
        Node *n = node_new(p->arena, NODE_DEF, s);
        n->def.recv = NULL;

        if (name_tok.kind == TOK_CONST && check(p, TOK_COLON2)) {
            const char *parts[64];
            int nparts = 0;
            parts[nparts++] = name_tok.sval;
            while (match(p, TOK_COLON2) && nparts < 64) {
                Token part = advance(p);
                if (part.kind == TOK_IDENT) {
                    /* def Foo::bar — bar is the method name (lowercase ident) */
                    parts[nparts++] = part.sval;
                    break;
                }
                if (part.kind != TOK_CONST) {
                    error(p, "expected constant or method name after '::'", part.line, part.col);
                    return NULL;
                }
                parts[nparts++] = part.sval;
            }
            if (nparts >= 2) {
                Node *recv_node = node_new(p->arena, NODE_CONST, tok_span(name_tok));
                recv_node->sval = join_const_parts(p, parts, nparts - 1);
                n->def.recv = recv_node;
                Token synthetic = name_tok;
                synthetic.sval = parts[nparts - 1];
                name_tok = synthetic;
            }
        }

        if (check(p, TOK_DOT)) {
            Node *recv_node = NULL;
            if (name_tok.kind == TOK_SELF) {
                recv_node = node_new(p->arena, NODE_SELF, tok_span(name_tok));
            } else if (name_tok.kind == TOK_IDENT) {
                recv_node = node_new(p->arena, NODE_LVAR, tok_span(name_tok));
                recv_node->sval = name_tok.sval;
            } else if (name_tok.kind == TOK_CONST) {
                recv_node = node_new(p->arena, NODE_CONST, tok_span(name_tok));
                recv_node->sval = name_tok.sval;
            } else if (name_tok.kind == TOK_IVAR) {
                recv_node = node_new(p->arena, NODE_IVAR, tok_span(name_tok));
                recv_node->sval = name_tok.sval;
            } else if (name_tok.kind == TOK_CVAR) {
                recv_node = node_new(p->arena, NODE_CVAR, tok_span(name_tok));
                recv_node->sval = name_tok.sval;
            } else if (name_tok.kind == TOK_GVAR) {
                recv_node = node_new(p->arena, NODE_GVAR, tok_span(name_tok));
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
        Token def_suffix_tok = peek(p);
        /* def -@ / def +@ / def !@ / def ~@ — unary operator definitions:
           the operator token is immediately followed by a bare '@' (TOK_IVAR with empty sval) */
        if (def_suffix_tok.kind == TOK_IVAR && def_suffix_tok.sval && def_suffix_tok.sval[0] == '\0' &&
            (strcmp(def_name, "-") == 0 || strcmp(def_name, "+") == 0 ||
             strcmp(def_name, "!") == 0 || strcmp(def_name, "~") == 0) &&
            token_adjacent(name_tok, def_suffix_tok)) {
            advance(p); /* consume the bare '@' */
            size_t nlen = strlen(def_name);
            char *buf = arena_alloc(p->arena, nlen + 2);
            memcpy(buf, def_name, nlen);
            buf[nlen] = '@'; buf[nlen + 1] = '\0';
            def_name = buf;
            def_suffix_tok = peek(p);
        }
        int setter_suffix = 0;
        if ((name_tok.kind == TOK_IDENT || name_tok.kind == TOK_CONST) &&
            def_suffix_tok.kind == TOK_EQ) {
            Token after_eq = peek_next_token_stmt(p);
            setter_suffix = token_adjacent(name_tok, def_suffix_tok) && after_eq.kind == TOK_LPAREN;
        }
        if ((name_tok.kind == TOK_IDENT || name_tok.kind == TOK_CONST) &&
            ((((def_suffix_tok.kind == TOK_QUESTION || def_suffix_tok.kind == TOK_BANG) &&
               token_adjacent(name_tok, def_suffix_tok))) || setter_suffix)) {
            Token suffix = advance(p);
            size_t nlen = strlen(def_name);
            char *buf = arena_alloc(p->arena, nlen + 2);
            memcpy(buf, def_name, nlen);
            buf[nlen] = suffix.kind == TOK_QUESTION ? '?' : (suffix.kind == TOK_BANG ? '!' : '=');
            buf[nlen + 1] = '\0';
            def_name = buf;
        }
        n->def.name = def_name;

        if (match(p, TOK_LPAREN)) {
            n->def.params = parse_params(p);
            expect(p, TOK_RPAREN, "expected ')'");
        } else {
            /* Unparenthesized param list: def foo x, y */
            Token nxt = peek(p);
            int same_line = (nxt.line == name_tok.line);
            int looks_like_param = same_line && (
                nxt.kind == TOK_IDENT || nxt.kind == TOK_CONST ||
                nxt.kind == TOK_STAR  || nxt.kind == TOK_STAR2 ||
                nxt.kind == TOK_AMP);
            if (looks_like_param)
                n->def.params = parse_params_unparen(p, name_tok.line);
        }
        /* Mark all param names as local variables in the lexer's scope */
        for (NodeList *pl = n->def.params; pl; pl = pl->next) {
            if (!pl->node) continue;
            if (pl->node->kind == NODE_PARAM && pl->node->param.name)
                lexer_mark_local(&p->lexer, pl->node->param.name);
            else if (pl->node->kind == NODE_ARRAY)
                for (NodeList *el = pl->node->array.elements; el; el = el->next)
                    if (el->node && el->node->kind == NODE_PARAM && el->node->param.name)
                        lexer_mark_local(&p->lexer, el->node->param.name);
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
        Token new_suffix_tok = peek(p);
        if ((new_tok.kind == TOK_IDENT || new_tok.kind == TOK_CONST) &&
            (new_suffix_tok.kind == TOK_QUESTION || new_suffix_tok.kind == TOK_BANG) &&
            token_adjacent(new_tok, new_suffix_tok)) {
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
        Token old_suffix_tok = peek(p);
        if ((old_tok.kind == TOK_IDENT || old_tok.kind == TOK_CONST) &&
            (old_suffix_tok.kind == TOK_QUESTION || old_suffix_tok.kind == TOK_BANG) &&
            token_adjacent(old_tok, old_suffix_tok)) {
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
        if (match(p, TOK_LSHIFT) || (match(p, TOK_LT) && match(p, TOK_LT))) {
            Node *recv = parse_expr(p, 0);
            if (!recv) return NULL;
            Node *n = node_new(p->arena, NODE_SCLASS, s);
            n->sclass.recv = recv;
            skip_terminators(p);
            n->sclass.body = parse_body(p, 0);
            expect(p, TOK_END, "expected 'end'");
            return n;
        }
        const char *name = parse_const_path_name(p);
        if (!name) return NULL;
        Node *n = node_new(p->arena, NODE_CLASS, s);
        n->klass.name = name;
        n->klass.superclass = NULL;

        if (match(p, TOK_LT)) {
            n->klass.superclass = parse_expr(p, 0);
            if (!n->klass.superclass) return NULL;
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

        /* Pattern matching: case/in (Ruby 3+) */
        if (check(p, TOK_IN)) {
            n->kind = NODE_PATCASE;
            n->patcase.subject = n->case_stmt.subject;
            NodeList *ins = NULL;
            while (check(p, TOK_IN)) {
                Span ws = tok_span(peek(p));
                advance(p);
                Node *pat = parse_pattern(p);
                Node *guard = NULL;
                if (check(p, TOK_IF)) { advance(p); guard = parse_expr(p, 0); }
                if (!match(p, TOK_THEN)) skip_terminators(p);
                Node *body = parse_body(p, 0);
                Node *in_node = node_new(p->arena, NODE_IN, ws);
                in_node->in_clause.pattern = pat;
                in_node->in_clause.guard = guard;
                in_node->in_clause.body = body;
                ins = nodelist_append(p->arena, ins, in_node);
            }
            n->patcase.ins = ins;
            if (match(p, TOK_ELSE)) {
                skip_terminators(p);
                n->patcase.else_body = parse_body(p, 0);
            }
            expect(p, TOK_END, "expected 'end'");
            return n;
        }

        if (match(p, TOK_ELSE)) {
            skip_terminators(p);
            n->case_stmt.else_body = parse_body(p, 0);
        }
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    if (t.kind == TOK_MODULE) {
        advance(p);
        const char *name = parse_const_path_name(p);
        if (!name) return NULL;
        Node *n = node_new(p->arena, NODE_MODULE, s);
        n->klass.name = name;
        skip_terminators(p);
        n->klass.body = parse_body(p, 0);
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    if (t.kind == TOK_FOR) {
        advance(p);
        Node *target = parse_assignment_target_elem(p);
        if (check(p, TOK_COMMA))
            target = parse_expr_list(p, target, 7, 1);
        if (!target) {
            Token t2 = peek(p);
            error(p, "expected loop variable after 'for'", t2.line, t2.col);
            return NULL;
        }
        if (!match(p, TOK_IN)) {
            Token t2 = peek(p);
            error(p, "expected 'in' after for target", t2.line, t2.col);
            return NULL;
        }
        p->command_arg_depth++;
        Node *iterable = parse_expr(p, 0);
        p->command_arg_depth--;
        Node *n = node_new(p->arena, NODE_FOR, s);
        n->for_loop.target = target;
        n->for_loop.iterable = iterable;
        mark_assign_targets_stmt(p, target);
        match(p, TOK_DO);
        skip_terminators(p);
        n->for_loop.body = parse_body(p, 0);
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    Node *expr = NULL;

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
        mark_assign_targets_stmt(p, lhs);
        expr = n;
    } else {
        expr = parse_expr(p, 0);
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
            mark_assign_targets_stmt(p, lhs);
            expr = n;
        }
    }

    t = peek(p);
    if (t.kind == TOK_RESCUE) {
        advance(p);
        Node *rescue_body = parse_expr(p, 0);
        Node *rc = node_new(p->arena, NODE_RESCUE, s);
        rc->rescue_clause.body = rescue_body;
        Node *begin_node = node_new(p->arena, NODE_BEGIN, s);
        Node *body = node_new(p->arena, NODE_BODY, s);
        if (expr && (expr->kind == NODE_ASSIGN || expr->kind == NODE_OP_ASSIGN)) {
            Node *rhs = expr->kind == NODE_ASSIGN ? expr->assign.value : expr->binop.right;
            body->body.stmts = rhs ? nodelist_append(p->arena, NULL, rhs) : NULL;
            begin_node->begin_stmt.body = body;
            begin_node->begin_stmt.rescues = nodelist_append(p->arena, NULL, rc);
            if (expr->kind == NODE_ASSIGN)
                expr->assign.value = begin_node;
            else
                expr->binop.right = begin_node;
        } else {
            body->body.stmts = expr ? nodelist_append(p->arena, NULL, expr) : NULL;
            begin_node->begin_stmt.body = body;
            begin_node->begin_stmt.rescues = nodelist_append(p->arena, NULL, rc);
            expr = begin_node;
        }
        t = peek(p);
    }

    if (t.kind == TOK_IF || t.kind == TOK_UNLESS) {
        advance(p);
        skip_terminators(p);
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
        skip_terminators(p);
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
            t.kind == TOK_WHEN || t.kind == TOK_IN)
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
