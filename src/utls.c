#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static size_t errors = 0;

char *mystrdup(char *str) {
    char *dup = malloc(strlen(str) + 1);
    strcpy(dup, str);
    return dup;
}

void inc_errors() {
    errors++;
}

size_t error_count() {
    return errors;
}
 
static void reverse(char *bin, int left, int right) {
    while (left < right) {
        char temp = bin[left];
        bin[left] = bin[right];
        bin[right] = temp;
        left++;
        right--;
    }
}

// function to convert decimal to binary
void int_to_bin(int64_t n, char *bin) {
    if (n == 0) {
        strcpy(bin, "0");
        return;
    }

    int index = 0;
  
    while (n > 0) {
        int bit = n % 2;
        bin[index++] = '0' + bit;
        n /= 2;
    }

    bin[index] = '\0';
	reverse(bin, 0, index - 1);
}