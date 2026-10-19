/**
 * @file symbol.h
 * @brief Header file defining Symbol structures and functions which
 * create/destroy them
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 12.10.2026
 */

#include <stdbool.h>

#ifndef SYMBOL_H
#define SYMBOL_H

/**
 * @brief Enum specifying the two possible options for a symbol - a variable
 * or a function
 */
typedef enum { VARIABLE, FUNCTION } SymbolType;

/**
 * @brief Enum specifying IFJ26 data types
 */
typedef enum {
  INT,
  DOUBLE,
  STRING,
  UNIT,
  BOOLEAN,
  ANY,     // Used by builtin functions
  UNKNOWN, // Intermediate data type when we don't know the type yet
} DataType;

/**
 * @brief Enum describing the mutability of variables - in IFJ26 a variable is
 * either val (immutable), var (mutable) or a function param
 */
typedef enum { VAL, VAR, FUNC_PARAM } VariableType;

/**
 * @brief Struct containing data related to the Variable symbol
 */
typedef struct {
  DataType data_type;
  VariableType variable_type;
} VariableSymbol;

/**
 * @brief Struct containing data related to a single function parameter
 */
typedef struct {
  char *name;
  DataType data_type;
} FunctionParam;

/**
 * @brief Struct containing data related to the Function symbol
 */
typedef struct {
  DataType return_type;
  bool return_type_defined; // True if it has a written return type, otherwise
                            // false
  bool is_main;             // True if function is annotated with @main
  bool is_builtin;          // True if function is builtin
  unsigned param_count;
  FunctionParam *params;
} FunctionSymbol;

/**
 * @brief Union containing either function symbol data or variable symbol data
 */
typedef union {
  FunctionSymbol function;
  VariableSymbol variable;
} SymbolData;

/**
 * @brief Struct describing a single Symbol (function/variable) and its data
 */
typedef struct {
  char *name; // Identifier of the symbol (function/variable name, acts as
              // hashtable key)
  SymbolType type;
  SymbolData data;
} Symbol;

/**
 * @brief A variable symbol constructor
 * @param name A string containing the variable name (also acts as hashtable
 * key)
 * @param data_type An enum describing the variable's data type (INT,
 * STRING,...)
 * @param variable_type An enum describing the variable's mutability (val -
 * const,...)
 * @return A pointer to the created symbol on success, otherwise NULL
 */
Symbol *init_variable_symbol(char *name, DataType data_type,
                             VariableType variable_type);

/**
 * @brief A function symbol constructor
 * @param name A string containing the function's name (also acts as hashtable
 * key)
 * @param return_type An enum describing the function's return type (INT,
 * STRING,...)
 * @param return_type_defined Is the return type written in the function
 * signature?
 * @param is_builtin Is the function builtin?
 * @param is_main Is the function annotated with @main?
 * @return A pointer to the created symbol on success, otherwise NULL
 */
Symbol *init_function_symbol(char *name, DataType return_type,
                             bool return_type_defined, bool is_builtin,
                             bool is_main);

/**
 * @brief A function which adds a parameter to an existing function symbol
 * @param function_sym A pointer to the function symbol
 * @param param_name A string containing the param's identifier
 * @param param_data_type The data type of the parameter
 */
void add_function_param(Symbol *function_sym, char *param_name,
                        DataType param_data_type);

/**
 * @brief A symbol destructor
 * @param symbol A pointer to the symbol
 */
void free_symbol(Symbol *symbol);

#endif
