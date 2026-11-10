/**
 * @file ast.c
 * @brief Implementation of functions which create/destroy Abstract syntax tree
 * nodes
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 31.10.2026
 */

#include "ast.h"
#include "return.h"
#include "symbol.h"
#include <stdlib.h>

ASTNode *ast_init_program(void) {
  ASTNode *program = malloc(sizeof(ASTNode));

  if (program == NULL) {
    print_error("Allocation failed!", 0);
    return NULL;
  }

  *program = (ASTNode){
      .node_type = NODE_PROGRAM,
      .data_type = UNKNOWN,
      .line = 0,
      .next = NULL,
      .data.program.functions = NULL,
  };

  return program;
}

ASTNode *ast_init_func_def(char *name, bool is_main, unsigned int line) {
  ASTNode *func_def = malloc(sizeof(ASTNode));

  if (func_def == NULL) {
    free(name);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTFunctionDefNodeData data = {
      .body = NULL,
      .is_main = is_main,
      .name = name,
      .param_count = 0,
      .params = NULL,
      .return_type = UNKNOWN,
  };

  *func_def = (ASTNode){
      .node_type = NODE_FUNCTION_DEF,
      .data_type = UNKNOWN,
      .line = line,
      .next = NULL,
      .data.function_def = data,
  };

  return func_def;
}

ReturnCodes ast_func_def_add_param(ASTNode *func_def, char *param_name,
                                   DataType data_type) {
  if (func_def == NULL || func_def->node_type != NODE_FUNCTION_DEF) {
    free(param_name);
    print_error("Received NULL pointer or node was not of type FUNCTION_DEF",
                0);
    return ERR_INTERNAL;
  }

  size_t param_count = func_def->data.function_def.param_count;

  FunctionParam *temp = realloc(func_def->data.function_def.params,
                                sizeof(FunctionParam) * (param_count + 1));

  if (temp == NULL) {
    free(param_name);
    print_error("Reallocation failed!", 0);
    return ERR_INTERNAL;
  }

  temp[param_count] =
      (FunctionParam){.name = param_name, .data_type = data_type};

  func_def->data.function_def.params = temp;
  func_def->data.function_def.param_count++;

  return RETURN_SUCCESS;
}

ASTNode *ast_init_block(unsigned int line) {
  ASTNode *block = malloc(sizeof(ASTNode));

  if (block == NULL) {
    print_error("Allocation failed!", 0);
    return NULL;
  }

  *block = (ASTNode){
      .node_type = NODE_BLOCK,
      .data_type = UNKNOWN,
      .line = line,
      .next = NULL,
      .data.block.statements = NULL,
  };

  return block;
}

ASTNode *ast_init_var_def(char *name, VariableType variable_type,
                          DataType specified_type, ASTNode *expr,
                          unsigned int line) {
  ASTNode *var_def = malloc(sizeof(ASTNode));

  if (var_def == NULL) {
    free(name);
    ast_free(expr);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTVarDefNodeData data = {
      .expr = expr,
      .name = name,
      .specified_type = specified_type,
      .variable_type = variable_type,
  };

  *var_def = (ASTNode){
      .node_type = NODE_VARIABLE_DEF,
      .data_type = UNIT, // Ex. type(val x: Int = 2) evaluates to UNIT
      .line = line,
      .next = NULL,
      .data.variable_def = data,
  };

  return var_def;
}

ASTNode *ast_init_assign(char *left, ASTNode *right, unsigned int line) {
  ASTNode *assign = malloc(sizeof(ASTNode));

  if (assign == NULL) {
    free(left);
    ast_free(right);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTAssignNodeData data = {.left = left, .right = right};

  *assign = (ASTNode){
      .node_type = NODE_ASSIGN,
      .data_type = UNIT, // Ex. type(x = 2) evaluates to UNIT
      .line = line,
      .next = NULL,
      .data.assign = data,
  };

  return assign;
}

ASTNode *ast_init_if(ASTNode *condition, ASTNode *then_branch,
                     ASTNode *else_branch, bool is_expr, unsigned int line) {
  ASTNode *if_node = malloc(sizeof(ASTNode));

  if (if_node == NULL) {
    ast_free(condition);
    ast_free(then_branch);
    ast_free(else_branch);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTIfNodeData data = {
      .condition = condition,
      .then_branch = then_branch,
      .else_branch = else_branch,
      .is_expr = is_expr,
  };

  *if_node = (ASTNode){
      .node_type = NODE_IF,
      .data_type =
          is_expr
              ? UNKNOWN
              : UNIT, // if is_expr, evaluates to the then/else branch data type
      .line = line,
      .next = NULL,
      .data.if_node = data,
  };

  return if_node;
}

ASTNode *ast_init_while(ASTNode *condition, ASTNode *body, unsigned int line) {
  ASTNode *while_node = malloc(sizeof(ASTNode));

  if (while_node == NULL) {
    ast_free(condition);
    ast_free(body);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTWhileNodeData data = {.body = body, .condition = condition};

  *while_node = (ASTNode){
      .node_type = NODE_WHILE,
      .data_type = UNIT,
      .line = line,
      .next = NULL,
      .data.while_node = data,
  };

  return while_node;
}

ASTNode *ast_init_for(char *generator_name, ASTNode *range_start,
                      ASTNode *range_end, ASTNode *range_step, ASTNode *body,
                      bool includes_end, unsigned int line) {
  ASTNode *for_node = malloc(sizeof(ASTNode));

  if (for_node == NULL) {
    free(generator_name);
    ast_free(range_start);
    ast_free(range_end);
    ast_free(range_step);
    ast_free(body);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTForNodeData data = {
      .generator_name = generator_name,
      .range_start = range_start,
      .range_end = range_end,
      .range_step = range_step,
      .guards = NULL,
      .body = body,
      .includes_end = includes_end,
  };

  *for_node = (ASTNode){
      .node_type = NODE_FOR,
      .data_type = UNIT,
      .line = line,
      .next = NULL,
      .data.for_node = data,
  };

  return for_node;
}

ASTNode *ast_init_return(ASTNode *value, unsigned int line) {
  ASTNode *return_node = malloc(sizeof(ASTNode));

  if (return_node == NULL) {
    ast_free(value);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  *return_node = (ASTNode){
      .node_type = NODE_RETURN,
      .data_type = UNKNOWN,
      .line = line,
      .next = NULL,
      .data.return_node.value = value,
  };

  return return_node;
}

ASTNode *ast_init_func_call(char *name, unsigned int line) {
  ASTNode *func_call = malloc(sizeof(ASTNode));

  if (func_call == NULL) {
    free(name);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTFunctionCallNodeData data = {.name = name, .args = NULL};

  *func_call = (ASTNode){
      .node_type = NODE_FUNCTION_CALL,
      .data_type = UNKNOWN,
      .line = line,
      .next = NULL,
      .data.function_call = data,
  };

  return func_call;
}

ASTNode *ast_init_binary_op(ASTOperator op, ASTNode *left, ASTNode *right,
                            unsigned int line) {
  ASTNode *binary_op = malloc(sizeof(ASTNode));

  if (binary_op == NULL) {
    ast_free(left);
    ast_free(right);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTBinaryOpNodeData data = {.op = op, .left = left, .right = right};

  *binary_op = (ASTNode){
      .node_type = NODE_BINARY_OP,
      .data_type = UNKNOWN, // Possibly could set BOOLEAN here for && and ||
      .line = line,
      .next = NULL,
      .data.binary_op = data,
  };

  return binary_op;
}

ASTNode *ast_init_unary_op(ASTOperator op, ASTNode *operand,
                           unsigned int line) {
  ASTNode *unary_op = malloc(sizeof(ASTNode));

  if (unary_op == NULL) {
    ast_free(operand);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  ASTUnaryOpNodeData data = {.op = op, .operand = operand};

  *unary_op = (ASTNode){
      .node_type = NODE_UNARY_OP,
      .data_type = UNKNOWN,
      .line = line,
      .next = NULL,
      .data.unary_op = data,
  };

  return unary_op;
}

ASTNode *ast_init_var_ref(char *name, unsigned int line) {
  ASTNode *var_ref = malloc(sizeof(ASTNode));

  if (var_ref == NULL) {
    free(name);
    print_error("Allocation failed!", 0);
    return NULL;
  }

  *var_ref = (ASTNode){
      .node_type = NODE_VARIABLE_REF,
      .data_type = UNKNOWN,
      .line = line,
      .next = NULL,
      .data.variable_ref.name = name,
  };

  return var_ref;
}

ASTNode *ast_init_literal(DataType data_type, ASTLiteralNodeData value,
                          unsigned int line) {
  ASTNode *literal = malloc(sizeof(ASTNode));

  if (literal == NULL) {
    if (data_type == STRING) {
      free(value.string);
    }
    print_error("Allocation failed!", 0);
    return NULL;
  }

  *literal = (ASTNode){
      .node_type = NODE_LITERAL,
      .data_type = data_type,
      .line = line,
      .next = NULL,
      .data.literal = value,
  };

  return literal;
}

void ast_ll_append(ASTNode **list_start, ASTNode *node) {
  if (list_start == NULL || node == NULL) {
    print_error("Received NULL list pointer or node", 0);
    return;
  }

  if (*list_start == NULL) {
    *list_start = node;
    return;
  }

  ASTNode *curr = *list_start;

  while (curr->next != NULL) {
    curr = curr->next;
  }

  curr->next = node;
}

void ast_free(ASTNode *node) {
  // Recursive algorithm
  while (node != NULL) {
    // Free internal node data and children
    switch (node->node_type) {
    case NODE_PROGRAM:
      ast_free(node->data.program.functions);
      break;
    case NODE_FUNCTION_DEF:
      free(node->data.function_def.name);
      for (size_t idx = 0; idx < node->data.function_def.param_count; idx++) {
        free(node->data.function_def.params[idx].name);
      }
      free(node->data.function_def.params);
      ast_free(node->data.function_def.body);
      break;
    case NODE_FUNCTION_CALL:
      free(node->data.function_call.name);
      ast_free(node->data.function_call.args);
      break;
    case NODE_BLOCK:
      ast_free(node->data.block.statements);
      break;
    case NODE_VARIABLE_DEF:
      free(node->data.variable_def.name);
      ast_free(node->data.variable_def.expr);
      break;
    case NODE_VARIABLE_REF:
      free(node->data.variable_ref.name);
      break;
    case NODE_ASSIGN:
      free(node->data.assign.left);
      ast_free(node->data.assign.right);
      break;
    case NODE_IF:
      ast_free(node->data.if_node.condition);
      ast_free(node->data.if_node.then_branch);
      ast_free(node->data.if_node.else_branch);
      break;
    case NODE_RETURN:
      ast_free(node->data.return_node.value);
      break;
    case NODE_WHILE:
      ast_free(node->data.while_node.condition);
      ast_free(node->data.while_node.body);
      break;
    case NODE_FOR:
      ast_free(node->data.for_node.body);
      ast_free(node->data.for_node.range_start);
      ast_free(node->data.for_node.range_end);
      ast_free(node->data.for_node.range_step);
      ast_free(node->data.for_node.guards);
      free(node->data.for_node.generator_name);
      break;
    case NODE_LITERAL:
      if (node->data_type == STRING) {
        free(node->data.literal.string);
      }
      break;
    case NODE_BINARY_OP:
      ast_free(node->data.binary_op.left);
      ast_free(node->data.binary_op.right);
      break;
    case NODE_UNARY_OP:
      ast_free(node->data.unary_op.operand);
      break;
    }

    // Do the same for its siblings
    ASTNode *next = node->next;
    free(node);
    node = next;
  }
}
