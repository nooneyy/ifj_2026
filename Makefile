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
$(TARGET): src/main.o src/symbol.o src/symtable.o src/scanner.o src/dynstr.o src/tokens.o
	$(CC) $(CFLAGS) -o $@ $^

# Scanner's testing main - every module except src/main.o, it has its own main
SCANNER_TEST := tests/scanner_main

scanner_test: $(SCANNER_TEST)

$(SCANNER_TEST): tests/scanner_main.o src/scanner.o src/dynstr.o src/tokens.o
	$(CC) $(CFLAGS) -o $@ $^

##
# ADD NEW DEPENDENCIES BELOW - example for new symtable.c and symtable.h files:
# src/main.o: src/symtable.h
# src/symtable.o: src/symtable.h
##

src/main.o: src/return.h
src/symbol.o: src/symbol.h
src/symtable.o: src/symtable.h src/symbol.h

# Auto-compile .c source files into .o object files
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleans the final executable, testing executables and all object files
clean:
	rm -f $(TARGET) $(SCANNER_TEST) src/*.o tests/*.o

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
.PHONY: all clean format tidy scanner_test

# TODO: tidy and format commands, zip commands, test commands,....