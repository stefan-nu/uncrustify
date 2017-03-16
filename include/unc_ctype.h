/**
 * @file unc_ctype.h
 * The ctype function are only required to handle values 0-255 and EOF.
 * A char is sign-extended when cast to an int.
 * With some C libraries, these values cause a crash.
 * These wrappers will properly handle all char values.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef UNC_CTYPE_H_INCLUDED
#define UNC_CTYPE_H_INCLUDED

#include <cctype>


/**
 * Truncate anything except EOF (-1) to 0-255
 */
static inline int unc_fix_ctype(
   int ch /**< [in]  */
)
{
   return((ch == -1) ? -1 : (ch & 0xff));
}


/**
 * check if a character is a space
 */
static inline int unc_isspace(
   int ch /**< [in] character to check */
)
{
   return(isspace(unc_fix_ctype(ch)));
}


/**
 * tbd
 */
static inline int unc_isprint(
   int ch /**< [in]  */
)
{
   return(isprint(unc_fix_ctype(ch)));
}


/**
 * tbd
 */
static inline int unc_isalpha(
   int ch /**< [in]  */
)
{
   return(isalpha(unc_fix_ctype(ch)));
}


/**
 * tbd
 */
static inline int unc_isalnum(
   int ch /**< [in]  */
)
{
   return(isalnum(unc_fix_ctype(ch)));
}


/**
 * convert a character to upper case
 */
static inline int unc_toupper(
   int ch /**< [in] character to convert */
)
{
   return(toupper(unc_fix_ctype(ch)));
}


/**
 * convert a character to lower case
 */
static inline int unc_tolower(
   int ch /**< [in] character to convert */
)
{
   return(tolower(unc_fix_ctype(ch)));
}


/**
 * tbd
 */
static inline int unc_isxdigit(
   int ch /**< [in] character to convert */
)
{
   return(isxdigit(unc_fix_ctype(ch)));
}


/**
 * tbd
 */
static inline int unc_isdigit(
   int ch /**< [in] character to convert */
)
{
   return(isdigit(unc_fix_ctype(ch)));
}


/**
 * tbd
 */
static inline int unc_isupper(
   int ch /**< [in] character to convert */
)
{
   int character = unc_fix_ctype(ch);
   return(isalpha(character) &&  unc_toupper(character) == ch);
}


#endif /* UNC_CTYPE_H_INCLUDED */
