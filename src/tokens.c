/**
 * @file tokens.c
 * @brief Token helper functions
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 16.10.2026
 */

#include "tokens.h"
#include <stdlib.h>

void token_free(Token *token) {
  free(token->token_content);
  token->token_content = NULL;
}
