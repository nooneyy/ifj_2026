/**
 * @file scanner.c
 * @brief main scanner file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "return.h"
#include "tokens.h"
#include <stdio.h>
#include <stdlib.h>

typedef enum FsmState {

  STATE_IDLE,
  STATE_LINE_START,
  STATE_ID,
  STATE_AT,
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

static ReturnCodes scanner_fsm(Token *token_to_fill, FILE *src) {

  (void)token_to_fill;

  FsmState state = STATE_IDLE;

  while (1) {

    int c = fgetc(src);
    (void)c;

    switch (state) {
    case STATE_IDLE:
      break;

    case STATE_LINE_START:
      break;

    case STATE_ID:
      break;

    case STATE_AT:
      break;

    case STATE_STR:
      break;

    case STATE_STR_ESC:
      break;

    case STATE_STR_ML:
      break;

    case STATE_1_QUOT:
      break;

    case STATE_2_QUOT:
      break;

    case STATE_STR_ML_1_QUOT:
      break;

    case STATE_STR_ML_2_QUOT:
      break;

    case STATE_INT:
      break;

    case STATE_INT_UNDER:
      break;

    case STATE_DOUBLE:
      break;

    case STATE_DOUBLE_UNDER:
      break;

    case STATE_DOT:
      break;

    case STATE_DOT_START:
      break;

    case STATE_E:
      break;

    case STATE_E_SIGN:
      break;

    case STATE_EXP:
      break;

    case STATE_EXP_UNDER:
      break;

    case STATE_ZERO:
      break;

    case STATE_SLASH:
      break;

    case STATE_COMM_LINE:
      break;

    case STATE_COMM_BLOCK:
      break;

    case STATE_COMM_BLOCK_STAR:
      break;

    case STATE_COMM_BLOCK_SLASH:
      break;

    case STATE_ASSIGN:
      break;

    case STATE_EXCL:
      break;

    case STATE_LOW:
      break;

    case STATE_GRE:
      break;
    }
  }

  return RETURN_SUCCESS;
}