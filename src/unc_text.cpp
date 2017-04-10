/**
 * @file unc_text.cpp

 * A simple class that handles the chunk text.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include <stdarg.h>
#include "chunk_list.h"
#include "unc_text.h"
#include "unc_ctype.h"
#include "unicode.h" /* encode_utf8() */


/**
 * tbd
 */
static void fix_len_idx(
   const uint32_t        size, /**< [in]  */
   const uint32_t& idx,  /**< [in]  */
   uint32_t&       len   /**< [in]  */
);


static void fix_len_idx(const uint32_t size, const uint32_t& idx, uint32_t& len)
{
   if (idx >= size)
   {
      len = 0;
   }
   else
   {
      uint32_t left = size - idx;
      len = min(len, left);
   }
}


void unc_text::update_logtext(void)
{
   if (m_logok == false)
   {
      /* make a pessimistic guess at the size */
      m_logtext.clear();
      m_logtext.reserve(m_chars.size() * 3);
      for (uint32_t m_char : m_chars)
      {
         if      (m_char == LINEFEED      ) { m_char = 0x2424; }
         else if (m_char == CARRIAGERETURN) { m_char = 0x240d; }
         encode_utf8((uint32_t)m_char, m_logtext);
      }
      m_logtext.push_back(0);
      m_logok = true;
   }
}


int32_t unc_text::compare(const unc_text& ref1, const unc_text& ref2, uint32_t len)
{
   uint32_t len1 = ref1.size();
   uint32_t len2 = ref2.size();

   uint32_t idx;
   for (idx = 0;
        (idx < len1) && (idx < len2) && (idx < len);
        idx++)
   {
      /*  exactly the same character ? */
      continue_if (ref1.m_chars[idx] == ref2.m_chars[idx]);

      int32_t diff = (int32_t)unc_tolower(ref1.m_chars[idx]) - (int32_t)unc_tolower(ref2.m_chars[idx]);
      if (diff == 0)
      {
         /* if we're comparing the same character but in different case
          * we want to favor lower case before upper case (e.g. a before A)
          * so the order is the reverse of ASCII order (we negate). */
         return(-((int32_t)ref1.m_chars[idx] - (int32_t)ref2.m_chars[idx]));
      }
      else
      {
         /* return the case-insensitive difference to sort alphabetically */
         return(diff);
      }
   }
   return (idx == len) ? 0 : ((int32_t)(len1 - len2));
}


bool unc_text::equals(const unc_text& ref) const
{
   uint32_t len = size();
   retval_if((ref.size() != len), false);

   for (uint32_t idx = 0; idx < len; idx++)
   {
      retval_if((m_chars[idx] != ref.m_chars[idx]), false);
   }
   return(true);
}


const char* unc_text::c_str(void)
{
   update_logtext();
   return(reinterpret_cast<const char*>(&m_logtext[0]));
}


const uint32_t* unc_text::c_unc(void)
{
   update_logtext();
   return(reinterpret_cast<const uint32_t*>(&m_logtext[0]));
}


void unc_text::set(uint32_t ch)
{
   m_chars.clear();
   m_chars.push_back(ch);
   m_logok = false;
}


void unc_text::set(const unc_text& ref)
{
   m_chars = ref.m_chars;
   m_logok = false;
}


void unc_text::set(const unc_text& ref, uint32_t idx, uint32_t len)
{
   uint32_t ref_size = ref.size();

   fix_len_idx(ref_size, idx, len);
   m_logok = false;
   if (len == ref_size)
   {
      m_chars = ref.m_chars;
   }
   else
   {
      m_chars.resize(len);
      uint32_t di = 0;
      while (len-- > 0)
      {
         m_chars[di] = ref.m_chars[idx];
         di++;
         idx++;
      }
   }
}


void unc_text::set(const string& ascii_text)
{
   uint32_t len = ascii_text.size();

   m_chars.resize((uint32_t)len);
   for (uint32_t idx = 0; idx < len; idx++)
   {
      m_chars[idx] = ascii_text[idx];
   }
   m_logok = false;
}


void unc_text::set(const char* ascii_text)
{
   uint32_t len = strlen(ascii_text);

   m_chars.resize((uint32_t)len);
   for (uint32_t idx = 0; idx < len; idx++)
   {
      m_chars[idx] = char2uint32(*ascii_text++);
   }
   m_logok = false;
}


void unc_text::set(const uint_list_t& data, uint32_t idx, uint32_t len)
{
   uint32_t data_size = data.size();

   fix_len_idx(data_size, idx, len);
   m_chars.resize(len);
   uint32_t di = 0;
   while (len-- > 0)
   {
      m_chars[di] = data[idx];
      di++;
      idx++;
   }
   m_logok = false;
}


void unc_text::resize(uint32_t new_size)
{
   if (size() != new_size)
   {
      m_chars.resize(new_size);
      m_logok = false;
   }
}


void unc_text::clear(void)
{
   m_chars.clear();
   m_logok = false;
}


void unc_text::insert(uint32_t idx, int32_t ch)
{
   m_chars.insert(m_chars.begin() + static_cast<int32_t>(idx), ch);
   m_logok = false;
}


void unc_text::insert(uint32_t idx, const unc_text& ref)
{
   m_chars.insert(m_chars.begin() + static_cast<int32_t>(idx), ref.m_chars.begin(), ref.m_chars.end());
   m_logok = false;
}


void unc_text::append(uint32_t ch)
{
   m_chars.push_back(ch);
   m_logok = false;
}


void unc_text::append(const unc_text& ref)
{
   m_chars.insert(m_chars.end(), ref.m_chars.begin(), ref.m_chars.end());
   m_logok = false;
}


void unc_text::append(const string &ascii_text)
{
   unc_text tmp(ascii_text);
   append(tmp);
}


#define MAX_MSG_SIZE 256
void unc_text::append_cond(const bool condition, const char* const msg, ...)
{
   if(condition == true)
   {
      va_list arg;         /* stores variable list of arguments */
      va_start(arg, msg);  /* convert msg to variable list */
      append(msg, arg);    /* forward the arguments to unconditional append */
      va_end(arg);
   }
}


void unc_text::append(const char* const msg, ...)
{
   va_list arg;         /* stores variable list of arguments */
   va_start(arg, msg);  /* convert msg to variable list */

   /* create fixed string from variable arguments */
   char orig_msg[MAX_MSG_SIZE];
   vsnprintf(orig_msg, MAX_MSG_SIZE, msg, arg);
   va_end(arg);

   /* convert string in unc_msg format and append it */
   unc_text unc_msg(orig_msg);
   append(unc_msg);
}


void unc_text::append(const uint_list_t& data, uint32_t idx, uint32_t len)
{
   unc_text tmp(data, idx, len);
   append(tmp);
}


bool unc_text::startswith(const char* text, uint32_t idx) const
{
   bool match = false;

   while ((idx < size()) && *text)
   {
      if (char2uint32(*text) != m_chars[idx])
      {
         return(false);
      }
      idx++;
      text++;
      match = true;
   }
   return(match && (*text == 0));
}


bool unc_text::startswith(const unc_text& text, uint32_t idx) const
{
   bool     match = false;
   uint32_t si    = 0;

   while ((idx < size()) && (si < text.size()))
   {
      if (text.m_chars[si] != m_chars[idx])
      {
         return(false);
      }
      idx++;
      si++;
      match = true;
   }
   return(match && (si == text.size()));
}


int32_t unc_text::find(const char* text, uint32_t sidx) const
{
   uint32_t len = strlen(text); /**< the length of 'text' we are looking for */
   uint32_t si  = size();       /**< the length of the string we are looking in */

   /* not enough place for 'text' */
   retval_if((si < len), -1);

   uint32_t midx = size() - len;

   for (uint32_t idx = sidx; idx <= midx; idx++)
   {
      bool match = true;
      for (uint32_t ii = 0; ii < len; ii++)
      {
         if (m_chars[idx + ii] != char2uint32(text[ii]))
         {
            match = false;
            break;
         }
      }
      if (match) /* 'text' found at position 'idx' */
      {
         return((int32_t)idx);
      }
   }
   return(-1);  /* 'text' not found */
}


uint32_t char2uint32(char in)
{
   return((uint32_t)(int32_t)in);
}


int32_t unc_text::rfind(const char* text, uint32_t sidx) const
{
   uint32_t len  = strlen(text);
   uint32_t midx = size() - len;

   sidx = min(sidx, midx);
   for (uint32_t idx = sidx; idx != 0; idx--)
   {
      bool match = true;
      for (uint32_t ii = 0; ii < len; ii++)
      {
         if (m_chars[idx + ii] != char2uint32(text[ii]))
         {
            match = false;
            break;
         }
      }
      if (match) { return((int32_t)idx); }
   }
   return(-1);
}


void unc_text::erase(uint32_t idx, uint32_t len)
{
   if (len >= 1)
   {
      m_chars.erase(m_chars.begin() + idx, m_chars.begin() + idx + len);
   }
}


int32_t unc_text::replace(const char* oldtext, const unc_text& newtext)
{
   int32_t  fidx         = find(oldtext);
   uint32_t olen         = strlen(oldtext);
   uint32_t rcnt         = 0;
   uint32_t newtext_size = newtext.size();

   while (fidx >= 0)
   {
      rcnt++;
      erase(static_cast<uint32_t>(fidx), olen);
      insert(static_cast<uint32_t>(fidx), newtext);
      fidx = find(oldtext, static_cast<uint32_t>(fidx) + newtext_size - olen + 1);
   }
   return((int32_t)rcnt);
}
