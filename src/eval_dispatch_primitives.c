#include "eval_internal.h"
#include "utf8.h"

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Wrap arr in Enumerator.new(arr); falls back to arr if class not found. */
static Value wrap_as_enumerator(Eval *ev, Env *env, Value arr, Node *site) {
    Value enum_class;
    if (env_get(ev->top_env, "Enumerator", &enum_class) && enum_class.kind == VAL_CLASS) {
        Value r = dispatch_method(ev, env, enum_class, "new", &arr, 1, NULL, site, 0, 1);
        if (!val_is_signal(r)) return r;
        ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
    }
    return arr;
}

static void append_utf8_pad(char *buf, size_t *pos, const char *pad, size_t pad_byte_len, size_t count) {
    size_t pad_chars = utf8_char_count(pad, pad_byte_len);
    if (pad_chars == 0) return;
    for (size_t i = 0; i < count; i++) {
        const char *ptr = NULL;
        size_t width = 0;
        utf8_char_at(pad, pad_byte_len, i % pad_chars, &ptr, &width, NULL);
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

static Value raise_float_domain_error(Eval *ev, Node *site, double f, int unsigned_infinity_message) {
    const char *msg = "NaN";
    if (isinf(f))
        msg = (unsigned_infinity_message || !signbit(f)) ? "Infinity" : "-Infinity";
    return eval_raise_class(ev, site, "FloatDomainError", "%s", msg);
}

static Value build_rational_value(Eval *ev, Env *env, int64_t numer, int64_t denom, Node *site) {
    Value rat_class;
    if (env_get(ev->top_env, "Rational", &rat_class) && rat_class.kind == VAL_CLASS) {
        Value rargs[2] = {val_int(numer), val_int(denom)};
        return dispatch_method(ev, env, rat_class, "new", rargs, 2, NULL, site, 0, 1);
    }
    if (denom == 1) return val_int(numer);
    return val_float((double)numer / (double)denom);
}

static Value build_decimal_rational_value(Eval *ev, Env *env, double f, Node *site) {
    Value rat_class;
    if (env_get(ev->top_env, "Rational", &rat_class) && rat_class.kind == VAL_CLASS) {
        int64_t denom = 10000000LL;
        int64_t numer = (int64_t)round(f * (double)denom);
        Value rargs[2] = {val_int(numer), val_int(denom)};
        return dispatch_method(ev, env, rat_class, "new", rargs, 2, NULL, site, 0, 1);
    }
    return val_float(f);
}

static int encoding_name_is_ascii_8bit(const char *name) {
    return name && (strcasecmp(name, "ASCII-8BIT") == 0 ||
                    strcasecmp(name, "ASCII_8BIT") == 0 ||
                    strcasecmp(name, "BINARY") == 0);
}

static int encoding_name_is_utf8(const char *name) {
    return name && (strcasecmp(name, "UTF-8") == 0 ||
                    strcasecmp(name, "UTF_8") == 0 ||
                    strcasecmp(name, "UTF8") == 0);
}

static Value string_encoding_object(Eval *ev, Value str) {
    Value enc_class = val_nil();
    if (env_get(ev->top_env, "Encoding", &enc_class) && enc_class.kind == VAL_CLASS) {
        Value enc = val_nil();
        const char *key = str.string_encoding == STRING_ENCODING_ASCII_8BIT ? "ASCII_8BIT" : "UTF_8";
        if (env_get(enc_class.klass->class_env, key, &enc) && enc.kind != VAL_NIL)
            return enc;
    }
    return val_string(ev->arena, str.string_encoding == STRING_ENCODING_ASCII_8BIT ? "ASCII-8BIT" : "UTF-8");
}

static StringEncodingTag string_encoding_from_value(Value enc) {
    const char *name = NULL;
    if (enc.kind == VAL_STRING || enc.kind == VAL_SYMBOL) {
        name = enc.sval;
    } else if (enc.kind == VAL_OBJECT && enc.obj && enc.obj->klass.kind == VAL_CLASS &&
               strcmp(enc.obj->klass.klass->name, "Encoding") == 0) {
        Value iv = val_nil();
        if (val_object_get_ivar(enc, "name", &iv) && iv.kind == VAL_STRING)
            name = iv.sval;
    }
    if (encoding_name_is_ascii_8bit(name))
        return STRING_ENCODING_ASCII_8BIT;
    if (encoding_name_is_utf8(name))
        return STRING_ENCODING_UTF8;
    return STRING_ENCODING_UTF8;
}

static Value float_to_exact_rational(Eval *ev, Env *env, double f, Node *site) {
    if (isnan(f) || isinf(f))
        return raise_float_domain_error(ev, site, f, 0);
    if (f == 0.0)
        return build_rational_value(ev, env, 0, 1, site);

    double mag = fabs(f);
    int exp = 0;
    double frac = frexp(mag, &exp);
    uint64_t mant = (uint64_t)llround(ldexp(frac, DBL_MANT_DIG));
    int shift = exp - DBL_MANT_DIG;

    while (shift < 0 && (mant & 1ULL) == 0) {
        mant >>= 1;
        shift++;
    }

    if (mant == 0)
        return build_rational_value(ev, env, 0, 1, site);

    if (shift >= 0) {
        if (shift >= 63 || mant > ((uint64_t)INT64_MAX >> shift))
            return build_decimal_rational_value(ev, env, f, site);
        int64_t numer = (int64_t)(mant << shift);
        if (signbit(f)) numer = -numer;
        return build_rational_value(ev, env, numer, 1, site);
    }

    int denom_shift = -shift;
    if (denom_shift >= 63 || mant > (uint64_t)INT64_MAX)
        return build_decimal_rational_value(ev, env, f, site);
    int64_t numer = (int64_t)mant;
    int64_t denom = (int64_t)1 << denom_shift;
    if (signbit(f)) numer = -numer;
    return build_rational_value(ev, env, numer, denom, site);
}

static int coerce_value_to_double(Eval *ev, Env *env, Value v, Node *site, double *out, Value *err) {
    if (v.kind == VAL_FLOAT) {
        *out = v.fval;
        return 1;
    }
    if (v.kind == VAL_INT) {
        *out = (double)v.ival;
        return 1;
    }
    Value converted = dispatch_method(ev, env, v, "to_f", NULL, 0, NULL, site, 0, -1);
    if (val_is_signal(converted)) {
        *err = converted;
        return 0;
    }
    if (converted.kind == VAL_FLOAT) {
        *out = converted.fval;
        return 1;
    }
    if (converted.kind == VAL_INT) {
        *out = (double)converted.ival;
        return 1;
    }
    *err = eval_raise_class(ev, site, "TypeError", "can't convert %s into Float", val_kind_name(v.kind));
    return 0;
}

static Value float_rationalize(Eval *ev, Env *env, double f, double eps, Node *site) {
    if (isnan(f) || isinf(f))
        return raise_float_domain_error(ev, site, f, 1);

    double target = fabs(f);
    if (target == 0.0 || eps == 0.0)
        return float_to_exact_rational(ev, env, f, site);
    if (eps < 0.0) eps = -eps;

    uint64_t h0 = 0, h1 = 1;
    uint64_t k0 = 1, k1 = 0;
    double x = target;

    for (int iter = 0; iter < 64; iter++) {
        double a_d = floor(x);
        if (!(a_d >= 0.0) || a_d > (double)INT64_MAX)
            break;
        uint64_t a = (uint64_t)a_d;
        if (h1 > (uint64_t)INT64_MAX || k1 > (uint64_t)INT64_MAX)
            break;
        if (a != 0) {
            if (h1 > (((uint64_t)INT64_MAX) - h0) / a) break;
            if (k1 > (((uint64_t)INT64_MAX) - k0) / a) break;
        }
        uint64_t h = a * h1 + h0;
        uint64_t k = a * k1 + k0;
        if (h > (uint64_t)INT64_MAX || k > (uint64_t)INT64_MAX || k == 0)
            break;

        double approx = (double)h / (double)k;
        if (fabs(target - approx) <= eps) {
            int64_t numer = (int64_t)h;
            if (signbit(f)) numer = -numer;
            return build_rational_value(ev, env, numer, (int64_t)k, site);
        }

        double rem = x - a_d;
        if (fabs(rem) < DBL_EPSILON) {
            int64_t numer = (int64_t)h;
            if (signbit(f)) numer = -numer;
            return build_rational_value(ev, env, numer, (int64_t)k, site);
        }

        h0 = h1; h1 = h;
        k0 = k1; k1 = k;
        x = 1.0 / rem;
    }

    return float_to_exact_rational(ev, env, f, site);
}

typedef struct {
    uint32_t lo;
    uint32_t hi;
} RuneRange;

static size_t rune_pattern_ranges(const char *pat, RuneRange *ranges, size_t cap) {
    size_t count = 0;
    size_t i = 0;
    size_t len = strlen(pat);
    if (len > 0 && pat[0] == '^') i = 1; /* skip '^' — negate handled by caller */
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
    if (strcmp(name, "to_r") == 0) {
        Value rat_class;
        if (env_get(ev->top_env, "Rational", &rat_class) && rat_class.kind == VAL_CLASS)
            *out = dispatch_method(ev, env, rat_class, "new", &recv, 1, NULL, site, 0, 1);
        else
            *out = recv;
        return 1;
    }
    if (argc == 1) {
        Value r = args[0];
        /* Coerce: Integer op Complex → Complex(n,0) op complex */
        if ((strcmp(name, "+") == 0 || strcmp(name, "-") == 0 ||
             strcmp(name, "*") == 0 || strcmp(name, "/") == 0) &&
            r.kind == VAL_OBJECT && r.obj->klass.kind == VAL_CLASS &&
            r.obj->klass.klass && strcmp(r.obj->klass.klass->name, "Complex") == 0) {
            Value cplx_class;
            if (env_get(ev->top_env, "Complex", &cplx_class) && cplx_class.kind == VAL_CLASS) {
                Value cargs[2] = {recv, val_int(0)};
                Value self_cplx = dispatch_method(ev, env, cplx_class, "new", cargs, 2, NULL, site, 0, 1);
                if (!val_is_signal(self_cplx)) {
                    *out = dispatch_method(ev, env, self_cplx, name, &r, 1, NULL, site, 0, 1);
                    return 1;
                }
            }
        }
        /* Coerce: Integer op Rational → Rational(n,1) op rational */
        if ((strcmp(name, "+") == 0 || strcmp(name, "-") == 0 ||
             strcmp(name, "*") == 0 || strcmp(name, "/") == 0) &&
            r.kind == VAL_OBJECT && r.obj->klass.kind == VAL_CLASS &&
            r.obj->klass.klass && strcmp(r.obj->klass.klass->name, "Rational") == 0) {
            Value rat_class;
            if (env_get(ev->top_env, "Rational", &rat_class) && rat_class.kind == VAL_CLASS) {
                Value self_rat = dispatch_method(ev, env, rat_class, "new", &recv, 1, NULL, site, 0, 1);
                if (!val_is_signal(self_rat)) {
                    *out = dispatch_method(ev, env, self_rat, name, &r, 1, NULL, site, 0, 1);
                    return 1;
                }
            }
        }
        int both_int = (r.kind == VAL_INT);
        double lf = (double)n, rf = both_int ? (double)r.ival : (r.kind == VAL_FLOAT ? r.fval : 0.0);
        if (strcmp(name, "+") == 0) { *out = both_int ? val_int(n + r.ival) : val_float(lf + rf); return 1; }
        if (strcmp(name, "-") == 0) { *out = both_int ? val_int(n - r.ival) : val_float(lf - rf); return 1; }
        if (strcmp(name, "*") == 0) { *out = both_int ? val_int(n * r.ival) : val_float(lf * rf); return 1; }
        if (strcmp(name, "/") == 0) {
            if (both_int) {
                if (r.ival == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
                /* Ruby floor division */
                int64_t q = n / r.ival, rem = n % r.ival;
                if (rem != 0 && ((rem ^ r.ival) < 0)) q--;
                *out = val_int(q); return 1;
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
            if (both_int && r.ival < 0) {
                /* Integer ** negative_integer → Rational(1, base^abs_exp) per Ruby 3+ */
                Value rat_class;
                if (env_get(ev->top_env, "Rational", &rat_class) && rat_class.kind == VAL_CLASS) {
                    double denom = pow(lf, -rf);
                    Value rat_args[2] = { val_int(1), val_int((int64_t)denom) };
                    *out = dispatch_method(ev, env, rat_class, "new", rat_args, 2, NULL, site, 0, 1);
                    return 1;
                }
            }
            *out = (both_int && r.ival >= 0) ? val_int((int64_t)pow(lf, rf)) : val_float(pow(lf, rf));
            return 1;
        }
        if (strcmp(name, "<")   == 0) { *out = val_bool(lf <  rf); return 1; }
        if (strcmp(name, "<=")  == 0) { *out = val_bool(lf <= rf); return 1; }
        if (strcmp(name, ">")   == 0) { *out = val_bool(lf >  rf); return 1; }
        if (strcmp(name, ">=")  == 0) { *out = val_bool(lf >= rf); return 1; }
        if (strcmp(name, "<=>") == 0) {
            /* nil for incomparable types */
            if (r.kind != VAL_INT && r.kind != VAL_FLOAT) { *out = val_nil(); return 1; }
            *out = val_int(lf < rf ? -1 : lf > rf ? 1 : 0); return 1;
        }
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
        strcmp(name, "round") == 0 || strcmp(name, "truncate") == 0) {
        /* With negative precision: round to nearest 10^|precision| */
        if (argc >= 1 && args[0].kind == VAL_INT && args[0].ival < 0) {
            int64_t factor = 1;
            for (int64_t i = 0; i < -args[0].ival; i++) factor *= 10;
            int64_t q = n / factor;
            int64_t r = n % factor;
            if (strcmp(name, "ceil") == 0)
                *out = val_int(r == 0 ? n : (n > 0 ? (q + 1) * factor : q * factor));
            else if (strcmp(name, "floor") == 0)
                *out = val_int(r == 0 ? n : (n > 0 ? q * factor : (q - 1) * factor));
            else if (strcmp(name, "round") == 0) {
                int64_t lo = (n > 0 ? q : q - 1) * factor;
                int64_t hi = lo + factor;
                int64_t mid = lo + factor / 2;
                *out = val_int((n < mid) ? lo : hi);
            } else { /* truncate */
                *out = val_int(q * factor);
            }
            return 1;
        }
        /* With non-negative precision or no arg: integer returns self */
        if (argc >= 1 && args[0].kind == VAL_INT && args[0].ival >= 0)
            *out = recv;
        else
            *out = recv;
        return 1;
    }
    if (strcmp(name, "succ") == 0 || strcmp(name, "next") == 0) { *out = val_int(n + 1); return 1; }
    if (strcmp(name, "pred") == 0) { *out = val_int(n - 1); return 1; }
    if (strcmp(name, "infinite?") == 0) { *out = val_nil(); return 1; }
    if (strcmp(name, "finite?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "nan?") == 0) { *out = val_false(); return 1; }
    if (strcmp(name, "real?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "integer?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "real") == 0) { *out = recv; return 1; }
    if (strcmp(name, "imaginary") == 0) { *out = val_int(0); return 1; }
    if (strcmp(name, "chr") == 0) {
        if (n < 0 || n > 0x10FFFF) { *out = eval_raise_class(ev, site, "RangeError", "%lld out of char range", (long long)n); return 1; }
        if (n <= 127) {
            char buf[2] = { (char)n, '\0' };
            *out = val_string(ev->arena, buf);
        } else {
            /* UTF-8 encode the codepoint */
            char buf[5]; int len = 0;
            if (n < 0x80)       { buf[0] = (char)n; len = 1; }
            else if (n < 0x800) { buf[0] = (char)(0xC0|(n>>6)); buf[1] = (char)(0x80|(n&0x3F)); len = 2; }
            else if (n < 0x10000) { buf[0] = (char)(0xE0|(n>>12)); buf[1] = (char)(0x80|((n>>6)&0x3F)); buf[2] = (char)(0x80|(n&0x3F)); len = 3; }
            else { buf[0] = (char)(0xF0|(n>>18)); buf[1] = (char)(0x80|((n>>12)&0x3F)); buf[2] = (char)(0x80|((n>>6)&0x3F)); buf[3] = (char)(0x80|(n&0x3F)); len = 4; }
            buf[len] = '\0';
            *out = val_string(ev->arena, buf);
        }
        return 1;
    }
    if (strcmp(name, "ord") == 0) { *out = recv; return 1; }
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
    if (strcmp(name, "div") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        int64_t b = args[0].kind == VAL_INT ? args[0].ival : (int64_t)args[0].fval;
        if (b == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
        /* floor division */
        int64_t q = n / b;
        if ((n ^ b) < 0 && q * b != n) q--;
        *out = val_int(q); return 1;
    }
    if (strcmp(name, "modulo") == 0 || strcmp(name, "%") == 0 || strcmp(name, "mod") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        int64_t b = args[0].kind == VAL_INT ? args[0].ival : (int64_t)args[0].fval;
        if (b == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
        int64_t r = n % b;
        if (r != 0 && (r ^ b) < 0) r += b;  /* floor modulo */
        *out = val_int(r); return 1;
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
        Value rat_class;
        if (env_get(ev->top_env, "Rational", &rat_class) && rat_class.kind == VAL_CLASS) {
            *out = dispatch_method(ev, env, rat_class, "new", &recv, 1, NULL, site, 0, 1);
        } else {
            *out = recv; /* fallback before prelude */
        }
        return 1;
    }
    if (strcmp(name, "to_c") == 0) {
        Value cplx_class;
        if (env_get(ev->top_env, "Complex", &cplx_class) && cplx_class.kind == VAL_CLASS) {
            Value cargs[2] = {recv, val_int(0)};
            *out = dispatch_method(ev, env, cplx_class, "new", cargs, 2, NULL, site, 0, 1);
        } else {
            char buf[32]; snprintf(buf, sizeof(buf), "(%lld+0i)", (long long)n);
            *out = val_string(ev->arena, buf);
        }
        return 1;
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
    if (strcmp(name, "bit_length") == 0) {
        uint64_t v = (uint64_t)(n < 0 ? ~n : n);
        *out = val_int(v == 0 ? 0 : 64 - __builtin_clzll(v));
        return 1;
    }
    if (strcmp(name, "ceildiv") == 0) {
        if (argc < 1 || args[0].kind != VAL_INT) { *out = eval_raise_class(ev, site, "TypeError", "Integer#ceildiv requires an Integer"); return 1; }
        int64_t b = args[0].ival;
        if (b == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
        /* ceil(a/b) = -((-a) / b) in Ruby (floor div on negated value) */
        int64_t q = n / b, r = n % b;
        if (r != 0 && ((r ^ b) > 0)) q++;  /* round up if same sign as divisor */
        *out = val_int(q);
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
            *out = wrap_as_enumerator(ev, env, arr, site);
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
            *out = wrap_as_enumerator(ev, env, arr, site);
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
            *out = wrap_as_enumerator(ev, env, arr, site);
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
        int limit_is_float = (args[0].kind == VAL_FLOAT);
        int step_is_float  = (argc >= 2 && args[1].kind == VAL_FLOAT);
        int use_float = limit_is_float || step_is_float;
        double limit = limit_is_float ? args[0].fval : (double)args[0].ival;
        double step  = argc >= 2 ? (step_is_float ? args[1].fval : (double)args[1].ival) : 1.0;
        if (step == 0.0) { *out = eval_raise_class(ev, site, "ArgumentError", "step cannot be 0"); return 1; }
        if (!blk) {
            Value arr = val_array_new();
            for (double i = (double)n; step > 0 ? i <= limit + 1e-10 : i >= limit - 1e-10; i += step)
                val_array_push(&arr, use_float ? val_float(i) : val_int((int64_t)i));
            *out = wrap_as_enumerator(ev, env, arr, site);
        } else {
            for (double i = (double)n; step > 0 ? i <= limit + 1e-10 : i >= limit - 1e-10; i += step) {
                Value arg = use_float ? val_float(i) : val_int((int64_t)i);
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
    if (strcmp(name, "to_r") == 0) {
        *out = float_to_exact_rational(ev, env, recv.fval, site);
        return 1;
    }
    if (strcmp(name, "rationalize") == 0) {
        if (argc > 1) {
            *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
            return 1;
        }
        double eps = 0.0;
        if (argc == 1) {
            Value err = val_nil();
            if (!coerce_value_to_double(ev, env, args[0], site, &eps, &err)) {
                *out = err;
                return 1;
            }
        } else {
            double mag = fabs(f);
            eps = (nextafter(mag, INFINITY) - mag) / 2.0;
        }
        *out = float_rationalize(ev, env, recv.fval, eps, site);
        return 1;
    }
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
    if (strcmp(name, "real?") == 0)     { *out = val_true(); return 1; }
    if (strcmp(name, "integer?") == 0)  { *out = val_false(); return 1; }
    if (strcmp(name, "real") == 0)      { *out = recv; return 1; }
    if (strcmp(name, "imaginary") == 0) { *out = val_int(0); return 1; }
    if (strcmp(name, "to_c") == 0) {
        Value cplx_class;
        if (!env_get(ev->top_env, "Complex", &cplx_class) || cplx_class.kind != VAL_CLASS)
            { *out = val_nil(); return 1; }
        Value cargs[2] = { recv, val_int(0) };
        *out = dispatch_method(ev, env, cplx_class, "new", cargs, 2, NULL, site, 0, 1);
        return 1;
    }
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
    if (strcmp(name, "to_s") == 0 || strcmp(name, "to_str") == 0) { *out = recv; return 1; }
    if (strcmp(name, "dump") == 0) {
        /* Like inspect but always double-quoted */
        *out = val_string(ev->arena, val_inspect(ev->arena, recv));
        return 1;
    }
    if (strcmp(name, "undump") == 0) {
        /* Inverse of dump/inspect: parse escaped string literal */
        size_t slen = strlen(s), si = 0;
        if (slen >= 2 && s[0] == '"') si = 1;  /* skip opening quote */
        size_t end = slen;
        if (slen >= 2 && s[slen-1] == '"') end = slen - 1;
        char *buf = arena_alloc(ev->arena, slen + 1);
        size_t bi = 0;
        while (si < end) {
            if (s[si] == '\\' && si + 1 < end) {
                si++;
                switch (s[si]) {
                    case 'n': buf[bi++] = '\n'; break;
                    case 't': buf[bi++] = '\t'; break;
                    case 'r': buf[bi++] = '\r'; break;
                    case '"': buf[bi++] = '"';  break;
                    case '\'': buf[bi++] = '\''; break;
                    case '\\': buf[bi++] = '\\'; break;
                    case 'a': buf[bi++] = '\a'; break;
                    case 'b': buf[bi++] = '\b'; break;
                    case 'e': buf[bi++] = '\x1b'; break;
                    case 'x': {
                        /* hex escape \xNN */
                        char h[3] = {0};
                        if (si+1 < end && isxdigit((unsigned char)s[si+1])) { h[0] = s[++si]; }
                        if (si+1 < end && isxdigit((unsigned char)s[si+1])) { h[1] = s[++si]; }
                        buf[bi++] = (char)strtol(h, NULL, 16);
                        break;
                    }
                    default: buf[bi++] = '\\'; buf[bi++] = s[si]; break;
                }
            } else {
                buf[bi++] = s[si];
            }
            si++;
        }
        buf[bi] = '\0';
        *out = val_string_n(ev->arena, buf, bi);
        return 1;
    }
    if (strcmp(name, "<=>") == 0) {
        if (argc < 1 || args[0].kind != VAL_STRING) { *out = val_nil(); return 1; }
        int c = strcmp(s, args[0].sval ? args[0].sval : "");
        *out = val_int(c < 0 ? -1 : c > 0 ? 1 : 0); return 1;
    }
    if (strcmp(name, "encoding") == 0) { *out = string_encoding_object(ev, recv); return 1; }
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
        size_t slen = recv.byte_len;
        for (size_t i = 0; i < slen; i++) {
            if ((unsigned char)s[i] > 127) { *out = val_false(); return 1; }
        }
        *out = val_true(); return 1;
    }
    if (strcmp(name, "bytesize") == 0) { *out = val_int((int64_t)recv.byte_len); return 1; }
    if (strcmp(name, "byteslice") == 0) {
        size_t slen = recv.byte_len;
        if (argc < 1) { *out = val_nil(); return 1; }
        if (args[0].kind == VAL_RANGE) {
            RubyRange *r = args[0].range;
            int64_t beg = r->begin_val.kind == VAL_INT ? r->begin_val.ival : 0;
            int64_t end = r->end_val.kind == VAL_INT ? r->end_val.ival : (int64_t)slen - 1;
            if (beg < 0) beg += (int64_t)slen;
            if (end < 0) end += (int64_t)slen;
            if (r->exclusive) end--;
            if (beg < 0 || beg > (int64_t)slen) { *out = val_nil(); return 1; }
            if (end >= (int64_t)slen) end = (int64_t)slen - 1;
            int64_t len = end - beg + 1;
            if (len < 0) len = 0;
            *out = val_string_n(ev->arena, s + beg, (size_t)len);
            out->string_encoding = recv.string_encoding;
            return 1;
        }
        int64_t start = args[0].kind == VAL_INT ? args[0].ival : 0;
        if (start < 0) start += (int64_t)slen;
        if (start < 0 || start > (int64_t)slen) { *out = val_nil(); return 1; }
        int64_t len = argc >= 2 && args[1].kind == VAL_INT ? args[1].ival : 1;
        if (len < 0) { *out = val_nil(); return 1; }
        if (start + len > (int64_t)slen) len = (int64_t)slen - start;
        *out = val_string_n(ev->arena, s + start, (size_t)len);
        out->string_encoding = recv.string_encoding;
        return 1;
    }
    if (strcmp(name, "b") == 0) {
        *out = recv;
        out->string_encoding = STRING_ENCODING_ASCII_8BIT;
        return 1;
    }
    if (strcmp(name, "force_encoding") == 0) {
        if (argc < 1) {
            *out = eval_raise_class(ev, site, "ArgumentError",
                                    "wrong number of arguments (given 0, expected 1)");
            return 1;
        }
        *out = recv;
        out->string_encoding = string_encoding_from_value(args[0]);
        return 1;
    }
    if (strcmp(name, "encode") == 0)
        { *out = recv; return 1; } /* stub: no transcoding, identity for UTF-8 */
    if (strcmp(name, "to_i") == 0) {
        int base = (argc > 0 && args[0].kind == VAL_INT) ? (int)args[0].ival : 10;
        if (base < 2 || base > 36) base = 10;
        *out = val_int((int64_t)strtoll(s, NULL, base)); return 1;
    }
    if (strcmp(name, "to_f") == 0) { *out = val_float(atof(s)); return 1; }
    if (strcmp(name, "to_r") == 0) {
        Value rat_class;
        if (!env_get(ev->top_env, "Rational", &rat_class) || rat_class.kind != VAL_CLASS) {
            *out = val_int(0);
            return 1;
        }
        /* Try "N/M" format */
        const char *slash = strchr(s, '/');
        if (slash) {
            int64_t num = strtoll(s, NULL, 10);
            int64_t den = strtoll(slash + 1, NULL, 10);
            if (den == 0) { *out = eval_raise_class(ev, site, "ZeroDivisionError", "divided by 0"); return 1; }
            Value rargs[2] = {val_int(num), val_int(den)};
            *out = dispatch_method(ev, env, rat_class, "new", rargs, 2, NULL, site, 0, 1);
            return 1;
        }
        /* Try float format or plain integer */
        char *endptr;
        double fval = strtod(s, &endptr);
        if (endptr > s) {
            char *endptr2;
            int64_t ival = strtoll(s, &endptr2, 10);
            if (*endptr2 == '\0' || *endptr2 == ' ') {
                /* Integer string: N/1 */
                Value rargs[2] = {val_int(ival), val_int(1)};
                *out = dispatch_method(ev, env, rat_class, "new", rargs, 2, NULL, site, 0, 1);
            } else {
                /* Float string: use numerator/denominator approximation */
                int64_t denom = 1000000;
                int64_t numer = (int64_t)round(fval * (double)denom);
                Value rargs[2] = {val_int(numer), val_int(denom)};
                *out = dispatch_method(ev, env, rat_class, "new", rargs, 2, NULL, site, 0, 1);
            }
        } else {
            *out = val_int(0);
        }
        return 1;
    }
    if (strcmp(name, "to_c") == 0) {
        Value cplx_class;
        if (!env_get(ev->top_env, "Complex", &cplx_class) || cplx_class.kind != VAL_CLASS) {
            *out = val_nil();
            return 1;
        }
        size_t slen = strlen(s);
        double real_part = 0.0, imag_part = 0.0;
        if (slen > 0 && s[slen - 1] == 'i') {
            /* Find the last + or - that is not at position 0 */
            const char *sep = NULL;
            for (const char *p2 = s + slen - 2; p2 > s; p2--) {
                if (*p2 == '+' || *p2 == '-') { sep = p2; break; }
            }
            if (sep) {
                real_part = strtod(s, NULL);
                imag_part = strtod(sep, NULL);
            } else {
                real_part = 0.0;
                imag_part = strtod(s, NULL);
            }
        } else {
            real_part = strtod(s, NULL);
            imag_part = 0.0;
        }
        Value r_val = (real_part == (double)(int64_t)real_part) ? val_int((int64_t)real_part) : val_float(real_part);
        Value i_val = (imag_part == (double)(int64_t)imag_part) ? val_int((int64_t)imag_part) : val_float(imag_part);
        Value cargs[2] = {r_val, i_val};
        *out = dispatch_method(ev, env, cplx_class, "new", cargs, 2, NULL, site, 0, 1);
        return 1;
    }
    if (strcmp(name, "to_sym") == 0) { *out = val_symbol(s); return 1; }
    if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0) {
        *out = recv.string_encoding == STRING_ENCODING_ASCII_8BIT
             ? val_int((int64_t)recv.byte_len)
             : val_int((int64_t)utf8_char_count(s, recv.byte_len));
        return 1;
    }
    if (strcmp(name, "empty?") == 0) { *out = val_bool(recv.byte_len == 0); return 1; }
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
    if (strcmp(name, "upcase!") == 0 || strcmp(name, "downcase!") == 0 ||
        strcmp(name, "capitalize!") == 0 || strcmp(name, "swapcase!") == 0 ||
        strcmp(name, "reverse!") == 0 || strcmp(name, "chomp!") == 0 ||
        strcmp(name, "chop!") == 0 || strcmp(name, "strip!") == 0 ||
        strcmp(name, "lstrip!") == 0 || strcmp(name, "rstrip!") == 0) {
        if (recv.frozen)
            { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen String: \"%s\"", s); return 1; }
        const char *nonbang = name;
        char nb[32]; size_t nl = strlen(name) - 1;
        memcpy(nb, name, nl); nb[nl] = '\0';
        Value r = val_nil();
        dispatch_string(ev, env, recv, nb, NULL, 0, NULL, NULL, &r);
        *out = (r.kind == VAL_STRING && strcmp(r.sval, s) != 0) ? r : val_nil();
        (void)nonbang;
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
        size_t chars = utf8_char_count(s, recv.byte_len);
        for (size_t i = 0; i < chars; i++) {
            const char *ptr = NULL;
            size_t width = 0;
            utf8_char_at(s, recv.byte_len, i, &ptr, &width, NULL);
            val_array_push(&arr, val_string_n(ev->arena, ptr, width));
        }
        if (blk) {
            for (size_t i = 0; i < arr.array->len; i++) {
                Value r = call_block(ev, env, *blk, &arr.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv; return 1;
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
    if (strcmp(name, "delete_prefix") == 0 || strcmp(name, "delete_prefix!") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#delete_prefix requires an argument"); return 1; }
        int bang = (name[strlen(name)-1] == '!');
        if (bang && recv.frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen String"); return 1; }
        const char *prefix = val_to_s(ev->arena, args[0]);
        size_t plen = strlen(prefix);
        if (strncmp(s, prefix, plen) == 0) *out = val_string(ev->arena, s + plen);
        else *out = bang ? val_nil() : val_string(ev->arena, s);
        return 1;
    }
    if (strcmp(name, "delete_suffix") == 0 || strcmp(name, "delete_suffix!") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#delete_suffix requires an argument"); return 1; }
        int bang = (name[strlen(name)-1] == '!');
        if (bang && recv.frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen String"); return 1; }
        const char *suffix = val_to_s(ev->arena, args[0]);
        size_t slen = strlen(s), suflen = strlen(suffix);
        if (slen >= suflen && strcmp(s + slen - suflen, suffix) == 0)
            *out = val_string_n(ev->arena, s, slen - suflen);
        else *out = bang ? val_nil() : val_string(ev->arena, s);
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
            size_t chars = utf8_char_count(s, recv.byte_len);
            for (size_t i = 0; i < chars; i++) {
                const char *ptr = NULL;
                size_t width = 0;
                utf8_char_at(s, recv.byte_len, i, &ptr, &width, NULL);
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
        /* Remove trailing empty strings when limit is 0 (default) */
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
    if (strcmp(name, "each_char") == 0) {
        size_t chars = utf8_char_count(s, recv.byte_len);
        if (!blk) {
            /* blockless: return Enumerator wrapping char array */
            Value arr = val_array_new();
            for (size_t i = 0; i < chars; i++) {
                const char *ptr = NULL; size_t width = 0;
                utf8_char_at(s, recv.byte_len, i, &ptr, &width, NULL);
                val_array_push(&arr, val_string_n(ev->arena, ptr, width));
            }
            *out = wrap_as_enumerator(ev, env, arr, site);
        } else {
            for (size_t i = 0; i < chars; i++) {
                const char *ptr = NULL;
                size_t width = 0;
                utf8_char_at(s, recv.byte_len, i, &ptr, &width, NULL);
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
    if (strcmp(name, "each_byte") == 0) {
        if (!blk) {
            /* Return Enumerator wrapping byte array */
            Value arr = val_array_new();
            for (size_t i = 0; s[i]; i++) val_array_push(&arr, val_int((int64_t)(unsigned char)s[i]));
            *out = wrap_as_enumerator(ev, env, arr, site); return 1;
        }
        for (size_t i = 0; s[i]; i++) {
            Value byte = val_int((int64_t)(unsigned char)s[i]);
            Value r = call_block(ev, env, *blk, &byte, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv; return 1;
    }
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
        size_t pad_byte_len = argc >= 2 && args[1].kind == VAL_STRING ? args[1].byte_len : 1;
        size_t pad_chars = utf8_char_count(pad, pad_byte_len);
        size_t slen = utf8_char_count(s, recv.byte_len);
        if (pad_chars == 0) pad_chars = 1;
        if ((int64_t)slen >= width) { *out = recv; return 1; }
        size_t total_chars = (size_t)width;
        size_t total = recv.byte_len + (total_chars - slen) * pad_byte_len + 1;
        char *buf = arena_alloc(ev->arena, total);
        size_t lpad = 0, rpad = 0;
        size_t pos = 0;
        if (strcmp(name, "ljust") == 0) { lpad = 0; rpad = total_chars - slen; }
        else if (strcmp(name, "rjust") == 0) { lpad = total_chars - slen; rpad = 0; }
        else { lpad = (total_chars - slen) / 2; rpad = total_chars - slen - lpad; }
        append_utf8_pad(buf, &pos, pad, pad_byte_len, lpad);
        memcpy(buf + pos, s, recv.byte_len);
        pos += recv.byte_len;
        append_utf8_pad(buf, &pos, pad, pad_byte_len, rpad);
        buf[pos] = '\0';
        *out = val_string_n(ev->arena, buf, pos);
        return 1;
    }
    if (strcmp(name, "ord") == 0) {
        uint32_t cp = 0;
        if (recv.byte_len == 0 || !utf8_decode_one(s, recv.byte_len, &cp, NULL)) {
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
    if (strcmp(name, "partition") == 0 || strcmp(name, "rpartition") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "partition requires an argument"); return 1; }
        const char *sep = val_to_s(ev->arena, args[0]);
        size_t seplen = strlen(sep);
        const char *found = NULL;
        if (strcmp(name, "partition") == 0) {
            found = strstr(s, sep);
        } else {
            /* rpartition: find last occurrence */
            const char *p = strstr(s, sep);
            while (p) { found = p; p = strstr(p + 1, sep); }
        }
        Value arr = val_array_new();
        if (found) {
            val_array_push(&arr, val_string_n(ev->arena, s, (size_t)(found - s)));
            val_array_push(&arr, val_string(ev->arena, sep));
            val_array_push(&arr, val_string(ev->arena, found + seplen));
        } else if (strcmp(name, "partition") == 0) {
            val_array_push(&arr, val_string(ev->arena, s));
            val_array_push(&arr, val_string(ev->arena, ""));
            val_array_push(&arr, val_string(ev->arena, ""));
        } else {
            val_array_push(&arr, val_string(ev->arena, ""));
            val_array_push(&arr, val_string(ev->arena, ""));
            val_array_push(&arr, val_string(ev->arena, s));
        }
        *out = arr; return 1;
    }
    if (strcmp(name, "bytes") == 0) {
        Value arr = val_array_new();
        size_t slen = recv.byte_len;
        for (size_t i = 0; i < slen; i++)
            val_array_push(&arr, val_int((int64_t)(unsigned char)s[i]));
        *out = arr;
        return 1;
    }
    if (strcmp(name, "unpack") == 0 || strcmp(name, "unpack1") == 0) {
        if (argc < 1 || args[0].kind != VAL_STRING) { *out = val_array_new(); return 1; }
        const char *tmpl = args[0].sval;
        Value arr = val_array_new();
        size_t si = 0, slen = strlen(s);
        static const char hexchars[] = "0123456789abcdef";
        for (size_t ti = 0; tmpl[ti] && si < slen; ti++) {
            char dir = tmpl[ti];
            int count = 1, star = 0;
            if (tmpl[ti+1] == '*') { star = 1; ti++; count = (int)(slen - si); }
            else if (isdigit((unsigned char)tmpl[ti+1])) { count = 0; while (isdigit((unsigned char)tmpl[ti+1])) count = count*10 + (tmpl[++ti] - '0'); }
            /* H/h: hex string — handle before byte-level loop */
            if (dir == 'H' || dir == 'h') {
                size_t nbytes = star ? slen - si : (size_t)(count > 0 ? count : 0);
                nbytes = nbytes > slen - si ? slen - si : nbytes;
                char *hbuf = arena_alloc(ev->arena, nbytes * 2 + 1);
                for (size_t bi = 0; bi < nbytes; bi++) {
                    uint8_t b = (uint8_t)s[si + bi];
                    hbuf[bi*2]     = hexchars[(b >> 4) & 0xF];
                    hbuf[bi*2 + 1] = hexchars[b & 0xF];
                }
                hbuf[nbytes*2] = '\0';
                si += nbytes;
                val_array_push(&arr, val_string(ev->arena, hbuf));
                continue;
            }
            for (int ci = 0; ci < count && si < slen; ci++) {
                switch (dir) {
                    case 'C': case 'c': val_array_push(&arr, val_int((dir=='c')?(int8_t)(unsigned char)s[si]:(unsigned char)s[si])); si++; break;
                    case 'S': case 's': if (si+2<=slen) { uint16_t v2; memcpy(&v2,s+si,2); val_array_push(&arr,val_int((dir=='s')?(int16_t)v2:(uint16_t)v2)); si+=2; } break;
                    case 'L': case 'l': if (si+4<=slen) { uint32_t v4; memcpy(&v4,s+si,4); val_array_push(&arr,val_int((dir=='l')?(int32_t)v4:(uint32_t)v4)); si+=4; } break;
                    case 'Q': case 'q': if (si+8<=slen) { uint64_t v8; memcpy(&v8,s+si,8); val_array_push(&arr,val_int((int64_t)v8)); si+=8; } break;
                    case 'N': if (si+4<=slen) { uint32_t v4=((uint8_t)s[si]<<24)|((uint8_t)s[si+1]<<16)|((uint8_t)s[si+2]<<8)|(uint8_t)s[si+3]; val_array_push(&arr,val_int((int64_t)v4)); si+=4; } break;
                    case 'n': if (si+2<=slen) { uint16_t v2=((uint8_t)s[si]<<8)|(uint8_t)s[si+1]; val_array_push(&arr,val_int((int64_t)v2)); si+=2; } break;
                    case 'V': if (si+4<=slen) { uint32_t v4=(uint8_t)s[si]|((uint8_t)s[si+1]<<8)|((uint8_t)s[si+2]<<16)|((uint8_t)s[si+3]<<24); val_array_push(&arr,val_int((int64_t)v4)); si+=4; } break;
                    case 'v': if (si+2<=slen) { uint16_t v2=(uint8_t)s[si]|((uint8_t)s[si+1]<<8); val_array_push(&arr,val_int((int64_t)v2)); si+=2; } break;
                    case 'f': if (si+4<=slen) { float fv; memcpy(&fv,s+si,4); val_array_push(&arr,val_float((double)fv)); si+=4; } break;
                    case 'd': case 'D': if (si+8<=slen) { double dv; memcpy(&dv,s+si,8); val_array_push(&arr,val_float(dv)); si+=8; } break;
                    case 'G': if (si+8<=slen) { uint8_t b[8]; for(int bi=7;bi>=0;bi--) b[bi]=s[si+7-bi]; double dv; memcpy(&dv,b,8); val_array_push(&arr,val_float(dv)); si+=8; } break;
                    case 'g': if (si+4<=slen) { uint8_t b[4]; for(int bi=3;bi>=0;bi--) b[bi]=s[si+3-bi]; float fv; memcpy(&fv,b,4); val_array_push(&arr,val_float((double)fv)); si+=4; } break;
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
        const char *rhs = args[0].kind == VAL_STRING ? (args[0].sval ? args[0].sval : "")
                                                      : val_to_s(ev->arena, args[0]);
        size_t slen = recv.byte_len;
        size_t rlen = args[0].kind == VAL_STRING ? args[0].byte_len : strlen(rhs);
        char *buf = arena_alloc(ev->arena, slen + rlen + 1);
        memcpy(buf, s, slen);
        memcpy(buf + slen, rhs, rlen);
        buf[slen + rlen] = '\0';
        Value r; r.kind = VAL_STRING; r.frozen = 0;
        r.string_encoding = recv.string_encoding; r.byte_len = slen + rlen; r.sval = buf;
        *out = r;
        return 1;
    }
    if (strcmp(name, "index") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#index requires an argument"); return 1; }
        size_t slen = recv.byte_len;
        size_t offset = (argc >= 2 && args[1].kind == VAL_INT) ? utf8_byte_offset_for_char(s, slen, (size_t)args[1].ival) : 0;
        if (offset > slen) { *out = val_nil(); return 1; }
        if (value_is_regexp(args[0])) {
            Regex *re = (Regex *)args[0].obj->native;
            if (!re) {
                Value src; RegexError rerr = {0};
                if (val_object_get_ivar(args[0], "source", &src) && src.kind == VAL_STRING)
                    regex_compile(ev->arena, src.sval, 0, &re, &rerr);
                if (re) args[0].obj->native = re;
            }
            if (!re) { *out = val_nil(); return 1; }
            RegexMatch rm = {0,0,0,NULL,NULL};
            RegexStatus st = regex_search(re, s, slen, (int)offset, &rm);
            *out = st == REGEX_OK ? val_int((int64_t)utf8_char_index_for_byte(s, slen, (size_t)rm.beg)) : val_nil();
            return 1;
        }
        const char *needle = val_to_s(ev->arena, args[0]);
        const char *found = strstr(s + offset, needle);
        *out = found ? val_int((int64_t)utf8_char_index_for_byte(s, slen, (size_t)(found - s))) : val_nil();
        return 1;
    }
    if (strcmp(name, "rindex") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#rindex requires an argument"); return 1; }
        size_t slen = recv.byte_len;
        size_t limit_byte = (argc >= 2 && args[1].kind == VAL_INT) ? utf8_byte_offset_for_char(s, slen, (size_t)args[1].ival) : slen;
        if (limit_byte > slen) limit_byte = slen;
        if (value_is_regexp(args[0])) {
            Regex *re = (Regex *)args[0].obj->native;
            if (!re) {
                Value src; RegexError rerr = {0};
                if (val_object_get_ivar(args[0], "source", &src) && src.kind == VAL_STRING)
                    regex_compile(ev->arena, src.sval, 0, &re, &rerr);
                if (re) args[0].obj->native = re;
            }
            if (!re) { *out = val_nil(); return 1; }
            /* Scan forward, keep last match within limit */
            long last_match = -1;
            for (size_t pos = 0; pos <= limit_byte; ) {
                RegexMatch rm = {0,0,0,NULL,NULL};
                RegexStatus st = regex_search(re, s, slen, (int)pos, &rm);
                if (st != REGEX_OK || (size_t)rm.beg > limit_byte) break;
                last_match = rm.beg;
                pos = rm.beg == rm.end ? rm.beg + 1 : (size_t)rm.end;
            }
            *out = last_match >= 0 ? val_int((int64_t)utf8_char_index_for_byte(s, slen, (size_t)last_match)) : val_nil();
            return 1;
        }
        const char *needle = val_to_s(ev->arena, args[0]);
        size_t nlen = strlen(needle);
        const char *last = NULL;
        for (size_t i = 0; i + nlen <= limit_byte + 1 && i <= limit_byte; i++) {
            if (strncmp(s + i, needle, nlen) == 0) last = s + i;
        }
        *out = last ? val_int((int64_t)utf8_char_index_for_byte(s, slen, (size_t)(last - s))) : val_nil();
        return 1;
    }
    if (strcmp(name, "[]") == 0 || strcmp(name, "slice") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#[] requires an argument"); return 1; }
        size_t slen = utf8_char_count(s, recv.byte_len);
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
            size_t bstart = utf8_byte_offset_for_char(s, recv.byte_len, (size_t)rbeg);
            size_t bend   = utf8_byte_offset_for_char(s, recv.byte_len, (size_t)rend);
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
            if (regex_search(compiled, s, recv.byte_len, 0, &m) != REGEX_OK) { *out = val_nil(); return 1; }
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
            size_t start = utf8_byte_offset_for_char(s, recv.byte_len, (size_t)idx);
            size_t end = utf8_byte_offset_for_char(s, recv.byte_len, (size_t)idx + take);
            *out = val_string_n(ev->arena, s + start, end - start);
        } else {
            const char *ptr = NULL;
            size_t width = 0;
            utf8_char_at(s, recv.byte_len, (size_t)idx, &ptr, &width, NULL);
            *out = val_string_n(ev->arena, ptr, width);
        }
        return 1;
    }
    if (strcmp(name, "[]=") == 0) {
        if (recv.frozen)
            { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen String"); return 1; }
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        size_t slen = utf8_char_count(s, recv.byte_len);
        size_t sbytes = recv.byte_len;
        /* Determine replacement string (last arg) */
        const char *repl = val_to_s(ev->arena, args[argc - 1]);
        size_t repl_bytes = args[argc - 1].kind == VAL_STRING ? args[argc - 1].byte_len : strlen(repl);
        /* Determine byte range to replace */
        size_t bstart = 0, bend = 0;
        if (argc == 3 && args[0].kind == VAL_INT && args[1].kind == VAL_INT) {
            /* s[idx, len] = repl */
            int64_t idx = args[0].ival, len = args[1].ival;
            if (idx < 0) idx += (int64_t)slen;
            if (idx < 0 || (size_t)idx > slen)
                { *out = eval_raise_class(ev, site, "IndexError", "index out of string"); return 1; }
            if (len < 0) len = 0;
            size_t take = (size_t)idx + (size_t)len > slen ? slen - (size_t)idx : (size_t)len;
            bstart = utf8_byte_offset_for_char(s, sbytes, (size_t)idx);
            bend   = utf8_byte_offset_for_char(s, sbytes, (size_t)idx + take);
        } else if (argc == 2 && args[0].kind == VAL_RANGE) {
            RubyRange *r = args[0].range;
            int64_t rbeg = r->begin_val.kind == VAL_INT ? r->begin_val.ival : 0;
            int64_t rend = r->end_val.kind == VAL_INT ? r->end_val.ival : (int64_t)slen;
            if (rbeg < 0) rbeg += (int64_t)slen;
            if (rend < 0) rend += (int64_t)slen;
            if (!r->exclusive) rend++;
            if (rbeg < 0) rbeg = 0;
            if ((size_t)rend > slen) rend = (int64_t)slen;
            bstart = utf8_byte_offset_for_char(s, sbytes, (size_t)rbeg);
            bend   = utf8_byte_offset_for_char(s, sbytes, (size_t)rend);
        } else if (argc == 2 && args[0].kind == VAL_INT) {
            /* s[idx] = repl */
            int64_t idx = args[0].ival;
            if (idx < 0) idx += (int64_t)slen;
            if (idx < 0 || (size_t)idx >= slen)
                { *out = eval_raise_class(ev, site, "IndexError", "index out of string"); return 1; }
            bstart = utf8_byte_offset_for_char(s, sbytes, (size_t)idx);
            bend   = utf8_byte_offset_for_char(s, sbytes, (size_t)idx + 1);
        } else if (argc == 2 && args[0].kind == VAL_STRING) {
            const char *needle = args[0].sval;
            const char *found = strstr(s, needle);
            if (!found) { *out = eval_raise_class(ev, site, "IndexError", "string not matched"); return 1; }
            bstart = (size_t)(found - s);
            bend   = bstart + args[0].byte_len;
        } else {
            *out = eval_raise_class(ev, site, "ArgumentError", "wrong arguments for String#[]="); return 1;
        }
        /* Build new string: before + repl + after */
        size_t new_len = bstart + repl_bytes + (sbytes - bend);
        char *buf = arena_alloc(ev->arena, new_len + 1);
        memcpy(buf, s, bstart);
        memcpy(buf + bstart, repl, repl_bytes);
        memcpy(buf + bstart + repl_bytes, s + bend, sbytes - bend);
        buf[new_len] = '\0';
        *out = val_string_n(ev->arena, buf, new_len);
        return 1;
    }
    if (strcmp(name, "each_line") == 0 || strcmp(name, "lines") == 0) {
        /* Optional separator argument; chomp: keyword */
        const char *sep = "\n";
        int do_chomp = 0;
        for (int i = 0; i < argc; i++) {
            if (args[i].kind == VAL_STRING) sep = args[i].sval;
            else if (args[i].kind == VAL_NIL) sep = NULL;
            else if (args[i].kind == VAL_HASH) {
                Value chv = val_nil();
                Value chopk = val_symbol("chomp");
                if (val_hash_get(args[i].hash, chopk, &chv)) do_chomp = val_truthy(chv);
            }
        }
        size_t seplen = sep ? strlen(sep) : 0;
        Value arr = val_array_new();
        const char *p = s;
        if (!sep || seplen == 0) {
            val_array_push(&arr, recv);
        } else {
            while (*p) {
                const char *found = strstr(p, sep);
                Value line;
                if (found) {
                    size_t llen = (size_t)(found - p + (do_chomp ? 0 : seplen));
                    line = val_string_n(ev->arena, p, llen);
                    p = found + seplen;
                } else {
                    line = val_string(ev->arena, p);
                    if (do_chomp) {
                        size_t ll = strlen(p);
                        if (ll > 0 && p[ll-1] == '\n') {
                            line = val_string_n(ev->arena, p, ll - (ll>1 && p[ll-2]=='\r' ? 2 : 1));
                        }
                    }
                    p += strlen(p);
                }
                val_array_push(&arr, line);
            }
        }
        if (strcmp(name, "lines") == 0) { *out = arr; return 1; }
        if (!blk) { *out = wrap_as_enumerator(ev, env, arr, site); return 1; }
        for (size_t i = 0; i < arr.array->len; i++) {
            Value r = call_block(ev, env, *blk, &arr.array->elems[i], 1, site);
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
        int negate_tr = (from_pat[0] == '^');
        uint32_t from_chars[1024], to_chars[1024];
        size_t from_len = rune_pattern_chars(from_pat, from_chars, 1024);
        size_t to_len   = rune_pattern_chars(to_pat,   to_chars,   1024);
        if (to_len == 0) { *out = recv; return 1; }
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen * 4 + 1);
        size_t used = 0;
        for (size_t i = 0; i < slen;) {
            uint32_t cp = 0, new_cp = 0;
            size_t width = 0, outw = 0;
            char enc[4];
            utf8_decode_one(s + i, slen - i, &cp, &width);
            size_t idx = rune_index_in_chars(cp, from_chars, from_len);
            if (negate_tr) {
                /* negate: replace chars NOT in from_chars */
                if (idx == (size_t)-1)
                    new_cp = to_chars[to_len - 1];  /* replace with last char of to */
                else
                    new_cp = cp;  /* keep as-is */
            } else {
                if (idx != (size_t)-1)
                    new_cp = to_chars[idx < to_len ? idx : to_len - 1];
                else
                    new_cp = cp;
            }
            outw = utf8_encode_one(new_cp, enc);
            memcpy(buf + used, enc, outw);
            used += outw;
            i += width;
        }
        buf[used] = '\0';
        *out = val_string_n(ev->arena, buf, used);
        return 1;
    }
    if (strcmp(name, "tr_s") == 0) {
        /* tr then squeeze translated chars */
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "String#tr_s requires two arguments"); return 1; }
        /* First do the tr translation */
        Value tr_result;
        if (!dispatch_string(ev, env, recv, "tr", args, 2, NULL, site, &tr_result)) tr_result = recv;
        if (val_is_signal(tr_result)) { *out = tr_result; return 1; }
        /* Then squeeze chars that appear in to_pat */
        const char *tr_s = tr_result.sval ? tr_result.sval : "";
        const char *to_pat = val_to_s(ev->arena, args[1]);
        uint32_t to_chars[1024];
        size_t to_len = rune_pattern_chars(to_pat, to_chars, 1024);
        size_t tlen = strlen(tr_s);
        char *sq = arena_alloc(ev->arena, tlen + 1);
        size_t qi = 0;
        uint32_t prev = 0;
        for (size_t i = 0; i < tlen;) {
            uint32_t cp = 0; size_t w = 0;
            utf8_decode_one(tr_s + i, tlen - i, &cp, &w);
            int in_to = (to_len == 0) || (rune_index_in_chars(cp, to_chars, to_len) != (size_t)-1);
            if (!in_to || cp != prev) {
                char enc[4]; size_t ew = utf8_encode_one(cp, enc);
                memcpy(sq + qi, enc, ew); qi += ew;
            }
            prev = in_to ? cp : 0;
            i += w;
        }
        sq[qi] = '\0';
        *out = val_string_n(ev->arena, sq, qi);
        return 1;
    }
    if (strcmp(name, "count") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#count requires an argument"); return 1; }
        const char *pat = val_to_s(ev->arena, args[0]);
        int negate = (pat[0] == '^');
        RuneRange ranges[256];
        size_t range_count = rune_pattern_ranges(pat, ranges, 256);
        int64_t cnt = 0;
        for (size_t i = 0, slen = strlen(s); i < slen;) {
            uint32_t cp = 0;
            size_t width = 0;
            utf8_decode_one(s + i, slen - i, &cp, &width);
            int in_set = rune_in_ranges(cp, ranges, range_count);
            if (negate ? !in_set : in_set) cnt++;
            i += width;
        }
        *out = val_int(cnt);
        return 1;
    }
    if (strcmp(name, "delete") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "String#delete requires an argument"); return 1; }
        const char *del_pat = val_to_s(ev->arena, args[0]);
        int negate_del = (del_pat[0] == '^');
        RuneRange ranges[256];
        size_t range_count = rune_pattern_ranges(del_pat, ranges, 256);
        size_t slen = strlen(s);
        char *buf = arena_alloc(ev->arena, slen + 1);
        size_t j = 0;
        for (size_t i = 0; i < slen;) {
            uint32_t cp = 0;
            size_t width = 0;
            utf8_decode_one(s + i, slen - i, &cp, &width);
            int in_set = rune_in_ranges(cp, ranges, range_count);
            int should_delete = negate_del ? !in_set : in_set;
            if (!should_delete) {
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
            /* Optional second arg: start position */
            Value search_str = recv;
            if (argc >= 2 && args[1].kind == VAL_INT) {
                int64_t pos = args[1].ival;
                size_t slen2 = strlen(s);
                if (pos < 0) pos = (int64_t)slen2 + pos;
                if (pos < 0) pos = 0;
                if ((size_t)pos <= slen2)
                    search_str = val_string(ev->arena, s + (size_t)pos);
                else
                    search_str = val_string(ev->arena, "");
            }
            Value md = regexp_search_value(ev, args[0], search_str, 0, site);
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
            /* match? must NOT set $~ or other match globals */
            Regex *compiled = (Regex *)args[0].obj->native;
            if (!compiled) {
                Value src;
                if (val_object_get_ivar(args[0], "source", &src) && src.kind == VAL_STRING) {
                    RegexError rerr = {0};
                    regex_compile(ev->arena, src.sval, 0, &compiled, &rerr);
                    args[0].obj->native = compiled;
                }
            }
            if (!compiled) { *out = val_false(); return 1; }
            RegexMatch rm = {0, 0, 0, NULL, NULL};
            int64_t pos = (argc >= 2 && args[1].kind == VAL_INT) ? args[1].ival : 0;
            RegexStatus st = regex_search(compiled, s, strlen(s), (int)pos, &rm);
            *out = val_bool(st == REGEX_OK);
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
        if (recv.frozen)
            { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen String: \"%s\"", s); return 1; }
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
                            hrepl = "";  /* key not found: replace with empty string */
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
                    /* Empty match at m.beg: emit char at that position as a literal
                       and advance past it. Pre-match chars (pos..m.beg-1) were
                       already copied, so use m.beg, not the stale pos. */
                    if ((size_t)m.beg < slen) {
                        while (used + 2 > cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); *out = val_nil(); return 1; } buf = nb; }
                        buf[used++] = s[m.beg];
                    }
                    pos = (size_t)m.beg + 1;
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
                        repl = val_hash_get(args[1].hash, mkey, &hval) ? val_to_s(ev->arena, hval) : "";
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

int dispatch_nil(Eval *ev, Env *env, Value recv, const char *name, Node *site, Value *out) {
    (void)recv;
    if (strcmp(name, "nil?") == 0) { *out = val_true(); return 1; }
    if (strcmp(name, "to_s") == 0) { *out = val_string(ev->arena, ""); return 1; }
    if (strcmp(name, "inspect") == 0) { *out = val_string(ev->arena, "nil"); return 1; }
    if (strcmp(name, "to_i") == 0 || strcmp(name, "to_int") == 0) { *out = val_int(0); return 1; }
    if (strcmp(name, "to_f") == 0) { *out = val_float(0.0); return 1; }
    if (strcmp(name, "to_a") == 0) { *out = val_array_new(); return 1; }
    if (strcmp(name, "to_h") == 0) { *out = val_hash_new(ev->arena); return 1; }
    if (strcmp(name, "to_r") == 0) {
        Value rat_class;
        if (env_get(ev->top_env, "Rational", &rat_class) && rat_class.kind == VAL_CLASS) {
            Value zero = val_int(0);
            *out = dispatch_method(ev, env, rat_class, "new", &zero, 1, NULL, site, 0, 1);
        } else {
            *out = val_int(0);
        }
        return 1;
    }
    if (strcmp(name, "to_c") == 0) {
        Value cplx_class;
        if (env_get(ev->top_env, "Complex", &cplx_class) && cplx_class.kind == VAL_CLASS) {
            Value cargs[2] = {val_int(0), val_int(0)};
            *out = dispatch_method(ev, env, cplx_class, "new", cargs, 2, NULL, site, 0, 1);
        } else {
            *out = val_string(ev->arena, "(0+0i)");
        }
        return 1;
    }
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
