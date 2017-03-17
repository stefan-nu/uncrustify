/**
 * @file tabulator.h
 * Header for tabulator.cpp
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef C_TABULATOR_H_INCLUDED
#define C_TABULATOR_H_INCLUDED

#include "uncrustify_types.h"


/**
 * Advances to the next tab stop.
 * Column 1 is the left-most column.
 *
 * @return the next tabstop column
 */
size_t calc_next_tab_column(
   size_t col,    /**< [in] the current column */
   size_t tabsize /**< [in] the tabsize */
);


/**
 * Advances to the next tab stop for output.
 *
 * @return the next tabstop column
 */
size_t next_tab_column(
   size_t col /**< [in] the current column */
);


/**
 * Advances to the next tab stop if not currently on one.
 *
 * @return the next tabstop column
 */
size_t align_tab_column(
   size_t col /**< [in] the current column */
);


#endif /* C_TABULATOR_H_INCLUDED */
