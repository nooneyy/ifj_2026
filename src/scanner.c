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

static const unsigned int DEFAULT_BUFFER_SIZE = 31;
static unsigned int ACC_BUFFER_SIZE = DEFAULT_BUFFER_SIZE;

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
static FILE *open_src_file(char *file_name);

static ReturnCodes scanner_fsm(Token *token_to_fill, FILE *src);

Token scanner(char *file_name) {
  FILE *src_file = open_src_file(file_name);
  (void)src_file;
  Token test_token = {TOKEN_ID, "test_string"};

  ReturnCodes fsm_status = scanner_fsm(&test_token, src_file);
  (void)fsm_status;

  (void)fclose(src_file);
  return test_token;
}

static FILE *open_src_file(char *file_name) {
  FILE *file;
  file = fopen(file_name, "r");
  if (file == NULL) {
    (void)fprintf(stderr, "Couldn't open source file.\n");
    exit(99);
  }

  return file;
}

static ReturnCodes token_content_ctor(char **buf) {
  ACC_BUFFER_SIZE = DEFAULT_BUFFER_SIZE;
  *buf = malloc(DEFAULT_BUFFER_SIZE + 1);
  if (*buf == NULL) {
    return ERR_INTERNAL;
  }
  return RETURN_SUCCESS;
}

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

static ReturnCodes scanner_fsm(Token *token_to_fill, FILE *src) {

  (void)token_to_fill;

  FsmState state = STATE_IDLE;

  char *buf = NULL;
  unsigned int buf_idx = 0;
  (void)buf_idx;
  (void)buffer_append;

  ReturnCodes rc = token_content_ctor(&buf);
  if (rc != RETURN_SUCCESS) {
    return rc;
  }

  while (1) {

    int c = fgetc(src);
    (void)c;

    switch (state) {
    case STATE_IDLE:
      if (isalpha(c) != 0) {
        state = STATE_ID;
      } else if (c == '_') {
        state = STATE_UND;
      }
      break;

    case STATE_ID:

    default:
      break;
    }
  }

  return RETURN_SUCCESS;
}