/**
 * @file return.h
 * @brief Header file defining return codes
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 05.10.2026
 */

#ifndef RETURN_H
#define RETURN_H

/**
 * @brief Prints error message to stderr
 *
 * Prints "ERROR on line {line}: {string}\n" to stderr
 *
 * @param message Error message to be printed
 * @param line    Line the error is on
 *                0 if no line needed
 */
void print_error(const char *message, unsigned int line);

typedef enum ReturnCodes {

  /**
   * RETURN_SUCCESS - no errors
   *
   * Returned when neither external nor internal
   * errors were caught.
   */
  RETURN_SUCCESS = 0,

  /**
   * ERR_LEX_ANA - Lexical analysis error
   *
   * Returned when an error was found during
   * the lexical analysis.
   *
   * Wrong structure of:
   *  current lexeme
   *  comment
   *  indentation
   */
  ERR_LEX_ANA = 1,

  /**
   * ERR_SYN_ANA - Syntactic analysis error
   *
   * Returned when an error was found during
   * the syntactic analysis.
   *
   * Wrong syntax
   * Missing prologue
   * Not allowed construction
   */
  ERR_SYN_ANA = 2,

  /**
   * ERR_SEM_UNDEF - Semantic missing definition error
   *
   * Returned when a semantic error of type
   * missing definition / undefined was found.
   *
   * Usage of und:
   *  functions
   *  variables
   *  types
   * Missing function with @main annotation
   */
  ERR_SEM_UNDEF = 3,

  /**
   * ERR_SEM_RED - Semantic redefinition error
   *
   * Returned when a semantic error of type
   * redefinition was found.
   *
   * Redefinition of:
   *  function
   *  variable
   */
  ERR_SEM_RED = 4,

  /**
   * ERR_SEM_ARG - Semantic argument error
   *
   * Returned when a semantic error with an
   * argument was found.
   *
   * Incorrect number of arguments
   * Incorrect type of argument
   * Not allowed parameters of @main annotated function
   */
  ERR_SEM_ARG = 5,

  /**
   * ERR_SEM_TYPE - Semantic type error
   *
   * Returned when a semantic error with
   * a type compatibility or derivation
   * was found.
   *
   * Compatibility or derivation of a type in:
   *  an expression
   *  an assignment
   *  an initialization
   *  a return value
   *  a control structure
   */
  ERR_SEM_TYPE = 6,

  /**
   * ERR_SEM_NONMODIFIABLE - Semantic modifiability error
   *
   * Returned when a semantic error with
   * (un)modifiability was found.
   *
   * Write attempt into an unmodifiable:
   *  variable
   *  formal function parameter
   */
  ERR_SEM_NONMODIFIABLE = 7,

  /**
   * ERR_SEM_OTHER - Other semantic error
   *
   * Returned when a semantic error that doesn't fit
   * errors 3-7 was found.
   *
   * Example:
   *  not allowed return
   *  non-exhaustive match
   *  zero range step
   */
  ERR_SEM_OTHER = 10,

  /**
   * ERR_INTERNAL - Internal compiler error
   *
   * Returned if an internal error inside the compiler occurred.
   *
   * Example:
   *  memory allocation error
   */
  ERR_INTERNAL = 99,
} ReturnCodes;

#endif
