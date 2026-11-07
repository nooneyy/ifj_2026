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
#include <stdbool.h>
#include <string.h>

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
  ReturnCodes rc;      // First error hit while parsing
} Parser;

/**
 * @brief Moves to the next token, next becomes current and a new next is read
 *
 * In case of an error, sets the parser's return code
 *
 * @param parser Parser to advance
 * @return false if getting next token failed, else true
 */
static bool parser_advance(Parser *parser) {
  ReturnCodes rc = RETURN_SUCCESS;
  token_free(&parser->current_token);
  parser->current_token = parser->next_token;
  // The string is now owned by current_token, prevents a double free
  parser->next_token = (Token){0};

  rc = scanner_get_token(parser->scanner, &parser->next_token);
  if (rc != RETURN_SUCCESS) {
    if (parser->rc == RETURN_SUCCESS) {
      parser->rc = rc;
    }
    return false;
  }
  return true;
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

/**
 * @brief Checks matching TokenType and scans for new token
 *
 * @param parser Parser's struct
 * @param type TokenType to match
 * @return false if token types don't match or parser_advance fails, else true
 */
static bool expect(Parser *parser, const TokenType type) {
  if (parser->current_token.type == type && parser_advance(parser)) {
    return true;
  }
  return false;
}

/**
 * @brief Prints a syntax error and sets the parser's return code
 *
 * Does nothing if an error was already set, so a scanner error
 * isn't overwritten.
 *
 * @param parser Parser's struct
 * @param message Message to be printed on stderr
 */
static void syntax_error(Parser *parser, const char *message) {
  if (parser->rc == RETURN_SUCCESS) {
    print_error(message, parser->current_token.line);
    parser->rc = ERR_SYN_ANA;
  }
}

/**
 * @brief Parses all function definitions
 *
 * @param parser Parser's struct
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_defs(Parser *parser) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_AT_MAIN ||
      parser->current_token.type == TOKEN_KW_DEF) {
    // TODO: correct = parse_fdef(Parser *parser)
    // TODO: if not correct, return false
    // TODO: correct = parse_defs(parser);
    correct = false;                                                 // TMP
    parser->rc = ERR_INTERNAL;                                       // TMP
    print_error("Not implemented yet.", parser->current_token.line); // TMP

  } else if (parser->current_token.type == TOKEN_EOF) {
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid function definition.");
  }
  return correct;
}

/**
 * @brief Parses the prologue and starts the whole parsing tree
 *
 * 1.RULE: PROGRAM —> KW_IMPORT ID(„ifj26“) DOT MUL EOL DEFS EOF
 *
 * @param parser Parser's struct
 * @return false on an error, else true
 */
static bool parse_program(Parser *parser) {
  bool correct = false;
  if (!expect(parser, TOKEN_KW_IMPORT)) {
    syntax_error(parser, "Syntactic error: Missing or invalid prologue.");
    return false;
  }
  if (parser->current_token.type != TOKEN_ID ||
      strcmp(parser->current_token.content.string, "ifj26") != 0 ||
      !parser_advance(parser)) {
    syntax_error(parser, "Syntactic error: Missing or invalid prologue.");
    return false;
  }
  if (!expect(parser, TOKEN_DOT)) {
    syntax_error(parser, "Syntactic error: Missing or invalid prologue.");
    return false;
  }

  if (!expect(parser, TOKEN_MUL)) {
    syntax_error(parser, "Syntactic error: Missing or invalid prologue.");
    return false;
  }
  if (!expect(parser, TOKEN_EOL)) {
    syntax_error(parser, "Syntactic error: Missing or invalid prologue.");
    return false;
  }
  correct = parse_defs(parser);
  if (correct && parser->current_token.type != TOKEN_EOF) {
    syntax_error(parser, "Syntactic error: Invalid content between or after "
                         "function definitions.");
    correct = false;
  }
  return correct;
}

ReturnCodes parse_input(Scanner *scanner, ASTNode **program) {
  ReturnCodes rc = RETURN_SUCCESS;
  bool correct = false;
  Parser parser = {0};

  rc = parser_init(&parser, scanner);
  if (rc != RETURN_SUCCESS) {
    parser_free(&parser);
    return rc;
  }

  correct = parse_program(&parser);
  parser_free(&parser);

  if (!correct) {
    ast_free(parser.program);
    if (parser.rc == RETURN_SUCCESS) {
      parser.rc = ERR_SYN_ANA;
    }
    return parser.rc;
  }

  *program = parser.program;
  return RETURN_SUCCESS;
}
