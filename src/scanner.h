/**
 * @file scanner.h
 * @brief scanner's header file
 * @author Adam Neugebauer <xneugea00@stud.fit.vut.cz>
 * @date 03.10.2026
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "tokens.h"

/**
 * scanner() - main scanner function
 * @file_name:  Name of the source file to compile handed from program's main
 *
 * Return: Token for further use in parser
 */
Token scanner(char *file_name);

#endif