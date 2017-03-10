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
 * @param ch the 31-bit char value
 */
void write_char(
   uint32_t ch
);


/**
 * tbd
 */
void write_string(
   const unc_text &text
);


/**
 * Figure out the encoding and convert to an int sequence
 */
bool decode_unicode(
   const vector<uint8_t> &in_data,
   deque<int> &out_data,
   char_encoding_e &enc,
   bool &has_bom
);


/**
 * tbd
 */
void encode_utf8(
   uint32_t ch,
   vector<uint8_t> &res
);


#endif /* UNICODE_H_INCLUDED */
