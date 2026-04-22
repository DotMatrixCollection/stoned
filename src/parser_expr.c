#include "parser_internal.h"
#include "rope.h"

#include <string.h>

typedef struct { int lbp; int rbp; } BP;

static NodeList *parse_command_args(Parser *p) {
    NodeList *args = NULL;
    Node *first = parse_expr(p, 0);
    if (first) args = nodelist_append(p->arena, args, first);

    while (p->allow_command_arg_commas && match(p, TOK_COMMA)) {
        Node *arg = parse_expr(p, 0);
        if (arg) args = nodelist_append(p->arena, args, arg);
    }

    return args;
}

static BP infix_bp(TokenKind k) {
    switch (k) {
        case TOK_OR: return (BP){2, 3};
        case TOK_AND: return (BP){4, 5};
        case TOK_EQ:
        case TOK_PLUS_EQ: case TOK_MINUS_EQ: case TOK_STAR_EQ:
        case TOK_SLASH_EQ: case TOK_PERCENT_EQ: case TOK_STAR2_EQ:
        case TOK_AMP_EQ: case TOK_PIPE_EQ: case TOK_CARET_EQ:
        case TOK_LSHIFT_EQ: case TOK_RSHIFT_EQ:
        case TOK_AMP2_EQ: case TOK_PIPE2_EQ:
            return (BP){6, 6};
        case TOK_QUESTION: return (BP){8, 7};
        case TOK_PIPE2: return (BP){10, 11};
        case TOK_AMP2: return (BP){12, 13};
        case TOK_EQ2: case TOK_NEQ: case TOK_EQ3: case TOK_MATCH: case TOK_NMATCH:
            return (BP){14, 15};
        case TOK_LT: case TOK_LEQ: case TOK_GT: case TOK_GEQ: case TOK_SPACESHIP:
            return (BP){16, 17};
        case TOK_PIPE: return (BP){18, 19};
        case TOK_CARET: return (BP){20, 21};
        case TOK_AMP: return (BP){22, 23};
        case TOK_LSHIFT: case TOK_RSHIFT: return (BP){24, 25};
        case TOK_PLUS: case TOK_MINUS: return (BP){26, 27};
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return (BP){28, 29};
        case TOK_STAR2: return (BP){32, 31};
        case TOK_DOT: case TOK_COLON2: return (BP){40, 41};
        case TOK_LBRACKET: return (BP){42, 43};
        default: return (BP){0, 0};
    }
}

static int prefix_bp(TokenKind k) {
    switch (k) {
        case TOK_NOT: return 6;
        case TOK_BANG:
        case TOK_MINUS:
        case TOK_TILDE:
        case TOK_PLUS:
            return 30;
        default:
            return 0;
    }
}

static Node *parse_hash_key(Parser *p, int *used_label) {
    *used_label = 0;
    Token t = peek(p);
    if ((t.kind == TOK_IDENT || t.kind == TOK_CONST) && p->allow_command_arg_commas == 0) {
        Span s = tok_span(t);
        advance(p);
        if (match(p, TOK_COLON)) {
            Node *key = node_new(p->arena, NODE_SYMBOL, s);
            key->sval = t.sval;
            *used_label = 1;
            return key;
        }
        p->panic = 1;
        error(p, "expected ':' or '=>' in hash literal", t.line, t.col);
        return NULL;
    }
    return parse_expr(p, 0);
}

Node *parse_expr(Parser *p, int min_bp) {
    Node *left = parse_primary(p);
    if (!left) return NULL;

    while (1) {
        Token op = peek(p);

        if (op.kind == TOK_QUESTION) {
            BP bp = infix_bp(TOK_QUESTION);
            if (bp.lbp < min_bp) break;
            advance(p);
            Node *then_expr = parse_expr(p, 0);
            expect(p, TOK_COLON, "expected ':' in ternary");
            Node *else_expr = parse_expr(p, bp.rbp);
            Node *n = node_new(p->arena, NODE_IF, left->span);
            n->cond.cond = left;
            n->cond.then_body = then_expr;
            n->cond.else_body = else_expr;
            left = n;
            continue;
        }

        BP bp = infix_bp(op.kind);
        if (bp.lbp == 0 || bp.lbp < min_bp) break;
        advance(p);

        if (op.kind == TOK_LBRACKET) {
            NodeList *args = NULL;
            if (!check(p, TOK_RBRACKET)) {
                Node *first = parse_expr(p, 0);
                if (first) args = nodelist_append(p->arena, args, first);
                while (match(p, TOK_COMMA)) {
                    Node *arg = parse_expr(p, 0);
                    if (arg) args = nodelist_append(p->arena, args, arg);
                }
            }
            expect(p, TOK_RBRACKET, "expected ']'");
            Node *call = node_new(p->arena, NODE_CALL, left->span);
            call->call.recv = left;
            call->call.method = "[]";
            call->call.args = args;
            call->call.block = NULL;
            left = call;
            continue;
        }

        if (op.kind == TOK_EQ) {
            Node *right = parse_expr(p, bp.rbp);
            if (left->kind == NODE_CALL && strcmp(left->call.method, "[]") == 0 && left->call.recv) {
                left->call.method = "[]=";
                left->call.args = nodelist_append(p->arena, left->call.args, right);
                continue;
            }
            if (left->kind == NODE_CALL && left->call.recv && !left->call.args) {
                size_t nlen = strlen(left->call.method);
                char *new_name = arena_alloc(p->arena, nlen + 2);
                memcpy(new_name, left->call.method, nlen);
                new_name[nlen] = '=';
                new_name[nlen + 1] = '\0';
                left->call.method = new_name;
                left->call.args = nodelist_append(p->arena, NULL, right);
                continue;
            }
            Node *n = node_new(p->arena, NODE_ASSIGN, left->span);
            n->assign.target = left;
            n->assign.value = right;
            left = n;
            continue;
        }

        if (op.kind >= TOK_PLUS_EQ && op.kind <= TOK_PIPE2_EQ) {
            Node *right = parse_expr(p, bp.rbp);
            Node *n = node_new(p->arena, NODE_OP_ASSIGN, left->span);
            n->binop.op = token_kind_name(op.kind);
            n->binop.left = left;
            n->binop.right = right;
            left = n;
            continue;
        }

        if (op.kind == TOK_DOT || op.kind == TOK_COLON2) {
            Token name_tok = advance(p);
            const char *method_name = NULL;
            if (name_tok.kind == TOK_IDENT || name_tok.kind == TOK_CONST) {
                method_name = name_tok.sval;
            } else {
                switch (name_tok.kind) {
                    case TOK_CLASS: method_name = "class"; break;
                    case TOK_NIL: method_name = "nil"; break;
                    case TOK_TRUE: method_name = "true"; break;
                    case TOK_FALSE: method_name = "false"; break;
                    case TOK_SELF: method_name = "self"; break;
                    case TOK_DEF: method_name = "def"; break;
                    case TOK_MODULE: method_name = "module"; break;
                    case TOK_AND: method_name = "and"; break;
                    case TOK_OR: method_name = "or"; break;
                    case TOK_NOT: method_name = "not"; break;
                    case TOK_RETURN: method_name = "return"; break;
                    default:
                        error(p, "expected method name after '.'", name_tok.line, name_tok.col);
                        break;
                }
                if (!method_name) break;
            }
            if (check(p, TOK_QUESTION) || check(p, TOK_BANG)) {
                Token suffix = advance(p);
                size_t nlen = strlen(method_name);
                char *buf = arena_alloc(p->arena, nlen + 2);
                memcpy(buf, method_name, nlen);
                buf[nlen] = suffix.kind == TOK_QUESTION ? '?' : '!';
                buf[nlen + 1] = '\0';
                method_name = buf;
            }
            Node *call = node_new(p->arena, NODE_CALL, left->span);
            call->call.recv = left;
            call->call.method = method_name;
            if (check(p, TOK_LPAREN)) {
                advance(p);
                call->call.args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
            } else {
                Token nxt = peek(p);
                int lbracket_as_arg = (nxt.kind == TOK_LBRACKET &&
                                       nxt.col > name_tok.col + name_tok.len);
                int can_be_arg =
                    lbracket_as_arg                ||
                    nxt.kind == TOK_SYMBOL         ||
                    nxt.kind == TOK_STRING         ||
                    nxt.kind == TOK_INTERP_BEG     ||
                    nxt.kind == TOK_INT            ||
                    nxt.kind == TOK_FLOAT          ||
                    nxt.kind == TOK_NIL            ||
                    nxt.kind == TOK_TRUE           ||
                    nxt.kind == TOK_FALSE          ||
                    nxt.kind == TOK_SELF           ||
                    nxt.kind == TOK_IVAR           ||
                    nxt.kind == TOK_GVAR           ||
                    nxt.kind == TOK_CONST          ||
                    nxt.kind == TOK_IDENT          ||
                    nxt.kind == TOK_BANG           ||
                    nxt.kind == TOK_TILDE;
                if (can_be_arg && nxt.line == name_tok.line) {
                    call->call.args = parse_command_args(p);
                }
            }
            if (check(p, TOK_LBRACE) || check(p, TOK_DO))
                call->call.block = parse_block(p);
            left = call;
            continue;
        }

        Node *right = parse_expr(p, bp.rbp);
        Node *n = node_new(p->arena, NODE_BINOP, left->span);
        n->binop.op = token_kind_name(op.kind);
        n->binop.left = left;
        n->binop.right = right;
        left = n;
    }

    return left;
}

NodeList *parse_args(Parser *p) {
    NodeList *args = NULL;
    int saved_allow_commas = p->allow_command_arg_commas;
    p->allow_command_arg_commas = 0;
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        Node *arg = parse_expr(p, 0);
        if (arg) args = nodelist_append(p->arena, args, arg);
        if (!match(p, TOK_COMMA)) break;
    }
    p->allow_command_arg_commas = saved_allow_commas;
    return args;
}

Node *parse_block(Parser *p) {
    Token t = peek(p);
    int brace = (t.kind == TOK_LBRACE);
    Span s = tok_span(t);
    advance(p);

    Node *n = node_new(p->arena, NODE_BLOCK, s);
    if (check(p, TOK_PIPE)) {
        advance(p);
        n->block.params = parse_params(p);
        expect(p, TOK_PIPE, "expected '|' to close block params");
    }
    skip_terminators(p);
    n->block.body = parse_body(p, brace);
    if (brace) expect(p, TOK_RBRACE, "expected '}'");
    else expect(p, TOK_END, "expected 'end'");
    return n;
}

static Node *parse_lambda_literal(Parser *p, Span s) {
    Node *block = node_new(p->arena, NODE_BLOCK, s);

    if (match(p, TOK_LPAREN)) {
        block->block.params = parse_params(p);
        expect(p, TOK_RPAREN, "expected ')'");
    }

    Token t = peek(p);
    int brace = (t.kind == TOK_LBRACE);
    if (!brace && t.kind != TOK_DO) {
        error(p, "expected '{' or 'do' after lambda parameters", t.line, t.col);
        return NULL;
    }
    advance(p);
    skip_terminators(p);
    block->block.body = parse_body(p, brace);
    if (brace) expect(p, TOK_RBRACE, "expected '}'");
    else expect(p, TOK_END, "expected 'end'");

    Node *call = node_new(p->arena, NODE_CALL, s);
    call->call.recv = NULL;
    call->call.method = "lambda";
    call->call.args = NULL;
    call->call.block = block;
    return call;
}

static Node *parse_param_node(Parser *p, int allow_default) {
    Token t = peek(p);
    Span s = tok_span(t);

    if (t.kind == TOK_LPAREN) {
        advance(p);
        Node *group = node_new(p->arena, NODE_ARRAY, s);
        NodeList *elems = NULL;
        while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
            Node *elem = parse_param_node(p, 0);
            if (elem) elems = nodelist_append(p->arena, elems, elem);
            if (!match(p, TOK_COMMA)) break;
        }
        expect(p, TOK_RPAREN, "expected ')'");
        group->array.elements = elems;
        return group;
    }

    Node *param = node_new(p->arena, NODE_PARAM, s);
    if (t.kind == TOK_STAR) {
        advance(p);
        param->param.splat = 1;
        t = advance(p);
        param->param.name = t.sval;
    } else if (t.kind == TOK_AMP) {
        advance(p);
        param->param.block_param = 1;
        t = advance(p);
        param->param.name = t.sval;
    } else {
        advance(p);
        param->param.name = t.sval;
        if (allow_default && match(p, TOK_EQ))
            param->param.default_val = parse_expr(p, 0);
    }
    return param;
}

NodeList *parse_params(Parser *p) {
    NodeList *params = NULL;
    while (!check(p, TOK_PIPE) && !check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        Node *param = parse_param_node(p, 1);
        params = nodelist_append(p->arena, params, param);
        if (!match(p, TOK_COMMA)) break;
    }
    return params;
}

Node *parse_primary(Parser *p) {
    Token t = peek(p);
    Span s = tok_span(t);

    {
        int bp = prefix_bp(t.kind);
        if (bp) {
            advance(p);
            Node *operand = parse_expr(p, bp);
            Node *n = node_new(p->arena, NODE_UNOP, s);
            n->unop.op = token_kind_name(t.kind);
            n->unop.operand = operand;
            return n;
        }
    }

    switch (t.kind) {
        case TOK_INT: { advance(p); Node *n = node_new(p->arena, NODE_INT, s); n->ival = t.ival; return n; }
        case TOK_FLOAT: { advance(p); Node *n = node_new(p->arena, NODE_FLOAT, s); n->fval = t.fval; return n; }
        case TOK_STRING: { advance(p); Node *n = node_new(p->arena, NODE_STRING, s); n->sval = t.sval; return n; }
        case TOK_INTERP_BEG: {
            advance(p);
            Node *n = node_new(p->arena, NODE_ROPE, s);
            RopeNode *rope = NULL;
            while (1) {
                Token seg = peek(p);
                if (seg.kind == TOK_INTERP_END || seg.kind == TOK_EOF) { advance(p); break; }
                if (seg.kind == TOK_INTERP_LIT) {
                    advance(p);
                    RopeNode *lit = rope_lit(p->arena, seg.sval, seg.sval ? strlen(seg.sval) : 0);
                    rope = rope ? rope_cat(p->arena, rope, lit) : lit;
                    continue;
                }
                if (seg.kind == TOK_INTERP_EXPR_BEG) {
                    advance(p);
                    if (check(p, TOK_INTERP_EXPR_END)) { advance(p); continue; }
                    Node *expr = parse_expr(p, 0);
                    expect(p, TOK_INTERP_EXPR_END, "expected '}' to close interpolation");
                    RopeNode *er = rope_expr(p->arena, expr);
                    rope = rope ? rope_cat(p->arena, rope, er) : er;
                    continue;
                }
                error(p, "unexpected token inside string interpolation", seg.line, seg.col);
                advance(p);
                break;
            }
            if (!rope) rope = rope_lit(p->arena, "", 0);
            n->interp.rope = rope;
            if (rope_is_static(rope)) {
                n->kind = NODE_STRING;
                n->sval = rope_flatten(p->arena, rope);
            }
            return n;
        }
        case TOK_SYMBOL: { advance(p); Node *n = node_new(p->arena, NODE_SYMBOL, s); n->sval = t.sval; return n; }
        case TOK_NIL: advance(p); return node_new(p->arena, NODE_NIL, s);
        case TOK_TRUE: advance(p); return node_new(p->arena, NODE_TRUE, s);
        case TOK_FALSE: advance(p); return node_new(p->arena, NODE_FALSE, s);
        case TOK_SELF: advance(p); return node_new(p->arena, NODE_SELF, s);
        case TOK_IVAR: { advance(p); Node *n = node_new(p->arena, NODE_IVAR, s); n->sval = t.sval; return n; }
        case TOK_CVAR: { advance(p); Node *n = node_new(p->arena, NODE_CVAR, s); n->sval = t.sval; return n; }
        case TOK_GVAR: { advance(p); Node *n = node_new(p->arena, NODE_GVAR, s); n->sval = t.sval; return n; }
        case TOK_CONST: { advance(p); Node *n = node_new(p->arena, NODE_CONST, s); n->sval = t.sval; return n; }
        case TOK_IDENT: {
            advance(p);
            if (check(p, TOK_LPAREN)) {
                advance(p);
                NodeList *args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
                Node *block = NULL;
                if (check(p, TOK_LBRACE) || check(p, TOK_DO)) block = parse_block(p);
                Node *n = node_new(p->arena, NODE_CALL, s);
                n->call.recv = NULL; n->call.method = t.sval; n->call.args = args; n->call.block = block;
                return n;
            }
            if ((check(p, TOK_LBRACE) || check(p, TOK_DO)) && peek(p).line == t.line) {
                Node *n = node_new(p->arena, NODE_CALL, s);
                n->call.recv = NULL; n->call.method = t.sval; n->call.args = NULL; n->call.block = parse_block(p);
                return n;
            }
            Token nxt = peek(p);
            int lbracket_as_arg = (nxt.kind == TOK_LBRACKET && nxt.col > t.col + t.len);
            int can_be_arg = lbracket_as_arg || nxt.kind == TOK_SYMBOL || nxt.kind == TOK_STRING ||
                             nxt.kind == TOK_INTERP_BEG || nxt.kind == TOK_INT || nxt.kind == TOK_FLOAT ||
                             nxt.kind == TOK_NIL || nxt.kind == TOK_TRUE || nxt.kind == TOK_FALSE ||
                             nxt.kind == TOK_SELF || nxt.kind == TOK_IVAR || nxt.kind == TOK_GVAR ||
                             nxt.kind == TOK_CONST || nxt.kind == TOK_IDENT || nxt.kind == TOK_BANG ||
                             nxt.kind == TOK_TILDE;
            if (can_be_arg && nxt.line == t.line) {
                NodeList *args = parse_command_args(p);
                Node *block = NULL;
                if (check(p, TOK_LBRACE) || check(p, TOK_DO)) block = parse_block(p);
                Node *n = node_new(p->arena, NODE_CALL, s);
                n->call.recv = NULL; n->call.method = t.sval; n->call.args = args; n->call.block = block;
                return n;
            }
            Node *n = node_new(p->arena, NODE_LVAR, s);
            n->sval = t.sval;
            return n;
        }
        case TOK_YIELD: {
            advance(p);
            Node *n = node_new(p->arena, NODE_CALL, s);
            n->call.recv = NULL; n->call.method = "yield"; n->call.args = NULL; n->call.block = NULL;
            if (check(p, TOK_LPAREN)) {
                advance(p);
                n->call.args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
                return n;
            }
            if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF) && !check(p, TOK_END)) {
                n->call.args = parse_command_args(p);
            }
            return n;
        }
        case TOK_LAMBDA:
            advance(p);
            return parse_lambda_literal(p, s);
        case TOK_LPAREN: {
            advance(p);
            Node *inner = parse_expr(p, 0);
            expect(p, TOK_RPAREN, "expected ')'");
            return inner;
        }
        case TOK_LBRACKET: {
            advance(p);
            Node *n = node_new(p->arena, NODE_ARRAY, s);
            NodeList *elems = NULL;
            int saved_allow_commas = p->allow_command_arg_commas;
            p->allow_command_arg_commas = 0;
            skip_terminators(p);
            while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
                Node *elem = parse_expr(p, 0);
                if (elem) elems = nodelist_append(p->arena, elems, elem);
                skip_terminators(p);
                if (!match(p, TOK_COMMA)) break;
                skip_terminators(p);
            }
            p->allow_command_arg_commas = saved_allow_commas;
            expect(p, TOK_RBRACKET, "expected ']'");
            n->array.elements = elems;
            return n;
        }
        case TOK_LBRACE: {
            advance(p);
            Node *n = node_new(p->arena, NODE_HASH, s);
            NodeList *pairs = NULL;
            int saved_allow_commas = p->allow_command_arg_commas;
            p->allow_command_arg_commas = 0;
            skip_terminators(p);
            while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                int used_label = 0;
                Node *key = parse_hash_key(p, &used_label);
                if (!key) break;
                if (!used_label)
                    expect(p, TOK_ARROW, "expected '=>' in hash literal");
                Node *val = parse_expr(p, 0);
                if (!val) break;
                Node *pair = node_new(p->arena, NODE_PAIR, key->span);
                pair->pair.key = key;
                pair->pair.value = val;
                pairs = nodelist_append(p->arena, pairs, pair);
                skip_terminators(p);
                if (!match(p, TOK_COMMA)) break;
                skip_terminators(p);
            }
            p->allow_command_arg_commas = saved_allow_commas;
            expect(p, TOK_RBRACE, "expected '}'");
            n->hash.pairs = pairs;
            return n;
        }
        case TOK_RETURN: {
            advance(p);
            Node *n = node_new(p->arena, NODE_RETURN, s);
            if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF) && !check(p, TOK_END))
                n->jump.value = parse_expr(p, 0);
            return n;
        }
        case TOK_BREAK: {
            advance(p);
            Node *n = node_new(p->arena, NODE_BREAK, s);
            if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF) && !check(p, TOK_END))
                n->jump.value = parse_expr(p, 0);
            return n;
        }
        case TOK_NEXT: {
            advance(p);
            Node *n = node_new(p->arena, NODE_NEXT, s);
            if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF) && !check(p, TOK_END))
                n->jump.value = parse_expr(p, 0);
            return n;
        }
        case TOK_SUPER: {
            advance(p);
            Node *n = node_new(p->arena, NODE_SUPER, s);
            if (check(p, TOK_LPAREN)) {
                advance(p);
                n->super_call.args = parse_args(p);
                n->super_call.forward_args = 0;
                expect(p, TOK_RPAREN, "expected ')'");
            } else {
                n->super_call.args = NULL;
                n->super_call.forward_args = 1;
            }
            return n;
        }
        default:
            error(p, "unexpected token in expression", t.line, t.col);
            advance(p);
            return NULL;
    }
}
