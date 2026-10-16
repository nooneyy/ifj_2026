/**
 * @file scanner.c
 * @brief main scanner file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "scanner.h"
#include "dynstr.h"
#include "return.h"
#include "tokens.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * enum FsmState - listing of all possible FSM states
 *
 * @STATE_IDLE:               idle state - initial state
 * @STATE_LINE_START:         start of a line - indentation scanning state
 * @STATE_ID:                 ID scanning state
 * @STATE_AT:                 "@" is first in the sequence
 * @STATE_UND:                "_" is first in the sequence
 * @STATE_STR:                string scanning state
 * @STATE_STR_ML:             multiline string scanning state
 * @STATE_1_QUOT:             " is first in the sequence
 * @STATE_2_QUOT:             "" is first in the sequence
 * @STATE_STR_ML_1_QUOT:      " in multiline scanning state
 * @STATE_STR_ML_2_QUOT:      "" in multiline scanning state
 * @STATE_INT:                int scanning state
 * @STATE_INT_UNDER:          "_" appeared inside an int
 * @STATE_DOUBLE:             double scanning state
 * @STATE_DOUBLE_UNDER:       "_" appeared inside a double
 * @STATE_DOT:                "." appeared after an int
 * @STATE_DOT_START:          "." is first in the sequence
 * @STATE_E:                  "e" or "E" appeared inside a number
 * @STATE_E_SIGN:             "+" or "-" follows right after "e" or "E"
 * @STATE_EXP:                exponent scanning state
 * @STATE_EXP_UNDER:          "_" appeared inside an exponent
 * @STATE_ZERO:               "0" is first in the sequence
 * @STATE_ZERO_INT:           "0" followed with int number
 * @STATE_SLASH:              "/" is first in the sequence
 * @STATE_COMM_LINE:          line comment scanning state
 * @STATE_COMM_BLOCK:         block comment scanning state
 * @STATE_COMM_BLOCK_STAR:    "*" in block comment scanning state
 * @STATE_ASSIGN:             "=" is first in the sequence
 * @STATE_EXCL:               "!" is first in the sequence
 * @STATE_LOW:                "<" is first in the sequence
 * @STATE_GRE:                ">" is first in the sequence
 */
typedef enum FsmState {
  STATE_IDLE,
  STATE_LINE_START,
  STATE_ID,
  STATE_AT,
  STATE_UND,
  STATE_STR,
  STATE_STR_ML,
  STATE_1_QUOT,
  STATE_2_QUOT,
  STATE_STR_ML_1_QUOT,
  STATE_STR_ML_2_QUOT,
  STATE_INT,
  STATE_INT_UNDER,
  STATE_DOUBLE,
  STATE_DOUBLE_UNDER,
  STATE_DOT,
  STATE_DOT_START,
  STATE_E,
  STATE_E_SIGN,
  STATE_EXP,
  STATE_EXP_UNDER,
  STATE_ZERO,
  STATE_ZERO_INT,
  STATE_SLASH,
  STATE_COMM_LINE,
  STATE_COMM_BLOCK,
  STATE_COMM_BLOCK_STAR,
  STATE_ASSIGN,
  STATE_EXCL,
  STATE_LOW,
  STATE_GRE,
} FsmState;

/**
 * struct KeyWord - For comparing strings from source with keywords
 * @text: keyword as a string for comparison
 * @type: respective token type for the keyword
 */
typedef struct KeyWord {
  const char *text;
  TokenType type;
} KeyWord;

/*
 * KEYWORDS - Initialization of all keywords and it's respective type
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
    {"match", TOKEN_KW_MATCH},     {"type", TOKEN_KW_TYPE}};

Scanner scanner_init(FILE *src_file) {
  Scanner to_fill = {.src = src_file, .at_line_start = 1};
  return to_fill;
}

/**
 * id_or_keyword() - checks whether a string is unique or a keyword
 * @string: string to check
 *
 * Takes the string and compares it with each keyword
 * from KEYWORDS[].

 * Return: Returns TokenType - ID if it's not a keyword
 *                           - KW_* if it is a keyword
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
 * double_ctor() - allocates space for a 64bit float
 * @d: pointer to allocate the memory to
 *
 * Allocates memory using malloc for a 64bit float (double).
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error with allocation
 */
static ReturnCodes double_ctor(double **d) {
  *d = malloc(sizeof(double));
  if (*d == NULL) {
    return ERR_INTERNAL;
  }
  return RETURN_SUCCESS;
}

/**
 * string_to_double() - Converts a string to 64bit float
 * @num_str: string containing the double
 * @d: allocated space for the 64bit float
 *
 * Converting float value inside in a string format in num_str to
 * a 64bit float (double) using strtod function.
 *
 * Return: Returns RETURN_SUCCESS
 */
static ReturnCodes string_to_double(const char *num_str, double *d) {
  *d = strtod(num_str, NULL);
  return RETURN_SUCCESS;
}

/**
 * double_to_token() - Converting a double from DynStr to completed token
 * @str: DynStr containing the double
 * @token: token to fill with type and token_content
 *
 * Lets double_ctor allocate memory for a 64bit float,
 * then lets string_to_double convert value from str
 * to the allocated 64bit float.
 * Finally writes that value into token_content, sets type of token
 * and frees str.
 * str is freed in every case, even in case of an error.
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case one of the function fails
 */
static ReturnCodes double_to_token(DynStr *str, Token *token) {
  ReturnCodes rc = RETURN_SUCCESS;

  double *d = NULL;
  rc = double_ctor(&d);
  if (rc != RETURN_SUCCESS) {
    dynstr_free(str);
    return rc;
  }

  rc = string_to_double(str->data, d);
  if (rc != RETURN_SUCCESS) {
    free(d);
    dynstr_free(str);
    return rc;
  }

  token->type = TOKEN_DOUBLE;
  token->token_content = d;
  d = NULL;
  dynstr_free(str);

  return RETURN_SUCCESS;
}

/**
 * int_ctor() - allocates space for a 64bit integer
 * @i: pointer to allocate the memory to
 *
 * Allocates memory using malloc for a 64bit integer (long long).
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error with allocation
 */
static ReturnCodes int_ctor(long long **i) {
  *i = malloc(sizeof(long long));
  if (*i == NULL) {
    return ERR_INTERNAL;
  }
  return RETURN_SUCCESS;
}

/**
 * string_to_int() - Converts a string to 64bit int
 * @num_str: string containing the int
 * @i: allocated space for the 64bit int
 *
 * Converting int value inside in a string format in num_str to
 * a 64bit int (long long) using strtoll function.
 *
 * Return: Returns RETURN_SUCCESS
 */
static ReturnCodes string_to_int(const char *num_str, long long *i) {
  *i = strtoll(num_str, NULL, 10);
  return RETURN_SUCCESS;
}

/**
 * int_to_token() - Converting an int from DynStr to completed token
 * @str: DynStr containing the int
 * @token: token to fill with type and token_content
 *
 * Lets int_ctor allocate memory for a 64bit int,
 * then lets string_to_int convert value from str
 * to the allocated 64bit int.
 * Finally writes that value into token_content, sets type of token
 * and frees str.
 * str is freed in every case, even in case of an error.
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case one of the function fails
 */
static ReturnCodes int_to_token(DynStr *str, Token *token) {
  ReturnCodes rc = RETURN_SUCCESS;

  long long *i = NULL; // 64bit int because of the IFJ26code int definition
  rc = int_ctor(&i);
  if (rc != RETURN_SUCCESS) {
    dynstr_free(str);
    return rc;
  }

  rc = string_to_int(str->data, i);
  if (rc != RETURN_SUCCESS) {
    free(i);
    dynstr_free(str);
    return rc;
  }

  token->type = TOKEN_INT;
  token->token_content = i;
  dynstr_free(str);

  return RETURN_SUCCESS;
}

/**
 * escape_seq_switch() - escape sequence processing
 * @c:   character that follows '\'
 * @s:   DynStr to store the string to
 *
 * A switch determining whether the character after '\' is part of
 * an escape sequence or not.
 * If so, uses the escape sequence.
 * If not, returns error
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_LEX_ANA in case of an invalid escape sequence
 *         Returns ERR_INTERNAL in case of an internal error
 */
static ReturnCodes escape_seq_switch(const int c, DynStr *s) {
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
    return ERR_LEX_ANA;
  }

  return RETURN_SUCCESS;
}

/**
 * scanner_fsm() - reading source file and creating a token
 * @token_to_fill: token to fill with type and token_content
 * @sc:            scanner struct filled with scanner's current state
 *
 * Reads sc->src char by char and moves through the FSM states
 * until the whole token is read and filled.
 * First char after the token is returned to sc->src
 * by ungetc() for the next token to start with it.
 * For tokens that need token_content (ID, INT, STRING, DOUBLE),
 * str (DynStr) is filled first. ID and STRING hand str's memory
 * over to token_content, INT and DOUBLE convert it to a number.
 * In every other case, str is freed.
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_LEX_ANA in case of a lexical error
 *         Returns ERR_INTERNAL in case of an internal error
 */
static ReturnCodes scanner_fsm(Token *token_to_fill, Scanner *sc) {
  token_to_fill->token_content = NULL; // tokens without content keep NULL

  FsmState state = STATE_IDLE;

  DynStr str = {NULL, 0, 0}; // empty, so dynstr_free() is always safe
  ReturnCodes rc = RETURN_SUCCESS;
  int comm_newline_flag = 0;
  int comm_depth = 0;
  int must_follow_eol = 0;
  int indent_cnt = 0;
  int tab_appeared = 0;
  while (1) {

    int c = fgetc(sc->src);

    switch (state) {
    case STATE_IDLE:
      if (must_follow_eol) {
        if (c != EOF && c != '/' && !isspace(c)) {
          return ERR_LEX_ANA;
        }
      }
      if (sc->at_line_start == 1) {
        (void)ungetc(c, sc->src);
        sc->at_line_start = 0;
        state = STATE_LINE_START;
        break;
      }
      if (sc->dedents_pending != 0) {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_DEDENT;
        sc->dedents_pending--;
        return RETURN_SUCCESS;
      }
      if (tab_appeared == 1) {
        return ERR_LEX_ANA;
      }
      if (isalpha(c)) {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_ID;
      } else if (c == '_') {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_UND;
      } else if (isdigit(c) && c != '0') {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_INT;
      } else if (c == '.') {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }

        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_DOT_START;
      } else if (c == '0') {
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }

        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_ZERO;
      } else if (c == '"') {
        state = STATE_1_QUOT;
        rc = dynstr_init(&str);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
      } else if (c == '\n') {
        if (sc->parenth_depth == 0) {
          token_to_fill->type = TOKEN_EOL;
          sc->at_line_start = 1;
          return RETURN_SUCCESS;
        }
        break;
      } else if (c == '(') {
        sc->parenth_depth++;
        token_to_fill->type = TOKEN_OPEN_PAR;
        return RETURN_SUCCESS;
      } else if (c == ')') {
        if (sc->parenth_depth > 0) {
          sc->parenth_depth--;
        }
        token_to_fill->type = TOKEN_CLOSE_PAR;
        return RETURN_SUCCESS;
      } else if (c == '!') {
        state = STATE_EXCL;
      } else if (c == '=') {
        state = STATE_ASSIGN;
      } else if (c == '>') {
        state = STATE_GRE;
      } else if (c == '<') {
        state = STATE_LOW;
      } else if (c == '@') {
        state = STATE_AT;
      } else if (c == '/') {
        state = STATE_SLASH;
      } else if (c == ';') {
        token_to_fill->type = TOKEN_SEMICOLON;
        return RETURN_SUCCESS;
      } else if (c == ',') {
        token_to_fill->type = TOKEN_COMMA;
        return RETURN_SUCCESS;
      } else if (c == ':') {
        token_to_fill->type = TOKEN_COLON;
        return RETURN_SUCCESS;
      } else if (c == EOF) {
        if (sc->indent_depth > 0) {
          sc->indent_depth--;
          token_to_fill->type = TOKEN_DEDENT;
        } else {
          token_to_fill->type = TOKEN_EOF;
        }
        return RETURN_SUCCESS;
      } else if (c == '*') {
        token_to_fill->type = TOKEN_MUL;
        return RETURN_SUCCESS;
      } else if (c == '+') {
        token_to_fill->type = TOKEN_PLUS;
        return RETURN_SUCCESS;
      } else if (c == '-') {
        token_to_fill->type = TOKEN_MINUS;
        return RETURN_SUCCESS;
      } else if (isspace(c)) {
        break;
      } else {
        return ERR_LEX_ANA;
      }
      break;

    case STATE_ID:
      if (isalnum(c) || c == '_') {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_ID;
      } else {
        (void)ungetc(c, sc->src); // for the next token to read
        token_to_fill->type = id_or_keyword(str.data);
        if (token_to_fill->type != TOKEN_ID) {
          dynstr_free(&str);
        } else {
          token_to_fill->token_content = dynstr_take(&str); // token owns it now
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_UND:
      if (isalnum(c) || c == '_') {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_ID;
      } else {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_INT:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_INT;
      } else if (c == '_') {
        state = STATE_INT_UNDER;
      } else if (c == '.') {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_DOT;
      } else if (c == 'e' || c == 'E') {
        rc = dynstr_append(&str, 'e');
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_E;
      } else {
        (void)ungetc(c, sc->src);
        rc = int_to_token(&str, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_INT_UNDER:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_INT;
      } else {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOT:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_DOUBLE;
      } else {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOUBLE:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_DOUBLE;
      } else if (c == '_') {
        state = STATE_DOUBLE_UNDER;
      } else if (c == 'e' || c == 'E') {
        rc = dynstr_append(&str, 'e');
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_E;
      } else if (c == 'd' || c == 'D') {
        rc = double_to_token(&str, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      } else {
        (void)ungetc(c, sc->src);
        rc = double_to_token(&str, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_DOUBLE_UNDER:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_DOUBLE;
      } else {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_E:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_EXP;
      } else if (c == '+' || c == '-') {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_E_SIGN;
      } else {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_E_SIGN:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_EXP;
      } else {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_EXP:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_EXP;
      } else if (c == '_') {
        state = STATE_EXP_UNDER;
      } else if (c == 'd' || c == 'D') {
        rc = double_to_token(&str, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      } else {
        (void)ungetc(c, sc->src);
        rc = double_to_token(&str, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_EXP_UNDER:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_EXP;
      } else {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOT_START:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_DOUBLE;
      } else {
        (void)ungetc(c, sc->src);
        dynstr_free(&str);
        token_to_fill->type = TOKEN_DOT;
        return RETURN_SUCCESS;
      }
      break;

    case STATE_ZERO:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_ZERO_INT;
      } else if (c == 'e' || c == 'E') {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_E;
      } else if (c == '.') {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_DOT;
      } else {
        (void)ungetc(c, sc->src);
        rc = int_to_token(&str, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_ZERO_INT:
      if (isdigit(c)) {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_ZERO_INT;
      } else if (c == '.') {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_DOT;
      } else if (c == 'e' || c == 'E') {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_E;
      } else {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      }
      break;

    case STATE_EXCL:
      if (c == '=') {
        token_to_fill->type = TOKEN_NOT_EQ;
      } else {
        return ERR_LEX_ANA;
      }
      return RETURN_SUCCESS;
      break;

    case STATE_ASSIGN:
      if (c == '=') {
        token_to_fill->type = TOKEN_IS_EQ;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_ASSIGN;
      }
      return RETURN_SUCCESS;
      break;

    case STATE_GRE:
      if (c == '=') {
        token_to_fill->type = TOKEN_GREEQ;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_GRE;
      }
      return RETURN_SUCCESS;
      break;

    case STATE_LOW:
      if (c == '=') {
        token_to_fill->type = TOKEN_LOWEQ;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_LOW;
      }
      return RETURN_SUCCESS;
      break;

    case STATE_AT:
      if (c == 'm') {
        if (fgetc(sc->src) == 'a') {
          if (fgetc(sc->src) == 'i') {
            if (fgetc(sc->src) == 'n') {
              int after = fgetc(sc->src);

              // Whitespace, start of commment or EOF are the only allowed
              // chars after @main
              if (isspace(after) || after == '/' || after == EOF) {
                (void)ungetc(after, sc->src);
                token_to_fill->type = TOKEN_AT_MAIN;
                return RETURN_SUCCESS;
              }
            }
          }
        }
      }
      return ERR_LEX_ANA;
      break;

    case STATE_SLASH:
      if (c == '/') {
        state = STATE_COMM_LINE;
      } else if (c == '*') {
        comm_depth++;
        state = STATE_COMM_BLOCK;
      } else if (must_follow_eol == 0) {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_DIV;
        return RETURN_SUCCESS;
      } else {
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
      if (c == '\n' && comm_newline_flag != 1) {
        comm_newline_flag = 1;
      } else if (c == '*') {
        state = STATE_COMM_BLOCK_STAR;
      } else if (c == '/') {
        int s = fgetc(sc->src);
        if (s == '*') {
          comm_depth++;
        } else {
          (void)ungetc(s, sc->src);
        }
      } else if (c == EOF) {
        return ERR_LEX_ANA;
      }
      break;

    case STATE_COMM_BLOCK_STAR:
      if (c == '*') {
        state = STATE_COMM_BLOCK_STAR;
      } else if (c == '/') {
        comm_depth--;
        if (comm_depth <= 0) {
          if (comm_newline_flag == 1) {
            must_follow_eol = 1;
          }
          comm_newline_flag = 0;
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
        return ERR_LEX_ANA;
      } else if (c == '\\') {
        int escape = fgetc(sc->src);
        rc = escape_seq_switch(escape, &str);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_STR;
      } else {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_STR;
      }
      break;

    case STATE_2_QUOT:
      if (c == '"') {
        state = STATE_STR_ML;
      } else {
        (void)ungetc(c, sc->src);
        token_to_fill->type = TOKEN_STRING;
        token_to_fill->token_content = dynstr_take(&str);
        return RETURN_SUCCESS;
      }
      break;

    case STATE_STR:
      if (c == '"') {
        token_to_fill->type = TOKEN_STRING;
        token_to_fill->token_content = dynstr_take(&str);
        return RETURN_SUCCESS;
      } else if (c == '\n' || c == EOF) {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      } else if (c == '\\') {
        int escape = fgetc(sc->src);
        rc = escape_seq_switch(escape, &str);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
      } else {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
      }
      break;

    case STATE_STR_ML:
      if (c == '"') {
        state = STATE_STR_ML_1_QUOT;
      } else if (c == EOF) {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      } else {
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
      }
      break;

    case STATE_STR_ML_1_QUOT:
      if (c == '"') {
        state = STATE_STR_ML_2_QUOT;
      } else if (c == EOF) {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      } else {
        rc = dynstr_append(&str, '"');
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_STR_ML;
      }
      break;

    case STATE_STR_ML_2_QUOT:
      if (c == '"') {
        while (1) {
          int quot = fgetc(sc->src);
          if (quot == '"') {
            rc = dynstr_append(&str, quot);
            if (rc != RETURN_SUCCESS) {
              dynstr_free(&str);
              return rc;
            }
          } else {
            (void)ungetc(quot, sc->src);
            break;
          }
        }
        token_to_fill->type = TOKEN_STRING;
        token_to_fill->token_content = dynstr_take(&str);
        return RETURN_SUCCESS;
      } else if (c == EOF) {
        dynstr_free(&str);
        return ERR_LEX_ANA;
      } else {
        for (int i = 0; i < 2; i++) {
          rc = dynstr_append(&str, '"');
          if (rc != RETURN_SUCCESS) {
            dynstr_free(&str);
            return rc;
          }
        }
        rc = dynstr_append(&str, c);
        if (rc != RETURN_SUCCESS) {
          dynstr_free(&str);
          return rc;
        }
        state = STATE_STR_ML;
      }
      break;

    case STATE_LINE_START:
      while (c == ' ' || c == '\t') {
        if (c == '\t') {
          tab_appeared = 1;
        } else {
          indent_cnt++;
        }
        c = fgetc(sc->src);
      }
      if (c == '\n') {
        tab_appeared = 0;
        indent_cnt = 0;
        break;
      }
      if (c == EOF) {
        (void)ungetc(c, sc->src);
        state = STATE_IDLE;
        break;
      }
      if (tab_appeared == 1) {
        return ERR_LEX_ANA;
      }
      // For everything else the indentation matters
      (void)ungetc(c, sc->src);
      state = STATE_IDLE;

      int top = sc->indent_levels[sc->indent_depth];

      if (indent_cnt > top) {
        if (sc->indent_depth + 1 == MAX_INDENT_LEVELS) {
          return ERR_INTERNAL;
        }
        sc->indent_depth++;
        sc->indent_levels[sc->indent_depth] = indent_cnt;
        token_to_fill->type = TOKEN_INDENT;
        return RETURN_SUCCESS;
      }

      if (indent_cnt == top) {
        break;
      }

      // only indent_cnt < top left now

      while (sc->indent_depth > 0 &&
             sc->indent_levels[sc->indent_depth] != indent_cnt) {
        sc->indent_depth--;
        sc->dedents_pending++;
      }
      if (sc->indent_levels[sc->indent_depth] != indent_cnt) {
        // indent_cnt is between 2 open levels and is not open by itself
        return ERR_LEX_ANA;
      }
      sc->dedents_pending--;
      token_to_fill->type = TOKEN_DEDENT;
      return RETURN_SUCCESS;

    default:
      break;
    }
  }

  return RETURN_SUCCESS;
}

ReturnCodes scanner(Scanner *sc, Token *token) {
  return scanner_fsm(token, sc);
}