/**
 * @file base_types.h
 *
 * Defines some base types, includes config.h
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef BASE_TYPES_H_INCLUDED
#define BASE_TYPES_H_INCLUDED

#include "error_types.h"
#include "char_table.h"


#ifdef WIN32
   #include "windows_compat.h"
   #define PATH_SEP    WIN_PATH_SEP
#else /* not WIN32 */
   #include "config.h"
   #define PATH_SEP    UNIX_PATH_SEP

   #define __STDC_FORMAT_MACROS

   #if defined HAVE_INTTYPES_H
      #include <inttypes.h>
   #else
      #error "Don't know where int8_t is defined"
   #endif

   /* \todo better use types from stdint.h */
   typedef char       CHAR;

   typedef int8_t     INT8;
   typedef int16_t    INT16;
   typedef int32_t    INT32;

   typedef uint8_t    UINT8;
   typedef uint16_t   UINT16;
   typedef uint32_t   UINT32;
   typedef uint64_t   UINT64;
#endif   /* ifdef WIN32 */


/* and the ever-so-important array size macro */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)    (sizeof(x) / sizeof((x)[0]))
#endif

#endif /* BASE_TYPES_H_INCLUDED */
