/**
 * @file logmask.cpp
 *
 * Functions to convert between a string and a severity mask.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "logmask.h"
#include <cstdio>      /* provides snprintf() */
#include <cstdlib>     /* provides strtoul() */
#include "chunk_list.h"
#include "unc_ctype.h"


char* logmask_to_str(const log_mask_t &mask, char* buf, uint32_t size)
{
   retval_if((ptr_is_invalid(buf) || (size == 0)), buf);

   int32_t  last_sev = -1;
   bool     is_range = false;
   uint32_t len      = 0;

   for (int32_t sev = 0; sev < 256; sev++)
   {
      if (logmask_test(mask, static_cast<log_sev_t>(sev)))
      {
         if (last_sev == -1)
         {
            len += (uint32_t)snprintf(&buf[len], size - len, "%d,", sev);
         }
         else
         {
            is_range = true;
         }
         last_sev = sev;
      }
      else
      {
         if (is_range)
         {
            buf[len - 1] = '-';  /* change last comma to a dash */
            len         += (uint32_t)snprintf(&buf[len], size - len, "%d,", last_sev);
            is_range     = false;
         }
         last_sev = -1;
      }
   }

   /* handle a range that ends on the last bit */
   if (is_range && (last_sev != -1))
   {
      buf[len - 1] = '-';  /* change last comma to a dash */
      len         += (uint32_t)snprintf(&buf[len], size - len, "%d", last_sev);
   }
   else
   {
      /* Eat the last comma */
      if (len > 0) { len--; }
   }

   buf[len] = 0;

   return(buf);
}


void logmask_from_string(const char* str, log_mask_t &mask)
{
   return_if(ptr_is_invalid(str));

   logmask_set_all(mask, false); /* Start with a clean mask */

   /* If the first character is 'a' or 'A', set all severities */
   if (unc_toupper(*str) == 'A')
   {
      logmask_set_all(mask, true);
      str++;
   }

   bool  was_dash   = false;
   int32_t   last_level = -1;
   while (*str != 0) /* check string until termination character */
   {
      if (unc_isspace(*str)) /* ignore spaces */
      {
         str++;              /* and go on with */
         continue;           /* next character */
      }

      if (unc_isdigit(*str))
      {
         char* ptmp;
         uint32_t level = strtoul(str, &ptmp, 10);
         str = ptmp;

         logmask_set_sev(mask, static_cast<log_sev_t>(level), true);
         if (was_dash)
         {
            for (uint32_t idx = (uint32_t)(last_level + 1); idx < level; idx++)
            {
               logmask_set_sev(mask, static_cast<log_sev_t>(idx), true);
            }
            was_dash = false;
         }

         last_level = (int32_t)level;
      }
      else if (*str == '-') /* a dash marks all bits */
      {                     /* until the next number */
         was_dash = true;
         str++;
      }
      else  /* probably a comma */
      {
         last_level = -1;
         was_dash   = false;
         str++;
      }
   }
}
