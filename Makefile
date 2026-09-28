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
$(TARGET): src/main.o
	$(CC) $(CFLAGS) -o $@ $^

##
# ADD NEW DEPENDENCIES BELOW - example for new symtable.c and symtable.h files:
# src/main.o: src/symtable.h
# src/symtable.o: src/symtable.h
##

# Auto-compile .c source files into .o object files
src/$.o: src/$.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleans the final executable and all object files
clean:
	rm -f $(TARGET) src/*.o

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
.PHONY: all clean format tidy 

# TODO: tidy and format commands, zip commands, test commands,....