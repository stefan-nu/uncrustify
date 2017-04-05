/**
 * @file space.h
 * prototypes for space.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef SPACE_H_INCLUDED
#define SPACE_H_INCLUDED

#include "uncrustify_types.h"


/** setup the function arrays that are used for
 * checking and doing the space processing
 *
 * When building the array o decisions only those decisions will be
 * included that are required by the language of the input source file
 * and the currently active uncrustify options.
 * This avoids unnecessary checks that always will fail and thus
 * speeds up the overall speed f the chunk processing
 *
 * A drawback is however that at the beginning of each file the
 * array has to be recalculated.
 */
void init_space_check_action_array(void);


/**
 * Marches through the whole file and checks to see
 * how many spaces should be between two chunks
 */
void space_text(void);


/**
 * Marches through the whole file and adds spaces around nested parenthesis
 */
void space_text_balance_nested_parens(void);


/**
 * Calculates the column difference between two chunks.
 * The rules are bent a bit here, as AV_IGNORE and AV_ADD become AV_FORCE.
 * So the column difference is either first->len or first->len + 1.
 *
 * @return the column difference between the two chunks
 */
uint32_t space_col_align(
   chunk_t* first, /**< [in] first  chunk */
   chunk_t* second /**< [in] second chunk */
);


/**
 * Determines how many spaces are required between two chunks
 *
 * @return number of required spaces
 */
uint32_t how_many_spaces_are_needed(
   chunk_t* first, /**< [in] first chunk */
   chunk_t* second /**< [in] second chunk */
);


/**
 * tbd
 */
void space_add_after(
   chunk_t* pc,   /**< [in]  */
   uint32_t count /**< [in]  */
);


#endif /* SPACE_H_INCLUDED */
