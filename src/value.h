#ifndef STONED_VALUE_H
#define STONED_VALUE_H

#include "arena.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Forward declarations */
struct Node;
struct Env;

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_SYMBOL,
    VAL_ARRAY,
    VAL_HASH,       /* key-value map, insertion-order preserving */
    VAL_RANGE,      /* begin..end or begin...end */
    VAL_CLASS,      /* user-defined class */
    VAL_OBJECT,     /* instance of a class */
    VAL_METHOD,       /* user-defined: Node* + closure Env* */
    VAL_UNDEF_METHOD, /* sentinel: undef_method blocks lookup from falling through */
    VAL_BLOCK,        /* block/proc: Node* + closure Env* */
    /* Control flow signals — not real values, used to unwind */
    VAL_EXCEPTION,
    VAL_RETURN,
    VAL_BREAK,
    VAL_NEXT,
    VAL_RETRY,
    VAL_THROW,   /* catch/throw: sval=tag, jump.wrapped=value */
} ValueKind;

typedef enum {
    METHOD_PUBLIC,
    METHOD_PRIVATE,
    METHOD_PROTECTED,
} MethodVisibility;

typedef enum {
    STRING_ENCODING_UTF8,
    STRING_ENCODING_ASCII_8BIT,
} StringEncodingTag;

/* Forward typedefs for class, object, and hash structures */
typedef struct RubyArray  RubyArray;
typedef struct RubyClass  RubyClass;
typedef struct RubyObject RubyObject;
typedef struct RubyHash   RubyHash;
typedef struct RubyRange  RubyRange;
typedef struct RubyModuleInclusion RubyModuleInclusion;

typedef struct Value {
    ValueKind kind;
    int       frozen; /* used for VAL_STRING/VAL_SYMBOL; arrays/hashes/objects have their own flag */
    StringEncodingTag string_encoding; /* used for VAL_STRING */
    size_t    byte_len; /* byte count of sval content for VAL_STRING (excl. NUL terminator) */
    union {
        int         bval;    /* VAL_BOOL */
        int64_t     ival;    /* VAL_INT */
        double      fval;    /* VAL_FLOAT */
        const char *sval;    /* VAL_STRING, VAL_SYMBOL */

        RubyArray  *array;     /* VAL_ARRAY */

        RubyHash   *hash;      /* VAL_HASH */
        RubyRange  *range;     /* VAL_RANGE */
        RubyClass  *klass;     /* VAL_CLASS */
        RubyObject *obj;       /* VAL_OBJECT */

        struct {             /* VAL_METHOD */
            struct Node *def_node;
            struct Env  *closure;
            MethodVisibility visibility;
            const char  *def_file;   /* ev->current_file at method definition time */
        } method;

        struct {             /* VAL_BLOCK */
            struct Node *block_node;
            struct Env  *closure;
            int          is_lambda;
            int          is_proc_object;
            const char  *def_file;   /* ev->current_file at block definition time */
        } block;

        struct {
            struct Value *wrapped;
            struct Env   *target_env;
        } jump; /* VAL_RETURN target env; VAL_BREAK/VAL_NEXT ignore target_env */

        struct {
            const char   *tag;       /* catch/throw symbol/string tag */
            struct Value *value;     /* thrown value */
        } throw_sig; /* VAL_THROW */
    };
} Value;

/* Class and Object runtime structures (defined after Value) */
struct RubyArray {
    Value  *elems;
    size_t  len;
    size_t  cap;
    struct Env *singleton_env;
    int     frozen;
};

typedef struct IVarEntry {
    const char *name;
    Value      val;
    struct IVarEntry *next;
} IVarEntry;

typedef struct RubyClass {
    const char *name;
    Value       superclass;  /* VAL_CLASS or VAL_NIL */
    struct Env *class_env;   /* Methods defined in this class */
    RubyModuleInclusion *prepended_modules;
    RubyModuleInclusion *included_modules;
    RubyModuleInclusion *extended_modules; /* Modules mixed into the singleton class via extend */
    int         is_module;
} RubyClass;

struct RubyModuleInclusion {
    RubyClass *mod;
    RubyModuleInclusion *next;
};

typedef struct RubyObject {
    Value       klass;       /* VAL_CLASS: the object's class */
    IVarEntry  *ivars;       /* Instance variables */
    struct Env *singleton_env; /* Methods added via extend */
    void       *native;      /* optional native payload */
    int         frozen;      /* 1 after freeze */
} RubyObject;

/* Insertion-order-preserving hash map (keys/vals malloc'd, struct arena'd) */
struct RubyHash {
    Value  *keys;
    Value  *vals;
    size_t  len;
    size_t  cap;
    struct Env *singleton_env;
    Value   default_value;
    Value   default_proc;
    int     compare_by_identity;
    int     frozen;
};

struct RubyRange {
    Value begin_val;
    Value end_val;
    int   exclusive; /* 1 for ..., 0 for .. */
};

/* ------------------------------------------------------------------ */
/* Constructors                                                         */
/* ------------------------------------------------------------------ */
static inline Value val_nil(void)           { Value v; v.kind = VAL_NIL;    v.frozen = 0; v.string_encoding = STRING_ENCODING_UTF8; v.byte_len = 0; v.ival = 0; return v; }
static inline Value val_true(void)          { Value v; v.kind = VAL_BOOL;   v.frozen = 0; v.string_encoding = STRING_ENCODING_UTF8; v.byte_len = 0; v.bval = 1; return v; }
static inline Value val_false(void)         { Value v; v.kind = VAL_BOOL;   v.frozen = 0; v.string_encoding = STRING_ENCODING_UTF8; v.byte_len = 0; v.bval = 0; return v; }
static inline Value val_bool(int b)         { return b ? val_true() : val_false(); }
static inline Value val_int(int64_t i)      { Value v; v.kind = VAL_INT;    v.frozen = 0; v.string_encoding = STRING_ENCODING_UTF8; v.byte_len = 0; v.ival = i; return v; }
static inline Value val_float(double f)     { Value v; v.kind = VAL_FLOAT;  v.frozen = 0; v.string_encoding = STRING_ENCODING_UTF8; v.byte_len = 0; v.fval = f; return v; }
static inline Value val_symbol(const char *s) { Value v; v.kind = VAL_SYMBOL; v.frozen = 0; v.string_encoding = STRING_ENCODING_UTF8; v.byte_len = 0; v.sval = s; return v; }

Value val_string(Arena *a, const char *s);
Value val_string_n(Arena *a, const char *s, size_t len);
Value val_array_new(void);
void  val_array_push(Value *arr, Value elem);

Value val_method(struct Node *def, struct Env *closure, MethodVisibility visibility, const char *def_file);
Value val_block(struct Node *blk, struct Env *closure);
Value val_proc(struct Node *blk, struct Env *closure);
Value val_lambda(struct Node *blk, struct Env *closure);

Value val_hash_new(Arena *a);
Value val_hash_new_with_defaults(Arena *a, Value default_value, Value default_proc);
void  val_hash_set(RubyHash *h, Value key, Value val);
int   val_hash_get(RubyHash *h, Value key, Value *out);
int   val_hash_delete(RubyHash *h, Value key);

Value val_range(Arena *a, Value begin_val, Value end_val, int exclusive);

Value val_return(Arena *a, Value inner, struct Env *target_env);
Value val_break(Arena *a, Value inner);
Value val_next(Arena *a, Value inner);
Value val_retry(void);
Value val_exception(void);

/* Class and object creation */
Value val_class(Arena *a, const char *name, Value superclass);
Value val_object(Arena *a, Value klass);
void  val_object_set_ivar(Arena *a, Value obj, const char *name, Value val);
int   val_object_get_ivar(Value obj, const char *name, Value *out);

/* ------------------------------------------------------------------ */
/* Predicates                                                           */
/* ------------------------------------------------------------------ */
static inline int val_truthy(Value v) {
    if (v.kind == VAL_NIL)  return 0;
    if (v.kind == VAL_BOOL) return v.bval;
    return 1;
}
static inline int val_is_signal(Value v) {
    return v.kind == VAL_EXCEPTION || v.kind == VAL_RETURN ||
           v.kind == VAL_BREAK || v.kind == VAL_NEXT || v.kind == VAL_RETRY ||
           v.kind == VAL_THROW;
}
static inline Value val_hash_val(RubyHash *h) {
    Value v; v.kind = VAL_HASH; v.frozen = 0; v.string_encoding = STRING_ENCODING_UTF8; v.byte_len = 0; v.hash = h; return v;
}
static inline int val_equal(Value a, Value b) {
    if (a.kind != b.kind) {
        /* int/float coercion */
        if (a.kind == VAL_INT   && b.kind == VAL_FLOAT) return (double)a.ival == b.fval;
        if (a.kind == VAL_FLOAT && b.kind == VAL_INT)   return a.fval == (double)b.ival;
        return 0;
    }
    switch (a.kind) {
        case VAL_NIL:    return 1;
        case VAL_BOOL:   return a.bval == b.bval;
        case VAL_INT:    return a.ival == b.ival;
        case VAL_FLOAT:  return a.fval == b.fval;
        case VAL_STRING: {
            if (!a.sval || !b.sval) return a.sval == b.sval;
            if (a.byte_len != b.byte_len) return 0;
            return memcmp(a.sval, b.sval, a.byte_len) == 0;
        }
        case VAL_SYMBOL: {
            if (!a.sval || !b.sval) return a.sval == b.sval;
            return strcmp(a.sval, b.sval) == 0;
        }
        case VAL_CLASS:  return a.klass == b.klass;
        case VAL_OBJECT: return a.obj   == b.obj;
        case VAL_RANGE:
            return a.range->exclusive == b.range->exclusive &&
                   val_equal(a.range->begin_val, b.range->begin_val) &&
                   val_equal(a.range->end_val, b.range->end_val);
        case VAL_ARRAY:
            if (a.array == b.array) return 1;
            if (!a.array || !b.array || a.array->len != b.array->len) return 0;
            for (size_t i = 0; i < a.array->len; i++)
                if (!val_equal(a.array->elems[i], b.array->elems[i])) return 0;
            return 1;
        case VAL_HASH:
            if (a.hash == b.hash) return 1;
            if (!a.hash || !b.hash || a.hash->len != b.hash->len) return 0;
            for (size_t i = 0; i < a.hash->len; i++) {
                Value v;
                if (!val_hash_get(b.hash, a.hash->keys[i], &v)) return 0;
                if (!val_equal(a.hash->vals[i], v)) return 0;
            }
            return 1;
        default: return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Stringification                                                      */
/* ------------------------------------------------------------------ */
const char *val_to_s(Arena *a, Value v);
const char *val_inspect(Arena *a, Value v);
const char *val_kind_name(ValueKind k);

#endif
