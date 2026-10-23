/**
 * @file symtable.c
 * @brief Implementation of a symtable using a hashtable with open addressing
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 6.10.2026
 */

#include "symtable.h"
#include "return.h"
#include "symbol.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// TODO: Auto-registering of builtins and other custom ops

#define INITIAL_S_TABLE_CAPACITY 16 // The initial capacity of the symtable

Symtable *symtable_init(void) {
  Symtable *s_table = malloc(sizeof(Symtable));

  if (s_table == NULL) {
    return NULL;
  }

  // calloc instead of malloc to fix clang-tidy warning
  s_table->items = calloc(INITIAL_S_TABLE_CAPACITY, sizeof(STItem));

  if (s_table->items == NULL) {
    free(s_table);
    return NULL;
  }

  s_table->capacity = INITIAL_S_TABLE_CAPACITY;
  s_table->size = 0;

  return s_table;
}

void symtable_free(Symtable *symtable) {
  for (size_t idx = 0; idx < symtable->capacity; idx++) {
    STItem *item = &symtable->items[idx];
    if (item->state == FILLED) {
      free_symbol(item->symbol);
    }
  }

  free(symtable->items);
  free(symtable);
}

/**
 * @brief An implementation of DJB hash that converts keys -> hash-map indexes
 * @param key A string which uniquely identifies the hash-map value
 * @return A hash that can be turned into an index into the hash-map's items
 * array
 */
static size_t hash(char *key) {
  size_t hash = 5381;

  while (*key != '\0') {
    hash = (hash * 33) + *key;
    key++;
  }

  return hash;
}

/**
 * @brief Enlarges the symtable by doubling its capacity
 * @param symtable The pointer to the symtable struct
 * @return RETURN_SUCCESS if the enlargement was successful, otherwise
 * ERR_INTERNAL on malloc error
 */
static ReturnCodes symtable_enlarge(Symtable *symtable) {
  assert(symtable->capacity > 0); // This removes clang-tidy warnings

  size_t new_capacity = symtable->capacity * 2;

  // new_capacity can overflow but this should realistically never happen
  // calloc instead of malloc to fix clang-tidy warning
  STItem *tmp_array = calloc(new_capacity, sizeof(STItem));

  if (tmp_array == NULL) {
    return ERR_INTERNAL;
  }

  // Rehash the old values and put them into free slots in the new table
  for (size_t idx = 0; idx < symtable->capacity; idx++) {
    STItem *old_item = &symtable->items[idx];
    if (old_item->state == FILLED) {
      size_t item_hash = hash(old_item->symbol->name);

      for (size_t offset = 0; offset < new_capacity; offset++) {
        STItem *new_item = &tmp_array[(item_hash + offset) % new_capacity];

        if (new_item->state == EMPTY) {
          *new_item = *old_item;
          break;
        }
      }
    }
  }

  free(symtable->items);
  symtable->items = tmp_array;
  symtable->capacity = new_capacity;

  return RETURN_SUCCESS;
}

/**
 * @brief A function which attemps to get the internal STItem from its key.
 * @param symtable A pointer to the symtable struct
 * @param key A string which identifies the value to be found
 * @return A pointer to an STItem if the search was successful, otherwise NULL
 */
static STItem *symtable_get_internal(Symtable *symtable, char *key) {
  size_t initial_hash = hash(key);

  // Use linear steps to iterate through n=capacity of items to find our item
  for (size_t offset = 0; offset < symtable->capacity; offset++) {
    STItem *item =
        &symtable->items[(initial_hash + offset) % symtable->capacity];

    if (item->state == FILLED && strcmp(item->symbol->name, key) == 0) {
      // Match found
      return item;
    }

    if (item->state == EMPTY) {
      return NULL;
    }
  }

  return NULL;
}

Symbol *symtable_get(Symtable *symtable, char *key) {
  STItem *item = symtable_get_internal(symtable, key);

  if (!item) {
    return NULL;
  }

  return item->symbol;
}

void symtable_delete(Symtable *symtable, char *key) {
  STItem *item = symtable_get_internal(symtable, key);

  if (!item) {
    return;
  }

  free_symbol(item->symbol);
  item->symbol = NULL;
  item->state = DELETED;

  symtable->size--;
}

ReturnCodes symtable_insert(Symtable *symtable, Symbol *symbol) {
  STItem *possible_match = symtable_get_internal(symtable, symbol->name);

  if (possible_match) {
    // Another item already has the same key
    if (possible_match->symbol != symbol) {
      // The symbol pointers are different => replace the old Symbol with the
      // new Symbol
      free_symbol(possible_match->symbol);
      possible_match->symbol = symbol;
    }
    // Do nothing if the pointers are the same
    return RETURN_SUCCESS;
  }

  if (symtable->size * 4 >= symtable->capacity * 3) {
    // Enlarges the symtable if it is 75% full
    ReturnCodes enlarge_result = symtable_enlarge(symtable);

    if (enlarge_result != RETURN_SUCCESS) {
      return enlarge_result;
    }
  }

  size_t hash_ret = hash(symbol->name);

  STItem *first_deleted = NULL;

  // Linearly step through n=capacity items to find an EMPTY slot to insert into
  for (size_t offset = 0; offset < symtable->capacity; offset++) {
    size_t idx = (hash_ret + offset) % symtable->capacity;

    STItem *item = &symtable->items[idx];

    switch (item->state) {
    case FILLED:
      // Skip filled slots
      break;
    case DELETED:
      if (!first_deleted) {
        first_deleted = item;
      }
      break;
    case EMPTY:
      if (first_deleted) {
        // We found an EMPTY item, but first replace a DELETED one if we found
        // it
        // No need to free the previous symbol as it has already been freed
        first_deleted->symbol = symbol;
        first_deleted->state = FILLED;
      } else {
        item->symbol = symbol;
        item->state = FILLED;
      }
      symtable->size++;
      return RETURN_SUCCESS;
    }
  }
  // If we didn't find any EMPTY items, but found a deleted one, replace it
  if (first_deleted) {
    first_deleted->symbol = symbol;
    first_deleted->state = FILLED;
    symtable->size++;
    return RETURN_SUCCESS;
  }

  // There was no slot to insert into, which should be impossible because we
  // enlarge the symtable when it is 75% full
  return ERR_INTERNAL;
}
