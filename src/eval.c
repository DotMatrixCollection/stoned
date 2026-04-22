#include "eval.h"
#include "rope.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Error helpers                                                        */
/* ------------------------------------------------------------------ */
static Value eval_error(Eval *ev, Node *n, const char *fmt, ...) {
    if (!ev->errored) {
        ev->errored = 1;
        va_list ap;
        va_start(ap, fmt);
        char tmp[480];
        vsnprintf(tmp, sizeof(tmp), fmt, ap);
        va_end(ap);
        if (n)
            snprintf(ev->errmsg, sizeof(ev->errmsg),
                     "%u:%u: %s", n->span.line, n->span.col, tmp);
        else
            snprintf(ev->errmsg, sizeof(ev->errmsg), "%s", tmp);
    }
    return val_nil();
}

#define ERR(node, ...) return eval_error(ev, node, __VA_ARGS__)
#define CHECK(v) do { if (ev->errored || val_is_signal(v)) return (v); } while(0)

/* ------------------------------------------------------------------ */
/* Rope flattening at runtime                                           */
/* ------------------------------------------------------------------ */
static const char *eval_rope(Eval *ev, Env *env, RopeNode *r);

static void rope_collect(Eval *ev, Env *env, RopeNode *r,
                         char **buf, size_t *len, size_t *cap) {
    if (!r) return;
    switch (r->kind) {
        case ROPE_LIT:
            if (r->lit.bytes && r->lit.len) {
                while (*len + r->lit.len + 1 > *cap) {
                    *cap  = (*cap < 64) ? 128 : *cap * 2;
                    *buf  = realloc(*buf, *cap);
                }
                memcpy(*buf + *len, r->lit.bytes, r->lit.len);
                *len += r->lit.len;
            }
            break;
        case ROPE_EXPR: {
            Value v = eval_node(ev, env, r->expr.node);
            if (ev->errored) return;
            const char *s = val_to_s(ev->arena, v);
            size_t slen = strlen(s);
            while (*len + slen + 1 > *cap) {
                *cap  = (*cap < 64) ? 128 : *cap * 2;
                *buf  = realloc(*buf, *cap);
            }
            memcpy(*buf + *len, s, slen);
            *len += slen;
            break;
        }
        case ROPE_CAT:
            rope_collect(ev, env, r->cat.left,  buf, len, cap);
            rope_collect(ev, env, r->cat.right, buf, len, cap);
            break;
    }
}

static const char *eval_rope(Eval *ev, Env *env, RopeNode *r) {
    size_t len = 0, cap = 128;
    char  *buf = malloc(cap);
    rope_collect(ev, env, r, &buf, &len, &cap);
    buf[len] = '\0';
    /* intern into arena so caller doesn't have to free */
    char *result = arena_alloc(ev->arena, len + 1);
    memcpy(result, buf, len + 1);
    free(buf);
    return result;
}

/* ------------------------------------------------------------------ */
/* Call a block value with arguments                                    */
/* ------------------------------------------------------------------ */
static Value call_block(Eval *ev, Value blk, Value *args, int argc, Node *call_site) {
    if (blk.kind != VAL_BLOCK) ERR(call_site, "no block given");

    Node *bn       = blk.block.block_node;
    Env  *closure  = blk.block.closure;
    Env  *frame    = env_new(ev->arena, closure, 0);

    /* bind params */
    NodeList *pl = bn->block.params;
    for (int i = 0; pl && i < argc; i++, pl = pl->next) {
        Node *p = pl->node;
        if (p && p->kind == NODE_PARAM && p->param.name)
            env_set(ev->arena, frame, p->param.name, args[i]);
    }

    Value result = eval_node(ev, frame, bn->block.body);

    /* catch next — it's "return from block iteration" */
    if (result.kind == VAL_NEXT) return *result.wrapped;
    return result;
}

/* Forward declaration */
static Value eval_call(Eval *ev, Env *env, Node *node);

/* ------------------------------------------------------------------ */
/* Built-in method dispatch                                             */
/* ------------------------------------------------------------------ */
static Value builtin_kernel(Eval *ev, Env *env __attribute__((unused)), const char *name,
                             Value *args, int argc, Value *blk, Node *site) {
    if (strcmp(name, "puts") == 0) {
        if (argc == 0) { fprintf(ev->out, "\n"); return val_nil(); }
        for (int i = 0; i < argc; i++) {
            if (args[i].kind == VAL_ARRAY) {
                for (size_t j = 0; j < args[i].array.len; j++)
                    fprintf(ev->out, "%s\n", val_to_s(ev->arena, args[i].array.elems[j]));
            } else {
                fprintf(ev->out, "%s\n", val_to_s(ev->arena, args[i]));
            }
        }
        return val_nil();
    }
    if (strcmp(name, "print") == 0) {
        for (int i = 0; i < argc; i++)
            fprintf(ev->out, "%s", val_to_s(ev->arena, args[i]));
        return val_nil();
    }
    if (strcmp(name, "p") == 0) {
        for (int i = 0; i < argc; i++)
            fprintf(ev->out, "%s\n", val_inspect(ev->arena, args[i]));
        if (argc == 1) return args[0];
        /* return array of args */
        Value arr = val_array_new();
        for (int i = 0; i < argc; i++) val_array_push(&arr, args[i]);
        return arr;
    }
    if (strcmp(name, "raise") == 0) {
        const char *msg = argc > 0 ? val_to_s(ev->arena, args[0]) : "RuntimeError";
        ERR(site, "RuntimeError: %s", msg);
    }
    if (strcmp(name, "rand") == 0) {
        if (argc == 0) return val_float((double)rand() / RAND_MAX);
        int64_t n = argc > 0 ? args[0].ival : 1;
        if (n <= 0) return val_int(0);
        return val_int((int64_t)(rand() % n));
    }
    if (strcmp(name, "exit") == 0) {
        int code = argc > 0 ? (int)args[0].ival : 0;
        exit(code);
    }
    (void)blk;
    return val_nil();  /* unknown — caller handles */
}

static Value dispatch_method(Eval *ev, Env *env __attribute__((unused)), Value recv,
                              const char *name, Value *args, int argc,
                              Value *blk, Node *site) {
    /* ---- Integer ---- */
    if (recv.kind == VAL_INT) {
        if (strcmp(name, "to_s")   == 0) return val_string(ev->arena, val_to_s(ev->arena, recv));
        if (strcmp(name, "to_f")   == 0) return val_float((double)recv.ival);
        if (strcmp(name, "to_i")   == 0) return recv;
        if (strcmp(name, "abs")    == 0) return val_int(recv.ival < 0 ? -recv.ival : recv.ival);
        if (strcmp(name, "even?")  == 0) return val_bool(recv.ival % 2 == 0);
        if (strcmp(name, "odd?")   == 0) return val_bool(recv.ival % 2 != 0);
        if (strcmp(name, "zero?")  == 0) return val_bool(recv.ival == 0);
        if (strcmp(name, "times")  == 0) {
            if (!blk) ERR(site, "Integer#times requires a block");
            for (int64_t i = 0; i < recv.ival; i++) {
                Value arg = val_int(i);
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
        if (strcmp(name, "upto") == 0) {
            if (argc < 1) ERR(site, "Integer#upto requires an argument");
            if (!blk) ERR(site, "Integer#upto requires a block");
            for (int64_t i = recv.ival; i <= args[0].ival; i++) {
                Value arg = val_int(i);
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
        if (strcmp(name, "downto") == 0) {
            if (argc < 1) ERR(site, "Integer#downto requires an argument");
            if (!blk) ERR(site, "Integer#downto requires a block");
            for (int64_t i = recv.ival; i >= args[0].ival; i--) {
                Value arg = val_int(i);
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
    }

    /* ---- Float ---- */
    if (recv.kind == VAL_FLOAT) {
        if (strcmp(name, "to_s")  == 0) return val_string(ev->arena, val_to_s(ev->arena, recv));
        if (strcmp(name, "to_f")  == 0) return recv;
        if (strcmp(name, "to_i")  == 0) return val_int((int64_t)recv.fval);
        if (strcmp(name, "abs")   == 0) return val_float(recv.fval < 0 ? -recv.fval : recv.fval);
        if (strcmp(name, "ceil")  == 0) return val_int((int64_t)ceil(recv.fval));
        if (strcmp(name, "floor") == 0) return val_int((int64_t)floor(recv.fval));
        if (strcmp(name, "round") == 0) return val_int((int64_t)round(recv.fval));
        if (strcmp(name, "zero?") == 0) return val_bool(recv.fval == 0.0);
    }

    /* ---- String ---- */
    if (recv.kind == VAL_STRING) {
        const char *s = recv.sval ? recv.sval : "";
        if (strcmp(name, "to_s")     == 0) return recv;
        if (strcmp(name, "to_i")     == 0) return val_int(atoll(s));
        if (strcmp(name, "to_f")     == 0) return val_float(atof(s));
        if (strcmp(name, "to_sym")   == 0) return val_symbol(s);
        if (strcmp(name, "length")   == 0 ||
            strcmp(name, "size")     == 0) return val_int((int64_t)strlen(s));
        if (strcmp(name, "empty?")   == 0) return val_bool(s[0] == '\0');
        if (strcmp(name, "upcase")   == 0) {
            size_t len = strlen(s);
            char *buf = arena_alloc(ev->arena, len + 1);
            for (size_t i = 0; i <= len; i++)
                buf[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
            return val_string(ev->arena, buf);
        }
        if (strcmp(name, "downcase") == 0) {
            size_t len = strlen(s);
            char *buf = arena_alloc(ev->arena, len + 1);
            for (size_t i = 0; i <= len; i++)
                buf[i] = (char)(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
            return val_string(ev->arena, buf);
        }
        if (strcmp(name, "strip")    == 0) {
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            size_t len = strlen(s);
            while (len > 0 && (s[len-1]==' '||s[len-1]=='\t'||
                               s[len-1]=='\n'||s[len-1]=='\r')) len--;
            return val_string_n(ev->arena, s, len);
        }
        if (strcmp(name, "chars")    == 0) {
            Value arr = val_array_new();
            for (size_t i = 0; s[i]; i++) val_array_push(&arr, val_string_n(ev->arena, s+i, 1));
            return arr;
        }
        if (strcmp(name, "include?") == 0) {
            if (argc < 1) ERR(site, "String#include? requires an argument");
            const char *needle = val_to_s(ev->arena, args[0]);
            return val_bool(strstr(s, needle) != NULL);
        }
        if (strcmp(name, "start_with?") == 0) {
            if (argc < 1) ERR(site, "String#start_with? requires an argument");
            const char *needle = val_to_s(ev->arena, args[0]);
            size_t nlen = strlen(needle);
            return val_bool(strncmp(s, needle, nlen) == 0);
        }
        if (strcmp(name, "end_with?") == 0) {
            if (argc < 1) ERR(site, "String#end_with? requires an argument");
            const char *needle = val_to_s(ev->arena, args[0]);
            size_t slen = strlen(s), nlen = strlen(needle);
            return val_bool(slen >= nlen && strcmp(s + slen - nlen, needle) == 0);
        }
        if (strcmp(name, "split") == 0) {
            Value arr = val_array_new();
            const char *sep = argc > 0 ? val_to_s(ev->arena, args[0]) : " ";
            size_t seplen = strlen(sep);
            if (seplen == 0) {
                for (size_t i = 0; s[i]; i++)
                    val_array_push(&arr, val_string_n(ev->arena, s+i, 1));
            } else {
                const char *p = s;
                const char *found;
                while ((found = strstr(p, sep)) != NULL) {
                    val_array_push(&arr, val_string_n(ev->arena, p, (size_t)(found - p)));
                    p = found + seplen;
                }
                val_array_push(&arr, val_string(ev->arena, p));
            }
            return arr;
        }
        if (strcmp(name, "each_char") == 0) {
            if (!blk) ERR(site, "String#each_char requires a block");
            for (size_t i = 0; s[i]; i++) {
                Value ch = val_string_n(ev->arena, s+i, 1);
                Value r  = call_block(ev, *blk, &ch, 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
        if (strcmp(name, "reverse") == 0) {
            size_t len = strlen(s);
            char *buf = arena_alloc(ev->arena, len + 1);
            for (size_t i = 0; i < len; i++) buf[i] = s[len - 1 - i];
            buf[len] = '\0';
            return val_string(ev->arena, buf);
        }
        if (strcmp(name, "replace") == 0) {
            if (argc < 1) ERR(site, "String#replace requires an argument");
            return val_string(ev->arena, val_to_s(ev->arena, args[0]));
        }
        if (strcmp(name, "*") == 0) {
            if (argc < 1) ERR(site, "String#* requires an argument");
            int64_t n = args[0].ival;
            if (n <= 0) return val_string(ev->arena, "");
            size_t slen = strlen(s);
            char *buf = arena_alloc(ev->arena, slen * (size_t)n + 1);
            buf[0] = '\0';
            for (int64_t i = 0; i < n; i++) strcat(buf, s);
            return val_string(ev->arena, buf);
        }
    }

    /* ---- Array ---- */
    if (recv.kind == VAL_ARRAY) {
        if (strcmp(name, "length") == 0 ||
            strcmp(name, "size")   == 0 ||
            strcmp(name, "count")  == 0) return val_int((int64_t)recv.array.len);
        if (strcmp(name, "empty?") == 0) return val_bool(recv.array.len == 0);
        if (strcmp(name, "first")  == 0) {
            if (recv.array.len == 0) return val_nil();
            return recv.array.elems[0];
        }
        if (strcmp(name, "last")   == 0) {
            if (recv.array.len == 0) return val_nil();
            return recv.array.elems[recv.array.len - 1];
        }
        if (strcmp(name, "push") == 0 || strcmp(name, "append") == 0) {
            for (int i = 0; i < argc; i++) val_array_push(&recv, args[i]);
            /* mutate in env — caller must update */
            return recv;
        }
        if (strcmp(name, "pop") == 0) {
            if (recv.array.len == 0) return val_nil();
            return recv.array.elems[--recv.array.len];
        }
        if (strcmp(name, "shift") == 0) {
            if (recv.array.len == 0) return val_nil();
            Value first = recv.array.elems[0];
            memmove(recv.array.elems, recv.array.elems + 1,
                    (recv.array.len - 1) * sizeof(Value));
            recv.array.len--;
            return first;
        }
        if (strcmp(name, "unshift") == 0 || strcmp(name, "prepend") == 0) {
            for (int i = argc - 1; i >= 0; i--) {
                /* grow if needed */
                if (recv.array.len >= recv.array.cap) {
                    size_t nc = recv.array.cap == 0 ? 8 : recv.array.cap * 2;
                    recv.array.elems = realloc(recv.array.elems, nc * sizeof(Value));
                    recv.array.cap = nc;
                }
                memmove(recv.array.elems + 1, recv.array.elems,
                        recv.array.len * sizeof(Value));
                recv.array.elems[0] = args[i];
                recv.array.len++;
            }
            return recv;
        }
        if (strcmp(name, "reverse") == 0) {
            Value arr = val_array_new();
            for (size_t i = recv.array.len; i > 0; i--)
                val_array_push(&arr, recv.array.elems[i - 1]);
            return arr;
        }
        if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0)
            return val_string(ev->arena, val_to_s(ev->arena, recv));
        if (strcmp(name, "join") == 0) {
            const char *sep = argc > 0 ? val_to_s(ev->arena, args[0]) : "";
            size_t total = 1;
            for (size_t i = 0; i < recv.array.len; i++)
                total += strlen(val_to_s(ev->arena, recv.array.elems[i])) + strlen(sep);
            char *buf = arena_alloc(ev->arena, total);
            buf[0] = '\0';
            for (size_t i = 0; i < recv.array.len; i++) {
                if (i) strcat(buf, sep);
                strcat(buf, val_to_s(ev->arena, recv.array.elems[i]));
            }
            return val_string(ev->arena, buf);
        }
        if (strcmp(name, "include?") == 0) {
            if (argc < 1) ERR(site, "Array#include? requires an argument");
            for (size_t i = 0; i < recv.array.len; i++)
                if (val_equal(recv.array.elems[i], args[0])) return val_true();
            return val_false();
        }
        if (strcmp(name, "each") == 0) {
            if (!blk) ERR(site, "Array#each requires a block");
            for (size_t i = 0; i < recv.array.len; i++) {
                Value arg = recv.array.elems[i];
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
        if (strcmp(name, "each_with_index") == 0) {
            if (!blk) ERR(site, "Array#each_with_index requires a block");
            for (size_t i = 0; i < recv.array.len; i++) {
                Value bargs[2] = { recv.array.elems[i], val_int((int64_t)i) };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
        if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
            if (!blk) ERR(site, "Array#map requires a block");
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array.len; i++) {
                Value arg = recv.array.elems[i];
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
                val_array_push(&result, r);
            }
            return result;
        }
        if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0) {
            if (!blk) ERR(site, "Array#select requires a block");
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array.len; i++) {
                Value arg = recv.array.elems[i];
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
                if (val_truthy(r)) val_array_push(&result, arg);
            }
            return result;
        }
        if (strcmp(name, "reject") == 0) {
            if (!blk) ERR(site, "Array#reject requires a block");
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array.len; i++) {
                Value arg = recv.array.elems[i];
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
                if (!val_truthy(r)) val_array_push(&result, arg);
            }
            return result;
        }
        if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
            if (!blk) ERR(site, "Array#reduce requires a block");
            if (recv.array.len == 0) return argc > 0 ? args[0] : val_nil();
            size_t start = 0;
            Value acc = argc > 0 ? args[0] : recv.array.elems[start++];
            for (size_t i = start; i < recv.array.len; i++) {
                Value bargs[2] = { acc, recv.array.elems[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
                acc = r;
            }
            return acc;
        }
        if (strcmp(name, "any?") == 0) {
            if (!blk) ERR(site, "Array#any? requires a block");
            for (size_t i = 0; i < recv.array.len; i++) {
                Value arg = recv.array.elems[i];
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (val_truthy(r)) return val_true();
            }
            return val_false();
        }
        if (strcmp(name, "all?") == 0) {
            if (!blk) ERR(site, "Array#all? requires a block");
            for (size_t i = 0; i < recv.array.len; i++) {
                Value arg = recv.array.elems[i];
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (!val_truthy(r)) return val_false();
            }
            return val_true();
        }
        if (strcmp(name, "none?") == 0) {
            if (!blk) ERR(site, "Array#none? requires a block");
            for (size_t i = 0; i < recv.array.len; i++) {
                Value arg = recv.array.elems[i];
                Value r   = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) return val_nil();
                if (val_truthy(r)) return val_false();
            }
            return val_true();
        }
        if (strcmp(name, "min") == 0) {
            if (recv.array.len == 0) return val_nil();
            Value m = recv.array.elems[0];
            for (size_t i = 1; i < recv.array.len; i++) {
                Value cur = recv.array.elems[i];
                if (cur.kind == VAL_INT && m.kind == VAL_INT && cur.ival < m.ival) m = cur;
                else if (cur.kind == VAL_FLOAT && m.kind == VAL_FLOAT && cur.fval < m.fval) m = cur;
            }
            return m;
        }
        if (strcmp(name, "max") == 0) {
            if (recv.array.len == 0) return val_nil();
            Value m = recv.array.elems[0];
            for (size_t i = 1; i < recv.array.len; i++) {
                Value cur = recv.array.elems[i];
                if (cur.kind == VAL_INT && m.kind == VAL_INT && cur.ival > m.ival) m = cur;
                else if (cur.kind == VAL_FLOAT && m.kind == VAL_FLOAT && cur.fval > m.fval) m = cur;
            }
            return m;
        }
        if (strcmp(name, "sum") == 0) {
            Value acc = argc > 0 ? args[0] : val_int(0);
            for (size_t i = 0; i < recv.array.len; i++) {
                Value cur = recv.array.elems[i];
                if (acc.kind == VAL_INT && cur.kind == VAL_INT)
                    acc.ival += cur.ival;
                else {
                    double a = acc.kind == VAL_FLOAT ? acc.fval : (double)acc.ival;
                    double c = cur.kind == VAL_FLOAT ? cur.fval : (double)cur.ival;
                    acc = val_float(a + c);
                }
            }
            return acc;
        }
        if (strcmp(name, "flatten") == 0) {
            /* one-level flatten */
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array.len; i++) {
                Value elem = recv.array.elems[i];
                if (elem.kind == VAL_ARRAY) {
                    for (size_t j = 0; j < elem.array.len; j++)
                        val_array_push(&result, elem.array.elems[j]);
                } else {
                    val_array_push(&result, elem);
                }
            }
            return result;
        }
        if (strcmp(name, "uniq") == 0) {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array.len; i++) {
                int found = 0;
                for (size_t j = 0; j < result.array.len; j++)
                    if (val_equal(result.array.elems[j], recv.array.elems[i])) { found = 1; break; }
                if (!found) val_array_push(&result, recv.array.elems[i]);
            }
            return result;
        }
        if (strcmp(name, "sort") == 0) {
            /* simple insertion sort on a copy */
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array.len; i++)
                val_array_push(&result, recv.array.elems[i]);
            for (size_t i = 1; i < result.array.len; i++) {
                Value key = result.array.elems[i];
                size_t j = i;
                while (j > 0) {
                    Value prev = result.array.elems[j-1];
                    int less = 0;
                    if (key.kind == VAL_INT && prev.kind == VAL_INT) less = key.ival < prev.ival;
                    else if (key.kind == VAL_FLOAT && prev.kind == VAL_FLOAT) less = key.fval < prev.fval;
                    else if (key.kind == VAL_STRING && prev.kind == VAL_STRING)
                        less = strcmp(key.sval, prev.sval) < 0;
                    if (!less) break;
                    result.array.elems[j] = prev;
                    j--;
                }
                result.array.elems[j] = key;
            }
            return result;
        }
        if (strcmp(name, "compact") == 0) {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array.len; i++)
                if (recv.array.elems[i].kind != VAL_NIL)
                    val_array_push(&result, recv.array.elems[i]);
            return result;
        }
        if (strcmp(name, "zip") == 0) {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array.len; i++) {
                Value pair = val_array_new();
                val_array_push(&pair, recv.array.elems[i]);
                for (int j = 0; j < argc; j++) {
                    if (args[j].kind == VAL_ARRAY && i < args[j].array.len)
                        val_array_push(&pair, args[j].array.elems[i]);
                    else
                        val_array_push(&pair, val_nil());
                }
                val_array_push(&result, pair);
            }
            return result;
        }
    }

    /* ---- Hash ---- */
    if (recv.kind == VAL_HASH) {
        RubyHash *h = recv.hash;
        if (strcmp(name, "[]") == 0) {
            if (argc < 1) ERR(site, "Hash#[] requires a key");
            Value out;
            return val_hash_get(h, args[0], &out) ? out : val_nil();
        }
        if (strcmp(name, "[]=") == 0) {
            if (argc < 2) ERR(site, "Hash#[]= requires key and value");
            val_hash_set(h, args[0], args[1]);
            return args[1];
        }
        if (strcmp(name, "fetch") == 0) {
            if (argc < 1) ERR(site, "Hash#fetch requires a key");
            Value out;
            if (val_hash_get(h, args[0], &out)) return out;
            if (argc > 1) return args[1];
            if (blk) return call_block(ev, *blk, &args[0], 1, site);
            ERR(site, "Hash#fetch: key not found");
        }
        if (strcmp(name, "has_key?") == 0 || strcmp(name, "key?")     == 0 ||
            strcmp(name, "include?") == 0 || strcmp(name, "member?")   == 0) {
            if (argc < 1) ERR(site, "Hash#has_key? requires a key");
            Value out;
            return val_bool(val_hash_get(h, args[0], &out));
        }
        if (strcmp(name, "has_value?") == 0 || strcmp(name, "value?") == 0) {
            if (argc < 1) ERR(site, "Hash#has_value? requires a value");
            for (size_t i = 0; i < h->len; i++)
                if (val_equal(h->vals[i], args[0])) return val_true();
            return val_false();
        }
        if (strcmp(name, "delete") == 0) {
            if (argc < 1) ERR(site, "Hash#delete requires a key");
            Value out;
            int found = val_hash_get(h, args[0], &out);
            val_hash_delete(h, args[0]);
            if (found) return out;
            if (blk) return call_block(ev, *blk, &args[0], 1, site);
            return val_nil();
        }
        if (strcmp(name, "keys")   == 0) {
            Value arr = val_array_new();
            for (size_t i = 0; i < h->len; i++) val_array_push(&arr, h->keys[i]);
            return arr;
        }
        if (strcmp(name, "values") == 0) {
            Value arr = val_array_new();
            for (size_t i = 0; i < h->len; i++) val_array_push(&arr, h->vals[i]);
            return arr;
        }
        if (strcmp(name, "length") == 0 || strcmp(name, "size")  == 0 ||
            strcmp(name, "count")  == 0)
            return val_int((int64_t)h->len);
        if (strcmp(name, "empty?") == 0)
            return val_bool(h->len == 0);
        if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0)
            return val_string(ev->arena, val_to_s(ev->arena, recv));
        if (strcmp(name, "to_a") == 0) {
            Value arr = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value pair = val_array_new();
                val_array_push(&pair, h->keys[i]);
                val_array_push(&pair, h->vals[i]);
                val_array_push(&arr, pair);
            }
            return arr;
        }
        if (strcmp(name, "merge") == 0) {
            if (argc < 1 || args[0].kind != VAL_HASH) ERR(site, "Hash#merge requires a Hash");
            Value result = val_hash_new(ev->arena);
            for (size_t i = 0; i < h->len; i++)
                val_hash_set(result.hash, h->keys[i], h->vals[i]);
            RubyHash *other = args[0].hash;
            for (size_t i = 0; i < other->len; i++)
                val_hash_set(result.hash, other->keys[i], other->vals[i]);
            return result;
        }
        if (strcmp(name, "merge!") == 0 || strcmp(name, "update") == 0) {
            if (argc < 1 || args[0].kind != VAL_HASH) ERR(site, "Hash#merge! requires a Hash");
            RubyHash *other = args[0].hash;
            for (size_t i = 0; i < other->len; i++)
                val_hash_set(h, other->keys[i], other->vals[i]);
            return recv;
        }
        if (strcmp(name, "each") == 0 || strcmp(name, "each_pair") == 0) {
            if (!blk) ERR(site, "Hash#each requires a block");
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
        if (strcmp(name, "each_key") == 0) {
            if (!blk) ERR(site, "Hash#each_key requires a block");
            for (size_t i = 0; i < h->len; i++) {
                Value r = call_block(ev, *blk, &h->keys[i], 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
        if (strcmp(name, "each_value") == 0) {
            if (!blk) ERR(site, "Hash#each_value requires a block");
            for (size_t i = 0; i < h->len; i++) {
                Value r = call_block(ev, *blk, &h->vals[i], 1, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
            }
            return recv;
        }
        if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
            if (!blk) ERR(site, "Hash#map requires a block");
            Value result = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
                val_array_push(&result, r);
            }
            return result;
        }
        if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0) {
            if (!blk) ERR(site, "Hash#select requires a block");
            Value result = val_hash_new(ev->arena);
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
                if (val_truthy(r)) val_hash_set(result.hash, h->keys[i], h->vals[i]);
            }
            return result;
        }
        if (strcmp(name, "reject") == 0) {
            if (!blk) ERR(site, "Hash#reject requires a block");
            Value result = val_hash_new(ev->arena);
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
                if (!val_truthy(r)) val_hash_set(result.hash, h->keys[i], h->vals[i]);
            }
            return result;
        }
        if (strcmp(name, "any?") == 0) {
            if (!blk) ERR(site, "Hash#any? requires a block");
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (val_truthy(r)) return val_true();
            }
            return val_false();
        }
        if (strcmp(name, "all?") == 0) {
            if (!blk) ERR(site, "Hash#all? requires a block");
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (!val_truthy(r)) return val_false();
            }
            return val_true();
        }
        if (strcmp(name, "min_by") == 0 || strcmp(name, "max_by") == 0 ||
            strcmp(name, "sort_by") == 0) {
            /* delegate to array form */
            Value as_arr = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value pair = val_array_new();
                val_array_push(&pair, h->keys[i]);
                val_array_push(&pair, h->vals[i]);
                val_array_push(&as_arr, pair);
            }
            return dispatch_method(ev, env, as_arr, name, args, argc, blk, site);
        }
        if (strcmp(name, "flat_map") == 0) {
            if (!blk) ERR(site, "Hash#flat_map requires a block");
            Value result = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_ARRAY)
                    for (size_t j = 0; j < r.array.len; j++)
                        val_array_push(&result, r.array.elems[j]);
                else
                    val_array_push(&result, r);
            }
            return result;
        }
        if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
            if (!blk) ERR(site, "Hash#reduce requires a block");
            if (h->len == 0) return argc > 0 ? args[0] : val_nil();
            size_t start = 0;
            Value acc;
            if (argc > 0) { acc = args[0]; }
            else {
                Value pair = val_array_new();
                val_array_push(&pair, h->keys[0]);
                val_array_push(&pair, h->vals[0]);
                acc = pair; start = 1;
            }
            for (size_t i = start; i < h->len; i++) {
                Value pair = val_array_new();
                val_array_push(&pair, h->keys[i]);
                val_array_push(&pair, h->vals[i]);
                Value bargs[2] = { acc, pair };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) return val_nil();
                if (r.kind == VAL_BREAK)  return *r.wrapped;
                if (r.kind == VAL_RETURN) return r;
                acc = r;
            }
            return acc;
        }
        if (strcmp(name, "store") == 0) {
            if (argc < 2) ERR(site, "Hash#store requires key and value");
            val_hash_set(h, args[0], args[1]);
            return args[1];
        }
        if (strcmp(name, "clear") == 0) { h->len = 0; return recv; }
        if (strcmp(name, "dup")   == 0) {
            Value result = val_hash_new(ev->arena);
            for (size_t i = 0; i < h->len; i++)
                val_hash_set(result.hash, h->keys[i], h->vals[i]);
            return result;
        }
        if (strcmp(name, "nil?")  == 0) return val_false();
        ERR(site, "undefined method '%s' for Hash", name);
    }

    /* ---- nil ---- */
    if (recv.kind == VAL_NIL) {
        if (strcmp(name, "nil?") == 0 || strcmp(name, "to_s") == 0) return val_nil();
        if (strcmp(name, "inspect") == 0) return val_string(ev->arena, "nil");
        ERR(site, "undefined method '%s' for nil", name);
    }

    /* ---- bool ---- */
    if (recv.kind == VAL_BOOL) {
        if (strcmp(name, "to_s")    == 0) return val_string(ev->arena, recv.bval ? "true" : "false");
        if (strcmp(name, "inspect") == 0) return val_string(ev->arena, recv.bval ? "true" : "false");
        if (strcmp(name, "!")       == 0) return val_bool(!recv.bval);
        if (strcmp(name, "nil?")    == 0) return val_false();
    }

    /* ---- Class ---- */
    if (recv.kind == VAL_CLASS) {
        if (strcmp(name, "new") == 0) {
            Value obj = val_object(ev->arena, recv);
            /* Walk hierarchy to find initialize */
            RubyClass *klass = recv.klass;
            while (klass) {
                Value init_method;
                if (env_get(klass->class_env, "initialize", &init_method)
                        && init_method.kind == VAL_METHOD) {
                    Env *method_env = env_new(ev->arena, init_method.method.closure, 1);
                    env_set(ev->arena, method_env, "self", obj);
                    env_set(ev->arena, method_env, "__method__", val_symbol("initialize"));
                    Value klass_val; klass_val.kind = VAL_CLASS; klass_val.klass = klass;
                    env_set(ev->arena, method_env, "__class__", klass_val);
                    if (blk) method_env->block_arg = blk;
                    NodeList *params = init_method.method.def_node->def.params;
                    for (int i = 0; i < argc && params; i++, params = params->next)
                        env_set(ev->arena, method_env, params->node->param.name, args[i]);
                    ev->call_depth++;
                    Value result = eval_node(ev, method_env, init_method.method.def_node->def.body);
                    ev->call_depth--;
                    if (ev->errored) return val_nil();
                    (void)result;
                    break;
                }
                klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
            }
            return obj;
        }
    }

    /* ---- Object ---- */
    if (recv.kind == VAL_OBJECT) {
        RubyClass *klass = recv.obj->klass.klass;
        while (klass) {
            Value method;
            if (env_get(klass->class_env, name, &method) && method.kind == VAL_METHOD) {
                Env *method_env = env_new(ev->arena, method.method.closure, 1);
                env_set(ev->arena, method_env, "self", recv);
                env_set(ev->arena, method_env, "__method__", val_symbol(name));
                Value klass_val; klass_val.kind = VAL_CLASS; klass_val.klass = klass;
                env_set(ev->arena, method_env, "__class__", klass_val);
                if (blk) method_env->block_arg = blk;
                NodeList *params = method.method.def_node->def.params;
                for (int i = 0; i < argc && params; i++, params = params->next)
                    env_set(ev->arena, method_env, params->node->param.name, args[i]);
                ev->call_depth++;
                Value result = eval_node(ev, method_env, method.method.def_node->def.body);
                ev->call_depth--;
                if (ev->errored) return val_nil();
                if (result.kind == VAL_RETURN) result = *result.wrapped;
                return result;
            }
            klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
        }
    }

    /* Fallthrough: undefined */
    ERR(site, "undefined method '%s' for %s", name, val_kind_name(recv.kind));
}

/* ------------------------------------------------------------------ */
/* Binary operator evaluation                                           */
/* ------------------------------------------------------------------ */
static Value eval_binop(Eval *ev, Env *env, Node *node) {
    const char *op = node->binop.op;

    /* Short-circuit operators */
    if (strcmp(op, "&&") == 0) {
        Value left = eval_node(ev, env, node->binop.left);
        CHECK(left);
        if (!val_truthy(left)) return left;
        return eval_node(ev, env, node->binop.right);
    }
    if (strcmp(op, "||") == 0) {
        Value left = eval_node(ev, env, node->binop.left);
        CHECK(left);
        if (val_truthy(left)) return left;
        return eval_node(ev, env, node->binop.right);
    }

    Value left  = eval_node(ev, env, node->binop.left);
    CHECK(left);
    Value right = eval_node(ev, env, node->binop.right);
    CHECK(right);

    /* String concatenation */
    if (strcmp(op, "+") == 0 && left.kind == VAL_STRING) {
        if (right.kind != VAL_STRING)
            ERR(node, "String can only be concatenated with String");
        size_t la = strlen(left.sval), lb = strlen(right.sval);
        char *buf = arena_alloc(ev->arena, la + lb + 1);
        memcpy(buf, left.sval, la);
        memcpy(buf + la, right.sval, lb + 1);
        return val_string(ev->arena, buf);
    }

    /* Array concatenation / append */
    if (strcmp(op, "+") == 0 && left.kind == VAL_ARRAY && right.kind == VAL_ARRAY) {
        Value result = val_array_new();
        for (size_t i = 0; i < left.array.len; i++)  val_array_push(&result, left.array.elems[i]);
        for (size_t i = 0; i < right.array.len; i++) val_array_push(&result, right.array.elems[i]);
        return result;
    }
    if (strcmp(op, "<<") == 0 && left.kind == VAL_ARRAY) {
        val_array_push(&left, right);
        return left;
    }

    /* String comparison */
    if (strcmp(op, "==") == 0) return val_bool(val_equal(left, right));
    if (strcmp(op, "!=") == 0) return val_bool(!val_equal(left, right));

    /* Numeric operations — coerce int/float */
    double lf = 0, rf = 0;
    int    both_int = (left.kind == VAL_INT && right.kind == VAL_INT);
    if (left.kind == VAL_INT)   lf = (double)left.ival;
    else if (left.kind == VAL_FLOAT) lf = left.fval;
    if (right.kind == VAL_INT)  rf = (double)right.ival;
    else if (right.kind == VAL_FLOAT) rf = right.fval;

    if (left.kind == VAL_INT || left.kind == VAL_FLOAT) {
        if (strcmp(op, "+")   == 0) return both_int ? val_int(left.ival + right.ival)  : val_float(lf + rf);
        if (strcmp(op, "-")   == 0) return both_int ? val_int(left.ival - right.ival)  : val_float(lf - rf);
        if (strcmp(op, "*")   == 0) {
            if (left.kind == VAL_INT && right.kind == VAL_STRING)
                return dispatch_method(ev, env, right, "*", &left, 1, NULL, node);
            return both_int ? val_int(left.ival * right.ival) : val_float(lf * rf);
        }
        if (strcmp(op, "/")   == 0) {
            if (both_int) {
                if (right.ival == 0) ERR(node, "divided by 0");
                return val_int(left.ival / right.ival);
            }
            if (rf == 0.0) ERR(node, "divided by 0");
            return val_float(lf / rf);
        }
        if (strcmp(op, "%")   == 0) {
            if (both_int) {
                if (right.ival == 0) ERR(node, "divided by 0");
                return val_int(left.ival % right.ival);
            }
            return val_float(fmod(lf, rf));
        }
        if (strcmp(op, "**")  == 0) return both_int && right.ival >= 0
            ? val_int((int64_t)pow((double)left.ival, (double)right.ival))
            : val_float(pow(lf, rf));
        if (strcmp(op, "<")   == 0) return val_bool(lf <  rf);
        if (strcmp(op, "<=")  == 0) return val_bool(lf <= rf);
        if (strcmp(op, ">")   == 0) return val_bool(lf >  rf);
        if (strcmp(op, ">=")  == 0) return val_bool(lf >= rf);
        if (strcmp(op, "<=>") == 0) return val_int(lf < rf ? -1 : lf > rf ? 1 : 0);
        /* Bitwise (int only) */
        if (both_int) {
            if (strcmp(op, "&")  == 0) return val_int(left.ival & right.ival);
            if (strcmp(op, "|")  == 0) return val_int(left.ival | right.ival);
            if (strcmp(op, "^")  == 0) return val_int(left.ival ^ right.ival);
            if (strcmp(op, "<<") == 0) return val_int(left.ival << right.ival);
            if (strcmp(op, ">>") == 0) return val_int(left.ival >> right.ival);
        }
    }

    /* String comparison */
    if (left.kind == VAL_STRING && right.kind == VAL_STRING) {
        if (strcmp(op, "<=>") == 0) {
            int r = strcmp(left.sval, right.sval);
            return val_int(r < 0 ? -1 : r > 0 ? 1 : 0);
        }
        if (strcmp(op, "<")  == 0) return val_bool(strcmp(left.sval, right.sval) <  0);
        if (strcmp(op, "<=") == 0) return val_bool(strcmp(left.sval, right.sval) <= 0);
        if (strcmp(op, ">")  == 0) return val_bool(strcmp(left.sval, right.sval) >  0);
        if (strcmp(op, ">=") == 0) return val_bool(strcmp(left.sval, right.sval) >= 0);
        if (strcmp(op, "*")  == 0) return dispatch_method(ev, env, left, "*", &right, 1, NULL, node);
    }

    ERR(node, "undefined operator '%s' for %s", op, val_kind_name(left.kind));
}

/* ------------------------------------------------------------------ */
/* Method call evaluation                                               */
/* ------------------------------------------------------------------ */
static Value eval_call(Eval *ev, Env *env, Node *node) {
    if (ev->call_depth > EVAL_MAX_DEPTH) ERR(node, "stack level too deep");

    /* Evaluate block if present */
    Value blk_val;
    Value *blk = NULL;
    if (node->call.block) {
        blk_val = val_block(node->call.block, env);
        blk = &blk_val;
    }

    /* Evaluate arguments */
    Value args[64];
    int   argc = 0;
    for (NodeList *l = node->call.args; l && argc < 64; l = l->next) {
        Value a = eval_node(ev, env, l->node);
        CHECK(a);
        args[argc++] = a;
    }

    /* No receiver — bare call */
    if (!node->call.recv) {
        const char *name = node->call.method;

        /* yield */
        if (strcmp(name, "yield") == 0) {
            Value *block_arg = NULL;
            for (Env *sc = env; sc; sc = sc->parent) {
                if (sc->block_arg) { block_arg = sc->block_arg; break; }
                if (sc->is_def) break;
            }
            if (!block_arg) ERR(node, "no block given (yield)");
            return call_block(ev, *block_arg, args, argc, node);
        }

        /* Check kernel methods first (highest priority) */
        static const char *kernel_names[] = {
            "puts", "print", "p", "raise", "rand", "exit", NULL
        };
        for (int i = 0; kernel_names[i]; i++) {
            if (strcmp(name, kernel_names[i]) == 0) {
                return builtin_kernel(ev, env, name, args, argc, blk, node);
            }
        }

        /* Look up in env (may be a user-defined method stored as a local) */
        Value fn;
        if (env_get(env, name, &fn) && fn.kind == VAL_METHOD) {
            goto call_method;
        }

        /* Check if we're in an instance method context (self is bound) */
        Value self;
        if (env_get(env, "self", &self) && self.kind == VAL_OBJECT) {
            RubyClass *klass = self.obj->klass.klass;
            while (klass) {
                if (env_get(klass->class_env, name, &fn) && fn.kind == VAL_METHOD) {
                    Env *method_env = env_new(ev->arena, fn.method.closure, 1);
                    env_set(ev->arena, method_env, "self", self);
                    env_set(ev->arena, method_env, "__method__", val_symbol(name));
                    Value klass_val; klass_val.kind = VAL_CLASS; klass_val.klass = klass;
                    env_set(ev->arena, method_env, "__class__", klass_val);
                    if (blk) method_env->block_arg = blk;
                    NodeList *params = fn.method.def_node->def.params;
                    for (int i = 0; i < argc && params; i++, params = params->next)
                        env_set(ev->arena, method_env, params->node->param.name, args[i]);
                    ev->call_depth++;
                    Value result = eval_node(ev, method_env, fn.method.def_node->def.body);
                    ev->call_depth--;
                    if (ev->errored) return val_nil();
                    if (result.kind == VAL_RETURN) result = *result.wrapped;
                    return result;
                }
                klass = klass->superclass.kind == VAL_CLASS ? klass->superclass.klass : NULL;
            }
        }

        /* Try looking up as a stored method */
        if (!env_get(env, name, &fn))
            ERR(node, "undefined method '%s'", name);
        if (fn.kind != VAL_METHOD)
            ERR(node, "'%s' is not a method", name);

        call_method: {
            /* Re-fetch in case we jumped here from the env lookup above */
            if (!env_get(env, name, &fn)) ERR(node, "undefined method '%s'", name);
            Node *def      = fn.method.def_node;
            Env  *closure  = fn.method.closure;
            Env  *frame    = env_new(ev->arena, closure, 1);
            if (blk) frame->block_arg = blk;

            /* bind params */
            NodeList *pl = def->def.params;
            int pi = 0;
            for (; pl; pl = pl->next, pi++) {
                Node *p = pl->node;
                if (!p || p->kind != NODE_PARAM) continue;
                if (p->param.splat) {
                    /* collect remaining args into array */
                    Value rest = val_array_new();
                    for (int j = pi; j < argc; j++) val_array_push(&rest, args[j]);
                    if (p->param.name) env_set(ev->arena, frame, p->param.name, rest);
                    pl = NULL; /* consumed all */
                    break;
                }
                Value pval = pi < argc ? args[pi]
                           : (p->param.default_val
                              ? eval_node(ev, frame, p->param.default_val)
                              : val_nil());
                if (p->param.name) env_set(ev->arena, frame, p->param.name, pval);
            }

            ev->call_depth++;
            Value result = eval_node(ev, frame, def->def.body);
            ev->call_depth--;

            if (result.kind == VAL_RETURN) return *result.wrapped;
            return result;
        }
    }

    /* Has receiver */
    Value recv = eval_node(ev, env, node->call.recv);
    CHECK(recv);

    /* Subscript read/write handled inline for arrays */
    if (recv.kind == VAL_ARRAY) {
        if (strcmp(node->call.method, "[]") == 0) {
            if (argc < 1) ERR(node, "wrong number of args for []");
            int64_t idx = args[0].ival;
            if (idx < 0) idx = (int64_t)recv.array.len + idx;
            if (idx < 0 || (size_t)idx >= recv.array.len) return val_nil();
            return recv.array.elems[idx];
        }
        if (strcmp(node->call.method, "[]=") == 0) {
            if (argc < 2) ERR(node, "wrong number of args for []=");
            int64_t idx = args[0].ival;
            if (idx < 0) idx = (int64_t)recv.array.len + idx;
            if (idx >= 0 && (size_t)idx < recv.array.len)
                recv.array.elems[idx] = args[1];
            return args[1];
        }
    }

    return dispatch_method(ev, env, recv, node->call.method, args, argc, blk, node);
}

/* ------------------------------------------------------------------ */
/* Assignment helpers                                                   */
/* ------------------------------------------------------------------ */
static void assign_lvar(Eval *ev, Env *env, const char *name, Value val) {
    /* Update existing binding anywhere in scope chain first */
    if (!env_update(env, name, val))
        env_set(ev->arena, env, name, val);
}

/* ------------------------------------------------------------------ */
/* Main evaluation                                                      */
/* ------------------------------------------------------------------ */
Value eval_node(Eval *ev, Env *env, Node *node) {
    if (!node || ev->errored) return val_nil();

    switch (node->kind) {

        /* ---- Literals ---- */
        case NODE_NIL:    return val_nil();
        case NODE_TRUE:   return val_true();
        case NODE_FALSE:  return val_false();
        case NODE_SELF: {
            Value v;
            if (env_get(env, "self", &v)) return v;
            return val_nil();
        }
        case NODE_INT:    return val_int(node->ival);
        case NODE_FLOAT:  return val_float(node->fval);
        case NODE_STRING: return val_string(ev->arena, node->sval);
        case NODE_SYMBOL: return val_symbol(node->sval);

        case NODE_ROPE: {
            const char *s = eval_rope(ev, env, node->interp.rope);
            if (ev->errored) return val_nil();
            return val_string(ev->arena, s);
        }

        /* ---- Variables ---- */
        case NODE_LVAR: {
            Value v;
            if (!env_get(env, node->sval, &v))
                ERR(node, "undefined local variable '%s'", node->sval);
            return v;
        }
        case NODE_IVAR: {
            Value self;
            if (env_get(env, "self", &self) && self.kind == VAL_OBJECT) {
                Value v;
                if (!val_object_get_ivar(self, node->sval, &v)) return val_nil();
                return v;
            }
            Value v;
            if (!global_get(&ev->globals, node->sval, &v)) return val_nil();
            return v;
        }
        case NODE_GVAR: {
            Value v;
            if (!global_get(&ev->globals, node->sval, &v)) return val_nil();
            return v;
        }
        case NODE_CONST: {
            Value v;
            /* Look up constants in top-level env */
            if (!env_get(ev->top_env, node->sval, &v))
                ERR(node, "uninitialized constant '%s'", node->sval);
            return v;
        }

        /* ---- Assignment ---- */
        case NODE_ASSIGN: {
            Value val = eval_node(ev, env, node->assign.value);
            CHECK(val);
            Node *target = node->assign.target;
            if (target->kind == NODE_LVAR)
                assign_lvar(ev, env, target->sval, val);
            else if (target->kind == NODE_IVAR) {
                Value self;
                if (env_get(env, "self", &self) && self.kind == VAL_OBJECT)
                    val_object_set_ivar(ev->arena, self, target->sval, val);
                else
                    global_set(ev->arena, &ev->globals, target->sval, val);
            } else if (target->kind == NODE_GVAR)
                global_set(ev->arena, &ev->globals, target->sval, val);
            else if (target->kind == NODE_CONST)
                env_set(ev->arena, ev->top_env, target->sval, val);
            return val;
        }

        case NODE_OP_ASSIGN: {
            /* lhs op= rhs  →  lhs = lhs op rhs */
            const char *raw_op = node->binop.op; /* e.g. "+=" */
            /* strip the trailing '=' to get the operator */
            char op[8];
            size_t oplen = strlen(raw_op);
            if (oplen > 0 && raw_op[oplen-1] == '=') {
                memcpy(op, raw_op, oplen - 1);
                op[oplen - 1] = '\0';
            } else {
                strcpy(op, raw_op);
            }

            /* Build a fake binop node to reuse eval_binop */
            Node fake; memset(&fake, 0, sizeof(fake));
            fake.kind          = NODE_BINOP;
            fake.span          = node->span;
            fake.binop.op      = op;
            fake.binop.left    = node->binop.left;
            fake.binop.right   = node->binop.right;

            Value val = eval_binop(ev, env, &fake);
            CHECK(val);

            Node *target = node->binop.left;
            if (target->kind == NODE_LVAR)
                assign_lvar(ev, env, target->sval, val);
            else if (target->kind == NODE_IVAR) {
                Value self;
                if (env_get(env, "self", &self) && self.kind == VAL_OBJECT)
                    val_object_set_ivar(ev->arena, self, target->sval, val);
                else
                    global_set(ev->arena, &ev->globals, target->sval, val);
            }
            return val;
        }

        /* ---- Operators ---- */
        case NODE_BINOP:
            return eval_binop(ev, env, node);

        case NODE_UNOP: {
            Value operand = eval_node(ev, env, node->unop.operand);
            CHECK(operand);
            const char *op = node->unop.op;
            if (strcmp(op, "!")   == 0 || strcmp(op, "not") == 0)
                return val_bool(!val_truthy(operand));
            if (strcmp(op, "-")   == 0) {
                if (operand.kind == VAL_INT)   return val_int(-operand.ival);
                if (operand.kind == VAL_FLOAT) return val_float(-operand.fval);
            }
            if (strcmp(op, "+")   == 0) return operand;
            if (strcmp(op, "~")   == 0 && operand.kind == VAL_INT)
                return val_int(~operand.ival);
            ERR(node, "undefined unary operator '%s'", op);
        }

        /* ---- Method call ---- */
        case NODE_CALL:
            return eval_call(ev, env, node);

        /* ---- Control flow ---- */
        case NODE_IF:
        case NODE_UNLESS: {
            Value cond = eval_node(ev, env, node->cond.cond);
            CHECK(cond);
            int taken = (node->kind == NODE_IF) ? val_truthy(cond) : !val_truthy(cond);
            if (taken)
                return eval_node(ev, env, node->cond.then_body);
            else if (node->cond.else_body)
                return eval_node(ev, env, node->cond.else_body);
            return val_nil();
        }

        case NODE_WHILE:
        case NODE_UNTIL: {
            Value result = val_nil();
            while (1) {
                Value cond = eval_node(ev, env, node->loop.cond);
                CHECK(cond);
                int cont = (node->kind == NODE_WHILE) ? val_truthy(cond) : !val_truthy(cond);
                if (!cont) break;
                result = eval_node(ev, env, node->loop.body);
                if (ev->errored) return val_nil();
                if (result.kind == VAL_BREAK)  return *result.wrapped;
                if (result.kind == VAL_RETURN) return result;
                if (result.kind == VAL_NEXT)   { result = val_nil(); continue; }
            }
            return result;
        }

        case NODE_RETURN:
            if (node->jump.value) {
                Value v = eval_node(ev, env, node->jump.value);
                CHECK(v);
                return val_return(ev->arena, v);
            }
            return val_return(ev->arena, val_nil());

        case NODE_BREAK:
            if (node->jump.value) {
                Value v = eval_node(ev, env, node->jump.value);
                CHECK(v);
                return val_break(ev->arena, v);
            }
            return val_break(ev->arena, val_nil());

        case NODE_NEXT:
            if (node->jump.value) {
                Value v = eval_node(ev, env, node->jump.value);
                CHECK(v);
                return val_next(ev->arena, v);
            }
            return val_next(ev->arena, val_nil());

        /* ---- Super ---- */
        case NODE_SUPER: {
            Value self;
            if (!env_get(env, "self", &self) || self.kind != VAL_OBJECT)
                ERR(node, "super called outside of instance method");

            Value cur_class_val;
            if (!env_get(env, "__class__", &cur_class_val) || cur_class_val.kind != VAL_CLASS)
                ERR(node, "super called outside of instance method");

            Value method_name_val;
            if (!env_get(env, "__method__", &method_name_val))
                ERR(node, "super called outside of instance method");
            const char *method_name = method_name_val.sval;

            /* Walk up from the superclass of the class that defined the current method */
            RubyClass *search = cur_class_val.klass->superclass.kind == VAL_CLASS
                                ? cur_class_val.klass->superclass.klass : NULL;

            /* Evaluate explicit args (or forward: not supported at runtime, use empty) */
            Value super_args[64];
            int   super_argc = 0;
            if (!node->super_call.forward_args) {
                for (NodeList *l = node->super_call.args; l && super_argc < 64; l = l->next) {
                    Value a = eval_node(ev, env, l->node);
                    CHECK(a);
                    super_args[super_argc++] = a;
                }
            } else {
                /* Forward: collect the current method's parameter bindings */
                Value cur_class_check;
                /* Walk the current env frame to find parameters (they're in the immediate frame) */
                /* The method frame is the current env (is_def=1, closest def boundary) */
                Env *frame = env;
                /* Gather positional params from def_node bound in the frame */
                Value method_val;
                if (env_get(cur_class_val.klass->class_env, method_name, &method_val)
                        && method_val.kind == VAL_METHOD) {
                    NodeList *params = method_val.method.def_node->def.params;
                    for (; params && super_argc < 64; params = params->next) {
                        Node *p = params->node;
                        if (p->kind != NODE_PARAM || !p->param.name || p->param.block_param) continue;
                        Value pval;
                        if (env_get(frame, p->param.name, &pval))
                            super_args[super_argc++] = pval;
                    }
                }
                (void)cur_class_check;
            }

            /* Dispatch to superclass method */
            while (search) {
                Value method;
                if (env_get(search->class_env, method_name, &method) && method.kind == VAL_METHOD) {
                    Env *method_env = env_new(ev->arena, method.method.closure, 1);
                    env_set(ev->arena, method_env, "self", self);
                    Value sc_val; sc_val.kind = VAL_CLASS; sc_val.klass = search;
                    env_set(ev->arena, method_env, "__method__", val_symbol(method_name));
                    env_set(ev->arena, method_env, "__class__", sc_val);
                    /* bind block if present */
                    Value *blk = NULL;
                    for (Env *sc = env; sc; sc = sc->parent) {
                        if (sc->block_arg) { blk = sc->block_arg; break; }
                        if (sc->is_def) break;
                    }
                    if (blk) method_env->block_arg = blk;
                    NodeList *params = method.method.def_node->def.params;
                    for (int i = 0; i < super_argc && params; i++, params = params->next)
                        env_set(ev->arena, method_env, params->node->param.name, super_args[i]);
                    ev->call_depth++;
                    Value result = eval_node(ev, method_env, method.method.def_node->def.body);
                    ev->call_depth--;
                    if (ev->errored) return val_nil();
                    if (result.kind == VAL_RETURN) result = *result.wrapped;
                    return result;
                }
                search = search->superclass.kind == VAL_CLASS ? search->superclass.klass : NULL;
            }
            ERR(node, "super: no superclass method '%s'", method_name);
        }

        /* ---- Definition ---- */
        case NODE_DEF:
            /* def always defines in the current frame (never updates a parent).
               Closes over top_env — Ruby's def doesn't capture enclosing method locals. */
            env_define(ev->arena, env, node->def.name, val_method(node, ev->top_env));
            return val_nil();

        /* ---- Class Definition ---- */
        case NODE_CLASS: {
            /* Support class reopening */
            Value existing;
            int reopen = env_get(ev->top_env, node->klass.name, &existing)
                         && existing.kind == VAL_CLASS;

            Value klass;
            if (reopen) {
                klass = existing;
            } else {
                Value superclass = val_nil();
                if (node->klass.superclass) {
                    superclass = eval_node(ev, env, node->klass.superclass);
                    CHECK(superclass);
                    if (superclass.kind != VAL_CLASS && superclass.kind != VAL_NIL)
                        ERR(node, "superclass must be a class");
                }
                klass = val_class(ev->arena, node->klass.name, superclass);
                /* class_env: parent=top_env so methods can see constants;
                   is_def=1 so local assignments in class body stay local */
                klass.klass->class_env = env_new(ev->arena, ev->top_env, 1);
            }

            if (node->klass.body)
                eval_node(ev, klass.klass->class_env, node->klass.body);
            if (ev->errored) return val_nil();

            if (!reopen)
                env_define(ev->arena, ev->top_env, node->klass.name, klass);
            return klass;
        }

        /* ---- Collections ---- */
        case NODE_ARRAY: {
            Value arr = val_array_new();
            for (NodeList *l = node->array.elements; l; l = l->next) {
                Value elem = eval_node(ev, env, l->node);
                CHECK(elem);
                val_array_push(&arr, elem);
            }
            return arr;
        }

        case NODE_HASH: {
            Value h = val_hash_new(ev->arena);
            for (NodeList *l = node->hash.pairs; l; l = l->next) {
                Node *pair = l->node;
                if (!pair || pair->kind != NODE_PAIR) continue;
                Value key = eval_node(ev, env, pair->pair.key);
                CHECK(key);
                Value val = eval_node(ev, env, pair->pair.value);
                CHECK(val);
                val_hash_set(h.hash, key, val);
            }
            return h;
        }

        /* ---- Body / Program ---- */
        case NODE_BODY:
        case NODE_PROGRAM: {
            Value result = val_nil();
            for (NodeList *l = node->body.stmts; l; l = l->next) {
                result = eval_node(ev, env, l->node);
                if (ev->errored || val_is_signal(result)) return result;
            }
            return result;
        }

        default:
            ERR(node, "cannot evaluate node kind %s", node_kind_name(node->kind));
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */
void eval_init(Eval *ev, Arena *arena, FILE *out) {
    memset(ev, 0, sizeof(*ev));
    ev->arena   = arena;
    ev->out     = out;
    ev->top_env = env_new(arena, NULL, 1);
}
