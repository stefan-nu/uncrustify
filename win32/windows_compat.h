/**
 * @file windows_compat.h
 * Hacks to work with different versions of windows.
 * This is only included if WIN32 is set.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef WINDOWS_COMPAT_H_INCLUDED
#define WINDOWS_COMPAT_H_INCLUDED

#include "windows.h"

#define HAVE_SYS_STAT_H

#define NO_MACRO_VARARG    /* variable parameter numbers don't work on windows */

typedef signed char          int8_t;
typedef short                int16_t;
typedef int                  int32_t;

typedef unsigned char        uint8_t;
typedef unsigned short       uint16_t;
typedef unsigned int         uint32_t;
typedef unsigned long long   uint64_t;

#ifndef PRIx64
#define PRIx64    "llx"
#endif

/* eliminate GNU's attribute */
#define __attribute__(x)

/* MSVC compilers before VC7 don't have __func__ at all;
 * later compiler MSVC versions call it __FUNCTION__. */

/* \todo does MinGW provide a __func__ macro? if so use it */
#ifdef _MSC_VER
#if _MSC_VER < 1300
   #define __func__    "unknown_fct"
#else
   #define __func__    __FUNCTION__
#endif
#else /* _MSC_VER */
   #define __func__    "unknown_fct" // __func__
#endif

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <cstring>
#include <windowsx.h>

#undef  snprintf
#define snprintf       _snprintf

#undef  vsnprintf
#define vsnprintf      _vsnprintf

#undef  strcasecmp
#define strcasecmp     _strcmpi

#undef  strncasecmp
#define strncasecmp    _strnicmp

#undef  strdup
#define strdup         _strdup

#undef  fileno
#define fileno         _fileno

/* includes for _setmode() */
#include <io.h>
#include <fcntl.h>
#include <direct.h>

/* on windows the file permissions have no meaning thus neglect them */
#define mkdir(x, y)    _mkdir(x)

#endif /* WINDOWS_COMPAT_H_INCLUDED */
