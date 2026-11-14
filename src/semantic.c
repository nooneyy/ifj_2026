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
 * @brief Item of the Symtable stack (used for checking function bodies)
 */
typedef struct SymtableStack {
  Symtable *sym;
  struct SymtableStack *next;
} SymtableStack;

/**
 * @brief Returns a symbol reachable from the current scope in the function
 * @param stack Pointer to the top of the symtable stack
 * @param key The key of the symbol to be found
 * @return Pointer to the Symbol struct if found, else NULL
 */
static Symbol *symtable_stack_get(SymtableStack *stack, char *key) {
  while (stack != NULL) {
    Symbol *ret = symtable_get(stack->sym, key);
    if (ret != NULL) {
      return ret;
    }
    stack = stack->next;
  }
  return NULL;
}

/**
 * @brief The first pass of the semantic analysis - registers functions to the
 * symtable, checks for @main definition(s)
 * @param symtable Pointer to the symtable struct
 * @param program Pointer to the root node of the AST (program node)
 * @return RETURN_SUCCESS on success
 */
static ReturnCodes semantic_global_pass(Symtable *global_sym,
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

    ReturnCodes ret = symtable_insert(global_sym, func_sym);

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

/**
 * @brief Checks a single program function for semantic errors (initiates a
 * scope with its params, then checks every statement inside the function)
 * @param global_sym A pointer to the global Symtable struct
 * @param function A pointer to the function ASTNode struct
 * @return RETURN_SUCCESS on success, or one of the semantic errors or
 * ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_function(Symtable *global_sym, ASTNode *function) {
  ReturnCodes ret = RETURN_SUCCESS;
  Symtable *function_symtable = symtable_init();

  if (function_symtable == NULL) {
    return ERR_INTERNAL;
  }

  SymtableStack stack = {.sym = function_symtable};

  ASTFunctionDefNodeData function_data = function->data.function_def;

  for (size_t param_idx = 0; param_idx < function_data.param_count;
       param_idx++) {
    FunctionParam *param = &function_data.params[param_idx];
    Symbol *param_var_sym =
        init_variable_symbol(param->name, param->data_type, FUNC_PARAM);

    if (param_var_sym == NULL) {
      ret = ERR_INTERNAL;
      break;
    }

    ret = symtable_insert(stack.sym, param_var_sym);

    if (ret == ERR_SEM_RED) {
      print_error(
          "Semantic error: multiple function variables with the same name",
          function->line);
    }

    if (ret != RETURN_SUCCESS) {
      break;
    }
  }

  // TODO
  // function_data.body->data.block.statements
  (void)symtable_stack_get;
  (void)global_sym;

  symtable_free(stack.sym);
  return ret;
}

/**
 * @brief Performs the second semantic analysis pass - checks all functions for
 * semantic errors and fills all the ASTNodes with relevant information
 * @param global_sym A pointer to the global Symtable struct
 * @param program A pointer to the root node of the AST (the program node)
 * @return RETURN_SUCCESS on success, or one of the semantic errors or
 * ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes semantic_body_pass(Symtable *global_sym, ASTNode *program) {
  ReturnCodes ret = RETURN_SUCCESS;
  ASTNode *function = program->data.program.functions;

  while (function != NULL) {
    ret = check_function(global_sym, function);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    function = function->next;
  }

  return ret;
}

ReturnCodes semantic_analysis(ASTNode *program) {
  ReturnCodes ret = RETURN_SUCCESS;
  Symtable *global_sym = symtable_init();

  if (global_sym == NULL) {
    return ERR_INTERNAL;
  }

  ret = register_builtins(global_sym);
  if (ret != RETURN_SUCCESS) {
    symtable_free(global_sym);
    return ret;
  }

  ret = semantic_global_pass(global_sym, program);
  if (ret != RETURN_SUCCESS) {
    symtable_free(global_sym);
    return ret;
  }

  ret = semantic_body_pass(global_sym, program);
  if (ret != RETURN_SUCCESS) {
    symtable_free(global_sym);
    return ret;
  }

  symtable_free(global_sym);
  return ret;
}
