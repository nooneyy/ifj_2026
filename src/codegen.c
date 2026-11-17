/**
 * @file codegen.c
 * @brief Implementation of the IFJcode2026 code generator
 * @author Tadeáš Čupr <xcuprta00@stud.fit.vut.cz>
 * @date 04.11.2026
 */

#include "codegen.h"
#include "ast.h"
#include "return.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *name;
  DataType type;
  int uid;
  int scope_depth;
} LocalVarEntry;

typedef struct {
  FILE *out;
  int label_counter;
  int tmp_counter;
  ASTNode *program;
  LocalVarEntry *vars;
  size_t var_count;
  size_t var_cap;
  int scope_depth;
  int next_var_uid;
  bool has_error;
} CodeGenContext;

static const char *binop_instr_table[] = {
    [OP_SUB] = "SUB", [OP_MUL] = "MUL", [OP_EQ] = "EQ", [OP_LT] = "LT",
    [OP_GT] = "GT",   [OP_AND] = "AND", [OP_OR] = "OR",
};

static const char *unop_instr_table[] = {
    [OP_NOT] = "NOT",
};

#define BINOP_TABLE_SIZE                                                       \
  (sizeof(binop_instr_table) / sizeof(binop_instr_table[0]))
#define UNOP_TABLE_SIZE (sizeof(unop_instr_table) / sizeof(unop_instr_table[0]))

static bool find_main_function(ASTNode *program, char **out) {
  *out = NULL;
  ASTNode *function = program->data.program.functions;

  while (function != NULL) {
    ASTFunctionDefNodeData *func_data = &function->data.function_def;
    if (func_data->is_main) {
      *out = func_data->name;
      return true;
    }
    function = function->next;
  }

  return false;
}

static bool generate_prolog(CodeGenContext *ctx) {
  char *main_name = NULL;
  if (!find_main_function(ctx->program, &main_name)) {
    return false;
  }

  fprintf(ctx->out, ".IFJcode26\n");
  fprintf(ctx->out, "CALL $%s\n", main_name);
  fprintf(ctx->out, "EXIT int@0\n\n");

  return true;
}

static void generate_builtin_library(CodeGenContext *ctx) {
  fprintf(ctx->out, "\n\n");

  fprintf(ctx->out, "LABEL $ifj_print\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@val\n");
  fprintf(ctx->out, "POPS LF@val\n");
  fprintf(ctx->out, "JUMPIFEQ $input_is_nil LF@val nil@nil\n");
  fprintf(ctx->out, "WRITE LF@val\n");
  fprintf(ctx->out, "JUMP $print_jump_end\n");
  fprintf(ctx->out, "LABEL $input_is_nil\n");
  fprintf(ctx->out, "WRITE string@()\n");
  fprintf(ctx->out, "LABEL $print_jump_end\n");
  fprintf(ctx->out, "PUSHS nil@nil\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_readLine\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "READ LF@res string\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_readInt\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "READ LF@res int\n");
  fprintf(ctx->out, "JUMPIFNEQ $ifj_readInt_end LF@res nil@nil\n");
  fprintf(ctx->out, "MOVE LF@res int@0\n");
  fprintf(ctx->out, "LABEL $ifj_readInt_end\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_readDouble\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "READ LF@res float\n");
  fprintf(ctx->out, "JUMPIFNEQ $ifj_readDouble_end LF@res nil@nil\n");
  fprintf(ctx->out, "MOVE LF@res float@0x0p+0\n");
  fprintf(ctx->out, "LABEL $ifj_readDouble_end\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_strLength\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@s\n");
  fprintf(ctx->out, "POPS LF@s\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "STRLEN LF@res LF@s\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_toStr\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@val\n");
  fprintf(ctx->out, "POPS LF@val\n");
  fprintf(ctx->out, "DEFVAR LF@type\n");
  fprintf(ctx->out, "TYPE LF@type LF@val\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "DEFVAR LF@cond\n");
  fprintf(ctx->out, "EQ LF@cond LF@type string@int\n");
  fprintf(ctx->out, "JUMPIFEQ $toStr_int LF@cond bool@true\n");
  fprintf(ctx->out, "EQ LF@cond LF@type string@float\n");
  fprintf(ctx->out, "JUMPIFEQ $toStr_float LF@cond bool@true\n");
  fprintf(ctx->out, "JUMPIFEQ $ifj_toStr_unit LF@type string@nil\n");
  fprintf(ctx->out, "JUMPIFEQ $ifj_toStr_bool LF@type string@bool\n");
  fprintf(ctx->out, "MOVE LF@res LF@val\n");
  fprintf(ctx->out, "JUMP $toStr_end\n");
  fprintf(ctx->out, "LABEL $toStr_int\n");
  fprintf(ctx->out, "INT2STR LF@res LF@val\n");
  fprintf(ctx->out, "JUMP $toStr_end\n");
  fprintf(ctx->out, "LABEL $toStr_float\n");
  fprintf(ctx->out, "FLOAT2STR LF@res LF@val\n");
  fprintf(ctx->out, "JUMP $toStr_end\n");
  fprintf(ctx->out, "LABEL $ifj_toStr_unit\n");
  fprintf(ctx->out, "MOVE LF@res string@()\n");
  fprintf(ctx->out, "JUMP $toStr_end\n");
  fprintf(ctx->out, "LABEL $ifj_toStr_bool\n");
  fprintf(ctx->out, "MOVE LF@res string@true\n");
  fprintf(ctx->out, "JUMPIFEQ $toStr_end LF@val bool@true\n");
  fprintf(ctx->out, "MOVE LF@res string@false\n");
  fprintf(ctx->out, "LABEL $toStr_end\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_toInt\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@val\n");
  fprintf(ctx->out, "POPS LF@val\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "FLOAT2INT LF@res LF@val\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_subStr\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@j\n");
  fprintf(ctx->out, "POPS LF@j\n");
  fprintf(ctx->out, "DEFVAR LF@i\n");
  fprintf(ctx->out, "POPS LF@i\n");
  fprintf(ctx->out, "DEFVAR LF@s\n");
  fprintf(ctx->out, "POPS LF@s\n");
  fprintf(ctx->out, "DEFVAR LF@len\n");
  fprintf(ctx->out, "STRLEN LF@len LF@s\n");
  fprintf(ctx->out, "DEFVAR LF@cond\n");
  fprintf(ctx->out, "LT LF@cond LF@i int@0\n");
  fprintf(ctx->out, "JUMPIFEQ $subStr_err LF@cond bool@true\n");
  fprintf(ctx->out, "LT LF@cond LF@j int@0\n");
  fprintf(ctx->out, "JUMPIFEQ $subStr_err LF@cond bool@true\n");
  fprintf(ctx->out, "GT LF@cond LF@i LF@j\n");
  fprintf(ctx->out, "JUMPIFEQ $subStr_err LF@cond bool@true\n");
  fprintf(ctx->out, "LT LF@cond LF@i LF@len\n");
  fprintf(ctx->out, "JUMPIFNEQ $subStr_err LF@cond bool@true\n");
  fprintf(ctx->out, "GT LF@cond LF@j LF@len\n");
  fprintf(ctx->out, "JUMPIFEQ $subStr_err LF@cond bool@true\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "MOVE LF@res string@\n");
  fprintf(ctx->out, "DEFVAR LF@k\n");
  fprintf(ctx->out, "MOVE LF@k LF@i\n");
  fprintf(ctx->out, "DEFVAR LF@char\n");
  fprintf(ctx->out, "LABEL $subStr_loop\n");
  fprintf(ctx->out, "EQ LF@cond LF@k LF@j\n");
  fprintf(ctx->out, "JUMPIFEQ $subStr_done LF@cond bool@true\n");
  fprintf(ctx->out, "GETCHAR LF@char LF@s LF@k\n");
  fprintf(ctx->out, "CONCAT LF@res LF@res LF@char\n");
  fprintf(ctx->out, "ADD LF@k LF@k int@1\n");
  fprintf(ctx->out, "JUMP $subStr_loop\n");
  fprintf(ctx->out, "LABEL $subStr_err\n");
  fprintf(ctx->out, "EXIT int@40\n");
  fprintf(ctx->out, "LABEL $subStr_done\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_ord\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@i\n");
  fprintf(ctx->out, "POPS LF@i\n");
  fprintf(ctx->out, "DEFVAR LF@s\n");
  fprintf(ctx->out, "POPS LF@s\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "DEFVAR LF@len\n");
  fprintf(ctx->out, "STRLEN LF@len LF@s\n");
  fprintf(ctx->out, "DEFVAR LF@cond\n");
  fprintf(ctx->out, "LT LF@cond LF@i int@0\n");
  fprintf(ctx->out, "JUMPIFEQ $ord_invalid LF@cond bool@true\n");
  fprintf(ctx->out, "LT LF@cond LF@i LF@len\n");
  fprintf(ctx->out, "JUMPIFEQ $ord_valid LF@cond bool@true\n");
  fprintf(ctx->out, "LABEL $ord_invalid\n");
  fprintf(ctx->out, "MOVE LF@res int@-1\n");
  fprintf(ctx->out, "JUMP $ord_end\n");
  fprintf(ctx->out, "LABEL $ord_valid\n");
  fprintf(ctx->out, "STRI2INT LF@res LF@s LF@i\n");
  fprintf(ctx->out, "LABEL $ord_end\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_chr\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@i\n");
  fprintf(ctx->out, "POPS LF@i\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "INT2CHAR LF@res LF@i\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_readBoolean\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "READ LF@res bool\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  return;
}

static bool is_expression_node(const ASTNode *node) {
  if (node == NULL)
    return false;
  switch (node->node_type) {
  case NODE_LITERAL:
  case NODE_VARIABLE_REF:
  case NODE_UNARY_OP:
  case NODE_BINARY_OP:
  case NODE_FUNCTION_CALL:
    return true;
  case NODE_IF:
  case NODE_WHILE:
    return false;
  default:
    return false;
  }
}

static bool is_builtin_func(const char *name) {
  static const char *builtins[] = {
      "print",     "readLine", "readInt", "readDouble", "toStr",       "toInt",
      "strLength", "subStr",   "ord",     "chr",        "readBoolean", NULL,
  };

  for (int i = 0; builtins[i] != NULL; i++) {
    if (strcmp(name, builtins[i]) == 0) {
      return true;
    }
  }
  return false;
}

static void print_escaped_string(CodeGenContext *ctx, const char *str) {
  fprintf(ctx->out, "string@");
  if (str == NULL)
    return;

  for (size_t i = 0; str[i] != '\0'; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c <= 32 || c == 35 || c == 92) {
      fprintf(ctx->out, "\\%03d", c);
    } else {
      fputc(c, ctx->out);
    }
  }

  return;
}

static void new_tmp_var(CodeGenContext *ctx, char buf[32]) {
  int id = ctx->tmp_counter++;
  snprintf(buf, 32, "$tmp_%d", id);

  return;
}

static void scope_enter(CodeGenContext *ctx) { ctx->scope_depth++; }

static void scope_leave(CodeGenContext *ctx) {
  if (ctx->scope_depth > 0) {
    ctx->scope_depth--;
  }
  while (ctx->var_count > 0 &&
         ctx->vars[ctx->var_count - 1].scope_depth > ctx->scope_depth) {
    ctx->var_count--;
  }
}

static int scope_push_var(CodeGenContext *ctx, const char *name,
                          DataType type) {
  if (ctx->var_count >= ctx->var_cap) {
    size_t new_cap = (ctx->var_cap == 0) ? 16 : ctx->var_cap * 2;
    LocalVarEntry *tmp = realloc(ctx->vars, new_cap * sizeof(LocalVarEntry));
    if (tmp == NULL) {
      ctx->has_error = true;
      return -1;
    }
    ctx->vars = tmp;
    ctx->var_cap = new_cap;
  }

  int uid = ctx->next_var_uid++;
  ctx->vars[ctx->var_count++] = (LocalVarEntry){
      .name = name,
      .type = type,
      .uid = uid,
      .scope_depth = ctx->scope_depth,
  };
  return uid;
}

static int count_ast_nodes(ASTNode *node) {
  int count = 0;
  while (node != NULL) {
    count += 2;
    switch (node->node_type) {
    case NODE_VARIABLE_DEF:
      count += count_ast_nodes(node->data.variable_def.expr);
      break;
    case NODE_ASSIGN:
      count += count_ast_nodes(node->data.assign.right);
      break;
    case NODE_UNARY_OP:
      count += count_ast_nodes(node->data.unary_op.operand);
      break;
    case NODE_BINARY_OP:
      count += count_ast_nodes(node->data.binary_op.left);
      count += count_ast_nodes(node->data.binary_op.right);
      break;
    case NODE_FUNCTION_CALL:
      count += count_ast_nodes(node->data.function_call.args);
      break;
    case NODE_RETURN:
      count += count_ast_nodes(node->data.return_node.value);
      break;
    case NODE_BLOCK:
      count += count_ast_nodes(node->data.block.statements);
      break;
    case NODE_IF:
      count += count_ast_nodes(node->data.if_node.condition);
      count += count_ast_nodes(node->data.if_node.then_branch);
      count += count_ast_nodes(node->data.if_node.else_branch);
      break;
    case NODE_WHILE:
      count += count_ast_nodes(node->data.while_node.condition);
      count += count_ast_nodes(node->data.while_node.body);
      break;
    default:
      break;
    }
    node = node->next;
  }
  return count;
}

static const LocalVarEntry *scope_lookup(const CodeGenContext *ctx,
                                         const char *name) {
  for (size_t i = ctx->var_count; i > 0; i--) {
    if (strcmp(ctx->vars[i - 1].name, name) == 0) {
      return &ctx->vars[i - 1];
    }
  }
  return NULL;
}

static ASTFunctionDefNodeData *find_function_def(ASTNode *program,
                                                 const char *name) {
  ASTNode *fn = program->data.program.functions;
  while (fn != NULL) {
    if (fn->node_type == NODE_FUNCTION_DEF &&
        strcmp(fn->data.function_def.name, name) == 0) {
      return &fn->data.function_def;
    }
    fn = fn->next;
  }

  return NULL;
}

static void emit_var_defs_walk(CodeGenContext *ctx, ASTNode *node,
                               int *uid_counter) {
  while (node != NULL) {
    switch (node->node_type) {
    case NODE_VARIABLE_DEF:
      fprintf(ctx->out, "DEFVAR LF@%s$%d\n", node->data.variable_def.name,
              (*uid_counter)++);
      if (node->data.variable_def.expr != NULL) {
        emit_var_defs_walk(ctx, node->data.variable_def.expr, uid_counter);
      }
      break;
    case NODE_BLOCK:
      emit_var_defs_walk(ctx, node->data.block.statements, uid_counter);
      break;
    case NODE_IF:
      emit_var_defs_walk(ctx, node->data.if_node.then_branch, uid_counter);
      emit_var_defs_walk(ctx, node->data.if_node.else_branch, uid_counter);
      break;
    case NODE_WHILE:
      emit_var_defs_walk(ctx, node->data.while_node.body, uid_counter);
      break;
    default:
      break;
    }
    node = node->next;
  }
}

static void generate_expression(CodeGenContext *ctx, ASTNode *expr,
                                const char *dest_var) {
  if (expr == NULL)
    return;

  switch (expr->node_type) {
  case NODE_LITERAL:
    if (dest_var == NULL)
      break;
    switch (expr->data_type) {
    case INT:
      fprintf(ctx->out, "MOVE LF@%s int@%lld\n", dest_var,
              expr->data.literal.int_val);
      break;
    case DOUBLE:
      fprintf(ctx->out, "MOVE LF@%s float@%a\n", dest_var,
              expr->data.literal.dbl_val);
      break;
    case STRING:
      fprintf(ctx->out, "MOVE LF@%s ", dest_var);
      print_escaped_string(ctx, expr->data.literal.string);
      fprintf(ctx->out, "\n");
      break;
    case BOOLEAN:
      fprintf(ctx->out, "MOVE LF@%s bool@%s\n", dest_var,
              expr->data.literal.bool_val ? "true" : "false");
      break;
    case UNIT:
      fprintf(ctx->out, "MOVE LF@%s nil@nil\n", dest_var);
      break;
    default:
      break;
    }
    break;

  case NODE_VARIABLE_REF:
    if (dest_var != NULL) {
      const LocalVarEntry *entry =
          scope_lookup(ctx, expr->data.variable_ref.name);
      if (entry != NULL) {
        fprintf(ctx->out, "MOVE LF@%s LF@%s$%d\n", dest_var, entry->name,
                entry->uid);
      }
    }
    break;

  case NODE_UNARY_OP: {
    char op_tmp[32];
    new_tmp_var(ctx, op_tmp);

    generate_expression(ctx, expr->data.unary_op.operand, op_tmp);

    if (dest_var != NULL) {
      ASTOperator op = expr->data.unary_op.op;
      if (op == OP_NEG) {
        const char *zero =
            (expr->data_type == DOUBLE) ? "float@0x0p+0" : "int@0";
        fprintf(ctx->out, "SUB LF@%s %s LF@%s\n", dest_var, zero, op_tmp);
      } else if (op < UNOP_TABLE_SIZE && unop_instr_table[op] != NULL) {
        fprintf(ctx->out, "%s LF@%s LF@%s\n", unop_instr_table[op], dest_var,
                op_tmp);
      }
    }
    break;
  }

  case NODE_BINARY_OP: {
    char left_tmp[32];
    new_tmp_var(ctx, left_tmp);
    char right_tmp[32];
    new_tmp_var(ctx, right_tmp);

    generate_expression(ctx, expr->data.binary_op.left, left_tmp);
    generate_expression(ctx, expr->data.binary_op.right, right_tmp);

    if (expr->data.binary_op.left->data_type !=
        expr->data.binary_op.right->data_type) {
      if (expr->data.binary_op.left->data_type == INT) {
        fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", left_tmp, left_tmp);
      }

      if (expr->data.binary_op.right->data_type == INT) {
        fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", right_tmp, right_tmp);
      }
    }

    if (dest_var != NULL) {
      ASTOperator op = expr->data.binary_op.op;
      const char *inst;

      switch (op) {
      case OP_ADD:
        inst = (expr->data_type == STRING) ? "CONCAT" : "ADD";
        fprintf(ctx->out, "%s LF@%s LF@%s LF@%s\n", inst, dest_var, left_tmp,
                right_tmp);
        break;
      case OP_DIV: {
        inst = (expr->data_type == INT) ? "IDIV" : "DIV";
        fprintf(ctx->out, "%s LF@%s LF@%s LF@%s\n", inst, dest_var, left_tmp,
                right_tmp);
        break;
      }
      case OP_NEQ:
        fprintf(ctx->out, "EQ LF@%s LF@%s LF@%s\n", dest_var, left_tmp,
                right_tmp);
        fprintf(ctx->out, "NOT LF@%s LF@%s\n", dest_var, dest_var);
        break;
      case OP_LTE:
        fprintf(ctx->out, "GT LF@%s LF@%s LF@%s\n", dest_var, left_tmp,
                right_tmp);
        fprintf(ctx->out, "NOT LF@%s LF@%s\n", dest_var, dest_var);
        break;
      case OP_GTE:
        fprintf(ctx->out, "LT LF@%s LF@%s LF@%s\n", dest_var, left_tmp,
                right_tmp);
        fprintf(ctx->out, "NOT LF@%s LF@%s\n", dest_var, dest_var);
        break;
      default:
        if (op < BINOP_TABLE_SIZE && binop_instr_table[op] != NULL) {
          fprintf(ctx->out, "%s LF@%s LF@%s LF@%s\n", binop_instr_table[op],
                  dest_var, left_tmp, right_tmp);
        }
        break;
      }
    }

    break;
  }

  case NODE_FUNCTION_CALL: {
    ASTFunctionDefNodeData *callee =
        find_function_def(ctx->program, expr->data.function_call.name);
    ASTNode *arg = expr->data.function_call.args;
    size_t arg_idx = 0;

    while (arg != NULL) {
      char arg_tmp[32];
      new_tmp_var(ctx, arg_tmp);

      generate_expression(ctx, arg, arg_tmp);
      if (callee != NULL && arg_idx < callee->param_count &&
          callee->params[arg_idx].data_type == DOUBLE &&
          arg->data_type == INT) {
        fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", arg_tmp, arg_tmp);
      }
      fprintf(ctx->out, "PUSHS LF@%s\n", arg_tmp);
      arg = arg->next;
      arg_idx++;
    }

    const char *name = expr->data.function_call.name;
    if (strncmp(name, "ifj.", 4) == 0) {
      fprintf(ctx->out, "CALL $ifj_%s\n", name + 4);
    } else if (is_builtin_func(name)) {
      fprintf(ctx->out, "CALL $ifj_%s\n", name);
    } else {
      fprintf(ctx->out, "CALL $%s\n", name);
    }

    if (dest_var != NULL) {
      fprintf(ctx->out, "POPS LF@%s\n", dest_var);
    } else {
      char dummy[32];
      new_tmp_var(ctx, dummy);

      fprintf(ctx->out, "POPS LF@%s\n", dummy);
    }
    break;
  }

  default:
    break;
  }
  return;
}

static void emit_return_expr(CodeGenContext *ctx, ASTNode *expr,
                             DataType expected_ret_type) {
  if (expr == NULL || expected_ret_type == UNIT) {
    if (expr != NULL) {
      generate_expression(ctx, expr, NULL);
    }
    fprintf(ctx->out, "PUSHS nil@nil\n");
    return;
  }

  char ret_tmp[32];
  new_tmp_var(ctx, ret_tmp);

  generate_expression(ctx, expr, ret_tmp);

  if (expected_ret_type == DOUBLE && expr->data_type == INT) {
    fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", ret_tmp, ret_tmp);
  }

  fprintf(ctx->out, "PUSHS LF@%s\n", ret_tmp);

  return;
}

static void generate_statement(CodeGenContext *ctx, ASTNode *statement,
                               DataType func_ret_type) {
  if (statement == NULL)
    return;

  switch (statement->node_type) {
  case NODE_VARIABLE_DEF: {
    ASTVarDefNodeData *def = &statement->data.variable_def;
    DataType var_type =
        (def->specified_type != UNKNOWN)
            ? def->specified_type
            : (def->expr != NULL ? def->expr->data_type : UNKNOWN);
    char ir_name[128];
    snprintf(ir_name, sizeof(ir_name), "%s$%d", def->name, ctx->next_var_uid);

    generate_expression(ctx, def->expr, ir_name);
    if (var_type == DOUBLE && def->expr != NULL &&
        def->expr->data_type == INT) {
      fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", ir_name, ir_name);
    }
    scope_push_var(ctx, def->name, var_type);
    break;
  }

  case NODE_ASSIGN: {
    ASTAssignNodeData *assign = &statement->data.assign;
    const LocalVarEntry *entry = scope_lookup(ctx, assign->left);
    if (entry != NULL) {
      char ir_name[128];
      snprintf(ir_name, sizeof(ir_name), "%s$%d", entry->name, entry->uid);
      generate_expression(ctx, assign->right, ir_name);
      if (entry->type == DOUBLE && assign->right != NULL &&
          assign->right->data_type == INT) {
        fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", ir_name, ir_name);
      }
    }
    break;
  }

  case NODE_BLOCK: {
    scope_enter(ctx);
    ASTNode *s = statement->data.block.statements;
    while (s != NULL) {
      generate_statement(ctx, s, func_ret_type);
      s = s->next;
    }
    scope_leave(ctx);
    break;
  }

    /* AI Model used: Gemini
    Prompt:
            case NODE_BLOCK:
                ASTNode *s = statement->data.block.statements;
                while (s != NULL) {
                    generate_statement(ctx, s);
                    s = s->next;
                }
                break;

    src/codegen.c:214:13: error: label followed by a declaration is a C23
    extension [-Werror,-Wc23-extensions]   214 |             ASTNode *s =
    statement->data.block.statements;


    Output:
    In C standards prior to C23, a case label must be followed by a statement,
    not a variable declaration. Wrap the case body in curly braces {} to create
    a compound statement (new scope):

    C
            case NODE_BLOCK: {
                ASTNode *s = statement->data.block.statements;
                while (s != NULL) {
                    generate_statement(ctx, s);
                    s = s->next;
                }
                break;
            }
    Alternatively, place an empty statement (;) right after case NODE_BLOCK:,
    though adding {} is cleaner and prevents variable scope bleed across cases.

    Note:   wow I'm stupid, this was staring me in the face
            code has been edited a lot more after prompt
    */

  case NODE_LITERAL:
  case NODE_VARIABLE_REF:
  case NODE_UNARY_OP:
  case NODE_BINARY_OP:
  case NODE_FUNCTION_CALL:
    generate_expression(ctx, statement, NULL);
    break;

  case NODE_RETURN:
    emit_return_expr(ctx, statement->data.return_node.value, func_ret_type);
    fprintf(ctx->out, "POPFRAME\n");
    fprintf(ctx->out, "RETURN\n");
    break;

  case NODE_IF: {
    int id = ctx->label_counter++;
    char cond_tmp[32];
    new_tmp_var(ctx, cond_tmp);

    generate_expression(ctx, statement->data.if_node.condition, cond_tmp);

    fprintf(ctx->out, "JUMPIFEQ $else_%d LF@%s bool@false\n", id, cond_tmp);

    generate_statement(ctx, statement->data.if_node.then_branch, func_ret_type);
    fprintf(ctx->out, "JUMP $if_end_%d\n", id);

    fprintf(ctx->out, "LABEL $else_%d\n", id);
    if (statement->data.if_node.else_branch != NULL) {
      generate_statement(ctx, statement->data.if_node.else_branch,
                         func_ret_type);
    }

    fprintf(ctx->out, "LABEL $if_end_%d\n", id);
    break;
  }

  case NODE_WHILE: {
    int id = ctx->label_counter++;
    char cond_tmp[32];
    new_tmp_var(ctx, cond_tmp);

    fprintf(ctx->out, "LABEL $while_start_%d\n", id);

    generate_expression(ctx, statement->data.while_node.condition, cond_tmp);

    fprintf(ctx->out, "JUMPIFEQ $while_end_%d LF@%s bool@false\n", id,
            cond_tmp);

    generate_statement(ctx, statement->data.while_node.body, func_ret_type);
    fprintf(ctx->out, "JUMP $while_start_%d\n", id);

    fprintf(ctx->out, "LABEL $while_end_%d\n", id);
    break;
  }

  default:
    break;
  }

  return;
}

static void generate_function(CodeGenContext *ctx, ASTNode *fn_node) {
  if (fn_node == NULL || fn_node->node_type != NODE_FUNCTION_DEF) {
    return;
  }

  ASTFunctionDefNodeData *fn = &fn_node->data.function_def;

  fprintf(ctx->out, "LABEL $%s\n", fn->name);

  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");

  ctx->tmp_counter = 0;
  ctx->var_count = 0;
  ctx->scope_depth = 0;
  ctx->next_var_uid = 0;

  if (fn->param_count > 0) {
    int *param_uids = malloc(fn->param_count * sizeof(int));
    if (param_uids == NULL) {
      ctx->has_error = true;
      return;
    }

    for (size_t i = 0; i < fn->param_count; i++) {
      param_uids[i] =
          scope_push_var(ctx, fn->params[i].name, fn->params[i].data_type);
      if (ctx->has_error) {
        free(param_uids);
        return;
      }
    }

    for (size_t i = fn->param_count; i > 0; i--) {
      size_t idx = i - 1;
      fprintf(ctx->out, "DEFVAR LF@%s$%d\n", fn->params[idx].name,
              param_uids[idx]);
      fprintf(ctx->out, "POPS LF@%s$%d\n", fn->params[idx].name,
              param_uids[idx]);
    }
    free(param_uids);
  }

  int max_tmps = count_ast_nodes(fn->body) + 4;
  for (int i = 0; i < max_tmps; i++) {
    fprintf(ctx->out, "DEFVAR LF@$tmp_%d\n", i);
  }

  int def_uid = ctx->next_var_uid;
  emit_var_defs_walk(ctx, fn->body, &def_uid);

  if (fn->body != NULL) {

    if (fn->body->node_type != NODE_BLOCK) {
      emit_return_expr(ctx, fn->body, fn->return_type);
    } else {
      ASTNode *s = fn->body->data.block.statements;
      while (s != NULL) {
        if (s->next == NULL && is_expression_node(s)) {
          emit_return_expr(ctx, s, fn->return_type);
        } else {
          generate_statement(ctx, s, fn->return_type);
          if (s->next == NULL) {
            fprintf(ctx->out, "PUSHS nil@nil\n");
          }
        }
        s = s->next;
      }
    }
  } else {
    fprintf(ctx->out, "PUSHS nil@nil\n");
  }

  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  return;
}

ReturnCodes generate_code(ASTNode *program, FILE *out) {
  if (program == NULL || out == NULL || program->node_type != NODE_PROGRAM) {
    return ERR_INTERNAL;
  }

  CodeGenContext ctx = {
      .out = out,
      .label_counter = 0,
      .tmp_counter = 0,
      .program = program,
  };

  if (!generate_prolog(&ctx)) {
    return ERR_INTERNAL;
  }

  ASTNode *function = program->data.program.functions;
  while (function != NULL) {
    generate_function(&ctx, function);
    function = function->next;
  }

  generate_builtin_library(&ctx);

  free(ctx.vars);
  return ctx.has_error ? ERR_INTERNAL : RETURN_SUCCESS;
}
