/**
 * @file keywords.h
 * prototypes for keywords.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef KEYWORDS_H_INCLUDED
#define KEYWORDS_H_INCLUDED

#include "uncrustify_types.h"


/**
 * tbd
 */
void init_keywords(void);


/**
 * Loads the dynamic keywords from a file
 *
 * @retval EX_OK    - successfully read keywords from file
 * @retval EX_IOERR - reading keywords file failed
 */
int load_keyword_file(
   const char*  filename,     /**< [in] path to file to read from */
   const size_t max_line_size /**< [in] maximal allowed characters per line */
);


/**
 * Search first the dynamic and then the static table for a matching keyword
 *
 * @return        CT_WORD (no match) or the keyword token
 */
c_token_t find_keyword_type(
   const char *word, /**< [in] Pointer to the text -- NOT zero terminated */
   size_t     len    /**< [in] The length of the text */
);


/**
 * Adds a keyword to the list of dynamic keywords
 */
void add_keyword(
   const char *tag, /**< [in] The tag (string) must be zero terminated */
   c_token_t  type  /**< [in] The type, usually CT_TYPE */
);


/**
 * tbd
 */
void print_keywords(
   FILE *pfile /**< [in]  */
);


/**
 * tbd
 */
void clear_keyword_file(void);


/**
 * Returns the pattern that the keyword needs based on the token
 */
pattern_class_e get_token_pattern_class(
   c_token_t tok /**< [in]  */
);


/**
 * tbd
 */
bool keywords_are_sorted(void);


#endif /* KEYWORDS_H_INCLUDED */
