#ifndef STONED_LEXER_H
#define STONED_LEXER_H

#include "arena.h"
#include <stdint.h>

typedef enum {
    /* Literals */
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,         /* single-quoted string (no interpolation) */
    TOK_SYMBOL,         /* :foo or :"foo" */
    TOK_WORDS,          /* %w[...] */
    TOK_SYMBOLS,        /* %i[...] */
    TOK_HEREDOC,
    TOK_REGEXP,          /* /pattern/flags */

    /* Interpolated string tokens */
    TOK_INTERP_BEG,     /* opening " */
    TOK_INTERP_LIT,     /* literal segment within "..." */
    TOK_INTERP_EXPR_BEG,/* #{ */
    TOK_INTERP_EXPR_END,/* } closing an interpolation */
    TOK_INTERP_END,     /* closing " */

    /* Identifiers & keywords */
    TOK_IDENT,          /* bare identifier — may be method or local var */
    TOK_CONST,          /* starts with uppercase */
    TOK_IVAR,           /* @foo */
    TOK_CVAR,           /* @@foo */
    TOK_GVAR,           /* $foo */

    TOK_NIL,
    TOK_TRUE,
    TOK_FALSE,
    TOK_SELF,
    TOK_IF,
    TOK_UNLESS,
    TOK_THEN,
    TOK_ELSIF,
    TOK_ELSE,
    TOK_END,
    TOK_WHILE,
    TOK_UNTIL,
    TOK_DO,
    TOK_DEF,
    TOK_CLASS,
    TOK_MODULE,
    TOK_ALIAS,
    TOK_RETURN,
    TOK_BREAK,
    TOK_NEXT,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_FOR,
    TOK_IN,
    TOK_RESCUE,
    TOK_ENSURE,
    TOK_BEGIN,
    TOK_YIELD,
    TOK_SUPER,
    TOK_RETRY,
    TOK_CASE,
    TOK_WHEN,
    TOK_DEFINED,

    /* Operators */
    TOK_PLUS,           /* + */
    TOK_MINUS,          /* - */
    TOK_STAR,           /* * */
    TOK_STAR2,          /* ** */
    TOK_SLASH,          /* / */
    TOK_PERCENT,        /* % */
    TOK_AMP,            /* & */
    TOK_ANDDOT,         /* &. */
    TOK_AMP2,           /* && */
    TOK_PIPE,           /* | */
    TOK_PIPE2,          /* || */
    TOK_CARET,          /* ^ */
    TOK_TILDE,          /* ~ */
    TOK_LSHIFT,         /* << */
    TOK_RSHIFT,         /* >> */
    TOK_EQ,             /* = */
    TOK_EQ2,            /* == */
    TOK_EQ3,            /* === */
    TOK_NEQ,            /* != */
    TOK_LT,             /* < */
    TOK_LEQ,            /* <= */
    TOK_GT,             /* > */
    TOK_GEQ,            /* >= */
    TOK_SPACESHIP,      /* <=> */
    TOK_MATCH,          /* =~ */
    TOK_NMATCH,         /* !~ */
    TOK_BANG,           /* ! */
    TOK_DOT,            /* . */
    TOK_DOT2,           /* .. */
    TOK_DOT3,           /* ... */
    TOK_COLON,          /* : */
    TOK_COLON2,         /* :: */
    TOK_COMMA,          /* , */
    TOK_SEMICOLON,      /* ; */
    TOK_LPAREN,         /* ( */
    TOK_RPAREN,         /* ) */
    TOK_LBRACKET,       /* [ */
    TOK_RBRACKET,       /* ] */
    TOK_LBRACE,         /* { */
    TOK_RBRACE,         /* } */
    TOK_PIPE_BLOCK,     /* | used as block param delim */
    TOK_ARROW,          /* => */
    TOK_LAMBDA,         /* -> */
    TOK_QUESTION,       /* ? */

    /* Compound assignment */
    TOK_PLUS_EQ,        /* += */
    TOK_MINUS_EQ,       /* -= */
    TOK_STAR_EQ,        /* *= */
    TOK_SLASH_EQ,       /* /= */
    TOK_PERCENT_EQ,     /* %= */
    TOK_AMP_EQ,         /* &= */
    TOK_PIPE_EQ,        /* |= */
    TOK_CARET_EQ,       /* ^= */
    TOK_LSHIFT_EQ,      /* <<= */
    TOK_RSHIFT_EQ,      /* >>= */
    TOK_STAR2_EQ,       /* **= */
    TOK_AMP2_EQ,        /* &&= */
    TOK_PIPE2_EQ,       /* ||= */

    /* Structure */
    TOK_NEWLINE,
    TOK_EOF,
    TOK_ERROR,
} TokenKind;

typedef struct {
    TokenKind   kind;
    uint32_t    line;
    uint32_t    col;
    uint32_t    len;
    /* For string/ident/symbol tokens: interned pointer into arena */
    const char *sval;
    int64_t     ival;
    double      fval;
} Token;

/* Lexer context — tracks state needed for context-sensitive scanning */
typedef enum {
    LEX_EXPR_BEG,   /* beginning of expression: / is regex, + is unary */
    LEX_EXPR_END,   /* end of expression: / is division, + is binary */
    LEX_EXPR_ARG,   /* after method name: space matters for args */
    LEX_EXPR_MID,   /* inside expression */
} LexState;

/* String interpolation mode stack */
#define LEX_INTERP_DEPTH 32

typedef enum {
    LMODE_NORMAL,           /* ordinary scanning */
    LMODE_INTERP_STR,       /* inside "..." — scanning string content */
    LMODE_INTERP_BACKTICK,  /* inside `...` — scanning backtick content */
    LMODE_INTERP_EXPR,      /* inside #{...} — scanning expression */
} LexMode;

typedef struct LexLocalVar {
    const char *name;
    struct LexLocalVar *next;
} LexLocalVar;

typedef struct {
    const char *src;
    size_t      len;
    size_t      pos;
    uint32_t    line;
    uint32_t    line_start;  /* pos of current line start, for col calc */
    LexState    state;
    Arena      *arena;

    /* interpolation mode stack */
    LexMode     imode[LEX_INTERP_DEPTH];
    int         imode_depth;
    int         ibrace[LEX_INTERP_DEPTH]; /* { } nesting depth per INTERP_EXPR level */

    /* heredoc body scanning state */
    int         hd_active;            /* non-zero while scanning heredoc body */
    size_t      hd_body_end;          /* exclusive end of body in l->src */
    size_t      hd_resume;            /* l->pos to restore after heredoc ends */
    uint32_t    hd_resume_line;       /* l->line at hd_resume */
    uint32_t    hd_resume_line_start; /* l->line_start at hd_resume */
    uint32_t    hd_body_line;         /* l->line at body start */
    uint32_t    hd_body_line_start;   /* l->line_start at body start */
    int         hd_min_indent;        /* chars to strip at BOL (squiggly) */
    int         hd_at_bol;            /* pending indent-strip at beginning of line */
    int         hd_imode_depth;       /* imode_depth at heredoc body start */

    /* rest-of-line buffer: tokens after <<HEREDOC on the same source line
       are deferred and emitted after the body's INTERP_END / TOK_STRING */
    int         hd_pending;               /* 1 while draining rest-of-line */
    size_t      hd_rol_start;             /* first char of rest-of-line */
    size_t      hd_rol_end;               /* exclusive end (before \n) */
    uint32_t    hd_rol_line;
    uint32_t    hd_rol_line_start;
    size_t      hd_final_resume;          /* destination after rol is exhausted */
    uint32_t    hd_final_resume_line;
    uint32_t    hd_final_resume_line_start;

    /* one-token lookahead buffer */
    Token       peeked;
    int         has_peeked;

    /* local-variable table for '/' disambiguation */
    LexLocalVar *local_vars;
    int          had_space;   /* set by skip_whitespace; used by '/' scanner */
    int          prev_was_dot; /* previous meaningful token was DOT/ANDDOT */

    /* close char for %(...) / %{...} / %[...] interpolated string literals */
    char         percent_close[LEX_INTERP_DEPTH];
} Lexer;

void  lexer_init(Lexer *l, const char *src, size_t len, Arena *arena);
Token lexer_next(Lexer *l);
Token lexer_peek(Lexer *l);
void  lexer_consume(Lexer *l);
void  lexer_mark_local(Lexer *l, const char *name);

const char *token_kind_name(TokenKind k);

#endif
