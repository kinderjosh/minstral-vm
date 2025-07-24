CC = gcc
EXEC = monovm
SRCS = $(wildcard src/*.c)

DEBUG ?= 0
CFLAGS = -Wall -Wextra -Wpedantic -Wno-missing-braces -std=c11 -march=native

ifeq ($(DEBUG),1)
CFLAGS += -g -Wl,-z,now -Wl,-z,relro \
	  -fsanitize=undefined,address \
	  -fstack-protector-strong \
	  -ftrampolines \
	  -ftrivial-auto-var-init=pattern
else
CFLAGS += -s -O3 -DNDEBUG
endif

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f ./$(EXEC)