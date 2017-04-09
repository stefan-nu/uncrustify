/**
 * @file output.h
 * prototypes for output.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED

#include <stdio.h>
#include "unc_text.h"


/**
 * This renders the chunk list to a file.
 */
void output_text(
   FILE* pfile  /**< [in]  */
);


/**
 * tbd
 */
void output_parsed(
   FILE* pfile  /**< [in]  */
);


/**
 * See also it's preprocessor counterpart
 *   add_long_closebrace_comment
 * in braces.cpp
 *
 * Note: since this concerns itself with the preprocessor -- which is line-oriented --
 * it turns out that just looking at pc->pp_level is NOT the right thing to do.
 * See a --parsed dump if you don't believe this: an '#endif' will be one level
 * UP from the corresponding #ifdef when you look at the tokens 'ifdef' versus 'endif',
 * but it's a whole another story when you look at their CT_PREPROC ('#') tokens!
 *
 * Hence we need to track and seek matching CT_PREPROC pp_levels here, which complicates
 * things a little bit, but not much.
 */
void add_long_preprocessor_conditional_block_comment(void);


/**
 * check if a given character is part of a DOS or UNIX newline
 * thus if it is either a carriage return CARRIAGERETURN or a
 * line feed character
 *
 * \todo parameter should not be int type
 */
bool is_part_of_nl(
   const uint32_t character /**< [in] character to check */
);


/**
 * check if a given character is either a space or a tabstop character
 *
 * \todo parameter should not be int32_t type
 */
bool is_space_or_tab(
   const uint32_t character /**< [in] character to check */
);


#endif /* OUTPUT_H_INCLUDED */
