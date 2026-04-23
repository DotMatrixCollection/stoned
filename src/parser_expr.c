#include "parser_internal.h"
#include "rope.h"

#include <string.h>

typedef struct { int lbp; int rbp; } BP;

static Node *parse_hash_key(Parser *p, int *used_label);

static Node *parse_percent_list(Parser *p, Span s, Token t, NodeKind elem_kind) {
    Node *array = node_new(p->arena, NODE_ARRAY, s);
    NodeList *elems = NULL;
    const char *src = t.sval ? t.sval : "";
    size_t len = strlen(src);
    size_t i = 0;

    while (i < len) {
        while (i < len && (src[i] == ' ' || src[i] == '\t' || src[i] == '\r' || src[i] == '\n' ||
                           src[i] == '\f' || src[i] == '\v'))
            i++;
        if (i >= len) break;

        size_t cap = 16;
        char *buf = arena_alloc(p->arena, cap);
        size_t blen = 0;

#define PBUF_PUSH(ch) do { \
    if (blen + 1 >= cap) { \
        char *nb = arena_alloc(p->arena, cap * 2); \
        memcpy(nb, buf, blen); \
        buf = nb; \
        cap *= 2; \
    } \
    buf[blen++] = (char)(ch); \
} while (0)

        while (i < len) {
            char c = src[i];
            if (c == '\\' && i + 1 < len) {
                i++;
                PBUF_PUSH(src[i]);
                i++;
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v')
                break;
            PBUF_PUSH(c);
            i++;
        }
        PBUF_PUSH('\0');

#undef PBUF_PUSH

        Node *elem = node_new(p->arena, elem_kind, s);
        elem->sval = buf;
        elems = nodelist_append(p->arena, elems, elem);
    }

    array->array.elements = elems;
    return array;
}

static int kernel_const_call_name(const char *name) {
    return strcmp(name, "Integer") == 0 || strcmp(name, "Float") == 0 ||
           strcmp(name, "String") == 0 || strcmp(name, "Array") == 0;
}

static int token_adjacent(Token left, Token right) {
    return left.line == right.line && right.col == left.col + left.len;
}

static Token peek_next_token(Parser *p) {
    Parser copy = *p;
    advance(&copy);
    return peek(&copy);
}

static int token_can_start_expr(Token t) {
    switch (t.kind) {
        case TOK_IDENT:
        case TOK_CONST:
        case TOK_IVAR:
        case TOK_GVAR:
        case TOK_NIL:
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_SELF:
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_STRING:
        case TOK_WORDS:
        case TOK_SYMBOLS:
        case TOK_INTERP_BEG:
        case TOK_LPAREN:
        case TOK_LBRACKET:
        case TOK_LBRACE:
        case TOK_DEFINED:
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_BANG:
        case TOK_TILDE:
            return 1;
        default:
            return 0;
    }
}

static int unary_prefix_arg(Token prev, Token sign, Token after_sign) {
    return (sign.kind == TOK_PLUS || sign.kind == TOK_MINUS) &&
           sign.line == prev.line &&
           sign.col > prev.col + prev.len &&
           token_adjacent(sign, after_sign) &&
           token_can_start_expr(after_sign);
}

static Node *parse_expr_continue(Parser *p, Node *left, int min_bp);

static Node *attach_pending_do_block(Parser *p, Node *arg, int min_bp) {
    if (!arg || !check(p, TOK_DO)) return arg;
    if (arg->kind == NODE_CALL && !arg->call.block) {
        arg->call.block = parse_block(p);
        return parse_expr_continue(p, arg, min_bp);
    }
    return arg;
}

static int command_hash_label_node(Node *node) {
    return node && (node->kind == NODE_LVAR || node->kind == NODE_CONST);
}

static Node *command_hash_label_key(Parser *p, Node *node) {
    Node *key = node_new(p->arena, NODE_SYMBOL, node->span);
    key->sval = node->sval;
    return key;
}

static Node *parse_command_hash(Parser *p, Node *first_key_node) {
    Node *n = node_new(p->arena, NODE_HASH, first_key_node->span);
    NodeList *pairs = NULL;
    int saved_allow_commas = p->allow_command_arg_commas;
    p->allow_command_arg_commas = 0;

    Node *key = command_hash_label_key(p, first_key_node);
    expect(p, TOK_COLON, "expected ':' in hash literal");
    Node *val = parse_expr(p, 0);
    if (val) {
        Node *pair = node_new(p->arena, NODE_PAIR, key->span);
        pair->pair.key = key;
        pair->pair.value = val;
        pairs = nodelist_append(p->arena, pairs, pair);
    }

    while (match(p, TOK_COMMA)) {
        Node *next_key_node = parse_expr(p, 0);
        if (!command_hash_label_node(next_key_node) || !check(p, TOK_COLON))
            break;
        Node *next_key = command_hash_label_key(p, next_key_node);
        advance(p);
        Node *next_val = parse_expr(p, 0);
        if (!next_val) break;
        Node *pair = node_new(p->arena, NODE_PAIR, next_key->span);
        pair->pair.key = next_key;
        pair->pair.value = next_val;
        pairs = nodelist_append(p->arena, pairs, pair);
    }

    p->allow_command_arg_commas = saved_allow_commas;
    n->hash.pairs = pairs;
    return n;
}

static Node *parse_arg_expr(Parser *p) {
    p->command_arg_depth++;
    Node *arg = parse_expr(p, 0);
    p->command_arg_depth--;
    if (command_hash_label_node(arg) && check(p, TOK_COLON))
        return parse_command_hash(p, arg);
    return arg;
}

static Node *parse_command_arg(Parser *p) {
    return parse_arg_expr(p);
}

static NodeList *parse_command_args(Parser *p) {
    NodeList *args = NULL;
    int saved_allow_commas = p->allow_command_arg_commas;
    p->allow_command_arg_commas = 0;
    Node *first = parse_command_arg(p);
    p->allow_command_arg_commas = saved_allow_commas;
    if (first) args = nodelist_append(p->arena, args, first);

    while (saved_allow_commas && match(p, TOK_COMMA)) {
        skip_terminators(p);
        p->allow_command_arg_commas = 0;
        Node *arg = parse_command_arg(p);
        p->allow_command_arg_commas = saved_allow_commas;
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
        case TOK_DOT2: case TOK_DOT3: return (BP){8, 9};
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

static Node *parse_expr_continue(Parser *p, Node *left, int min_bp) {
    while (1) {
        Token op = peek(p);
        if (p->stop_at_param_pipe && op.kind == TOK_PIPE) break;

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
            if (op.kind == TOK_DOT && name_tok.kind == TOK_LPAREN && token_adjacent(op, name_tok)) {
                method_name = "call";
            } else if (name_tok.kind == TOK_IDENT || name_tok.kind == TOK_CONST) {
                method_name = name_tok.sval;
            } else {
                /* All keyword tokens carry their string in sval — allow any as method name */
                switch (name_tok.kind) {
                    case TOK_NIL: case TOK_TRUE: case TOK_FALSE: case TOK_SELF:
                    case TOK_IF: case TOK_UNLESS: case TOK_THEN: case TOK_ELSIF:
                    case TOK_ELSE: case TOK_END: case TOK_WHILE: case TOK_UNTIL:
                    case TOK_DO: case TOK_DEF: case TOK_CLASS: case TOK_MODULE:
                    case TOK_RETURN: case TOK_BREAK: case TOK_NEXT: case TOK_RETRY:
                    case TOK_AND: case TOK_OR: case TOK_NOT:
                    case TOK_IN: case TOK_RESCUE: case TOK_ENSURE:
                    case TOK_BEGIN: case TOK_YIELD: case TOK_SUPER:
                    case TOK_CASE: case TOK_WHEN:
                        method_name = name_tok.sval;
                        break;
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
            Token nxt = peek(p);
            if (name_tok.kind == TOK_LPAREN && strcmp(method_name, "call") == 0) {
                call->call.args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
            } else if (nxt.kind == TOK_LPAREN && token_adjacent(name_tok, nxt)) {
                advance(p);
                call->call.args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
            } else {
                int lparen_as_arg = (nxt.kind == TOK_LPAREN &&
                                     nxt.col > name_tok.col + name_tok.len);
                int lbracket_as_arg = (nxt.kind == TOK_LBRACKET &&
                                       nxt.col > name_tok.col + name_tok.len);
                Token sign_arg = peek_next_token(p);
                int can_be_arg =
                    lparen_as_arg                  ||
                    lbracket_as_arg                ||
                    nxt.kind == TOK_SYMBOL         ||
                    nxt.kind == TOK_STRING         ||
                    nxt.kind == TOK_WORDS          ||
                    nxt.kind == TOK_SYMBOLS        ||
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
                    nxt.kind == TOK_TILDE          ||
                    unary_prefix_arg(name_tok, nxt, sign_arg);
                if (can_be_arg && nxt.line == name_tok.line) {
                    call->call.args = parse_command_args(p);
                }
            }
            if (check(p, TOK_LBRACE) || (check(p, TOK_DO) && p->command_arg_depth <= 1))
                call->call.block = parse_block(p);
            left = call;
            continue;
        }

        if (op.kind == TOK_DOT2 || op.kind == TOK_DOT3) {
            Node *right = parse_expr(p, bp.rbp);
            Node *n = node_new(p->arena, NODE_RANGE, left->span);
            n->range.begin = left;
            n->range.end   = right;
            n->range.exclusive = (op.kind == TOK_DOT3);
            left = n;
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

Node *parse_expr(Parser *p, int min_bp) {
    Node *left = parse_primary(p);
    if (!left) return NULL;
    return parse_expr_continue(p, left, min_bp);
}

NodeList *parse_args(Parser *p) {
    NodeList *args = NULL;
    int saved_allow_commas = p->allow_command_arg_commas;
    p->allow_command_arg_commas = 0;
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        if (check(p, TOK_AMP)) {
            Span ss = tok_span(peek(p));
            advance(p);
            Node *bp = node_new(p->arena, NODE_BLOCK_PASS, ss);
            bp->block_pass.expr = parse_expr(p, 0);
            args = nodelist_append(p->arena, args, bp);
            break; /* & must be last arg */
        }
        if (check(p, TOK_STAR)) {
            Span ss = tok_span(peek(p));
            advance(p);
            Node *splat = node_new(p->arena, NODE_UNOP, ss);
            splat->unop.op = "*";
            splat->unop.operand = parse_expr(p, 0);
            splat->unop.operand = attach_pending_do_block(p, splat->unop.operand, 0);
            args = nodelist_append(p->arena, args, splat);
        } else {
            Node *arg = parse_arg_expr(p);
            arg = attach_pending_do_block(p, arg, 0);
            if (arg) args = nodelist_append(p->arena, args, arg);
        }
        if (!match(p, TOK_COMMA)) break;
        skip_terminators(p);
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
        int saved_stop_at_param_pipe = p->stop_at_param_pipe;
        p->stop_at_param_pipe = 1;
        n->block.params = parse_params(p);
        p->stop_at_param_pipe = saved_stop_at_param_pipe;
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
        case TOK_WORDS:
            advance(p);
            return parse_percent_list(p, s, t, NODE_STRING);
        case TOK_SYMBOLS:
            advance(p);
            return parse_percent_list(p, s, t, NODE_SYMBOL);
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
        case TOK_CONST: {
            advance(p);
            Token nxt = peek(p);
            if (kernel_const_call_name(t.sval)) {
                if (nxt.kind == TOK_LPAREN && token_adjacent(t, nxt)) {
                    advance(p);
                    NodeList *args = parse_args(p);
                    expect(p, TOK_RPAREN, "expected ')'");
                    Node *block = NULL;
                    if (check(p, TOK_LBRACE) || check(p, TOK_DO)) block = parse_block(p);
                    Node *n = node_new(p->arena, NODE_CALL, s);
                    n->call.recv = NULL; n->call.method = t.sval; n->call.args = args; n->call.block = block;
                    return n;
                }
                int lparen_as_arg = (nxt.kind == TOK_LPAREN && nxt.col > t.col + t.len);
                int lbracket_as_arg = (nxt.kind == TOK_LBRACKET && nxt.col > t.col + t.len);
                Token sign_arg = peek_next_token(p);
                int can_be_arg = lparen_as_arg || lbracket_as_arg || nxt.kind == TOK_SYMBOL || nxt.kind == TOK_STRING ||
                                 nxt.kind == TOK_WORDS || nxt.kind == TOK_SYMBOLS ||
                                 nxt.kind == TOK_INTERP_BEG || nxt.kind == TOK_INT || nxt.kind == TOK_FLOAT ||
                                 nxt.kind == TOK_NIL || nxt.kind == TOK_TRUE || nxt.kind == TOK_FALSE ||
                                 nxt.kind == TOK_SELF || nxt.kind == TOK_IVAR || nxt.kind == TOK_GVAR ||
                                 nxt.kind == TOK_CONST || nxt.kind == TOK_IDENT || nxt.kind == TOK_BANG ||
                                 nxt.kind == TOK_TILDE || unary_prefix_arg(t, nxt, sign_arg);
                if (can_be_arg && nxt.line == t.line) {
                    NodeList *args = parse_command_args(p);
                    Node *block = NULL;
                    if (check(p, TOK_LBRACE) || (check(p, TOK_DO) && p->command_arg_depth <= 1)) block = parse_block(p);
                    Node *n = node_new(p->arena, NODE_CALL, s);
                    n->call.recv = NULL; n->call.method = t.sval; n->call.args = args; n->call.block = block;
                    return n;
                }
            }
            Node *n = node_new(p->arena, NODE_CONST, s); n->sval = t.sval; return n;
        }
        case TOK_IDENT: {
            advance(p);
            Token nxt = peek(p);
            if (nxt.kind == TOK_LPAREN && token_adjacent(t, nxt)) {
                advance(p);
                NodeList *args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
                Node *block = NULL;
                if (check(p, TOK_LBRACE) || check(p, TOK_DO)) block = parse_block(p);
                Node *n = node_new(p->arena, NODE_CALL, s);
                n->call.recv = NULL; n->call.method = t.sval; n->call.args = args; n->call.block = block;
                return n;
            }
            if ((check(p, TOK_LBRACE) || (check(p, TOK_DO) && p->command_arg_depth <= 1)) &&
                peek(p).line == t.line) {
                Node *n = node_new(p->arena, NODE_CALL, s);
                n->call.recv = NULL; n->call.method = t.sval; n->call.args = NULL; n->call.block = parse_block(p);
                return n;
            }
            int lparen_as_arg = (nxt.kind == TOK_LPAREN && nxt.col > t.col + t.len);
            int lbracket_as_arg = (nxt.kind == TOK_LBRACKET && nxt.col > t.col + t.len);
            Token sign_arg = peek_next_token(p);
            int can_be_arg = lparen_as_arg || lbracket_as_arg || nxt.kind == TOK_SYMBOL || nxt.kind == TOK_STRING ||
                             nxt.kind == TOK_WORDS || nxt.kind == TOK_SYMBOLS ||
                             nxt.kind == TOK_INTERP_BEG || nxt.kind == TOK_INT || nxt.kind == TOK_FLOAT ||
                             nxt.kind == TOK_NIL || nxt.kind == TOK_TRUE || nxt.kind == TOK_FALSE ||
                             nxt.kind == TOK_SELF || nxt.kind == TOK_IVAR || nxt.kind == TOK_GVAR ||
                             nxt.kind == TOK_CONST || nxt.kind == TOK_IDENT || nxt.kind == TOK_BANG ||
                             nxt.kind == TOK_TILDE || unary_prefix_arg(t, nxt, sign_arg);
            if (can_be_arg && nxt.line == t.line) {
                NodeList *args = parse_command_args(p);
                Node *block = NULL;
                if (check(p, TOK_LBRACE) || (check(p, TOK_DO) && p->command_arg_depth <= 1)) block = parse_block(p);
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
            Token nxt = peek(p);
            if (nxt.kind == TOK_LPAREN && token_adjacent(t, nxt)) {
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
        case TOK_DEFINED: {
            advance(p);
            Node *expr = NULL;
            if (match(p, TOK_LPAREN)) {
                expr = parse_expr(p, 0);
                expect(p, TOK_RPAREN, "expected ')'");
            } else {
                expr = parse_expr(p, 30);
            }
            Node *n = node_new(p->arena, NODE_DEFINED, s);
            n->defined_expr.expr = expr;
            return n;
        }
        case TOK_LPAREN: {
            advance(p);
            Node *inner = parse_expr(p, 0);
            inner = attach_pending_do_block(p, inner, 0);
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
        case TOK_RETRY: {
            advance(p);
            return node_new(p->arena, NODE_RETRY, s);
        }
        case TOK_IF: case TOK_UNLESS: case TOK_WHILE: case TOK_UNTIL:
        case TOK_BEGIN: case TOK_CASE:
            return parse_stmt(p);
        case TOK_SUPER: {
            advance(p);
            Node *n = node_new(p->arena, NODE_SUPER, s);
            Token nxt = peek(p);
            if (nxt.kind == TOK_LPAREN && token_adjacent(t, nxt)) {
                advance(p);
                n->super_call.args = parse_args(p);
                n->super_call.forward_args = 0;
                expect(p, TOK_RPAREN, "expected ')'");
            } else {
                int lparen_as_arg = (nxt.kind == TOK_LPAREN && nxt.col > t.col + t.len);
                int lbracket_as_arg = (nxt.kind == TOK_LBRACKET && nxt.col > t.col + t.len);
                Token sign_arg = peek_next_token(p);
                int can_be_arg = lparen_as_arg || lbracket_as_arg || nxt.kind == TOK_SYMBOL ||
                                 nxt.kind == TOK_STRING || nxt.kind == TOK_WORDS || nxt.kind == TOK_SYMBOLS ||
                                 nxt.kind == TOK_INTERP_BEG ||
                                 nxt.kind == TOK_INT || nxt.kind == TOK_FLOAT || nxt.kind == TOK_NIL ||
                                 nxt.kind == TOK_TRUE || nxt.kind == TOK_FALSE || nxt.kind == TOK_SELF ||
                                 nxt.kind == TOK_IVAR || nxt.kind == TOK_GVAR || nxt.kind == TOK_CONST ||
                                 nxt.kind == TOK_IDENT || nxt.kind == TOK_BANG || nxt.kind == TOK_TILDE ||
                                 unary_prefix_arg(t, nxt, sign_arg);
                if (can_be_arg && nxt.line == t.line) {
                    n->super_call.args = parse_command_args(p);
                    n->super_call.forward_args = 0;
                } else {
                    n->super_call.args = NULL;
                    n->super_call.forward_args = 1;
                }
            }
            return n;
        }
        default:
            error(p, "unexpected token in expression", t.line, t.col);
            advance(p);
            return NULL;
    }
}
