/**
 * @file scanner.h
 * @brief scanner's header file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "return.h"
#include "tokens.h"
#include <stdio.h>

/**
 * scanner() - main scanner function
 * @src_file: Source file to read from
 *
 * Return: Token for further use in parser
 */
ReturnCodes scanner(FILE *src_file, Token *token);

#endif