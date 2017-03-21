/**
 * @file unicode.h
 * prototypes for unicode.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef UNICODE_H_INCLUDED
#define UNICODE_H_INCLUDED

#include "uncrustify_types.h"
#include "unc_text.h"


/**
 * tbd
 */
void write_bom(void);


/**
 * tbd
 */
void write_char(
   uint32_t ch /**< [in] the 31-bit char value */
);


/**
 * tbd
 */
void write_string(
   const unc_text &text /**< [in]  */
);


/**
 * Figure out the encoding and convert to an int sequence
 */
bool decode_unicode(
   const vector<uint8_t> &in,     /**< [in]  */
   deque<int32_t>        &out,    /**< [in]  */
   char_encoding_e       &enc,    /**< [in]  */
   bool                  &has_bom /**< [in]  */
);


/**
 * tbd
 */
void encode_utf8(
   uint32_t        ch,  /**< [in]  */
   vector<uint8_t> &res /**< [in]  */
);


#endif /* UNICODE_H_INCLUDED */
