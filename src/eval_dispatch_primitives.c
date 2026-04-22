#include "eval_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

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

/* Expand a tr-style pattern (e.g. "a-z", "^aeiou") into a 256-entry bool set.
   Returns 1 if the pattern was negated (^). */
static int tr_expand_set(const char *pat, int set[256]) {
    int negate = 0;
    memset(set, 0, 256 * sizeof(int));
    if (*pat == '^') { negate = 1; pat++; }
    while (*pat) {
        unsigned char c = (unsigned char)*pat;
        if (pat[1] == '-' && pat[2]) {
            unsigned char lo = c, hi = (unsigned char)pat[2];
            if (lo > hi) { unsigned char t = lo; lo = hi; hi = t; }
            for (unsigned int i = lo; i <= (unsigned int)hi; i++) set[i] = 1;
            pat += 3;
        } else {
            set[c] = 1;
            pat++;
        }
    }
    if (negate)
        for (int i = 0; i < 256; i++) set[i] = !set[i];
    return negate;
}

/* Expand a tr-style pattern into a character array. Returns number of chars. */
static size_t tr_expand_chars(const char *pat, char out[256]) {
    size_t n = 0;
    while (*pat && n < 256) {
        unsigned char c = (unsigned char)*pat;
        if (pat[1] == '-' && pat[2]) {
            unsigned char lo = c, hi = (unsigned char)pat[2];
            if (lo > hi) { unsigned char t = lo; lo = hi; hi = t; }
            for (unsigned int i = lo; i <= (unsigned int)hi && n < 256; i++)
                out[n++] = (char)i;
            pat += 3;
        } else {
            out[n++] = (char)c;
            pat++;
        }
    }
    return n;
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
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        int64_t lo = args[0].kind == VAL_INT ? args[0].ival : (int64_t)args[0].fval;
        int64_t hi = args[1].kind == VAL_INT ? args[1].ival : (int64_t)args[1].fval;
        *out = val_int(n < lo ? lo : n > hi ? hi : n);
        return 1;
    }
    if (strcmp(name, "times") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Integer#times requires a block");
        else {
            for (int64_t i = 0; i < n; i++) {
                Value arg = val_int(i);
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "upto") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Integer#upto requires an argument");
        else if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Integer#upto requires a block");
        else {
            for (int64_t i = n; i <= args[0].ival; i++) {
                Value arg = val_int(i);
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "downto") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Integer#downto requires an argument");
        else if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Integer#downto requires a block");
        else {
            for (int64_t i = n; i >= args[0].ival; i--) {
                Value arg = val_int(i);
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "step") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Integer#step requires a limit"); return 1; }
        if (!blk)     { *out = eval_raise_class(ev, site, "LocalJumpError", "Integer#step requires a block"); return 1; }
        double limit = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
        double step  = argc >= 2 ? (args[1].kind == VAL_INT ? (double)args[1].ival : args[1].fval) : 1.0;
        if (step == 0.0) { *out = eval_raise_class(ev, site, "ArgumentError", "step cannot be 0"); return 1; }
        for (double i = (double)n; step > 0 ? i <= limit : i >= limit; i += step) {
            Value arg = val_int((int64_t)i);
            Value r = call_block(ev, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv;
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
    if (strcmp(name, "abs") == 0)    { *out = val_float(f < 0 ? -f : f); return 1; }
    if (strcmp(name, "abs2") == 0)   { *out = val_float(f * f); return 1; }
    if (strcmp(name, "zero?") == 0)  { *out = val_bool(f == 0.0); return 1; }
    if (strcmp(name, "nonzero?") == 0) { *out = f == 0.0 ? val_nil() : recv; return 1; }
    if (strcmp(name, "positive?") == 0) { *out = val_bool(f > 0.0); return 1; }
    if (strcmp(name, "negative?") == 0) { *out = val_bool(f < 0.0); return 1; }
    if (strcmp(name, "integer?") == 0)  { *out = val_false(); return 1; }
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
        double lo = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
        double hi = args[1].kind == VAL_INT ? (double)args[1].ival : args[1].fval;
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
            Value r = call_block(ev, *blk, &arg, 1, site);
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
    if (strcmp(name, "to_i") == 0) { *out = val_int(atoll(s)); return 1; }
    if (strcmp(name, "to_f") == 0) { *out = val_float(atof(s)); return 1; }
    if (strcmp(name, "to_sym") == 0) { *out = val_symbol(s); return 1; }
    if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0) { *out = val_int((int64_t)strlen(s)); return 1; }
    if (strcmp(name, "empty?") == 0) { *out = val_bool(s[0] == '\0'); return 1; }
    if (strcmp(name, "upcase") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len + 1);
        for (size_t i = 0; i <= len; i++) buf[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "downcase") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len + 1);
        for (size_t i = 0; i <= len; i++) buf[i] = (char)(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "strip") == 0) {
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t len = strlen(s);
        while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
        *out = val_string_n(ev->arena, s, len);
        return 1;
    }
    if (strcmp(name, "chars") == 0) {
        Value arr = val_array_new();
        for (size_t i = 0; s[i]; i++) val_array_push(&arr, val_string_n(ev->arena, s + i, 1));
        *out = arr;
        return 1;
    }
    if (strcmp(name, "include?") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "String#include? requires an argument");
        else *out = val_bool(strstr(s, val_to_s(ev->arena, args[0])) != NULL);
        return 1;
    }
    if (strcmp(name, "start_with?") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "String#start_with? requires an argument");
        else {
            const char *needle = val_to_s(ev->arena, args[0]);
            *out = val_bool(strncmp(s, needle, strlen(needle)) == 0);
        }
        return 1;
    }
    if (strcmp(name, "end_with?") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "String#end_with? requires an argument");
        else {
            const char *needle = val_to_s(ev->arena, args[0]);
            size_t slen = strlen(s), nlen = strlen(needle);
            *out = val_bool(slen >= nlen && strcmp(s + slen - nlen, needle) == 0);
        }
        return 1;
    }
    if (strcmp(name, "split") == 0) {
        Value arr = val_array_new();
        const char *sep = argc > 0 ? val_to_s(ev->arena, args[0]) : " ";
        size_t seplen = strlen(sep);
        if (seplen == 0) {
            for (size_t i = 0; s[i]; i++) val_array_push(&arr, val_string_n(ev->arena, s + i, 1));
        } else {
            const char *p = s, *found;
            while ((found = strstr(p, sep)) != NULL) {
                val_array_push(&arr, val_string_n(ev->arena, p, (size_t)(found - p)));
                p = found + seplen;
            }
            val_array_push(&arr, val_string(ev->arena, p));
        }
        *out = arr;
        return 1;
    }
    if (strcmp(name, "each_char") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "String#each_char requires a block");
        else {
            for (size_t i = 0; s[i]; i++) {
                Value ch = val_string_n(ev->arena, s + i, 1);
                Value r = call_block(ev, *blk, &ch, 1, site);
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
        for (size_t i = 0; i < len; i++) buf[i] = s[len - 1 - i];
        buf[len] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "next") == 0 || strcmp(name, "succ") == 0) {
        size_t len = strlen(s);
        if (len == 0) { *out = val_string(ev->arena, ""); return 1; }
        int has_alnum = 0;
        for (size_t i = 0; i < len && !has_alnum; i++) has_alnum = isalnum((unsigned char)s[i]);
        char *buf = arena_alloc(ev->arena, len + 2);
        memcpy(buf, s, len + 1);
        if (!has_alnum) {
            buf[len - 1]++;
            *out = val_string(ev->arena, buf);
            return 1;
        }
        char prepend = '\0';
        for (int i = (int)len - 1; i >= 0; i--) {
            unsigned char c = (unsigned char)buf[i];
            if (!isalnum(c)) continue;
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
        if (len > 0 && s[len - 1] == '\n') {
            len--;
            if (len > 0 && s[len - 1] == '\r') len--;
        } else if (len > 0 && s[len - 1] == '\r') {
            len--;
        }
        *out = val_string_n(ev->arena, s, len);
        return 1;
    }
    if (strcmp(name, "chop") == 0) {
        size_t len = strlen(s);
        if (len == 0) { *out = val_string(ev->arena, ""); return 1; }
        if (len >= 2 && s[len - 2] == '\r' && s[len - 1] == '\n') len -= 2;
        else len--;
        *out = val_string_n(ev->arena, s, len);
        return 1;
    }
    if (strcmp(name, "lstrip") == 0) {
        const char *p = s;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        *out = val_string(ev->arena, p);
        return 1;
    }
    if (strcmp(name, "rstrip") == 0) {
        size_t len = strlen(s);
        while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                            s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
        *out = val_string_n(ev->arena, s, len);
        return 1;
    }
    if (strcmp(name, "capitalize") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len + 1);
        for (size_t i = 0; i < len; i++)
            buf[i] = (char)(i == 0 ? toupper((unsigned char)s[i]) : tolower((unsigned char)s[i]));
        buf[len] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "swapcase") == 0) {
        size_t len = strlen(s);
        char *buf = arena_alloc(ev->arena, len + 1);
        for (size_t i = 0; i < len; i++) {
            unsigned char c = (unsigned char)s[i];
            buf[i] = (char)(isupper(c) ? tolower(c) : islower(c) ? toupper(c) : c);
        }
        buf[len] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "ljust") == 0 || strcmp(name, "rjust") == 0 || strcmp(name, "center") == 0) {
        if (argc < 1) { *out = recv; return 1; }
        int64_t width = args[0].kind == VAL_INT ? args[0].ival : 0;
        const char *pad = argc >= 2 && args[1].kind == VAL_STRING ? args[1].sval : " ";
        size_t padlen = strlen(pad);
        if (padlen == 0) padlen = 1;
        size_t slen = strlen(s);
        if ((int64_t)slen >= width) { *out = recv; return 1; }
        size_t total = (size_t)width;
        char *buf = arena_alloc(ev->arena, total + 1);
        size_t lpad = 0, rpad = 0;
        if (strcmp(name, "ljust") == 0) { lpad = 0; rpad = total - slen; }
        else if (strcmp(name, "rjust") == 0) { lpad = total - slen; rpad = 0; }
        else { lpad = (total - slen) / 2; rpad = total - slen - lpad; }
        for (size_t i = 0; i < lpad; i++) buf[i] = pad[i % padlen];
        memcpy(buf + lpad, s, slen);
        for (size_t i = 0; i < rpad; i++) buf[lpad + slen + i] = pad[i % padlen];
        buf[total] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "ord") == 0) {
        if (s[0] == '\0') { *out = eval_raise_class(ev, site, "ArgumentError", "empty string"); return 1; }
        *out = val_int((int64_t)(unsigned char)s[0]);
        return 1;
    }
    if (strcmp(name, "hex") == 0) {
        *out = val_int((int64_t)strtoll(s, NULL, 16));
        return 1;
    }
    if (strcmp(name, "oct") == 0) {
        *out = val_int((int64_t)strtoll(s, NULL, 0));
        return 1;
    }
    if (strcmp(name, "bytes") == 0) {
        Value arr = val_array_new();
        for (size_t i = 0; s[i]; i++)
            val_array_push(&arr, val_int((int64_t)(unsigned char)s[i]));
        *out = arr;
        return 1;
    }
    if (strcmp(name, "<<") == 0) {
        if (argc < 1) { *out = recv; return 1; }
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
        size_t offset = (argc >= 2 && args[1].kind == VAL_INT) ? (size_t)args[1].ival : 0;
        if (offset > strlen(s)) { *out = val_nil(); return 1; }
        const char *found = strstr(s + offset, needle);
        *out = found ? val_int((int64_t)(found - s)) : val_nil();
        return 1;
    }
    if (strcmp(name, "rindex") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#rindex requires an argument"); return 1; }
        const char *needle = val_to_s(ev->arena, args[0]);
        size_t nlen = strlen(needle), slen = strlen(s);
        size_t limit = (argc >= 2 && args[1].kind == VAL_INT) ? (size_t)args[1].ival : slen;
        if (limit > slen) limit = slen;
        const char *last = NULL;
        for (size_t i = 0; i + nlen <= limit + 1 && i <= limit; i++) {
            if (strncmp(s + i, needle, nlen) == 0) last = s + i;
        }
        *out = last ? val_int((int64_t)(last - s)) : val_nil();
        return 1;
    }
    if (strcmp(name, "[]") == 0 || strcmp(name, "slice") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#[] requires an argument"); return 1; }
        size_t slen = strlen(s);
        int64_t idx = args[0].kind == VAL_INT ? args[0].ival : 0;
        if (idx < 0) idx += (int64_t)slen;
        if (idx < 0 || (size_t)idx >= slen) { *out = val_nil(); return 1; }
        if (argc >= 2 && args[1].kind == VAL_INT) {
            int64_t len = args[1].ival;
            if (len < 0) { *out = val_nil(); return 1; }
            size_t take = (size_t)idx + (size_t)len > slen ? slen - (size_t)idx : (size_t)len;
            *out = val_string_n(ev->arena, s + idx, take);
        } else {
            *out = val_string_n(ev->arena, s + idx, 1);
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
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "String#each_line requires a block"); return 1; }
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
            Value r = call_block(ev, *blk, &line, 1, site);
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
        char from_chars[256], to_chars[256];
        size_t from_len = tr_expand_chars(from_pat, from_chars);
        size_t to_len   = tr_expand_chars(to_pat,   to_chars);
        if (from_len == 0 || to_len == 0) { *out = recv; return 1; }
        char map[256];
        for (int i = 0; i < 256; i++) map[i] = (char)i;
        for (size_t i = 0; i < from_len; i++) {
            unsigned char fc = (unsigned char)from_chars[i];
            map[fc] = to_chars[i < to_len ? i : to_len - 1];
        }
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen + 1);
        for (size_t i = 0; i < slen; i++) buf[i] = map[(unsigned char)s[i]];
        buf[slen] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "count") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#count requires an argument"); return 1; }
        int set[256];
        tr_expand_set(val_to_s(ev->arena, args[0]), set);
        int64_t n = 0;
        for (size_t i = 0; s[i]; i++) if (set[(unsigned char)s[i]]) n++;
        *out = val_int(n);
        return 1;
    }
    if (strcmp(name, "delete") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#delete requires an argument"); return 1; }
        int set[256];
        tr_expand_set(val_to_s(ev->arena, args[0]), set);
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen + 1);
        size_t j = 0;
        for (size_t i = 0; i < slen; i++) if (!set[(unsigned char)s[i]]) buf[j++] = s[i];
        buf[j] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "squeeze") == 0) {
        int set[256];
        if (argc >= 1) tr_expand_set(val_to_s(ev->arena, args[0]), set);
        else           for (int i = 0; i < 256; i++) set[i] = 1;
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen + 1);
        size_t j = 0;
        for (size_t i = 0; i < slen; i++) {
            if (j > 0 && buf[j - 1] == s[i] && set[(unsigned char)s[i]]) continue;
            buf[j++] = s[i];
        }
        buf[j] = '\0';
        *out = val_string(ev->arena, buf);
        return 1;
    }
    if (strcmp(name, "scan") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#scan requires an argument"); return 1; }
        const char *needle = val_to_s(ev->arena, args[0]);
        size_t nlen = strlen(needle);
        Value arr = val_array_new();
        if (nlen == 0) { *out = arr; return 1; }
        const char *p = s;
        while ((p = strstr(p, needle)) != NULL) {
            Value match = val_string_n(ev->arena, p, nlen);
            if (blk) {
                Value r = call_block(ev, *blk, &match, 1, site);
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
    if (strcmp(name, "sub") == 0 || strcmp(name, "gsub") == 0) {
        int global = strcmp(name, "gsub") == 0;
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#%s requires an argument", name); return 1; }
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
                    Value r = call_block(ev, *blk, &match, 1, site);
                    if (ev->errored) { free(buf); *out = val_nil(); return 1; }
                    if (val_is_signal(r)) { free(buf); *out = r; return 1; }
                    repl = val_to_s(ev->arena, r);
                } else {
                    if (argc < 2) { free(buf); *out = eval_raise_class(ev, site, "ArgumentError", "String#%s requires a replacement or block", name); return 1; }
                    repl = val_to_s(ev->arena, args[1]);
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
    return 0;
}

int dispatch_nil(Eval *ev, Value recv, const char *name, Node *site, Value *out) {
    (void)recv;
    if (strcmp(name, "nil?") == 0 || strcmp(name, "to_s") == 0) { *out = val_nil(); return 1; }
    if (strcmp(name, "inspect") == 0) { *out = val_string(ev->arena, "nil"); return 1; }
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
    return 0;
}
