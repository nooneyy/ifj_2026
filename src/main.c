/**
 * @file main.c
 * @brief compiler's main
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include "return.h"
#include "scanner.h"
#include "semantic.h"
#include <stdlib.h>

int main(void) {
  Scanner sc = scanner_init(stdin);
  ASTNode *program = NULL;

  ReturnCodes rc = parse_input(&sc, &program);
  if (rc != RETURN_SUCCESS) {
    return rc;
  }

  rc = semantic_analysis(program);
  if (rc != RETURN_SUCCESS) {
    ast_free(program);
    return rc;
  }

  rc = generate_code(program, stdout);

  ast_free(program);
  return rc;
}
