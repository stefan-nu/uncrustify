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

#include <cstring>
#include <cstdlib>
#include <map>
#include "defines.h"
#include "uncrustify_types.h"
#include "char_table.h"
#include "args.h"
#include "cfg_file.h"
#include "keywords.h"
#include "unc_ctype.h"
#include "chunk_list.h"

using namespace std;

typedef map<string, string> defmap;
defmap defines;


void add_define(const char* tag, const char* value)
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


int32_t load_define_file(const char* filename, const uint32_t max_line_size)
{
   return load_from_file(filename, max_line_size, true);
}


void print_defines(FILE* pfile)
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
