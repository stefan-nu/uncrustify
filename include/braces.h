/**
 * @file braces.h
 * prototypes for braces.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef BRACES_H_INCLUDED
#define BRACES_H_INCLUDED

#include "uncrustify_types.h"



/** setup the function arrays that are used for
 * checking and doing the virtual brace conversion
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
void init_vbrace_check_array(void);


/**
 * \brief Change virtual braces into real braces
 */
void do_braces(void);


/**
 * See also it's preprocessor counterpart
 *   add_long_preprocessor_conditional_block_comment
 * in defines.cpp
 */
void add_long_closebrace_comment(void);


/**
 * Adds a comment after the ref chunk
 * Returns the added chunk or nullptr
 */
chunk_t* insert_comment_after(
   chunk_t*       ref,      /**< [in]  */
   c_token_t      cmt_type, /**< [in]  */
   const unc_text &cmt_text /**< [in]  */
);


#endif /* BRACES_H_INCLUDED */
