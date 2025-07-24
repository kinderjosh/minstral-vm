#ifndef PARSER_H
#define PARSER_H

#include "vm.h"
#include "token.h"
#include <stdio.h>
#include <stdint.h>

typedef struct {
    Opcode opcode;
    i64 operand;
} Op;

typedef struct {
    char *file;
    Token *tokens;
    size_t token_count;
    Token *tok;
    size_t pos;
    unsigned int flags;
} Parser;

typedef struct {
    Op *ops;
    size_t op_count;
} Root;

Op parse_stmt(Parser *prs);
Root parse_root(char *file);
void delete_root(Root *root);

#endif