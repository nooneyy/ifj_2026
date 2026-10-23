/**
 * @file symtable.h
 * @brief Header file describing operations and type definitions related to the
 * symtable implementation
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 6.10.2026
 */

// SELECTED VARIANT: TRP-izp (hash-table implementation with open addressing)

#include "symbol.h"
#include <stdbool.h>
#include <stddef.h>

#ifndef SYMTABLE_H
#define SYMTABLE_H

/**
 * @brief Enum describing the possible states of hashtable/symtable slots
 */
typedef enum {
  EMPTY,   // The slot is empty and we can insert into it
  FILLED,  // The slot is filled with data
  DELETED, // The slot's data was removed and it can be inserted into again
} STItemState;

/**
 *  @brief Struct describing a single hashtable/symtable slot
 */
typedef struct {
  STItemState state; // The current state of the slot
  Symbol *symbol; // A pointer to a symbol which contains both the key and the
                  // data of the item
} STItem;

/**
 *   @brief Struct of the entire symtable
 */
typedef struct {
  size_t capacity; // The maximum amount of items the symtable can hold
                   // (before resizing)
  size_t size;     // The current amount of filled slots
  STItem *items;   // Array of symtable slots
} Symtable;

/**
    @brief Function that instantiates a new symtable.
    @return A pointer to the symtable struct if successful, NULL in case of an
   error
 */
Symtable *symtable_init(void);

/**
    @brief Function that inserts (or possibly replaces) a Symbol
    @param symtable A pointer to the Symtable struct
    @param symbol A pointer to an already instantiated Symbol
 */
void symtable_insert(Symtable *symtable, Symbol *symbol);

/**
    @brief Function that deletes a value specified by its
   key (the name of a variable/function). It also frees the Symbol if found
    @param symtable A pointer to the Symtable struct
    @param key A string which identifies the value to be deleted
 */
void symtable_delete(Symtable *symtable, char *key);

/**
    @brief Function that gets a Symbol specified by its
   key.
    @param symtable A pointer to the Symtable struct
    @param key A string which identifies the value to be found
    @return A pointer to the found Symbol if the search went well, otherwise
   NULL
 */
Symbol *symtable_get(Symtable *symtable, char *key);

/**
 * @brief Function that frees the memory of our initialized Symtable struct and
 * all its held Symbols.
 * @param symtable A pointer to the Symtable struct
 */
void symtable_free(Symtable *symtable);

#endif
