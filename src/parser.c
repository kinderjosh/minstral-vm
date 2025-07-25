#include "parser.h"
#include "vm.h"
#include "token.h"
#include "lexer.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#define NOOP (Op){ .opcode = NOP }
#define STARTING_TOK_CAP 32 
#define STARTING_ROOT_CAP 16

#define IN_FIRST_PASS (0x01)

#define TABLE_SIZE 1000

typedef struct {
    char *name;
    i64 value;
    bool used;
    char *file;
    size_t ln;
    size_t col;
} Label;

static Label labels[TABLE_SIZE];
static size_t label_count = 0;
static size_t op_count = 0;

uint32_t hash_FNV1a(const char *data, size_t size) {
    uint32_t h = 2166136261UL;

    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 16777619;
    }

    return h % TABLE_SIZE;
}

Label *find_label(char *name) {
    return &labels[hash_FNV1a(name, strlen(name))];
}

void add_label(char *name, i64 value, char *file, size_t ln, size_t col) {
    Label *label = find_label(name);

    if (label->used) {
        fprintf(stderr, "%s:%d: TABLE COLLISION\n", __FILE__, __LINE__);
        inc_errors();
        assert(false);
    }

    *label = (Label){ .name = name, .value = value, .used = true, .file = file, .ln = ln, .col = col };
    label_count++;
}

void delete_labels() {
    for (size_t i = 0; i < label_count; i++) {
        Label *label = &labels[i];

        if (!label->used)
            continue;

        free(label->name);
        free(label->file);
    }
}

Parser create_parser(char *file) {
    Lexer lex = create_lexer(file);
    Token tok;

    Token *tokens = malloc(STARTING_TOK_CAP * sizeof(Token));
    size_t token_count = 0;
    size_t token_capacity = STARTING_TOK_CAP;

    while ((tok = lex_next_token(&lex)).type != TOK_EOF) {
        if (token_count + 2 >= token_capacity) {
            token_capacity *= 2;
            tokens = realloc(tokens, token_capacity * sizeof(Token));
        }

        tokens[token_count++] = tok;
    }

    tokens[token_count++] = tok; // eof
    delete_lexer(&lex);

    return (Parser){
        .file = file,
        .tokens = tokens,
        .token_count = token_count,
        .tok = &tokens[0],
        .pos = 0,
        .flags = IN_FIRST_PASS
    };
}

void delete_parser(Parser *prs) {
    for (size_t i = 0; i < prs->token_count; i++)
        delete_token(&prs->tokens[i]);

    free(prs->tokens);
}

void eat(Parser *prs, TokenType type) {
    if (type != prs->tok->type) {
        fprintf(stderr, "%s:%zu:%zu: error: found token '%s' when expecting '%s'\n", prs->file, prs->tok->ln, prs->tok->col, tokentype_to_string(prs->tok->type), tokentype_to_string(type));
        inc_errors();
    }

    if (prs->tok->type != TOK_EOF)
        prs->tok = &prs->tokens[++prs->pos];
}

void eat_until(Parser *prs, TokenType type) {
    while (prs->tok->type != TOK_EOF && prs->tok->type != type)
        eat(prs, prs->tok->type);
}

Token *peek(Parser *prs, int offset) {
    if (prs->pos + offset >= prs->token_count)
        return &prs->tokens[prs->token_count - 1];
    else if ((int)prs->pos + offset < 1)
        return &prs->tokens[0];
        
    return &prs->tokens[prs->pos + offset];
}

i64 parse_digit(Parser *prs) {
    errno = 0;
    char *endptr;
    i64 value = strtoll(prs->tok->value, &endptr, 10);

    if (endptr == prs->tok->value || *endptr != '\0') {
        fprintf(stderr, "%s:%zu:%zu: error: digit conversion failed\n", prs->file, prs->tok->ln, prs->tok->col);
        inc_errors();
        value = 0;
    } else if (errno == EINVAL || errno == ERANGE) {
        fprintf(stderr, "%s:%zu:%zu: error: digit conversion failed: %s\n", prs->file, prs->tok->ln, prs->tok->col, strerror(errno));
        inc_errors();
        value = 0;
    }

    eat(prs, TOK_INT);
    return value;
}

i64 parse_dat(Parser *prs) {
    Label *label = find_label(prs->tok->value);

    if (!label->used) {
        fprintf(stderr, "%s:%zu:%zu: error: undefined label '%s'\n", prs->file, prs->tok->ln, prs->tok->col, prs->tok->value);
        inc_errors();
        eat(prs, TOK_ID);
        return 0;
    }

    eat(prs, TOK_ID);
    return label->value;
}

i64 parse_operand(Parser *prs, bool *out_is_imm) {
    switch (prs->tok->type) {
        case TOK_INT:
            *out_is_imm = true;
            return parse_digit(prs);
        case TOK_ID:
            *out_is_imm = false;
            return parse_dat(prs);
        default: break;
    }

    fprintf(stderr, "%s:%zu:%zu: error: invalid operand type '%s'\n", prs->file, prs->tok->ln, prs->tok->col, tokentype_to_string(prs->tok->type));
    inc_errors();

    eat(prs, prs->tok->type);
    *out_is_imm = true;
    return 0;
}

i64 parse_label_operand(Parser *prs) {
    if (prs->tok->type != TOK_ID) {
        fprintf(stderr, "%s:%zu:%zu: error: expected label name but found '%s'\n", prs->file, prs->tok->ln, prs->tok->col, tokentype_to_string(prs->tok->type));
        inc_errors();
        eat(prs, prs->tok->type);
        return 0;
    }

    Label *label = find_label(prs->tok->value);

    if (!label->used & !(prs->flags & IN_FIRST_PASS)) {
        fprintf(stderr, "%s:%zu:%zu: error: undefined label '%s'\n", prs->file, prs->tok->ln, prs->tok->col, prs->tok->value);
        inc_errors();
        eat(prs, TOK_ID);
        return 0;
    }

    eat(prs, TOK_ID);
    return label->value;
}

Op parse_lda(Parser *prs) {
    bool is_imm;
    i64 operand = parse_operand(prs, &is_imm);
    return (Op){ .opcode = is_imm ? LDI : LDM, operand };
}

Op parse_sta(Parser *prs) {
    return (Op){ .opcode = STM, .operand = parse_label_operand(prs) };
}

Op parse_pr(Parser *prs, bool as_char) {
    if (prs->tok->type == TOK_EOL)
        return (Op){ .opcode = as_char ? PRCA : PRIA, 0 };
    else if (prs->tok->type == TOK_INT)
        return (Op){ .opcode = as_char ? PRCI : PRII, parse_digit(prs) };
    
    return (Op){ .opcode = as_char ? PRCM : PRIM, parse_label_operand(prs) };
}

Op parse_math(Parser *prs, char *instr) {
    Opcode opcode;

    if (strcmp(instr, "add") == 0)
        opcode = prs->tok->type == TOK_INT ? ADDI : ADDM;
    else if (strcmp(instr, "sub") == 0)
        opcode = prs->tok->type == TOK_INT ? SUBI : SUBM;
    else if (strcmp(instr, "mul") == 0)
        opcode = prs->tok->type == TOK_INT ? MULI : MULM;
    else if (strcmp(instr, "div") == 0)
        opcode = prs->tok->type == TOK_INT ? DIVI : DIVM;
    else if (strcmp(instr, "mod") == 0)
        opcode = prs->tok->type == TOK_INT ? MODI : MODM;
    else if (strcmp(instr, "shl") == 0)
        opcode = prs->tok->type == TOK_INT ? SHLI : SHLM;
    else if (strcmp(instr, "shr") == 0)
        opcode = prs->tok->type == TOK_INT ? SHRI : SHRM;
    else if (strcmp(instr, "and") == 0)
        opcode = prs->tok->type == TOK_INT ? ANDI : ANDM;
    else if (strcmp(instr, "or") == 0)
        opcode = prs->tok->type == TOK_INT ? ORI : ORM;
    else if (strcmp(instr, "xor") == 0)
        opcode = prs->tok->type == TOK_INT ? XORI : XORM;
    else if (strcmp(instr, "not") == 0)
        return (Op){ .opcode = NOT, .operand = 0 };
    else
        return (Op){ .opcode = NEG, .operand = 0 };

    return (Op){ .opcode = opcode, .operand = prs->tok->type == TOK_INT ? parse_digit(prs) : parse_label_operand(prs) };
}

Op parse_br(Parser *prs, char *instr) {
    Opcode opcode;

    if (strcmp(instr, "bra") == 0)
        opcode = BRA;
    else if (strcmp(instr, "brz") == 0)
        opcode = BRZ;
    else if (strcmp(instr, "brp") == 0)
        opcode = BRP;
    else
        opcode = BRN;

    return (Op){ .opcode = opcode, .operand = parse_label_operand(prs) };
}

Op parse_rd(Parser *prs, char *instr) {
    Opcode opcode;

    if (strcmp(instr, "rdc") == 0)
        opcode = prs->tok->type == TOK_EOL ? RDCA : RDCM;
    else
        opcode = prs->tok->type == TOK_EOL ? RDIA : RDIM;

    return (Op){ .opcode = opcode, .operand = prs->tok->type == TOK_EOL ? 0 : parse_label_operand(prs) };
}

Op parse_ref(Parser *prs) {
    return (Op){ .opcode = REFM, .operand = parse_label_operand(prs) };
}

Op parse_ldd(Parser *prs) {
    if (prs->tok->type == TOK_EOL)
        return (Op){ .opcode = LDDA, .operand = 0 };

    return (Op){ .opcode = LDDM, .operand = parse_label_operand(prs) };
}

Op parse_std(Parser *prs) {
    return (Op){ .opcode = STDM, .operand = parse_label_operand(prs) };
}

Op parse_id(Parser *prs) {
    size_t ln = prs->tok->ln;
    size_t col = prs->tok->col;
    
    char *id = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);

    if (prs->tok->type == TOK_COLON) {
        eat(prs, TOK_COLON);

        Label *label = find_label(id);

        if (label->used && prs->flags & IN_FIRST_PASS) {
            fprintf(stderr, "%s:%zu:%zu: error: redefinition of label '%s'; first defined at %s:%zu:%zu\n", prs->file, ln, col, id, label->file, label->ln, label->col);
            inc_errors();
            free(id);
        } else if (prs->flags & IN_FIRST_PASS) {
            add_label(id, op_count, mystrdup(prs->file), ln, col);

            if (strcmp(prs->tok->value, "dat") == 0) {
                eat(prs, TOK_ID);

                if (prs->tok->type == TOK_INT)
                    eat(prs, TOK_INT);
            }
        } else {
            free(id);

            if (strcmp(prs->tok->value, "dat") == 0) {
                eat(prs, TOK_ID);

                if (prs->tok->type == TOK_INT)
                    return (Op){ .opcode = NOP, .operand = parse_digit(prs) };
            }
        }

        return NOOP;
    } else if (strcmp(id, "hlt") == 0) {
        free(id);
        return (Op){ .opcode = HLT, .operand = 0 };
    } else if (strcmp(id, "lda") == 0) {
        free(id);
        return parse_lda(prs);
    } else if (strcmp(id, "sta") == 0) {
        free(id);
        return parse_sta(prs);
    } else if (strcmp(id, "prc") == 0 || strcmp(id, "pri") == 0) {
        Op op = parse_pr(prs, strcmp(id, "prc") == 0);
        free(id);
        return op;
    } else if (strcmp(id, "add") == 0 || strcmp(id, "sub") == 0 || strcmp(id, "mul") == 0 || strcmp(id, "div") == 0 || strcmp(id, "mod") == 0 || strcmp(id, "shl") == 0 || strcmp(id, "or") == 0 || strcmp(id, "xor") == 0 || strcmp(id, "not") == 0 || strcmp(id, "neg") == 0) {
        Op op = parse_math(prs, id);
        free(id);
        return op;
    } else if (strcmp(id, "bra") == 0 || strcmp(id, "brz") == 0 || strcmp(id, "brp") == 0 || strcmp(id, "brn") == 0) {
        Op op = parse_br(prs, id);
        free(id);
        return op;
    } else if (strcmp(id, "rdc") == 0 || strcmp(id, "rdi") == 0) {
        Op op = parse_rd(prs, id);
        free(id);
        return op;
    } else if (strcmp(id, "ref") == 0) {
        free(id);
        return parse_ref(prs);
    } else if (strcmp(id, "ldd") == 0) {
        free(id);
        return parse_ldd(prs);
    } else if (strcmp(id, "std") == 0) {
        free(id);
        return parse_std(prs);
    }

    if (prs->flags & IN_FIRST_PASS) {
        // Only show this error in the first pass because
        // we don't want it to show twice.
        fprintf(stderr, "%s:%zu:%zu: error: undefined identifier '%s'\n", prs->file, ln, col, id);
        inc_errors();
    }

    free(id);
    return NOOP;
}

Op parse_stmt(Parser *prs) {
    while (prs->tok->type == TOK_EOL)
        eat(prs, TOK_EOL);

    switch (prs->tok->type) {
        case TOK_ID: return parse_id(prs);
        default: break;
    }

    fprintf(stderr, "%s:%zu:%zu: error: invalid statement '%s'\n", prs->file, prs->tok->ln, prs->tok->col, tokentype_to_string(prs->tok->type));
    inc_errors();
    eat(prs, prs->tok->type);
    return NOOP;
}

Root parse_root(char *file) {
    Parser prs = create_parser(file);

    // We only care about adding labels to the table
    // in the first pass, they only returns NOOPs,
    // so we can just discard the return values.
    while (prs.tok->type != TOK_EOF) {
        // TOFIX: We're literally parsing twice. I don't like this.
        // This will make larger programs compile even slower.
        // But we do it so that labels can be used before they're
        // defined and and know their position in memory.
        // It'll do for now.
        // Fuck.
        parse_stmt(&prs);
        op_count++;
    }

    // Now the second pass.
    prs.tok = &prs.tokens[0];
    prs.pos = 0;
    prs.flags = 0;

    size_t op_capacity = STARTING_ROOT_CAP;
    Op *ops = malloc(STARTING_ROOT_CAP * sizeof(Op));
    op_count = 0;

    while (prs.tok->type != TOK_EOF) {
        Op stmt = parse_stmt(&prs);

        if (op_count + 1 >= op_capacity) {
            op_capacity *= 2;
            ops = realloc(ops, op_capacity * sizeof(Op));
        }

        ops[op_count++] = stmt;
    }

    delete_parser(&prs);
    delete_labels();
    return (Root){ .ops = ops, .op_count = op_count };
}

void delete_root(Root *root) {
    free(root->ops);
}