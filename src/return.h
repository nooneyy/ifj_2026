/**
 * @file return.h
 * @brief Header file defining return codes
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 05.10.2026
 */

#ifndef RETURN_H
#define RETURN_H

/**
 * define RETURN_SUCCESS - no errors
 *
 * Returned when neither external nor internal
 * errors were caught.
 */
#define RETURN_SUCCESS 0

/**
 * define ERR_LEX_ANA - Lexical analysis error
 *
 * Returned when an error was found during
 * the lexical analysis.
 *
 * Wrong structure of:
 *  current lexeme
 *  comment
 *  indentation
 */
#define ERR_LEX_ANA 1

/**
 * define ERR_SYN_ANA - Syntactic analysis error
 *
 * Returned when an error was found during
 * the syntactic analysis.
 *
 * Wrong syntax
 * Missing prologue
 * Not allowed construction
 */
#define ERR_SYN_ANA 2

/**
 * define ERR_SEM_UNDEFINED - Semantic missing definition error
 *
 * Returned when a semantic error of type
 * missing definition / undefined was found.
 *
 * Usage of undefined:
 *  functions
 *  variables
 *  types
 * Missing function with @main annotation
 */
#define ERR_SEM_UNDEFINED 3

/**
 * define ERR_SEM_REDEFINED - Semantic redefinition error
 *
 * Returned when a semantic error of type
 * redefinition was found.
 *
 * Redefinition of:
 *  function
 *  variable
 */
#define ERR_SEM_REDEFINED 4

/**
 * define ERR_SEM_ARG - Semantic argument error
 *
 * Returned when a semantic error with an
 * argument was found.
 *
 * Incorrect number of arguments
 * Incorrect type of argument
 * Not allowed parameters of @main annotated function
 */
#define ERR_SEM_ARG 5

/**
 * define ERR_SEM_TYPE - Semantic type error
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
#define ERR_SEM_TYPE 6

/**
 * define ERR_SEM_NONMODIFIABLE - Semantic modifiability error
 *
 * Returned when a semantic error with
 * (un)modifiability was found.
 *
 * Write attempt into an unmodifiable:
 *  variable
 *  formal function parameter
 */
#define ERR_SEM_NONMODIFIABLE 7

/**
 * define ERR_SEM_OTHER - Other semantic error
 *
 * Returned when a semantic error that doesn't fit
 * errors 3-7 was found.
 *
 * Example:
 *  not allowed return
 *  non-exhaustive match
 *  zero range step
 */
#define ERR_SEM_OTHER 10

/**
 * define ERR_INTERNAL - Internal compiler error
 *
 * Returned if an internal error inside the compiler occurred.
 *
 * Example:
 *  memory allocation error
 */
#define ERR_INTERNAL 99

#endif
