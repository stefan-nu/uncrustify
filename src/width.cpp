/**
 * @file width.cpp
 * Limits line width.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "width.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "uncrustify.h"
#include "indent.h"
#include "newlines.h"
#include <cstdlib>


/**
 * abbreviations used:
 * - fparen = function parenthesis
 */

/** tbd */
struct cw_entry
{
   chunk_t* pc;  /**<  */
   uint32_t pri; /**<  */
};


/** tbd */
struct token_pri
{
   c_token_t tok; /**<  */
   uint32_t  pri; /**<  */
};


/**
 * tbd
 */
static inline bool is_past_width(
   chunk_t* pc /**< in]  */
);


/**
 * Split right after the chunk
 */
static void split_before_chunk(
   chunk_t* pc /**< in]  */
);


/**
 * tbd
 */
static uint32_t get_split_pri(
   c_token_t tok /**< in]  */
);


/**
 * Checks to see if pc is a better spot to split.
 * This should only be called going BACKWARDS (i.e. prev)
 * A lower level wins
 *
 * Splitting Preference:
 *  - semicolon
 *  - comma
 *  - boolean op
 *  - comparison
 *  - arithmetic op
 *  - assignment
 *  - concatenated strings
 *  - ? :
 *  - function open paren not followed by close paren
 */
static void try_split_here(
   cw_entry& ent, /**< in]  */
   chunk_t*  pc   /**< in]  */
);


/**
 * Scan backwards to find the most appropriate spot to split the line
 * and insert a newline.
 *
 * See if this needs special function handling.
 * Scan backwards and find the best token for the split.
 *
 * @param start The first chunk that exceeded the limit
 */
static bool split_line(
   chunk_t* pc /**< in]  */
);


/**
 * Figures out where to split a function def/proto/call
 *
 * For function prototypes and definition. Also function calls where
 * level == brace_level:
 *   - find the open function parenthesis
 *     + if it doesn't have a newline right after it
 *       * see if all parameters will fit individually after the paren
 *       * if not, throw a newline after the open paren & return
 *   - scan backwards to the open function parenthesis or comma
 *     + if there isn't a newline after that item, add one & return
 *     + otherwise, add a newline before the start token
 *
 * @param start   the offending token
 * @return        the token that should have a newline
 *                inserted before it
 */
static void split_fcn_params(
   chunk_t* start /**< in]  */
);


/**
 * Splits the parameters at every comma that is at the
 * function parenthesis level.
 *
 * @param start   the offending token
 */
static void split_fcn_params_full(
   chunk_t* start /**< in]  */
);


/**
 * \brief split a for statement in several lines
 */
static void split_for_statement(
   chunk_t* start /**< in] any chunk inside the for statement */
);



/** \brief search for semicolons in a for statement */
void find_semicolons(
   chunk_t*       start,   /**< [in]  chunk to start search */
   chunk_t**      semi,    /**< [out] array to store found semicolons  */
   uint32_t*      count,   /**< [in]  number of semicolons found so far */
   const uint32_t max_cnt, /**< [in]  maximal required number of semicolons */
   const dir_e    dir      /**< [in]  search direction */
);


/** priorities of the different tokens
 *
 * low   numbers mean high priority
 * large numbers mean low  priority */
static const token_pri pri_table[] =
{
   { CT_SEMICOLON,    1 },
   { CT_COMMA,        2 },
   { CT_BOOL,         3 },
   { CT_COMPARE,      4 },
   { CT_ARITH,        5 },
   { CT_CARET,        6 },
   { CT_ASSIGN,       7 },
   { CT_STRING,       8 },
   { CT_FOR_COLON,    9 },
 //{ CT_DC_MEMBER,   10 },
 //{ CT_MEMBER,      10 },
   { CT_QUESTION,    20 }, // allow break in ? : for ls_code_width
   { CT_COND_COLON,  20 },
   { CT_FPAREN_OPEN, 21 }, // break after function open paren not followed by close paren
   { CT_QUALIFIER,   25 },
   { CT_CLASS,       25 },
   { CT_STRUCT,      25 },
   { CT_TYPE,        25 },
   { CT_TYPENAME,    25 },
   { CT_VOLATILE,    25 },
};


static inline bool is_past_width(chunk_t* pc)
{
   assert(is_valid(pc));
   /* allow char to sit at last column by subtracting 1 */
   return((pc->column + pc->len() - 1u) > get_uval(UO_code_width));
}


static void split_before_chunk(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(pc));
   LOG_FMT(LSPLIT, "%s: %s\n", __func__, pc->text());

   if (!is_nl(pc                ) &&
       !is_nl(chunk_get_prev(pc)) )
   {
      newline_add_before(pc);
      /* reindent needs to include the indent_continue value and was off by one */
      reindent_line(pc, pc->brace_level * get_uval(UO_indent_columns) +
                    (uint32_t)abs(get_ival(UO_indent_continue)) + 1u);
      cpd.changes++;
   }
}


void do_code_width(void)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPLIT, "%s\n", __func__);

   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if ((!is_cmt_or_nl(pc)) &&
          (not_type(pc, CT_SPACE) ) &&
          (is_past_width(pc)) )
      {
         if (split_line(pc) == false)
         {
            LOG_FMT(LSPLIT, "%s: Bailed on %u:%u %s\n",
                    __func__, pc->orig_line, pc->orig_col, pc->text());
            break;
         }
      }
   }
}


static uint32_t get_split_pri(c_token_t tok)
{
   for (auto token : pri_table)
   {
      retval_if(token.tok == tok, token.pri);
   }
   return(0);
}


static void try_split_here(cw_entry& ent, chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   uint32_t pc_pri = get_split_pri(pc->type);
   return_if(pc_pri == 0);

   /* Can't split after a newline */
   chunk_t *prev = chunk_get_prev(pc);
   return_if(is_invalid(prev) || (is_nl(prev) && not_type(pc, CT_STRING)) );

   /* Can't split a function without arguments */
   if (is_type(pc, CT_FPAREN_OPEN))
   {
      chunk_t *next = chunk_get_next(pc);
      return_if(is_type(next, CT_FPAREN_CLOSE));
   }

   /* Only split concatenated strings */
   if (is_type(pc, CT_STRING))
   {
      chunk_t *next = chunk_get_next(pc);
      return_if(not_type(next, CT_STRING));
   }

   /* keep common groupings unless ls_code_width */
   return_if((is_false(UO_ls_code_width)) &&
             (pc_pri                           >= 20   ) );

   /* don't break after last term of a qualified type */
   if (pc_pri == 25)
   {
      chunk_t* next = chunk_get_next(pc);
      return_if(not_type(next, CT_WORD) &&
                (get_split_pri(next->type) != 25) );
   }

   /* Check levels first */
   bool change = false;
   if (is_invalid(ent.pc) || (pc->level <  ent.pc->level))
   {
      change = true;
   }
   else
   {
      if ((pc->level >= ent.pc->level) &&
          (pc_pri    <  ent.pri      ) )
      {
         change = true;
      }
   }

   if (change == true)
   {
      ent.pc  = pc;
      ent.pri = pc_pri;
   }
}


static bool split_line(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPLIT, "%s: line %u, col %u token: '%s' [%s] (IN_FUNC=%d) ",
           __func__, start->orig_line, start->column, start->text(),
           get_token_name(start->type),
           is_flag(start, (PCF_IN_FCN_DEF | PCF_IN_FCN_CALL)));
#ifdef DEBUG
   LOG_FMT(LSPLIT, "\n");
#endif

   /* break at maximum line length if ls_code_width is true */
   if (is_flag(start, PCF_ONE_LINER))
   {
      LOG_FMT(LSPLIT, " ** ONCE LINER SPLIT **\n");
      undo_one_liner(start);
      cleanup_braces(false);
      return(false);
   }

   if (is_true(UO_ls_code_width)) {  }

   /* Check to see if we are in a for statement */
   else if (is_flag(start, PCF_IN_FOR))
   {
      LOG_FMT(LSPLIT, " ** FOR SPLIT **\n");
      split_for_statement(start);
      retval_if(!is_past_width(start), true);

      LOG_FMT(LSPLIT, "%s: for split didn't work\n", __func__);
   }

   /* If this is in a function call or prototype, split on commas or right
    * after the open parenthesis */
   else if ( is_flag (start, PCF_IN_FCN_DEF) ||
            (is_level(start, (start->brace_level + 1)) &&
             is_flag (start, PCF_IN_FCN_CALL)))
   {
      LOG_FMT(LSPLIT, " ** FUNC SPLIT **\n");

      if (is_true(UO_ls_func_split_full))
      {
         split_fcn_params_full(start);
         retval_if(!is_past_width(start), true);
      }
      split_fcn_params(start);
      return(true);
   }

   /* Try to find the best spot to split the line */
   cw_entry ent;
   memset(&ent, 0, sizeof(ent));
   chunk_t* pc = start;
   chunk_t* prev;

   while (((pc = chunk_get_prev(pc)) != nullptr) &&
           (!is_nl(pc)) )
   {
      LOG_FMT(LSPLIT, "%s: at %s, col=%u\n", __func__, pc->text(), pc->orig_col);
      if (not_type(pc, CT_SPACE))
      {
         try_split_here(ent, pc);
         /*  break at maximum line length */
         break_if(is_valid(ent.pc) && is_true(UO_ls_code_width));
      }
   }

   if (is_invalid(ent.pc))
   {
      LOG_FMT(LSPLIT, "\n%s:    TRY_SPLIT yielded NO SOLUTION for line %u at %s [%s]\n",
              __func__, start->orig_line, start->text(), get_token_name(start->type));
   }
   else
   {
      LOG_FMT(LSPLIT, "\n%s:    TRY_SPLIT yielded '%s' [%s] on line %u\n",
              __func__, ent.pc->text(), get_token_name(ent.pc->type), ent.pc->orig_line);
      LOG_FMT(LSPLIT, "%s: ent at %s, col=%u\n",
              __func__, ent.pc->text(), ent.pc->orig_col);
   }

   /* Break before the token instead of after it according to the pos_xxx rules */
   if (is_invalid(ent.pc)) { pc = nullptr; }
   else
   {
      if(( (is_type(ent.pc, CT_ARITH, CT_CARET        ) ) && is_token_set(cpd.settings[UO_pos_arith      ].tp, TP_LEAD) ) ||
         ( (is_type(ent.pc, CT_ASSIGN                 ) ) && is_token_set(cpd.settings[UO_pos_assign     ].tp, TP_LEAD) ) ||
         ( (is_type(ent.pc, CT_COMPARE                ) ) && is_token_set(cpd.settings[UO_pos_compare    ].tp, TP_LEAD) ) ||
         ( (is_type(ent.pc, CT_COND_COLON, CT_QUESTION) ) && is_token_set(cpd.settings[UO_pos_conditional].tp, TP_LEAD) ) ||
         ( (is_type(ent.pc, CT_BOOL                   ) ) && is_token_set(cpd.settings[UO_pos_bool       ].tp, TP_LEAD) ) )
      {
         pc = ent.pc;
      }
      else { pc = chunk_get_next(ent.pc); }

      assert(is_valid(pc));
      LOG_FMT(LSPLIT, "%s: at %s, col=%u\n", __func__, pc->text(), pc->orig_col);
   }

   if (is_invalid(pc))
   {
      pc = start;
      /* Don't break before a close, comma, or colon */
      if(is_type(start, 11, CT_COMMA,     CT_PAREN_CLOSE, CT_PAREN_OPEN,
         CT_FPAREN_OPEN, CT_SPAREN_CLOSE, CT_VSEMICOLON,  CT_FPAREN_CLOSE,
         CT_BRACE_CLOSE, CT_SPAREN_OPEN,  CT_SEMICOLON,   CT_ANGLE_CLOSE) ||
          (start->len() == 0) )
      {
         LOG_FMT(LSPLIT, " ** NO GO **\n");

         /* TODO: Add in logic to handle 'hard' limits by backing up a token */
         return(true);
      }
   }

   /* add a newline before pc */
   prev = chunk_get_prev(pc);
   if (!is_nl(pc) && !is_nl(prev) )
   {
      //int plen = (pc->len() < 5) ? pc->len() : 5;
      //int slen = (start->len() < 5) ? start->len() : 5;
      //LOG_FMT(LSPLIT, " '%.*s' [%s], started on token '%.*s' [%s]\n",
      //        plen, pc->text(), get_token_name(pc->type),
      //        slen, start->text(), get_token_name(start->type));
      LOG_FMT(LSPLIT, "  %s [%s], started on token '%s' [%s]\n",
              pc->text(),    get_token_name(pc->type),
              start->text(), get_token_name(start->type));

      split_before_chunk(pc);
   }
   return(true);
}


void find_semicolons(chunk_t* start, chunk_t** semi, uint32_t* count,
                     const uint32_t max_cnt, const dir_e dir)
{
   chunk_t* pc = start;
   do
   {
      if (is_type_and_ptype(pc, CT_SEMICOLON, CT_FOR))
      {
         semi[*count] = pc;
         (*count)++;
      }
   }
   while ( (*count < max_cnt) &&
           ((pc = chunk_get(pc, scope_e::ALL, dir)) != nullptr) &&
            get_flags(pc, PCF_IN_SPAREN) );
}


/* The for statement split algorithm works as follows:
 * 1. Step backwards and forwards to find the semicolons
 * 2. Try splitting at the semicolons first.
 * 3. If that doesn't work, then look for a comma at paren level.
 * 4. If that doesn't work, then look for an assignment at paren level.
 * 5. If that doesn't work, then give up. */
static void split_for_statement(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(start));

   LOG_FMT(LSPLIT, "%s: starting on %s, line %u\n",
           __func__, start->text(), start->orig_line);

   chunk_t* open_paren = nullptr;
   uint32_t nl_cnt     = 0;

   /* Find the open paren so we know the level and count newlines */
   chunk_t *pc = start;
   while ((pc = chunk_get_prev(pc)) != nullptr)
   {
      if (is_type(pc, CT_SPAREN_OPEN))
      {
         open_paren = pc;
         break;
      }
      nl_cnt += pc->nl_count;
   }
   if (is_invalid(open_paren))
   {
      LOG_FMT(LSPLIT, "No open parenthesis found, cannot split for() \n");
      return;
   }

   /* how many semicolons (1 or 2) do we need to find */
   uint32_t max_cnt = is_true(UO_ls_for_split_full) ? 2 : 1;

   /* scan for the semicolons */
   uint32_t count = 0;
   chunk_t* st[2];
   st[0] = nullptr;
   st[1] = nullptr;
   /* \todo why not search forward from the open_parenthesis onward ? */
   find_semicolons(start, &st[0], &count, max_cnt, dir_e::BEFORE);
   find_semicolons(start, &st[0], &count, max_cnt, dir_e::AFTER );

   while (count != 0)
   {
      count--;
      if(is_valid(st[count]))
      {
         LOG_FMT(LSPLIT, "%s: split before %s\n", __func__, st[count]->text());
         split_before_chunk(chunk_get_next(st[count]));
      }
   }

   return_if(!is_past_width(start) || (nl_cnt > 0));

   /* Still past width, check for commas at parenthese level */
   pc = open_paren;
   while ((pc = chunk_get_next(pc)) != start)
   {
      if (is_type_and_level(pc, CT_COMMA, (open_paren->level + 1)))
      {
         split_before_chunk(chunk_get_next(pc));
         return_if(!is_past_width(pc));
      }
   }

   /* Still past width, check for a assignments at parenthese level */
   pc = open_paren;
   while ((pc = chunk_get_next(pc)) != start)
   {
      if (is_type_and_level(pc, CT_ASSIGN, (open_paren->level + 1)))
      {
         split_before_chunk(chunk_get_next(pc));
         return_if(!is_past_width(pc));
      }
   }
}


#if 0
/*
 * scan through the chunk list and count the number
 * of chunks of a given type
 */
void scan_for_type(chunk_t* pc, int32_t* count, uint32_t  max_cnt, dir_e dir)
{
   const search_t search_function = select_search_fct(dir);

   do
   {
      if (is_type_and_ptype(pc, CT_SEMICOLON, CT_FOR))
      {
         st[count] = pc;
         count++;
      }
   }
   while ( (count < (int32_t)max_cnt                ) &&
           ((pc = chunk_get_prev(pc)) != nullptr) &&
           is_flag(pc, PCF_IN_SPAREN           ) );
}
#endif


static void split_fcn_params_full(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPLIT, "%s", __func__);

   /* Find the opening function parenthesis */
   chunk_t* fpopen = start;
   while ((fpopen = chunk_get_prev(fpopen)) != nullptr)
   {
#ifdef DEBUG
      LOG_FMT(LSPLIT, "%s: %s, Col=%zu, Level=%zu\n",
              __func__, fpopen->text(), fpopen->orig_col, fpopen->level);
#endif
      if ((fpopen->type  == CT_FPAREN_OPEN) &&
          (fpopen->level == start->level - 1))
      {
         break; /* opening parenthesis found. Issue #1020 */
      }
   }

   /* Now break after every comma */
   chunk_t* pc = fpopen;
   while ((pc = get_next_ncnl(pc)) != nullptr)
   {
      break_if(pc->level <= fpopen->level);

      if (is_type_and_level(pc, CT_COMMA, (fpopen->level + 1)))
      {
         split_before_chunk(chunk_get_next(pc));
      }
   }
}


static void split_fcn_params(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPLIT, "  %s: %s", __func__, start->text());
#ifdef DEBUG
   LOG_FMT(LSPLIT, "\n");
#endif

   /* Find the opening function parenthesis */
   chunk_t* fpopen = get_prev_fparen_open(start);
   assert(is_valid(fpopen));

   chunk_t* pc = get_next_ncnl(fpopen);
   assert(is_valid(pc));

   uint32_t  min_col = pc->column;

   LOG_FMT(LSPLIT, " mincol=%u, max_width=%u ",
           min_col, get_uval(UO_code_width) - min_col);

   int32_t cur_width =  0;
   int32_t last_col  = -1;
   while (is_valid(pc))
   {
      if (is_nl(pc))
      {
         cur_width =  0;
         last_col  = -1;
      }
      else
      {
         last_col   = max(last_col, (int32_t)pc->column);
         cur_width += (int32_t) pc->column + (int32_t)pc->len() - last_col;
         last_col   = (int32_t)(pc->column +          pc->len());

         if(is_type(pc, CT_COMMA, CT_FPAREN_CLOSE))
         {
            cur_width--;
            LOG_FMT(LSPLIT, " width=%d ", cur_width);
            break_if (((last_col - 1) > static_cast<int32_t>(get_uval(UO_code_width))) ||
                  is_type(pc, CT_FPAREN_CLOSE));
         }
      }
      pc = chunk_get_next(pc);
   }

   /* back up until the prev is a comma */
   chunk_t* prev = pc;
   while ((prev = chunk_get_prev(prev)) != nullptr)
   {
      break_if(is_type(prev, CT_COMMA, CT_NEWLINE, CT_NL_CONT));

      assert(is_valid(pc));
      last_col -= (int32_t)pc->len();
      if (is_type(prev, CT_FPAREN_OPEN))
      {
         pc = chunk_get_next(prev);
         assert(is_valid(pc));
         if (is_false(UO_indent_paren_nl))
         {
            min_col = pc->brace_level * get_uval(UO_indent_columns) + 1u;
            if (get_ival(UO_indent_continue) == 0)
            {
               min_col += get_uval(UO_indent_columns);
            }
            else
            {
               min_col += (uint32_t)abs(get_ival(UO_indent_continue));
            }
         }

         /* Don't split "()" */
         break_if((int32_t)pc->type != ((int32_t)prev->type + 1)); /* \todo make clearer */
      }
   }

   if (is_valid(prev) && !is_nl(prev))
   {
      LOG_FMT(LSPLIT, " -- ended on [%s] --\n", get_token_name(prev->type));
      pc = chunk_get_next(prev);
      newline_add_before(pc);
      reindent_line(pc, min_col);
      cpd.changes++;
   }
}
