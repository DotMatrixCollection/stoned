#include "ast.h"
#include <string.h>

Node *node_new(Arena *a, NodeKind kind, Span span) {
    Node *n = arena_alloc(a, sizeof(Node));
    memset(n, 0, sizeof(Node));
    n->kind = kind;
    n->span = span;
    return n;
}

NodeList *nodelist_append(Arena *a, NodeList *list, Node *node) {
    NodeList *entry = arena_alloc(a, sizeof(NodeList));
    entry->node = node;
    entry->next = NULL;

    if (!list) return entry;

    NodeList *tail = list;
    while (tail->next) tail = tail->next;
    tail->next = entry;
    return list;
}

const char *node_kind_name(NodeKind k) {
    switch (k) {
        case NODE_INT:       return "INT";
        case NODE_FLOAT:     return "FLOAT";
        case NODE_STRING:    return "STRING";
        case NODE_SYMBOL:    return "SYMBOL";
        case NODE_NIL:       return "NIL";
        case NODE_TRUE:      return "TRUE";
        case NODE_FALSE:     return "FALSE";
        case NODE_SELF:      return "SELF";
        case NODE_ARRAY:     return "ARRAY";
        case NODE_HASH:      return "HASH";
        case NODE_PAIR:      return "PAIR";
        case NODE_LVAR:      return "LVAR";
        case NODE_IVAR:      return "IVAR";
        case NODE_CVAR:      return "CVAR";
        case NODE_GVAR:      return "GVAR";
        case NODE_CONST:     return "CONST";
        case NODE_ASSIGN:    return "ASSIGN";
        case NODE_OP_ASSIGN: return "OP_ASSIGN";
        case NODE_BINOP:     return "BINOP";
        case NODE_UNOP:      return "UNOP";
        case NODE_CALL:      return "CALL";
        case NODE_BLOCK:     return "BLOCK";
        case NODE_IF:        return "IF";
        case NODE_UNLESS:    return "UNLESS";
        case NODE_WHILE:     return "WHILE";
        case NODE_UNTIL:     return "UNTIL";
        case NODE_RETURN:    return "RETURN";
        case NODE_BREAK:     return "BREAK";
        case NODE_NEXT:      return "NEXT";
        case NODE_DEF:       return "DEF";
        case NODE_PARAM:     return "PARAM";
        case NODE_SUPER:     return "SUPER";
        case NODE_ROPE:      return "ROPE";
        case NODE_BODY:      return "BODY";
        case NODE_PROGRAM:   return "PROGRAM";
        default:             return "UNKNOWN";
    }
}
