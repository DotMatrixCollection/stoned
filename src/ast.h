#ifndef STONED_AST_H
#define STONED_AST_H

#include "arena.h"
#include "rope.h"
#include <stdint.h>

/* Source location — every node carries this */
typedef struct {
    uint32_t line;
    uint32_t col;
    uint32_t len;
} Span;

/* ------------------------------------------------------------------ */
/* Node kinds                                                           */
/* ------------------------------------------------------------------ */
typedef enum {
    /* Literals */
    NODE_INT,
    NODE_FLOAT,
    NODE_STRING,
    NODE_SYMBOL,
    NODE_NIL,
    NODE_TRUE,
    NODE_FALSE,
    NODE_SELF,

    /* Collections */
    NODE_ARRAY,
    NODE_HASH,
    NODE_PAIR,          /* key => value inside a hash */
    NODE_RANGE,         /* begin..end or begin...end */

    /* Variables */
    NODE_LVAR,          /* local */
    NODE_IVAR,          /* @foo */
    NODE_CVAR,          /* @@foo */
    NODE_GVAR,          /* $foo */
    NODE_CONST,         /* Foo */
    NODE_CONST_ACCESS,  /* expr::Foo */

    /* Assignment */
    NODE_ASSIGN,        /* lhs = rhs */
    NODE_OP_ASSIGN,     /* lhs += rhs etc. */

    /* Operators */
    NODE_BINOP,
    NODE_UNOP,

    /* Method call */
    NODE_CALL,          /* recv.meth(args) or meth(args) */
    NODE_BLOCK,         /* do |params| body end */

    /* Control flow */
    NODE_IF,
    NODE_UNLESS,
    NODE_WHILE,
    NODE_UNTIL,
    NODE_FOR,
    NODE_BEGIN,
    NODE_RESCUE,
    NODE_RETURN,
    NODE_BREAK,
    NODE_NEXT,
    NODE_RETRY,

    /* Definition */
    NODE_DEF,
    NODE_CLASS,         /* class Foo < Bar ... end */
    NODE_SCLASS,        /* class << expr ... end */
    NODE_MODULE,        /* module Foo ... end */
    NODE_ALIAS,         /* alias new_name old_name */
    NODE_PARAM,         /* single formal parameter */

    /* case/when */
    NODE_CASE,           /* case subject; when ...; end */
    NODE_WHEN,           /* when pattern, ...; body */

    /* pattern matching (Ruby 3+) */
    NODE_PATCASE,        /* case subject; in pattern ...; end */
    NODE_IN,             /* in pattern [if guard]; body */
    NODE_PATTERN_ARRAY,  /* [p1, p2, *rest] */
    NODE_PATTERN_HASH,   /* {key: pat, **rest} */
    NODE_PATTERN_CAPTURE,/* pattern => ident */
    NODE_PATTERN_PIN,    /* ^var */
    NODE_PATTERN_OR,     /* pat1 | pat2 */

    /* Super call */
    NODE_SUPER,

    /* Block pass — &expr in call args */
    NODE_BLOCK_PASS,
    NODE_DEFINED,

    /* Regexp literal */
    NODE_REGEXP,

    /* Interpolated string (rope) */
    NODE_ROPE,

    /* Structure */
    NODE_BODY,          /* sequence of statements */
    NODE_PROGRAM,       /* top-level body */
} NodeKind;

/* Forward declaration */
typedef struct Node Node;

/* ------------------------------------------------------------------ */
/* Node list (for args, body statements, params, etc.)                 */
/* ------------------------------------------------------------------ */
typedef struct NodeList {
    Node           *node;
    struct NodeList *next;
} NodeList;

/* ------------------------------------------------------------------ */
/* Node                                                                 */
/* ------------------------------------------------------------------ */
struct Node {
    NodeKind kind;
    Span     span;

    union {
        /* NODE_INT */
        int64_t     ival;

        /* NODE_FLOAT */
        double      fval;

        /* NODE_STRING, NODE_SYMBOL, NODE_LVAR, NODE_IVAR,
           NODE_CVAR, NODE_GVAR, NODE_CONST */
        const char *sval;

        /* NODE_CONST_ACCESS */
        struct {
            Node *recv;
            const char *name;
        } const_access;

        /* NODE_BINOP, NODE_OP_ASSIGN */
        struct {
            const char *op;
            Node       *left;
            Node       *right;
        } binop;

        /* NODE_UNOP */
        struct {
            const char *op;
            Node       *operand;
        } unop;

        /* NODE_ASSIGN */
        struct {
            Node *target;
            Node *value;
        } assign;

        /* NODE_CALL */
        struct {
            Node     *recv;     /* NULL for bare calls */
            const char *method;
            NodeList *args;
            Node     *block;    /* NULL if no block */
            int       safe_nav; /* recv&.meth */
        } call;

        /* NODE_BLOCK */
        struct {
            NodeList *params;
            Node     *body;
        } block;

        /* NODE_IF, NODE_UNLESS */
        struct {
            Node *cond;
            Node *then_body;
            Node *else_body;    /* NULL if absent */
        } cond;

        /* NODE_WHILE, NODE_UNTIL */
        struct {
            Node *cond;
            Node *body;
            int   post_test; /* 1 for begin...end while/until */
        } loop;

        /* NODE_FOR */
        struct {
            Node *target;
            Node *iterable;
            Node *body;
        } for_loop;

        /* NODE_BEGIN */
        struct {
            Node *body;
            NodeList *rescues;
            Node *else_body;
            Node *ensure_body;
        } begin_stmt;

        /* NODE_RESCUE */
        struct {
            NodeList *exception_classes; /* one or more; NULL = bare rescue */
            const char *exception_var;
            Node *body;
        } rescue_clause;

        /* NODE_DEF */
        struct {
            Node       *recv;       /* NULL for regular def */
            const char *name;
            NodeList   *params;
            Node       *body;
        } def;

        /* NODE_ALIAS */
        struct {
            const char *new_name;
            const char *old_name;
        } alias_stmt;

        /* NODE_CLASS */
        struct {
            const char *name;
            Node       *superclass;  /* NULL if no < */
            Node       *body;
        } klass;

        /* NODE_SCLASS */
        struct {
            Node *recv;
            Node *body;
        } sclass;

        /* NODE_PARAM */
        struct {
            const char *name;
            Node       *default_val; /* NULL if none */
            int         splat;       /* 1 if *param */
            int         block_param; /* 1 if &param */
            int         keyword_param;  /* 1 if key: or key: default */
            int         keyword_splat;  /* 1 if **opts */
        } param;

        /* NODE_RETURN, NODE_BREAK, NODE_NEXT */
        struct {
            Node *value;   /* NULL if bare */
        } jump;

        /* NODE_ARRAY */
        struct {
            NodeList *elements;
        } array;

        /* NODE_HASH */
        struct {
            NodeList *pairs;   /* list of NODE_PAIR */
            int       keyword_style; /* 1 for call-site label kwargs like foo(a: 1) */
        } hash;

        /* NODE_PAIR */
        struct {
            Node *key;
            Node *value;
        } pair;

        /* NODE_RANGE */
        struct {
            Node *begin;
            Node *end;
            int   exclusive;
        } range;

        /* NODE_CASE */
        struct {
            Node     *subject;   /* NULL for caseless form */
            NodeList *whens;     /* list of NODE_WHEN */
            Node     *else_body; /* NULL if absent */
        } case_stmt;

        /* NODE_WHEN */
        struct {
            NodeList *patterns;  /* one or more match expressions */
            Node     *body;
        } when_clause;

        /* NODE_PATCASE */
        struct {
            Node     *subject;   /* value being matched */
            NodeList *ins;       /* list of NODE_IN */
            Node     *else_body; /* NULL if absent */
        } patcase;

        /* NODE_IN */
        struct {
            Node *pattern;   /* pattern to match */
            Node *guard;     /* optional `if cond` guard, NULL if absent */
            Node *body;      /* body if matched */
        } in_clause;

        /* NODE_PATTERN_ARRAY, NODE_PATTERN_HASH */
        struct {
            NodeList *elements;    /* pattern elements; for hash: alternating key/value */
            int       has_rest;    /* 1 if has trailing *rest or **rest */
            int       has_pre_rest; /* 1 if leading * (find pattern) */
            const char *rest_name; /* name of trailing rest var, NULL if anonymous */
            const char *pre_rest_name; /* name of leading rest var, NULL if anonymous */
        } pattern_collection;

        /* NODE_PATTERN_CAPTURE */
        struct {
            Node       *pattern; /* sub-pattern (NULL means match-all) */
            const char *name;    /* variable to bind to */
        } pattern_capture;

        /* NODE_PATTERN_PIN: sval = variable name */
        /* NODE_PATTERN_OR */
        struct {
            Node *left;
            Node *right;
        } pattern_or;

        /* NODE_SUPER */
        struct {
            NodeList *args;
            int       forward_args; /* 1 = bare super, forward current args */
        } super_call;

        /* NODE_BLOCK_PASS */
        struct {
            Node *expr;
        } block_pass;

        /* NODE_DEFINED */
        struct {
            Node *expr;
        } defined_expr;

        /* NODE_REGEXP */
        struct {
            const char  *pattern;
            unsigned int options;
        } regexp_lit;

        /* NODE_ROPE */
        struct {
            RopeNode *rope;
        } interp;

        /* NODE_BODY, NODE_PROGRAM */
        struct {
            NodeList *stmts;
        } body;
    };
};

/* ------------------------------------------------------------------ */
/* Constructors — all allocate from arena                              */
/* ------------------------------------------------------------------ */
Node     *node_new(Arena *a, NodeKind kind, Span span);
NodeList *nodelist_append(Arena *a, NodeList *list, Node *node);

/* Helpers */
const char *node_kind_name(NodeKind k);

#endif
