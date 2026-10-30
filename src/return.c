#include "return.h"
#include <stdio.h>

void print_error(const char *err_message, unsigned int line) {
  if (line == 0) {
    (void)fprintf(stderr, "INTERNAL ERROR: %s\n", err_message);
  } else {
    (void)fprintf(stderr, "ERROR on line %u: %s\n", line, err_message);
  }
}
