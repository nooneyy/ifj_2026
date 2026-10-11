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
 * ACC_BUFFER_SIZE - Actual buffer size - 1
 *
 * Used to define how many bytes in the buffer
 * can be filled.
 * Last byte reserved for '\0'
 * On reallocation ACC_BUFFER_SIZE = (ACC_BUFFER_SIZE * 2) + 1
 * Reset to DEFAULT_BUFFER_SIZE in token_content_ctor()
 */
static unsigned int ACC_BUFFER_SIZE = DEFAULT_BUFFER_SIZE;

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
 * open_src_file() - Opening source file to compile
 * @file_name: Name of the source file to compile,
 *             typically from argv
 *
 * Uses fopen() to open source_file for READING.
 *
 * Return: Returns pointer to the source file
 *         In case of an error, exits with errcode 99
 */
static FILE *open_src_file(char *file_name) {
  FILE *file;
  file = fopen(file_name, "r");
  if (file == NULL) {
    (void)fprintf(stderr, "Couldn't open source file.\n");
    exit(99);
  }

  return file;
}

/**
 * token_content_ctor() - allocating buffer to store token_content in
 * @buf: pointer to buffer to allocate
 *
 * Using malloc to allocate DEFAULT_BUFFER_SIZE + 1 bytes
 * for a buf buffer. Memory is later handed to Token.token_content
 * Resets ACC_BUFFER_SIZE to DEFAULT_BUFFER_SIZE
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error
 */
static ReturnCodes token_content_ctor(char **buf) {
  ACC_BUFFER_SIZE = DEFAULT_BUFFER_SIZE;
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
 * Updates ACC_BUFFER_SIZE accordingly.
 * In case of an error, buf is freed and set to NULL.
 * Memory is later handed to Token.token_content
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error
 */
static ReturnCodes token_content_realloc(char **buf) {
  ACC_BUFFER_SIZE = (ACC_BUFFER_SIZE * 2) + 1;
  char *ptr = realloc(*buf, sizeof(char) * (ACC_BUFFER_SIZE + 1));
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
  if (*buf_idx > ACC_BUFFER_SIZE - 1) {
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
  FsmState state = STATE_IDLE;

  char *buf = NULL;
  unsigned int buf_idx = 0;

  ReturnCodes rc = token_content_ctor(&buf);
  if (rc != RETURN_SUCCESS) {
    return rc;
  }

  while (1) {

    int c = fgetc(src);

    switch (state) {
    case STATE_IDLE:
      if (isalpha(c) != 0) {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_ID;
      } else if (c == '_') {
        rc = buffer_append(c, &buf, &buf_idx);
        if (rc != RETURN_SUCCESS) {
          return rc;
        }
        state = STATE_UND;
      }
      break;

    case STATE_ID:
      if (isalnum(c) != 0 || c == '_') {
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
      if (isalnum(c) != 0 || c == '_') {
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

    default:
      break;
    }
  }

  return RETURN_SUCCESS;
}

Token scanner(char *file_name) {
  FILE *src_file = open_src_file(file_name);
  (void)src_file;
  Token test_token = {TOKEN_ID, "test_string"};

  ReturnCodes fsm_status = scanner_fsm(&test_token, src_file);
  (void)fsm_status;

  (void)fclose(src_file);
  return test_token;
}