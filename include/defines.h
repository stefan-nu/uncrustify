/**
 * @file defines.h
 * prototypes for defines.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef DEFINES_H_INCLUDED
#define DEFINES_H_INCLUDED

#include "uncrustify_types.h"


/**
 * Loads the defines from a file
 *
 * @param filename   The path to the file to load
 * @retval           EX_OK    - defines successfully loaded from file
 * @retval           EX_IOERR - reading defines file failed
 */
int load_define_file(
   const char *filename /**< [in]  */
);


/**
 * Adds an entry to the define list
 *
 * @param tag        The tag (string) must be zero terminated
 * @param value      nullptr or the value of the define
 */
void add_define(
   const char *tag,  /**< [in]  */
   const char *value /**< [in]  */
);


/**
 * tbd
 */
void print_defines(
   FILE *pfile /**< [in]  */
);


/**
 * tbd
 */
void clear_defines(void);


#endif /* DEFINES_H_INCLUDED */
