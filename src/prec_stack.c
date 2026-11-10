/**
 * @file prec_stack.c
 * @brief Implementation of operations for the precedence analysis Stack
 * @author Oldřich Vašíček <xvasico00@stud.fit.vut.cz>
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 4.11.2026
 */

#include "prec_stack.h"
#include "ast.h"
#include "return.h"
#include "tokens.h"
#include <stdlib.h>
#include <string.h>

PrecStack *prec_stack_init(void) {
  PrecStack *ps = malloc(sizeof(*ps));
  if (ps == NULL) {
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ps->items = malloc(INITIAL_PSTACK_CAPACITY * sizeof(*ps->items));
  if (ps->items == NULL) {
    free(ps);
    print_error("Allocation failed!", 0);
    return NULL;
  }
  ps->items[0] = (PrecItem){.type = PREC_TERMINAL, .terminal = PREC_DELIM};
  ps->item_count = 1;
  ps->capacity = INITIAL_PSTACK_CAPACITY;
  return ps;
}

/**
 * @brief Function for enlarging the precedence stack's item array by doubling
 * it
 * @param ps Pointer to the precedence stack
 * @return ERR_INTERNAL on memory allocation error, RETURN_SUCCESS otherwise
 */
static ReturnCodes prec_stack_enlarge(PrecStack *ps) {
  size_t new_capacity = ps->capacity * 2;
  PrecItem *temp = realloc(ps->items, sizeof(PrecItem) * new_capacity);

  if (temp == NULL) {
    print_error("Reallocation failed!", 0);
    return ERR_INTERNAL;
  }

  ps->capacity = new_capacity;
  ps->items = temp;

  return RETURN_SUCCESS;
}

/**
 * @brief Destructor for PrecItem's contents (frees the expr if type =
 * PREC_EXPR, frees the terminal's token if type = PREC_TERMINAL)
 * @param item Pointer to the PrecItem you wish to free
 */
static void free_prec_item(PrecItem *item) {
  if (item->type == PREC_EXPR) {
    ast_free(item->expr);
  } else if (item->type == PREC_TERMINAL) {
    token_free(&item->terminal_token);
  }
}

ReturnCodes prec_stack_push(PrecStack *ps, PrecItem item) {
  if (ps->capacity == ps->item_count) {
    ReturnCodes ret = prec_stack_enlarge(ps);
    if (ret != RETURN_SUCCESS) {
      free_prec_item(&item);
      return ret;
    }
  }

  ps->items[ps->item_count] = item;
  ps->item_count++;

  return RETURN_SUCCESS;
}

/**
 * @brief Finds the index of the topmost terminal on the precedence stack
 * @param ps Pointer to the precedence stack
 * @return Index of the topmost terminal (never fails because '$' is always on
 * the stack and is a terminal)
 */
static size_t prec_stack_top_terminal_idx(PrecStack *ps) {
  for (size_t idx = ps->item_count - 1; idx > 0; idx--) {
    PrecItem item = ps->items[idx];

    if (item.type == PREC_TERMINAL) {
      return idx;
    }
  }

  return 0;
}

ReturnCodes prec_stack_insert_less(PrecStack *ps) {
  size_t topmost_terminal_idx = prec_stack_top_terminal_idx(ps);

  if (ps->item_count == ps->capacity) {
    ReturnCodes ret = prec_stack_enlarge(ps);

    if (ret != RETURN_SUCCESS) {
      return ret;
    }
  }

  memmove(&ps->items[topmost_terminal_idx + 2],
          &ps->items[topmost_terminal_idx + 1],
          sizeof(PrecItem) * (ps->item_count - (topmost_terminal_idx + 1)));

  ps->items[topmost_terminal_idx + 1] = (PrecItem){.type = PREC_LESS};
  ps->item_count++;

  return RETURN_SUCCESS;
}

size_t prec_stack_find_less(PrecStack *ps) {
  for (size_t idx = ps->item_count - 1; idx > 0; idx--) {
    if (ps->items[idx].type == PREC_LESS) {
      return idx;
    }
  }

  return 0;
}

void prec_stack_pop_to_less(PrecStack *ps) {
  size_t less_idx = prec_stack_find_less(ps);
  if (less_idx != 0) {
    ps->item_count = less_idx;
  }
}

PrecTerminal prec_stack_top_terminal(PrecStack *ps) {
  return ps->items[prec_stack_top_terminal_idx(ps)].terminal;
}

ASTNode *prec_stack_pop_result(PrecStack *ps) {
  if (ps->item_count != 2 || ps->items[1].type != PREC_EXPR) {
    print_error("Invalid state of precedence stack!", 0);
    return NULL;
  }

  PrecItem *expr_item = &ps->items[1];
  ASTNode *expr = expr_item->expr;
  expr_item->expr = NULL;
  ps->item_count--;
  return expr;
}

void prec_stack_free(PrecStack *ps) {
  for (size_t idx = 1; idx < ps->item_count; idx++) {
    free_prec_item(&ps->items[idx]);
  }

  free(ps->items);
  free(ps);
}
