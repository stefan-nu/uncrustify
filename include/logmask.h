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
 * Tests whether a sev bit is set in the mask
 *
 * @param sev  The severity to check
 * @return     true (is set) or false (not set)
 */
static inline bool logmask_test(const log_mask_t &mask, log_sev_t sev)
{
   return(mask.test(sev));
}


/**
 * Sets a set bit in the mask
 *
 * @param sev     The severity to check
 * @param value   true (set bit) or false (clear bit)
 */
static inline void logmask_set_sev(log_mask_t &mask, log_sev_t sev, bool value)
{
   mask.set(sev, value);
}


/**
 * Sets all bits to the same value
 *
 * @param value true (set bit) or false (clear bit)
 */
static inline void logmask_set_all(log_mask_t &mask, bool value)
{
   if (value) { mask.set  (); }
   else       { mask.reset(); }
}


/**
 * Convert a logmask into a string.
 * The string is a comma-delimited list of severities.
 * Example: 1,3,5-10
 *
 * @return     buf (pass through)
 */
char *logmask_to_str(
   const log_mask_t &mask, /**< [in] the mask to convert */
   char             *buf,  /**< [in] buffer to hold the string */
   size_t           size   /**< [in] size of the buffer */
);


/**
 * Parses a string into a log severity
 */
void logmask_from_string(
   const char *str,  /**< [in] string to parse */
   log_mask_t &mask  /**< [in] mask to populate */
);


#endif /* LOGMASK_H_INCLUDED */
