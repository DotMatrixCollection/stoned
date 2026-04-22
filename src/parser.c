#include "parser.h"
#include "rope.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */
static Node *parse_stmt(Parser *p);
static Node *parse_expr(Parser *p, int min_bp);
static Node *parse_primary(Parser *p);
static Node *parse_body(Parser *p, int stop_at_rbrace);
static NodeList *parse_args(Parser *p);
static NodeList *parse_params(Parser *p);
static Node *parse_block(Parser *p);

/* ------------------------------------------------------------------ */
/* Error handling                                                       */
/* ------------------------------------------------------------------ */
static void error(Parser *p, const char *msg, uint32_t line, uint32_t col) {
    if (p->error_count < MAX_ERRORS) {
        p->errors[p->error_count].message = msg;
        p->errors[p->error_count].line    = line;
        p->errors[p->error_count].col     = col;
        p->error_count++;
    }
    p->panic = 1;
}

static void sync(Parser *p) {
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

/* ------------------------------------------------------------------ */
/* Token utilities                                                      */
/* ------------------------------------------------------------------ */
static Token peek(Parser *p)   { return lexer_peek(&p->lexer); }
static Token advance(Parser *p) { return lexer_next(&p->lexer); }

static int check(Parser *p, TokenKind k) { return peek(p).kind == k; }

static int match(Parser *p, TokenKind k) {
    if (check(p, k)) { advance(p); return 1; }
    return 0;
}

static Token expect(Parser *p, TokenKind k, const char *msg) {
    Token t = peek(p);
    if (t.kind == k) return advance(p);
    error(p, msg, t.line, t.col);
    return t;
}

static void skip_terminators(Parser *p) {
    while (check(p, TOK_NEWLINE) || check(p, TOK_SEMICOLON))
        advance(p);
}

static Span tok_span(Token t) {
    Span s; s.line = t.line; s.col = t.col; s.len = t.len;
    return s;
}

/* ------------------------------------------------------------------ */
/* Pratt operator table                                                 */
/*                                                                      */
/* Returns {left_bp, right_bp} for infix operators.                    */
/* right_bp > left_bp = left-associative                               */
/* right_bp = left_bp+1 = right-associative (assignment, **)           */
/* Returns {0,0} if not an infix operator.                             */
/* ------------------------------------------------------------------ */
typedef struct { int lbp; int rbp; } BP;

static BP infix_bp(TokenKind k) {
    switch (k) {
        /* lowest: keyword operators */
        case TOK_OR:        return (BP){2, 3};
        case TOK_AND:       return (BP){4, 5};

        /* assignment — right-associative */
        case TOK_EQ:
        case TOK_PLUS_EQ:   case TOK_MINUS_EQ:  case TOK_STAR_EQ:
        case TOK_SLASH_EQ:  case TOK_PERCENT_EQ: case TOK_STAR2_EQ:
        case TOK_AMP_EQ:    case TOK_PIPE_EQ:   case TOK_CARET_EQ:
        case TOK_LSHIFT_EQ: case TOK_RSHIFT_EQ:
        case TOK_AMP2_EQ:   case TOK_PIPE2_EQ:
                            return (BP){6, 6};  /* right-assoc: same bp */

        case TOK_QUESTION:  return (BP){8, 7};  /* ternary, right-assoc */

        case TOK_PIPE2:     return (BP){10, 11};
        case TOK_AMP2:      return (BP){12, 13};

        case TOK_EQ2:  case TOK_NEQ:
        case TOK_EQ3:  case TOK_MATCH: case TOK_NMATCH:
                            return (BP){14, 15};

        case TOK_LT:   case TOK_LEQ:
        case TOK_GT:   case TOK_GEQ:
        case TOK_SPACESHIP:
                            return (BP){16, 17};

        case TOK_PIPE:      return (BP){18, 19};
        case TOK_CARET:     return (BP){20, 21};
        case TOK_AMP:       return (BP){22, 23};

        case TOK_LSHIFT:    case TOK_RSHIFT:
                            return (BP){24, 25};

        case TOK_PLUS:      case TOK_MINUS:
                            return (BP){26, 27};

        case TOK_STAR:      case TOK_SLASH:  case TOK_PERCENT:
                            return (BP){28, 29};

        case TOK_STAR2:     return (BP){32, 31};  /* right-assoc */

        case TOK_DOT:       return (BP){40, 41};
        case TOK_COLON2:    return (BP){40, 41};

        default: return (BP){0, 0};
    }
}

/* prefix binding power (for unary operators) */
static int prefix_bp(TokenKind k) {
    switch (k) {
        case TOK_NOT:   return 6;
        case TOK_BANG:  return 30;
        case TOK_MINUS: return 30;
        case TOK_TILDE: return 30;
        case TOK_PLUS:  return 30;
        default:        return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Expression parser (Pratt)                                           */
/* ------------------------------------------------------------------ */
static Node *parse_expr(Parser *p, int min_bp) {
    Node *left = parse_primary(p);
    if (!left) return NULL;

    while (1) {
        Token op = peek(p);

        /* handle ternary specially */
        if (op.kind == TOK_QUESTION) {
            BP bp = infix_bp(TOK_QUESTION);
            if (bp.lbp < min_bp) break;
            advance(p);
            Node *then_expr = parse_expr(p, 0);
            expect(p, TOK_COLON, "expected ':' in ternary");
            Node *else_expr = parse_expr(p, bp.rbp);
            Node *n = node_new(p->arena, NODE_IF, left->span);
            n->cond.cond      = left;
            n->cond.then_body = then_expr;
            n->cond.else_body = else_expr;
            left = n;
            continue;
        }

        BP bp = infix_bp(op.kind);
        if (bp.lbp == 0 || bp.lbp < min_bp) break;

        advance(p);

        /* assignment */
        if (op.kind == TOK_EQ) {
            Node *right = parse_expr(p, bp.rbp);
            Node *n = node_new(p->arena, NODE_ASSIGN, left->span);
            n->assign.target = left;
            n->assign.value  = right;
            left = n;
            continue;
        }

        /* compound assignment */
        if (op.kind >= TOK_PLUS_EQ && op.kind <= TOK_PIPE2_EQ) {
            Node *right = parse_expr(p, bp.rbp);
            Node *n = node_new(p->arena, NODE_OP_ASSIGN, left->span);
            n->binop.op    = token_kind_name(op.kind);
            n->binop.left  = left;
            n->binop.right = right;
            left = n;
            continue;
        }

        /* method call: recv.method or recv::Const */
        if (op.kind == TOK_DOT || op.kind == TOK_COLON2) {
            Token name_tok = advance(p);
            if (name_tok.kind != TOK_IDENT && name_tok.kind != TOK_CONST) {
                error(p, "expected method name after '.'", name_tok.line, name_tok.col);
                break;
            }
            Node *call = node_new(p->arena, NODE_CALL, left->span);
            call->call.recv   = left;
            call->call.method = name_tok.sval;

            /* optional args */
            if (check(p, TOK_LPAREN)) {
                advance(p);
                call->call.args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
            }

            /* optional block */
            if (check(p, TOK_LBRACE) || check(p, TOK_DO)) {
                call->call.block = parse_block(p);
            }

            left = call;
            continue;
        }

        /* regular binary op */
        Node *right = parse_expr(p, bp.rbp);
        Node *n = node_new(p->arena, NODE_BINOP, left->span);
        n->binop.op    = token_kind_name(op.kind);
        n->binop.left  = left;
        n->binop.right = right;
        left = n;
    }

    return left;
}

/* ------------------------------------------------------------------ */
/* Primary expressions                                                  */
/* ------------------------------------------------------------------ */
static Node *parse_primary(Parser *p) {
    Token t = peek(p);
    Span  s = tok_span(t);

    /* unary prefix */
    {
        int bp = prefix_bp(t.kind);
        if (bp) {
            advance(p);
            Node *operand = parse_expr(p, bp);
            Node *n = node_new(p->arena, NODE_UNOP, s);
            n->unop.op      = token_kind_name(t.kind);
            n->unop.operand = operand;
            return n;
        }
    }

    switch (t.kind) {
        case TOK_INT: {
            advance(p);
            Node *n = node_new(p->arena, NODE_INT, s);
            n->ival = t.ival;
            return n;
        }
        case TOK_FLOAT: {
            advance(p);
            Node *n = node_new(p->arena, NODE_FLOAT, s);
            n->fval = t.fval;
            return n;
        }
        case TOK_STRING: {
            advance(p);
            Node *n = node_new(p->arena, NODE_STRING, s);
            n->sval = t.sval;
            return n;
        }

        case TOK_INTERP_BEG: {
            advance(p);  /* consume the opening " */
            Node     *n    = node_new(p->arena, NODE_ROPE, s);
            RopeNode *rope = NULL;

            while (1) {
                Token seg = peek(p);

                if (seg.kind == TOK_INTERP_END || seg.kind == TOK_EOF) {
                    advance(p);
                    break;
                }

                if (seg.kind == TOK_INTERP_LIT) {
                    advance(p);
                    RopeNode *lit = rope_lit(p->arena, seg.sval,
                                            seg.sval ? strlen(seg.sval) : 0);
                    rope = rope ? rope_cat(p->arena, rope, lit) : lit;
                    continue;
                }

                if (seg.kind == TOK_INTERP_EXPR_BEG) {
                    advance(p);
                    /* skip empty #{} */
                    if (check(p, TOK_INTERP_EXPR_END)) { advance(p); continue; }
                    Node *expr = parse_expr(p, 0);
                    expect(p, TOK_INTERP_EXPR_END, "expected '}' to close interpolation");
                    RopeNode *er = rope_expr(p->arena, expr);
                    rope = rope ? rope_cat(p->arena, rope, er) : er;
                    continue;
                }

                /* shouldn't happen — bail */
                error(p, "unexpected token inside string interpolation",
                      seg.line, seg.col);
                advance(p);
                break;
            }

            /* empty string */
            if (!rope) rope = rope_lit(p->arena, "", 0);
            n->interp.rope = rope;

            /* Optimise: if the whole rope is static, produce NODE_STRING */
            if (rope_is_static(rope)) {
                n->kind  = NODE_STRING;
                n->sval  = rope_flatten(p->arena, rope);
            }

            return n;
        }
        case TOK_SYMBOL: {
            advance(p);
            Node *n = node_new(p->arena, NODE_SYMBOL, s);
            n->sval = t.sval;
            return n;
        }
        case TOK_NIL:   { advance(p); return node_new(p->arena, NODE_NIL,   s); }
        case TOK_TRUE:  { advance(p); return node_new(p->arena, NODE_TRUE,  s); }
        case TOK_FALSE: { advance(p); return node_new(p->arena, NODE_FALSE, s); }
        case TOK_SELF:  { advance(p); return node_new(p->arena, NODE_SELF,  s); }

        case TOK_IVAR: {
            advance(p);
            Node *n = node_new(p->arena, NODE_IVAR, s);
            n->sval = t.sval;
            return n;
        }
        case TOK_CVAR: {
            advance(p);
            Node *n = node_new(p->arena, NODE_CVAR, s);
            n->sval = t.sval;
            return n;
        }
        case TOK_GVAR: {
            advance(p);
            Node *n = node_new(p->arena, NODE_GVAR, s);
            n->sval = t.sval;
            return n;
        }
        case TOK_CONST: {
            advance(p);
            Node *n = node_new(p->arena, NODE_CONST, s);
            n->sval = t.sval;
            return n;
        }

        case TOK_IDENT: {
            advance(p);
            /* method call without parens: foo bar, baz */
            /* method call with parens: foo(bar, baz)   */
            /* bare local variable                      */
            if (check(p, TOK_LPAREN)) {
                advance(p);
                NodeList *args = parse_args(p);
                expect(p, TOK_RPAREN, "expected ')'");
                Node *block = NULL;
                if (check(p, TOK_LBRACE) || check(p, TOK_DO))
                    block = parse_block(p);
                Node *n = node_new(p->arena, NODE_CALL, s);
                n->call.recv   = NULL;
                n->call.method = t.sval;
                n->call.args   = args;
                n->call.block  = block;
                return n;
            }
            /* bare ident — could be local var or zero-arg method call;
               the semantic pass will distinguish */
            Node *n = node_new(p->arena, NODE_LVAR, s);
            n->sval = t.sval;
            return n;
        }

        /* Grouped expression */
        case TOK_LPAREN: {
            advance(p);
            Node *inner = parse_expr(p, 0);
            expect(p, TOK_RPAREN, "expected ')'");
            return inner;
        }

        /* Array literal */
        case TOK_LBRACKET: {
            advance(p);
            Node *n = node_new(p->arena, NODE_ARRAY, s);
            NodeList *elems = NULL;
            skip_terminators(p);
            while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
                Node *elem = parse_expr(p, 0);
                if (elem) elems = nodelist_append(p->arena, elems, elem);
                skip_terminators(p);
                if (!match(p, TOK_COMMA)) break;
                skip_terminators(p);
            }
            expect(p, TOK_RBRACKET, "expected ']'");
            n->array.elements = elems;
            return n;
        }

        /* Hash literal */
        case TOK_LBRACE: {
            advance(p);
            Node *n = node_new(p->arena, NODE_HASH, s);
            NodeList *pairs = NULL;
            skip_terminators(p);
            while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                Node *key = parse_expr(p, 0);
                expect(p, TOK_ARROW, "expected '=>' in hash literal");
                Node *val = parse_expr(p, 0);
                Node *pair = node_new(p->arena, NODE_PAIR, key->span);
                pair->pair.key   = key;
                pair->pair.value = val;
                pairs = nodelist_append(p->arena, pairs, pair);
                skip_terminators(p);
                if (!match(p, TOK_COMMA)) break;
                skip_terminators(p);
            }
            expect(p, TOK_RBRACE, "expected '}'");
            n->hash.pairs = pairs;
            return n;
        }

        case TOK_RETURN: {
            advance(p);
            Node *n = node_new(p->arena, NODE_RETURN, s);
            if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) &&
                !check(p, TOK_EOF) && !check(p, TOK_END))
                n->jump.value = parse_expr(p, 0);
            return n;
        }
        case TOK_BREAK: {
            advance(p);
            Node *n = node_new(p->arena, NODE_BREAK, s);
            if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) &&
                !check(p, TOK_EOF) && !check(p, TOK_END))
                n->jump.value = parse_expr(p, 0);
            return n;
        }
        case TOK_NEXT: {
            advance(p);
            Node *n = node_new(p->arena, NODE_NEXT, s);
            if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) &&
                !check(p, TOK_EOF) && !check(p, TOK_END))
                n->jump.value = parse_expr(p, 0);
            return n;
        }

        case TOK_SUPER: {
            advance(p);
            Node *n = node_new(p->arena, NODE_SUPER, s);
            if (check(p, TOK_LPAREN)) {
                /* super() or super(args) — explicit args, no forwarding */
                advance(p);
                n->super_call.args = parse_args(p);
                n->super_call.forward_args = 0;
                expect(p, TOK_RPAREN, "expected ')'");
            } else {
                /* bare super — forward current method's arguments */
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

/* ------------------------------------------------------------------ */
/* Argument list (already inside parens)                               */
/* ------------------------------------------------------------------ */
static NodeList *parse_args(Parser *p) {
    NodeList *args = NULL;
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        Node *arg = parse_expr(p, 0);
        if (arg) args = nodelist_append(p->arena, args, arg);
        if (!match(p, TOK_COMMA)) break;
    }
    return args;
}

/* ------------------------------------------------------------------ */
/* Block: do |params| body end  OR  { |params| body }                 */
/* ------------------------------------------------------------------ */
static Node *parse_block(Parser *p) {
    Token t = peek(p);
    int brace = (t.kind == TOK_LBRACE);
    Span s = tok_span(t);
    advance(p);

    Node *n = node_new(p->arena, NODE_BLOCK, s);

    /* optional params */
    if (check(p, TOK_PIPE)) {
        advance(p);
        n->block.params = parse_params(p);
        expect(p, TOK_PIPE, "expected '|' to close block params");
    }

    skip_terminators(p);
    n->block.body = parse_body(p, brace);

    if (brace) expect(p, TOK_RBRACE, "expected '}'");
    else       expect(p, TOK_END,    "expected 'end'");

    return n;
}

/* ------------------------------------------------------------------ */
/* Formal parameter list                                               */
/* ------------------------------------------------------------------ */
static NodeList *parse_params(Parser *p) {
    NodeList *params = NULL;
    while (!check(p, TOK_PIPE) && !check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        Token t = peek(p);
        Span s = tok_span(t);
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
            if (match(p, TOK_EQ)) {
                param->param.default_val = parse_expr(p, 0);
            }
        }

        params = nodelist_append(p->arena, params, param);
        if (!match(p, TOK_COMMA)) break;
    }
    return params;
}

/* ------------------------------------------------------------------ */
/* Statements                                                           */
/* ------------------------------------------------------------------ */
static Node *parse_stmt(Parser *p) {
    Token t = peek(p);
    Span  s = tok_span(t);

    /* if/unless as statement */
    if (t.kind == TOK_IF || t.kind == TOK_UNLESS) {
        advance(p);
        NodeKind kind = (t.kind == TOK_IF) ? NODE_IF : NODE_UNLESS;
        Node *cond = parse_expr(p, 0);
        /* optional 'then' or newline */
        if (!match(p, TOK_THEN)) skip_terminators(p);
        Node *then_body = parse_body(p, 0);
        Node *else_body = NULL;

        /* elsif / else chains */
        while (check(p, TOK_ELSIF)) {
            advance(p);
            Node *elsif_cond = parse_expr(p, 0);
            if (!match(p, TOK_THEN)) skip_terminators(p);
            Node *elsif_body = parse_body(p, 0);
            Node *elsif_node = node_new(p->arena, NODE_IF, s);
            elsif_node->cond.cond      = elsif_cond;
            elsif_node->cond.then_body = elsif_body;
            elsif_node->cond.else_body = NULL;
            /* chain: append to end of else_body chain */
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
        n->cond.cond      = cond;
        n->cond.then_body = then_body;
        n->cond.else_body = else_body;
        return n;
    }

    /* while / until */
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

    /* def */
    if (t.kind == TOK_DEF) {
        advance(p);
        Token name_tok = advance(p);
        if (name_tok.kind != TOK_IDENT && name_tok.kind != TOK_CONST) {
            error(p, "expected method name after 'def'", name_tok.line, name_tok.col);
            return NULL;
        }
        Node *n = node_new(p->arena, NODE_DEF, s);
        n->def.name = name_tok.sval;

        /* params */
        if (match(p, TOK_LPAREN)) {
            n->def.params = parse_params(p);
            expect(p, TOK_RPAREN, "expected ')'");
        }

        skip_terminators(p);
        n->def.body = parse_body(p, 0);
        expect(p, TOK_END, "expected 'end'");
        return n;
    }

    /* class */
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

        /* optional superclass: class Foo < Bar */
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

    /* expression statement — may have trailing if/unless/while/until */
    Node *expr = parse_expr(p, 0);
    if (!expr) return NULL;

    /* statement modifiers */
    t = peek(p);
    if (t.kind == TOK_IF || t.kind == TOK_UNLESS) {
        advance(p);
        NodeKind kind = (t.kind == TOK_IF) ? NODE_IF : NODE_UNLESS;
        Node *cond = parse_expr(p, 0);
        Node *n = node_new(p->arena, kind, s);
        n->cond.cond      = cond;
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

/* ------------------------------------------------------------------ */
/* Body: sequence of statements until end/else/elsif/ensure/rescue/EOF */
/* ------------------------------------------------------------------ */
static Node *parse_body(Parser *p, int stop_at_rbrace) {
    Span s = tok_span(peek(p));
    Node *n = node_new(p->arena, NODE_BODY, s);
    NodeList *stmts = NULL;

    while (1) {
        skip_terminators(p);
        Token t = peek(p);
        if (t.kind == TOK_EOF    || t.kind == TOK_END    ||
            t.kind == TOK_ELSE   || t.kind == TOK_ELSIF  ||
            t.kind == TOK_ENSURE || t.kind == TOK_RESCUE)
            break;
        if (stop_at_rbrace && t.kind == TOK_RBRACE)
            break;

        Node *stmt = parse_stmt(p);
        if (p->panic) { sync(p); continue; }
        if (stmt) stmts = nodelist_append(p->arena, stmts, stmt);
    }

    n->body.stmts = stmts;
    return n;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */
void parser_init(Parser *p, const char *src, size_t len, Arena *arena) {
    memset(p, 0, sizeof(*p));
    lexer_init(&p->lexer, src, len, arena);
    p->arena = arena;
}

Node *parse_program(Parser *p) {
    Span s = {1, 1, 0};
    Node *n = node_new(p->arena, NODE_PROGRAM, s);
    NodeList *stmts = NULL;

    skip_terminators(p);
    while (!check(p, TOK_EOF)) {
        Node *stmt = parse_stmt(p);
        if (p->panic) { sync(p); continue; }
        if (stmt) stmts = nodelist_append(p->arena, stmts, stmt);
        skip_terminators(p);
    }

    n->body.stmts = stmts;
    return n;
}
