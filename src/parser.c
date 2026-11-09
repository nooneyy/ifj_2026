/**
 * @file parser.c
 * @brief Compiler's parser with LL analysis and precedence analysis
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 7.11.2026
 */

#include "parser.h"
#include "ast.h"
#include "prec_stack.h"
#include "return.h"
#include "scanner.h"
#include "symbol.h"
#include "tokens.h"
#include <stdbool.h>
#include <stdlib.h>
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
 * @brief The precedence table, the first array idx being the terminal on
 * the stack and the second being the input terminal
 */
static const char PREC_TABLE[PREC_TABLE_SIZE][PREC_TABLE_SIZE] = {
    [PREC_OPERAND] =
        {
            ' ', ' ', ' ', '>', '>', ' ', ' ', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_FUNC] =
        {
            ' ', ' ', '=', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        },
    [PREC_LPAREN] =
        {
            '<', '<', '<', '=', '=', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '<', '<', '<', ' ',
        },
    [PREC_RPAREN] =
        {
            ' ', ' ', ' ', '>', '>', ' ', ' ', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_COMMA] =
        {
            '<', '<', '<', '=', '=', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '<', '<', '<', ' ',
        },
    [PREC_NEG] =
        {
            '<', '<', '<', '>', '>', '<', '<', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_NOT] =
        {
            '<', '<', '<', '>', '>', '<', '<', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_MUL] =
        {
            '<', '<', '<', '>', '>', '<', '<', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_DIV] =
        {
            '<', '<', '<', '>', '>', '<', '<', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_ADD] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_SUB] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_LT] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<',
            '<', ' ', ' ', ' ', ' ', '>', '>', '>', '>', '>',
        },
    [PREC_LTE] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<',
            '<', ' ', ' ', ' ', ' ', '>', '>', '>', '>', '>',
        },
    [PREC_GT] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<',
            '<', ' ', ' ', ' ', ' ', '>', '>', '>', '>', '>',
        },
    [PREC_GTE] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<',
            '<', ' ', ' ', ' ', ' ', '>', '>', '>', '>', '>',
        },
    [PREC_EQ] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', ' ', ' ', '>', '>', '>',
        },
    [PREC_NEQ] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', ' ', ' ', '>', '>', '>',
        },
    [PREC_AND] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '<', '>', '>', '>',
        },
    [PREC_OR] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '<', '<', '>', '>',
        },
    [PREC_DELIM] =
        {
            '<', '<', '<', ' ', ' ', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '<', '<', '<', ' ',
        },
};

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
 * @brief Checks whether token's type is in base <expr>'s FIRST()
 *
 * FIRST(base <expr>) = {id, int, string, double, (}
 *
 * @param type TokenType of token to check
 * @return true if is in FIRST(), else false
 */
static bool is_expr_base_first(const TokenType type) {
  switch (type) {
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
  case TOKEN_ID:
  case TOKEN_OPEN_PAR:
    return true;
  default:
    return false;
  }
}

/**
 * @brief Checks whether token's type is in extended <expr>'s FIRST()
 *
 * FIRST(extended <expr>) = {id, int, double, string, (, -, !}
 *
 * @param type TokenType of token to check
 * @return true if is in FIRST(), else false
 */
static bool is_expr_ext_first(const TokenType type) {
  if (is_expr_base_first(type)) {
    return true;
  }
  switch (type) {
  case TOKEN_MINUS:
  case TOKEN_EXCL_MARK:
    return true;
  default:
    return false;
  }
}

/**
 * @brief Checks whether token's type is in TYPE's FIRST()
 *
 * FIRST(TYPE) = {kw_int, kw_double, kw_string, kw_boolean, kw_unit}
 *
 * @param type TokenType of token to check
 * @return true if is in FIRST(), else false
 */
static bool is_type_first(const TokenType type) {
  switch (type) {
  case TOKEN_KW_INT:
  case TOKEN_KW_DOUBLE:
  case TOKEN_KW_STRING:
  case TOKEN_KW_BOOLEAN:
  case TOKEN_KW_UNIT:
    return true;
  default:
    return false;
  }
}

/**
 * @brief Checks whether token's type is in STATEMENT's FIRST()
 *
 * FIRST(STATEMENT) = {kw_if, kw_while, kw_for, kw_return, kw_var, kw_val, id,
 * int, double, string, true, false, (}
 *
 * @param type TokenType of token to check
 * @return true if is in FIRST(), else false
 */
static bool is_statement_first(const TokenType type) {
  if (is_expr_base_first(type)) {
    return true;
  }
  switch (type) {
  case TOKEN_KW_IF:
  case TOKEN_KW_WHILE:
  case TOKEN_KW_FOR:
  case TOKEN_KW_RETURN:
  case TOKEN_KW_VAR:
  case TOKEN_KW_VAL:
    return true;
  default:
    return false;
  }
}

/**
 * @brief Parses each parameter
 *
 * RULE 14: PARAMS -> ID : TYPE NEXT
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_params(Parser *parser, ASTNode *func);

// TODO:
static bool parse_statement(const Parser *parser,
                            const ASTBlockNodeData *block) {
  (void)parser;
  (void)block;
  return false;
}

/**
 * @brief Parses block statements except for the first statement
 *
 * RULE 72: IN_BLOCK2 -> STATEMENT IN_BLOCK2
 * RULE 73: IN_BLOCK2 -> eps
 *
 * @param parser Parser's current state
 * @param block Current block to add statements to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_in_block2(Parser *parser, ASTBlockNodeData *block) {
  bool correct = false;
  if (is_statement_first(parser->current_token.type)) {
    // STATEMENT
    // RULES 74-80
    correct = parse_statement(parser, block);
    if (!correct) {
      return false;
    }
    correct = parse_in_block2(parser, block);
  } else if (parser->current_token.type == TOKEN_DEDENT) {
    // eps
    // no deeper RULE
    correct = true;
  } else {
    syntax_error(parser,
                 "Syntactic error: Expected a statement or end of block.");
  }
  return correct;
}

/**
 * @brief Parses the first statement in block
 *
 * RULE 71: IN_BLOCK -> STATEMENT IN_BLOCK2
 *
 * @param parser Parser's current state
 * @param block Current block to add statements to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_in_block(Parser *parser, ASTBlockNodeData *block) {
  bool correct = false;
  // STATEMENT

  correct = parse_statement(parser, block);
  if (!correct) {
    return false;
  }
  correct = parse_in_block2(parser, block);
  return correct;
}

/**
 * @brief Parses the entry to a Block
 *
 * RULE 70: BLOCK -> INDENT IN_BLOCK DEDENT
 *
 * @param parser Parser's current state
 * @param out Where to put the block
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_block(Parser *parser, ASTNode **out) {
  bool correct = false;
  if (!expect(parser, TOKEN_INDENT)) {
    syntax_error(parser, "Syntactic error: Missing indentation for a block.");
    return false;
  }
  // INDENT

  *out = ast_init_block(parser->current_token.line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }
  if (is_statement_first(parser->current_token.type)) {
    // INDENT IN_BLOCK
    // RULE 71
    correct = parse_in_block(parser, &(*out)->data.block);
    if (!correct) {
      return false;
    }
    if (!expect(parser, TOKEN_DEDENT)) {
      syntax_error(parser, "Syntactic error: Missing dedent.");
      correct = false;
    }
  } else {
    syntax_error(parser, "Syntactic error: Empty block.");
    correct = false;
  }
  return correct;
}

/**
 * @brief Parses TYPE and saves it into its node
 *
 * RULE 24 -> KW_INT
 * RULE 25 -> KW_DOUBLE
 * RULE 26 -> KW_STRING
 * RULE 27 -> KW_BOOLEAN
 * RULE 28 -> KW_UNIT
 *
 * @param parser Parser's current state
 * @param type Where the parsed type is stored
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_type(Parser *parser, DataType *type) {
  switch (parser->current_token.type) {
  case TOKEN_KW_INT:
    // KW_INT
    // no deeper RULE
    *type = INT;
    break;
  case TOKEN_KW_DOUBLE:
    // KW_DOUBLE
    // no deeper RULE
    *type = DOUBLE;
    break;
  case TOKEN_KW_STRING:
    // KW_STRING
    // no deeper RULE
    *type = STRING;
    break;
  case TOKEN_KW_BOOLEAN:
    // KW_BOOLEAN
    // no deeper RULE
    *type = BOOLEAN;
    break;
  case TOKEN_KW_UNIT:
    // KW_UNIT
    // no deeper RULE
    *type = UNIT;
    break;
  default:
    syntax_error(parser,
                 "Syntactic error: Invalid type assigned in declaration.");
    return false;
  }
  return parser_advance(parser);
}

/**
 * @brief Parses type declaration
 *
 * RULE 22: TYPE_DECL -> : TYPE
 * RULE 23: TYPE_DECL -> eps
 *
 * @param parser Parser's current state
 * @param type Where the parsed type is stored
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_type_decl(Parser *parser, DataType *type) {
  bool correct = false;
  if (expect(parser, TOKEN_COLON)) {
    // :

    if (is_type_first(parser->current_token.type)) {
      // : TYPE
      // RULES 24, 25, 26, 27, 28
      correct = parse_type(parser, type);
    } else {
      syntax_error(
          parser,
          "Syntactic error: Invalid syntax after ':' in type declaration.");
      return false;
    }
  } else if (parser->current_token.type == TOKEN_ASSIGN) {
    // TODO: Could also be different than just = in val, var decl
    // eps
    // no deeper rules
    *type = UNKNOWN;
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid type declaration.");
    correct = false;
  }
  return correct;
}

/**
 * @brief Parses variable declaration
 *
 * RULE 19: VAR_DECL -> EDIT_TYPE ID TYPE_DECL = ECC
 *
 * @param parser Parser's current state
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_var_decl(Parser *parser) {
  bool correct = false;
  // TODO: const char *name = {};
  // VariableType val_var;
  DataType var_type;
  // correct = parse_edit_type(parser, &val_var);
  if (!correct) {
    return false;
  }
  if (parser->current_token.type != TOKEN_ID) {
    syntax_error(parser, "Syntactic error: Variable declaration without name.");
    return false;
  }
  // name = parser->current_token.content.string;
  if (!parser_advance(parser)) {
    return false;
  }
  correct = parse_type_decl(parser, &var_type);
  if (!correct) {
    return false;
  }
  if (!expect(parser, TOKEN_ASSIGN)) {
    syntax_error(parser, "Syntactic error: Invalid variable declaration.");
    return false;
  }
  // TODO: use these
  // (void)name;
  // (void)val_var;
  (void)var_type;
  // TODO: correct = parse_ecc(parser);
  return correct;
}

/**
 * @brief Parses what follow a ,
 *
 * RULE 17: NEXT_CONT -> )
 * RULE 18: NEXT_CONT -> PARAMS
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_next_cont(Parser *parser, ASTNode *func) {
  (void)parse_var_decl; // TODO: delete this
  bool correct = false;
  if (parser->current_token.type == TOKEN_CLOSE_PAR) {
    // )
    // no deeper RULE
    // Must check whether the trailing comma that led to this point is from
    // multi-line parameter declaration.
    if (parser->current_token.lf_before) {
      correct = true;
    } else {
      // trailing comma in a single-line parameter declaration
      syntax_error(parser, "Syntactic error: Trailing comma in one-line "
                           "parameter definition.");
      return false;
    }
    if (!parser_advance(parser)) {
      return false;
    }
  } else if (parser->current_token.type == TOKEN_ID) {
    // PARAMS
    // RULE 14
    correct = parse_params(parser, func);
  } else {
    syntax_error(parser,
                 "Syntactic error: Invalid syntax in parameters' declaration.");
    correct = false;
  }
  return correct;
}

/**
 * @brief Parses , or ) after a parameter
 *
 * RULE 15: NEXT -> , NEXT_CONT
 * RULE 16: NEXT -> )
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_next(Parser *parser, ASTNode *func) {
  bool correct = false;
  if (expect(parser, TOKEN_COMMA)) {
    // ,
    // RULES 17, 18
    correct = parse_next_cont(parser, func);
  } else if (expect(parser, TOKEN_CLOSE_PAR)) {
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid syntax after a parameter.");
    correct = false;
  }
  return correct;
}

static bool parse_params(Parser *parser, ASTNode *func) {
  bool correct = false;
  char *name = parser->current_token.content.string;
  parser->current_token.content.string = NULL;
  DataType type;
  // ID
  if (!parser_advance(parser)) {
    free(name);
    return false;
  }

  if (!expect(parser, TOKEN_COLON)) {
    syntax_error(parser, "Syntactic error: Missing parameter's data type in "
                         "function definition.");
    free(name);
    return false;
  }
  // ID :

  if (!is_type_first(parser->current_token.type)) {
    syntax_error(parser, "Syntactic error: Missing or invalid data type of a "
                         "parameter in function declaration.");
    free(name);
    return false;
  }
  // ID : TYPE

  correct = parse_type(parser, &type);
  if (!correct) {
    free(name);
    return false;
  }

  const ReturnCodes rc = ast_func_def_add_param(func, name, type);
  if (rc != RETURN_SUCCESS) {
    parser->rc = rc;
    return false;
  }

  if (!(parser->current_token.type == TOKEN_COMMA ||
        parser->current_token.type == TOKEN_CLOSE_PAR)) {
    syntax_error(parser, "Syntactic error: Invalid parameter definition.");
    return false;
  }

  // RULES 15, 16
  correct = parse_next(parser, func);
  return correct;
}

/**
 * @brief Parses parameters as a whole
 *
 * RULE 12: PARAM_BODY -> PARAMS
 * RULE 13: PARAM_BODY -> )
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_param_body(Parser *parser, ASTNode *func) {
  bool correct = false;

  if (parser->current_token.type == TOKEN_ID) {
    // PARAMS
    // RULE 14
    correct = parse_params(parser, func);
  } else if (parser->current_token.type == TOKEN_CLOSE_PAR) {
    correct = parser_advance(parser);
  }
  return correct;
}

/**
 * @brief Parses function's right side
 *
 * RULE 9:  DEF_EX -> IF_REQ_ELSE
 * RULE 10: DEF_EX -> <expr> EOL
 * RULE 11: DEF_EX -> EOL BLOCK
 *
 * @param parser Parser's current state
 * @param func Function to fill
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_def_ex(Parser *parser, ASTNode *func) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_KW_IF) {
    // IF_REQ_ELSE
    // RULE 47
    // TODO: parse_if_req_else()
    correct = false; // TODO
  } else if (is_expr_ext_first(parser->current_token.type)) {
    // <expr>
    // PRECEDENCE ANALYSIS
    // TODO: correct = parse_expr(parser)
    // TODO: check correct
    // TODO: if (!expect(parser, TOKEN_EOL)) {syntax_error; return false;}
    correct = true; // TODO
  } else if (expect(parser, TOKEN_EOL)) {
    // EOL
    // RULE 70
    correct = parse_block(parser, &func->data.function_def.body);
  }
  return correct;
}

/**
 * @brief Parses optional EOL after \@main
 *
 * RULE 7: OPT_EOL -> EOL
 * RULE 8: OPT_EOL -> eps
 *
 * @param parser Parser's current state
 * @return false if an error occured in this or any other called function,
 * else true
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
 * @return false if an error occured in this or any other called function,
 * else true
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
 * @return false if an error occured in this or any other called function,
 * else true
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
    syntax_error(parser, "Syntactic error: Missing function declaration.");
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
    correct = parse_param_body(parser, func);
    if (!correct) {
      return false;
    }
    correct = parse_type_decl(parser, &func->data.function_def.return_type);
    if (!correct) {
      return false;
    }
    if (!expect(parser, TOKEN_ASSIGN)) {
      syntax_error(parser, "Syntactic error: Missing function initialization "
                           "after declaration.");
      return false;
    }
    // OPT_MAIN KW_DEF ID ( PARAM_BODY TYPE_DECL =

    if (is_expr_ext_first(parser->current_token.type) ||
        parser->current_token.type == TOKEN_EOL ||
        parser->current_token.type == TOKEN_KW_IF) {
      // OPT_MAIN KW_DEF ID ( PARAM_BODY TYPE_DECL = DEF_EX
      // RULES 9, 10, 11
      correct = parse_def_ex(parser, func);
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

/**
 * @brief Function for converting a Token into a Terminal based on some
 * conditions (check PrecTerminal struct)
 * @param parser Pointer to the parser struct
 * @param prev Previous processed terminal (PREC_DELIM for the first terminal)
 * @return An output terminal corresponding to the input token
 */
static PrecTerminal terminal_from_token(Parser *parser, PrecTerminal prev) {
  // TODO: implement OR, AND once scanner has tokens for them
  switch (parser->current_token.type) {
  case TOKEN_ID:
    if (parser->next_token.type == TOKEN_OPEN_PAR) {
      return PREC_FUNC;
    }
    return PREC_OPERAND;
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
    return PREC_OPERAND;
  case TOKEN_OPEN_PAR:
    return PREC_LPAREN;
  case TOKEN_CLOSE_PAR:
    return PREC_RPAREN;
  case TOKEN_COMMA:
    return PREC_COMMA;
  case TOKEN_MINUS:
    if (prev != PREC_OPERAND && prev != PREC_RPAREN) {
      return PREC_NEG;
    }
    return PREC_SUB;
  case TOKEN_EXCL_MARK:
    return PREC_NOT;
  case TOKEN_MUL:
    return PREC_MUL;
  case TOKEN_DIV:
    return PREC_DIV;
  case TOKEN_PLUS:
    return PREC_ADD;
  case TOKEN_LOW:
    return PREC_LT;
  case TOKEN_LOWEQ:
    return PREC_LTE;
  case TOKEN_GRE:
    return PREC_GT;
  case TOKEN_GREEQ:
    return PREC_GTE;
  case TOKEN_IS_EQ:
    return PREC_EQ;
  case TOKEN_NOT_EQ:
    return PREC_NEQ;
  default:
    return PREC_DELIM;
  }
}
