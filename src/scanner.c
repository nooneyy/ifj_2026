/**
 * @file scanner.c
 * @brief main scanner file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "scanner.h"
#include "return.h"
#include "tokens.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * DEFAULT_BUFFER_SIZE - Initial buffer size - 1
 *
 * Used for initial buffer allocation.
 * -1 because allocation adds 1 byte for '\0'
 * so allocations are powers of 2 (32, 64, 128...)
 */
static const unsigned int DEFAULT_BUFFER_SIZE = 31;

/*
 * acc_buffer_size - Actual buffer size - 1
 *
 * Used to define how many bytes in the buffer
 * can be filled.
 * Last byte reserved for '\0'
 * On reallocation acc_buffer_size = (acc_buffer_size * 2) + 1
 * Reset to DEFAULT_BUFFER_SIZE in token_content_ctor()
 */
static unsigned int acc_buffer_size = DEFAULT_BUFFER_SIZE;

/**
 * enum FsmState - listing of all possible FSM states
 *
 * @STATE_IDLE:               idle state - initial state
 * @STATE_LINE_START:         start of a line - indentation scanning state
 * @STATE_ID:                 ID scanning state
 * @STATE_AT:                 "@" is first in the sequence
 * @STATE_UND:                "_" is first in the sequence
 * @STATE_STR:                string scanning state
 * @STATE_STR_ESC:            string escape sequence scanning state
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
 * @STATE_COMM_BLOCK_SLASH:   "/" in block comment scanning state
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
  STATE_STR_ESC,
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
  STATE_COMM_BLOCK_SLASH,
  STATE_ASSIGN,
  STATE_EXCL,
  STATE_LOW,
  STATE_GRE,
} FsmState;

/**
 * token_content_ctor() - allocating buffer to store token_content in
 * @buf: pointer to buffer to allocate
 *
 * Using malloc to allocate DEFAULT_BUFFER_SIZE + 1 bytes
 * for a buf buffer. Memory is later handed to Token.token_content
 * Resets acc_buffer_size to DEFAULT_BUFFER_SIZE
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error
 */
static ReturnCodes token_content_ctor(char **buf) {
  acc_buffer_size = DEFAULT_BUFFER_SIZE;
  *buf = malloc(DEFAULT_BUFFER_SIZE + 1);
  if (*buf == NULL) {
    return ERR_INTERNAL;
  }
  return RETURN_SUCCESS;
}

/**
 * token_content_realloc() - reallocating buffer to double the bytes
 * @buf: pointer to buffer to double
 *
 * Using realloc to double buf size.
 * Updates acc_buffer_size accordingly.
 * In case of an error, buf is freed and set to NULL.
 * Memory is later handed to Token.token_content
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error
 */
static ReturnCodes token_content_realloc(char **buf) {
  acc_buffer_size = (acc_buffer_size * 2) + 1;
  char *ptr = realloc(*buf, sizeof(char) * (acc_buffer_size + 1));
  if (ptr == NULL) {
    free(*buf);
    *buf = NULL;
    return ERR_INTERNAL;
  }
  *buf = ptr;
  return RETURN_SUCCESS;
}

/**
 * buffer_append() - appending a char into buf
 * @c:       char to append
 * @buf:     buffer to append to
 * @buf_idx: index to write to, incremented after writing
 *
 * Appending char c into buffer.
 * buf_idx is the index inside buffer to write c to.
 * If buf_idx is out of boundaries of the buffer,
 * calls token_content_realloc().
 * '\0' is not appended, caller adds it at the end of a token
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error
 */
static ReturnCodes buffer_append(int c, char **buf, unsigned int *buf_idx) {
  int rc = RETURN_SUCCESS;
  if (*buf_idx > acc_buffer_size - 1) {
    rc = token_content_realloc(buf);
    if (rc != RETURN_SUCCESS) {
      return ERR_INTERNAL;
    }
  }
  (*buf)[*buf_idx] = (char)c;
  (*buf_idx)++;
  return RETURN_SUCCESS;
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
 * string_to_double() - Converts a string in buffer to 64bit float
 * @buf: buffer containing the double in string format
 * @d: allocated space for the 64bit float
 *
 * Converting float value inside in a string format in buf to
 * a 64bit float (double) using strtod function.
 *
 * Return: Returns RETURN_SUCCESS
 */
static ReturnCodes string_to_double(char **buf, double *d) {
  *d = strtod(*buf, NULL);
  return RETURN_SUCCESS;
}

/**
 * double_to_token() - Converting a double from buffer to completed token
 * @buf: buffer containing the double
 * @buf_idx: index to the first item after double in buf
 * @token: token to fill with type and token_content
 *
 * Appends the buffer with '\0', then lets double_ctor allocate
 * memory for a 64bit float, then lets string_to_double convert value from
 * buf to the allocated 64bit float.
 * Finally writes that value into token_content, sets type of token
 * and frees buffer.
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case one of the function fails
 */
static ReturnCodes double_to_token(char **buf, unsigned int *buf_idx,
                                   Token *token) {
  ReturnCodes rc = RETURN_SUCCESS;
  rc = buffer_append('\0', buf, buf_idx);
  if (rc != RETURN_SUCCESS) {
    return rc;
  }

  double *d = NULL;
  rc = double_ctor(&d);
  if (rc != RETURN_SUCCESS) {
    free(*buf);
    *buf = NULL;
    return rc;
  }

  rc = string_to_double(buf, d);
  if (rc != RETURN_SUCCESS) {
    free(d);
    free(*buf);
    *buf = NULL;
    return rc;
  }

  token->type = TOKEN_DOUBLE;
  token->token_content = d;
  d = NULL;
  free(*buf);
  *buf = NULL;

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
 * string_to_int() - Converts a string in buffer to 64bit int
 * @buf: buffer containing the int in string format
 * @i: allocated space for the 64bit int
 *
 * Converting int value inside in a string format in buf to
 * a 64bit int (long long) using strtoll function.
 *
 * Return: Returns RETURN_SUCCESS
 */
static ReturnCodes string_to_int(char **buf, long long *i) {
  *i = strtoll(*buf, NULL, 10);
  return RETURN_SUCCESS;
}

/**
 * int_to_token() - Converting an int from buffer to completed token
 * @buf: buffer containing the int
 * @buf_idx: index to the first item after int in buf
 * @token: token to fill with type and token_content
 *
 * Appends the buffer with '\0', then lets int_ctor allocate
 * memory for a 64bit int, then lets string_to_int convert value from
 * buf to the allocated 64bit int.
 * Finally writes that value into token_content, sets type of token
 * and frees buffer.
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case one of the function fails
 */
static ReturnCodes int_to_token(char **buf, unsigned int *buf_idx,
                                Token *token) {
  ReturnCodes rc = RETURN_SUCCESS;
  rc = buffer_append('\0', buf, buf_idx);
  if (rc != RETURN_SUCCESS) {
    return rc;
  }

  long long *i = NULL; // 64bit int because of the IFJ26code int definition
  rc = int_ctor(&i);
  if (rc != RETURN_SUCCESS) {
    free(*buf);
    *buf = NULL;
    return rc;
  }

  rc = string_to_int(buf, i);
  if (rc != RETURN_SUCCESS) {
    free(i);
    free(*buf);
    *buf = NULL;
    return rc;
  }

  token->type = TOKEN_INT;
  token->token_content = i;
  free(*buf);
  *buf = NULL;

  return RETURN_SUCCESS;
}

/**
 * scanner_fsm() - reading source file and creating a token
 * @token_to_fill: token to fill with type and token_content
 * @src:           source file to read from
 *
 * Reads src char by char and moves through the FSM states
 * until the whole token is read and filled.
 * First char after the token is returned to src
 * by ungetc() for the next token to start with it.
 * For tokens that need token_content (ID, INT, STRING, DOUBLE),
 * the buffer is filled and then the memory is handed over
 * to token_content. For other tokens, the buffer is freed.
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_LEX_ANA in case of a lexical error
 *         Returns ERR_INTERNAL in case of an internal error
 */
static ReturnCodes scanner_fsm(Token *token_to_fill, FILE *src) {
  token_to_fill->token_content = NULL; // tokens without content keep NULL

  FsmState state = STATE_IDLE;

  char *buf = NULL;
  unsigned int buf_idx = 0;
  ReturnCodes rc = RETURN_SUCCESS;

  while (1) {

    int c = fgetc(src);

    switch (state) {
    case STATE_IDLE:
      if (isalpha(c)) {
        rc = token_content_ctor(&buf);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_ID;
      } else if (c == '_') {
        rc = token_content_ctor(&buf);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_UND;
      } else if (isdigit(c) && c != '0') {
        rc = token_content_ctor(&buf);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_INT;
      } else if (c == '.') {
        rc = token_content_ctor(&buf);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }

        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_DOT_START;
      } else if (c == '0') {
        rc = token_content_ctor(&buf);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }

        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_ZERO;
      } else if (c == ';') {
        token_to_fill->type = TOKEN_SEMICOLON;
        return RETURN_SUCCESS;
      } else if (c == '(') {
        token_to_fill->type = TOKEN_OPEN_PAR;
        return RETURN_SUCCESS;
      } else if (c == ')') {
        token_to_fill->type = TOKEN_CLOSE_PAR;
        return RETURN_SUCCESS;
      } else if (c == ',') {
        token_to_fill->type = TOKEN_COMMA;
        return RETURN_SUCCESS;
      } else if (c == ':') {
        token_to_fill->type = TOKEN_COLON;
        return RETURN_SUCCESS;
      } else if (c == EOF) {
        token_to_fill->type = TOKEN_EOF;
        return RETURN_SUCCESS;
      } else if (c == '\n') {
        token_to_fill->type = TOKEN_EOL;
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
      }
      break;

    case STATE_ID:
      if (isalnum(c) || c == '_') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_ID;
      } else {
        (void)ungetc(c, src); // for the next token to read
        buf[buf_idx] = '\0';
        /*
         * TODO: send to symtable to check if it's a keyword
         * if so, handle correctly
         * else{
         */
        token_to_fill->type = TOKEN_ID;
        token_to_fill->token_content = buf;
        buf = NULL;
        return RETURN_SUCCESS;
      }
      break;

    case STATE_UND:
      if (isalnum(c) || c == '_') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_ID;
      } else {
        free(buf);
        buf = NULL;
        return ERR_LEX_ANA;
      }
      break;

    case STATE_INT:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_INT;
      } else if (c == '_') {
        state = STATE_INT_UNDER;
      } else if (c == '.') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_DOT;
      } else if (c == 'e' || c == 'E') {
        rc = buffer_append('e', &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_E;
      } else {
        (void)ungetc(c, src);
        rc = int_to_token(&buf, &buf_idx, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_INT_UNDER:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_INT;
      } else {
        free(buf);
        buf = NULL;
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOT:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_DOUBLE;
      } else {
        free(buf);
        buf = NULL;
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOUBLE:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_DOUBLE;
      } else if (c == '_') {
        state = STATE_DOUBLE_UNDER;
      } else if (c == 'e' || c == 'E') {
        rc = buffer_append('e', &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_E;
      } else if (c == 'd' || c == 'D') {
        rc = double_to_token(&buf, &buf_idx, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      } else {
        (void)ungetc(c, src);
        rc = double_to_token(&buf, &buf_idx, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_DOUBLE_UNDER:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_DOUBLE;
      } else {
        free(buf);
        buf = NULL;
        return ERR_LEX_ANA;
      }
      break;

    case STATE_E:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_EXP;
      } else if (c == '+' || c == '-') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_E_SIGN;
      } else {
        free(buf);
        buf = NULL;
        return ERR_LEX_ANA;
      }
      break;

    case STATE_E_SIGN:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_EXP;
      } else {
        free(buf);
        buf = NULL;
        return ERR_LEX_ANA;
      }
      break;

    case STATE_EXP:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_EXP;
      } else if (c == '_') {
        state = STATE_EXP_UNDER;
      } else if (c == 'd' || c == 'D') {
        rc = double_to_token(&buf, &buf_idx, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      } else {
        (void)ungetc(c, src);
        rc = double_to_token(&buf, &buf_idx, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_EXP_UNDER:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_EXP;
      } else {
        free(buf);
        buf = NULL;
        return ERR_LEX_ANA;
      }
      break;

    case STATE_DOT_START:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_DOUBLE;
      } else {
        (void)ungetc(c, src);
        free(buf);
        buf = NULL;
        token_to_fill->type = TOKEN_DOT;
        return RETURN_SUCCESS;
      }
      break;

    case STATE_ZERO:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_ZERO_INT;
      } else if (c == 'e' || c == 'E') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_E;
      } else if (c == '.') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_DOT;
      } else {
        (void)ungetc(c, src);
        rc = int_to_token(&buf, &buf_idx, token_to_fill);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        return RETURN_SUCCESS;
      }
      break;

    case STATE_ZERO_INT:
      if (isdigit(c)) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_ZERO_INT;
      } else if (c == '.') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_DOT;
      } else if (c == 'e' || c == 'E') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_E;
      } else {
        free(buf);
        buf = NULL;
        return ERR_LEX_ANA;
      }
      break;

    default:
      break;
    }
  }

  return RETURN_SUCCESS;
}

Token scanner(FILE *src_file) {
  Token test_token = {TOKEN_ID, "test_string"};

  ReturnCodes fsm_status = scanner_fsm(&test_token, src_file);
  (void)fsm_status;

  return test_token;
}