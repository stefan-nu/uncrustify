/**
 * @file char_table.h
 * A simple table to help tokenize stuff.
 * Used to parse strings (paired char) and words.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef CHAR_TABLE_H_INCLUDED
#define CHAR_TABLE_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

#define SPACE           ' '
#define TABSTOP         '\t'
#define CARRIAGERETURN  '\r'
#define LINEFEED        '\n'
#define BACKSLASH       '\\'
#define SLASH           '/'

#define WIN_PATH_SEP   BACKSLASH
#define UNIX_PATH_SEP  SLASH


/**
 * bit0-7 = paired char
 * bit8   = OK for keyword 1st char
 * bit9   = OK for keyword 2+ char
 */
#define CHAR_TABLE_SIZE 128
struct CharTable
{
   static uint32_t chars[CHAR_TABLE_SIZE];

   enum
   {
      KEYWORD1 = 0x0100,
      KEYWORD2 = 0x0200,
   };


   static inline uint32_t Get(uint32_t idx)
   {
      if (idx < CHAR_TABLE_SIZE)
      {
         return(chars[idx]);
      }
      else
      {
         /* HACK: If the top bit is set, then we are likely dealing with UTF-8,
          * and since that is only allowed in identifiers, then assume that is
          * what this is. This only prevents corruption, it does not properly
          * handle UTF-8 because the byte length and screen size are assumed to be
          * the same. */
         return(KEYWORD1 | KEYWORD2);
      }
   }


   static inline bool IsKW1(uint32_t idx)
   {
      return((Get(idx) & KEYWORD1) != 0);
   }

   static inline bool IsKW2(uint32_t idx)
   {
      return((Get(idx) & KEYWORD2) != 0);
   }
};


#ifdef DEFINE_CHAR_TABLE
uint32_t CharTable::chars[] =
{
   0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,   /* [........] */
   0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,   /* [........] */
   0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,   /* [........] */
   0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,   /* [........] */
   0x000, 0x000, 0x022, 0x000, 0x300, 0x000, 0x000, 0x027,   /* [ !"#$%&'] */
   0x029, 0x028, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,   /* [()*+,-./] */
   0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,   /* [01234567] */
   0x200, 0x200, 0x000, 0x000, 0x03e, 0x000, 0x03c, 0x000,   /* [89:;<=>?] */
   0x200, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,   /* [@ABCDEFG] */
   0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,   /* [HIJKLMNO] */
   0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,   /* [PQRSTUVW] */
   0x300, 0x300, 0x300, 0x05d, 0x000, 0x05b, 0x000, 0x300,   /* [XYZ[\]^_] */
   0x060, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,   /* [`abcdefg] */
   0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,   /* [hijklmno] */
   0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300, 0x300,   /* [pqrstuvw] */
   0x300, 0x300, 0x300, 0x07d, 0x000, 0x07b, 0x000, 0x000,   /* [xyz{|}~.] */
};
#endif


#endif /* CHAR_TABLE_H_INCLUDED */
