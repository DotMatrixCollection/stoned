#include "value.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Value val_class(Arena *a, const char *name, Value superclass) {
    struct RubyClass *klass = arena_alloc(a, sizeof(struct RubyClass));
    klass->name = name;
    klass->superclass = superclass;
    klass->class_env = NULL;  /* Will be set by eval when methods are defined */
    Value v; v.kind = VAL_CLASS; v.klass = klass;
    return v;
}

Value val_object(Arena *a, Value klass) {
    struct RubyObject *obj = arena_alloc(a, sizeof(struct RubyObject));
    obj->klass = klass;
    obj->ivars = NULL;
    Value v; v.kind = VAL_OBJECT; v.obj = obj;
    return v;
}

void val_object_set_ivar(Arena *a, Value obj, const char *name, Value val) {
    if (obj.kind != VAL_OBJECT) return;
    struct RubyObject *o = obj.obj;
    /* Update if exists */
    for (IVarEntry *e = o->ivars; e; e = e->next) {
        if (strcmp(e->name, name) == 0) { e->val = val; return; }
    }
    /* Create new */
    IVarEntry *e = arena_alloc(a, sizeof(IVarEntry));
    e->name = name;
    e->val = val;
    e->next = o->ivars;
    o->ivars = e;
}

int val_object_get_ivar(Value obj, const char *name, Value *out) {
    if (obj.kind != VAL_OBJECT) return 0;
    struct RubyObject *o = obj.obj;
    for (IVarEntry *e = o->ivars; e; e = e->next) {
        if (strcmp(e->name, name) == 0) { *out = e->val; return 1; }
    }
    return 0;
}

Value val_string(Arena *a, const char *s) {
    Value v; v.kind = VAL_STRING;
    if (!s) { v.sval = ""; return v; }
    size_t len = strlen(s);
    char *buf = arena_alloc(a, len + 1);
    memcpy(buf, s, len + 1);
    v.sval = buf;
    return v;
}

Value val_string_n(Arena *a, const char *s, size_t len) {
    Value v; v.kind = VAL_STRING;
    char *buf = arena_alloc(a, len + 1);
    if (s) memcpy(buf, s, len);
    buf[len] = '\0';
    v.sval = buf;
    return v;
}

Value val_array_new(void) {
    Value v; v.kind = VAL_ARRAY;
    v.array.elems = NULL;
    v.array.len   = 0;
    v.array.cap   = 0;
    return v;
}

void val_array_push(Value *arr, Value elem) {
    if (arr->array.len >= arr->array.cap) {
        size_t new_cap = arr->array.cap == 0 ? 8 : arr->array.cap * 2;
        arr->array.elems = realloc(arr->array.elems,
                                   new_cap * sizeof(arr->array.elems[0]));
        arr->array.cap = new_cap;
    }
    arr->array.elems[arr->array.len++] = elem;
}

Value val_method(struct Node *def, struct Env *closure) {
    Value v; v.kind = VAL_METHOD;
    v.method.def_node = def;
    v.method.closure  = closure;
    return v;
}

Value val_block(struct Node *blk, struct Env *closure) {
    Value v; v.kind = VAL_BLOCK;
    v.block.block_node = blk;
    v.block.closure    = closure;
    return v;
}

static Value *alloc_val(Arena *a, Value inner) {
    Value *p = arena_alloc(a, sizeof(Value));
    *p = inner;
    return p;
}

Value val_return(Arena *a, Value inner) {
    Value v; v.kind = VAL_RETURN; v.wrapped = alloc_val(a, inner); return v;
}
Value val_break(Arena *a, Value inner) {
    Value v; v.kind = VAL_BREAK; v.wrapped = alloc_val(a, inner); return v;
}
Value val_next(Arena *a, Value inner) {
    Value v; v.kind = VAL_NEXT; v.wrapped = alloc_val(a, inner); return v;
}

/* ------------------------------------------------------------------ */
/* to_s / inspect                                                       */
/* ------------------------------------------------------------------ */
const char *val_to_s(Arena *a, Value v) {
    char *buf;
    switch (v.kind) {
        case VAL_NIL:    return "";
        case VAL_BOOL:   return v.bval ? "true" : "false";
        case VAL_INT:
            buf = arena_alloc(a, 32);
            snprintf(buf, 32, "%lld", (long long)v.ival);
            return buf;
        case VAL_FLOAT:
            buf = arena_alloc(a, 64);
            snprintf(buf, 64, "%g", v.fval);
            return buf;
        case VAL_STRING: return v.sval ? v.sval : "";
        case VAL_SYMBOL:
            buf = arena_alloc(a, strlen(v.sval) + 2);
            buf[0] = ':';
            strcpy(buf + 1, v.sval);
            return buf;
        case VAL_ARRAY: {
            /* "[elem, elem, ...]" */
            size_t total = 3;
            for (size_t i = 0; i < v.array.len; i++) {
                const char *s = val_inspect(a, v.array.elems[i]);
                total += strlen(s) + 2;
            }
            buf = arena_alloc(a, total);
            buf[0] = '['; buf[1] = '\0';
            for (size_t i = 0; i < v.array.len; i++) {
                if (i) strcat(buf, ", ");
                strcat(buf, val_inspect(a, v.array.elems[i]));
            }
            strcat(buf, "]");
            return buf;
        }
        case VAL_METHOD: return "#<Method>";
        case VAL_BLOCK:  return "#<Proc>";
        case VAL_CLASS: {
            char *buf = arena_alloc(a, strlen(v.klass->name) + 20);
            snprintf(buf, strlen(v.klass->name) + 20, "#<Class:%s>", v.klass->name);
            return buf;
        }
        case VAL_OBJECT: {
            const char *class_name = v.obj->klass.kind == VAL_CLASS ? v.obj->klass.klass->name : "?";
            char *buf = arena_alloc(a, strlen(class_name) + 30);
            snprintf(buf, strlen(class_name) + 30, "#<%s:0x%lx>", class_name, (unsigned long)v.obj);
            return buf;
        }
        default:         return "#<?>";
    }
}

const char *val_inspect(Arena *a, Value v) {
    char *buf;
    switch (v.kind) {
        case VAL_NIL:    return "nil";
        case VAL_BOOL:   return v.bval ? "true" : "false";
        case VAL_STRING: {
            size_t len = v.sval ? strlen(v.sval) : 0;
            buf = arena_alloc(a, len + 3);
            buf[0] = '"';
            if (v.sval) memcpy(buf + 1, v.sval, len);
            buf[len + 1] = '"';
            buf[len + 2] = '\0';
            return buf;
        }
        default: return val_to_s(a, v);
    }
}

const char *val_kind_name(ValueKind k) {
    switch (k) {
        case VAL_NIL:    return "NilClass";
        case VAL_BOOL:   return "TrueClass/FalseClass";
        case VAL_INT:    return "Integer";
        case VAL_FLOAT:  return "Float";
        case VAL_STRING: return "String";
        case VAL_SYMBOL: return "Symbol";
        case VAL_ARRAY:  return "Array";
        case VAL_CLASS: return "Class";
        case VAL_OBJECT: return "Object";
        case VAL_METHOD: return "Method";
        case VAL_BLOCK:  return "Proc";
        default:         return "?";
    }
}
