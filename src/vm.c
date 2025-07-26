#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

VM *create_vm() {
    VM *vm = malloc(sizeof(VM));
    vm->acc = vm->pc = vm->mar = vm->cir = vm->mdr = vm->op_count = 0;

    memset(vm->instructions, NOP, MEMORY_CAP - 1);
    memset(vm->data, NOP, MEMORY_CAP - 1);

    vm->running = false;
    return vm;
}

void delete_vm(VM *vm) {
    free(vm);
}

__attribute__((noreturn)) void kill(VM *vm) {
    fprintf(stderr, "aborting...\n");
    delete_vm(vm);
    exit(EXIT_FAILURE);
}

static void fetch(VM *vm) {
    if ((size_t)vm->pc >= MEMORY_CAP) {
        fprintf(stderr, "vm: error: reached end of memory\n");
        kill(vm);
    }

    vm->mar = vm->pc++;
}

static void decode(VM *vm) {
    vm->cir = vm->instructions[vm->mar];
    vm->mdr = vm->data[vm->mar];
}

static void execute(VM *vm) {
    switch (vm->cir) {
        case NOP: break;
        case HLT:
            vm->running = false;
            break;
        case LDI:
            vm->acc = vm->mdr;
            break;
        case LDM:
            vm->acc = vm->data[vm->mdr];
            break;
        case STM:
            vm->data[vm->mdr] = vm->acc;
            break;
        case PRCI:
            fputc((char)vm->mdr, stdout);
            break;
        case PRCM:
            fputc((char)vm->data[vm->mdr], stdout);
            break;
        case PRCA:
            fputc((char)vm->acc, stdout);
            break;
        case PRII:
            fprintf(stdout, "%" PRId64, vm->mdr);
            break;
        case PRIM:
            fprintf(stdout, "%" PRId64, vm->data[vm->mdr]);
            break;
        case PRIA:
            fprintf(stdout, "%" PRId64, vm->acc);
            break;
        case ADDI:
            vm->acc += vm->mdr;
            break;
        case ADDM:
            vm->acc += vm->data[vm->mdr];
            break;
        case SUBI:
            vm->acc -= vm->mdr;
            break;
        case SUBM:
            vm->acc -= vm->data[vm->mdr];
            break;
        case MULI:
            vm->acc *= vm->mdr;
            break;
        case MULM:
            vm->acc *= vm->data[vm->mdr];
            break;
        case DIVI:
            vm->acc /= vm->mdr;
            break;
        case DIVM:
            vm->acc /= vm->data[vm->mdr];
            break;
        case MODI:
            vm->acc %= vm->mdr;
            break;
        case MODM:
            vm->acc %= vm->data[vm->mdr];
            break;
        case SHLI:
            vm->acc <<= vm->mdr;
            break;
        case SHLM:
            vm->acc <<= vm->data[vm->mdr];
            break;
        case SHRI:
            vm->acc >>= vm->mdr;
            break;
        case SHRM:
            vm->acc >>= vm->data[vm->mdr];
            break;
        case ANDI:
            vm->acc &= vm->mdr;
            break;
        case ANDM:
            vm->acc &= vm->data[vm->mdr];
            break;
        case ORI:
            vm->acc |= vm->mdr;
            break;
        case ORM:
            vm->acc |= vm->data[vm->mdr];
            break;
        case XORI:
            vm->acc ^= vm->mdr;
            break;
        case XORM:
            vm->acc ^= vm->data[vm->mdr];
            break;
        case NOT:
            vm->acc = !vm->acc;
            break;
        case NEG:
            vm->acc = -vm->acc;
            break;
        case BRA:
            vm->pc = vm->mdr;
            break;
        case BRAA:
            vm->pc = vm->acc;
            break;
        case BRZ:
            if (vm->acc == 0)
                vm->pc = vm->mdr;
            break;
        case BRP:
            if (vm->acc >= 0)
                vm->pc = vm->mdr;
            break;
        case BRN:
            if (vm->acc < 0)
                vm->pc = vm->mdr;
            break;
        case RDCA: {
            char buffer[4];
            fgets(buffer, 3, stdin);
            buffer[strlen(buffer) - 1] = '\0'; // Remove newline.
            vm->acc = buffer[0];
            break;
        }
        case RDCM: {
            char buffer[4];
            fgets(buffer, 3, stdin);
            buffer[strlen(buffer) - 1] = '\0'; // Remove newline.
            vm->data[vm->mdr] = buffer[0];
            break;
        }
        case RDIA: {
            char buffer[32];
            fgets(buffer, 31, stdin);
            buffer[strlen(buffer) - 1] = '\0'; // Remove newline.

            vm->acc = atoi(buffer);
            break;
        }
        case RDIM: {
            char buffer[32];
            fgets(buffer, 31, stdin);
            buffer[strlen(buffer) - 1] = '\0'; // Remove newline.

            vm->data[vm->mdr] = atoi(buffer);
            break;
        }
        case REFM:
            vm->acc = vm->mdr;
            break;
        case LDDA:
            vm->acc = vm->data[vm->acc];
            break;
        case LDDM:
            vm->acc = vm->data[vm->data[vm->mdr]];
            break;
        case STDM:
            vm->data[vm->data[vm->mdr]] = vm->acc;
            break;
        default:
            fprintf(stderr, "vm: error: undefined instruction %" PRIu64 "\n", (u64)vm->cir);
            kill(vm);
    }
}

void start_vm(VM *vm) {
    vm->running = true;

    while (vm->running)
        cycle_vm(vm);
}

void cycle_vm(VM *vm) {
    fetch(vm);
    decode(vm);
    execute(vm);
}

void push_op(VM *vm, Opcode opcode, i64 operand) {
    if (vm->op_count >= MEMORY_CAP) {
        fprintf(stderr, "memory overflow\n");
        kill(vm);
    }

    vm->instructions[vm->op_count] = opcode;
    vm->data[vm->op_count++] = operand;
}

char *opcode_to_string(Opcode opcode) {
    switch (opcode) {
        case NOP: return "nop";
        case HLT: return "hlt";
        case LDI:
        case LDM: return "lda";
        case STM: return "sta";
        case PRCI:
        case PRCM:
        case PRCA: return "prc";
        case PRII:
        case PRIM:
        case PRIA: return "pri";
        case ADDI:
        case ADDM: return "add";
        case SUBI:
        case SUBM: return "sub";
        case MULI:
        case MULM: return "mul";
        case DIVI:
        case DIVM: return "div";
        case MODI:
        case MODM: return "mod";
        case SHLI:
        case SHLM: return "shl";
        case SHRI:
        case SHRM: return "shr";
        case ANDI:
        case ANDM: return "and";
        case ORI:
        case ORM: return "or";
        case XORI:
        case XORM: return "xor";
        case NOT: return "not";
        case NEG: return "neg";
        case BRAA:
        case BRA: return "bra";
        case BRZ: return "brz";
        case BRP: return "brp";
        case BRN: return "brp";
        case RDCA:
        case RDCM: return "rdc";
        case RDIA:
        case RDIM: return "rdi";
        case REFM: return "ref";
        case LDDA:
        case LDDM: return "ldd";
        case STDM: return "std";
        default: break;
    }

    assert(false);
    return "undefined";
}