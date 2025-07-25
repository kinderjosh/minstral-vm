#include "lexer.h"
#include "token.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

Lexer create_lexer(char *file) {
    FILE *f = fopen(file, "r");

    if (f == NULL) {
        fprintf(stderr, "%s: error: no such file exists\n", file);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    char *src = malloc(file_size + 1);
    size_t read_size = fread(src, 1, file_size, f);
    fclose(f);

    if (file_size != read_size) {
        fprintf(stderr, "%s: error: failed to read file\n", file);
        free(src);
        exit(1);
    }

    src[read_size] = '\0';

    return (Lexer){ 
        .file = file,
        .src = src, 
        .src_len = read_size,
        .cur = src[0],
        .pos = 0, 
        .ln = 1, 
        .col = 1 
    };
}

void delete_lexer(Lexer *lex) {
    free(lex->src);
}

static void step(Lexer *lex) {
    if (lex->pos >= lex->src_len)
        return;
    else if (lex->cur == '\n') {
        lex->ln++;
        lex->col = 1;
    } else
        lex->col++;
    
    lex->cur = lex->src[++lex->pos];
}

static char peek(Lexer *lex, int offset) {
    if (lex->pos + offset >= lex->src_len)
        return lex->src[lex->src_len - 1];
    else if ((int)lex->pos + offset < 1)
        return lex->src[0];
    
    return lex->src[lex->pos + offset];
}

static Token create_and_step(Lexer *lex, TokenType type, char *value) {
    Token tok = create_token(type, mystrdup(value), lex->ln, lex->col);

    while (*value) {
        step(lex);
        value++;
    }

    return tok;
}

static Token skip_comment(Lexer *lex) {
    step(lex);

    while (lex->cur != '\0' && lex->cur != '\n')
        step(lex);

    if (lex->cur == '\n')
        return create_and_step(lex, TOK_EOL, "\n");

    return create_token(TOK_EOF, mystrdup("eof"), lex->ln, lex->col);
}

static Token lex_id(Lexer *lex) {
    size_t col = lex->col;
    size_t realloc_size = 16;
    char *value = malloc(realloc_size);
    size_t len = 0;

    while (isalnum(lex->cur) || lex->cur == '_') {
        if (len + 2 >= realloc_size) {
            realloc_size *= 2;
            value = realloc(value, realloc_size);
        }

        value[len++] = tolower(lex->cur);
        step(lex);
    }

    value[len] = '\0';
    return create_token(TOK_ID, value, lex->ln, col);
}

static Token lex_prefixed_digit(Lexer *lex, size_t col, bool has_minus) {
    size_t realloc_size = 16;
    char *value = malloc(realloc_size);
    size_t len = 0;

    if (has_minus)
        value[len++] = '-';

    value[len++] = '0';
    step(lex);

    bool is_hex = false;

    if (lex->cur == 'x') {
        value[len++] = 'x';
        step(lex);
        is_hex = true;
    }

    while ((is_hex && (isdigit(lex->cur) || 
            (isalpha(lex->cur) && (tolower(lex->cur) >= 'a' || tolower(lex->cur <= 'z'))))) ||
            (!is_hex && isdigit(lex->cur) && lex->cur >= '0' && lex->cur <= '7')) {

        if (len + 2 >= realloc_size) {
            realloc_size *= 2;
            value = realloc(value, realloc_size);
        }

        value[len++] = lex->cur;
        step(lex);
    }

    value[len] = '\0';

    int64_t val;
    char *endptr;
    errno = 0;

    if (has_minus)
        val = strtol(value, &endptr, is_hex ? 0 : 8);
    else
        val = strtoul(value, &endptr, is_hex ? 0 : 8);

    if (endptr == value || *endptr != '\0') {
        fprintf(stderr, "%s:%zu:%zu: error: digit conversion failed\n", lex->file, lex->ln, lex->col);
        inc_errors();
        strcpy(value, "0");
        return create_token(TOK_INT, value, lex->ln, lex->col);
    } else if (errno == EINVAL || errno == ERANGE) {
        fprintf(stderr, "%s:%zu:%zu: error: digit conversion failed: %s\n", lex->file, lex->ln, lex->col, strerror(errno));
        inc_errors();
        strcpy(value, "0");
        return create_token(TOK_INT, value, lex->ln, lex->col);
    }

    value = realloc(value, 32);

    if (has_minus)
        sprintf(value, "%" PRId32, (int32_t)val);
    else
        sprintf(value, "%" PRIu32, (uint32_t)val);

    return create_token(TOK_INT, value, lex->ln, col);
}

static Token lex_digit(Lexer *lex) {
    size_t col = lex->col;
    size_t realloc_size = 16;
    char *value = malloc(realloc_size);
    size_t len = 0;

    bool has_decimal = false;
    bool has_minus = false;

    if (lex->cur == '-') {
        has_minus = true;
        value[len++] = '-';
        step(lex);
    }

    if (lex->cur == '0' && (peek(lex, 1) == 'x' || isdigit(peek(lex, 1)))) {
        free(value);
        return lex_prefixed_digit(lex, col, has_minus);
    }

    while (isdigit(lex->cur) || (lex->cur == '.' && len > 0 && !has_decimal && isdigit(peek(lex, 1))) ||
            (lex->cur == '_' && isdigit(peek(lex, 1)))) {

        if (lex->cur == '.')
            has_decimal = true;
        else if (lex->cur == '_') {
            step(lex);
            continue;
        }

        if (len + 2 >= realloc_size) {
            realloc_size *= 2;
            value = realloc(value, realloc_size);
        }

        value[len++] = lex->cur;
        step(lex);
    }

    value[len] = '\0';

    if (lex->cur == 'f') {
        step(lex);

        if (!has_decimal) {
            value = realloc(value, len + 3);
            strcat(value, ".0");
        }

        return create_token(TOK_FLOAT, value, lex->ln, col);
    } else if (!has_decimal && (lex->cur == 'h' || lex->cur == 'o' || lex->cur == 'b')) {
        int radix;

        if (lex->cur == 'h')
            radix = 16;
        else if (lex->cur == 'o')
            radix = 8;
        else if (lex->cur == 'b')
            radix = 2;

        step(lex);

        char *endptr;
        errno = 0;
        int64_t val;

        if (value[0] == '-')
            val = strtol(value, &endptr, radix);
        else
            val = strtoul(value, &endptr, radix);

        if (endptr == value || *endptr != '\0') {
            fprintf(stderr, "%s:%zu:%zu: error: digit conversion failed\n", lex->file, lex->ln, lex->col);
            inc_errors();
            strcpy(value, "0");
            return create_token(TOK_INT, value, lex->ln, lex->col);
        } else if (errno == EINVAL || errno == ERANGE) {
            fprintf(stderr, "%s:%zu:%zu: error: digit conversion failed: %s\n", lex->file, lex->ln, lex->col, strerror(errno));
            inc_errors();
            strcpy(value, "0");
            return create_token(TOK_INT, value, lex->ln, lex->col);
        }

        value = realloc(value, 32);

        if (value[0] == '-')
            sprintf(value, "%" PRId32, (int32_t)val);
        else
            sprintf(value, "%" PRIu32, (uint32_t)val);
    }

    return create_token(has_decimal ? TOK_FLOAT : TOK_INT, value, lex->ln, col);
}

static Token lex_char(Lexer *lex) {
    size_t col = lex->col;
    char *value = malloc(16);
    step(lex);

    if (lex->cur == '\\') {
        step(lex);

        switch (lex->cur) {
            case 'n':
                strcpy(value, "10");
                break;
            case 't':
                strcpy(value, "9");
                break;
            case 'r':
                strcpy(value, "13");
                break;
            case '0':
                strcpy(value, "0");
                break;
            case '\'':
            case '"':
            case '\\':
                sprintf(value, "%d", (int)lex->cur);
                break;
            default:
                fprintf(stderr, "%s:%zu:%zu: unsupported escape sequence '\\%c'\n", lex->file, lex->ln, lex->col, lex->cur);
                inc_errors();
                value[0] = '\0';
                break;
        }
    } else
        sprintf(value, "%d", (int)lex->cur);

    char *endptr;
    errno = 0;
    int32_t val = strtol(value, &endptr, 10);

    if (endptr == value || *endptr != '\0') {
        fprintf(stderr, "%s:%zu:%zu: error: digit conversion failed\n", lex->file, lex->ln, lex->col);
        val = 0;
    } else if (errno == ERANGE || errno == EINVAL) {
        fprintf(stderr, "%s:%zu:%zu: error: digit conversion failed: %s\n", lex->file, lex->ln, lex->col, strerror(errno));
        val = 0;
    }

    sprintf(value, "%" PRId32, val);
    step(lex);

    if (lex->cur != '\'') {
        fprintf(stderr, "%s:%zu:%zu: error: unclosed character constant\n", lex->file, lex->ln, lex->col);
        inc_errors();
    } else
        step(lex);

    return (Token){ .type = TOK_INT, .value = value, .ln = lex->ln, .col = col };
}

/*
static Token lex_string(Lexer *lex) {
    size_t ln = lex->ln;
    size_t col = lex->col;

    size_t realloc_size = 16;
    char *value = malloc(realloc_size);
    size_t len = 0;
    step(lex);

    while (lex->cur != '\0' && lex->cur != '"') {
        if (len + 2 >= realloc_size) {
            realloc_size *= 2;
            value = realloc(value, realloc_size);
        }

        value[len++] = lex->cur;

        // Ehhhh I don't really like repeating myself here,
        // but trying to catch a \" in the while condition
        // gets a bit confusing.
        if (lex->cur == '\\' && peek(lex, 1) == '"') {
            if (len + 2 >= realloc_size) {
                realloc_size *= 2;
                value = realloc(value, realloc_size);
            }

            value[len++] = lex->cur;
            step(lex);
        }

        step(lex);
    }
    
    value[len] = '\0';

    if (lex->cur != '"') {
        log_error(lex->file, ln, col);
        fprintf(stderr, "unclosed string literal\n");
        show_error(ERR_UNCLOSED_CONST, lex->file, ln, col);
    } else
        step(lex);

    // Concatenate a following string if present.
    while (isspace(lex->cur))
        step(lex);

    while (lex->cur == '"') {
        Token next = lex_string(lex);

        value = realloc(value, (strlen(value) + strlen(next.value) + 1) * sizeof(char));
        strcat(value, next.value);
        free(next.value);

        while (isspace(lex->cur))
            step(lex);
    }

    return (Token){ .type = TOK_STRING, .value = value, .ln = ln, .col = col };
}
*/

Token lex_next_token(Lexer *lex) {
    while (isspace(lex->cur) && lex->cur != '\n')
        step(lex);

    if (lex->cur == '\n')
        return create_and_step(lex, TOK_EOL, "\n");
    else if (lex->cur == ';')
        return skip_comment(lex);
    else if (isalpha(lex->cur) || lex->cur == '_')
        return lex_id(lex);
    else if (isdigit(lex->cur) || (lex->cur == '-' && isdigit(peek(lex, 1))))
        return lex_digit(lex);
    else if (lex->cur == '\'')
        return lex_char(lex);
    //else if (lex->cur == '"')
      //  return lex_string(lex);

    switch (lex->cur) {
        case '\0': return create_token(TOK_EOF, mystrdup("eof"), lex->ln, lex->col);
        case ':': return create_and_step(lex, TOK_COLON, ":");
        default: break;
    }

    fprintf(stderr, "%s:%zu:%zu: error: unknown token '%c'\n", lex->file, lex->ln, lex->col, lex->cur);
    inc_errors();
    step(lex);
    return lex_next_token(lex);
}