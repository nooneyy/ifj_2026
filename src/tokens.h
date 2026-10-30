/**
 * @file tokens.h
 * @brief Adding Token as a structure
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#ifndef TOKENS_H
#define TOKENS_H

#include <stdbool.h>

/**
 * @brief Listing of all possible Token types returned by FSM
 */
typedef enum TokenType {
  TOKEN_ID,           // Identifier - token->content.string has its name
  TOKEN_INT,          // Int literal - token->content.int_val has its value
  TOKEN_DOUBLE,       // Double literal - token->content.dbl_val has its value
  TOKEN_STRING,       // String - token->content.string has its value
  TOKEN_ASSIGN,       // "="
  TOKEN_IS_EQ,        // "=="
  TOKEN_NOT_EQ,       // "!="
  TOKEN_MINUS,        // "-"
  TOKEN_PLUS,         // "+"
  TOKEN_MUL,          // "*"
  TOKEN_DIV,          // "/"
  TOKEN_LOW,          // "<"
  TOKEN_LOWEQ,        // "<="
  TOKEN_GRE,          // ">"
  TOKEN_GREEQ,        // ">="
  TOKEN_OPEN_PAR,     // "("
  TOKEN_CLOSE_PAR,    // ")"
  TOKEN_COMMA,        // ","
  TOKEN_COLON,        // ":"
  TOKEN_SEMICOLON,    // ";"
  TOKEN_DOT,          // "."
  TOKEN_AT_MAIN,      // "@main"
  TOKEN_EOL,          // End of a logical line
  TOKEN_INDENT,       // Indentation increased
  TOKEN_DEDENT,       // Indentation decreased
  TOKEN_EOF,          // End of file
  TOKEN_KW_DEF,       // "def"
  TOKEN_KW_DO,        // "do"
  TOKEN_KW_DOUBLE,    // "Double"
  TOKEN_KW_ELSE,      // "else"
  TOKEN_KW_IF,        // "if"
  TOKEN_KW_IMPORT,    // "import"
  TOKEN_KW_INT,       // "Int"
  TOKEN_KW_RETURN,    // "return"
  TOKEN_KW_STRING,    // "String"
  TOKEN_KW_THEN,      // "then"
  TOKEN_KW_UNIT,      // "Unit"
  TOKEN_KW_VAL,       // "val"
  TOKEN_KW_VAR,       // "var"
  TOKEN_KW_WHILE,     // "while"
  TOKEN_KW_BOOLEAN,   // "Boolean"
  TOKEN_KW_CASE,      // "case"
  TOKEN_KW_CLASS,     // "class"
  TOKEN_KW_EXTENSION, // "extension"
  TOKEN_KW_FOR,       // "for"
  TOKEN_KW_LIST,      // "List"
  TOKEN_KW_MATCH,     // "match"
  TOKEN_KW_TYPE,      // "type"
} TokenType;

/**
 * @brief A union describing the possible content of a token
 *
 * content.string: If type == TOKEN_STRING or TOKEN_ID, contains a pointer to
 * the string, owned by the token, that has to be freed by token_free(),
 * null-terminated, escape sequences already decoded
 */
typedef union TokenContent {
  char *string;      // if type == TOKEN_STRING or TOKEN_ID
  long long int_val; // if type == TOKEN_INT
  double dbl_val;    // if type == TOKEN_DOUBLE
} TokenContent;

/**
 * @brief For Scanner to describe a lexeme and send it to parser
 */
typedef struct Token {
  TokenType type; // Defining Token's type
                  // For symbols, it's the only used value
  bool lf_before; // True if a newline was skipped before this token (used for
                  // trailing commas)
  TokenContent content; // Content of the token (see TokenContent union)
  unsigned int line;    // Line the token is located on
} Token;

/**
 * @brief Frees the string content of a token and sets it to NULL
 *
 * Frees token->content.string and sets it to NULL,
 * so calling it more than once is safe.
 * Safe to call for tokens without a string content as well.
 * Has to be called by the owner of the token
 * once its content is no longer needed.
 *
 * @param token Token whose content to free
 */
void token_free(Token *token);

#endif
