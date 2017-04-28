/**
 * @file unicode.cpp
 * Detects, read and writes characters in the proper format.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "unicode.h"
#include "uncrustify.h"
#include "uncrustify_types.h"
#include "unc_ctype.h"
#include <cstdlib>
#include "base_types.h"
#include "chunk_list.h"


/**
 * See if all characters are ASCII (0-127)
 */
static bool is_ascii(
   const vector<uint8_t>& data,          /**< [in]  */
   uint32_t&              non_ascii_cnt, /**< [in]  */
   uint32_t&              zero_cnt       /**< [in]  */
);


/**
 * Convert the array of bytes into an array of ints
 */
static bool decode_bytes(
   const vector<uint8_t>& in, /**< [in]  */
   deque<uint32_t>&       out /**< [in]  */
);


/**
 * Decode UTF-8 sequences from in_data and put the chars in out_data.
 * If there are any decoding errors, then return false.
 */
static bool decode_utf8(
   const vector<uint8_t>& in, /**< [in]  */
   deque<uint32_t>&       out /**< [in]  */
);


/**
 * Extract 2 bytes from the stream and increment idx by 2
 */
static uint32_t get_word(
   const vector<uint8_t>& in,  /**< [in] byte vector with input data */
   uint32_t&              idx, /**< [in] index points to working position in vector */
   bool                   be   /**< [in]  */
);


/**
 * Decode a UTF-16 sequence.
 * Sets enc based on the BOM.
 * Must have the BOM as the first two bytes.
 */
static bool decode_utf16(
   const vector<uint8_t>& in,  /**< [in]  */
   deque<uint32_t>&       out, /**< [in]  */
   char_encoding_e&       enc  /**< [in]  */
);


/**
 * Looks for the BOM of UTF-16 BE/LE and UTF-8.
 * If found, set enc and return true.
 * Sets enc to char_encoding_e::ASCII and returns false if not found.
 */
static bool decode_bom(
   const vector<uint8_t>& in, /**< [in]  */
   char_encoding_e&       enc /**< [in]  */
);


/**
 * Write for ASCII and BYTE encoding
 */
static void write_byte(
   uint32_t ch /**< [in]  */
);


/**
 * Writes a single character to a file using UTF-8 encoding
 */
static void write_utf8(
   uint32_t ch /**< [in]  */
);


/**  */
static void write_utf16(
   uint32_t ch, /**< [in]  */
   bool     be  /**< [in]  */
);


static bool is_ascii(const vector<uint8_t>& data, uint32_t& non_ascii_cnt, uint32_t& zero_cnt)
{
   non_ascii_cnt = 0;
   zero_cnt      = 0;
   for (unsigned char value : data)
   {
      if (value & 0x80) { non_ascii_cnt++; }
      if (value == 0)   { zero_cnt++;      }
   }
   return((non_ascii_cnt + zero_cnt) == 0);
}


static bool decode_bytes(const vector<uint8_t>& in, deque<uint32_t>& out)
{
   uint32_t out_size = in.size();
   out.resize(out_size);
   for (uint32_t idx = 0; idx < out_size; idx++)
   {
      out[idx] = in[idx];
   }
   return(true);
}


void encode_utf8(uint32_t ch, vector<uint8_t>& res)
{
   if (ch < 0x80)
   {
      /* 0xxxxxxx */
      res.push_back(ch);
   }
   else if (ch < 0x0800)
   {
      /* 110xxxxx 10xxxxxx */
      res.push_back(0xC0 | ((ch >> 6)       ));
      res.push_back(0x80 | ((ch >> 0) & 0x3f));
   }
   else if (ch < 0x10000)
   {
      /* 1110xxxx 10xxxxxx 10xxxxxx */
      res.push_back(0xE0 | ((ch >> 12)      ));
      res.push_back(0x80 | ((ch >> 6) & 0x3f));
      res.push_back(0x80 | ((ch >> 0) & 0x3f));
   }
   else if (ch < 0x200000)
   {
      /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
      res.push_back(0xF0 | ((ch >> 18)       )); /* \todo DRY */
      res.push_back(0x80 | ((ch >> 12) & 0x3f));
      res.push_back(0x80 | ((ch >>  6) & 0x3f));
      res.push_back(0x80 | ((ch >>  0) & 0x3f));
   }
   else if (ch < 0x4000000)
   {
      /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      res.push_back(0xF8 | ((ch >> 24)       ));
      res.push_back(0x80 | ((ch >> 18) & 0x3f));
      res.push_back(0x80 | ((ch >> 12) & 0x3f));
      res.push_back(0x80 | ((ch >>  6) & 0x3f));
      res.push_back(0x80 | ((ch >>  0) & 0x3f));
   }
   else /* (ch <= 0x7fffffff) */
   {
      /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
      res.push_back(0xFC | ((ch >> 30)       ));
      res.push_back(0x80 | ((ch >> 24) & 0x3f));
      res.push_back(0x80 | ((ch >> 18) & 0x3f));
      res.push_back(0x80 | ((ch >> 12) & 0x3f));
      res.push_back(0x80 | ((ch >>  6) & 0x3f));
      res.push_back(0x80 | ((ch >>  0) & 0x3f));
   }
}


static bool decode_utf8(const vector<uint8_t>& in, deque<uint32_t>& out)
{
   /* check for UTF-8 BOM silliness and skip */
   uint32_t idx = 0;
   if (in.size() >= 3)
   {
      if ((in[0] == 0xef) &&
          (in[1] == 0xbb) &&
          (in[2] == 0xbf) )
      {
         idx = 3;/* skip it */
      }
   }

   out.clear();
   int32_t cnt;
   while (idx < in.size())
   {
      uint32_t ch = in[idx++];

      if      ( ch < 0x80         ) { out.push_back(ch); continue; }/* 1-byte sequence */
      else if ((ch & 0xE0) == 0xC0) { ch &= 0x1F; cnt = 1; } /* 2-byte sequence */
      else if ((ch & 0xF0) == 0xE0) { ch &= 0x0F; cnt = 2; } /* 3-byte sequence */
      else if ((ch & 0xF8) == 0xF0) { ch &= 0x07; cnt = 3; } /* 4-byte sequence */
      else if ((ch & 0xFC) == 0xF8) { ch &= 0x03; cnt = 4; } /* 5-byte sequence */
      else if ((ch & 0xFE) == 0xFC) { ch &= 0x01; cnt = 5; } /* 6-byte sequence */
      else { return(false); } /* invalid UTF-8 sequence */

      while ((cnt-- > 0) && (idx < in.size()))
      {
         uint32_t tmp = in[idx++];
         retval_if((tmp & 0xC0) != 0x80, false); /* invalid UTF-8 sequence */
         ch = (ch << 6) | (tmp & 0x3f);
      }
      retval_if(cnt >= 0, false); /* short UTF-8 sequence */
      out.push_back(ch);
   }
   return(true);
}


static uint32_t get_word(const vector<uint8_t>& in, uint32_t& idx, bool be)
{
   uint32_t ch;
   if ((idx + 2) > in.size())
   {
      /* \todo invalid index we should indicate an error */
      return 0;
   }
   else if (be) { ch = (in[idx] << 8) | (in[idx + 1] << 0); }
   else         { ch = (in[idx] << 0) | (in[idx + 1] << 8); }
   idx += 2;
   return(ch);
}


static bool decode_utf16(const vector<uint8_t>& in, deque<uint32_t>& out, char_encoding_e& enc)
{
   out.clear();

   retval_if(in.size() & 1, false); /* can't have and odd length */
   retval_if(in.size() < 2, false); /* we require the BOM or at least 1 char */

   uint32_t idx = 2;
   if ((in[0] == 0xfe) &&
       (in[1] == 0xff) )
   {
      enc = char_encoding_e::UTF16_BE;
   }
   else if ((in[0] == 0xff) &&
            (in[1] == 0xfe) )
   {
      enc = char_encoding_e::UTF16_LE;
   }
   else
   {
      /* If we have a few words, we can take a guess, assuming the first few
       * chars are ASCII */
      enc = char_encoding_e::ASCII;
      idx = 0;
      if (in.size() >= 6)
      {
         if ((in[0] == 0) &&
             (in[2] == 0) &&
             (in[4] == 0) )
         {
            enc = char_encoding_e::UTF16_BE;
         }
         else if ((in[1] == 0) &&
                  (in[3] == 0) &&
                  (in[5] == 0) )
         {
            enc = char_encoding_e::UTF16_LE;
         }
      }
      retval_if(enc == char_encoding_e::ASCII, false);
   }

   bool be = (enc == char_encoding_e::UTF16_BE);

   while (idx < in.size())
   {
      uint32_t ch = get_word(in, idx, be);
      if ((ch & 0xfc00) == 0xd800)
      {
         ch  &= 0x3ff;
         ch <<= 10;
         uint32_t tmp = get_word(in, idx, be);
         retval_if((tmp & 0xfc00) != 0xdc00, false);
         ch |= (tmp & 0x3ff);
         ch += 0x10000;
         out.push_back(ch);
      }
      else if (((ch < 0xD800)) || (ch >= 0xE000))
      {
         out.push_back(ch);
      }
      else
      {
         /* invalid character */
         return(false);
      }
   }
   return(true);
}


static bool decode_bom(const vector<uint8_t>& in, char_encoding_e& enc)
{
   enc = char_encoding_e::ASCII;
   if (in.size() >= 2)
   {
      if ((in[0] == 0xfe) &&
          (in[1] == 0xff))
      {
         enc = char_encoding_e::UTF16_BE;
         return(true);
      }
      else if ((in[0] == 0xff) &&
               (in[1] == 0xfe) )
      {
         enc = char_encoding_e::UTF16_LE;
         return(true);
      }
      else if ((in.size() >= 3) &&
               (in[0] == 0xef ) &&
               (in[1] == 0xbb ) &&
               (in[2] == 0xbf ) )
      {
         enc = char_encoding_e::UTF8;
         return(true);
      }
   }
   return(false);
}


bool decode_unicode(const vector<uint8_t>& in, deque<uint32_t>& out, char_encoding_e& enc, bool& has_bom)
{
   /* check for a BOM */
   if (decode_bom(in, enc))
   {
      has_bom = true;
      return(enc == char_encoding_e::UTF8) ?
             decode_utf8 (in, out     ) :
             decode_utf16(in, out, enc);
   }
   has_bom = false;

   /* Check for simple ASCII */
   uint32_t non_ascii_cnt;
   uint32_t zero_cnt;
   if (is_ascii(in, non_ascii_cnt, zero_cnt))
   {
      enc = char_encoding_e::ASCII;
      return(decode_bytes(in, out));
   }

   /* There are a lot of 0's in UTF-16 (~50%) */
   if ((zero_cnt >  (in.size() / 4u)) &&
       (zero_cnt <= (in.size() / 2u)) )
   {
      /* likely is UTF-16 */
      retval_if(decode_utf16(in, out, enc), true);
   }

   if (decode_utf8(in, out))
   {
      enc = char_encoding_e::UTF8;
      return(true);
   }

   /* it is an unrecognized byte sequence */
   enc = char_encoding_e::BYTE;
   return(decode_bytes(in, out));
}


static void write_byte(uint32_t ch)
{
   if ((ch & 0xff) == ch)
   {
      if (cpd.fout) { fputc(ch, cpd.fout);                         }
      if (cpd.bout) { cpd.bout->push_back(static_cast<uint8_t>(ch)); }
   }
   else
   {
      /* illegal code - do not store */
   }
}


static void write_utf8(uint32_t ch)
{
   vector<uint8_t> vv;
   vv.reserve(6);

   encode_utf8(ch, vv);
   for (unsigned char char_val : vv)
   {
      write_byte(char_val);
   }
}


static void write_utf16(uint32_t ch, bool be)
{
   /* U+0000 to U+D7FF and U+E000 to U+FFFF */
   if ((                  (ch < 0x0D800)) ||
       ((ch >= 0xE000) && (ch < 0x10000)) )
   {
      if (be) { write_byte(ch >> 8);   write_byte(ch & 0xff); }
      else    { write_byte(ch & 0xff); write_byte(ch >> 8);   }
   }
   else if ((ch >= 0x10000) && (ch < 0x110000))
   {
      uint32_t v1 = ch - 0x10000;
      uint32_t w1 = 0xD800 + (v1 >> 10);
      uint32_t w2 = 0xDC00 + (v1 & 0x3ff);
      if (be)
      {
         write_byte(w1 >> 8); write_byte(w1 & 0xff);
         write_byte(w2 >> 8); write_byte(w2 & 0xff);
      }
      else
      {
         write_byte(w1 & 0xff); write_byte(w1 >> 8);
         write_byte(w2 & 0xff); write_byte(w2 >> 8);
      }
   }
   else
   {
      /* illegal code - do not store */
   }
}


void write_bom(void)
{
   switch (cpd.enc)
   {
      case char_encoding_e::UTF8:
         write_byte(0xef);
         write_byte(0xbb);
         write_byte(0xbf);
      break;

      case char_encoding_e::UTF16_LE:
         write_utf16(0xfeff, false);
      break;

      case char_encoding_e::UTF16_BE:
         write_utf16(0xfeff, true);
      break;

      default:
         /* do nothing */
      break;
   }
}


void write_char(uint32_t ch)
{
   switch (cpd.enc)
   {
      case char_encoding_e::BYTE:     write_byte (ch & 0xff); break;
      case char_encoding_e::UTF8:     write_utf8 (ch       ); break;
      case char_encoding_e::UTF16_LE: write_utf16(ch, false); break;
      case char_encoding_e::UTF16_BE: write_utf16(ch, true ); break;
      case char_encoding_e::ASCII:    /* fallthrough */
      default:                        write_byte (ch       ); break;
   }
}


void write_string(const unc_text& text)
{
   for (uint32_t idx = 0; idx < text.size(); idx++)
   {
      write_char((uint32_t)text[idx]);
   }
}
