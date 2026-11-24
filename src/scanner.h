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
 * @brief Maximum number of indentation levels
 * no matter the space between them
 */
#define MAX_INDENT_LEVELS 64

/**
 * @brief Scanner's own state, surviving between calls
 */
typedef struct Scanner {
  FILE *src;                            // Source file scanner reads from
  int indent_levels[MAX_INDENT_LEVELS]; // Buffer of levels used for indentation
                                        // for example: [0, 2, 5, 9] - [0] is
                                        // always 0
  int indent_depth;    // Index of the deepest indent in indent_levels
  int dedents_pending; // Number of dedents still to be returned
  int parenth_depth;   // Current parenthesis depth
  bool at_line_start;  // A flag to determine, whether the scanner is
                       // on a new line and should mind the indentation
  bool emit_eol_eof; // A flag that determines whether to emit an EOL before an
                     // EOF
  bool emit_dot; // A flag that determines whether to emit a DOT after it was
                 // consumed to look ahead
  unsigned int line; // Line the scanner is currently on
} Scanner;

/**
 * @brief Initializing scanner information
 *
 * Initializing Scanner struct's information and state.
 * Scanner.src gets set to src_file from argument.
 * Scanner.at_line_start is set to 1.
 * Everything else is set to 0s.
 *
 * @param src_file Source file the scanner reads from
 * @return An initialized Scanner
 */
Scanner scanner_init(FILE *src_file);

/**
 * @brief Main scanner function, reads the next token
 * @param sc Scanner struct of information about the scanner's state
 * @param token Token to fill by the scanner
 * @return RETURN_SUCCESS in case of a success
 *         ERR_LEX_ANA in case of a lexical analysis error
 *         ERR_INTERNAL in case of an internal error
 */
ReturnCodes scanner_get_token(Scanner *sc, Token *token);

#endif
