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

/* \todo better avoid inline and move implementation to cpp file */

/**
 * Truncate symbol to unsigned 8 Bit range 0-255
 */
static inline uint32_t unc_fix_ctype(
   uint32_t ch /**< [in] symbol to convert to uint8_t */
)
{
   return(ch & 0xff);
}


/**
 * check if a character is a space
 */
static inline bool unc_isspace(
   uint32_t ch /**< [in] character to check */
)
{
   return(isspace(unc_fix_ctype(ch)) != 0);
}


/**
 * check if a character is a printing character
 */
static inline bool unc_isprint(
   uint32_t ch /**< [in] character to check */
)
{
   return(isprint(unc_fix_ctype(ch)) != 0);
}


/**
 * check if a character is an alphabetic character (a letter).
 */
static inline bool unc_isalpha(
   uint32_t ch /**< [in] character to check */
)
{
   return(isalpha(unc_fix_ctype(ch)) != 0);
}


/**
 * check if a character is an alphanumeric character.
 */
static inline uint32_t unc_isalnum(
   uint32_t ch /**< [in] character to check */
)
{
   return((uint32_t)isalnum(unc_fix_ctype(ch)));
}


/**
 * convert a character to upper case
 */
static inline uint32_t unc_toupper(
   uint32_t ch /**< [in] character to convert */
)
{
   return((uint32_t)toupper(unc_fix_ctype(ch)));
}


/**
 * convert a character to lower case
 */
static inline uint32_t unc_tolower(
   uint32_t ch /**< [in] character to convert */
)
{
   return((uint32_t)tolower(unc_fix_ctype(ch)));
}


/**
 * check if a character is a hexadecimal digit
 */
static inline uint32_t unc_isxdigit(
   uint32_t ch /**< [in] character to convert */
)
{
   return((uint32_t)isxdigit(unc_fix_ctype(ch)));
}


/**
 * check if a character is a decimal digit
 */
static inline bool unc_isdigit(
   uint32_t ch /**< [in] character to convert */
)
{
   return(isdigit(unc_fix_ctype(ch)) != 0);
}


/**
 * check if a character is upper case
 */
static inline bool unc_isupper(
   uint32_t ch /**< [in] character to convert */
)
{
   uint32_t character = unc_fix_ctype(ch);
   return( (isalpha(character) != 0) &&
            unc_toupper(character) == ch);
}


#endif /* UNC_CTYPE_H_INCLUDED */
