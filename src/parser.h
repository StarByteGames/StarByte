#ifndef STARBYTE_PARSER_H
#define STARBYTE_PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer lx;
    Token cur;
    Token peek;
    const char *filename;
    bool has_peek;
} Parser;

void  parser_init(Parser *p, const char *source, const char *filename);
Node *parser_parse_program(Parser *p);  /* returns ST_BLOCK with top-level statements */
void  parser_dispose(Parser *p);

#endif
