/**
 * @file unc_text.h
 * A simple class that handles the chunk text.
 * At the start of processing, the entire file is decoded into a vector of ints.
 * This class is intended to hold sections of that large vector.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef UNC_TEXT_H_INCLUDED
#define UNC_TEXT_H_INCLUDED

#include "base_types.h"
#include <vector>
#include <deque>
#include <string>
using namespace std;

/**
 *  abbreviations used:
 * - unc_text - uncrustify text
 */

class unc_text
{
public:
   typedef deque<int32_t> int_list_t;   /* double encoded list of int32_t values */

public:
   unc_text(void)
      : m_logok(false)
   {
   }


   ~unc_text(void)
   {
   }


   unc_text(const unc_text &ref)
   {
      set(ref);
   }


   unc_text(const unc_text &ref, uint32_t idx, uint32_t len = 0)
   {
      set(ref, idx, len);
   }


   unc_text(const char *ascii_text)
   {
      set(ascii_text);
   }


   explicit unc_text(const string &ascii_text)
   {
      set(ascii_text);
   }


   unc_text(const int_list_t &data, uint32_t idx = 0, uint32_t len = 0)
   {
      set(data, idx, len);
   }


   /**
    * tbd
    */
   void resize(
      uint32_t new_size
   );


   /**
    * tbd
    */
   void clear(void);


   /**
    * grab the number of characters
    */
   uint32_t size(void) const
   {
      return(m_chars.size());
   }


   /**
    * tbd
    */
   void set(
      int32_t ch
   );


   void set(
      const unc_text &ref
   );


   void set(
      const unc_text &ref,
      uint32_t idx,
      uint32_t len = 0
   );


   void set(
      const string &ascii_text
   );


   void set(
      const char* ascii_text
   );


   void set(
      const int_list_t &data,
      uint32_t idx = 0,
      uint32_t len = 0
   );


   /**
    * tbd
    */
   unc_text &operator =(int32_t ch)
   {
      set(ch);
      return(*this);
   }


   unc_text &operator =(const unc_text &ref)
   {
      set(ref);
      return(*this);
   }


   unc_text &operator =(const string &ascii_text)
   {
      set(ascii_text);
      return(*this);
   }


   unc_text &operator =(const char* ascii_text)
   {
      set(ascii_text);
      return(*this);
   }


   /**
    * tbd
    */
   void insert(
      uint32_t idx,
      int32_t ch
   );


   void insert(
      uint32_t idx,
      const unc_text &ref
   );


   void erase(
      uint32_t idx,
      uint32_t len = 1
   );


   /*
    * Add a single character to an unc_text
    */
   void append(
      int32_t ch
   );


   /*
    * Add a unc_text character to an unc_text
    */
   void append(const unc_text &ref);


   /*
    * Add a string to an unc_text
    */
   void append(
      const string &ascii_text
   );


   /*
    * Add a variable length string to an unc_text
    *
    * The variable length string format is similar as for printf
    *
    * \note the overall length of the string must not exceed 256 characters
    */
   void append(
      const char* const msg, ... /**< [in] a variable length string */
   );


   /*
    * Conditionally add a variable length string to an unc_text
    *
    * The variable length string format is similar as for printf
    *
    * \note the overall length of the string must not exceed 256 characters
    */
   void append_cond(
      const bool condition,      /**< [in] condition when to append string */
      const char* const msg, ... /**< [in] a variable length string */
   );


   void append(
      const int_list_t &data,
      uint32_t idx = 0,
      uint32_t len = 0
   );


   /**
    * tbd
    */
   unc_text &operator +=(int32_t ch)
   {
      append(ch);
      return(*this);
   }


   unc_text &operator +=(const unc_text &ref)
   {
      append(ref);
      return(*this);
   }


   unc_text &operator +=(const string &ascii_text)
   {
      append(ascii_text);
      return(*this);
   }

   unc_text &operator +=(const char *ascii_text)
   {
      append(ascii_text);
      return(*this);
   }


   /**
    *  get the UTF-8 string for logging
    */
   const char* c_str(void);

   /**
    * compares the content of two unc_text instances
    *
    * \todo explain the return values, see compare_chunks
    * @retval == 0 - both elements are equal
    * @retval  > 0 - tbd
    * @retval  < 0 - tbd
    */
   static int32_t compare(
      const  unc_text &ref1,  /**< [in] first  instance to compare */
      const  unc_text &ref2,  /**< [in] second instance to compare */
      uint32_t len = 0          /**< [in] number of character to compare */
   );


   /**
    * tbd
    */
   bool equals(
      const unc_text &ref
   ) const;

   /**
    *  grab the data as a series of ints for outputting to a file
    */
   int_list_t &get(void)
   {
      m_logok = false;
      return(m_chars);
   }


   const int_list_t &get(void) const
   {
      return(m_chars);
   }


   int32_t operator[](uint32_t idx) const
   {
      return((idx < m_chars.size()) ? m_chars[idx] : 0);
   }


   /**
    * throws an exception if out of bounds
    */
   int32_t &at(uint32_t idx)
   {
      return(m_chars.at(idx));
   }


   const int32_t &at(uint32_t idx) const
   {
      return(m_chars.at(idx));
   }


   /**
    * tbd
    */
   const int32_t &back(void) const
   {
      return(m_chars.back());
   }


   /*
    * returns the last element of the character list
    */
   int32_t &back(void)
   {
      /* \todo returning a temporary via a reference
       * this has to be checked and probably changed */
      return(m_chars.back());
   }


   /**
    * tbd
    */
   void push_back(int32_t ch)
   {
      append(ch);
   }


   /**
    * tbd
    */
   void pop_back()
   {
      if (size() > 0)
      {
         m_chars.pop_back();
         m_logok = false;
      }
   }


   /**
    * tbd
    */
   void pop_front()
   {
      if (size() > 0)
      {
         m_chars.pop_front();
         m_logok = false;
      }
   }


   /**
    * tbd
    */
   bool startswith(
      const unc_text &text,
      uint32_t idx = 0
   ) const;


   bool startswith(
      const char *text,
      uint32_t idx = 0
   ) const;


   /**
    * look for 'text', beginning with position 'idx'
    *
    * @retval == -1 if not found
    * @retval >=  0 if found gives position
    */
   int32_t find(
      const char *text,  /**< [in] text to search for */
      uint32_t idx = 0     /**< [in] position to start search */
   ) const;


   /**
    * tbd
    */
   int32_t rfind(
      const char *text,
      uint32_t idx = 0
   ) const;


   /**
    * tbd
    */
   int32_t replace(
      const char *oldtext,
      const unc_text &newtext
   );

protected:
   void update_logtext();


   int_list_t    m_chars;   /**< contains the non-encoded 31-bit chars */
   vector<uint8_t> m_logtext; /**< logging text, utf8 encoded - updated in c_str() */
   bool          m_logok;
};

#endif /* UNC_TEXT_H_INCLUDED */
