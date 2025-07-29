#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

Token create_token(TokenType type, char *value, size_t ln, size_t col) {
    return (Token){ .type = type, .value = value, .ln = ln, .col = col };
}

void delete_token(Token *tok) {
    free(tok->value);
}

char *tokentype_to_string(TokenType type) {
    switch (type) {
        case TOK_EOF: return "eof";
        case TOK_EOL: return "eol";
        case TOK_ID: return "identifier";
        case TOK_INT: return "int";
        case TOK_FLOAT: return "float";
        case TOK_STRING: return "string";
        case TOK_COLON: return "colon";
        case TOK_DOT: return "dot";
        case TOK_TOS: return "tos";
        default: break;
    }

    assert(false);
    return "undefined";
}