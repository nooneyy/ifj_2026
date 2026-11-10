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
 * @brief Returns false and sets the parser's RC to ERR_INTERNAL if an AST node
 * allocation went wrong (is NULL)
 * @param node A pointer to an ASTNode
 */
#define CHECK_NODE_ALLOC(node)                                                 \
  do {                                                                         \
    if ((node) == NULL) {                                                      \
      parser->rc = ERR_INTERNAL;                                               \
      return false;                                                            \
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
 * @brief Parses if that requires else
 *
 * RULE 47: IF_REQ_ELSE -> kw_if <expr> kw_then THEN_REQ_ELSE
 *
 * @param parser Parser's current state
 * @param out ASTNode to store the whole if
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_if_req_else(Parser *parser, ASTNode **out);

/**
 * @brief Parses end where can't be end of file
 *
 * RULE 31: END -> EOL
 * RULE 32: END -> ;
 *
 * @param parser Parser's current state
 * @return true if end of line or semicolon, else false
 */
static bool parse_end(Parser *parser);

/**
 * @brief Parses variable declaration
 *
 * RULE 19: VAR_DECL -> EDIT_TYPE ID TYPE_DECL = ECC
 *
 * @param parser Parser's current state
 * @param out ASTNode to fill
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_var_decl(Parser *parser, ASTNode **out);

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

/**
 * @brief Parses an expression
 * @param parser Pointer to the parser struct
 * @param out A pointer to the location of the to-be-created ASTNode pointer
 * @return true on success, false on an error with the error code in parser->rc,
 * *out being NULL
 */
static bool parse_expr(Parser *parser, ASTNode **out);

// TODO:
static bool parse_statement(const Parser *parser,
                            const ASTBlockNodeData *block) {
  (void)parser;
  (void)block;
  (void)parse_var_decl;
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
 * @return false if an error occured in this or any other called function, else
 * true
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
 * @brief Parses whether there's else if or just else
 *
 * RULE 50: IS_REQ_ELIF -> IF_REQ_ELSE
 * RULE 51: IS_REQ_ELIF -> <expr> END
 * RULE 52: IS_REQ_ELIF -> EOL BLOCK
 *
 * @param parser Parser's current state
 * @param else_out Where to store the else branch
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_is_req_elif(Parser *parser, ASTNode **else_out) {
  bool correct = false;
  *else_out = NULL;

  if (parser->current_token.type == TOKEN_KW_IF) {
    // IF_REQ_ELSE
    // RULE 47
    correct = parse_if_req_else(parser, else_out);
  } else if (is_expr_ext_first(parser->current_token.type)) {
    // <expr> END
    correct = parse_expr(parser, else_out);
    if (!correct) {
      return false;
    }
    correct = parse_end(parser);
  } else if (expect(parser, TOKEN_EOL)) {
    // EOL BLOCK
    correct = parse_block(parser, else_out);
  } else {
    syntax_error(parser,
                 "Syntactic error: Missing statement or if after else.");
    return false;
  }
  return correct;
}

/**
 * @brief Parses state after if or else block where else is required
 *
 * RULE 53: AFTER_REQ_BLOCK -> kw_else IS_REQ_ELIF
 *
 * @param parser Parser's current state
 * @param else_out Where to store the else branch
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_after_req_block(Parser *parser, ASTNode **else_out) {
  bool correct = false;
  *else_out = NULL;
  if (!expect(parser, TOKEN_KW_ELSE)) {
    syntax_error(
        parser, "Syntactic error: Missing else after if where it's mandatory.");
    return false;
  }
  correct = parse_is_req_elif(parser, else_out);
  return correct;
}

/**
 * @brief Parses then statements in if where else is required
 *
 * RULE 48: THEN_REQ_ELSE -> <expr> KW_ELSE IS_REQ_ELIF
 * RULE 49: THEN_REQ_ELSE -> EOL BLOCK AFTER_REQ_BLOCK
 *
 * @param parser Parser's current state
 * @param then_out Where to store the then branch
 * @param else_out Where to store the else branch
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_then_req_else(
    Parser *parser,
    ASTNode **then_out, // NOLINT(bugprone-easily-swappable-parameters)
    ASTNode **else_out) {
  bool correct = false;
  *then_out = NULL;
  *else_out = NULL;
  if (is_expr_ext_first(parser->current_token.type)) {
    correct = parse_expr(parser, then_out);
    if (!correct) {
      return false;
    }
    if (!expect(parser, TOKEN_KW_ELSE)) {
      syntax_error(parser,
                   "Syntactic error: Missing else where it's mandatory.");
      return false;
    }
    correct = parse_is_req_elif(parser, else_out);
  } else if (expect(parser, TOKEN_EOL)) {
    correct = parse_block(parser, then_out);
    if (!correct) {
      return false;
    }
    correct = parse_after_req_block(parser, else_out);
  } else {
    syntax_error(parser, "Syntactic error: Invalid input after then.");
    correct = false;
  }
  return correct;
}

static bool parse_if_req_else(Parser *parser, ASTNode **out) {
  bool correct = false;
  ASTNode *if_node = NULL;
  ASTNode *then_node = NULL;
  ASTNode *else_node = NULL;
  const unsigned int line = parser->current_token.line;
  *out = NULL;

  if (!expect(parser, TOKEN_KW_IF)) {
    return false;
  }

  correct = parse_expr(parser, &if_node);
  if (!correct) {
    return false;
  }
  if (!expect(parser, TOKEN_KW_THEN)) {
    syntax_error(parser, "Syntactic error: Missing \"then\" after \"if\".");
    ast_free(if_node);
    return false;
  }
  correct = parse_then_req_else(parser, &then_node, &else_node);
  if (!correct) {
    ast_free(if_node);
    ast_free(then_node);
    ast_free(else_node);
    return false;
  }

  *out = ast_init_if(if_node, then_node, else_node, true, line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }

  return correct;
}

static bool parse_end(Parser *parser) {
  bool correct = false;
  if (expect(parser, TOKEN_EOL) || expect(parser, TOKEN_SEMICOLON)) {
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Expected ; or end of line");
  }
  return correct;
}

/**
 * @brief Parses expression, call or control (ECC (Idk I made that up :D))
 *
 * RULE 29: ECC -> <expr> END     covers expressions and calls EC
 * RULE 30: ECC -> IF_REQ_ELSE    covers control C
 *
 * @param parser Parser's current state
 * @param out Node to connect to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_ecc(Parser *parser, ASTNode **out) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_KW_IF) {
    // IF_REQ_ELSE
    // RULE 47
    correct = parse_if_req_else(parser, out);
  } else if (is_expr_ext_first(parser->current_token.type)) {
    // <expr> END
    // does with precedence analysis
    correct = parse_expr(parser, out);
    if (!correct) {
      return false;
    }
    correct = parse_end(parser);
  } else {
    syntax_error(parser,
                 "Syntactic error: Expected expression, call or control.");
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
 * @brief Parses Val/Var in Variable declaration
 *
 * RULE 20: EDIT_TYPE -> KW_VAR
 * RULE 21: EDIT_TYPE -> KW_VAL
 *
 * @param parser Parser's current state
 * @param to_set Edit type variable to be set
 * @return false if current token is not val nor var or parser_advance fails,
 * else true
 */
static bool parse_edit_type(Parser *parser, VariableType *to_set) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_KW_VAR) {
    // KW_VAR
    // No deeper rule
    *to_set = VAR;
    if (!parser_advance(parser)) {
      return false;
    }
    correct = true;
  } else if (parser->current_token.type == TOKEN_KW_VAL) {
    // KW_VAL
    // No deeper rule
    *to_set = VAL;
    if (!parser_advance(parser)) {
      return false;
    }
    correct = true;
  } else {
    syntax_error(
        parser,
        "Syntactic error: Invalid variable data type. Must be val or var.");
    correct = false;
  }
  return correct;
}

static bool parse_var_decl(Parser *parser, ASTNode **out) {
  bool correct = false;
  char *name = NULL;
  VariableType val_var;
  DataType data_type;
  ASTNode *init_node = NULL;
  const unsigned int line = parser->current_token.line;

  *out = NULL;

  correct = parse_edit_type(parser, &val_var);
  if (!correct) {
    return false;
  }
  if (parser->current_token.type != TOKEN_ID) {
    syntax_error(parser, "Syntactic error: Variable declaration without name.");
    return false;
  }
  name = parser->current_token.content.string;
  parser->current_token.content.string = NULL;

  if (!parser_advance(parser)) {
    free(name);
    return false;
  }
  correct = parse_type_decl(parser, &data_type);
  if (!correct) {
    free(name);
    return false;
  }

  if (!expect(parser, TOKEN_ASSIGN)) {
    syntax_error(parser, "Syntactic error: Invalid variable declaration.");
    free(name);
    return false;
  }

  correct = parse_ecc(parser, &init_node);
  if (!correct) {
    ast_free(init_node);
    free(name);
    return false;
  }

  *out = ast_init_var_def(name, val_var, data_type, init_node, line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }

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
    correct = parse_if_req_else(parser, &func->data.function_def.body);
  } else if (is_expr_ext_first(parser->current_token.type)) {
    // <expr>
    // PRECEDENCE ANALYSIS
    correct = parse_expr(parser, &func->data.function_def.body);
    if (!correct) {
      return false;
    }
    if (!expect(parser, TOKEN_EOL)) {
      syntax_error(parser,
                   "Syntactic error: Missing end of line after an expression.");
      return false;
    }
  } else if (expect(parser, TOKEN_EOL)) {
    // EOL
    // RULE 70
    correct = parse_block(parser, &func->data.function_def.body);
  } else {
    syntax_error(parser, "Syntactic error: Invalid missing expression or if.");
    correct = false;
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
  switch (parser->current_token.type) {
  case TOKEN_ID:
    if (parser->next_token.type == TOKEN_OPEN_PAR) {
      return PREC_FUNC;
    }
    return PREC_OPERAND;
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
  case TOKEN_BOOL:
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
  case TOKEN_OR:
    return PREC_OR;
  case TOKEN_AND:
    return PREC_AND;
  default:
    return PREC_DELIM;
  }
}

/**
 * @brief Decides whether the item on the stack is a terminal of a certain type
 * @param item A precedence stack item (PrecItem struct)
 * @param terminal The type of the terminal
 * @return true if yes, false if not :P
 */
static bool is_terminal(PrecItem item, PrecTerminal terminal) {
  if (item.type == PREC_TERMINAL && item.terminal == terminal) {
    return true;
  }

  return false;
}

/**
 * @brief Decides whether the item on the stack is a binary operator
 * @param item A precedence stack item (PrecItem struct)
 * @return true if yes, false if not :P
 */
static bool is_binary_op(PrecItem item) {
  if (item.type == PREC_TERMINAL &&
      (item.terminal == PREC_ADD || item.terminal == PREC_SUB ||
       item.terminal == PREC_MUL || item.terminal == PREC_DIV ||
       item.terminal == PREC_LT || item.terminal == PREC_LTE ||
       item.terminal == PREC_GT || item.terminal == PREC_GTE ||
       item.terminal == PREC_EQ || item.terminal == PREC_NEQ ||
       item.terminal == PREC_AND || item.terminal == PREC_OR)) {
    return true;
  }

  return false;
}

/**
 * @brief Decides whether the item on the stack is a unary operator
 * @param item A precedence stack item (PrecItem struct)
 * @return true if yes, false if not :P
 */
static bool is_unary_op(PrecItem item) {
  if (item.type == PREC_TERMINAL &&
      (item.terminal == PREC_NEG || item.terminal == PREC_NOT)) {
    return true;
  }

  return false;
}

/**
 * @brief Decides whether the item on the stack is an expression
 * @param item A precedence stack item (PrecItem struct)
 * @return true if yes, false if not :P
 */
static bool is_expr(PrecItem item) {
  if (item.type == PREC_EXPR && item.expr != NULL) {
    return true;
  }

  return false;
}

/**
 * @brief Reduces the items from the top of the stack into an expression
 * until the '<' terminal, then pushes the result onto the stack
 * @param parser Pointer to the parser struct
 * @param ps Pointer to the precedence stack
 * @return true on success, false on failure with the error code in parser->rc
 */
static bool prec_reduce(Parser *parser, PrecStack *ps) {
  size_t less_idx =
      prec_stack_find_less(ps); // The location of the topmost '<' terminal

  ASTNode *expr = NULL; // The resulting expression (E)

  if (less_idx == 0) {
    // No '<' terminal on the stack
    syntax_error(parser, "Syntactic error: failed to parse expression");
    return false;
  }

  size_t expr_len =
      ps->item_count - less_idx - 1; // The amount of terminals to be reduced

  PrecItem first_item =
      ps->items[less_idx +
                1]; // The first item pushed on the stack after the '<' terminal

  if (is_terminal(first_item, PREC_OPERAND) && expr_len == 1) {
    // E -> i

    prec_stack_pop_to_less(ps); // Pop <i from stack

    switch (first_item.terminal_token.type) {
    case TOKEN_ID:
      // Parse as identifier
      expr = ast_init_var_ref(first_item.terminal_token.content.string,
                              first_item.terminal_token.line);
      break;
    case TOKEN_DOUBLE:
      // Parse as double literal
      expr = ast_init_literal(
          DOUBLE,
          (ASTLiteralNodeData){
              .dbl_val = first_item.terminal_token.content.dbl_val,
          },
          first_item.terminal_token.line);
      break;
    case TOKEN_INT:
      // Parse as integer literal
      expr = ast_init_literal(
          INT,
          (ASTLiteralNodeData){
              .int_val = first_item.terminal_token.content.int_val,
          },
          first_item.terminal_token.line);
      break;
    case TOKEN_STRING:
      // Parse as string literal
      expr = ast_init_literal(
          STRING,
          (ASTLiteralNodeData){
              .string = first_item.terminal_token.content.string,
          },
          first_item.terminal_token.line);
      break;
    case TOKEN_BOOL:
      // Parse as boolean literal
      expr = ast_init_literal(
          BOOLEAN,
          (ASTLiteralNodeData){
              .bool_val = first_item.terminal_token.content.boolean,
          },
          first_item.terminal_token.line);
      break;
    default:
      parser->rc = ERR_INTERNAL;
      return false;
    }
    CHECK_NODE_ALLOC(expr);
  } else if (is_expr(first_item) && expr_len == 3) {
    PrecItem second_item = ps->items[less_idx + 2];
    PrecItem third_item = ps->items[less_idx + 3];

    if (is_binary_op(second_item) && is_expr(third_item)) {
      // E -> E op E
      prec_stack_pop_to_less(ps); // Pop <E op E from stack

      switch (second_item.terminal) {
      case PREC_MUL:
        // Parse as E * E
        expr = ast_init_binary_op(OP_MUL, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_DIV:
        // Parse as E / E
        expr = ast_init_binary_op(OP_DIV, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_ADD:
        // Parse as E + E
        expr = ast_init_binary_op(OP_ADD, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_SUB:
        // Parse as E - E
        expr = ast_init_binary_op(OP_SUB, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_LT:
        // Parse as E < E
        expr = ast_init_binary_op(OP_LT, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_LTE:
        // Parse as E <= E
        expr = ast_init_binary_op(OP_LTE, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_GT:
        // Parse as E > E
        expr = ast_init_binary_op(OP_GT, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_GTE:
        // Parse as E >= E
        expr = ast_init_binary_op(OP_GTE, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_EQ:
        // Parse as E == E
        expr = ast_init_binary_op(OP_EQ, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_NEQ:
        // Parse as E != E
        expr = ast_init_binary_op(OP_NEQ, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_AND:
        // Parse as E && E
        expr = ast_init_binary_op(OP_AND, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      case PREC_OR:
        // Parse as E || E
        expr = ast_init_binary_op(OP_OR, first_item.expr, third_item.expr,
                                  first_item.expr->line);
        break;
      default:
        // This should never happen (it's checked by is_binary_op)
        parser->rc = ERR_INTERNAL;
        return false;
      }
      CHECK_NODE_ALLOC(expr);
    }
  } else if (is_unary_op(first_item) && expr_len == 2) {
    PrecItem second_item = ps->items[less_idx + 2];

    if (is_expr(second_item)) {
      // E -> op E
      prec_stack_pop_to_less(ps); // Pop <opE from the stack

      if (first_item.terminal == PREC_NEG) {
        // Parse as -E
        expr =
            ast_init_unary_op(OP_NEG, second_item.expr, second_item.expr->line);
      } else {
        // Parse as !E
        expr =
            ast_init_unary_op(OP_NOT, second_item.expr, second_item.expr->line);
      }
      CHECK_NODE_ALLOC(expr);
    }
  } else if (is_terminal(first_item, PREC_LPAREN)) {
    PrecItem second_item = ps->items[less_idx + 2];

    if (expr_len == 2 && is_terminal(second_item, PREC_RPAREN)) {
      // unit: E -> ()
      prec_stack_pop_to_less(ps); // Pop <() from the stack
      expr = ast_init_literal(UNIT, (ASTLiteralNodeData){0},
                              first_item.terminal_token.line);
      CHECK_NODE_ALLOC(expr);
    } else if (expr_len == 3) {
      PrecItem third_item = ps->items[less_idx + 3];
      if (is_expr(second_item) && is_terminal(third_item, PREC_RPAREN)) {
        // E -> (E)
        prec_stack_pop_to_less(ps); // Pop <(E) from the stack
        expr = second_item.expr;    // We'll push the parentheses contents back
      }
    }
  } else if (is_terminal(first_item, PREC_FUNC) && expr_len >= 3) {
    // E -> func(E, E,...)
    PrecItem second_item = ps->items[less_idx + 2];
    PrecItem last_item = ps->items[less_idx + expr_len];

    if (is_terminal(second_item, PREC_LPAREN) &&
        is_terminal(last_item, PREC_RPAREN)) {
      // OK: func(___)
      // left to check: E, E, E,...

      size_t arg_len =
          expr_len - 3; // The length of the arguments inside the parentheses

      for (size_t arg_idx = 0; arg_idx < arg_len; arg_idx++) {
        PrecItem arg_item = ps->items[arg_idx + less_idx + 3];

        if (arg_idx % 2 == 0) {
          // Even argument index... must be E
          if (!is_expr(arg_item)) {
            syntax_error(parser,
                         "Syntactic error: wrong arguments inside function "
                         "call, expected expr, got something else");
            return false;
          }
        } else {
          // Odd argument index... must be comma
          if (!is_terminal(arg_item, PREC_COMMA)) {
            syntax_error(parser,
                         "Syntactic error: wrong arguments inside "
                         "function call, expected ',', got something else");
            return false;
          }
        }
      }

      // Check trailing comma
      if (arg_len > 0 && arg_len % 2 == 0 &&
          !last_item.terminal_token.lf_before) {
        syntax_error(parser, "Syntactic error: trailing comma inside "
                             "function call not allowed");
        return false;
      }

      // Function call is OK
      prec_stack_pop_to_less(ps); // Pop <func(E, E,...) from the stack

      expr = ast_init_func_call(first_item.terminal_token.content.string,
                                first_item.terminal_token.line);

      if (expr != NULL) {
        for (size_t arg_idx = 0; arg_idx < arg_len; arg_idx += 2) {
          // for every E, add it as an argument
          ast_ll_append(&expr->data.function_call.args,
                        ps->items[arg_idx + less_idx + 3].expr);
        }
      } else {
        for (size_t arg_idx = 0; arg_idx < arg_len; arg_idx += 2) {
          // for every E, free it because allocation failed
          ast_free(ps->items[arg_idx + less_idx + 3].expr);
        }
        parser->rc = ERR_INTERNAL;
        return false;
      }
    }
  }

  if (expr == NULL) {
    syntax_error(
        parser,
        "Syntactic error: precedence analysis failed to reduce expression");
    return false;
  }

  ReturnCodes ret =
      prec_stack_push(ps, (PrecItem){.type = PREC_EXPR, .expr = expr});

  if (ret != RETURN_SUCCESS) {
    parser->rc = ret;
    return false;
  }

  return true;
}

static bool parse_expr(Parser *parser, ASTNode **out) {
  PrecStack *ps = prec_stack_init();

  *out = NULL;

  if (ps == NULL) {
    // Failed to init prec stack
    parser->rc = ERR_INTERNAL;
    return false;
  }

  PrecTerminal prev_terminal = PREC_DELIM; // The stack always starts with a '$'
  ReturnCodes ret;
  while (true) {
    PrecTerminal input_terminal =
        terminal_from_token(parser, prev_terminal); // The token from the input
    PrecTerminal top_terminal =
        prec_stack_top_terminal(ps); // The topmost terminal on the stack

    if (top_terminal == PREC_DELIM && input_terminal == PREC_DELIM) {
      // The input is empty or the expression is successfully parsed
      break;
    }

    switch (PREC_TABLE[top_terminal][input_terminal]) {
    case '<':
      // Insert a '<' terminal to the stack and go to case '='
      ret = prec_stack_insert_less(ps);
      if (ret != RETURN_SUCCESS) {
        prec_stack_free(ps);
        parser->rc = ERR_INTERNAL;
        return false;
      }
    case '=':
      // Push the input terminal to the stack
      ret = prec_stack_push(ps, (PrecItem){
                                    .type = PREC_TERMINAL,
                                    .terminal = input_terminal,
                                    .terminal_token = parser->current_token,
                                });
      parser->current_token.content.string =
          NULL; // Take ownership of the string content

      if (ret != RETURN_SUCCESS) {
        prec_stack_free(ps);
        parser->rc = ERR_INTERNAL;
        return false;
      }

      prev_terminal = input_terminal; // The input terminal now becomes the
                                      // previous terminal

      if (!parser_advance(parser)) {
        prec_stack_free(ps);
        return false;
      }
      break;
    case '>':
      // Reduce the stack from the top until a '<'
      if (!prec_reduce(parser, ps)) {
        prec_stack_free(ps);
        return false;
      }
      break;
    default:
    case ' ':
      syntax_error(parser, "Syntactic error: failed to parse expression");
      prec_stack_free(ps);
      return false;
    }
  }

  if (ps->item_count == 1) {
    // Only '$' on the stack
    syntax_error(parser, "Syntactic error: empty expression");
    prec_stack_free(ps);
    return false;
  }

  *out = prec_stack_pop_result(ps);
  prec_stack_free(ps);

  if (*out == NULL) {
    // Wrong state of precedence stack
    parser->rc = ERR_INTERNAL;
    return false;
  }

  return true;
}
