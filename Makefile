CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
SOURCES = src/main.c src/ast.c src/semantic.c src/codegen.c generated/parser.c generated/lexer.c

.PHONY: all grammar test clean
all: bin/translator

bin/translator: $(SOURCES) src/ast.h generated/parser.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Isrc -Igenerated $(SOURCES) -lm -o $@

grammar:
	bison --warnings=all --warnings=error --defines=generated/parser.h -o generated/parser.c grammar/parser.y
	flex -o generated/lexer.c grammar/lexer.l

test: all
	python3 tests/run_tests.py --translator bin/translator

clean:
	rm -f bin/translator
