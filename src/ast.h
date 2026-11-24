/**
 * @file ast.h
 * @brief Header file defining Abstract syntax tree nodes and functions which
 * create/destroy them
 * @author Oldřich Vašíček <xvasico00@stud.fit.vut.cz>
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 30.10.2026
 */

#ifndef AST_H
#define AST_H

#include "return.h"
#include "symbol.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Enum specifying the different types of AST nodes
 */
typedef enum {
  NODE_PROGRAM,       // A program node - the root node of the AST
  NODE_FUNCTION_DEF,  // A function definition node
  NODE_FUNCTION_CALL, // A function call node
  NODE_BLOCK,         // A block node containing a list of statements
  NODE_VARIABLE_DEF,  // A variable definition node
  NODE_VARIABLE_REF,  // A variable reference node
  NODE_ASSIGN,        // An assignment node
  NODE_IF,            // An if statement node
  NODE_RETURN,        // A return statement node
  NODE_WHILE,         // A while loop node
  NODE_FOR,           // A for loop node
  NODE_LITERAL,   // A literal value node (Int, Double, String, Unit, Boolean)
  NODE_BINARY_OP, // A binary operation node
  NODE_UNARY_OP,  // A unary operation node
} ASTNodeType;

/**
 * @brief An enum specifying supported operators
 */
typedef enum {
  OP_ADD, // '+'
  OP_SUB, // '-' (binary)
  OP_MUL, // '*'
  OP_DIV, // '/'
  OP_EQ,  // '=='
  OP_NEQ, // '!='
  OP_LT,  // '<'
  OP_GT,  // '>'
  OP_LTE, // '<='
  OP_GTE, // '>='
  OP_AND, // '&&'
  OP_OR,  // '||'
  OP_NEG, // '-' (unary)
  OP_NOT, // '!'
} ASTOperator;

typedef struct ASTNode ASTNode;

/**
 * @brief Data specific to the Program node
 */
typedef struct {
  ASTNode *functions; // A linked list of function definitions in the program
} ASTProgramNodeData;

/**
 * @brief Data specific to the Block node
 */
typedef struct {
  ASTNode *statements; // A linked list of statements in the block
} ASTBlockNodeData;

/**
 * @brief Data specific to the Function Definition node
 */
typedef struct {
  char *
      name; // The name of the function, owned by this struct (taken from token)
  FunctionParam *params; // Array of function parameters
  size_t param_count;    // Count of function parameters
  size_t
      *list_sizes; // Sizes of parameter lists in params (NULL if no parameters)
  size_t list_count;    // Count of parameter lists in params
  DataType return_type; // The return type of the function (UNKNOWN until set by
                        // parser)
  bool is_main;         // Is this function annotated with "@main"?
  ASTNode *body;        // The body of the function (Block node, non-NULL once
                        // successfully parsed)
} ASTFunctionDefNodeData;

/**
 * @brief Data specific to the Function Call node
 */
typedef struct {
  char *name;    // The name of the function being called, owned by this struct
                 // (taken from token)
  ASTNode *args; // Linked list of function arguments (any expr nodes -
                 // NODE_LITERAL, NODE_VARIABLE_REF, NODE_BINARY_OP,
                 // NODE_UNARY_OP, NODE_FUNCTION_CALL)
} ASTFunctionCallNodeData;

/**
 * @brief Data specific to the Variable Definition node
 */
typedef struct {
  char *
      name; // The name of the variable, owned by this struct (taken from token)
  VariableType variable_type; // The mutability of the variable (val - const,
                              // var - mutable)
  DataType specified_type; // The specified type of the variable (val x: Int =
                           // ... => INT)
  ASTNode *expr;           // The expression node on the right side of the def.
} ASTVarDefNodeData;

/**
 * @brief Data specific to the Assignment node
 */
typedef struct {
  char *left;     // The name of the reassigned variable, owned by this struct
                  // (taken from token)
  ASTNode *right; // The expression node on the right side of the assignment
} ASTAssignNodeData;

/**
 * @brief Data specific to the Variable Reference node
 */
typedef struct {
  char *name; // The name of the referenced variable, owned by this struct
              // (taken from token)
} ASTVarRefNodeData;

/**
 * @brief Data specific to the If node
 */
typedef struct {
  ASTNode *condition;   // An expression node specifying the condition of the if
                        // statement
  ASTNode *then_branch; // A block or expression node specifying the "then"
                        // branch of the if statement
  ASTNode *else_branch; // A block, if or expression node specifying the "else"
                        // branch of the if statement (can be NULL)
  bool is_expr; // True if used as an expr (returns a value), false if used as a
                // statement
} ASTIfNodeData;

/**
 * @brief Data specific to the Return node
 */
typedef struct {
  // An expression node specifying the returned value (can be NULL)
  ASTNode *value;
} ASTReturnNodeData;

/**
 * @brief Data specific to the While node
 */
typedef struct {
  ASTNode *condition; // An expression node specifying the condition of the
                      // while statement
  ASTNode *body;      // A block node specifying the body of the while
                      // statement
} ASTWhileNodeData;

/**
 * @brief Data specific to the For loop node
 */
typedef struct {
  char *generator_name; // The name of the for loop variable [example: (for x <-
                        // 0 to 10) => x]
  ASTNode *range_start; // The expression of the start boundary [example: (for x
                        // <- (z + 1) to 10) => (z + 1)]
  ASTNode *range_end;  // The expression of the end boundary [example: (for x <-
                       // 0 to (z - 5)) => (z - 5)]
  ASTNode *range_step; // The expression of the step size [example: (for x <- 0
                       // to 10 by 2) => 2]
  ASTNode *guards; // The guard conditions of the for loop (i < 5, i < z,...) -
                   // MULTIPLE! Append with ast_ll_append
  ASTNode *body; // A block or expression node(if one-line) specifying the body
                 // of the for loop
  bool includes_end; // If the end boundary is included in the range or not (to
                     // = true, until = false)
} ASTForNodeData;

/**
 * @brief Data specific to the Literal node
 */
typedef union {
  long long int_val; // if data_type == INT
  double dbl_val;    // if data_type == DOUBLE
  char *
      string; // if data_type == STRING, owned by this struct (taken from token)
  bool bool_val; // if data_type == BOOLEAN
} ASTLiteralNodeData;

/**
 * @brief Data specific to the Binary Operation node
 */
typedef struct {
  ASTNode *left;  // The left operand of the binary operation
  ASTNode *right; // The right operand of the binary operation
  ASTOperator op;
} ASTBinaryOpNodeData;

/**
 * @brief Data specific to the Unary Operation node
 */
typedef struct {
  ASTNode *operand;
  ASTOperator op;
} ASTUnaryOpNodeData;

/**
 * @brief Union of node content depending on the node type
 */
typedef union {
  ASTProgramNodeData program;
  ASTBlockNodeData block;
  ASTFunctionDefNodeData function_def;
  ASTFunctionCallNodeData function_call;
  ASTVarDefNodeData variable_def;
  ASTVarRefNodeData variable_ref;
  ASTAssignNodeData assign;
  ASTIfNodeData if_node;
  ASTReturnNodeData return_node;
  ASTWhileNodeData while_node;
  ASTForNodeData for_node;
  ASTLiteralNodeData literal;
  ASTBinaryOpNodeData binary_op;
  ASTUnaryOpNodeData unary_op;
} ASTNodeData;

/**
 * @brief Struct describing an AST node and data (that every node type must
 * have)
 */
struct ASTNode {
  ASTNodeType node_type;
  DataType data_type; // The data type the node may evaluate to (usually filled
                      // by semantic analysis, UNKNOWN if not known yet)
  unsigned int line;  // The line number in the input where the node was created
                      // (error logging)
  ASTNode *
      next; // An optional pointer to the next node if in a linked list of nodes
            // (program functions, block statements, function call args,...)
  ASTNodeData data; // The data specific to the node type
};

/**
 * @brief A constructor function for the root Program node of the AST
 * @return A pointer to the created AST node on success, otherwise NULL
 */
ASTNode *ast_init_program(void);

/**
 * @brief A constructor function for a Function Definition node
 * @param name A string containing the function name
 * @param is_main Is the function annotated with "@main"?
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (name
 * gets freed)
 */
ASTNode *ast_init_func_def(char *name, bool is_main, unsigned int line);

/**
 * @brief Function for adding a parameter to a Function Definition node
 * @param func_def A pointer to the Function Definition node
 * @param param_name A string containing the parameter name
 * @param data_type The data type of the parameter
 * @return RETURN_SUCCESS on success, ERR_INTERNAL if func_def is not a Function
 * Definition node or if realloc fails (param_name gets freed)
 */
ReturnCodes ast_func_def_add_param(ASTNode *func_def, char *param_name,
                                   DataType data_type);

/**
 * @brief Function for adding a new (empty) parameter list to a Function
 * Definition node
 * @param func_def A pointer to the Function Definition node
 * @return RETURN_SUCCESS on success, ERR_INTERNAL if func_def is not a Function
 * Definition node or if realloc fails
 */
ReturnCodes ast_func_def_add_param_list(ASTNode *func_def);

/**
 * @brief A constructor function for a Block node
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL
 */
ASTNode *ast_init_block(unsigned int line);

/**
 * @brief A constructor function for a Variable Definition node
 * @param name A string containing the variable name
 * @param variable_type The mutability type of the variable
 * @param specified_type The explicitly specified data type (UNKNOWN if not
 * specified)
 * @param expr A pointer to the expression node representing the initial value
 * (REQUIRED!)
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (name
 * and expr get freed)
 */
ASTNode *ast_init_var_def(char *name, VariableType variable_type,
                          DataType specified_type, ASTNode *expr,
                          unsigned int line);

/**
 * @brief A constructor function for an Assignment node
 * @param left A string containing the variable name being assigned to
 * @param right A pointer to the expression node representing the value being
 * assigned
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (left
 * and right get freed)
 */
ASTNode *ast_init_assign(char *left, ASTNode *right, unsigned int line);

/**
 * @brief A constructor function for an If node
 * @param condition A pointer to the expression node representing the condition
 * @param then_branch A pointer to the block or expression node representing the
 * "then" branch
 * @param else_branch A pointer to the block or expression node representing the
 * "else" branch (can be NULL)
 * @param is_expr True if used as an expression (returns a value), false if used
 * as a statement
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (
 * condition, then_branch and else_branch get freed)
 */
ASTNode *ast_init_if(ASTNode *condition, ASTNode *then_branch,
                     ASTNode *else_branch, bool is_expr, unsigned int line);

/**
 * @brief A constructor function for a While node
 * @param condition A pointer to the expression node representing the condition
 * @param body A pointer to the block node representing the body of the while
 * loop
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL
 * (condition and body get freed)
 */
ASTNode *ast_init_while(ASTNode *condition, ASTNode *body, unsigned int line);

/**
 * @brief A constructor function for a For node
 * @param generator_name The name of the for loop variable (i,...)
 * @param range_start A pointer to the expression node representing the start
 * boundary
 * @param range_end A pointer to the expression node representing the end
 * boundary
 * @param range_step A pointer to the expression node representing the step size
 * (can be NULL if no "by" in for loop)
 * @param body A pointer to the body or expression node(if one-line)
 * representing the body of the for loop
 * @param includes_end Is the end boundary included in the for loop range?
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (all
 * ASTNode pointers and generator_name get freed)
 */
ASTNode *ast_init_for(char *generator_name, ASTNode *range_start,
                      ASTNode *range_end, ASTNode *range_step, ASTNode *body,
                      bool includes_end, unsigned int line);

/**
 * @brief A constructor function for a Return node
 * @param value A pointer to the expression node representing the return value
 * (can be NULL)
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (value
 * gets freed)
 */
ASTNode *ast_init_return(ASTNode *value, unsigned int line);

/**
 * @brief A constructor function for a Function Call node
 * @param name A string containing the function name being called
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (name
 * gets freed)
 */
ASTNode *ast_init_func_call(char *name, unsigned int line);

/**
 * @brief A constructor function for a Binary Operation node
 * @param op A binary operator
 * @param left A pointer to the left operand node
 * @param right A pointer to the right operand node
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (operand
 * nodes get freed)
 */
ASTNode *ast_init_binary_op(ASTOperator op, ASTNode *left, ASTNode *right,
                            unsigned int line);

/**
 * @brief A constructor function for a Unary Operation node
 * @param op A unary operator
 * @param operand A pointer to the operand node
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (operand
 * node gets freed)
 */
ASTNode *ast_init_unary_op(ASTOperator op, ASTNode *operand, unsigned int line);

/**
 * @brief A constructor function for a Variable Reference node
 * @param name A string containing the variable name
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL (name
 * gets freed)
 */
ASTNode *ast_init_var_ref(char *name, unsigned int line);

/**
 * @brief A constructor function for a Literal node
 * @param data_type The data type of the literal (INT, DOUBLE, STRING, UNIT,
 * BOOLEAN)
 * @param value The value of the literal (from the ASTLiteralNodeData union)
 * @param line The line number in the input where the node was created
 * @return A pointer to the created AST node on success, otherwise NULL
 * (value.string gets freed if data_type == STRING)
 */
ASTNode *ast_init_literal(DataType data_type, ASTLiteralNodeData value,
                          unsigned int line);

/**
 * @brief Appends a node to the end of a linked list of AST nodes (program
 * functions, block statements, function call args,...)
 * @param list_start A pointer to the head pointer of the linked list (if NULL,
 * starts a new list there)
 * @param node A pointer to the node to be appended
 */
void ast_ll_append(ASTNode **list_start, ASTNode *node);

/**
 * @brief Destructor of an AST node, its linked-list siblings and all its
 * children
 * @param node A pointer to the node to be freed (NULL is allowed)
 */
void ast_free(ASTNode *node);

#endif
