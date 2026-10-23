/**
 * @file dynstr.c
 * @brief Dynamic string - string that grows itself when it gets full
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 13.10.2026
 */

#include "dynstr.h"
#include "return.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Initial capacity of DynStr
 *
 * Used for the first allocation in dynstr_init().
 * Allocation adds 1 byte for '\0',
 * so allocations are powers of 2 (32, 64, 128...)
 */
static const size_t DYNSTR_DEFAULT_CAP = 31;

/**
 * @brief Reallocating DynStr to double the bytes
 *
 * Using realloc to double the allocated bytes of s->data.
 * New capacity is (s->cap * 2) + 1, so the allocation
 * (capacity + 1 byte for '\0') stays a power of 2.
 * Updates s->cap accordingly.
 * In case of an error, s stays unchanged and is NOT freed,
 * the owner of s has to call dynstr_free().
 *
 * @param s DynStr to grow
 * @return RETURN_SUCCESS in case of a success
 *         ERR_INTERNAL in case of an error with reallocation
 */
static ReturnCodes dynstr_realloc(DynStr *s) {
  // new allocation would be (s->cap * 2) + 2 bytes, it can't overflow size_t
  if (s->cap > (SIZE_MAX - 2) / 2) {
    return ERR_INTERNAL;
  }

  size_t new_cap = (s->cap * 2) + 1;
  char *ptr = realloc(s->data, new_cap + 1);
  if (ptr == NULL) {
    return ERR_INTERNAL; // s->data is still valid and unchanged
  }

  s->data = ptr;
  s->cap = new_cap;
  return RETURN_SUCCESS;
}

ReturnCodes dynstr_init(DynStr *s) {
  s->len = 0;
  s->cap = 0;
  s->data = malloc(DYNSTR_DEFAULT_CAP + 1);
  if (s->data == NULL) {
    return ERR_INTERNAL;
  }

  s->cap = DYNSTR_DEFAULT_CAP;
  s->data[0] = '\0';
  return RETURN_SUCCESS;
}

ReturnCodes dynstr_append(DynStr *s, int c) {
  if (s->len == s->cap) {
    ReturnCodes rc = dynstr_realloc(s);
    if (rc != RETURN_SUCCESS) {
      return rc;
    }
  }

  // s->len < s->cap here, so both writes fit into cap + 1 bytes
  s->data[s->len] = (char)c;
  s->len++;
  s->data[s->len] = '\0';
  return RETURN_SUCCESS;
}

void dynstr_clear(DynStr *s) {
  s->len = 0;
  if (s->data != NULL) {
    s->data[0] = '\0';
  }
}

char *dynstr_take(DynStr *s) {
  char *data = s->data;
  s->data = NULL;
  s->len = 0;
  s->cap = 0;
  return data;
}

void dynstr_free(DynStr *s) {
  free(s->data);
  s->data = NULL;
  s->len = 0;
  s->cap = 0;
}
