/**
 * @file scanner.h
 * @brief scanner's header file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "return.h"
#include "tokens.h"
#include <stdbool.h>
#include <stdio.h>

/**
 * MAX_INDENT_LEVELS - maximum number of indentation levels
 *                     no matter the space between them
 */
#define MAX_INDENT_LEVELS 64

/**
 * struct Scanner - Scanner's own state, surviving between calls
 * @src:              Source file scanner reads from
 * @indent_levels:    Buffer of levels used for indentation
 *                    for example: [0, 2, 5, 9] - [0] is always 0
 * @indent_depth:     Index of the deepest indent in indent_levels
 * @dedents_pending:  Number of dedents still to be returned
 * @parenth_depth:    Current parenthesis depth
 * @at_line_start:    A flag to determine, whether the scanner is
 *                    on a new line and should mind the indentation
 */
typedef struct Scanner {
  FILE *src;
  int indent_levels[MAX_INDENT_LEVELS];
  int indent_depth;
  int dedents_pending;
  int parenth_depth;
  bool at_line_start;
} Scanner;

/**
 * scanner_init() - Initializing scanner information
 * @src_file: Source file the scanner reads from
 *
 * Initializing Scanner struct's information and state.
 * Scanner.src gets set to src_file from argument.
 * Scanner.at_line_start is set to 1.
 * Everything else is set to 0s.
 *
 * Return: Returns an initialized Scanner
 */
Scanner scanner_init(FILE *src_file);

/**
 * scanner() - main scanner function
 * @sc:    Scanner struct of information about the scanner's state
 * @token: Token to fill by the scanner
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_LEX_ANA in case of a lexical analysis error
 *         Returns ERR_INTERNAL in case of an internal error
 */
ReturnCodes scanner(Scanner *sc, Token *token);

#endif