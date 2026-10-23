/**
 * @file tokens.c
 * @brief Token helper functions
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 16.10.2026
 */

#include "tokens.h"
#include <stdlib.h>

void token_free(Token *token) {
  if (token->type == TOKEN_STRING || token->type == TOKEN_ID) {
    free(token->content.string);
  }

  token->content.string = NULL;
}
