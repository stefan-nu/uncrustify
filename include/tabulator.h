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
 * Calculate the column of the the next tab stop.
 * Column 1 is the left-most column.
 *
 * @return the next tabstop column
 */
uint32_t calc_next_tab_column(
   uint32_t col,    /**< [in] current column */
   uint32_t tabsize /**< [in] tabsize to use */
);


/**
 * Advances to the next tab stop for output.
 *
 * @return the next tabstop column
 */
uint32_t next_tab_column(
   uint32_t col /**< [in] current column */
);


/**
 * Advances to the next tab stop if not currently on one.
 *
 * @return the next tabstop column
 */
uint32_t align_tab_column(
   uint32_t col /**< [in] current column */
);


#endif /* C_TABULATOR_H_INCLUDED */
