/**
 * @file newlines.h
 * prototypes for newlines.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef NEWLINES_H_INCLUDED
#define NEWLINES_H_INCLUDED

#include "uncrustify_types.h"
#include "chunk_list.h"


/**
 * Remove all extra newlines.
 * Modify line breaks as needed.
 */
void remove_newlines(void);


/**
 * Step through all chunks.
 */
void cleanup_braces(
   bool first /**< [in]  */
);


/**
 * Handle insertion/removal of blank lines before if/for/while/do and functions
 */
void insert_blank_lines(void);


/**
 * Handle removal of extra blank lines in functions
 * x <= 0: do nothing, x > 0: allow max x-1 blank lines
 */
void newlines_functions_remove_extra_blank_lines(void);


/**
 * tbd
 */
void newlines_squeeze_ifdef(void);


/**
 * \brief removes unnecessary newlines at start and end of a file
 */
void nl_eat_start_and_end(void);


/**
 * Searches for a chunk of type chunk_type and moves them, if needed.
 * Will not move tokens that are on their own line or have other than
 * exactly 1 newline before (UO_pos_comma == TRAIL) or after (UO_pos_comma == LEAD).
 * We can't remove a newline if it is right before a preprocessor.
 */
void nl_chunk_pos(
   c_token_t  chunk_type, /**< [in]  */
   tokenpos_t mode        /**< [in]  */
);


/**
 * Searches for CT_CLASS_COLON and moves them, if needed.
 * Also breaks up the args
 */
void nl_class_colon_pos(
   c_token_t tok /**< [in]  */
);


/**
 * tbd
 */
void newlines_cleanup_dup(void);


/**
 * tbd
 */
void annotations_nl(void);


/**
 * tbd
 */
void nl_after_multiline_cmt(void);


/**
 * Handle insertion of blank lines after label colons
 */
void newline_after_label_colon(void);


/**
 * Scans for newline tokens and changes the nl_count.
 * A newline token has a minimum nl_count of 1.
 * Note that a blank line is actually 2 newlines, unless the newline is the
 * first chunk.
 * So, most comparisons have +1 below.
 */
void do_blank_lines(void);


/**
 * Clears the PCF_ONE_LINER flag on the current line.
 * Done right before inserting a newline.
 */
void undo_one_liner(
   chunk_t* pc /**< [in]  */
);


/**
 * Does a simple Ignore, Add, Remove, or Force after the given chunk
 */
void nl_iarf(
   chunk_t* pc, /**< [in] chunk to operate on */
   argval_t av  /**< [in] The IARF value */
);


/**
 * Add a newline before the chunk if there isn't already a newline present.
 * Virtual braces are skipped, as they do not contribute to the output.
 */
chunk_t* newline_add_before(
   chunk_t* pc /**< [in]  */
);


/**
 * tbd
 */
chunk_t* newline_force_before(
   chunk_t* pc /**< [in]  */
);


/**
 * Add a newline after the chunk if there isn't already a newline present.
 * Virtual braces are skipped, as they do not contribute to the output.
 */
chunk_t* newline_add_after(
   chunk_t* pc /**< [in]  */
);


/**
 * tbd
 */
chunk_t* newline_force_after(
   chunk_t* pc /**< [in]  */
);


/**
 * Removes any CT_NEWLINE or CT_NL_CONT between start and end.
 * Start must be before end on the chunk list.
 * If the 'PCF_IN_PREPROC' status differs between two tags, we can't remove
 * the newline.
 *
 * @return        true/false - removed something
 */
void newline_del_between(
   chunk_t* start, /**< [in] starting chunk (if it is a newline, it will be removed!) */
   chunk_t* end    /**< [in] ending chunk (will not be removed, even if it is a newline) */
);


/**
 * Add a newline between two tokens.
 * If there is already a newline between then, nothing is done.
 * Otherwise a newline is inserted.
 *
 * If end is CT_BRACE_OPEN and a comment and newline follow, then
 * the brace open is moved instead of inserting a newline.
 *
 * In this situation:
 *    if (...) { //comment
 *
 * you get:
 *    if (...)   //comment
 *    {
 */
chunk_t* newline_add_between(
   chunk_t* start, /**< [in]  */
   chunk_t* end    /**< [in]  */
);

#define INVALID_COUNT -1

/**
 *  \brief count the number of newlines between two chunks
 *
 *  @retval >= 0               the number of newlines
 *  @retval INVALID_COUNT (-1) an error occurred
 * */
int32_t newlines_between(
   chunk_t* pc_start,           /**< [in] chunk to start with */
   chunk_t* pc_end,             /**< [in] chunk to stop counting at */
   scope_e scope = scope_e::ALL /**< [in] count all code or only preprocessor */
);

#endif /* NEWLINES_H_INCLUDED */
