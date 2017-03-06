/**
 * @file defines.cpp
 * Manages the table of defines for some future time when these will be used to
 * help decide whether a block of #if'd code should be formatted.
 *
 * !! This isn't used right now. !!
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "defines.h"
#include "uncrustify_types.h"
#include "char_table.h"
#include "args.h"
#include <cstring>
#include <cstdlib>
#include <map>
#include "unc_ctype.h"
#include "chunk_list.h"

using namespace std;

typedef map<string, string> defmap;
defmap defines;


void add_define(const char *tag, const char *value)
{
   return_if(ptr_is_invalid(tag) || (*tag == 0));

   value = (ptr_is_valid(value)) ? value : "";

   /* Try to update an existing entry first */
   defmap::iterator it = defines.find(tag);
   if (it != defines.end())
   {
      (*it).second = value;
      LOG_FMT(LDEFVAL, "%s: updated '%s' = '%s'\n", __func__, tag, value);
      return;
   }

   /* Insert a new entry */
   defines.insert(defmap::value_type(tag, value));
   LOG_FMT(LDEFVAL, "%s: added '%s' = '%s'\n", __func__, tag, value);
}


/* \todo DRY with load_keyword_file */
int load_define_file(const char *filename)
{
   retval_if(ptr_is_invalid(filename), EX_CONFIG);
   FILE *pf = fopen(filename, "r");

   if (ptr_is_invalid(pf))
   {
      LOG_FMT(LERR, "%s: fopen(%s) failed: %s (%d)\n", __func__, filename, strerror(errno), errno);
      cpd.error_count++;
      return(EX_IOERR);
   }

   const size_t max_line_size = 160;/**< maximal allowed line size in the define file */
   char   buf[max_line_size];
   size_t line_no = 0;

   /* read file line by line */
   while (fgets(buf, sizeof(buf), pf) != nullptr)
   {
      line_no++;

      /* remove comments after '#' sign */
      char *ptr;
      if ((ptr = strchr(buf, '#')) != nullptr)
      {
         *ptr = 0; /* set string end where comment begins */
      }

      const size_t arg_parts  = 3;  /**< each define argument consists of three parts */
      char *args[arg_parts];
      size_t argc = Args::SplitLine(buf, args, arg_parts-1 );
      args[arg_parts-1] = 0; /* third element of defines is not used currently */

      if (argc > 0)
      {
         if ((argc < arg_parts                ) &&
             (CharTable::IsKW1(*args[0]) ) )
         {
            LOG_FMT(LDEFVAL, "%s: line %zu - %s\n", filename, line_no, args[0]);
            add_define(args[0], args[1]);
         }
         else
         {
            LOG_FMT(LWARN, "%s: line %zu invalid (starts with '%s')\n",
                    filename, line_no, args[0]);
            cpd.error_count++;
         }
      }
   }

   fclose(pf);
   return(EX_OK);
}


void print_defines(FILE *pfile)
{
   defmap::iterator it;
   for (it = defines.begin(); it != defines.end(); ++it)
   {
      fprintf(pfile, "define %*.s%s \"%s\"\n",
              MAX_OPTION_NAME_LEN - 6, " ", (*it).first.c_str(), (*it).second.c_str());
   }
}


void clear_defines(void)
{
   defines.clear();
}
