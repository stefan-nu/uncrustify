/**
 * @file tokenize.cpp
 * This file breaks up the text stream into tokens or chunks.
 *
 * Each routine needs to set pc.len and pc.type.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "tokenize.h"
#include "uncrustify_types.h"
#include "char_table.h"
#include "punctuators.h"
#include "chunk_list.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "unc_ctype.h"
#include "uncrustify.h"
#include "keywords.h"
#include "output.h"
#include "tabulator.h"


struct tok_info
{
   tok_info(void)
      : last_ch(0)
      , idx(0)
      , row(1)
      , col(1)
   {
   }

   uint32_t last_ch;  /**< [in]  */
   uint32_t idx;      /**< [in]  */
   uint32_t row;      /**< [in]  */
   uint32_t col;      /**< [in]  */
};

struct tok_ctx
{
   explicit tok_ctx(const deque<uint32_t> &d)
      : data(d)
   {
   }

#if 0
   // \todo separate declarations and implementation
   /* save before trying to parse something that may fail */
   void save();
   void save(tok_info &info) const;

   /* restore previous saved state */
   void restore();
   void restore(const tok_info &info);

   bool more() const;

   uint32_t peek() const;
   uint32_t peek(uint32_t idx) const;

   int32_t get();

   bool expect(uint32_t ch);
#endif


   void save(void)
   {
      save(s);
   }


   void save(tok_info &info) const
   {
      info = c;
   }


   void restore(void)
   {
      restore(s);
   }


   void restore(const tok_info &info)
   {
      c = info;
   }


   bool more(void) const
   {
      return(c.idx < data.size());
   }


   uint32_t peek(void) const
   {
      return((more() == true) ? data[c.idx] : 0u);
   }


   uint32_t peek(uint32_t idx) const
   {
      idx += c.idx;
      return((idx < data.size()) ? data[idx] : 0u);
   }


   uint32_t get()
   {
      if (more())
      {
         uint32_t ch = data[c.idx++];
         switch (ch)
         {
            case TABSTOP:
               c.col = calc_next_tab_column(c.col, get_uval(UO_input_tab_size));
               break;

            case LINEFEED:
               if (c.last_ch != CARRIAGERETURN)
               {
                  c.row++;
                  c.col = 1;
               }
               break;

            case CARRIAGERETURN: c.row++; c.col = 1; break;
            default:             c.col++;            break;
            }
            c.last_ch = ch;
            return(ch);
      }
      return(0);
   }


   bool expect(uint32_t ch)
   {
      if (peek() == ch)
      {
         get();
         return(true);
      }
      return(false);
   }

   const deque<uint32_t> &data;
   tok_info         c; /* current */
   tok_info         s; /* saved */
};


/**
 * Count the number of characters in a word.
 * The first character is already valid for a keyword
 *
 * @return     Whether a word was parsed (always true)
 */
static bool parse_word(
   tok_ctx &ctx,      /**< [in]  */
   chunk_t &pc,       /**< [in] structure to update, str is an input. */
   bool    skipcheck  /**< [in]  */
);


/**
 * Count the number of characters in a quoted string.
 * The next bit of text starts with a quote char " or ' or <.
 * Count the number of characters until the matching character.
 *
 * @return Whether a string was parsed
 */
static bool parse_string(
   tok_ctx& ctx,         /**< [in]  */
   chunk_t& pc,          /**< [in] The structure to update, str is an input. */
   uint32_t quote_idx,   /**< [in]  */
   bool     allow_escape /**< [in]  */
);


/**
 * Literal string, ends with single "
 * Two "" don't end the string.
 *
 * @return Whether a string was parsed
 */
static bool parse_cs_string(
   tok_ctx& ctx,
   chunk_t& pc  /**< [in] structure to update, str is an input. */
);


/**
 * Interpolated strings start with $" end with a single "
 * Double quotes are escaped by doubling.
 * Need to track embedded { } pairs and ignore anything between.
 *
 * @return Whether a string was parsed
 */
static bool tag_compare(
   const deque<uint32_t>& d,
   uint32_t               a_idx,
   uint32_t               b_idx,
   uint32_t               len
);


/**
 * VALA verbatim string, ends with three quotes (""")
 */
static void parse_verbatim_string(
   tok_ctx& ctx,
   chunk_t& pc   /**< [in] structure to update, str is an input. */
);


/**
 * Parses a C++0x 'R' string. R"( xxx )" R"tag(  )tag" u8R"(x)" uR"(x)"
 * Newlines may be in the string.
 */
static bool parse_cr_string(
   tok_ctx& ctx,
   chunk_t& pc,   /**< [in] structure to update, str is an input. */
   uint32_t q_idx
);


/**
 * Count the number of whitespace characters.
 *
 * @return Whether whitespace was parsed
 */
static bool parse_whitespace(
   tok_ctx& ctx,
   chunk_t& pc   /**< [in] structure to update, str is an input. */
);


/**
 * Called when we hit a backslash.
 * If there is nothing but whitespace until the newline, then this is a
 * backslash newline
 */
static bool parse_bs_newline(
   tok_ctx& ctx,
   chunk_t& pc   /**< [in] structure to update, str is an input. */
);


/**
 * Parses any number of tab or space chars followed by a newline.
 * Does not change pc.len if a newline isn't found.
 * This is not the same as parse_whitespace() because it only consumes until
 * a single newline is encountered.
 */
static bool parse_newline(
   tok_ctx& ctx
);


/**
 * PAWN #define is different than C/C++.
 *   #define PATTERN REPLACEMENT_TEXT
 * The PATTERN may not contain a space or '[' or ']'.
 * A generic whitespace check should be good enough.
 * Do not change the pattern.
 */
static void parse_pawn_pattern(
   tok_ctx&  ctx,
   chunk_t&  pc,  /**< [in] structure to update, str is an input. */
   c_token_t tt
);


/**
 * tbd
 */
static bool parse_ignored(
   tok_ctx& ctx,
   chunk_t& pc  /**< [in] structure to update, str is an input. */
);


/**
 * Skips the next bit of whatever and returns the type of block.
 *
 * pc.str is the input text.
 * pc.len in the output length.
 * pc.type is the output type
 * pc.column is output column
 *
 * @return        true/false - whether anything was parsed
 */
static bool parse_next(
   tok_ctx& ctx,
   chunk_t& pc   /**< [in] structure to update, str is an input. */
);


/**
 * Parses all legal D string constants.
 *
 * Quoted strings:
 *   r"Wysiwyg"      # WYSIWYG string
 *   x"hexstring"    # Hexadecimal array
 *   `Wysiwyg`       # WYSIWYG string
 *   'char'          # single character
 *   "reg_string"    # regular string
 *
 * Non-quoted strings:
 * \x12              # 1-byte hex constant
 * \u1234            # 2-byte hex constant
 * \U12345678        # 4-byte hex constant
 * \123              # octal constant
 * \&amp;            # named entity
 * \n                # single character
 *
 * @return     Whether a string was parsed
 */
static bool d_parse_string(
   tok_ctx& ctx,
   chunk_t& pc   /**< [in] structure to update, str is an input. */
);


/**
 * Figure of the length of the comment at text.
 * The next bit of text starts with a '/', so it might be a comment.
 * There are three types of comments:
 *  - C comments that start with  '/ *' and end with '* /'
 *  - C++ comments that start with //
 *  - D nestable comments '/+SPACE+/'
 *
 * @return Whether a comment was parsed
 */
static bool parse_comment(
   tok_ctx& ctx,
   chunk_t& pc  /**< [in] structure to update, str is an input. */
);


/**
 * Figure of the length of the code placeholder at text, if present.
 * This is only for Xcode which sometimes inserts temporary code placeholder
 * chunks, which in plaintext <#look like this#>.
 *
 * @return Whether a placeholder was parsed.
 */
static bool parse_code_placeholder(
   tok_ctx& ctx, /**< [in]  */
   chunk_t& pc   /**< [in] structure to update, str is an input. */
);


/**
 * Parse any attached suffix, which may be a user-defined literal suffix.
 * If for a string, explicitly exclude common format and scan specifiers, ie,
 * PRIx32 and SCNx64.
 */
static void parse_suffix(
   tok_ctx& ctx,       /**< [in]  */
   chunk_t& pc,        /**< [in]  */
   bool     forstring  /**< [in]  */
);


/** check if a symbol holds a boolean value */
static bool is_bin (uint32_t ch); /**< [in] symbol to check */

/** check if a symbol holds a octal value */
static bool is_oct (uint32_t ch); /**< [in] symbol to check */

/** check if a symbol holds a decimal value */
static bool is_dec (uint32_t ch); /**< [in] symbol to check */

/** check if a symbol holds a hexadecimal value */
static bool is_hex (uint32_t ch); /**< [in] symbol to check */

/** check if a symbol holds a boolean value or an underscore */
static bool is_bin_or_underline(uint32_t ch); /**< [in] symbol to check */

/** check if a symbol holds a octal value or an underscore */
static bool is_oct_or_underline(uint32_t ch); /**< [in] symbol to check */

/** check if a symbol holds a decimal value or an underscore */
static bool is_dec_or_underline(uint32_t ch); /**< [in] symbol to check */

/** check if a symbol holds a hexadecimal value or an underscore */
static bool is_hex_or_underline(uint32_t ch); /**< [in] symbol to check */


/**
 * Count the number of characters in the number.
 * The next bit of text starts with a number (0-9 or '.'), so it is a number.
 * Count the number of characters in the number.
 *
 * This should cover all number formats for all languages.
 * Note that this is not a strict parser. It will happily parse numbers in
 * an invalid format.
 *
 * For example, only D allows underscores in the numbers, but they are
 * allowed in all formats.
 *
 * @return Whether a number was parsed
 */
static bool parse_number(
   tok_ctx& ctx, /**< [in]  */
   chunk_t& pc   /**< [in,out] The structure to update, str is an input. */
);


/**
 * tbd
 */
void append_multiple(
   tok_ctx& ctx, /**< [in]  */
   chunk_t& pc,  /**< [in]  */
   uint32_t cnt  /**< [in]  */
);


void append_multiple(tok_ctx &ctx, chunk_t &pc, uint32_t cnt)
{
   while (cnt--)
   {
      pc.str.append(ctx.get());
   }
}


static bool d_parse_string(tok_ctx& ctx, chunk_t& pc)
{
   uint32_t ch = ctx.peek();

   if ((ch == '"' ) ||
       (ch == '\'') ||
       (ch == '`' ) )
   {
      return(parse_string(ctx, pc, 0, true));
   }
   else if (ch == '\\')
   {
      ctx.save();
      pc.str.clear();
      while (ctx.peek() == '\\')
      {
         pc.str.append(ctx.get());
         /* Check for end of file */
         switch (ctx.peek())
         {
            case 'x': append_multiple(ctx, pc, 3); break; /* \x HexDigit HexDigit */
            case 'u': append_multiple(ctx, pc, 5); break; /* \u HexDigit HexDigit HexDigit HexDigit */
            case 'U': append_multiple(ctx, pc, 9); break; /* \U HexDigit (x8) */
            case '0': /* fallthrough */
            case '1': /* fallthrough */
            case '2': /* fallthrough */
            case '3': /* fallthrough */
            case '4': /* fallthrough */
            case '5': /* fallthrough */
            case '6': /* fallthrough */
            case '7':
               /* handle up to 3 octal digits */
               pc.str.append(ctx.get());
               ch = ctx.peek();
               if (is_oct(ch))
               {
                  pc.str.append(ctx.get());
                  ch = ctx.peek();
                  if (is_oct(ch))
                  {
                     pc.str.append(ctx.get());
                  }
               }
               break;

            case '&':
               /* \& NamedCharacterEntity ; */
               pc.str.append(ctx.get());
               while (unc_isalpha(ctx.peek()))
               {
                  pc.str.append(ctx.get());
               }
               if (ctx.peek() == ';')
               {
                  pc.str.append(ctx.get());
               }
               break;

            default:
               /* Everything else is a single character */
               pc.str.append(ctx.get());
               break;
         }
      }

      if (pc.str.size() > 1)
      {
         pc.type = CT_STRING;
         return(true);
      }
      ctx.restore();
   }
   else if (((ch == 'r'         ) ||
             (ch == 'x'         ) ) &&
             (ctx.peek(1) == '"')   )
   {
      return(parse_string(ctx, pc, 1, false));
   }
   return(false);
}


#if 0
/**
 * A string-in-string search.  Like strstr() with a haystack length.
 */
static const char *str_search(const char *needle, const char *haystack, int32_t haystack_len)
{
   int32_t needle_len = strlen(needle);

   while (haystack_len-- >= needle_len)
   {
      if (memcmp(needle, haystack, needle_len) == 0)
      {
         return(haystack);
      }
      haystack++;
   }
   return(nullptr);
}
#endif


void parse_char(
   tok_ctx& ctx,
   chunk_t& pc
);

/* \todo name might be improved */
void parse_char(tok_ctx& ctx, chunk_t& pc)
{
  uint32_t ch = ctx.get();
  pc.str.append(ch);

  if (is_part_of_nl(ch))
  {
     pc.type = CT_COMMENT_MULTI;
     pc.nl_count++;

     if (ch == CARRIAGERETURN)
     {
        if (ctx.peek() == LINEFEED)
        {
           cpd.le_counts[LE_CRLF]++;
           pc.str.append(ctx.get());  /* store the LINEFEED */
        }
        else
        {
           cpd.le_counts[LE_CR]++;
        }
     }
     else
     {
        cpd.le_counts[LE_LF]++;
     }
  }
}


static bool parse_comment(tok_ctx &ctx, chunk_t &pc)
{
   bool   is_d    = (is_lang(cpd, LANG_D ));
   bool   is_cs   = (is_lang(cpd, LANG_CS));
   uint32_t d_level = 0;

   /* does this start with '/ /' or '/ *' or '/ +' (d) */
   if (  (ctx.peek( ) != '/'  )     ||
       ( (ctx.peek(1) != '*'  )   &&
         (ctx.peek(1) != '/'  )   &&
        ((ctx.peek(1) != '+'  ) ||
         (is_d        == false) ) ) )
   {
      return(false);
   }

   ctx.save();

   /* account for opening two chars */
   pc.str = ctx.get(); /* opening '/' */
   uint32_t ch = ctx.get();
   pc.str.append(ch);     /* second char */

   if (ch == '/')
   {
      pc.type = CT_COMMENT_CPP;
      while (true)
      {
         int32_t bs_cnt = 0;
         while (ctx.more())
         {
            ch = ctx.peek();
            break_if(is_part_of_nl(ch));

            if ((ch    == BACKSLASH) &&
                (is_cs == false    ) ) /* backslashes aren't special in comments in C# */
            {
               bs_cnt++;
            }
            else
            {
               bs_cnt = 0;
            }
            pc.str.append(ctx.get());
         }

         /* If we hit an odd number of backslashes right before the newline,
          * then we keep going.
          */
         break_if(((bs_cnt & 1) == 0    ) ||
                   (ctx.more()  == false) );

         if (ctx.peek() == CARRIAGERETURN) { pc.str.append(ctx.get()); }
         if (ctx.peek() == LINEFEED      ) { pc.str.append(ctx.get()); }
         pc.nl_count++;
         cpd.did_newline = true;
      }
   }
   else if (ctx.more() == false)
   {
      /* unexpected end of file */
      ctx.restore();
      return(false);
   }
   else if (ch == '+')
   {
      pc.type = CT_COMMENT;
      d_level++;
      while ((d_level > 0) && ctx.more())
      {
         if ((ctx.peek( ) == '+') &&
             (ctx.peek(1) == '/') )
         {
            pc.str.append(ctx.get());  /* store the '+' */
            pc.str.append(ctx.get());  /* store the '/' */
            d_level--;
            continue;
         }

         if ((ctx.peek( ) == '/') &&
             (ctx.peek(1) == '+') )
         {
            pc.str.append(ctx.get());  /* store the '/' */
            pc.str.append(ctx.get());  /* store the '+' */
            d_level++;
            continue;
         }
         parse_char(ctx, pc);
      }

   }
   else  /* must be '/ *' */
   {
      pc.type = CT_COMMENT;
      while (ctx.more())
      {
         if ((ctx.peek( ) == '*') &&
             (ctx.peek(1) == '/') )
         {
            pc.str.append(ctx.get());  /* store the '*' */
            pc.str.append(ctx.get());  /* store the '/' */

            tok_info ss;
            ctx.save(ss);
            uint32_t oldsize = pc.str.size();

            /* If there is another C comment right after this one, combine them */
            while ((ctx.peek() == SPACE  ) ||
                   (ctx.peek() == TABSTOP) )
            {
               pc.str.append(ctx.get());
            }
            if ((ctx.peek( ) != '/') ||
                (ctx.peek(1) != '*') )
            {
               /* undo the attempt to join */
               ctx.restore(ss);
               pc.str.resize(oldsize);
               break;
            }
         }
         parse_char(ctx, pc);
      }
   }

   if (cpd.unc_off)
   {
      const char *ontext = cpd.settings[UO_enable_processing_cmt].str;
      if ((ontext == nullptr) ||
          !ontext[0])
      {
         ontext = UNCRUSTIFY_ON_TEXT;
      }

      if (pc.str.find(ontext) >= 0)
      {
         LOG_FMT(LBCTRL, "Found '%s' on line %u\n", ontext, pc.orig_line);
         cpd.unc_off = false;
      }
   }
   else
   {
      const char *offtext = cpd.settings[UO_disable_processing_cmt].str;
      if ((offtext == nullptr) ||
          !offtext[0])
      {
         offtext = UNCRUSTIFY_OFF_TEXT;
      }

      if (pc.str.find(offtext) >= 0)
      {
         LOG_FMT(LBCTRL, "Found '%s' on line %u\n", offtext, pc.orig_line);
         cpd.unc_off      = true;
         cpd.unc_off_used = true;          // Issue #842
      }
   }
   return(true);
}


static bool parse_code_placeholder(tok_ctx &ctx, chunk_t &pc)
{
   if ((ctx.peek( ) != '<') ||
       (ctx.peek(1) != '#') )
   {
      return(false);
   }

   ctx.save();

   /* account for opening two chars '<#' */
   pc.str = ctx.get();
   pc.str.append(ctx.get());

   /* grab everything until '#>', fail if not found. */
   uint32_t last1 = 0;
   while (ctx.more())
   {
      uint32_t last2 = last1;
      last1 = ctx.get();
      pc.str.append(last1);

      if ((last2 == '#') &&
          (last1 == '>') )
      {
         pc.type = CT_WORD;
         return(true);
      }
   }
   ctx.restore();
   return(false);
}


static void parse_suffix(tok_ctx &ctx, chunk_t &pc, bool forstring = false)
{
   if (CharTable::IsKW1(ctx.peek()))
   {
      uint32_t slen    = 0;
      uint32_t oldsize = pc.str.size();

      /* don't add the suffix if we see L" or L' or S" */
      uint32_t p1 = ctx.peek();
      uint32_t p2 = ctx.peek(1);
      if (( forstring == true                           ) &&
          (((p1 == 'L') && ((p2 == '"') || (p2 == '\''))) ||
           ((p1 == 'S') &&  (p2 == '"')               ) ) )
      {
         return;
      }

      tok_info ss;
      ctx.save(ss);
      while (ctx.more() &&
             CharTable::IsKW2(ctx.peek()))
      {
         slen++;
         pc.str.append(ctx.get());
      }

      if ((forstring == true) &&
          (slen      >= 4   ) &&
          (pc.str.startswith("PRI", oldsize) ||
           pc.str.startswith("SCN", oldsize) ) )
      {
         ctx.restore(ss);
         pc.str.resize(oldsize);
      }
   }
}


static bool is_hex(uint32_t ch)
{
   return(((ch >= '0') && (ch <= '9')) ||
          ((ch >= 'a') && (ch <= 'f')) ||
          ((ch >= 'A') && (ch <= 'F')) );
}


static bool is_bin(uint32_t ch) { return((ch >= '0') && (ch <= '1')); }
static bool is_oct(uint32_t ch) { return((ch >= '0') && (ch <= '7')); }
static bool is_dec(uint32_t ch) { return((ch >= '0') && (ch <= '9')); }

static bool is_bin_or_underline(uint32_t ch) { return(is_bin(ch) || (ch == '_')); }
static bool is_oct_or_underline(uint32_t ch) { return(is_oct(ch) || (ch == '_')); }
static bool is_dec_or_underline(uint32_t ch) { return(is_dec(ch) || (ch == '_')); }
static bool is_hex_or_underline(uint32_t ch) { return(is_hex(ch) || (ch == '_')); }

bool analyze_character(
   tok_ctx& ctx,
   chunk_t& pc
);

bool analyze_character(tok_ctx& ctx, chunk_t& pc)
{
   bool did_hex = false;

   switch (unc_toupper(ctx.peek()) )
   {
      case 'X':  /* hex */
         did_hex = true;
         do
         {
            pc.str.append(ctx.get());  /* store the 'x' and then the rest */
         } while (is_hex_or_underline(ctx.peek()));
         break;

      case 'B':  /* binary */
         do
         {
            pc.str.append(ctx.get());  /* store the 'b' and then the rest */
         } while (is_bin_or_underline(ctx.peek()));
         break;

      case '0':  /* octal or decimal */
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
         do
         {
            pc.str.append(ctx.get());
         } while (is_oct_or_underline(ctx.peek()));
         break;

      default:
         /* either just 0 or 0.1 or 0UL, etc */
         break;
   }

   return(did_hex);
}


static bool parse_number(tok_ctx &ctx, chunk_t &pc)
{
   /* A number must start with a digit or a dot, followed by a digit */
   if (!is_dec(ctx.peek()) &&
       ((ctx.peek() != '.') || !is_dec(ctx.peek(1))))
   {
      return(false);
   }

   bool is_float = (ctx.peek( ) == '.');
   if ((is_float    == true) &&
       (ctx.peek(1) == '.' ) )
   {
      return(false);
   }

   /* Check for Hex, Octal, or Binary
    * Note that only D and Pawn support binary, but who cares? */
   bool did_hex = false;
   if (ctx.peek() == '0')
   {
      pc.str.append(ctx.get());  /* store the '0' */
      uint32_t  ch;
      uint32_t  pc_length;
      chunk_t pc_temp;
      pc_temp.str.append('0');
      // MS constant might have an "h" at the end. Look for it
      ctx.save();
      while (ctx.more() && CharTable::IsKW2(ctx.peek()))
      {
         ch = ctx.get();
         pc_temp.str.append(ch);
      }
      pc_length = pc_temp.len();
      ch        = pc_temp.str[pc_length - 1];
      ctx.restore();
      LOG_FMT(LGUY, "%s(%d): pc_temp:%s\n", __func__, __LINE__, pc_temp.text());
      if (ch == 'h') /** \todo can we combine this in analyze_character */
      {
         // we have an MS hexadecimal number with "h" at the end
         LOG_FMT(LGUY, "%s(%d): MS hexadecimal number\n", __func__, __LINE__);
         did_hex = true;
         do
         {
            pc.str.append(ctx.get()); /* store the rest */
         } while (is_hex_or_underline(ctx.peek()));
         pc.str.append(ctx.get());    /* store the h */
         LOG_FMT(LGUY, "%s(%d): pc:%s\n", __func__, __LINE__, pc.text());
      }
      else
      {
         did_hex = analyze_character(ctx, pc);
      }
   }
   else
   {
      /* Regular int or float */
      while (is_dec_or_underline(ctx.peek()))
      {
         pc.str.append(ctx.get());
      }
   }

   /* Check if we stopped on a decimal point & make sure it isn't '..' */
   if ((ctx.peek() == '.') && (ctx.peek(1) != '.'))
   {
      pc.str.append(ctx.get());
      is_float = true;
      if (did_hex) { while (is_hex_or_underline(ctx.peek())) { pc.str.append(ctx.get()); } }
      else         { while (is_dec_or_underline(ctx.peek())) { pc.str.append(ctx.get()); } }
   }

   /* Check exponent
    * Valid exponents per language (not that it matters):
    * C/C++/D/Java: eEpP
    * C#/Pawn:      eE */
   uint32_t tmp = unc_toupper(ctx.peek());
   if ((tmp == 'E') ||
       (tmp == 'P') )
   {
      is_float = true;
      pc.str.append(ctx.get());
      if ((ctx.peek() == '+') ||
          (ctx.peek() == '-') )  { pc.str.append(ctx.get()); }
      while (is_dec_or_underline(ctx.peek())){ pc.str.append(ctx.get()); }
   }

   /* Check the suffixes
    * Valid suffixes per language (not that it matters):
    *        Integer       Float
    * C/C++: uUlL64        lLfF
    * C#:    uUlL          fFdDMm
    * D:     uUL           ifFL
    * Java:  lL            fFdD
    * Pawn:  (none)        (none)
    *
    * Note that i, f, d, and m only appear in floats. */
   while (1)
   {
      uint32_t tmp2 = unc_toupper(ctx.peek());
      if ((tmp2 == 'I') ||
          (tmp2 == 'F') ||
          (tmp2 == 'D') ||
          (tmp2 == 'M') )
      {
         is_float = true;
      }
      else if ((tmp2 != 'L') &&
               (tmp2 != 'U') )
      {
         break;
      }
      pc.str.append(ctx.get());
   }

   /* skip the Microsoft-specific '64' suffix */
   if ((ctx.peek( ) == '6') &&
       (ctx.peek(1) == '4') )
   {
      pc.str.append(ctx.get());
      pc.str.append(ctx.get());
   }

   pc.type = is_float ? CT_NUMBER_FP : CT_NUMBER;

   /* If there is anything left, then we are probably dealing with garbage or
    * some sick macro junk. Eat it. */
   parse_suffix(ctx, pc);

   return(true);
}


static bool parse_string(tok_ctx& ctx, chunk_t& pc, uint32_t quote_idx, bool allow_escape)
{
   uint32_t escape_char      = get_uval(UO_string_escape_char);
   uint32_t escape_char2     = get_uval(UO_string_escape_char2);
   bool   should_escape_tabs = is_true(UO_string_replace_tab_chars) && (cpd.lang_flags & LANG_ALLC);

   pc.str.clear();
   while (quote_idx-- > 0)
   {
      pc.str.append(ctx.get());
   }

   pc.type = CT_STRING;
   uint32_t end_ch = CharTable::Get(ctx.peek()) & 0xff;
   pc.str.append(ctx.get());  /* store the " */

   bool escaped = false;
   while (ctx.more())
   {
      uint32_t lastcol = ctx.c.col;
      uint32_t ch      = ctx.get();

      if ((ch == TABSTOP) &&
           should_escape_tabs)
      {
         ctx.c.col = lastcol + 2;
         pc.str.append(escape_char);
         pc.str.append('t');
         continue;
      }

      pc.str.append(ch);
      if (ch == LINEFEED)
      {
         pc.nl_count++;
         pc.type = CT_STRING_MULTI;
         escaped = false;
         continue;
      }
      if ((ch         == CARRIAGERETURN) &&
          (ctx.peek() != LINEFEED      ) )
      {
         pc.str.append(ctx.get());
         pc.nl_count++;
         pc.type = CT_STRING_MULTI;
         escaped = false;
         continue;
      }
      if (!escaped)
      {
         if (ch == escape_char)
         {
            escaped = (escape_char != 0);
         }
         else if ((ch         == escape_char2) &&
                  (ctx.peek() == end_ch      ) )
         {
            escaped = allow_escape;
         }
         else if (ch == end_ch)
         {
            break;
         }
      }
      else
      {
         escaped = false;
      }
   }

   parse_suffix(ctx, pc, true);
   return(true);
}


static bool parse_cs_string(tok_ctx &ctx, chunk_t &pc)
{
   pc.str = ctx.get();
   pc.str.append(ctx.get());
   pc.type = CT_STRING;

   bool should_escape_tabs = is_true(UO_string_replace_tab_chars);

   /* go until we hit a zero (end of file) or a single " */
   while (ctx.more())
   {
      uint32_t ch = ctx.get();
      pc.str.append(ch);

      if(is_part_of_nl(ch))
      {
         pc.type = CT_STRING_MULTI;
         pc.nl_count++;
      }
      else if (ch == TABSTOP)
      {
         if (should_escape_tabs && !cpd.warned_unable_string_replace_tab_chars)
         {
            cpd.warned_unable_string_replace_tab_chars = true;

            log_sev_t warnlevel = static_cast<log_sev_t>(get_uval(UO_warn_level_tabs_found_in_verbatim_string_literals));

            /* a tab char can't be replaced with \\t because escapes don't work in here-strings. best we can do is warn. */
            LOG_FMT(warnlevel, "%s:%u Detected non-replaceable tab char in literal string\n", cpd.filename, pc.orig_line);
            if (warnlevel < LWARN)
            {
               cpd.error_count++;
            }
         }
      }
      else if (ch == '"')
      {
         if (ctx.peek() == '"')
         {
            pc.str.append(ctx.get());
         }
         else
         {
            break;
         }
      }
   }

   return(true);
}


static bool parse_cs_interpolated_string(tok_ctx &ctx, chunk_t &pc)
{
   pc.str = ctx.get();        // '$'
   pc.str.append(ctx.get());  // '"'
   pc.type = CT_STRING;

   int32_t depth = 0;

   /* go until we hit a zero (end of file) or a single " */
   while (ctx.more())
   {
      uint32_t ch = ctx.get();
      pc.str.append(ch);

      /* if we are inside a { }, then we only look for a } */
      if (depth > 0)
      {
         if (ch == '}')
         {
            if (ctx.peek() == '}')
            {
               // }} doesn't decrease the depth
               pc.str.append(ctx.get());  // '{'
            }
            else
            {
               depth--;
            }
         }
      }
      else
      {
         if (ch == '{')
         {
            if (ctx.peek() == '{')
            {
               // {{ doesn't increase the depth
               pc.str.append(ctx.get());
            }
            else
            {
               depth++;
            }
         }
         else if (ch == '"')
         {
            if (ctx.peek() == '"')
            {
               pc.str.append(ctx.get());
            }
            else
            {
               break;
            }
         }
      }
   }

   return(true);
}


static void parse_verbatim_string(tok_ctx &ctx, chunk_t &pc)
{
   pc.type = CT_STRING;

   // consume the initial """
   pc.str = ctx.get();
   pc.str.append(ctx.get());
   pc.str.append(ctx.get());

   /* go until we hit a zero (end of file) or a """ */
   while (ctx.more())
   {
      uint32_t ch = ctx.get();
      pc.str.append(ch);
      if ((ch          == '"') &&
          (ctx.peek( ) == '"') &&
          (ctx.peek(1) == '"') )
      {
         pc.str.append(ctx.get());
         pc.str.append(ctx.get());
         break;
      }

      if(is_part_of_nl(ch))
      {
         pc.type = CT_STRING_MULTI;
         pc.nl_count++;
      }
   }
}


static bool tag_compare(const deque<uint32_t> &d, uint32_t a_idx, uint32_t b_idx, uint32_t len)
{
   if (a_idx != b_idx)
   {
      while (len-- > 0)
      {
         if (d[a_idx] != d[b_idx])
         {
            return(false);
         }
      }
   }
   return(true);
}


static bool parse_cr_string(tok_ctx &ctx, chunk_t &pc, uint32_t q_idx)
{
   uint32_t tag_idx = ctx.c.idx + q_idx + 1;
   uint32_t tag_len = 0;

   ctx.save();

   /* Copy the prefix + " to the string */
   pc.str.clear();
   uint32_t cnt = q_idx + 1;
   while (cnt--)
   {
      pc.str.append(ctx.get());
   }

   /* Add the tag and get the length of the tag */
   while ((ctx.more() == true) &&
          (ctx.peek() != '(' ) )
   {
      tag_len++;
      pc.str.append(ctx.get());
   }
   if (ctx.peek() != '(')
   {
      ctx.restore();
      return(false);
   }

   pc.type = CT_STRING;
   while (ctx.more())
   {
      if ((ctx.peek()            == ')') &&
          (ctx.peek(tag_len + 1) == '"') &&
          tag_compare(ctx.data, tag_idx, ctx.c.idx + 1, tag_len))
      {
         cnt = tag_len + 2;   /* for the )" */
         while (cnt--)
         {
            pc.str.append(ctx.get());
         }
         parse_suffix(ctx, pc);
         return(true);
      }
      if (ctx.peek() == LINEFEED)
      {
         pc.str.append(ctx.get());
         pc.nl_count++;
         pc.type = CT_STRING_MULTI;
      }
      else
      {
         pc.str.append(ctx.get());
      }
   }
   ctx.restore();
   return(false);
}


static bool parse_word(tok_ctx &ctx, chunk_t &pc, bool skipcheck)
{
   static const unc_text intr_txt("@interface");

   /* The first character is already valid */
   pc.str.clear();
   pc.str.append(ctx.get());

   while (ctx.more() == true)
   {
      uint32_t ch = ctx.peek();
      if (CharTable::IsKW2(ch))
      {
         pc.str.append(ctx.get());
      }
      else if ((ch == BACKSLASH) && (unc_tolower(ctx.peek(1)) == 'u'))
      {
         pc.str.append(ctx.get());
         pc.str.append(ctx.get());
         skipcheck = true;
      }
      else
      {
         break;
      }

      /* HACK: Non-ASCII character are only allowed in identifiers */
      if (ch > 0x7f) { skipcheck = true; }

   }
   pc.type = CT_WORD;

   if (skipcheck) { return(true); }

   /* Detect pre-processor functions now */
   if ((cpd.is_preproc == CT_PP_DEFINE) && (cpd.preproc_ncnl_count == 1))
   {
      if (ctx.peek() == '(') { pc.type = CT_MACRO_FUNC; }
      else
      {
         pc.type = CT_MACRO;
         if (is_true(UO_pp_ignore_define_body))
         {
            /* We are setting the PP_IGNORE preproc state because the following
             * chunks are part of the macro body and will have to be ignored. */
            cpd.is_preproc = CT_PP_IGNORE;
         }
      }
   }
   else
   {
      /* '@interface' is reserved, not an interface itself */
      if (is_lang(cpd, LANG_JAVA) &&
           pc.str.startswith("@") &&
          !pc.str.equals(intr_txt))
      {
         pc.type = CT_ANNOTATION;
      }
      else
      {
         /* Turn it into a keyword now */
         pc.type = find_keyword_type(pc.text(), pc.str.size());

         /* Special pattern: if we're trying to redirect a preprocessor directive to PP_IGNORE,
          * then ensure we're actually part of a preprocessor before doing the swap, or we'll
          * end up with a function named 'define' as PP_IGNORE. This is necessary because with
          * the config 'set' feature, there's no way to do a pair of tokens as a word
          * substitution. */
         if (is_type(pc.type, CT_PP_IGNORE) && !cpd.is_preproc)
         {
            pc.type = find_keyword_type(pc.text(), pc.str.size());
         }
      }
   }

   return(true);
}


static bool parse_whitespace(tok_ctx &ctx, chunk_t &pc)
{
   uint32_t nl_count = 0;
   uint32_t ch       = 0;

   /* REVISIT: use a better whitespace detector? */
   while (ctx.more() && unc_isspace(ctx.peek()))
   {
      ch = ctx.get();   /* throw away the whitespace char */
      switch (ch)
      {
         case CARRIAGERETURN:
            if (ctx.expect(LINEFEED)) { cpd.le_counts[LE_CRLF]++; } /* CRLF ending */
            else                      { cpd.le_counts[LE_CR  ]++; } /* CR ending */
            nl_count++;
            pc.orig_prev_sp = 0;
            break;

         case LINEFEED:
            /* LF ending */
            cpd.le_counts[LE_LF]++;
            nl_count++;
            pc.orig_prev_sp = 0;
            break;

         case TABSTOP:
            pc.orig_prev_sp += calc_next_tab_column(cpd.column, get_uval(UO_input_tab_size)) - cpd.column;
            break;

         case SPACE:
            pc.orig_prev_sp++;
            break;

         default:
            break;
      }
   }

   if (ch != 0)
   {
      pc.str.clear();
      pc.nl_count  = nl_count;
      pc.type      = nl_count ? CT_NEWLINE : CT_WHITESPACE;
      pc.after_tab = (ctx.c.last_ch == TABSTOP);
      return(true);
   }
   return(false);
}


static bool parse_bs_newline(tok_ctx &ctx, chunk_t &pc)
{
   ctx.save();
   ctx.get(); /* skip the '\' */

   uint32_t ch;
   while ((ctx.more()) && unc_isspace(ch = ctx.peek()))
   {
      ctx.get();
      if(is_part_of_nl(ch))
      {
         if (ch == CARRIAGERETURN)
         {
            ctx.expect(LINEFEED);
         }
         pc.str      = "\\";
         pc.type     = CT_NL_CONT;
         pc.nl_count = 1;
         return(true);
      }
   }

   ctx.restore();
   return(false);
}


static bool parse_newline(tok_ctx &ctx)
{
   ctx.save();

   /* Eat whitespace */
   while(is_space_or_tab(ctx.peek())) { ctx.get(); }

   if(is_part_of_nl(ctx.peek()))
   {
      if (ctx.expect(LINEFEED) == false)
      {
         ctx.get();
         ctx.expect(LINEFEED);
      }
      return(true);
   }
   ctx.restore();
   return(false);
}


static void parse_pawn_pattern(tok_ctx &ctx, chunk_t &pc, c_token_t tt)
{
   pc.str.clear();
   pc.type = tt;
   while (!unc_isspace(ctx.peek()))
   {
      /* end the pattern on an escaped newline */
      if (ctx.peek() == BACKSLASH)
      {
         uint32_t ch = ctx.peek(1);
         break_if(is_part_of_nl(ch));
      }
      pc.str.append(ctx.get());
   }
}


static bool parse_ignored(tok_ctx &ctx, chunk_t &pc)
{
   /* Parse off newlines/blank lines */
   uint32_t nl_count = 0;
   while (parse_newline(ctx))
   {
      nl_count++;
   }

   if (nl_count > 0)
   {
      pc.nl_count = nl_count;
      pc.type     = CT_NEWLINE;
      return(true);
   }

   /* See if the UO_enable_processing_cmt text is on this line */
   ctx.save();
   pc.str.clear();
   while ((ctx.more() == true          ) &&
          (ctx.peek() != CARRIAGERETURN) &&
          (ctx.peek() != LINEFEED      ) )
   {
      pc.str.append(ctx.get());
   }

   retval_if((pc.str.size() == 0), false);   /* end of file? */

   /* Note that we aren't actually making sure this is in a comment, yet */
   if ((((pc.str.find("#pragma ") >= 0) || (pc.str.find("#pragma	") >= 0)) &&
        ((pc.str.find(" endasm" ) >= 0) || (pc.str.find("	endasm") >= 0))) ||
         (pc.str.find("#endasm" ) >= 0))
   {
      cpd.unc_off = false;
      ctx.restore();
      pc.str.clear();
      return(false);
   }
   /* Note that we aren't actually making sure this is in a comment, yet */
   const char *ontext = cpd.settings[UO_enable_processing_cmt].str;
   if (ptr_is_invalid(ontext))
   {
      ontext = UNCRUSTIFY_ON_TEXT;
   }

   if (pc.str.find(ontext) < 0)
   {
      pc.type = CT_IGNORED;
      return(true);
   }
   ctx.restore();

   /* parse off whitespace leading to the comment */
   if (parse_whitespace(ctx, pc))
   {
      pc.type = CT_IGNORED;
      return(true);
   }

   /* Look for the ending comment and let it pass */
   retval_if((parse_comment(ctx, pc) && !cpd.unc_off), true);

   /* Reset the chunk & scan to until a newline */
   pc.str.clear();
   while ((ctx.more() == true          ) &&
          (ctx.peek() != CARRIAGERETURN) &&
          (ctx.peek() != LINEFEED      ) )
   {
      pc.str.append(ctx.get());
   }
   if (pc.str.size() > 0)
   {
      pc.type = CT_IGNORED;
      return(true);
   }
   return(false);
}


static bool parse_next(tok_ctx &ctx, chunk_t &pc)
{
   retval_if(ctx.more() == false, false);

   /* Save off the current column */
   pc.orig_line = ctx.c.row;
   pc.column    = ctx.c.col;
   pc.orig_col  = ctx.c.col;
   pc.type      = CT_NONE;
   pc.nl_count  = 0;
   pc.flags     = 0;

   /* If it is turned off, we put everything except newlines into CT_UNKNOWN */
   if (cpd.unc_off)
   {
      retval_if(parse_ignored(ctx, pc), true);
   }

   /* Parse whitespace */
   if (parse_whitespace(ctx, pc)){ return(true); }

   /* Handle unknown/unhandled preprocessors */
   if ((cpd.is_preproc >  CT_PP_BODYCHUNK) &&
       (cpd.is_preproc <= CT_PP_OTHER    ) )
   {
      pc.str.clear();
      tok_info ss;
      ctx.save(ss);
      /* Chunk to a newline or comment */
      pc.type = CT_PREPROC_BODY;
      uint32_t last = 0;
      while (ctx.more())
      {
         uint32_t ch = ctx.peek();

         if(is_part_of_nl(ch))
         {
            /* Back off if this is an escaped newline */
            if (last == BACKSLASH)
            {
               ctx.restore(ss);
               pc.str.pop_back();
            }
            break;
         }
         /* Quit on a C++ comment start */
         if ((ch          == SLASH) &&
             (ctx.peek(1) == SLASH) )
         {
            break;
         }
         last = ch;
         ctx.save(ss);

         pc.str.append(ctx.get());
      }
      if (pc.str.size() > 0) { return(true); }
   }

   /* Detect backslash-newline */
   if ((ctx.peek() == BACKSLASH) &&
       parse_bs_newline(ctx, pc))
   {
      return(true);
   }

   /* Parse comments */
   if (parse_comment(ctx, pc)) { return(true); }

   /* Parse code placeholders */
   if (parse_code_placeholder(ctx, pc)) { return(true); }

   /* Check for C# literal strings, ie @"hello" and identifiers @for*/
   if (is_lang(cpd, LANG_CS) && (ctx.peek() == '@'))
   {
      if (ctx.peek(1) == '"')
      {
         parse_cs_string(ctx, pc);
         return(true);
      }
      /* check for non-keyword identifiers such as @if @switch, etc */
      if (CharTable::IsKW1(ctx.peek(1)))
      {
         parse_word(ctx, pc, true);
         return(true);
      }
   }

   /* Check for C# Interpolated strings */
   if (is_lang(cpd, LANG_CS) &&
       (ctx.peek( ) == '$' ) &&
       (ctx.peek(1) == '"' ) )
   {
      parse_cs_interpolated_string(ctx, pc);
      return(true);
   }

   /* handle VALA """ strings """ */
   if (is_lang(cpd, LANG_VALA) &&
       (ctx.peek( ) == '"'   ) &&
       (ctx.peek(1) == '"'   ) &&
       (ctx.peek(2) == '"'   ) )
   {
      parse_verbatim_string(ctx, pc);
      return(true);
   }

   /* handle C++0x strings u8"x" u"x" U"x" R"x" u8R"XXX(I'm a "raw UTF-8" string.)XXX" */
   uint32_t ch = ctx.peek();
   if (is_lang(cpd, LANG_CPP) &&
       ((ch == 'u') ||
        (ch == 'U') ||
        (ch == 'R') ) )
   {
      uint32_t idx     = 0;
      bool   is_real = false;

      if ((ch          == 'u') &&
          (ctx.peek(1) == '8') )
      {
         idx = 2;
      }
      else if (unc_tolower(ch) == 'u')
      {
         idx++;
      }

      if (ctx.peek(idx) == 'R')
      {
         idx++;
         is_real = true;
      }
      if (ctx.peek(idx) == '"')
      {
         if (is_real)
         {
            if (parse_cr_string(ctx, pc, idx)) { return(true); }
         }
         else
         {
            if (parse_string(ctx, pc, idx, true))
            {
               parse_suffix(ctx, pc, true);
               return(true);
            }
         }
      }
   }

   /* PAWN specific stuff */
   if (is_lang(cpd, LANG_PAWN))
   {
      if (( cpd.preproc_ncnl_count == 1   )   &&
          ((cpd.is_preproc == CT_PP_DEFINE) ||
           (cpd.is_preproc == CT_PP_EMIT  ) ) )
      {
         parse_pawn_pattern(ctx, pc, CT_MACRO);
         return(true);
      }
      /* Check for PAWN strings: \"hi" or !"hi" or !\"hi" or \!"hi" */
      if ((ctx.peek() == BACKSLASH) ||
          (ctx.peek() == '!' ) )
      {
         if (ctx.peek(1) == '"')
         {
            parse_string(ctx, pc, 1, (ctx.peek() == '!'));
            return(true);
         }
         else if (((ctx.peek(1) == BACKSLASH) ||
                   (ctx.peek(1) == '!'      ) ) &&
                   (ctx.peek(2) == '"'      )   )
         {
            parse_string(ctx, pc, 2, false);
            return(true);
         }
      }

      /* handle PAWN preprocessor args %0 .. %9 */
      if ((cpd.is_preproc == CT_PP_DEFINE) &&
          (ctx.peek() == '%') &&
          unc_isdigit(ctx.peek(1)))
      {
         pc.str.clear();
         pc.str.append(ctx.get());
         pc.str.append(ctx.get());
         pc.type = CT_WORD;
         return(true);
      }
   }

   /* Parse strings and character constants */

//parse_word(ctx, pc_temp, true);
//ctx.restore(ctx.c);
   if (parse_number(ctx, pc)) { return(true); }

   if (is_lang(cpd, LANG_D))
   {
      /* D specific stuff */
      if (d_parse_string(ctx, pc)) { return(true); }
   }
   else
   {
      /* Not D stuff */

      /* Check for L'a', L"abc", 'a', "abc", <abc> strings */
      ch = ctx.peek();
      uint32_t ch1 = ctx.peek(1);
      if((((ch  == 'L' ) || (ch  == 'S' )) &&
          ((ch1 == '"' ) || (ch1 == '\''))) ||
          ( ch  == '"' ) ||
          ( ch  == '\'') ||
          ((ch  == '<' ) && (cpd.is_preproc == CT_PP_INCLUDE)))
      {
         parse_string(ctx, pc, unc_isalpha(ch) ? 1 : 0, true);
         return(true);
      }

      if ((ch == '<') && (cpd.is_preproc == CT_PP_DEFINE))
      {
         if (is_type(chunk_get_tail(), CT_MACRO))
         {
            /* We have "#define XXX <", assume '<' starts an include string */
            parse_string(ctx, pc, 0, false);
            return(true);
         }
      }
   }

   /* Check for Objective C literals and VALA identifiers ('@1', '@if')*/
   if ((cpd.lang_flags & (LANG_OC | LANG_VALA)) && (ctx.peek() == '@'))
   {
      uint32_t nc = ctx.peek(1);
      if ((nc == '"' ) ||
          (nc == '\'') )
      {
         /* literal string */
         parse_string(ctx, pc, 1, true);
         return(true);
      }
      else if (is_dec(nc))
      {
         /* literal number */
         pc.str.append(ctx.get());  /* store the '@' */
         parse_number(ctx, pc);
         return(true);
      }
   }

   /* Check for pawn/ObjectiveC/Java and normal identifiers */
   if (CharTable::IsKW1(ctx.peek()) ||
       ((ctx.peek() == BACKSLASH) && (unc_tolower(ctx.peek(1)) == 'u')) ||
       ((ctx.peek() == '@'      ) && CharTable::IsKW1(ctx.peek(1))))
   {
      parse_word(ctx, pc, false);
      return(true);
   }

   /* see if we have a punctuator */
   char punc_txt[4];
   punc_txt[0] = (char)ctx.peek( );
   punc_txt[1] = (char)ctx.peek(1);
   punc_txt[2] = (char)ctx.peek(2);
   punc_txt[3] = (char)ctx.peek(3);
   const chunk_tag_t *punc;
   if ((punc = find_punctuator(punc_txt, cpd.lang_flags)) != nullptr)
   {
      int32_t cnt = (int32_t)strlen(punc->tag);
      while (cnt--)
      {
         pc.str.append(ctx.get());
      }
      pc.type   = punc->type;
      pc.flags |= PCF_PUNCTUATOR;
      return(true);
   }

   /* throw away this character */
   pc.type = CT_UNKNOWN;
   pc.str.append(ctx.get());

   LOG_FMT(LWARN, "%s:%u Garbage in col %d: %x\n",
           cpd.filename, pc.orig_line, (int32_t)ctx.c.col, pc.str[0]);
   cpd.error_count++;
   return(true);
}


void tokenize(const deque<uint32_t> &data, chunk_t* ref)
{
   tok_ctx ctx(data);
   chunk_t  chunk;
   chunk_t* pc           = nullptr;
   chunk_t* rprev        = nullptr;
   bool     last_was_tab = false;
   uint32_t prev_sp      = 0;

   cpd.unc_stage = unc_stage_e::TOKENIZE;

   parse_frame_t frm;
   memset(&frm, 0, sizeof(frm));

   while (ctx.more())
   {
      chunk.reset();
      if (parse_next(ctx, chunk) == false)
      {
         LOG_FMT(LERR, "%s:%u Bailed before the end?\n",
                 cpd.filename, ctx.c.row);
         cpd.error_count++;
         break;
      }

      /* Don't create an entry for whitespace */
      if (chunk.type == CT_WHITESPACE)
      {
         last_was_tab = chunk.after_tab;
         prev_sp      = chunk.orig_prev_sp;
         continue;
      }
      chunk.orig_prev_sp = prev_sp;
      prev_sp            = 0;

      switch(chunk.type)
      {
         case(CT_NEWLINE): { last_was_tab = chunk.after_tab; chunk.after_tab = false; chunk.str.clear();  break; }
         case(CT_NL_CONT): { last_was_tab = chunk.after_tab; chunk.after_tab = false; chunk.str = "\\\n"; break; }
         default:          { chunk.after_tab = last_was_tab; last_was_tab    = false;                     break; }
      }

      /* Strip trailing whitespace (for CPP comments and PP blocks) */
      while ((chunk.str.size() > 0                        ) &&
             is_space_or_tab(chunk.str[chunk.str.size()-1]) )
      {
         /* If comment contains backslash '\' followed by whitespace chars, keep last one;
          * this will prevent it from turning '\' into line continuation. */
         break_if ((chunk.str.size()               > 1        ) &&
                   (chunk.str[chunk.str.size()-2] == BACKSLASH) );
         chunk.str.pop_back();
      }

      /* Store off the end column */
      chunk.orig_col_end = ctx.c.col;

      /* Add the chunk to the list */
      rprev = pc;
      if (is_valid(rprev))
      {
         set_flags(pc, get_flags(rprev, PCF_COPY_FLAGS));

         /* a newline can't be in a preprocessor */
         if (is_type(pc, CT_NEWLINE))
         {
            clr_flags(pc, PCF_IN_PREPROC);
         }
      }
      if (is_valid(ref)) { chunk.flags |=  PCF_INSERTED; }
      else               { chunk.flags &= ~PCF_INSERTED; }

      pc = chunk_add_before(&chunk, ref);
      assert(is_valid(pc));

      /* A newline marks the end of a preprocessor */
      if (is_type(pc, CT_NEWLINE)) // || is_type(pc, CT_COMMENT_MULTI))
      {
         cpd.is_preproc         = CT_NONE;
         cpd.preproc_ncnl_count = 0;
      }

      /* Special handling for preprocessor stuff */
      if (is_type(pc, CT_PP_ASM))
      {
         LOG_FMT(LBCTRL, "Found a directive %s on line %u\n", "#asm", pc->orig_line);
         cpd.unc_off = true;
      }

      /* Special handling for preprocessor stuff */
      if (cpd.is_preproc != CT_NONE)
      {
         set_flags(pc, PCF_IN_PREPROC);

         /* Count words after the preprocessor */
         if(!is_cmt_or_nl(pc))
         {
            cpd.preproc_ncnl_count++;
         }

         /* Figure out the type of preprocessor for #include parsing */
         if (cpd.is_preproc == CT_PP_PRAGMA)
         {
            if (memcmp(pc->text(), "asm", 3) == 0)
            {
               LOG_FMT(LBCTRL, "Found a pragma %s on line %u\n", "asm", pc->orig_line);
               cpd.unc_off = true;
            }
         }

         /* Figure out the type of preprocessor for #include parsing */
         if (cpd.is_preproc == CT_PREPROC)
         {
            if(is_no_preproc_type(pc))
            {
               set_type(pc, CT_PP_OTHER);
            }
            cpd.is_preproc = pc->type;
         }
         else if (cpd.is_preproc == CT_PP_IGNORE)
         {
            // ASSERT(is_true(UO_pp_ignore_define_body));
            if (not_type(pc, CT_NL_CONT, CT_COMMENT_CPP))
            {
               set_type(pc, CT_PP_IGNORE);
            }
         }
         else if (cpd.is_preproc == CT_PP_DEFINE    &&
                  is_type(pc, CT_PAREN_CLOSE      ) &&
                  is_true(UO_pp_ignore_define_body) )
         {
            // When we have a PAREN_CLOSE in a PP_DEFINE we should be terminating a MACRO_FUNC
            // arguments list. Therefore we can enter the PP_IGNORE state and ignore next chunks.
            cpd.is_preproc = CT_PP_IGNORE;
         }
      }
      else
      {
         /* Check for a preprocessor start */
         if (is_type(pc, CT_POUND) &&
             is_invalid_or_type(rprev, CT_NEWLINE) )
         {
            set_type_and_flag(pc, CT_PREPROC, PCF_IN_PREPROC);
            cpd.is_preproc = CT_PREPROC;
         }
      }
      if (is_type(pc, CT_NEWLINE))
      {
         LOG_FMT(LGUY, "%s(%d): (%u)<NL> col=%u\n",
                 __func__, __LINE__, pc->orig_line, pc->orig_col);
      }
      else
      {
         LOG_FMT(LGUY, "%s(%d): (%u)text():%s, type:%s, orig_col=%u, orig_col_end=%u\n",
                 __func__, __LINE__, pc->orig_line, pc->text(), get_token_name(pc->type), pc->orig_col, pc->orig_col_end);
      }
   }

   /* Set the cpd.newline string for this file */
   if ( (cpd.settings[UO_newlines].le == LE_LF  ) ||
       ((cpd.settings[UO_newlines].le == LE_AUTO) &&
        (cpd.le_counts[LE_LF] >= cpd.le_counts[LE_CRLF]) &&
        (cpd.le_counts[LE_LF] >= cpd.le_counts[LE_CR  ]) ) )
   {
      /* LF line ends */
      cpd.newline = "\n";
      LOG_FMT(LLINEENDS, "Using LF line endings\n");
   }
   else if ( (cpd.settings[UO_newlines].le == LE_CRLF) ||
            ((cpd.settings[UO_newlines].le == LE_AUTO)        &&
             (cpd.le_counts[LE_CRLF] >= cpd.le_counts[LE_LF]) &&
             (cpd.le_counts[LE_CRLF] >= cpd.le_counts[LE_CR]) ) )
   {
      /* CRLF line ends */
      cpd.newline = "\r\n";
      LOG_FMT(LLINEENDS, "Using CRLF line endings\n");
   }
   else
   {
      /* CR line ends */
      cpd.newline = "\r";
      LOG_FMT(LLINEENDS, "Using CR line endings\n");
   }
}


// /**
//  * A simplistic fixed-sized needle in the fixed-size haystack string search.
//  */
// int str_find(const char *needle, int needle_len,
//              const char *haystack, int haystack_len)
// {
//    for (int idx = 0; idx < (haystack_len - needle_len); idx++)
//    {
//       if (memcmp(needle, haystack + idx, needle_len) == 0)
//       {
//          return(idx);
//       }
//    }
//    return(-1);
// }
