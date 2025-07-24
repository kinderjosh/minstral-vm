#include "loader.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#define BUFFER_CAP 65

void load_file(VM *vm, char *filename, bool is_binary) {
    FILE *f = fopen(filename, "r");

    if (f == NULL) {
        fprintf(stderr, "loader: error: no such file '%s'\n", filename);
        kill(vm);
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    char *src = malloc(file_size + 1);
    size_t read_size = fread(src, 1, file_size, f);
    fclose(f);

    if (file_size != read_size) {
        fprintf(stderr, "loader: error: failed to read file '%s'\n", filename);
        kill(vm);
    }

    src[read_size] = '\0';

    char buffer[BUFFER_CAP];
    size_t buffer_size = 0;
    const size_t len = strlen(src);
    i64 opcode = 0;
    i64 operand = 0;
    int mode = 2;
    size_t i = 0;

    while (i < len) {
        if (buffer_size == BUFFER_CAP) {
            fprintf(stderr, "loader: error: constant exceeds maximum size of %u\n", BUFFER_CAP);
            free(src);
            kill(vm);
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
                fprintf(stderr, "loader: error: constant conversion failed\n");
                free(src);
                kill(vm);
            } else if (errno == ERANGE || errno == EINVAL) {
                fprintf(stderr, "loader: error: constant conversion failed: %s\n", strerror(errno));
                free(src);
                kill(vm);
            }

            mode--;
            buffer_size = 0;
        }

        if (mode == 0) {
            push_op(vm, opcode, operand);
            mode = 2;
        }
    }

    free(src);
}
