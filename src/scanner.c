/**
 * @file scanner.c
 * @brief main scanner file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "tokens.h"
#include <stdio.h>
#include <stdlib.h>

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

Token scanner(char *file_name) {
  FILE *src_file = open_src_file(file_name);
  (void)src_file;
  Token test_token = {TOKEN_ID, "String", NULL, NULL};

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