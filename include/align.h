/**
 * @file align.h
 * prototypes for align.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#ifndef ALIGN_H_INCLUDED
#define ALIGN_H_INCLUDED

#include "uncrustify_types.h"


enum CmtAlignType_t
{
   CAT_REGULAR,
   CAT_BRACE,
   CAT_ENDIF
};


void align_all(void);


/**
 * Aligns all backslash-newline combos in the file.
 * This should be done LAST.
 */
void align_backslash_newline(void);


/**
 * tbd
 */
void align_right_comments(void);


/**
 * Scans the whole file for #defines. Aligns all within X lines of each other
 */
void align_preprocessor(void);


/**
 * Aligns stuff inside a multi-line "= { ... }" sequence.
 */
void align_struct_initializers(void);


/**
 * For a series of lines ending in backslash-newline, align them.
 * The series ends when a newline or multi-line C comment is encountered.
 *
 * @param start   Start point
 * @return        pointer the last item looked at (nullptr/newline/comment)
 */
chunk_t* align_nl_cont(
   chunk_t* start /**< [in]  */
);


/**
 * Aligns all assignment operators that are on the same level as the
 * first one, starting with the first.
 *
 * For variable definitions, only consider the '=' for the first variable.
 * Otherwise, only look at the first '=' on the line.
 */
chunk_t* align_assign(
   chunk_t*  first,     /**< [in] chunk pointing to the first assignment */
   uint32_t  span,      /**< [in]  */
   uint32_t  thresh,    /**< [in]  */
   uint32_t* p_nl_count /**< [in]  */
);


/**
 * tbd
 */
void quick_align_again(void);


#endif /* ALIGN_H_INCLUDED */
