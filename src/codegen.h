/**
 * @file codegen.h
 * @brief Code generator's header file
 * @author Tadeáš Čupr <xcuprta00@stud.fit.vut.cz>
 * @date 04.11.2026
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "return.h"
#include <stdio.h>

/**
 * @brief Generates IFJcode26 instructions from an annotated AST.
 * 
 * Traverses the AST (already verified and annotated by semantics) and writes the corresponding intermediate code to the given output stream.
 * 
 * @param program Pointer to the AST root of the entire program.
 * @param out Output stream where the code is written (stdout)
 * @return ReturnCodes RETURN_SUCCESS or ERR_INTERNAL (99)
 */

ReturnCodes generate_code(ASTProgramNodeData *program, FILE *out);

#endif // CODEGEN_H
