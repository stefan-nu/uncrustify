/**
 * @file unc_tools.cpp
 * This file contains lot of tools for debugging
 *
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */

#include "unc_tools.h"
#include "uncrustify.h"


static void log_newline(
   chunk_t *pc /**< [in] chunk to operate with */
);


void prot_the_line(int32_t theLine, uint32_t actual_line)
{
   LOG_FMT(LGUY, "Prot_the_line:(%d) \n", theLine);
   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = pc->next)
   {
      if (pc->orig_line == actual_line)
      {
         LOG_FMT(LGUY, "(%d) orig_line=%d, ", theLine, actual_line);
         switch(pc->type)
         {  /* \todo combine into a single log line */
            case(CT_VBRACE_OPEN ): LOG_FMT(LGUY, "<VBRACE_OPEN>\n");           break;
            case(CT_NEWLINE     ): LOG_FMT(LGUY, "<NL>(%u)\n", pc->nl_count); break;
            case(CT_VBRACE_CLOSE): LOG_FMT(LGUY, "<CT_VBRACE_CLOSE>\n");       break;
            case(CT_SPACE       ): LOG_FMT(LGUY, "<CT_SPACE>\n");              break;
            default:
            {
               LOG_FMT(LGUY, "text() %s, type %s, orig_col=%u, column=%u\n",
                       pc->text(), get_token_name(pc->type), pc->orig_col, pc->column);
               break;
            }
         }
      }
   }
   LOG_FMT(LGUY, "\n");
}


static void log_newline(chunk_t *pc)
{
   if (is_type(pc, CT_NEWLINE))
   {
      LOG_FMT(LGUY, "(%u)<NL> col=%u\n\n", pc->orig_line, pc->orig_col);
   }
   else
   {
      LOG_FMT(LGUY, "(%u)%s %s, col=%u, column=%u\n", pc->orig_line, pc->text(),
            get_token_name(pc->type), pc->orig_col, pc->column);
   }
}


/* \todo examine_Data seems not to be used, is it still required? */
void examine_Data(const char *func_name, int32_t theLine, int32_t what)
{
   LOG_FMT(LGUY, "\n%s:", func_name);

   chunk_t *pc;
   switch (what)
   {
   case 1:
      for (pc = chunk_get_head(); is_valid(pc); pc = pc->next)
      {
         if (is_type(pc, CT_SQUARE_CLOSE, CT_TSQUARE))
         {
            LOG_FMT(LGUY, "\n");
            LOG_FMT(LGUY, "1:(%d),", theLine);
            LOG_FMT(LGUY, "%s, orig_col=%u, orig_col_end=%u\n", pc->text(), pc->orig_col, pc->orig_col_end);
         }
      }
      break;

   case 2:
      LOG_FMT(LGUY, "2:(%d)\n", theLine);
      for (pc = chunk_get_head(); is_valid(pc); pc = pc->next)
      {
         if (pc->orig_line == 7) { log_newline(pc); }
      }
      break;

   case 3:
      LOG_FMT(LGUY, "3:(%d)\n", theLine);
      for (pc = chunk_get_head(); is_valid(pc); pc = pc->next)
      {
         log_newline(pc);
      }
      break;

   case 4:
      LOG_FMT(LGUY, "4:(%d)\n", theLine);
      for (pc = chunk_get_head(); is_valid(pc); pc = pc->next)
      {
         if (pc->orig_line == 6)
         {
            log_newline(pc);
         }
      }
      break;

   default:
      break;
   }
}
