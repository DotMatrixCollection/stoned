#include "eval_internal.h"

#include <stdlib.h>
#include <string.h>

int dispatch_array(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out) {
    (void)env;
    if (recv.kind != VAL_ARRAY) return 0;
    if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0 || strcmp(name, "count") == 0) { *out = val_int((int64_t)recv.array->len); return 1; }
    if (strcmp(name, "empty?") == 0) { *out = val_bool(recv.array->len == 0); return 1; }
    if (strcmp(name, "first") == 0) { *out = recv.array->len == 0 ? val_nil() : recv.array->elems[0]; return 1; }
    if (strcmp(name, "last") == 0) { *out = recv.array->len == 0 ? val_nil() : recv.array->elems[recv.array->len - 1]; return 1; }
    if (strcmp(name, "push") == 0 || strcmp(name, "append") == 0) {
        for (int i = 0; i < argc; i++) val_array_push(&recv, args[i]);
        *out = recv; return 1;
    }
    if (strcmp(name, "pop") == 0) { *out = recv.array->len == 0 ? val_nil() : recv.array->elems[--recv.array->len]; return 1; }
    if (strcmp(name, "shift") == 0) {
        if (recv.array->len == 0) *out = val_nil();
        else {
            Value first = recv.array->elems[0];
            memmove(recv.array->elems, recv.array->elems + 1, (recv.array->len - 1) * sizeof(Value));
            recv.array->len--;
            *out = first;
        }
        return 1;
    }
    if (strcmp(name, "unshift") == 0 || strcmp(name, "prepend") == 0) {
        for (int i = argc - 1; i >= 0; i--) {
            if (recv.array->len >= recv.array->cap) {
                size_t nc = recv.array->cap == 0 ? 8 : recv.array->cap * 2;
                recv.array->elems = realloc(recv.array->elems, nc * sizeof(Value));
                recv.array->cap = nc;
            }
            memmove(recv.array->elems + 1, recv.array->elems, recv.array->len * sizeof(Value));
            recv.array->elems[0] = args[i];
            recv.array->len++;
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "reverse") == 0) {
        Value arr = val_array_new();
        for (size_t i = recv.array->len; i > 0; i--) val_array_push(&arr, recv.array->elems[i - 1]);
        *out = arr; return 1;
    }
    if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0) { *out = val_string(ev->arena, val_to_s(ev->arena, recv)); return 1; }
    if (strcmp(name, "join") == 0) {
        const char *sep = argc > 0 ? val_to_s(ev->arena, args[0]) : "";
        size_t total = 1;
        for (size_t i = 0; i < recv.array->len; i++) total += strlen(val_to_s(ev->arena, recv.array->elems[i])) + strlen(sep);
        char *buf = arena_alloc(ev->arena, total);
        buf[0] = '\0';
        for (size_t i = 0; i < recv.array->len; i++) {
            if (i) strcat(buf, sep);
            strcat(buf, val_to_s(ev->arena, recv.array->elems[i]));
        }
        *out = val_string(ev->arena, buf); return 1;
    }
    if (strcmp(name, "include?") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "Array#include? requires an argument");
        else {
            *out = val_false();
            for (size_t i = 0; i < recv.array->len; i++) if (val_equal(recv.array->elems[i], args[0])) { *out = val_true(); break; }
        }
        return 1;
    }
    if (strcmp(name, "each") == 0 || strcmp(name, "each_with_index") == 0 || strcmp(name, "map") == 0 ||
        strcmp(name, "collect") == 0 || strcmp(name, "select") == 0 || strcmp(name, "filter") == 0 ||
        strcmp(name, "reject") == 0 || strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0 ||
        strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) {
        /* Keep iteration-heavy behavior centralized in this file. */
    }
    if (strcmp(name, "each") == 0) {
        if (!blk) *out = eval_error(ev, site, "Array#each requires a block");
        else {
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "each_with_index") == 0) {
        if (!blk) *out = eval_error(ev, site, "Array#each_with_index requires a block");
        else {
            for (size_t i = 0; i < recv.array->len; i++) {
                Value bargs[2] = { recv.array->elems[i], val_int((int64_t)i) };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
        if (!blk) *out = eval_error(ev, site, "Array#map requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
                val_array_push(&result, r);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0) {
        if (!blk) *out = eval_error(ev, site, "Array#select requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
                if (val_truthy(r)) val_array_push(&result, arg);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "reject") == 0) {
        if (!blk) *out = eval_error(ev, site, "Array#reject requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
                if (!val_truthy(r)) val_array_push(&result, arg);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
        if (!blk) *out = eval_error(ev, site, "Array#reduce requires a block");
        else if (recv.array->len == 0) *out = argc > 0 ? args[0] : val_nil();
        else {
            size_t start = 0;
            Value acc = argc > 0 ? args[0] : recv.array->elems[start++];
            for (size_t i = start; i < recv.array->len; i++) {
                Value bargs[2] = { acc, recv.array->elems[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
                acc = r;
            }
            *out = acc;
        }
        return 1;
    }
    if (strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) {
        if (!blk) *out = eval_error(ev, site, strcmp(name, "any?") == 0 ? "Array#any? requires a block" :
                                              strcmp(name, "all?") == 0 ? "Array#all? requires a block" :
                                                                           "Array#none? requires a block");
        else {
            *out = strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0 ? val_true() : val_false();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (strcmp(name, "any?") == 0 && val_truthy(r)) { *out = val_true(); return 1; }
                if (strcmp(name, "all?") == 0 && !val_truthy(r)) { *out = val_false(); return 1; }
                if (strcmp(name, "none?") == 0 && val_truthy(r)) { *out = val_false(); return 1; }
            }
        }
        return 1;
    }
    if (strcmp(name, "min") == 0) {
        if (recv.array->len == 0) *out = val_nil();
        else {
            Value m = recv.array->elems[0];
            for (size_t i = 1; i < recv.array->len; i++) {
                Value cur = recv.array->elems[i];
                if (cur.kind == VAL_INT && m.kind == VAL_INT && cur.ival < m.ival) m = cur;
                else if (cur.kind == VAL_FLOAT && m.kind == VAL_FLOAT && cur.fval < m.fval) m = cur;
            }
            *out = m;
        }
        return 1;
    }
    if (strcmp(name, "max") == 0) {
        if (recv.array->len == 0) *out = val_nil();
        else {
            Value m = recv.array->elems[0];
            for (size_t i = 1; i < recv.array->len; i++) {
                Value cur = recv.array->elems[i];
                if (cur.kind == VAL_INT && m.kind == VAL_INT && cur.ival > m.ival) m = cur;
                else if (cur.kind == VAL_FLOAT && m.kind == VAL_FLOAT && cur.fval > m.fval) m = cur;
            }
            *out = m;
        }
        return 1;
    }
    if (strcmp(name, "sum") == 0) {
        Value acc = argc > 0 ? args[0] : val_int(0);
        for (size_t i = 0; i < recv.array->len; i++) {
            Value cur = recv.array->elems[i];
            if (acc.kind == VAL_INT && cur.kind == VAL_INT) acc.ival += cur.ival;
            else {
                double a = acc.kind == VAL_FLOAT ? acc.fval : (double)acc.ival;
                double c = cur.kind == VAL_FLOAT ? cur.fval : (double)cur.ival;
                acc = val_float(a + c);
            }
        }
        *out = acc;
        return 1;
    }
    if (strcmp(name, "flatten") == 0) {
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value elem = recv.array->elems[i];
            if (elem.kind == VAL_ARRAY) {
                for (size_t j = 0; j < elem.array->len; j++) val_array_push(&result, elem.array->elems[j]);
            } else {
                val_array_push(&result, elem);
            }
        }
        *out = result;
        return 1;
    }
    if (strcmp(name, "uniq") == 0) {
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            int found = 0;
            for (size_t j = 0; j < result.array->len; j++) if (val_equal(result.array->elems[j], recv.array->elems[i])) { found = 1; break; }
            if (!found) val_array_push(&result, recv.array->elems[i]);
        }
        *out = result;
        return 1;
    }
    if (strcmp(name, "sort") == 0) {
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) val_array_push(&result, recv.array->elems[i]);
        for (size_t i = 1; i < result.array->len; i++) {
            Value key = result.array->elems[i];
            size_t j = i;
            while (j > 0) {
                Value prev = result.array->elems[j - 1];
                int less = 0;
                if (key.kind == VAL_INT && prev.kind == VAL_INT) less = key.ival < prev.ival;
                else if (key.kind == VAL_FLOAT && prev.kind == VAL_FLOAT) less = key.fval < prev.fval;
                else if (key.kind == VAL_STRING && prev.kind == VAL_STRING) less = strcmp(key.sval, prev.sval) < 0;
                if (!less) break;
                result.array->elems[j] = prev;
                j--;
            }
            result.array->elems[j] = key;
        }
        *out = result;
        return 1;
    }
    if (strcmp(name, "compact") == 0) {
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) if (recv.array->elems[i].kind != VAL_NIL) val_array_push(&result, recv.array->elems[i]);
        *out = result;
        return 1;
    }
    if (strcmp(name, "zip") == 0) {
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value pair = val_array_new();
            val_array_push(&pair, recv.array->elems[i]);
            for (int j = 0; j < argc; j++) {
                if (args[j].kind == VAL_ARRAY && i < args[j].array->len) val_array_push(&pair, args[j].array->elems[i]);
                else val_array_push(&pair, val_nil());
            }
            val_array_push(&result, pair);
        }
        *out = result;
        return 1;
    }
    return 0;
}

int dispatch_hash(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                  Value *blk, Node *site, Value *out) {
    if (recv.kind != VAL_HASH) return 0;
    RubyHash *h = recv.hash;
    if (strcmp(name, "[]") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "Hash#[] requires a key");
        else {
            Value found;
            *out = val_hash_get(h, args[0], &found) ? found : val_nil();
        }
        return 1;
    }
    if (strcmp(name, "[]=") == 0) {
        if (argc < 2) *out = eval_error(ev, site, "Hash#[]= requires key and value");
        else { val_hash_set(h, args[0], args[1]); *out = args[1]; }
        return 1;
    }
    if (strcmp(name, "fetch") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "Hash#fetch requires a key");
        else {
            Value found;
            if (val_hash_get(h, args[0], &found)) *out = found;
            else if (argc > 1) *out = args[1];
            else if (blk) *out = call_block(ev, *blk, &args[0], 1, site);
            else *out = eval_error(ev, site, "Hash#fetch: key not found");
        }
        return 1;
    }
    if (strcmp(name, "has_key?") == 0 || strcmp(name, "key?") == 0 || strcmp(name, "include?") == 0 || strcmp(name, "member?") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "Hash#has_key? requires a key");
        else {
            Value found;
            *out = val_bool(val_hash_get(h, args[0], &found));
        }
        return 1;
    }
    if (strcmp(name, "has_value?") == 0 || strcmp(name, "value?") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "Hash#has_value? requires a value");
        else {
            *out = val_false();
            for (size_t i = 0; i < h->len; i++) if (val_equal(h->vals[i], args[0])) { *out = val_true(); break; }
        }
        return 1;
    }
    if (strcmp(name, "delete") == 0) {
        if (argc < 1) *out = eval_error(ev, site, "Hash#delete requires a key");
        else {
            Value found;
            int ok = val_hash_get(h, args[0], &found);
            val_hash_delete(h, args[0]);
            if (ok) *out = found;
            else if (blk) *out = call_block(ev, *blk, &args[0], 1, site);
            else *out = val_nil();
        }
        return 1;
    }
    if (strcmp(name, "keys") == 0) {
        Value arr = val_array_new();
        for (size_t i = 0; i < h->len; i++) val_array_push(&arr, h->keys[i]);
        *out = arr; return 1;
    }
    if (strcmp(name, "values") == 0) {
        Value arr = val_array_new();
        for (size_t i = 0; i < h->len; i++) val_array_push(&arr, h->vals[i]);
        *out = arr; return 1;
    }
    if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0 || strcmp(name, "count") == 0) { *out = val_int((int64_t)h->len); return 1; }
    if (strcmp(name, "empty?") == 0) { *out = val_bool(h->len == 0); return 1; }
    if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0) { *out = val_string(ev->arena, val_to_s(ev->arena, recv)); return 1; }
    if (strcmp(name, "to_a") == 0) {
        Value arr = val_array_new();
        for (size_t i = 0; i < h->len; i++) {
            Value pair = val_array_new();
            val_array_push(&pair, h->keys[i]);
            val_array_push(&pair, h->vals[i]);
            val_array_push(&arr, pair);
        }
        *out = arr; return 1;
    }
    if (strcmp(name, "merge") == 0) {
        if (argc < 1 || args[0].kind != VAL_HASH) *out = eval_error(ev, site, "Hash#merge requires a Hash");
        else {
            Value result = val_hash_new(ev->arena);
            for (size_t i = 0; i < h->len; i++) val_hash_set(result.hash, h->keys[i], h->vals[i]);
            RubyHash *other = args[0].hash;
            for (size_t i = 0; i < other->len; i++) val_hash_set(result.hash, other->keys[i], other->vals[i]);
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "merge!") == 0 || strcmp(name, "update") == 0) {
        if (argc < 1 || args[0].kind != VAL_HASH) *out = eval_error(ev, site, "Hash#merge! requires a Hash");
        else {
            RubyHash *other = args[0].hash;
            for (size_t i = 0; i < other->len; i++) val_hash_set(h, other->keys[i], other->vals[i]);
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "each") == 0 || strcmp(name, "each_pair") == 0) {
        if (!blk) *out = eval_error(ev, site, "Hash#each requires a block");
        else {
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "each_key") == 0 || strcmp(name, "each_value") == 0) {
        if (!blk) *out = eval_error(ev, site, strcmp(name, "each_key") == 0 ? "Hash#each_key requires a block" : "Hash#each_value requires a block");
        else {
            for (size_t i = 0; i < h->len; i++) {
                Value arg = strcmp(name, "each_key") == 0 ? h->keys[i] : h->vals[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
        if (!blk) *out = eval_error(ev, site, "Hash#map requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
                val_array_push(&result, r);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0 || strcmp(name, "reject") == 0) {
        if (!blk) *out = eval_error(ev, site, strcmp(name, "reject") == 0 ? "Hash#reject requires a block" : "Hash#select requires a block");
        else {
            Value result = val_hash_new(ev->arena);
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
                if ((strcmp(name, "reject") == 0 && !val_truthy(r)) || (strcmp(name, "reject") != 0 && val_truthy(r)))
                    val_hash_set(result.hash, h->keys[i], h->vals[i]);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0) {
        if (!blk) *out = eval_error(ev, site, strcmp(name, "any?") == 0 ? "Hash#any? requires a block" : "Hash#all? requires a block");
        else {
            *out = strcmp(name, "all?") == 0 ? val_true() : val_false();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (strcmp(name, "any?") == 0 && val_truthy(r)) { *out = val_true(); return 1; }
                if (strcmp(name, "all?") == 0 && !val_truthy(r)) { *out = val_false(); return 1; }
            }
        }
        return 1;
    }
    if (strcmp(name, "min_by") == 0 || strcmp(name, "max_by") == 0 || strcmp(name, "sort_by") == 0) {
        Value as_arr = val_array_new();
        for (size_t i = 0; i < h->len; i++) {
            Value pair = val_array_new();
            val_array_push(&pair, h->keys[i]);
            val_array_push(&pair, h->vals[i]);
            val_array_push(&as_arr, pair);
        }
        *out = dispatch_method(ev, env, as_arr, name, args, argc, blk, site);
        return 1;
    }
    if (strcmp(name, "flat_map") == 0) {
        if (!blk) *out = eval_error(ev, site, "Hash#flat_map requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_ARRAY) for (size_t j = 0; j < r.array->len; j++) val_array_push(&result, r.array->elems[j]);
                else val_array_push(&result, r);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
        if (!blk) *out = eval_error(ev, site, "Hash#reduce requires a block");
        else if (h->len == 0) *out = argc > 0 ? args[0] : val_nil();
        else {
            size_t start = 0;
            Value acc;
            if (argc > 0) acc = args[0];
            else {
                Value pair = val_array_new();
                val_array_push(&pair, h->keys[0]);
                val_array_push(&pair, h->vals[0]);
                acc = pair;
                start = 1;
            }
            for (size_t i = start; i < h->len; i++) {
                Value pair = val_array_new();
                val_array_push(&pair, h->keys[i]);
                val_array_push(&pair, h->vals[i]);
                Value bargs[2] = { acc, pair };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_BREAK) { *out = *r.wrapped; return 1; }
                if (r.kind == VAL_RETURN) { *out = r; return 1; }
                acc = r;
            }
            *out = acc;
        }
        return 1;
    }
    if (strcmp(name, "store") == 0) {
        if (argc < 2) *out = eval_error(ev, site, "Hash#store requires key and value");
        else { val_hash_set(h, args[0], args[1]); *out = args[1]; }
        return 1;
    }
    if (strcmp(name, "clear") == 0) { h->len = 0; *out = recv; return 1; }
    if (strcmp(name, "dup") == 0) {
        Value result = val_hash_new(ev->arena);
        for (size_t i = 0; i < h->len; i++) val_hash_set(result.hash, h->keys[i], h->vals[i]);
        *out = result;
        return 1;
    }
    if (strcmp(name, "nil?") == 0) { *out = val_false(); return 1; }
    *out = eval_error(ev, site, "undefined method '%s' for Hash", name);
    return 1;
}
