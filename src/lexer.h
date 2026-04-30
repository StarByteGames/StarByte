#ifndef STARBYTE_LEXER_H
#define STARBYTE_LEXER_H

#include "common.h"

typedef enum {
    TK_EOF = 0,

    /* literals & identifiers */
    TK_IDENT,
    TK_INT,
    TK_FLOAT,
    TK_STRING,
    TK_CHAR,

    /* keywords */
    TK_KW_MODULE,
    TK_KW_IF,
    TK_KW_ELSE,
    TK_KW_WHILE,
    TK_KW_FOR,
    TK_KW_DO,
    TK_KW_RETURN,
    TK_KW_BREAK,
    TK_KW_CONTINUE,
    TK_KW_TRUE,
    TK_KW_FALSE,
    TK_KW_NULL,
    TK_KW_CONST,
    TK_KW_VOID,
    TK_KW_INT,
    TK_KW_FLOAT,
    TK_KW_CHAR,
    TK_KW_BOOL,
    TK_KW_STRING,
    TK_KW_STRUCT,
    TK_KW_ENUM,
    TK_KW_SWITCH,
    TK_KW_CASE,
    TK_KW_DEFAULT,
    TK_KW_CLASS,
    TK_KW_INTERFACE,
    TK_KW_NEW,

    /* punctuation */
    TK_LPAREN,    /* ( */
    TK_RPAREN,    /* ) */
    TK_LBRACE,    /* { */
    TK_RBRACE,    /* } */
    TK_LBRACKET,  /* [ */
    TK_RBRACKET,  /* ] */
    TK_SEMI,      /* ; */
    TK_COMMA,     /* , */
    TK_DOT,       /* . */
    TK_COLON,     /* : */

    /* operators */
    TK_ASSIGN,    /* = */
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_PLUSPLUS, TK_MINUSMINUS,
    TK_PLUSEQ, TK_MINUSEQ, TK_STAREQ, TK_SLASHEQ, TK_PERCENTEQ,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_AND, TK_OR, TK_NOT,
    TK_QUESTION
} TokenType;

typedef struct {
    TokenType type;
    const char *start;   /* pointer into source */
    size_t      length;
    int         line;
    /* literal cache */
    long long   ivalue;
    double      fvalue;
    char       *svalue;  /* malloc'd for TK_STRING / TK_IDENT (escaped/copied) */
} Token;

typedef struct {
    const char *source;
    const char *cur;
    int         line;
} Lexer;

void  lexer_init(Lexer *lx, const char *source);
Token lexer_next(Lexer *lx);
void  token_free(Token *tk);
const char *token_type_name(TokenType t);

#endif
