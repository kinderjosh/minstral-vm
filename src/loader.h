#ifndef LOADER_H
#define LOADER_H

#include "vm.h"
#include <stdbool.h>

void load_file(VM *vm, char *filename, bool is_binary);

#endif