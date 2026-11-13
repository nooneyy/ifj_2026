/**
 * @file semantic.h
 * @brief Semantic analysis header file
 * @author Tomáš Kolařík <xkolart01@stud.fit.vut.cz>
 * @date 13.11.2026
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "return.h"

/**
 * @brief Function which performs semantic analysis on the generated AST
 * @param program The root node of the AST (program node)
 * @return RETURN_SUCCESS on success, or one of the many Semantic Errors on
 * failure
 */
ReturnCodes semantic_analysis(ASTNode *program);

#endif
