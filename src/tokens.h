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
 * @TOKEN_STRING:    String type - Token also uses token_content
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
 * @TOKEN_COMMA:     ","
 * @TOKEN_COLON:     ":"
 * @TOKEN_SEMICOLON: ";"
 * @TOKEN_DOT:       "."
 * @TOKEN_AT_MAIN:   "@main"
 * @TOKEN_EOL:       End of a logical line
 * @TOKEN_INDENT:    Indentation increased
 * @TOKEN_DEDENT:    Indentation decreased
 * @TOKEN_EOF:       End of file
 * @TOKEN_KW_DEF:    "def"
 * @TOKEN_KW_DO:     "do"
 * @TOKEN_KW_DOUBLE: "Double"
 * @TOKEN_KW_ELSE:   "else"
 * @TOKEN_KW_IF:     "if"
 * @TOKEN_KW_IMPORT: "import"
 * @TOKEN_KW_INT:    "Int"
 * @TOKEN_KW_RETURN: "return"
 * @TOKEN_KW_STRING: "String"
 * @TOKEN_KW_THEN:   "then"
 * @TOKEN_KW_UNIT:   "Unit"
 * @TOKEN_KW_VAL:    "val"
 * @TOKEN_KW_VAR:    "var"
 * @TOKEN_KW_WHILE:  "while"
 */
typedef enum TokenType {
  TOKEN_ID,
  TOKEN_INT,
  TOKEN_DOUBLE,
  TOKEN_STRING,
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
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_SEMICOLON,
  TOKEN_DOT,
  TOKEN_AT_MAIN,
  TOKEN_EOL,
  TOKEN_INDENT,
  TOKEN_DEDENT,
  TOKEN_EOF,
  TOKEN_KW_DEF,
  TOKEN_KW_DO,
  TOKEN_KW_DOUBLE,
  TOKEN_KW_ELSE,
  TOKEN_KW_IF,
  TOKEN_KW_IMPORT,
  TOKEN_KW_INT,
  TOKEN_KW_RETURN,
  TOKEN_KW_STRING,
  TOKEN_KW_THEN,
  TOKEN_KW_UNIT,
  TOKEN_KW_VAL,
  TOKEN_KW_VAR,
  TOKEN_KW_WHILE,
} TokenType;

/**
 * struct Token - For Scanner to describe a lexeme and send it to parser
 * @type:           Defining Token's type
 *                  For symbols, it's the only used value
 * @token_content:  Void pointer in case the token has to
 *                  carry content. Typically Int, Double or Char*
 */
typedef struct Token {
  TokenType type;
  void *token_content;
} Token;

#endif