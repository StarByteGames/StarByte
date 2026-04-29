#include "lexer.h"
#include <ctype.h>

void lexer_init(Lexer *lx, const char *source) {
    lx->source = source;
    lx->cur = source;
    lx->line = 1;
}

void token_free(Token *tk) {
    if (!tk) return;
    if (tk->svalue) { free(tk->svalue); tk->svalue = NULL; }
}

static void skip_ws_and_comments(Lexer *lx) {
    for (;;) {
        char c = *lx->cur;
        if (c == ' ' || c == '\t' || c == '\r') { lx->cur++; }
        else if (c == '\n') { lx->line++; lx->cur++; }
        else if (c == '/' && lx->cur[1] == '/') {
            while (*lx->cur && *lx->cur != '\n') lx->cur++;
        } else if (c == '/' && lx->cur[1] == '*') {
            lx->cur += 2;
            while (*lx->cur && !(lx->cur[0] == '*' && lx->cur[1] == '/')) {
                if (*lx->cur == '\n') lx->line++;
                lx->cur++;
            }
            if (*lx->cur) lx->cur += 2;
        } else if (c == '#') {
            /* preprocessor-style line: ignore until newline (kept simple) */
            while (*lx->cur && *lx->cur != '\n') lx->cur++;
        } else {
            break;
        }
    }
}

typedef struct { const char *kw; TokenType t; } KW;
static const KW KEYWORDS[] = {
    {"module", TK_KW_MODULE}, {"if", TK_KW_IF}, {"else", TK_KW_ELSE},
    {"while", TK_KW_WHILE}, {"for", TK_KW_FOR}, {"do", TK_KW_DO},
    {"return", TK_KW_RETURN}, {"break", TK_KW_BREAK}, {"continue", TK_KW_CONTINUE},
    {"true", TK_KW_TRUE}, {"false", TK_KW_FALSE}, {"null", TK_KW_NULL},
    {"const", TK_KW_CONST}, {"void", TK_KW_VOID}, {"int", TK_KW_INT},
    {"float", TK_KW_FLOAT}, {"char", TK_KW_CHAR}, {"bool", TK_KW_BOOL},
    {"string", TK_KW_STRING}, {"struct", TK_KW_STRUCT}, {"enum", TK_KW_ENUM},
    {"switch", TK_KW_SWITCH}, {"case", TK_KW_CASE}, {"default", TK_KW_DEFAULT},
    {NULL, TK_EOF}
};

static TokenType ident_keyword(const char *s, size_t n) {
    for (int i = 0; KEYWORDS[i].kw; i++) {
        if (strlen(KEYWORDS[i].kw) == n && strncmp(KEYWORDS[i].kw, s, n) == 0)
            return KEYWORDS[i].t;
    }
    return TK_IDENT;
}

static Token make_simple(Lexer *lx, TokenType t, const char *start, size_t len) {
    Token tk = {0};
    tk.type = t;
    tk.start = start;
    tk.length = len;
    tk.line = lx->line;
    return tk;
}

static Token lex_string(Lexer *lx) {
    int line = lx->line;
    lx->cur++; /* opening " */
    /* dynamic buffer for escape processing */
    size_t cap = 16, len = 0;
    char *buf = (char*)sb_xmalloc(cap);
    while (*lx->cur && *lx->cur != '"') {
        char c = *lx->cur++;
        if (c == '\\' && *lx->cur) {
            char e = *lx->cur++;
            switch (e) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case '\'': c = '\''; break;
                case '0': c = '\0'; break;
                default: c = e; break;
            }
        }
        if (c == '\n') lx->line++;
        if (len + 1 >= cap) { cap *= 2; buf = (char*)sb_xrealloc(buf, cap); }
        buf[len++] = c;
    }
    if (*lx->cur == '"') lx->cur++;
    buf[len] = '\0';
    Token tk = {0};
    tk.type = TK_STRING;
    tk.line = line;
    tk.svalue = buf;
    tk.length = len;
    return tk;
}

static Token lex_char(Lexer *lx) {
    int line = lx->line;
    lx->cur++; /* ' */
    char c = *lx->cur ? *lx->cur++ : 0;
    if (c == '\\' && *lx->cur) {
        char e = *lx->cur++;
        switch (e) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            case '"': c = '"'; break;
            case '0': c = '\0'; break;
            default: c = e; break;
        }
    }
    if (*lx->cur == '\'') lx->cur++;
    Token tk = {0};
    tk.type = TK_CHAR;
    tk.line = line;
    tk.ivalue = (long long)(unsigned char)c;
    return tk;
}

static Token lex_number(Lexer *lx) {
    const char *start = lx->cur;
    int line = lx->line;
    bool is_float = false;
    while (isdigit((unsigned char)*lx->cur)) lx->cur++;
    if (*lx->cur == '.' && isdigit((unsigned char)lx->cur[1])) {
        is_float = true;
        lx->cur++;
        while (isdigit((unsigned char)*lx->cur)) lx->cur++;
    }
    if (*lx->cur == 'e' || *lx->cur == 'E') {
        is_float = true;
        lx->cur++;
        if (*lx->cur == '+' || *lx->cur == '-') lx->cur++;
        while (isdigit((unsigned char)*lx->cur)) lx->cur++;
    }
    Token tk = {0};
    tk.start = start; tk.length = (size_t)(lx->cur - start); tk.line = line;
    char *tmp = sb_strndup(start, tk.length);
    if (is_float) { tk.type = TK_FLOAT; tk.fvalue = strtod(tmp, NULL); }
    else          { tk.type = TK_INT;   tk.ivalue = strtoll(tmp, NULL, 10); }
    free(tmp);
    return tk;
}

static Token lex_ident(Lexer *lx) {
    const char *start = lx->cur;
    int line = lx->line;
    while (isalnum((unsigned char)*lx->cur) || *lx->cur == '_') lx->cur++;
    size_t n = (size_t)(lx->cur - start);
    Token tk = {0};
    tk.type = ident_keyword(start, n);
    tk.start = start; tk.length = n; tk.line = line;
    if (tk.type == TK_IDENT) tk.svalue = sb_strndup(start, n);
    return tk;
}

Token lexer_next(Lexer *lx) {
    skip_ws_and_comments(lx);
    const char *start = lx->cur;
    char c = *lx->cur;
    if (c == '\0') return make_simple(lx, TK_EOF, start, 0);

    if (isalpha((unsigned char)c) || c == '_') return lex_ident(lx);
    if (isdigit((unsigned char)c)) return lex_number(lx);
    if (c == '"') return lex_string(lx);
    if (c == '\'') return lex_char(lx);

    lx->cur++;
    char n = *lx->cur;
    switch (c) {
        case '(': return make_simple(lx, TK_LPAREN, start, 1);
        case ')': return make_simple(lx, TK_RPAREN, start, 1);
        case '{': return make_simple(lx, TK_LBRACE, start, 1);
        case '}': return make_simple(lx, TK_RBRACE, start, 1);
        case '[': return make_simple(lx, TK_LBRACKET, start, 1);
        case ']': return make_simple(lx, TK_RBRACKET, start, 1);
        case ';': return make_simple(lx, TK_SEMI, start, 1);
        case ',': return make_simple(lx, TK_COMMA, start, 1);
        case '.': return make_simple(lx, TK_DOT, start, 1);
        case ':': return make_simple(lx, TK_COLON, start, 1);
        case '?': return make_simple(lx, TK_QUESTION, start, 1);
        case '+':
            if (n == '+') { lx->cur++; return make_simple(lx, TK_PLUSPLUS, start, 2); }
            if (n == '=') { lx->cur++; return make_simple(lx, TK_PLUSEQ, start, 2); }
            return make_simple(lx, TK_PLUS, start, 1);
        case '-':
            if (n == '-') { lx->cur++; return make_simple(lx, TK_MINUSMINUS, start, 2); }
            if (n == '=') { lx->cur++; return make_simple(lx, TK_MINUSEQ, start, 2); }
            return make_simple(lx, TK_MINUS, start, 1);
        case '*':
            if (n == '=') { lx->cur++; return make_simple(lx, TK_STAREQ, start, 2); }
            return make_simple(lx, TK_STAR, start, 1);
        case '/':
            if (n == '=') { lx->cur++; return make_simple(lx, TK_SLASHEQ, start, 2); }
            return make_simple(lx, TK_SLASH, start, 1);
        case '%':
            if (n == '=') { lx->cur++; return make_simple(lx, TK_PERCENTEQ, start, 2); }
            return make_simple(lx, TK_PERCENT, start, 1);
        case '=':
            if (n == '=') { lx->cur++; return make_simple(lx, TK_EQ, start, 2); }
            return make_simple(lx, TK_ASSIGN, start, 1);
        case '!':
            if (n == '=') { lx->cur++; return make_simple(lx, TK_NEQ, start, 2); }
            return make_simple(lx, TK_NOT, start, 1);
        case '<':
            if (n == '=') { lx->cur++; return make_simple(lx, TK_LE, start, 2); }
            return make_simple(lx, TK_LT, start, 1);
        case '>':
            if (n == '=') { lx->cur++; return make_simple(lx, TK_GE, start, 2); }
            return make_simple(lx, TK_GT, start, 1);
        case '&':
            if (n == '&') { lx->cur++; return make_simple(lx, TK_AND, start, 2); }
            break;
        case '|':
            if (n == '|') { lx->cur++; return make_simple(lx, TK_OR, start, 2); }
            break;
    }
    fprintf(stderr, "starbyte: lex error: unexpected character '%c' at line %d\n", c, lx->line);
    exit(1);
}

const char *token_type_name(TokenType t) {
    switch (t) {
        case TK_EOF: return "EOF";
        case TK_IDENT: return "ident";
        case TK_INT: return "int";
        case TK_FLOAT: return "float";
        case TK_STRING: return "string";
        case TK_CHAR: return "char";
        default: return "token";
    }
}
