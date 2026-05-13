#include "print_ast.h"
#include "rope.h"

static void indent_print(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) fprintf(out, "  ");
}

void print_ast(FILE *out, Node *node, int indent) {
    if (!node) { indent_print(out, indent); fprintf(out, "(null)\n"); return; }

    indent_print(out, indent);

    switch (node->kind) {
        case NODE_INT:    fprintf(out, "INT(%lld)\n", (long long)node->ival); break;
        case NODE_FLOAT:  fprintf(out, "FLOAT(%g)\n", node->fval); break;
        case NODE_STRING: fprintf(out, "STRING(%s)\n", node->sval); break;
        case NODE_SYMBOL: fprintf(out, "SYMBOL(%s)\n", node->sval); break;
        case NODE_NIL:    fprintf(out, "NIL\n"); break;
        case NODE_TRUE:   fprintf(out, "TRUE\n"); break;
        case NODE_FALSE:  fprintf(out, "FALSE\n"); break;
        case NODE_SELF:   fprintf(out, "SELF\n"); break;

        case NODE_LVAR:   fprintf(out, "LVAR(%s)\n", node->sval); break;
        case NODE_IVAR:   fprintf(out, "IVAR(@%s)\n", node->sval); break;
        case NODE_CVAR:   fprintf(out, "CVAR(@@%s)\n", node->sval); break;
        case NODE_GVAR:   fprintf(out, "GVAR($%s)\n", node->sval); break;
        case NODE_CONST:  fprintf(out, "CONST(%s)\n", node->sval); break;

        case NODE_BINOP:
            fprintf(out, "BINOP(%s)\n", node->binop.op);
            print_ast(out, node->binop.left,  indent+1);
            print_ast(out, node->binop.right, indent+1);
            break;

        case NODE_UNOP:
            fprintf(out, "UNOP(%s)\n", node->unop.op);
            print_ast(out, node->unop.operand, indent+1);
            break;

        case NODE_ASSIGN:
            fprintf(out, "ASSIGN\n");
            print_ast(out, node->assign.target, indent+1);
            print_ast(out, node->assign.value,  indent+1);
            break;

        case NODE_OP_ASSIGN:
            fprintf(out, "OP_ASSIGN(%s)\n", node->binop.op);
            print_ast(out, node->binop.left,  indent+1);
            print_ast(out, node->binop.right, indent+1);
            break;

        case NODE_CALL:
            fprintf(out, "CALL(%s)\n", node->call.method);
            if (node->call.recv) {
                indent_print(out, indent+1);
                fprintf(out, "recv:\n");
                print_ast(out, node->call.recv, indent+2);
            }
            if (node->call.args) {
                indent_print(out, indent+1);
                fprintf(out, "args:\n");
                for (NodeList *l = node->call.args; l; l = l->next)
                    print_ast(out, l->node, indent+2);
            }
            if (node->call.block) {
                indent_print(out, indent+1);
                fprintf(out, "block:\n");
                print_ast(out, node->call.block, indent+2);
            }
            break;

        case NODE_BLOCK:
            fprintf(out, "BLOCK\n");
            if (node->block.params) {
                indent_print(out, indent+1); fprintf(out, "params:\n");
                for (NodeList *l = node->block.params; l; l = l->next)
                    print_ast(out, l->node, indent+2);
            }
            print_ast(out, node->block.body, indent+1);
            break;

        case NODE_IF:
        case NODE_UNLESS:
            fprintf(out, "%s\n", node->kind == NODE_IF ? "IF" : "UNLESS");
            indent_print(out, indent+1); fprintf(out, "cond:\n");
            print_ast(out, node->cond.cond, indent+2);
            indent_print(out, indent+1); fprintf(out, "then:\n");
            print_ast(out, node->cond.then_body, indent+2);
            if (node->cond.else_body) {
                indent_print(out, indent+1); fprintf(out, "else:\n");
                print_ast(out, node->cond.else_body, indent+2);
            }
            break;

        case NODE_WHILE:
        case NODE_UNTIL:
            fprintf(out, "%s\n", node->kind == NODE_WHILE ? "WHILE" : "UNTIL");
            indent_print(out, indent+1); fprintf(out, "cond:\n");
            print_ast(out, node->loop.cond, indent+2);
            indent_print(out, indent+1); fprintf(out, "body:\n");
            print_ast(out, node->loop.body, indent+2);
            break;

        case NODE_BEGIN:
            fprintf(out, "BEGIN\n");
            indent_print(out, indent+1); fprintf(out, "body:\n");
            print_ast(out, node->begin_stmt.body, indent+2);
            if (node->begin_stmt.rescues) {
                indent_print(out, indent+1); fprintf(out, "rescues:\n");
                for (NodeList *l = node->begin_stmt.rescues; l; l = l->next)
                    print_ast(out, l->node, indent+2);
            }
            if (node->begin_stmt.else_body) {
                indent_print(out, indent+1); fprintf(out, "else:\n");
                print_ast(out, node->begin_stmt.else_body, indent+2);
            }
            if (node->begin_stmt.ensure_body) {
                indent_print(out, indent+1); fprintf(out, "ensure:\n");
                print_ast(out, node->begin_stmt.ensure_body, indent+2);
            }
            break;

        case NODE_RESCUE:
            fprintf(out, "RESCUE\n");
            if (node->rescue_clause.exception_classes) {
                indent_print(out, indent+1); fprintf(out, "classes:\n");
                for (NodeList *l = node->rescue_clause.exception_classes; l; l = l->next)
                    print_ast(out, l->node, indent+2);
            }
            if (node->rescue_clause.exception_var) {
                indent_print(out, indent+1); fprintf(out, "var: %s\n", node->rescue_clause.exception_var);
            }
            indent_print(out, indent+1); fprintf(out, "body:\n");
            print_ast(out, node->rescue_clause.body, indent+2);
            break;

        case NODE_DEF:
            fprintf(out, "DEF(%s)\n", node->def.name);
            if (node->def.params) {
                indent_print(out, indent+1); fprintf(out, "params:\n");
                for (NodeList *l = node->def.params; l; l = l->next)
                    print_ast(out, l->node, indent+2);
            }
            print_ast(out, node->def.body, indent+1);
            break;

        case NODE_SCLASS:
            fprintf(out, "SCLASS\n");
            indent_print(out, indent+1); fprintf(out, "recv:\n");
            print_ast(out, node->sclass.recv, indent+2);
            indent_print(out, indent+1); fprintf(out, "body:\n");
            print_ast(out, node->sclass.body, indent+2);
            break;

        case NODE_MODULE:
            fprintf(out, "MODULE(%s)\n", node->klass.name);
            print_ast(out, node->klass.body, indent+1);
            break;

        case NODE_PARAM:
            fprintf(out, "PARAM(%s%s%s)\n",
                node->param.splat       ? "*" : "",
                node->param.block_param ? "&" : "",
                node->param.name ? node->param.name : "");
            if (node->param.default_val)
                print_ast(out, node->param.default_val, indent+1);
            break;

        case NODE_RETURN:
            fprintf(out, "RETURN\n");
            if (node->jump.value) print_ast(out, node->jump.value, indent+1);
            break;
        case NODE_BREAK:
            fprintf(out, "BREAK\n");
            if (node->jump.value) print_ast(out, node->jump.value, indent+1);
            break;
        case NODE_NEXT:
            fprintf(out, "NEXT\n");
            if (node->jump.value) print_ast(out, node->jump.value, indent+1);
            break;
        case NODE_FOR:
            fprintf(out, "FOR\n");
            indent_print(out, indent+1); fprintf(out, "target:\n");
            print_ast(out, node->for_loop.target, indent+2);
            indent_print(out, indent+1); fprintf(out, "iterable:\n");
            print_ast(out, node->for_loop.iterable, indent+2);
            indent_print(out, indent+1); fprintf(out, "body:\n");
            print_ast(out, node->for_loop.body, indent+2);
            break;
        case NODE_RETRY:
            fprintf(out, "RETRY\n");
            break;

        case NODE_ARRAY:
            fprintf(out, "ARRAY\n");
            for (NodeList *l = node->array.elements; l; l = l->next)
                print_ast(out, l->node, indent+1);
            break;

        case NODE_HASH:
            fprintf(out, "HASH\n");
            for (NodeList *l = node->hash.pairs; l; l = l->next)
                print_ast(out, l->node, indent+1);
            break;

        case NODE_PAIR:
            fprintf(out, "PAIR\n");
            print_ast(out, node->pair.key,   indent+1);
            print_ast(out, node->pair.value, indent+1);
            break;

        case NODE_ROPE: {
            fprintf(out, "ROPE\n");
            /* Walk the rope tree and print segments */
            /* Use a simple recursive helper via a local lambda workaround — just
               inline a small stack-based traversal */
            typedef struct { RopeNode *r; int depth; } Frame;
            Frame stack[256]; int top = 0;
            stack[top++] = (Frame){ node->interp.rope, indent+1 };
            while (top > 0) {
                Frame f = stack[--top];
                if (!f.r) continue;
                indent_print(out, f.depth);
                switch (f.r->kind) {
                    case ROPE_LIT:
                        fprintf(out, "LIT(%s)\n", f.r->lit.bytes ? f.r->lit.bytes : "");
                        break;
                    case ROPE_EXPR:
                        fprintf(out, "EXPR\n");
                        print_ast(out, f.r->expr.node, f.depth+1);
                        break;
                    case ROPE_CAT:
                        fprintf(out, "CAT\n");
                        /* push right first so left prints first */
                        stack[top++] = (Frame){ f.r->cat.right, f.depth+1 };
                        stack[top++] = (Frame){ f.r->cat.left,  f.depth+1 };
                        break;
                }
            }
            break;
        }

        case NODE_BODY:
        case NODE_PROGRAM:
            fprintf(out, "%s\n", node->kind == NODE_PROGRAM ? "PROGRAM" : "BODY");
            for (NodeList *l = node->body.stmts; l; l = l->next)
                print_ast(out, l->node, indent+1);
            break;

        default:
            fprintf(out, "%s\n", node_kind_name(node->kind));
            break;
    }
}
