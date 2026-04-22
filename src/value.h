#ifndef STONED_VALUE_H
#define STONED_VALUE_H

#include "arena.h"
#include <stdint.h>
#include <stddef.h>

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
    VAL_CLASS,      /* user-defined class */
    VAL_OBJECT,     /* instance of a class */
    VAL_METHOD,     /* user-defined: Node* + closure Env* */
    VAL_BLOCK,      /* block/proc: Node* + closure Env* */
    /* Control flow signals — not real values, used to unwind */
    VAL_RETURN,
    VAL_BREAK,
    VAL_NEXT,
} ValueKind;

/* Forward typedefs for class and object structures */
typedef struct RubyClass RubyClass;
typedef struct RubyObject RubyObject;

typedef struct Value {
    ValueKind kind;
    union {
        int         bval;    /* VAL_BOOL */
        int64_t     ival;    /* VAL_INT */
        double      fval;    /* VAL_FLOAT */
        const char *sval;    /* VAL_STRING, VAL_SYMBOL */

        struct {             /* VAL_ARRAY */
            struct Value  *elems;   /* flat array of Value, grows with realloc */
            size_t         len;
            size_t         cap;
        } array;

        RubyClass  *klass;     /* VAL_CLASS */
        RubyObject *obj;       /* VAL_OBJECT */

        struct {             /* VAL_METHOD */
            struct Node *def_node;
            struct Env  *closure;
        } method;

        struct {             /* VAL_BLOCK */
            struct Node *block_node;
            struct Env  *closure;
        } block;

        struct Value *wrapped; /* VAL_RETURN, VAL_BREAK, VAL_NEXT: carried value */
    };
} Value;

/* Class and Object runtime structures (defined after Value) */
typedef struct IVarEntry {
    const char *name;
    Value      val;
    struct IVarEntry *next;
} IVarEntry;

typedef struct RubyClass {
    const char *name;
    Value       superclass;  /* VAL_CLASS or VAL_NIL */
    struct Env *class_env;   /* Methods defined in this class */
} RubyClass;

typedef struct RubyObject {
    Value       klass;       /* VAL_CLASS: the object's class */
    IVarEntry  *ivars;       /* Instance variables */
} RubyObject;

/* ------------------------------------------------------------------ */
/* Constructors                                                         */
/* ------------------------------------------------------------------ */
static inline Value val_nil(void)           { Value v; v.kind = VAL_NIL;  return v; }
static inline Value val_true(void)          { Value v; v.kind = VAL_BOOL; v.bval = 1; return v; }
static inline Value val_false(void)         { Value v; v.kind = VAL_BOOL; v.bval = 0; return v; }
static inline Value val_bool(int b)         { return b ? val_true() : val_false(); }
static inline Value val_int(int64_t i)      { Value v; v.kind = VAL_INT;  v.ival = i; return v; }
static inline Value val_float(double f)     { Value v; v.kind = VAL_FLOAT; v.fval = f; return v; }
static inline Value val_symbol(const char *s) { Value v; v.kind = VAL_SYMBOL; v.sval = s; return v; }

Value val_string(Arena *a, const char *s);
Value val_string_n(Arena *a, const char *s, size_t len);
Value val_array_new(void);
void  val_array_push(Value *arr, Value elem);

Value val_method(struct Node *def, struct Env *closure);
Value val_block(struct Node *blk, struct Env *closure);

Value val_return(Arena *a, Value inner);
Value val_break(Arena *a, Value inner);
Value val_next(Arena *a, Value inner);

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
    return v.kind == VAL_RETURN || v.kind == VAL_BREAK || v.kind == VAL_NEXT;
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
        case VAL_STRING:
        case VAL_SYMBOL: {
            if (!a.sval || !b.sval) return a.sval == b.sval;
            int i = 0;
            while (a.sval[i] && b.sval[i] && a.sval[i] == b.sval[i]) i++;
            return a.sval[i] == b.sval[i];
        }
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
