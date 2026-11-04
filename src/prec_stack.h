/**
 * @file prec_stack.h
 * @brief Header file defining Symbols and Stack structs/operations for
 * precedence analysis
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 2.11.2026
 */

#ifndef PREC_STACK_H
#define PREC_STACK_H

#include "ast.h"
#include "return.h"
#include "tokens.h"
#include <stddef.h>

// The initial capacity of the dynamic stack array
#define INITIAL_PSTACK_CAPACITY 16

/**
 * @brief Enum defining the types of Terminals the precedence stack accepts
 */
typedef enum {
  PREC_OPERAND, // a literal value or a variable identifier
  PREC_FUNC,    // a function identifier, "test" for "test(a, b)"
  PREC_LPAREN,  // (
  PREC_RPAREN,  // )
  PREC_COMMA,   // ,
  PREC_NEG,     // TOKEN_MINUS if the previous token wasn't an operand or RPAREN
  PREC_NOT,     // !
  PREC_MUL,     // *
  PREC_DIV,     // /
  PREC_ADD,     // +
  PREC_SUB,     // -
  PREC_LT,      // <
  PREC_LTE,     // <=
  PREC_GT,      // >
  PREC_GTE,     // >=
  PREC_EQ,      // ==
  PREC_NEQ,     // !=
  PREC_AND,     // &&
  PREC_OR,      // ||
  PREC_DELIM,   // $ - the delimiter for the input start and input end
} PrecTerminal;

/**
 * @brief Enum defining the type of Item in the Stack
 */
typedef enum {
  PREC_TERMINAL, // The item is a terminal
  PREC_EXPR,     // The item is an expression (E)
  PREC_LESS,     // The '<' symbol which starts a reducible subexpr.
} PrecItemType;

/**
 * @brief Struct defining the fields of a Stack item
 */
typedef struct {
  PrecItemType type;
  PrecTerminal terminal; // zero if type != PREC_TERMINAL
  Token terminal_token;  // zero if type != PREC_TERMINAL || terminal == '$'
  ASTNode *expr;         // NULL if type != PREC_EXPR
} PrecItem;

/**
 * @brief Struct defining the Precedence Stack fields
 */
typedef struct {
  size_t capacity;   // The length of the dynamic stack array
  size_t item_count; // The current count of valid stack items
  PrecItem *items;   // The dynamic stack array
} PrecStack;

/**
 * @brief Constructor for the precedence stack
 * @return Pointer to PrecStack if successful, otherwise NULL
 */
PrecStack *prec_stack_init(void);

/**
 * @brief Function for inserting to the top of the stack, the stack takes
 * ownership of Token string values and ASTNode* expr.
 * @param ps Pointer to the precedence stack
 * @param item The PrecItem to be pushed to the stack
 * @return ERR_INTERNAL in case of a memory allocation error (and item token
 * string value or item.expr is freed), otherwise RETURN_SUCCESS
 */
ReturnCodes prec_stack_push(PrecStack *ps, PrecItem item);

/**
 * @brief Function for inserting a '<' symbol above the topmost terminal
 * @param ps Pointer to the precedence stack
 * @return ERR_INTERNAL in case of a memory allocation error, otherwise
 * RETURN_SUCCESS
 */
ReturnCodes prec_stack_insert_less(PrecStack *ps);

/**
 * @brief Function for finding the index of the topmost '<' symbol
 * @param ps Pointer to the precedence stack
 * @return The index of the topmost '<' symbol, or 0 on error (ps->items[0]
 * should always be '$')
 */
size_t prec_stack_find_less(PrecStack *ps);

/**
 * @brief Function for removing items from the top of the stack until the first
 * '<', '<' included. Does not free the stack item contents, as they will be
 * added to an expr
 * @param ps Pointer to the precedence stack
 */
void prec_stack_pop_to_less(PrecStack *ps);

/**
 * @brief Function for returning the topmost Terminal on the stack
 * @param ps Pointer to the precedence stack
 * @return The topmost PrecTerminal on the stack
 */
PrecTerminal prec_stack_top_terminal(PrecStack *ps);

/**
 * @brief Function for popping the ASTNode value of the final expression after
 * reducing ($E)
 * @param ps Pointer to the precedence stack
 * @return A pointer to the resulting ASTNode on success, NULL on failure
 */
ASTNode *prec_stack_pop_result(PrecStack *ps);

/**
 * @brief Precedence stack destructor, frees all the items and the PrecStack
 * struct
 * @param ps Pointer to the precedence stack
 */
void prec_stack_free(PrecStack *ps);

#endif
