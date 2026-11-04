/**
 * @file prec_stack.c
 * @brief Implementation of operations for the precedence analysis Stack
 * @author Oldřich Vašíček <xvasico00@stud.fit.vut.cz>
 * @date 4.11.2026
 */

#include "prec_stack.h"
#include "ast.h"
#include "return.h"
#include <stddef.h>
#include <stdlib.h>

PrecStack *prec_stack_init(void) {
  PrecStack *ps = malloc(sizeof(*ps));
  if (ps == NULL) {
    print_error("Stack allocation failed!", 0);
    return NULL;
  }

  ps->items = malloc(INITIAL_PSTACK_CAPACITY * sizeof(*ps->items));
  if (ps->items == NULL) {
    free(ps);
    print_error("Allocation of stack array failed!", 0);
    return NULL;
  }
  ps->item_count = 0;
  ps->capacity = INITIAL_PSTACK_CAPACITY;
  return ps;
}

ReturnCodes prec_stack_push(PrecStack *ps, PrecItem item) {
  if (ps == NULL) {
    print_error("Invalid stack pointer!", 0);
    return ERR_INTERNAL;
  }
  if (ps->capacity == ps->item_count) {
    PrecItem *ps_items_tmp =
        realloc(ps->items, ps->capacity * 2 * sizeof(*ps_items_tmp));
    if (ps_items_tmp == NULL) {
      print_error("Stack reallocation failed!", 0);
      return ERR_INTERNAL;
    }

    ps->capacity = ps->capacity * 2;
    ps->items = ps_items_tmp;
  }

  ps->items[ps->item_count] = item;
  ps->item_count++;

  return RETURN_SUCCESS;
}

ReturnCodes prec_stack_insert_less(PrecStack *ps) {
  if (ps == NULL) {
    print_error("Invalid stack pointer!", 0);
    return ERR_INTERNAL;
  }
  if (ps->items == NULL || ps->item_count == 0) {
    print_error("No items on stack!", 0);
    return ERR_INTERNAL;
  }
  for (size_t i = ps->item_count; i != 0; i--) {
    if (ps->items[i - 1].type == PREC_TERMINAL) {
      if (ps->capacity == ps->item_count) {
        PrecItem *ps_items_tmp =
            realloc(ps->items, ps->capacity * 2 * sizeof(*ps_items_tmp));
        if (ps_items_tmp == NULL) {
          print_error("Stack reallocation failed!", 0);
          return ERR_INTERNAL;
        }

        ps->capacity = ps->capacity * 2;
        ps->items = ps_items_tmp;
      }
      for (size_t j = ps->item_count; j > i; j--) {
        ps->items[j] = ps->items[j - 1];
      }
      ps->items[i].type = PREC_LESS;
      ps->items[i].expr = NULL;
      ps->items[i].terminal = 0;
      // ps->items[i].terminal_token = 0;

      ps->item_count++;

      return RETURN_SUCCESS;
    }
  }
  print_error("No terminal on stack!", 0);
  return ERR_INTERNAL;
}

size_t prec_stack_find_less(PrecStack *ps) {
  if (ps == NULL) {
    print_error("Invalid stack pointer!", 0);
    return 0;
  }
  if (ps->items == NULL || ps->item_count == 0) {
    print_error("No items on stack!", 0);
    return 0;
  }
  for (size_t i = ps->item_count; i > 1; i--) {
    if (ps->items[i - 1].type == PREC_LESS) {
      return i - 1;
    }
  }
  print_error("No '<' on stack!", 0);
  return 0;
}

void prec_stack_pop_to_less(PrecStack *ps) {
  size_t new_ps_item_count = prec_stack_find_less(ps);
  if (new_ps_item_count != 0) {
    ps->item_count = new_ps_item_count;
  }
}

PrecTerminal prec_stack_top_terminal(PrecStack *ps) {
  if (ps == NULL) {
    print_error("Invalid stack pointer!", 0);
    return NULL;
  }
  if (ps->items == NULL || ps->item_count == 0) {
    print_error("No items on stack!", 0);
    return NULL;
  }
  for (size_t i = ps->item_count; i != 0; i--) {
    if (ps->items[i - 1].type == PREC_TERMINAL) {
      return ps->items[i - 1].terminal;
    }
    print_error("No terminal on stack!", 0);
    return NULL;
  }
}

ASTNode *prec_stack_pop_result(PrecStack *ps) {
  if (ps == NULL) {
    print_error("Invalid stack pointer!", 0);
    return NULL;
  }
  if (ps->items == NULL || ps->item_count == 0) {
    print_error("No items on stack!", 0);
    return NULL;
  }
  if (ps->item_count > 2 || ps->items[0].terminal != PREC_DELIM ||
      ps->items[1].type != PREC_EXPR) {
    print_error("Invalid state of stack!", 0);
    return NULL;
  }
  if (ps->items[1].expr == NULL) {
    print_error("Invalid expression!", 0);
    return NULL;
  }
  ASTNode *expr_tmp = ps->items[1].expr;
  ps->items[1].expr = NULL;
  ps->item_count = 1;
  return expr_tmp;
}

void prec_stack_free(PrecStack *ps) {
  if (ps == NULL) {
    return;
  }
  if (ps->item_count == 0) {
    free(ps->items);
    free(ps);
  }
  if (ps->items != NULL) {
    for (size_t i = 0; i < ps->item_count - 1; i++) {
      switch (ps->items[i].type) {
      case PREC_EXPR:
        ast_free(ps->items[i].expr);
        break;
      case PREC_TERMINAL:
        if (ps->items[i].terminal == PREC_DELIM) {
          break;
        }
        token_free(&ps->items[i].terminal_token);
        break;
      case PREC_LESS:
        break;
      }
    }
    free(ps->items);
    free(ps);
  }
}
