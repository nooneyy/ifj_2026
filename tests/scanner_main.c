/**
 * @file scanner_main.c
 * @brief Testing main for the scanner, prints every token of a source file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 16.10.2026
 */

/**
 * Disclaimer!
 * This testing file was made by Claude Opus 5.5 AI model in xHigh effort mode
 * This file is only for testing purposes.
 *
 * Prompt:
 * "Make a testing main for the scanner that initializes the scanner struct with
 * a file from argv[1], in a loop requests tokens from scanner until TOKEN_EOF
 * is returned, prints each token (with it's value if that's the case) the
 * scanner produces and returns with RETURN_SUCCESS if there wasnt an error
 * returned by the scanner. If the scanner returns an error, it skips everything
 * and immediately returns the ReturnCode it's given by scanner."
 */

#include "../src/return.h"
#include "../src/scanner.h"
#include "../src/tokens.h"
#include <stdio.h>

/*
 * TOKEN_NAMES - printable names of all token types
 *
 * Indexed by TokenType, used by print_token().
 * A type missing here is printed as UNKNOWN.
 */
static const char *const TOKEN_NAMES[] = {
    [TOKEN_ID] = "ID",
    [TOKEN_INT] = "INT",
    [TOKEN_DOUBLE] = "DOUBLE",
    [TOKEN_STRING] = "STRING",
    [TOKEN_BOOL] = "BOOL",
    [TOKEN_ASSIGN] = "ASSIGN",
    [TOKEN_IS_EQ] = "IS_EQ",
    [TOKEN_NOT_EQ] = "NOT_EQ",
    [TOKEN_EXCL_MARK] = "EXCL_MARK",
    [TOKEN_MINUS] = "MINUS",
    [TOKEN_PLUS] = "PLUS",
    [TOKEN_MUL] = "MUL",
    [TOKEN_DIV] = "DIV",
    [TOKEN_LOW] = "LOW",
    [TOKEN_LOWEQ] = "LOWEQ",
    [TOKEN_GENERATOR] = "GENERATOR",
    [TOKEN_GRE] = "GRE",
    [TOKEN_GREEQ] = "GREEQ",
    [TOKEN_AND] = "AND",
    [TOKEN_OR] = "OR",
    [TOKEN_OPEN_PAR] = "OPEN_PAR",
    [TOKEN_CLOSE_PAR] = "CLOSE_PAR",
    [TOKEN_COMMA] = "COMMA",
    [TOKEN_COLON] = "COLON",
    [TOKEN_SEMICOLON] = "SEMICOLON",
    [TOKEN_DOT] = "DOT",
    [TOKEN_AT_MAIN] = "AT_MAIN",
    [TOKEN_EOL] = "EOL",
    [TOKEN_INDENT] = "INDENT",
    [TOKEN_DEDENT] = "DEDENT",
    [TOKEN_EOF] = "EOF",
    [TOKEN_KW_DEF] = "KW_DEF",
    [TOKEN_KW_DO] = "KW_DO",
    [TOKEN_KW_DOUBLE] = "KW_DOUBLE",
    [TOKEN_KW_ELSE] = "KW_ELSE",
    [TOKEN_KW_IF] = "KW_IF",
    [TOKEN_KW_IMPORT] = "KW_IMPORT",
    [TOKEN_KW_INT] = "KW_INT",
    [TOKEN_KW_RETURN] = "KW_RETURN",
    [TOKEN_KW_STRING] = "KW_STRING",
    [TOKEN_KW_THEN] = "KW_THEN",
    [TOKEN_KW_UNIT] = "KW_UNIT",
    [TOKEN_KW_VAL] = "KW_VAL",
    [TOKEN_KW_VAR] = "KW_VAR",
    [TOKEN_KW_WHILE] = "KW_WHILE",
    [TOKEN_KW_BOOLEAN] = "KW_BOOLEAN",
    [TOKEN_KW_CASE] = "KW_CASE",
    [TOKEN_KW_CLASS] = "KW_CLASS",
    [TOKEN_KW_EXTENSION] = "KW_EXTENSION",
    [TOKEN_KW_FOR] = "KW_FOR",
    [TOKEN_KW_LIST] = "KW_LIST",
    [TOKEN_KW_MATCH] = "KW_MATCH",
    [TOKEN_KW_TYPE] = "KW_TYPE",
};

/**
 * token_name() - getting printable name of a token type
 * @type: token type to get the name of
 *
 * Return: Returns the name from TOKEN_NAMES
 *         Returns "UNKNOWN" if the type has no name there
 */
static const char *token_name(TokenType type) {
  size_t count = sizeof(TOKEN_NAMES) / sizeof(TOKEN_NAMES[0]);
  if ((size_t)type >= count || TOKEN_NAMES[type] == NULL) {
    return "UNKNOWN";
  }
  return TOKEN_NAMES[type];
}

/**
 * print_string() - printing a string with visible escape sequences
 * @str: string to print
 *
 * Prints str in double quotes. Characters that would not be visible
 * or would make the output ambiguous are printed as escape sequences
 * \n, \t, \r, \\ and \", other control characters as \ddd.
 */
static void print_string(const char *str) {
  (void)putchar('"');
  for (const char *p = str; *p != '\0'; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '\n') {
      (void)fputs("\\n", stdout);
    } else if (c == '\t') {
      (void)fputs("\\t", stdout);
    } else if (c == '\r') {
      (void)fputs("\\r", stdout);
    } else if (c == '\\') {
      (void)fputs("\\\\", stdout);
    } else if (c == '"') {
      (void)fputs("\\\"", stdout);
    } else if (c < 32 || c == 127) {
      (void)printf("\\%03d", c);
    } else {
      (void)putchar(c);
    }
  }
  (void)putchar('"');
}

/**
 * print_token() - printing one token on its own line
 * @token: token to print
 *
 * Prints the token's type name, followed by its value
 * for tokens that carry content (ID, INT, DOUBLE, STRING, BOOL).
 * DOUBLE is printed readable (%g) and exact (%a) in parentheses.
 */
static void print_token(const Token *token) {
  (void)fputs(token_name(token->type), stdout);

  switch (token->type) {
  case TOKEN_ID:
    (void)printf(" %s", token->content.string);
    break;

  case TOKEN_INT:
    (void)printf(" %lld", token->content.int_val);
    break;

  case TOKEN_DOUBLE: {
    double d = token->content.dbl_val;
    (void)printf(" %g (%a)", d, d);
    break;
  }

  case TOKEN_STRING:
    (void)putchar(' ');
    print_string(token->content.string);
    break;

  case TOKEN_BOOL:
    if (token->content.boolean) {
      (void)printf(" true");
    } else {
      (void)printf(" false");
    }
    break;

  default:
    break; // tokens without content
  }

  if (token->lf_before) {
    printf(" (after LF)");
  }
  (void)putchar('\n');
}

int main(int argc, char **argv) {
  if (argc < 2) {
    (void)fprintf(stderr, "Usage: %s <source file>\n", argv[0]);
    return ERR_INTERNAL;
  }

  FILE *src = fopen(argv[1], "r");
  if (src == NULL) {
    (void)fprintf(stderr, "Couldn't open source file.\n");
    return ERR_INTERNAL;
  }

  Scanner sc = scanner_init(src);
  Token token = {.type = TOKEN_EOF, .lf_before = false};
  ReturnCodes rc = RETURN_SUCCESS;

  do {
    rc = scanner_get_token(&sc, &token);
    if (rc != RETURN_SUCCESS) {
      // only the file needs closing, then the scanner's code goes up
      (void)fclose(src);
      return rc;
    }

    print_token(&token);
    token_free(&token);
  } while (token.type != TOKEN_EOF);

  (void)fclose(src);
  return RETURN_SUCCESS;
}
