/**
 * @file punctuators.h
 * header for punctuators.cpp
 */

#ifndef PUNCTUATORS_H_INCLUDED
#define PUNCTUATORS_H_INCLUDED

#include "uncrustify_types.h"


/**
 * tbd
 */
const chunk_tag_t *find_punctuator(
   const char *str,  /**< [in]  */
   lang_t lang_flags /**< [in]  */
);


#endif /* PUNCTUATORS_H_INCLUDED */
