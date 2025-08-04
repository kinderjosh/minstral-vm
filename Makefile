CC = gcc
EXEC = minstral
SRCS = $(wildcard src/*.c)

DEBUG ?= 0
CFLAGS = -Wall -Wextra -Wpedantic -Wno-unusued-results -Wno-missing-braces -std=c11 -march=native

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

install: all
	cp ./$(EXEC) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(EXEC)
	rm -rf /usr/local/share/$(EXEC)
