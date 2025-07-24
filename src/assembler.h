#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdbool.h>

int assemble(char *infile, char *outfile, bool linebreak_after_ops, bool as_decimal);

#endif