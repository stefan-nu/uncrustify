/**
 * @file tabulator.cpp
 * Calculation of tabstop columns
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "tabulator.h"
#include "uncrustify_types.h"
#include "chunk_list.h"


uint32_t calc_next_tab_column(uint32_t col, uint32_t tabsize)
{
   col = max(col, 1u); /* ensure column >= 1 */

   /* \todo explain this calculation */
   if (cpd.frag_cols > 0) { col += cpd.frag_cols-1; }
   col = 1 + ((( (col-1) / tabsize) + 1) * tabsize);
   if (cpd.frag_cols > 0) { col -= cpd.frag_cols-1; }
   return(col);
}


uint32_t next_tab_column(uint32_t col)
{
   return(calc_next_tab_column(col, cpd.settings[UO_output_tab_size].u));
}


uint32_t align_tab_column(uint32_t col)
{
   col = max(col, 1u); /* ensure column >= 1 */

   /* if the current position is not a tab column ... */
   if ((col % cpd.settings[UO_output_tab_size].u) != 1)
   {
      col = next_tab_column(col); /* ... advance to next tab column */
   }
   return(col);
}
