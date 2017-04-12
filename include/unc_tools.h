/**
 * @file unc_tools.h
 *
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */

#ifndef UNC_TOOLS_H_INCLUDED
#define UNC_TOOLS_H_INCLUDED

#include "uncrustify_types.h"
#include "chunk_list.h"


/**
 *  protocol of the line
 *
 *  examples:
 *  prot_the_line(__LINE__, pc->orig_line);
 *  prot_the_line(__LINE__, 6);
 *  examine_Data(__func__, __LINE__, n);
 */
void prot_the_line(
   int32_t  theLine,
   uint32_t actual_line
);


/**
 * tbd
 */
void examine_Data(
   const char* func_name,
   int32_t     theLine,
   int32_t     what
);

/* dump the chunk list to a file */
void dump_out(
   uint32_t type
);

/* create chunk list from a file */
void dump_in(
   uint32_t type
);


#endif /* UNC_TOOLS_H_INCLUDED */
