/**
 * @file main.c
 * @brief compiler's main
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "return.h"
#include "scanner.h"
#include "tokens.h"
#include <stdio.h>

/**
 * open_src_file() - Opening source file to compile
 * @file_name: Name of the source file to compile,
 *             typically from argv
 * @file:      pointer to the FILE pointer to fill
 *
 * Uses fopen() to open file_name for READING
 * and stores the opened file into *file.
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL if the file couldn't be opened
 */
static ReturnCodes open_src_file(char *file_name, FILE **file) {
  *file = fopen(file_name, "r");
  if (*file == NULL) {
    (void)fprintf(stderr, "Couldn't open source file.\n");
    return ERR_INTERNAL;
  }

  return RETURN_SUCCESS;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    (void)fprintf(stderr, "Usage: %s <source file>\n", argv[0]);
    return ERR_INTERNAL;
  }

  FILE *src = NULL;
  ReturnCodes rc = open_src_file(argv[1], &src);
  if (rc != RETURN_SUCCESS) {
    return rc;
  }

  Token test_token = scanner(src);
  (void)test_token;

  (void)fclose(src);
  return rc;
}
