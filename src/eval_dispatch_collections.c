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
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Array#include? requires an argument");
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
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Array#each requires a block");
        else {
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "each_with_index") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Array#each_with_index requires a block");
        else {
            for (size_t i = 0; i < recv.array->len; i++) {
                Value bargs[2] = { recv.array->elems[i], val_int((int64_t)i) };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Array#map requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                val_array_push(&result, r);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Array#select requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                if (val_truthy(r)) val_array_push(&result, arg);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "reject") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Array#reject requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                if (!val_truthy(r)) val_array_push(&result, arg);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Array#reduce requires a block");
        else if (recv.array->len == 0) *out = argc > 0 ? args[0] : val_nil();
        else {
            size_t start = 0;
            Value acc = argc > 0 ? args[0] : recv.array->elems[start++];
            for (size_t i = start; i < recv.array->len; i++) {
                Value bargs[2] = { acc, recv.array->elems[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                acc = r;
            }
            *out = acc;
        }
        return 1;
    }
    if (strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError",
                                          strcmp(name, "any?") == 0 ? "Array#any? requires a block" :
                                          strcmp(name, "all?") == 0 ? "Array#all? requires a block" :
                                                                       "Array#none? requires a block");
        else {
            *out = strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0 ? val_true() : val_false();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_EXCEPTION) { *out = r; return 1; }
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
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#[] requires a key");
        else {
            Value found;
            *out = val_hash_get(h, args[0], &found) ? found : val_nil();
        }
        return 1;
    }
    if (strcmp(name, "[]=") == 0) {
        if (argc < 2) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#[]= requires key and value");
        else { val_hash_set(h, args[0], args[1]); *out = args[1]; }
        return 1;
    }
    if (strcmp(name, "fetch") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#fetch requires a key");
        else {
            Value found;
            if (val_hash_get(h, args[0], &found)) *out = found;
            else if (argc > 1) *out = args[1];
            else if (blk) *out = call_block(ev, *blk, &args[0], 1, site);
            else *out = eval_raise_class(ev, site, "KeyError", "Hash#fetch: key not found");
        }
        return 1;
    }
    if (strcmp(name, "has_key?") == 0 || strcmp(name, "key?") == 0 || strcmp(name, "include?") == 0 || strcmp(name, "member?") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#has_key? requires a key");
        else {
            Value found;
            *out = val_bool(val_hash_get(h, args[0], &found));
        }
        return 1;
    }
    if (strcmp(name, "has_value?") == 0 || strcmp(name, "value?") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#has_value? requires a value");
        else {
            *out = val_false();
            for (size_t i = 0; i < h->len; i++) if (val_equal(h->vals[i], args[0])) { *out = val_true(); break; }
        }
        return 1;
    }
    if (strcmp(name, "delete") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#delete requires a key");
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
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#merge requires a Hash");
        else if (args[0].kind != VAL_HASH) *out = eval_raise_class(ev, site, "TypeError", "Hash#merge requires a Hash");
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
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#merge! requires a Hash");
        else if (args[0].kind != VAL_HASH) *out = eval_raise_class(ev, site, "TypeError", "Hash#merge! requires a Hash");
        else {
            RubyHash *other = args[0].hash;
            for (size_t i = 0; i < other->len; i++) val_hash_set(h, other->keys[i], other->vals[i]);
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "each") == 0 || strcmp(name, "each_pair") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Hash#each requires a block");
        else {
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "each_key") == 0 || strcmp(name, "each_value") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError",
                                          strcmp(name, "each_key") == 0 ? "Hash#each_key requires a block" : "Hash#each_value requires a block");
        else {
            for (size_t i = 0; i < h->len; i++) {
                Value arg = strcmp(name, "each_key") == 0 ? h->keys[i] : h->vals[i];
                Value r = call_block(ev, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        }
        return 1;
    }
    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Hash#map requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                val_array_push(&result, r);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0 || strcmp(name, "reject") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError",
                                          strcmp(name, "reject") == 0 ? "Hash#reject requires a block" : "Hash#select requires a block");
        else {
            Value result = val_hash_new(ev->arena);
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                if ((strcmp(name, "reject") == 0 && !val_truthy(r)) || (strcmp(name, "reject") != 0 && val_truthy(r)))
                    val_hash_set(result.hash, h->keys[i], h->vals[i]);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError",
                                          strcmp(name, "any?") == 0 ? "Hash#any? requires a block" : "Hash#all? requires a block");
        else {
            *out = strcmp(name, "all?") == 0 ? val_true() : val_false();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_EXCEPTION) { *out = r; return 1; }
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
        *out = dispatch_method(ev, env, as_arr, name, args, argc, blk, site, 0, 1);
        return 1;
    }
    if (strcmp(name, "flat_map") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Hash#flat_map requires a block");
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_EXCEPTION) { *out = r; return 1; }
                if (r.kind == VAL_ARRAY) for (size_t j = 0; j < r.array->len; j++) val_array_push(&result, r.array->elems[j]);
                else val_array_push(&result, r);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
        if (!blk) *out = eval_raise_class(ev, site, "LocalJumpError", "Hash#reduce requires a block");
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
                if (flow_signal_out(r, out)) return 1;
                acc = r;
            }
            *out = acc;
        }
        return 1;
    }
    if (strcmp(name, "store") == 0) {
        if (argc < 2) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#store requires key and value");
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
    *out = eval_raise_class(ev, site, "NoMethodError", "undefined method '%s' for Hash", name);
    return 1;
}

static int range_include_value(Eval *ev, Env *env, RubyRange *r, Value v, Node *site) {
    /* Fast path for integer ranges */
    if (r->begin_val.kind == VAL_INT && r->end_val.kind == VAL_INT && v.kind == VAL_INT) {
        return v.ival >= r->begin_val.ival &&
               (r->exclusive ? v.ival < r->end_val.ival : v.ival <= r->end_val.ival);
    }
    /* General: use <=> comparison */
    Value cmp_lo = dispatch_method(ev, env, v, "<=>", &r->begin_val, 1, NULL, site, 0, 1);
    if (cmp_lo.kind != VAL_INT || cmp_lo.ival < 0) return 0;
    Value cmp_hi = dispatch_method(ev, env, v, "<=>", &r->end_val, 1, NULL, site, 0, 1);
    if (cmp_hi.kind != VAL_INT) return 0;
    return r->exclusive ? cmp_hi.ival < 0 : cmp_hi.ival <= 0;
}

int dispatch_range(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out) {
    if (recv.kind != VAL_RANGE) return 0;
    RubyRange *r = recv.range;

    if (strcmp(name, "begin") == 0) {
        if (argc > 0) {
            /* first(n) semantics */
            if (r->begin_val.kind != VAL_INT)
                { *out = eval_raise_class(ev, site, "TypeError", "Range#first(n) requires Integer range"); return 1; }
            int64_t n = args[0].kind == VAL_INT ? args[0].ival : 0;
            Value arr = val_array_new();
            int64_t hi = r->end_val.kind == VAL_INT ? r->end_val.ival : INT64_MAX;
            for (int64_t i = r->begin_val.ival; i < r->begin_val.ival + n; i++) {
                if (r->exclusive ? i >= hi : i > hi) break;
                val_array_push(&arr, val_int(i));
            }
            *out = arr; return 1;
        }
        *out = r->begin_val; return 1;
    }
    if (strcmp(name, "first") == 0) {
        if (argc > 0) {
            if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
                { *out = eval_raise_class(ev, site, "TypeError", "Range#first(n) requires Integer range"); return 1; }
            int64_t n = args[0].kind == VAL_INT ? args[0].ival : 0;
            Value arr = val_array_new();
            int64_t hi = r->end_val.ival;
            for (int64_t i = r->begin_val.ival; i < r->begin_val.ival + n; i++) {
                if (r->exclusive ? i >= hi : i > hi) break;
                val_array_push(&arr, val_int(i));
            }
            *out = arr; return 1;
        }
        *out = r->begin_val; return 1;
    }
    if (strcmp(name, "end") == 0) {
        if (argc > 0) {
            if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
                { *out = eval_raise_class(ev, site, "TypeError", "Range#last(n) requires Integer range"); return 1; }
            int64_t n = args[0].kind == VAL_INT ? args[0].ival : 0;
            int64_t hi = r->exclusive ? r->end_val.ival - 1 : r->end_val.ival;
            Value arr = val_array_new();
            int64_t lo = hi - n + 1;
            for (int64_t i = lo; i <= hi; i++) val_array_push(&arr, val_int(i));
            *out = arr; return 1;
        }
        *out = r->end_val; return 1;
    }
    if (strcmp(name, "last") == 0) {
        if (argc > 0) {
            if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
                { *out = eval_raise_class(ev, site, "TypeError", "Range#last(n) requires Integer range"); return 1; }
            int64_t n = args[0].kind == VAL_INT ? args[0].ival : 0;
            int64_t hi = r->exclusive ? r->end_val.ival - 1 : r->end_val.ival;
            Value arr = val_array_new();
            int64_t lo = hi - n + 1;
            for (int64_t i = lo; i <= hi; i++) val_array_push(&arr, val_int(i));
            *out = arr; return 1;
        }
        *out = r->end_val; return 1;
    }
    if (strcmp(name, "exclude_end?") == 0) { *out = val_bool(r->exclusive); return 1; }

    if (strcmp(name, "include?") == 0 || strcmp(name, "member?") == 0 ||
        strcmp(name, "cover?") == 0 || strcmp(name, "===") == 0) {
        if (argc < 1)
            { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        *out = val_bool(range_include_value(ev, env, r, args[0], site));
        return 1;
    }

    if (strcmp(name, "each") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#each requires a block"); return 1; }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#each requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
        }
        *out = recv; return 1;
    }

    if (strcmp(name, "each_with_index") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#each_with_index requires a block"); return 1; }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#each_with_index requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        int64_t idx = 0;
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++, idx++) {
            Value bargs[2] = { val_int(i), val_int(idx) };
            Value res = call_block(ev, *blk, bargs, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
        }
        *out = recv; return 1;
    }

    if (strcmp(name, "to_a") == 0 || strcmp(name, "entries") == 0) {
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#to_a requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        Value arr = val_array_new();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++)
            val_array_push(&arr, val_int(i));
        *out = arr; return 1;
    }

    if (strcmp(name, "size") == 0 || strcmp(name, "count") == 0 || strcmp(name, "length") == 0) {
        if (argc > 0 && strcmp(name, "count") == 0) {
            /* count with block — fall through to Enumerable */
            return 0;
        }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = val_nil(); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        int64_t sz = r->exclusive ? (hi > lo ? hi - lo : 0) : (hi >= lo ? hi - lo + 1 : 0);
        *out = val_int(sz); return 1;
    }

    if (strcmp(name, "min") == 0) {
        if (blk) return 0; /* min with block — fall through to Enumerable */
        *out = r->begin_val; return 1;
    }
    if (strcmp(name, "max") == 0) {
        if (blk) return 0; /* max with block — fall through to Enumerable */
        if (r->exclusive && r->begin_val.kind == VAL_INT && r->end_val.kind == VAL_INT)
            *out = val_int(r->end_val.ival - 1);
        else
            *out = r->end_val;
        return 1;
    }

    if (strcmp(name, "sum") == 0) {
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#sum requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival;
        int64_t hi = r->exclusive ? r->end_val.ival - 1 : r->end_val.ival;
        int64_t init = argc > 0 && args[0].kind == VAL_INT ? args[0].ival : 0;
        if (hi < lo) { *out = val_int(init); return 1; }
        *out = val_int(init + (hi - lo + 1) * (lo + hi) / 2); return 1;
    }

    if (strcmp(name, "step") == 0) {
        if (argc < 1)
            { *out = eval_raise_class(ev, site, "ArgumentError", "Range#step requires a step value"); return 1; }
        if (!blk)
            { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#step requires a block"); return 1; }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT || args[0].kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#step requires Integer range and step"); return 1; }
        int64_t step = args[0].ival;
        if (step <= 0)
            { *out = eval_raise_class(ev, site, "ArgumentError", "step must be positive"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i += step) {
            Value arg = val_int(i);
            Value res = call_block(ev, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
        }
        *out = recv; return 1;
    }

    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#map requires a block"); return 1; }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#map requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        Value result = val_array_new();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
            val_array_push(&result, res);
        }
        *out = result; return 1;
    }

    if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#select requires a block"); return 1; }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#select requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        Value result = val_array_new();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
            if (val_truthy(res)) val_array_push(&result, arg);
        }
        *out = result; return 1;
    }

    if (strcmp(name, "reject") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#reject requires a block"); return 1; }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#reject requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        Value result = val_array_new();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
            if (!val_truthy(res)) val_array_push(&result, arg);
        }
        *out = result; return 1;
    }

    if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#reduce requires a block"); return 1; }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#reduce requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        int64_t start = lo;
        Value acc = argc > 0 ? args[0] : val_int(start++);
        for (int64_t i = start; r->exclusive ? i < hi : i <= hi; i++) {
            Value bargs[2] = { acc, val_int(i) };
            Value res = call_block(ev, *blk, bargs, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
            acc = res;
        }
        *out = acc; return 1;
    }

    if (strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#any? requires a block"); return 1; }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range iteration requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        *out = (strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) ? val_true() : val_false();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (res.kind == VAL_EXCEPTION) { *out = res; return 1; }
            if (strcmp(name, "any?") == 0 && val_truthy(res)) { *out = val_true(); return 1; }
            if (strcmp(name, "all?") == 0 && !val_truthy(res)) { *out = val_false(); return 1; }
            if (strcmp(name, "none?") == 0 && val_truthy(res)) { *out = val_false(); return 1; }
        }
        return 1;
    }

    if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0)
        { *out = val_string(ev->arena, val_to_s(ev->arena, recv)); return 1; }

    if (strcmp(name, "nil?") == 0) { *out = val_false(); return 1; }

    return 0;
}
