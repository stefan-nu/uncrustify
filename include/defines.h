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
 * @retval EX_OK    - defines successfully loaded from file
 * @retval EX_IOERR - reading defines file failed
 */
int load_define_file(
   const char*  filename,     /**< [in] path to file to read from */
   const size_t max_line_size /**< [in] maximal allowed characters per line */
);


/**
 * Adds an entry to the define list
 */
void add_define(
   const char *tag,  /**< [in] tag (string) must be zero terminated */
   const char *value /**< [in] nullptr or the value of the define */
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
