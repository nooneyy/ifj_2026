/**
 * @file codegen.c
 * @brief Implementation of the IFJcode2026 code generator
 * @author Tadeáš Čupr <xcuprta00@stud.fit.vut.cz>
 * @date 04.11.2026
 */

#include "codegen.h"
#include "ast.h"
#include "return.h"
#include "symbol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMP_VAR_BUF_SIZE 32
#define INITIAL_VAR_CAP 16
#define TMPS_PER_AST_NODE 2
#define EXTRA_TMP_SLOTS 4
#define IFJ_PREFIX "ifj."
#define IFJ_PREFIX_LEN (sizeof(IFJ_PREFIX) - 1)

/**
 * @brief Represents a local variable
 */
typedef struct {
  const char *name; // Name of variable straight from the source code
  DataType type;    // Data type of the variable
  int uid;          // Unique ID for the variable LF@<name>$<uid>
  int scope_depth;  // How deep the variable was declared
} LocalVarEntry;

/**
 * @brief Internal context used by the code generator
 */
typedef struct {
  FILE *out;           // Desired output stream for emitted IFJcode26 code
  int label_counter;   // Counter for unique labels
  int tmp_counter;     // Counter for unique IDs for temporary variables
  ASTNode *program;    // Root AST node of the program
  LocalVarEntry *vars; // Stack of local variables
  size_t var_count;    // Number of active variables
  size_t var_cap;      // Allocated capacity for vars
  int scope_depth;     // Current block depth
  int next_var_uid;    // Next unique ID for variables
  bool has_error;      // Set to true in case of an internal error
  DataType current_func_ret_type; // Return type of the function being generated
} CodeGenContext;

static const char *binop_instr_table[] = {
    [OP_SUB] = "SUB", [OP_MUL] = "MUL", [OP_EQ] = "EQ",
    [OP_LT] = "LT",   [OP_GT] = "GT",
};

static const char *unop_instr_table[] = {
    [OP_NOT] = "NOT",
};

#define BINOP_TABLE_SIZE                                                       \
  (sizeof(binop_instr_table) / sizeof(binop_instr_table[0]))
#define UNOP_TABLE_SIZE (sizeof(unop_instr_table) / sizeof(unop_instr_table[0]))

// Forward declarations
static void generate_expression(CodeGenContext *ctx, ASTNode *expr,
                                const char *dest_var);

static void generate_statement(CodeGenContext *ctx, ASTNode *statement,
                               DataType func_ret_type);
// Forward declarations end

/**
 * @brief Searches the AST for the @main function
 * @param program Root AST node of the program
 * @param out Output parameter that gets the main function's name or NULL if it
 * isn't named
 * @return true if main is named
 *         false if main is unnamed
 */
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

/**
 * @brief Emits the IFJcode26 header, call to the main function and a program
 * exit
 * @param ctx Code generator context
 * @return true on success
 *         false if there is no main function in the AST
 */
static bool generate_prolog(CodeGenContext *ctx) {
  char *main_name = NULL;
  if (!find_main_function(ctx->program, &main_name)) {
    return false;
  }

  fprintf(ctx->out, ".IFJcode26\n");
  fprintf(ctx->out, "CALL $fn_%s\n", main_name);
  fprintf(ctx->out, "EXIT int@0\n\n");

  return true;
}

/**
 * @brief Emits IFJcode26 built-in library functions
 * @param ctx Code generator context
 */
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
  fprintf(ctx->out, "DEFVAR LF@type\n");
  fprintf(ctx->out, "TYPE LF@type LF@val\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "MOVE LF@res LF@val\n");
  fprintf(ctx->out, "JUMPIFEQ $ifj_toInt_end LF@type string@int\n");
  fprintf(ctx->out, "FLOAT2INT LF@res LF@val\n");
  fprintf(ctx->out, "LABEL $ifj_toInt_end\n");
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
  fprintf(ctx->out, "JUMPIFNEQ $ifj_readBoolean_end LF@res nil@nil\n");
  fprintf(ctx->out, "MOVE LF@res bool@false\n");
  fprintf(ctx->out, "LABEL $ifj_readBoolean_end\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");

  fprintf(ctx->out, "LABEL $ifj_floor\n");
  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");
  fprintf(ctx->out, "DEFVAR LF@val\n");
  fprintf(ctx->out, "POPS LF@val\n");
  fprintf(ctx->out, "DEFVAR LF@res\n");
  fprintf(ctx->out, "DEFVAR LF@cond\n");
  fprintf(ctx->out, "DEFVAR LF@i\n");
  fprintf(ctx->out, "MOVE LF@res LF@val\n");
  fprintf(ctx->out, "LT LF@cond LF@val float@0x1p+53\n");
  fprintf(ctx->out, "JUMPIFEQ $ifj_floor_end LF@cond bool@false\n");
  fprintf(ctx->out, "SUB LF@i float@0x0p+0 float@0x1p+53\n");
  fprintf(ctx->out, "GT LF@cond LF@val LF@i\n");
  fprintf(ctx->out, "JUMPIFEQ $ifj_floor_end LF@cond bool@false\n");
  fprintf(ctx->out, "FLOAT2INT LF@i LF@val\n");
  fprintf(ctx->out, "INT2FLOAT LF@res LF@i\n");
  fprintf(ctx->out, "LT LF@cond LF@val LF@res\n");
  fprintf(ctx->out, "JUMPIFEQ $ifj_floor_end LF@cond bool@false\n");
  fprintf(ctx->out, "SUB LF@res LF@res float@0x1p+0\n");
  fprintf(ctx->out, "LABEL $ifj_floor_end\n");
  fprintf(ctx->out, "PUSHS LF@res\n");
  fprintf(ctx->out, "POPFRAME\n");
  fprintf(ctx->out, "RETURN\n\n");
}

/**
 * @brief Determines if an AST node produces a value
 * @param node AST node to inspect
 * @return true if node produces a value
 *         false if node is NULL or if the node doesn't produce a value
 */
static bool is_expression_node(const ASTNode *node) {
  if (node == NULL) {
    return false;
  }
  switch (node->node_type) {
  case NODE_LITERAL:
  case NODE_VARIABLE_REF:
  case NODE_UNARY_OP:
  case NODE_BINARY_OP:
  case NODE_FUNCTION_CALL:
  case NODE_VALMEM_FIELD:
    return true;
  case NODE_IF:
    return (node->data.if_node.is_expr ||
            (node->data.if_node.else_branch != NULL &&
             node->data_type != UNIT && node->data_type != UNKNOWN)) != 0;
  default:
    return false;
  }
}

/**
 * @brief Checks if a function name refers to an IFJcode2026 library function
 * @param name Function name to check
 * @return true if the name matches a library function
 *         false otherwise
 */
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

/**
 * @brief Writes an IFJcode2026 string literal with required escape codes
 *
 * Escapes characters with ASCII value <= 32, '#' and '\' as \xyz
 *
 * @param ctx Code generator context
 * @param str String content to emit
 */
static void print_escaped_string(CodeGenContext *ctx, const char *str) {
  fprintf(ctx->out, "string@");
  if (str == NULL) {
    return;
  }

  for (size_t i = 0; str[i] != '\0'; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c <= ' ' || c == '#' || c == '\\') {
      fprintf(ctx->out, "\\%03d", c);
    } else {
      fputc(c, ctx->out);
    }
  }
}

/**
 * @brief Allocates the next unique temporary variable name
 * @param ctx Code generator context
 * @param buf Output buffer for the name
 */
static void new_tmp_var(CodeGenContext *ctx, char buf[TMP_VAR_BUF_SIZE]) {
  int id = ctx->tmp_counter++;
  snprintf(buf, TMP_VAR_BUF_SIZE, "$tmp_%d", id);
}

/**
 * @brief Enters a new lexical scope
 * @param ctx Code generator context
 */
static void scope_enter(CodeGenContext *ctx) { ctx->scope_depth++; }

/**
 * @brief Exits the correct lexical scope and pops all variables in it
 * @param ctx Code generator context
 */
static void scope_leave(CodeGenContext *ctx) {
  if (ctx->scope_depth > 0) {
    ctx->scope_depth--;
  }
  while (ctx->var_count > 0 &&
         ctx->vars[ctx->var_count - 1].scope_depth > ctx->scope_depth) {
    ctx->var_count--;
  }
}

/**
 * @brief Registers a variable or parameter in the lexical scope with a given
 * UID
 *
 * Grows the stack if necessary and assigns a pre-allocated unique ID
 * Sets ctx->has_error if malloc fails
 *
 * @param ctx Code generator context
 * @param name Name for the variable from the source code
 * @param type Type of the variable
 * @param uid Pre-allocated unique ID
 * @return Unique ID on success
 *         -1 on malloc fail
 */
static int scope_push_var_with_uid(CodeGenContext *ctx, const char *name,
                                   DataType type, int uid) {
  if (ctx->var_count >= ctx->var_cap) {
    size_t new_cap = (ctx->var_cap == 0) ? INITIAL_VAR_CAP : ctx->var_cap * 2;
    LocalVarEntry *tmp = realloc(ctx->vars, new_cap * sizeof(LocalVarEntry));
    if (tmp == NULL) {
      ctx->has_error = true;
      return -1;
    }
    ctx->vars = tmp;
    ctx->var_cap = new_cap;
  }

  ctx->vars[ctx->var_count++] = (LocalVarEntry){
      .name = name,
      .type = type,
      .uid = uid,
      .scope_depth = ctx->scope_depth,
  };
  return uid;
}

/**
 * @brief Registers a variable or parameter in the lexical scope
 *
 * Grows the stack if necessary and assigns a unique ID
 * Sets ctx->has_error if malloc fails
 *
 * @param ctx Code generator context
 * @param name Name for the variable from the source code
 * @param type Type of the variable
 * @return Unique ID on success
 *         -1 on malloc fail
 */
static int scope_push_var(CodeGenContext *ctx, const char *name,
                          DataType type) {
  return scope_push_var_with_uid(ctx, name, type, ctx->next_var_uid++);
}

/**
 * @brief Estimates how many temporary variables are needed for an AST subtree
 * @param node AST node list to traverse
 * @return Estimated number of temporary variables
 */
static int count_ast_nodes(ASTNode *node) {
  int count = 0;
  while (node != NULL) {
    count += TMPS_PER_AST_NODE;
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

    case NODE_FOR:
      count += TMPS_PER_AST_NODE; // For loop required additional temp vars
      count += count_ast_nodes(node->data.for_node.range_start);
      count += count_ast_nodes(node->data.for_node.range_end);
      count += count_ast_nodes(node->data.for_node.range_step);
      count += count_ast_nodes(node->data.for_node.guards);
      count += count_ast_nodes(node->data.for_node.body);
      break;

    case NODE_VALMEM_FIELD:
      count += count_ast_nodes(node->data.valmem_field.receiver);
      break;

    default:
      break;
    }
    node = node->next;
  }
  return count;
}

/**
 * @brief Looks up the innermost local variable or parameter by name
 *
 * Searches the active stack from newest to oldest to support shadowing
 *
 * @param ctx Code generator context
 * @param name Name of variable or parameter from the source code
 * @return Pointer to the matching LocalVarEntry
 *         NULL if not found
 */
static const LocalVarEntry *scope_lookup(const CodeGenContext *ctx,
                                         const char *name) {
  for (size_t i = ctx->var_count; i > 0; i--) {
    if (strcmp(ctx->vars[i - 1].name, name) == 0) {
      return &ctx->vars[i - 1];
    }
  }
  return NULL;
}

/**
 * @brief Finds a function's AST data by name
 * @param program Root AST node of program
 * @param name Name of the function to find
 * @return Pointer to the function's ASTFunctionDefNodeData
 *         NULL if not found
 */
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

/**
 * @brief Emits DEFVAR for all local variables in the function's body
 *
 * Hoists every local variable so IFJcode26 variables are never defined inside
 * loops
 *
 * @param ctx Code generator context
 * @param node Root of the AST subtree to walk
 * @param uid_counter Running UID counter
 */
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

    case NODE_ASSIGN:
      emit_var_defs_walk(ctx, node->data.assign.right, uid_counter);
      break;

    case NODE_UNARY_OP:
      emit_var_defs_walk(ctx, node->data.unary_op.operand, uid_counter);
      break;

    case NODE_BINARY_OP:
      emit_var_defs_walk(ctx, node->data.binary_op.left, uid_counter);
      emit_var_defs_walk(ctx, node->data.binary_op.right, uid_counter);
      break;

    case NODE_FUNCTION_CALL:
      emit_var_defs_walk(ctx, node->data.function_call.args, uid_counter);
      break;

    case NODE_RETURN:
      emit_var_defs_walk(ctx, node->data.return_node.value, uid_counter);
      break;

    case NODE_BLOCK:
      emit_var_defs_walk(ctx, node->data.block.statements, uid_counter);
      break;

    case NODE_IF:
      emit_var_defs_walk(ctx, node->data.if_node.condition, uid_counter);
      emit_var_defs_walk(ctx, node->data.if_node.then_branch, uid_counter);
      emit_var_defs_walk(ctx, node->data.if_node.else_branch, uid_counter);
      break;

    case NODE_WHILE:
      emit_var_defs_walk(ctx, node->data.while_node.condition, uid_counter);
      emit_var_defs_walk(ctx, node->data.while_node.body, uid_counter);
      break;

    case NODE_FOR:
      emit_var_defs_walk(ctx, node->data.for_node.range_start, uid_counter);
      emit_var_defs_walk(ctx, node->data.for_node.range_end, uid_counter);
      emit_var_defs_walk(ctx, node->data.for_node.range_step, uid_counter);
      fprintf(ctx->out, "DEFVAR LF@%s$%d\n", node->data.for_node.generator_name,
              (*uid_counter)++);
      emit_var_defs_walk(ctx, node->data.for_node.guards, uid_counter);
      emit_var_defs_walk(ctx, node->data.for_node.body, uid_counter);
      break;

    case NODE_VALMEM_FIELD:
      emit_var_defs_walk(ctx, node->data.valmem_field.receiver, uid_counter);
      break;

    default:
      break;
    }
    node = node->next;
  }
}

/**
 * @brief Emits IFJcode26 code for binary operations
 * @param ctx Code generator context
 * @param expr AST node of the binary expression
 * @param dest_var Name of the variable where the result is stored
 */
static void expression_binary_op(CodeGenContext *ctx, ASTNode *expr,
                                 const char *dest_var) {

  const ASTBinaryOpNodeData *bin = &expr->data.binary_op;
  ASTOperator op = bin->op;

  if (op == OP_AND || op == OP_OR) {
    int id = ctx->label_counter++;
    char left_tmp[TMP_VAR_BUF_SIZE];

    new_tmp_var(ctx, left_tmp);
    generate_expression(ctx, bin->left, left_tmp);

    const char *short_val = (op == OP_AND) ? "bool@false" : "bool@true";
    fprintf(ctx->out, "JUMPIFEQ $sc_short_%d LF@%s %s\n", id, left_tmp,
            short_val);

    generate_expression(ctx, bin->right, dest_var);

    fprintf(ctx->out, "JUMP $sc_end_%d\n", id);
    fprintf(ctx->out, "LABEL $sc_short_%d\n", id);

    if (dest_var != NULL) {
      fprintf(ctx->out, "MOVE LF@%s %s\n", dest_var, short_val);
    }
    fprintf(ctx->out, "LABEL $sc_end_%d\n", id);
    return;
  }

  char left_tmp[TMP_VAR_BUF_SIZE];
  char right_tmp[TMP_VAR_BUF_SIZE];
  new_tmp_var(ctx, left_tmp);
  new_tmp_var(ctx, right_tmp);
  generate_expression(ctx, bin->left, left_tmp);
  generate_expression(ctx, bin->right, right_tmp);

  if (dest_var == NULL &&
      op != OP_DIV) { // if the OP_DIV check wasn't here the code would
                      // sometimes not run into divide by zero errors since if
                      // dest_var == NULL it would be skipped
    return;
  }

  if (bin->left->data_type != bin->right->data_type) {
    if (bin->left->data_type == INT) {
      fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", left_tmp, left_tmp);
    }
    if (bin->right->data_type == INT) {
      fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", right_tmp, right_tmp);
    }
  }

  const char *inst;

  switch (op) {
  case OP_ADD:
    inst = (expr->data_type == STRING) ? "CONCAT" : "ADD";
    fprintf(ctx->out, "%s LF@%s LF@%s LF@%s\n", inst, dest_var, left_tmp,
            right_tmp);
    break;
  case OP_DIV: {
    const char *target = (dest_var != NULL)
                             ? dest_var
                             : left_tmp; // second part of dest_var == NULL
                                         // divide by zero skipping
    inst = (expr->data_type == INT) ? "IDIV" : "DIV";
    fprintf(ctx->out, "%s LF@%s LF@%s LF@%s\n", inst, target, left_tmp,
            right_tmp);
    break;
  }
  case OP_NEQ:
    fprintf(ctx->out, "EQ LF@%s LF@%s LF@%s\n", dest_var, left_tmp, right_tmp);
    fprintf(ctx->out, "NOT LF@%s LF@%s\n", dest_var, dest_var);
    break;
  case OP_LTE:
    fprintf(ctx->out, "GT LF@%s LF@%s LF@%s\n", dest_var, left_tmp, right_tmp);
    fprintf(ctx->out, "NOT LF@%s LF@%s\n", dest_var, dest_var);
    break;
  case OP_GTE:
    fprintf(ctx->out, "LT LF@%s LF@%s LF@%s\n", dest_var, left_tmp, right_tmp);
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

/**
 * @brief Emits IFJcode26 code for one branch of an if branch into dest_var
 * @param ctx Code generator context
 * @param branch AST node of the then/else branch
 * @param dest_var Name of the variable where the result is stored
 * @param expected_type Expected data type of the if expression
 */
static void expression_if_branch(CodeGenContext *ctx, ASTNode *branch,
                                 const char *dest_var, DataType expected_type) {
  if (branch == NULL) {
    if (dest_var != NULL) {
      fprintf(ctx->out, "MOVE LF@%s nil@nil\n", dest_var);
    }
    return;
  }

  if (branch->node_type == NODE_BLOCK) {
    scope_enter(ctx);
    ASTNode *s = branch->data.block.statements;
    if (s == NULL && dest_var != NULL) {
      fprintf(ctx->out, "MOVE LF@%s nil@nil\n", dest_var);
    }
    while (s != NULL) {
      if (s->next == NULL &&
          (is_expression_node(s) || s->node_type == NODE_BLOCK)) {
        expression_if_branch(ctx, s, dest_var, expected_type);
      } else {
        generate_statement(ctx, s, ctx->current_func_ret_type);
        if (s->next == NULL && dest_var != NULL) {
          fprintf(ctx->out, "MOVE LF@%s nil@nil\n", dest_var);
        }
      }
      s = s->next;
    }
    scope_leave(ctx);
    return;
  }

  generate_expression(ctx, branch, dest_var);
  if (dest_var != NULL && expected_type == DOUBLE && branch->data_type == INT) {
    fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", dest_var, dest_var);
  }
}

/**
 * @brief Emits IFJcode26 code for an if node
 * @param ctx Code generator context
 * @param expr AST node of the if branch
 * @param dest_var Name of the variable where the result is stored
 */
static void expression_if(CodeGenContext *ctx, ASTNode *expr,
                          const char *dest_var) {
  const ASTIfNodeData *if_node = &expr->data.if_node;
  int id = ctx->label_counter++;
  char cond_tmp[TMP_VAR_BUF_SIZE];
  new_tmp_var(ctx, cond_tmp);

  generate_expression(ctx, if_node->condition, cond_tmp);
  fprintf(ctx->out, "JUMPIFEQ $else_%d LF@%s bool@false\n", id, cond_tmp);

  expression_if_branch(ctx, if_node->then_branch, dest_var, expr->data_type);
  fprintf(ctx->out, "JUMP $if_end_%d\n", id);

  fprintf(ctx->out, "LABEL $else_%d\n", id);
  expression_if_branch(ctx, if_node->else_branch, dest_var, expr->data_type);

  fprintf(ctx->out, "LABEL $if_end_%d\n", id);
}

/**
 * @brief Emits IFJcode26 code for a VALMEM field access
 * @param ctx Code generator context
 * @param expr AST node of the VALMEM field expression
 * @param dest_var Name of the variable where the result is stored
 *                 NULL if result is discarded
 */
static void expression_valmem_field(CodeGenContext *ctx, const ASTNode *expr,
                                    const char *dest_var) {
  const ASTValmemFieldData *valmem = &expr->data.valmem_field;

  if (valmem->name == NULL || valmem->receiver == NULL) {
    return;
  }

  const char *callee;
  if (strcmp(valmem->name, "toInt") == 0) {
    callee = "$ifj_toInt";
  } else if (strcmp(valmem->name, "floor") == 0) {
    callee = "$ifj_floor";
  } else if (strcmp(valmem->name, "length") == 0) {
    callee = "$ifj_strLength";
  } else if (strcmp(valmem->name, "toString") == 0) {
    callee = "$ifj_toStr";
  } else {
    return; // This could've been better if the AST had a valmem->type
  }

  char recv_tmp[TMP_VAR_BUF_SIZE];
  new_tmp_var(ctx, recv_tmp);

  generate_expression(ctx, valmem->receiver, recv_tmp);
  fprintf(ctx->out, "PUSHS LF@%s\n", recv_tmp);
  fprintf(ctx->out, "CALL %s\n", callee);

  if (dest_var != NULL) {
    fprintf(ctx->out, "POPS LF@%s\n", dest_var);
  } else {
    char dummy[TMP_VAR_BUF_SIZE];
    new_tmp_var(ctx, dummy);
    fprintf(ctx->out, "POPS LF@%s\n", dummy);
  }
}

/**
 * @brief Emits IFJcode26 code for expressions
 *
 * Handles implicit INT to DOUBLE retyping
 *
 * @param ctx Code generator context
 * @param expr Expression's AST node
 * @param dest_var Name of the variable to store the result in
 *                 NULL if result is discarded
 */
static void generate_expression(CodeGenContext *ctx, ASTNode *expr,
                                const char *dest_var) {
  if (expr == NULL) {
    return;
  }

  switch (expr->node_type) {
  case NODE_LITERAL:
    if (dest_var == NULL) {
      break;
    }
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
    char op_tmp[TMP_VAR_BUF_SIZE];
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
    expression_binary_op(ctx, expr, dest_var);
    break;
  }

  case NODE_FUNCTION_CALL: {
    ASTFunctionDefNodeData *callee =
        find_function_def(ctx->program, expr->data.function_call.name);
    ASTNode *arg = expr->data.function_call.args;
    size_t arg_idx = 0;

    while (arg != NULL) {
      char arg_tmp[TMP_VAR_BUF_SIZE];
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
    if (strncmp(name, IFJ_PREFIX, IFJ_PREFIX_LEN) == 0) {
      fprintf(ctx->out, "CALL $ifj_%s\n", name + IFJ_PREFIX_LEN);
    } else if (is_builtin_func(name)) {
      fprintf(ctx->out, "CALL $ifj_%s\n", name);
    } else {
      fprintf(ctx->out, "CALL $fn_%s\n", name);
    }

    if (dest_var != NULL) {
      fprintf(ctx->out, "POPS LF@%s\n", dest_var);
    } else {
      char dummy[TMP_VAR_BUF_SIZE];
      new_tmp_var(ctx, dummy);

      fprintf(ctx->out, "POPS LF@%s\n", dummy);
    }
    break;
  }

  case NODE_IF:
    expression_if(ctx, expr, dest_var);
    break;

  case NODE_VALMEM_FIELD:
    expression_valmem_field(ctx, expr, dest_var);
    break;

  default:
    break;
  }
}

/**
 * @brief Evaluates a return expression and pushes it onto the stack
 *
 * Handles implicit INT to DOUBLE retyping
 * pushes nil@nil for Unit functions
 *
 * @param ctx Code generator context
 * @param expr Return value expression
 * @param expected_ret_type Expected return type of the function
 */
static void emit_return_expr(CodeGenContext *ctx, ASTNode *expr,
                             DataType expected_ret_type) {
  if (expr == NULL || expected_ret_type == UNIT) {
    if (expr != NULL) {
      generate_expression(ctx, expr, NULL);
    }
    fprintf(ctx->out, "PUSHS nil@nil\n");
    return;
  }

  char ret_tmp[TMP_VAR_BUF_SIZE];
  new_tmp_var(ctx, ret_tmp);

  generate_expression(ctx, expr, ret_tmp);

  if (expected_ret_type == DOUBLE && expr->data_type == INT) {
    fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", ret_tmp, ret_tmp);
  }

  fprintf(ctx->out, "PUSHS LF@%s\n", ret_tmp);
}

/**
 * @brief Emits IFJcode26 code for a for loop
 * @param ctx Code generator context
 * @param statement For loop's AST node
 * @param func_ret_type Return type of the function
 */
static void statement_for(CodeGenContext *ctx, ASTNode *statement,
                          DataType func_ret_type) {
  const ASTForNodeData *for_node = &statement->data.for_node;
  int id = ctx->label_counter++;

  char start_tmp[TMP_VAR_BUF_SIZE];
  char end_tmp[TMP_VAR_BUF_SIZE];
  char step_tmp[TMP_VAR_BUF_SIZE];
  char cond_tmp[TMP_VAR_BUF_SIZE];
  new_tmp_var(ctx, start_tmp);
  new_tmp_var(ctx, end_tmp);
  new_tmp_var(ctx, step_tmp);
  new_tmp_var(ctx, cond_tmp);

  generate_expression(ctx, for_node->range_start, start_tmp);
  generate_expression(ctx, for_node->range_end, end_tmp);
  if (for_node->range_step != NULL) {
    generate_expression(ctx, for_node->range_step, step_tmp);
    fprintf(ctx->out, "JUMPIFNEQ $for_step_ok_%d LF@%s int@0\n", id, step_tmp);
    fprintf(ctx->out, "EXIT int@40\n");
    fprintf(ctx->out, "LABEL $for_step_ok_%d\n", id);
  } else {
    fprintf(ctx->out, "MOVE LF@%s int@1\n", step_tmp);
  }

  scope_enter(ctx);
  int gen_uid = scope_push_var(ctx, for_node->generator_name, INT);
  if (gen_uid < 0) {
    scope_leave(ctx);
    return;
  }

  const char *gen_name = for_node->generator_name;
  fprintf(ctx->out, "MOVE LF@%s$%d LF@%s\n", gen_name, gen_uid, start_tmp);

  fprintf(ctx->out, "LABEL $for_start_%d\n", id);
  fprintf(ctx->out, "LT LF@%s LF@%s int@0\n", cond_tmp, step_tmp);
  fprintf(ctx->out, "JUMPIFEQ $for_neg_%d LF@%s bool@true\n", id, cond_tmp);

  if (for_node->includes_end) {
    fprintf(ctx->out, "GT LF@%s LF@%s$%d LF@%s\n", cond_tmp, gen_name, gen_uid,
            end_tmp);
    fprintf(ctx->out, "JUMPIFEQ $for_end_%d LF@%s bool@true\n", id, cond_tmp);
  } else {
    fprintf(ctx->out, "LT LF@%s LF@%s$%d LF@%s\n", cond_tmp, gen_name, gen_uid,
            end_tmp);
    fprintf(ctx->out, "JUMPIFEQ $for_end_%d LF@%s bool@false\n", id, cond_tmp);
  }
  fprintf(ctx->out, "JUMP $for_guards_%d\n", id);

  fprintf(ctx->out, "LABEL $for_neg_%d\n", id);
  if (for_node->includes_end) {
    fprintf(ctx->out, "LT LF@%s LF@%s$%d LF@%s\n", cond_tmp, gen_name, gen_uid,
            end_tmp);
    fprintf(ctx->out, "JUMPIFEQ $for_end_%d LF@%s bool@true\n", id, cond_tmp);
  } else {
    fprintf(ctx->out, "GT LF@%s LF@%s$%d LF@%s\n", cond_tmp, gen_name, gen_uid,
            end_tmp);
    fprintf(ctx->out, "JUMPIFEQ $for_end_%d LF@%s bool@false\n", id, cond_tmp);
  }

  fprintf(ctx->out, "LABEL $for_guards_%d\n", id);
  ASTNode *guard = for_node->guards;
  while (guard != NULL) {
    generate_expression(ctx, guard, cond_tmp);
    fprintf(ctx->out, "JUMPIFEQ $for_step_%d LF@%s bool@false\n", id, cond_tmp);
    guard = guard->next;
  }

  generate_statement(ctx, for_node->body, func_ret_type);
  fprintf(ctx->out, "LABEL $for_step_%d\n", id);
  fprintf(ctx->out, "ADD LF@%s$%d LF@%s$%d LF@%s\n", gen_name, gen_uid,
          gen_name, gen_uid, step_tmp);
  fprintf(ctx->out, "JUMP $for_start_%d\n", id);

  fprintf(ctx->out, "LABEL $for_end_%d\n", id);
  scope_leave(ctx);
}

/**
 * @brief Emits IFJcode26 code for a single statement AST node
 * @param ctx Code generator context
 * @param statement AST node with the statement to emit
 * @param func_ret_type Return type of the statement
 */
static void generate_statement(CodeGenContext *ctx, ASTNode *statement,
                               DataType func_ret_type) {
  if (statement == NULL) {
    return;
  }

  switch (statement->node_type) {
  case NODE_VARIABLE_DEF: {
    ASTVarDefNodeData *def = &statement->data.variable_def;

    DataType var_type = UNKNOWN;

    if (def->specified_type != UNKNOWN) {
      var_type = def->specified_type;
    } else if (def->expr != NULL) {
      var_type = def->expr->data_type;
    }

    int uid = ctx->next_var_uid++;
    if (def->expr != NULL) {
      char val_tmp[TMP_VAR_BUF_SIZE];
      new_tmp_var(ctx, val_tmp);

      generate_expression(ctx, def->expr, val_tmp);
      if (var_type == DOUBLE && def->expr->data_type == INT) {
        fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", val_tmp, val_tmp);
      }
      fprintf(ctx->out, "MOVE LF@%s$%d LF@%s\n", def->name, uid, val_tmp);
    }
    scope_push_var_with_uid(ctx, def->name, var_type, uid);
    break;
  }

  case NODE_ASSIGN: {
    ASTAssignNodeData *assign = &statement->data.assign;
    const LocalVarEntry *entry = scope_lookup(ctx, assign->left);
    if (entry != NULL && assign->right != NULL) {
      const char *target_name = entry->name;
      int target_uid = entry->uid;
      DataType target_type = entry->type;
      char val_tmp[TMP_VAR_BUF_SIZE];
      new_tmp_var(ctx, val_tmp);

      generate_expression(ctx, assign->right, val_tmp);
      if (target_type == DOUBLE && assign->right->data_type == INT) {
        fprintf(ctx->out, "INT2FLOAT LF@%s LF@%s\n", val_tmp, val_tmp);
      }
      fprintf(ctx->out, "MOVE LF@%s$%d LF@%s\n", target_name, target_uid,
              val_tmp);
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

    /* AI Model used: Gemini 4 Hydrogen High
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
  case NODE_VALMEM_FIELD:
    generate_expression(ctx, statement, NULL);
    break;

  case NODE_RETURN:
    emit_return_expr(ctx, statement->data.return_node.value, func_ret_type);
    fprintf(ctx->out, "POPFRAME\n");
    fprintf(ctx->out, "RETURN\n");
    break;

  case NODE_IF: {
    int id = ctx->label_counter++;
    char cond_tmp[TMP_VAR_BUF_SIZE];
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
    char cond_tmp[TMP_VAR_BUF_SIZE];
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

  case NODE_FOR:
    statement_for(ctx, statement, func_ret_type);
    break;

  default:
    break;
  }
}

/**
 * @brief Generates the complete IFJcode26 code for a function
 *
 * Emits the function label, frame creation, parameter popping, DEFVARs for
 * temporary and local variables, body instructions (RETURN always as last) and
 * frame popping
 *
 * @param ctx Code generator context
 * @param fn_node AST node of the function
 */
static void generate_function(CodeGenContext *ctx, ASTNode *fn_node) {
  if (fn_node == NULL || fn_node->node_type != NODE_FUNCTION_DEF) {
    return;
  }

  ASTFunctionDefNodeData *fn = &fn_node->data.function_def;

  fprintf(ctx->out, "LABEL $fn_%s\n", fn->name);

  fprintf(ctx->out, "CREATEFRAME\n");
  fprintf(ctx->out, "PUSHFRAME\n");

  ctx->tmp_counter = 0;
  ctx->var_count = 0;
  ctx->scope_depth = 0;
  ctx->next_var_uid = 0;
  ctx->current_func_ret_type = fn->return_type;

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

  int max_tmps = count_ast_nodes(fn->body) + EXTRA_TMP_SLOTS;
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

      if (s == NULL) {
          fprintf(ctx->out, "PUSHS nil@nil\n");
      }
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
  while (function != NULL && !ctx.has_error) {
    generate_function(&ctx, function);
    function = function->next;
  }

  if (!ctx.has_error) {
    generate_builtin_library(&ctx);
  }

  free(ctx.vars);
  return ctx.has_error ? ERR_INTERNAL : RETURN_SUCCESS;
}
