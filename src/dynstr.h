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
 * struct DynStr - String that grows itself when it gets full
 * @data: Characters of the string, always terminated with '\0'
 *        NULL after dynstr_take() or dynstr_free()
 * @len:  Number of characters in data, '\0' not included
 * @cap:  Number of characters that fit into data, '\0' not included
 *        data is always allocated with cap + 1 bytes
 */
typedef struct DynStr {
  char *data;
  size_t len;
  size_t cap;
} DynStr;

/**
 * dynstr_init() - allocating an empty DynStr
 * @s: DynStr to initialize
 *
 * Has to be called before any other dynstr function.
 * Allocates data and makes it an empty string ("").
 * In case of an error, s is left empty (data is NULL),
 * so calling dynstr_free() on it is still safe.
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error with allocation
 */
ReturnCodes dynstr_init(DynStr *s);

/**
 * dynstr_append() - appending a char to the end of DynStr
 * @s: DynStr to append to
 * @c: char to append, should not be '\0'
 *
 * If s is full, calls dynstr_realloc() first.
 * '\0' is added after c automatically,
 * so data stays a valid string after every append.
 * In case of an error, s stays unchanged and is NOT freed,
 * the owner of s has to call dynstr_free().
 *
 * Return: Returns RETURN_SUCCESS in case of a success
 *         Returns ERR_INTERNAL in case of an error with reallocation
 */
ReturnCodes dynstr_append(DynStr *s, int c);

/**
 * dynstr_clear() - emptying DynStr without freeing it
 * @s: DynStr to clear
 *
 * Sets data to an empty string ("") and keeps the allocated memory,
 * so s can be filled again without a new allocation.
 */
void dynstr_clear(DynStr *s);

/**
 * dynstr_take() - handing data over to a new owner
 * @s: DynStr to take data from
 *
 * The caller becomes the owner of the returned string
 * and is responsible for freeing it.
 * s is left empty (data is NULL), dynstr_free() on it is safe.
 * Before using s again, dynstr_init() has to be called.
 *
 * Return: Returns pointer to the string
 *         Returns NULL if s is already empty
 */
char *dynstr_take(DynStr *s);

/**
 * dynstr_free() - freeing DynStr's data
 * @s: DynStr to free
 *
 * Frees data and leaves s empty (data is NULL).
 * Safe to call more than once and after dynstr_take().
 * Before using s again, dynstr_init() has to be called.
 */
void dynstr_free(DynStr *s);

#endif
