#include "lexer.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Interpolation mode stack helpers                                     */
/* ------------------------------------------------------------------ */
static LexMode imode_top(Lexer *l) {
    return l->imode_depth > 0 ? l->imode[l->imode_depth - 1] : LMODE_NORMAL;
}
static void imode_push(Lexer *l, LexMode m) {
    if (l->imode_depth < LEX_INTERP_DEPTH) {
        l->imode[l->imode_depth]  = m;
        l->ibrace[l->imode_depth] = 0;
        l->imode_depth++;
    }
}
static void imode_pop(Lexer *l) {
    if (l->imode_depth > 0) l->imode_depth--;
}

/* ------------------------------------------------------------------ */
/* Keyword table                                                        */
/* ------------------------------------------------------------------ */
static struct { const char *word; TokenKind kind; } KEYWORDS[] = {
    {"nil",     TOK_NIL},
    {"true",    TOK_TRUE},
    {"false",   TOK_FALSE},
    {"self",    TOK_SELF},
    {"if",      TOK_IF},
    {"unless",  TOK_UNLESS},
    {"then",    TOK_THEN},
    {"elsif",   TOK_ELSIF},
    {"else",    TOK_ELSE},
    {"end",     TOK_END},
    {"while",   TOK_WHILE},
    {"until",   TOK_UNTIL},
    {"do",      TOK_DO},
    {"def",     TOK_DEF},
    {"class",   TOK_CLASS},
    {"module",  TOK_MODULE},
    {"alias",   TOK_ALIAS},
    {"return",  TOK_RETURN},
    {"break",   TOK_BREAK},
    {"next",    TOK_NEXT},
    {"and",     TOK_AND},
    {"or",      TOK_OR},
    {"not",     TOK_NOT},
    {"for",     TOK_FOR},
    {"in",      TOK_IN},
    {"rescue",  TOK_RESCUE},
    {"ensure",  TOK_ENSURE},
    {"begin",   TOK_BEGIN},
    {"yield",   TOK_YIELD},
    {"super",   TOK_SUPER},
    {"retry",   TOK_RETRY},
    {"case",    TOK_CASE},
    {"when",    TOK_WHEN},
    {"defined?", TOK_DEFINED},
    {NULL, 0}
};

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */
static int at_end(Lexer *l) { return l->pos >= l->len; }
static char peek_ch(Lexer *l) { return at_end(l) ? '\0' : l->src[l->pos]; }
static char peek2(Lexer *l)   { return (l->pos+1 < l->len) ? l->src[l->pos+1] : '\0'; }

static char advance(Lexer *l) {
    char c = l->src[l->pos++];
    if (c == '\n') { l->line++; l->line_start = l->pos; }
    return c;
}

static uint32_t col_of(Lexer *l, size_t pos) {
    return (uint32_t)(pos - l->line_start) + 1;
}

static void skip_whitespace(Lexer *l) {
    while (!at_end(l)) {
        char c = peek_ch(l);
        if (c == ' ' || c == '\t' || c == '\r') { advance(l); l->had_space = 1; }
        else if (c == '\\' && peek2(l) == '\n') {
            advance(l);
            advance(l);
            l->had_space = 1;
        }
        else if (c == '#') {
            while (!at_end(l) && peek_ch(l) != '\n') advance(l);
            l->had_space = 1;
        }
        else break;
    }
}

static int is_local_var(Lexer *l, const char *name) {
    for (LexLocalVar *e = l->local_vars; e; e = e->next)
        if (strcmp(e->name, name) == 0) return 1;
    return 0;
}

void lexer_mark_local(Lexer *l, const char *name) {
    if (!name) return;
    for (LexLocalVar *e = l->local_vars; e; e = e->next)
        if (strcmp(e->name, name) == 0) return;
    LexLocalVar *entry = arena_alloc(l->arena, sizeof(*entry));
    entry->name = name;
    entry->next = l->local_vars;
    l->local_vars = entry;
}

static const char *intern(Lexer *l, const char *start, size_t len) {
    char *buf = arena_alloc(l->arena, len + 1);
    memcpy(buf, start, len);
    buf[len] = '\0';
    return buf;
}

static Token make_tok(Lexer *l, TokenKind kind, size_t start_pos, uint32_t start_line, uint32_t start_col) {
    Token t;
    memset(&t, 0, sizeof(t));
    t.kind = kind;
    t.line = start_line;
    t.col  = start_col;
    t.len  = (uint32_t)(l->pos - start_pos);
    return t;
}

/* ------------------------------------------------------------------ */
/* Number scanning                                                      */
/* ------------------------------------------------------------------ */
static Token scan_number(Lexer *l, size_t start, uint32_t sline, uint32_t scol) {
    int is_float = 0;

    /* hex */
    if (peek_ch(l) == '0' && (peek2(l) == 'x' || peek2(l) == 'X')) {
        advance(l); advance(l);
        while (isxdigit(peek_ch(l)) || peek_ch(l) == '_') advance(l);
        Token t = make_tok(l, TOK_INT, start, sline, scol);
        t.ival = strtoll(l->src + start, NULL, 0);
        return t;
    }
    /* binary */
    if (peek_ch(l) == '0' && (peek2(l) == 'b' || peek2(l) == 'B')) {
        advance(l); advance(l);
        while (peek_ch(l) == '0' || peek_ch(l) == '1' || peek_ch(l) == '_') advance(l);
        Token t = make_tok(l, TOK_INT, start, sline, scol);
        t.ival = strtoll(l->src + start, NULL, 0);
        return t;
    }

    while (isdigit(peek_ch(l)) || peek_ch(l) == '_') advance(l);
    if (peek_ch(l) == '.' && isdigit(peek2(l))) {
        is_float = 1;
        advance(l);
        while (isdigit(peek_ch(l)) || peek_ch(l) == '_') advance(l);
    }
    if (peek_ch(l) == 'e' || peek_ch(l) == 'E') {
        is_float = 1;
        advance(l);
        if (peek_ch(l) == '+' || peek_ch(l) == '-') advance(l);
        while (isdigit(peek_ch(l))) advance(l);
    }

    Token t = make_tok(l, is_float ? TOK_FLOAT : TOK_INT, start, sline, scol);
    if (is_float) t.fval = strtod(l->src + start, NULL);
    else          t.ival = strtoll(l->src + start, NULL, 10);
    return t;
}

/* ------------------------------------------------------------------ */
/* String scanning (single-quoted, no interpolation)                   */
/* ------------------------------------------------------------------ */
static Token scan_string_sq(Lexer *l, size_t start, uint32_t sline, uint32_t scol) {
    /* opening ' already consumed */
    size_t content_start = l->pos;
    while (!at_end(l)) {
        char c = peek_ch(l);
        if (c == '\'') break;
        if (c == '\\' && peek2(l) == '\'') { advance(l); }
        advance(l);
    }
    Token t = make_tok(l, TOK_STRING, start, sline, scol);
    t.sval = intern(l, l->src + content_start, l->pos - content_start);
    if (!at_end(l)) advance(l); /* consume closing ' */
    return t;
}

/* Double-quoted string — basic escape handling, no interpolation yet */
static Token scan_string_dq(Lexer *l, size_t start, uint32_t sline, uint32_t scol) {
    size_t cap = 64;
    char *buf = arena_alloc(l->arena, cap);
    size_t blen = 0;
    const char *err = NULL;

#define BUF_PUSH(ch) do { \
    if (blen + 1 >= cap) { \
        char *nb = arena_alloc(l->arena, cap * 2); \
        memcpy(nb, buf, blen); buf = nb; cap *= 2; \
    } \
    buf[blen++] = (ch); \
} while(0)

    while (!at_end(l)) {
        char c = peek_ch(l);
        if (c == '"') break;
        advance(l);
        if (c == '\\') {
            char esc = advance(l);
            switch (esc) {
                case 'n':  BUF_PUSH('\n'); break;
                case 't':  BUF_PUSH('\t'); break;
                case 'r':  BUF_PUSH('\r'); break;
                case '\\': BUF_PUSH('\\'); break;
                case '"':  BUF_PUSH('"');  break;
                case '0':  err = "invalid \\0 escape in UTF-8 string"; break;
                default:   BUF_PUSH('\\'); BUF_PUSH(esc); break;
            }
            if (err) break;
        } else {
            BUF_PUSH(c);
        }
    }
    BUF_PUSH('\0');
    if (!at_end(l)) advance(l); /* consume closing " */

    Token t = make_tok(l, err ? TOK_ERROR : TOK_STRING, start, sline, scol);
    t.sval = buf;
    if (err) t.sval = err;
    return t;
#undef BUF_PUSH
}

static char paired_delim(char open) {
    switch (open) {
        case '[': return ']';
        case '(': return ')';
        case '{': return '}';
        case '<': return '>';
        default: return open;
    }
}

static Token scan_percent_list(Lexer *l, size_t start, uint32_t sline, uint32_t scol, TokenKind kind) {
    advance(l); /* consume w or i */
    if (at_end(l)) {
        Token t = make_tok(l, TOK_ERROR, start, sline, scol);
        t.sval = "unterminated percent literal";
        return t;
    }

    char open = advance(l);
    char close = paired_delim(open);
    int nested = (open != close);
    int depth = 0;
    size_t content_start = l->pos;

    while (!at_end(l)) {
        char c = peek_ch(l);
        if (c == '\\') {
            advance(l);
            if (!at_end(l)) advance(l);
            continue;
        }
        if (nested && c == open) {
            depth++;
            advance(l);
            continue;
        }
        if (c == close) {
            if (depth == 0) {
                Token t = make_tok(l, kind, start, sline, scol);
                t.sval = intern(l, l->src + content_start, l->pos - content_start);
                advance(l);
                return t;
            }
            depth--;
            advance(l);
            continue;
        }
        advance(l);
    }

    Token t = make_tok(l, TOK_ERROR, start, sline, scol);
    t.sval = "unterminated percent literal";
    return t;
}

/* ------------------------------------------------------------------ */
/* Symbol scanning                                                      */
/* ------------------------------------------------------------------ */
static Token scan_symbol(Lexer *l, size_t start, uint32_t sline, uint32_t scol) {
    /* ':' already consumed */
    if (peek_ch(l) == '"') {
        advance(l);
        Token inner = scan_string_dq(l, start, sline, scol);
        inner.kind = TOK_SYMBOL;
        return inner;
    }
    if (peek_ch(l) == '\'') {
        advance(l);
        Token inner = scan_string_sq(l, start, sline, scol);
        inner.kind = TOK_SYMBOL;
        return inner;
    }
    size_t sym_start = l->pos;
    if (isalpha(peek_ch(l)) || peek_ch(l) == '_') {
        while (isalnum(peek_ch(l)) || peek_ch(l) == '_') advance(l);
        /* allow trailing ? or ! */
        if (peek_ch(l) == '?' || peek_ch(l) == '!') advance(l);
    } else if (peek_ch(l) == '@') {
        advance(l);
        if (peek_ch(l) == '@') advance(l);
        while (isalnum(peek_ch(l)) || peek_ch(l) == '_') advance(l);
    } else if (peek_ch(l) == '$') {
        advance(l);
        if (isalnum(peek_ch(l)) || peek_ch(l) == '_') {
            while (isalnum(peek_ch(l)) || peek_ch(l) == '_') advance(l);
        } else if (peek_ch(l) != '\0') {
            advance(l);
        }
    } else {
        /* operator symbols: :+ :- :* :/ :% :** :<< :>> :<=> :<= :>= :< :> :== :!= :=== :[] :[]= :& :| :^ :~ :! */
        char c = peek_ch(l);
        if (c == '+' || c == '-' || c == '%' || c == '~' || c == '&' || c == '|' || c == '^') {
            advance(l);
        } else if (c == '*') {
            advance(l);
            if (peek_ch(l) == '*') advance(l);
        } else if (c == '/') {
            advance(l);
        } else if (c == '<') {
            advance(l);
            if (peek_ch(l) == '<') advance(l);
            else if (peek_ch(l) == '=') { advance(l); if (peek_ch(l) == '>') advance(l); }
        } else if (c == '>') {
            advance(l);
            if (peek_ch(l) == '>') advance(l);
            else if (peek_ch(l) == '=') advance(l);
        } else if (c == '=') {
            advance(l);
            if (peek_ch(l) == '=') { advance(l); if (peek_ch(l) == '=') advance(l); }
        } else if (c == '!') {
            advance(l);
            if (peek_ch(l) == '=') advance(l);
        } else if (c == '[') {
            advance(l);
            if (peek_ch(l) == ']') {
                advance(l);
                if (peek_ch(l) == '=') advance(l);
            }
        }
    }
    Token t = make_tok(l, TOK_SYMBOL, start, sline, scol);
    t.sval = intern(l, l->src + sym_start, l->pos - sym_start);
    return t;
}

/* ------------------------------------------------------------------ */
/* Identifier / keyword scanning                                        */
/* ------------------------------------------------------------------ */
static Token scan_ident(Lexer *l, size_t start, uint32_t sline, uint32_t scol) {
    while (isalnum(peek_ch(l)) || peek_ch(l) == '_') advance(l);
    if (peek_ch(l) == '?' || peek_ch(l) == '!') advance(l);

    size_t word_len = l->pos - start;
    const char *word = l->src + start;

    /* keyword check */
    for (int i = 0; KEYWORDS[i].word; i++) {
        if (strlen(KEYWORDS[i].word) == word_len &&
            memcmp(KEYWORDS[i].word, word, word_len) == 0) {
            Token t = make_tok(l, KEYWORDS[i].kind, start, sline, scol);
            t.sval = KEYWORDS[i].word;
            return t;
        }
    }

    TokenKind kind = isupper((unsigned char)word[0]) ? TOK_CONST : TOK_IDENT;
    Token t = make_tok(l, kind, start, sline, scol);
    t.sval = intern(l, word, word_len);
    return t;
}

/* ------------------------------------------------------------------ */
/* Interpolated string content scanner                                  */
/*                                                                      */
/* Called when imode_top == LMODE_INTERP_STR.                          */
/* Returns:                                                             */
/*   TOK_INTERP_LIT      — literal segment (may be empty only at EOF)  */
/*   TOK_INTERP_EXPR_BEG — encountered #{, pushed LMODE_INTERP_EXPR    */
/*   TOK_INTERP_END      — encountered closing ", popped mode           */
/* ------------------------------------------------------------------ */
static Token scan_interp_str_content(Lexer *l) {
    size_t   start = l->pos;
    uint32_t sline = l->line;
    uint32_t scol  = col_of(l, start);

    /* Closing quote or end-of-string before any chars → INTERP_END */
    if (at_end(l) || peek_ch(l) == '"') {
        if (!at_end(l)) advance(l);
        imode_pop(l);
        Token t; memset(&t, 0, sizeof(t));
        t.kind = TOK_INTERP_END;
        t.line = sline; t.col = scol; t.len = 1;
        return t;
    }

    /* Interpolation start before any chars → INTERP_EXPR_BEG */
    if (peek_ch(l) == '#' && peek2(l) == '{') {
        advance(l); advance(l); /* consume #{ */
        imode_push(l, LMODE_INTERP_EXPR);
        Token t; memset(&t, 0, sizeof(t));
        t.kind = TOK_INTERP_EXPR_BEG;
        t.line = sline; t.col = scol; t.len = 2;
        return t;
    }

    /* Accumulate literal chars into the arena */
    size_t cap  = 64;
    char  *buf  = arena_alloc(l->arena, cap);
    size_t blen = 0;
    const char *err = NULL;

#define IBUF_PUSH(ch) do { \
    if (blen + 1 >= cap) { \
        char *nb = arena_alloc(l->arena, cap * 2); \
        memcpy(nb, buf, blen); buf = nb; cap *= 2; \
    } \
    buf[blen++] = (char)(ch); \
} while(0)

    while (!at_end(l)) {
        char c = peek_ch(l);

        if (c == '"') break;  /* closing quote — will be consumed next call */

        if (c == '#' && peek2(l) == '{') break;  /* interpolation — handled next call */

        advance(l);
        if (c == '\\') {
            if (at_end(l)) break;
            char esc = advance(l);
            switch (esc) {
                case 'n':  IBUF_PUSH('\n'); break;
                case 't':  IBUF_PUSH('\t'); break;
                case 'r':  IBUF_PUSH('\r'); break;
                case '\\': IBUF_PUSH('\\'); break;
                case '"':  IBUF_PUSH('"');  break;
                case '#':  IBUF_PUSH('#');  break;
                case '0':  err = "invalid \\0 escape in UTF-8 string"; break;
                default:   IBUF_PUSH('\\'); IBUF_PUSH(esc); break;
            }
            if (err) break;
        } else {
            IBUF_PUSH(c);
        }
    }
    IBUF_PUSH('\0');

#undef IBUF_PUSH

    Token t; memset(&t, 0, sizeof(t));
    t.kind = err ? TOK_ERROR : TOK_INTERP_LIT;
    t.line = sline; t.col = scol;
    t.len  = (uint32_t)(l->pos - start);
    t.sval = err ? err : buf;
    return t;
}

/* ------------------------------------------------------------------ */
/* Heredoc support                                                      */
/* ------------------------------------------------------------------ */

static int heredoc_min_indent(const char *src, size_t start, size_t end) {
    int min_ind = -1;
    size_t i = start;
    while (i < end) {
        int indent = 0;
        while (i < end && (src[i] == ' ' || src[i] == '\t')) { i++; indent++; }
        if (i < end && src[i] != '\n') {
            if (min_ind < 0 || indent < min_ind) min_ind = indent;
        }
        while (i < end && src[i] != '\n') i++;
        if (i < end) i++;
    }
    return min_ind < 0 ? 0 : min_ind;
}

/* Called when hd_active && imode_top == LMODE_INTERP_STR.
   Reads body content from l->src[l->pos..hd_body_end), stripping
   hd_min_indent leading chars at the start of each line. */
static Token scan_heredoc_content(Lexer *l) {
    /* Skip leading indent at beginning of line */
    if (l->hd_at_bol && l->hd_min_indent > 0) {
        for (int i = 0; i < l->hd_min_indent && l->pos < l->hd_body_end; i++) {
            char c = l->src[l->pos];
            if (c != ' ' && c != '\t') break;
            advance(l);
        }
        l->hd_at_bol = 0;
    }

    uint32_t sline = l->line;
    uint32_t scol  = col_of(l, l->pos);

    /* End of body */
    if (l->pos >= l->hd_body_end) {
        imode_pop(l);
        l->hd_active = 0;
        l->pos        = l->hd_resume;
        l->line       = l->hd_resume_line;
        l->line_start = l->hd_resume_line_start;
        Token t; memset(&t, 0, sizeof(t));
        t.kind = TOK_INTERP_END; t.line = sline; t.col = scol;
        return t;
    }

    /* Interpolation start */
    if (l->src[l->pos] == '#' &&
        l->pos + 1 < l->hd_body_end && l->src[l->pos + 1] == '{') {
        advance(l); advance(l);
        imode_push(l, LMODE_INTERP_EXPR);
        Token t; memset(&t, 0, sizeof(t));
        t.kind = TOK_INTERP_EXPR_BEG; t.line = sline; t.col = scol; t.len = 2;
        return t;
    }

    /* Accumulate literal chars */
    size_t cap  = 64;
    char  *buf  = arena_alloc(l->arena, cap);
    size_t blen = 0;

#define HD_PUSH(ch) do { \
    if (blen + 1 >= cap) { \
        char *nb = arena_alloc(l->arena, cap * 2); \
        memcpy(nb, buf, blen); buf = nb; cap *= 2; \
    } \
    buf[blen++] = (char)(ch); \
} while(0)

    while (l->pos < l->hd_body_end) {
        char c = l->src[l->pos];
        if (c == '#' && l->pos + 1 < l->hd_body_end && l->src[l->pos + 1] == '{') break;
        advance(l);
        if (c == '\\') {
            if (l->pos >= l->hd_body_end) { HD_PUSH('\\'); break; }
            char esc = advance(l);
            switch (esc) {
                case 'n':  HD_PUSH('\n'); break;
                case 't':  HD_PUSH('\t'); break;
                case 'r':  HD_PUSH('\r'); break;
                case '\\': HD_PUSH('\\'); break;
                case '#':  HD_PUSH('#');  break;
                default:   HD_PUSH('\\'); HD_PUSH(esc); break;
            }
        } else {
            HD_PUSH(c);
            if (c == '\n' && l->hd_min_indent > 0) {
                l->hd_at_bol = 1;
                break; /* emit segment; next call handles indent skip */
            }
        }
    }
    HD_PUSH('\0');

#undef HD_PUSH

    Token t; memset(&t, 0, sizeof(t));
    t.kind = TOK_INTERP_LIT; t.line = sline; t.col = scol;
    t.len  = (uint32_t)(blen - 1);
    t.sval = buf;
    return t;
}

static Token scan_heredoc(Lexer *l, size_t start, uint32_t sline, uint32_t scol) {
    /* `<<` already consumed */
    int squiggly = 0, indent_ok = 0, interp = 1;
    char quote = 0;

    if (peek_ch(l) == '-') { indent_ok = 1; advance(l); }
    else if (peek_ch(l) == '~') { squiggly = 1; indent_ok = 1; advance(l); }
    char nc = peek_ch(l);
    if      (nc == '"')  { quote = '"';  interp = 1; advance(l); }
    else if (nc == '\'') { quote = '\''; interp = 0; advance(l); }

    if (!isalpha((unsigned char)peek_ch(l)) && peek_ch(l) != '_') {
        /* Not a heredoc after all */
        Token t = make_tok(l, TOK_LSHIFT, start, sline, scol);
        return t;
    }

    size_t term_start = l->pos;
    while (isalnum((unsigned char)peek_ch(l)) || peek_ch(l) == '_') advance(l);
    size_t      term_len = l->pos - term_start;
    const char *term     = l->src + term_start;

    if (quote && peek_ch(l) == quote) advance(l);

    /* Consume rest of current line (rest-of-line after <<...IDENT is discarded) */
    while (!at_end(l) && peek_ch(l) != '\n') advance(l);
    if (!at_end(l)) advance(l); /* consume the \n */

    /* l->pos is now at the first body line */
    size_t   body_start      = l->pos;
    uint32_t body_line       = l->line;
    uint32_t body_line_start = l->line_start;

    /* Scan forward to locate the terminator line */
    size_t   body_end   = body_start;
    size_t   resume_pos = body_start;
    int      found      = 0;

    while (l->pos < l->len) {
        size_t line_head = l->pos;

        /* Check for optional leading whitespace before terminator */
        size_t ws = l->pos;
        while (l->pos < l->len && (l->src[l->pos] == ' ' || l->src[l->pos] == '\t'))
            l->pos++;
        size_t indent = l->pos - ws;

        int is_term = 0;
        if (l->pos + term_len <= l->len &&
            memcmp(l->src + l->pos, term, term_len) == 0) {
            size_t after = l->pos + term_len;
            if (after >= l->len || l->src[after] == '\n' || l->src[after] == '\r') {
                if (indent_ok || indent == 0) is_term = 1;
            }
        }

        if (is_term) {
            body_end  = line_head;
            l->pos   += term_len;
            if (l->pos < l->len && l->src[l->pos] == '\r') l->pos++;
            if (l->pos < l->len && l->src[l->pos] == '\n') {
                l->line++; l->line_start = l->pos + 1; l->pos++;
            }
            resume_pos = l->pos;
            found = 1;
            break;
        }

        while (l->pos < l->len && l->src[l->pos] != '\n') l->pos++;
        if (l->pos < l->len) { l->line++; l->line_start = l->pos + 1; l->pos++; }
    }

    if (!found) { body_end = resume_pos = l->len; }

    if (!interp) {
        /* Non-interpolating: build the body string directly */
        const char *raw     = l->src + body_start;
        size_t      raw_len = body_end - body_start;
        char *body;
        if (squiggly && raw_len > 0) {
            int min_ind = heredoc_min_indent(l->src, body_start, body_end);
            body = arena_alloc(l->arena, raw_len + 1);
            size_t blen = 0, i = body_start;
            while (i < body_end) {
                int sk = 0;
                while (sk < min_ind && i < body_end &&
                       (l->src[i] == ' ' || l->src[i] == '\t')) { i++; sk++; }
                while (i < body_end && l->src[i] != '\n') body[blen++] = l->src[i++];
                if (i < body_end) { body[blen++] = '\n'; i++; }
            }
            body[blen] = '\0';
        } else {
            body = arena_alloc(l->arena, raw_len + 1);
            memcpy(body, raw, raw_len);
            body[raw_len] = '\0';
        }
        l->pos = resume_pos;
        /* l->line and l->line_start already reflect resume_pos from the forward scan */
        Token t = make_tok(l, TOK_STRING, start, sline, scol);
        t.sval = body;
        return t;
    }

    /* Interpolating: configure heredoc body scan state */
    l->hd_active            = 1;
    l->hd_body_end          = body_end;
    l->hd_resume            = resume_pos;
    l->hd_resume_line       = l->line;
    l->hd_resume_line_start = l->line_start;
    l->hd_min_indent        = squiggly ? heredoc_min_indent(l->src, body_start, body_end) : 0;
    l->hd_at_bol            = (l->hd_min_indent > 0) ? 1 : 0;

    /* Rewind to body start so scan_heredoc_content reads from there */
    l->pos        = body_start;
    l->line       = body_line;
    l->line_start = body_line_start;

    imode_push(l, LMODE_INTERP_STR);
    Token t = make_tok(l, TOK_INTERP_BEG, start, sline, scol);
    return t;
}

/* ------------------------------------------------------------------ */
/* Regexp literal scanner                                               */
/* ------------------------------------------------------------------ */
static Token scan_regexp(Lexer *l, size_t start, uint32_t sline, uint32_t scol) {
    /* opening '/' already consumed */
    size_t cap = 64;
    char *buf = arena_alloc(l->arena, cap);
    size_t blen = 0;
    int in_class = 0;

#define RBUF_PUSH(ch) do { \
    if (blen + 1 >= cap) { \
        char *nb = arena_alloc(l->arena, cap * 2); \
        memcpy(nb, buf, blen); buf = nb; cap *= 2; \
    } \
    buf[blen++] = (ch); \
} while(0)

    while (!at_end(l)) {
        char c = peek_ch(l);
        if (c == '\\') {
            advance(l); RBUF_PUSH('\\');
            if (!at_end(l)) RBUF_PUSH(advance(l));
            continue;
        }
        if (c == '[' && !in_class) { in_class = 1; advance(l); RBUF_PUSH('['); continue; }
        if (c == ']' && in_class)  { in_class = 0; advance(l); RBUF_PUSH(']'); continue; }
        if (c == '/' && !in_class) break;
        if (c == '\n') break;
        RBUF_PUSH(advance(l));
    }
    RBUF_PUSH('\0');

    if (at_end(l) || peek_ch(l) != '/') {
        Token t = make_tok(l, TOK_ERROR, start, sline, scol);
        t.sval = "unterminated regexp literal";
        return t;
    }
    advance(l); /* consume closing '/' */

    /* flags: i=ignorecase(1), m=multiline(2), x=extended(4) */
    int64_t flags = 0;
    while (!at_end(l)) {
        char f = peek_ch(l);
        if      (f == 'i') { flags |= 1; advance(l); }
        else if (f == 'm') { flags |= 2; advance(l); }
        else if (f == 'x') { flags |= 4; advance(l); }
        else if (isalpha((unsigned char)f)) { advance(l); } /* skip unknown */
        else break;
    }

    Token t = make_tok(l, TOK_REGEXP, start, sline, scol);
    t.sval = buf;
    t.ival = flags;
    return t;
#undef RBUF_PUSH
}

/* ------------------------------------------------------------------ */
/* Main scan                                                            */
/* ------------------------------------------------------------------ */
static Token scan(Lexer *l) {
    /* String content mode — don't skip whitespace, it's significant */
    if (imode_top(l) == LMODE_INTERP_STR) {
        if (l->hd_active) return scan_heredoc_content(l);
        return scan_interp_str_content(l);
    }

    l->had_space = 0;
    skip_whitespace(l);

    if (at_end(l)) {
        Token t; memset(&t, 0, sizeof(t));
        t.kind = TOK_EOF;
        t.line = l->line;
        t.col  = col_of(l, l->pos);
        return t;
    }

    size_t   start = l->pos;
    uint32_t sline = l->line;
    uint32_t scol  = col_of(l, start);
    char c = advance(l);

#define SIMPLE(k) do { return make_tok(l, (k), start, sline, scol); } while(0)
#define PEEK_EQ(k1, k2) ((peek_ch(l) == '=') ? (advance(l), (k2)) : (k1))

    switch (c) {
        case '\n': {
            Token t = make_tok(l, TOK_NEWLINE, start, sline, scol);
            return t;
        }
        case ';': SIMPLE(TOK_SEMICOLON);
        case ',': SIMPLE(TOK_COMMA);
        case '(': l->state = LEX_EXPR_BEG; SIMPLE(TOK_LPAREN);
        case ')': l->state = LEX_EXPR_END; SIMPLE(TOK_RPAREN);
        case '[': l->state = LEX_EXPR_BEG; SIMPLE(TOK_LBRACKET);
        case ']': l->state = LEX_EXPR_END; SIMPLE(TOK_RBRACKET);
        case '{':
            l->state = LEX_EXPR_BEG;
            if (imode_top(l) == LMODE_INTERP_EXPR)
                l->ibrace[l->imode_depth - 1]++;
            SIMPLE(TOK_LBRACE);
        case '}':
            l->state = LEX_EXPR_END;
            /* Close interpolation if we're inside #{...} */
            if (imode_top(l) == LMODE_INTERP_EXPR) {
                if (l->ibrace[l->imode_depth - 1] > 0) {
                    l->ibrace[l->imode_depth - 1]--;
                    SIMPLE(TOK_RBRACE);
                }
                imode_pop(l);  /* back to LMODE_INTERP_STR */
                Token t = make_tok(l, TOK_INTERP_EXPR_END, start, sline, scol);
                return t;
            }
            SIMPLE(TOK_RBRACE);
        case '~': SIMPLE(TOK_TILDE);
        case '?': SIMPLE(TOK_QUESTION);

        case '+':
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_PLUS_EQ); }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_PLUS);
        case '-':
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_MINUS_EQ); }
            if (peek_ch(l) == '>') { advance(l); SIMPLE(TOK_LAMBDA); }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_MINUS);
        case '%':
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_PERCENT_EQ); }
            if (peek_ch(l) == 'w') return scan_percent_list(l, start, sline, scol, TOK_WORDS);
            if (peek_ch(l) == 'i') return scan_percent_list(l, start, sline, scol, TOK_SYMBOLS);
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_PERCENT);
        case '^':
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_CARET_EQ); }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_CARET);

        case '*':
            if (peek_ch(l) == '*') {
                advance(l);
                if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_STAR2_EQ); }
                l->state = LEX_EXPR_BEG;
                SIMPLE(TOK_STAR2);
            }
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_STAR_EQ); }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_STAR);

        case '/':
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_SLASH_EQ); }
            if (l->state == LEX_EXPR_BEG || l->state == LEX_EXPR_MID ||
                (l->state == LEX_EXPR_ARG && l->had_space))
                return scan_regexp(l, start, sline, scol);
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_SLASH);

        case '&':
            if (peek_ch(l) == '.') {
                advance(l);
                SIMPLE(TOK_ANDDOT);
            }
            if (peek_ch(l) == '&') {
                advance(l);
                if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_AMP2_EQ); }
                l->state = LEX_EXPR_BEG;
                SIMPLE(TOK_AMP2);
            }
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_AMP_EQ); }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_AMP);

        case '|':
            if (peek_ch(l) == '|') {
                advance(l);
                if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_PIPE2_EQ); }
                l->state = LEX_EXPR_BEG;
                SIMPLE(TOK_PIPE2);
            }
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_PIPE_EQ); }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_PIPE);

        case '<':
            if (peek_ch(l) == '<') {
                advance(l);
                {
                    char hd_nc = peek_ch(l);
                    if (hd_nc == '-' || hd_nc == '~' || hd_nc == '"' || hd_nc == '\'' ||
                        isalpha((unsigned char)hd_nc) || hd_nc == '_') {
                        return scan_heredoc(l, start, sline, scol);
                    }
                }
                if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_LSHIFT_EQ); }
                l->state = LEX_EXPR_BEG;
                SIMPLE(TOK_LSHIFT);
            }
            if (peek_ch(l) == '=') {
                advance(l);
                if (peek_ch(l) == '>') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_SPACESHIP); }
                l->state = LEX_EXPR_BEG;
                SIMPLE(TOK_LEQ);
            }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_LT);

        case '>':
            if (peek_ch(l) == '>') {
                advance(l);
                if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_RSHIFT_EQ); }
                l->state = LEX_EXPR_BEG;
                SIMPLE(TOK_RSHIFT);
            }
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_GEQ); }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_GT);

        case '=':
            if (peek_ch(l) == '=') {
                advance(l);
                if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_EQ3); }
                l->state = LEX_EXPR_BEG;
                SIMPLE(TOK_EQ2);
            }
            if (peek_ch(l) == '>') { advance(l); SIMPLE(TOK_ARROW); }
            if (peek_ch(l) == '~') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_MATCH); }
            l->state = LEX_EXPR_BEG;
            SIMPLE(TOK_EQ);

        case '!':
            if (peek_ch(l) == '=') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_NEQ); }
            if (peek_ch(l) == '~') { advance(l); l->state = LEX_EXPR_BEG; SIMPLE(TOK_NMATCH); }
            SIMPLE(TOK_BANG);

        case '.':
            if (peek_ch(l) == '.') {
                advance(l);
                if (peek_ch(l) == '.') { advance(l); SIMPLE(TOK_DOT3); }
                SIMPLE(TOK_DOT2);
            }
            SIMPLE(TOK_DOT);

        case ':':
            if (peek_ch(l) == ':') { advance(l); SIMPLE(TOK_COLON2); }
            /* symbol if followed by ident char, quote, or operator char */
            {
                char pc = peek_ch(l);
                if (isalpha(pc) || pc == '_' || pc == '"' || pc == '\'' ||
                    pc == '@' || pc == '$' ||
                    pc == '+' || pc == '-' || pc == '*' || pc == '/' || pc == '%' ||
                    pc == '<' || pc == '>' || pc == '=' || pc == '!' ||
                    pc == '&' || pc == '|' || pc == '^' || pc == '~' || pc == '[') {
                    return scan_symbol(l, start, sline, scol);
                }
            }
            SIMPLE(TOK_COLON);

        case '@':
            if (peek_ch(l) == '@') {
                advance(l);
                size_t ns = l->pos;
                while (isalnum(peek_ch(l)) || peek_ch(l) == '_') advance(l);
                Token t = make_tok(l, TOK_CVAR, start, sline, scol);
                t.sval = intern(l, l->src + ns, l->pos - ns);
                return t;
            } else {
                size_t ns = l->pos;
                while (isalnum(peek_ch(l)) || peek_ch(l) == '_') advance(l);
                Token t = make_tok(l, TOK_IVAR, start, sline, scol);
                t.sval = intern(l, l->src + ns, l->pos - ns);
                return t;
            }

        case '$': {
            size_t ns = l->pos;
            if (isalnum(peek_ch(l)) || peek_ch(l) == '_') {
                while (isalnum(peek_ch(l)) || peek_ch(l) == '_') advance(l);
            } else if (peek_ch(l) != '\0') {
                advance(l);
            }
            Token t = make_tok(l, TOK_GVAR, start, sline, scol);
            t.sval = intern(l, l->src + ns, l->pos - ns);
            return t;
        }

        case '\'': return scan_string_sq(l, start, sline, scol);
        case '"': {
            /* Push interpolation string mode; content scanned on next call */
            imode_push(l, LMODE_INTERP_STR);
            Token t = make_tok(l, TOK_INTERP_BEG, start, sline, scol);
            return t;
        }

        default:
            if (isdigit((unsigned char)c)) {
                /* back up one so scan_number sees the full number */
                l->pos--;
                return scan_number(l, start, sline, scol);
            }
            if (isalpha((unsigned char)c) || c == '_') {
                l->pos--;
                return scan_ident(l, start, sline, scol);
            }
            {
                Token t = make_tok(l, TOK_ERROR, start, sline, scol);
                t.sval = intern(l, &c, 1);
                return t;
            }
    }

#undef SIMPLE
#undef PEEK_EQ
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */
void lexer_init(Lexer *l, const char *src, size_t len, Arena *arena) {
    memset(l, 0, sizeof(*l));
    l->src   = src;
    l->len   = len;
    l->line  = 1;
    l->state = LEX_EXPR_BEG;
    l->arena = arena;
}

Token lexer_next(Lexer *l) {
    Token t;
    if (l->has_peeked) {
        l->has_peeked = 0;
        t = l->peeked;
    } else {
        t = scan(l);
    }
    /* Track expr state so '/' disambiguation works on the next token. */
    switch (t.kind) {
        case TOK_IDENT:
            /* Known local var → value expression; unknown → might be method call */
            l->state = is_local_var(l, t.sval) ? LEX_EXPR_END : LEX_EXPR_ARG;
            break;
        case TOK_CONST:
        case TOK_IVAR:  case TOK_CVAR:  case TOK_GVAR:
        case TOK_INT:   case TOK_FLOAT:
        case TOK_STRING: case TOK_SYMBOL:
        case TOK_WORDS:  case TOK_SYMBOLS: case TOK_REGEXP:
        case TOK_INTERP_END:
        case TOK_NIL:  case TOK_TRUE:  case TOK_FALSE:  case TOK_SELF:
        case TOK_RPAREN: case TOK_RBRACKET: case TOK_RBRACE:
        case TOK_RETRY:
            l->state = LEX_EXPR_END;
            break;
        default:
            l->state = LEX_EXPR_BEG;
            break;
    }
    return t;
}

Token lexer_peek(Lexer *l) {
    if (!l->has_peeked) {
        l->peeked    = scan(l);
        l->has_peeked = 1;
    }
    return l->peeked;
}

void lexer_consume(Lexer *l) {
    lexer_next(l);
}

const char *token_kind_name(TokenKind k) {
    switch (k) {
        case TOK_INT:        return "INT";
        case TOK_FLOAT:      return "FLOAT";
        case TOK_STRING:          return "STRING";
        case TOK_SYMBOL:          return "SYMBOL";
        case TOK_HEREDOC:         return "HEREDOC";
        case TOK_REGEXP:          return "REGEXP";
        case TOK_INTERP_BEG:      return "INTERP_BEG";
        case TOK_INTERP_LIT:      return "INTERP_LIT";
        case TOK_INTERP_EXPR_BEG: return "INTERP_EXPR_BEG";
        case TOK_INTERP_EXPR_END: return "INTERP_EXPR_END";
        case TOK_INTERP_END:      return "INTERP_END";
        case TOK_IDENT:      return "IDENT";
        case TOK_CONST:      return "CONST";
        case TOK_IVAR:       return "IVAR";
        case TOK_CVAR:       return "CVAR";
        case TOK_GVAR:       return "GVAR";
        case TOK_NIL:        return "nil";
        case TOK_TRUE:       return "true";
        case TOK_FALSE:      return "false";
        case TOK_SELF:       return "self";
        case TOK_IF:         return "if";
        case TOK_UNLESS:     return "unless";
        case TOK_THEN:       return "then";
        case TOK_ELSIF:      return "elsif";
        case TOK_ELSE:       return "else";
        case TOK_END:        return "end";
        case TOK_WHILE:      return "while";
        case TOK_UNTIL:      return "until";
        case TOK_DO:         return "do";
        case TOK_DEF:        return "def";
        case TOK_CLASS:      return "class";
        case TOK_MODULE:     return "module";
        case TOK_ALIAS:      return "alias";
        case TOK_RETURN:     return "return";
        case TOK_BREAK:      return "break";
        case TOK_NEXT:       return "next";
        case TOK_AND:        return "and";
        case TOK_OR:         return "or";
        case TOK_NOT:        return "not";
        case TOK_IN:         return "in";
        case TOK_RESCUE:     return "rescue";
        case TOK_ENSURE:     return "ensure";
        case TOK_BEGIN:      return "begin";
        case TOK_YIELD:      return "yield";
        case TOK_SUPER:      return "super";
        case TOK_CASE:       return "case";
        case TOK_WHEN:       return "when";
        case TOK_DEFINED:    return "defined?";
        case TOK_WORDS:      return "%w";
        case TOK_SYMBOLS:    return "%i";
        case TOK_PLUS:       return "+";
        case TOK_MINUS:      return "-";
        case TOK_STAR:       return "*";
        case TOK_STAR2:      return "**";
        case TOK_SLASH:      return "/";
        case TOK_PERCENT:    return "%";
        case TOK_AMP:        return "&";
        case TOK_ANDDOT:     return "&.";
        case TOK_AMP2:       return "&&";
        case TOK_PIPE:       return "|";
        case TOK_PIPE2:      return "||";
        case TOK_CARET:      return "^";
        case TOK_TILDE:      return "~";
        case TOK_LSHIFT:     return "<<";
        case TOK_RSHIFT:     return ">>";
        case TOK_EQ:         return "=";
        case TOK_EQ2:        return "==";
        case TOK_EQ3:        return "===";
        case TOK_NEQ:        return "!=";
        case TOK_LT:         return "<";
        case TOK_LEQ:        return "<=";
        case TOK_GT:         return ">";
        case TOK_GEQ:        return ">=";
        case TOK_SPACESHIP:  return "<=>";
        case TOK_MATCH:      return "=~";
        case TOK_NMATCH:     return "!~";
        case TOK_BANG:       return "!";
        case TOK_DOT:        return ".";
        case TOK_DOT2:       return "..";
        case TOK_DOT3:       return "...";
        case TOK_COLON:      return ":";
        case TOK_COLON2:     return "::";
        case TOK_COMMA:      return ",";
        case TOK_SEMICOLON:  return ";";
        case TOK_LPAREN:     return "(";
        case TOK_RPAREN:     return ")";
        case TOK_LBRACKET:   return "[";
        case TOK_RBRACKET:   return "]";
        case TOK_LBRACE:     return "{";
        case TOK_RBRACE:     return "}";
        case TOK_PIPE_BLOCK: return "|block|";
        case TOK_ARROW:      return "=>";
        case TOK_LAMBDA:     return "->";
        case TOK_QUESTION:   return "?";
        case TOK_PLUS_EQ:    return "+=";
        case TOK_MINUS_EQ:   return "-=";
        case TOK_STAR_EQ:    return "*=";
        case TOK_SLASH_EQ:   return "/=";
        case TOK_PERCENT_EQ: return "%=";
        case TOK_AMP_EQ:     return "&=";
        case TOK_PIPE_EQ:    return "|=";
        case TOK_CARET_EQ:   return "^=";
        case TOK_LSHIFT_EQ:  return "<<=";
        case TOK_RSHIFT_EQ:  return ">>=";
        case TOK_STAR2_EQ:   return "**=";
        case TOK_AMP2_EQ:    return "&&=";
        case TOK_PIPE2_EQ:   return "||=";
        case TOK_NEWLINE:    return "NEWLINE";
        case TOK_EOF:        return "EOF";
        case TOK_ERROR:      return "ERROR";
        default:             return "???";
    }
}
