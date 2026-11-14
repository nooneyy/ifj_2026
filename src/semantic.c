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
 * @brief Inserts a new variable into the topmost symtable on the scope stack
 * @param stack Pointer to the top of the symtable stack
 * @param name The name of the variable to be inserted
 * @param data_type The data type of the variable to be inserted
 * @param var_type The mutability of the variable to be inserted
 * @param line The input line number the variable was scanned on
 * @return RETURN_SUCCESS on success, ERR_INTERNAL on memory allocation failure,
 * ERR_SEM_RED on variable redefinition
 */
static ReturnCodes symtable_stack_new_var(SymtableStack *stack, char *name,
                                          DataType data_type,
                                          VariableType var_type,
                                          unsigned int line) {
  Symbol *var_sym = init_variable_symbol(name, data_type, var_type);

  if (var_sym == NULL) {
    return ERR_INTERNAL;
  }

  ReturnCodes ret = symtable_insert(stack->sym, var_sym);

  if (ret == ERR_SEM_RED) {
    print_error(
        "Semantic error: redefinition of variable within the same scope", line);
  }

  return ret;
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
 * @brief Checks a statement ASTNode for semantic errors
 * @param global_sym A pointer to the global Symtable struct
 * @param statement A pointer to the statement's AST node (what's a statement?
 * see func impl)
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_statement(Symtable *global_sym, ASTNode *statement,
                                   SymtableStack stack);

/**
 * @brief Checks an expression ASTNode for semantic errors
 * @param global_sym A pointer to the global Symtable struct
 * @param expr A pointer to the expr's AST node (what's an expr? see func impl)
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @param out Pointer to the out DataType enum - the function sets this to the
 * type the expr evaluates to
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure with *out set to UNKNOWN
 */
static ReturnCodes check_expr(Symtable *global_sym, ASTNode *expr,
                              SymtableStack stack, DataType *out) {
  ReturnCodes ret = RETURN_SUCCESS;
  *out = UNKNOWN;

  switch (expr->node_type) {
  case NODE_LITERAL:
  case NODE_VARIABLE_REF:
    // todo
    break;
  case NODE_UNARY_OP: {
    // UNARY OP - check expr of operand
    ASTUnaryOpNodeData *unary_op_data = &expr->data.unary_op;
    ret = check_expr(global_sym, unary_op_data->operand, stack, out);
    break;
  }
  case NODE_BINARY_OP: {
    // BINARY op - check expr of left and right operands
    ASTBinaryOpNodeData *binary_op_data = &expr->data.binary_op;
    ret = check_expr(global_sym, binary_op_data->left, stack, out);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    ret = check_expr(global_sym, binary_op_data->right, stack, out);
    break;
  }
  case NODE_FUNCTION_CALL: {
    // FUNCTION CALL - check parameter expressions
    // Not a pointer because the LL advancing needs a copy
    ASTFunctionCallNodeData fun_call_data = expr->data.function_call;

    while (fun_call_data.args != NULL) {
      ret = check_expr(global_sym, fun_call_data.args, stack, out);
      if (ret != RETURN_SUCCESS) {
        break;
      }
      fun_call_data.args = fun_call_data.args->next;
    }
    break;
  }
  case NODE_IF: {
    // IF - check conditions, then branch and else branch (if it exists)
    ASTIfNodeData *if_data = &expr->data.if_node;
    ret = check_expr(global_sym, if_data->condition, stack, out);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    ret = check_statement(global_sym, if_data->then_branch, stack);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    if (if_data->else_branch != NULL) {
      ret = check_statement(global_sym, if_data->else_branch, stack);
    }
    break;
  }
  default:
    print_error("Wrong node type passed to check_expr!", 0);
    return ERR_INTERNAL;
  }
  return ret;
}

/**
 * @brief Iterates through a list of statement nodes and calls check_statement
 * for all of them
 * @param global_sym A pointer to the global Symtable struct
 * @param statement_ll The starting element of the linked list of statement
 * nodes
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_statement_list(Symtable *global_sym,
                                        ASTNode *statement_ll,
                                        SymtableStack stack);

/**
 * @brief Initiates a new symtable scope and calls check_statement_list() on
 * the block statements
 * @param global_sym A pointer to the global Symtable struct
 * @param block A pointer to the block ASTNode
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_block(Symtable *global_sym, ASTNode *block,
                               SymtableStack stack) {
  // BLOCK - Push new scope, check statement list

  Symtable *block_sym = symtable_init();

  if (block_sym == NULL) {
    return ERR_INTERNAL;
  }

  SymtableStack block_stack = {.sym = block_sym, .next = &stack};

  ASTBlockNodeData *block_data = &block->data.block;

  ReturnCodes ret =
      check_statement_list(global_sym, block_data->statements, block_stack);

  symtable_free(block_sym);
  return ret;
}

/**
 * @brief Checks the for loop node for semantic errors. Initiates a new inner
 * scope with the for loop generator variable in it
 * @param global_sym A pointer to the global Symtable struct
 * @param for_node A pointer to the for ASTNode
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_for(Symtable *global_sym, ASTNode *for_node,
                             SymtableStack stack) {
  // FOR - Check range start, range end, optionally range step,
  // then push new scope, then check guards and body

  ReturnCodes ret = RETURN_SUCCESS;
  ASTForNodeData for_data = for_node->data.for_node;
  DataType expr_type = UNKNOWN;

  ret = check_expr(global_sym, for_data.range_start, stack, &expr_type);

  if (ret != RETURN_SUCCESS) {
    return ret;
  }

  ret = check_expr(global_sym, for_data.range_end, stack, &expr_type);

  if (ret != RETURN_SUCCESS) {
    return ret;
  }

  if (for_data.range_step != NULL) {
    ret = check_expr(global_sym, for_data.range_step, stack, &expr_type);
    if (ret != RETURN_SUCCESS) {
      return ret;
    }
  }

  Symtable *for_sym = symtable_init();

  if (for_sym == NULL) {
    return ERR_INTERNAL;
  }

  SymtableStack for_stack = {.sym = for_sym, .next = &stack};

  // Add generator to new scope
  ret = symtable_stack_new_var(&for_stack, for_data.generator_name, INT, VAL,
                               for_node->line);

  if (ret != RETURN_SUCCESS) {
    symtable_free(for_sym);
    return ret;
  }

  // Generator is now defined, use the inner scope

  while (for_data.guards != NULL) {
    ret = check_expr(global_sym, for_data.guards, for_stack, &expr_type);
    if (ret != RETURN_SUCCESS) {
      symtable_free(for_sym);
      return ret;
    }
    for_data.guards = for_data.guards->next;
  }

  if (for_data.body->node_type == NODE_BLOCK) {
    // Cannot use check_block because it adds another scope
    ret = check_statement_list(global_sym, for_data.body->data.block.statements,
                               for_stack);
  } else {
    // One-line body without BLOCK
    ret = check_statement(global_sym, for_data.body, for_stack);
  }

  symtable_free(for_sym);
  return ret;
}

static ReturnCodes check_statement(Symtable *global_sym, ASTNode *statement,
                                   SymtableStack stack) {
  DataType expr_type = UNKNOWN;
  ReturnCodes ret = RETURN_SUCCESS;

  switch (statement->node_type) {
  case NODE_BLOCK: {
    // BLOCK NODE - call check_block()
    ret = check_block(global_sym, statement, stack);
    break;
  }
  case NODE_VARIABLE_DEF: {
    // VARIABLE DEF - check the expression on the right side of the assignment,
    // insert variable into scope
    ASTVarDefNodeData *var_def_data = &statement->data.variable_def;
    ret = check_expr(global_sym, var_def_data->expr, stack, &expr_type);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    ret = symtable_stack_new_var(&stack, var_def_data->name,
                                 var_def_data->specified_type,
                                 var_def_data->variable_type, statement->line);

    break;
  }
  case NODE_ASSIGN: {
    // ASSIGNMENT - check the expression on the right side of the assignment
    ASTAssignNodeData *assign_data = &statement->data.assign;
    ret = check_expr(global_sym, assign_data->right, stack, &expr_type);
    break;
  }
  case NODE_WHILE: {
    // WHILE - check condition expr, check body statement
    ASTWhileNodeData *while_data = &statement->data.while_node;

    ret = check_expr(global_sym, while_data->condition, stack, &expr_type);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    ret = check_statement(global_sym, while_data->body, stack);
    break;
  }
  case NODE_FOR: {
    ret = check_for(global_sym, statement, stack);
    break;
  }
  case NODE_RETURN: {
    // RETURN node - if it has a value, check its expr
    ASTReturnNodeData *return_data = &statement->data.return_node;

    if (return_data->value != NULL) {
      ret = check_expr(global_sym, return_data->value, stack, &expr_type);
    }

    break;
  }
  default:
    // Could be an expression that is meant to be thrown away, if not, it will
    // error in check_expr anyways
    ret = check_expr(global_sym, statement, stack, &expr_type);
    break;
  }

  return ret;
}

static ReturnCodes check_statement_list(Symtable *global_sym,
                                        ASTNode *statement_ll,
                                        SymtableStack stack) {
  ReturnCodes ret = RETURN_SUCCESS;

  while (statement_ll != NULL) {
    // Iterate through all statements, check every statement
    ret = check_statement(global_sym, statement_ll, stack);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    statement_ll = statement_ll->next;
  }

  return ret;
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
    ret = symtable_stack_new_var(&stack, param->name, param->data_type,
                                 FUNC_PARAM, function->line);

    if (ret != RETURN_SUCCESS) {
      break;
    }
  }

  (void)symtable_stack_get;

  if (ret == RETURN_SUCCESS) {
    if (function_data.body->node_type == NODE_BLOCK) {
      ret = check_statement_list(
          global_sym, function_data.body->data.block.statements, stack);
    } else {
      // Could be an expr or an if statement used as an expr
      DataType return_type = UNKNOWN;
      ret = check_expr(global_sym, function_data.body, stack, &return_type);
    }
  }

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
