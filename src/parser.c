/**
 * @file parser.c
 * @brief Compiler's parser with LL analysis and precedence analysis
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 7.11.2026
 */

#include "parser.h"
#include "ast.h"
#include "return.h"
#include "scanner.h"
#include "tokens.h"
#include <stdlib.h>

/**
 * @brief Returns rc from the current function if it isn't RETURN_SUCCESS
 *
 * Expects a local variable named rc to exist in the calling function.
 */
#define CHECK_RC()                                                             \
  do {                                                                         \
    if (rc != RETURN_SUCCESS) {                                                \
      return rc;                                                               \
    }                                                                          \
  } while (0)

/**
 * @brief Parser's own state, surviving between parse functions
 */
typedef struct Parser {
  Scanner *scanner; // Scanner the parser asks for tokens (borrowed, not owned)
  Token current_token; // Token being processed
  Token next_token;    // One token of lookahead
  ASTNode *program;    // Root of the AST being built
  ReturnCodes rc;      // Parser's code to return
} Parser;

/**
 * @brief Moves to the next token, next becomes current and a new next is read
 *
 * In case of an error, sets the parser's return code
 *
 * @param parser Parser to advance
 */
static void parser_advance(Parser *parser) {
  token_free(&parser->current_token);
  parser->current_token = parser->next_token;
  // The string is now owned by current_token, prevents a double free
  parser->next_token = (Token){0};

  parser->rc = scanner_get_token(parser->scanner, &parser->current_token);
}

/**
 * @brief Reads the first two tokens and creates the program node
 *
 * @param parser  Zero-initialized parser to set up
 * @param scanner Scanner the parser asks for tokens
 * @return RETURN_SUCCESS in case of a success, error code otherwise
 */
static ReturnCodes parser_init(Parser *parser, Scanner *scanner) {
  ReturnCodes rc = RETURN_SUCCESS;

  parser->scanner = scanner;

  rc = scanner_get_token(parser->scanner, &parser->current_token);
  CHECK_RC();
  rc = scanner_get_token(parser->scanner, &parser->next_token);
  CHECK_RC();

  parser->program = ast_init_program();
  if (parser->program == NULL) {
    return ERR_INTERNAL;
  }
  return RETURN_SUCCESS;
}

/**
 * @brief Frees everything the parser owns, except the AST
 *
 * @param parser Parser to free
 */
static void parser_free(Parser *parser) {
  token_free(&parser->current_token);
  token_free(&parser->next_token);
}

ReturnCodes parse_program(Scanner *scanner, ASTNode **program) {
  ReturnCodes rc = RETURN_SUCCESS;
  Parser parser = {0};

  rc = parser_init(&parser, scanner);
  (void)parser_advance;

  parser_free(&parser);
  if (rc != RETURN_SUCCESS) {
    ast_free(parser.program);
    return rc;
  }

  *program = parser.program;
  return RETURN_SUCCESS;
}
