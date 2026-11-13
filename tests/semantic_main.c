/**
 * @file semantic_main.c
 * @brief Testing main for the semantic analysis, returns its return code
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 13.11.2026
 */

/**
 * Disclaimer!
 * This testing file was made by Claude Opus 5.5 AI model
 * This file is only for testing purposes.
 *
 * Prompt:
 * "Ok you can start!"
 * (context: agreed design - a testing main like parser_main that parses
 * argv[1] with parse_input, runs semantic_analysis on the AST, prints nothing
 * on stdout and returns the code of the first error)
 */

#include "../src/ast.h"
#include "../src/parser.h"
#include "../src/return.h"
#include "../src/scanner.h"
#include "../src/semantic.h"
#include <stdio.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    (void)fprintf(stderr, "Usage: %s <source file>\n", argv[0]);
    return ERR_INTERNAL;
  }

  FILE *src = fopen(argv[1], "r");
  if (src == NULL) {
    (void)fprintf(stderr, "Couldn't open source file.\n");
    return ERR_INTERNAL;
  }

  Scanner sc = scanner_init(src);
  ASTNode *program = NULL;

  // on an error the parser frees the AST itself, only its code goes up
  ReturnCodes rc = parse_input(&sc, &program);
  if (rc == RETURN_SUCCESS) {
    rc = semantic_analysis(program);
    // the semantic analysis never frees the AST, not even on an error
    ast_free(program);
  }

  (void)fclose(src);
  return rc;
}
