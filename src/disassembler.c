#include "disassembler.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>

#define BUFFER_CAP 65

void disassemble_op(char *buffer, Opcode opcode, i64 operand) {
    strcpy(buffer, opcode_to_string(opcode));

    // Not all instructions have operands.
    switch (opcode) {
        case NOP:
        case HLT:
        case PRCA:
        case PRIA:
        case NOT:
        case NEG:
        case BRAA:
        case DRP: break;
        default: {
            strcat(buffer, " ");

            char operand_buffer[BUFFER_CAP];

            // Add [] to indicate a memory access.
            switch (opcode) {
                case LDM:
                case STM:
                case PRCM:
                case PRIM:
                case ADDM:
                case SUBM:
                case MULM:
                case DIVM:
                case MODM:
                case SHLM:
                case SHRM:
                case ANDM:
                case ORM:
                case XORM:
                case RDCM:
                case RDIM:
                case REFM:
                case LDDM:
                case STDM:
                case CMPM:
                case INCM:
                case DECM:
                case PSHM:
                case POPM:
                    sprintf(operand_buffer, "[%" PRId64 "]", operand);
                    break;
                default:
                    sprintf(operand_buffer, "%" PRId64, operand);
                    break;
            }

            strcat(buffer, operand_buffer);
            break;
        }
    }
}

int disassemble(char *infile, char *outfile) {
    FILE *f = fopen(infile, "r");

    if (f == NULL) {
        fprintf(stderr, "%s: error: failed to open file\n", infile);
        return EXIT_FAILURE;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    char *src = malloc(file_size + 1);
    size_t read_size = fread(src, 1, file_size, f);
    fclose(f);

    if (file_size != read_size) {
        fprintf(stderr, "%s: error: failed to read file\n", infile);
        free(src);
        return EXIT_FAILURE;
    }

    src[read_size] = '\0';

    f = fopen(outfile, "w");

    if (f == NULL) {
        fprintf(stderr, "%s: error: failed to open file '%s'\n", infile, outfile);
        free(src);
        return EXIT_FAILURE;
    }

    char buffer[BUFFER_CAP];
    size_t buffer_size = 0;
    const size_t len = strlen(src);
    bool is_binary = true;
    i64 opcode;
    i64 operand;
    int mode = 2;
    size_t i = 0;

    while (i < len) {
        if (buffer_size == BUFFER_CAP) {
            fprintf(stderr, "disassembler: error: constant exceeds maximum size of %u\n", BUFFER_CAP);
            free(src);
            fclose(f);
            return EXIT_FAILURE;
        }

        if (is_binary && isdigit(src[i]) && src[i] != '0' && src[i] != '1')
            is_binary = false;

        buffer[buffer_size++] = src[i++];

        if (i != len && !isspace(src[i]))
            continue;

        while (i != len && isspace(src[i]))
            i++;

        if (mode > 0) {
            buffer[buffer_size] = '\0';
            char *endptr;
            errno = 0;

            if (mode == 2)
                opcode = strtoll(buffer, &endptr, is_binary ? 2 : 10);
            else
                operand = strtoll(buffer, &endptr, is_binary ? 2 : 10);

            if (endptr == buffer || *endptr != '\0') {
                fprintf(stderr, "disassembler: error: constant conversion failed\n");
                free(src);
                fclose(f);
                return EXIT_FAILURE;
            } else if (errno == ERANGE || errno == EINVAL) {
                fprintf(stderr, "disassembler: error: constant conversion failed: %s\n", strerror(errno));
                free(src);
                fclose(f);
                return EXIT_FAILURE;
            }

            mode--;
            buffer_size = 0;
        }

        if (mode == 0) {
            char op_buffer[BUFFER_CAP * 2 + 2];
            disassemble_op(op_buffer, opcode, operand);
            fputs(op_buffer, f);

            if (i != len)
                fputc('\n', f);

            mode = 2;
        }
    }

    free(src);
    fclose(f);
    return EXIT_SUCCESS;
}