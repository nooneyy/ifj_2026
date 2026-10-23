/**
 * @file dynstr.h
 * @brief Dynamic string's header file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 13.10.2026
 */

#ifndef DYNSTR_H
#define DYNSTR_H

#include "return.h"
#include <stddef.h>

/**
 * @brief String that grows itself when it gets full
 */
typedef struct DynStr {
  char *data; // Characters of the string, always terminated with '\0'
              // NULL after dynstr_take() or dynstr_free()
  size_t len; // Number of characters in data, '\0' not included
  size_t cap; // Number of characters that fit into data, '\0' not included
              // data is always allocated with cap + 1 bytes
} DynStr;

/**
 * @brief Allocating an empty DynStr
 *
 * Has to be called before any other dynstr function.
 * Allocates data and makes it an empty string ("").
 * In case of an error, s is left empty (data is NULL),
 * so calling dynstr_free() on it is still safe.
 *
 * @param s DynStr to initialize
 * @return RETURN_SUCCESS in case of a success
 *         ERR_INTERNAL in case of an error with allocation
 */
ReturnCodes dynstr_init(DynStr *s);

/**
 * @brief Appending a char to the end of DynStr
 *
 * If s is full, calls dynstr_realloc() first.
 * '\0' is added after c automatically,
 * so data stays a valid string after every append.
 * In case of an error, s stays unchanged and is NOT freed,
 * the owner of s has to call dynstr_free().
 *
 * @param s DynStr to append to
 * @param c Char to append, should not be '\0'
 * @return RETURN_SUCCESS in case of a success
 *         ERR_INTERNAL in case of an error with reallocation
 */
ReturnCodes dynstr_append(DynStr *s, int c);

/**
 * @brief Emptying DynStr without freeing it
 *
 * Sets data to an empty string ("") and keeps the allocated memory,
 * so s can be filled again without a new allocation.
 *
 * @param s DynStr to clear
 */
void dynstr_clear(DynStr *s);

/**
 * @brief Handing data over to a new owner
 *
 * The caller becomes the owner of the returned string
 * and is responsible for freeing it.
 * s is left empty (data is NULL), dynstr_free() on it is safe.
 * Before using s again, dynstr_init() has to be called.
 *
 * @param s DynStr to take data from
 * @return Pointer to the string
 *         NULL if s is already empty
 */
char *dynstr_take(DynStr *s);

/**
 * @brief Freeing DynStr's data
 *
 * Frees data and leaves s empty (data is NULL).
 * Safe to call more than once and after dynstr_take().
 * Before using s again, dynstr_init() has to be called.
 *
 * @param s DynStr to free
 */
void dynstr_free(DynStr *s);

#endif
