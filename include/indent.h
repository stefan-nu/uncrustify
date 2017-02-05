/**
 * @file indent.h
 * prototypes for indent.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef INDENT_H_INCLUDED
#define INDENT_H_INCLUDED

#include "uncrustify_types.h"


/**
 * Change the top-level indentation only by changing the column member in
 * the chunk structures.
 * The level indicator must already be set.
 */
void indent_text(void);


/**
 * Indent the preprocessor stuff from column 1.
 * FIXME: This is broken if there is a comment or escaped newline
 * between '#' and 'define'.
 */
void indent_preproc(void);


/**
 * tbd
 */
void indent_to_column(
   chunk_t *pc,   /**< [in] chunk at the start of the line */
   size_t  column /**< [in] desired column */
);


/**
 * Same as indent_to_column, except we can move both ways
 */
void align_to_column(
   chunk_t *pc,   /**< [in] chunk at the start of the line */
   size_t  column /**< [in] desired column */
);


/**
 * Scan to see if the whole file is covered by one #ifdef
 */
bool ifdef_over_whole_file(void);


/**
 * Changes the initial indent for a line to the given column
 */
void reindent_line(
   chunk_t *pc,   /**< [in] chunk at the start of the line */
   size_t  column /**< [in] desired column */
);


/**
 * tbd
 */
void quick_indent_again(void);


#endif /* INDENT_H_INCLUDED */
