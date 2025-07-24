#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>

char *mystrdup(char *str);
void inc_errors();
size_t error_count();
void int_to_bin(int64_t x, char *buffer);

#endif