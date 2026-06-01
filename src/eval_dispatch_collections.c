#define _POSIX_C_SOURCE 200809L

#include "eval_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Wrap an already-computed result array in Enumerator.new(arr). */
static Value wrap_result_as_enumerator(Eval *ev, Env *env, Value arr, Node *site) {
    Value enum_class;
    if (env_get(ev->top_env, "Enumerator", &enum_class) && enum_class.kind == VAL_CLASS) {
        Value r = dispatch_method(ev, env, enum_class, "new", &arr, 1, NULL, site, 0, 1);
        if (!val_is_signal(r)) return r;
        ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
    }
    return arr;
}

static Value hash_pairs_array(RubyHash *h) {
    Value pairs = val_array_new();
    for (size_t i = 0; i < h->len; i++) {
        Value pair = val_array_new();
        val_array_push(&pair, h->keys[i]);
        val_array_push(&pair, h->vals[i]);
        val_array_push(&pairs, pair);
    }
    return pairs;
}

static Value hash_keys_or_values_array(RubyHash *h, int keys) {
    Value arr = val_array_new();
    for (size_t i = 0; i < h->len; i++)
        val_array_push(&arr, keys ? h->keys[i] : h->vals[i]);
    return arr;
}

static int hash_identity_key_equal(Value a, Value b) {
    if (a.kind != b.kind) return 0;
    switch (a.kind) {
        case VAL_OBJECT: return a.obj == b.obj;
        case VAL_BLOCK:  return a.block.block_node == b.block.block_node;
        case VAL_STRING: return a.sval == b.sval;
        case VAL_ARRAY:  return a.array == b.array;
        case VAL_HASH:   return a.hash == b.hash;
        case VAL_CLASS:  return a.klass == b.klass;
        default:         return val_equal(a, b);
    }
}

static int hash_key_equal_for_mode(RubyHash *h, Value a, Value b) {
    return h->compare_by_identity ? hash_identity_key_equal(a, b) : val_equal(a, b);
}

static int hash_is_process_env(Eval *ev, RubyHash *h) {
    Value env_hash = val_nil();
    return env_get(ev->top_env, "ENV", &env_hash) &&
           env_hash.kind == VAL_HASH &&
           env_hash.hash == h;
}

static void sync_process_env_pair(Eval *ev, RubyHash *h, Value key, Value value) {
    if (!hash_is_process_env(ev, h))
        return;
    const char *env_key = val_to_s(ev->arena, key);
    if (!env_key || env_key[0] == '\0')
        return;
    if (value.kind == VAL_NIL) {
        unsetenv(env_key);
        return;
    }
    const char *env_value = val_to_s(ev->arena, value);
    if (!env_value)
        env_value = "";
    setenv(env_key, env_value, 1);
}

static void sync_process_env_delete(Eval *ev, RubyHash *h, Value key) {
    if (!hash_is_process_env(ev, h))
        return;
    const char *env_key = val_to_s(ev->arena, key);
    if (!env_key || env_key[0] == '\0')
        return;
    unsetenv(env_key);
}

static void combination_helper(Value *elems, size_t n, size_t k, size_t start,
                               Value *current, size_t depth, Value *result) {
    if (depth == k) {
        Value combo = val_array_new();
        for (size_t i = 0; i < k; i++) val_array_push(&combo, current[i]);
        val_array_push(result, combo);
        return;
    }
    for (size_t i = start; i <= n - (k - depth); i++) {
        current[depth] = elems[i];
        combination_helper(elems, n, k, i + 1, current, depth + 1, result);
    }
}

static void permutation_helper(Value *elems, size_t n, size_t k,
                                int *used, Value *current, size_t depth, Value *result) {
    if (depth == k) {
        Value perm = val_array_new();
        for (size_t i = 0; i < k; i++) val_array_push(&perm, current[i]);
        val_array_push(result, perm);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        if (!used[i]) {
            used[i] = 1;
            current[depth] = elems[i];
            permutation_helper(elems, n, k, used, current, depth + 1, result);
            used[i] = 0;
        }
    }
}

static void repeated_combination_helper(Value *elems, size_t n, size_t k,
                                        size_t start, Value *current, size_t depth, Value *result) {
    if (depth == k) {
        Value combo = val_array_new();
        for (size_t i = 0; i < k; i++) val_array_push(&combo, current[i]);
        val_array_push(result, combo);
        return;
    }
    for (size_t i = start; i < n; i++) {
        current[depth] = elems[i];
        repeated_combination_helper(elems, n, k, i, current, depth + 1, result);
    }
}

static void repeated_permutation_helper(Value *elems, size_t n, size_t k,
                                         Value *current, size_t depth, Value *result) {
    if (depth == k) {
        Value perm = val_array_new();
        for (size_t i = 0; i < k; i++) val_array_push(&perm, current[i]);
        val_array_push(result, perm);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        current[depth] = elems[i];
        repeated_permutation_helper(elems, n, k, current, depth + 1, result);
    }
}

static void product_helper(Value *arrays, int narrays, int idx,
                           Value *current, Value *result) {
    if (idx == narrays) {
        Value row = val_array_new();
        for (int i = 0; i < narrays; i++) val_array_push(&row, current[i]);
        val_array_push(result, row);
        return;
    }
    Value arr = arrays[idx];
    if (arr.kind != VAL_ARRAY) return;
    for (size_t i = 0; i < arr.array->len; i++) {
        current[idx] = arr.array->elems[i];
        product_helper(arrays, narrays, idx + 1, current, result);
    }
}

static void array_flatten_into(Eval *ev, Env *env, Value arr, Value *result, int depth) {
    for (size_t i = 0; i < arr.array->len; i++) {
        Value elem = arr.array->elems[i];
        if (elem.kind == VAL_ARRAY && depth != 0) {
            array_flatten_into(ev, env, elem, result, depth > 0 ? depth - 1 : depth);
        } else if (depth != 0 && elem.kind == VAL_OBJECT && ev && env) {
            /* Check for to_ary on user objects */
            Value to_ary_res = dispatch_method(ev, env, elem, "to_ary", NULL, 0, NULL, NULL, 0, -1);
            if (!val_is_signal(to_ary_res) && to_ary_res.kind == VAL_ARRAY) {
                array_flatten_into(ev, env, to_ary_res, result, depth > 0 ? depth - 1 : depth);
            } else {
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
                val_array_push(result, elem);
            }
        } else {
            val_array_push(result, elem);
        }
    }
}

int dispatch_array(Eval *ev, Env *env, Value recv, const char *name, Value *args, int argc,
                   Value *blk, Node *site, Value *out) {
    (void)env;
    if (recv.kind != VAL_ARRAY) return 0;
    if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0) { *out = val_int((int64_t)recv.array->len); return 1; }
    if (strcmp(name, "count") == 0) {
        if (argc == 0 && !blk) { *out = val_int((int64_t)recv.array->len); return 1; }
        int64_t cnt = 0;
        for (size_t i = 0; i < recv.array->len; i++) {
            if (blk) {
                Value r = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
                if (val_is_signal(r)) { *out = r; return 1; }
                if (val_truthy(r)) cnt++;
            } else {
                if (val_equal(recv.array->elems[i], args[0])) cnt++;
            }
        }
        *out = val_int(cnt); return 1;
    }
    if (strcmp(name, "empty?") == 0) { *out = val_bool(recv.array->len == 0); return 1; }
    if (strcmp(name, "at") == 0) {
        if (argc < 1 || args[0].kind != VAL_INT) { *out = val_nil(); return 1; }
        int64_t idx = args[0].ival;
        if (idx < 0) idx += (int64_t)recv.array->len;
        *out = (idx >= 0 && (size_t)idx < recv.array->len) ? recv.array->elems[idx] : val_nil();
        return 1;
    }
    if (strcmp(name, "fetch") == 0) {
        if (argc < 1 || args[0].kind != VAL_INT) {
            *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments");
            return 1;
        }
        int64_t idx = args[0].ival;
        if (idx < 0) idx += (int64_t)recv.array->len;
        if (idx >= 0 && (size_t)idx < recv.array->len) {
            *out = recv.array->elems[idx];
        } else if (blk) {
            *out = call_block(ev, env, *blk, &args[0], 1, site);
        } else if (argc >= 2) {
            *out = args[1];
        } else {
            *out = eval_raise_class(ev, site, "IndexError", "index %lld outside of array bounds", (long long)args[0].ival);
        }
        return 1;
    }
    if (strcmp(name, "slice") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        int64_t alen = (int64_t)recv.array->len;
        int64_t idx = 0, len = 1;
        if (args[0].kind == VAL_RANGE) {
            RubyRange *r = args[0].range;
            idx = r->begin_val.kind == VAL_INT ? r->begin_val.ival : 0;
            int64_t end = r->end_val.kind == VAL_INT ? r->end_val.ival : alen;
            if (idx < 0) idx += alen;
            if (end < 0) end += alen;
            if (!r->exclusive) end++;
            len = end - idx;
        } else if (args[0].kind == VAL_INT) {
            idx = args[0].ival;
            if (idx < 0) idx += alen;
            len = (argc >= 2 && args[1].kind == VAL_INT) ? args[1].ival : 1;
        } else {
            *out = val_nil(); return 1;
        }
        if (idx < 0 || idx > alen || len < 0) { *out = val_nil(); return 1; }
        if (argc < 2 && args[0].kind == VAL_INT) {
            *out = ((size_t)idx < recv.array->len) ? recv.array->elems[idx] : val_nil();
            return 1;
        }
        if (idx + len > alen) len = alen - idx;
        Value result = val_array_new();
        for (int64_t i = idx; i < idx + len; i++) val_array_push(&result, recv.array->elems[i]);
        *out = result;
        return 1;
    }
    if (strcmp(name, "first") == 0) {
        if (argc >= 1 && args[0].kind == VAL_INT) {
            size_t n = args[0].ival < 0 ? 0 : (size_t)args[0].ival;
            if (n > recv.array->len) n = recv.array->len;
            Value r = val_array_new();
            for (size_t i = 0; i < n; i++) val_array_push(&r, recv.array->elems[i]);
            *out = r;
        } else {
            *out = recv.array->len == 0 ? val_nil() : recv.array->elems[0];
        }
        return 1;
    }
    if (strcmp(name, "last") == 0) {
        if (argc >= 1 && args[0].kind == VAL_INT) {
            size_t n = args[0].ival < 0 ? 0 : (size_t)args[0].ival;
            if (n > recv.array->len) n = recv.array->len;
            Value r = val_array_new();
            for (size_t i = recv.array->len - n; i < recv.array->len; i++) val_array_push(&r, recv.array->elems[i]);
            *out = r;
        } else {
            *out = recv.array->len == 0 ? val_nil() : recv.array->elems[recv.array->len - 1];
        }
        return 1;
    }
    if (strcmp(name, "push") == 0 || strcmp(name, "append") == 0 || strcmp(name, "<<") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        for (int i = 0; i < argc; i++) val_array_push(&recv, args[i]);
        *out = recv; return 1;
    }
    if (strcmp(name, "concat") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        for (int i = 0; i < argc; i++) {
            if (args[i].kind == VAL_ARRAY) {
                for (size_t j = 0; j < args[i].array->len; j++)
                    val_array_push(&recv, args[i].array->elems[j]);
            }
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "pop") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        if (argc > 0 && args[0].kind == VAL_INT) {
            int64_t n = args[0].ival;
            if (n < 0) { *out = eval_raise_class(ev, site, "ArgumentError", "negative array size"); return 1; }
            if ((size_t)n > recv.array->len) n = (int64_t)recv.array->len;
            Value result = val_array_new();
            for (int64_t i = (int64_t)recv.array->len - n; i < (int64_t)recv.array->len; i++)
                val_array_push(&result, recv.array->elems[i]);
            recv.array->len -= (size_t)n;
            *out = result;
        } else {
            *out = recv.array->len == 0 ? val_nil() : recv.array->elems[--recv.array->len];
        }
        return 1;
    }
    if (strcmp(name, "delete") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        if (argc < 1) { *out = val_nil(); return 1; }
        Value deleted = val_nil();
        size_t w = 0;
        for (size_t i = 0; i < recv.array->len; i++) {
            if (val_equal(recv.array->elems[i], args[0])) {
                deleted = recv.array->elems[i];
            } else {
                recv.array->elems[w++] = recv.array->elems[i];
            }
        }
        recv.array->len = w;
        if (deleted.kind == VAL_NIL && blk) {
            *out = call_block(ev, env, *blk, NULL, 0, site);
        } else {
            *out = deleted;
        }
        return 1;
    }
    if (strcmp(name, "delete_at") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        if (argc < 1 || args[0].kind != VAL_INT) { *out = val_nil(); return 1; }
        int64_t idx = args[0].ival;
        int64_t alen = (int64_t)recv.array->len;
        if (idx < 0) idx += alen;
        if (idx < 0 || idx >= alen) { *out = val_nil(); return 1; }
        Value deleted = recv.array->elems[(size_t)idx];
        memmove(recv.array->elems + idx, recv.array->elems + idx + 1,
                (recv.array->len - (size_t)idx - 1) * sizeof(Value));
        recv.array->len--;
        *out = deleted; return 1;
    }
    if (strcmp(name, "clear") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        recv.array->len = 0; *out = recv; return 1;
    }
    if (strcmp(name, "shift") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        if (argc > 0 && args[0].kind == VAL_INT) {
            /* shift(n) — remove first n elements and return them as array */
            int64_t n2 = args[0].ival;
            if (n2 < 0) { *out = eval_raise_class(ev, site, "ArgumentError", "negative array size"); return 1; }
            if ((size_t)n2 > recv.array->len) n2 = (int64_t)recv.array->len;
            Value result2 = val_array_new();
            for (int64_t i = 0; i < n2; i++) val_array_push(&result2, recv.array->elems[i]);
            memmove(recv.array->elems, recv.array->elems + n2, (recv.array->len - (size_t)n2) * sizeof(Value));
            recv.array->len -= (size_t)n2;
            *out = result2;
        } else if (recv.array->len == 0) {
            *out = val_nil();
        } else {
            Value first = recv.array->elems[0];
            memmove(recv.array->elems, recv.array->elems + 1, (recv.array->len - 1) * sizeof(Value));
            recv.array->len--;
            *out = first;
        }
        return 1;
    }
    if (strcmp(name, "unshift") == 0 || strcmp(name, "prepend") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
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
    if (strcmp(name, "insert") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        if (argc < 1 || args[0].kind != VAL_INT) { *out = eval_raise_class(ev, site, "ArgumentError", "Array#insert requires an Integer index"); return 1; }
        int64_t idx = args[0].ival;
        int64_t alen = (int64_t)recv.array->len;
        if (idx < 0) idx = alen + idx + 1;
        if (idx < 0) idx = 0;
        while ((int64_t)recv.array->len < idx) {
            while (recv.array->len >= recv.array->cap) {
                size_t nc = recv.array->cap == 0 ? 8 : recv.array->cap * 2;
                recv.array->elems = realloc(recv.array->elems, nc * sizeof(Value));
                recv.array->cap = nc;
            }
            recv.array->elems[recv.array->len++] = val_nil();
        }
        int ins_count = argc - 1;
        while (recv.array->len + (size_t)ins_count > recv.array->cap) {
            size_t nc = recv.array->cap == 0 ? 8 : recv.array->cap * 2;
            if (nc < recv.array->len + (size_t)ins_count) nc = recv.array->len + (size_t)ins_count;
            recv.array->elems = realloc(recv.array->elems, nc * sizeof(Value));
            recv.array->cap = nc;
        }
        memmove(recv.array->elems + idx + ins_count, recv.array->elems + idx,
                (recv.array->len - (size_t)idx) * sizeof(Value));
        for (int i = 0; i < ins_count; i++)
            recv.array->elems[(size_t)idx + i] = args[1 + i];
        recv.array->len += (size_t)ins_count;
        *out = recv; return 1;
    }
    if (strcmp(name, "fill") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        Value fill_value = argc > 0 ? args[0] : val_nil();
        int arg_offset = blk ? 0 : 1;
        if (!blk && argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        int64_t alen = (int64_t)recv.array->len;
        int64_t start = 0;
        int64_t len = alen;
        if (argc > arg_offset) {
            if (args[arg_offset].kind == VAL_RANGE) {
                RubyRange *r = args[arg_offset].range;
                start = r->begin_val.kind == VAL_INT ? r->begin_val.ival : 0;
                int64_t end = r->end_val.kind == VAL_INT ? r->end_val.ival : alen;
                if (start < 0) start += alen;
                if (end < 0) end += alen;
                if (!r->exclusive) end++;
                len = end - start;
            } else if (args[arg_offset].kind == VAL_INT) {
                start = args[arg_offset].ival;
                if (start < 0) start += alen;
                len = (argc > arg_offset + 1 && args[arg_offset + 1].kind == VAL_INT)
                    ? args[arg_offset + 1].ival : alen - start;
            }
        }
        if (start < 0) start = 0;
        if (len < 0) { *out = recv; return 1; }
        int64_t end = start + len;
        while ((int64_t)recv.array->len < end) val_array_push(&recv, val_nil());
        for (int64_t i = start; i < end; i++) {
            if (blk) {
                Value idx_arg = val_int(i);
                Value r = call_block(ev, env, *blk, &idx_arg, 1, site);
                if (val_is_signal(r)) { *out = r; return 1; }
                recv.array->elems[i] = r;
            } else {
                recv.array->elems[i] = fill_value;
            }
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "dig") == 0) {
        if (argc < 1 || args[0].kind != VAL_INT) { *out = val_nil(); return 1; }
        int64_t idx = args[0].ival;
        if (idx < 0) idx += (int64_t)recv.array->len;
        if (idx < 0 || (size_t)idx >= recv.array->len) { *out = val_nil(); return 1; }
        Value current = recv.array->elems[idx];
        for (int i = 1; i < argc; i++) {
            if (current.kind == VAL_ARRAY) {
                if (args[i].kind != VAL_INT) { *out = val_nil(); return 1; }
                int64_t ci = args[i].ival;
                if (ci < 0) ci += (int64_t)current.array->len;
                if (ci < 0 || (size_t)ci >= current.array->len) { *out = val_nil(); return 1; }
                current = current.array->elems[ci];
            } else if (current.kind == VAL_HASH) {
                Value next;
                if (!val_hash_get(current.hash, args[i], &next)) { *out = val_nil(); return 1; }
                current = next;
            } else { *out = val_nil(); return 1; }
        }
        *out = current; return 1;
    }
    if (strcmp(name, "[]=") == 0) {
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        if (args[0].kind == VAL_RANGE) {
            /* Array[range] = val — replace slice */
            RubyRange *r = args[0].range;
            int64_t alen = (int64_t)recv.array->len;
            int64_t beg = r->begin_val.kind == VAL_INT ? r->begin_val.ival : 0;
            int64_t end = r->end_val.kind == VAL_INT ? r->end_val.ival : alen;
            if (beg < 0) beg += alen;
            if (end < 0) end += alen;
            if (!r->exclusive) end++;
            if (beg < 0) beg = 0;
            if (end > alen) end = alen;
            /* Simple: rebuild array */
            Value repl = args[1].kind == VAL_ARRAY ? args[1] : val_array_new();
            if (args[1].kind != VAL_ARRAY) val_array_push(&repl, args[1]);
            Value result = val_array_new();
            for (int64_t i = 0; i < beg; i++) val_array_push(&result, recv.array->elems[i]);
            for (size_t i = 0; i < repl.array->len; i++) val_array_push(&result, repl.array->elems[i]);
            for (int64_t i = end; i < alen; i++) val_array_push(&result, recv.array->elems[i]);
            /* Update in place */
            recv.array->len = 0;
            for (size_t i = 0; i < result.array->len; i++) val_array_push(&recv, result.array->elems[i]);
        } else if (args[0].kind == VAL_INT) {
            int64_t idx = args[0].ival;
            if (idx < 0) idx += (int64_t)recv.array->len;
            if (idx >= 0) {
                /* Extend array if needed */
                while ((size_t)idx >= recv.array->len) val_array_push(&recv, val_nil());
                recv.array->elems[idx] = args[1];
            }
        }
        *out = args[1]; return 1;
    }
    if (strcmp(name, "replace") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Array#replace requires an argument"); return 1; }
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        if (args[0].kind != VAL_ARRAY) { *out = eval_raise_class(ev, site, "TypeError", "no implicit conversion into Array"); return 1; }
        if (args[0].array == recv.array) { *out = recv; return 1; }
        recv.array->len = 0;
        for (size_t i = 0; i < args[0].array->len; i++) val_array_push(&recv, args[0].array->elems[i]);
        *out = recv; return 1;
    }
    if (strcmp(name, "<=>") == 0) {
        if (argc < 1 || args[0].kind != VAL_ARRAY) { *out = val_nil(); return 1; }
        size_t alen = recv.array->len, blen = args[0].array->len;
        size_t minlen = alen < blen ? alen : blen;
        for (size_t i = 0; i < minlen; i++) {
            Value cmp = dispatch_method(ev, env, recv.array->elems[i], "<=>",
                                        &args[0].array->elems[i], 1, NULL, site, 0, 1);
            if (cmp.kind == VAL_INT && cmp.ival != 0) { *out = cmp; return 1; }
            if (cmp.kind == VAL_NIL) { *out = val_nil(); return 1; }
        }
        *out = val_int(alen < blen ? -1 : alen > blen ? 1 : 0);
        return 1;
    }
    if (strcmp(name, "reverse") == 0) {
        Value arr = val_array_new();
        for (size_t i = recv.array->len; i > 0; i--) val_array_push(&arr, recv.array->elems[i - 1]);
        *out = arr; return 1;
    }
    if (strcmp(name, "reverse_each") == 0) {
        if (!blk) {
            /* Return an enumerator-like (just return self reversed for now) */
            Value arr = val_array_new();
            for (size_t i = recv.array->len; i > 0; i--) val_array_push(&arr, recv.array->elems[i - 1]);
            *out = arr; return 1;
        }
        for (size_t i = recv.array->len; i > 0; i--) {
            Value elem = recv.array->elems[i - 1];
            Value r = call_block(ev, env, *blk, &elem, 1, site);
            if (r.kind == VAL_BREAK) { *out = *r.jump.wrapped; return 1; }
            if (r.kind == VAL_THROW) { *out = r; return 1; }
            if (val_is_signal(r)) { *out = r; return 1; }
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "to_s") == 0 || strcmp(name, "inspect") == 0) { *out = val_string(ev->arena, val_to_s(ev->arena, recv)); return 1; }
    if (strcmp(name, "to_a") == 0 || strcmp(name, "to_ary") == 0) { *out = recv; return 1; }
    if (strcmp(name, "join") == 0) {
        const char *sep = argc > 0 ? val_to_s(ev->arena, args[0]) : "";
        size_t seplen = strlen(sep);
        /* Collect string representations, dispatching to_s for user objects */
        const char *strs[4096]; size_t str_lens[4096];
        size_t n = recv.array->len < 4096 ? recv.array->len : 4096;
        size_t total = 1;
        for (size_t i = 0; i < n; i++) {
            Value elem = recv.array->elems[i];
            if (elem.kind == VAL_OBJECT || elem.kind == VAL_CLASS) {
                /* Call Ruby to_s */
                Value ts = dispatch_method(ev, env, elem, "to_s", NULL, 0, NULL, site, 0, -1);
                strs[i] = (!val_is_signal(ts) && ts.kind == VAL_STRING) ? ts.sval : val_to_s(ev->arena, elem);
            } else if (elem.kind == VAL_ARRAY) {
                /* Recursively join nested arrays */
                Value inner_out;
                dispatch_array(ev, env, elem, "join", args, argc, blk, site, &inner_out);
                strs[i] = (inner_out.kind == VAL_STRING) ? inner_out.sval : "";
            } else {
                strs[i] = val_to_s(ev->arena, elem);
            }
            str_lens[i] = strlen(strs[i]);
            total += str_lens[i] + (i > 0 ? seplen : 0);
        }
        char *buf = arena_alloc(ev->arena, total);
        buf[0] = '\0';
        for (size_t i = 0; i < n; i++) {
            if (i) { memcpy(buf + strlen(buf), sep, seplen + 1); }
            memcpy(buf + strlen(buf), strs[i], str_lens[i] + 1);
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
        if (!blk) {
            /* Return an Enumerator wrapping this array */
            Value enum_class;
            if (env_get(ev->top_env, "Enumerator", &enum_class) && enum_class.kind == VAL_CLASS) {
                Value r = dispatch_method(ev, env, enum_class, "new", &recv, 1, NULL, site, 0, 1);
                if (!val_is_signal(r)) { *out = r; return 1; }
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
            }
            *out = recv; return 1;
        }
        for (size_t i = 0; i < recv.array->len; i++) {
            Value arg = recv.array->elems[i];
            Value r = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv;
        return 1;
    }
    if (strcmp(name, "each_with_index") == 0) {
        if (!blk) {
            Value arr = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value pair = val_array_new();
                val_array_push(&pair, recv.array->elems[i]);
                val_array_push(&pair, val_int((int64_t)i));
                val_array_push(&arr, pair);
            }
            *out = wrap_result_as_enumerator(ev, env, arr, site);
            return 1;
        }
        for (size_t i = 0; i < recv.array->len; i++) {
            Value bargs[2] = { recv.array->elems[i], val_int((int64_t)i) };
            Value r = call_block(ev, env, *blk, bargs, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv;
        return 1;
    }
    if (strcmp(name, "each_with_object") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "each_with_object requires block and init"); return 1; }
        if (!blk) {
            Value arr = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value pair = val_array_new();
                val_array_push(&pair, recv.array->elems[i]);
                val_array_push(&pair, args[0]);
                val_array_push(&arr, pair);
            }
            *out = wrap_result_as_enumerator(ev, env, arr, site);
            return 1;
        }
        Value acc = args[0];
        for (size_t i = 0; i < recv.array->len; i++) {
            Value bargs[2] = { recv.array->elems[i], acc };
            Value r = call_block(ev, env, *blk, bargs, 2, site);
            if (val_is_signal(r)) { *out = r; return 1; }
        }
        *out = acc; return 1;
    }
    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, recv, site); return 1; }
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value arg = recv.array->elems[i];
            Value r = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            val_array_push(&result, r);
        }
        *out = result;
        return 1;
    }
    if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, recv, site); return 1; }
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value arg = recv.array->elems[i];
            Value r = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (val_truthy(r)) val_array_push(&result, arg);
        }
        *out = result;
        return 1;
    }
    if (strcmp(name, "reject") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, recv, site); return 1; }
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value arg = recv.array->elems[i];
            Value r = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (!val_truthy(r)) val_array_push(&result, arg);
        }
        *out = result;
        return 1;
    }
    if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
        /* Determine initial accumulator and optional symbol operator */
        const char *sym_op = NULL;
        size_t start = 0;
        Value acc;
        if (!blk) {
            /* symbol-only: reduce(:+) */
            if (argc == 1 && args[0].kind == VAL_SYMBOL) {
                sym_op = args[0].sval;
                if (recv.array->len == 0) { *out = val_nil(); return 1; }
                acc = recv.array->elems[start++];
            /* initial + symbol: reduce(0, :+) */
            } else if (argc == 2 && args[1].kind == VAL_SYMBOL) {
                acc = args[0]; sym_op = args[1].sval;
            } else {
                *out = eval_raise_class(ev, site, "LocalJumpError", "Array#reduce requires a block or symbol");
                return 1;
            }
        } else {
            acc = argc > 0 ? args[0] : recv.array->elems[start++];
        }
        for (size_t i = start; i < recv.array->len; i++) {
            Value r;
            if (sym_op) {
                r = dispatch_method(ev, env, acc, sym_op, &recv.array->elems[i], 1, NULL, site, 0, 1);
            } else {
                Value bargs[2] = { acc, recv.array->elems[i] };
                r = call_block(ev, env, *blk, bargs, 2, site);
            }
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            acc = r;
        }
        *out = acc;
        return 1;
    }
    if (strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0 ||
        strcmp(name, "none?") == 0 || strcmp(name, "one?") == 0) {
        int is_one = strcmp(name, "one?") == 0;
        if (!blk) {
            /* no-block: test element truthiness */
            int truthy_count = 0;
            for (size_t i = 0; i < recv.array->len; i++) {
                int t = val_truthy(recv.array->elems[i]);
                if (strcmp(name, "any?") == 0 && t)  { *out = val_true();  return 1; }
                if (strcmp(name, "all?") == 0 && !t) { *out = val_false(); return 1; }
                if (strcmp(name, "none?") == 0 && t) { *out = val_false(); return 1; }
                if (is_one && t) { truthy_count++; if (truthy_count > 1) { *out = val_false(); return 1; } }
            }
            if (is_one) { *out = val_bool(truthy_count == 1); return 1; }
            *out = strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0 ? val_true() : val_false();
            return 1;
        }
        {
            int truthy_count = 0;
            *out = (strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) ? val_true() : val_false();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value arg = recv.array->elems[i];
                Value r = call_block(ev, env, *blk, &arg, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_EXCEPTION) { *out = r; return 1; }
                int t = val_truthy(r);
                if (strcmp(name, "any?") == 0 && t) { *out = val_true(); return 1; }
                if (strcmp(name, "all?") == 0 && !t) { *out = val_false(); return 1; }
                if (strcmp(name, "none?") == 0 && t) { *out = val_false(); return 1; }
                if (is_one && t) { truthy_count++; if (truthy_count > 1) { *out = val_false(); return 1; } }
            }
            if (is_one) *out = val_bool(truthy_count == 1);
        }
        return 1;
    }
    if (strcmp(name, "min") == 0) {
        if (blk) return 0; /* min with block — fall through to Enumerable */
        if (argc > 0 && args[0].kind == VAL_INT) {
            /* min(n): return n smallest as sorted array */
            Value sorted_out;
            dispatch_array(ev, env, recv, "sort", NULL, 0, NULL, site, &sorted_out);
            if (sorted_out.kind != VAL_ARRAY) { *out = val_nil(); return 1; }
            int64_t n = args[0].ival;
            if (n < 0) { *out = eval_raise_class(ev, site, "ArgumentError", "count must be positive"); return 1; }
            if ((size_t)n > sorted_out.array->len) n = (int64_t)sorted_out.array->len;
            Value result = val_array_new();
            for (int64_t i = 0; i < n; i++) val_array_push(&result, sorted_out.array->elems[i]);
            *out = result; return 1;
        }
        if (recv.array->len == 0) { *out = val_nil(); return 1; }
        Value m = recv.array->elems[0];
        for (size_t i = 1; i < recv.array->len; i++) {
            Value cur = recv.array->elems[i];
            int less = 0;
            if (cur.kind == VAL_INT && m.kind == VAL_INT) less = cur.ival < m.ival;
            else if (cur.kind == VAL_FLOAT && m.kind == VAL_FLOAT) less = cur.fval < m.fval;
            else if (cur.kind == VAL_INT && m.kind == VAL_FLOAT) less = (double)cur.ival < m.fval;
            else if (cur.kind == VAL_FLOAT && m.kind == VAL_INT) less = cur.fval < (double)m.ival;
            else if ((cur.kind == VAL_STRING || cur.kind == VAL_SYMBOL) && cur.kind == m.kind)
                less = strcmp(cur.sval, m.sval) < 0;
            else {
                Value cmp = dispatch_method(ev, env, cur, "<=>", &m, 1, NULL, site, 0, 1);
                if (cmp.kind == VAL_INT) less = cmp.ival < 0;
            }
            if (less) m = cur;
        }
        *out = m;
        return 1;
    }
    if (strcmp(name, "max") == 0) {
        if (blk) return 0; /* max with block — fall through to Enumerable */
        if (argc > 0 && args[0].kind == VAL_INT) {
            /* max(n): return n largest as descending-sorted array */
            Value sorted_out;
            dispatch_array(ev, env, recv, "sort", NULL, 0, NULL, site, &sorted_out);
            if (sorted_out.kind != VAL_ARRAY) { *out = val_nil(); return 1; }
            int64_t n = args[0].ival;
            if (n < 0) { *out = eval_raise_class(ev, site, "ArgumentError", "count must be positive"); return 1; }
            size_t len = sorted_out.array->len;
            if ((size_t)n > len) n = (int64_t)len;
            Value result = val_array_new();
            for (int64_t i = (int64_t)len - 1; i >= (int64_t)len - n; i--)
                val_array_push(&result, sorted_out.array->elems[i]);
            *out = result; return 1;
        }
        if (recv.array->len == 0) { *out = val_nil(); return 1; }
        Value m = recv.array->elems[0];
        for (size_t i = 1; i < recv.array->len; i++) {
            Value cur = recv.array->elems[i];
            int greater = 0;
            if (cur.kind == VAL_INT && m.kind == VAL_INT) greater = cur.ival > m.ival;
            else if (cur.kind == VAL_FLOAT && m.kind == VAL_FLOAT) greater = cur.fval > m.fval;
            else if (cur.kind == VAL_INT && m.kind == VAL_FLOAT) greater = (double)cur.ival > m.fval;
            else if (cur.kind == VAL_FLOAT && m.kind == VAL_INT) greater = cur.fval > (double)m.ival;
            else if ((cur.kind == VAL_STRING || cur.kind == VAL_SYMBOL) && cur.kind == m.kind)
                greater = strcmp(cur.sval, m.sval) > 0;
            else {
                Value cmp = dispatch_method(ev, env, cur, "<=>", &m, 1, NULL, site, 0, 1);
                if (cmp.kind == VAL_INT) greater = cmp.ival > 0;
            }
            if (greater) m = cur;
        }
        *out = m;
        return 1;
    }
    if (strcmp(name, "sum") == 0) {
        Value acc = argc > 0 ? args[0] : val_int(0);
        for (size_t i = 0; i < recv.array->len; i++) {
            Value cur = blk ? call_block(ev, env, *blk, &recv.array->elems[i], 1, site)
                            : recv.array->elems[i];
            if (val_is_signal(cur)) { *out = cur; return 1; }
            if (acc.kind == VAL_INT && cur.kind == VAL_INT) {
                acc.ival += cur.ival;
            } else if ((acc.kind == VAL_INT || acc.kind == VAL_FLOAT) &&
                       (cur.kind == VAL_INT || cur.kind == VAL_FLOAT)) {
                double a = acc.kind == VAL_FLOAT ? acc.fval : (double)acc.ival;
                double c = cur.kind == VAL_FLOAT ? cur.fval : (double)cur.ival;
                acc = val_float(a + c);
            } else if (acc.kind == VAL_ARRAY && cur.kind == VAL_ARRAY) {
                /* Array concatenation: build new array */
                Value result = val_array_new();
                for (size_t j = 0; j < acc.array->len; j++) val_array_push(&result, acc.array->elems[j]);
                for (size_t j = 0; j < cur.array->len; j++) val_array_push(&result, cur.array->elems[j]);
                acc = result;
            } else if (acc.kind == VAL_STRING && cur.kind == VAL_STRING) {
                /* String concatenation */
                size_t alen = strlen(acc.sval), clen = strlen(cur.sval);
                char *buf = arena_alloc(ev->arena, alen + clen + 1);
                memcpy(buf, acc.sval, alen);
                memcpy(buf + alen, cur.sval, clen + 1);
                acc = val_string(ev->arena, buf);
            } else {
                /* General: dispatch + method */
                Value plus_args[1] = { cur };
                Value r = dispatch_method(ev, env, acc, "+", plus_args, 1, NULL, site, 0, -1);
                if (val_is_signal(r)) { *out = r; return 1; }
                acc = r;
            }
        }
        *out = acc;
        return 1;
    }
    if (strcmp(name, "chunk") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, val_array_new(), site); return 1; }
        Value result = val_array_new();
        if (recv.array->len == 0) { *out = result; return 1; }
        Value prev_key = val_nil();
        Value group = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value elem = recv.array->elems[i];
            Value key = call_block(ev, env, *blk, &elem, 1, site);
            if (val_is_signal(key)) { *out = key; return 1; }
            if (i == 0 || !val_equal(key, prev_key)) {
                if (i > 0) {
                    Value pair = val_array_new();
                    val_array_push(&pair, prev_key);
                    val_array_push(&pair, group);
                    val_array_push(&result, pair);
                }
                group = val_array_new();
                prev_key = key;
            }
            val_array_push(&group, elem);
        }
        Value pair = val_array_new();
        val_array_push(&pair, prev_key);
        val_array_push(&pair, group);
        val_array_push(&result, pair);
        *out = result; return 1;
    }
    if (strcmp(name, "chunk_while") == 0 || strcmp(name, "slice_when") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "ArgumentError", "tried to create Proc object without a block"); return 1; }
        int invert = (strcmp(name, "slice_when") == 0);
        Value result = val_array_new();
        if (recv.array->len == 0) { *out = result; return 1; }
        Value group = val_array_new();
        val_array_push(&group, recv.array->elems[0]);
        for (size_t i = 1; i < recv.array->len; i++) {
            Value bargs[2] = { recv.array->elems[i-1], recv.array->elems[i] };
            Value test = call_block(ev, env, *blk, bargs, 2, site);
            if (val_is_signal(test)) { *out = test; return 1; }
            int cont = val_truthy(test);
            if (invert) cont = !cont;
            if (!cont) {
                val_array_push(&result, group);
                group = val_array_new();
            }
            val_array_push(&group, recv.array->elems[i]);
        }
        val_array_push(&result, group);
        *out = result; return 1;
    }
    if (strcmp(name, "slice_before") == 0) {
        if (!blk && argc < 1) { *out = recv; return 1; }
        Value result = val_array_new();
        Value group = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value elem = recv.array->elems[i];
            int cut = 0;
            if (blk) {
                Value r = call_block(ev, env, *blk, &elem, 1, site);
                if (val_is_signal(r)) { *out = r; return 1; }
                cut = val_truthy(r);
            } else {
                cut = val_equal(elem, args[0]);
            }
            if (cut && group.array->len > 0) {
                val_array_push(&result, group);
                group = val_array_new();
            }
            val_array_push(&group, elem);
        }
        if (group.array->len > 0) val_array_push(&result, group);
        *out = result; return 1;
    }
    if (strcmp(name, "pack") == 0) {
        if (argc < 1 || args[0].kind != VAL_STRING) { *out = eval_raise_class(ev, site, "ArgumentError", "Array#pack requires a template string"); return 1; }
        const char *tmpl = args[0].sval;
        size_t out_cap = recv.array->len + 16, out_len = 0;
        char *out_buf = malloc(out_cap);
        if (!out_buf) { *out = val_nil(); return 1; }
        size_t ai = 0; /* array index */
        for (size_t ti = 0; tmpl[ti] && ai <= recv.array->len; ti++) {
            char dir = tmpl[ti];
            /* Parse count: * means all remaining, N means N times */
            int count = 1, star = 0;
            if (tmpl[ti+1] == '*') { star = 1; ti++; count = (int)(recv.array->len - ai); }
            else if (isdigit((unsigned char)tmpl[ti+1])) {
                count = 0;
                while (isdigit((unsigned char)tmpl[ti+1])) { count = count*10 + (tmpl[++ti] - '0'); }
            }
            for (int ci = 0; ci < count && ai < recv.array->len; ci++, ai++) {
                Value v = recv.array->elems[ai];
                if (out_len + 8 > out_cap) { out_cap *= 2; char *nb = realloc(out_buf, out_cap); if (!nb) { free(out_buf); *out = val_nil(); return 1; } out_buf = nb; }
                switch (dir) {
                    case 'C': case 'c': { uint8_t b = (uint8_t)(v.kind==VAL_INT ? v.ival : 0); out_buf[out_len++] = (char)b; break; }
                    case 'S': case 's': { uint16_t n2 = (uint16_t)(v.kind==VAL_INT ? v.ival : 0); memcpy(out_buf+out_len, &n2, 2); out_len+=2; break; }
                    case 'L': case 'l': { uint32_t n4 = (uint32_t)(v.kind==VAL_INT ? v.ival : 0); memcpy(out_buf+out_len, &n4, 4); out_len+=4; break; }
                    case 'Q': case 'q': { uint64_t n8 = (uint64_t)(v.kind==VAL_INT ? v.ival : 0); memcpy(out_buf+out_len, &n8, 8); out_len+=8; break; }
                    case 'N': { uint32_t n4 = (uint32_t)(v.kind==VAL_INT ? v.ival : 0); out_buf[out_len]=(n4>>24)&0xFF; out_buf[out_len+1]=(n4>>16)&0xFF; out_buf[out_len+2]=(n4>>8)&0xFF; out_buf[out_len+3]=n4&0xFF; out_len+=4; break; }
                    case 'n': { uint16_t n2 = (uint16_t)(v.kind==VAL_INT ? v.ival : 0); out_buf[out_len]=(n2>>8)&0xFF; out_buf[out_len+1]=n2&0xFF; out_len+=2; break; }
                    case 'V': { uint32_t n4 = (uint32_t)(v.kind==VAL_INT ? v.ival : 0); out_buf[out_len]=n4&0xFF; out_buf[out_len+1]=(n4>>8)&0xFF; out_buf[out_len+2]=(n4>>16)&0xFF; out_buf[out_len+3]=(n4>>24)&0xFF; out_len+=4; break; }
                    case 'v': { uint16_t n2 = (uint16_t)(v.kind==VAL_INT ? v.ival : 0); out_buf[out_len]=n2&0xFF; out_buf[out_len+1]=(n2>>8)&0xFF; out_len+=2; break; }
                    case 'f': { float f = (float)(v.kind==VAL_FLOAT ? v.fval : v.kind==VAL_INT ? (double)v.ival : 0.0); memcpy(out_buf+out_len, &f, 4); out_len+=4; break; }
                    case 'd': case 'D': { double d = v.kind==VAL_FLOAT ? v.fval : v.kind==VAL_INT ? (double)v.ival : 0.0; memcpy(out_buf+out_len, &d, 8); out_len+=8; break; }
                    case 'G': { /* big-endian double */
                        double d = v.kind==VAL_FLOAT ? v.fval : v.kind==VAL_INT ? (double)v.ival : 0.0;
                        uint8_t b[8]; memcpy(b, &d, 8);
                        for (int bi=7; bi>=0; bi--) out_buf[out_len++] = b[bi];
                        break; }
                    case 'g': { /* big-endian float */
                        float f = (float)(v.kind==VAL_FLOAT ? v.fval : v.kind==VAL_INT ? (double)v.ival : 0.0);
                        uint8_t b[4]; memcpy(b, &f, 4);
                        for (int bi=3; bi>=0; bi--) out_buf[out_len++] = b[bi];
                        break; }
                    case 'A': case 'a': case 'Z': {
                        const char *sv = (v.kind==VAL_STRING) ? v.sval : "";
                        size_t sl = strlen(sv);
                        while (out_len + sl + 2 > out_cap) { out_cap *= 2; char *nb = realloc(out_buf, out_cap); if (!nb) { free(out_buf); *out=val_nil(); return 1; } out_buf = nb; }
                        memcpy(out_buf+out_len, sv, sl); out_len += sl;
                        if (dir == 'Z') out_buf[out_len++] = '\0'; /* null-terminated */
                        break;
                    }
                    default: break;
                }
            }
            if (star) break;
        }
        char *result = arena_alloc(ev->arena, out_len + 1);
        memcpy(result, out_buf, out_len); result[out_len] = '\0';
        free(out_buf);
        *out = val_string_n(ev->arena, result, out_len);
        return 1;
    }
    if (strcmp(name, "transpose") == 0) {
        if (recv.array->len == 0) { *out = val_array_new(); return 1; }
        /* Determine column count from first sub-array */
        size_t ncols = 0;
        if (recv.array->elems[0].kind == VAL_ARRAY)
            ncols = recv.array->elems[0].array->len;
        Value result = val_array_new();
        for (size_t col = 0; col < ncols; col++) {
            Value row = val_array_new();
            for (size_t ri = 0; ri < recv.array->len; ri++) {
                Value src = recv.array->elems[ri];
                if (src.kind == VAL_ARRAY && col < src.array->len)
                    val_array_push(&row, src.array->elems[col]);
                else
                    val_array_push(&row, val_nil());
            }
            val_array_push(&result, row);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "flatten") == 0) {
        int depth = (argc > 0 && args[0].kind == VAL_INT) ? (int)args[0].ival :
                   (argc > 0 && args[0].kind == VAL_FLOAT) ? (int)args[0].fval : -1;
        Value result = val_array_new();
        array_flatten_into(ev, env, recv, &result, depth);
        *out = result;
        return 1;
    }
    if (strcmp(name, "uniq") == 0 || strcmp(name, "uniq!") == 0) {
        Value result = val_array_new();
        Value seen_keys = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value key = recv.array->elems[i];
            if (blk) {
                Value k = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
                if (val_is_signal(k)) { *out = k; return 1; }
                key = k;
            }
            int found = 0;
            for (size_t j = 0; j < seen_keys.array->len; j++)
                if (val_equal(seen_keys.array->elems[j], key)) { found = 1; break; }
            if (!found) {
                val_array_push(&seen_keys, key);
                val_array_push(&result, recv.array->elems[i]);
            }
        }
        if (strcmp(name, "uniq!") == 0) {
            if (result.array->len == recv.array->len) { *out = val_nil(); return 1; }
            recv.array->len = 0;
            for (size_t i = 0; i < result.array->len; i++) val_array_push(&recv, result.array->elems[i]);
            *out = recv; return 1;
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
                if (blk) {
                    Value pair[2] = { key, prev };
                    Value cmp = call_block(ev, env, *blk, pair, 2, site);
                    if (val_is_signal(cmp)) { *out = cmp; return 1; }
                    less = cmp.kind == VAL_INT ? cmp.ival < 0 : 0;
                } else if (key.kind == VAL_INT && prev.kind == VAL_INT) less = key.ival < prev.ival;
                else if (key.kind == VAL_FLOAT && prev.kind == VAL_FLOAT) less = key.fval < prev.fval;
                else if (key.kind == VAL_INT && prev.kind == VAL_FLOAT) less = (double)key.ival < prev.fval;
                else if (key.kind == VAL_FLOAT && prev.kind == VAL_INT) less = key.fval < (double)prev.ival;
                else if (key.kind == VAL_STRING && prev.kind == VAL_STRING) less = strcmp(key.sval, prev.sval) < 0;
                else if (key.kind == VAL_SYMBOL && prev.kind == VAL_SYMBOL) less = strcmp(key.sval, prev.sval) < 0;
                else {
                    Value cmp = dispatch_method(ev, env, key, "<=>", &prev, 1, NULL, site, 0, 1);
                    if (val_is_signal(cmp)) { *out = cmp; return 1; }
                    less = cmp.kind == VAL_INT ? cmp.ival < 0 : 0;
                }
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
    if (strcmp(name, "compact!") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        size_t w = 0;
        for (size_t i = 0; i < recv.array->len; i++)
            if (recv.array->elems[i].kind != VAL_NIL) recv.array->elems[w++] = recv.array->elems[i];
        int changed = w != recv.array->len;
        recv.array->len = w;
        *out = changed ? recv : val_nil();
        return 1;
    }
    if (strcmp(name, "flatten!") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        int depth = (argc > 0 && args[0].kind == VAL_INT) ? (int)args[0].ival :
                   (argc > 0 && args[0].kind == VAL_FLOAT) ? (int)args[0].fval : -1;
        Value result = val_array_new();
        array_flatten_into(ev, env, recv, &result, depth);
        int changed = result.array->len != recv.array->len;
        if (!changed) {
            for (size_t i = 0; i < recv.array->len && !changed; i++)
                if (!val_equal(recv.array->elems[i], result.array->elems[i])) changed = 1;
        }
        recv.array->len = 0;
        for (size_t i = 0; i < result.array->len; i++) val_array_push(&recv, result.array->elems[i]);
        *out = changed ? recv : val_nil();
        return 1;
    }
    if (strcmp(name, "uniq!") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        size_t orig = recv.array->len;
        Value seen = val_array_new();
        size_t w = 0;
        for (size_t i = 0; i < recv.array->len; i++) {
            int found = 0;
            for (size_t j = 0; j < seen.array->len; j++) if (val_equal(seen.array->elems[j], recv.array->elems[i])) { found = 1; break; }
            if (!found) { val_array_push(&seen, recv.array->elems[i]); recv.array->elems[w++] = recv.array->elems[i]; }
        }
        recv.array->len = w;
        *out = w != orig ? recv : val_nil();
        return 1;
    }
    if (strcmp(name, "reverse!") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        size_t n = recv.array->len;
        for (size_t i = 0; i < n / 2; i++) {
            Value tmp = recv.array->elems[i];
            recv.array->elems[i] = recv.array->elems[n - 1 - i];
            recv.array->elems[n - 1 - i] = tmp;
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "sort!") == 0) {
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        Value sorted = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) val_array_push(&sorted, recv.array->elems[i]);
        Value sort_out;
        dispatch_array(ev, env, sorted, "sort", args, argc, blk, site, &sort_out);
        if (sort_out.kind == VAL_ARRAY) {
            for (size_t i = 0; i < sort_out.array->len; i++) recv.array->elems[i] = sort_out.array->elems[i];
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "bsearch") == 0 || strcmp(name, "bsearch_index") == 0) {
        int want_index = (strcmp(name, "bsearch_index") == 0);
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, recv, site); return 1; }
        size_t n = recv.array->len;
        if (n == 0) { *out = val_nil(); return 1; }

        size_t lo = 0, hi = n;
        size_t first_mid = lo + (hi - lo) / 2;
        int have_first = 1;
        Value first = call_block(ev, env, *blk, &recv.array->elems[first_mid], 1, site);
        if (ev->errored) { *out = val_nil(); return 1; }
        if (flow_signal_out(first, out)) return 1;
        int numeric_mode = (first.kind == VAL_INT || first.kind == VAL_FLOAT);
        if (!(numeric_mode || first.kind == VAL_BOOL || first.kind == VAL_NIL)) {
            *out = eval_raise_class(ev, site, "TypeError",
                                    "wrong argument type %s (must be numeric, true, false or nil)",
                                    value_class_name(ev, first));
            return 1;
        }

        if (numeric_mode) {
            while (lo < hi) {
                size_t mid = lo + (hi - lo) / 2;
                Value r;
                if (have_first && mid == first_mid) {
                    r = first;
                    have_first = 0;
                } else {
                    r = call_block(ev, env, *blk, &recv.array->elems[mid], 1, site);
                    if (ev->errored) { *out = val_nil(); return 1; }
                    if (flow_signal_out(r, out)) return 1;
                }
                if (!(r.kind == VAL_INT || r.kind == VAL_FLOAT)) {
                    *out = eval_raise_class(ev, site, "TypeError",
                                            "wrong argument type %s (must be numeric)",
                                            value_class_name(ev, r));
                    return 1;
                }
                double cmp = r.kind == VAL_INT ? (double)r.ival : r.fval;
                if (cmp == 0.0) { *out = want_index ? val_int((int64_t)mid) : recv.array->elems[mid]; return 1; }
                if (cmp > 0.0) lo = mid + 1;
                else hi = mid;
            }
            *out = val_nil();
            return 1;
        }

        /* find-minimum mode: block returns true for the target range */
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            Value r;
            if (have_first && mid == first_mid) {
                r = first;
                have_first = 0;
            } else {
                r = call_block(ev, env, *blk, &recv.array->elems[mid], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            if (!(r.kind == VAL_BOOL || r.kind == VAL_NIL)) {
                *out = eval_raise_class(ev, site, "TypeError",
                                        "wrong argument type %s (must be numeric, true, false or nil)",
                                        value_class_name(ev, r));
                return 1;
            }
            if (r.kind == VAL_BOOL && r.bval) hi = mid;
            else lo = mid + 1;
        }
        if (lo >= n) { *out = val_nil(); return 1; }
        *out = want_index ? val_int((int64_t)lo) : recv.array->elems[lo];
        return 1;
    }
    if (strcmp(name, "map!" ) == 0 || strcmp(name, "collect!") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, recv, site); return 1; }
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        for (size_t i = 0; i < recv.array->len; i++) {
            Value r = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            recv.array->elems[i] = r;
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "select!") == 0 || strcmp(name, "filter!") == 0 || strcmp(name, "keep_if") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, recv, site); return 1; }
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        size_t w = 0;
        size_t orig = recv.array->len;
        for (size_t i = 0; i < recv.array->len; i++) {
            Value r = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (val_truthy(r)) recv.array->elems[w++] = recv.array->elems[i];
        }
        recv.array->len = w;
        *out = (strcmp(name, "keep_if") == 0 || w != orig) ? recv : val_nil();
        return 1;
    }
    if (strcmp(name, "reject!") == 0 || strcmp(name, "delete_if") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, recv, site); return 1; }
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        size_t w = 0;
        size_t orig = recv.array->len;
        for (size_t i = 0; i < recv.array->len; i++) {
            Value r = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (!val_truthy(r)) recv.array->elems[w++] = recv.array->elems[i];
        }
        recv.array->len = w;
        *out = (strcmp(name, "delete_if") == 0 || w != orig) ? recv : val_nil();
        return 1;
    }
    if (strcmp(name, "slice!") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        if (recv.array->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Array"); return 1; }
        int64_t alen = (int64_t)recv.array->len;
        int64_t idx, len;
        if (args[0].kind == VAL_RANGE) {
            RubyRange *r = args[0].range;
            idx = r->begin_val.kind == VAL_INT ? r->begin_val.ival : 0;
            int64_t end = r->end_val.kind == VAL_INT ? r->end_val.ival : alen;
            if (idx < 0) idx += alen;
            if (end < 0) end += alen;
            if (!r->exclusive) end++;
            len = end - idx;
        } else if (args[0].kind == VAL_INT) {
            idx = args[0].ival;
            if (idx < 0) idx += alen;
            len = (argc >= 2 && args[1].kind == VAL_INT) ? args[1].ival : 1;
        } else { *out = val_nil(); return 1; }
        if (idx < 0 || idx > alen || len < 0) { *out = val_nil(); return 1; }
        if (idx + len > alen) len = alen - idx;
        /* Extract the removed portion */
        Value removed = val_array_new();
        for (int64_t i = idx; i < idx + len; i++) val_array_push(&removed, recv.array->elems[i]);
        /* Compact remaining elements */
        int64_t tail = alen - (idx + len);
        for (int64_t i = 0; i < tail; i++) recv.array->elems[idx + i] = recv.array->elems[idx + len + i];
        recv.array->len = (size_t)(alen - len);
        /* Return removed element (not array) for single-index form without length */
        if (argc == 1 && args[0].kind == VAL_INT && removed.array->len == 1)
            *out = removed.array->elems[0];
        else
            *out = removed;
        return 1;
    }
    if (strcmp(name, "to_h") == 0) {
        Value result = val_hash_new(ev->arena);
        if (blk) {
            for (size_t i = 0; i < recv.array->len; i++) {
                Value r = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                if (r.kind == VAL_ARRAY && r.array->len >= 2)
                    val_hash_set(result.hash, r.array->elems[0], r.array->elems[1]);
            }
        } else {
            for (size_t i = 0; i < recv.array->len; i++) {
                Value pair = recv.array->elems[i];
                if (pair.kind == VAL_ARRAY && pair.array->len >= 2)
                    val_hash_set(result.hash, pair.array->elems[0], pair.array->elems[1]);
            }
        }
        *out = result; return 1;
    }
    if (strcmp(name, "with_index") == 0) {
        int64_t offset = (argc > 0 && args[0].kind == VAL_INT) ? args[0].ival : 0;
        if (blk) {
            /* collect block results — enables arr.map.with_index { |x,i| ... } */
            Value result = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) {
                Value bargs[2] = { recv.array->elems[i], val_int((int64_t)i + offset) };
                Value r = call_block(ev, env, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                val_array_push(&result, r);
            }
            *out = result; return 1;
        }
        Value arr = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            Value pair = val_array_new();
            val_array_push(&pair, recv.array->elems[i]);
            val_array_push(&pair, val_int((int64_t)i + offset));
            val_array_push(&arr, pair);
        }
        *out = arr; return 1;
    }
    if (strcmp(name, "values_at") == 0) {
        Value result = val_array_new();
        for (int i = 0; i < argc; i++) {
            if (args[i].kind == VAL_INT) {
                int64_t idx = args[i].ival;
                if (idx < 0) idx += (int64_t)recv.array->len;
                if (idx >= 0 && (size_t)idx < recv.array->len)
                    val_array_push(&result, recv.array->elems[idx]);
                else
                    val_array_push(&result, val_nil());
            }
        }
        *out = result; return 1;
    }
    if (strcmp(name, "index") == 0 || strcmp(name, "find_index") == 0) {
        if (blk) {
            for (size_t i = 0; i < recv.array->len; i++) {
                Value r = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                if (val_truthy(r)) { *out = val_int((int64_t)i); return 1; }
            }
            *out = val_nil(); return 1;
        }
        if (argc < 1) { *out = val_nil(); return 1; }
        for (size_t i = 0; i < recv.array->len; i++)
            if (val_equal(recv.array->elems[i], args[0])) { *out = val_int((int64_t)i); return 1; }
        *out = val_nil(); return 1;
    }
    if (strcmp(name, "rindex") == 0) {
        if (blk) {
            for (size_t i = recv.array->len; i-- > 0;) {
                Value r = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                if (val_truthy(r)) { *out = val_int((int64_t)i); return 1; }
            }
            *out = val_nil(); return 1;
        }
        if (argc < 1) { *out = val_nil(); return 1; }
        for (size_t i = recv.array->len; i-- > 0;)
            if (val_equal(recv.array->elems[i], args[0])) { *out = val_int((int64_t)i); return 1; }
        *out = val_nil(); return 1;
    }
    if (strcmp(name, "cycle") == 0) {
        int64_t n = (argc > 0 && args[0].kind == VAL_INT) ? args[0].ival : -1;
        if (n == 0 || recv.array->len == 0) { *out = val_nil(); return 1; }
        if (blk) {
            int64_t count = 0;
            while (n < 0 || count < n) {
                for (size_t i = 0; i < recv.array->len; i++) {
                    Value r = call_block(ev, env, *blk, &recv.array->elems[i], 1, site);
                    if (ev->errored) { *out = val_nil(); return 1; }
                    if (flow_signal_out(r, out)) return 1;
                }
                count++;
                if (n > 0 && count >= n) break;
            }
            *out = val_nil(); return 1;
        }
        if (n < 0) { *out = eval_raise_class(ev, site, "TypeError", "Array#cycle requires a count when no block"); return 1; }
        Value result = val_array_new();
        for (int64_t c = 0; c < n; c++)
            for (size_t i = 0; i < recv.array->len; i++)
                val_array_push(&result, recv.array->elems[i]);
        *out = result; return 1;
    }
    if (strcmp(name, "sample") == 0) {
        if (recv.array->len == 0) { *out = argc > 0 ? val_array_new() : val_nil(); return 1; }
        if (argc > 0 && args[0].kind == VAL_INT) {
            int64_t n = args[0].ival;
            if (n < 0) { *out = eval_raise_class(ev, site, "ArgumentError", "negative sample number"); return 1; }
            if ((size_t)n > recv.array->len) n = (int64_t)recv.array->len;
            /* Simple Fisher-Yates on a copy */
            Value tmp = val_array_new();
            for (size_t i = 0; i < recv.array->len; i++) val_array_push(&tmp, recv.array->elems[i]);
            Value result = val_array_new();
            for (int64_t i = 0; i < n; i++) {
                size_t j = i + (size_t)rand() % (tmp.array->len - (size_t)i);
                Value t = tmp.array->elems[i]; tmp.array->elems[i] = tmp.array->elems[j]; tmp.array->elems[j] = t;
                val_array_push(&result, tmp.array->elems[i]);
            }
            *out = result; return 1;
        }
        size_t idx = (size_t)rand() % recv.array->len;
        *out = recv.array->elems[idx]; return 1;
    }
    if (strcmp(name, "shuffle") == 0 || strcmp(name, "shuffle!") == 0) {
        size_t n = recv.array->len;
        Value result = val_array_new();
        for (size_t i = 0; i < n; i++) val_array_push(&result, recv.array->elems[i]);
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = (size_t)rand() % (i + 1);
            Value tmp = result.array->elems[i];
            result.array->elems[i] = result.array->elems[j];
            result.array->elems[j] = tmp;
        }
        if (strcmp(name, "shuffle!") == 0) {
            for (size_t i = 0; i < n; i++) recv.array->elems[i] = result.array->elems[i];
            *out = recv;
        } else {
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "-") == 0) {
        if (argc < 1 || args[0].kind != VAL_ARRAY) { *out = eval_raise_class(ev, site, "TypeError", "Array#- requires an Array"); return 1; }
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            int found = 0;
            for (size_t j = 0; j < args[0].array->len; j++)
                if (val_equal(recv.array->elems[i], args[0].array->elems[j])) { found = 1; break; }
            if (!found) val_array_push(&result, recv.array->elems[i]);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "intersection") == 0) {
        /* Ruby 2.7+ named form — same as & but accepts multiple arrays */
        Value result = recv;
        for (int ai = 0; ai < argc; ai++) {
            if (args[ai].kind != VAL_ARRAY) continue;
            Value inter_args[1] = { args[ai] };
            Value inter_out;
            dispatch_array(ev, env, result, "&", inter_args, 1, NULL, site, &inter_out);
            result = inter_out;
        }
        *out = result; return 1;
    }
    if (strcmp(name, "union") == 0) {
        Value result = recv;
        for (int ai = 0; ai < argc; ai++) {
            if (args[ai].kind != VAL_ARRAY) continue;
            Value u_args[1] = { args[ai] };
            Value u_out;
            dispatch_array(ev, env, result, "|", u_args, 1, NULL, site, &u_out);
            result = u_out;
        }
        *out = result; return 1;
    }
    if (strcmp(name, "difference") == 0) {
        Value result = recv;
        for (int ai = 0; ai < argc; ai++) {
            if (args[ai].kind != VAL_ARRAY) continue;
            Value d_args[1] = { args[ai] };
            Value d_out;
            dispatch_array(ev, env, result, "-", d_args, 1, NULL, site, &d_out);
            result = d_out;
        }
        *out = result; return 1;
    }
    if (strcmp(name, "&") == 0) {
        if (argc < 1 || args[0].kind != VAL_ARRAY) { *out = eval_raise_class(ev, site, "TypeError", "Array#& requires an Array"); return 1; }
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            int in_other = 0, already = 0;
            for (size_t j = 0; j < args[0].array->len; j++)
                if (val_equal(recv.array->elems[i], args[0].array->elems[j])) { in_other = 1; break; }
            if (!in_other) continue;
            for (size_t j = 0; j < result.array->len; j++)
                if (val_equal(recv.array->elems[i], result.array->elems[j])) { already = 1; break; }
            if (!already) val_array_push(&result, recv.array->elems[i]);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "|") == 0) {
        if (argc < 1 || args[0].kind != VAL_ARRAY) { *out = eval_raise_class(ev, site, "TypeError", "Array#| requires an Array"); return 1; }
        Value result = val_array_new();
        for (size_t i = 0; i < recv.array->len; i++) {
            int found = 0;
            for (size_t j = 0; j < result.array->len; j++)
                if (val_equal(recv.array->elems[i], result.array->elems[j])) { found = 1; break; }
            if (!found) val_array_push(&result, recv.array->elems[i]);
        }
        for (size_t i = 0; i < args[0].array->len; i++) {
            int found = 0;
            for (size_t j = 0; j < result.array->len; j++)
                if (val_equal(args[0].array->elems[i], result.array->elems[j])) { found = 1; break; }
            if (!found) val_array_push(&result, args[0].array->elems[i]);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "*") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Array#* requires an argument"); return 1; }
        if (args[0].kind == VAL_INT) {
            int64_t times = args[0].ival;
            if (times < 0) { *out = eval_raise_class(ev, site, "ArgumentError", "Array#* count cannot be negative"); return 1; }
            Value result = val_array_new();
            for (int64_t t = 0; t < times; t++)
                for (size_t i = 0; i < recv.array->len; i++)
                    val_array_push(&result, recv.array->elems[i]);
            *out = result; return 1;
        }
        if (args[0].kind == VAL_STRING) {
            /* join with separator */
            return dispatch_array(ev, env, recv, "join", args, 1, blk, site, out);
        }
        *out = eval_raise_class(ev, site, "TypeError", "Array#* argument must be Integer or String"); return 1;
    }
    if (strcmp(name, "combination") == 0) {
        if (argc < 1 || args[0].kind != VAL_INT) { *out = eval_raise_class(ev, site, "ArgumentError", "Array#combination requires an Integer"); return 1; }
        size_t n = recv.array->len;
        size_t k = (size_t)args[0].ival;
        Value result = val_array_new();
        if (k == 0) { val_array_push(&result, val_array_new()); }
        else if (k <= n) {
            Value current[64];
            combination_helper(recv.array->elems, n, k, 0, current, 0, &result);
        }
        if (blk) {
            for (size_t i = 0; i < result.array->len; i++) {
                Value r = call_block(ev, env, *blk, &result.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        } else {
            *out = wrap_result_as_enumerator(ev, env, result, site);
        }
        return 1;
    }
    if (strcmp(name, "permutation") == 0) {
        size_t n = recv.array->len;
        size_t k = (argc > 0 && args[0].kind == VAL_INT) ? (size_t)args[0].ival : n;
        Value result = val_array_new();
        if (k == 0) { val_array_push(&result, val_array_new()); }
        else if (k <= n) {
            int used[64] = {0};
            Value current[64];
            permutation_helper(recv.array->elems, n, k, used, current, 0, &result);
        }
        if (blk) {
            for (size_t i = 0; i < result.array->len; i++) {
                Value r = call_block(ev, env, *blk, &result.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        } else {
            *out = wrap_result_as_enumerator(ev, env, result, site);
        }
        return 1;
    }
    if (strcmp(name, "repeated_combination") == 0) {
        size_t n = recv.array->len;
        size_t k = (argc > 0 && args[0].kind == VAL_INT) ? (size_t)args[0].ival : 0;
        Value result = val_array_new();
        if (k == 0) { val_array_push(&result, val_array_new()); }
        else if (n > 0) {
            Value current[64];
            repeated_combination_helper(recv.array->elems, n, k, 0, current, 0, &result);
        }
        if (blk) {
            for (size_t i = 0; i < result.array->len; i++) {
                Value r = call_block(ev, env, *blk, &result.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        } else { *out = wrap_result_as_enumerator(ev, env, result, site); }
        return 1;
    }
    if (strcmp(name, "repeated_permutation") == 0) {
        size_t n = recv.array->len;
        size_t k = (argc > 0 && args[0].kind == VAL_INT) ? (size_t)args[0].ival : n;
        Value result = val_array_new();
        if (k == 0) { val_array_push(&result, val_array_new()); }
        else if (n > 0) {
            Value current[64];
            repeated_permutation_helper(recv.array->elems, n, k, current, 0, &result);
        }
        if (blk) {
            for (size_t i = 0; i < result.array->len; i++) {
                Value r = call_block(ev, env, *blk, &result.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        } else { *out = wrap_result_as_enumerator(ev, env, result, site); }
        return 1;
    }
    if (strcmp(name, "product") == 0) {
        Value all_arrays[65];
        all_arrays[0] = recv;
        int narrays = 1;
        for (int i = 0; i < argc && narrays < 65; i++) {
            if (args[i].kind != VAL_ARRAY) { *out = eval_raise_class(ev, site, "TypeError", "Array#product requires Arrays"); return 1; }
            all_arrays[narrays++] = args[i];
        }
        Value result = val_array_new();
        Value current[65];
        product_helper(all_arrays, narrays, 0, current, &result);
        if (blk) {
            for (size_t i = 0; i < result.array->len; i++) {
                Value r = call_block(ev, env, *blk, &result.array->elems[i], 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            }
            *out = recv;
        } else {
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "rotate") == 0 || strcmp(name, "rotate!") == 0) {
        size_t n = recv.array->len;
        if (n == 0) { *out = recv; return 1; }
        int64_t by = (argc > 0 && args[0].kind == VAL_INT) ? args[0].ival : 1;
        by = ((by % (int64_t)n) + (int64_t)n) % (int64_t)n;
        Value result = val_array_new();
        for (size_t i = 0; i < n; i++) val_array_push(&result, recv.array->elems[(i + (size_t)by) % n]);
        if (strcmp(name, "rotate!") == 0) {
            for (size_t i = 0; i < n; i++) recv.array->elems[i] = result.array->elems[i];
            *out = recv;
        } else {
            *out = result;
        }
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
            if (blk) {
                Value r = call_block(ev, env, *blk, &pair, 1, site);
                if (ev->errored || val_is_signal(r)) { *out = r; return 1; }
            } else {
                val_array_push(&result, pair);
            }
        }
        *out = blk ? val_nil() : result;
        return 1;
    }
    if (strcmp(name, "assoc") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        for (size_t i = 0; i < recv.array->len; i++) {
            Value elem = recv.array->elems[i];
            if (elem.kind == VAL_ARRAY && elem.array->len > 0 && val_equal(elem.array->elems[0], args[0])) {
                *out = elem; return 1;
            }
        }
        *out = val_nil(); return 1;
    }
    if (strcmp(name, "rassoc") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        for (size_t i = 0; i < recv.array->len; i++) {
            Value elem = recv.array->elems[i];
            if (elem.kind == VAL_ARRAY && elem.array->len > 1 && val_equal(elem.array->elems[1], args[0])) {
                *out = elem; return 1;
            }
        }
        *out = val_nil(); return 1;
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
            if (val_hash_get(h, args[0], &found)) {
                *out = found;
            } else if (h->default_proc.kind == VAL_BLOCK) {
                Value block_args[2] = { recv, args[0] };
                *out = call_block(ev, env, h->default_proc, block_args, 2, site);
            } else {
                *out = h->default_value;
            }
        }
        return 1;
    }
    if (strcmp(name, "[]=") == 0 || strcmp(name, "store") == 0) {
        if (argc < 2) { *out = eval_raise_class(ev, site, "ArgumentError", "Hash#[]= requires key and value"); return 1; }
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        val_hash_set(h, args[0], args[1]);
        sync_process_env_pair(ev, h, args[0], args[1]);
        *out = args[1];
        return 1;
    }
    if (strcmp(name, "fetch") == 0) {
        if (argc < 1) *out = eval_raise_class(ev, site, "ArgumentError", "Hash#fetch requires a key");
        else {
            Value found;
            if (val_hash_get(h, args[0], &found)) *out = found;
            else if (argc > 1) *out = args[1];
            else if (blk) *out = call_block(ev, env, *blk, &args[0], 1, site);
            else *out = eval_raise_class(ev, site, "KeyError", "key not found: %s", val_inspect(ev->arena, args[0]));
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
            if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
            Value found;
            int ok = val_hash_get(h, args[0], &found);
            val_hash_delete(h, args[0]);
            sync_process_env_delete(ev, h, args[0]);
            if (ok) *out = found;
            else if (blk) *out = call_block(ev, env, *blk, &args[0], 1, site);
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
    if (strcmp(name, "length") == 0 || strcmp(name, "size") == 0) { *out = val_int((int64_t)h->len); return 1; }
    if (strcmp(name, "count") == 0 && !blk) { *out = val_int((int64_t)h->len); return 1; }
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
        Value result = val_hash_new_with_defaults(ev->arena, h->default_value, h->default_proc);
        result.hash->compare_by_identity = h->compare_by_identity;
        for (size_t i = 0; i < h->len; i++) val_hash_set(result.hash, h->keys[i], h->vals[i]);
        for (int i = 0; i < argc; i++) {
            if (args[i].kind != VAL_HASH) { *out = eval_raise_class(ev, site, "TypeError", "Hash#merge requires Hash arguments"); return 1; }
            RubyHash *other = args[i].hash;
            if (blk) {
                for (size_t j = 0; j < other->len; j++) {
                    Value existing;
                    if (val_hash_get(result.hash, other->keys[j], &existing)) {
                        Value bargs[3] = { other->keys[j], existing, other->vals[j] };
                        Value r = call_block(ev, env, *blk, bargs, 3, site);
                        if (ev->errored) { *out = val_nil(); return 1; }
                        if (flow_signal_out(r, out)) return 1;
                        val_hash_set(result.hash, other->keys[j], r);
                    } else {
                        val_hash_set(result.hash, other->keys[j], other->vals[j]);
                    }
                }
            } else {
                for (size_t j = 0; j < other->len; j++) val_hash_set(result.hash, other->keys[j], other->vals[j]);
            }
        }
        *out = result; return 1;
    }
    if (strcmp(name, "merge!") == 0 || strcmp(name, "update") == 0) {
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        for (int i = 0; i < argc; i++) {
            if (args[i].kind != VAL_HASH) { *out = eval_raise_class(ev, site, "TypeError", "Hash#merge! requires Hash arguments"); return 1; }
            RubyHash *other = args[i].hash;
            for (size_t j = 0; j < other->len; j++) {
                Value existing = val_nil();
                if (blk && val_hash_get(h, other->keys[j], &existing)) {
                    /* Conflict: call block(key, old_value, new_value) */
                    Value bargs[3] = { other->keys[j], existing, other->vals[j] };
                    Value resolved = call_block(ev, env, *blk, bargs, 3, site);
                    if (val_is_signal(resolved)) { *out = resolved; return 1; }
                    val_hash_set(h, other->keys[j], resolved);
                } else {
                    val_hash_set(h, other->keys[j], other->vals[j]);
                }
            }
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "dig") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        Value current;
        if (!val_hash_get(h, args[0], &current)) { *out = val_nil(); return 1; }
        for (int i = 1; i < argc; i++) {
            if (current.kind == VAL_HASH) {
                Value next;
                if (!val_hash_get(current.hash, args[i], &next)) { *out = val_nil(); return 1; }
                current = next;
            } else if (current.kind == VAL_ARRAY) {
                if (args[i].kind != VAL_INT) { *out = val_nil(); return 1; }
                int64_t idx = args[i].ival;
                if (idx < 0) idx += (int64_t)current.array->len;
                if (idx < 0 || (size_t)idx >= current.array->len) { *out = val_nil(); return 1; }
                current = current.array->elems[idx];
            } else { *out = val_nil(); return 1; }
        }
        *out = current; return 1;
    }
    if (strcmp(name, "find") == 0 || strcmp(name, "detect") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        for (size_t i = 0; i < h->len; i++) {
            Value pair[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, pair, 2, site);
            if (val_is_signal(r)) { *out = r; return 1; }
            if (val_truthy(r)) {
                Value result = val_array_new();
                val_array_push(&result, h->keys[i]);
                val_array_push(&result, h->vals[i]);
                *out = result; return 1;
            }
        }
        *out = val_nil(); return 1;
    }
    if (strcmp(name, "find_all") == 0 || strcmp(name, "filter") == 0 || strcmp(name, "select") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        Value result = val_hash_new(ev->arena);
        result.hash->compare_by_identity = h->compare_by_identity;
        for (size_t i = 0; i < h->len; i++) {
            Value pair[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, pair, 2, site);
            if (val_is_signal(r)) { *out = r; return 1; }
            if (val_truthy(r)) val_hash_set(result.hash, h->keys[i], h->vals[i]);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "flat_map") == 0 || strcmp(name, "collect_concat") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        Value result = val_array_new();
        for (size_t i = 0; i < h->len; i++) {
            Value pair[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, pair, 2, site);
            if (val_is_signal(r)) { *out = r; return 1; }
            if (r.kind == VAL_ARRAY) {
                for (size_t j = 0; j < r.array->len; j++) val_array_push(&result, r.array->elems[j]);
            } else { val_array_push(&result, r); }
        }
        *out = result; return 1;
    }
    if (strcmp(name, "each") == 0 || strcmp(name, "each_pair") == 0) {
        if (!blk) {
            /* Return Enumerator wrapping the key-value pairs as arrays */
            Value enum_class;
            if (env_get(ev->top_env, "Enumerator", &enum_class) && enum_class.kind == VAL_CLASS) {
                Value pairs = val_array_new();
                for (size_t i = 0; i < h->len; i++) {
                    Value pair = val_array_new();
                    val_array_push(&pair, h->keys[i]);
                    val_array_push(&pair, h->vals[i]);
                    val_array_push(&pairs, pair);
                }
                Value r = dispatch_method(ev, env, enum_class, "new", &pairs, 1, NULL, site, 0, 1);
                if (!val_is_signal(r)) { *out = r; return 1; }
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
            }
            *out = recv; return 1;
        }
        for (size_t i = 0; i < h->len; i++) {
            /* Determine block arity to decide yield mode.
               MRI Hash#each: |k,v| gets key+value; |x| gets [key,value] pair.
               We always pass 2 args; block param binding handles destructuring. */
            Value bargs[2] = { h->keys[i], h->vals[i] };
            int block_arity = -1;
            if (blk->block.block_node && blk->block.block_node->kind == NODE_BLOCK) {
                int cnt = 0; NodeList *pl = blk->block.block_node->block.params;
                for (; pl; pl = pl->next) cnt++;
                block_arity = cnt;
            }
            Value r;
            if (block_arity == 1) {
                /* Single-param block: yield [key, value] pair as one arg */
                Value pair = val_array_new();
                val_array_push(&pair, h->keys[i]);
                val_array_push(&pair, h->vals[i]);
                r = call_block(ev, env, *blk, &pair, 1, site);
            } else {
                r = call_block(ev, env, *blk, bargs, 2, site);
            }
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv;
        return 1;
    }
    if (strcmp(name, "each_with_index") == 0) {
        if (!blk) {
            Value arr = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value pair = val_array_new();
                Value entry = val_array_new();
                val_array_push(&entry, h->keys[i]);
                val_array_push(&entry, h->vals[i]);
                val_array_push(&pair, entry);
                val_array_push(&pair, val_int((int64_t)i));
                val_array_push(&arr, pair);
            }
            *out = wrap_result_as_enumerator(ev, env, arr, site);
            return 1;
        }
        for (size_t i = 0; i < h->len; i++) {
            Value pair = val_array_new();
            val_array_push(&pair, h->keys[i]);
            val_array_push(&pair, h->vals[i]);
            Value block_args[2] = { pair, val_int((int64_t)i) };
            Value r = call_block(ev, env, *blk, block_args, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "each_key") == 0 || strcmp(name, "each_value") == 0) {
        if (!blk) {
            Value arr = val_array_new();
            for (size_t i = 0; i < h->len; i++)
                val_array_push(&arr, strcmp(name, "each_key") == 0 ? h->keys[i] : h->vals[i]);
            *out = wrap_result_as_enumerator(ev, env, arr, site); return 1;
        }
        for (size_t i = 0; i < h->len; i++) {
            Value arg = strcmp(name, "each_key") == 0 ? h->keys[i] : h->vals[i];
            Value r = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
        }
        *out = recv;
        return 1;
    }
    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, env, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                val_array_push(&result, r);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0 || strcmp(name, "reject") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        else {
            Value result = val_hash_new_with_defaults(ev->arena, h->default_value, h->default_proc);
            result.hash->compare_by_identity = h->compare_by_identity;
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, env, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
                if ((strcmp(name, "reject") == 0 && !val_truthy(r)) || (strcmp(name, "reject") != 0 && val_truthy(r)))
                    val_hash_set(result.hash, h->keys[i], h->vals[i]);
            }
            *out = result;
        }
        return 1;
    }
    /* Hash#select! / Hash#filter! / Hash#keep_if — mutate in place, keep matching */
    if (strcmp(name, "select!") == 0 || strcmp(name, "filter!") == 0 || strcmp(name, "keep_if") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        int changed = 0;
        for (size_t i = 0; i < h->len; ) {
            Value bargs[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, bargs, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (!val_truthy(r)) {
                val_hash_delete(h, h->keys[i]);
                changed = 1;
            } else {
                i++;
            }
        }
        *out = (strcmp(name, "keep_if") == 0 || changed) ? recv : val_nil();
        return 1;
    }
    /* Hash#reject! / Hash#delete_if — mutate in place, remove matching */
    if (strcmp(name, "reject!") == 0 || strcmp(name, "delete_if") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        int changed = 0;
        for (size_t i = 0; i < h->len; ) {
            Value bargs[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, bargs, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (val_truthy(r)) {
                val_hash_delete(h, h->keys[i]);
                changed = 1;
            } else {
                i++;
            }
        }
        *out = (strcmp(name, "delete_if") == 0 || changed) ? recv : val_nil();
        return 1;
    }
    if (strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) {
        if (!blk) {
            /* no-block: any?=non-empty, all?=always-true, none?=empty */
            if (strcmp(name, "none?") == 0) { *out = val_bool(h->len == 0); return 1; }
            if (strcmp(name, "any?") == 0)  { *out = val_bool(h->len > 0);  return 1; }
            *out = val_true(); return 1;
        }
        *out = (strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) ? val_true() : val_false();
        for (size_t i = 0; i < h->len; i++) {
            Value bargs[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, bargs, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (r.kind == VAL_EXCEPTION) { *out = r; return 1; }
            if (strcmp(name, "any?")  == 0 && val_truthy(r))  { *out = val_true();  return 1; }
            if (strcmp(name, "all?")  == 0 && !val_truthy(r)) { *out = val_false(); return 1; }
            if (strcmp(name, "none?") == 0 && val_truthy(r))  { *out = val_false(); return 1; }
        }
        return 1;
    }
    if (strcmp(name, "group_by") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        Value result = val_hash_new(ev->arena);
        for (size_t i = 0; i < h->len; i++) {
            Value pair[2] = { h->keys[i], h->vals[i] };
            Value key = call_block(ev, env, *blk, pair, 2, site);
            if (val_is_signal(key)) { *out = key; return 1; }
            Value group = val_nil();
            if (!val_hash_get(result.hash, key, &group)) {
                group = val_array_new();
            }
            Value kv_pair = val_array_new();
            val_array_push(&kv_pair, h->keys[i]);
            val_array_push(&kv_pair, h->vals[i]);
            val_array_push(&group, kv_pair);
            val_hash_set(result.hash, key, group);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "sort") == 0 || strcmp(name, "sort_by") == 0 ||
        strcmp(name, "min_by") == 0 || strcmp(name, "max_by") == 0) {
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
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        else {
            Value result = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value bargs[2] = { h->keys[i], h->vals[i] };
                Value r = call_block(ev, env, *blk, bargs, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (r.kind == VAL_EXCEPTION) { *out = r; return 1; }
                if (r.kind == VAL_ARRAY) for (size_t j = 0; j < r.array->len; j++) val_array_push(&result, r.array->elems[j]);
                else val_array_push(&result, r);
            }
            *out = result;
        }
        return 1;
    }
    if (strcmp(name, "each_with_object") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "each_with_object requires block and init"); return 1; }
        if (!blk) {
            Value arr = val_array_new();
            for (size_t i = 0; i < h->len; i++) {
                Value pair = val_array_new();
                Value entry = val_array_new();
                val_array_push(&entry, h->keys[i]);
                val_array_push(&entry, h->vals[i]);
                val_array_push(&pair, entry);
                val_array_push(&pair, args[0]);
                val_array_push(&arr, pair);
            }
            *out = wrap_result_as_enumerator(ev, env, arr, site);
            return 1;
        }
        Value acc = args[0];
        for (size_t i = 0; i < h->len; i++) {
            Value pair = val_array_new();
            val_array_push(&pair, h->keys[i]);
            val_array_push(&pair, h->vals[i]);
            Value block_args[2] = { pair, acc };
            Value r = call_block(ev, env, *blk, block_args, 2, site);
            if (val_is_signal(r)) { *out = r; return 1; }
        }
        *out = acc; return 1;
    }
    if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
        if (!blk) { *out = eval_raise_class(ev, site, "LocalJumpError", "Hash#reduce requires a block"); return 1; }
        if (h->len == 0) { *out = argc > 0 ? args[0] : val_nil(); return 1; }
        {
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
                Value r = call_block(ev, env, *blk, bargs, 2, site);
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
        else {
            if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
            val_hash_set(h, args[0], args[1]); *out = args[1];
        }
        return 1;
    }
    if (strcmp(name, "clear") == 0) {
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        h->len = 0; *out = recv; return 1;
    }
    if (strcmp(name, "dup") == 0) {
        Value result = val_hash_new_with_defaults(ev->arena, h->default_value, h->default_proc);
        result.hash->compare_by_identity = h->compare_by_identity;
        for (size_t i = 0; i < h->len; i++) val_hash_set(result.hash, h->keys[i], h->vals[i]);
        *out = result;
        return 1;
    }
    if (strcmp(name, "transform_values") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_keys_or_values_array(h, 0), site); return 1; }
        Value result = val_hash_new_with_defaults(ev->arena, h->default_value, h->default_proc);
        result.hash->compare_by_identity = h->compare_by_identity;
        for (size_t i = 0; i < h->len; i++) {
            Value r = call_block(ev, env, *blk, &h->vals[i], 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            val_hash_set(result.hash, h->keys[i], r);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "transform_values!") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_keys_or_values_array(h, 0), site); return 1; }
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        for (size_t i = 0; i < h->len; i++) {
            Value r = call_block(ev, env, *blk, &h->vals[i], 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            h->vals[i] = r;
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "transform_keys") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_keys_or_values_array(h, 1), site); return 1; }
        Value result = val_hash_new(ev->arena);
        result.hash->compare_by_identity = h->compare_by_identity;
        for (size_t i = 0; i < h->len; i++) {
            Value r = call_block(ev, env, *blk, &h->keys[i], 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            val_hash_set(result.hash, r, h->vals[i]);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "transform_keys!") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_keys_or_values_array(h, 1), site); return 1; }
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        /* collect new keys first to avoid aliasing the underlying array */
        Value new_keys[256];
        size_t n = h->len < 256 ? h->len : 256;
        for (size_t i = 0; i < n; i++) {
            Value r = call_block(ev, env, *blk, &h->keys[i], 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            new_keys[i] = r;
        }
        for (size_t i = 0; i < n; i++) h->keys[i] = new_keys[i];
        *out = recv; return 1;
    }
    if (strcmp(name, "filter_map") == 0) {
        if (!blk) { *out = wrap_result_as_enumerator(ev, env, hash_pairs_array(h), site); return 1; }
        Value result = val_array_new();
        for (size_t i = 0; i < h->len; i++) {
            Value pair[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, pair, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (val_truthy(r)) val_array_push(&result, r);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "count") == 0) {
        if (!blk) { *out = val_int((int64_t)h->len); return 1; }
        int64_t n = 0;
        for (size_t i = 0; i < h->len; i++) {
            Value pair[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, pair, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (val_truthy(r)) n++;
        }
        *out = val_int(n); return 1;
    }
    if (strcmp(name, "sum") == 0) {
        Value acc = (argc > 0) ? args[0] : val_int(0);
        for (size_t i = 0; i < h->len; i++) {
            Value pair[2] = { h->keys[i], h->vals[i] };
            Value r;
            if (blk) {
                r = call_block(ev, env, *blk, pair, 2, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r, out)) return 1;
            } else {
                r = val_array_new();
                val_array_push(&r, h->keys[i]);
                val_array_push(&r, h->vals[i]);
            }
            acc = dispatch_method(ev, env, acc, "+", &r, 1, NULL, site, 0, 1);
            if (ev->errored) { *out = val_nil(); return 1; }
        }
        *out = acc; return 1;
    }
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
    if (strcmp(name, "flatten") == 0) {
        /* h.flatten(n) = h.to_a.flatten(n); default depth = 1 */
        int depth = (argc > 0 && args[0].kind == VAL_INT) ? (int)args[0].ival : 1;
        Value pairs = val_array_new();
        for (size_t i = 0; i < h->len; i++) {
            Value pair = val_array_new();
            val_array_push(&pair, h->keys[i]);
            val_array_push(&pair, h->vals[i]);
            val_array_push(&pairs, pair);
        }
        Value result = val_array_new();
        array_flatten_into(ev, env, pairs, &result, depth);
        *out = result; return 1;
    }
    if (strcmp(name, "default") == 0) {
        if (argc > 0) {
            /* Hash#default(key) - return default for that key */
            if (h->default_proc.kind == VAL_BLOCK) {
                Value bargs[2] = { recv, args[0] };
                *out = call_block(ev, env, h->default_proc, bargs, 2, site);
            } else {
                *out = h->default_value;
            }
        } else {
            *out = h->default_proc.kind == VAL_BLOCK ? val_nil() : h->default_value;
        }
        return 1;
    }
    if (strcmp(name, "default=") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        h->default_value = args[0];
        h->default_proc = val_nil();
        *out = args[0]; return 1;
    }
    if (strcmp(name, "default_proc") == 0) {
        *out = h->default_proc.kind == VAL_BLOCK ? h->default_proc : val_nil(); return 1;
    }
    if (strcmp(name, "default_proc=") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "wrong number of arguments"); return 1; }
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        if (args[0].kind == VAL_BLOCK) { h->default_proc = args[0]; h->default_value = val_nil(); }
        else if (args[0].kind == VAL_NIL) { h->default_proc = val_nil(); }
        *out = args[0]; return 1;
    }
    if (strcmp(name, "rehash") == 0) {
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        *out = recv; return 1;
    }
    if (strcmp(name, "compare_by_identity") == 0) {
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        h->compare_by_identity = 1;
        *out = recv; return 1;
    }
    if (strcmp(name, "compare_by_identity?") == 0) {
        *out = val_bool(h->compare_by_identity);
        return 1;
    }
    if (strcmp(name, "fetch_values") == 0) {
        Value result = val_array_new();
        for (int i = 0; i < argc; i++) {
            Value v = val_nil();
            if (val_hash_get(h, args[i], &v)) {
                val_array_push(&result, v);
            } else if (blk) {
                Value r = call_block(ev, env, *blk, &args[i], 1, site);
                if (val_is_signal(r)) { *out = r; return 1; }
                val_array_push(&result, r);
            } else {
                *out = eval_raise_class(ev, site, "KeyError", "key not found: %s", val_inspect(ev->arena, args[i]));
                return 1;
            }
        }
        *out = result; return 1;
    }
    if (strcmp(name, "compact") == 0) {
        Value result = val_hash_new(ev->arena);
        result.hash->compare_by_identity = h->compare_by_identity;
        for (size_t i = 0; i < h->len; i++)
            if (h->vals[i].kind != VAL_NIL) val_hash_set(result.hash, h->keys[i], h->vals[i]);
        *out = result; return 1;
    }
    if (strcmp(name, "compact!") == 0) {
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        size_t w = 0;
        for (size_t i = 0; i < h->len; i++) {
            if (h->vals[i].kind != VAL_NIL) {
                h->keys[w] = h->keys[i];
                h->vals[w] = h->vals[i];
                w++;
            }
        }
        int changed = (w != h->len);
        h->len = w;
        *out = changed ? recv : val_nil(); return 1;
    }
    if (strcmp(name, "any?") == 0 && !blk) {
        *out = val_bool(h->len > 0); return 1;
    }
    if (strcmp(name, "none?") == 0 && !blk) {
        *out = val_bool(h->len == 0); return 1;
    }
    if (strcmp(name, "invert") == 0) {
        Value result = val_hash_new(ev->arena);
        for (size_t i = 0; i < h->len; i++) val_hash_set(result.hash, h->vals[i], h->keys[i]);
        *out = result; return 1;
    }
    if (strcmp(name, "slice") == 0) {
        Value result = val_hash_new(ev->arena);
        result.hash->compare_by_identity = h->compare_by_identity;
        for (int i = 0; i < argc; i++) {
            Value v;
            if (val_hash_get(h, args[i], &v)) val_hash_set(result.hash, args[i], v);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "except") == 0) {
        Value result = val_hash_new(ev->arena);
        result.hash->compare_by_identity = h->compare_by_identity;
        for (size_t i = 0; i < h->len; i++) {
            int excluded = 0;
            for (int j = 0; j < argc; j++) {
                if (hash_key_equal_for_mode(h, h->keys[i], args[j])) { excluded = 1; break; }
            }
            if (!excluded) val_hash_set(result.hash, h->keys[i], h->vals[i]);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "replace") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Hash#replace requires an argument"); return 1; }
        if (h->frozen) { *out = eval_raise_class(ev, site, "FrozenError", "can't modify frozen Hash"); return 1; }
        if (args[0].kind != VAL_HASH) { *out = eval_raise_class(ev, site, "TypeError", "no implicit conversion into Hash"); return 1; }
        RubyHash *other = args[0].hash;
        if (other == h) { *out = recv; return 1; }
        h->len = 0;
        h->default_value = other->default_value;
        h->default_proc = other->default_proc;
        h->compare_by_identity = other->compare_by_identity;
        for (size_t i = 0; i < other->len; i++) {
            val_hash_set(h, other->keys[i], other->vals[i]);
            sync_process_env_pair(ev, h, other->keys[i], other->vals[i]);
        }
        *out = recv; return 1;
    }
    if (strcmp(name, "key") == 0 || strcmp(name, "index") == 0) {
        if (argc < 1) { *out = eval_raise_class(ev, site, "ArgumentError", "Hash#key requires a value"); return 1; }
        for (size_t i = 0; i < h->len; i++) if (val_equal(h->vals[i], args[0])) { *out = h->keys[i]; return 1; }
        *out = val_nil(); return 1;
    }
    if (strcmp(name, "assoc") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        for (size_t i = 0; i < h->len; i++) if (val_equal(h->keys[i], args[0])) {
            Value pair = val_array_new();
            val_array_push(&pair, h->keys[i]); val_array_push(&pair, h->vals[i]);
            *out = pair; return 1;
        }
        *out = val_nil(); return 1;
    }
    if (strcmp(name, "rassoc") == 0) {
        if (argc < 1) { *out = val_nil(); return 1; }
        for (size_t i = 0; i < h->len; i++) if (val_equal(h->vals[i], args[0])) {
            Value pair = val_array_new();
            val_array_push(&pair, h->keys[i]); val_array_push(&pair, h->vals[i]);
            *out = pair; return 1;
        }
        *out = val_nil(); return 1;
    }
    if (strcmp(name, "values_at") == 0) {
        Value result = val_array_new();
        for (int i = 0; i < argc; i++) {
            Value v;
            val_array_push(&result, val_hash_get(h, args[i], &v) ? v : h->default_value);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "to_h") == 0) {
        if (!blk) { *out = recv; return 1; }
        Value result = val_hash_new(ev->arena);
        for (size_t i = 0; i < h->len; i++) {
            Value bargs[2] = { h->keys[i], h->vals[i] };
            Value r = call_block(ev, env, *blk, bargs, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(r, out)) return 1;
            if (r.kind == VAL_ARRAY && r.array->len >= 2)
                val_hash_set(result.hash, r.array->elems[0], r.array->elems[1]);
        }
        *out = result; return 1;
    }
    if (strcmp(name, "nil?") == 0) { *out = val_false(); return 1; }
    return 0;  /* fall through to Enumerable / user-defined method dispatch */
}

static int range_include_value(Eval *ev, Env *env, RubyRange *r, Value v, Node *site) {
    /* Fast path for integer ranges with defined endpoints */
    if (r->begin_val.kind == VAL_INT && r->end_val.kind == VAL_INT && v.kind == VAL_INT) {
        return v.ival >= r->begin_val.ival &&
               (r->exclusive ? v.ival < r->end_val.ival : v.ival <= r->end_val.ival);
    }
    /* Endless range: end is nil — any value >= begin is included */
    if (r->end_val.kind == VAL_NIL) {
        if (r->begin_val.kind == VAL_NIL) return 1; /* (nil..nil) = all */
        Value cmp_lo = dispatch_method(ev, env, v, "<=>", &r->begin_val, 1, NULL, site, 0, 1);
        return cmp_lo.kind == VAL_INT && cmp_lo.ival >= 0;
    }
    /* Beginless range: begin is nil — any value <= end is included */
    if (r->begin_val.kind == VAL_NIL) {
        Value cmp_hi = dispatch_method(ev, env, v, "<=>", &r->end_val, 1, NULL, site, 0, 1);
        if (cmp_hi.kind != VAL_INT) return 0;
        return r->exclusive ? cmp_hi.ival < 0 : cmp_hi.ival <= 0;
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
        /* String range: iterate through successive characters */
        if (r->begin_val.kind == VAL_STRING && (r->end_val.kind == VAL_STRING || r->end_val.kind == VAL_NIL)) {
            const char *cur = r->begin_val.sval ? r->begin_val.sval : "";
            const char *end_s = r->end_val.kind == VAL_STRING && r->end_val.sval ? r->end_val.sval : NULL;
            if (!blk) {
                Value arr;
                dispatch_range(ev, env, recv, "to_a", NULL, 0, NULL, site, &arr);
                if (val_is_signal(arr)) { *out = arr; return 1; }
                *out = wrap_result_as_enumerator(ev, env, arr, site);
                return 1;
            }
            /* Simple ASCII string succession for now */
            char next_buf[16]; strncpy(next_buf, cur, 15); next_buf[15] = '\0';
            for (int iters = 0; iters < 1000; iters++) {
                Value cur_val = val_string(ev->arena, next_buf);
                if (end_s) {
                    int cmp = strcmp(next_buf, end_s);
                    if (r->exclusive ? cmp >= 0 : cmp > 0) break;
                }
                Value r2 = call_block(ev, env, *blk, &cur_val, 1, site);
                if (ev->errored) { *out = val_nil(); return 1; }
                if (flow_signal_out(r2, out)) return 1;
                if (!end_s) break;
                /* Compute successor */
                Value succ_val;
                dispatch_string(ev, env, cur_val, "succ", NULL, 0, NULL, NULL, &succ_val);
                if (succ_val.kind != VAL_STRING) break;
                strncpy(next_buf, succ_val.sval, 15); next_buf[15] = '\0';
            }
            *out = recv; return 1;
        }
        if (r->begin_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#each requires Integer range"); return 1; }
        /* Allow Float::INFINITY as end for lazy/break-able iteration */
        int infinite = (r->end_val.kind == VAL_FLOAT && r->end_val.fval > 1e300) ||
                       r->end_val.kind == VAL_NIL;
        int64_t lo = r->begin_val.ival;
        int64_t hi = r->end_val.kind == VAL_INT ? r->end_val.ival : INT64_MAX;
        if (!blk) {
            if (infinite) { *out = eval_raise_class(ev, site, "TypeError", "Range#each: cannot build array from infinite range"); return 1; }
            /* Return Enumerator for finite integer range */
            Value enum_class;
            if (env_get(ev->top_env, "Enumerator", &enum_class) && enum_class.kind == VAL_CLASS) {
                Value arr = val_array_new();
                for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++)
                    val_array_push(&arr, val_int(i));
                Value eres = dispatch_method(ev, env, enum_class, "new", &arr, 1, NULL, site, 0, 1);
                if (!val_is_signal(eres)) { *out = eres; return 1; }
                ev->errored = 0; ev->exception_class = NULL; ev->exception_msg[0] = '\0';
            }
            Value arr = val_array_new();
            for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++)
                val_array_push(&arr, val_int(i));
            *out = arr; return 1;
        }
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
        }
        *out = recv; return 1;
    }

    if (strcmp(name, "each_with_index") == 0) {
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#each_with_index requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        if (!blk) {
            Value arr = val_array_new();
            int64_t idx = 0;
            for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++, idx++) {
                Value pair = val_array_new();
                val_array_push(&pair, val_int(i));
                val_array_push(&pair, val_int(idx));
                val_array_push(&arr, pair);
            }
            *out = wrap_result_as_enumerator(ev, env, arr, site); return 1;
        }
        int64_t idx = 0;
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++, idx++) {
            Value bargs[2] = { val_int(i), val_int(idx) };
            Value res = call_block(ev, env, *blk, bargs, 2, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
        }
        *out = recv; return 1;
    }

    if (strcmp(name, "to_a") == 0 || strcmp(name, "entries") == 0) {
        if (r->begin_val.kind == VAL_STRING && r->end_val.kind == VAL_STRING) {
            /* String range: "a".."e" etc. using String#succ */
            Value arr = val_array_new();
            const char *beg = r->begin_val.sval, *end_s = r->end_val.sval;
            if (strlen(beg) == 1 && strlen(end_s) == 1) {
                /* Single-char optimization */
                for (char c = beg[0]; r->exclusive ? c < end_s[0] : c <= end_s[0]; c++) {
                    char buf[2] = {c, '\0'};
                    val_array_push(&arr, val_string(ev->arena, buf));
                }
            } else {
                /* Multi-char: use succ-based iteration */
                Value cur = r->begin_val;
                int limit = 10000;
                while (limit-- > 0) {
                    int cmp_to_end = strcmp(cur.sval, end_s);
                    if (r->exclusive ? cmp_to_end >= 0 : cmp_to_end > 0) break;
                    val_array_push(&arr, cur);
                    /* cur = cur.succ */
                    Value succ = dispatch_method(ev, env, cur, "succ", NULL, 0, NULL, site, 0, 1);
                    if (val_is_signal(succ)) { *out = succ; return 1; }
                    cur = succ;
                    if (strcmp(cur.sval, end_s) == 0 && !r->exclusive) {
                        val_array_push(&arr, cur); break;
                    }
                }
            }
            *out = arr; return 1;
        }
        /* String range: build array via successive succ calls */
        if (r->begin_val.kind == VAL_STRING) {
            const char *cur = r->begin_val.sval ? r->begin_val.sval : "";
            const char *end_s = r->end_val.kind == VAL_STRING && r->end_val.sval ? r->end_val.sval : NULL;
            Value arr = val_array_new();
            char nb[64]; strncpy(nb, cur, 63); nb[63] = '\0';
            for (int iters = 0; iters < 10000; iters++) {
                Value cv = val_string(ev->arena, nb);
                if (end_s) {
                    int cmp = strcmp(nb, end_s);
                    if (r->exclusive ? cmp >= 0 : cmp > 0) break;
                }
                val_array_push(&arr, cv);
                if (!end_s) break;
                Value sv; dispatch_string(ev, env, cv, "succ", NULL, 0, NULL, NULL, &sv);
                if (sv.kind != VAL_STRING) break;
                strncpy(nb, sv.sval, 63); nb[63] = '\0';
            }
            *out = arr; return 1;
        }
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
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT) {
            /* String range: count by iterating */
            if (r->begin_val.kind == VAL_STRING && r->end_val.kind == VAL_STRING) {
                Value arr_out;
                dispatch_range(ev, env, recv, "to_a", NULL, 0, NULL, site, &arr_out);
                *out = val_int(arr_out.kind == VAL_ARRAY ? (int64_t)arr_out.array->len : 0);
                return 1;
            }
            /* Float range: size is Infinity (uncountably many) */
            if (r->begin_val.kind == VAL_FLOAT || r->end_val.kind == VAL_FLOAT) {
                *out = val_float(1.0 / 0.0); return 1;
            }
            *out = val_nil(); return 1;
        }
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

    if ((strcmp(name, "find") == 0 || strcmp(name, "detect") == 0) && blk) {
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = val_nil(); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value v = val_int(i);
            Value res = call_block(ev, env, *blk, &v, 1, site);
            if (val_is_signal(res)) { *out = res; return 1; }
            if (val_truthy(res)) { *out = v; return 1; }
        }
        *out = val_nil(); return 1;
    }
    if ((strcmp(name, "find_index") == 0 || strcmp(name, "index") == 0)) {
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = val_nil(); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival, idx = 0;
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++, idx++) {
            Value v = val_int(i);
            int match = 0;
            if (blk) {
                Value res = call_block(ev, env, *blk, &v, 1, site);
                if (val_is_signal(res)) { *out = res; return 1; }
                match = val_truthy(res);
            } else if (argc > 0) {
                match = val_equal(v, args[0]);
            }
            if (match) { *out = val_int(idx); return 1; }
        }
        *out = val_nil(); return 1;
    }
    if (strcmp(name, "sum") == 0) {
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#sum requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival;
        int64_t hi = r->exclusive ? r->end_val.ival - 1 : r->end_val.ival;
        int64_t init = argc > 0 && args[0].kind == VAL_INT ? args[0].ival : 0;
        if (hi < lo) { *out = val_int(init); return 1; }
        if (blk) {
            /* sum with block: iterate and accumulate */
            Value acc = val_int(init);
            for (int64_t i = lo; i <= hi; i++) {
                Value arg = val_int(i);
                Value r_val = call_block(ev, env, *blk, &arg, 1, site);
                if (val_is_signal(r_val)) { *out = r_val; return 1; }
                if (acc.kind == VAL_INT && r_val.kind == VAL_INT)
                    acc.ival += r_val.ival;
                else {
                    double a = acc.kind == VAL_FLOAT ? acc.fval : (double)acc.ival;
                    double c = r_val.kind == VAL_FLOAT ? r_val.fval : (double)r_val.ival;
                    acc = val_float(a + c);
                }
            }
            *out = acc; return 1;
        }
        *out = val_int(init + (hi - lo + 1) * (lo + hi) / 2); return 1;
    }

    if (strcmp(name, "step") == 0) {
        if (argc < 1)
            { *out = eval_raise_class(ev, site, "ArgumentError", "Range#step requires a step value"); return 1; }
        /* Integer or Float range */
        int use_float = (r->begin_val.kind == VAL_FLOAT || r->end_val.kind == VAL_FLOAT ||
                         args[0].kind == VAL_FLOAT);
        if (use_float) {
            double lo = r->begin_val.kind == VAL_INT ? (double)r->begin_val.ival : r->begin_val.fval;
            double hi = r->end_val.kind == VAL_INT ? (double)r->end_val.ival : r->end_val.fval;
            double step = args[0].kind == VAL_INT ? (double)args[0].ival : args[0].fval;
            if (step <= 0.0) { *out = eval_raise_class(ev, site, "ArgumentError", "step must be positive"); return 1; }
            Value arr = val_array_new();
            int iter = 0;
            for (double x = lo; r->exclusive ? x < hi : x <= hi + step * 1e-10; x = lo + (++iter) * step) {
                if (r->exclusive ? x >= hi : x > hi + step * 1e-10) break;
                Value arg = val_float(x);
                if (blk) {
                    Value res = call_block(ev, env, *blk, &arg, 1, site);
                    if (ev->errored) { *out = val_nil(); return 1; }
                    if (flow_signal_out(res, out)) return 1;
                } else {
                    val_array_push(&arr, arg);
                }
            }
            *out = blk ? recv : arr; return 1;
        }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT || args[0].kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#step requires numeric range and step"); return 1; }
        int64_t step = args[0].ival;
        if (step <= 0)
            { *out = eval_raise_class(ev, site, "ArgumentError", "step must be positive"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        if (!blk) {
            Value arr = val_array_new();
            for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i += step)
                val_array_push(&arr, val_int(i));
            *out = arr; return 1;
        }
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i += step) {
            Value arg = val_int(i);
            Value res = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
        }
        *out = recv; return 1;
    }

    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0 ||
        strcmp(name, "select") == 0 || strcmp(name, "filter") == 0 ||
        strcmp(name, "flat_map") == 0 || strcmp(name, "each_with_index") == 0 ||
        strcmp(name, "each_with_object") == 0 || strcmp(name, "each_slice") == 0 ||
        strcmp(name, "each_cons") == 0 || strcmp(name, "reduce") == 0 ||
        strcmp(name, "inject") == 0 || strcmp(name, "find") == 0 ||
        strcmp(name, "detect") == 0 || strcmp(name, "all?") == 0 ||
        strcmp(name, "any?") == 0 || strcmp(name, "none?") == 0) {
        /* For string ranges, convert to array first */
        if (r->begin_val.kind == VAL_STRING) {
            Value arr;
            dispatch_range(ev, env, recv, "to_a", NULL, 0, NULL, site, &arr);
            if (arr.kind == VAL_ARRAY)
                return dispatch_array(ev, env, arr, name, args, argc, blk, site, out);
        }
    }
    if (strcmp(name, "map") == 0 || strcmp(name, "collect") == 0) {
        if (!blk) {
            Value arr;
            dispatch_range(ev, env, recv, "to_a", NULL, 0, NULL, site, &arr);
            if (val_is_signal(arr)) { *out = arr; return 1; }
            *out = wrap_result_as_enumerator(ev, env, arr, site); return 1;
        }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#map requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        Value result = val_array_new();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
            val_array_push(&result, res);
        }
        *out = result; return 1;
    }

    if (strcmp(name, "select") == 0 || strcmp(name, "filter") == 0) {
        if (!blk) {
            Value arr;
            dispatch_range(ev, env, recv, "to_a", NULL, 0, NULL, site, &arr);
            if (val_is_signal(arr)) { *out = arr; return 1; }
            *out = wrap_result_as_enumerator(ev, env, arr, site); return 1;
        }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#select requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        Value result = val_array_new();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
            if (val_truthy(res)) val_array_push(&result, arg);
        }
        *out = result; return 1;
    }

    if (strcmp(name, "reject") == 0) {
        if (!blk) {
            Value arr;
            dispatch_range(ev, env, recv, "to_a", NULL, 0, NULL, site, &arr);
            if (val_is_signal(arr)) { *out = arr; return 1; }
            *out = wrap_result_as_enumerator(ev, env, arr, site); return 1;
        }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#reject requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        Value result = val_array_new();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, env, *blk, &arg, 1, site);
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
            if (!val_truthy(res)) val_array_push(&result, arg);
        }
        *out = result; return 1;
    }

    if (strcmp(name, "reduce") == 0 || strcmp(name, "inject") == 0) {
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range#reduce requires Integer range"); return 1; }
        const char *sym_op = NULL;
        if (!blk) {
            if (argc == 1 && args[0].kind == VAL_SYMBOL) sym_op = args[0].sval;
            else if (argc == 2 && args[1].kind == VAL_SYMBOL) sym_op = args[1].sval;
            else { *out = eval_raise_class(ev, site, "LocalJumpError", "Range#reduce requires a block or symbol"); return 1; }
        }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        int64_t start = lo;
        Value acc;
        if (sym_op && argc == 2) acc = args[0];
        else if (!sym_op && argc > 0) acc = args[0];
        else { if (r->exclusive ? lo >= hi : lo > hi) { *out = val_nil(); return 1; } acc = val_int(start++); }
        for (int64_t i = start; r->exclusive ? i < hi : i <= hi; i++) {
            Value elem = val_int(i);
            Value res;
            if (sym_op) res = dispatch_method(ev, env, acc, sym_op, &elem, 1, NULL, site, 0, 1);
            else { Value bargs[2] = { acc, elem }; res = call_block(ev, env, *blk, bargs, 2, site); }
            if (ev->errored) { *out = val_nil(); return 1; }
            if (flow_signal_out(res, out)) return 1;
            acc = res;
        }
        *out = acc; return 1;
    }

    if (strcmp(name, "any?") == 0 || strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) {
        if (!blk) {
            int empty = 0;
            if (r->begin_val.kind == VAL_INT && r->end_val.kind == VAL_INT) {
                int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
                empty = r->exclusive ? lo >= hi : lo > hi;
            }
            if (strcmp(name, "any?") == 0) { *out = val_bool(!empty); return 1; }
            if (strcmp(name, "none?") == 0) { *out = val_bool(empty); return 1; }
            *out = val_true(); return 1;
        }
        if (r->begin_val.kind != VAL_INT || r->end_val.kind != VAL_INT)
            { *out = eval_raise_class(ev, site, "TypeError", "Range iteration requires Integer range"); return 1; }
        int64_t lo = r->begin_val.ival, hi = r->end_val.ival;
        *out = (strcmp(name, "all?") == 0 || strcmp(name, "none?") == 0) ? val_true() : val_false();
        for (int64_t i = lo; r->exclusive ? i < hi : i <= hi; i++) {
            Value arg = val_int(i);
            Value res = call_block(ev, env, *blk, &arg, 1, site);
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
