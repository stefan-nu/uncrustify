/**
 * @file cfg_file.h
 * prototypes for cfg_file.c
 *
 * @author  Stefan Nunninger
 */
#ifndef CFG_FILE_H_INCLUDED
#define CFG_FILE_H_INCLUDED

#include "uncrustify_types.h"


/**
 * Loads configuration values from a file
 *
 * @retval EX_OK    - configuration successfully loaded from file
 * @retval EX_IOERR - reading configuration file failed
 */
int32_t load_from_file(
   const char*    filename,      /**< [in] path to file to read from */
   const uint32_t max_line_size, /**< [in] maximal allowed characters per line */
   const bool     define         /**< true = read define file, false = read keyword file */
);


#endif /* CFG_FILE_H_INCLUDED */
