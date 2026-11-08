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
 * @param parser Parser's current state
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
 * @param parser Parser's current state
 * @param message Message to be printed on stderr
 */
static void syntax_error(Parser *parser, const char *message) {
  if (parser->rc == RETURN_SUCCESS) {
    print_error(message, parser->current_token.line);
    parser->rc = ERR_SYN_ANA;
  }
}

/**
 * @brief Parses optional EOL after \@main
 *
 * RULE 7: OPT_EOL -> EOL
 * RULE 8: OPT_EOL -> eps
 *
 * @param parser Parser's current state
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_opt_eol(Parser *parser) {
  bool correct = false;

  if (parser->current_token.type == TOKEN_EOL) {
    // EOL
    // no deeper RULE
    if (!parser_advance(parser)) {
      return false;
    }
    correct = true;
  } else if (parser->current_token.type == TOKEN_KW_DEF) {
    // eps
    // no deeper RULE
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid function definition.");
    correct = false;
  }
  return correct;
}

/**
 * @brief Parses optional \@main
 *
 * RULE 5: OPT_MAIN -> AT_MAIN OPT_EOL
 * RULE 6: OPT_MAIN -> eps
 *
 * @param parser Parser's current state
 * @param is_main Signals whether \@main occured
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_opt_main(Parser *parser, bool *is_main) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_AT_MAIN) {
    // AT_MAIN OPT_EOL
    // RULES 7, 8
    *is_main = true;
    if (!parser_advance(parser)) {
      return false;
    }
    correct = parse_opt_eol(parser);
  } else if (parser->current_token.type == TOKEN_KW_DEF) {
    // eps
    // no deeper RULE
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid function definition.");
  }
  return correct;
}

/**
 * @brief Parses individual functions
 *
 * RULE 4: FDEF -> OPT_MAIN KW_DEF ID ( PARAM_BODY TYPE_DECL = DEF_EX
 *
 * @param parser Parser's current state
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_fdef(Parser *parser) {
  bool correct = false;
  bool is_main = false;

  if (parser->current_token.type == TOKEN_AT_MAIN) {
    // OPT_MAIN
    // RULES 5, 6
    correct = parse_opt_main(parser, &is_main);
    if (!correct) {
      return false;
    }
  }

  if (!expect(parser, TOKEN_KW_DEF)) {
    syntax_error(parser, "Syntactic error: Missing function definition.");
    return false;
  }
  // OPT_MAIN KW_DEF

  if (parser->current_token.type != TOKEN_ID) {
    syntax_error(parser, "Syntactic error: Missing function identification in "
                         "declaration.");
    return false;
  }
  // OPT_MAIN KW_DEF ID

  ASTNode *func = ast_init_func_def(parser->current_token.content.string,
                                    is_main, parser->current_token.line);
  parser->current_token.content.string = NULL;
  if (func == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }
  ast_ll_append(&parser->program->data.program.functions, func);
  if (!parser_advance(parser)) {
    return false;
  }

  if (!expect(parser, TOKEN_OPEN_PAR)) {
    syntax_error(parser, "Syntactic error: Missing parameters in function "
                         "declaration.");
    return false;
  }
  // OPT_MAIN KW_DEF ID (

  if ((parser->current_token.type == TOKEN_CLOSE_PAR) ||
      parser->current_token.type == TOKEN_ID) {
    // OPT_MAIN KW_DEF ID ( PARAM_BODY
    // RULES 12, 13
    // TODO: correct = parse_param_body(parser, func)
    if (parser->current_token.type == TOKEN_COLON) {
      // OPT_MAIN KW_DEF ID ( PARAM_BODY TYPE_DECL
      // RULES 22, 23
      // TODO: correct = parse_type_decl(parser, func)
      correct = false;
    }
    if (!expect(parser, TOKEN_ASSIGN)) {
      return false;
    }
    // OPT_MAIN KW_DEF ID ( PARAM_BODY TYPE_DECL =

    if (parser->current_token.type == TOKEN_EOL ||
        parser->current_token.type == TOKEN_KW_IF ||
        parser->current_token.type == TOKEN_ID ||
        parser->current_token.type == TOKEN_MINUS ||
        parser->current_token.type == TOKEN_EXCL_MARK ||
        parser->current_token.type == TOKEN_INT ||
        parser->current_token.type == TOKEN_DOUBLE ||
        parser->current_token.type == TOKEN_OPEN_PAR) {
      // OPT_MAIN KW_DEF ID ( PARAM_BODY TYPE_DECL = DEF_EX
      // RULES 9, 10, 11
      // TODO: correct = parse_def_ex
    } else {
      syntax_error(parser, "Syntactic error: Invalid function definition.");
    }
  } else {
    syntax_error(
        parser, "Syntactic error: Missing parameters in function declaration.");
  }

  return correct;
}

/**
 * @brief Parses all function definitions
 *
 * RULE 2: DEFS -> FDEF DEFS
 * RULE 3: DEFS -> eps
 *
 * @param parser Parser's current state
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_defs(Parser *parser) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_AT_MAIN ||
      parser->current_token.type == TOKEN_KW_DEF) {
    correct = parse_fdef(parser);
    if (!correct) {
      return false;
    }
    correct = parse_defs(parser);

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
 * RULE 1: PROGRAM —> KW_IMPORT ID(„ifj26“) DOT MUL EOL DEFS EOF
 *
 * @param parser Parser's current state
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
