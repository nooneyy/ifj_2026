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
            ' ', ' ', ' ', '>', '>', ' ', ' ', '>', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_FUNC] =
        {
            ' ', ' ', '=', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        },
    [PREC_LPAREN] =
        {
            '<', '<', '<', '=', '=', '<', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '<', '<', '<', ' ',
        },
    [PREC_RPAREN] =
        {
            ' ', ' ', '=', '>', '>', ' ', ' ', '>', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
        },
    [PREC_COMMA] =
        {
            '<', '<', '<', '=', '=', '<', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '<', '<', '<', ' ',
        },
    [PREC_NEG] =
        {
            '<', '<', '<', '>', '>', '<', '<', '>', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '<', '>',
        },
    [PREC_NOT] =
        {
            '<', '<', '<', '>', '>', '<', '<', '>', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '<', '>',
        },
    [PREC_MUL] =
        {
            '<', '<', '<', '>', '>', '<', '<', '>', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '<', '>',
        },
    [PREC_DIV] =
        {
            '<', '<', '<', '>', '>', '<', '<', '>', '>', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '<', '>',
        },
    [PREC_ADD] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '<', '>',
        },
    [PREC_SUB] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '>', '>',
            '>', '>', '>', '>', '>', '>', '>', '>', '<', '>',
        },
    [PREC_LT] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<', '<',
            ' ', ' ', ' ', ' ', '>', '>', '>', '>', '<', '>',
        },
    [PREC_LTE] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<', '<',
            ' ', ' ', ' ', ' ', '>', '>', '>', '>', '<', '>',
        },
    [PREC_GT] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<', '<',
            ' ', ' ', ' ', ' ', '>', '>', '>', '>', '<', '>',
        },
    [PREC_GTE] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<', '<',
            ' ', ' ', ' ', ' ', '>', '>', '>', '>', '<', '>',
        },
    [PREC_EQ] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', ' ', ' ', '>', '>', '<', '>',
        },
    [PREC_NEQ] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', ' ', ' ', '>', '>', '<', '>',
        },
    [PREC_AND] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '>', '>', '<', '>',
        },
    [PREC_OR] =
        {
            '<', '<', '<', '>', '>', '<', '<', '<', '<', '<', '<',
            '<', '<', '<', '<', '<', '<', '<', '>', '<', '>',
        },
    [PREC_DOT] =
        {
            '=', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        },
    [PREC_DELIM] =
        {
            '<', '<', '<', ' ', ' ', '<', '<', '<', '<', '<', '<',
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
 * @brief Prints a type error and sets the parser's return code
 *
 * Does nothing if an error was already set, so a scanner error
 * isn't overwritten.
 *
 * @param parser Parser's current state
 */
static void type_error(Parser *parser) {
  if (parser->rc == RETURN_SUCCESS) {
    print_error("Semantic error: usage of undefined type",
                parser->current_token.line);
    parser->rc = ERR_SEM_UNDEF;
  }
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
 * FIRST(base <expr>) = {id, int, string, double, true, false, (}
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
  case TOKEN_BOOL:
    return true;
  default:
    return false;
  }
}

/**
 * @brief Checks whether token's type is in extended <expr>'s FIRST()
 *
 * FIRST(extended <expr>) = {id, int, double, string, true, false, (, -, !}
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
 * @brief Parses the prologue and starts the whole parsing tree
 *
 * RULE 1: PROGRAM -> KW_IMPORT ID(„ifj26“) DOT MUL EOL DEFS EOF
 *
 * @param parser Parser's current state
 * @return false on an error, else true
 */
static bool parse_program(Parser *parser);

/**
 * @brief Parses all function definitions
 *
 * RULE 2: DEFS -> DEF DEFS
 * RULE 3: DEFS -> eps
 *
 * @param parser Parser's current state
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_defs(Parser *parser);

/**
 * @brief Parses individual functions
 *
 * RULE 4: DEF -> OPT_MAIN KW_DEF ID PARAM_LIST TYPE_DECL = DEF_EX
 *
 * @param parser Parser's current state
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_fdef(Parser *parser);

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
static bool parse_opt_main(Parser *parser, bool *is_main);

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
static bool parse_opt_eol(Parser *parser);

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
static bool parse_def_ex(Parser *parser, ASTNode *func);

/**
 * @brief Parses all parameter lists in function definition
 *
 * RULE 12: PARAM_LIST -> eps
 * RULE 13: PARAM_LIST -> ( PARAM_BODY
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_param_list(Parser *parser, ASTNode *func);

/**
 * @brief Parses one parameter list as a whole
 *
 * RULE 14: PARAM_BODY -> PARAMS
 * RULE 15: PARAM_BODY -> ) PARAM_LIST
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_param_body(Parser *parser, ASTNode *func);

/**
 * @brief Parses each parameter
 *
 * RULE 16: PARAMS -> ID : TYPE NEXT
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_params(Parser *parser, ASTNode *func);

/**
 * @brief Parses , or ) after a parameter
 *
 * RULE 17: NEXT -> , NEXT_CONT
 * RULE 18: NEXT -> ) PARAM_LIST
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_next(Parser *parser, ASTNode *func);

/**
 * @brief Parses what follow a ,
 *
 * RULE 19: NEXT_CONT -> ) PARAM_LIST
 * RULE 20: NEXT_CONT -> PARAMS
 *
 * @param parser Parser's current state
 * @param func Function to assign parameters to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_next_cont(Parser *parser, ASTNode *func);

/**
 * @brief Parses variable declaration
 *
 * RULE 21: VAR_DECL -> EDIT_TYPE ID TYPE_DECL = ECC
 *
 * @param parser Parser's current state
 * @param out ASTNode to fill
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_var_decl(Parser *parser, ASTNode **out);

/**
 * @brief Parses Val/Var in Variable declaration
 *
 * RULE 22: EDIT_TYPE -> KW_VAR
 * RULE 23: EDIT_TYPE -> KW_VAL
 *
 * @param parser Parser's current state
 * @param to_set Edit type variable to be set
 * @return false if current token is not val nor var or parser_advance fails,
 * else true
 */
static bool parse_edit_type(Parser *parser, VariableType *to_set);

/**
 * @brief Parses type declaration
 *
 * RULE 24: TYPE_DECL -> : TYPE
 * RULE 25: TYPE_DECL -> eps
 *
 * @param parser Parser's current state
 * @param type Where the parsed type is stored
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_type_decl(Parser *parser, DataType *type);

/**
 * @brief Parses TYPE and saves it into its node
 *
 * RULE 26: TYPE -> KW_INT
 * RULE 27: TYPE -> KW_DOUBLE
 * RULE 28: TYPE -> KW_STRING
 * RULE 29: TYPE -> KW_BOOLEAN
 * RULE 30: TYPE -> KW_UNIT
 *
 * @param parser Parser's current state
 * @param type Where the parsed type is stored
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_type(Parser *parser, DataType *type);

/**
 * @brief Parses expression, call or control (ECC (Idk I made that up :D))
 *
 * RULE 31: ECC -> <expr> END     covers expressions and calls EC
 * RULE 32: ECC -> IF_REQ_ELSE    covers control C
 *
 * @param parser Parser's current state
 * @param out Node to connect to
 * @return false if an error occured in this or any other called function,
 * else true
 */
static bool parse_ecc(Parser *parser, ASTNode **out);

/**
 * @brief Parses end where can't be end of file
 *
 * RULE 33: END -> EOL
 * RULE 34: END -> ;
 *
 * @param parser Parser's current state
 * @return true if end of line or semicolon, else false
 */
static bool parse_end(Parser *parser);

/**
 * @brief Parses assignment to an already declared variable
 *
 * RULE 35: ASSIGN -> ID = ECC
 *
 * @param parser Parser's current state
 * @param out ASTNode to store the whole assignment
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_assign(Parser *parser, ASTNode **out);

/**
 * @brief Parses if that doesnt require else
 *
 * RULE 36: IF -> kw_if <expr> kw_then THEN_ONE
 *
 * @param parser Parser's current state
 * @param out ASTNode to be filled
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_if(Parser *parser, ASTNode **out);

/**
 * @brief Parses THEN's statement if one-line or parses THEN block
 *
 * RULE 37: THEN_ONE -> <expr> ONLY_IF
 * RULE 38: THEN_ONE -> eol BLOCK AFTER_BLOCK
 *
 * @param parser Parser's current state
 * @param then_out AST branch for if then branch
 * @param else_out AST branch for else branch
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_then_one(Parser *parser, ASTNode **then_out,
                           ASTNode **else_out);

/**
 * @brief Parses whether else branch exists
 *
 * RULE 39: ONLY_IF -> ;
 * RULE 40: ONLY_IF -> EOL AFTER_BLOCK
 * RULE 41: ONLY_IF -> kw_else ELIF
 *
 * @param parser Parser's current state
 * @param else_out ASTNode to be filled
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_only_if(Parser *parser, ASTNode **else_out);

/**
 * @brief Parses syntax after IF THEN or ELSE block
 *
 * RULE 42: AFTER_BLOCK -> eps
 * RULE 43: AFTER_BLOCK -> kw_else ELIF
 *
 * @param parser Parser's current state
 * @param out ASTNode to be filled
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_after_block(Parser *parser, ASTNode **out);

/**
 * @brief Parses else and checks if it's else if
 *
 * RULE 44: ELIF -> EOL BLOCK
 * RULE 45: ELIF -> IF
 * RULE 46: ELIF -> <expr> END
 *
 * @param parser Parser's current state
 * @param out ASTNode to be filled
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_elif(Parser *parser, ASTNode **out);

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
static bool parse_then_req_else(Parser *parser, ASTNode **then_out,
                                ASTNode **else_out);

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
static bool parse_is_req_elif(Parser *parser, ASTNode **else_out);

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
static bool parse_after_req_block(Parser *parser, ASTNode **else_out);

/**
 * @brief Parses while loop
 *
 * RULE 54: WHILE -> KW_WHILE <expr> KW_DO EOL BLOCK
 *
 * @param parser Parser's current state
 * @param out ASTNode to store the whole while
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_while(Parser *parser, ASTNode **out);

/**
 * @brief Parses for loop
 *
 * RULE 55: FOR -> KW_FOR FOR_SET KW_DO AFTER_DO
 *
 * @param parser Parser's current state
 * @param out ASTNode to store the whole for
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_for(Parser *parser, ASTNode **out);

/**
 * @brief Parses the generator and guards of a for loop
 *
 * RULE 56: FOR_SET -> EOL INDENT ID ARROW <expr> UN_TO EOL GUARDS DEDENT
 * RULE 57: FOR_SET -> ID ARROW <expr> UN_TO ONELINE_GUARDS
 *
 * @param parser Parser's current state
 * @param gen_name Where to store the name of the generator variable
 * @param range_start Where to store the start of the range
 * @param range_end Where to store the end of the range
 * @param range_step Where to store the step of the range
 * @param guards Where to store the guards
 * @param is_to Set to true if to, false if until
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_for_set(Parser *parser, char **gen_name,
                          ASTNode **range_start, ASTNode **range_end,
                          ASTNode **range_step, ASTNode **guards, bool *is_to);

/**
 * @brief Parses the end of the range after until or to
 *
 * RULE 58: UN_TO -> until <expr>
 * RULE 59: UN_TO -> to <expr> OPT_BY
 *
 * @param parser Parser's current state
 * @param range_end Where to store the end of the range
 * @param range_step Where to store the step of the range
 * @param is_to Set to true if to, false if until
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_un_to(Parser *parser, ASTNode **range_end,
                        ASTNode **range_step, bool *is_to);

/**
 * @brief Parses optional step of the range after to
 *
 * RULE 60: OPT_BY -> by <expr>
 * RULE 61: OPT_BY -> eps
 *
 * @param parser Parser's current state
 * @param range_step Where to store the step, stays NULL if there's no by
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_opt_by(Parser *parser, ASTNode **range_step);

/**
 * @brief Parses guards of a for loop
 *
 * RULE 62: GUARDS -> KW_IF <expr> EOL GUARDS
 * RULE 63: GUARDS -> eps
 *
 * @param parser Parser's current state
 * @param guards List to add the guards to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_guards(Parser *parser, ASTNode **guards);

/**
 * @brief Parses guards of a for loop in a one-line syntax
 *
 * RULE 64: ONELINE_GUARDS -> KW_IF <expr> ONELINE_GUARDS
 * RULE 65: ONELINE_GUARDS -> eps
 *
 * @param parser Parser's current state
 * @param guards List to add the guards to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_oneline_guards(Parser *parser, ASTNode **guards);

/**
 * @brief Parses body of a for loop after do
 *
 * RULE 66: AFTER_DO -> EOL BLOCK
 * RULE 67: AFTER_DO -> ECC
 *
 * @param parser Parser's current state
 * @param body Where to store the body of the for
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_after_do(Parser *parser, ASTNode **body);

/**
 * @brief Parses return statement
 *
 * RULE 68: RETURN -> KW_RETURN OPT_ECC
 *
 * @param parser Parser's current state
 * @param out ASTNode to store the whole return
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_return(Parser *parser, ASTNode **out);

/**
 * @brief Parses optional returned value after return
 *
 * RULE 69: OPT_ECC -> ECC
 * RULE 70: OPT_ECC -> END
 *
 * @param parser Parser's current state
 * @param out ASTNode to fill, stays NULL if there's no returned value
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_opt_ecc(Parser *parser, ASTNode **out);

/**
 * @brief Parses the entry to a Block
 *
 * RULE 71: BLOCK -> INDENT IN_BLOCK DEDENT
 *
 * @param parser Parser's current state
 * @param out Where to put the block
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_block(Parser *parser, ASTNode **out);

/**
 * @brief Parses the first statement in block
 *
 * RULE 72: IN_BLOCK -> STATEMENT IN_BLOCK2
 *
 * @param parser Parser's current state
 * @param block Current block to add statements to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_in_block(Parser *parser, ASTBlockNodeData *block);

/**
 * @brief Parses block statements except for the first statement
 *
 * RULE 73: IN_BLOCK2 -> STATEMENT IN_BLOCK2
 * RULE 74: IN_BLOCK2 -> eps
 *
 * @param parser Parser's current state
 * @param block Current block to add statements to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_in_block2(Parser *parser, ASTBlockNodeData *block);

/**
 * @brief Parses a single statement and appends it to the block
 *
 * RULE 75: STATEMENT -> VAR_DECL
 * RULE 76: STATEMENT -> WHILE
 * RULE 77: STATEMENT -> IF
 * RULE 78: STATEMENT -> <expr> END
 * RULE 79: STATEMENT -> FOR
 * RULE 80: STATEMENT -> ASSIGN
 * RULE 81: STATEMENT -> RETURN
 *
 * @param parser Parser's current state
 * @param block Current block to add the statement to
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_statement(Parser *parser, ASTBlockNodeData *block);

/**
 * @brief Parses an interpolated string
 *
 * RULE 82: INTERPOLATEDS -> INTERP_BEGIN INTERP_PARTS INTERP_END
 *
 * The string is built as a concatenation of its parts,
 * s"a $x ${e}" -> "a " + toStr(x) + " " + toStr(e),
 * s"" -> ""
 *
 * @param parser Parser's current state
 * @param out ASTNode to store the whole interpolated string
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_interpolateds(Parser *parser, ASTNode **out);

/**
 * @brief Parses parts of an interpolated string and joins them with +
 *
 * RULE 83: INTERP_PARTS -> INTERP_PART INTERP_PARTS
 * RULE 84: INTERP_PARTS -> eps
 *
 * @param parser Parser's current state
 * @param acc Parts joined so far, NULL before the first part
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_interp_parts(Parser *parser, ASTNode **acc);

/**
 * @brief Parses one part of an interpolated string
 *
 * RULE 85: INTERP_PART -> STRING
 * RULE 86: INTERP_PART -> ID
 * RULE 87: INTERP_PART -> INTERP_EXPR_BEGIN <expr> INTERP_EXPR_END
 *
 * @param parser Parser's current state
 * @param part ASTNode to store the part, ID and <expr> are wrapped in toStr
 * @return false if an error occured in this or any other called function, else
 * true
 */
static bool parse_interp_part(Parser *parser, ASTNode **part);

/**
 * @brief Builds a call of toStr with one argument
 *
 * Used for $id and ${expr} parts of an interpolated string.
 * arg is always taken over, on success it's inside *out,
 * on an error it gets freed.
 *
 * @param parser Parser's current state
 * @param arg Argument of the call
 * @param line Line of the interpolated part
 * @param out Set to the built function call node, NULL on an error
 * @return false if an allocation failed, else true
 */
static bool build_to_str(Parser *parser, ASTNode *arg, unsigned int line,
                         ASTNode **out);

/**
 * @brief Parses an expression
 * @param parser Pointer to the parser struct
 * @param out A pointer to the location of the to-be-created ASTNode pointer
 * @return true on success, false on an error with the error code in parser->rc,
 * *out being NULL
 */
static bool parse_expr(Parser *parser, ASTNode **out);

static bool parse_interpolateds(Parser *parser, ASTNode **out) {
  // placeholder
  *out = NULL;
  syntax_error(parser, "unimplemented");
  return false;
}

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
  // OPT_MAIN KW_DEF ID PARAM_LIST
  // RULES 12, 13
  correct = parse_param_list(parser, func);
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
  // OPT_MAIN KW_DEF ID PARAM_LIST TYPE_DECL =

  if (is_expr_ext_first(parser->current_token.type) ||
      parser->current_token.type == TOKEN_EOL ||
      parser->current_token.type == TOKEN_KW_IF) {
    // OPT_MAIN KW_DEF ID PARAM_LIST TYPE_DECL = DEF_EX
    // RULES 9, 10, 11
    correct = parse_def_ex(parser, func);
  } else {
    syntax_error(parser, "Syntactic error: Invalid function definition.");
  }

  return correct;
}

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
    // RULE 71
    correct = parse_block(parser, &func->data.function_def.body);
  } else {
    syntax_error(parser, "Syntactic error: Invalid missing expression or if.");
    correct = false;
  }
  return correct;
}

static bool parse_param_list(Parser *parser, ASTNode *func) {
  bool correct = false;

  if (parser->current_token.type == TOKEN_ASSIGN ||
      parser->current_token.type == TOKEN_COLON) {
    // eps
    // no deeper RULE
    correct = true;
  } else if (expect(parser, TOKEN_OPEN_PAR)) {
    // ( PARAM_BODY
    ReturnCodes rc = ast_func_def_add_param_list(func);
    if (rc != RETURN_SUCCESS) {
      parser->rc = ERR_INTERNAL;
      return false;
    }
    correct = parse_param_body(parser, func);
  } else {
    syntax_error(parser, "Unexpected syntax in function definition.");
    correct = false;
  }
  return correct;
}

static bool parse_param_body(Parser *parser, ASTNode *func) {
  bool correct = false;

  if (parser->current_token.type == TOKEN_ID) {
    // PARAMS
    // RULE 16
    correct = parse_params(parser, func);
  } else if (parser->current_token.type == TOKEN_CLOSE_PAR) {
    if (!parser_advance(parser)) {
      return false;
    }
    correct = parse_param_list(parser, func);
  } else {
    syntax_error(parser, "Syntactic error: Invalid parameter list in "
                         "function definition.");
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
    if (parser->current_token.type == TOKEN_ID) {
      type_error(parser);
    } else {
      syntax_error(parser, "Syntactic error: Missing or invalid data type of a "
                           "parameter in function declaration.");
    }
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

  // RULES 17, 18
  correct = parse_next(parser, func);
  return correct;
}

static bool parse_next(Parser *parser, ASTNode *func) {
  bool correct = false;
  if (expect(parser, TOKEN_COMMA)) {
    // ,
    // RULES 19, 20
    correct = parse_next_cont(parser, func);
  } else if (expect(parser, TOKEN_CLOSE_PAR)) {
    correct = parse_param_list(parser, func);
  } else {
    syntax_error(parser, "Syntactic error: Invalid syntax after a parameter.");
    correct = false;
  }
  return correct;
}

static bool parse_next_cont(Parser *parser, ASTNode *func) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_CLOSE_PAR) {
    // )
    // no deeper RULE
    // Must check whether the trailing comma that led to this point is from
    // multi-line parameter declaration.
    if (!parser->current_token.lf_before) {
      // trailing comma in a single-line parameter declaration
      syntax_error(parser, "Syntactic error: Trailing comma in one-line "
                           "parameter definition.");
      return false;
    }
    if (!parser_advance(parser)) {
      return false;
    }
    correct = parse_param_list(parser, func);
  } else if (parser->current_token.type == TOKEN_ID) {
    // PARAMS
    // RULE 16
    correct = parse_params(parser, func);
  } else {
    syntax_error(parser,
                 "Syntactic error: Invalid syntax in parameters' declaration.");
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

static bool parse_type_decl(Parser *parser, DataType *type) {
  bool correct = false;
  if (expect(parser, TOKEN_COLON)) {
    // :

    if (is_type_first(parser->current_token.type)) {
      // : TYPE
      // RULES 26, 27, 28, 29, 30
      correct = parse_type(parser, type);
    } else {
      if (parser->current_token.type == TOKEN_ID) {
        type_error(parser);
      } else {
        syntax_error(
            parser,
            "Syntactic error: Invalid syntax after ':' in type declaration.");
      }
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

static bool parse_end(Parser *parser) {
  bool correct = false;
  if (expect(parser, TOKEN_EOL) || expect(parser, TOKEN_SEMICOLON)) {
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Expected ; or end of line");
  }
  return correct;
}

static bool parse_assign(Parser *parser, ASTNode **out) {
  bool correct = false;
  ASTNode *expr = NULL;
  char *name = NULL;
  const unsigned int line = parser->current_token.line;
  *out = NULL;

  if (parser->current_token.type != TOKEN_ID) {
    syntax_error(parser, "Syntactic error: Missing identifier in assignment.");
    return false;
  }

  name = parser->current_token.content.string;
  parser->current_token.content.string = NULL;

  if (!parser_advance(parser)) {
    free(name);
    return false;
  }
  // ID

  if (!expect(parser, TOKEN_ASSIGN)) {
    free(name);
    syntax_error(parser, "Syntactic error: Missing assignment symbol (=).");
    return false;
  }
  // ID =

  if (!(parser->current_token.type == TOKEN_KW_IF ||
        is_expr_ext_first(parser->current_token.type))) {
    free(name);
    syntax_error(parser,
                 "Syntactic error: Missing expression after assignment.");
    return false;
  }

  // ID = ECC
  // RULES 31, 32
  correct = parse_ecc(parser, &expr);
  if (!correct) {
    ast_free(expr);
    free(name);
    return false;
  }

  *out = ast_init_assign(name, expr, line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }
  return correct;
}

static bool parse_if(Parser *parser, ASTNode **out) {
  bool correct = false;
  ASTNode *if_node = NULL;
  ASTNode *then_node = NULL;
  ASTNode *else_node = NULL;
  const unsigned int line = parser->current_token.line;
  *out = NULL;

  if (!expect(parser, TOKEN_KW_IF)) {
    syntax_error(parser, "Syntactic error: Missing \"if\" in an if statement.");
    return false;
  }
  // kw_if
  correct = parse_expr(parser, &if_node);
  if (!correct) {
    return false;
  }
  // kw_if <expr>

  if (!expect(parser, TOKEN_KW_THEN)) {
    ast_free(if_node);
    syntax_error(parser, "Syntactic error: Missing \"then\" in if structure.");
    return false;
  }
  // kw_if <expr> kw_then

  correct = parse_then_one(parser, &then_node, &else_node);
  if (!correct) {
    ast_free(if_node);
    ast_free(then_node);
    ast_free(else_node);
    return false;
  }
  // kw_if <expr> kw_then THEN_ONE

  *out = ast_init_if(if_node, then_node, else_node, false, line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }

  return correct;
}

static bool parse_then_one(Parser *parser, ASTNode **then_out,
                           ASTNode **else_out) {
  bool correct = false;
  *then_out = NULL;
  *else_out = NULL;
  if (is_expr_ext_first(parser->current_token.type)) {
    // <expr> ONLY_IF
    // RULES 39, 40, 41
    correct = parse_expr(parser, then_out);
    if (!correct) {
      return false;
    }
    correct = parse_only_if(parser, else_out);
  } else if (expect(parser, TOKEN_EOL)) {
    // eol BLOCK AFTER_BLOCK
    // RULE 71
    // RULES 42, 43
    correct = parse_block(parser, then_out);
    if (!correct) {
      return false;
    }
    correct = parse_after_block(parser, else_out);
  } else {
    syntax_error(parser, "Syntactic error: Missing expression after then.");
    correct = false;
  }
  return correct;
}

static bool parse_only_if(Parser *parser, ASTNode **else_out) {
  bool correct = false;
  *else_out = NULL;

  if (parser->current_token.type == TOKEN_SEMICOLON) {
    // ;
    // no deeper RULE
    correct = parse_end(parser);
  } else if (parser->current_token.type == TOKEN_EOL) {
    // EOL AFTER_BLOCK
    // RULES 42, 43
    if (!expect(parser, TOKEN_EOL)) {
      return false;
    }
    correct = parse_after_block(parser, else_out);
  } else if (expect(parser, TOKEN_KW_ELSE)) {
    // kw_else ELIF
    // RULES 44-46
    correct = parse_elif(parser, else_out);
  } else {
    syntax_error(parser,
                 "Syntactic error: Invalid syntax after then statement(s).");
  }

  return correct;
}

static bool parse_after_block(Parser *parser, ASTNode **out) {
  bool correct = false;
  *out = NULL;
  if (expect(parser, TOKEN_KW_ELSE)) {
    // kw_else ELIF
    // RULES 44-46
    correct = parse_elif(parser, out);
  } else if (is_statement_first(parser->current_token.type) ||
             parser->current_token.type == TOKEN_DEDENT) {
    // eps
    // No deeper rule
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid input after block.");
    return false;
  }
  return correct;
}

static bool parse_elif(Parser *parser, ASTNode **out) {
  bool correct = false;
  *out = NULL;
  if (expect(parser, TOKEN_EOL)) {
    // EOL BLOCK
    // RULE 71
    correct = parse_block(parser, out);
  } else if (parser->current_token.type == TOKEN_KW_IF) {
    // IF
    // RULE 36
    correct = parse_if(parser, out);
  } else if (is_expr_ext_first(parser->current_token.type)) {
    // <expr> END
    // RULES 33, 34
    correct = parse_expr(parser, out);
    if (!correct) {
      return false;
    }
    correct = parse_end(parser);
  } else {
    syntax_error(
        parser,
        "Syntactic error: Expected expression, if or end of line after else.");
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

static bool parse_then_req_else(Parser *parser, ASTNode **then_out,
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

static bool parse_while(Parser *parser, ASTNode **out) {
  bool correct = false;
  ASTNode *condition = NULL;
  ASTNode *body = NULL;
  const unsigned int line = parser->current_token.line;
  *out = NULL;

  if (!expect(parser, TOKEN_KW_WHILE)) {
    syntax_error(parser, "Syntactic error: Missing while.");
    return false;
  }
  // KW_WHILE

  correct = parse_expr(parser, &condition);
  if (!correct) {
    ast_free(condition);
    return false;
  }
  // KW_WHILE <expr>

  if (!expect(parser, TOKEN_KW_DO)) {
    ast_free(condition);
    syntax_error(parser, "Syntactic error: Missing do after while.");
    return false;
  }
  // KW_WHILE <expr> KW_DO

  if (!expect(parser, TOKEN_EOL)) {
    ast_free(condition);
    syntax_error(parser, "Syntactic error: Missing end of line after do.");
    return false;
  }
  // KW_WHILE <expr> KW_DO EOL

  // KW_WHILE <expr> KW_DO EOL BLOCK
  // RULE 71
  correct = parse_block(parser, &body);
  if (!correct) {
    ast_free(condition);
    ast_free(body);
    return false;
  }
  *out = ast_init_while(condition, body, line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }

  return correct;
}

static bool parse_for(Parser *parser, ASTNode **out) {
  bool correct = false;
  char *gen_name = NULL;
  bool is_to = false;
  ASTNode *range_start = NULL;
  ASTNode *range_end = NULL;
  ASTNode *range_step = NULL;
  ASTNode *guards = NULL;
  ASTNode *body = NULL;
  const unsigned int line = parser->current_token.line;
  *out = NULL;

  if (!expect(parser, TOKEN_KW_FOR)) {
    syntax_error(parser, "Syntactic error: Missing for.");
    return false;
  }
  // KW_FOR

  // KW_FOR FOR_SET
  // RULES 56, 57
  correct = parse_for_set(parser, &gen_name, &range_start, &range_end,
                          &range_step, &guards, &is_to);
  if (!correct) {
    free(gen_name);
    ast_free(range_start);
    ast_free(range_end);
    ast_free(range_step);
    ast_free(guards);
    return false;
  }

  if (!expect(parser, TOKEN_KW_DO)) {
    syntax_error(parser, "Syntactic error: Missing do after for.");
    free(gen_name);
    ast_free(range_start);
    ast_free(range_end);
    ast_free(range_step);
    ast_free(guards);
    return false;
  }
  // KW_FOR FOR_SET KW_DO

  // KW_FOR FOR_SET KW_DO AFTER_DO
  // RULES 66, 67
  correct = parse_after_do(parser, &body);
  if (!correct) {
    free(gen_name);
    ast_free(range_start);
    ast_free(range_end);
    ast_free(range_step);
    ast_free(guards);
    ast_free(body);
    return false;
  }

  *out = ast_init_for(gen_name, range_start, range_end, range_step, body, is_to,
                      line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    ast_free(guards);
    return false;
  }

  (*out)->data.for_node.guards = guards;

  return correct;
}

static bool parse_for_set(Parser *parser, char **gen_name,
                          ASTNode **range_start, ASTNode **range_end,
                          ASTNode **range_step, ASTNode **guards, bool *is_to) {
  bool correct = false;
  if (expect(parser, TOKEN_EOL)) {
    // EOL

    if (!expect(parser, TOKEN_INDENT)) {
      syntax_error(parser, "Syntactic error: Missing indent.");
      return false;
    }
    // EOL INDENT

    if (parser->current_token.type != TOKEN_ID) {
      syntax_error(parser, "Syntactic error: Missing identifier.");
      return false;
    }
    *gen_name = parser->current_token.content.string;
    parser->current_token.content.string = NULL;
    if (!parser_advance(parser)) {
      return false;
    }
    // EOL INDENT ID

    if (!expect(parser, TOKEN_GENERATOR)) {
      syntax_error(parser, "Syntactic error: Missing generator symbol <-.");
      return false;
    }
    // EOL INDENT ID ARROW

    correct = parse_expr(parser, range_start);
    if (!correct) {
      return false;
    }
    // EOL INDENT ID ARROW <expr>

    // EOL INDENT ID ARROW <expr> UN_TO
    // RULES 58, 59
    correct = parse_un_to(parser, range_end, range_step, is_to);
    if (!correct) {
      return false;
    }

    if (!expect(parser, TOKEN_EOL)) {
      syntax_error(parser,
                   "Syntactic error: Missing end of line after for setup.");
      return false;
    }
    // EOL INDENT ID ARROW <expr> UN_TO EOL

    // EOL INDENT ID ARROW <expr> UN_TO EOL GUARDS
    // RULES 62, 63
    correct = parse_guards(parser, guards);
    if (!correct) {
      return false;
    }

    if (!expect(parser, TOKEN_DEDENT)) {
      syntax_error(
          parser, "Syntactic error: Missing dedent after guards or for setup.");
      return false;
    }
    // EOL INDENT ID ARROW <expr> UN_TO EOL GUARDS DEDENT
  } else if (parser->current_token.type == TOKEN_ID) {
    // ID

    *gen_name = parser->current_token.content.string;
    parser->current_token.content.string = NULL;
    if (!parser_advance(parser)) {
      return false;
    }
    if (!expect(parser, TOKEN_GENERATOR)) {
      syntax_error(parser, "Syntactic error: Missing generator symbol <-.");
      return false;
    }
    // ID ARROW

    correct = parse_expr(parser, range_start);
    if (!correct) {
      return false;
    }
    // ID ARROW <expr>

    // ID ARROW <expr> UN_TO
    // RULES 58, 59
    correct = parse_un_to(parser, range_end, range_step, is_to);
    if (!correct) {
      return false;
    }

    // ID ARROW <expr> UN_TO ONELINE_GUARDS
    // RULES 64, 65
    correct = parse_oneline_guards(parser, guards);
  } else {
    syntax_error(parser, "Syntactic error: Invalid symbol in for setup.");
    return false;
  }
  return correct;
}

static bool parse_un_to(Parser *parser, ASTNode **range_end,
                        ASTNode **range_step, bool *is_to) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_ID &&
      strcmp(parser->current_token.content.string, "until") == 0) {
    // until <expr>
    *is_to = false;
    if (!parser_advance(parser)) {
      return false;
    }
    correct = parse_expr(parser, range_end);
  } else if (parser->current_token.type == TOKEN_ID &&
             strcmp(parser->current_token.content.string, "to") == 0) {
    // to <expr> OPT_BY
    *is_to = true;
    if (!parser_advance(parser)) {
      return false;
    }
    correct = parse_expr(parser, range_end);
    if (!correct) {
      return false;
    }
    // RULES 60, 61
    correct = parse_opt_by(parser, range_step);
  } else {
    syntax_error(parser, "Syntactic error: Missing until or to in for setup.");
    return false;
  }
  return correct;
}

static bool parse_opt_by(Parser *parser, ASTNode **range_step) {
  bool correct = false;
  if (parser->current_token.type == TOKEN_ID &&
      strcmp(parser->current_token.content.string, "by") == 0) {
    // by <expr>
    if (!parser_advance(parser)) {
      return false;
    }
    correct = parse_expr(parser, range_step);
  } else if (parser->current_token.type == TOKEN_EOL ||
             parser->current_token.type == TOKEN_KW_DO ||
             parser->current_token.type == TOKEN_KW_IF) {
    // eps
    // no deeper RULE
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid syntax after to.");
    return false;
  }
  return correct;
}

static bool parse_guards(Parser *parser, ASTNode **guards) {
  bool correct = false;
  ASTNode *guard = NULL;
  if (expect(parser, TOKEN_KW_IF)) {
    // KW_IF <expr> EOL GUARDS
    correct = parse_expr(parser, &guard);
    if (!correct) {
      return false;
    }
    ast_ll_append(guards, guard);
    if (!expect(parser, TOKEN_EOL)) {
      syntax_error(parser,
                   "Syntactic error: Missing end of line after a guard.");
      return false;
    }
    // RULES 62, 63
    correct = parse_guards(parser, guards);
    if (!correct) {
      return false;
    }
  } else if (parser->current_token.type == TOKEN_DEDENT) {
    // eps
    // no deeper RULE
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid guard definition.");
    return false;
  }
  return correct;
}

static bool parse_oneline_guards(Parser *parser, ASTNode **guards) {
  bool correct = false;
  ASTNode *guard = NULL;
  if (expect(parser, TOKEN_KW_IF)) {
    // RULE 64
    // KW_IF <expr> ONELINE_GUARDS
    correct = parse_expr(parser, &guard);
    if (!correct) {
      return false;
    }
    ast_ll_append(guards, guard);
    correct = parse_oneline_guards(parser, guards);
    if (!correct) {
      return false;
    }
  } else if (parser->current_token.type == TOKEN_KW_DO) {
    // RULE 65
    // eps
    // no deeper RULE
    correct = true;
  } else {
    syntax_error(parser, "Syntactic error: Invalid oneline guard definition.");
    return false;
  }
  return correct;
}

static bool parse_after_do(Parser *parser, ASTNode **body) {
  bool correct = false;
  if (expect(parser, TOKEN_EOL)) {
    // EOL BLOCK
    // RULE 71
    correct = parse_block(parser, body);
  } else if (is_expr_ext_first(parser->current_token.type) ||
             parser->current_token.type == TOKEN_KW_IF) {
    // ECC
    // RULES 31, 32
    correct = parse_ecc(parser, body);
  } else {
    syntax_error(parser, "Syntactic error: Invalid body of a for loop.");
    return false;
  }
  return correct;
}

static bool parse_return(Parser *parser, ASTNode **out) {
  bool correct = false;
  ASTNode *ret = NULL;
  const unsigned int line = parser->current_token.line;
  *out = NULL;

  if (!expect(parser, TOKEN_KW_RETURN)) {
    syntax_error(parser, "Syntactic error: Missing return statement.");
    return false;
  }
  // KW_RETURN

  // KW_RETURN OPT_ECC
  // RULES 69, 70
  correct = parse_opt_ecc(parser, &ret);
  if (!correct) {
    ast_free(ret);
    return false;
  }

  *out = ast_init_return(ret, line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }
  return correct;
}

static bool parse_opt_ecc(Parser *parser, ASTNode **out) {
  bool correct = false;
  if (is_expr_ext_first(parser->current_token.type) ||
      parser->current_token.type == TOKEN_KW_IF) {
    // ECC
    // RULES 31, 32
    correct = parse_ecc(parser, out);
  } else if (parser->current_token.type == TOKEN_EOL ||
             parser->current_token.type == TOKEN_SEMICOLON) {
    // END
    // RULES 33, 34
    correct = parse_end(parser);
  } else {
    syntax_error(parser, "Syntactic error: Invalid syntax after return.");
    correct = false;
  }
  return correct;
}

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
    // RULE 72
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

static bool parse_in_block2(Parser *parser, ASTBlockNodeData *block) {
  bool correct = false;
  if (is_statement_first(parser->current_token.type)) {
    // STATEMENT
    // RULES 75-81
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

static bool parse_statement(Parser *parser, ASTBlockNodeData *block) {
  bool correct = false;
  ASTNode *statement = NULL;
  if (parser->current_token.type == TOKEN_KW_VAL ||
      parser->current_token.type == TOKEN_KW_VAR) {
    // VAR_DECL
    // RULE 21
    correct = parse_var_decl(parser, &statement);
    if (!correct) {
      return false;
    }
  } else if (parser->current_token.type == TOKEN_KW_WHILE) {
    // WHILE
    // RULE 54
    correct = parse_while(parser, &statement);
    if (!correct) {
      return false;
    }
  } else if (parser->current_token.type == TOKEN_KW_IF) {
    // IF
    // RULE 36
    correct = parse_if(parser, &statement);
    if (!correct) {
      return false;
    }
  } else if (is_expr_base_first(parser->current_token.type) &&
             parser->next_token.type != TOKEN_ASSIGN) {
    // <expr> END
    // RULES 33, 34
    correct = parse_expr(parser, &statement);
    if (!correct) {
      return false;
    }
    correct = parse_end(parser);
    if (!correct) {
      ast_free(statement);
      return false;
    }
  } else if (parser->current_token.type == TOKEN_KW_FOR) {
    // FOR
    // RULE 55
    correct = parse_for(parser, &statement);
    if (!correct) {
      ast_free(statement);
      return false;
    }
  } else if (parser->current_token.type == TOKEN_ID &&
             parser->next_token.type == TOKEN_ASSIGN) {
    // ASSIGN
    // RULE 35
    correct = parse_assign(parser, &statement);
    if (!correct) {
      return false;
    }
  } else if (parser->current_token.type == TOKEN_KW_RETURN) {
    // RETURN
    // RULE 68
    correct = parse_return(parser, &statement);
    if (!correct) {
      return false;
    }
  } else {
    syntax_error(parser, "Syntactic error: Invalid statement in block.");
    return false;
  }
  ast_ll_append(&block->statements, statement);
  return correct;
}

static bool parse_interpolateds(Parser *parser, ASTNode **out) {
  bool correct = false;
  *out = NULL;
  const unsigned int line = parser->current_token.line;

  if (!expect(parser, TOKEN_INTERP_BEGIN)) {
    syntax_error(parser,
                 "Syntactic error: Missing dollar sign for interpolation.");
    return false;
  }
  // INTERP_BEGIN

  // INTERP_BEGIN INTERP_PARTS
  // RULES 83, 84
  correct = parse_interp_parts(parser, out);
  if (!correct) {
    return false;
  }

  if (!expect(parser, TOKEN_INTERP_END)) {
    syntax_error(parser, "Syntactic error: Missing interpolation end.");
    ast_free(*out);
    *out = NULL;
    return false;
  }
  // INTERP_BEGIN INTERP_PARTS INTERP_END

  if (*out == NULL) {
    // s"" has no parts, it's an empty string literal
    char *empty = malloc(1);
    if (empty == NULL) {
      parser->rc = ERR_INTERNAL;
      print_error("Internal error: Failed to allocate memory.", line);
      return false;
    }
    empty[0] = '\0';
    *out =
        ast_init_literal(STRING, (ASTLiteralNodeData){.string = empty}, line);
    if (*out == NULL) {
      parser->rc = ERR_INTERNAL;
      return false;
    }
  }
  return correct;
}

static bool parse_interp_parts(Parser *parser, ASTNode **acc) {
  (void)parse_interpolateds;
  bool correct = false;
  ASTNode *part = NULL;
  const unsigned int line = parser->current_token.line;
  if (parser->current_token.type == TOKEN_INTERP_END) {
    // eps
    // no deeper RULE
    correct = true;
  } else if (parser->current_token.type == TOKEN_STRING ||
             parser->current_token.type == TOKEN_ID ||
             parser->current_token.type == TOKEN_INTERP_EXPR_BEGIN) {
    // INTERP_PART
    // RULES 85, 86, 87
    correct = parse_interp_part(parser, &part);
    if (!correct) {
      ast_free(*acc);
      *acc = NULL;
      return false;
    }
    if (*acc == NULL) {
      // first part, nothing to join it with yet
      *acc = part;
    } else {
      // parts joined so far + part
      *acc = ast_init_binary_op(OP_ADD, *acc, part, line);
      if (*acc == NULL) {
        parser->rc = ERR_INTERNAL;
        return false;
      }
    }
    // INTERP_PART INTERP_PARTS
    // RULES 83, 84
    correct = parse_interp_parts(parser, acc);
  } else {
    syntax_error(parser, "Syntactic error: Invalid interpolation in string.");
    ast_free(*acc);
    *acc = NULL;
    return false;
  }
  return correct;
}

static bool build_to_str(Parser *parser, ASTNode *arg, const unsigned int line,
                         ASTNode **out) {
  *out = NULL;

  char *name = malloc(sizeof("toStr"));
  if (name == NULL) {
    ast_free(arg);
    parser->rc = ERR_INTERNAL;
    return false;
  }
  strcpy(name, "toStr");

  *out = ast_init_func_call(name, line);
  if (*out == NULL) {
    ast_free(arg);
    parser->rc = ERR_INTERNAL;
    return false;
  }

  ReturnCodes rc = ast_func_call_new_arg_list(*out);
  if (rc != RETURN_SUCCESS) {
    *out = NULL;
    ast_free(arg);
    parser->rc = rc;
    return false;
  }

  rc = ast_func_call_add_arg(*out, arg);
  if (rc != RETURN_SUCCESS) {
    *out = NULL;
    parser->rc = rc;
    return false;
  }
  return true;
}

static bool parse_interp_part(Parser *parser, ASTNode **part) {
  bool correct = false;
  *part = NULL;
  const unsigned int line = parser->current_token.line;
  if (parser->current_token.type == TOKEN_STRING) {
    // STRING
    // no deeper RULE
    char *text = parser->current_token.content.string;
    parser->current_token.content.string = NULL;
    *part =
        ast_init_literal(STRING, (ASTLiteralNodeData){.string = text}, line);
    if (*part == NULL) {
      parser->rc = ERR_INTERNAL;
      return false;
    }
    correct = expect(parser, TOKEN_STRING);
    if (!correct) {
      ast_free(*part);
      *part = NULL;
      return false;
    }
  } else if (parser->current_token.type == TOKEN_ID) {
    // ID
    // no deeper RULE
    char *name = parser->current_token.content.string;
    parser->current_token.content.string = NULL;
    ASTNode *var = ast_init_var_ref(name, line);
    if (var == NULL) {
      parser->rc = ERR_INTERNAL;
      return false;
    }
    if (!expect(parser, TOKEN_ID)) {
      ast_free(var);
      return false;
    }
    correct = build_to_str(parser, var, line, part);
  } else if (expect(parser, TOKEN_INTERP_EXPR_BEGIN)) {
    // INTERP_EXPR_BEGIN

    // INTERP_EXPR_BEGIN <expr>
    // PRECEDENCE ANALYSIS
    ASTNode *expr = NULL;
    correct = parse_expr(parser, &expr);
    if (!correct) {
      return false;
    }
    if (!expect(parser, TOKEN_INTERP_EXPR_END)) {
      syntax_error(parser,
                   "Syntactic error: Missing } in interpolated string.");
      ast_free(expr);
      return false;
    }
    // INTERP_EXPR_BEGIN <expr> INTERP_EXPR_END
    correct = build_to_str(parser, expr, line, part);
  } else {
    syntax_error(parser,
                 "Syntactic error: Invalid syntax inside expression in an "
                 "interpolated string.");
  }
  return correct;
}

/**
 * @brief Parses the whole program and builds its AST
 *
 * @param scanner Scanner the parser asks for tokens
 * @param program Set to the root of the built AST on success,
 *                the caller frees it with ast_free()
 * @return RETURN_SUCCESS in case of a success
 *         ERR_LEX_ANA in case of a lexical analysis error
 *         ERR_SYN_ANA in case of a syntactic analysis error
 *         ERR_INTERNAL in case of an internal error
 */
ReturnCodes parse_input(Scanner *scanner, ASTNode **program) {
  ReturnCodes rc = RETURN_SUCCESS;
  bool correct = false;
  Parser parser = {0};

  rc = parser_init(&parser, scanner);
  if (rc != RETURN_SUCCESS) {
    parser_free(&parser);
    return rc;
  }

  // PROGRAM
  // RULE 1
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
  case TOKEN_ID: {
    bool prev_can_end_expr =
        ((prev == PREC_RPAREN) || (prev == PREC_OPERAND)) != 0;
    bool curr_eq_to = strcmp(parser->current_token.content.string, "to") == 0;
    bool curr_eq_until =
        strcmp(parser->current_token.content.string, "until") == 0;
    bool curr_eq_by = strcmp(parser->current_token.content.string, "by") == 0;

    if (prev_can_end_expr && (curr_eq_to || curr_eq_until || curr_eq_by)) {
      // Parsed expression is in a for loop generator, end the expression when
      // hitting a "to" | "until" | "by"
      return PREC_DELIM;
    }

    if (parser->next_token.type == TOKEN_OPEN_PAR) {
      return PREC_FUNC;
    }

    return PREC_OPERAND;
  }
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
  case TOKEN_DOT:
    return PREC_DOT;
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
    } else if (is_terminal(second_item, PREC_DOT) &&
               is_terminal(third_item, PREC_OPERAND) &&
               third_item.terminal_token.type == TOKEN_ID) {
      // E -> E . id (VALMEM)
      prec_stack_pop_to_less(ps); // Pop <E . id from stack

      expr = ast_init_valmem_field(first_item.expr,
                                   third_item.terminal_token.content.string,
                                   first_item.expr->line);
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
    // E -> func(E, E,...)(E, E,...)...
    size_t item_idx = less_idx + 2;
    size_t last_idx = less_idx + expr_len;

    if (!is_terminal(ps->items[last_idx], PREC_RPAREN)) {
      syntax_error(
          parser,
          "Syntactic error: wrong function call, doesn't end with RPAREN!");
      return false;
    }

    // Check for correct function call shape
    while (item_idx <= last_idx) {
      if (!is_terminal(ps->items[item_idx], PREC_LPAREN)) {
        // First item is not LPAREN
        syntax_error(parser, "Syntactic error: wrong function "
                             "call, expected LPAREN, got something else");
        return false;
      }
      item_idx++;

      if (is_terminal(ps->items[item_idx], PREC_RPAREN)) {
        // () -> list with no arguments
        item_idx++;
        continue; // Skip inner checks
      }

      while (true) {
        if (!is_expr(ps->items[item_idx])) {
          // Odd item in arg list is not E
          syntax_error(parser,
                       "Syntactic error: wrong arguments inside function "
                       "call, expected expr, got something else");
          return false;
        }
        item_idx++;

        if (is_terminal(ps->items[item_idx], PREC_RPAREN)) {
          // ')', list done
          item_idx++;
          break;
        }

        if (!is_terminal(ps->items[item_idx], PREC_COMMA)) {
          // Even item in arg list is not comma
          syntax_error(parser,
                       "Syntactic error: wrong arguments inside "
                       "function call, expected ',', got something else");
          return false;
        }
        item_idx++;

        if (is_terminal(ps->items[item_idx], PREC_RPAREN)) {
          if (!ps->items[item_idx].terminal_token.lf_before) {
            // Comma before RPAREN -> LF_BEFORE must be true
            syntax_error(parser, "Syntactic error: trailing comma inside "
                                 "function call not allowed");
            return false;
          }

          item_idx++;
          break;
        }
      }
    }

    // Function call is OK
    prec_stack_pop_to_less(ps); // Pop <func(E, E,...) from the stack

    expr = ast_init_func_call(first_item.terminal_token.content.string,
                              first_item.terminal_token.line);

    if (expr == NULL) {
      for (size_t arg_idx = less_idx + 2; arg_idx < less_idx + expr_len;
           arg_idx++) {
        // for every E, free it because allocation failed
        PrecItem maybe_arg_item = ps->items[arg_idx];
        if (is_expr(maybe_arg_item)) {
          ast_free(maybe_arg_item.expr);
        }
      }

      parser->rc = ERR_INTERNAL;
      return false;
    }

    for (item_idx = less_idx + 2; item_idx < less_idx + expr_len; item_idx++) {
      // Looping from the first item to the last item...
      ReturnCodes ret = RETURN_SUCCESS;

      PrecItem item = ps->items[item_idx];

      if (is_terminal(item, PREC_LPAREN)) {
        // '(', start new list
        ret = ast_func_call_new_arg_list(expr);
      } else if (is_expr(item)) {
        // E, add argument
        ret = ast_func_call_add_arg(expr, item.expr);
      }

      if (ret != RETURN_SUCCESS) {
        // Something failed, free the rest of the arguments
        for (size_t arg_idx = item_idx + 1; arg_idx < less_idx + expr_len;
             arg_idx++) {
          PrecItem maybe_arg_item = ps->items[arg_idx];
          if (is_expr(maybe_arg_item)) {
            ast_free(maybe_arg_item.expr);
          }
        }
        parser->rc = ret;
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

/**
 * @brief Function that determines whether there's an if statement in the expr
 * input
 * @param parser Pointer to the parser struct
 * @param prev_terminal The previously parsed Terminal from the input
 * @return true if the function caught a valid beginning of an if statement,
 * otherwise false
 */
static bool starts_if_statement(Parser *parser, PrecTerminal prev_terminal) {
  /* Three possible options:
  1) IF statement is at the beginning of the expr,
  2) IF statement is in parentheses and will get reduced to E -> (E),
  3) IF statement is in a function call separated by commas
  */
  if (parser->current_token.type == TOKEN_KW_IF &&
      (prev_terminal == PREC_DELIM || prev_terminal == PREC_COMMA ||
       prev_terminal == PREC_LPAREN)) {
    return true;
  }

  return false;
}

/**
 * @brief Recursive descent function for parsing if node in expression
 * (precedence analysis)
 * @param parser Pointer to the parser struct
 * @param out A pointer to the location of the to-be-created ASTNode pointer
 * @return true on success,  false on error with the error code being in
 * parser->rc, *out being NULL
 */
static bool parse_if_in_expr(Parser *parser, ASTNode **out) {
  bool correct = false;
  ASTNode *condition = NULL;
  ASTNode *then_node = NULL;
  ASTNode *else_node = NULL;
  const unsigned int line = parser->current_token.line;
  *out = NULL;

  if (!expect(parser, TOKEN_KW_IF)) {
    return false;
  }

  correct = parse_expr(parser, &condition);
  if (!correct) {
    return false;
  }

  if (!expect(parser, TOKEN_KW_THEN)) {
    syntax_error(parser,
                 "Syntactic error: Missing \"then\" after \"if\" in expr.");
    ast_free(condition);
    return false;
  }

  correct = parse_expr(parser, &then_node);
  if (!correct) {
    ast_free(condition);
    return false;
  }

  if (!expect(parser, TOKEN_KW_ELSE)) {
    syntax_error(parser,
                 "Syntactic error: Missing \"else\" after \"then\" in expr.");
    ast_free(condition);
    ast_free(then_node);
    return false;
  }

  correct = parse_expr(parser, &else_node);

  if (!correct) {
    ast_free(condition);
    ast_free(then_node);
    return false;
  }

  *out = ast_init_if(condition, then_node, else_node, true, line);
  if (*out == NULL) {
    parser->rc = ERR_INTERNAL;
    return false;
  }

  return correct;
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
    if (starts_if_statement(parser, prev_terminal)) {
      // CONTROL extension - if statement evaluates into an OPERAND
      ASTNode *if_node = NULL;
      bool correct = parse_if_in_expr(parser, &if_node);

      if (!correct) {
        prec_stack_free(ps);
        return false;
      }

      ret = prec_stack_push(ps, (PrecItem){.type = PREC_EXPR, .expr = if_node});

      if (ret != RETURN_SUCCESS) {
        parser->rc = ERR_INTERNAL;
        prec_stack_free(ps);
        return false;
      }

      prev_terminal = PREC_OPERAND;
      continue;
    }

    if (parser->current_token.type == TOKEN_INTERP_BEGIN) {
      // INTERPOLATEDS extension
      ASTNode *interpolateds_node = NULL;
      bool correct = parse_interpolateds(parser, &interpolateds_node);

      if (!correct) {
        prec_stack_free(ps);
        return false;
      }

      ret = prec_stack_push(
          ps, (PrecItem){.type = PREC_EXPR, .expr = interpolateds_node});

      if (ret != RETURN_SUCCESS) {
        parser->rc = ERR_INTERNAL;
        prec_stack_free(ps);
        return false;
      }

      prev_terminal = PREC_OPERAND;
      continue;
    }

    PrecTerminal input_terminal =
        terminal_from_token(parser, prev_terminal); // The token from the input
    PrecTerminal top_terminal =
        prec_stack_top_terminal(ps); // The topmost terminal on the stack

    if (top_terminal == PREC_DELIM &&
        (input_terminal == PREC_DELIM || input_terminal == PREC_COMMA ||
         input_terminal == PREC_RPAREN)) {
      // The input is empty, the expression is successfully parsed or the next
      // terminal which would lead to a syntax error belongs to an outer
      // expression
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
