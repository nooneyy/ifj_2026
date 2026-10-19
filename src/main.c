/**
 * @file main.c
 * @brief compiler's main
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#include "return.h"
#include "scanner.h"

int main(void) {
  Scanner sc = scanner_init(stdin);
  (void)sc; // TODO: hand sc over to the parser, it asks the scanner for tokens

  return RETURN_SUCCESS;
}
