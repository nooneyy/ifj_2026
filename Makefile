###
# @file Makefile
# @brief File describing make commands
# @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
# @date 28.09.2026
###

# Compiler
CC := clang

# Compiler flags
# C99 standard, as much warnings as possible, treat all warnings as errors, add debug info
CFLAGS := -std=c99 -Wall -Wextra -Werror -pedantic -g

# The name of our finished executable
TARGET := ifj26

all: $(TARGET)

# IMPORTANT - add all object files to be linked with final executable here with the .o extension 
$(TARGET): src/main.o src/symbol.o src/symtable.o src/scanner.o src/dynstr.o src/tokens.o src/return.o src/ast.o src/parser.o src/prec_stack.o src/semantic.o
	$(CC) $(CFLAGS) -o $@ $^

# Scanner's testing main - every module except src/main.o, it has its own main
SCANNER_TEST := tests/scanner_main

scanner_test: $(SCANNER_TEST)

$(SCANNER_TEST): tests/scanner_main.o src/scanner.o src/dynstr.o src/tokens.o src/return.o
	$(CC) $(CFLAGS) -o $@ $^

# Disclaimer! The parser_test and test targets were made by Claude Opus 5.5
# Prompt: "Since AI can be used freely to make tests, could you make them for
# me? Let's collaborate on the design, the test contents and then i'll let u
# implement it"

# Parser's testing main - prints the AST, has its own main
PARSER_TEST := tests/parser_main

parser_test: $(PARSER_TEST)

$(PARSER_TEST): tests/parser_main.o src/parser.o src/prec_stack.o src/ast.o src/symbol.o src/scanner.o src/dynstr.o src/tokens.o src/return.o
	$(CC) $(CFLAGS) -o $@ $^

# Disclaimer! The semantic_test target was made by Claude Opus 5.5
# Prompt: "Ok you can start!" (context: a tests/semantic/ suite like the
# parser tests, its main parses, runs the semantic analysis and returns the code)

# Semantic analysis' testing main - prints nothing, returns the code, has its
# own main
SEMANTIC_TEST := tests/semantic_main

semantic_test: $(SEMANTIC_TEST)

$(SEMANTIC_TEST): tests/semantic_main.o src/semantic.o src/parser.o src/prec_stack.o src/ast.o src/symbol.o src/symtable.o src/scanner.o src/dynstr.o src/tokens.o src/return.o
	$(CC) $(CFLAGS) -o $@ $^

# Runs every test suite (tests/<suite>/ with its tests/<suite>_main)
test:
	tests/run_tests.sh scanner parser semantic

##
# ADD NEW DEPENDENCIES BELOW - example for new symtable.c and symtable.h files:
# src/main.o: src/symtable.h
# src/symtable.o: src/symtable.h
##

src/main.o: src/return.h src/scanner.h src/tokens.h src/parser.h src/ast.h src/semantic.h
src/scanner.o: src/scanner.h src/return.h src/tokens.h src/dynstr.h
src/parser.o: src/parser.h src/ast.h src/tokens.h src/return.h src/scanner.h src/prec_stack.h src/symbol.h
src/semantic.o: src/semantic.h src/ast.h src/return.h src/symbol.h src/symtable.h
src/ast.o: src/ast.h src/symbol.h src/return.h
src/prec_stack.o: src/prec_stack.h src/ast.h src/return.h src/tokens.h
src/dynstr.o: src/dynstr.h src/return.h
src/tokens.o: src/tokens.h
src/symbol.o: src/symbol.h src/return.h
src/symtable.o: src/symtable.h src/symbol.h
src/return.o: src/return.h
tests/scanner_main.o: src/return.h src/scanner.h src/tokens.h
tests/parser_main.o: src/ast.h src/parser.h src/return.h src/scanner.h src/symbol.h src/tokens.h
tests/semantic_main.o: src/ast.h src/parser.h src/return.h src/scanner.h src/semantic.h src/symbol.h src/tokens.h

# Auto-compile .c source files into .o object files
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleans the final executable, testing executables and all object files
clean:
	rm -f $(TARGET) $(SCANNER_TEST) $(PARSER_TEST) $(SEMANTIC_TEST) src/*.o tests/*.o

# Format all source code files in the repo
# Wildcard is there for not throwing errors on non-existent header files
format:
	clang-format -i $(wildcard src/*.c src/*.h)

# Tidy all source code files in the repo
tidy: compile_commands.json
	clang-tidy --quiet src/*.c

# Recursively call make, then generate compile_commands from the build
compile_commands.json:
	bear -- $(MAKE) clean all

# Commands that are not build targets
.PHONY: all clean format tidy scanner_test parser_test semantic_test test

# TODO: tidy and format commands, zip commands, test commands,....
