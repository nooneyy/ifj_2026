/**
 * @file tokens.h
 * @brief Adding Token as a structure
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

/**
 * Creates a token type for each
 * final state in the FSM.
 * Such types are used in Token struct
 * to define it's type
 *
 * @TOKEN_ID:        Identificator type - Token also uses *id_string
 * @TOKEN_INT:       Integer type - Token also uses int_value
 * @TOKEN_DOUBLE:    Double type - Token also uses double_value
 * @TOKEN_ASSIGN:    "="
 * @TOKEN_IS_EQ:     "=="
 * @TOKEN_NOT_EQ:    "!="
 * @TOKEN_MINUS:     "-"
 * @TOKEN_PLUS:      "+"
 * @TOKEN_MUL:       "*"
 * @TOKEN_DIV:       "/"
 * @TOKEN_LOW:       "<"
 * @TOKEN_LOWEQ:     "<="
 * @TOKEN_GRE:       ">"
 * @TOKEN_GREEQ:     ">="
 * @TOKEN_OPEN_PAR:  "("
 * @TOKEN_CLOSE_PAR: ")"
 */
enum TOKEN_TYPE {
  TOKEN_ID,
  TOKEN_INT,
  TOKEN_DOUBLE,
  TOKEN_ASSIGN,
  TOKEN_IS_EQ,
  TOKEN_NOT_EQ,
  TOKEN_MINUS,
  TOKEN_PLUS,
  TOKEN_MUL,
  TOKEN_DIV,
  TOKEN_LOW,
  TOKEN_LOWEQ,
  TOKEN_GRE,
  TOKEN_GREEQ,
  TOKEN_OPEN_PAR,
  TOKEN_CLOSE_PAR,
};

/**
 * struct Token - For Scanner to describe a lexeme and send it to parser
 * @type:           Defining Token's type
 *                  For symbols, it's the only used value
 * @token_content:  Void pointer in case the token has to
 *                  carry content. Typically Int, Double or Char*
 */
typedef struct Token {
  enum TOKEN_TYPE type;
  void *token_content;
} Token;

#endif