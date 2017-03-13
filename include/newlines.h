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


/**
 * Remove all extra newlines.
 * Modify line breaks as needed.
 */
void newlines_remove_newlines(void);


/**
 * Step through all chunks.
 */
void newlines_cleanup_braces(
   bool first
);


/**
 * Handle insertion/removal of blank lines before if/for/while/do and functions
 */
void newlines_insert_blank_lines(void);


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
 * tbd
 */
void newlines_eat_start_end(void);


/**
 * Searches for a chunk of type chunk_type and moves them, if needed.
 * Will not move tokens that are on their own line or have other than
 * exactly 1 newline before (UO_pos_comma == TRAIL) or after (UO_pos_comma == LEAD).
 * We can't remove a newline if it is right before a preprocessor.
 */
void newlines_chunk_pos(
   c_token_t chunk_type,
   tokenpos_t mode
);


/**
 * Searches for CT_CLASS_COLON and moves them, if needed.
 * Also breaks up the args
 */
void newlines_class_colon_pos(
   c_token_t tok
);


/**
 * tbd
 */
void newlines_cleanup_dup(void);


/**
 * tbd
 */
void annotations_newlines(void);


/**
 * tbd
 */
void newline_after_multiline_comment(void);


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
   chunk_t *pc
);


/**
 * Does a simple Ignore, Add, Remove, or Force after the given chunk
 *
 * @param pc   The chunk
 * @param av   The IARF value
 */
void newline_iarf(
   chunk_t *pc,
   argval_t av
);


/**
 * Add a newline before the chunk if there isn't already a newline present.
 * Virtual braces are skipped, as they do not contribute to the output.
 */
chunk_t *newline_add_before(
   chunk_t *pc
);


/**
 * tbd
 */
chunk_t *newline_force_before(
   chunk_t *pc
);


/**
 * Add a newline after the chunk if there isn't already a newline present.
 * Virtual braces are skipped, as they do not contribute to the output.
 */
chunk_t *newline_add_after(
   chunk_t *pc
);


/**
 * tbd
 */
chunk_t *newline_force_after(
   chunk_t *pc
);


/**
 * Removes any CT_NEWLINE or CT_NL_CONT between start and end.
 * Start must be before end on the chunk list.
 * If the 'PCF_IN_PREPROC' status differs between two tags, we can't remove
 * the newline.
 *
 * @param start   The starting chunk (if it is a newline, it will be removed!)
 * @param end     The ending chunk (will not be removed, even if it is a newline)
 * @return        true/false - removed something
 */
void newline_del_between(
   chunk_t *start,
   chunk_t *end
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
chunk_t *newline_add_between(
   chunk_t *start,
   chunk_t *end
);


#endif /* NEWLINES_H_INCLUDED */
