/**
 * @file scanner.c
 * @brief main scanner file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "scanner.h"
#include "dynstr.h"
#include "return.h"
#include "tokens.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Macro for appending a character to a DynStr and returning with an
 * error code if it fails.
 */
#define APPEND_OR_ERROR(str, c)                                                \
  do {                                                                         \
    rc = dynstr_append(&(str), c);                                             \
    if (rc != RETURN_SUCCESS) {                                                \
      dynstr_free(&(str));                                                     \
      return rc;                                                               \
    }                                                                          \
  } while (0)

/**
 * @brief Listing of all possible FSM states
 */
typedef enum FsmState {
  STATE_IDLE,            // Idle state - initial state
  STATE_LINE_START,      // Start of a line - indentation scanning state
  STATE_ID,              // ID scanning state
  STATE_AT,              // "@" is first in the sequence
  STATE_UND,             // "_" is first in the sequence
  STATE_STR,             // String scanning state
  STATE_STR_ML,          // Multiline string scanning state
  STATE_1_QUOT,          // " is first in the sequence
  STATE_2_QUOT,          // "" is first in the sequence
  STATE_STR_ML_1_QUOT,   // " in multiline scanning state
  STATE_STR_ML_2_QUOT,   // "" in multiline scanning state
  STATE_INT,             // Int scanning state
  STATE_INT_UNDER,       // "_" appeared inside an int
  STATE_DOUBLE,          // Double scanning state
  STATE_DOUBLE_UNDER,    // "_" appeared inside a double
  STATE_DOT,             // "." appeared after an int
  STATE_DOT_START,       // "." is first in the sequence
  STATE_E,               // "e" or "E" appeared inside a number
  STATE_E_SIGN,          // "+" or "-" follows right after "e" or "E"
  STATE_EXP,             // Exponent scanning state
  STATE_EXP_UNDER,       // "_" appeared inside an exponent
  STATE_ZERO,            // "0" is first in the sequence
  STATE_ZERO_INT,        // "0" followed with int number
  STATE_SLASH,           // "/" is first in the sequence
  STATE_COMM_LINE,       // Line comment scanning state
  STATE_COMM_BLOCK,      // Block comment scanning state
  STATE_COMM_BLOCK_STAR, // "*" in block comment scanning state
  STATE_ASSIGN,          // "=" is first in the sequence
  STATE_EXCL,            // "!" is first in the sequence
  STATE_LOW,             // "<" is first in the sequence
  STATE_GRE,             // ">" is first in the sequence
  STATE_AND,             // "&" is first in the sequence
  STATE_OR,              // "|" is first in the sequence
  STATE_PAREN_NEWLINE,   // Newline inside of parenthesis
} FsmState;

/**
 * @brief For comparing strings from source with keywords
 */
typedef struct KeyWord {
  const char *text; // Keyword as a string for comparison
  TokenType type;   // Respective token type for the keyword
} KeyWord;

/**
 * @brief Initialization of all keywords and it's respective type
 *
 * Used by id_or_keyword to find out whether a string is
 * a keyword or not.
 */
static const KeyWord KEYWORDS[] = {
    {"def", TOKEN_KW_DEF},         {"do", TOKEN_KW_DO},
    {"Double", TOKEN_KW_DOUBLE},   {"else", TOKEN_KW_ELSE},
    {"if", TOKEN_KW_IF},           {"import", TOKEN_KW_IMPORT},
    {"Int", TOKEN_KW_INT},         {"return", TOKEN_KW_RETURN},
    {"String", TOKEN_KW_STRING},   {"then", TOKEN_KW_THEN},
    {"Unit", TOKEN_KW_UNIT},       {"val", TOKEN_KW_VAL},
    {"var", TOKEN_KW_VAR},         {"while", TOKEN_KW_WHILE},
    {"Boolean", TOKEN_KW_BOOLEAN}, {"case", TOKEN_KW_CASE},
    {"class", TOKEN_KW_CLASS},     {"extension", TOKEN_KW_EXTENSION},
    {"for", TOKEN_KW_FOR},         {"List", TOKEN_KW_LIST},
    {"match", TOKEN_KW_MATCH},     {"type", TOKEN_KW_TYPE},
    {"true", TOKEN_BOOL},          {"false", TOKEN_BOOL},
};

Scanner scanner_init(FILE *src_file) {
  Scanner to_fill = {.src = src_file, .at_line_start = true, .line = 1};
  return to_fill;
}

/**
 * @brief Checks whether a string is unique or a keyword
 *
 * Takes the string and compares it with each keyword
 * from KEYWORDS[].
 *
 * @param string String to check
 * @return TokenType - ID if it's not a keyword
 *                   - KW_* if it is a keyword
 */
static TokenType id_or_keyword(const char *string) {
  size_t len = sizeof(KEYWORDS) / sizeof(KEYWORDS[0]);
  for (size_t i = 0; i < len; i++) {
    if (strcmp(string, KEYWORDS[i].text) == 0) {
      return KEYWORDS[i].type;
    }
  }
  return TOKEN_ID;
}

/**
 * @brief Converting a double from DynStr to completed token
 * @param str DynStr containing the double
 * @param token Token to fill with type and content
 * @param line Current line from the scanner's state
 * @return RETURN_SUCCESS in case of a success
 *         ERR_LEX_ANA on double overflow
 */
static ReturnCodes double_to_token(DynStr *str, Token *token,
                                   unsigned int line) {
  double d = strtod(str->data, NULL);
  dynstr_free(str);

  // Check for overflow that could violate spec (Double d = INF)
  if (isinf(d)) {
    print_error("Overflow of type Double.", line);
    return ERR_LEX_ANA;
  }

  token->type = TOKEN_DOUBLE;
  token->content.dbl_val = d;
  token->line = line;

  return RETURN_SUCCESS;
}

/**
 * @brief Converting an int from DynStr to completed token
 * @param str DynStr containing the int
 * @param token Token to fill with type and content
 * @param line Current line from the scanner's state
 * @return Always returns RETURN_SUCCESS
 */
static ReturnCodes int_to_token(DynStr *str, Token *token, unsigned int line) {
  token->type = TOKEN_INT;
  token->content.int_val = strtoll(str->data, NULL, 10);
  token->line = line;
  dynstr_free(str);

  return RETURN_SUCCESS;
}

/**
 * @brief Escape sequence processing
 *
 * A switch determining whether the character after '\' is part of
 * an escape sequence or not.
 * If so, uses the escape sequence.
 * If not, returns error
 *
 * @param c Character that follows '\'
 * @param s DynStr to store the string to
 * @return RETURN_SUCCESS in case of a success
 *         ERR_LEX_ANA in case of an invalid escape sequence
 *         ERR_INTERNAL in case of an internal error
 */
static ReturnCodes escape_seq_switch(const int c, DynStr *s,
                                     unsigned int line) {
  ReturnCodes rc = RETURN_SUCCESS;
  switch (c) {
  case '\\':
  case '"':
    rc = dynstr_append(s, c);
    if (rc != RETURN_SUCCESS) {
      return rc;
    }
    break;

  case 'n':
    rc = dynstr_append(s, '\n');
    if (rc != RETURN_SUCCESS) {
      return rc;
    }
    break;

  case 'r':
    rc = dynstr_append(s, '\r');
    if (rc != RETURN_SUCCESS) {
      return rc;
    }
    break;

  case 't':
    rc = dynstr_append(s, '\t');
    if (rc != RETURN_SUCCESS) {
      return rc;
    }
    break;

  default:
    print_error("Use of an invalid escape sequence.", line);
    return ERR_LEX_ANA;
  }

  return RETURN_SUCCESS;
}

/**
 * @brief Counting indentation at the start of a line
 *
 * Reads spaces and tabs from src, starting with c.
 * Only spaces count into indent_cnt, a tab only sets tab_appeared,
 * because a tab is not allowed in the indentation of a non-empty line.
 * The first character after the indentation is NOT returned to src,
 * caller decides what to do with it.
 *
 * @param src Source file to read from
 * @param c First character of the line
 * @param indent_cnt Number of spaces, incremented for every space
 * @param tab_appeared Set to true if a tab appeared in the indentation
 * @return The first character after the indentation
 */
static int count_indent(FILE *src, int c, int *indent_cnt, bool *tab_appeared) {
  while (c == ' ' || c == '\t') {
    if (c == '\t') {
      *tab_appeared = true;
    } else {
      (*indent_cnt)++;
    }
    c = fgetc(src);
  }
  return c;
}

/**
 * @brief Comparing new line's indentation with open levels
 *
 * Compares indent_cnt with the innermost open level.
 * If indent_cnt is deeper, opens a new level and fills INDENT.
 * If it is the same, no token is filled and the line continues.
 * If it is shallower, closes levels until indent_cnt is reached
 * and fills one DEDENT, the rest is stored in sc->dedents_pending.
 *
 * @param sc Scanner struct with the open indentation levels
 * @param indent_cnt Number of spaces at the start of the new line
 * @param token Token to fill with INDENT or DEDENT
 * @param token_ready Set to true if token was filled, false if not
 * @return RETURN_SUCCESS in case of a success
 *         ERR_LEX_ANA if indent_cnt is not an open level
 *         ERR_INTERNAL if there are too many indentation levels
 */
static ReturnCodes indent_compare(Scanner *sc, int indent_cnt, Token *token,
                                  bool *token_ready) {
  int top = sc->indent_levels[sc->indent_depth];
  *token_ready = false;

  if (indent_cnt > top) {
    if (sc->indent_depth + 1 == MAX_INDENT_LEVELS) {
      print_error("Too many indentation levels.", sc->line);
      return ERR_INTERNAL;
    }
    sc->indent_depth++;
    sc->indent_levels[sc->indent_depth] = indent_cnt;
    token->type = TOKEN_INDENT;
    token->line = sc->line;
    *token_ready = true;
    return RETURN_SUCCESS;
  }

  if (indent_cnt == top) {
    return RETURN_SUCCESS;
  }

  // only indent_cnt < top left now

  while (sc->indent_depth > 0 &&
         sc->indent_levels[sc->indent_depth] != indent_cnt) {
    sc->indent_depth--;
    sc->dedents_pending++;
  }
  if (sc->indent_levels[sc->indent_depth] != indent_cnt) {
    // indent_cnt is between 2 open levels and is not open by itself
    print_error("New indentation level between 2 open levels.", sc->line);
    return ERR_LEX_ANA;
  }
  sc->dedents_pending--;
  token->type = TOKEN_DEDENT;
  token->line = sc->line;
  *token_ready = true;
  return RETURN_SUCCESS;
}

/**
 * @brief Reading source file and creating a token
 *
 * Reads sc->src char by char and moves through the FSM states
 * until the whole token is read and filled.
 * First char after the token is returned to sc->src
 * by ungetc() for the next token to start with it.
 * For tokens that need content (ID, INT, STRING, DOUBLE),
 * str (DynStr) is filled first. ID and STRING hand str's memory
 * over to content, INT and DOUBLE convert it to a number.
 * In every other case, str is freed.
 *
 * @param token_to_fill Token to fill with type and content
 * @param sc Scanner struct filled with scanner's current state
 * @return RETURN_SUCCESS in case of a success
 *         ERR_LEX_ANA in case of a lexical error
 *         ERR_INTERNAL in case of an internal error
 */
// FSM has a lot of cases, splitting it would be even more unreadable
// NOLINTNEXTLINE(readability-function-size)
static ReturnCodes scanner_fsm(Token *token_to_fill, Scanner *sc) {
  token_to_fill->content.string = NULL; // tokens without content keep NULL
  token_to_fill->lf_before =
      false; // By default, no LF was read for a new token

  FsmState state = STATE_IDLE;

  DynStr str = {NULL, 0, 0}; // empty, so dynstr_free() is always safe
  ReturnCodes rc = RETURN_SUCCESS;
  int indent_cnt = 0;
  int comm_depth = 0;
  bool comm_newline_flag = false;
  bool must_follow_eol = false;
  bool tab_appeared = false;
  bool indent_pending = false;
  bool token_ready = false;
  while (1) {

    int c = fgetc(sc->src);

    switch (state) {
    case STATE_IDLE:

      if (must_follow_eol) {
        // for multiline block comments containing \n
        if (c != EOF && c != '/' && !isspace(c)) {
          print_error("Missing end of line.", sc->line);
          return ERR_LEX_ANA;
        }
      }
      if (indent_pending && c != '/' && !isspace(c)) {
        // means there is an indent pending that appeared before block comment
        (void)ungetc(c, sc->src);
        indent_pending = false;
        state = STATE_LINE_START;
        break;
      }
      if (sc->at_line_start) {
        (void)ungetc(c, sc->src);
        sc->at_line_start = false;
        state = STATE_LINE_START;
        break;
      }
      if (sc->dedents_pending != 0) {
        /*
         * means there are dedentation tokens to be sent
         * because the last dedentation was multiple-level
         */
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_DEDENT;
        token_to_fill->line = sc->line;
        sc->dedents_pending--;
        return RETURN_SUCCESS;
      }

      if (c != EOF && c != '/' && !isspace(c)) {
        // We have to emit an EOL before an EOF (and dedents) for statements to
        // be parsed properly
        sc->emit_eol_eof = true;
      }

      if (isalpha(c)) {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        APPEND_OR_ERROR(str, c);
        state = STATE_ID;
      } else if (c == '_') {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        APPEND_OR_ERROR(str, c);
        state = STATE_UND;
      } else if (isdigit(c) && c != '0') {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        APPEND_OR_ERROR(str, c);
        state = STATE_INT;
      } else if (c == '.') {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }

        APPEND_OR_ERROR(str, c);
        state = STATE_DOT_START;
      } else if (c == '0') {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }

        APPEND_OR_ERROR(str, c);
        state = STATE_ZERO;
      } else if (c == '"') {
        state = STATE_1_QUOT;
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
      } else if (c == '\n') {
        sc->line++;
        if (sc->parenth_depth == 0) {
          if (indent_pending) {
            // means that the line only contained a comment

            indent_pending = false;
            indent_cnt = 0;
            tab_appeared = false;
            must_follow_eol = false;
            state = STATE_LINE_START;
            break;
          }
          token_to_fill->type = TOKEN_EOL;
          token_to_fill->line = sc->line - 1;
          sc->at_line_start = true;
          sc->emit_eol_eof = false;
          return RETURN_SUCCESS;
        }
        token_to_fill->lf_before = true;
        state = STATE_PAREN_NEWLINE;
        indent_cnt = 0;
        tab_appeared = false;
        break;
      } else if (c == '(') {
        sc->parenth_depth++;
        token_to_fill->type = TOKEN_OPEN_PAR;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (c == ')') {
        if (sc->parenth_depth > 0) {
          sc->parenth_depth--;
        }
        token_to_fill->type = TOKEN_CLOSE_PAR;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (c == '!') {
        state = STATE_EXCL;
      } else if (c == '=') {
        state = STATE_ASSIGN;
      } else if (c == '>') {
        state = STATE_GRE;
      } else if (c == '&') {
        state = STATE_AND;
      } else if (c == '|') {
        state = STATE_OR;
      } else if (c == '<') {
        state = STATE_LOW;
      } else if (c == '@') {
        state = STATE_AT;
      } else if (c == '/') {
        state = STATE_SLASH;
      } else if (c == ';') {
        token_to_fill->type = TOKEN_SEMICOLON;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (c == ',') {
        token_to_fill->type = TOKEN_COMMA;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (c == ':') {
        token_to_fill->type = TOKEN_COLON;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (c == EOF) {
        if (sc->emit_eol_eof) {
          token_to_fill->type = TOKEN_EOL;
          sc->emit_eol_eof = false;
        } else if (sc->indent_depth > 0) {
          sc->indent_depth--;
          token_to_fill->type = TOKEN_DEDENT;
        } else {
          token_to_fill->type = TOKEN_EOF;
        }
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (c == '*') {
        token_to_fill->type = TOKEN_MUL;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (c == '+') {
        token_to_fill->type = TOKEN_PLUS;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (c == '-') {
        token_to_fill->type = TOKEN_MINUS;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else if (isspace(c)) {
        break;
      } else {
        print_error("Unknown character.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_ID:
      if (isalnum(c) || c == '_') {
        APPEND_OR_ERROR(str, c);
        state = STATE_ID;
      } else {
        (void)ungetc(c, sc->src); // for the next token to read
        token_to_fill->type = id_or_keyword(str.data);
        if (token_to_fill->type == TOKEN_ID) {
          token_to_fill->content.string = dynstr_take(&str);
        } else {
          if (token_to_fill->type == TOKEN_BOOL) {
            // Because token is TOKEN_BOOL, its string data must be either
            // "true" or "false"
            if (strcmp(str.data, "true") == 0) {
              token_to_fill->content.boolean = true;
            } else {
              token_to_fill->content.boolean = false;
            }
          }
          dynstr_free(&str);
        }
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      }
      break;

    case STATE_UND:
      if (isalnum(c) || c == '_') {
        APPEND_OR_ERROR(str, c);
        state = STATE_ID;
      } else {
        dynstr_free(&str);
        print_error("Unexpected character after '_'.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_INT:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_INT;
      } else if (c == '_') {
        state = STATE_INT_UNDER;
      } else if (c == '.') {
        APPEND_OR_ERROR(str, c);
        state = STATE_DOT;
      } else if (c == 'e' || c == 'E') {
        APPEND_OR_ERROR(str, 'e');
        state = STATE_E;
      } else {
        if (isalpha(c)) {
          // A unrecognized char after number -> lexical error
          dynstr_free(&str);
          print_error("Unexpected character after a digit.", sc->line);
          return ERR_LEX_ANA;
        }
        (void)ungetc(c, sc->src);
        return int_to_token(&str, token_to_fill, sc->line);
      }
      break;

    case STATE_INT_UNDER:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_INT;
      } else if (c == '_') {
        // do nothing, the amount of '_' is not limited to 1
      } else {
        dynstr_free(&str);
        print_error("Unexpected identifier after '_' in integer.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOT:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_DOUBLE;
      } else {
        dynstr_free(&str);
        print_error("Unexpected indentifier after '.' in a number.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOUBLE:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_DOUBLE;
      } else if (c == '_') {
        state = STATE_DOUBLE_UNDER;
      } else if (c == 'e' || c == 'E') {
        APPEND_OR_ERROR(str, 'e');
        state = STATE_E;
      } else if (c == 'd' || c == 'D') {
        c = fgetc(sc->src);
        if (isalnum(c) || c == '_') {
          // Unrecognized char after double def. -> lexical error
          dynstr_free(&str);
          print_error("Unexpected character after 'd' suffix.", sc->line);
          return ERR_LEX_ANA;
        }
        (void)ungetc(c, sc->src);
        return double_to_token(&str, token_to_fill, sc->line);
      } else {
        if (isalpha(c)) {
          // A unrecognized char after number -> lexical error
          dynstr_free(&str);
          print_error("Unexpected character after a digit.", sc->line);
          return ERR_LEX_ANA;
        }
        (void)ungetc(c, sc->src);
        return double_to_token(&str, token_to_fill, sc->line);
      }
      break;

    case STATE_DOUBLE_UNDER:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_DOUBLE;
      } else if (c == '_') {
        // do nothing, the amount of '_' is not limited to 1
      } else {
        dynstr_free(&str);
        print_error("Unexpected character after '_' in a number.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_E:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_EXP;
      } else if (c == '+' || c == '-') {
        APPEND_OR_ERROR(str, c);
        state = STATE_E_SIGN;
      } else {
        dynstr_free(&str);
        print_error("Missing exponent after 'e'.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_E_SIGN:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_EXP;
      } else {
        dynstr_free(&str);
        print_error("Missing exponent after the exponent sign.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_EXP:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_EXP;
      } else if (c == '_') {
        state = STATE_EXP_UNDER;
      } else if (c == 'd' || c == 'D') {
        c = fgetc(sc->src);
        if (isalnum(c) || c == '_') {
          // Unrecognized char after suffix -> lexical error
          dynstr_free(&str);
          print_error("Unexpected character after 'd' suffix.", sc->line);
          return ERR_LEX_ANA;
        }
        (void)ungetc(c, sc->src);
        return double_to_token(&str, token_to_fill, sc->line);
      } else {
        if (isalpha(c)) {
          // Unrecognized char after suffix -> lexical error
          dynstr_free(&str);
          print_error("Unexpected character after a digit.", sc->line);
          return ERR_LEX_ANA;
        }
        (void)ungetc(c, sc->src);
        return double_to_token(&str, token_to_fill, sc->line);
      }
      break;

    case STATE_EXP_UNDER:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_EXP;
      } else if (c == '_') {
        // do nothing, the amount of '_' is not limited to 1
      } else {
        dynstr_free(&str);
        print_error("Unexpected character after '_' in a number.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOT_START:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_DOUBLE;
      } else {
        (void)ungetc(c, sc->src);
        dynstr_free(&str);
        token_to_fill->type = TOKEN_DOT;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      }
      break;

    case STATE_ZERO:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_ZERO_INT;
      } else if (c == 'e' || c == 'E') {
        APPEND_OR_ERROR(str, c);
        state = STATE_E;
      } else if (c == '.') {
        APPEND_OR_ERROR(str, c);
        state = STATE_DOT;
      } else {
        if (isalpha(c) || c == '_') {
          // A unrecognized char after number -> lexical error
          dynstr_free(&str);
          print_error("Unexpected character after a digit.", sc->line);
          return ERR_LEX_ANA;
        }
        (void)ungetc(c, sc->src);
        return int_to_token(&str, token_to_fill, sc->line);
      }
      break;

    case STATE_ZERO_INT:
      if (isdigit(c)) {
        APPEND_OR_ERROR(str, c);
        state = STATE_ZERO_INT;
      } else if (c == '.') {
        APPEND_OR_ERROR(str, c);
        state = STATE_DOT;
      } else if (c == 'e' || c == 'E') {
        APPEND_OR_ERROR(str, 'e');
        state = STATE_E;
      } else {
        dynstr_free(&str);
        print_error("Integer with a leading zero.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_EXCL:
      if (c == '=') {
        token_to_fill->type = TOKEN_NOT_EQ;
        token_to_fill->line = sc->line;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_EXCL_MARK;
        token_to_fill->line = sc->line;
      }
      return RETURN_SUCCESS;

    case STATE_ASSIGN:
      if (c == '=') {
        token_to_fill->type = TOKEN_IS_EQ;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_ASSIGN;
      }
      token_to_fill->line = sc->line;
      return RETURN_SUCCESS;

    case STATE_GRE:
      if (c == '=') {
        token_to_fill->type = TOKEN_GREEQ;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_GRE;
      }
      token_to_fill->line = sc->line;
      return RETURN_SUCCESS;

    case STATE_AND:
      if (c == '&') {
        token_to_fill->type = TOKEN_AND;
        token_to_fill->line = sc->line;
      } else {
        print_error("Expected '&' after '&'.", sc->line);
        return ERR_LEX_ANA;
      }
      return RETURN_SUCCESS;

    case STATE_OR:
      if (c == '|') {
        token_to_fill->type = TOKEN_OR;
        token_to_fill->line = sc->line;
      } else {
        print_error("Expected '|' after '|'.", sc->line);
        return ERR_LEX_ANA;
      }
      return RETURN_SUCCESS;

    case STATE_LOW:
      if (c == '=') {
        token_to_fill->type = TOKEN_LOWEQ;
      } else if (c == '-') {
        token_to_fill->type = TOKEN_GENERATOR;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_LOW;
      }
      token_to_fill->line = sc->line;
      return RETURN_SUCCESS;

    case STATE_AT:
      if ((c == 'm') && (fgetc(sc->src) == 'a') && (fgetc(sc->src) == 'i') &&
          (fgetc(sc->src) == 'n')) {
        c = fgetc(sc->src);

        // Whitespace, start of commment or EOF are the only allowed
        // chars after @main
        if (isspace(c) || c == '/' || c == EOF) {
          (void)ungetc(c, sc->src);
          token_to_fill->type = TOKEN_AT_MAIN;
          token_to_fill->line = sc->line;
          return RETURN_SUCCESS;
        }
      }
      print_error("Unknown annotation, expected '@main'.", sc->line);
      return ERR_LEX_ANA;

    case STATE_SLASH:
      if (c == '/') {
        state = STATE_COMM_LINE;
      } else if (c == '*') {
        comm_depth++;
        state = STATE_COMM_BLOCK;
      } else if (!must_follow_eol) {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_DIV;
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      } else {
        print_error("Missing end of line.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_COMM_LINE:
      if (c == '\n' || c == EOF) {
        (void)ungetc(c, sc->src);
        state = STATE_IDLE;
      }
      break;

    case STATE_COMM_BLOCK:
      if (c == '\n') {
        sc->line++;
        if (!comm_newline_flag) {
          comm_newline_flag = true;
        }
      } else if (c == '*') {
        state = STATE_COMM_BLOCK_STAR;
      } else if (c == '/') {
        c = fgetc(sc->src);
        if (c == '*') {
          comm_depth++;
        } else {
          (void)ungetc(c, sc->src);
        }
      } else if (c == EOF) {
        print_error("Unterminated block comment.", sc->line);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_COMM_BLOCK_STAR:
      if (c == '*') {
        state = STATE_COMM_BLOCK_STAR;
      } else if (c == '/') {
        comm_depth--;
        if (comm_depth <= 0) {
          if (comm_newline_flag) {
            if (sc->parenth_depth == 0) {
              // outside parentheses, comment acts as line end
              must_follow_eol = true;
            } else {
              // inside parentheses, it's only a whitespace
              token_to_fill->lf_before = true;
            }
          }
          comm_newline_flag = false;
          state = STATE_IDLE;
        } else {
          state = STATE_COMM_BLOCK;
        }
      } else {
        (void)ungetc(c, sc->src); // in case a '\n' follows
        state = STATE_COMM_BLOCK;
      }
      break;

    case STATE_1_QUOT:
      if (c == '"') {
        state = STATE_2_QUOT;
      } else if (c == '\n' || c == EOF) {
        dynstr_free(&str);
        print_error("Unterminated string.", sc->line);
        return ERR_LEX_ANA;
      } else if (c == '\\') {
        c = fgetc(sc->src);
        rc = escape_seq_switch(c, &str, sc->line);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_STR;
      } else {
        APPEND_OR_ERROR(str, c);
        state = STATE_STR;
      }
      break;

    case STATE_2_QUOT:
      if (c == '"') {
        state = STATE_STR_ML;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_STRING;
        token_to_fill->content.string = dynstr_take(&str);
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      }
      break;

    case STATE_STR:
      if (c == '"') {
        token_to_fill->type = TOKEN_STRING;
        token_to_fill->content.string = dynstr_take(&str);
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      }

      if (c == '\n' || c == EOF) {
        dynstr_free(&str);
        print_error("Unterminated string.", sc->line);
        return ERR_LEX_ANA;
      }

      if (c == '\\') {
        c = fgetc(sc->src);
        rc = escape_seq_switch(c, &str, sc->line);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
      } else {
        APPEND_OR_ERROR(str, c);
      }
      break;

    case STATE_STR_ML:
      if (c == '"') {
        state = STATE_STR_ML_1_QUOT;
      } else if (c == EOF) {
        dynstr_free(&str);
        print_error("Unterminated multiline string.", sc->line);
        return ERR_LEX_ANA;
      } else {
        if (c == '\n') {
          sc->line++;
        }
        APPEND_OR_ERROR(str, c);
      }
      break;

    case STATE_STR_ML_1_QUOT:
      if (c == '"') {
        state = STATE_STR_ML_2_QUOT;
      } else if (c == EOF) {
        dynstr_free(&str);
        print_error("Unterminated multiline string.", sc->line);
        return ERR_LEX_ANA;
      } else {
        if (c == '\n') {
          sc->line++;
        }
        APPEND_OR_ERROR(str, '"');
        APPEND_OR_ERROR(str, c);
        state = STATE_STR_ML;
      }
      break;

    case STATE_STR_ML_2_QUOT:
      if (c == '"') {
        while (1) {
          c = fgetc(sc->src);
          if (c == '"') {
            APPEND_OR_ERROR(str, c);
          } else {
            (void)ungetc(c, sc->src);
            break;
          }
        }
        token_to_fill->type = TOKEN_STRING;
        token_to_fill->content.string = dynstr_take(&str);
        token_to_fill->line = sc->line;
        return RETURN_SUCCESS;
      }
      if (c == EOF) {
        dynstr_free(&str);
        print_error("Unterminated multiline string.", sc->line);
        return ERR_LEX_ANA;
      }
      for (int i = 0; i < 2; i++) {
        APPEND_OR_ERROR(str, '"');
      }
      if (c == '\n') {
        sc->line++;
      }
      APPEND_OR_ERROR(str, c);
      state = STATE_STR_ML;

      break;

    case STATE_LINE_START:
      c = count_indent(sc->src, c, &indent_cnt, &tab_appeared);
      if (c == '/') {
        (void)ungetc(c, sc->src);
        indent_pending = true;
        state = STATE_IDLE;
        break;
      }
      if (c == '\n') {
        sc->line++;
        tab_appeared = false;
        indent_cnt = 0;
        break;
      }
      if (c == EOF) {
        (void)ungetc(c, sc->src);
        state = STATE_IDLE;
        break;
      }
      if (tab_appeared) {
        print_error("Tab in indentation.", sc->line);
        return ERR_LEX_ANA;
      }
      // For everything else the indentation matters
      (void)ungetc(c, sc->src);
      state = STATE_IDLE;

      rc = indent_compare(sc, indent_cnt, token_to_fill, &token_ready);
      if (rc != RETURN_SUCCESS) {
        return rc;
      }
      if (token_ready) {
        return RETURN_SUCCESS; // INDENT or DEDENT
      }
      break; // same level, the line continues in IDLE

    case STATE_PAREN_NEWLINE:
      c = count_indent(sc->src, c, &indent_cnt, &tab_appeared);
      if (c == '\n') {
        sc->line++;
        tab_appeared = false;
        indent_cnt = 0;
        break;
      }
      (void)ungetc(c, sc->src);
      state = STATE_IDLE;
      if (c == EOF || c == '/') {
        break; // EOF or a comment, nothing to check
      }
      if (tab_appeared) {
        print_error("Tab in indentation.", sc->line);
        return ERR_LEX_ANA; // tab at the start of a non-empty line
      }
      if (indent_cnt <= sc->indent_levels[sc->indent_depth]) {
        print_error("Not enough indentation inside parentheses.", sc->line);
        return ERR_LEX_ANA;
      }
      break;
    }
  }

  return RETURN_SUCCESS;
}

ReturnCodes scanner_get_token(Scanner *sc, Token *token) {
  return scanner_fsm(token, sc);
}
