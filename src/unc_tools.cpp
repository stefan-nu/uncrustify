/**
 * @file unc_tools.cpp
 * This file contains lot of tools for debugging
 *
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */

#include "args.h"
#include "unc_tools.h"
#include "uncrustify.h"
#include "windows_compat.h"


static void log_newline(
   chunk_t* pc /**< [in] chunk to operate with */
);


void prot_the_line(int32_t theLine, uint32_t actual_line)
{
   LOG_FMT(LGUY, "Prot_the_line:(%d) \n", theLine);
   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = pc->next)
   {
      if (pc->orig_line == actual_line)
      {
         LOG_FMT(LGUY, " orig_line=%d, ", actual_line);
         switch(pc->type)
         {  /* \todo combine into a single log line */
            case(CT_VBRACE_OPEN ): LOG_FMT(LGUY, "<VBRACE_OPEN>, ");          break;
            case(CT_NEWLINE     ): LOG_FMT(LGUY, "<NL>(%u), ", pc->nl_count); break;
            case(CT_VBRACE_CLOSE): LOG_FMT(LGUY, "<CT_VBRACE_CLOSE>, ");      break;
            case(CT_SPACE       ): LOG_FMT(LGUY, "<CT_SPACE>, ");             break;
            default:
            {
               LOG_FMT(LGUY, "text() %s, type %s, parent_type %s, orig_col=%u, ",
                       pc->text(), get_token_name(pc->type),
                       get_token_name(pc->ptype), pc->orig_col);
               break;
            }
         }
      }
   }
   LOG_FMT(LGUY, "\n");
}


static void log_newline(chunk_t* pc)
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
void examine_Data(const char* func_name, int32_t theLine, int32_t what)
{
   LOG_FMT(LGUY, "\n%s:", func_name);

   chunk_t* pc;
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


void dump_out(uint32_t type)
{
   char dumpFileName[300];

   if (cpd.dumped_file == nullptr)
   {
      sprintf(dumpFileName, "%s.%u", cpd.filename, type);
   }
   else
   {
      sprintf(dumpFileName, "%s.%u", cpd.dumped_file, type);
   }
   FILE* D_file = fopen(dumpFileName, "w");
   if (D_file != nullptr)
   {
      for (chunk_t* pc = chunk_get_head(); pc != nullptr; pc = pc->next)
      {
         fprintf(D_file, "[%p]\n", pc);
         fprintf(D_file, "  type %s\n",         get_token_name(pc->type));
         fprintf(D_file, "  orig_line %u\n",    pc->orig_line);
         fprintf(D_file, "  orig_col %u\n",     pc->orig_col);
         fprintf(D_file, "  orig_col_end %u\n", pc->orig_col_end);
         fprintf(D_file, (pc->orig_prev_sp  != 0) ? "  orig_prev_sp %u\n"      : "", pc->orig_prev_sp);
         fprintf(D_file, (pc->flags         != 0) ? "  flags %016" PRIu64 "\n" : "", pc->flags);
         fprintf(D_file, (pc->column        != 0) ? "  column %u\n"            : "", pc->column);
         fprintf(D_file, (pc->column_indent != 0) ? "  column_indent %u\n"     : "", pc->column_indent);
         fprintf(D_file, (pc->nl_count      != 0) ? "  nl_count %u\n"          : "", pc->nl_count);
         fprintf(D_file, (pc->level         != 0) ? "  level %u\n"             : "", pc->level);
         fprintf(D_file, (pc->brace_level   != 0) ? "  brace_level %u\n"       : "", pc->brace_level);
         fprintf(D_file, (pc->pp_level      != 0) ? "  pp_level %u\n"          : "", pc->pp_level);
         fprintf(D_file, (pc->after_tab     != 0) ? "  after_tab %d\n"         : "", pc->after_tab);
         if (pc->type != CT_NEWLINE)
         {
            fprintf(D_file, "  text %s\n", pc->text());
         }
      }
      fclose(D_file);
   }
}


#define MAX_BUF_SIZE  256
#define MAX_NAME_SIZE 300
void dump_in(uint32_t type)
{
   char    dumpFileName[MAX_NAME_SIZE];
   char    buffer      [MAX_BUF_SIZE ];
   bool    aNewChunkIsFound = false;
   chunk_t chunk;

   const char* pfile = (cpd.dumped_file == nullptr) ?
                        cpd.filename : cpd.dumped_file;
   sprintf(dumpFileName, "%s.%u", pfile, type);

   FILE* D_file = fopen(dumpFileName, "r");

   if (D_file != nullptr)
   {
      unsigned int lineNumber = 0;
      while (fgets(buffer, sizeof(buffer), D_file) != nullptr)
      {
         ++lineNumber;
         if (aNewChunkIsFound)
         {
            /* look for the next chunk */
            char first = buffer[0];
            if (first == '[')
            {
               aNewChunkIsFound = false;
               /* add the chunk in the list */
               chunk_add_before(&chunk, nullptr);
               chunk.reset();
               aNewChunkIsFound = true;
               continue;
            }
            /* the line as the form
             * part value
             * Split the line */
#define NUMBER_OF_PARTS    3
            char* parts[NUMBER_OF_PARTS];
            int partCount = Args::SplitLine(buffer, parts, NUMBER_OF_PARTS - 1);
            if (partCount != 2)
            {
               exit(EX_SOFTWARE);
            }

            /* \todo better use switch here */
            if (strcasecmp(parts[0], "type") == 0)
            {
               c_token_t tokenName = find_token_name(parts[1]);
               set_type(&chunk, tokenName);
            }
            else if (strcasecmp(parts[0], "orig_line"   ) == 0) { chunk.orig_line    = strtol(parts[1], nullptr, 0); }
            else if (strcasecmp(parts[0], "orig_col"    ) == 0) { chunk.orig_col     = strtol(parts[1], nullptr, 0); }
            else if (strcasecmp(parts[0], "orig_col_end") == 0) { chunk.orig_col_end = strtol(parts[1], nullptr, 0); }
            else if (strcasecmp(parts[0], "orig_prev_sp") == 0) { chunk.orig_prev_sp = strtol(parts[1], nullptr, 0); }
            else if (strcasecmp(parts[0], "flags"       ) == 0) { chunk.flags        = strtol(parts[1], nullptr, 0); }
            else if (strcasecmp(parts[0], "column"      ) == 0) { chunk.column       = strtol(parts[1], nullptr, 0); }
            else if (strcasecmp(parts[0], "nl_count"    ) == 0) { chunk.nl_count     = strtol(parts[1], nullptr, 0); }
            else if (strcasecmp(parts[0], "text"        ) == 0)
            {
               if (chunk.type != CT_NEWLINE)
               {
                  chunk.str = parts[1];
               }
            }
            else
            {
               fprintf(stderr, "on line=%d, for '%s'\n", lineNumber, parts[0]);
               log_flush(true);
               exit(EX_SOFTWARE);
            }
         }
         else
         {
            /* look for a new chunk */
            char first = buffer[0];
            if (first == '[')
            {
               aNewChunkIsFound = true;
               chunk.reset();
            }
         }
      }
      /*  add the last chunk in the list*/
      chunk_add_before(&chunk, nullptr);
      fclose(D_file);
   }
   else
   {
      fprintf(stderr, "FATAL: file not found '%s'\n", dumpFileName);
      log_flush(true);
      exit(EX_SOFTWARE);
   }
}
