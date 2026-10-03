/**
 * @file main.c
 * @brief compiler's main
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "scanner.h"
#include "tokens.h"

int main(int argc, char **argv) {
  (void)argc;
  Token test_token = scanner(argv[1]);
  (void)test_token;
  return 0;
}
