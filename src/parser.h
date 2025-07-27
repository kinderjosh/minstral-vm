#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include "vm.h"
#include <stdio.h>

typedef struct {
    char *file;
    Token *tokens;
    size_t token_count;
    Token *tok;
    size_t pos;
} Parser;

typedef struct {
    Op *ops;
    size_t op_count;
    size_t op_capacity;
} Root;

Op parse_stmt(Parser *prs);
Root parse_root(char *file);
void delete_root(Root *root);

#endif