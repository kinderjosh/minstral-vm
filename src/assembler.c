#include "assembler.h"
#include "parser.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

int assemble(char *infile, char *outfile, bool linebreak_after_ops, bool as_decimal) {
    Root root = parse_root(infile);

    if (error_count() > 0) {
        delete_root(&root);
        return EXIT_FAILURE;
    }

    FILE *f = fopen(outfile, "w");

    if (f == NULL) {
        fprintf(stderr, "error: failed to write to file '%s'\n", outfile);
        delete_root(&root);
        return EXIT_FAILURE;
    }

    char buffer[131];
    char *code = malloc(root.op_count * 128 + 1);
    code[0] = '\0';

    char opcode_binary[65];
    char operand_binary[65];

    for (size_t i = 0; i < root.op_count; i++) {
        if (as_decimal)
            sprintf(buffer, "%u %" PRId64, root.ops[i].opcode, root.ops[i].operand);
        else {
            int_to_bin(root.ops[i].opcode, opcode_binary);
            int_to_bin(root.ops[i].operand, operand_binary);
            sprintf(buffer, "%s %s", opcode_binary, operand_binary);
        }

        strcat(code, buffer);

        if (linebreak_after_ops)
            strcat(code, "\n");
        else
            strcat(code, " ");
    }

    code[strlen(code) - 1] = '\0'; // Remove the last space or line break.
    delete_root(&root);

    fputs(code, f);
    fclose(f);
    free(code);
    return EXIT_SUCCESS;
}