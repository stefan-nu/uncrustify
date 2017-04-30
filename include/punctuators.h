/**
 * @file punctuators.h
 * header for punctuators.cpp
 */

#ifndef PUNCTUATORS_H_INCLUDED
#define PUNCTUATORS_H_INCLUDED

#include "uncrustify_types.h"


/**
 * Checks if the first max. 6 chars of a given string match a punctuator
 *
 * @retval chunk tag of the found punctuator
 * @retval nullptr if nothing found
 */
const chunk_tag_t* find_punctuator(
   const char* str,       /**< [in] string that will be checked, can be shorter than 6 chars */
   lang_t      lang_flags /**< [in] specifies from which language punctuators will be considered */
);


#endif /* PUNCTUATORS_H_INCLUDED */
