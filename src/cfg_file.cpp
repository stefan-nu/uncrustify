/*******************************************************************************
 * Copyright (c) 2017 nuinno
 * Alle Rechte vorbehalten. All Rights Reserved.
 *
 * Information contained herein is subject to change without notice.
 * nuinno retains ownership and all other rights in the software and each
 * component thereof.
 * Any reproduction of the software or components thereof without the prior
 * written permission of nuinno is prohibited.
 ***************************************************************************//**
 * \file       cfg_file.c
 * \brief      read configuration files
 * \note		   Project: uncrustify
 * \note		   Compiler: GCC
 * \date		   30.03.2017
 * \author     Stefan Nunninger
 * \author     nuinno, Erlangen
 ******************************************************************************/

/** @addtogroup common
 *  @{
 */


/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "cfg_file.h"
#include "args.h"
#include "char_table.h"
#include "chunk_list.h"
#include "defines.h"
#include "keywords.h"


/*******************************************************************************
 * Private Defines, Typedefs, Enums, etc.
 ******************************************************************************/


/*******************************************************************************
 * Private Function Prototyps
 ******************************************************************************/


/*******************************************************************************
 * Global variables
 ******************************************************************************/


/*******************************************************************************
 * Function Implementation
 ******************************************************************************/
int32_t load_from_file(const char* filename, const uint32_t max_line_size, const bool define)
{
   retval_if(ptr_is_invalid(filename), EX_CONFIG);

   FILE* pf = fopen(filename, "r");
   if (ptr_is_invalid(pf))
   {
      LOG_FMT(LERR, "%s: fopen(%s) failed: %s (%d)\n", __func__, filename, strerror(errno), errno);
      cpd.error_count++;
      return(EX_IOERR);
   }

   char     buf[max_line_size];
   uint32_t line_no = 0;

   /* read file line by line */
   while (fgets(buf, sizeof(buf), pf) != nullptr)
   {
      line_no++;

      /* remove comments after '#' sign */
      char* ptr = strchr(buf, '#');
      if (ptr_is_valid(ptr))
      {
         *ptr = 0; /* set string end where comment begins */
      }

      const uint32_t arg_parts = 2;  /**< each define argument consists of three parts */
      char*    args[arg_parts+1];
      uint32_t argc = Args::SplitLine(buf, args, arg_parts);
      args[arg_parts] = 0; /* third element of defines is not used currently */

      if (argc > 0)
      {
         if ((argc < arg_parts           ) &&
             (CharTable::IsKW1(*args[0]) ) )
         {
            LOG_FMT(LDEFVAL, "%s: line %u - %s\n", filename, line_no, args[0]);

            /* currently we read either defines or keywords from a file */
            if(define == true) { add_define (args[0], args[1]); }
            else               { add_keyword(args[0], CT_TYPE); }
         }
         else
         {
            LOG_FMT(LWARN, "%s line %u invalid (starts with '%s')\n",
                    filename, line_no, args[0]);
            cpd.error_count++;
         }
      }
      else { continue; } /* the line is empty */
   }

   fclose(pf);
   return(EX_OK);
}


/** @} */
