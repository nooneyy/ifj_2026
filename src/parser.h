/**
 * @file parser.h
 * @brief Parser's header file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 07.11.2026
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "return.h"
#include "scanner.h"

/**
 * @brief Parses the whole program and builds its AST
 *
 * @param scanner Scanner the parser asks for tokens
 * @param program Set to the root of the built AST on success,
 *                the caller frees it with ast_free()
 * @return RETURN_SUCCESS in case of a success
 *         ERR_LEX_ANA in case of a lexical analysis error
 *         ERR_SYN_ANA in case of a syntactic analysis error
 *         ERR_INTERNAL in case of an internal error
 */
ReturnCodes parse_program(Scanner *scanner, ASTNode **program);

#endif
