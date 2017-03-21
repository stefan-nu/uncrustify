/**
 * @file logmask.h
 *
 * Functions to manipulate a log severity mask.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef LOGMASK_H_INCLUDED
#define LOGMASK_H_INCLUDED

#include "base_types.h"
#include <cstring>     /* memset() */
#include <bitset>
#include "log_levels.h"


/** A simple array of 256 bits */
typedef std::bitset<256> log_mask_t;


/**
 * Tests whether a bit is set in a log mask
 *
 * @return true (is set) or false (not set)
 */
static inline bool logmask_test(
   const log_mask_t &mask, /**< [in] log mask to evaluate */
   log_sev_t        sev    /**< [in] severity bit to check */
)
{
   return(mask.test(sev));
}


/**
 * Sets or clears a set of bits in a bit mask
 */
static inline void logmask_set_sev(
   log_mask_t &mask, /**< [in,out] log mask to modify */
   log_sev_t  sev,   /**< [in] The severity bits to set or clear */
   bool       value  /**< [in] true (set bit) or false (clear bit) */
)
{
   mask.set(sev, value);
}


/**
 * Sets or clears all bits in a bit mask
 */
static inline void logmask_set_all(
   log_mask_t &mask,  /**< [in] log mask to operate on */
   bool       value   /**< [in] true (set bits) or false (clear bits) */
)
{
   if (value) { mask.set  (); }
   else       { mask.reset(); }
}


/**
 * Convert a logmask into a string.
 * The string is a comma-delimited list of severities.
 * Example: 1,3,5-10
 *
 * @return buf (pass through)
 */
char *logmask_to_str(
   const log_mask_t &mask, /**< [in] the mask to convert */
   char*            buf,   /**< [in] buffer to hold the string */
   uint32_t         size   /**< [in] size of the buffer */
);


/**
 * Parses a string to create a log severity bit mask
 *
 * the following kind of input strings are supported
 * "A"    -> sets all log levels
 * "a"    -> sets all log levels
 * "1"    -> sets log level 1
 * "0 1"  -> sets log levels 0,1
 * "3-5"  -> sets log level 3,4,5
 * "2,8"  -> sets log level 2 and 8
 * "4,6-8"-> sets log level 4,6,7,9
 * ""     -> sets no log level
 * " "    -> sets no log level
 */
void logmask_from_string(
   const char* str,  /**< [in]  string to parse */
   log_mask_t& mask  /**< [out] bit mask to populate with log levels */
);


#endif /* LOGMASK_H_INCLUDED */
