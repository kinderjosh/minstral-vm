#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>

typedef enum {
    TOK_EOF,
    TOK_EOL,
    TOK_ID,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_COLON,
    TOK_DOT,
    TOK_TOS
} TokenType;

typedef struct {
    TokenType type;
    char *value;
    size_t ln;
    size_t col;
} Token;

Token create_token(TokenType type, char *value, size_t ln, size_t col);
void delete_token(Token *tok);
char *tokentype_to_string(TokenType type);

#endif