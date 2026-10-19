/**
 * @file tokens.h
 * @brief Adding Token as a structure
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>

/**
 * enum TokenType - listing of all possible Token types returned by FSM
 *
 * @TOKEN_ID:           Identifier - token_content is char*, null-terminated
 * @TOKEN_INT:          Int literal - token_content is long long*
 * @TOKEN_DOUBLE:       Double literal - token_content is double*
 * @TOKEN_STRING:       String - token_content is char*, null-terminated
 * @TOKEN_ASSIGN:       "="
 * @TOKEN_IS_EQ:        "=="
 * @TOKEN_NOT_EQ:       "!="
 * @TOKEN_MINUS:        "-"
 * @TOKEN_PLUS:         "+"
 * @TOKEN_MUL:          "*"
 * @TOKEN_DIV:          "/"
 * @TOKEN_LOW:          "<"
 * @TOKEN_LOWEQ:        "<="
 * @TOKEN_GRE:          ">"
 * @TOKEN_GREEQ:        ">="
 * @TOKEN_OPEN_PAR:     "("
 * @TOKEN_CLOSE_PAR:    ")"
 * @TOKEN_COMMA:        ","
 * @TOKEN_COLON:        ":"
 * @TOKEN_SEMICOLON:    ";"
 * @TOKEN_DOT:          "."
 * @TOKEN_AT_MAIN:      "@main"
 * @TOKEN_EOL:          End of a logical line
 * @TOKEN_INDENT:       Indentation increased
 * @TOKEN_DEDENT:       Indentation decreased
 * @TOKEN_EOF:          End of file
 * @TOKEN_KW_DEF:       "def"
 * @TOKEN_KW_DO:        "do"
 * @TOKEN_KW_DOUBLE:    "Double"
 * @TOKEN_KW_ELSE:      "else"
 * @TOKEN_KW_IF:        "if"
 * @TOKEN_KW_IMPORT:    "import"
 * @TOKEN_KW_INT:       "Int"
 * @TOKEN_KW_RETURN:    "return"
 * @TOKEN_KW_STRING:    "String"
 * @TOKEN_KW_THEN:      "then"
 * @TOKEN_KW_UNIT:      "Unit"
 * @TOKEN_KW_VAL:       "val"
 * @TOKEN_KW_VAR:       "var"
 * @TOKEN_KW_WHILE:     "while"
 * @TOKEN_KW_BOOLEAN:   "Boolean"
 * @TOKEN_KW_CASE:      "case"
 * @TOKEN_KW_CLASS:     "class"
 * @TOKEN_KW_EXTENSION: "extension"
 * @TOKEN_KW_FOR:       "for"
 * @TOKEN_KW_LIST:      "List"
 * @TOKEN_KW_MATCH:     "match"
 * @TOKEN_KW_TYPE:      "type"
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
  TOKEN_KW_BOOLEAN,
  TOKEN_KW_CASE,
  TOKEN_KW_CLASS,
  TOKEN_KW_EXTENSION,
  TOKEN_KW_FOR,
  TOKEN_KW_LIST,
  TOKEN_KW_MATCH,
  TOKEN_KW_TYPE
} TokenType;

/**
 * struct Token - For Scanner to describe a lexeme and send it to parser
 * @type:           Defining Token's type
 *                  For symbols, it's the only used value
 *
 * @lf_before:      true if a newline was skipped before this token (used for
 *                  trailing commas)
 *
 * @token_content:  Content of the token, NULL if it has none
 *                  ID, STRING: char*, null-terminated string
 *                  INT: long long*, DOUBLE: double*
 *                  STRING has its escape sequences already decoded
 *                  Owned by the token, freed by token_free()
 */
typedef struct Token {
  TokenType type;
  bool lf_before;
  void *token_content;
} Token;

/**
 * token_free() - freeing token's content
 * @token: token whose content to free
 *
 * Frees token_content and sets it to NULL,
 * so calling it more than once is safe.
 * Safe to call for tokens without content as well.
 * Has to be called by the owner of the token
 * once its content is no longer needed.
 */
void token_free(Token *token);

#endif