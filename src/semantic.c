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
#include <stdbool.h>
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
 * @brief Shared state of the body pass, passed to every check_* function
 */
typedef struct {
  Symtable *global_sym;     // global symtable (functions incl. builtins)
  Symbol *current_function; // the function whose body is being checked
} SemanticContext;

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
 * @param context Pointer to the semantic context (global symtable, current
 * function)
 * @param statement A pointer to the statement's AST node (what's a statement?
 * see func impl)
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @param out Pointer to the out DataType enum - the function sets this to the
 * value of the statement (the type of an expression, UNIT for definitions,
 * assignments and loops)
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_statement(SemanticContext *context, ASTNode *statement,
                                   SymtableStack stack, DataType *out);

/**
 * @brief Decides whether the data type is a number type
 * @param operand The data type of the operand
 * @return true for Int || Double, otherwise false
 */
static bool is_number(DataType operand) {
  return (operand == INT || operand == DOUBLE) != 0;
}

/**
 * @brief Decides whether the unary operator matches the operand type and is
 * therefore allowed (also sets *out to the resulting data type)
 * @param op The unary operator (ASTOperator struct)
 * @param operand_data_type the DataType of the operand
 * @param out The pointer to the resulting data type to be set by this function
 * @return true if allowed, false if this should throw an error with *out set to
 * UNKNOWN
 */
static bool is_correct_unary(ASTOperator op, DataType operand_data_type,
                             DataType *out) {
  *out = UNKNOWN;

  // Until rest of semantic analysis is done
  if (operand_data_type == UNKNOWN) {
    if (op == OP_NOT) {
      *out = BOOLEAN;
    }
    return true;
  }

  if (op == OP_NOT && operand_data_type == BOOLEAN) {
    *out = operand_data_type;
    return true;
  }

  if (op == OP_NEG && is_number(operand_data_type)) {
    *out = operand_data_type;
    return true;
  }

  return false;
}

/**
 * @brief Decides whether a combination of data types and an operator is a valid
 * binary combination or not. Sets the resulting data type to *out
 * @param op An ASTOperator enum
 * @param left The data type of the left expression
 * @param right The data type of the right expression
 * @param out The pointer to the resulting data type to be set by this function
 * @return true if the combination is a correct binary operation, false
 * otherwise and *out is set to UNKNOWN
 */
static bool is_correct_binary(ASTOperator op, DataType left, DataType right,
                              DataType *out) {
  *out = UNKNOWN;

  // Until rest of semantic analysis is done
  if (left == UNKNOWN || right == UNKNOWN) {
    if (op == OP_LT || op == OP_LTE || op == OP_GT || op == OP_GTE ||
        op == OP_EQ || op == OP_NEQ || op == OP_AND || op == OP_OR) {
      *out = BOOLEAN;
    }
    return true;
  }

  switch (op) {
  case OP_ADD:
    // Numbers
    if (is_number(left) && is_number(right)) {
      *out = left;

      if (right == DOUBLE) {
        *out = DOUBLE;
      }
      return true;
    }

    // String, String
    if (left == right && left == STRING) {
      *out = STRING;
      return true;
    }
    break;
  case OP_SUB:
  case OP_MUL:
  case OP_DIV:
    // Numbers
    if (is_number(left) && is_number(right)) {
      *out = left;

      if (right == DOUBLE) {
        *out = DOUBLE;
      }

      return true;
    }
    break;
  case OP_EQ:
  case OP_NEQ:
    // LEFT == RIGHT or numbers
    if ((left == right) || (is_number(left) && is_number(right))) {
      *out = BOOLEAN;
      return true;
    }
    break;
  case OP_LT:
  case OP_GT:
  case OP_LTE:
  case OP_GTE:
    // String, String or numbers
    if ((is_number(left) && is_number(right)) ||
        (left == STRING && right == STRING)) {
      *out = BOOLEAN;
      return true;
    }
    break;
  case OP_AND:
  case OP_OR:
    // Bool, Bool
    if (left == BOOLEAN && right == BOOLEAN) {
      *out = BOOLEAN;
      return true;
    }
    break;
  default:
    break;
  }

  return false;
}

/**
 * @brief Decides whether the 'from' DataType can be assigned to the 'to'
 * DataType
 * @param from The source DataType enum
 * @param to The destination DataType enum
 * @return true if it can, otherwise false
 */
static bool is_assignable(DataType from, DataType to) {
  if (to == ANY) {
    // Any takes any data type
    return true;
  }

  if (from == INT && to == DOUBLE) {
    // INT -> DOUBLE
    return true;
  }

  if (from == to) {
    // FROM == TO
    return true;
  }

  if (from == UNKNOWN || to == UNKNOWN) {
    return true;
  }

  return false;
}

/**
 * @brief Checks if the data types of the 'then' and 'else' branches are
 * compatible. Sets the resulting data type to *out
 * @param then_type The data type of the 'then' branch
 * @param else_type The data type of the 'else' branch
 * @param out The pointer to the resulting data type to be set by this function
 * @return true if types are compatible, false otherwise and *out is set to
 * UNKNOWN
 */
static bool unify_types(DataType then_type, DataType else_type, DataType *out) {
  *out = UNKNOWN;

  if (then_type == else_type) {
    *out = then_type;
    return true;
  }

  if (then_type == UNKNOWN || else_type == UNKNOWN) {
    *out = UNKNOWN;
    return true;
  }

  if ((then_type == INT && else_type == DOUBLE) ||
      (then_type == DOUBLE && else_type == INT)) {
    *out = DOUBLE;
    return true;
  }

  return false;
}

/**
 * @brief Checks an expression ASTNode for semantic errors
 * @param context Pointer to the semantic context (global symtable, current
 * function)
 * @param expr A pointer to the expr's AST node (what's an expr? see func impl)
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @param out Pointer to the out DataType enum - the function sets this to the
 * type the expr evaluates to
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure with *out set to UNKNOWN
 */
static ReturnCodes check_expr(SemanticContext *context, ASTNode *expr,
                              SymtableStack stack, DataType *out) {
  ReturnCodes ret = RETURN_SUCCESS;
  *out = UNKNOWN;

  switch (expr->node_type) {
  case NODE_LITERAL:
    // LITERAL - return its data type
    *out = expr->data_type;
    break;
  case NODE_VARIABLE_REF: {
    // VARIABLE REF - read from all reachable scopes
    ASTVarRefNodeData var_ref_data = expr->data.variable_ref;
    Symbol *var_sym = symtable_stack_get(&stack, var_ref_data.name);

    if (var_sym == NULL) {
      print_error("Semantic error: undefined variable!", expr->line);
      return ERR_SEM_UNDEF;
    }

    *out = var_sym->data.variable.data_type;
    break;
  }
  case NODE_UNARY_OP: {
    // UNARY OP - check expr of operand, check if valid unary op
    ASTUnaryOpNodeData *unary_op_data = &expr->data.unary_op;

    DataType operand_out = UNKNOWN;

    ret = check_expr(context, unary_op_data->operand, stack, &operand_out);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    if (!is_correct_unary(unary_op_data->op, operand_out, out)) {
      print_error("Semantic error: unary operator not allowed on this operand",
                  expr->line);
      return ERR_SEM_TYPE;
    }

    break;
  }
  case NODE_BINARY_OP: {
    // BINARY op - check expr of left and right operands, check if valid binary
    // op
    ASTBinaryOpNodeData *binary_op_data = &expr->data.binary_op;
    DataType out_left = UNKNOWN;
    DataType out_right = UNKNOWN;

    ret = check_expr(context, binary_op_data->left, stack, &out_left);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    ret = check_expr(context, binary_op_data->right, stack, &out_right);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    if (!is_correct_binary(binary_op_data->op, out_left, out_right, out)) {
      print_error(
          "Semantic error: binary operator not allowed with these operands",
          expr->line);
      return ERR_SEM_TYPE;
    }

    break;
  }
  case NODE_FUNCTION_CALL: {
    // FUNCTION CALL - check for function definition in global symtable, check
    // parameter expressions, match parameters to function arguments

    // Not a pointer because the LL advancing needs a copy
    ASTFunctionCallNodeData fun_call_data = expr->data.function_call;

    Symbol *fun_sym = symtable_get(context->global_sym, fun_call_data.name);

    if (fun_sym == NULL) {
      print_error("Semantic error: calling undefined function", expr->line);
      return ERR_SEM_UNDEF;
    }

    *out = fun_sym->data.function.return_type;

    FunctionSymbol *func_data = &fun_sym->data.function;
    size_t arg_idx = 0;

    while (fun_call_data.args != NULL) {
      DataType arg_out = UNKNOWN;
      ret = check_expr(context, fun_call_data.args, stack, &arg_out);

      if (ret != RETURN_SUCCESS) {
        return ret;
      }

      if (arg_idx >= func_data->param_count) {
        print_error("Semantic error: too many arguments supplied to function!",
                    expr->line);
        return ERR_SEM_ARG;
      }

      FunctionParam param = func_data->params[arg_idx];

      if (!is_assignable(arg_out, param.data_type)) {
        print_error("Semantic error: mismatch between function argument types!",
                    expr->line);
        return ERR_SEM_ARG;
      }

      fun_call_data.args = fun_call_data.args->next;
      arg_idx++;
    }

    if (arg_idx != func_data->param_count) {
      print_error("Semantic error: too little arguments supplied to function!",
                  expr->line);
      return ERR_SEM_ARG;
    }

    break;
  }
  case NODE_IF: {
    // IF - check condition (including type), then branch and else branch (if it
    // exists)
    ASTIfNodeData *if_data = &expr->data.if_node;
    DataType condition_out = UNKNOWN;
    DataType then_type = UNKNOWN;
    DataType else_type = UNKNOWN;

    ret = check_expr(context, if_data->condition, stack, &condition_out);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    // The IF condition expression must evaluate to BOOLEAN if not UNKNOWN
    if (condition_out != BOOLEAN && condition_out != UNKNOWN) {
      print_error("Semantic error: if condition doesn't evaluate to boolean!",
                  expr->line);
      return ERR_SEM_TYPE;
    }
    ret = check_statement(context, if_data->then_branch, stack, &then_type);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    if (if_data->else_branch != NULL) {
      ret = check_statement(context, if_data->else_branch, stack, &else_type);
    }

    if (ret != RETURN_SUCCESS) {
      break;
    }

    if (if_data->else_branch == NULL) {
      *out = UNIT;
    } else if (!unify_types(then_type, else_type, out)) {
      if (if_data->is_expr) {
        print_error(
            "Semantic error: if expression branches have incompatible types!",
            expr->line);
        return ERR_SEM_TYPE;
      }
      // if is statement
      *out = UNIT;
    }
    // There is no need for checking if unify_types is true
    // the result is already in *out

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
 * @param context Pointer to the semantic context (global symtable, current
 * function)
 * @param statement_ll The starting element of the linked list of statement
 * nodes
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @param out Pointer to the out DataType enum - the function sets this to the
 * value of the last statement in the list
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_statement_list(SemanticContext *context,
                                        ASTNode *statement_ll,
                                        SymtableStack stack, DataType *out);

/**
 * @brief Initiates a new symtable scope and calls check_statement_list() on
 * the block statements
 * @param context Pointer to the semantic context (global symtable, current
 * function)
 * @param block A pointer to the block ASTNode
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @param out Pointer to the out DataType enum - the function sets this to the
 * value of the block (its last statement)
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_block(SemanticContext *context, ASTNode *block,
                               SymtableStack stack, DataType *out) {
  // BLOCK - Push new scope, check statement list

  *out = UNKNOWN;
  Symtable *block_sym = symtable_init();

  if (block_sym == NULL) {
    return ERR_INTERNAL;
  }

  SymtableStack block_stack = {.sym = block_sym, .next = &stack};

  ASTBlockNodeData *block_data = &block->data.block;

  ReturnCodes ret =
      check_statement_list(context, block_data->statements, block_stack, out);

  symtable_free(block_sym);
  return ret;
}

/**
 * @brief Checks the for loop node for semantic errors. Initiates a new inner
 * scope with the for loop generator variable in it
 * @param context Pointer to the semantic context (global symtable, current
 * function)
 * @param for_node A pointer to the for ASTNode
 * @param stack The stack of local(scoped) Symtables - the first item is the
 * topmost scope, the last is the function scope
 * @return RETURN_SUCCESS on success, else one of the many possible semantic
 * errors or ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_for(SemanticContext *context, ASTNode *for_node,
                             SymtableStack stack) {
  // FOR - Check range start, range end, optionally range step,
  // then push new scope, then check guards and body

  ReturnCodes ret = RETURN_SUCCESS;
  ASTForNodeData for_data = for_node->data.for_node;
  DataType expr_type = UNKNOWN;

  ret = check_expr(context, for_data.range_start, stack, &expr_type);

  if (ret != RETURN_SUCCESS) {
    return ret;
  }

  if (expr_type != INT && expr_type != UNKNOWN) {
    print_error("Semantic error: for range start doesn't evaluate to integer!",
                for_data.range_start->line);
    return ERR_SEM_TYPE;
  }

  ret = check_expr(context, for_data.range_end, stack, &expr_type);

  if (ret != RETURN_SUCCESS) {
    return ret;
  }

  if (expr_type != INT && expr_type != UNKNOWN) {
    print_error("Semantic error: for range end doesn't evaluate to integer!",
                for_data.range_end->line);
    return ERR_SEM_TYPE;
  }

  if (for_data.range_step != NULL) {
    ret = check_expr(context, for_data.range_step, stack, &expr_type);
    if (ret != RETURN_SUCCESS) {
      return ret;
    }
    if (expr_type != INT && expr_type != UNKNOWN) {
      print_error("Semantic error: for range step doesn't evaluate to integer!",
                  for_data.range_step->line);
      return ERR_SEM_TYPE;
    }
    if (for_data.range_step->node_type == NODE_LITERAL && expr_type == INT &&
        for_data.range_step->data.literal.int_val == 0) {
      print_error("Semantic error: for range step equals to 0!",
                  for_data.range_step->line);
      return ERR_SEM_OTHER;
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
    ret = check_expr(context, for_data.guards, for_stack, &expr_type);
    if (ret != RETURN_SUCCESS) {
      symtable_free(for_sym);
      return ret;
    }
    if (expr_type != BOOLEAN && expr_type != UNKNOWN) {
      print_error("Semantic error: for guards don't evaluate to boolean!",
                  for_data.guards->line);
      symtable_free(for_sym);
      return ERR_SEM_TYPE;
    }
    for_data.guards = for_data.guards->next;
  }

  //  Loop body value is discarded (spec 2.3 rule 2)
  DataType body_type = UNKNOWN;
  if (for_data.body->node_type == NODE_BLOCK) {
    // Cannot use check_block because it adds another scope
    ret = check_statement_list(context, for_data.body->data.block.statements,
                               for_stack, &body_type);
  } else {
    // One-line body without BLOCK
    ret = check_statement(context, for_data.body, for_stack, &body_type);
  }

  symtable_free(for_sym);
  return ret;
}

static ReturnCodes check_statement(SemanticContext *context, ASTNode *statement,
                                   SymtableStack stack, DataType *out) {
  DataType expr_type = UNKNOWN;
  ReturnCodes ret = RETURN_SUCCESS;
  *out = UNKNOWN;

  switch (statement->node_type) {
  case NODE_BLOCK: {
    // BLOCK NODE - call check_block()
    ret = check_block(context, statement, stack, out);
    break;
  }
  case NODE_VARIABLE_DEF: {
    // VARIABLE DEF - check the expression on the right side of the assignment,
    // verify if type matches, insert variable into scope
    ASTVarDefNodeData *var_def_data = &statement->data.variable_def;
    *out = UNIT;
    ret = check_expr(context, var_def_data->expr, stack, &expr_type);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    if (!is_assignable(expr_type, var_def_data->specified_type)) {
      print_error("Semantic error: type mismatch between variable and its "
                  "initial value!",
                  statement->line);
      return ERR_SEM_TYPE;
    }

    DataType result_type = var_def_data->specified_type != UNKNOWN
                               ? var_def_data->specified_type
                               : expr_type;

    ret = symtable_stack_new_var(&stack, var_def_data->name, result_type,
                                 var_def_data->variable_type, statement->line);

    break;
  }
  case NODE_ASSIGN: {
    // ASSIGNMENT - check the expression on the right side of the assignment and
    // check the left side for definition within scope and mutability, then
    // check if right is assignable to left
    ASTAssignNodeData *assign_data = &statement->data.assign;
    *out = UNIT;
    ret = check_expr(context, assign_data->right, stack, &expr_type);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    Symbol *left_sym = symtable_stack_get(&stack, assign_data->left);

    if (left_sym == NULL) {
      print_error("Semantic error: assigning to undefined variable!",
                  statement->line);
      // Could also be ERR_SEM_UNDEF, but this fits as well and is a higher
      // priority return
      return ERR_SEM_NONMODIFIABLE;
    }

    if (left_sym->data.variable.variable_type != VAR) {
      print_error("Semantic error: assigning to unmodifiable variable "
                  "(function param/val)!",
                  statement->line);
      return ERR_SEM_NONMODIFIABLE;
    }

    VariableSymbol var_data = left_sym->data.variable;

    if (!is_assignable(expr_type, var_data.data_type)) {
      print_error("Semantic error: type mismatch between variable type and "
                  "right side of assignment!",
                  statement->line);
      return ERR_SEM_TYPE;
    }

    break;
  }
  case NODE_WHILE: {
    // WHILE - check condition expr, check body statement
    ASTWhileNodeData *while_data = &statement->data.while_node;
    *out = UNIT;
    ret = check_expr(context, while_data->condition, stack, &expr_type);

    if (ret != RETURN_SUCCESS) {
      break;
    }

    if (expr_type != BOOLEAN && expr_type != UNKNOWN) {
      print_error(
          "Semantic error: while condition doesn't evaluate to boolean!",
          statement->line);
      return ERR_SEM_TYPE;
    }

    DataType body_type = UNKNOWN;
    ret = check_statement(context, while_data->body, stack, &body_type);
    break;
  }
  case NODE_FOR: {
    ret = check_for(context, statement, stack);
    *out = UNIT;
    break;
  }
  case NODE_RETURN: {
    // RETURN node - if it has a value, check its expr
    ASTReturnNodeData *return_data = &statement->data.return_node;
    FunctionSymbol *func_data = &context->current_function->data.function;

    if (return_data->value != NULL) {
      ret = check_expr(context, return_data->value, stack, &expr_type);
    }

    if (ret != RETURN_SUCCESS) {
      break;
    }

    if (!func_data->return_type_defined) {
      print_error("Semantic error: return used in a function without an "
                  "explicit return type!",
                  statement->line);
      return ERR_SEM_TYPE;
    }

    if (func_data->return_type != UNIT) {
      if (return_data->value == NULL) {
        print_error("Semantic error: return doesn't have value!",
                    statement->line);
        return ERR_SEM_TYPE;
      }

      if (!is_assignable(expr_type, func_data->return_type)) {
        print_error("Semantic error: return type isn't assignable!",
                    statement->line);
        return ERR_SEM_TYPE;
      }
    }

    *out = func_data->return_type;
    break;
  }
  default:
    // Could be an expression that is meant to be thrown away, if not, it will
    // error in check_expr anyways
    ret = check_expr(context, statement, stack, &expr_type);
    *out = expr_type;
    break;
  }

  return ret;
}

static ReturnCodes check_statement_list(SemanticContext *context,
                                        ASTNode *statement_ll,
                                        SymtableStack stack, DataType *out) {
  ReturnCodes ret = RETURN_SUCCESS;

  *out = UNKNOWN;

  while (statement_ll != NULL) {
    // Iterate through all statements, check every statement
    ret = check_statement(context, statement_ll, stack, out);

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
 * @param context Pointer to the semantic context (global symtable, current
 * function)
 * @param function A pointer to the function ASTNode struct
 * @return RETURN_SUCCESS on success, or one of the semantic errors or
 * ERR_INTERNAL on memory allocation failure
 */
static ReturnCodes check_function(SemanticContext *context, ASTNode *function) {
  ReturnCodes ret = RETURN_SUCCESS;
  Symtable *function_symtable = symtable_init();

  if (function_symtable == NULL) {
    return ERR_INTERNAL;
  }

  SymtableStack stack = {.sym = function_symtable};

  ASTFunctionDefNodeData function_data = function->data.function_def;

  context->current_function =
      symtable_get(context->global_sym, function_data.name);

  for (size_t param_idx = 0; param_idx < function_data.param_count;
       param_idx++) {
    FunctionParam *param = &function_data.params[param_idx];
    ret = symtable_stack_new_var(&stack, param->name, param->data_type,
                                 FUNC_PARAM, function->line);

    if (ret != RETURN_SUCCESS) {
      break;
    }
  }

  DataType body_type = UNKNOWN;

  if (ret == RETURN_SUCCESS) {
    if (function_data.body->node_type == NODE_BLOCK) {
      ret = check_statement_list(context,
                                 function_data.body->data.block.statements,
                                 stack, &body_type);
    } else {
      // Could be an expr or an if statement used as an expr
      ret = check_expr(context, function_data.body, stack, &body_type);
    }
  }

  if (ret == RETURN_SUCCESS) {
    if (function_data.return_type != UNKNOWN &&
        function_data.return_type != UNIT) {
      if (!is_assignable(body_type, function_data.return_type)) {
        print_error("Semantic error: function body value doesn't match the "
                    "function's return type!",
                    function->line);
        ret = ERR_SEM_TYPE;
      }
    }
  }

  if (ret == RETURN_SUCCESS) {
    FunctionSymbol *func_data = &context->current_function->data.function;

    if (!func_data->return_type_defined) {
      func_data->return_type = body_type;

      if (func_data->is_main && body_type != UNIT && body_type != UNKNOWN) {
        print_error(
            "Semantic error: function annotated with @main has an inferred "
            "return type other than UNIT!",
            function->line);
        ret = ERR_SEM_TYPE;
      }
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
  SemanticContext context = {.global_sym = global_sym,
                             .current_function = NULL};
  ASTNode *function = program->data.program.functions;

  while (function != NULL) {
    ret = check_function(&context, function);

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
