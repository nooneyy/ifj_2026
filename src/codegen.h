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
 * Traverses the AST (already verified and annotated by semantics) and writes
 * the corresponding intermediate code to the output stream.
 * Neither the AST nor the output stream are freed/closed by this function.
 *
 * @param program Pointer to the AST root of the entire program. (caller retains
 * ownership)
 * @param out Output stream where the code is written (stdout)
 * @return RETURN_SUCCESS in case of a success
 *         ERR_INTERNAL in case of an error during codegen
 */
ReturnCodes generate_code(ASTNode *program, FILE *out);

#endif // CODEGEN_H
