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
   size_t       size, /**< [in]  */
   const size_t &idx, /**< [in]  */
   size_t       &len  /**< [in]  */
);


static void fix_len_idx(size_t size, const size_t &idx, size_t &len)
{
   if (idx >= size)
   {
      len = 0;
   }
   else
   {
      size_t left = size - idx;
#if 1
      len = min(len, left);
#else
      if (len > left)
      {
         len = left;
      }
#endif
   }
}


void unc_text::update_logtext()
{
   if (m_logok == false)
   {
      /* make a pessimistic guess at the size */
      m_logtext.clear();
      m_logtext.reserve(m_chars.size() * 3);
      for (int m_char : m_chars)
      {
         if      (m_char == LINEFEED      ) { m_char = 0x2424; }
         else if (m_char == CARRIAGERETURN) { m_char = 0x240d; }
         encode_utf8((uint32_t)m_char, m_logtext);
      }
      m_logtext.push_back(0);
      m_logok = true;
   }
}


int unc_text::compare(const unc_text &ref1, const unc_text &ref2, size_t len)
{
   size_t len1 = ref1.size();
   size_t len2 = ref2.size();

   size_t idx;
   for (idx = 0;
        (idx < len1) && (idx < len2) && (idx < len);
        idx++)
   {
      /*  exactly the same character ? */
      continue_if (ref1.m_chars[idx] == ref2.m_chars[idx]);

      int diff = unc_tolower(ref1.m_chars[idx]) - unc_tolower(ref2.m_chars[idx]);
      if (diff == 0)
      {
         /* if we're comparing the same character but in different case
          * we want to favor lower case before upper case (e.g. a before A)
          * so the order is the reverse of ASCII order (we negate). */
         return(-(ref1.m_chars[idx] - ref2.m_chars[idx]));
      }
      else
      {
         /* return the case-insensitive difference to sort alphabetically */
         return(diff);
      }
   }
   return (idx == len) ? 0 : ((int)(len1 - len2));
}


bool unc_text::equals(const unc_text &ref) const
{
   size_t len = size();
   retval_if((ref.size() != len), false);

   for (size_t idx = 0; idx < len; idx++)
   {
      retval_if((m_chars[idx] != ref.m_chars[idx]), false);
   }
   return(true);
}


const char *unc_text::c_str()
{
   update_logtext();
   return(reinterpret_cast<const char *>(&m_logtext[0]));
}


void unc_text::set(int ch)
{
   m_chars.clear();
   m_chars.push_back(ch);
   m_logok = false;
}


void unc_text::set(const unc_text &ref)
{
   m_chars = ref.m_chars;
   m_logok = false;
}


void unc_text::set(const unc_text &ref, size_t idx, size_t len)
{
   size_t ref_size = ref.size();

   fix_len_idx(ref_size, idx, len);
   m_logok = false;
   if (len == ref_size)
   {
      m_chars = ref.m_chars;
   }
   else
   {
      m_chars.resize(len);
      size_t di = 0;
      while (len-- > 0)
      {
         m_chars[di] = ref.m_chars[idx];
         di++;
         idx++;
      }
   }
}


void unc_text::set(const string &ascii_text)
{
   size_t len = ascii_text.size();

   m_chars.resize((size_t)len);
   for (size_t idx = 0; idx < len; idx++)
   {
      m_chars[idx] = ascii_text[idx];
   }
   m_logok = false;
}


void unc_text::set(const char *ascii_text)
{
   size_t len = strlen(ascii_text);

   m_chars.resize((size_t)len);
   for (size_t idx = 0; idx < len; idx++)
   {
      m_chars[idx] = *ascii_text++;
   }
   m_logok = false;
}


void unc_text::set(const int_list_t &data, size_t idx, size_t len)
{
   size_t data_size = data.size();

   fix_len_idx(data_size, idx, len);
   m_chars.resize(len);
   size_t di = 0;
   while (len-- > 0)
   {
      m_chars[di] = data[idx];
      di++;
      idx++;
   }
   m_logok = false;
}


void unc_text::resize(size_t new_size)
{
   if (size() != new_size)
   {
      m_chars.resize(new_size);
      m_logok = false;
   }
}


void unc_text::clear()
{
   m_chars.clear();
   m_logok = false;
}


void unc_text::insert(size_t idx, int ch)
{
   m_chars.insert(m_chars.begin() + static_cast<int>(idx), ch);
   m_logok = false;
}


void unc_text::insert(size_t idx, const unc_text &ref)
{
   m_chars.insert(m_chars.begin() + static_cast<int>(idx), ref.m_chars.begin(), ref.m_chars.end());
   m_logok = false;
}


void unc_text::append(int ch)
{
   m_chars.push_back(ch);
   m_logok = false;
}


void unc_text::append(const unc_text &ref)
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


void unc_text::append(const int_list_t &data, size_t idx, size_t len)
{
   unc_text tmp(data, idx, len);
   append(tmp);
}


bool unc_text::startswith(const char *text, size_t idx) const
{
   bool match = false;

   while ((idx < size()) && *text)
   {
      if (*text != m_chars[idx])
      {
         return(false);
      }
      idx++;
      text++;
      match = true;
   }
   return(match && (*text == 0));
}


bool unc_text::startswith(const unc_text &text, size_t idx) const
{
   bool   match = false;
   size_t si    = 0;

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


int unc_text::find(const char *text, size_t sidx) const
{
   size_t len = strlen(text); /**< the length of 'text' we are looking for */
   size_t si  = size();       /**< the length of the string we are looking in */

   if (si < len)              /* not enough place for 'text' */
   {
      return(-1);
   }
   size_t midx = size() - len;

   for (size_t idx = sidx; idx <= midx; idx++)
   {
      bool match = true;
      for (size_t ii = 0; ii < len; ii++)
      {
         if (m_chars[idx + ii] != text[ii])
         {
            match = false;
            break;
         }
      }
      if (match) // found at position 'idx'
      {
         return((int)idx);
      }
   }
   return(-1);  // 'text' not found
}


int unc_text::rfind(const char *text, size_t sidx) const
{
   size_t len  = strlen(text);
   size_t midx = size() - len;

   sidx = min(sidx, midx);
   for (size_t idx = sidx; idx != 0; idx--)
   {
      bool match = true;
      for (size_t ii = 0; ii < len; ii++)
      {
         if (m_chars[idx + ii] != text[ii])
         {
            match = false;
            break;
         }
      }
      if (match) { return((int)idx); }
   }
   return(-1);
}


void unc_text::erase(size_t idx, size_t len)
{
   if (len >= 1)
   {
      m_chars.erase(m_chars.begin() + static_cast<int>(idx), m_chars.begin() + static_cast<int>(idx) + static_cast<int>(len));
   }
}


int unc_text::replace(const char *oldtext, const unc_text &newtext)
{
   int    fidx         = find(oldtext);
   size_t olen         = strlen(oldtext);
   size_t rcnt         = 0;
   size_t newtext_size = newtext.size();

   while (fidx >= 0)
   {
      rcnt++;
      erase(static_cast<size_t>(fidx), olen);
      insert(static_cast<size_t>(fidx), newtext);
      fidx = find(oldtext, static_cast<size_t>(fidx) + newtext_size - olen + 1);
   }
   return((int)rcnt);
}
