/**
 * @file compat.h
 * prototypes for compat_xxx.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef COMPAT_H_INCLUDED
#define COMPAT_H_INCLUDED

#include "uncrustify_types.h"


/**
 * tbd
 */
bool unc_getenv(
   const char *name, /**< [in]  */
   std::string &str  /**< [in]  */
);


/**
 * tbd
 */
bool unc_homedir(
   std::string &home /**< [in]  */
);


#endif /* COMPAT_H_INCLUDED */
