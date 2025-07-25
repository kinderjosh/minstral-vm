#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdbool.h>

// 1024 available slots, * 8 due to being int64_t,
// * 2 due to 2 separate memories, so really it's 16KiB.
#define MEMORY_CAP 1024

typedef enum {
    NOP,
    HLT,
    LDI,
    LDM,
    STM,
    PRCI,
    PRCM,
    PRCA,
    PRII,
    PRIM,
    PRIA,
    ADDI,
    ADDM,
    SUBI,
    SUBM,
    MULI,
    MULM,
    DIVI,
    DIVM,
    MODI,
    MODM,
    SHLI,
    SHLM,
    SHRI,
    SHRM,
    ANDI,
    ANDM,
    ORI,
    ORM,
    XORI,
    XORM,
    NOT,
    NEG,
    BRA,
    BRZ,
    BRP,
    BRN,
    RDCA,
    RDCM,
    RDIA,
    RDIM
} Opcode;

typedef int64_t i64;
typedef uint64_t u64;

typedef struct {
    i64 acc;
    u64 pc;
    u64 mar;
    Opcode cir;
    i64 mdr;
    Opcode instructions[MEMORY_CAP];
    i64 data[MEMORY_CAP];
    u64 op_count;
    bool running;
} VM;

VM *create_vm();
void delete_vm(VM *vm);
void start_vm(VM *vm);
void push_op(VM *vm, Opcode opcode, i64 operand);
__attribute__((noreturn)) void kill(VM *vm);
char *opcode_to_string(Opcode opcode);

#endif