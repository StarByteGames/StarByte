#include "ast.h"

void nodelist_push(NodeList *l, Node *n) {
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->items = (Node**)sb_xrealloc(l->items, l->cap * sizeof(Node*));
    }
    l->items[l->count++] = n;
}

void nodelist_free(NodeList *l) {
    if (!l) return;
    for (size_t i = 0; i < l->count; i++) node_free(l->items[i]);
    free(l->items);
    l->items = NULL; l->count = l->cap = 0;
}

Node *node_new(NodeKind k, int line) {
    Node *n = (Node*)sb_xcalloc(1, sizeof(Node));
    n->kind = k;
    n->line = line;
    return n;
}

static void typeref_free(TypeRef *t) {
    if (t && t->type_name) { free(t->type_name); t->type_name = NULL; }
}

void node_free(Node *n) {
    if (!n) return;
    switch (n->kind) {
        case EX_STRING: free(n->as.s); break;
        case EX_IDENT: free(n->as.ident.name); break;
        case EX_BINARY: node_free(n->as.binary.lhs); node_free(n->as.binary.rhs); break;
        case EX_LOGICAL: node_free(n->as.logical.lhs); node_free(n->as.logical.rhs); break;
        case EX_UNARY: node_free(n->as.unary.operand); break;
        case EX_POSTFIX: node_free(n->as.postfix.operand); break;
        case EX_ASSIGN: node_free(n->as.assign.target); node_free(n->as.assign.value); break;
        case EX_CALL:
            node_free(n->as.call.callee);
            nodelist_free(&n->as.call.args);
            break;
        case EX_MEMBER:
            node_free(n->as.member.object);
            free(n->as.member.name);
            break;
        case EX_INDEX:
            node_free(n->as.index_expr.object);
            node_free(n->as.index_expr.index);
            break;
        case ST_EXPR: node_free(n->as.expr_stmt.expr); break;
        case ST_VAR_DECL:
            typeref_free(&n->as.var_decl.type);
            free(n->as.var_decl.name);
            node_free(n->as.var_decl.init);
            break;
        case ST_BLOCK: nodelist_free(&n->as.block.stmts); break;
        case ST_IF:
            node_free(n->as.if_stmt.cond);
            node_free(n->as.if_stmt.then_branch);
            node_free(n->as.if_stmt.else_branch);
            break;
        case ST_WHILE:
            node_free(n->as.while_stmt.cond);
            node_free(n->as.while_stmt.body);
            break;
        case ST_FOR:
            node_free(n->as.for_stmt.init);
            node_free(n->as.for_stmt.cond);
            node_free(n->as.for_stmt.post);
            node_free(n->as.for_stmt.body);
            break;
        case ST_RETURN: node_free(n->as.ret.value); break;
        case ST_FUNC_DECL:
            typeref_free(&n->as.func.ret_type);
            free(n->as.func.name);
            for (size_t i = 0; i < n->as.func.param_count; i++) {
                typeref_free(&n->as.func.params[i].type);
                free(n->as.func.params[i].name);
            }
            free(n->as.func.params);
            node_free(n->as.func.body);
            break;
        case ST_MODULE: free(n->as.module_stmt.name); break;
        case EX_STRUCT_LIT: nodelist_free(&n->as.struct_lit.values); break;
        case ST_STRUCT_DECL:
            free(n->as.struct_decl.name);
            for (size_t i = 0; i < n->as.struct_decl.field_count; i++) {
                typeref_free(&n->as.struct_decl.fields[i].type);
                free(n->as.struct_decl.fields[i].name);
            }
            free(n->as.struct_decl.fields);
            break;
        case ST_ENUM_DECL:
            free(n->as.enum_decl.name);
            for (size_t i = 0; i < n->as.enum_decl.count; i++) {
                free(n->as.enum_decl.members[i].name);
            }
            free(n->as.enum_decl.members);
            break;
        case ST_CLASS_DECL:
            free(n->as.class_decl.name);
            free(n->as.class_decl.base_name);
            for (size_t i = 0; i < n->as.class_decl.interface_count; i++)
                free(n->as.class_decl.interface_names[i]);
            free(n->as.class_decl.interface_names);
            for (size_t i = 0; i < n->as.class_decl.field_count; i++) {
                typeref_free(&n->as.class_decl.fields[i].type);
                free(n->as.class_decl.fields[i].name);
                node_free(n->as.class_decl.fields[i].init);
            }
            free(n->as.class_decl.fields);
            for (size_t i = 0; i < n->as.class_decl.method_count; i++)
                node_free(n->as.class_decl.methods[i]);
            free(n->as.class_decl.methods);
            break;
        case ST_INTERFACE_DECL:
            free(n->as.iface_decl.name);
            for (size_t i = 0; i < n->as.iface_decl.base_count; i++)
                free(n->as.iface_decl.base_names[i]);
            free(n->as.iface_decl.base_names);
            for (size_t i = 0; i < n->as.iface_decl.method_count; i++) {
                typeref_free(&n->as.iface_decl.methods[i].ret_type);
                free(n->as.iface_decl.methods[i].name);
                for (size_t j = 0; j < n->as.iface_decl.methods[i].param_count; j++) {
                    typeref_free(&n->as.iface_decl.methods[i].params[j].type);
                    free(n->as.iface_decl.methods[i].params[j].name);
                }
                free(n->as.iface_decl.methods[i].params);
            }
            free(n->as.iface_decl.methods);
            break;
        case ST_TRY:
            node_free(n->as.try_stmt.body);
            free(n->as.try_stmt.catch_name);
            node_free(n->as.try_stmt.catch_body);
            node_free(n->as.try_stmt.finally_body);
            break;
        case ST_THROW:
            node_free(n->as.throw_stmt.value);
            break;
        default: break;
    }
    free(n);
}
