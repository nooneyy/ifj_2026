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
#include <stdlib.h>

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

  Scanner sc = scanner_init(src);

  Token token;
  do {
    rc = scanner(&sc, &token);
    if (rc != RETURN_SUCCESS) {
      (void)fprintf(stderr, "Scanner failed\n");
      break;
    }

    if (token.type == TOKEN_ID) {
      (void)printf("ID     %s\n", (char *)token.token_content);
    } else if (token.type == TOKEN_INT) {
      (void)printf("INT   %lld\n", *(long long *)token.token_content);
    } else if (token.type == TOKEN_DOUBLE) {
      (void)printf("DBL   %g\n", *(double *)token.token_content);
    } else {
      (void)printf("type  %d\n", token.type);
    }

    free(token.token_content);
  } while (token.type != TOKEN_EOF);

  (void)fclose(src);
  return rc;
}
