#include "vm.h"
#include "loader.h"
#include "assembler.h"
#include "disassembler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void help(char *prog) {
    printf("usage: %s <command> [options] <input file>\n"
           "commands:\n"
           "    asm               assemble a machine code file\n"
           "    dis               disassemble a machine code file\n"
           "    exe               execute a machine code file\n"
           "    run               assemble a machine code file\n"
           "options:\n"
           "    -decimal          output decimal machine code\n"
           "    -linebreak        output linebreaks in machine code\n"
           "    -o <output file>  specify the output filename\n"
           , prog);
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        help(argv[0]);
        return EXIT_SUCCESS;
    }

    const char *command = argv[1];
    bool dis = false;
    bool exe = false;
    bool run = false;

    if (strcmp(command, "exe") == 0)
        exe = true;
    else if (strcmp(command, "run") == 0)
        run = true;
    else if (strcmp(command, "dis") == 0)
        dis = true;
    else if (strcmp(command, "asm") != 0) {
        fprintf(stderr, "error: no such command '%s'\n", command);
        return EXIT_FAILURE;
    }

    char *infile = NULL;
    char *outfile = "a.out";
    bool decimal = false;
    bool linebreak = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-decimal") == 0)
            decimal = true;
        else if (strcmp(argv[i], "-linebreak") == 0)
            linebreak = true;
        else if (strcmp(argv[i], "-o") == 0) {
            if (i == argc - 1) {
                fprintf(stderr, "error: missing output filename for option '-o'\n");
                return EXIT_FAILURE;
            } else if (exe) {
                fprintf(stderr, "error: invalid option '-o' used with command 'exe'\n");
                return EXIT_FAILURE;
            }

            outfile = argv[++i];
        } else if (i == argc - 1)
            infile = argv[i];
        else {
            fprintf(stderr, "error: undefined option '%s'\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (infile == NULL) {
        fprintf(stderr, "error: missing input file\n");
        return EXIT_FAILURE;
    }

    if (dis) {
        // a.dis.sm probably doesn't already exist to overwrite.
        if (strcmp(outfile, "a.out") == 0)
            outfile = "dis.min";

        return disassemble(infile, outfile);
    } else if (!exe) {
        if (run) {
            int status = assemble(infile, outfile, linebreak, decimal);
            
            if (status == EXIT_FAILURE)
                return status;

            infile = outfile;

            if (dis)
                return status;
        } else
            return assemble(infile, outfile, linebreak, decimal);
    }

    VM *vm = create_vm();
    load_file(vm, infile, true);
    start_vm(vm);
    delete_vm(vm);
    return EXIT_SUCCESS;
}