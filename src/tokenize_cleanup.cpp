/**
 * @file tokenize_cleanup.cpp
 * Looks at simple sequences to refine the chunk types.
 * Examples:
 *  - change '[' + ']' into '[]'/
 *  - detect "version = 10;" vs "version (xxx) {"
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */
#include "tokenize_cleanup.h"
#include "char_table.h"
#include "chunk_list.h"
#include "combine.h"
#include "keywords.h"
#include "punctuators.h"
#include "space.h"
#include "uncrustify.h"
#include "uncrustify_types.h"
#include "unc_ctype.h"
#include <cstring>


/**
 * If there is nothing but CT_WORD and CT_MEMBER, then it's probably a
 * template thingy.  Otherwise, it's likely a comparison.
 */
static void check_template(
   chunk_t *start  /**< [in]  */
);


/**
 * Convert '>' + '>' into '>>'
 * If we only have a single '>', then change it to CT_COMPARE.
 */
static chunk_t *handle_double_angle_close(
   chunk_t *pc  /**< [in]  */
);


static chunk_t *handle_double_angle_close(chunk_t *pc)
{
   chunk_t *next = chunk_get_next(pc);

   if (is_valid(next))
   {
      if (is_type_and_ptype(pc,   CT_ANGLE_CLOSE, CT_NONE) &&
          is_type_and_ptype(next, CT_ANGLE_CLOSE, CT_NONE) &&
          ((pc->orig_col_end + 1) == next->orig_col) )
      {
         pc->str.append('>');
         set_type(pc, CT_ARITH);
         pc->orig_col_end = next->orig_col_end;

         chunk_t *tmp = get_next_ncnl(next);
         chunk_del(next);
         next = tmp;
      }
      else
      {
         set_type(pc, CT_COMPARE);
      }
   }
   return(next);
}


void split_off_angle_close(chunk_t *pc)
{
   const chunk_tag_t *ct = find_punctuator(pc->text() + 1, cpd.lang_flags);
   return_if(ptr_is_invalid(ct));

   chunk_t nc = *pc;
   pc->str.resize(1);
   pc->orig_col_end = pc->orig_col + 1;
   set_type(pc, CT_ANGLE_CLOSE);

   nc.type = ct->type;
   nc.str.pop_front();
   nc.orig_col++;
   nc.column++;
   chunk_add_after(&nc, pc);
}


void tokenize_cleanup(void)
{
   LOG_FUNC_ENTRY();

   cpd.unc_stage = unc_stage_e::TOKENIZE_CLEANUP;

   /* Since [] is expected to be TSQUARE for the 'operator', we need to make
    * this change in the first pass. */
   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      if (is_type(pc, CT_SQUARE_OPEN))
      {
         chunk_t *next = get_next_ncnl(pc);
         if (is_type(next, CT_SQUARE_CLOSE))
         {
            /* Change '[' + ']' into '[]' */
            set_type(pc, CT_TSQUARE);
            pc->str = "[]";

            // The original orig_col_end of CT_SQUARE_CLOSE is stored at orig_col_end of CT_TSQUARE.
            // pc->orig_col_end += 1;
            pc->orig_col_end = next->orig_col_end;
            chunk_del(next);
         }
      }
      if ( is_type(pc, CT_SEMICOLON) &&
           is_preproc(pc) &&
          !get_next_ncnl(pc, scope_e::PREPROC))
      {
         LOG_FMT(LNOTE, "%s:%zu Detected a macro that ends with a semicolon. Possible failures if used.\n",
                 cpd.filename, pc->orig_line);
      }
   }

   /* We can handle everything else in the second pass */
   chunk_t *pc   = chunk_get_head();
   chunk_t *next = get_next_ncnl(pc);
   while (are_valid(pc, next))
   {
      if ((is_type(pc, CT_DOT     ) && (cpd.lang_flags & LANG_ALLC)) ||
          (is_type(pc, CT_NULLCOND) && (cpd.lang_flags & LANG_CS  )) )
      {
         set_type(pc, CT_MEMBER);
      }

      /* Determine the version stuff (D only) */
      if (is_type(pc, CT_D_VERSION))
      {
         if (is_type(next, CT_PAREN_OPEN))
         {
            set_type(pc, CT_D_VERSION_IF);
         }
         else
         {
            if (not_type(next, CT_ASSIGN))
            {
               LOG_FMT(LERR, "%s:%zu %s: version: Unexpected token %s\n",
                       cpd.filename, pc->orig_line, __func__, get_token_name(next->type));
               cpd.error_count++;
            }
            set_type(pc, CT_WORD);
         }
      }

      /* Determine the scope stuff (D only) */
      if (is_type(pc, CT_D_SCOPE))
      {
         if (is_type(next, CT_PAREN_OPEN)) { set_type(pc, CT_D_SCOPE_IF); }
         else                                    { set_type(pc, CT_TYPE      ); }
      }

      /* Change CT_BASE before CT_PAREN_OPEN to CT_WORD.
       * public myclass() : base() { } */
      if (is_type(pc, CT_BASE) && is_type(next, CT_PAREN_OPEN)) { set_type(pc,   CT_WORD      ); }
      if (is_type(pc, CT_ENUM) && is_type(next, CT_CLASS     )) { set_type(next, CT_ENUM_CLASS); }

      /* Change CT_WORD after CT_ENUM, CT_UNION, or CT_STRUCT to CT_TYPE
       * Change CT_WORD before CT_WORD to CT_TYPE */
      if (is_type(next, CT_WORD))
      {
         if(is_type(pc, CT_ENUM, CT_ENUM_CLASS, CT_UNION, CT_STRUCT))
         {
            set_type(next, CT_TYPE);
         }
         if (is_type(pc, CT_WORD)) { set_type(pc, CT_TYPE); }
      }

      /* change extern to qualifier if extern isn't followed by a string or
       * an open parenthesis */
      if (is_type(pc, CT_EXTERN))
      {
         if (is_type(next, CT_STRING))
         {
            /* Probably 'extern "C"' */
         }
         else if (is_type(next, CT_PAREN_OPEN))
         {
            /* Probably 'extern (C)' */
         }
         else
         {
            /* Something else followed by a open brace */
            chunk_t *tmp = get_next_ncnl(next);
            if(not_type(tmp, CT_BRACE_OPEN))
            {
               set_type(pc, CT_QUALIFIER);
            }
         }
      }

      /* Change CT_STAR to CT_PTR_TYPE if preceded by CT_TYPE,
       * CT_QUALIFIER, or CT_PTR_TYPE. */
      if (is_type(next, CT_STAR                           ) &&
          is_type(pc,   CT_TYPE, CT_QUALIFIER, CT_PTR_TYPE) )
      {
         set_type(next, CT_PTR_TYPE);
      }

      static bool in_type_cast = false;
      if (are_types(pc, CT_TYPE_CAST, next, CT_ANGLE_OPEN))
      {
         set_ptype(next, CT_TYPE_CAST);
         in_type_cast = true;
      }

      /* Change angle open/close to CT_COMPARE, if not a template thingy */
      if (is_type(pc, CT_ANGLE_OPEN) &&
          (pc->ptype != CT_TYPE_CAST ) )
      {
         /* pretty much all languages except C use <> for something other than
          * comparisons.  "#include<xxx>" is handled elsewhere. */
         if (is_lang(cpd, LANG_CPPCSJOV))
         {
            check_template(pc);
         }
         else
         {
            /* convert CT_ANGLE_OPEN to CT_COMPARE */
            set_type(pc, CT_COMPARE);
         }
      }

#if 1
      if (is_type(pc, CT_ANGLE_CLOSE) &&
          (pc->ptype != CT_TEMPLATE ) )
#else
         // many Cpp and some other tests fail
      if (is_type  (pc, CT_ANGLE_CLOSE) &&
          not_ptype(pc, CT_TEMPLATE   ) )
#endif
      {
         if (in_type_cast)
         {
            in_type_cast = false;
            set_ptype(pc, CT_TYPE_CAST);
         }
         else
         {
            next = handle_double_angle_close(pc);
         }
      }

      assert(is_valid(next));

      static chunk_t *prev = nullptr;
      if (is_lang(cpd, LANG_D))
      {
         /* Check for the D string concat symbol '~' */
         if (  is_type(pc,   CT_INV            )   &&
              (is_type(prev, CT_STRING, CT_WORD) ||
               is_type(next, CT_STRING         ) ) )
         {
            set_type(pc, CT_CONCAT);
         }

         /* Check for the D template symbol '!' (word + '!' + word or '(') */
         if ( are_types(pc, CT_NOT, prev, CT_WORD          ) &&
              is_type(next, CT_WORD, CT_PAREN_OPEN, CT_TYPE) )
         {
            set_type(pc, CT_D_TEMPLATE);
         }

         /* handle "version(unittest) { }" vs "unittest { }" */
         if (are_types(pc, CT_UNITTEST, prev, CT_PAREN_OPEN))
         {
            set_type(pc, CT_WORD);
         }

         /* handle 'static if' and merge the tokens */
         if ( is_type(pc,   CT_IF      ) &&
              is_str (prev, "static", 6) )
         {
            /* delete PREV and merge with IF */
            pc->str.insert(0, ' ');
            pc->str.insert(0, prev->str);
            pc->orig_col  = prev->orig_col;
            pc->orig_line = prev->orig_line;
            chunk_t *to_be_deleted = prev;
            prev = chunk_get_prev_ncnl(prev);
            chunk_del(to_be_deleted);
         }
      }

      if (is_lang(cpd, LANG_CPP))
      {
         /* Change Word before '::' into a type */
         if (are_types(pc, CT_WORD, next, CT_DC_MEMBER))
         {
            set_type(pc, CT_TYPE);
         }
      }

      /* Change get/set to CT_WORD if not followed by a brace open */
      if (is_type    (pc,   CT_GETSET    ) &&
          not_type(next, CT_BRACE_OPEN) )
      {
         assert(is_valid(prev));
         if(is_type(next, CT_SEMICOLON                               ) &&
            is_type(prev, CT_SEMICOLON, CT_BRACE_CLOSE, CT_BRACE_OPEN) )
         {
            set_type (pc,   CT_GETSET_EMPTY);
            set_ptype(next, CT_GETSET      );
         }
         else
         {
            set_type(pc, CT_WORD);
         }
      }

      /* Interface is only a keyword in MS land if followed by 'class' or 'struct'
       * likewise, 'class' may be a member name in Java. */
      if (is_type(pc, CT_CLASS) &&
          !CharTable::IsKW1(next->str[0]) &&
          not_type(pc->next, CT_DC_MEMBER))
      {
         set_type(pc, CT_WORD);
      }

      /* Change item after operator (>=, ==, etc) to a CT_OPERATOR_VAL
       * Usually the next item is part of the operator.
       * In a few cases the next few tokens are part of it:
       *  operator +       - common case
       *  operator >>      - need to combine '>' and '>'
       *  operator ()
       *  operator []      - already converted to TSQUARE
       *  operator new []
       *  operator delete []
       *  operator const char *
       *  operator const B&
       *  operator std::allocator<U>
       *
       * In all cases except the last, this will put the entire operator value
       * in one chunk. */
      if (pc->type == CT_OPERATOR)
      {
         chunk_t *tmp2 = chunk_get_next(next);
         /* Handle special case of () operator -- [] already handled */
         if (next->type == CT_PAREN_OPEN)
         {
            chunk_t *tmp = chunk_get_next(next);
            if (is_type(tmp, CT_PAREN_CLOSE) )
            {
               next->str = "()";
               set_type(next, CT_OPERATOR_VAL);
               chunk_del(tmp);
               next->orig_col_end += 1;
            }
         }
         else if (are_types(next, tmp2, CT_ANGLE_CLOSE) &&
                  (tmp2->orig_col == next->orig_col_end) )
         {
            next->str.append('>');
            next->orig_col_end++;
            set_type(next, CT_OPERATOR_VAL);
            chunk_del(tmp2);
         }
         else if (is_flag(next, PCF_PUNCTUATOR))
         {
            set_type(next, CT_OPERATOR_VAL);
         }
         else
         {
            set_type(next, CT_TYPE);

            /* Replace next with a collection of all tokens that are part of
             * the type. */
            tmp2 = next;
            chunk_t *tmp;
            while ((tmp = chunk_get_next(tmp2)) != nullptr)
            {
               break_if(not_type(tmp, 7, CT_WORD, CT_AMP,  CT_TSQUARE,
                         CT_QUALIFIER, CT_TYPE, CT_STAR, CT_CARET));

               /* Change tmp into a type so that space_needed() works right */
               make_type(tmp);
               size_t num_sp = space_needed(tmp2, tmp);
               while (num_sp-- > 0)
               {
                  next->str.append(" ");
               }
               next->str.append(tmp->str);
               tmp2 = tmp;
            }

            while ((tmp2 = chunk_get_next(next)) != tmp)
            {
               chunk_del(tmp2);
            }

            set_type(next, CT_OPERATOR_VAL);
            next->orig_col_end = next->orig_col + next->len();
         }
         set_ptype(next, CT_OPERATOR);

         LOG_FMT(LOPERATOR, "%s: %zu:%zu operator '%s'\n",
                 __func__, pc->orig_line, pc->orig_col, next->text());
      }

      /* Change private, public, protected into either a qualifier or label */
      if (is_type(pc, CT_PRIVATE))
      {
         /* Handle Qt slots - maybe should just check for a CT_WORD? */
         if (is_str(next, "slots",   5) ||
             is_str(next, "Q_SLOTS", 7) )
         {
            chunk_t *tmp = chunk_get_next(next);
            if (is_type(tmp, CT_COLON) )
            {
               next = tmp;
            }
         }
         if (is_type(next, CT_COLON))
         {
            set_type(next, CT_PRIVATE_COLON);
            chunk_t *tmp;
            if ((tmp = get_next_ncnl(next)) != nullptr)
            {
               set_flags(tmp, PCF_STMT_START | PCF_EXPR_START);
            }
         }
         else
         {
            const c_token_t type = (is_str(pc, "signals",   7) ||
                                    is_str(pc, "Q_SIGNALS", 9) ) ?
                                     CT_WORD : CT_QUALIFIER;
            set_type(pc, type);
         }
      }

      /* Look for <newline> 'EXEC' 'SQL' */
      if ((is_str_case(pc,   "EXEC", 4) &&
           is_str_case(next, "SQL",  3) ) ||
          ((*pc->str.c_str() == '$')    &&
           not_type(pc, CT_SQL_WORD) ) )
      {
         chunk_t *tmp = chunk_get_prev(pc);
         if (is_nl(tmp))
         {
            if (*pc->str.c_str() == '$')
            {
               set_type(pc, CT_SQL_EXEC);
               if (pc->len() > 1)
               {
                  /* SPLIT OFF '$' */
                  chunk_t nc = *pc;
                  pc->str.resize(1);
                  pc->orig_col_end = pc->orig_col + 1;

                  nc.type = CT_SQL_WORD;
                  nc.str.pop_front();
                  nc.orig_col++;
                  nc.column++;
                  chunk_add_after(&nc, pc);

                  next = chunk_get_next(pc);
               }
            }
            tmp = chunk_get_next(next);
            if      (is_str_case(tmp, "BEGIN", 5)) { set_type(pc, CT_SQL_BEGIN); }
            else if (is_str_case(tmp, "END",   3)) { set_type(pc, CT_SQL_END  ); }
            else                                   { set_type(pc, CT_SQL_EXEC ); }

            /* Change words into CT_SQL_WORD until CT_SEMICOLON */
            while (is_valid(tmp))
            {
               break_if(is_type(tmp, CT_SEMICOLON));

               if ((tmp->len() > 0                      )   &&
                   (unc_isalpha(*tmp->str.c_str()       ) ||
                               (*tmp->str.c_str() == '$') ) )
               {
                  set_type(tmp, CT_SQL_WORD);
               }
               tmp = get_next_ncnl(tmp);
            }
         }
      }

      /* handle MS abomination 'for each' */
      if (is_type(pc,   CT_FOR   ) &&
          is_str (next, "each", 4) &&
          (next == chunk_get_next(pc)  ) )
      {
         assert(is_valid(next));
         /* merge the two with a space between */
         pc->str.append(' ');
         pc->str         += next->str;
         pc->orig_col_end = next->orig_col_end;
         chunk_del(next);
         next = get_next_ncnl(pc);
         /* label the 'in' */
         if (is_type(next, CT_PAREN_OPEN))
         {
            chunk_t *tmp = get_next_ncnl(next);
            while (not_type(tmp, CT_PAREN_CLOSE))
            {
               if (is_str(tmp, "in", 2))
               {
                  set_type(tmp, CT_IN);
                  break;
               }
               tmp = get_next_ncnl(tmp);
            }
         }
      }

      /* ObjectiveC allows keywords to be used as identifiers in some situations
       * This is a dirty hack to allow some of the more common situations. */
      if (is_lang(cpd, LANG_OC))
      {
         if( is_type(pc,   CT_IF, CT_FOR, CT_WHILE) &&
            !is_type(next, CT_PAREN_OPEN          ) )
         {
            set_type(pc, CT_WORD);
         }
         if ( is_type(pc,   CT_DO                               ) &&
             (any_is_type(prev, CT_MINUS, next, CT_SQUARE_CLOSE)) )
         {
            set_type(pc, CT_WORD);
         }
      }

      /* Another hack to clean up more keyword abuse */
      if ( is_type(pc,   CT_CLASS         ) &&
          (any_is_type(prev, next, CT_DOT)) )
      {
         set_type(pc, CT_WORD);
      }

      /* Detect Objective C class name */
      if(is_type(pc, CT_OC_IMPL, CT_OC_INTF, CT_OC_PROTOCOL))
      {
         assert(is_valid(next));
         if (not_type(next, CT_PAREN_OPEN))
         {
            set_type(next, CT_OC_CLASS);
         }
         set_ptype(next, pc->type);

         chunk_t *tmp = get_next_ncnl(next);
         if (is_valid(tmp))
         {
            set_flags(tmp, PCF_STMT_START | PCF_EXPR_START);
         }

         tmp = get_next_type(pc, CT_OC_END, (int)pc->level);
         if (is_valid(tmp))
         {
            set_ptype(tmp, pc->type);
         }
      }

      if (is_type(pc, CT_OC_INTF))
      {
         chunk_t *tmp = get_next_ncnl(pc, scope_e::PREPROC);
         while (not_type(tmp, CT_OC_END) )
         {
            if (get_token_pattern_class(tmp->type) != pattern_class_e::NONE)
            {
               LOG_FMT(LOBJCWORD, "@interface %zu:%zu change '%s' (%s) to CT_WORD\n",
                       pc->orig_line, pc->orig_col, tmp->text(),
                       get_token_name(tmp->type));
               set_type(tmp, CT_WORD);
            }
            tmp = get_next_ncnl(tmp, scope_e::PREPROC);
         }
      }

      /* Detect Objective-C categories and class extensions */
      /* @interface ClassName (CategoryName) */
      /* @implementation ClassName (CategoryName) */
      /* @interface ClassName () */
      /* @implementation ClassName () */
      if ((is_ptype(pc,   CT_OC_IMPL, CT_OC_INTF) ||
           is_type (pc,   CT_OC_CLASS           ) ) &&
           is_type (next, CT_PAREN_OPEN         ) )
      {
         set_ptype(next, pc->ptype);

         chunk_t *tmp = chunk_get_next(next);
         if (are_valid(tmp ,tmp->next))
         {
            if (is_type(tmp, CT_PAREN_CLOSE))
            {
               //set_chunk_type(tmp, CT_OC_CLASS_EXT);
               set_ptype(tmp, pc->ptype);
            }
            else
            {
               set_type_and_ptype(tmp, CT_OC_CATEGORY, pc->ptype);
            }
         }

         tmp = get_next_type(pc, CT_PAREN_CLOSE, (int)pc->level);
         set_ptype(tmp, pc->ptype);
      }

      /* Detect Objective C @property
       *  @property NSString *stringProperty;
       *  @property(nonatomic, retain) NSMutableDictionary *shareWith; */
      if (is_type(pc, CT_OC_PROPERTY))
      {
         assert(is_valid(next));
         if (not_type(next, CT_PAREN_OPEN))
         {
            set_flags(next, PCF_STMT_START | PCF_EXPR_START);
         }
         else
         {
            set_ptype(next, pc->type);

            chunk_t *tmp = get_next_type(pc, CT_PAREN_CLOSE, (int)pc->level);
            if (is_valid(tmp))
            {
               set_ptype(tmp, pc->type);
               tmp = get_next_ncnl(tmp);
               if (is_valid(tmp))
               {
                  set_flags(tmp, PCF_STMT_START | PCF_EXPR_START);

                  tmp = get_next_type(tmp, CT_SEMICOLON, (int)pc->level);
                  if (is_valid(tmp))
                  {
                     set_ptype(tmp, pc->type);
                  }
               }
            }
         }
      }

      /* Detect Objective C @selector
       *  @selector(msgNameWithNoArg)
       *  @selector(msgNameWith1Arg:)
       *  @selector(msgNameWith2Args:arg2Name:) */
      if ( are_types(pc, CT_OC_SEL, next, CT_PAREN_OPEN))
      {
         set_ptype(next, pc->type);

         chunk_t *tmp = chunk_get_next(next);
         if (is_valid(tmp))
         {
            set_type_and_ptype(tmp, CT_OC_SEL_NAME, pc->type);

            while ((tmp = get_next_ncnl(tmp)) != nullptr)
            {
               if (is_type(tmp, CT_PAREN_CLOSE))
               {
                  set_ptype(tmp, CT_OC_SEL);
                  break;
               }
               set_type_and_ptype(tmp, CT_OC_SEL_NAME, pc->type);
            }
         }
      }

      /* Handle special preprocessor junk */
      if (is_type(pc, CT_PREPROC))
      {
         assert(is_valid(next));
         set_ptype(pc, next->type);
      }

      /* Detect "pragma region" and "pragma endregion" */
      if (is_type(pc, CT_PP_PRAGMA))
      {
         assert(is_valid(next));
         if(is_type(next, CT_PREPROC_BODY))
         {
            const char*  str = next->str.c_str();
            if ((strncmp(str, "region",    6) == 0) ||
                (strncmp(str, "endregion", 9) == 0) )
            {
               set_type(pc, (*next->str.c_str() == 'r') ?
                     CT_PP_REGION : CT_PP_ENDREGION);

               set_ptype(prev, pc->type);
            }
         }
      }

      /* Check for C# nullable types '?' is in next */
      if (is_lang(cpd, LANG_CS ) &&
           is_type(next, CT_QUESTION) &&
          (next->orig_col == (pc->orig_col + pc->len())))
      {
         chunk_t *tmp = get_next_ncnl(next);
         if (is_valid(tmp))
         {
            bool do_it = (is_type(tmp, CT_PAREN_CLOSE, CT_ANGLE_CLOSE));

            if (is_type(tmp, CT_WORD))
            {
               chunk_t *tmp2 = get_next_ncnl(tmp);
               if (is_type(tmp2, CT_SEMICOLON,  CT_ASSIGN,
                                 CT_BRACE_OPEN, CT_COMMA) )
               {
                  do_it = true;
               }
            }

            if (do_it == true)
            {
               assert(is_valid(next));
               pc->str         += next->str;
               pc->orig_col_end = next->orig_col_end;
               chunk_del(next);
               next = tmp;
            }
         }
      }

      /* Change 'default(' into a sizeof-like statement */
      if (is_lang(cpd, LANG_CS  ) &&
          are_types(pc, CT_DEFAULT, next, CT_PAREN_OPEN))
      {
         set_type(pc, CT_SIZEOF);
      }

      if (is_type (pc,   CT_UNSAFE    ) &&
          not_type(next, CT_BRACE_OPEN) )
      {
         set_type(pc, CT_QUALIFIER);
      }

#if 1
      if ((is_type(pc, CT_USING                           ) ||
          (is_type(pc, CT_TRY) && is_lang(cpd, LANG_JAVA))) &&
           is_type(next, CT_PAREN_OPEN)                   )
#else
      // makes test 12101 fail
      if (is_type(pc, CT_USING, CT_TRY) &&
            is_lang(cpd, LANG_JAVA) &&
           is_type(next, CT_PAREN_OPEN) )
#endif
      {
         set_type(pc, CT_USING_STMT);
      }

      /* Add minimal support for C++0x rvalue references */
      if (is_type(pc, CT_BOOL) &&
          is_str (pc, "&&", 2) )
      {
         assert(is_valid(prev));
         if (is_type(prev, CT_TYPE))
         {
            set_type(pc, CT_BYREF);
         }
      }

      /* HACK: treat try followed by a colon as a qualifier to handle this:
       *   A::A(int) try : B() { } catch (...) { } */
      if ( is_type(pc,   CT_TRY  ) &&
           is_str (pc,   "try", 3) &&
           is_type(next, CT_COLON) )
      {
         set_type(pc, CT_QUALIFIER);
      }

      /* If Java's 'synchronized' is in a method declaration, it should be
       * a qualifier. */
      if (is_lang(cpd, LANG_JAVA   ) &&
          is_type (pc,   CT_SYNCHRONIZED) &&
          not_type(next, CT_PAREN_OPEN  ) )
      {
         set_type(pc, CT_QUALIFIER);
      }

      /* change CT_DC_MEMBER + CT_FOR into CT_DC_MEMBER + CT_FUNC_CALL */
      if (are_types(pc, CT_FOR, pc->prev, CT_DC_MEMBER))
      {
         set_type(pc, CT_FUNC_CALL);
      }
      /* TODO: determine other stuff here */

      prev = pc;
      pc   = next;
      next = get_next_ncnl(pc);
   }
}


static void check_template(chunk_t *start)
{
   LOG_FMT(LTEMPL, "%s: Line %zu, col %zu:", __func__, start->orig_line, start->orig_col);

   chunk_t *prev = chunk_get_prev_ncnl(start, scope_e::PREPROC);
   return_if(is_invalid(prev));

   chunk_t *end;
   chunk_t *pc;
   if (is_type(prev, CT_TEMPLATE))
   {
      LOG_FMT(LTEMPL, " CT_TEMPLATE:");

      /* We have: "template< ... >", which is a template declaration */
      size_t level = 1;
      for (pc = get_next_ncnl(start, scope_e::PREPROC);
           is_valid(pc);
           pc = get_next_ncnl(pc,    scope_e::PREPROC))
      {
         LOG_FMT(LTEMPL, " [%s,%zu]", get_token_name(pc->type), level);

         if ((pc->str[0] == '>') && (pc->len() > 1))
         {
            LOG_FMT(LTEMPL, " {split '%s' at %zu:%zu}",
                    pc->text(), pc->orig_line, pc->orig_col);
            split_off_angle_close(pc);
         }

         if (is_str(pc, "<", 1))
         {
            level++;
         }
         else if (is_str(pc, ">", 1))
         {
            level--;
            break_if(level == 0);
         }
      }
      end = pc;
   }
   else
   {
      /* We may have something like "a< ... >", which is a template where
       * '...' may consist of anything except braces {}, a semicolon, and
       * unbalanced parens.
       * if we are inside an 'if' statement and hit a CT_BOOL, then it isn't a
       * template. */

      /* A template requires a word/type right before the open angle */
#if 1
      if ((prev->type        != CT_WORD        ) &&
          (prev->type        != CT_TYPE        ) &&
          (prev->type        != CT_COMMA       ) &&
          (prev->type        != CT_OPERATOR_VAL) &&
          (prev->ptype != CT_OPERATOR    ) )
#else
        // test 31001 fails
      if (not_type (prev, 4, CT_WORD, CT_TYPE, CT_COMMA, CT_OPERATOR_VAL) &&
          chunk_is_not_ptype(prev,    CT_OPERATOR                                ) )
#endif
      {
         LOG_FMT(LTEMPL, " - after %s + ( - Not a template\n", get_token_name(prev->type));
         set_type(start, CT_COMPARE);
         return;
      }

      LOG_FMT(LTEMPL, " - prev %s -", get_token_name(prev->type));

      /* Scan back and make sure we aren't inside square parens */
      bool in_if = false;
      pc = start;
      while ((pc = chunk_get_prev_ncnl(pc, scope_e::PREPROC)) != nullptr)
      {
         break_if (is_type(pc, CT_SEMICOLON,   CT_BRACE_OPEN,
                               CT_BRACE_CLOSE, CT_SQUARE_CLOSE));

         if (is_type(pc, CT_IF, CT_RETURN))
         {
            in_if = true;
            break;
         }
         if (is_type(pc, CT_SQUARE_OPEN))
         {
            LOG_FMT(LTEMPL, " - Not a template: after a square open\n");
            set_type(start, CT_COMPARE);
            return;
         }
      }

      /* Scan forward to the angle close
       * If we have a comparison in there, then it can't be a template. */
#define MAX_NUMBER_OF_TOKEN    1024
      c_token_t tokens[MAX_NUMBER_OF_TOKEN];
      size_t    num_tokens = 1;

      tokens[0] = CT_ANGLE_OPEN;
      for (pc = get_next_ncnl(start, scope_e::PREPROC);
           is_valid(pc);
           pc = get_next_ncnl(pc, scope_e::PREPROC))
      {
         LOG_FMT(LTEMPL, " [%s,%zu]", get_token_name(pc->type), num_tokens);

         if ((tokens[num_tokens - 1] == CT_ANGLE_OPEN) &&
             (pc->str[0] == '>') &&
             (pc->len() > 1    ) &&
             (cpd.settings[UO_tok_split_gte].b ||
             (is_str(pc, ">>", 2) &&
             (num_tokens >= 2   ) ) ) )
         {
            LOG_FMT(LTEMPL, " {split '%s' at %zu:%zu}",
                    pc->text(), pc->orig_line, pc->orig_col);
            split_off_angle_close(pc);
         }

         if (is_str(pc, "<", 1))
         {
            tokens[num_tokens] = CT_ANGLE_OPEN;
            num_tokens++;
         }
         else if (is_str(pc, ">", 1))
         {
            if ((num_tokens             >  0            ) &&
                (tokens[num_tokens - 1] == CT_PAREN_OPEN) )
            {
               handle_double_angle_close(pc);
            }
            else if (--num_tokens == 0)
            {
               break;
            }
            else if (tokens[num_tokens] != CT_ANGLE_OPEN)
            {
               break; /* unbalanced parentheses */
            }
         }
         else if (in_if && is_type(pc, CT_BOOL, CT_COMPARE))
         {
            break;
         }
         else if (is_type(pc, CT_BRACE_OPEN, CT_BRACE_CLOSE, CT_SEMICOLON))
         {
            break;
         }
         else if (is_type(pc, CT_PAREN_OPEN))
         {
            if (num_tokens >= MAX_NUMBER_OF_TOKEN - 1)
            {
               break;
            }
            tokens[num_tokens] = CT_PAREN_OPEN;
            num_tokens++;
         }
         else if (is_type(pc, CT_PAREN_CLOSE))
         {
            num_tokens--;
            if (tokens[num_tokens] != CT_PAREN_OPEN)
            {
               /* unbalanced parentheses */
               break;
            }
         }
      }
      end = pc;
   }

   if (is_type(end, CT_ANGLE_CLOSE))
   {
      pc = get_next_ncnl(end, scope_e::PREPROC);
      if (is_invalid_or_not_type(pc, CT_NUMBER))
      {
         LOG_FMT(LTEMPL, " - Template Detected\n");

         set_ptype(start, CT_TEMPLATE);

         pc = start;
         while (pc != end)
         {
            chunk_t *next = get_next_ncnl(pc, scope_e::PREPROC);
            set_flags(pc, PCF_IN_TEMPLATE);
            if (not_type(next, CT_PAREN_OPEN) )
            {
               make_type(pc);
            }
            pc = next;
         }
         set_ptype_and_flag(end, CT_TEMPLATE, PCF_IN_TEMPLATE);
         return;
      }
   }

   LOG_FMT(LTEMPL, " - Not a template: end = %s\n",
           (is_valid(end)) ? get_token_name(end->type) : "<null>");
   set_type(start, CT_COMPARE);
}
