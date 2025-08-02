#include "parser.h"
#include "token.h"
#include "vm.h"
#include "lexer.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>

#define OP(code, and) (Op){ .opcode = code, .operand = and }
#define NOOP (Op){ .opcode = NOP, .operand = 0 }

#define STARTING_TOK_CAP 32
#define STARTING_ROOT_CAP 16
#define TABLE_SIZE 10000

// Give each unresolved label a unique number so they
// can be resolved to their correct locations later.
#define UNRESOLVED_LABEL_LOCATION (-(i64)label_count - 98473492432239434) // Magic number from my ass.

typedef struct {
    char *name;
    i64 value;
    i64 resolved_value; // After we know where in memory the label is.
    bool resolved;
    bool used;
    bool is_subroutine;
    char *file;
    size_t ln;
    size_t col;
} Label;

static Label labels[TABLE_SIZE];
static size_t label_count = 0;

static bool text_initialized = false;
static bool data_initialized = false;
static bool in_text = false;
static i64 subroutine_ret_address = 0;

static Root root;

void root_push(Op stmt) {
    if (root.op_count + 1 >= root.op_capacity) {
        root.op_capacity *= 2;
        root.ops = realloc(root.ops, root.op_capacity * sizeof(Op));
    }

    root.ops[root.op_count++] = stmt;
}

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

Label *add_label(char *name, i64 value, char *file, size_t ln, size_t col) {
    Label *label = find_label(name);

    if (label->used) {
        fprintf(stderr, "%s:%d: TABLE COLLISION\n", __FILE__, __LINE__);
        inc_errors();
        assert(false);
    }

    label->name = name;
    label->value = value;
    label->resolved = false;
    label->used = true;
    label->file = file;
    label->ln = ln;
    label->col = col;
    label_count++;
    return label;
}

void delete_labels() {
    for (size_t i = 0; i < label_count; i++) {
        Label *label = &labels[i];

        if (!label->used)
            continue;

        free(label->name);
        free(label->file);
        label->used = false;
    }
}

Parser create_parser(char *file) {
    Lexer lex = create_lexer(file);
    Token tok;

    Token *tokens = malloc(STARTING_TOK_CAP * sizeof(Token));
    size_t token_count = 0;
    size_t token_capacity = STARTING_TOK_CAP;

    while ((tok = lex_next_token(&lex)).type != TOK_EOF) {
        // +2, extra +1 for the EOF.
        if (token_count + 2 >= token_capacity) {
            token_capacity *= 2;
            tokens = realloc(tokens, token_capacity * sizeof(Token));
        }

        tokens[token_count++] = tok;
    }

    tokens[token_count++] = tok;
    delete_lexer(&lex);

    return (Parser){ .file = file, .tokens = tokens, .token_count = token_count, .tok = &tokens[0], .pos = 0 };
}

void delete_parser(Parser *prs) {
    for (size_t i = 0; i < prs->token_count; i++)
        delete_token(&prs->tokens[i]);

    free(prs->tokens);
}

static void eat(Parser *prs, TokenType type) {
    if (type != prs->tok->type) {
        fprintf(stderr, "%s:%zu:%zu: error: found token '%s' when expecting '%s'\n", prs->file, prs->tok->ln, prs->tok->col, tokentype_to_string(prs->tok->type), tokentype_to_string(type));
        inc_errors();
    }

    if (prs->tok->type != TOK_EOF)
        prs->tok = &prs->tokens[++prs->pos];
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

Op parse_label_decl(Parser *prs, char *id, size_t ln, size_t col) {
    // Data label outside of the data section.
    if (prs->tok->type != TOK_EOL && strcmp(prs->tok->value, "dsr") != 0 && !data_initialized) {
        fprintf(stderr, "%s:%zu:%zu: error: defining data label '%s' outside of the data section\n", prs->file, ln, col, id);
        free(id);
        return NOOP;
    } 
    // Branch label outside of the text section.
    else if (prs->tok->type == TOK_EOL && !text_initialized) {
        fprintf(stderr, "%s:%zu:%zu: error: defining branch label '%s' outside of the text section\n", prs->file, ln, col, id);
        free(id);
        return NOOP;
    }
    // Any label, no sections found.
    else if (!text_initialized && !data_initialized) {
        fprintf(stderr, "%s:%zu:%zu: error: defining label '%s' outside of a section\n", prs->file, ln, col, id);
        free(id);
        return NOOP;
    }

    Label *label = find_label(id);

    // Check if this label already exists.
    if (label->used) {
        if (label->resolved) {
            fprintf(stderr, "%s:%zu:%zu: error: redefinition of label '%s'; first defined at %s:%zu:%zu\n", prs->file, ln, col, id, label->file, label->ln, label->col);
            inc_errors();
        } else {
            label->resolved = true;
            label->resolved_value = root.op_count;
        }

        if (strcmp(prs->tok->value, "dsr") == 0) {
            free(id);
            eat(prs, TOK_ID);

            label->is_subroutine = true;
            subroutine_ret_address = label->resolved_value;
            root_push(NOOP);

            // Store the return address.
            return OP(STM, subroutine_ret_address);
        } else if (strcmp(prs->tok->value, "dat") != 0) {
            free(id);
            return NOOP;
        }

        eat(prs, TOK_ID);

        if (prs->tok->type != TOK_INT) {
            fprintf(stderr, "%s:%zu:%zu: error: expected constant data value for label '%s' but found '%s'\n", prs->file, ln, col, id, tokentype_to_string(prs->tok->type));
            inc_errors();
            add_label(id, 0, mystrdup(prs->file), ln, col);
            return NOOP;
        }

        free(id);
        return OP(DAT, parse_digit(prs));
    }

    // Parse a branch label, only allowed in the text section.
    if (in_text) {
        if (strcmp(prs->tok->value, "dsr") == 0) {
            eat(prs, TOK_ID);
            label = add_label(id, root.op_count, mystrdup(prs->file), ln, col);
            label->resolved = true;
            label->resolved_value = label->value;
            label->is_subroutine = true;
            return NOOP;
        }

        label = add_label(id, root.op_count, mystrdup(prs->file), ln, col);
        label->resolved = true;
        label->resolved_value = root.op_count;
        return NOOP;
    }

    if (prs->tok->type != TOK_ID) {
        fprintf(stderr, "%s:%zu:%zu: error: expected DAT following data label '%s' but found '%s'\n", prs->file, ln, col, id, tokentype_to_string(prs->tok->type));
        inc_errors();

        add_label(id, 0, mystrdup(prs->file), ln, col);
        return NOOP;
    } else if (strcmp(prs->tok->value, "dat") != 0) {
        fprintf(stderr, "%s:%zu:%zu: error: expected DAT following data label '%s' but found '%s'\n", prs->file, ln, col, id, prs->tok->value);
        inc_errors();

        add_label(id, 0, mystrdup(prs->file), ln, col);
        return NOOP;
    }

    eat(prs, TOK_ID);

    if (prs->tok->type != TOK_INT) {
        fprintf(stderr, "%s:%zu:%zu: error: expected constant data value for label '%s' but found '%s'\n", prs->file, ln, col, id, tokentype_to_string(prs->tok->type));
        inc_errors();
        add_label(id, 0, mystrdup(prs->file), ln, col);
        return NOOP;
    }

    add_label(id, UNRESOLVED_LABEL_LOCATION, mystrdup(prs->file), ln, col);
    return OP(DAT, parse_digit(prs));
}

void assert_instr_in_text(Parser *prs, char *instr, size_t ln, size_t col) {
    if (in_text)
        return;

    fprintf(stderr, "%s:%zu:%zu: instruction '%s' outside of the text section\n", prs->file, ln, col, instr);
    inc_errors();
}

i64 parse_label(Parser *prs) {
    Label *label = find_label(prs->tok->value);

    if (!label->used) {
        label = add_label(mystrdup(prs->tok->value), UNRESOLVED_LABEL_LOCATION, mystrdup(prs->file), prs->tok->ln, prs->tok->col);
        eat(prs, TOK_ID);
        return label->value;
    }

    eat(prs, TOK_ID);
    return label->resolved ? label->resolved_value : label->value;
}

i64 parse_operand(Parser *prs) {
    if (prs->tok->type == TOK_INT)
        return parse_digit(prs);
    else if (prs->tok->type == TOK_ID)
        return parse_label(prs);

    fprintf(stderr, "%s:%zu:%zu: error: invalid operand '%s'\n", prs->file, prs->tok->ln, prs->tok->col, tokentype_to_string(prs->tok->type));
    inc_errors();
    return 0;
}

Op parse_id(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;

    char *id = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);
    
    // Check for instructions.
    if (strcmp(id, "hlt") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(HLT, 0);
    } else if (strcmp(id, "lda") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(LDAS, 0);

        return OP(prs->tok->type == TOK_INT ? LDI : LDM, parse_operand(prs));
    } else if (strcmp(id, "sta") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(STAS, 0);

        return OP(STM, parse_label(prs));
    } else if (strcmp(id, "prc") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(PRCA, 0);
        else if (prs->tok->type == TOK_INT)
            return OP(PRCI, parse_digit(prs));
        else if (prs->tok->type == TOK_TOS)
            return OP(PRCS, 0);

        return OP(PRCM, parse_label(prs));
    } else if (strcmp(id, "pri") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(PRIA, 0);
        else if (prs->tok->type == TOK_INT)
            return OP(PRII, parse_digit(prs));
        else if (prs->tok->type == TOK_TOS)
            return OP(PRIS, 0);

        return OP(PRIM, parse_label(prs));
    } else if (strcmp(id, "add") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(ADDS, 0);

        return OP(prs->tok->type == TOK_INT ? ADDI : ADDM, parse_operand(prs));
    } else if (strcmp(id, "sub") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(SUBS, 0);

        return OP(prs->tok->type == TOK_INT ? SUBI : SUBM, parse_operand(prs));
    } else if (strcmp(id, "mul") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(MULS, 0);

        return OP(prs->tok->type == TOK_INT ? MULI : MULM, parse_operand(prs));
    } else if (strcmp(id, "div") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(DIVS, 0);

        return OP(prs->tok->type == TOK_INT ? DIVI : DIVM, parse_operand(prs));
    } else if (strcmp(id, "mod") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(MODS, 0);

        return OP(prs->tok->type == TOK_INT ? MODI : MODM, parse_operand(prs));
    } else if (strcmp(id, "shl") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(SHLS, 0);

        return OP(prs->tok->type == TOK_INT ? SHLI : SHLM, parse_operand(prs));
    } else if (strcmp(id, "shr") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(SHRS, 0);

        return OP(prs->tok->type == TOK_INT ? SHRI : SHRM, parse_operand(prs));
    } else if (strcmp(id, "and") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(ANDS, 0);

        return OP(prs->tok->type == TOK_INT ? ANDI : ANDM, parse_operand(prs));
    } else if (strcmp(id, "or") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(ORS, 0);

        return OP(prs->tok->type == TOK_INT ? ORI : ORM, parse_operand(prs));
    } else if (strcmp(id, "xor") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(XORS, 0);

        return OP(prs->tok->type == TOK_INT ? XORI : XORM, parse_operand(prs));
    } else if (strcmp(id, "not") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(NOT, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(NOTS, 0);

        return OP(NOTM, parse_label(prs));
    } else if (strcmp(id, "neg") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(NEG, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(NOTS, 0);

        return OP(NEGM, parse_label(prs));
    } else if (strcmp(id, "jmp") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(BRAA, 0);

        return OP(BRA, parse_operand(prs));
    } else if (strcmp(id, "brz") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BRZ, parse_operand(prs));
    } else if (strcmp(id, "brp") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BRP, parse_operand(prs));
    } else if (strcmp(id, "brn") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BRN, parse_operand(prs));
    } else if (strcmp(id, "rdc") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(RDCA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(RDCS, 0);

        return OP(RDCM, parse_label(prs));
    } else if (strcmp(id, "rdi") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(RDIA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(RDIS, 0);

        return OP(RDIM, parse_label(prs));
    } else if (strcmp(id, "ref") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(REFS, 0);

        return OP(REFM, parse_label(prs));
    } else if (strcmp(id, "ldd") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(LDDA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(LDDS, 0);

        return OP(LDDM, parse_label(prs));
    } else if (strcmp(id, "std") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(STDS, 0);

        return OP(STDM, parse_label(prs));
    } else if (strcmp(id, "cmp") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_TOS)
            return OP(CMPS, 0);

        return OP(prs->tok->type == TOK_INT ? CMPI : CMPM, parse_operand(prs));
    } else if (strcmp(id, "beq") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BEQ, parse_label(prs));
    } else if (strcmp(id, "bne") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BNE, parse_label(prs));
    } else if (strcmp(id, "blt") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BLT, parse_label(prs));
    } else if (strcmp(id, "ble") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BLE, parse_label(prs));
    } else if (strcmp(id, "bgt") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BGT, parse_label(prs));
    } else if (strcmp(id, "bge") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(BGE, parse_label(prs));
    } else if (strcmp(id, "csr") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        // We want to load the accumulator with the return address
        // so that the subroutine can immediately store it in the
        // NOP data where the label is defined.
        // This way, the subroutine knows where to branch back to
        // when it finds the RSR instruction.
        // The return address will be the instruction after CSR.
        // TOFIX: Will this break shit if there's no instruction after CSR?

        root_push(OP(LDI, root.op_count + 2));
        return OP(CSR, parse_label(prs));
    } else if (strcmp(id, "rsr") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        root_push(OP(LDM, subroutine_ret_address));
        return OP(BRAA, 0);
    } else if (strcmp(id, "inc") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(INCA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(INCS, 0);

        return OP(INCM, parse_label(prs));
    } else if (strcmp(id, "dec") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(DECA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(DECS, 0);

        return OP(DECM, parse_label(prs));
    } else if (strcmp(id, "psh") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(PSHA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(PSHS, 0);

        return OP(prs->tok->type == TOK_INT ? PSHI : PSHM, parse_operand(prs));
    } else if (strcmp(id, "pop") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(POPA, 0);

        return OP(POPM, parse_label(prs));
    } else if (strcmp(id, "drp") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);
        return OP(DRP, 0);
    } else if (strcmp(id, "swp") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SWPS, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SWPS, 0);

        return OP(SWPM, parse_label(prs));
    } else if (strcmp(id, "sez") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SEZA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SEZS, 0);

        return OP(SEZM, parse_label(prs));
    } else if (strcmp(id, "sep") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SEPA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SEPS, 0);

        return OP(SEPM, parse_label(prs));
    } else if (strcmp(id, "sen") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SENA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SENS, 0);

        return OP(SENM, parse_label(prs));
    } else if (strcmp(id, "seq") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SEQA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SEQS, 0);

        return OP(SEQM, parse_label(prs));
    } else if (strcmp(id, "sne") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SNEA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SNES, 0);

        return OP(SNEM, parse_label(prs));
    } else if (strcmp(id, "slt") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SLTA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SLTS, 0);

        return OP(SLTM, parse_label(prs));
    } else if (strcmp(id, "sle") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SLEA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SLES, 0);

        return OP(SLEM, parse_label(prs));
    } else if (strcmp(id, "sgt") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SGTA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SGTS, 0);

        return OP(SGTM, parse_label(prs));
    } else if (strcmp(id, "sge") == 0) {
        assert_instr_in_text(prs, id, ln, col);
        free(id);

        if (prs->tok->type == TOK_EOL || prs->tok->type == TOK_EOF)
            return OP(SGEA, 0);
        else if (prs->tok->type == TOK_TOS)
            return OP(SGES, 0);

        return OP(SGEM, parse_label(prs));
    }

    // Assume any non-instruction and data identifier
    // as a label.
    return parse_label_decl(prs, id, ln, col);
}

Op parse_section_header(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_DOT);

    // TODO: Should we allow for sections to be redefined?

    if (strcmp(prs->tok->value, "text") == 0) {
        if (text_initialized) {
            fprintf(stderr, "%s:%zu:%zu: error: redefinition of text section\n", prs->file, ln, col);
            inc_errors();
        } else
            text_initialized = true;

        in_text = true;
        eat(prs, TOK_ID);
        return parse_stmt(prs);
    } else if (strcmp(prs->tok->value, "data") == 0) {
        if (data_initialized) {
            fprintf(stderr, "%s:%zu:%zu: error: redefinition of data section\n", prs->file, ln, col);
            inc_errors();
        } else
            data_initialized = true;

        in_text = false;
        eat(prs, TOK_ID);
        return parse_stmt(prs);
    }

    fprintf(stderr, "%s:%zu:%zu: error: invalid section '%s'\n", prs->file, ln, col, prs->tok->value);
    inc_errors();
    eat(prs, TOK_ID);
    return parse_stmt(prs);
}

Op parse_stmt(Parser *prs) {
    while (prs->tok->type == TOK_EOL || prs->tok->type == TOK_TOS)
        eat(prs, prs->tok->type);

    switch (prs->tok->type) {
        case TOK_ID: return parse_id(prs);
        case TOK_DOT: return parse_section_header(prs);
        case TOK_EOF: return NOOP;
        default: break;
    }

    fprintf(stderr, "%s:%zu:%zu: error: invalid statement '%s'\n", prs->file, prs->tok->ln, prs->tok->col, tokentype_to_string(prs->tok->type));
    inc_errors();
    eat(prs, prs->tok->type);
    return NOOP;
}

void resolve_and_delete_labels() {
    // EWWWWWWW gross!!
    for (size_t i = 0; i < TABLE_SIZE && label_count > 0; i++) {
        Label *label = &labels[i];

        if (!label->used)
            continue;

        // Check if the unresolved value of this label
        // is used anywhere in the program.
        for (size_t j = 0; j < root.op_count; j++) {
            Op *op = &root.ops[j];

            if (op->operand == label->value) {
                if (!label->resolved) {
                    fprintf(stderr, "%s:%zu:%zu: error: undefined label '%s'\n", label->file, label->ln, label->col, label->name);
                    inc_errors();
                    break;
                } else if (op->opcode == CSR && !label->is_subroutine) {
                    fprintf(stderr, "%s:%zu:%zu: error: calling non-subroutine '%s'\n", label->file, label->ln, label->col, label->name);
                    inc_errors();
                    break;
                }

                op->operand = label->resolved_value;
            }
        }

        free(label->name);
        free(label->file);
        label_count--;
    }
}

Root parse_root(char *file) {
    // Just in case we're compiling multiple times and
    // global variables are messed up.
    text_initialized = data_initialized = in_text = false;
    label_count = 0;

    Parser prs = create_parser(file);
    root = (Root){ .ops = malloc(STARTING_ROOT_CAP * sizeof(Op)), .op_count = 0, .op_capacity = STARTING_ROOT_CAP };

    while (prs.tok->type != TOK_EOF)
        root_push(parse_stmt(&prs));

    resolve_and_delete_labels();
    delete_parser(&prs);

    if (root.op_count == 0)
        // Just don't do anything.
        root_push(OP(HLT, 0));

    return root;
}

void delete_root(Root *root) {
    free(root->ops);
}