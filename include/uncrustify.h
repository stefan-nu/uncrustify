/**
 * @file uncrustify.h
 * prototypes for uncrustify.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef UNCRUSTIFY_H_INCLUDED
#define UNCRUSTIFY_H_INCLUDED

#include <stdio.h>
#include "token_enum.h"
#include "log_levels.h"
#include "base_types.h"


/**
 * tbd
 */
int main(
   int argc,    /**< [in]  */
   char *argv[] /**< [in]  */
);


/**
 * tbd
 */
const char *get_token_name(
   c_token_t token /**< [in]  */
);


/**
 * Grab the token id for the text.
 * returns CT_NONE on failure to match
 */
c_token_t find_token_name(
   const char *text /**< [in]  */
);


/**
 * tbd
 */
void log_pcf_flags(
   log_sev_t sev, /**< [in]  */
   uint64_t flags /**< [in]  */
);


/**
 * \brief checks if a file uses any of a given language set
 */
bool is_lang(
   cp_data_t &cpd, /**< [in] configuration with language */
   lang_t    lang  /**< [in] set of languages to check for */
);


/**
 * Replace the brain-dead and non-portable basename().
 * Returns a pointer to the character after the last '/'.
 * The returned value always points into path, unless path is nullptr.
 *
 * Input            Returns
 * nullptr          => ""
 * "/some/path/" => ""
 * "/some/path"  => "path"
 * "afile"       => "afile"
 *
 * @param path The path to look at
 * @return     Pointer to the character after the last path separator
 */
const char *path_basename(
   const char *path /**< [in]  */
);


/**
 * Returns the length of the directory part of the filename.
 *
 * @return character size of path
 */
size_t path_dirname_len(
   const char *full_name /**< [in] filename including full path */
);


/**
 * Set idx = 0 before the first call.
 * Done when returns nullptr
 */
const char *get_file_extension(
   size_t &idx /**< [in]  */
);


/**
 * Prints custom file extensions to the file
 */
void print_extensions(
   FILE *pfile /**< [in]  */
);


/**
 * tbd
 */
void usage_exit(
   const char *msg,   /**< [in]  */
   const char *argv0, /**< [in]  */
   int        code    /**< [in]  */
);


/**
 * tbd
 */
const char *extension_add(
   const char *ext_text, /**< [in]  */
   const char *lang_text /**< [in]  */
);


/**
 * tbd
 */
void usage_exit(
   const char *msg,   /**< [in]  */
   const char *argv0, /**< [in]  */
   int        code    /**< [in]  */
);


#endif /* UNCRUSTIFY_H_INCLUDED */
