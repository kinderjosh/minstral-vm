#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "vm.h"

void disassemble_op(char *buffer, Opcode opcode, i64 operand);
int disassemble(char *infile, char *outfile);

#endif