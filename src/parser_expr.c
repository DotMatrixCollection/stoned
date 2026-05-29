#include "parser_internal.h"
#include "rope.h"

#include <string.h>

static void mark_assign_targets(Parser *p, Node *target) {
    if (!target) return;
    if (target->kind == NODE_LVAR)
        lexer_mark_local(&p->lexer, target->sval);
    else if (target->kind == NODE_ARRAY)
        for (NodeList *el = target->array.elements; el; el = el->next)
            mark_assign_targets(p, el->node);
}

static void mark_params_locals(Parser *p, NodeList *params) {
    for (NodeList *pl = params; pl; pl = pl->next) {
        if (!pl->node) continue;
        if (pl->node->kind == NODE_PARAM && pl->node->param.name)
            lexer_mark_local(&p->lexer, pl->node->param.name);
        else if (pl->node->kind == NODE_ARRAY)
            mark_params_locals(p, pl->node->array.elements);
    }
}

typedef struct { int lbp; int rbp; } BP;

static Node *parse_hash_key(Parser *p, int *used_label);

static int jump_value_terminator(TokenKind kind) {
    return kind == TOK_NEWLINE || kind == TOK_SEMICOLON || kind == TOK_EOF || kind == TOK_END ||
           kind == TOK_IF || kind == TOK_UNLESS || kind == TOK_WHILE || kind == TOK_UNTIL ||
           kind == TOK_RESCUE;
}

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
        size_t elem_len = blen;
        PBUF_PUSH('\0');

#undef PBUF_PUSH

        Node *elem = node_new(p->arena, elem_kind, s);
        elem->sval = buf;
        elem->slen = elem_len;
        elems = nodelist_append(p->arena, elems, elem);
    }

    array->array.elements = elems;
    return array;
}

static Node *parse_jump_value(Parser *p, Node *first, Span s) {
    if (!first) return NULL;
    if (!check(p, TOK_COMMA)) return first;

    Node *array = node_new(p->arena, NODE_ARRAY, s);
    NodeList *elems = NULL;
    elems = nodelist_append(p->arena, elems, first);
    while (match(p, TOK_COMMA)) {
        if (jump_value_terminator(peek(p).kind))
            break;
        Node *elem = parse_expr(p, 0);
        if (elem) elems = nodelist_append(p->arena, elems, elem);
    }
    array->array.elements = elems;
    return array;
}

static int kernel_const_call_name(const char *name) {
    return strcmp(name, "Integer") == 0 || strcmp(name, "Float") == 0 ||
           strcmp(name, "String") == 0 || strcmp(name, "Array") == 0 ||
           strcmp(name, "Hash") == 0 ||
           strcmp(name, "Complex") == 0 || strcmp(name, "Rational") == 0;
}

static Token peek_next_token(Parser *p) {
    Parser copy = *p;
    advance(&copy);
    return peek(&copy);
}

static Node *parse_body_until_rparen(Parser *p) {
    Span s = tok_span(peek(p));
    Node *n = node_new(p->arena, NODE_BODY, s);
    NodeList *stmts = NULL;

    while (1) {
        skip_terminators(p);
        Token t = peek(p);
        if (t.kind == TOK_RPAREN || t.kind == TOK_EOF)
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
        case TOK_RATIONAL:
        case TOK_IMAGINARY:
        case TOK_STRING:
        case TOK_REGEXP:
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

static int const_path_segment_is_call(Parser *p, Token name_tok) {
    Token nxt = peek(p);
    Token sign_arg = peek_next_token(p);
    int lparen_as_arg = (nxt.kind == TOK_LPAREN && token_adjacent(name_tok, nxt));
    int can_be_arg =
        lparen_as_arg                  ||
        nxt.kind == TOK_SYMBOL         ||
        nxt.kind == TOK_STRING         ||
        nxt.kind == TOK_WORDS          ||
        nxt.kind == TOK_SYMBOLS        ||
        nxt.kind == TOK_INTERP_BEG     ||
        nxt.kind == TOK_INT            ||
        nxt.kind == TOK_FLOAT          ||
        nxt.kind == TOK_RATIONAL       ||
        nxt.kind == TOK_IMAGINARY      ||
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
    return can_be_arg && nxt.line == name_tok.line;
}

static int range_rhs_omitted(Parser *p) {
    return check(p, TOK_RPAREN) || check(p, TOK_RBRACKET) || check(p, TOK_RBRACE) ||
           check(p, TOK_COMMA) || check(p, TOK_NEWLINE) || check(p, TOK_SEMICOLON) ||
           check(p, TOK_EOF) || check(p, TOK_END) || check(p, TOK_THEN) ||
           check(p, TOK_DO) || check(p, TOK_IF) || check(p, TOK_UNLESS) ||
           check(p, TOK_WHILE) || check(p, TOK_UNTIL);
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

    Node *key = command_hash_label_key(p, first_key_node);
    expect(p, TOK_COLON, "expected ':' in hash literal");
    Node *val = parse_expr(p, 0);
    if (val) {
        Node *pair = node_new(p->arena, NODE_PAIR, key->span);
        pair->pair.key = key;
        pair->pair.value = val;
        pairs = nodelist_append(p->arena, pairs, pair);
    }

    n->hash.pairs = pairs;
    n->hash.keyword_style = 1;
    return n;
}

static Node *parse_arrow_hash(Parser *p, Node *first_key_node) {
    Node *n = node_new(p->arena, NODE_HASH, first_key_node->span);
    NodeList *pairs = NULL;
    Node *key = first_key_node;

    while (1) {
        expect(p, TOK_ARROW, "expected '=>' in hash literal");
        Node *val = parse_expr(p, 0);
        if (val) {
            Node *pair = node_new(p->arena, NODE_PAIR, key->span);
            pair->pair.key = key;
            pair->pair.value = val;
            pairs = nodelist_append(p->arena, pairs, pair);
        }
        if (!match(p, TOK_COMMA))
            break;
        key = parse_expr(p, 0);
        if (!key || !check(p, TOK_ARROW))
            break;
    }

    n->hash.pairs = pairs;
    n->hash.keyword_style = 0;
    return n;
}

static Node *parse_arg_expr(Parser *p) {
    if (check(p, TOK_STAR2)) {
        Span ss = tok_span(peek(p));
        advance(p);
        Node *ds = node_new(p->arena, NODE_UNOP, ss);
        ds->unop.op = "**";
        ds->unop.operand = parse_expr(p, 0);
        return ds;
    }
    p->command_arg_depth++;
    Node *arg = parse_expr(p, 0);
    p->command_arg_depth--;
    if (command_hash_label_node(arg) && check(p, TOK_COLON))
        return parse_command_hash(p, arg);
    if (arg && check(p, TOK_ARROW))
        return parse_arrow_hash(p, arg);
    return arg;
}

static Node *parse_command_arg(Parser *p) {
    return parse_arg_expr(p);
}

static NodeList *parse_command_args(Parser *p) {
    NodeList *args = NULL;
    int saved_allow_commas = p->allow_command_arg_commas;
    p->allow_command_arg_commas = 0;
    while (1) {
        if (check(p, TOK_AMP)) {
            Span ss = tok_span(peek(p));
            advance(p);
            Node *bp = node_new(p->arena, NODE_BLOCK_PASS, ss);
            bp->block_pass.expr = parse_expr(p, 0);
            args = nodelist_append(p->arena, args, bp);
            break;
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
            Node *arg = parse_command_arg(p);
            if (arg) args = nodelist_append(p->arena, args, arg);
        }
        if (!(saved_allow_commas && match(p, TOK_COMMA)))
            break;
        skip_terminators(p);
        p->allow_command_arg_commas = 0;
    }
    p->allow_command_arg_commas = saved_allow_commas;
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
        case TOK_DOT: case TOK_COLON2: case TOK_ANDDOT: return (BP){40, 41};
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
        case TOK_STAR: return 2; /* splat — low bp so it captures full expression (ranges etc.) */
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

        /* Leading-dot continuation: newline followed by . or &. continues the chain */
        if (op.kind == TOK_NEWLINE) {
            Parser lookahead = *p;
            advance(&lookahead);
            Token nxt = peek(&lookahead);
            if (nxt.kind == TOK_DOT || nxt.kind == TOK_ANDDOT) {
                advance(p); /* consume the newline */
                op = peek(p);
            }
        }

        if (op.kind == TOK_QUESTION) {
            BP bp = infix_bp(TOK_QUESTION);
            if (bp.lbp < min_bp) break;
            advance(p);
            skip_terminators(p);  /* allow newline after ? */
            Node *then_expr = parse_expr(p, 0);
            skip_terminators(p);  /* allow newline before : */
            expect(p, TOK_COLON, "expected ':' in ternary");
            skip_terminators(p);  /* allow newline after : */
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
            skip_terminators(p);
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
            mark_assign_targets(p, left);
            left = n;
            continue;
        }

        if (op.kind >= TOK_PLUS_EQ && op.kind <= TOK_PIPE2_EQ) {
            skip_terminators(p);
            Node *right = parse_expr(p, bp.rbp);
            Node *n = node_new(p->arena, NODE_OP_ASSIGN, left->span);
            n->binop.op = token_kind_name(op.kind);
            n->binop.left = left;
            n->binop.right = right;
            left = n;
            continue;
        }

        if (op.kind == TOK_DOT || op.kind == TOK_COLON2 || op.kind == TOK_ANDDOT) {
            Token name_tok = advance(p);
            const char *method_name = NULL;
            if (op.kind == TOK_COLON2 && left->kind == NODE_CONST && name_tok.kind == TOK_CONST) {
                Token nxt = peek(p);
                Token sign_arg = peek_next_token(p);
                int lparen_as_arg = (nxt.kind == TOK_LPAREN &&
                                     token_adjacent(name_tok, nxt));
                int can_be_arg =
                    lparen_as_arg                  ||
                    nxt.kind == TOK_SYMBOL         ||
                    nxt.kind == TOK_STRING         ||
                    nxt.kind == TOK_WORDS          ||
                    nxt.kind == TOK_SYMBOLS        ||
                    nxt.kind == TOK_INTERP_BEG     ||
                    nxt.kind == TOK_INT            ||
                    nxt.kind == TOK_FLOAT          ||
                    nxt.kind == TOK_RATIONAL       ||
                    nxt.kind == TOK_IMAGINARY      ||
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
                if (!(can_be_arg && nxt.line == name_tok.line)) {
                    size_t llen = strlen(left->sval);
                    size_t rlen = strlen(name_tok.sval);
                    char *full = arena_alloc(p->arena, llen + 2 + rlen + 1);
                    memcpy(full, left->sval, llen);
                    memcpy(full + llen, "::", 2);
                    memcpy(full + llen + 2, name_tok.sval, rlen + 1);
                    Node *n = node_new(p->arena, NODE_CONST, left->span);
                    n->sval = full;
                    left = n;
                    continue;
                }
            }
            if (op.kind == TOK_COLON2 && name_tok.kind == TOK_CONST) {
                Token nxt = peek(p);
                Token sign_arg = peek_next_token(p);
                int lparen_as_arg = (nxt.kind == TOK_LPAREN &&
                                     token_adjacent(name_tok, nxt));
                int can_be_arg =
                    lparen_as_arg                  ||
                    nxt.kind == TOK_SYMBOL         ||
                    nxt.kind == TOK_STRING         ||
                    nxt.kind == TOK_WORDS          ||
                    nxt.kind == TOK_SYMBOLS        ||
                    nxt.kind == TOK_INTERP_BEG     ||
                    nxt.kind == TOK_INT            ||
                    nxt.kind == TOK_FLOAT          ||
                    nxt.kind == TOK_RATIONAL       ||
                    nxt.kind == TOK_IMAGINARY      ||
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
                if (!(can_be_arg && nxt.line == name_tok.line)) {
                    Node *n = node_new(p->arena, NODE_CONST_ACCESS, left->span);
                    n->const_access.recv = left;
                    n->const_access.name = name_tok.sval;
                    left = n;
                    continue;
                }
            }
            if ((op.kind == TOK_DOT || op.kind == TOK_ANDDOT) &&
                name_tok.kind == TOK_LPAREN && token_adjacent(op, name_tok)) {
                method_name = "call";
            } else if (name_tok.kind == TOK_LBRACKET) {
                /* obj.[] / obj.[]= — bracket method as explicit method name */
                if (!check(p, TOK_RBRACKET)) {
                    error(p, "expected ']' to close '[]' method name", peek(p).line, peek(p).col);
                    break;
                }
                Token rb = advance(p); /* consume ] */
                if (check(p, TOK_EQ) && token_adjacent(rb, peek(p))) {
                    advance(p);
                    method_name = "[]=";
                } else {
                    method_name = "[]";
                }
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
                    case TOK_CASE: case TOK_WHEN: case TOK_ALIAS: case TOK_FOR:
                        method_name = name_tok.sval;
                        break;
                    /* Operator method names after dot: obj.%(s), obj.+(x), etc. */
                    case TOK_PERCENT:    method_name = "%";   break;
                    case TOK_PLUS:       method_name = "+";   break;
                    case TOK_MINUS:      method_name = "-";   break;
                    case TOK_STAR:       method_name = "*";   break;
                    case TOK_STAR2:      method_name = "**";  break;
                    case TOK_SLASH:      method_name = "/";   break;
                    case TOK_LT:         method_name = "<";   break;
                    case TOK_GT:         method_name = ">";   break;
                    case TOK_LEQ:        method_name = "<=";  break;
                    case TOK_GEQ:        method_name = ">=";  break;
                    case TOK_EQ2:        method_name = "==";  break;
                    case TOK_EQ3:        method_name = "==="; break;
                    case TOK_NEQ:        method_name = "!=";  break;
                    case TOK_SPACESHIP:  method_name = "<=>"; break;
                    case TOK_MATCH:      method_name = "=~";  break;
                    case TOK_NMATCH:     method_name = "!~";  break;
                    case TOK_LSHIFT:     method_name = "<<";  break;
                    case TOK_RSHIFT:     method_name = ">>";  break;
                    case TOK_AMP:        method_name = "&";   break;
                    case TOK_PIPE:       method_name = "|";   break;
                    case TOK_CARET:      method_name = "^";   break;
                    case TOK_TILDE:      method_name = "~";   break;
                    default:
                        error(p, "expected method name after '.'", name_tok.line, name_tok.col);
                        break;
                }
                if (!method_name) break;
            }
            Token suffix_tok = peek(p);
            if ((suffix_tok.kind == TOK_QUESTION || suffix_tok.kind == TOK_BANG) &&
                token_adjacent(name_tok, suffix_tok)) {
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
            call->call.safe_nav = (op.kind == TOK_ANDDOT);
            int args_from_paren = 0;
            Token nxt = peek(p);
            if (name_tok.kind == TOK_LPAREN && strcmp(method_name, "call") == 0) {
                args_from_paren = 1;
                call->call.args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
            } else if (nxt.kind == TOK_LPAREN && token_adjacent(name_tok, nxt)) {
                advance(p);
                args_from_paren = 1;
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
                    nxt.kind == TOK_RATIONAL       ||
                    nxt.kind == TOK_IMAGINARY      ||
                    nxt.kind == TOK_NIL            ||
                    nxt.kind == TOK_TRUE           ||
                    nxt.kind == TOK_FALSE          ||
                    nxt.kind == TOK_SELF           ||
                    nxt.kind == TOK_IVAR           ||
                    nxt.kind == TOK_GVAR           ||
                    nxt.kind == TOK_COLON2         ||
                    nxt.kind == TOK_CONST          ||
                    nxt.kind == TOK_IDENT          ||
                    nxt.kind == TOK_BANG           ||
                    nxt.kind == TOK_TILDE          ||
                    unary_prefix_arg(name_tok, nxt, sign_arg);
                if (can_be_arg && nxt.line == name_tok.line) {
                    call->call.args = parse_command_args(p);
                }
            }
            if (check(p, TOK_LBRACE) ||
                (check(p, TOK_DO) && (p->command_arg_depth == 0 || args_from_paren)))
                call->call.block = parse_block(p);
            left = call;
            continue;
        }

        if (op.kind == TOK_DOT2 || op.kind == TOK_DOT3) {
            Node *right = NULL;
            if (!range_rhs_omitted(p)) {
                skip_terminators(p);
                right = parse_expr(p, bp.rbp);
            }
            Node *n = node_new(p->arena, NODE_RANGE, left->span);
            n->range.begin = left;
            n->range.end   = right;
            n->range.exclusive = (op.kind == TOK_DOT3);
            left = n;
            continue;
        }

        skip_terminators(p);
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
    skip_terminators(p);
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        if (check(p, TOK_AMP)) {
            Span ss = tok_span(peek(p));
            advance(p);
            Node *bp = node_new(p->arena, NODE_BLOCK_PASS, ss);
            bp->block_pass.expr = parse_expr(p, 0);
            args = nodelist_append(p->arena, args, bp);
            skip_terminators(p);
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
        if (!match(p, TOK_COMMA)) {
            skip_terminators(p);
            break;
        }
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
    skip_terminators(p);
    if (check(p, TOK_PIPE)) {
        advance(p);
        int saved_stop_at_param_pipe = p->stop_at_param_pipe;
        p->stop_at_param_pipe = 1;
        n->block.params = parse_params(p);
        mark_params_locals(p, n->block.params);
        p->stop_at_param_pipe = saved_stop_at_param_pipe;
        expect(p, TOK_PIPE, "expected '|' to close block params");
    }
    skip_terminators(p);
    int saved_allow_commas = p->allow_command_arg_commas;
    p->allow_command_arg_commas = 1;
    n->block.body = parse_body(p, brace);
    p->allow_command_arg_commas = saved_allow_commas;
    if (check(p, TOK_RESCUE) || check(p, TOK_ENSURE)) {
        n->block.body = wrap_rescue_ensure(p, s, n->block.body);
        if (!n->block.body) return NULL;
    }
    if (brace) expect(p, TOK_RBRACE, "expected '}'");
    else expect(p, TOK_END, "expected 'end'");
    return n;
}

static Node *parse_lambda_literal(Parser *p, Span s) {
    Node *block = node_new(p->arena, NODE_BLOCK, s);

    if (match(p, TOK_LPAREN)) {
        block->block.params = parse_params(p);
        expect(p, TOK_RPAREN, "expected ')'");
    } else {
        /* unparenthesized params: -> x, y { } */
        Token pt = peek(p);
        if (pt.kind == TOK_IDENT || pt.kind == TOK_STAR || pt.kind == TOK_AMP ||
            pt.kind == TOK_STAR2 || pt.kind == TOK_IVAR) {
            block->block.params = parse_params(p);
        }
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
    if (check(p, TOK_RESCUE) || check(p, TOK_ENSURE)) {
        block->block.body = wrap_rescue_ensure(p, s, block->block.body);
        if (!block->block.body) return NULL;
    }
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
        t = peek(p);
        if (t.kind == TOK_IDENT || t.kind == TOK_CONST) {
            advance(p);
            param->param.name = t.sval;
        }
    } else if (t.kind == TOK_STAR2) {
        advance(p);
        param->param.keyword_splat = 1;
        t = peek(p);
        if (t.kind == TOK_IDENT || t.kind == TOK_CONST) {
            advance(p);
            param->param.name = t.sval;
        }
    } else if (t.kind == TOK_AMP) {
        advance(p);
        param->param.block_param = 1;
        t = advance(p);
        param->param.name = t.sval;
    } else {
        advance(p);
        param->param.name = t.sval;
        if (check(p, TOK_COLON)) {
            /* keyword parameter: key: or key: default */
            advance(p);
            param->param.keyword_param = 1;
            /* optional default — anything that isn't a param separator */
            if (allow_default &&
                !check(p, TOK_COMMA) && !check(p, TOK_RPAREN) &&
                !check(p, TOK_PIPE)  && !check(p, TOK_NEWLINE) && !check(p, TOK_EOF))
                param->param.default_val = parse_expr(p, 0);
        } else if (allow_default && match(p, TOK_EQ)) {
            param->param.default_val = parse_expr(p, 0);
        }
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

NodeList *parse_params_unparen(Parser *p, uint32_t def_line) {
    NodeList *params = NULL;
    Token nxt = peek(p);
    while (nxt.line == def_line &&
           !check(p, TOK_PIPE) && !check(p, TOK_RPAREN) &&
           !check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) &&
           !check(p, TOK_EOF)) {
        Node *param = parse_param_node(p, 1);
        params = nodelist_append(p->arena, params, param);
        nxt = peek(p);
        if (nxt.kind != TOK_COMMA || nxt.line != def_line) break;
        advance(p); /* consume comma */
        nxt = peek(p);
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
        case TOK_RATIONAL: {
            advance(p);
            Node *n = node_new(p->arena, NODE_CALL, s);
            n->call.recv = NULL;
            n->call.method = "Rational";
            Node *num_node = node_new(p->arena, t.fval != 0.0 ? NODE_FLOAT : NODE_INT, s);
            if (t.fval != 0.0) num_node->fval = t.fval;
            else num_node->ival = t.ival;
            Node *den_node = node_new(p->arena, NODE_INT, s);
            den_node->ival = 1;
            n->call.args = nodelist_append(p->arena, NULL, num_node);
            n->call.args = nodelist_append(p->arena, n->call.args, den_node);
            n->call.block = NULL;
            return n;
        }
        case TOK_IMAGINARY: {
            advance(p);
            Node *n = node_new(p->arena, NODE_CALL, s);
            n->call.recv = NULL;
            n->call.method = "Complex";
            Node *zero_node = node_new(p->arena, NODE_INT, s);
            zero_node->ival = 0;
            Node *imag_node = node_new(p->arena, t.fval != 0.0 ? NODE_FLOAT : NODE_INT, s);
            if (t.fval != 0.0) imag_node->fval = t.fval;
            else imag_node->ival = t.ival;
            n->call.args = nodelist_append(p->arena, NULL, zero_node);
            n->call.args = nodelist_append(p->arena, n->call.args, imag_node);
            n->call.block = NULL;
            return n;
        }
        case TOK_STRING: {
            advance(p);
            Node *str = node_new(p->arena, NODE_STRING, s);
            str->sval = t.sval;
            str->slen = t.slen;
            if (t.ival == 1) {
                /* backtick command: wrap in Kernel.` call */
                Node *call = node_new(p->arena, NODE_CALL, s);
                call->call.recv = NULL;
                call->call.method = "`";
                NodeList *args = nodelist_append(p->arena, NULL, str);
                call->call.args = args;
                return call;
            }
            /* Adjacent string literal concatenation: "hello" "world" → "hello world" */
            while (peek(p).kind == TOK_STRING && peek(p).ival != 1) {
                Token nt = advance(p);
                size_t la = str->slen;
                size_t lb = nt.slen;
                char *cat = arena_alloc(p->arena, la + lb + 1);
                if (str->sval) memcpy(cat, str->sval, la);
                if (nt.sval) memcpy(cat + la, nt.sval, lb);
                cat[la + lb] = '\0';
                str->sval = cat;
                str->slen = la + lb;
            }
            return str;
        }
        case TOK_REGEXP: {
            advance(p);
            Node *n = node_new(p->arena, NODE_REGEXP, s);
            n->regexp_lit.pattern = t.sval;
            n->regexp_lit.options = (unsigned int)t.ival;
            return n;
        }
        case TOK_WORDS:
            advance(p);
            return parse_percent_list(p, s, t, NODE_STRING);
        case TOK_SYMBOLS:
            advance(p);
            return parse_percent_list(p, s, t, NODE_SYMBOL);
        case TOK_INTERP_BEG: {
            int is_backtick = (t.ival == 1);
            advance(p);
            Node *n = node_new(p->arena, NODE_ROPE, s);
            RopeNode *rope = NULL;
            parse_one_interp_string:
            while (1) {
                Token seg = peek(p);
                if (seg.kind == TOK_INTERP_END || seg.kind == TOK_EOF) { advance(p); break; }
                if (seg.kind == TOK_INTERP_LIT) {
                    advance(p);
                    RopeNode *lit = rope_lit(p->arena, seg.sval ? seg.sval : "", seg.slen);
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
            /* Adjacent string literal concatenation */
            if (!is_backtick) {
                if (peek(p).kind == TOK_INTERP_BEG && peek(p).ival != 1) {
                    advance(p); /* consume the opening " of the next string */
                    goto parse_one_interp_string;
                }
                if (peek(p).kind == TOK_STRING && peek(p).ival != 1) {
                    Token sq = advance(p);
                    RopeNode *lit = rope_lit(p->arena, sq.sval ? sq.sval : "", sq.slen);
                    rope = rope ? rope_cat(p->arena, rope, lit) : lit;
                }
            }
            if (!rope) rope = rope_lit(p->arena, "", 0);
            n->interp.rope = rope;
            if (rope_is_static(rope)) {
                n->kind = NODE_STRING;
                n->sval = rope_flatten(p->arena, rope);
                n->slen = rope_byte_len(rope);
            }
            if (is_backtick) {
                /* Wrap interpolated string in a backtick method call `(string) */
                Node *call = node_new(p->arena, NODE_CALL, s);
                call->call.recv = NULL;
                call->call.method = "`";
                NodeList *args = arena_alloc(p->arena, sizeof(NodeList));
                args->node = n;
                args->next = NULL;
                call->call.args = args;
                call->call.block = NULL;
                return call;
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
        case TOK_COLON2: {
            advance(p);
            Token name = advance(p);
            if (name.kind != TOK_CONST) {
                error(p, "expected constant name after '::'", name.line, name.col);
                return NULL;
            }
            Node *n = node_new(p->arena, NODE_CONST, s);
            n->sval = name.sval;
            return n;
        }
        case TOK_DOT2:
        case TOK_DOT3: {
            advance(p);
            Node *n = node_new(p->arena, NODE_RANGE, s);
            n->range.begin = NULL;
            n->range.end = range_rhs_omitted(p) ? NULL : parse_expr(p, infix_bp(t.kind).rbp);
            n->range.exclusive = (t.kind == TOK_DOT3);
            return n;
        }
        case TOK_CONST: {
            advance(p);
            const char *const_name = t.sval;
            size_t const_len = strlen(const_name);
            while (check(p, TOK_COLON2)) {
                Parser copy = *p;
                advance(&copy);
                Token part = advance(&copy);
                if (part.kind != TOK_CONST || const_path_segment_is_call(&copy, part))
                    break;
                match(p, TOK_COLON2);
                advance(p);
                size_t plen = strlen(part.sval);
                char *full = arena_alloc(p->arena, const_len + 2 + plen + 1);
                memcpy(full, const_name, const_len);
                memcpy(full + const_len, "::", 2);
                memcpy(full + const_len + 2, part.sval, plen + 1);
                const_name = full;
                const_len += 2 + plen;
            }
            if (const_name != t.sval) {
                Node *n = node_new(p->arena, NODE_CONST, s);
                n->sval = const_name;
                return n;
            }
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
                                 nxt.kind == TOK_REGEXP || nxt.kind == TOK_WORDS || nxt.kind == TOK_SYMBOLS ||
                                 nxt.kind == TOK_INTERP_BEG || nxt.kind == TOK_INT || nxt.kind == TOK_FLOAT ||
                                 nxt.kind == TOK_RATIONAL || nxt.kind == TOK_IMAGINARY ||
                                 nxt.kind == TOK_NIL || nxt.kind == TOK_TRUE || nxt.kind == TOK_FALSE ||
                                 nxt.kind == TOK_SELF || nxt.kind == TOK_IVAR || nxt.kind == TOK_GVAR ||
                                 nxt.kind == TOK_COLON2 ||
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
            if ((check(p, TOK_LBRACE) || (check(p, TOK_DO) && p->command_arg_depth == 0)) &&
                peek(p).line == t.line) {
                Node *n = node_new(p->arena, NODE_CALL, s);
                n->call.recv = NULL; n->call.method = t.sval; n->call.args = NULL; n->call.block = parse_block(p);
                return n;
            }
            int lparen_as_arg = (nxt.kind == TOK_LPAREN && nxt.col > t.col + t.len);
            int lbracket_as_arg = (nxt.kind == TOK_LBRACKET && nxt.col > t.col + t.len);
            Token sign_arg = peek_next_token(p);
            int can_be_arg = lparen_as_arg || lbracket_as_arg || nxt.kind == TOK_SYMBOL || nxt.kind == TOK_STRING ||
                             nxt.kind == TOK_REGEXP || nxt.kind == TOK_WORDS || nxt.kind == TOK_SYMBOLS ||
                             nxt.kind == TOK_INTERP_BEG || nxt.kind == TOK_INT || nxt.kind == TOK_FLOAT ||
                             nxt.kind == TOK_RATIONAL || nxt.kind == TOK_IMAGINARY ||
                             nxt.kind == TOK_NIL || nxt.kind == TOK_TRUE || nxt.kind == TOK_FALSE ||
                             nxt.kind == TOK_SELF || nxt.kind == TOK_IVAR || nxt.kind == TOK_GVAR ||
                             nxt.kind == TOK_COLON2 ||
                             nxt.kind == TOK_CONST || nxt.kind == TOK_IDENT || nxt.kind == TOK_BANG ||
                             nxt.kind == TOK_TILDE || nxt.kind == TOK_DEFINED ||
                             unary_prefix_arg(t, nxt, sign_arg);
            if (can_be_arg && nxt.line == t.line) {
                NodeList *args = parse_command_args(p);
                Node *block = NULL;
                if (check(p, TOK_LBRACE) || (check(p, TOK_DO) && p->command_arg_depth <= 1)) block = parse_block(p);
                Node *n = node_new(p->arena, NODE_CALL, s);
                n->call.recv = NULL; n->call.method = t.sval; n->call.args = args; n->call.block = block;
                return n;
            }
            /* Identifiers ending in ? or ! are always method calls, never local vars */
            {
                size_t ilen = strlen(t.sval);
                if (ilen > 0 && (t.sval[ilen - 1] == '?' || t.sval[ilen - 1] == '!')) {
                    Node *mc = node_new(p->arena, NODE_CALL, s);
                    mc->call.recv = NULL; mc->call.method = t.sval; mc->call.args = NULL;
                    return mc;
                }
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
            if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF) &&
                !check(p, TOK_END) && !check(p, TOK_IF) && !check(p, TOK_UNLESS) &&
                !check(p, TOK_WHILE) && !check(p, TOK_UNTIL) && !check(p, TOK_RESCUE) &&
                !check(p, TOK_RPAREN) && !check(p, TOK_RBRACKET) && !check(p, TOK_RBRACE) &&
                !check(p, TOK_COMMA) && !check(p, TOK_DOT) && !check(p, TOK_COLON2)) {
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
            Node *inner = NULL;
            skip_terminators(p);
            inner = parse_body_until_rparen(p);
            /* Unwrap single-statement body to keep the tree clean */
            if (inner && inner->kind == NODE_BODY && inner->body.stmts && !inner->body.stmts->next)
                inner = inner->body.stmts->node;
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
            if (!jump_value_terminator(peek(p).kind)) {
                Node *first = parse_expr(p, 0);
                n->jump.value = parse_jump_value(p, first, s);
            }
            return n;
        }
        case TOK_BREAK: {
            advance(p);
            Node *n = node_new(p->arena, NODE_BREAK, s);
            if (!jump_value_terminator(peek(p).kind))
                n->jump.value = parse_expr(p, 0);
            return n;
        }
        case TOK_NEXT: {
            advance(p);
            Node *n = node_new(p->arena, NODE_NEXT, s);
            if (!jump_value_terminator(peek(p).kind))
                n->jump.value = parse_expr(p, 0);
            return n;
        }
        case TOK_RETRY: {
            advance(p);
            return node_new(p->arena, NODE_RETRY, s);
        }
        case TOK_IF: case TOK_UNLESS: case TOK_WHILE: case TOK_UNTIL:
        case TOK_BEGIN: case TOK_CASE: case TOK_CLASS:
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
                                 nxt.kind == TOK_STRING || nxt.kind == TOK_REGEXP ||
                                 nxt.kind == TOK_WORDS || nxt.kind == TOK_SYMBOLS ||
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
