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
#include <stdio.h>

/**
 * MAX_INDENT_LEVELS - maximum number of indentation levels
 *                     no matter the space between them
 */
#define MAX_INDENT_LEVELS 64

/**
 * struct Scanner - Scanner's own state, surviving between calls
 * @src:             Source file scanner reads from
 * @indent_levels:   Buffer of levels used for indentation
 *                   for example: [0, 2, 5, 9] - [0] is always 0
 * @indent_depth:    Current indentation depth
 * @indents_pending: Number of dedents still to be returned
 * @parenth_depth:   Current parenthesis depth
 * @at_line_start:   A flag to determine, whether the scanner is
 *                   on a new line and should mind the indentation
 */
typedef struct Scanner {
  FILE *src;
  int indent_levels[MAX_INDENT_LEVELS];
  int indent_depth;
  int indents_pending;
  int parenth_depth;
  int at_line_start;
} Scanner;

/**
 * scanner() - main scanner function
 * @src_file: Source file to read from
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_LEX_ANA in case of a lexical analysis error
 *         Returns ERR_INTERNAL in case of an internal error
 */
ReturnCodes scanner(FILE *src_file, Token *token);

#endif