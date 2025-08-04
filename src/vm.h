#ifndef VM_H
#define VM_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// 1024 available slots, * 8 due to being int64_t,
// * 2 due to 2 separate memories, so really it's 16KiB.
#define MEMORY_CAP (size_t)1024

// 128 * 8 due to int64_t = 1024
#define STACK_CAP (size_t)128

typedef enum {
    NOP,
    HLT,
    LDI,
    LDM,
    LDAS,
    STM,
    STAS,
    PRCI,
    PRCM,
    PRCA,
    PRCS,
    PRII,
    PRIM,
    PRIA,
    PRIS,
    ADDI,
    ADDM,
    ADDS,
    SUBI,
    SUBM,
    SUBS,
    MULI,
    MULM,
    MULS,
    DIVI,
    DIVM,
    DIVS,
    MODI,
    MODM,
    MODS,
    SHLI,
    SHLM,
    SHLS,
    SHRI,
    SHRM,
    SHRS,
    ANDI,
    ANDM,
    ANDS,
    ORI,
    ORM,
    ORS,
    XORI,
    XORM,
    XORS,
    NOT,
    NOTM,
    NOTS,
    NEG,
    NEGM,
    NEGS,
    BRA,
    BRAA,
    BRZ,
    BRP,
    BRN,
    RDCA,
    RDCM,
    RDCS,
    RDIA,
    RDIM,
    RDIS,
    REFM,
    REFS,
    LDDA,
    LDDM,
    LDDS,
    STDM,
    STDS,
    DAT,
    CMPI,
    CMPM,
    CMPS,
    BEQ,
    BNE,
    BLT,
    BLE,
    BGT,
    BGE,
    CSR,
    INCA,
    INCM,
    INCS,
    DECA,
    DECM,
    DECS,
    PSHA,
    PSHI,
    PSHM,
    PSHS,
    POPA,
    POPM,
    DRP,
    SWPM,
    SWPS,
    SEZA,
    SEZM,
    SEZS,
    SEPA,
    SEPM,
    SEPS,
    SENA,
    SENM,
    SENS,
    SEQA,
    SEQM,
    SEQS,
    SNEA,
    SNEM,
    SNES,
    SLTA,
    SLTM,
    SLTS,
    SLEA,
    SLEM,
    SLES,
    SGTA,
    SGTM,
    SGTS,
    SGEA,
    SGEM,
    SGES,
    IPS
} Opcode;

typedef int64_t i64;
typedef uint64_t u64;

typedef struct {
    i64 acc;
    i64 pc;
    i64 mar;
    Opcode cir;
    i64 mdr;

    Opcode instructions[MEMORY_CAP];
    i64 data[MEMORY_CAP];
    u64 op_count;

    bool cf;
    bool zf;
    bool nf;

    i64 stack[STACK_CAP];
    i64 sp;

    bool running;
} VM;

typedef struct {
    Opcode opcode;
    i64 operand;
} Op;

VM *create_vm();
void delete_vm(VM *vm);
void start_vm(VM *vm);
void cycle_vm(VM *vm);
void push_op(VM *vm, Opcode opcode, i64 operand);
__attribute__((noreturn)) void kill(VM *vm);
char *opcode_to_string(Opcode opcode);

#endif