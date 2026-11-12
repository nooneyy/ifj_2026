/**
 * @file parser_main.c
 * @brief Testing main for the parser, prints the AST of a source file
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 12.11.2026
 */

/**
 * Disclaimer!
 * This testing file was made by Claude Opus 5.5 AI model
 * This file is only for testing purposes.
 *
 * Prompt:
 * "Since AI can be used freely to make tests, could you make them for me?
 * Let's collaborate on the design, the test contents and then i'll let u
 * implement it"
 * (context: agreed design - a testing main like scanner_main that parses
 * argv[1] with parse_input, prints the AST on success with one node per line
 * and children indented, prints nothing on stdout on error and returns the
 * parser's return code)
 */

#include "../src/ast.h"
#include "../src/parser.h"
#include "../src/return.h"
#include "../src/scanner.h"
#include "../src/symbol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * OPERATOR_NAMES - printable symbols of all AST operators
 *
 * Indexed by ASTOperator, used by print_node().
 */
static const char *const OPERATOR_NAMES[] = {
    [OP_ADD] = "+",  [OP_SUB] = "-",  [OP_MUL] = "*",  [OP_DIV] = "/",
    [OP_EQ] = "==",  [OP_NEQ] = "!=", [OP_LT] = "<",   [OP_GT] = ">",
    [OP_LTE] = "<=", [OP_GTE] = ">=", [OP_AND] = "&&", [OP_OR] = "||",
    [OP_NEG] = "-",  [OP_NOT] = "!",
};

/*
 * TYPE_NAMES - printable names of all data types
 *
 * Indexed by DataType, a type not known yet is printed as "?".
 */
static const char *const TYPE_NAMES[] = {
    [INT] = "Int",   [DOUBLE] = "Double",   [STRING] = "String",
    [UNIT] = "Unit", [BOOLEAN] = "Boolean", [ANY] = "Any",
    [UNKNOWN] = "?",
};

static void print_node(const ASTNode *node, int depth);

/**
 * operator_name() - getting printable symbol of an operator
 * @op: operator to get the symbol of
 *
 * Return: Returns the symbol from OPERATOR_NAMES
 *         Returns "UNKNOWN_OP" if the operator has no symbol there
 */
static const char *operator_name(ASTOperator op) {
  size_t count = sizeof(OPERATOR_NAMES) / sizeof(OPERATOR_NAMES[0]);
  if ((size_t)op >= count || OPERATOR_NAMES[op] == NULL) {
    return "UNKNOWN_OP";
  }
  return OPERATOR_NAMES[op];
}

/**
 * type_name() - getting printable name of a data type
 * @type: data type to get the name of
 *
 * Return: Returns the name from TYPE_NAMES
 *         Returns "UNKNOWN_TYPE" if the type has no name there
 */
static const char *type_name(DataType type) {
  size_t count = sizeof(TYPE_NAMES) / sizeof(TYPE_NAMES[0]);
  if ((size_t)type >= count || TYPE_NAMES[type] == NULL) {
    return "UNKNOWN_TYPE";
  }
  return TYPE_NAMES[type];
}

/**
 * print_indent() - printing the indentation of one tree level
 * @depth: how deep in the tree the printed line is
 */
static void print_indent(int depth) {
  for (int i = 0; i < depth; i++) {
    (void)fputs("  ", stdout);
  }
}

/**
 * print_string() - printing a string with visible escape sequences
 * @str: string to print
 *
 * Prints str in double quotes. Characters that would not be visible
 * or would make the output ambiguous are printed as escape sequences
 * \n, \t, \r, \\ and \", other control characters as \ddd.
 */
static void print_string(const char *str) {
  (void)putchar('"');
  for (const char *p = str; *p != '\0'; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '\n') {
      (void)fputs("\\n", stdout);
    } else if (c == '\t') {
      (void)fputs("\\t", stdout);
    } else if (c == '\r') {
      (void)fputs("\\r", stdout);
    } else if (c == '\\') {
      (void)fputs("\\\\", stdout);
    } else if (c == '"') {
      (void)fputs("\\\"", stdout);
    } else if (c < 32 || c == 127) {
      (void)printf("\\%03d", c);
    } else {
      (void)putchar(c);
    }
  }
  (void)putchar('"');
}

/**
 * print_double() - printing a double in its shortest exact form
 * @value: double to print
 *
 * Uses the lowest precision that still reads back as the same value,
 * so 0.1 is printed as 0.1, not as 0.10000000000000001.
 * A value without a decimal point or exponent gets ".0" appended,
 * so the Double 1.0 can't be mistaken for the Int 1.
 */
static void print_double(double value) {
  char buf[64];
  for (int precision = 1; precision <= 17; precision++) {
    (void)snprintf(buf, sizeof(buf), "%.*g", precision, value);
    if (strtod(buf, NULL) == value) {
      break;
    }
  }
  (void)fputs(buf, stdout);
  // 'n' catches inf and nan
  if (strpbrk(buf, ".en") == NULL) {
    (void)fputs(".0", stdout);
  }
}

/**
 * print_literal() - printing the type and value of a literal node
 * @node: NODE_LITERAL node to print
 */
static void print_literal(const ASTNode *node) {
  (void)printf("LITERAL %s", type_name(node->data_type));
  switch (node->data_type) {
  case INT:
    (void)printf(" %lld", node->data.literal.int_val);
    break;
  case DOUBLE:
    (void)putchar(' ');
    print_double(node->data.literal.dbl_val);
    break;
  case STRING:
    (void)putchar(' ');
    print_string(node->data.literal.string);
    break;
  case BOOLEAN:
    (void)fputs(node->data.literal.bool_val ? " true" : " false", stdout);
    break;
  default:
    break; // Unit has no value to print
  }
}

/**
 * print_list() - printing every node of a linked list on the same level
 * @head: first node of the list (NULL prints nothing)
 * @depth: how deep in the tree the nodes are
 */
static void print_list(const ASTNode *head, int depth) {
  for (const ASTNode *node = head; node != NULL; node = node->next) {
    print_node(node, depth);
  }
}

/**
 * print_slot() - printing a labeled child of a node
 * @label: name of the child, e.g. "cond" or "then"
 * @node: the child, printed one level below its label
 * @depth: how deep in the tree the label is
 */
static void print_slot(const char *label, const ASTNode *node, int depth) {
  print_indent(depth);
  (void)printf("%s:\n", label);
  print_node(node, depth + 1);
}

/**
 * print_func_def() - printing the header line of a function definition
 * @node: NODE_FUNCTION_DEF node to print
 *
 * Format: FUNC_DEF name [@main] (param: Type, ...) : ReturnType
 */
static void print_func_def(const ASTNode *node) {
  const ASTFunctionDefNodeData *def = &node->data.function_def;
  (void)printf("FUNC_DEF %s", def->name);
  if (def->is_main) {
    (void)fputs(" @main", stdout);
  }
  (void)fputs(" (", stdout);
  for (size_t i = 0; i < def->param_count; i++) {
    if (i > 0) {
      (void)fputs(", ", stdout);
    }
    (void)printf("%s: %s", def->params[i].name,
                 type_name(def->params[i].data_type));
  }
  (void)printf(") : %s", type_name(def->return_type));
}

/**
 * print_node() - printing a node and all its children
 * @node: node to print (NULL prints "NULL"), its next siblings aren't printed
 * @depth: how deep in the tree the node is
 *
 * Every node takes one line, its children follow indented by one level.
 * Children whose role isn't clear from their order are printed
 * under a label (cond:, then:, else:, start:, end:, step:, guard:, body:).
 */
static void print_node(const ASTNode *node, int depth) {
  print_indent(depth);
  if (node == NULL) {
    (void)puts("NULL");
    return;
  }

  switch (node->node_type) {
  case NODE_PROGRAM:
    (void)puts("PROGRAM");
    print_list(node->data.program.functions, depth + 1);
    break;

  case NODE_FUNCTION_DEF:
    print_func_def(node);
    (void)putchar('\n');
    print_node(node->data.function_def.body, depth + 1);
    break;

  case NODE_FUNCTION_CALL:
    (void)printf("CALL %s\n", node->data.function_call.name);
    print_list(node->data.function_call.args, depth + 1);
    break;

  case NODE_BLOCK:
    (void)puts("BLOCK");
    print_list(node->data.block.statements, depth + 1);
    break;

  case NODE_VARIABLE_DEF:
    (void)printf("VAR_DEF %s %s : %s\n",
                 node->data.variable_def.variable_type == VAR ? "var" : "val",
                 node->data.variable_def.name,
                 type_name(node->data.variable_def.specified_type));
    print_node(node->data.variable_def.expr, depth + 1);
    break;

  case NODE_VARIABLE_REF:
    (void)printf("VAR_REF %s\n", node->data.variable_ref.name);
    break;

  case NODE_ASSIGN:
    (void)printf("ASSIGN %s\n", node->data.assign.left);
    print_node(node->data.assign.right, depth + 1);
    break;

  case NODE_IF:
    (void)printf("IF %s\n", node->data.if_node.is_expr ? "expr" : "stmt");
    print_slot("cond", node->data.if_node.condition, depth + 1);
    print_slot("then", node->data.if_node.then_branch, depth + 1);
    if (node->data.if_node.else_branch != NULL) {
      print_slot("else", node->data.if_node.else_branch, depth + 1);
    }
    break;

  case NODE_RETURN:
    (void)puts("RETURN");
    if (node->data.return_node.value != NULL) {
      print_node(node->data.return_node.value, depth + 1);
    }
    break;

  case NODE_WHILE:
    (void)puts("WHILE");
    print_slot("cond", node->data.while_node.condition, depth + 1);
    print_slot("body", node->data.while_node.body, depth + 1);
    break;

  case NODE_FOR:
    (void)printf("FOR %s %s\n", node->data.for_node.generator_name,
                 node->data.for_node.includes_end ? "to" : "until");
    print_slot("start", node->data.for_node.range_start, depth + 1);
    print_slot("end", node->data.for_node.range_end, depth + 1);
    if (node->data.for_node.range_step != NULL) {
      print_slot("step", node->data.for_node.range_step, depth + 1);
    }
    for (const ASTNode *guard = node->data.for_node.guards; guard != NULL;
         guard = guard->next) {
      print_slot("guard", guard, depth + 1);
    }
    print_slot("body", node->data.for_node.body, depth + 1);
    break;

  case NODE_LITERAL:
    print_literal(node);
    (void)putchar('\n');
    break;

  case NODE_BINARY_OP:
    (void)printf("BINARY_OP %s\n", operator_name(node->data.binary_op.op));
    print_node(node->data.binary_op.left, depth + 1);
    print_node(node->data.binary_op.right, depth + 1);
    break;

  case NODE_UNARY_OP:
    (void)printf("UNARY_OP %s\n", operator_name(node->data.unary_op.op));
    print_node(node->data.unary_op.operand, depth + 1);
    break;

  default:
    (void)printf("UNKNOWN_NODE %d\n", (int)node->node_type);
    break;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    (void)fprintf(stderr, "Usage: %s <source file>\n", argv[0]);
    return ERR_INTERNAL;
  }

  FILE *src = fopen(argv[1], "r");
  if (src == NULL) {
    (void)fprintf(stderr, "Couldn't open source file.\n");
    return ERR_INTERNAL;
  }

  Scanner sc = scanner_init(src);
  ASTNode *program = NULL;

  // on an error the parser frees the AST itself, only its code goes up
  ReturnCodes rc = parse_input(&sc, &program);
  if (rc == RETURN_SUCCESS) {
    print_node(program, 0);
    ast_free(program);
  }

  (void)fclose(src);
  return rc;
}
