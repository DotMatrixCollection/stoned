#include "eval_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int dispatch_integer(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                     Value *blk, Node *site, Value *out) {
    (void)env;
    if (recv.kind != VAL_INT) return 0;
    if (strcmp(name, "to_s") == 0) { *out = val_string(ev->arena, val_to_s(ev->arena, recv)); return 1; }
    if (strcmp(name, "to_f") == 0) { *out = val_float((double)recv.ival); return 1; }
    if (strcmp(name, "to_i") == 0) { *out = recv; return 1; }
    if (strcmp(name, "abs") == 0) { *out = val_int(recv.ival < 0 ? -recv.ival : recv.ival); return 1; }
    if (strcmp(name, "even?") == 0) { *out = val_bool(recv.ival % 2 == 0); return 1; }
    if (strcmp(name, "odd?") == 0) { *out = val_bool(recv.ival % 2 != 0); return 1; }
    if (strcmp(name, "zero?") == 0) { *out = val_bool(recv.ival == 0); return 1; }
    if (strcmp(name, "times") == 0) {
        if (!blk) *out = eval_error(ev, site, "Integer#times requires a block");
        else {
            for (int64_t i = 0; i < recv.ival; i++) {
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
        if (argc < 1) *out = eval_error(ev, site, "Integer#upto requires an argument");
        else if (!blk) *out = eval_error(ev, site, "Integer#upto requires a block");
        else {
            for (int64_t i = recv.ival; i <= args[0].ival; i++) {
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
        if (argc < 1) *out = eval_error(ev, site, "Integer#downto requires an argument");
        else if (!blk) *out = eval_error(ev, site, "Integer#downto requires a block");
        else {
            for (int64_t i = recv.ival; i >= args[0].ival; i--) {
                Value arg = val_int(i);
                Value r = call_block(ev, *blk, &arg, 1, site);
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
    (void)env; (void)args; (void)argc; (void)blk; (void)site;
    if (recv.kind != VAL_FLOAT) return 0;
    if (strcmp(name, "to_s") == 0) { *out = val_string(ev->arena, val_to_s(ev->arena, recv)); return 1; }
    if (strcmp(name, "to_f") == 0) { *out = recv; return 1; }
    if (strcmp(name, "to_i") == 0) { *out = val_int((int64_t)recv.fval); return 1; }
    if (strcmp(name, "abs") == 0) { *out = val_float(recv.fval < 0 ? -recv.fval : recv.fval); return 1; }
    if (strcmp(name, "ceil") == 0) { *out = val_int((int64_t)ceil(recv.fval)); return 1; }
    if (strcmp(name, "floor") == 0) { *out = val_int((int64_t)floor(recv.fval)); return 1; }
    if (strcmp(name, "round") == 0) { *out = val_int((int64_t)round(recv.fval)); return 1; }
    if (strcmp(name, "zero?") == 0) { *out = val_bool(recv.fval == 0.0); return 1; }
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
        if (argc < 1) *out = eval_error(ev, site, "String#include? requires an argument");
        else *out = val_bool(strstr(s, val_to_s(ev->arena, args[0])) != NULL);
        return 1;
    }
    if (strcmp(name, "start_with?") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "String#start_with? requires an argument");
        else {
            const char *needle = val_to_s(ev->arena, args[0]);
            *out = val_bool(strncmp(s, needle, strlen(needle)) == 0);
        }
        return 1;
    }
    if (strcmp(name, "end_with?") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "String#end_with? requires an argument");
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
        if (!blk) *out = eval_error(ev, site, "String#each_char requires a block");
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
    if (strcmp(name, "replace") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "String#replace requires an argument");
        else *out = val_string(ev->arena, val_to_s(ev->arena, args[0]));
        return 1;
    }
    if (strcmp(name, "*") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "String#* requires an argument");
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
    *out = eval_error(ev, site, "undefined method '%s' for nil", name);
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
