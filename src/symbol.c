/**
 * @file symbol.c
 * @brief File containing the implementation of functions that create Symbol
 * structs
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 12.10.2026
 */

#include "symbol.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief A helper function which allocates a new buffer and copies a src string
 * into it
 * @param src The string to be copied
 * @return A pointer to the copied string, or NULL on failure
 */
static char *copy_string(char *src) {
  char *dest = malloc((strlen(src) + 1) * sizeof(char));

  if (!dest) {
    // todo error handling
    return NULL;
  }

  strcpy(dest, src);

  return dest;
}

Symbol *init_variable_symbol(char *name, DataType data_type,
                             VariableType variable_type) {
  Symbol *new_sym = malloc(sizeof(Symbol));

  if (!new_sym) {
    // todo error handling
    return NULL;
  }

  new_sym->name = copy_string(name);

  if (new_sym->name == NULL) {
    // todo error handling
    free(new_sym);
    return NULL;
  }

  new_sym->type = VARIABLE;

  new_sym->data.variable =
      (VariableSymbol){.data_type = data_type, .variable_type = variable_type};

  return new_sym;
}

Symbol *init_function_symbol(char *name, DataType return_type,
                             bool return_type_defined, bool is_builtin,
                             bool is_main) {
  Symbol *new_sym = malloc(sizeof(Symbol));

  if (!new_sym) {
    // todo error handling
    return NULL;
  }

  new_sym->name = copy_string(name);

  if (new_sym->name == NULL) {
    // todo error handling
    free(new_sym);
    return NULL;
  }

  new_sym->type = FUNCTION;

  new_sym->data.function = (FunctionSymbol){
      .return_type = return_type,
      .return_type_defined = return_type_defined,
      .is_builtin = is_builtin,
      .is_main = is_main,
      .param_count = 0,
      .params = NULL,
  };

  return new_sym;
}

void add_function_param(Symbol *function_sym, char *param_name,
                        DataType param_data_type) {
  if (function_sym->type != FUNCTION) {
    // todo error handling
    return;
  }

  FunctionSymbol *data = &function_sym->data.function;

  char *copied_param_name = copy_string(param_name);

  if (copied_param_name == NULL) {
    // todo error handling
    return;
  }

  // Resizing the param array to add one more param
  FunctionParam *temp_ptr =
      realloc(data->params, sizeof(FunctionParam) * (data->param_count + 1));

  if (!temp_ptr) {
    // todo error handling
    free(copied_param_name);
    return;
  }

  temp_ptr[data->param_count] =
      (FunctionParam){.name = copied_param_name, .data_type = param_data_type};

  data->params = temp_ptr;
  data->param_count++;
}

void free_symbol(Symbol *symbol) {
  if (symbol->type == FUNCTION) {
    // Function symbol => we have to clear its param array
    for (unsigned idx = 0; idx < symbol->data.function.param_count; idx++) {
      free(symbol->data.function.params[idx].name);
    }

    free(symbol->data.function.params);
  }

  free(symbol->name);
  free(symbol);
}
