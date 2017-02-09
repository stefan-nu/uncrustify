/**
 * @file backup.cpp
 * Make a backup of a source file
 * The current plans are to use two files.
 *
 *  - A '.unc-backup~' file that contains the original contents
 *  - A '.unc-backup-md5~' file that contains the MD5 over the last output
 *    that uncrustify generated
 *
 * The logic goes like this:
 *  1. If there isn't a .backup-md5 or the md5 over the input file doesn't
 *     match what is in .backup-md5, then copy the source file to .backup.
 *
 *  2. Create the output file.
 *
 *  3. Calculate the md5 over the output file.
 *     Create the .backup-md5 file.
 *
 * This will let you run uncrustify multiple times over the same file without
 * losing the original file.  If you edit the file, then a new backup is made.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "backup.h"
#include "md5.h"
#include "logger.h"
#include <cstdio>
#include "unc_ctype.h"
#include <cstring>
#include "uncrustify.h"

#define MD5_CHAR_SIZE   2u   /**< size of one hexadecimal character in MD5 string */
#define MD5_CHAR_COUNT 16u   /**< number of word in a MD5 checksum */

/**< overall size of a MD5 checksum string including termination character */
#define MD5_STR_SIZE   ((MD5_CHAR_COUNT * MD5_CHAR_SIZE) + 1u)

void md5_to_string(char *md5_str, const size_t str_len, UINT8 dig[16]);

void md5_to_string(char *md5_str, const size_t str_len, UINT8 dig[16])
{
   int pos = 0;

   for(size_t i = 0; i < MD5_CHAR_COUNT; i++)
   {
      if(pos < (int)(str_len - MD5_CHAR_SIZE))
      {
         pos += snprintf(&md5_str[pos], MD5_CHAR_SIZE, "%02X", dig[i]);
      }
   }

#if 0
   remove only after new code was checked
   snprintf(md5_str, str_len,
            "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
            dig[ 0], dig[ 1], dig[ 2], dig[ 3],
            dig[ 4], dig[ 5], dig[ 6], dig[ 7],
            dig[ 8], dig[ 9], dig[10], dig[11],
            dig[12], dig[13], dig[14], dig[15]);
#endif
}


int backup_copy_file(const char *filename, const vector<UINT8> &data)
{
   char  md5_str_in[MD5_STR_SIZE];
   md5_str_in[0] = 0;

   UINT8 md5_bin   [MD5_CHAR_COUNT];
   MD5::Calc(&data[0], data.size(), md5_bin);
#if 1
   char  md5_str   [MD5_STR_SIZE];
   md5_to_string(md5_str, sizeof(md5_str), md5_bin);
#else
   remove only after new code was checked
   snprintf(md5_str, sizeof(md5_str),
            "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
            md5_bin[ 0], md5_bin[ 1], md5_bin[ 2], md5_bin[ 3],
            md5_bin[ 4], md5_bin[ 5], md5_bin[ 6], md5_bin[ 7],
            md5_bin[ 8], md5_bin[ 9], md5_bin[10], md5_bin[11],
            md5_bin[12], md5_bin[13], md5_bin[14], md5_bin[15]);
#endif

   /* Create the backup-md5 filename, open it and read the md5 */
   char  newpath   [1024];
   snprintf(newpath, sizeof(newpath), "%s%s", filename, UNC_BACKUP_MD5_SUFFIX);

   FILE *thefile = fopen(newpath, "rb");
   if (thefile != NULL)
   {
      char buffer[128];
      if (fgets(buffer, sizeof(buffer), thefile) != NULL)
      {
         for (int i = 0; buffer[i] != 0; i++)
         {
            if (unc_isxdigit(buffer[i]))
            {
               md5_str_in[i] = (char)unc_tolower(buffer[i]);
            }
            else
            {
               md5_str_in[i] = 0;
               break;
            }
         }
      }
      fclose(thefile);
   }

   /* if the MD5s match, then there is no need to back up the file */
   if (memcmp(md5_str, md5_str_in, (MD5_STR_SIZE-1) ) == 0)
   {
      LOG_FMT(LNOTE, "%s: MD5 match for %s\n", __func__, filename);
      return(EX_OK);
   }

   LOG_FMT(LNOTE, "%s: MD5 mismatch - backing up %s\n", __func__, filename);

   /* Create the backup file */
   snprintf(newpath, sizeof(newpath), "%s%s", filename, UNC_BACKUP_SUFFIX);

   thefile = fopen(newpath, "wb");
   if (thefile != NULL)
   {
      const size_t retval   = fwrite(&data[0], data.size(), 1, thefile);
      const int    my_errno = errno;

      fclose(thefile);
      if (retval == 1) { return(EX_OK); }

      LOG_FMT(LERR, "fwrite(%s) failed: %s (%d)\n", newpath, strerror(my_errno), my_errno);
      cpd.error_count++;
   }
   else
   {
      LOG_FMT(LERR, "fopen(%s) failed: %s (%d)\n", newpath, strerror(errno), errno);
      cpd.error_count++;
   }
   return(EX_IOERR);
}


#define FILE_CHUNK 4096
void backup_create_md5_file(const char *filename)
{
   /* Try to open file */
   FILE *thefile = fopen(filename, "rb");
   if (thefile == NULL)
   {
      LOG_FMT(LERR, "%s: fopen(%s) failed: %s (%d)\n",
              __func__, filename, strerror(errno), errno);
      cpd.error_count++;
      return;  /* \todo return or set error code */
   }

   /* read file chunk by chunk and calculate its MD5 checksum */
   MD5 md5;
   md5.Init();
   UINT8  buf[FILE_CHUNK];
   size_t len;
   while ((len = fread(buf, 1, sizeof(buf), thefile)) > 0)
   {
      md5.Update(buf, len);
   }
   fclose(thefile);
   UINT8 md5_bin[16];
   md5.Final(md5_bin);

   char   newpath[1024];
   snprintf(newpath, sizeof(newpath), "%s%s", filename, UNC_BACKUP_MD5_SUFFIX);

   thefile = fopen(newpath, "wb");
   if (thefile != NULL)
   {
#if 1
      char  md5_str[MD5_STR_SIZE];
      md5_to_string(md5_str, sizeof(md5_str), md5_bin);
      fprintf(thefile, "%s  %s\n", md5_str, path_basename(filename));
#else
      fprintf(thefile,
              "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x  %s\n",
              md5_bin[ 0], md5_bin[ 1], md5_bin[ 2], md5_bin[ 3],
              md5_bin[ 4], md5_bin[ 5], md5_bin[ 6], md5_bin[ 7],
              md5_bin[ 8], md5_bin[ 9], md5_bin[10], md5_bin[11],
              md5_bin[12], md5_bin[13], md5_bin[14], md5_bin[15],
              path_basename(filename));
#endif
      fclose(thefile);
   }
}
