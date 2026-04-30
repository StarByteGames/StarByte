#include "parser.h"

static void p_error(Parser *p, const char *msg) {
    fprintf(stderr, "%s:%d: parse error: %s\n",
            p->filename ? p->filename : "<input>", p->cur.line, msg);
    exit(1);
}

static void advance(Parser *p) {
    token_free(&p->cur);
    if (p->has_peek) { p->cur = p->peek; p->has_peek = false; memset(&p->peek, 0, sizeof(Token)); }
    else { p->cur = lexer_next(&p->lx); }
}

static Token *peek_tok(Parser *p) {
    if (!p->has_peek) { p->peek = lexer_next(&p->lx); p->has_peek = true; }
    return &p->peek;
}

static bool check(Parser *p, TokenType t) { return p->cur.type == t; }
static bool match(Parser *p, TokenType t) {
    if (p->cur.type == t) { advance(p); return true; }
    return false;
}
static void expect(Parser *p, TokenType t, const char *what) {
    if (p->cur.type != t) {
        char buf[128];
        snprintf(buf, sizeof buf, "expected %s", what);
        p_error(p, buf);
    }
    advance(p);
}

void parser_init(Parser *p, const char *source, const char *filename) {
    memset(p, 0, sizeof(*p));
    lexer_init(&p->lx, source);
    p->filename = filename;
    advance(p);
}

void parser_dispose(Parser *p) {
    token_free(&p->cur);
    if (p->has_peek) token_free(&p->peek);
}

/* ---------- helpers ---------- */

static bool is_type_keyword(TokenType t) {
    return t == TK_KW_INT || t == TK_KW_FLOAT || t == TK_KW_CHAR
        || t == TK_KW_BOOL || t == TK_KW_STRING || t == TK_KW_VOID;
}

static const char *type_kw_name(TokenType t) {
    switch (t) {
        case TK_KW_INT: return "int";
        case TK_KW_FLOAT: return "float";
        case TK_KW_CHAR: return "char";
        case TK_KW_BOOL: return "bool";
        case TK_KW_STRING: return "string";
        case TK_KW_VOID: return "void";
        default: return "?";
    }
}

static TypeRef parse_type(Parser *p) {
    TypeRef t = {0};
    t.is_const = false;
    if (match(p, TK_KW_CONST)) t.is_const = true;
    if (!is_type_keyword(p->cur.type) && p->cur.type != TK_IDENT) {
        p_error(p, "expected type name");
    }
    if (is_type_keyword(p->cur.type)) {
        t.type_name = sb_strdup(type_kw_name(p->cur.type));
        advance(p);
    } else {
        t.type_name = sb_strdup(p->cur.svalue);
        advance(p);
    }
    return t;
}

/* ---------- expressions (Pratt-ish recursive descent) ---------- */

static Node *parse_expr(Parser *p);
static Node *parse_assign(Parser *p);
static Node *parse_logic_or(Parser *p);
static Node *parse_logic_and(Parser *p);
static Node *parse_equality(Parser *p);
static Node *parse_compare(Parser *p);
static Node *parse_addsub(Parser *p);
static Node *parse_muldiv(Parser *p);
static Node *parse_unary(Parser *p);
static Node *parse_postfix(Parser *p);
static Node *parse_primary(Parser *p);

static Node *parse_primary(Parser *p) {
    int line = p->cur.line;
    switch (p->cur.type) {
        case TK_INT: {
            Node *n = node_new(EX_INT, line);
            n->as.i = p->cur.ivalue;
            advance(p);
            return n;
        }
        case TK_FLOAT: {
            Node *n = node_new(EX_FLOAT, line);
            n->as.f = p->cur.fvalue;
            advance(p);
            return n;
        }
        case TK_STRING: {
            Node *n = node_new(EX_STRING, line);
            n->as.s = p->cur.svalue;       /* take ownership */
            p->cur.svalue = NULL;
            advance(p);
            return n;
        }
        case TK_CHAR: {
            Node *n = node_new(EX_CHAR, line);
            n->as.i = p->cur.ivalue;
            advance(p);
            return n;
        }
        case TK_KW_TRUE:  { advance(p); Node *n = node_new(EX_BOOL, line); n->as.b = true;  return n; }
        case TK_KW_FALSE: { advance(p); Node *n = node_new(EX_BOOL, line); n->as.b = false; return n; }
        case TK_KW_NULL:  { advance(p); return node_new(EX_NULL, line); }
        case TK_KW_NEW: {
            /* `new ClassName(args)` — desugar to a regular call where callee
               is the class name identifier. The runtime treats V_CLASS as
               callable (constructs an instance). */
            advance(p);
            return parse_postfix(p);
        }
        case TK_LPAREN: {
            advance(p);
            Node *e = parse_expr(p);
            expect(p, TK_RPAREN, "')'");
            return e;
        }
        case TK_IDENT: {
            Node *n = node_new(EX_IDENT, line);
            n->as.ident.name = p->cur.svalue;
            p->cur.svalue = NULL;
            advance(p);
            return n;
        }
        default:
            p_error(p, "expected expression");
            return NULL;
    }
}

static Node *parse_postfix(Parser *p) {
    Node *e = parse_primary(p);
    for (;;) {
        int line = p->cur.line;
        if (match(p, TK_DOT)) {
            if (p->cur.type != TK_IDENT) p_error(p, "expected member name after '.'");
            Node *m = node_new(EX_MEMBER, line);
            m->as.member.object = e;
            m->as.member.name = p->cur.svalue;
            p->cur.svalue = NULL;
            advance(p);
            e = m;
        } else if (match(p, TK_LPAREN)) {
            Node *call = node_new(EX_CALL, line);
            call->as.call.callee = e;
            if (!check(p, TK_RPAREN)) {
                for (;;) {
                    Node *arg = parse_expr(p);
                    nodelist_push(&call->as.call.args, arg);
                    if (!match(p, TK_COMMA)) break;
                }
            }
            expect(p, TK_RPAREN, "')'");
            e = call;
        } else if (check(p, TK_PLUSPLUS) || check(p, TK_MINUSMINUS)) {
            OpKind op = (p->cur.type == TK_PLUSPLUS) ? OP_INC : OP_DEC;
            advance(p);
            Node *pf = node_new(EX_POSTFIX, line);
            pf->as.postfix.op = op;
            pf->as.postfix.operand = e;
            e = pf;
        } else {
            break;
        }
    }
    return e;
}

static Node *parse_unary(Parser *p) {
    int line = p->cur.line;
    if (check(p, TK_MINUS) || check(p, TK_PLUS) || check(p, TK_NOT)) {
        OpKind op = OP_NEG;
        if (p->cur.type == TK_PLUS) op = OP_POS;
        else if (p->cur.type == TK_NOT) op = OP_NOT;
        advance(p);
        Node *u = node_new(EX_UNARY, line);
        u->as.unary.op = op;
        u->as.unary.operand = parse_unary(p);
        return u;
    }
    return parse_postfix(p);
}

static Node *bin(OpKind op, Node *lhs, Node *rhs, int line) {
    Node *n = node_new(EX_BINARY, line);
    n->as.binary.op = op;
    n->as.binary.lhs = lhs;
    n->as.binary.rhs = rhs;
    return n;
}

static Node *parse_muldiv(Parser *p) {
    Node *lhs = parse_unary(p);
    for (;;) {
        int line = p->cur.line;
        OpKind op;
        if (check(p, TK_STAR)) op = OP_MUL;
        else if (check(p, TK_SLASH)) op = OP_DIV;
        else if (check(p, TK_PERCENT)) op = OP_MOD;
        else break;
        advance(p);
        Node *rhs = parse_unary(p);
        lhs = bin(op, lhs, rhs, line);
    }
    return lhs;
}

static Node *parse_addsub(Parser *p) {
    Node *lhs = parse_muldiv(p);
    for (;;) {
        int line = p->cur.line;
        OpKind op;
        if (check(p, TK_PLUS)) op = OP_ADD;
        else if (check(p, TK_MINUS)) op = OP_SUB;
        else break;
        advance(p);
        Node *rhs = parse_muldiv(p);
        lhs = bin(op, lhs, rhs, line);
    }
    return lhs;
}

static Node *parse_compare(Parser *p) {
    Node *lhs = parse_addsub(p);
    for (;;) {
        int line = p->cur.line;
        OpKind op;
        if (check(p, TK_LT)) op = OP_LT;
        else if (check(p, TK_GT)) op = OP_GT;
        else if (check(p, TK_LE)) op = OP_LE;
        else if (check(p, TK_GE)) op = OP_GE;
        else break;
        advance(p);
        Node *rhs = parse_addsub(p);
        lhs = bin(op, lhs, rhs, line);
    }
    return lhs;
}

static Node *parse_equality(Parser *p) {
    Node *lhs = parse_compare(p);
    for (;;) {
        int line = p->cur.line;
        OpKind op;
        if (check(p, TK_EQ)) op = OP_EQ;
        else if (check(p, TK_NEQ)) op = OP_NEQ;
        else break;
        advance(p);
        Node *rhs = parse_compare(p);
        lhs = bin(op, lhs, rhs, line);
    }
    return lhs;
}

static Node *parse_logic_and(Parser *p) {
    Node *lhs = parse_equality(p);
    while (check(p, TK_AND)) {
        int line = p->cur.line;
        advance(p);
        Node *rhs = parse_equality(p);
        Node *n = node_new(EX_LOGICAL, line);
        n->as.logical.op = OP_AND; n->as.logical.lhs = lhs; n->as.logical.rhs = rhs;
        lhs = n;
    }
    return lhs;
}

static Node *parse_logic_or(Parser *p) {
    Node *lhs = parse_logic_and(p);
    while (check(p, TK_OR)) {
        int line = p->cur.line;
        advance(p);
        Node *rhs = parse_logic_and(p);
        Node *n = node_new(EX_LOGICAL, line);
        n->as.logical.op = OP_OR; n->as.logical.lhs = lhs; n->as.logical.rhs = rhs;
        lhs = n;
    }
    return lhs;
}

static Node *parse_assign(Parser *p) {
    Node *lhs = parse_logic_or(p);
    int line = p->cur.line;
    if (check(p, TK_ASSIGN) || check(p, TK_PLUSEQ) || check(p, TK_MINUSEQ)
        || check(p, TK_STAREQ) || check(p, TK_SLASHEQ) || check(p, TK_PERCENTEQ)) {
        TokenType tt = p->cur.type;
        advance(p);
        Node *rhs = parse_assign(p);
        Node *a = node_new(EX_ASSIGN, line);
        a->as.assign.target = lhs;
        a->as.assign.value = rhs;
        a->as.assign.is_compound = (tt != TK_ASSIGN);
        switch (tt) {
            case TK_PLUSEQ: a->as.assign.compound = OP_ADD; break;
            case TK_MINUSEQ: a->as.assign.compound = OP_SUB; break;
            case TK_STAREQ: a->as.assign.compound = OP_MUL; break;
            case TK_SLASHEQ: a->as.assign.compound = OP_DIV; break;
            case TK_PERCENTEQ: a->as.assign.compound = OP_MOD; break;
            default: break;
        }
        return a;
    }
    return lhs;
}

static Node *parse_expr(Parser *p) { return parse_assign(p); }

/* ---------- statements ---------- */

static Node *parse_stmt(Parser *p);
static Node *parse_block(Parser *p);

static Node *parse_var_decl(Parser *p, TypeRef ty) {
    int line = p->cur.line;
    if (p->cur.type != TK_IDENT) p_error(p, "expected variable name");
    char *name = p->cur.svalue; p->cur.svalue = NULL;
    advance(p);
    Node *init = NULL;
    if (match(p, TK_ASSIGN)) {
        if (check(p, TK_LBRACE)) {
            int blit = p->cur.line;
            advance(p); /* { */
            Node *lit = node_new(EX_STRUCT_LIT, blit);
            if (!check(p, TK_RBRACE)) {
                for (;;) {
                    Node *e = parse_expr(p);
                    nodelist_push(&lit->as.struct_lit.values, e);
                    if (!match(p, TK_COMMA)) break;
                }
            }
            expect(p, TK_RBRACE, "'}'");
            init = lit;
        } else {
            init = parse_expr(p);
        }
    }
    expect(p, TK_SEMI, "';'");
    Node *n = node_new(ST_VAR_DECL, line);
    n->as.var_decl.type = ty;
    n->as.var_decl.name = name;
    n->as.var_decl.init = init;
    return n;
}

static Node *parse_func_decl(Parser *p, TypeRef ret) {
    int line = p->cur.line;
    if (p->cur.type != TK_IDENT) p_error(p, "expected function name");
    char *name = p->cur.svalue; p->cur.svalue = NULL;
    advance(p);
    expect(p, TK_LPAREN, "'('");
    Param *params = NULL;
    size_t pc = 0, pcap = 0;
    if (!check(p, TK_RPAREN)) {
        for (;;) {
            TypeRef pt = parse_type(p);
            if (p->cur.type != TK_IDENT) p_error(p, "expected parameter name");
            char *pn = p->cur.svalue; p->cur.svalue = NULL;
            advance(p);
            if (pc == pcap) { pcap = pcap ? pcap*2 : 4; params = (Param*)sb_xrealloc(params, pcap*sizeof(Param)); }
            params[pc].type = pt; params[pc].name = pn; pc++;
            if (!match(p, TK_COMMA)) break;
        }
    }
    expect(p, TK_RPAREN, "')'");
    Node *body = parse_block(p);
    Node *n = node_new(ST_FUNC_DECL, line);
    n->as.func.ret_type = ret;
    n->as.func.name = name;
    n->as.func.params = params;
    n->as.func.param_count = pc;
    n->as.func.body = body;
    return n;
}

static Node *parse_block(Parser *p) {
    int line = p->cur.line;
    expect(p, TK_LBRACE, "'{'");
    Node *blk = node_new(ST_BLOCK, line);
    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        nodelist_push(&blk->as.block.stmts, parse_stmt(p));
    }
    expect(p, TK_RBRACE, "'}'");
    return blk;
}

static Node *parse_if(Parser *p) {
    int line = p->cur.line;
    advance(p); /* if */
    expect(p, TK_LPAREN, "'('");
    Node *cond = parse_expr(p);
    expect(p, TK_RPAREN, "')'");
    Node *then_b = parse_stmt(p);
    Node *else_b = NULL;
    if (match(p, TK_KW_ELSE)) else_b = parse_stmt(p);
    Node *n = node_new(ST_IF, line);
    n->as.if_stmt.cond = cond;
    n->as.if_stmt.then_branch = then_b;
    n->as.if_stmt.else_branch = else_b;
    return n;
}

static Node *parse_while(Parser *p) {
    int line = p->cur.line;
    advance(p);
    expect(p, TK_LPAREN, "'('");
    Node *cond = parse_expr(p);
    expect(p, TK_RPAREN, "')'");
    Node *body = parse_stmt(p);
    Node *n = node_new(ST_WHILE, line);
    n->as.while_stmt.cond = cond;
    n->as.while_stmt.body = body;
    return n;
}

/* Detect type-starting tokens for declarations inside for-init / statements */
static bool starts_type(Parser *p) {
    if (p->cur.type == TK_KW_CONST) return true;
    if (is_type_keyword(p->cur.type)) return true;
    return false;
}

static Node *parse_for(Parser *p) {
    int line = p->cur.line;
    advance(p);
    expect(p, TK_LPAREN, "'('");
    Node *init = NULL;
    if (!check(p, TK_SEMI)) {
        if (starts_type(p)) {
            TypeRef ty = parse_type(p);
            init = parse_var_decl(p, ty);  /* consumes ; */
        } else {
            Node *e = parse_expr(p);
            Node *es = node_new(ST_EXPR, line);
            es->as.expr_stmt.expr = e;
            init = es;
            expect(p, TK_SEMI, "';'");
        }
    } else {
        advance(p); /* ; */
    }
    Node *cond = NULL;
    if (!check(p, TK_SEMI)) cond = parse_expr(p);
    expect(p, TK_SEMI, "';'");
    Node *post = NULL;
    if (!check(p, TK_RPAREN)) post = parse_expr(p);
    expect(p, TK_RPAREN, "')'");
    Node *body = parse_stmt(p);
    Node *n = node_new(ST_FOR, line);
    n->as.for_stmt.init = init;
    n->as.for_stmt.cond = cond;
    n->as.for_stmt.post = post;
    n->as.for_stmt.body = body;
    return n;
}

static Node *parse_return(Parser *p) {
    int line = p->cur.line;
    advance(p);
    Node *v = NULL;
    if (!check(p, TK_SEMI)) v = parse_expr(p);
    expect(p, TK_SEMI, "';'");
    Node *n = node_new(ST_RETURN, line);
    n->as.ret.value = v;
    return n;
}

static Node *parse_module(Parser *p) {
    int line = p->cur.line;
    advance(p); /* module */
    /* dotted name: a.b.c */
    char *buf = NULL; size_t len = 0, cap = 0;
    if (p->cur.type != TK_IDENT) p_error(p, "expected module name");
    for (;;) {
        const char *s = p->cur.svalue;
        size_t sl = strlen(s);
        if (len + sl + 2 > cap) { cap = (len + sl + 2) * 2; buf = (char*)sb_xrealloc(buf, cap); }
        memcpy(buf + len, s, sl); len += sl; buf[len] = '\0';
        advance(p);
        if (!match(p, TK_DOT)) break;
        if (p->cur.type != TK_IDENT) p_error(p, "expected identifier after '.'");
        buf[len++] = '.'; buf[len] = '\0';
    }
    expect(p, TK_SEMI, "';'");
    Node *n = node_new(ST_MODULE, line);
    n->as.module_stmt.name = buf ? buf : sb_strdup("");
    return n;
}

static Node *parse_struct_decl(Parser *p) {
    int line = p->cur.line;
    advance(p); /* struct */
    if (p->cur.type != TK_IDENT) p_error(p, "expected struct name");
    char *name = p->cur.svalue; p->cur.svalue = NULL;
    advance(p);
    expect(p, TK_LBRACE, "'{'");
    StructField *fields = NULL;
    size_t fc = 0, fcap = 0;
    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        TypeRef ft = parse_type(p);
        if (p->cur.type != TK_IDENT) p_error(p, "expected field name");
        char *fn = p->cur.svalue; p->cur.svalue = NULL;
        advance(p);
        expect(p, TK_SEMI, "';'");
        if (fc == fcap) { fcap = fcap ? fcap * 2 : 4; fields = (StructField*)sb_xrealloc(fields, fcap * sizeof(StructField)); }
        fields[fc].type = ft; fields[fc].name = fn; fc++;
    }
    expect(p, TK_RBRACE, "'}'");
    /* trailing ';' optional (C-style) */
    match(p, TK_SEMI);
    Node *n = node_new(ST_STRUCT_DECL, line);
    n->as.struct_decl.name = name;
    n->as.struct_decl.fields = fields;
    n->as.struct_decl.field_count = fc;
    return n;
}

static Node *parse_enum_decl(Parser *p) {
    int line = p->cur.line;
    advance(p); /* enum */
    if (p->cur.type != TK_IDENT) p_error(p, "expected enum name");
    char *name = p->cur.svalue; p->cur.svalue = NULL;
    advance(p);
    expect(p, TK_LBRACE, "'{'");
    EnumMember *members = NULL;
    size_t mc = 0, mcap = 0;
    if (!check(p, TK_RBRACE)) {
        for (;;) {
            if (p->cur.type != TK_IDENT) p_error(p, "expected enum member name");
            char *mn = p->cur.svalue; p->cur.svalue = NULL;
            advance(p);
            long long mv = 0;
            bool has_v = false;
            if (match(p, TK_ASSIGN)) {
                bool neg = false;
                if (match(p, TK_MINUS)) neg = true;
                else match(p, TK_PLUS);
                if (p->cur.type != TK_INT) p_error(p, "expected integer literal in enum value");
                mv = neg ? -p->cur.ivalue : p->cur.ivalue;
                has_v = true;
                advance(p);
            }
            if (mc == mcap) { mcap = mcap ? mcap * 2 : 4; members = (EnumMember*)sb_xrealloc(members, mcap * sizeof(EnumMember)); }
            members[mc].name = mn;
            members[mc].value = mv;
            members[mc].has_value = has_v;
            mc++;
            if (!match(p, TK_COMMA)) break;
            if (check(p, TK_RBRACE)) break;
        }
    }
    expect(p, TK_RBRACE, "'}'");
    match(p, TK_SEMI);
    Node *n = node_new(ST_ENUM_DECL, line);
    n->as.enum_decl.name = name;
    n->as.enum_decl.members = members;
    n->as.enum_decl.count = mc;
    return n;
}

/* ---------- class / interface ---------- */

static Node *parse_class_body(Parser *p, const char *class_name, Node *out) {
    expect(p, TK_LBRACE, "'{'");
    ClassField *fields = NULL; size_t fc = 0, fcap = 0;
    Node      **methods = NULL; size_t mc = 0, mcap = 0;

    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        int mline = p->cur.line;
        /* Constructor: IDENT == class name and next is '(' */
        if (p->cur.type == TK_IDENT && p->cur.svalue
            && strcmp(p->cur.svalue, class_name) == 0)
        {
            Token *pk = peek_tok(p);
            if (pk->type == TK_LPAREN) {
                /* synthesize a void return type */
                TypeRef rt = {0}; rt.type_name = sb_strdup("void"); rt.is_const = false;
                Node *ctor = parse_func_decl(p, rt);
                /* rename to "<init>" so user-facing name doesn't clash with the class itself */
                free(ctor->as.func.name);
                ctor->as.func.name = sb_strdup("<init>");
                if (mc == mcap) { mcap = mcap ? mcap * 2 : 4; methods = (Node**)sb_xrealloc(methods, mcap * sizeof(Node*)); }
                methods[mc] = ctor;
                out->as.class_decl.ctor_index = (int)mc;
                mc++;
                continue;
            }
        }
        /* otherwise: parse a type, then ident, then either '(' (method) or '=' / ';' (field) */
        TypeRef ty = parse_type(p);
        if (p->cur.type != TK_IDENT) p_error(p, "expected field or method name");
        char *member_name = p->cur.svalue; p->cur.svalue = NULL;
        advance(p);
        if (check(p, TK_LPAREN)) {
            advance(p); /* ( */
            Param *params = NULL; size_t pc = 0, pcap = 0;
            if (!check(p, TK_RPAREN)) {
                for (;;) {
                    TypeRef pt = parse_type(p);
                    if (p->cur.type != TK_IDENT) p_error(p, "expected parameter name");
                    char *pn = p->cur.svalue; p->cur.svalue = NULL;
                    advance(p);
                    if (pc == pcap) { pcap = pcap ? pcap*2 : 4; params = (Param*)sb_xrealloc(params, pcap*sizeof(Param)); }
                    params[pc].type = pt; params[pc].name = pn; pc++;
                    if (!match(p, TK_COMMA)) break;
                }
            }
            expect(p, TK_RPAREN, "')'");
            Node *body = parse_block(p);
            Node *fn = node_new(ST_FUNC_DECL, mline);
            fn->as.func.ret_type = ty;
            fn->as.func.name = member_name;
            fn->as.func.params = params;
            fn->as.func.param_count = pc;
            fn->as.func.body = body;
            if (mc == mcap) { mcap = mcap ? mcap * 2 : 4; methods = (Node**)sb_xrealloc(methods, mcap * sizeof(Node*)); }
            methods[mc++] = fn;
        } else {
            Node *init = NULL;
            if (match(p, TK_ASSIGN)) init = parse_expr(p);
            expect(p, TK_SEMI, "';'");
            if (fc == fcap) { fcap = fcap ? fcap * 2 : 4; fields = (ClassField*)sb_xrealloc(fields, fcap * sizeof(ClassField)); }
            fields[fc].type = ty;
            fields[fc].name = member_name;
            fields[fc].init = init;
            fc++;
        }
    }
    expect(p, TK_RBRACE, "'}'");
    match(p, TK_SEMI);
    out->as.class_decl.fields = fields;
    out->as.class_decl.field_count = fc;
    out->as.class_decl.methods = methods;
    out->as.class_decl.method_count = mc;
    return out;
}

static Node *parse_class_decl(Parser *p) {
    int line = p->cur.line;
    advance(p); /* class */
    if (p->cur.type != TK_IDENT) p_error(p, "expected class name");
    char *name = p->cur.svalue; p->cur.svalue = NULL;
    advance(p);

    char  *base_name = NULL;
    char **iface_names = NULL;
    size_t iface_count = 0, iface_cap = 0;

    if (match(p, TK_COLON)) {
        if (p->cur.type != TK_IDENT) p_error(p, "expected base name after ':'");
        base_name = p->cur.svalue; p->cur.svalue = NULL;
        advance(p);
        while (match(p, TK_COMMA)) {
            if (p->cur.type != TK_IDENT) p_error(p, "expected interface name");
            if (iface_count == iface_cap) {
                iface_cap = iface_cap ? iface_cap * 2 : 4;
                iface_names = (char**)sb_xrealloc(iface_names, iface_cap * sizeof(char*));
            }
            iface_names[iface_count++] = p->cur.svalue;
            p->cur.svalue = NULL;
            advance(p);
        }
    }

    Node *n = node_new(ST_CLASS_DECL, line);
    n->as.class_decl.name = name;
    n->as.class_decl.base_name = base_name;
    n->as.class_decl.interface_names = iface_names;
    n->as.class_decl.interface_count = iface_count;
    n->as.class_decl.ctor_index = -1;
    parse_class_body(p, name, n);
    return n;
}

static Node *parse_interface_decl(Parser *p) {
    int line = p->cur.line;
    advance(p); /* interface */
    if (p->cur.type != TK_IDENT) p_error(p, "expected interface name");
    char *name = p->cur.svalue; p->cur.svalue = NULL;
    advance(p);

    char **bases = NULL; size_t bc = 0, bcap = 0;
    if (match(p, TK_COLON)) {
        for (;;) {
            if (p->cur.type != TK_IDENT) p_error(p, "expected interface name");
            if (bc == bcap) { bcap = bcap ? bcap * 2 : 4; bases = (char**)sb_xrealloc(bases, bcap * sizeof(char*)); }
            bases[bc++] = p->cur.svalue; p->cur.svalue = NULL;
            advance(p);
            if (!match(p, TK_COMMA)) break;
        }
    }

    expect(p, TK_LBRACE, "'{'");
    InterfaceMethod *methods = NULL; size_t mc = 0, mcap = 0;
    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        TypeRef rt = parse_type(p);
        if (p->cur.type != TK_IDENT) p_error(p, "expected method name");
        char *mn = p->cur.svalue; p->cur.svalue = NULL;
        advance(p);
        expect(p, TK_LPAREN, "'('");
        Param *params = NULL; size_t pc = 0, pcap = 0;
        if (!check(p, TK_RPAREN)) {
            for (;;) {
                TypeRef pt = parse_type(p);
                if (p->cur.type != TK_IDENT) p_error(p, "expected parameter name");
                char *pn = p->cur.svalue; p->cur.svalue = NULL;
                advance(p);
                if (pc == pcap) { pcap = pcap ? pcap*2 : 4; params = (Param*)sb_xrealloc(params, pcap*sizeof(Param)); }
                params[pc].type = pt; params[pc].name = pn; pc++;
                if (!match(p, TK_COMMA)) break;
            }
        }
        expect(p, TK_RPAREN, "')'");
        expect(p, TK_SEMI, "';'");
        if (mc == mcap) { mcap = mcap ? mcap * 2 : 4; methods = (InterfaceMethod*)sb_xrealloc(methods, mcap * sizeof(InterfaceMethod)); }
        methods[mc].ret_type = rt;
        methods[mc].name = mn;
        methods[mc].params = params;
        methods[mc].param_count = pc;
        mc++;
    }
    expect(p, TK_RBRACE, "'}'");
    match(p, TK_SEMI);
    Node *n = node_new(ST_INTERFACE_DECL, line);
    n->as.iface_decl.name = name;
    n->as.iface_decl.base_names = bases;
    n->as.iface_decl.base_count = bc;
    n->as.iface_decl.methods = methods;
    n->as.iface_decl.method_count = mc;
    return n;
}

static Node *parse_stmt(Parser *p) {
    int line = p->cur.line;
    switch (p->cur.type) {
        case TK_KW_MODULE: return parse_module(p);
        case TK_LBRACE: return parse_block(p);
        case TK_KW_IF: return parse_if(p);
        case TK_KW_WHILE: return parse_while(p);
        case TK_KW_FOR: return parse_for(p);
        case TK_KW_RETURN: return parse_return(p);
        case TK_KW_BREAK: advance(p); expect(p, TK_SEMI, "';'"); return node_new(ST_BREAK, line);
        case TK_KW_CONTINUE: advance(p); expect(p, TK_SEMI, "';'"); return node_new(ST_CONTINUE, line);
        case TK_KW_STRUCT: return parse_struct_decl(p);
        case TK_KW_ENUM:   return parse_enum_decl(p);
        case TK_KW_CLASS:  return parse_class_decl(p);
        case TK_KW_INTERFACE: return parse_interface_decl(p);
        case TK_SEMI: advance(p); { Node *e = node_new(ST_EXPR, line); e->as.expr_stmt.expr = NULL; return e; }
        default: break;
    }

    /* declaration vs expression-stmt: if token sequence is type ident '(' -> func; type ident -> var; else expr */
    if (starts_type(p)) {
        /* lookahead: peek after type to detect form */
        /* simplest: parse type, then look at ident, then peek next */
        TypeRef ty = parse_type(p);
        if (p->cur.type != TK_IDENT) p_error(p, "expected identifier after type");
        Token *pk = peek_tok(p);
        if (pk->type == TK_LPAREN) {
            return parse_func_decl(p, ty);
        }
        return parse_var_decl(p, ty);
    }

    /* user-typed declaration: IDENT IDENT (= ... ;) or IDENT IDENT ( ... ) */
    if (p->cur.type == TK_IDENT) {
        Token *pk = peek_tok(p);
        if (pk->type == TK_IDENT) {
            TypeRef ty = parse_type(p);
            Token *pk2 = peek_tok(p);
            if (pk2->type == TK_LPAREN) return parse_func_decl(p, ty);
            return parse_var_decl(p, ty);
        }
    }

    Node *e = parse_expr(p);
    expect(p, TK_SEMI, "';'");
    Node *s = node_new(ST_EXPR, line);
    s->as.expr_stmt.expr = e;
    return s;
}

Node *parser_parse_program(Parser *p) {
    Node *prog = node_new(ST_BLOCK, 1);
    while (!check(p, TK_EOF)) {
        nodelist_push(&prog->as.block.stmts, parse_stmt(p));
    }
    return prog;
}
