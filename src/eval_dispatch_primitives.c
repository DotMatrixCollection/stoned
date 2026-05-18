#include "eval_internal.h"
#include "utf8.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void append_utf8_pad(char *buf, size_t *pos, const char *pad, size_t count) {
    size_t pad_chars = utf8_char_count(pad);
    if (pad_chars == 0) return;
    for (size_t i = 0; i < count; i++) {
        const char *ptr = NULL;
        size_t width = 0;
        utf8_char_at(pad, i % pad_chars, &ptr, &width, NULL);
        memcpy(buf + *pos, ptr, width);
        *pos += width;
    }
}

static uint32_t utf8_simple_upcase(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    if (cp >= 0x00E0 && cp <= 0x00F6) return cp - 0x20;
    if (cp >= 0x00F8 && cp <= 0x00FE) return cp - 0x20;
    return cp;
}

static uint32_t utf8_simple_downcase(uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 32;
    if (cp >= 0x00C0 && cp <= 0x00D6) return cp + 0x20;
    if (cp >= 0x00D8 && cp <= 0x00DE) return cp + 0x20;
    return cp;
}

static int utf8_ascii_alnum(uint32_t cp) {
    return (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
}

static int utf8_space(uint32_t cp) {
    switch (cp) {
        case 0x0009: case 0x000A: case 0x000B: case 0x000C: case 0x000D:
        case 0x0020: case 0x0085: case 0x00A0: case 0x1680:
        case 0x2000: case 0x2001: case 0x2002: case 0x2003: case 0x2004:
        case 0x2005: case 0x2006: case 0x2007: case 0x2008: case 0x2009:
        case 0x200A: case 0x2028: case 0x2029: case 0x202F: case 0x205F:
        case 0x3000:
            return 1;
        default:
            return 0;
    }
}

typedef struct {
    uint32_t lo;
    uint32_t hi;
} RuneRange;

static size_t rune_pattern_ranges(const char *pat, RuneRange *ranges, size_t cap) {
    size_t count = 0;
    size_t i = 0;
    size_t len = strlen(pat);
    while (i < len && count < cap) {
        uint32_t first = 0, second = 0;
        size_t w1 = 0, w2 = 0;
        if (!utf8_decode_one(pat + i, len - i, &first, &w1)) break;
        if (i + w1 < len && pat[i + w1] == '-' && i + w1 + 1 < len &&
            utf8_decode_one(pat + i + w1 + 1, len - i - w1 - 1, &second, &w2)) {
            ranges[count].lo = first < second ? first : second;
            ranges[count].hi = first < second ? second : first;
            count++;
            i += w1 + 1 + w2;
        } else {
            ranges[count].lo = first;
            ranges[count].hi = first;
            count++;
            i += w1;
        }
    }
    return count;
}

static size_t rune_pattern_chars(const char *pat, uint32_t *out, size_t cap) {
    size_t n = 0;
    size_t i = 0;
    size_t len = strlen(pat);
    while (i < len && n < cap) {
        uint32_t first = 0, second = 0;
        size_t w1 = 0, w2 = 0;
        if (!utf8_decode_one(pat + i, len - i, &first, &w1)) break;
        if (i + w1 < len && pat[i + w1] == '-' && i + w1 + 1 < len &&
            utf8_decode_one(pat + i + w1 + 1, len - i - w1 - 1, &second, &w2)) {
            uint32_t lo = first < second ? first : second;
            uint32_t hi = first < second ? second : first;
            for (uint32_t cp = lo; cp <= hi && n < cap; cp++) out[n++] = cp;
            i += w1 + 1 + w2;
        } else {
            out[n++] = first;
            i += w1;
        }
    }
    return n;
}

static int rune_in_ranges(uint32_t cp, RuneRange *ranges, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (cp >= ranges[i].lo && cp <= ranges[i].hi) return 1;
    }
    return 0;
}

static size_t rune_index_in_chars(uint32_t cp, uint32_t *chars, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (chars[i] == cp) return i;
    }
    return (size_t)-1;
}

/* Integer to string in an arbitrary base (2-36). */
static void int_to_s_base(int64_t n, int base, char *buf) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    int neg = n < 0;
    uint64_t un = neg ? (uint64_t)(-(n + 1)) + 1 : (uint64_t)n;
    char tmp[128]; size_t i = 0;
    while (un > 0) { tmp[i++] = digits[(size_t)(un % (uint64_t)base)]; un /= (uint64_t)base; }
    if (neg) tmp[i++] = '-';
    size_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* Euclidean GCD (always non-negative). */
static int64_t int_gcd(int64_t a, int64_t b) {
    a = a < 0 ? -a : a; b = b < 0 ? -b : b;
    while (b) { int64_t t = b; b = a % b; a = t; }
    return a;
}

int dispatch_integer(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                     Value *blk, Node *site, Value *out) {
    (void)env;
    if (recv.kind != VAL_INT) return 0;
    int64_t n = recv.ival;
    if (strcmp(name, "to_s") == 0) {
        if (argc >= 1 && args[0].kind == VAL_INT) {
            int base = (int)args[0].ival;
            if (base < 2 || base > 36) { *out = eval_raise_class(ev, site, "ArgumentError", "invalid radix %d", base); return 1; }
            char buf[128]; int_to_s_base(n, base, buf);
            *out = val_string(ev->arena, buf);
        } else {
            *out = val_string(ev->arena, val_to_s(ev->arena, recv));
        }
        return 1;
    }
    if (strcmp(name, "to_f") == 0) { *out = val_float((double)n); return 1; }
    if (strcmp(name, "to_i") == 0 || strcmp(name, "to_int") == 0) { *out = recv; return 1; }
    if (strcmp(name, "to_r") == 0) { *out = recv; return 1; } /* simplification: n/1 */
    if (argc == 1) {
        Value r = args[0];
        int both_int = (r.kind == VAL_INT);
        double lf = (double)n, rf = both_int ? (double)r.ival : (r.kind == VAL_FLOAT ? r.fval : 0.0);
        if (strcmp(name, "+") == 0) { *out = both_int ? val_int(n + r.ival) : val_float(lf + rf); return 1; }
        if (strcmp(name, "-") == 0) { *out = both_int ? val_int(n - r.ival) : val_float(lf - rf); return 1; }
        if (strcmp(name, "*") == 0) { *out = both_int ? val_int(n * r.ival) : val_float(lf * rf); return 1; }
        if (strcmp(name, "/") == 0) {
            if (both_int) {
                if (r.ival == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
                *out = val_int(n / r.ival); return 1;
            }
            *out = val_float(lf / rf); return 1;
        }
        if (strcmp(name, "%") == 0) {
            if (both_int) {
                if (r.ival == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
                *out = val_int(n % r.ival); return 1;
            }
            *out = val_float(fmod(lf, rf)); return 1;
        }
        if (strcmp(name, "**") == 0) {
            *out = (both_int && r.ival >= 0) ? val_int((int64_t)pow(lf, rf)) : val_float(pow(lf, rf));
            return 1;
        }
        if (strcmp(name, "<")   == 0) { *out = val_bool(lf <  rf); return 1; }
        if (strcmp(name, "<=")  == 0) { *out = val_bool(lf <= rf); return 1; }
        if (strcmp(name, ">")   == 0) { *out = val_bool(lf >  rf); return 1; }
        if (strcmp(name, ">=")  == 0) { *out = val_bool(lf >= rf); return 1; }
        if (strcmp(name, "<=>") == 0) { *out = val_int(lf < rf ? -1 : lf > rf ? 1 : 0); return 1; }
        if (both_int) {
            if (strcmp(name, "&")  == 0) { *out = val_int(n & r.ival); return 1; }
            if (strcmp(name, "|")  == 0) { *out = val_int(n | r.ival); return 1; }
            if (strcmp(name, "^")  == 0) { *out = val_int(n ^ r.ival); return 1; }
            if (strcmp(name, "<<") == 0) { *out = val_int(n << r.ival); return 1; }
            if (strcmp(name, ">>") == 0) { *out = val_int(n >> r.ival); return 1; }
        }
    }
    if (strcmp(name, "abs") == 0)      { *out = val_int(n < 0 ? -n : n); return 1; }
    if (strcmp(name, "abs2") == 0)     { *out = val_int(n * n); return 1; }
    if (strcmp(name, "even?") == 0)    { *out = val_bool(n % 2 == 0); return 1; }
    if (strcmp(name, "odd?") == 0)     { *out = val_bool(n % 2 != 0); return 1; }
    if (strcmp(name, "zero?") == 0)    { *out = val_bool(n == 0); return 1; }
    if (strcmp(name, "nonzero?") == 0) { *out = n == 0 ? val_nil() : recv; return 1; }
    if (strcmp(name, "positive?") == 0){ *out = val_bool(n > 0); return 1; }
    if (strcmp(name, "negative?") == 0){ *out = val_bool(n < 0); return 1; }
    if (strcmp(name, "integer?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "ceil") == 0 || strcmp(name, "floor") == 0 ||
        strcmp(name, "round") == 0 || strcmp(name, "truncate") == 0) { *out = recv; return 1; }
    if (strcmp(name, "succ") == 0 || strcmp(name, "next") == 0) { *out = val_int(n + 1); return 1; }
    if (strcmp(name, "pred") == 0) { *out = val_int(n - 1); return 1; }
    if (strcmp(name, "chr") == 0) {
        if (n < 0 || n > 127) { *out = eval_raise_class(ev, site, "RangeError", "%lld out of char range", (long long)n); return 1; }
        char buf[2] = { (char)n, '\0' };
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "[]") == 0) {
        /* Bit indexing: n[i] returns the ith bit of n (LSB = 0) */
        if (argc < 1 || args[0].kind != VAL_INT) { *out = val_int(0); return 1; }
        int64_t idx = args[0].ival;
        if (idx < 0) { *out = val_int(0); return 1; }
        *out = val_int((n >> idx) & 1);
        return 1;
    }
    if (strcmp(name, "coerce") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        Value pair = val_array_new();
        if (args[0].kind == VAL_FLOAT) {
            val_array_push(&pair, args[0]);
            val_array_push(&pair, val_float((double)n));
        } else {
            val_array_push(&pair, args[0].kind == VAL_INT ? args[0] : val_int(n));
            val_array_push(&pair, recv);
        }
        *out = pair; return 1;
    }
    if (strcmp(name, "gcd") == 0) {
        if (argc < 1 || args[0].kind != VAL_INT) { *out = eval_raise_class(ev, site, "TypeError", "Integer#gcd requires an Integer"); return 1; }
        *out = val_int(int_gcd(n, args[0].ival));
        return 1;
    }
    if (strcmp(name, "lcm") == 0) {
        if (argc < 1 || args[0].kind != VAL_INT) { *out = eval_raise_class(ev, site, "TypeError", "Integer#lcm requires an Integer"); return 1; }
        int64_t b = args[0].ival;
        int64_t g = int_gcd(n, b);
        *out = val_int(g == 0 ? 0 : (n < 0 ? -n : n) / g * (b < 0 ? -b : b));
        return 1;
    }
    if (strcmp(name, "pow") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Integer#pow requires an argument"); return 1; }
        int64_t exp = args[0].kind == VAL_INT ? args[0].ival : (int64_t)args[0].fval;
        if (argc >= 2 && args[1].kind == VAL_INT) {
            int64_t mod = args[1].ival;
            if (mod == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
            int64_t result = 1, base = ((n % mod) + mod) % mod;
            for (int64_t e = exp; e > 0; e >>= 1) {
                if (e & 1) result = result * base % mod;
                base = base * base % mod;
            }
            *out = val_int(result);
        } else {
            if (exp < 0) { *out = val_float(pow((double)n, (double)exp)); return 1; }
            int64_t result = 1;
            for (int64_t e = exp; e > 0; e--) result *= n;
            *out = val_int(result);
        }
        return 1;
    }
    if (strcmp(name, "remainder") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        int64_t b = args[0].kind == VAL_INT ? args[0].ival : (int64_t)args[0].fval;
        if (b == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
        *out = val_int(n % b);  /* C's % is truncated, matching Ruby's remainder */
        return 1;
    }
    if (strcmp(name, "integer?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "to_r") == 0) {
        /* Return a simplified Rational representation as a string for now */
        char buf[32]; snprintf(buf, sizeof(buf), "(%lld/1)", (long long)n);
        *out = val_string(ev->arena, buf); return 1;
    }
    if (strcmp(name, "to_c") == 0) {
        char buf[32]; snprintf(buf, sizeof(buf), "(%lld+0i)", (long long)n);
        *out = val_string(ev->arena, buf); return 1;
    }
    if (strcmp(name, "ceil") == 0 || strcmp(name, "floor") == 0 ||
        strcmp(name, "truncate") == 0 || strcmp(name, "round") == 0) {
        *out = recv; return 1;  /* integers are already integers */
    }
    if (strcmp(name, "fdiv") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        double b = args[0].kind == VAL_FLOAT ? args[0].fval : (double)args[0].ival;
        *out = val_float(b == 0.0 ? (n >= 0 ? 1.0/0.0 : -1.0/0.0) : (double)n / b); return 1;
    }
    if (strcmp(name, "divmod") == 0) {
        if (argc < 1 || args[0].kind != VAL_INT) { *out = eval_raise_class(ev, site, "TypeError", "Integer#divmod requires an Integer"); return 1; }
        int64_t b = args[0].ival;
        if (b == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
        int64_t q = n / b, r = n % b;
        if (r != 0 && ((r ^ b) < 0)) { q--; r += b; }
        Value arr = val_array_new();
        val_array_push(&arr, val_int(q));
        val_array_push(&arr, val_int(r));
        *out = arr;
        return 1;
    }
    if (strcmp(name, "digits") == 0) {
        int64_t base = argc >= 1 && args[0].kind == VAL_INT ? args[0].ival : 10;
        if (base < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "invalid radix %lld", (long long)base); return 1; }
        Value arr = val_array_new();
        int64_t un = n < 0 ? -n : n;
        if (un == 0) { val_array_push(&arr, val_int(0)); }
        else { while (un > 0) { val_array_push(&arr, val_int(un % base)); un /= base; } }
        *out = arr;
        return 1;
    }
    if (strcmp(name, "between?") == 0) {
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        double lo = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
        double hi = args[1].kind == VAL_INT ? (double)args[1].ival : args[1].fval;
        *out = val_bool((double)n >= lo && (double)n <= hi);
        return 1;
    }
    if (strcmp(name, "clamp") == 0) {
        if (argc == 1 && args[0].kind == VAL_RANGE) {
            RubyRange *r = args[0].range;
            int64_t lo = r->begin_val.kind == VAL_INT ? r->begin_val.ival :
                         r->begin_val.kind == VAL_NIL ? INT64_MIN : (int64_t)r->begin_val.fval;
            int64_t hi = r->end_val.kind == VAL_INT ? r->end_val.ival :
                         r->end_val.kind == VAL_NIL ? INT64_MAX : (int64_t)r->end_val.fval;
            if (r->exclusive && r->end_val.kind == VAL_INT) hi--;
            *out = val_int(n < lo ? lo : n > hi ? hi : n); return 1;
        }
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        /* nil bounds mean "no bound" (Ruby 2.7+) */
        int has_lo = args[0].kind != VAL_NIL;
        int has_hi = args[1].kind != VAL_NIL;
        int64_t lo = has_lo ? (args[0].kind == VAL_INT ? args[0].ival : (int64_t)args[0].fval) : INT64_MIN;
        int64_t hi = has_hi ? (args[1].kind == VAL_INT ? args[1].ival : (int64_t)args[1].fval) : INT64_MAX;
        *out = val_int(n < lo ? lo : n > hi ? hi : n);
        return 1;
    }
    if (strcmp(name, "times") == 0) {
        if (!blk) {
            Value arr = val_array_new();
            for (int64_t i = 0; i < n; i++) val_array_push(&arr, val_int(i));
            *out = arr;
        } else {
            for (int64_t i = 0; i < n; i++) {
                Value arg = val_int(i);
                Value r = call_block(ev, env, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "upto") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Integer#upto requires an argument"); return 1; }
        int64_t limit = args[0].kind == VAL_INT ? args[0].ival : (int64_t)args[0].fval;
        if (!blk) {
            Value arr = val_array_new();
            for (int64_t i = n; i <= limit; i++) val_array_push(&arr, val_int(i));
            *out = arr;
        } else {
            for (int64_t i = n; i <= limit; i++) {
                Value arg = val_int(i);
                Value r = call_block(ev, env, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "downto") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Integer#downto requires an argument"); return 1; }
        int64_t limit = args[0].kind == VAL_INT ? args[0].ival : (int64_t)args[0].fval;
        if (!blk) {
            Value arr = val_array_new();
            for (int64_t i = n; i >= limit; i--) val_array_push(&arr, val_int(i));
            *out = arr;
        } else {
            for (int64_t i = n; i >= limit; i--) {
                Value arg = val_int(i);
                Value r = call_block(ev, env, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "step") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Integer#step requires a limit"); return 1; }
        double limit = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
        double step  = argc >= 2 ? (args[1].kind == VAL_INT ? (double)args[1].ival : args[1].fval) : 1.0;
        if (step == 0.0) { *out = eval_raise_class(ev, site, "ArgumentError", "step cannot be 0"); return 1; }
        if (!blk) {
            Value arr = val_array_new();
            for (double i = (double)n; step > 0 ? i <= limit : i >= limit; i += step)
                val_array_push(&arr, val_int((int64_t)i));
            *out = arr;
        } else {
            for (double i = (double)n; step > 0 ? i <= limit : i >= limit; i += step) {
                Value arg = val_int((int64_t)i);
                Value r = call_block(ev, env, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    return 0;
}

int dispatch_float(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out) {
    (void)env; (void)blk;
    if (recv.kind != VAL_FLOAT) return 0;
    double f = recv.fval;
    if (strcmp(name, "to_s") == 0)   { *out = val_string(ev->arena, val_to_s(ev->arena, recv)); return 1; }
    if (strcmp(name, "to_f") == 0)   { *out = recv; return 1; }
    if (strcmp(name, "to_i") == 0 || strcmp(name, "truncate") == 0) {
        int ndigits = (argc >= 1 && args[0].kind == VAL_INT) ? (int)args[0].ival : 0;
        if (ndigits == 0) { *out = val_int((int64_t)f); return 1; }
        double factor = pow(10.0, (double)ndigits);
        *out = val_float(trunc(f * factor) / factor);
        return 1;
    }
    if (strcmp(name, "to_r") == 0)   { *out = recv; return 1; } /* simplification */
    if (argc == 1) {
        double rf = (args[0].kind == VAL_FLOAT) ? args[0].fval
                  : (args[0].kind == VAL_INT)   ? (double)args[0].ival : 0.0;
        if (strcmp(name, "+")   == 0) { *out = val_float(f + rf); return 1; }
        if (strcmp(name, "-")   == 0) { *out = val_float(f - rf); return 1; }
        if (strcmp(name, "*")   == 0) { *out = val_float(f * rf); return 1; }
        if (strcmp(name, "/")   == 0) { *out = val_float(f / rf); return 1; }
        if (strcmp(name, "%")   == 0) { *out = val_float(fmod(f, rf)); return 1; }
        if (strcmp(name, "**")  == 0) { *out = val_float(pow(f, rf)); return 1; }
        if (strcmp(name, "<")   == 0) { *out = val_bool(f <  rf); return 1; }
        if (strcmp(name, "<=")  == 0) { *out = val_bool(f <= rf); return 1; }
        if (strcmp(name, ">")   == 0) { *out = val_bool(f >  rf); return 1; }
        if (strcmp(name, ">=")  == 0) { *out = val_bool(f >= rf); return 1; }
        if (strcmp(name, "<=>") == 0) { *out = val_int(f < rf ? -1 : f > rf ? 1 : 0); return 1; }
    }
    if (strcmp(name, "abs") == 0)    { *out = val_float(f < 0 ? -f : f); return 1; }
    if (strcmp(name, "abs2") == 0)   { *out = val_float(f * f); return 1; }
    if (strcmp(name, "zero?") == 0)  { *out = val_bool(f == 0.0); return 1; }
    if (strcmp(name, "nonzero?") == 0) { *out = f == 0.0 ? val_nil() : recv; return 1; }
    if (strcmp(name, "positive?") == 0) { *out = val_bool(f > 0.0); return 1; }
    if (strcmp(name, "negative?") == 0) { *out = val_bool(f < 0.0); return 1; }
    if (strcmp(name, "integer?") == 0)  { *out = val_false(); return 1; }
    if (strcmp(name, "coerce") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        Value pair = val_array_new();
        double other = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
        val_array_push(&pair, val_float(other));
        val_array_push(&pair, recv);
        *out = pair; return 1;
    }
    if (strcmp(name, "nan?") == 0)      { *out = val_bool(isnan(f)); return 1; }
    if (strcmp(name, "finite?") == 0)   { *out = val_bool(isfinite(f)); return 1; }
    if (strcmp(name, "infinite?") == 0) {
        *out = isinf(f) ? val_int(f > 0 ? 1 : -1) : val_nil();
        return 1;
    }
    if (strcmp(name, "ceil") == 0) {
        int ndigits = (argc >= 1 && args[0].kind == VAL_INT) ? (int)args[0].ival : 0;
        if (ndigits == 0) { *out = val_int((int64_t)ceil(f)); return 1; }
        double factor = pow(10.0, (double)ndigits);
        *out = val_float(ceil(f * factor) / factor);
        return 1;
    }
    if (strcmp(name, "floor") == 0) {
        int ndigits = (argc >= 1 && args[0].kind == VAL_INT) ? (int)args[0].ival : 0;
        if (ndigits == 0) { *out = val_int((int64_t)floor(f)); return 1; }
        double factor = pow(10.0, (double)ndigits);
        *out = val_float(floor(f * factor) / factor);
        return 1;
    }
    if (strcmp(name, "round") == 0) {
        int ndigits = (argc >= 1 && args[0].kind == VAL_INT) ? (int)args[0].ival : 0;
        if (ndigits == 0) { *out = val_int((int64_t)round(f)); return 1; }
        double factor = pow(10.0, (double)ndigits);
        *out = val_float(round(f * factor) / factor);
        return 1;
    }
    if (strcmp(name, "divmod") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Float#divmod requires an argument"); return 1; }
        double b = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
        if (b == 0.0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
        double q = floor(f / b);
        Value arr = val_array_new();
        val_array_push(&arr, val_int((int64_t)q));
        val_array_push(&arr, val_float(f - q * b));
        *out = arr;
        return 1;
    }
    if (strcmp(name, "between?") == 0) {
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        double lo = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
        double hi = args[1].kind == VAL_INT ? (double)args[1].ival : args[1].fval;
        *out = val_bool(f >= lo && f <= hi);
        return 1;
    }
    if (strcmp(name, "clamp") == 0) {
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        int has_lo = args[0].kind != VAL_NIL;
        int has_hi = args[1].kind != VAL_NIL;
        double lo = has_lo ? (args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval) : -1.0/0.0;
        double hi = has_hi ? (args[1].kind == VAL_INT ? (double)args[1].ival : args[1].fval) :  1.0/0.0;
        *out = val_float(f < lo ? lo : f > hi ? hi : f);
        return 1;
    }
    if (strcmp(name, "step") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Float#step requires a limit"); return 1; }
        if (!blk)     { *out = eval_raise_class(ev, site, "LocalJumpError", "Float#step requires a block"); return 1; }
        double limit = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
        double step  = argc >= 2 ? (args[1].kind == VAL_INT ? (double)args[1].ival : args[1].fval) : 1.0;
        if (step == 0.0) { *out = eval_raise_class(ev, site, "ArgumentError", "step cannot be 0"); return 1; }
        for (double i = f; step > 0 ? i <= limit : i >= limit; i += step) {
            Value arg = val_float(i);
            Value r = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv;
        return 1;
    }
    return 0;
}

int dispatch_string(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                    Value *blk, Node *site, Value *out) {
    (void)env;
    if (recv.kind != VAL_STRING) return 0;
    const char *s = recv.sval ? recv.sval : "";
    if (strcmp(name, "to_s") == 0) { *out = recv; return 1; }
    if (strcmp(name, "<=>") == 0) {
        if (argc < 1 || args[0].kind != VAL_STRING) { *out = val_nil(); return 1; }
        int c = strcmp(s, args[0].sval ? args[0].sval : "");
        *out = val_int(c < 0 ? -1 : c > 0 ? 1 : 0); return 1;
    }
    if (strcmp(name, "encoding") == 0) { *out = val_string(ev->arena, "UTF-8"); return 1; }
    if (strcmp(name, "valid_encoding?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "scrub") == 0) {
        /* All our strings are valid UTF-8; block form replaces invalid bytes (no-op here) */
        *out = recv; return 1;
    }
    if (strcmp(name, "casecmp") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        if (args[0].kind != VAL_STRING) { *out = val_nil(); return 1; }
        int r = strcasecmp(s, args[0].sval);
        *out = val_int(r < 0 ? -1 : r > 0 ? 1 : 0); return 1;
    }
    if (strcmp(name, "casecmp?") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        if (args[0].kind != VAL_STRING) { *out = val_nil(); return 1; }
        *out = val_bool(strcasecmp(s, args[0].sval) == 0); return 1;
    }
    if (strcmp(name, "ascii_only?") == 0) {
        const char *p = s;
        while (*p) { if ((unsigned char)*p > 127) { *out = val_false(); return 1; } p++; }
        *out = val_true(); return 1;
    }
    if (strcmp(name, "bytesize") == 0) { *out = val_int((int64_t)strlen(s)); return 1; }
    if (strcmp(name, "b") == 0 || strcmp(name, "force_encoding") == 0)
        { *out = recv; return 1; } /* no-op: we're already UTF-8-only */
    if (strcmp(name, "encode") == 0)
        { *out = recv; return 1; } /* stub: no transcoding, identity for UTF-8 */
    if (strcmp(name, "to_i") == 0) {
        int base = (argc > 0 && args[0].kind == VAL_INT) ? (int)args[0].ival : 10;
        if (base < 2 || base > 36) base = 10;
        *out = val_int((int64_t)strtoll(s, NULL, base)); return 1;
    }
    if (strcmp(name, "to_f") == 0) { *out = val_float(atof(s)); return 1; }
    if (strcmp(name, "to_sym") == 0) { *out = val_symbol(s); return 1; }
    if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0) { *out = val_int((int64_t)utf8_char_count(s)); return 1; }
    if (strcmp(name, "empty?") == 0) { *out = val_bool(s[0] == '\0'); return 1; }
    if (strcmp(name, "upcase") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len * 2 + 1);
        size_t used = 0;
        for (size_t i = 0; i < len;) {
            uint32_t cp = 0;
            size_t width = 0, outw = 0;
            char enc[4];
            utf8_decode_one(s + i, len - i, &cp, &width);
            outw = utf8_encode_one(utf8_simple_upcase(cp), enc);
            memcpy(buf + used, enc, outw);
            used += outw;
            i += width;
        }
        buf[used] = '\0';
        *out = val_string_n(ev->arena, buf, used);
        return 1;
    }
    if (strcmp(name, "downcase") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len * 2 + 1);
        size_t used = 0;
        for (size_t i = 0; i < len;) {
            uint32_t cp = 0;
            size_t width = 0, outw = 0;
            char enc[4];
            utf8_decode_one(s + i, len - i, &cp, &width);
            outw = utf8_encode_one(utf8_simple_downcase(cp), enc);
            memcpy(buf + used, enc, outw);
            used += outw;
            i += width;
        }
        buf[used] = '\0';
        *out = val_string_n(ev->arena, buf, used);
        return 1;
    }
    if (strcmp(name, "upcase!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "upcase", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "downcase!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "downcase", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "strip") == 0) {
        size_t len = strlen(s);
        size_t start = 0;
        size_t end = len;
        while (start < len) {
            uint32_t cp = 0;
            size_t width = 0;
            utf8_decode_one(s + start, len - start, &cp, &width);
            if (!utf8_space(cp)) break;
            start += width;
        }
        while (end > start) {
            size_t prev = utf8_prev_char_start(s, end);
            uint32_t cp = 0;
            utf8_decode_one(s + prev, end - prev, &cp, NULL);
            if (!utf8_space(cp)) break;
            end = prev;
        }
        *out = val_string_n(ev->arena, s + start, end - start);
        return 1;
    }
    if (strcmp(name, "strip!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "strip", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "lstrip!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "lstrip", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "rstrip!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "rstrip", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "chomp!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "chomp", args, argc, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "chop!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "chop", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && s[0] != '\0') ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "reverse!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "reverse", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "capitalize!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "capitalize", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "swapcase!") == 0) {
        Value r = val_nil();
        dispatch_string(ev, env, recv, "swapcase", NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "chars") == 0) {
        Value arr = val_array_new();
        size_t chars = utf8_char_count(s);
        for (size_t i = 0; i < chars; i++) {
            const char *ptr = NULL;
            size_t width = 0;
            utf8_char_at(s, i, &ptr, &width, NULL);
            val_array_push(&arr, val_string_n(ev->arena, ptr, width));
        }
        *out = arr;
        return 1;
    }
    if (strcmp(name, "include?") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "String#include? requires an argument");
        else *out = val_bool(strstr(s, val_to_s(ev->arena, args[0])) != NULL);
        return 1;
    }
    if (strcmp(name, "start_with?") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#start_with? requires an argument"); return 1; }
        for (int i = 0; i < argc; i++) {
            const char *needle = val_to_s(ev->arena, args[i]);
            if (strncmp(s, needle, strlen(needle)) == 0) { *out = val_true(); return 1; }
        }
        *out = val_false(); return 1;
    }
    if (strcmp(name, "end_with?") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#end_with? requires an argument"); return 1; }
        size_t slen = strlen(s);
        for (int i = 0; i < argc; i++) {
            const char *needle = val_to_s(ev->arena, args[i]);
            size_t nlen = strlen(needle);
            if (slen >= nlen && strcmp(s + slen - nlen, needle) == 0) { *out = val_true(); return 1; }
        }
        *out = val_false(); return 1;
    }
    if (strcmp(name, "insert") == 0) {
        if (argc < 2 || args[0].kind != VAL_INT || args[1].kind != VAL_STRING)
            { *out = eval_raise_class(ev, site, "ArgumentError", "String#insert requires index and string"); return 1; }
        size_t slen = strlen(s);
        int64_t idx = args[0].ival;
        if (idx < 0) idx = (int64_t)slen + 1 + idx;
        if (idx < 0) idx = 0;
        if ((size_t)idx > slen) idx = (int64_t)slen;
        const char *ins = args[1].sval;
        size_t ilen = strlen(ins);
        char *buf = arena_alloc(ev->arena, slen + ilen + 1);
        memcpy(buf, s, (size_t)idx);
        memcpy(buf + idx, ins, ilen);
        memcpy(buf + idx + ilen, s + idx, slen - (size_t)idx);
        buf[slen + ilen] = '\0';
        *out = val_string(ev->arena, buf); return 1;
    }
    if (strcmp(name, "prepend") == 0) {
        /* In-place prepend: str.prepend("prefix") */
        if (argc < 1) { *out = recv; return 1; }
        size_t slen = strlen(s);
        /* Concatenate all args in order, prepend the result */
        size_t total_ins = 0;
        for (int i = 0; i < argc; i++) total_ins += strlen(val_to_s(ev->arena, args[i]));
        char *buf = arena_alloc(ev->arena, total_ins + slen + 1);
        size_t pos = 0;
        for (int i = 0; i < argc; i++) {
            const char *part = val_to_s(ev->arena, args[i]);
            size_t plen = strlen(part);
            memcpy(buf + pos, part, plen);
            pos += plen;
        }
        memcpy(buf + pos, s, slen);
        buf[pos + slen] = '\0';
        *out = val_string(ev->arena, buf); return 1;
    }
    if (strcmp(name, "slice!") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        size_t slen = strlen(s);
        int64_t idx = args[0].kind == VAL_INT ? args[0].ival : 0;
        if (idx < 0) idx += (int64_t)slen;
        if (idx < 0 || (size_t)idx >= slen) { *out = val_nil(); return 1; }
        if (argc >= 2 && args[1].kind == VAL_INT) {
            int64_t len = args[1].ival;
            if (len < 0) { *out = val_nil(); return 1; }
            size_t take = ((size_t)idx + (size_t)len > slen) ? slen - (size_t)idx : (size_t)len;
            *out = val_string_n(ev->arena, s + idx, take);
        } else {
            *out = val_string_n(ev->arena, s + idx, 1);
        }
        return 1;
    }
    if (strcmp(name, "delete_prefix") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#delete_prefix requires an argument"); return 1; }
        const char *prefix = val_to_s(ev->arena, args[0]);
        size_t plen = strlen(prefix);
        if (strncmp(s, prefix, plen) == 0) *out = val_string(ev->arena, s + plen);
        else *out = val_string(ev->arena, s);
        return 1;
    }
    if (strcmp(name, "delete_suffix") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#delete_suffix requires an argument"); return 1; }
        const char *suffix = val_to_s(ev->arena, args[0]);
        size_t slen = strlen(s), suflen = strlen(suffix);
        if (slen >= suflen && strcmp(s + slen - suflen, suffix) == 0)
            *out = val_string_n(ev->arena, s, slen - suflen);
        else *out = val_string(ev->arena, s);
        return 1;
    }
    if (strcmp(name, "split") == 0) {
        int64_t limit = (argc >= 2 && args[1].kind == VAL_INT) ? args[1].ival : 0;
        if (argc >= 1 && value_is_regexp(args[0])) {
            Regex *compiled = args[0].obj->native;
            if (!compiled) {
                Value src; RegexError rerr = {0};
                if (!val_object_get_ivar(args[0], "source", &src) || src.kind != VAL_STRING) {
                    *out = eval_raise_class(ev, site, "RuntimeError", "invalid Regexp object"); return 1;
                }
                if (regex_compile(ev->arena, src.sval, 0, &compiled, &rerr) != REGEX_OK) {
                    *out = eval_raise_class(ev, site, "RegexpError", "%s", rerr.message[0] ? rerr.message : "regexp compile failed"); return 1;
                }
                args[0].obj->native = compiled;
            }
            size_t slen = strlen(s);
            Value arr = val_array_new();
            size_t pos = 0;
            int64_t count = 0;
            while (pos <= slen) {
                if (limit > 0 && count >= limit - 1) break;
                RegexMatch m = {0, 0, 0, NULL, NULL};
                if (regex_search(compiled, s, slen, pos, &m) != REGEX_OK) break;
                size_t mlen = (size_t)(m.end - m.beg);
                if (mlen == 0 && (long)pos == m.beg) { regex_match_free(&m); if (pos < slen) pos++; else break; continue; }
                val_array_push(&arr, val_string_n(ev->arena, s + pos, (size_t)(m.beg - pos)));
                count++;
                for (size_t i = 0; i < m.capture_count; i++) {
                    long gb = m.cap_beg ? m.cap_beg[i] : -1;
                    long ge = m.cap_end ? m.cap_end[i] : -1;
                    val_array_push(&arr, (gb < 0) ? val_nil()
                                                  : val_string_n(ev->arena, s + (size_t)gb, (size_t)(ge - gb)));
                }
                pos = (size_t)m.end;
                regex_match_free(&m);
            }
            val_array_push(&arr, val_string_n(ev->arena, s + pos, slen - pos));
            if (limit == 0) {
                while (arr.array->len > 0) {
                    Value last = arr.array->elems[arr.array->len - 1];
                    if (last.kind == VAL_STRING && last.sval && last.sval[0] == '\0')
                        arr.array->len--;
                    else break;
                }
            }
            *out = arr;
            return 1;
        }
        Value arr = val_array_new();
        /* Special case: no arg, nil arg, or " " literal → Ruby whitespace split
           (strip leading/trailing, split on any whitespace run, no empty fields) */
        int ws_split = (argc == 0) || (args[0].kind == VAL_NIL) ||
                       (args[0].kind == VAL_STRING && strcmp(args[0].sval, " ") == 0);
        if (ws_split) {
            const char *p = s;
            /* skip leading whitespace */
            while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
            int64_t count = 0;
            while (*p) {
                if (limit > 0 && count >= limit - 1) {
                    /* last field: rest of string (trimmed leading whitespace already done) */
                    val_array_push(&arr, val_string(ev->arena, p));
                    break;
                }
                const char *start = p;
                while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
                if (p > start) { val_array_push(&arr, val_string_n(ev->arena, start, (size_t)(p - start))); count++; }
                while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
            }
            *out = arr; return 1;
        }
        const char *sep = val_to_s(ev->arena, args[0]);
        size_t seplen = strlen(sep);
        if (seplen == 0) {
            size_t chars = utf8_char_count(s);
            for (size_t i = 0; i < chars; i++) {
                const char *ptr = NULL;
                size_t width = 0;
                utf8_char_at(s, i, &ptr, &width, NULL);
                val_array_push(&arr, val_string_n(ev->arena, ptr, width));
            }
        } else {
            const char *p = s, *found;
            int64_t count = 0;
            while ((found = strstr(p, sep)) != NULL) {
                if (limit > 0 && count >= limit - 1) break;
                val_array_push(&arr, val_string_n(ev->arena, p, (size_t)(found - p)));
                p = found + seplen;
                count++;
            }
            val_array_push(&arr, val_string(ev->arena, p));
        }
        *out = arr;
        return 1;
    }
    if (strcmp(name, "each_char") == 0) {
        size_t chars = utf8_char_count(s);
        if (!blk) {
            /* blockless: return array of chars */
            Value arr = val_array_new();
            for (size_t i = 0; i < chars; i++) {
                const char *ptr = NULL; size_t width = 0;
                utf8_char_at(s, i, &ptr, &width, NULL);
                val_array_push(&arr, val_string_n(ev->arena, ptr, width));
            }
            *out = arr;
        } else {
            for (size_t i = 0; i < chars; i++) {
                const char *ptr = NULL;
                size_t width = 0;
                utf8_char_at(s, i, &ptr, &width, NULL);
                Value ch = val_string_n(ev->arena, ptr, width);
                Value r = call_block(ev, env, *blk, &ch, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "reverse") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len + 1);
        size_t pos = 0;
        size_t end = len;
        while (end > 0) {
            size_t start = utf8_prev_char_start(s, end);
            memcpy(buf + pos, s + start, end - start);
            pos += end - start;
            end = start;
        }
        buf[pos] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "next") == 0 || strcmp(name, "succ") == 0) {
        size_t len = strlen(s);
        if (len == 0) { *out = val_string(ev->arena, ""); return 1; }
        int has_alnum = 0;
        for (size_t i = 0; i < len;) {
            uint32_t cp = 0;
            size_t width = 0;
            utf8_decode_one(s + i, len - i, &cp, &width);
            if (utf8_ascii_alnum(cp)) { has_alnum = 1; break; }
            i += width;
        }
        char *buf = arena_alloc(ev->arena, len + 2);
        memcpy(buf, s, len + 1);
        if (!has_alnum) {
            size_t start = utf8_prev_char_start(s, len);
            uint32_t cp = 0;
            size_t width = 0, outw = 0;
            char enc[4];
            utf8_decode_one(s + start, len - start, &cp, &width);
            if (cp < 0x10FFFF) cp++;
            memcpy(buf, s, start);
            outw = utf8_encode_one(cp, enc);
            memcpy(buf + start, enc, outw);
            buf[start + outw] = '\0';
            *out = val_string_n(ev->arena, buf, start + outw);
            return 1;
        }
        char prepend = '\0';
        for (int i = (int)len - 1; i >= 0; i--) {
            unsigned char c = (unsigned char)buf[i];
            if (!utf8_ascii_alnum(c)) continue;
            if      (c == 'z') { buf[i] = 'a'; prepend = 'a'; }
            else if (c == 'Z') { buf[i] = 'A'; prepend = 'A'; }
            else if (c == '9') { buf[i] = '0'; prepend = '1'; }
            else               { buf[i] = (char)(c + 1); prepend = '\0'; break; }
        }
        if (!prepend) { *out = val_string(ev->arena, buf); return 1; }
        size_t ins = 0;
        while (ins < len && !isalnum((unsigned char)buf[ins])) ins++;
        char *result = arena_alloc(ev->arena, len + 2);
        memcpy(result, buf, ins);
        result[ins] = prepend;
        memcpy(result + ins + 1, buf + ins, len - ins + 1);
        *out = val_string(ev->arena, result);
        return 1;
    }
    if (strcmp(name, "replace") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "String#replace requires an argument");
        else *out = val_string(ev->arena, val_to_s(ev->arena, args[0]));
        return 1;
    }
    if (strcmp(name, "clear") == 0) {
        *out = val_string(ev->arena, "");
        return 1;
    }
    if (strcmp(name, "setbyte") == 0) { *out = recv; return 1; }
    if (strcmp(name, "upto") == 0) {
        if (argc < 1 || args[0].kind != VAL_STRING) {
            *out = eval_raise_class(ev, site, "ArgumentError", "String#upto requires a string argument");
            return 1;
        }
        Value arr = val_array_new();
        Value current = recv;
        const char *end_s = args[0].sval;
        /* Iterate using succ until we reach or pass the end string */
        for (int max_iter = 0; max_iter < 100000; max_iter++) {
            const char *cur_s = current.sval;
            /* Stop if current > end (lexicographic) */
            if (strcmp(cur_s, end_s) > 0) break;
            if (blk) {
                Value r = call_block(ev, env, *blk, &current, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            } else {
                val_array_push(&arr, current);
            }
            if (strcmp(cur_s, end_s) == 0) break;
            /* Advance to next using succ */
            Value next_val = val_nil();
            dispatch_string(ev, env, current, "succ", NULL, 0, NULL, NULL, &next_val);
            if (next_val.kind != VAL_STRING) break;
            current = next_val;
        }
        *out = blk ? recv : arr;
        return 1;
    }
    if (strcmp(name, "inspect") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len * 2 + 3);
        size_t j = 0;
        buf[j++] = '"';
        for (size_t i = 0; i < len; i++) {
            switch (s[i]) {
                case '"':  buf[j++] = '\\'; buf[j++] = '"';  break;
                case '\\': buf[j++] = '\\'; buf[j++] = '\\'; break;
                case '\n': buf[j++] = '\\'; buf[j++] = 'n';  break;
                case '\r': buf[j++] = '\\'; buf[j++] = 'r';  break;
                case '\t': buf[j++] = '\\'; buf[j++] = 't';  break;
                default:   buf[j++] = s[i]; break;
            }
        }
        buf[j++] = '"';
        buf[j] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "chomp") == 0) {
        size_t len = strlen(s);
        if (argc > 0 && args[0].kind == VAL_STRING) {
            /* chomp with explicit separator */
            const char *sep = args[0].sval;
            size_t seplen = strlen(sep);
            if (seplen > 0 && len >= seplen && strcmp(s + len - seplen, sep) == 0)
                len -= seplen;
        } else {
            if (len > 0 && s[len - 1] == '\n') {
                len--;
                if (len > 0 && s[len - 1] == '\r') len--;
            } else if (len > 0 && s[len - 1] == '\r') {
                len--;
            }
        }
        *out = val_string_n(ev->arena, s, len);
        return 1;
    }
    if (strcmp(name, "chop") == 0) {
        size_t len = strlen(s);
        if (len == 0) { *out = val_string(ev->arena, ""); return 1; }
        if (len >= 2 && s[len - 2] == '\r' && s[len - 1] == '\n') len -= 2;
        else len = utf8_prev_char_start(s, len);
        *out = val_string_n(ev->arena, s, len);
        return 1;
    }
    if (strcmp(name, "lstrip") == 0) {
        size_t len = strlen(s);
        size_t start = 0;
        while (start < len) {
            uint32_t cp = 0;
            size_t width = 0;
            utf8_decode_one(s + start, len - start, &cp, &width);
            if (!utf8_space(cp)) break;
            start += width;
        }
        *out = val_string(ev->arena, s + start);
        return 1;
    }
    if (strcmp(name, "rstrip") == 0) {
        size_t len = strlen(s);
        while (len > 0) {
            size_t prev = utf8_prev_char_start(s, len);
            uint32_t cp = 0;
            utf8_decode_one(s + prev, len - prev, &cp, NULL);
            if (!utf8_space(cp)) break;
            len = prev;
        }
        *out = val_string_n(ev->arena, s, len);
        return 1;
    }
    if (strcmp(name, "capitalize") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len * 2 + 1);
        size_t used = 0;
        int first = 1;
        for (size_t i = 0; i < len;) {
            uint32_t cp = 0;
            size_t width = 0, outw = 0;
            char enc[4];
            utf8_decode_one(s + i, len - i, &cp, &width);
            cp = first ? utf8_simple_upcase(cp) : utf8_simple_downcase(cp);
            outw = utf8_encode_one(cp, enc);
            memcpy(buf + used, enc, outw);
            used += outw;
            first = 0;
            i += width;
        }
        buf[used] = '\0';
        *out = val_string_n(ev->arena, buf, used);
        return 1;
    }
    if (strcmp(name, "swapcase") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len * 2 + 1);
        size_t used = 0;
        for (size_t i = 0; i < len;) {
            uint32_t cp = 0;
            size_t width = 0, outw = 0;
            char enc[4];
            utf8_decode_one(s + i, len - i, &cp, &width);
            if (utf8_simple_upcase(cp) != cp) cp = utf8_simple_upcase(cp);
            else cp = utf8_simple_downcase(cp);
            outw = utf8_encode_one(cp, enc);
            memcpy(buf + used, enc, outw);
            used += outw;
            i += width;
        }
        buf[used] = '\0';
        *out = val_string_n(ev->arena, buf, used);
        return 1;
    }
    if (strcmp(name, "ljust") == 0 || strcmp(name, "rjust") == 0 || strcmp(name, "center") == 0) {
        if (argc < 1) { *out = recv; return 1; }
        int64_t width = args[0].kind == VAL_INT ? args[0].ival : 0;
        const char *pad = argc >= 2 && args[1].kind == VAL_STRING ? args[1].sval : " ";
        size_t pad_chars = utf8_char_count(pad);
        size_t slen = utf8_char_count(s);
        if (pad_chars == 0) pad_chars = 1;
        if ((int64_t)slen >= width) { *out = recv; return 1; }
        size_t total_chars = (size_t)width;
        size_t total = strlen(s) + (total_chars - slen) * strlen(pad) + 1;
        char *buf = arena_alloc(ev->arena, total);
        size_t lpad = 0, rpad = 0;
        size_t pos = 0;
        if (strcmp(name, "ljust") == 0) { lpad = 0; rpad = total_chars - slen; }
        else if (strcmp(name, "rjust") == 0) { lpad = total_chars - slen; rpad = 0; }
        else { lpad = (total_chars - slen) / 2; rpad = total_chars - slen - lpad; }
        append_utf8_pad(buf, &pos, pad, lpad);
        memcpy(buf + pos, s, strlen(s));
        pos += strlen(s);
        append_utf8_pad(buf, &pos, pad, rpad);
        buf[pos] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "ord") == 0) {
        uint32_t cp = 0;
        if (s[0] == '\0' || !utf8_decode_one(s, strlen(s), &cp, NULL)) {
            *out = eval_raise_class(ev, site, "ArgumentError", "empty string");
            return 1;
        }
        *out = val_int((int64_t)cp);
        return 1;
    }
    if (strcmp(name, "hex") == 0) {
        *out = val_int((int64_t)strtoll(s, NULL, 16));
        return 1;
    }
    if (strcmp(name, "oct") == 0) {
        const char *p = s;
        while (*p == ' ' || *p == '\t') p++;
        int base = 8;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
        else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) { base = 2; p += 2; }
        else if (p[0] == '0' && (p[1] == 'o' || p[1] == 'O')) { base = 8; p += 2; }
        *out = val_int((int64_t)strtoll(p, NULL, base));
        return 1;
    }
    if (strcmp(name, "bytes") == 0) {
        Value arr = val_array_new();
        for (size_t i = 0; s[i]; i++)
            val_array_push(&arr, val_int((int64_t)(unsigned char)s[i]));
        *out = arr;
        return 1;
    }
    if (strcmp(name, "unpack") == 0 || strcmp(name, "unpack1") == 0) {
        if (argc < 1 || args[0].kind != VAL_STRING) { *out = val_array_new(); return 1; }
        const char *tmpl = args[0].sval;
        Value arr = val_array_new();
        size_t si = 0, slen = strlen(s);
        for (size_t ti = 0; tmpl[ti] && si < slen; ti++) {
            char dir = tmpl[ti];
            int count = 1, star = 0;
            if (tmpl[ti+1] == '*') { star = 1; ti++; count = (int)(slen - si); }
            else if (isdigit((unsigned char)tmpl[ti+1])) { count = 0; while (isdigit((unsigned char)tmpl[ti+1])) count = count*10 + (tmpl[++ti] - '0'); }
            for (int ci = 0; ci < count && si < slen; ci++) {
                switch (dir) {
                    case 'C': case 'c': val_array_push(&arr, val_int((dir=='c')?(int8_t)(unsigned char)s[si]:(unsigned char)s[si])); si++; break;
                    case 'A': case 'a': case 'Z': {
                        /* eat until null or end */
                        size_t end = si; while (end < slen && s[end]) end++;
                        val_array_push(&arr, val_string_n(ev->arena, s+si, end-si)); si = end+1; break;
                    }
                    default: si++; break;
                }
            }
            if (star) break;
        }
        if (strcmp(name, "unpack1") == 0) {
            *out = arr.array->len > 0 ? arr.array->elems[0] : val_nil();
        } else { *out = arr; }
        return 1;
    }
    if (strcmp(name, "b") == 0) {
        /* Return string with binary encoding (stub: return as-is) */
        *out = recv; return 1;
    }
    if (strcmp(name, "+") == 0 || strcmp(name, "<<") == 0 || strcmp(name, "concat") == 0) {
        if (argc < 1) { *out = recv; return 1; }
        if (strcmp(name, "<<") == 0 && recv.frozen)
            { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen String"); return 1; }
        if (strcmp(name, "+") == 0 && args[0].kind != VAL_STRING) {
            /* Try implicit to_str coercion */
            Value coerced = dispatch_method(ev, env, args[0], "to_str", NULL, 0, NULL, site, 0, -1);
            if (!val_is_signal(coerced) && coerced.kind == VAL_STRING) {
                args[0] = coerced;
            } else {
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
                *out = eval_raise_class(ev, site, "TypeError", "no implicit conversion of %s into String",
                                        value_class_name(ev, args[0]));
                return 1;
            }
        }
        const char *rhs = val_to_s(ev->arena, args[0]);
        size_t slen = strlen(s), rlen = strlen(rhs);
        char *buf = arena_alloc(ev->arena, slen + rlen + 1);
        memcpy(buf, s, slen);
        memcpy(buf + slen, rhs, rlen);
        buf[slen + rlen] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "index") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#index requires an argument"); return 1; }
        const char *needle = val_to_s(ev->arena, args[0]);
        size_t offset = (argc >= 2 && args[1].kind == VAL_INT) ? utf8_byte_offset_for_char(s, (size_t)args[1].ival) : 0;
        if (offset > strlen(s)) { *out = val_nil(); return 1; }
        const char *found = strstr(s + offset, needle);
        *out = found ? val_int((int64_t)utf8_char_index_for_byte(s, (size_t)(found - s))) : val_nil();
        return 1;
    }
    if (strcmp(name, "rindex") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#rindex requires an argument"); return 1; }
        const char *needle = val_to_s(ev->arena, args[0]);
        size_t nlen = strlen(needle), slen = strlen(s);
        size_t limit = (argc >= 2 && args[1].kind == VAL_INT) ? utf8_byte_offset_for_char(s, (size_t)args[1].ival) : slen;
        if (limit > slen) limit = slen;
        const char *last = NULL;
        for (size_t i = 0; i + nlen <= limit + 1 && i <= limit; i++) {
            if (strncmp(s + i, needle, nlen) == 0) last = s + i;
        }
        *out = last ? val_int((int64_t)utf8_char_index_for_byte(s, (size_t)(last - s))) : val_nil();
        return 1;
    }
    if (strcmp(name, "[]") == 0 || strcmp(name, "slice") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#[] requires an argument"); return 1; }
        size_t slen = utf8_char_count(s);
        if (args[0].kind == VAL_RANGE) {
            RubyRange *r = args[0].range;
            if (r->begin_val.kind != VAL_INT && r->begin_val.kind != VAL_NIL) { *out = val_nil(); return 1; }
            if (r->end_val.kind != VAL_INT && r->end_val.kind != VAL_NIL) { *out = val_nil(); return 1; }
            int64_t rbeg = (r->begin_val.kind == VAL_INT) ? r->begin_val.ival : 0;
            int64_t rend = (r->end_val.kind == VAL_INT) ? r->end_val.ival : (int64_t)slen;
            if (r->begin_val.kind == VAL_INT && rbeg < 0) rbeg += (int64_t)slen;
            if (r->end_val.kind == VAL_INT && rend < 0) rend += (int64_t)slen;
            if (rbeg < 0 || (size_t)rbeg > slen) { *out = val_nil(); return 1; }
            if (r->end_val.kind == VAL_INT && !r->exclusive) rend++;
            if (rend < rbeg) { *out = val_string(ev->arena, ""); return 1; }
            if ((size_t)rend > slen) rend = (int64_t)slen;
            size_t bstart = utf8_byte_offset_for_char(s, (size_t)rbeg);
            size_t bend   = utf8_byte_offset_for_char(s, (size_t)rend);
            *out = val_string_n(ev->arena, s + bstart, bend - bstart);
            return 1;
        }
        /* Regex match: s[/pattern/] or s[/pattern/, capture_group_idx] */
        if (value_is_regexp(args[0])) {
            Regex *compiled = args[0].obj->native;
            if (!compiled) {
                Value src;
                RegexError rerr = {0};
                if (!val_object_get_ivar(args[0], "source", &src) || src.kind != VAL_STRING ||
                    regex_compile(ev->arena, src.sval, 0, &compiled, &rerr) != REGEX_OK) {
                    *out = val_nil(); return 1;
                }
                args[0].obj->native = compiled;
            }
            RegexMatch m = {0, 0, 0, NULL, NULL};
            size_t blen = strlen(s);
            if (regex_search(compiled, s, blen, 0, &m) != REGEX_OK) { *out = val_nil(); return 1; }
            if (argc >= 2 && args[1].kind == VAL_INT) {
                int64_t ci = args[1].ival;
                if (ci == 0) {
                    *out = val_string_n(ev->arena, s + (size_t)m.beg, (size_t)(m.end - m.beg));
                } else if (ci > 0 && ci <= (int64_t)m.capture_count &&
                           m.cap_beg[ci-1] >= 0) {
                    *out = val_string_n(ev->arena, s + (size_t)m.cap_beg[ci-1],
                                       (size_t)(m.cap_end[ci-1] - m.cap_beg[ci-1]));
                } else {
                    *out = val_nil();
                }
            } else {
                *out = val_string_n(ev->arena, s + (size_t)m.beg, (size_t)(m.end - m.beg));
            }
            regex_match_free(&m);
            return 1;
        }
        /* String match: s[substr] */
        if (args[0].kind == VAL_STRING) {
            const char *needle = args[0].sval;
            const char *found = strstr(s, needle);
            *out = found ? val_string(ev->arena, needle) : val_nil();
            return 1;
        }
        int64_t idx = args[0].kind == VAL_INT ? args[0].ival : 0;
        if (idx < 0) idx += (int64_t)slen;
        if (idx < 0 || (size_t)idx >= slen) { *out = val_nil(); return 1; }
        if (argc >= 2 && args[1].kind == VAL_INT) {
            int64_t len = args[1].ival;
            if (len < 0) { *out = val_nil(); return 1; }
            size_t take = (size_t)idx + (size_t)len > slen ? slen - (size_t)idx : (size_t)len;
            size_t start = utf8_byte_offset_for_char(s, (size_t)idx);
            size_t end = utf8_byte_offset_for_char(s, (size_t)idx + take);
            *out = val_string_n(ev->arena, s + start, end - start);
        } else {
            const char *ptr = NULL;
            size_t width = 0;
            utf8_char_at(s, (size_t)idx, &ptr, &width, NULL);
            *out = val_string_n(ev->arena, ptr, width);
        }
        return 1;
    }
    if (strcmp(name, "lines") == 0) {
        Value arr = val_array_new();
        const char *p = s;
        while (*p) {
            const char *nl = strchr(p, '\n');
            if (nl) {
                val_array_push(&arr, val_string_n(ev->arena, p, (size_t)(nl - p + 1)));
                p = nl + 1;
            } else {
                val_array_push(&arr, val_string(ev->arena, p));
                break;
            }
        }
        *out = arr;
        return 1;
    }
    if (strcmp(name, "each_line") == 0) {
        if (!blk) {
            /* No block: return array of lines */
            *out = dispatch_method(ev, env, recv, "lines", args, argc, blk, site, 0, 1);
            return 1;
        }
        const char *p = s;
        while (*p) {
            const char *nl = strchr(p, '\n');
            Value line;
            if (nl) {
                line = val_string_n(ev->arena, p, (size_t)(nl - p + 1));
                p = nl + 1;
            } else {
                line = val_string(ev->arena, p);
                p += strlen(p);
            }
            Value r = call_block(ev, env, *blk, &line, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv;
        return 1;
    }
    if (strcmp(name, "tr") == 0) {
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "String#tr requires two arguments"); return 1; }
        const char *from_pat = val_to_s(ev->arena, args[0]);
        const char *to_pat   = val_to_s(ev->arena, args[1]);
        uint32_t from_chars[1024], to_chars[1024];
        size_t from_len = rune_pattern_chars(from_pat, from_chars, 1024);
        size_t to_len   = rune_pattern_chars(to_pat,   to_chars,   1024);
        if (from_len == 0 || to_len == 0) { *out = recv; return 1; }
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen * 4 + 1);
        size_t used = 0;
        for (size_t i = 0; i < slen;) {
            uint32_t cp = 0;
            size_t width = 0, outw = 0;
            char enc[4];
            size_t idx;
            utf8_decode_one(s + i, slen - i, &cp, &width);
            idx = rune_index_in_chars(cp, from_chars, from_len);
            if (idx != (size_t)-1) cp = to_chars[idx < to_len ? idx : to_len - 1];
            outw = utf8_encode_one(cp, enc);
            memcpy(buf + used, enc, outw);
            used += outw;
            i += width;
        }
        buf[used] = '\0';
        *out = val_string_n(ev->arena, buf, used);
        return 1;
    }
    if (strcmp(name, "count") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#count requires an argument"); return 1; }
        RuneRange ranges[256];
        size_t range_count = rune_pattern_ranges(val_to_s(ev->arena, args[0]), ranges, 256);
        int64_t n = 0;
        for (size_t i = 0, slen = strlen(s); i < slen;) {
            uint32_t cp = 0;
            size_t width = 0;
            utf8_decode_one(s + i, slen - i, &cp, &width);
            if (rune_in_ranges(cp, ranges, range_count)) n++;
            i += width;
        }
        *out = val_int(n);
        return 1;
    }
    if (strcmp(name, "delete") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#delete requires an argument"); return 1; }
        RuneRange ranges[256];
        size_t range_count = rune_pattern_ranges(val_to_s(ev->arena, args[0]), ranges, 256);
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen + 1);
        size_t j = 0;
        for (size_t i = 0; i < slen;) {
            uint32_t cp = 0;
            size_t width = 0;
            utf8_decode_one(s + i, slen - i, &cp, &width);
            if (!rune_in_ranges(cp, ranges, range_count)) {
                memcpy(buf + j, s + i, width);
                j += width;
            }
            i += width;
        }
        buf[j] = '\0';
        *out = val_string_n(ev->arena, buf, j);
        return 1;
    }
    if (strcmp(name, "squeeze") == 0) {
        RuneRange ranges[256];
        size_t range_count = 0;
        if (argc >= 1) range_count = rune_pattern_ranges(val_to_s(ev->arena, args[0]), ranges, 256);
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen + 1);
        size_t j = 0;
        uint32_t prev_cp = 0;
        int have_prev = 0;
        for (size_t i = 0; i < slen;) {
            uint32_t cp = 0;
            size_t width = 0;
            int selected;
            utf8_decode_one(s + i, slen - i, &cp, &width);
            selected = argc == 0 ? 1 : rune_in_ranges(cp, ranges, range_count);
            if (have_prev && prev_cp == cp && selected) {
                i += width;
                continue;
            }
            memcpy(buf + j, s + i, width);
            j += width;
            prev_cp = cp;
            have_prev = 1;
            i += width;
        }
        buf[j] = '\0';
        *out = val_string_n(ev->arena, buf, j);
        return 1;
    }
    if (strcmp(name, "scan") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#scan requires an argument"); return 1; }
        if (value_is_regexp(args[0])) {
            Regex *compiled = args[0].obj->native;
            if (!compiled) {
                Value src;
                RegexError rerr = {0};
                if (!val_object_get_ivar(args[0], "source", &src) || src.kind != VAL_STRING) {
                    *out = eval_raise_class(ev, site, "RuntimeError", "invalid Regexp object");
                    return 1;
                }
                if (regex_compile(ev->arena, src.sval, 0, &compiled, &rerr) != REGEX_OK) {
                    *out = eval_raise_class(ev, site, "RegexpError", "%s",
                                           rerr.message[0] ? rerr.message : "regexp compile failed");
                    return 1;
                }
                args[0].obj->native = compiled;
            }
            size_t slen = strlen(s);
            Value arr = val_array_new();
            size_t pos = 0;
            while (pos <= slen) {
                RegexMatch m = {0, 0, 0, NULL, NULL};
                if (regex_search(compiled, s, slen, pos, &m) != REGEX_OK) break;
                size_t mlen = (size_t)(m.end - m.beg);
                size_t ncaps = m.capture_count;
                if (ncaps > 0) {
                    if (blk) {
                        Value *blk_args = arena_alloc(ev->arena, ncaps * sizeof(Value));
                        for (size_t i = 0; i < ncaps; i++)
                            blk_args[i] = (m.cap_beg && m.cap_beg[i] >= 0)
                                ? val_string_n(ev->arena, s + (size_t)m.cap_beg[i], (size_t)(m.cap_end[i] - m.cap_beg[i]))
                                : val_nil();
                        regex_match_free(&m);
                        Value r = call_block(ev, env, *blk, blk_args, (int)ncaps, site);
                        if (ev->errored) { *out = val_nil(); return 1; }
                        if (flow_signal_out(r, out)) return 1;
                    } else {
                        Value cap_arr = val_array_new();
                        for (size_t i = 0; i < ncaps; i++)
                            val_array_push(&cap_arr, (m.cap_beg && m.cap_beg[i] >= 0)
                                ? val_string_n(ev->arena, s + (size_t)m.cap_beg[i], (size_t)(m.cap_end[i] - m.cap_beg[i]))
                                : val_nil());
                        regex_match_free(&m);
                        val_array_push(&arr, cap_arr);
                    }
                } else {
                    Value matched = val_string_n(ev->arena, s + (size_t)m.beg, mlen);
                    regex_match_free(&m);
                    if (blk) {
                        Value r = call_block(ev, env, *blk, &matched, 1, site);
                        if (ev->errored) { *out = val_nil(); return 1; }
                        if (flow_signal_out(r, out)) return 1;
                    } else {
                        val_array_push(&arr, matched);
                    }
                }
                pos = mlen == 0 ? pos + 1 : (size_t)m.end;
            }
            *out = blk ? recv : arr;
            return 1;
        }
        const char *needle = val_to_s(ev->arena, args[0]);
        size_t nlen = strlen(needle);
        Value arr = val_array_new();
        if (nlen == 0) { *out = arr; return 1; }
        const char *p = s;
        while ((p = strstr(p, needle)) != NULL) {
            Value match = val_string_n(ev->arena, p, nlen);
            if (blk) {
                Value r = call_block(ev, env, *blk, &match, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            } else {
                val_array_push(&arr, match);
            }
            p += nlen;
        }
        *out = blk ? recv : arr;
        return 1;
    }
    if (strcmp(name, "match") == 0) {
        if (argc < 1) {
            *out = eval_raise_class(ev, site, "ArgumentError", "String#match requires an argument");
            return 1;
        }
        if (value_is_regexp(args[0])) {
            Value md = regexp_search_value(ev, args[0], recv, 0, site);
            if (ev->errored) { *out = md; return 1; }
            if (blk && md.kind != VAL_NIL) {
                Value r = call_block(ev, env, *blk, &md, 1, site);
                if (ev->errored || val_is_signal(r)) { *out = r; return 1; }
                *out = r;
                return 1;
            }
            *out = md;
            return 1;
        }
    }
    if (strcmp(name, "match?") == 0) {
        if (argc < 1) {
            *out = eval_raise_class(ev, site, "ArgumentError", "String#match? requires an argument");
            return 1;
        }
        if (value_is_regexp(args[0])) {
            Value md = regexp_search_value(ev, args[0], recv, 0, site);
            if (ev->errored) { *out = md; return 1; }
            *out = val_bool(md.kind != VAL_NIL);
            return 1;
        }
        *out = val_bool(strstr(s, val_to_s(ev->arena, args[0])) != NULL);
        return 1;
    }
    if (strcmp(name, "=~") == 0) {
        if (argc < 1) {
            *out = val_nil();
            return 1;
        }
        if (value_is_regexp(args[0])) {
            *out = regexp_search_value(ev, args[0], recv, 1, site);
            return 1;
        }
    }
    if (strcmp(name, "gsub!") == 0 || strcmp(name, "sub!") == 0) {
        /* Delegate to non-bang, return nil if string unchanged */
        const char *nonbang = strcmp(name, "gsub!") == 0 ? "gsub" : "sub";
        Value r = val_nil();
        dispatch_string(ev, env, recv, nonbang, args, argc, blk, site, &r);
        if (val_is_signal(r)) { *out = r; return 1; }
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        return 1;
    }
    if (strcmp(name, "sub") == 0 || strcmp(name, "gsub") == 0) {
        int global = strcmp(name, "gsub") == 0;
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#%s requires an argument", name); return 1; }
        if (value_is_regexp(args[0])) {
            Regex *compiled = args[0].obj->native;
            if (!compiled) {
                Value src;
                RegexError rerr = {0};
                if (!val_object_get_ivar(args[0], "source", &src) || src.kind != VAL_STRING) {
                    *out = eval_raise_class(ev, site, "RuntimeError", "invalid Regexp object");
                    return 1;
                }
                if (regex_compile(ev->arena, src.sval, 0, &compiled, &rerr) != REGEX_OK) {
                    *out = eval_raise_class(ev, site, "RegexpError", "%s",
                                           rerr.message[0] ? rerr.message : "regexp compile failed");
                    return 1;
                }
                args[0].obj->native = compiled;
            }
            size_t slen = strlen(s);
            size_t cap = slen * 2 + 64, used = 0;
            char *buf = malloc(cap);
            if (!buf) { *out = eval_raise_class(ev, site, "RuntimeError", "out of memory"); return 1; }
            size_t pos = 0;
            int replaced = 0;
            while (pos <= slen) {
                RegexMatch m = {0, 0, 0, NULL, NULL};
                if (!global && replaced) {
                    size_t rem = slen - pos;
                    while (used + rem + 1 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); *out = val_nil(); return 1; } buf = nb; }
                    memcpy(buf + used, s + pos, rem);
                    used += rem;
                    break;
                }
                if (regex_search(compiled, s, slen, pos, &m) != REGEX_OK) {
                    size_t rem = slen - pos;
                    while (used + rem + 1 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); *out = val_nil(); return 1; } buf = nb; }
                    memcpy(buf + used, s + pos, rem);
                    used += rem;
                    break;
                }
                size_t pre = (size_t)m.beg - pos;
                while (used + pre + 1 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); regex_match_free(&m); *out = val_nil(); return 1; } buf = nb; }
                memcpy(buf + used, s + pos, pre);
                used += pre;
                size_t mlen = (size_t)(m.end - m.beg);
                const char *repl;
                if (blk) {
                    Value matched = val_string_n(ev->arena, s + (size_t)m.beg, mlen);
                    /* Set $~, $1..$9 from capture groups before calling block */
                    {
                        static const char *cap_keys[] = {"1","2","3","4","5","6","7","8","9"};
                        for (size_t ci = 0; ci < m.capture_count && ci < 9; ci++) {
                            Value cv = (m.cap_beg[ci] >= 0 && m.cap_end[ci] >= m.cap_beg[ci])
                                ? val_string_n(ev->arena, s + (size_t)m.cap_beg[ci],
                                               (size_t)(m.cap_end[ci] - m.cap_beg[ci]))
                                : val_nil();
                            global_set(ev->arena, &ev->globals, cap_keys[ci], cv);
                        }
                        for (size_t ci = m.capture_count; ci < 9; ci++)
                            global_set(ev->arena, &ev->globals, cap_keys[ci], val_nil());
                    }
                    regex_match_free(&m);
                    Value r = call_block(ev, env, *blk, &matched, 1, site);
                    if (ev->errored) { free(buf); *out = val_nil(); return 1; }
                    if (val_is_signal(r)) { free(buf); *out = r; return 1; }
                    repl = val_to_s(ev->arena, r);
                } else {
                    if (argc < 2) { free(buf); regex_match_free(&m); *out = eval_raise_class(ev, site, "ArgumentError", "String#%s requires a replacement or block", name); return 1; }
                    /* Hash replacement: look up matched string in hash */
                    if (args[1].kind == VAL_HASH) {
                        size_t mlen2 = (size_t)(m.end - m.beg);
                        Value mkey = val_string_n(ev->arena, s + m.beg, mlen2);
                        Value hval;
                        const char *hrepl;
                        if (val_hash_get(args[1].hash, mkey, &hval))
                            hrepl = val_to_s(ev->arena, hval);
                        else
                            hrepl = val_string_n(ev->arena, s + m.beg, mlen2).sval;
                        regex_match_free(&m);
                        size_t hrlen = strlen(hrepl);
                        while (used + hrlen + 1 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); *out = val_nil(); return 1; } buf = nb; }
                        memcpy(buf + used, hrepl, hrlen);
                        used += hrlen;
                        replaced = 1;
                        goto regex_gsub_after_replace;
                    }
                    const char *raw = val_to_s(ev->arena, args[1]);
                    /* Expand backreferences: \1..\9, \0/\& (match), \\ (literal \) */
                    {
                        size_t rraw = strlen(raw), rout_cap = rraw * 2 + 64;
                        char *rout = arena_alloc(ev->arena, rout_cap);
                        size_t ri = 0, ro = 0;
                        while (ri < rraw) {
                            if (raw[ri] == '\\' && ri + 1 < rraw) {
                                char next = raw[ri + 1];
                                if (next >= '1' && next <= '9') {
                                    int ci = next - '1';
                                    if (ci < (int)m.capture_count && m.cap_beg[ci] >= 0) {
                                        size_t clen = (size_t)(m.cap_end[ci] - m.cap_beg[ci]);
                                        while (ro + clen + 1 > rout_cap) { rout_cap *= 2; char *nr = arena_alloc(ev->arena, rout_cap); memcpy(nr, rout, ro); rout = nr; }
                                        memcpy(rout + ro, s + m.cap_beg[ci], clen); ro += clen;
                                    }
                                    ri += 2; continue;
                                }
                                if (next == '0' || next == '&') {
                                    size_t mlen2 = (size_t)(m.end - m.beg);
                                    while (ro + mlen2 + 1 > rout_cap) { rout_cap *= 2; char *nr = arena_alloc(ev->arena, rout_cap); memcpy(nr, rout, ro); rout = nr; }
                                    memcpy(rout + ro, s + m.beg, mlen2); ro += mlen2;
                                    ri += 2; continue;
                                }
                                if (next == '\\') { if (ro + 2 > rout_cap) { rout_cap *= 2; char *nr = arena_alloc(ev->arena, rout_cap); memcpy(nr, rout, ro); rout = nr; } rout[ro++] = '\\'; ri += 2; continue; }
                            }
                            if (ro + 2 > rout_cap) { rout_cap *= 2; char *nr = arena_alloc(ev->arena, rout_cap); memcpy(nr, rout, ro); rout = nr; }
                            rout[ro++] = raw[ri++];
                        }
                        rout[ro] = '\0';
                        repl = rout;
                    }
                    regex_match_free(&m);
                }
                size_t rlen = strlen(repl);
                while (used + rlen + 1 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); *out = val_nil(); return 1; } buf = nb; }
                memcpy(buf + used, repl, rlen);
                used += rlen;
                replaced = 1;
                regex_gsub_after_replace:
                if (mlen == 0) {
                    if (pos < slen) {
                        while (used + 2 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); *out = val_nil(); return 1; } buf = nb; }
                        buf[used++] = s[pos];
                    }
                    pos++;
                } else {
                    pos = (size_t)m.end;
                }
            }
            buf[used] = '\0';
            char *result = arena_alloc(ev->arena, used + 1);
            memcpy(result, buf, used + 1);
            free(buf);
            *out = val_string(ev->arena, result);
            return 1;
        }
        const char *needle = val_to_s(ev->arena, args[0]);
        size_t nlen = strlen(needle);
        size_t slen = strlen(s);
        if (nlen == 0) { *out = recv; return 1; }
        /* Build result into a malloc buffer, then copy to arena */
        size_t cap = slen * 2 + 64, used = 0;
        char *buf = malloc(cap);
        if (!buf) { *out = eval_raise_class(ev, site, "RuntimeError", "out of memory"); return 1; }
        const char *p = s;
        int replaced = 0;
        while (*p) {
            if ((!global && replaced) || strncmp(p, needle, nlen) != 0) {
                if (used + 2 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); *out = val_nil(); return 1; } buf = nb; }
                buf[used++] = *p++;
            } else {
                const char *repl;
                if (blk) {
                    Value match = val_string_n(ev->arena, p, nlen);
                    Value r = call_block(ev, env, *blk, &match, 1, site);
                    if (ev->errored) { free(buf); *out = val_nil(); return 1; }
                    if (val_is_signal(r)) { free(buf); *out = r; return 1; }
                    repl = val_to_s(ev->arena, r);
                } else {
                    if (argc < 2) { free(buf); *out = eval_raise_class(ev, site, "ArgumentError", "String#%s requires a replacement or block", name); return 1; }
                    if (args[1].kind == VAL_HASH) {
                        Value mkey = val_string_n(ev->arena, p, nlen);
                        Value hval;
                        repl = val_hash_get(args[1].hash, mkey, &hval) ? val_to_s(ev->arena, hval) : val_string_n(ev->arena, p, nlen).sval;
                    } else {
                        repl = val_to_s(ev->arena, args[1]);
                    }
                }
                size_t rlen = strlen(repl);
                while (used + rlen + 2 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); *out = val_nil(); return 1; } buf = nb; }
                memcpy(buf + used, repl, rlen);
                used += rlen;
                p += nlen;
                replaced = 1;
            }
        }
        buf[used] = '\0';
        char *result = arena_alloc(ev->arena, used + 1);
        memcpy(result, buf, used + 1);
        free(buf);
        *out = val_string(ev->arena, result);
        return 1;
    }
    if (strcmp(name, "*") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "String#* requires an argument");
        else {
            int64_t n = args[0].ival;
            if (n <= 0) *out = val_string(ev->arena, "");
            else {
                size_t slen = strlen(s);
                char *buf = arena_alloc(ev->arena, slen * (size_t)n + 1);
                buf[0] = '\0';
                for (int64_t i = 0; i < n; i++) strcat(buf, s);
                *out = val_string(ev->arena, buf);
            }
        }
        return 1;
    }
    if (strcmp(name, "%") == 0) {
        if (argc < 1) {
            *out = eval_raise_class(ev, site, "ArgumentError", "String#% requires an argument");
            return 1;
        }
        Value *fmt_args = &args[0];
        int fmt_argc = 1;
        if (args[0].kind == VAL_ARRAY) {
            fmt_args = args[0].array->elems;
            fmt_argc = (int)args[0].array->len;
        }
        *out = eval_format_string(ev, env, s, fmt_args, fmt_argc, site);
        return 1;
    }
    return 0;
}

int dispatch_nil(Eval *ev, Value recv, const char *name, Node *site, Value *out) {
    (void)recv;
    if (strcmp(name, "nil?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "to_s") == 0) { *out = val_string(ev->arena, ""); return 1; }
    if (strcmp(name, "inspect") == 0) { *out = val_string(ev->arena, "nil"); return 1; }
    if (strcmp(name, "to_i") == 0 || strcmp(name, "to_int") == 0) { *out = val_int(0); return 1; }
    if (strcmp(name, "to_f") == 0) { *out = val_float(0.0); return 1; }
    if (strcmp(name, "to_a") == 0) { *out = val_array_new(); return 1; }
    if (strcmp(name, "to_h") == 0) { *out = val_hash_new(ev->arena); return 1; }
    if (strcmp(name, "to_r") == 0) { *out = val_int(0); return 1; } /* stub */
    if (strcmp(name, "freeze") == 0 || strcmp(name, "frozen?") == 0 || strcmp(name, "dup") == 0)
        { *out = recv; return 1; }
    if (strcmp(name, "!") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "match?") == 0 || strcmp(name, "match") == 0 || strcmp(name, "=~") == 0)
        { *out = val_nil(); return 1; }
    if (strcmp(name, "byteslice") == 0 || strcmp(name, "[]") == 0)
        { *out = val_nil(); return 1; }
    if (strcmp(name, "empty?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0) { *out = val_int(0); return 1; }
    if (strcmp(name, "chomp") == 0 || strcmp(name, "strip") == 0 || strcmp(name, "chop") == 0)
        { *out = val_string(ev->arena, ""); return 1; }
    if (strcmp(name, "lines") == 0) { *out = val_array_new(); return 1; }
    /* For "method", "respond_to?", "class", "object_id", etc. — let dispatch_method handle them */
    if (strcmp(name, "method") == 0 || strcmp(name, "respond_to?") == 0 ||
        strcmp(name, "class") == 0 || strcmp(name, "object_id") == 0 ||
        strcmp(name, "is_a?") == 0 || strcmp(name, "kind_of?") == 0 ||
        strcmp(name, "instance_of?") == 0 || strcmp(name, "send") == 0 ||
        strcmp(name, "tap") == 0 || strcmp(name, "then") == 0 ||
        strcmp(name, "itself") == 0 || strcmp(name, "equal?") == 0 ||
        strcmp(name, "==" ) == 0 || strcmp(name, "!=" ) == 0)
        return 0;
    *out = eval_raise_class(ev, site, "NoMethodError", "undefined method '%s' for nil", name);
    return 1;
}

int dispatch_bool(Eval *ev, Value recv, const char *name, Node *site, Value *out) {
    (void)site;
    if (recv.kind != VAL_BOOL) return 0;
    if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0) {
        *out = val_string(ev->arena, recv.bval ? "true" : "false");
        return 1;
    }
    if (strcmp(name, "!") == 0) { *out = val_bool(!recv.bval); return 1; }
    if (strcmp(name, "nil?") == 0) { *out = val_false(); return 1; }
    if (strcmp(name, "freeze") == 0 || strcmp(name, "frozen?") == 0) { *out = recv; return 1; }
    if (strcmp(name, "dup") == 0) { *out = recv; return 1; }
    return 0;
}
