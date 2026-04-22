#include "sema.h"
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Error reporting                                                      */
/* ------------------------------------------------------------------ */
static void sema_error(Sema *s, const char *msg, uint32_t line, uint32_t col) {
    if (s->error_count < MAX_ERRORS) {
        s->errors[s->error_count].message = msg;
        s->errors[s->error_count].line    = line;
        s->errors[s->error_count].col     = col;
        s->error_count++;
    }
}

/* ------------------------------------------------------------------ */
/* Scope management                                                     */
/* ------------------------------------------------------------------ */
static void scope_push(Sema *s, int is_def) {
    Scope *sc    = arena_alloc(s->arena, sizeof(Scope));
    sc->parent   = s->scope;
    sc->is_def   = is_def;
    sc->locals   = NULL;
    s->scope     = sc;
}

static void scope_pop(Sema *s) {
    if (s->scope) s->scope = s->scope->parent;
}

static void scope_add(Sema *s, const char *name) {
    /* deduplicate */
    for (LocalEntry *e = s->scope->locals; e; e = e->next)
        if (strcmp(e->name, name) == 0) return;
    LocalEntry *e = arena_alloc(s->arena, sizeof(LocalEntry));
    e->name       = name;
    e->next       = s->scope->locals;
    s->scope->locals = e;
}

/* Look up name in scope chain. Stops at def boundaries. */
static int scope_has(Sema *s, const char *name) {
    for (Scope *sc = s->scope; sc; sc = sc->parent) {
        for (LocalEntry *e = sc->locals; e; e = e->next)
            if (strcmp(e->name, name) == 0) return 1;
        if (sc->is_def) break;  /* hard boundary */
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Phase 1: collect local assignments within the current scope.        */
/*                                                                      */
/* We scan the body shallowly — we don't recurse into def, class, or   */
/* module, but we DO recurse into if/while/block bodies since those    */
/* share the scope.                                                     */
/*                                                                      */
/* excl: param list whose names must not be added to the outer scope.  */
/* Pass NULL at the top level; pass the block's params when recurring  */
/* into a block body.                                                   */
/* ------------------------------------------------------------------ */
static int param_list_has(NodeList *params, const char *name) {
    for (NodeList *l = params; l; l = l->next) {
        Node *p = l->node;
        if (p && p->kind == NODE_PARAM && p->param.name &&
            strcmp(p->param.name, name) == 0)
            return 1;
    }
    return 0;
}

static void collect(Sema *s, Node *node, NodeList *excl) {
    if (!node) return;

    switch (node->kind) {
        case NODE_ASSIGN:
            if (node->assign.target && node->assign.target->kind == NODE_LVAR) {
                const char *name = node->assign.target->sval;
                if (!param_list_has(excl, name))
                    scope_add(s, name);
            }
            collect(s, node->assign.value, excl);
            break;

        case NODE_OP_ASSIGN:
            collect(s, node->binop.right, excl);
            break;

        case NODE_IF:
        case NODE_UNLESS:
            collect(s, node->cond.cond,      excl);
            collect(s, node->cond.then_body, excl);
            collect(s, node->cond.else_body, excl);
            break;

        case NODE_WHILE:
        case NODE_UNTIL:
            collect(s, node->loop.cond, excl);
            collect(s, node->loop.body, excl);
            break;

        case NODE_BODY:
        case NODE_PROGRAM:
            for (NodeList *l = node->body.stmts; l; l = l->next)
                collect(s, l->node, excl);
            break;

        case NODE_CALL:
            collect(s, node->call.recv, excl);
            for (NodeList *l = node->call.args; l; l = l->next)
                collect(s, l->node, excl);
            /* Recurse into block body — assignments there ARE visible in the
               outer scope unless they're the block's own params. */
            if (node->call.block && node->call.block->kind == NODE_BLOCK) {
                Node *blk = node->call.block;
                collect(s, blk->block.body, blk->block.params);
            }
            break;

        case NODE_RETURN:
        case NODE_BREAK:
        case NODE_NEXT:
            collect(s, node->jump.value, excl);
            break;

        case NODE_BINOP:
            collect(s, node->binop.left,  excl);
            collect(s, node->binop.right, excl);
            break;

        case NODE_UNOP:
            collect(s, node->unop.operand, excl);
            break;

        case NODE_DEF:
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Phase 2: resolve — walk tree, fix NODE_LVAR → NODE_CALL where       */
/* needed, check break/next/return context.                            */
/* ------------------------------------------------------------------ */
static void resolve(Sema *s, Node *node);

static void resolve_list(Sema *s, NodeList *list) {
    for (NodeList *l = list; l; l = l->next)
        resolve(s, l->node);
}

static void resolve(Sema *s, Node *node) {
    if (!node) return;

    switch (node->kind) {

        /* ---- Leaves that need no resolution ---- */
        case NODE_INT: case NODE_FLOAT: case NODE_STRING:
        case NODE_SYMBOL: case NODE_NIL: case NODE_TRUE:
        case NODE_FALSE: case NODE_SELF:
        case NODE_IVAR: case NODE_CVAR: case NODE_GVAR: case NODE_CONST:
            break;

        /* ---- The key resolution: bare name ---- */
        case NODE_LVAR: {
            const char *name = node->sval;
            if (!scope_has(s, name)) {
                /* Not a local — rewrite in-place to a zero-arg method call */
                node->kind        = NODE_CALL;
                node->call.recv   = NULL;
                node->call.method = name;
                node->call.args   = NULL;
                node->call.block  = NULL;
            }
            break;
        }

        case NODE_ASSIGN:
            /* resolve the value; target is always a variable node, no recursion needed */
            resolve(s, node->assign.value);
            /* for LVAR target: it's already known local from collect phase */
            if (node->assign.target)
                resolve(s, node->assign.target);
            break;

        case NODE_OP_ASSIGN:
            resolve(s, node->binop.left);
            resolve(s, node->binop.right);
            break;

        case NODE_BINOP:
            resolve(s, node->binop.left);
            resolve(s, node->binop.right);
            break;

        case NODE_UNOP:
            resolve(s, node->unop.operand);
            break;

        case NODE_CALL:
            resolve(s, node->call.recv);
            resolve_list(s, node->call.args);
            if (node->call.block) resolve(s, node->call.block);
            break;

        case NODE_BLOCK: {
            /* Blocks are soft scopes */
            scope_push(s, 0);
            /* Block params are locals in the block's scope */
            for (NodeList *l = node->block.params; l; l = l->next) {
                Node *param = l->node;
                if (param->kind == NODE_PARAM && param->param.name)
                    scope_add(s, param->param.name);
            }
            /* Collect any assignments inside the block body */
            collect(s, node->block.body, node->block.params);
            s->block_depth++;
            resolve(s, node->block.body);
            s->block_depth--;
            scope_pop(s);
            break;
        }

        case NODE_IF:
        case NODE_UNLESS:
            resolve(s, node->cond.cond);
            resolve(s, node->cond.then_body);
            resolve(s, node->cond.else_body);
            break;

        case NODE_WHILE:
        case NODE_UNTIL:
            resolve(s, node->loop.cond);
            s->loop_depth++;
            resolve(s, node->loop.body);
            s->loop_depth--;
            break;

        case NODE_DEF: {
            /* Hard scope boundary */
            scope_push(s, 1);
            scope_add(s, "self");  /* self is always in scope inside methods */
            /* Parameters are locals in the def's scope */
            for (NodeList *l = node->def.params; l; l = l->next) {
                Node *param = l->node;
                if (param->kind == NODE_PARAM && param->param.name)
                    scope_add(s, param->param.name);
                if (param->param.default_val)
                    resolve(s, param->param.default_val);
            }
            /* Collect locals in the def body */
            collect(s, node->def.body, NULL);
            s->def_depth++;
            resolve(s, node->def.body);
            s->def_depth--;
            scope_pop(s);
            break;
        }

        case NODE_RETURN:
            if (s->def_depth == 0)
                sema_error(s, "'return' outside of method", node->span.line, node->span.col);
            resolve(s, node->jump.value);
            break;

        case NODE_BREAK:
            if (s->loop_depth == 0 && s->block_depth == 0)
                sema_error(s, "'break' outside of loop or block", node->span.line, node->span.col);
            resolve(s, node->jump.value);
            break;

        case NODE_NEXT:
            if (s->loop_depth == 0 && s->block_depth == 0)
                sema_error(s, "'next' outside of loop or block", node->span.line, node->span.col);
            resolve(s, node->jump.value);
            break;

        case NODE_SUPER:
            resolve_list(s, node->super_call.args);
            break;

        case NODE_CLASS: {
            /* Class body is a hard scope; resolve it so method bodies get analyzed */
            scope_push(s, 1);
            if (node->klass.superclass) resolve(s, node->klass.superclass);
            if (node->klass.body) {
                collect(s, node->klass.body, NULL);
                resolve(s, node->klass.body);
            }
            scope_pop(s);
            break;
        }

        case NODE_ARRAY:
            resolve_list(s, node->array.elements);
            break;

        case NODE_HASH:
            for (NodeList *l = node->hash.pairs; l; l = l->next) {
                Node *pair = l->node;
                if (pair && pair->kind == NODE_PAIR) {
                    resolve(s, pair->pair.key);
                    resolve(s, pair->pair.value);
                }
            }
            break;

        case NODE_ROPE: {
            /* Resolve expressions inside rope segments */
            RopeNode *r = node->interp.rope;
            /* Iterative traversal to avoid deep stack on long strings */
            RopeNode *stack[256];
            int top = 0;
            if (r) stack[top++] = r;
            while (top > 0) {
                RopeNode *rn = stack[--top];
                switch (rn->kind) {
                    case ROPE_LIT: break;
                    case ROPE_EXPR: resolve(s, rn->expr.node); break;
                    case ROPE_CAT:
                        if (rn->cat.right) stack[top++] = rn->cat.right;
                        if (rn->cat.left)  stack[top++] = rn->cat.left;
                        break;
                }
            }
            break;
        }

        case NODE_BODY:
        case NODE_PROGRAM:
            for (NodeList *l = node->body.stmts; l; l = l->next)
                resolve(s, l->node);
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */
void sema_init(Sema *s, Arena *arena) {
    memset(s, 0, sizeof(*s));
    s->arena = arena;
}

void sema_run(Sema *s, Node *program) {
    scope_push(s, 1);          /* top-level is a hard scope */
    collect(s, program, NULL); /* collect top-level locals */
    resolve(s, program);
    scope_pop(s);
}
