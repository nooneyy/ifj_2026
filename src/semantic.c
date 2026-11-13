/**
 * @file semantic.c
 * @brief File containing implementation of semantic analysis
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 13.11.2026
 */

#include "semantic.h"
#include "ast.h"
#include "return.h"
#include "symbol.h"
#include "symtable.h"
#include <stddef.h>

// The max amount of params a builtin function has
#define MAX_BUILTIN_PARAM_COUNT 3

/**
 * @brief All the information needed to insert a builtin function to the
 * symtable
 */
typedef struct {
  char *name;
  DataType return_type;
  size_t param_count;
  FunctionParam params[MAX_BUILTIN_PARAM_COUNT];
} BuiltInFunc;

/**
 * @brief Array of builtin functions to be registered
 */
static const BuiltInFunc BUILTINS[] = {
    {"print", UNIT, 1, {{"value", ANY}}},
    {"readLine", STRING, 0, {0}},
    {"readInt", INT, 0, {0}},
    {"readDouble", DOUBLE, 0, {0}},
    {"readBoolean", BOOLEAN, 0, {0}},
    {"toStr", STRING, 1, {{"value", ANY}}},
    {"toInt", INT, 1, {{"value", DOUBLE}}},
    {"strLength", INT, 1, {{"value", STRING}}},
    {
        "subStr",
        STRING,
        3,
        {{"value", STRING}, {"start", INT}, {"end", INT}},
    },
    {"ord", INT, 2, {{"value", STRING}, {"index", INT}}},
    {"chr", STRING, 1, {{"code", INT}}},
};

/**
 * @brief Registers builtin functions to the symtable
 * @param symtable Pointer to the symtable struct
 * @return RETURN_SUCCESS on success, else ERR_INTERNAL on memory allocation
 * failure
 */
static ReturnCodes register_builtins(Symtable *symtable) {
  size_t builtin_size = sizeof(BUILTINS) / sizeof(BuiltInFunc);

  for (size_t idx = 0; idx < builtin_size; idx++) {
    const BuiltInFunc *item = &BUILTINS[idx];
    Symbol *func_sym =
        init_function_symbol(item->name, item->return_type, true, true, false);

    if (func_sym == NULL) {
      return ERR_INTERNAL;
    }

    for (size_t param_idx = 0; param_idx < item->param_count; param_idx++) {
      const FunctionParam *param = &item->params[param_idx];
      ReturnCodes ret =
          add_function_param(func_sym, param->name, param->data_type);

      if (ret != RETURN_SUCCESS) {
        free_symbol(func_sym);
        return ret;
      }
    }

    ReturnCodes ret = symtable_insert(symtable, func_sym);
    if (ret != RETURN_SUCCESS) {
      return ret;
    }
  }

  return RETURN_SUCCESS;
}

/**
 * @brief The first pass of the semantic analysis - registers functions to the
 * symtable, checks for @main definition(s)
 * @param symtable Pointer to the symtable struct
 * @param program Pointer to the root node of the AST (program node)
 * @return RETURN_SUCCESS on success
 */
static ReturnCodes semantic_function_pass(Symtable *symtable,
                                          ASTNode *program) {
  int main_count = 0;
  ASTNode *function = program->data.program.functions;

  while (function != NULL) {
    // Iterating through the program functions...
    ASTFunctionDefNodeData *function_data = &function->data.function_def;

    if (function_data->is_main) {
      main_count++;

      if (main_count > 1) {
        print_error(
            "Semantic error: more than 1 function annotated with @main!",
            function->line);
        return ERR_SEM_RED;
      }

      if (function_data->param_count > 0) {
        print_error(
            "Semantic error: function annotated with @main cannot have params!",
            function->line);
        return ERR_SEM_ARG;
      }

      if (function_data->return_type != UNKNOWN &&
          function_data->return_type != UNIT) {
        print_error("Semantic error: function annotated with @main has other "
                    "return type than UNIT!",
                    function->line);
        return ERR_SEM_TYPE;
      }
    }

    Symbol *func_sym = init_function_symbol(
        function_data->name, function_data->return_type,
        function_data->return_type != UNKNOWN, false, function_data->is_main);

    if (func_sym == NULL) {
      return ERR_INTERNAL;
    }

    for (size_t param_idx = 0; param_idx < function_data->param_count;
         param_idx++) {
      // Iterating through the function params...
      FunctionParam *param = &function_data->params[param_idx];
      ReturnCodes ret =
          add_function_param(func_sym, param->name, param->data_type);

      if (ret != RETURN_SUCCESS) {
        free_symbol(func_sym);
        return ret;
      }
    }

    ReturnCodes ret = symtable_insert(symtable, func_sym);

    if (ret == ERR_SEM_RED) {
      print_error("Semantic error: redefinition of function with the "
                  "same name!",
                  function->line);
    }

    if (ret != RETURN_SUCCESS) {
      return ret;
    }

    function = function->next;
  }

  if (main_count == 0) {
    print_error("Semantic error: no function annotated with "
                "@main!",
                1); // we don't have a line number here...
    return ERR_SEM_UNDEF;
  }

  return RETURN_SUCCESS;
}

ReturnCodes semantic_analysis(ASTNode *program) {
  ReturnCodes ret = RETURN_SUCCESS;
  Symtable *symtable = symtable_init();

  if (symtable == NULL) {
    return ERR_INTERNAL;
  }

  ret = register_builtins(symtable);
  if (ret != RETURN_SUCCESS) {
    symtable_free(symtable);
    return ret;
  }

  ret = semantic_function_pass(symtable, program);
  if (ret != RETURN_SUCCESS) {
    symtable_free(symtable);
    return ret;
  }

  symtable_free(symtable);
  return ret;
}
