/**
 * @file newlines.cpp
 * Adds or removes newlines.
 *
 * Informations
 *   "Ignore" means do not change it.
 *   "Add" in the context of spaces means make sure there is at least 1.
 *   "Add" elsewhere means make sure one is present.
 *   "Remove" mean remove the space/brace/newline/etc.
 *   "Force" in the context of spaces means ensure that there is exactly 1.
 *   "Force" in other contexts means the same as "add".
 *
 *   Rmk: spaces = space + nl
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */

#include "newlines.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "chunk_list.h"
#include "unc_ctype.h"
#include "unc_tools.h"
#include "uncrustify.h"
#include "indent.h"
#include "logger.h"
#include "space.h"
#include "combine.h"
#include "keywords.h"


/** tbd */
static void mark_change(
   const char* func,  /**< [in]  */
   uint32_t    line   /**< [in]  */
);


/**
 * Check to see if we are allowed to increase the newline count.
 * We can't increase the newline count:
 *  - if nl_squeeze_ifdef and a preproc is after the newline.
 *  - if eat_blanks_before_close_brace and the next is '}'
 *  - if eat_blanks_after_open_brace and the prev is '{'
 */
static bool can_increase_nl(
   chunk_t* nl  /**< [in]  */
);


/**
 * Double the newline, if allowed.
 */
static void double_newline(
   chunk_t* nl  /**< [in]  */
);


/**
 * Basic approach:
 * 1. Find next open brace
 * 2. Find next close brace
 * 3. Determine why the braces are there
 * a. struct/union/enum "enum [name] {"
 * c. assignment "= {"
 * b. if/while/switch/for/etc ") {"
 * d. else "} else {"
 */
static void setup_newline_add(
   chunk_t* prev, /**< [in]  */
   chunk_t* nl,   /**< [in]  */
   chunk_t* next  /**< [in]  */
);


/**
 * Make sure there is a blank line after a commented group of values
 */
static void nl_double_space_struct_enum_union(
   chunk_t* open_brace /**< [in]  */
);


/**
 * If requested, make sure each entry in an enum is on its own line
 */
static void nl_enum_entries(
   chunk_t* open_brace, /**< [in]  */
   argval_t av          /**< [in]  */
);


/**
 * Checks to see if it is OK to add a newline around the chunk.
 * Don't want to break one-liners...
 *
 * @retval true:  a new line may     be added
 * @retval false: a new line may NOT be added
 */
static bool one_liner_nl_ok(
   chunk_t* pc /**< [in]  */
);


/**
 * tbd
 */
static void nl_create_one_liner(
   chunk_t* vbrace_open /**< [in]  */
);


/**
 * Find the next newline or nl_cont
 */
static void handle_define(
   chunk_t* pc /**< [in]  */
);


/**
 * Does the Ignore, Add, Remove, or Force thing between two chunks
 */
static void nl_iarf_pair(
   chunk_t* before, /**< [in] The first chunk */
   chunk_t* after,  /**< [in] The second chunk */
   argval_t av      /**< [in] The IARF value */
);


/**
 * Adds newlines to multi-line function call/decl/def
 * Start points to the open parenthesis
 */
static void nl_func_multi_line(
   chunk_t* start /**< [in]  */
);


/**
 * Formats a function declaration
 * Start points to the open parenthesis
 */
static void nl_func_def(
   chunk_t* start /**< [in]  */
);


/**
 * Formats a message, adding newlines before the item before the colons.
 *
 * Start points to the open '[' in:
 * [myObject doFooWith:arg1 name:arg2  // some lines with >1 arg
 *            error:arg3];
 */
static void nl_oc_msg(
   chunk_t* start /**< [in]  */
);


/**
 * Ensure that the next non-comment token after close brace is a newline
 */
static void newline_end_newline(
   chunk_t* br_close /**< [in]  */
);


/**
 * Add or remove a newline between the closing paren and opening brace.
 * Also uncuddles anything on the closing brace. (may get fixed later)
 *
 * "if (...) { \n" or "if (...) \n { \n"
 *
 * For virtual braces, we can only add a newline after the vbrace open.
 * If we do so, also add a newline after the vbrace close.
 */
static bool nl_if_for_while_switch(
   chunk_t* start, /**< [in]  */
   argval_t nl_opt /**< [in]  */
);


/**
 * Add or remove extra newline before the chunk.
 * Adds before comments
 * Doesn't do anything if open brace before it
 * "code\n\ncomment\nif (...)" or "code\ncomment\nif (...)"
 */
static void nl_if_for_while_switch_pre_blank_lines(
   chunk_t* start, /**< [in]  */
   argval_t nl_opt /**< [in]  */
);


/**   */
static void _blank_line_set(
   chunk_t*    pc,   /**< [in]  */
   const char* text, /**< [in]  */
   uo_t        uo    /**< [in]  */
);


/**
 * Add one/two newline(s) before the chunk.
 * Adds before comments
 * Adds before destructor
 * Doesn't do anything if open brace before it
 * "code\n\ncomment\nif (...)" or "code\ncomment\nif (...)"
 */
static void nl_func_pre_blank_lines(
   chunk_t* start /**< [in]  */
);


/**   */
static chunk_t* get_closing_brace(
   chunk_t* start /**< [in]  */
);


/**
 * remove any consecutive newlines following this chunk
 * skip virtual braces
 */
static void remove_next_nl(
   chunk_t* start /**< [in]  */
);


/**
 * Add or remove extra newline after end of the block started in chunk.
 * Doesn't do anything if close brace after it
 * Interesting issue is that at this point, newlines can be before or after
 * virtual braces
 * VBraces will stay VBraces, conversion to real ones should have already happened
 * "if (...)\ncode\ncode" or "if (...)\ncode\n\ncode"
 */
static void nl_if_for_while_switch_post_blank_lines(
   chunk_t* start, /**< [in]  */
   argval_t nl_opt /**< [in]  */
);


/**
 * Adds or removes a newline between the keyword and the open brace.
 * If there is something after the '{' on the same line, then
 * the newline is removed unconditionally.
 * If there is a '=' between the keyword and '{', do nothing.
 *
 * "struct [name] {" or "struct [name] \n {"
 */
static void nl_struct_enum_union(
   chunk_t* start,         /**< [in]  */
   argval_t nl_opt,        /**< [in]  */
   bool     leave_trailing /**< [in]  */
);


/** tbd  */
static void nl_enum(
   chunk_t* start /**< [in]  */
);


/**
 * Cuddles or uncuddles a chunk with a previous close brace
 *
 * "} while" vs "} \n while"
 * "} else"  vs "} \n else"
 */
static void nl_cuddle_uncuddle(
   chunk_t* start, /**< [in] chunk to operate on - should be CT_ELSE or CT_WHILE_OF_DO */
   argval_t nl_opt /**< [in]  */
);


/**
 * Adds/removes a newline between else and '{'.
 * "else {" or "else \n {"
 */
static void nl_do_else(
   chunk_t* start, /**< [in]  */
   argval_t nl_opt /**< [in]  */
);


/**
 * Put a newline before and after a block of variable definitions
 */
static chunk_t* nl_def_blk(
   chunk_t* start, /**< [in]  */
   bool     fn_top /**< [in]  */
);


/**
 * Handles the brace_on_func_line setting and decides if the closing brace
 * of a pair should be right after a newline.
 * The only cases where the closing brace shouldn't be the first thing on a line
 * is where the opening brace has junk after it AND where a one-liner in a
 * class is supposed to be preserved.
 *
 * General rule for break before close brace:
 * If the brace is part of a function (call or definition) OR if the only
 * thing after the opening brace is comments, the there must be a newline
 * before the close brace.
 *
 * Example of no newline before close
 * struct mystring { int32_t  len;
 *                   char str[]; };
 * while (*(++ptr) != 0) { }
 *
 * Examples of newline before close
 * void foo() {
 * }
 */
static void nl_brace_pair(
   chunk_t* br_open /**< [in]  */
);


/**
 * Put a empty line between the 'case' statement and the previous case colon
 * or semicolon.
 * Does not work with PAWN (?)
 */
static void nl_case(
   chunk_t* start /**< [in]  */
);


/**
 * tbd
 */
static void nl_case_colon(
   chunk_t* start /**< [in]  */
);


/**
 * Put a blank line before a return statement, unless it is after an open brace
 */
static void nl_before_return(
   chunk_t* start /**< [in]  */
);


/**
 * Put a empty line after a return statement, unless it is followed by a
 * close brace.
 *
 * May not work with PAWN
 */
static void nl_after_return(
   chunk_t* start /**< [in]  */
);


/**
 * add a log entry that helps debug the newline detection
 */
static void log_nl_func(
   chunk_t* pc /**< [in]  */
);


/**
 * mark a line as blank line
 */
static void set_blank_line(
   uo_t     option, /**< [in]  */
   chunk_t* last_nl /**< [in]  */
);


/**
 * \brief clear the one-liner flag in all chunks that
 * follow a given chunk until a chunk without the
 * one-liner flag is found
 */
void clear_one_liner_flag(
   chunk_t* pc,  /**< [in] chunk to start with  */
   dir_e    dir  /**< [in] direction to move */
);


/** remove empty newlines at start or end of a file */
static void nl_eat(
   const dir_e dir /**< [in] AFTER = end of file, BEFORE = start of file */
);


/** remove empty newlines at start of a file */
static void nl_eat_start(void);


/** remove empty newlines at end of a file */
static void nl_eat_end(void);


#define MARK_CHANGE()    mark_change(__func__, __LINE__)


static void mark_change(const char *func, uint32_t line)
{
   LOG_FUNC_ENTRY();
   cpd.changes++;
   if (cpd.pass_count == 0)
   {
      LOG_FMT(LCHANGE, "%s: change %d on %s:%u\n",
              __func__, cpd.changes, func, line);
   }
}


static bool can_increase_nl(chunk_t* nl)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(nl), false);

   chunk_t* prev       = get_prev_nc   (nl);
   const chunk_t* pcmt = chunk_get_prev(nl);
   chunk_t* next       = chunk_get_next(nl);

   if (is_true(UO_nl_squeeze_ifdef))
   {
      if (is_type_and_ptype(prev, CT_PREPROC, CT_PP_ENDIF) &&
          (prev->level > 0 || is_true(UO_nl_squeeze_ifdef_top_level)))
      {
         LOG_FMT(LBLANKD, "%s: nl_squeeze_ifdef %u (prev) pp_lvl=%u rv=0\n",
                 __func__, nl->orig_line, nl->pp_level);
         return(false);
      }

      if (is_type_and_ptype(next, CT_PREPROC, CT_PP_ENDIF) &&
          (next->level > 0 || is_true(UO_nl_squeeze_ifdef_top_level)))
      {
         bool rv = ifdef_over_whole_file() && is_flag(next, PCF_WF_ENDIF);
         LOG_FMT(LBLANKD, "%s: nl_squeeze_ifdef %u (next) pp_lvl=%u rv=%d\n",
                 __func__, nl->orig_line, nl->pp_level, rv);
         return(rv);
      }
   }

   if (is_true(UO_eat_blanks_before_close_brace) && is_type(next, CT_BRACE_CLOSE))
   {
      LOG_FMT(LBLANKD, "%s: eat_blanks_before_close_brace %u\n", __func__, nl->orig_line);
      return(false);
   }

   if (is_true(UO_eat_blanks_after_open_brace) && is_type(prev, CT_BRACE_OPEN))
   {
      LOG_FMT(LBLANKD, "%s: eat_blanks_after_open_brace %u\n", __func__, nl->orig_line);
      return(false);
   }

   if (!pcmt && not_ignore(UO_nl_start_of_file))
   {
      LOG_FMT(LBLANKD, "%s: SOF no prev %u\n", __func__, nl->orig_line);
      return(false);
   }

   if (!next && not_ignore(UO_nl_end_of_file))
   {
      LOG_FMT(LBLANKD, "%s: EOF no next %u\n", __func__, nl->orig_line);
      return(false);
   }

   return(true);
}


static void double_newline(chunk_t* nl)
{
   LOG_FUNC_ENTRY();
   chunk_t* prev = chunk_get_prev(nl);
   assert(is_valid(prev));

   LOG_FMT(LNEWLINE, "%s: add newline after %s on line %u",
           __func__, prev->text(), prev->orig_line);

   if (!can_increase_nl(nl))
   {
      LOG_FMT(LNEWLINE, " - denied\n");
      return;
   }
   LOG_FMT(LNEWLINE, " - done\n");
   assert(is_valid(nl));
   if (nl->nl_count != 2)
   {
      nl->nl_count = 2;
      MARK_CHANGE();
   }
}


static void setup_newline_add(chunk_t* prev, chunk_t* nl, chunk_t* next)
{
   LOG_FUNC_ENTRY();
   return_if(are_invalid(prev, nl, next));

   undo_one_liner(prev);

   nl->orig_line   = prev->orig_line;
   nl->level       = prev->level;
   nl->brace_level = prev->brace_level;
   nl->pp_level    = prev->pp_level;
   nl->nl_count    = 1;
   update_flags(nl, PCF_IN_PREPROC, get_flags(prev, PCF_COPY_FLAGS));
   nl->orig_col    = prev->orig_col_end;
   nl->column      = prev->orig_col;

   if (is_preproc(prev) && is_preproc(next))
   {
      set_flags(nl, PCF_IN_PREPROC);
   }
   if (is_preproc(nl))
   {
      set_type(nl, CT_NL_CONT);
      nl->str = "\\\n";
   }
   else
   {
      set_type(nl, CT_NEWLINE);
      nl->str = "\n";
   }
}

#if 0
chunk_t* newline_add(chunk_t* pc, const dir_e pos)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(pc), pc);

   chunk_t* next = chunk_search(pc, is_vbrace, scope_e::ALL, pos, false));

   /* if there is already a newline no need to add another one */
   retval_if(is_nl(next), next);

   LOG_FMT(LNEWLINE, "%s: '%s' on line %u, col %u, pc->column=%u",
           __func__, pc->text(), pc->orig_line, pc->orig_col, pc->column);
   log_func_stack_inline(LNEWLINE);

   chunk_t nl;
   setup_newline_add(pc, &nl, next);
   LOG_FMT(LNEWLINE, "%s: '%s' on line %u, col %u, nl.column=%u\n",
           __func__, nl.text(), nl.orig_line, nl.orig_col, nl.column);

   MARK_CHANGE();
   return(chunk_add(&nl, pc, pos));
}
#endif

/* \todo DRY with newline_add_after */
chunk_t* newline_add_before(chunk_t* pc)
{
//   return newline_add(pc, dir_e::BEFORE);

   LOG_FUNC_ENTRY();
   retval_if(is_invalid(pc), pc);

   chunk_t* prev = get_prev_nvb(pc);

   /* Already has a newline before this chunk */
   retval_if(is_nl(prev), prev);

   LOG_FMT(LNEWLINE, "%s: '%s' on line %u, col %u, pc->column=%u",
           __func__, pc->text(), pc->orig_line, pc->orig_col, pc->column);
   log_func_stack_inline(LNEWLINE);

   chunk_t nl;
   setup_newline_add(prev, &nl, pc);
   LOG_FMT(LNEWLINE, "%s: '%s' on line %u, col %u, nl.column=%u\n",
           __func__, nl.text(), nl.orig_line, nl.orig_col, nl.column);

   MARK_CHANGE();
   return(chunk_add_before(&nl, pc));
}


/* \todo DRY with newline_add_before */
chunk_t* newline_add_after(chunk_t* pc)
{
//   return newline_add(pc, dir_e::AFTER);

   LOG_FUNC_ENTRY();
   retval_if(is_invalid(pc), pc);

   chunk_t* next = get_next_nvb(pc);

   /* Already has a newline after this chunk */
   retval_if(is_nl(next), next);

   LOG_FMT(LNEWLINE, "%s: '%s' on line %u, col %u, pc->column=%u",
           __func__, pc->text(), pc->orig_line, pc->orig_col, pc->column);
   log_func_stack_inline(LNEWLINE);

   chunk_t nl;
   setup_newline_add(pc, &nl, next);
   LOG_FMT(LNEWLINE, "%s: '%s' on line %u, col %u, nl.column=%u\n",
           __func__, nl.text(), nl.orig_line, nl.orig_col, nl.column);

   MARK_CHANGE();
   return(chunk_add_after(&nl, pc));
}


/* \todo DRY with newline_force_before */
chunk_t* newline_force_after(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   chunk_t* nl = newline_add_after(pc);   /* add a newline */
   if (is_valid(nl) && (nl->nl_count > 1))/* check if there are more than 1 newline */
   {
      nl->nl_count = 1;                   /* if so change the newline count back to 1 */
      MARK_CHANGE();
   }
   return(nl);
}


/* \todo DRY with newline_force_after */
chunk_t* newline_force_before(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   chunk_t* nl = newline_add_before(pc);
   if (is_valid(nl) && (nl->nl_count > 1))
   {
      nl->nl_count = 1;
      MARK_CHANGE();
   }
   return(nl);
}


static void newline_end_newline(chunk_t* br_close)
{
   LOG_FUNC_ENTRY();
   chunk_t* next = chunk_get_next(br_close);
   chunk_t  nl;

   if (!is_cmt_or_nl(next))
   {
      nl.orig_line = br_close->orig_line;
      nl.nl_count  = 1;
      update_flags(&nl, PCF_IN_PREPROC, get_flags(br_close, PCF_COPY_FLAGS));
      if (is_preproc(br_close) && is_valid(next) && is_preproc(next))
      {
         nl.flags |= PCF_IN_PREPROC;
      }
      if (nl.flags & PCF_IN_PREPROC)
      {
         nl.type = CT_NL_CONT;
         nl.str  = "\\\n";
      }
      else
      {
         nl.type = CT_NEWLINE;
         nl.str  = "\n";
      }
      MARK_CHANGE();
      LOG_FMT(LNEWLINE, "%s: %u:%u add newline after '%s'\n",
              __func__, br_close->orig_line, br_close->orig_col, br_close->text());
      chunk_add_after(&nl, br_close);
   }
}


static void newline_min_after(chunk_t* ref, uint32_t count, uint64_t flag)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(ref));

   LOG_FMT(LNEWLINE, "%s: '%s' line %u - count=%u flg=0x%" PRIx64 ":",
           __func__, ref->text(), ref->orig_line, count, flag);
   log_func_stack_inline(LNEWLINE);

   chunk_t* pc = ref;
   pc = get_next_nl(pc);
   if (is_valid(pc))
   {
      LOG_FMT(LNEWLINE, "%s: on %s, line %u, col %u\n",
              __func__, get_token_name(pc->type), pc->orig_line, pc->orig_col);
   }

   chunk_t* next = chunk_get_next(pc);
   return_if(is_invalid(next));

   if (is_cmt(next) && (next->nl_count == 1) &&
       is_cmt(chunk_get_prev(pc)))
   {
      newline_min_after(next, count, flag);
      return;
   }
   else
   {
      set_flags(pc, flag);
      if (is_nl(pc) && can_increase_nl(pc))
      {
         assert(is_valid(pc));
         if (pc->nl_count < count)
         {
            pc->nl_count = count;
            MARK_CHANGE();
         }
      }
   }
}


chunk_t* newline_add_between(chunk_t* start, chunk_t* end)
{
   LOG_FUNC_ENTRY();
   retval_if(are_invalid(start, end), nullptr);

   LOG_FMT(LNEWLINE, "%s: '%s'[%s] line %u:%u and '%s' line %u:%u :",
           __func__, start->text(), get_token_name(start->type),
           start->orig_line, start->orig_col,
           end->text(), end->orig_line, end->orig_col);
   log_func_stack_inline(LNEWLINE);

   /* Back-up check for one-liners (should never be true) */
   retval_if(!one_liner_nl_ok(start), nullptr);

   /* if there is a line break between start and end
    * we won't add another one */
   for (chunk_t* pc = start; pc != end; pc = chunk_get_next(pc))
   {
      retval_if(is_nl(pc), pc);
   }

   /* If the second one is a brace open, then check to see
    * if a comment + newline follows */
   if(is_type(end, CT_BRACE_OPEN))
   {
      chunk_t* pc = chunk_get_next(end);
      if (is_cmt(pc))
      {
         pc = chunk_get_next(pc);
         if (is_nl(pc))
         {
            /* Move the open brace to after the newline */
            chunk_move_after(end, pc);
            return(pc);
         }
      }
   }

   return(newline_add_before(end));
}


void newline_del_between(chunk_t* start, chunk_t* end)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(start));
   LOG_FMT(LNEWLINE, "%s: '%s' line %u:%u and '%s' line %u:%u : preproc=%d/%d ",
           __func__, start->text(), start->orig_line, start->orig_col,
           end->text(), end->orig_line, end->orig_col,
           is_preproc(start), is_preproc(end));
   log_func_stack_inline(LNEWLINE);

   /* Can't remove anything if the preproc status differs */
   return_if(!are_same_pp(start, end));

   chunk_t* pc           = start;
   bool    start_removed = false;
   do
   {
      chunk_t* next = chunk_get_next(pc);
      if (is_nl(pc))
      {
         chunk_t* prev = chunk_get_prev(pc);
         if ((!is_cmt(prev) && !is_cmt(next)) ||
               is_nl (prev) ||  is_nl (next))
         {
            if (is_safe_to_del_nl(pc))
            {
               if (pc == start) { start_removed = true; }

               chunk_del(pc);
               MARK_CHANGE();
               if (is_valid(prev))
               {
                  align_to_column(next, (prev->column + space_col_align(prev, next)) );
               }
            }
         }
         else
         {
            if (pc->nl_count > 1)
            {
               pc->nl_count = 1;
               MARK_CHANGE();
            }
         }
      }
      pc = next;
   } while (pc != end);

   if ( is_str(end,   "{" ) && !start_removed &&
       (is_str(start, ")" ) || is_type(start, CT_DO, CT_ELSE) ) )
   {
      chunk_move_after(end, start);
   }
}


// DRY with newlines_if_for_while_switch_post_blank_lines
static bool nl_if_for_while_switch(chunk_t* start, argval_t nl_opt)
{
   LOG_FUNC_ENTRY();

   retval_if((nl_opt == AV_IGNORE) ||
             (is_preproc(start) && is_false(UO_nl_define_macro)), false);

   bool    retval = false;
   chunk_t* pc    = get_next_ncnl(start);
   if (is_type(pc, CT_SPAREN_OPEN))
   {
      chunk_t* close_paren = get_next_type(pc, CT_SPAREN_CLOSE, (int32_t)pc->level);
      chunk_t* brace_open  = get_next_ncnl(close_paren);

      if (is_type(brace_open, CT_BRACE_OPEN, CT_VBRACE_OPEN) &&
          one_liner_nl_ok(brace_open                       ) )
      {
         if (is_true(UO_nl_multi_line_cond))
         {
            while ((pc = chunk_get_next(pc)) != close_paren)
            {
               if (is_nl(pc))
               {
                  nl_opt = AV_ADD;
                  break;
               }
            }
         }

         if (is_type(brace_open, CT_VBRACE_OPEN))
         {
            /* Can only add - we don't want to create a one-line here */
            if (is_arg_set(nl_opt, AV_ADD))
            {
               nl_iarf_pair(close_paren, get_next_ncnl(brace_open), nl_opt);
               pc = get_next_type(brace_open, CT_VBRACE_CLOSE, (int32_t)brace_open->level);
               if (!is_nl(get_prev_nc(pc)) &&
                   !is_nl(get_next_nc(pc)) )
               {
                  newline_add_after(pc);
                  retval = true;
               }
            }
         }
         else
         {
            nl_iarf_pair  (close_paren, brace_open, nl_opt);
            newline_add_between(brace_open, get_next_ncnl(brace_open));

            /* Make sure nothing is cuddled with the closing brace */
            pc = get_next_type(brace_open, CT_BRACE_CLOSE, (int32_t)brace_open->level);
            newline_add_between(pc, get_next_nblank(pc));
            retval = true;
         }
      }
   }
   return(retval);
}


static void nl_if_for_while_switch_pre_blank_lines(chunk_t* start, argval_t nl_opt)
{
   LOG_FUNC_ENTRY();
   return_if( (nl_opt == AV_IGNORE) ||
              (is_preproc(start) && is_false(UO_nl_define_macro)));

   chunk_t* prev;
   chunk_t* next;
   chunk_t* last_nl = nullptr;
   uint32_t level   = start->level;
   bool     do_add  = is_arg_set(nl_opt, AV_ADD);

   /* look backwards until we find
    *  open brace (don't add or remove)
    *  2 newlines in a row (don't add)
    *  something else (don't remove) */
   for (chunk_t* pc = chunk_get_prev(start); is_valid(pc); pc = chunk_get_prev(pc))
   {
      if (is_nl(pc))
      {
         last_nl = pc;
         /* if we found 2 or more in a row */
         if ((pc->nl_count > 1) || is_nl(get_prev_nvb(pc)))
         {
            /* need to remove */
            if (is_arg_set(nl_opt, AV_REMOVE) && not_flag(pc, PCF_VAR_DEF))
            {
               /* if we're also adding, take care of that here */
               uint32_t nl_count = do_add ? 2 : 1;
               if (nl_count != pc->nl_count)
               {
                  pc->nl_count = nl_count;
                  MARK_CHANGE();
               }
               /* can keep using pc because anything other than newline stops loop, and we delete if newline */
               while (is_nl(prev = get_prev_nvb(pc)))
               {
                  /* Make sure we don't combine a preproc and non-preproc */
                  break_if(!is_safe_to_del_nl(prev));
                  chunk_del(prev);
                  MARK_CHANGE();
               }
            }
            return;
         }
      }
      else if (is_opening_brace(pc) || (pc->level < level))
      {
         return;
      }
      else if (is_cmt(pc))
      {
         /* vbrace close is ok because it won't go into output, so we should skip it */
         last_nl = nullptr;
         continue;
      }
      else
      {
         if (do_add) /* we found something previously besides a comment or a new line */
         {
            /* if we have run across a newline */
            if (is_valid(last_nl))
            {
               if (last_nl->nl_count < 2)
               {
                  double_newline(last_nl);
               }
            }
            else
            {
               /* we didn't run into a newline, so we need to add one */
               if (((next = chunk_get_next(pc)) != nullptr) &&
                   is_cmt(next))
               {
                  pc = next;
               }
               if ((last_nl = newline_add_after(pc)) != nullptr)
               {
                  double_newline(last_nl);
               }
            }
         }
         return;
      }
   }
}


#define blank_line_set(pc, op) _blank_line_set(pc, #op, op)
static void _blank_line_set(chunk_t* pc, const char *text, uo_t uo)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   const option_map_value_t *option = get_option_name(uo);
   assert(ptr_is_valid(option));
   if (option->type != AT_UNUM)
   {
      fprintf(stderr, "Program error for UO_=%d\n", static_cast<int32_t>(uo));
      fprintf(stderr, "Please make a report\n");
      exit(2);
   }

   if ((get_uval(uo) > 0) && (pc->nl_count != get_uval(uo)))
   {
      LOG_FMT(LBLANKD, "do_blank_lines: %s set line %u to %u\n", text + 3, pc->orig_line, get_uval(uo));
      pc->nl_count = get_uval(uo);
      MARK_CHANGE();
   }
}


static void nl_func_pre_blank_lines(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LNLFUNCT, "\n%s: set blank line(s): for %s at line %u\n",
           __func__, start->text(), start->orig_line);

   /*  look backwards until we find:
    *  - open brace (don't add or remove)
    *  - two newlines in a row (don't add)
    *  - a destructor
    *  - something else (don't remove) */
   chunk_t* last_nl      = nullptr;
   chunk_t* last_comment = nullptr;
   chunk_t* pc;
   for (pc = chunk_get_prev(start); is_valid(pc); pc = chunk_get_prev(pc))
   {
      switch(pc->type)
      {
         case(CT_NEWLINE):
         case(CT_NL_CONT):
            last_nl = pc;
            LOG_FMT(LNLFUNCT, "   <chunk_is_newline> found at line=%u column=%u\n", pc->orig_line, pc->orig_col);
         break;

         case(CT_COMMENT_MULTI):
         case(CT_COMMENT):
         case(CT_COMMENT_CPP):
            LOG_FMT(LNLFUNCT, "   <chunk_is_comment> found at line=%u column=%u\n", pc->orig_line, pc->orig_col);
            if ((pc->orig_line < start->orig_line &&
                 (((start->orig_line - pc->orig_line) - (pc->type == CT_COMMENT_MULTI ? pc->nl_count : 0))) < 2) ||
                (is_type(last_comment, pc->type) && // don't mix comment types
                 is_type(pc, CT_COMMENT_CPP    ) && // combine only cpp comments
                  last_comment->orig_line > pc->orig_line &&
                 (last_comment->orig_line - pc->orig_line) < 2))
            {
               last_comment = pc;
            }
            else
            {
               goto set_blank_line;
            }
         break;

         case(CT_DESTRUCTOR): /* fallthrough */
         case(CT_TYPE      ): /* fallthrough */
         case(CT_QUALIFIER ): /* fallthrough */
         case(CT_PTR_TYPE  ): /* fallthrough */
         case(CT_DC_MEMBER ): log_nl_func(pc);     break;
         default:             log_nl_func(pc);
                              goto set_blank_line; break;
      }
   }
   return;

set_blank_line:
   if (is_valid(last_nl))
   {
      LOG_FMT(LNLFUNCT, "   set blank line(s): for <NL> at line=%u column=%u\n",
            last_nl->orig_line, last_nl->orig_col);
      switch (start->type)
      {
         case CT_FUNC_CLASS_DEF:   set_blank_line(UO_nl_before_func_class_def,   last_nl); break;
         case CT_FUNC_CLASS_PROTO: set_blank_line(UO_nl_before_func_class_proto, last_nl); break;
         case CT_FUNC_DEF:         set_blank_line(UO_nl_before_func_body_def,    last_nl); break;
         case CT_FUNC_PROTO:       set_blank_line(UO_nl_before_func_body_proto,  last_nl); break;

         default:
            assert(is_valid(pc));
            LOG_FMT(LERR, "   setting to blank line(s) at line %u not possible\n", pc->orig_line);
            break;
      }
   }
}


static void log_nl_func(chunk_t* pc)
{
   LOG_FMT(LNLFUNCT, "   <%s> %s found at line=%u column=%u\n",
         get_token_name(pc->type), pc->text(), pc->orig_line, pc->orig_col);
}


static void set_blank_line(uo_t option, chunk_t* last_nl)
{
   if (get_uval(option) > 0)
   {
      if (get_uval(option) != last_nl->nl_count)
      {
         LOG_FMT(LNLFUNCT, "   set blank line(s) to %u\n", get_uval(option));
         blank_line_set(last_nl, option);
      }
   }
}

static chunk_t* get_closing_brace(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   chunk_t* pc;
   uint32_t level = start->level;

   for (pc = start; (pc = chunk_get_next(pc)) != nullptr; )
   {
      retval_if(is_type (pc, CT_BRACE_CLOSE, CT_VBRACE_CLOSE) &&
                is_level(pc, level), pc);

      /* for some reason, we can have newlines between if and opening
       * brace that are lower level than either */
      retval_if (!is_nl(pc) && (pc->level < level), nullptr);
   }
   return(nullptr);
}


static void remove_next_nl(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   chunk_t* next;
   while ((next = chunk_get_next(start)) != nullptr)
   {
      if (is_nl(next) && is_safe_to_del_nl(next))
      {
         chunk_del(next);
         MARK_CHANGE();
      }
      else if (is_vbrace(next))
      {
         start = next;
      }
      else
      {
         break;
      }
   }
}


static void nl_if_for_while_switch_post_blank_lines(chunk_t* start, argval_t nl_opt)
{
   LOG_FUNC_ENTRY();
   return_if (is_arg(nl_opt, AV_IGNORE) ||
       (is_preproc(start) && is_false(UO_nl_define_macro)));

   /* first find ending brace */
   chunk_t* pc;
   return_if ((pc = get_closing_brace(start)) == nullptr);

   chunk_t* next;
   chunk_t* prev;
   /* if we're dealing with an if, we actually want to add or remove
    * blank lines after any else */
   if (is_type(start, CT_IF))
   {
      while (true)
      {
         next = get_next_ncnl(pc);
         if (is_type(next, CT_ELSE, CT_ELSEIF))
         {
            /* point to the closing brace of the else */
            return_if((pc = get_closing_brace(next)) == nullptr);
         }
         else { break; }
      }
   }

   /* if we're dealing with a do/while, we actually want to add or
    * remove blank lines after while and its condition */
   if (is_type(start, CT_DO))
   {
      /* point to the next semicolon */
      return_if((pc = get_next_type(pc, CT_SEMICOLON, (int32_t)start->level)) == nullptr);
   }

   bool isVBrace = pc->type == CT_VBRACE_CLOSE;
   return_if((prev = get_prev_nvb(pc)) == nullptr);

   bool have_pre_vbrace_nl = isVBrace && is_nl(prev);
   if (is_arg_set(nl_opt, AV_REMOVE))
   {
      /* if vbrace, have to check before and after */
      /* if chunk before vbrace, remove any newlines after vbrace */
      if (have_pre_vbrace_nl)
      {
         if (prev->nl_count != 1)
         {
            prev->nl_count = 1;
            MARK_CHANGE();
         }
         remove_next_nl(pc);
      }
      else if (is_nl(next = get_next_nvb(pc)) &&
               not_flag(next, PCF_VAR_DEF))
      {
         /* otherwise just deal with newlines after brace */
         if (next->nl_count != 1)
         {
            next->nl_count = 1;
            MARK_CHANGE();
         }
         remove_next_nl(next);
      }
   }

   /* may have a newline before and after vbrace */
   /* don't do anything with it if the next non newline chunk is a closing brace */
   if (is_arg_set(nl_opt, AV_ADD))
   {
      return_if ((next = get_next_nnl(pc)) == nullptr);

      if (not_type(next, CT_BRACE_CLOSE))
      {
         /* if vbrace, have to check before and after */
         /* if chunk before vbrace, check its count */
         uint32_t nl_count = have_pre_vbrace_nl ? prev->nl_count : 0;
         if (is_nl(next = get_next_nvb(pc)))
         {
            assert(is_valid(next));
            nl_count += next->nl_count;
         }

         /* if we have no newlines, add one and make it double */
         if (nl_count == 0)
         {
            if (((next = chunk_get_next(pc)) != nullptr) &&
                is_cmt(next))
            {
               pc = next;
            }

            return_if((next = newline_add_after(pc)) == nullptr);
            double_newline(next);
         }
         else if (nl_count == 1) /* if we don't have enough newlines */
         {
            /* if we have one before vbrace, need to add one after */
            if (have_pre_vbrace_nl)
            {
               next = newline_add_after(pc);
            }
            else
            {
               prev = get_prev_nnl(next);
               pc   = get_next_nl (next);
               //LOG_FMT(LSYS, "  -- pc1=%s [%s]\n", pc->text(), get_token_name(pc->type));

               pc = chunk_get_next(pc);
               //LOG_FMT(LSYS, "  -- pc2=%s [%s]\n", pc->text(), get_token_name(pc->type));
               if (is_type_and_ptype(pc, CT_PREPROC, CT_PP_ENDIF) &&
                   is_true(UO_nl_squeeze_ifdef))
               {
                  assert(is_valid(prev));
                  LOG_FMT(LNEWLINE, "%s: cannot add newline after line %u due to nl_squeeze_ifdef\n",
                          __func__, prev->orig_line);
               }
               else
               {
                  double_newline(next); /* make nl after double */
               }
            }
         }
      }
   }
} /*lint !e438 */


static void nl_struct_enum_union(chunk_t* start, argval_t nl_opt, bool leave_trailing)
{
   LOG_FUNC_ENTRY();
   return_if ((nl_opt == AV_IGNORE) ||
              (is_preproc(start) && is_false(UO_nl_define_macro)));

   // \todo DRY with line 1347
   /* step past any junk between the keyword and the open brace
    * Quit if we hit a semicolon or '=', which are not expected. */
   uint32_t level = start->level;
   chunk_t* pc = start;
   while (((pc = get_next_ncnl(pc)) != nullptr) &&
           (pc->level >= level))
   {
      break_if(is_level(pc, level) &&
               is_type (pc, CT_VSEMICOLON, CT_BRACE_OPEN,
                            CT_SEMICOLON,  CT_ASSIGN));
      start = pc;
   }

   /* If we hit a brace open, then we need to toy with the newlines */
   if (is_type(pc, CT_BRACE_OPEN))
   {
      /* Skip over embedded C comments */
      chunk_t* next = chunk_get_next(pc);
      while (is_type(next, CT_COMMENT))
      {
         next = chunk_get_next(next);
      }
      if (leave_trailing &&
          !is_cmt_or_nl(next))
      {
         nl_opt = AV_IGNORE;
      }

      nl_iarf_pair(start, pc, nl_opt);
   }
}


// enum {
// enum class angle_state_e : unsigned int {
// enum-key attr(optional) identifier(optional) enum-base(optional) { enumerator-list(optional) }
// enum-key attr(optional) nested-name-specifier(optional) identifier enum-base(optional) ; TODO
// enum-key         - one of enum, enum class or enum struct  TODO
// identifier       - the name of the enumeration that's being declared
// enum-base(C++11) - colon (:), followed by a type-specifier-seq
// enumerator-list  - comma-separated list of enumerator definitions
static void nl_enum(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   return_if (is_preproc(start) && is_false(UO_nl_define_macro));

   /* look for 'enum class' */
   chunk_t* pcClass = get_next_ncnl(start);
   if (is_type(pcClass, CT_ENUM_CLASS))
   {
      nl_iarf_pair(start, pcClass, get_arg(UO_nl_enum_class));
      /* look for 'identifier'/ 'type' */
      chunk_t* pcType = get_next_ncnl(pcClass);
      if (is_type(pcType, CT_TYPE))
      {
         nl_iarf_pair(pcClass, pcType, get_arg(UO_nl_enum_class_identifier));
         /* look for ':' */
         chunk_t* pcColon = get_next_ncnl(pcType);
         if (is_type(pcColon, CT_BIT_COLON))
         {
            nl_iarf_pair(pcType, pcColon, get_arg(UO_nl_enum_identifier_colon));
            /* look for 'type' i.e. unsigned */
            chunk_t* pcType1 = get_next_ncnl(pcColon);
            if (is_type(pcType1, CT_TYPE))
            {
               nl_iarf_pair(pcColon, pcType1, get_arg(UO_nl_enum_colon_type));
               /* look for 'type' i.e. int */
               chunk_t* pcType2 = get_next_ncnl(pcType1);
               if (is_type(pcType2, CT_TYPE))
               {
                  nl_iarf_pair(pcType1, pcType2, get_arg(UO_nl_enum_colon_type));
               }
            }
         }
      }
   }

   // \todo DRY with line 1272
   /* step past any junk between the keyword and the open brace
    * Quit if we hit a semicolon or '=', which are not expected. */
   uint32_t level = start->level;
   chunk_t* pc = start;
   while (((pc = get_next_ncnl(pc)) != nullptr) &&
           (pc->level >= level))
   {
      break_if (is_level(pc, level) &&
                is_type (pc, CT_VSEMICOLON, CT_BRACE_OPEN,
                             CT_SEMICOLON,  CT_ASSIGN));
      start = pc;
   }

   /* If we hit a brace open, then we need to toy with the newlines */
   if (is_type(pc, CT_BRACE_OPEN))
   {
      /* Skip over embedded C comments */
      chunk_t* next = chunk_get_next(pc);
      while (is_type(next, CT_COMMENT))
      {
         next = chunk_get_next(next);
      }

      argval_t nl_opt = (!is_cmt_or_nl(next)) ?
                AV_IGNORE : get_arg(UO_nl_enum_brace);

      nl_iarf_pair(start, pc, nl_opt);
   }
}


static void nl_cuddle_uncuddle(chunk_t* start, argval_t nl_opt)
{
   LOG_FUNC_ENTRY();
   return_if(is_preproc(start) && is_false(UO_nl_define_macro));

   chunk_t* br_close = get_prev_ncnl(start);
   if (is_closing_rbrace(br_close))
   {
      nl_iarf_pair(br_close, start, nl_opt);
   }
}


static void nl_do_else(chunk_t* start, argval_t nl_opt)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(start));

   return_if(is_arg(nl_opt, AV_IGNORE) ||
            (is_preproc(start) && is_false(UO_nl_define_macro)));

   chunk_t* next = get_next_ncnl(start);
   if (is_opening_brace(next))
   {
      if (!one_liner_nl_ok(next))
      {
         LOG_FMT(LNL1LINE, "a new line may NOT be added\n");
         return;
      }
      else
      {
         LOG_FMT(LNL1LINE, "a new line may be added\n");
      }

      if (is_opening_vbrace(next))
      {
         /* Can only add - we don't want to create a one-line here */
         if (is_arg_set(nl_opt, AV_ADD))
         {
            nl_iarf_pair(start, get_next_ncnl(next), nl_opt);
            chunk_t* tmp = get_next_type(next, CT_VBRACE_CLOSE, (int32_t)next->level);
            if (!is_nl(get_next_nc(tmp)) &&
                !is_nl(get_prev_nc(tmp)) )
            {
               newline_add_after(tmp);
            }
         }
      }
      else
      {
         nl_iarf_pair(start, next, nl_opt);
         newline_add_between(next, get_next_ncnl(next));
      }
   }
}


static chunk_t* nl_def_blk(chunk_t* start, bool fn_top)
{
   LOG_FUNC_ENTRY();

   bool did_this_line = false;
   bool first_var_blk = true;
   bool typedef_blk   = false;
   bool var_blk       = false;

   chunk_t* pc;
   chunk_t* prev = get_prev_ncnl(start);
   /* can't be any variable definitions in a "= {" block */
   if (is_type(prev, CT_ASSIGN))
   {
      pc = get_next_type(start, CT_BRACE_CLOSE, (int32_t)start->level);
      return(get_next_ncnl(pc));
   }
   pc = chunk_get_next(start);
   while (is_valid(pc) &&
         ((pc->level >= start->level) || is_level(pc, 0)))
   {
      if (is_cmt (pc)          ) { pc = chunk_get_next(pc);    continue; }
      if (is_opening_rbrace(pc)) { pc = nl_def_blk(pc, false); continue; } /* process nested braces */
      if (is_closing_rbrace(pc)) { pc = chunk_get_next(pc);    break;    } /* Done with this brace set? */

      /* skip virtual braces */
      if (is_opening_vbrace(pc))
      {
         pc = get_next_type(pc, CT_VBRACE_CLOSE, (int32_t)pc->level);
         if (is_valid(pc))
         {
            pc = chunk_get_next(pc);
         }
         continue;
      }

      /* Ignore stuff inside parenthesis/squares/angles */
      if (pc->level > pc->brace_level)
      {
         pc = chunk_get_next(pc);
         continue;
      }

      if (is_nl(pc))
      {
         did_this_line = false;
         pc            = chunk_get_next(pc);
         continue;
      }

      /* Determine if this is a variable def or code */
      if ( (did_this_line == false) &&
           not_type(pc, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO) &&
          (is_level(pc, (start->level + 1) ) ||
           is_level(pc, 0)))
      {
         chunk_t* next = get_next_ncnl(pc);
         break_if(is_invalid(next));

         prev = get_prev_ncnl(pc);
         if (is_type(pc, CT_TYPEDEF))
         {
            /* set newlines before typedef block */
            if (!typedef_blk &&
                (is_valid(prev)) &&
                (get_uval(UO_nl_typedef_blk_start) > 0))
            {
               newline_min_after(prev, get_uval(UO_nl_typedef_blk_start), PCF_VAR_DEF);
            }
            /* set newlines within typedef block */
            else if (typedef_blk && (get_uval(UO_nl_typedef_blk_in) > 0))
            {
               prev = chunk_get_prev(pc);
               if (is_nl(prev))
               {
                  if (prev->nl_count > get_uval(UO_nl_typedef_blk_in))
                  {
                     prev->nl_count = get_uval(UO_nl_typedef_blk_in);
                     MARK_CHANGE();
                  }
               }
            }
            /* set blank lines after first var def block */
            if (var_blk && first_var_blk && fn_top &&
                (get_uval(UO_nl_func_var_def_blk) > 0))
            {
               newline_min_after(prev, 1 + get_uval(UO_nl_func_var_def_blk), PCF_VAR_DEF);
            }
            /* set newlines after var def block */
            else if (var_blk && (get_uval(UO_nl_var_def_blk_end) > 0))
            {
               newline_min_after(prev, get_uval(UO_nl_var_def_blk_end), PCF_VAR_DEF);
            }
            pc            = get_next_type(pc, CT_SEMICOLON, (int32_t)pc->level);
            typedef_blk   = true;
            first_var_blk = false;
            var_blk       = false;
         }
         else if (  is_var_type(pc  ) &&
                  ((is_var_type(next) ||
                    is_type(next, CT_WORD, CT_FUNC_CTOR_VAR))) &&
                    not_type(next, CT_DC_MEMBER)) /* DbConfig::configuredDatabase()->apply(db); */
                                                  /*  is NOT a declaration of a variable */
         {
            /* set newlines before var def block */
            if (var_blk       == false &&
                first_var_blk == false &&
                (get_uval(UO_nl_var_def_blk_start) > 0))
            {
               newline_min_after(prev, get_uval(UO_nl_var_def_blk_start), PCF_VAR_DEF);
            }
            /* set newlines within var def block */
            else if (var_blk && (get_uval(UO_nl_var_def_blk_in) > 0))
            {
               prev = chunk_get_prev(pc);
               if (is_nl(prev))
               {
                  if (prev->nl_count > get_uval(UO_nl_var_def_blk_in))
                  {
                     prev->nl_count = get_uval(UO_nl_var_def_blk_in);
                     MARK_CHANGE();
                  }
               }
            }
            /* set newlines after typedef block */
            else if (typedef_blk && (get_uval(UO_nl_typedef_blk_end) > 0))
            {
               newline_min_after(prev, get_uval(UO_nl_typedef_blk_end), PCF_VAR_DEF);
            }
            pc          = get_next_type(pc, CT_SEMICOLON, (int32_t)pc->level);
            typedef_blk = false;
            var_blk     = true;
         }
         else
         {
            /* set newlines after typedef block */
            if (typedef_blk && (get_uval(UO_nl_var_def_blk_end) > 0))
            {
               newline_min_after(prev, get_uval(UO_nl_var_def_blk_end), PCF_VAR_DEF);
            }
            /* set blank lines after first var def block */
            if ((var_blk       == true) &&
                (first_var_blk == true) &&
                (fn_top        == true) &&
                (get_uval(UO_nl_func_var_def_blk) > 0))
            {
               newline_min_after(prev, 1 + get_uval(UO_nl_func_var_def_blk), PCF_VAR_DEF);
            }
            /* set newlines after var def block */
            else if ( (var_blk == true) && (get_uval(UO_nl_var_def_blk_end) > 0))
            {
               newline_min_after(prev, get_uval(UO_nl_var_def_blk_end), PCF_VAR_DEF);
            }
            typedef_blk   = false;
            first_var_blk = false;
            var_blk       = false;
         }
      }
      did_this_line = true;
      pc            = chunk_get_next(pc);
   }

   return(pc);
}


static void nl_brace_pair(chunk_t* br_open)
{
   LOG_FUNC_ENTRY();

   return_if(is_preproc(br_open) && is_false(UO_nl_define_macro));

   chunk_t* next;
   chunk_t* pc;

   if (is_true(UO_nl_collapse_empty_body))
   {
      next = get_next_nnl(br_open);
      if (is_closing_rbrace(next))
      {
         pc = chunk_get_next(br_open);

         while (not_type(pc, CT_BRACE_CLOSE))
         {
            next = chunk_get_next(pc);
            if (is_type(pc, CT_NEWLINE))
            {
               if (is_safe_to_del_nl(pc))
               {
                  chunk_del(pc);
                  MARK_CHANGE();
               }
            }
            pc = next;
         }
         return;
      }
   }

   /* Make sure we don't break a one-liner */
   if (!one_liner_nl_ok(br_open))
   {
      LOG_FMT(LNL1LINE, "a new line may NOT be added\n");
      return;
   }
   else
   {
      LOG_FMT(LNL1LINE, "a new line may be added\n");
   }

   next = get_next_nc(br_open);
   chunk_t* prev;
   /** Insert a newline between the '=' and open brace, if needed */
   if (is_ptype(br_open, CT_ASSIGN))
   {
      /* Only mess with it if the open brace is followed by a newline */
      if (is_nl(next))
      {
         prev = get_prev_ncnl(br_open);
         nl_iarf_pair(prev, br_open, get_arg(UO_nl_assign_brace));
      }
   }

   /* Eat any extra newlines after the brace open */
   if (is_true(UO_eat_blanks_after_open_brace))
   {
      if (is_nl(next))
      {
         if (next->nl_count > 1)
         {
            next->nl_count = 1;
            LOG_FMT(LBLANKD, "%s: eat_blanks_after_open_brace %u\n", __func__, next->orig_line);
            MARK_CHANGE();
         }
      }
   }

   argval_t val            = AV_IGNORE;
   bool     nl_close_brace = false;
   /* Handle the cases where the brace is part of a function call or definition */
   if (is_ptype(br_open, 8, CT_FUNC_DEF,    CT_FUNC_CALL,
         CT_FUNC_CALL_USER, CT_OC_CLASS,    CT_CPP_LAMBDA,
         CT_FUNC_CLASS_DEF, CT_OC_MSG_DECL, CT_CS_PROPERTY) )
   {
      /* Need to force a newline before the close brace, if not in a class body */
      if(not_flag(br_open, PCF_IN_CLASS))
      {
         nl_close_brace = true;
      }

      /* handle newlines after the open brace */
      pc = get_next_ncnl(br_open);
      newline_add_between(br_open, pc);

      switch(br_open->ptype)
      {
         case(CT_FUNC_DEF      ): /* fallthrough */
         case(CT_FUNC_CLASS_DEF): /* fallthrough */
         case(CT_OC_CLASS      ): /* fallthrough */
         case(CT_OC_MSG_DECL   ): val = get_arg(UO_nl_fdef_brace    ); break;
         case(CT_CS_PROPERTY   ): val = get_arg(UO_nl_property_brace); break;
         case(CT_CPP_LAMBDA    ): val = get_arg(UO_nl_cpp_ldef_brace); break;
         default:                 val = get_arg(UO_nl_fcall_brace   ); break;
      }
      if (val != AV_IGNORE)
      {
         /* Grab the chunk before the open brace */
         prev = get_prev_ncnl(br_open);
         nl_iarf_pair(prev, br_open, val);
      }
      nl_def_blk(br_open, true);
   }

   /* Handle the cases where the brace is part of a class or struct */
   if (is_ptype(br_open, CT_CLASS, CT_STRUCT))
   {
      nl_def_blk(br_open, false);
   }

   /* Grab the matching brace close */
   chunk_t* br_close;
   br_close = get_next_type(br_open, CT_BRACE_CLOSE, (int32_t)br_open->level);
   return_if(is_invalid(br_close));

   if (!nl_close_brace)
   {
      /* If the open brace hits a CT_NEWLINE, CT_NL_CONT, CT_COMMENT_MULTI, or
       * CT_COMMENT_CPP without hitting anything other than CT_COMMENT, then
       * there should be a newline before the close brace. */
      pc = chunk_get_next(br_open);
      while (is_type(pc, CT_COMMENT))
      {
         pc = chunk_get_next(pc);
      }
      if (is_cmt_or_nl(pc))
      {
         nl_close_brace = true;
      }
   }

   prev = get_prev_nblank(br_close);
   if (nl_close_brace) { newline_add_between(prev, br_close); }
   else                { newline_del_between(prev, br_close); }
}


static void nl_case(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   /* Scan backwards until a '{' or ';' or ':'. Abort if a multi-newline is found */
   chunk_t* prev = start;
   do
   {
      prev = get_prev_nc(prev);
      return_if(is_nl(prev) && (prev->nl_count > 1));
   } while (not_type(prev, 4, CT_BRACE_OPEN, CT_BRACE_CLOSE,
                              CT_SEMICOLON,  CT_CASE_COLON));

   return_if(is_invalid(prev));

   chunk_t* pc = newline_add_between(prev, start);
   return_if(is_invalid(pc));

   /* Only add an extra line after a semicolon or brace close */
   if (is_type(prev, CT_SEMICOLON, CT_BRACE_CLOSE))
   {
      if (is_nl(pc) && (pc->nl_count < 2))
      {
         double_newline(pc);
      }
   }
}


static void nl_case_colon(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   chunk_t* pc = get_next_nc(start);
   if (is_valid(pc) && !is_nl(pc))
   {
      newline_add_before(pc);
   }
}


static void nl_before_return(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   chunk_t* nl = chunk_get_prev(start);
   return_if(is_invalid(nl));
   return_if(!is_nl(nl))        /* line has to start with 'return' */
   return_if(nl->nl_count > 1); /* no need to add a newline before a blank line */

   chunk_t* pc = chunk_get_prev(nl);
   return_if(is_invalid(pc) || is_opening_brace(pc));

   if (is_cmt(pc))
   {
      pc = chunk_get_prev(pc);
      return_if(is_invalid(pc) || !is_nl(pc));
      nl = pc;
   }
   if (nl->nl_count < 2)
   {
      nl->nl_count++;
      MARK_CHANGE();
   }
}


static void nl_after_return(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   chunk_t* semi  = get_next_type(start, CT_SEMICOLON, (int32_t)start->level);
   chunk_t* after = get_next_nblank(semi);

   /* If we hit a brace or an 'else', then a newline is not needed */
   return_if(is_type(after, CT_BRACE_CLOSE, CT_VBRACE_CLOSE, CT_ELSE));

   chunk_t* pc;
   for (pc = chunk_get_next(semi); pc != after; pc = chunk_get_next(pc))
   {
      if (is_type(pc, CT_NEWLINE))
      {
         if (pc->nl_count < 2) { double_newline(pc); }
         return;
      }
   }
}


static void nl_iarf_pair(chunk_t* before, chunk_t* after, argval_t av)
{
   LOG_FUNC_ENTRY();
   log_func_stack(LNEWLINE, "Call Stack:");

   if(are_valid(before, after))
   {
      if (is_arg_set(av, AV_ADD))
      {
         chunk_t* nl = newline_add_between(before, after);
         if ( (is_valid(nl)    ) &&
              (av == AV_FORCE  ) &&
              (nl->nl_count > 1) )
         {
            nl->nl_count = 1;
         }
      }
      else if (is_arg_set(av, AV_REMOVE))
      {
         newline_del_between(before, after);
      }
   }
}


void nl_iarf(chunk_t* pc, argval_t av)
{
   LOG_FUNC_ENTRY();
   log_func_stack(LNEWLINE, "CallStack:");

   nl_iarf_pair(pc, get_next_nnl(pc), av);
}


/*
 * count how many commas are present
 */
static uint32_t count_commas(
   chunk_t* *end,               /**< [out] chunk where counting stopped */
   chunk_t* start,              /**< [in]  chunk to start with */
   const argval_t newline,      /**< [in]  defines if a newline is added */
   const bool force_nl = false  /**< [in]  add newline independently of existing newline */
);


static uint32_t count_commas(chunk_t* *end, chunk_t* start, const argval_t newline, bool force_nl)
{
   chunk_t* pc;
   uint32_t comma_count = 0;
   for ( pc = get_next_ncnl(start);
        (is_valid(pc)) &&
        (pc->level > start->level);
         pc = get_next_ncnl(pc))
   {
      if (is_type (pc, CT_COMMA        ) &&
          is_level(pc, (start->level+1)) )
      {
         comma_count++;
         chunk_t* tmp = chunk_get_next(pc);
         if (is_cmt(tmp)) { pc = tmp; }

         if ((force_nl                  == true ) ||
             (is_nl(chunk_get_next(pc)) == false) )
         {
            nl_iarf(pc, newline);
         }
      }
   }
   *end = pc;
   return comma_count;
}


static void nl_func_multi_line(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LNFD, "%s: called on %u:%u '%s' [%s/%s]\n",
           __func__, start->orig_line, start->orig_col,
           start->text(), get_token_name(start->type), get_token_name(start->ptype));

   bool add_start;
   bool add_args;
   bool add_end;

   if (is_ptype(start, CT_FUNC_DEF, CT_FUNC_CLASS_DEF))
   {
      add_start = is_true(UO_nl_func_def_start_multi_line);
      add_args  = is_true(UO_nl_func_def_args_multi_line );
      add_end   = is_true(UO_nl_func_def_end_multi_line  );
   }
   else if (is_ptype(start, CT_FUNC_CALL, CT_FUNC_CALL_USER))
   {
      add_start = is_true(UO_nl_func_call_start_multi_line);
      add_args  = is_true(UO_nl_func_call_args_multi_line );
      add_end   = is_true(UO_nl_func_call_end_multi_line  );
   }
   else
   {
      add_start = is_true(UO_nl_func_decl_start_multi_line);
      add_args  = is_true(UO_nl_func_decl_args_multi_line );
      add_end   = is_true(UO_nl_func_decl_end_multi_line  );
   }

   return_if((add_start == false) &&
             (add_args  == false) &&
             (add_end   == false) );

   chunk_t* pc = get_next_ncnl(start);
   while (exceeds_level(pc, start->level))
   {
      pc = get_next_ncnl(pc);
   }

   if (is_type(pc, CT_FPAREN_CLOSE) && is_newline_between(start, pc))
   {
      if (add_start && !is_nl(chunk_get_next(start))) { nl_iarf(start,              AV_ADD); }
      if (add_end   && !is_nl(chunk_get_prev(pc)   )) { nl_iarf(chunk_get_prev(pc), AV_ADD); }
      if (add_args)                                   { count_commas(&pc, start,    AV_ADD); }
   }
}


static void nl_func_def(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LNFD, "%s: called on %u:%u '%s' [%s/%s]\n",
           __func__, start->orig_line, start->orig_col, start->text(),
           get_token_name(start->type), get_token_name(start->ptype));

   bool     is_def = is_ptype(start, CT_FUNC_DEF, CT_FUNC_CLASS_DEF);
   argval_t atmp   = get_arg(is_def ? UO_nl_func_def_paren : UO_nl_func_paren);
   chunk_t  *prev;
   if (atmp != AV_IGNORE)
   {
      prev = get_prev_ncnl(start);
      if (is_valid(prev)) { nl_iarf(prev, atmp); }
   }

   /* Handle break newlines type and function */
   prev = get_prev_ncnl(start);
   prev = skip_template_prev(prev );
   /* Don't split up a function variable */
   prev = is_paren_close(prev) ? nullptr : get_prev_ncnl(prev);

   if (is_type(prev, CT_DC_MEMBER) &&
       not_ignore(UO_nl_func_class_scope))
   {
      nl_iarf(get_prev_ncnl(prev), get_arg(UO_nl_func_class_scope));
   }

   if (not_type(prev, CT_PRIVATE_COLON))
   {
      chunk_t* tmp;
      if (is_type(prev, CT_OPERATOR))
      {
         tmp  = prev;
         prev = get_prev_ncnl(prev);
      }
      else
      {
         tmp = start;
      }

      if (is_type(prev, CT_DC_MEMBER))
      {
         if (not_ignore(UO_nl_func_scope_name))
         {
            nl_iarf(prev, get_arg(UO_nl_func_scope_name));
         }
      }

      chunk_t* next1 = get_next_ncnl(prev);
      if (not_type(next1, CT_FUNC_CLASS_DEF))
      {
         const uo_t option = is_ptype(tmp, CT_FUNC_PROTO) ?
               UO_nl_func_proto_type_name : UO_nl_func_type_name;
         argval_t a = get_arg(option);

         if (is_flag(tmp, PCF_IN_CLASS) &&
             not_ignore(UO_nl_func_type_name_class))
         {
            a = get_arg(UO_nl_func_type_name_class);
         }

         if (a != AV_IGNORE)
         {
            assert(is_valid(prev));
            LOG_FMT(LNFD, "%s: prev %u:%u '%s' [%s/%s]\n",
                    __func__, prev->orig_line, prev->orig_col, prev->text(),
                    get_token_name(prev->type ),
                    get_token_name(prev->ptype));

            if (is_type(prev, CT_DESTRUCTOR))
            {
               prev = get_prev_ncnl(prev);
            }

            /* If we are on a '::', step back two tokens
             * TODO: do we also need to check for '.' ? */
            while (is_type(prev, CT_DC_MEMBER) )
            {
               prev = get_prev_ncnl(prev);
               prev = skip_template_prev (prev);
               prev = get_prev_ncnl(prev);
            }

            if (not_type(prev, 5, CT_BRACE_CLOSE, CT_VBRACE_CLOSE,
                 CT_BRACE_OPEN, CT_SEMICOLON, CT_PRIVATE_COLON)
              //(prev->ptype != CT_TEMPLATE) TODO: create some examples to test the option
                )
            {
               nl_iarf(prev, a);
            }
         }
      }
   }

   chunk_t* pc = get_next_ncnl(start);
   if (is_str(pc, ")"))
   {
      atmp = get_arg(is_def ? UO_nl_func_def_empty : UO_nl_func_decl_empty);
      if (atmp != AV_IGNORE) { nl_iarf(start, atmp); }
      return;
   }

   /* Now scan for commas */
   uo_t option = (is_ptype(start, CT_FUNC_DEF, CT_FUNC_CLASS_DEF) ) ?
                   UO_nl_func_def_args : UO_nl_func_decl_args;
   uint32_t comma_count = count_commas(&pc, start, get_arg(option), true);

   argval_t as = get_arg(is_def ? UO_nl_func_def_start : UO_nl_func_decl_start);
   argval_t ae = get_arg(is_def ? UO_nl_func_def_end   : UO_nl_func_decl_end  );
   if (comma_count == 0)
   {
      atmp = get_arg(is_def ? UO_nl_func_def_start_single : UO_nl_func_decl_start_single);
      if (atmp != AV_IGNORE) { as = atmp; }

      atmp = get_arg(is_def ? UO_nl_func_def_end_single : UO_nl_func_decl_end_single);
      if (atmp != AV_IGNORE) { ae = atmp; }
   }
   nl_iarf(start, as);

   /* and fix up the close parenthesis */
   if (is_type(pc, CT_FPAREN_CLOSE) )
   {
      prev = get_prev_nnl(pc);
      if (not_type(prev, CT_FPAREN_OPEN) )
      {
         nl_iarf(prev, ae);
      }
      nl_func_multi_line(start);
   }
}


static void nl_oc_msg(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   chunk_t* sq_c = chunk_skip_to_match(start);
   return_if(is_invalid(sq_c));

   /* mark one-liner */
   bool     one_liner = true;
   chunk_t* pc;
   for (pc = chunk_get_next(start);
       (is_valid(pc)) && (pc != sq_c);
        pc = chunk_get_next(pc))
   {
      break_if(pc->level <= start->level);
      if (is_nl(pc)) { one_liner = false; }
   }

   /* we don't use the 1-liner flag, but set it anyway */
   uint64_t flags = one_liner ? PCF_ONE_LINER : 0;
   flag_series(start, sq_c, flags, flags ^ PCF_ONE_LINER);

   return_if(is_true(UO_nl_oc_msg_leave_one_liner) && one_liner);

   for (pc = get_next_ncnl(start); pc; pc = get_next_ncnl(pc))
   {
      break_if(pc->level <= start->level);
      if (is_type(pc, CT_OC_MSG_NAME)) { newline_add_before(pc); }
   }
}


static bool one_liner_nl_ok(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LNL1LINE, "%s: check [%s] parent=[%s] flags=%" PRIx64 ", on line %u, col %u - ",
           __func__, get_token_name(pc->type), get_token_name(pc->ptype),
           get_flags(pc), pc->orig_line, pc->orig_col);

   if (not_flag(pc, PCF_ONE_LINER))
   {
      LOG_FMT(LNL1LINE, "true (not one-liner), a new line may be added\n");
      return(true);
   }

   /* Step back to find the opening brace */
   chunk_t* br_open = pc;
   if (is_closing_brace(br_open))
   {
      /* determine if we search a real or virtual brace */
      const c_token_t searched_brace = is_closing_rbrace(br_open) ?
                                         CT_BRACE_OPEN : CT_VBRACE_OPEN;
      br_open = get_prev_type(br_open, searched_brace, (int32_t)br_open->level);
   }
   else
   {
      while (is_flag(br_open, PCF_ONE_LINER) && !is_any_brace(br_open))
      {
         br_open = chunk_get_prev(br_open);
      }
   }
   pc = br_open;
   if (is_flag(pc, PCF_ONE_LINER) && is_any_brace(pc))
   {
      /* \todo use a check array here */
      bool cond;
      cond = is_true(UO_nl_class_leave_one_liners);
      if (cond && is_flag(pc, PCF_IN_CLASS))     { LOG_FMT(LNL1LINE, "false (class)\n" );     return(false); }
      cond = is_true(UO_nl_assign_leave_one_liners);
      if (cond && is_ptype(pc, CT_ASSIGN))       { LOG_FMT(LNL1LINE, "false (assign)\n");     return(false); }
      cond = is_true(UO_nl_enum_leave_one_liners);
      if (cond && is_ptype(pc, CT_ENUM))         { LOG_FMT(LNL1LINE, "false (enum)\n");       return(false); }
      cond = is_true(UO_nl_getset_leave_one_liners);
      if (cond && is_ptype(pc, CT_GETSET))       { LOG_FMT(LNL1LINE, "false (get/set)\n");    return(false); }
      cond = is_true(UO_nl_func_leave_one_liners);
      if (cond && is_ptype(pc, CT_FUNC_DEF,
                         CT_FUNC_CLASS_DEF))     { LOG_FMT(LNL1LINE, "false (func def)\n");   return(false); }
      cond = is_true(UO_nl_func_leave_one_liners);
      if (cond && is_ptype(pc, CT_OC_MSG_DECL))  { LOG_FMT(LNL1LINE, "false (method def)\n"); return(false); }
      cond = is_true(UO_nl_cpp_lambda_leave_one_liners);
      if (cond && is_ptype(pc, CT_CPP_LAMBDA))   { LOG_FMT(LNL1LINE, "false (lambda)\n");     return(false); }
      cond = is_true(UO_nl_oc_msg_leave_one_liner);
      if (cond && is_flag(pc, PCF_IN_OC_MSG))    { LOG_FMT(LNL1LINE, "false (message)\n");    return(false); }
      cond = is_true(UO_nl_if_leave_one_liners);
      if (cond && is_ptype(pc, CT_IF, CT_ELSE))  { LOG_FMT(LNL1LINE, "false (if/else)\n");    return(false); }
      cond = is_true(UO_nl_while_leave_one_liners);
      if (cond && is_ptype(pc, CT_WHILE))        { LOG_FMT(LNL1LINE, "false (while)\n");      return(false); }
   }
   LOG_FMT(LNL1LINE, "true, a new line may be added\n");
   return(true);
}


void clear_one_liner_flag(chunk_t* pc, dir_e dir)
{
   chunk_t* tmp = pc;
   while ((tmp = chunk_get(tmp, scope_e::ALL, dir)) != nullptr)
   {
      return_if(not_flag(tmp, PCF_ONE_LINER));
      LOG_FMT(LNL1LINE, " %s", tmp->text());
      clr_flags(tmp, PCF_ONE_LINER);
   }
}


void undo_one_liner(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   if (is_flag(pc, PCF_ONE_LINER))
   {
      LOG_FMT(LNL1LINE, "%s: [%s]", __func__, pc->text());
      clr_flags(pc, PCF_ONE_LINER);

      clear_one_liner_flag(pc, dir_e::BEFORE);
      clear_one_liner_flag(pc, dir_e::AFTER );
      LOG_FMT(LNL1LINE, "\n");
   }
}


static void nl_create_one_liner(chunk_t* vbrace_open)
{
   LOG_FUNC_ENTRY();

   /* See if we get a newline between the next text and the vbrace_close */
   chunk_t* tmp   = get_next_ncnl(vbrace_open);
   chunk_t* first = tmp;
   return_if (is_invalid(first) ||
       (get_token_pattern_class(first->type) != pattern_class_e::NONE));


   uint32_t nl_total = 0;
   while (not_type(tmp, CT_VBRACE_CLOSE))
   {
      if (is_nl(tmp))
      {
         nl_total += tmp->nl_count;
         return_if(nl_total > 1);
      }
      tmp = chunk_get_next(tmp);
   }

   if (are_valid(tmp, first))
   {
      newline_del_between(vbrace_open, first);
   }
}


void remove_newlines(void)
{
   LOG_FUNC_ENTRY();

   chunk_t* pc = chunk_get_head();
   if (!is_nl(pc)) { pc = get_next_nl(pc); }

   chunk_t* next;
   chunk_t* prev;
   while (is_valid(pc))
   {
      /* Remove all newlines not in preproc */
      if (!is_preproc(pc))
      {
         next = pc->next;
         prev = pc->prev;
         nl_iarf(pc, AV_REMOVE);
         if (next == chunk_get_head())
         {
            pc = next;
            continue;
         }
         else if (is_valid(prev) && !is_nl(prev->next))
         {
            pc = prev;
         }
      }
      pc = get_next_nl(pc);
   }
}


#if 0
// currently not used
static void struct_union(chunk_t* start, argval_t nl_opt, bool leave_trailing)
{
   LOG_FUNC_ENTRY();
   return_if (is_arg(nl_opt, AV_IGNORE) ||
       ((start->flags & PCF_IN_PREPROC) && !get_bool(UO_nl_define_macro)));

   /* step past any junk between the keyword and the open brace
    * Quit if we hit a semicolon or '=', which are not expected. */
   size_t level = start->level;
   chunk_t* pc = start;
   while (((pc = get_next_ncnl(pc)) != nullptr) && (pc->level >= level))
   {
      break_if (is_level(pc, level) &&
         (is_type(pc, CT_BRACE_OPEN, CT_ASSIGN) || is_semicolon(pc)));
      start = pc;
   }

   /* If we hit a brace open, then we need to toy with the newlines */
   if (is_type(pc, CT_BRACE_OPEN))
   {
      /* Skip over embedded C comments */
      chunk_t* next = get_next_nc(pc);
      if (leave_trailing && !is_cmt_or_nl(next))
      {
         nl_opt = AV_IGNORE;
      }

      nl_iarf_pair(start, pc, nl_opt);
   }
}
#endif


void cleanup_braces(bool first)
{
   LOG_FUNC_ENTRY();

   /* Get the first token that's not an empty line: */
   chunk_t* pc = chunk_get_head();
   if (is_nl(pc)) { pc = get_next_ncnl(pc); }

   chunk_t* next;
   chunk_t* prev;
   chunk_t* tmp;
   for ( ; is_valid(pc); pc = get_next_ncnl(pc))
   {
      switch(pc->type)
      {
         case(CT_ELSEIF):
         {
            argval_t arg = get_arg(UO_nl_elseif_brace);
            nl_if_for_while_switch(pc, (arg != AV_IGNORE) ? arg : get_arg(UO_nl_if_brace));
         }
         break;

         case(CT_CATCH):
         {
            nl_cuddle_uncuddle(pc, get_arg(UO_nl_brace_catch));
            next = get_next_ncnl(pc);

            argval_t argval = get_arg(UO_nl_catch_brace);
            if (is_type(next, CT_BRACE_OPEN))
                 { nl_do_else            (pc, argval); }
            else { nl_if_for_while_switch(pc, argval); }
         }
         break;

         /* \todo use check array here */
         case(CT_IF          ): nl_if_for_while_switch(pc, get_arg(UO_nl_if_brace           )); break;
         case(CT_FOR         ): nl_if_for_while_switch(pc, get_arg(UO_nl_for_brace          )); break;
         case(CT_WHILE       ): nl_if_for_while_switch(pc, get_arg(UO_nl_while_brace        )); break;
         case(CT_USING_STMT  ): nl_if_for_while_switch(pc, get_arg(UO_nl_using_brace        )); break;
         case(CT_D_SCOPE_IF  ): nl_if_for_while_switch(pc, get_arg(UO_nl_scope_brace        )); break;
         case(CT_D_VERSION_IF): nl_if_for_while_switch(pc, get_arg(UO_nl_version_brace      )); break;
         case(CT_SWITCH      ): nl_if_for_while_switch(pc, get_arg(UO_nl_switch_brace       )); break;
         case(CT_SYNCHRONIZED): nl_if_for_while_switch(pc, get_arg(UO_nl_synchronized_brace )); break;
         case(CT_UNITTEST    ): nl_do_else            (pc, get_arg(UO_nl_unittest_brace     )); break;
         case(CT_DO          ): nl_do_else            (pc, get_arg(UO_nl_do_brace           )); break;
         case(CT_TRY         ): nl_do_else            (pc, get_arg(UO_nl_try_brace          )); break;
         case(CT_GETSET      ): nl_do_else            (pc, get_arg(UO_nl_getset_brace       )); break;
         case(CT_WHILE_OF_DO ): nl_cuddle_uncuddle    (pc, get_arg(UO_nl_brace_while        )); break;
         case(CT_STRUCT      ): nl_struct_enum_union  (pc, get_arg(UO_nl_struct_brace), true ); break;
         case(CT_UNION       ): nl_struct_enum_union  (pc, get_arg(UO_nl_union_brace ), true ); break;
         case(CT_ENUM        ): nl_struct_enum_union  (pc, get_arg(UO_nl_enum_brace  ), true );
                                nl_enum(pc);break;
         case(CT_NAMESPACE   ): nl_struct_enum_union  (pc, get_arg(UO_nl_namespace_brace), false); break;

         case(CT_ELSE):
            nl_cuddle_uncuddle(pc, get_arg(UO_nl_brace_else));
            next = get_next_ncnl(pc);
            if (is_type(next, CT_ELSEIF))
            {
               nl_iarf_pair(pc, next, get_arg(UO_nl_else_if));
            }
            nl_do_else(pc, get_arg(UO_nl_else_brace));
         break;

         case(CT_FINALLY):
            nl_cuddle_uncuddle(pc, get_arg(UO_nl_brace_finally));
            nl_do_else(pc, get_arg(UO_nl_finally_brace));
         break;

         case(CT_BRACE_OPEN):
            if (is_ptype(pc, CT_DOUBLE_BRACE) &&
                not_ignore(UO_nl_paren_dbrace_open))
            {
               prev = get_prev_ncnl(pc, scope_e::PREPROC);
               if (is_paren_close(prev))
               {
                  nl_iarf_pair(prev, pc, get_arg(UO_nl_paren_dbrace_open));
               }
            }

            if (not_ignore(UO_nl_brace_brace))
            {
               next = get_next_nc(pc, scope_e::PREPROC);
               if (is_type(next, CT_BRACE_OPEN))
               {
                  nl_iarf_pair(pc, next, get_arg(UO_nl_brace_brace));
               }
            }

            if (is_ptype(pc, CT_ENUM) &&
                not_ignore(UO_nl_enum_own_lines))
            {
               nl_enum_entries(pc, get_arg(UO_nl_enum_own_lines));
            }

            if (is_true(UO_nl_ds_struct_enum_cmt) &&
                is_ptype(pc, CT_ENUM, CT_STRUCT, CT_UNION ) )
            {
               nl_double_space_struct_enum_union(pc);
            }

            if (is_ptype(pc, CT_CLASS) &&
                is_level(pc, pc->brace_level))
            {
               nl_do_else(get_prev_nnl(pc), get_arg(UO_nl_class_brace));
            }

            if (is_ptype(pc, CT_OC_BLOCK_EXPR))
            {
               nl_iarf_pair(chunk_get_prev(pc), pc, get_arg(UO_nl_oc_block_brace));
            }

            next = get_next_nnl(pc);
            if (is_invalid(next))
            {
               // do nothing
            }
            else if (is_type(next, CT_BRACE_CLOSE))
            {
               /* \todo: add an option to split open empty statements? { }; */
            }
            else if (is_type(next, CT_BRACE_OPEN))
            {
               /* already handled */
            }
            else
            {
               next = get_next_ncnl(pc);

               /* Handle nl_after_brace_open */
               if ((is_ptype(pc, CT_CPP_LAMBDA  ) ||
                    is_level(pc, pc->brace_level) ) &&
                   is_true(UO_nl_after_brace_open))
               {
                  if (!one_liner_nl_ok(pc))
                  {
                     LOG_FMT(LNL1LINE, "a new line may NOT be added\n");
                     /* no change - preserve one liner body */
                  }
                  else if (is_flag(pc, PCF_IN_ARRAY_ASSIGN) || is_preproc(pc))
                  {
                     /* no change - don't break up array assignments or preprocessors */
                  }
                  else
                  {
                     /* Step back from next to the first non-newline item */
                     tmp = chunk_get_prev(next);
                     assert(is_valid(tmp));
                     while (tmp != pc)
                     {
                        if (is_cmt(tmp))
                        {
                           break_if(is_false(UO_nl_after_brace_open_cmt) &&
                                    not_type(tmp, CT_COMMENT_MULTI));
                        }
                        tmp = chunk_get_prev(tmp);
                     }
                     /* Add the newline */
                     nl_iarf(tmp, AV_ADD);
                  }
               }
            }
            nl_brace_pair(pc);
         break;

         case(CT_BRACE_CLOSE):
            if (not_ignore(UO_nl_brace_brace))
            {
               next = get_next_nc(pc, scope_e::PREPROC);
               if (is_type(next, CT_BRACE_CLOSE))
               {
                  nl_iarf_pair(pc, next, get_arg(UO_nl_brace_brace));
               }
            }

            if (not_ignore(UO_nl_brace_square))
            {
               next = get_next_nc(pc, scope_e::PREPROC);
               if (is_type(next, CT_SQUARE_CLOSE))
               {
                  nl_iarf_pair(pc, next, get_arg(UO_nl_brace_square));
               }
            }

            if (not_ignore(UO_nl_brace_fparen))
            {
               next = get_next_nc(pc, scope_e::PREPROC);
               if ( is_type(next, CT_NEWLINE) &&
                   (get_arg(UO_nl_brace_fparen) == AV_REMOVE))
               {
                  next = get_next_nc(next, scope_e::PREPROC);
               }
               if (is_type(next, CT_FPAREN_CLOSE))
               {
                  nl_iarf_pair(pc, next, get_arg(UO_nl_brace_fparen));
               }
            }

            if (is_true(UO_eat_blanks_before_close_brace))
            {
               /* Limit the newlines before the close brace to 1 */
               prev = chunk_get_prev(pc);
               assert(is_valid(prev));
               if (is_nl(prev))
               {
                  if (prev->nl_count != 1)
                  {
                     prev->nl_count = 1;
                     LOG_FMT(LBLANKD, "%s: eat_blanks_before_close_brace %u\n", __func__, prev->orig_line);
                     MARK_CHANGE();
                  }
               }
            }
            else if (is_true(UO_nl_ds_struct_enum_close_brace) &&
                     (is_ptype(pc, CT_ENUM, CT_STRUCT, CT_UNION)))
            {
               if (not_flag(pc, PCF_ONE_LINER))
               {
                  /* Make sure the brace is preceded by two newlines */
                  prev = chunk_get_prev(pc);
                  assert(is_valid(prev));

                  if (!is_nl(prev)) { prev = newline_add_before(pc); }
                  assert(is_valid(prev));

                  if (prev->nl_count < 2) { double_newline(prev); }
               }
            }

            /* Force a newline after a close brace */
            if (not_ignore(UO_nl_brace_struct_var) &&
                (is_ptype(pc, CT_STRUCT, CT_ENUM, CT_UNION ) ) )
            {
               next = get_next_ncnl(pc, scope_e::PREPROC);
               if (not_type(next, CT_SEMICOLON, CT_COMMA))
               {
                  nl_iarf(pc, get_arg(UO_nl_brace_struct_var));
               }
            }
            else if (is_true(UO_nl_after_brace_close) ||
                     is_ptype(pc, CT_FUNC_CLASS_DEF, CT_FUNC_DEF, CT_OC_MSG_DECL))
            {
               next = chunk_get_next(pc);
               if (not_type(next, 7, CT_SEMICOLON, CT_COMMA, CT_SPAREN_CLOSE,
                     CT_SQUARE_CLOSE, CT_FPAREN_CLOSE, CT_WHILE_OF_DO, CT_VBRACE_CLOSE) &&
                   not_flag(pc, (PCF_IN_ARRAY_ASSIGN | PCF_IN_TYPEDEF)) &&
                   !is_cmt_or_nl(next))
               {
                  newline_end_newline(pc);
               }
            }
         break;

         case(CT_VBRACE_OPEN):
            if (is_true(UO_nl_after_vbrace_open) ||
                is_true(UO_nl_after_vbrace_open_empty) )
            {
               next = chunk_get_next(pc, scope_e::PREPROC);
               assert(is_valid(next));
               bool add_it;
               if (is_semicolon(next))
               {
                  add_it = is_true(UO_nl_after_vbrace_open_empty);
               }
               else
               {
                  add_it = (is_true(UO_nl_after_vbrace_open) &&
                            not_type(next, CT_VBRACE_CLOSE) &&
                            !is_cmt_or_nl(next));
               }
               if (add_it) { nl_iarf(pc, AV_ADD); }
            }

            if ((is_ptype(pc, 3, CT_IF, CT_ELSEIF, CT_ELSE)
                                        && is_true(UO_nl_create_if_one_liner   )) ||
                (is_ptype(pc, CT_FOR  ) && is_true(UO_nl_create_for_one_liner  )) ||
                (is_ptype(pc, CT_WHILE) && is_true(UO_nl_create_while_one_liner)) )
            {
               nl_create_one_liner(pc);
            }
            if ((is_ptype(pc, 3, CT_IF, CT_ELSEIF, CT_ELSE)
                                        && is_true(UO_nl_split_if_one_liner   )) ||
                (is_ptype(pc, CT_FOR  ) && is_true(UO_nl_split_for_one_liner  )) ||
                (is_ptype(pc, CT_WHILE) && is_true(UO_nl_split_while_one_liner)) )
            {
               if (is_flag(pc, PCF_ONE_LINER))
               {
                  /* split one-liner */
                  const chunk_t* end = chunk_get_next(get_next_type(pc->next, CT_SEMICOLON)); /* \todo -1 is invalid enum */
                  /* Scan for clear flag */

                  LOG_FMT(LGUY, "\n");
                  for (chunk_t* temp = pc; temp != end; temp = chunk_get_next(temp))
                  {
                     LOG_FMT(LGUY, "%s type=%s , level=%u", temp->text(), get_token_name(temp->type), temp->level);
                     log_pcf_flags(LGUY, get_flags(temp));
                     clr_flags(temp, PCF_ONE_LINER);
                  }
                  // split
                  newline_add_between(pc, pc->next);
               }
            }
         break;

         case(CT_VBRACE_CLOSE):
            if (is_true(UO_nl_after_vbrace_close))
            {
               if (!is_nl(get_next_nc(pc))) { nl_iarf(pc, AV_ADD); }
            }
         break;

         case(CT_CASE):
            /* Note: 'default' also maps to CT_CASE */
            if (is_true(UO_nl_before_case)) { nl_case(pc); }
         break;

         case(CT_THROW):
            prev = chunk_get_prev(pc);
            if (is_type(prev, CT_PAREN_CLOSE) )
            {
               nl_iarf(get_prev_ncnl(pc), get_arg(UO_nl_before_throw));
            }
         break;

         case(CT_CASE_COLON):
            next = get_next_nnl(pc);
            if (is_type(next, CT_BRACE_OPEN) &&
                not_ignore(UO_nl_case_colon_brace))
            {
               nl_iarf(pc, get_arg(UO_nl_case_colon_brace));
            }
            else if (is_true(UO_nl_after_case))
            {
               nl_case_colon(pc);
            }
         break;

         case(CT_SPAREN_CLOSE):
            next = get_next_ncnl(pc);
            if (is_type(next, CT_BRACE_OPEN))
            {
               /* \TODO: this could be used to control newlines between the
                * the if/while/for/switch close parenthesis and the open brace, but
                * that is currently handled elsewhere. */
            }
         break;

         case(CT_RETURN):
            if (is_true(UO_nl_before_return)) { nl_before_return(pc); }
            if (is_true(UO_nl_after_return )) { nl_after_return (pc); }
         break;

         case(CT_SEMICOLON):
            if (not_flag(pc, (PCF_IN_SPAREN | PCF_IN_PREPROC)) &&
                is_true(UO_nl_after_semicolon))
            {
               next = chunk_get_next(pc);
               while (is_type(next, CT_VBRACE_CLOSE))
               {
                  next = chunk_get_next(next);
               }

               if (is_valid(next) && !is_cmt_or_nl(next))
               {
                  if (one_liner_nl_ok(next))
                  {
                     LOG_FMT(LNL1LINE, "a new line may be added\n");
                     nl_iarf(pc, AV_ADD);
                  }
                  else
                  {
                     LOG_FMT(LNL1LINE, "a new line may NOT be added\n");
                  }
               }
            }
            else if (is_ptype(pc, CT_CLASS))
            {
               if (get_uval(UO_nl_after_class) > 0)
               {
                  nl_iarf(pc, AV_ADD);
               }
            }
         break;

         case(CT_FPAREN_OPEN):
            if ((is_ptype(pc, 5, CT_FUNC_CLASS_DEF,   CT_FUNC_DEF,
                  CT_FUNC_PROTO, CT_FUNC_CLASS_PROTO, CT_OPERATOR)) &&
                ((is_true(UO_nl_func_decl_start_multi_line) ) ||
                 (is_true(UO_nl_func_def_start_multi_line)  ) ||
                 (is_true(UO_nl_func_decl_args_multi_line)  ) ||
                 (is_true(UO_nl_func_def_args_multi_line)   ) ||
                 (is_true(UO_nl_func_decl_end_multi_line)   ) ||
                 (is_true(UO_nl_func_def_end_multi_line)    ) ||
                 not_ignore(UO_nl_func_decl_start           ) ||
                 not_ignore(UO_nl_func_def_start            ) ||
                 not_ignore(UO_nl_func_decl_start_single    ) ||
                 not_ignore(UO_nl_func_def_start_single     ) ||
                 not_ignore(UO_nl_func_decl_args            ) ||
                 not_ignore(UO_nl_func_def_args             ) ||
                 not_ignore(UO_nl_func_decl_end             ) ||
                 not_ignore(UO_nl_func_def_end              ) ||
                 not_ignore(UO_nl_func_decl_end_single      ) ||
                 not_ignore(UO_nl_func_def_end_single       ) ||
                 not_ignore(UO_nl_func_decl_empty           ) ||
                 not_ignore(UO_nl_func_def_empty            ) ||
                 not_ignore(UO_nl_func_type_name            ) ||
                 not_ignore(UO_nl_func_type_name_class      ) ||
                 not_ignore(UO_nl_func_class_scope          ) ||
                 not_ignore(UO_nl_func_scope_name           ) ||
                 not_ignore(UO_nl_func_proto_type_name      ) ||
                 not_ignore(UO_nl_func_paren                ) ||
                 not_ignore(UO_nl_func_def_paren            )))
            {
               nl_func_def(pc);
            }
            else if ( is_ptype(pc, CT_FUNC_CALL, CT_FUNC_CALL_USER) &&
                     (is_true(UO_nl_func_call_start_multi_line) ||
                      is_true(UO_nl_func_call_args_multi_line ) ||
                      is_true(UO_nl_func_call_end_multi_line  ) ) )
            {
               nl_func_multi_line(pc);
            }
            else if (first && (get_uval(UO_nl_remove_extra_newlines) == 1))
            {
               nl_iarf(pc, AV_REMOVE);
            }
         break;

         case(CT_ANGLE_CLOSE):
            if (is_ptype(pc, CT_TEMPLATE))
            {
               next = get_next_ncnl(pc);
               if (is_level(next, next->brace_level))
               {
                  tmp = get_prev_ncnl(get_prev_type(pc, CT_ANGLE_OPEN, (int32_t)pc->level));
                  if (is_type(tmp, CT_TEMPLATE))
                  {
                     nl_iarf(pc, get_arg(UO_nl_template_class));
                  }
               }
            }
         break;

         case(CT_SQUARE_OPEN):
            if (is_ptype(pc, CT_OC_MSG))
            {
               if (is_true(UO_nl_oc_msg_args)) { nl_oc_msg(pc); }
            }

            if (is_ptype(pc, CT_ASSIGN) && not_flag(pc, PCF_ONE_LINER))
            {
               tmp = get_prev_ncnl(pc);
               nl_iarf(tmp, get_arg(UO_nl_assign_square));

               argval_t arg = get_arg(UO_nl_after_square_assign);

               if (is_arg_set(UO_nl_assign_square, AV_ADD))
               {
                  arg = AV_ADD;
               }
               nl_iarf(pc, arg);

               /* if there is a newline after the open, then force a newline
                * before the close */
               tmp = get_next_nc(pc);
               if (is_nl(tmp))
               {
                  tmp = get_next_type(pc, CT_SQUARE_CLOSE, (int32_t)pc->level);
                  if (is_valid(tmp))
                  {
                     newline_add_before(tmp);
                  }
               }
            }
         break;

         case(CT_PRIVATE):
            /** Make sure there is a newline before an access spec */
            if (get_uval(UO_nl_before_access_spec) > 0)
            {
               prev = chunk_get_prev(pc);
               if (!is_nl(prev))
               {
                  newline_add_before(pc);
               }
            }
         break;

         case(CT_PRIVATE_COLON):
            /** Make sure there is a newline after an access spec */
            if (get_uval(UO_nl_after_access_spec) > 0)
            {
               next = chunk_get_next(pc);
               if (!is_nl(next))
               {
                  newline_add_before(next);
               }
            }
         break;

         case(CT_PP_DEFINE):
            if (is_true(UO_nl_multi_line_define))
            {
               handle_define(pc);
            }
         break;

         default:
            if ((first == true) &&
                (is_preproc(pc) && (get_uval(UO_nl_remove_extra_newlines) == 1)))
            {
               nl_iarf(pc, AV_REMOVE);
            }
            else
            {
               /* ignore it */
            }
         break;
      }
   }
   nl_def_blk(chunk_get_head(), false);
}


static void handle_define(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   chunk_t* nl  = pc;
   while ((nl = chunk_get_next(nl)) != nullptr)
   {
      return_if(is_type(nl, CT_NEWLINE));

      static chunk_t* ref = nullptr;
      if (is_type          (nl, CT_MACRO                      ) ||
          is_type_and_ptype(nl, CT_FPAREN_CLOSE, CT_MACRO_FUNC) )
      {
         ref = nl;
      }
      if (is_type(nl, CT_NL_CONT))
      {
         newline_add_after(ref);
         return;
      }
   }
}


void nl_after_multiline_cmt(void)
{
   LOG_FUNC_ENTRY();

   for(chunk_t* pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if(not_type(pc, CT_COMMENT_MULTI));

      chunk_t* tmp = pc;
      while (((tmp = chunk_get_next(tmp)) != nullptr) &&
                    !is_nl(tmp))
      {
         if (!is_cmt(tmp))
         {
            newline_add_before(tmp);
            break;
         }
      }
   }
}


void newline_after_label_colon(void)
{
   LOG_FUNC_ENTRY();

   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if(not_type(pc, CT_LABEL_COLON));
      newline_add_after(pc);
   }
}


void add_nl_before_and_after(
   chunk_t* pc,
   uo_t     option
);


void insert_blank_lines(void)
{
   LOG_FUNC_ENTRY();

   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      switch(pc->type)
      {
         case(CT_IF              ): add_nl_before_and_after(pc, UO_nl_before_if          ); break;
         case(CT_FOR             ): add_nl_before_and_after(pc, UO_nl_before_for         ); break;
         case(CT_WHILE           ): add_nl_before_and_after(pc, UO_nl_before_while       ); break;
         case(CT_SWITCH          ): add_nl_before_and_after(pc, UO_nl_before_switch      ); break;
         case(CT_SYNCHRONIZED    ): add_nl_before_and_after(pc, UO_nl_before_synchronized); break;
         case(CT_DO              ): add_nl_before_and_after(pc, UO_nl_before_do          ); break;
         case(CT_FUNC_CLASS_DEF  ): /* fallthrough */
         case(CT_FUNC_DEF        ): /* fallthrough */
         case(CT_FUNC_CLASS_PROTO): /* fallthrough */
         case(CT_FUNC_PROTO      ): nl_func_pre_blank_lines(pc);                      break;
         default:                   /* ignore it   */                                 break;
      }
   }
}


void add_nl_before_and_after(chunk_t* pc, uo_t option)
{
   nl_if_for_while_switch_pre_blank_lines (pc, get_arg(option                ));
   nl_if_for_while_switch_post_blank_lines(pc, get_arg(get_inverse_uo(option)));
}


void newlines_functions_remove_extra_blank_lines(void)
{
   LOG_FUNC_ENTRY();

   const uint32_t nl_max_blank_in_func = get_uval(UO_nl_max_blank_in_func);
   return_if (nl_max_blank_in_func == 0);

   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if (not_type (pc, CT_BRACE_OPEN) ||
                   not_ptype(pc, 2, CT_FUNC_DEF, CT_CPP_LAMBDA) );

      const uint32_t startMoveLevel = pc->level;

      while (is_valid(pc))
      {
         break_if(is_type_and_level(pc, CT_BRACE_CLOSE, startMoveLevel));
         /* delete newlines */
         if (pc->nl_count > nl_max_blank_in_func)
         {
            pc->nl_count = nl_max_blank_in_func;
            MARK_CHANGE();
            remove_next_nl(pc);
         }
         else
         {
            pc = chunk_get_next(pc);
         }
      }
   }
}


void newlines_squeeze_ifdef(void)
{
   LOG_FUNC_ENTRY();

   chunk_t* pc;
   for (pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      if (is_type(pc, CT_PREPROC) &&
          (pc->level > 0 || is_true(UO_nl_squeeze_ifdef_top_level)))
      {
         chunk_t* ppr = chunk_get_next(pc);
         assert(is_valid(ppr));

         if (is_type(ppr, CT_PP_IF, CT_PP_ELSE, CT_PP_ENDIF) )
         {
            chunk_t* pnl = nullptr;
            chunk_t* nnl = get_next_nl(ppr);
            if (is_type(ppr, CT_PP_ELSE, CT_PP_ENDIF))
            {
               pnl = get_prev_nl(pc);
            }

            if (is_valid(nnl))
            {
               if (is_valid(pnl))
               {
                  if (pnl->nl_count > 1)
                  {
                     //nnl->nl_count += pnl->nl_count - 1;
                     pnl->nl_count = 1;
                     MARK_CHANGE();

                     const chunk_t* tmp1 = get_prev_nnl(pnl);
                     const chunk_t* tmp2 = get_prev_nnl(nnl);
                     assert(are_valid(tmp1, tmp2));

                     LOG_FMT(LNEWLINE, "%s: moved from after line %u to after %u\n",
                             __func__, tmp1->orig_line, tmp2->orig_line);
                  }
               }

               if (is_type(ppr, CT_PP_IF, CT_PP_ELSE))
               {
                  if (nnl->nl_count > 1)
                  {
                     const chunk_t* tmp1 = get_prev_nnl(nnl);
                     assert(is_valid(tmp1));
                     LOG_FMT(LNEWLINE, "%s: trimmed newlines after line %u from %u\n",
                             __func__, tmp1->orig_line, nnl->nl_count);
                     nnl->nl_count = 1;
                     MARK_CHANGE();
                  }
               }
            }
         }
      }
   }
}


void nl_eat_start_and_end(void)
{
   LOG_FUNC_ENTRY();
   nl_eat_start();
   nl_eat_end();
}


static void nl_eat(const dir_e dir)
{
   const char*    str     = (dir == dir_e::AFTER) ? "end"                 : "start";
   const uo_t     opt     = (dir == dir_e::AFTER) ? UO_nl_end_of_file     : UO_nl_start_of_file;
   const uo_t     opt_min = (dir == dir_e::AFTER) ? UO_nl_end_of_file_min : UO_nl_start_of_file_min;
         chunk_t* pc      = (dir == dir_e::AFTER) ? chunk_get_tail()      : chunk_get_head();
   const argval_t arg     = get_arg (opt);
   const uint32_t min_nl  = get_uval(opt_min);

   /* Process newlines at the end or start of the file */
   if ((cpd.frag_cols == 0) &&
        (is_arg_set(arg, AV_REMOVE) ||
        (is_arg_set(arg, AV_ADD   ) && (min_nl > 0))))
   {
      if (is_valid(pc))
      {
         if (is_type(pc, CT_NEWLINE))
         {
            if (arg == AV_REMOVE)
            {
               LOG_FMT(LBLANKD, "%s: eat_blanks_%s_of_file %u\n", __func__, str, pc->orig_line);
               chunk_del(pc);
               MARK_CHANGE();
            }
            else if ((arg == AV_FORCE) || (pc->nl_count < min_nl))
            {
               if (pc->nl_count != min_nl)
               {
                  LOG_FMT(LBLANKD, "%s: set_blanks_%s_of_file %u\n", __func__, str, pc->orig_line);
                  pc->nl_count = min_nl;
                  MARK_CHANGE();
               }
            }
         }
         else if ( is_arg_set(arg, AV_ADD) && (min_nl > 0))
         {
            chunk_t chunk;
            chunk.orig_line = pc->orig_line;
            chunk.type      = CT_NEWLINE;
            chunk.nl_count  = min_nl;
            chunk_add_after(&chunk, nullptr);
            LOG_FMT(LNEWLINE, "%s: %u:%u add newline before '%s'\n",
                    __func__, pc->orig_line, pc->orig_col, pc->text());
            MARK_CHANGE();
         }
      }
   }
}

static void nl_eat_start(void) { nl_eat(dir_e::BEFORE); }
static void nl_eat_end  (void) { nl_eat(dir_e::AFTER ); }


void nl_chunk_pos(c_token_t chunk_type, tokenpos_t mode)
{
   LOG_FUNC_ENTRY();

   return_if( (is_tok_unset(mode, TP_LEAD_TRAIL_JOIN)) &&
              (chunk_type != CT_COMMA                ) );

   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      if (is_type(pc, chunk_type))
      {
         tokenpos_t mode_local;
         if (is_type(chunk_type, CT_COMMA))
         {
            /* for chunk_type == CT_COMMA
             * we get 'mode' from cpd.settings[UO_pos_comma].tp
             * BUT we must take care of cpd.settings[UO_pos_class_comma].tp
             * TODO and cpd.settings[UO_pos_constr_comma].tp */
            if (is_flag(pc, PCF_IN_CLASS_BASE))
            {
               /*  change mode */
               mode_local = cpd.settings[UO_pos_class_comma].tp;
            }
            else if (is_flag(pc, PCF_IN_ENUM))
            {
               mode_local = cpd.settings[UO_pos_enum_comma].tp;
            }
            else
            {
               mode_local = mode;
            }
         }
         else
         {
            mode_local = mode;
         }
         chunk_t* prev = get_prev_nc(pc);
         chunk_t* next = get_next_nc(pc);

         uint32_t  nl_flag = ((is_nl(prev) ? 1 : 0) |
                              (is_nl(next) ? 2 : 0));

         if (is_token_set(mode_local,TP_JOIN))
         {
            if (nl_flag & 1)
            {
               /* remove newline if not preceded by a comment */
               chunk_t* prev2 = chunk_get_prev(prev);

               if (is_valid(prev2) && !is_cmt(prev2))
               {
                  remove_next_nl(prev2);
               }
            }
            if (nl_flag & 2)
            {
               /* remove newline if not followed by a comment */
               chunk_t* next2 = chunk_get_next(next);

               if (is_valid(next2) && !is_cmt(next2))
               {
                  remove_next_nl(pc);
               }
            }
            continue;
         }

         if ( ((nl_flag == 0) && is_tok_unset(mode_local, TP_FORCE_BREAK)) ||
              ((nl_flag == 3) && is_tok_unset(mode_local, TP_FORCE      )) )
         {
            /* No newlines and not adding any or both and not forcing */
            continue;
         }

         if ( ((nl_flag == 1) && is_token_set(mode_local, TP_LEAD )) ||
              ((nl_flag == 2) && is_token_set(mode_local, TP_TRAIL)) )
         {
            /* Already a newline before (lead) or after (trail) */
            continue;
         }

         /* If there were no newlines, we need to add one */
         if (nl_flag == 0)
         {
            if (is_token_set(mode_local, TP_LEAD))
            {
               newline_add_before(pc);
            }
            else
            {
               newline_add_after(pc);
            }
            continue;
         }

         /* If there were both newlines, we need to remove one */
         if (nl_flag == 3)
         {
            if (is_token_set(mode_local, TP_LEAD))
            {
               remove_next_nl(pc);
            }
            else
            {
               remove_next_nl(get_prev_ncnl(pc));
            }
            continue;
         }

         /* we need to move the newline */
         if (is_token_set(mode_local, TP_LEAD))
         {
            const chunk_t* next2 = chunk_get_next(next);
            continue_if(is_type(next2, CT_PREPROC   ) ||
                       (is_type(next2, CT_BRACE_OPEN) && is_type(chunk_type, CT_ASSIGN)) );

            assert(is_valid(next));
            if (next->nl_count == 1)
            {
               /* move the CT_BOOL to after the newline */
               chunk_move_after(pc, next);
            }
         }
         else
         {
            assert(is_valid(prev));
            if (prev->nl_count == 1)
            {
               /* Back up to the next non-comment item */
               prev = get_prev_nc(prev);
               if (is_valid(prev) && !is_nl(prev) && !is_preproc(prev))
               {
                  chunk_move_after(pc, prev);
               }
            }
         }
      }
   }
}


void nl_class_colon_pos(c_token_t tok)
{
   LOG_FUNC_ENTRY();

   tokenpos_t tpc, pcc;
   argval_t   anc, ncia;

   if (tok == CT_CLASS_COLON)
   {
      tpc  = get_tok(UO_pos_class_colon   );
      anc  = get_arg(UO_nl_class_colon    );
      ncia = get_arg(UO_nl_class_init_args);
      pcc  = get_tok(UO_pos_class_comma   );
   }
   else /* tok == CT_CONSTR_COLON */
   {
      tpc  = get_tok(UO_pos_constr_colon   );
      anc  = get_arg(UO_nl_constr_colon    );
      ncia = get_arg(UO_nl_constr_init_args);
      pcc  = get_tok(UO_pos_constr_comma   );
   }

   const chunk_t* ccolon = nullptr;
   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      continue_if(is_invalid(ccolon) && (pc->type != tok));

      chunk_t* prev;
      chunk_t* next;
      if (is_type(pc, tok))
      {
         ccolon = pc;
         prev   = get_prev_nc(pc);
         next   = get_next_nc(pc);

         if (!is_nl(prev) &&
             !is_nl(next) &&
             is_arg_set(anc, AV_ADD))
         {
            newline_add_after(pc);
            prev = get_prev_nc(pc);
            next = get_next_nc(pc);
         }

         if (is_arg(anc, AV_REMOVE))
         {
            if (is_nl(prev) && is_safe_to_del_nl(prev)) // \todo DRY
            {
               chunk_del(prev);
               MARK_CHANGE();
               prev = get_prev_nc(pc);
            }
            if (is_nl(next) && is_safe_to_del_nl(next)) // \todo DRY
            {
               chunk_del(next);
               MARK_CHANGE();
               next = get_next_nc(pc);
            }
         }

         assert(are_valid(next, prev));
         if (is_token_set(tpc, TP_TRAIL)) // \todo DRY
         {
            if (is_nl(prev) && (prev->nl_count == 1) &&
                is_safe_to_del_nl(prev))
            {
               swap_chunks(pc, prev);
            }
         }
         else if (is_token_set(tpc, TP_LEAD)) // \todo DRY
         {
            if (is_nl(next) && (next->nl_count == 1) &&
                is_safe_to_del_nl(next))
            {
               swap_chunks(pc, next);
            }
         }
      }
      else
      {
         if (is_type(pc, CT_BRACE_OPEN, CT_SEMICOLON ))
         {
            ccolon = nullptr;
            continue;
         }

         assert(is_valid(ccolon));
         if (is_type_and_level(pc, CT_COMMA, ccolon->level))
         {
            if (is_arg_set(ncia, AV_ADD))
            {
               if (is_token_set(pcc, TP_TRAIL)) // \todo DRY
               {
                  if (ncia == AV_FORCE) { newline_force_after(pc); }
                  else                  { newline_add_after  (pc); }
                  prev = get_prev_nc(pc);
                  if (is_nl(prev) && is_safe_to_del_nl(prev) )
                  {
                     chunk_del(prev);
                     MARK_CHANGE();
                  }
               }
               else if (is_token_set(pcc, TP_LEAD)) // \todo DRY
               {
                  if (ncia == AV_FORCE) { newline_force_before(pc); }
                  else                  { newline_add_before  (pc); }

                  next = get_next_nc(pc);
                  if (is_nl(next) && is_safe_to_del_nl(next))
                  {
                     chunk_del(next);
                     MARK_CHANGE();
                  }
               }
            }
            else if (ncia == AV_REMOVE)
            {
               next = chunk_get_next(pc);
               if (is_nl(next) && is_safe_to_del_nl(next))
               {
                  chunk_del(next);
                  MARK_CHANGE();
               }
            }
         }
      }
   }
}


#if 0
// currently not used
static void _blank_line_max(chunk_t* pc, const char* text, uo_t uo)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   const option_map_value_t* option = get_option_name(uo);
   if (option->type != AT_UNUM)
   {
      fprintf(stderr, "Program error for UO_=%d\n", static_cast<int32_t>(uo));
      fprintf(stderr, "Please make a report\n");
      exit(2);
   }
   if ((get_uval(uo) > 0) && (pc->nl_count > get_uval(uo)))
   {
      LOG_FMT(LBLANKD, "do_blank_lines: %s max line %u\n", text + 3, pc->orig_line);
      pc->nl_count = get_uval(uo);
      MARK_CHANGE();
   }
}
#endif


void do_blank_lines(void)
{
   LOG_FUNC_ENTRY();

   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if(not_type(pc, CT_NEWLINE));

      bool line_added = false;
      chunk_t* next  = chunk_get_next(pc);
      chunk_t* prev  = get_prev_nc(pc);
      chunk_t* pcmt  = chunk_get_prev(pc);
      uint32_t  old_nl = pc->nl_count;
      if (are_valid(next, prev))
      {
         LOG_FMT(LBLANK, "%s: line %u [%s][%s] vs [%s][%s] nl=%u\n",
                 __func__, pc->orig_line,    prev->text(),
                 get_token_name(prev->type), next->text(),
                 get_token_name(next->type), pc->nl_count);
      }

      /* If this is the first or the last token, pretend that there
       * is an extra line. It will be removed at the end. */
      if (pc == chunk_get_head() || is_invalid(next))
      {
         line_added = true;
         ++pc->nl_count;
      }

      /* Limit consecutive newlines */
      if ((get_uval(UO_nl_max) > 0) &&
          (pc->nl_count > get_uval(UO_nl_max)))
      {
         blank_line_set(pc, UO_nl_max);
      }

      if (!can_increase_nl(pc))
      {
         LOG_FMT(LBLANKD, "do_blank_lines: force to 1 line %u\n", pc->orig_line);
         if (pc->nl_count != 1)
         {
            pc->nl_count = 1;
            MARK_CHANGE();
         }
         continue;
      }

      /* Control blanks before multi-line comments */
      if ((get_uval(UO_nl_before_block_comment) > pc->nl_count) &&
          is_type(next, CT_COMMENT_MULTI))
      {
         /* Don't add blanks after a open brace */
         if (is_invalid (prev                            ) ||
             not_type(prev, CT_BRACE_OPEN, CT_VBRACE_OPEN) )
         {
            blank_line_set(pc, UO_nl_before_block_comment);
         }
      }

      /* Control blanks before single line C comments */
      if ((get_uval(UO_nl_before_c_comment) > pc->nl_count) &&
           is_type(next, CT_COMMENT))
      {
         /* Don't add blanks after a open brace or a comment */
         if ( (is_invalid(prev)                           )   ||
             (not_type(prev, CT_BRACE_OPEN, CT_VBRACE_OPEN) &&
              not_type(pcmt, CT_COMMENT                   ) ) )
         {
            blank_line_set(pc, UO_nl_before_c_comment);
         }
      }

      /* Control blanks before CPP comments */
      if ((get_uval(UO_nl_before_cpp_comment) > pc->nl_count) &&
           is_type(next, CT_COMMENT_CPP))
      {
         /* Don't add blanks after a open brace */
         if (( is_invalid(prev)                           )   ||
             (not_type(prev, CT_BRACE_OPEN, CT_VBRACE_OPEN) &&
              not_type(pcmt, CT_COMMENT_CPP               ) ) )
         {
            blank_line_set(pc, UO_nl_before_cpp_comment);
         }
      }

      /* Control blanks before an access spec */
      if ((get_uval(UO_nl_before_access_spec) >   0          ) &&
          (get_uval(UO_nl_before_access_spec) != pc->nl_count) &&
           is_type(next, CT_PRIVATE))
      {
         /* Don't add blanks after a open brace */
         if (is_invalid(prev) || not_type(prev, CT_BRACE_OPEN, CT_VBRACE_OPEN))
         {
            blank_line_set(pc, UO_nl_before_access_spec);
         }
      }

      /* Control blanks before a class */
      if ( is_type (prev, CT_SEMICOLON, CT_BRACE_CLOSE) &&
           is_ptype(prev, CT_CLASS                    ) )
      {
         chunk_t* tmp = get_prev_type(prev, CT_CLASS, (int32_t)prev->level);
         tmp = get_prev_nc(tmp);
         if (get_uval(UO_nl_before_class) > pc->nl_count)
         {
            blank_line_set(tmp, UO_nl_before_class);
         }
      }

      /* Control blanks after an access spec */
      if ((get_uval(UO_nl_after_access_spec) >  0           ) &&
          (get_uval(UO_nl_after_access_spec) != pc->nl_count) &&
           is_type(prev, CT_PRIVATE_COLON) )
      {
         blank_line_set(pc, UO_nl_after_access_spec);
      }

      /* Add blanks after function bodies */
      if (is_type (prev, CT_BRACE_CLOSE) &&
          is_ptype(prev, 4, CT_FUNC_DEF, CT_FUNC_CLASS_DEF,
                            CT_ASSIGN,   CT_OC_MSG_DECL) )
      {
         if (is_flag(prev, PCF_ONE_LINER))
         {
            if (get_uval(UO_nl_after_func_body_one_liner) > pc->nl_count)
            {
               blank_line_set(pc, UO_nl_after_func_body_one_liner);
            }
         }
         else
         {
            if ((is_flag(prev, PCF_IN_CLASS)) &&
                (get_uval(UO_nl_after_func_body_class) > 0))
            {
               if (get_uval(UO_nl_after_func_body_class) != pc->nl_count)
               {
                  blank_line_set(pc, UO_nl_after_func_body_class);
               }
            }
            else if (get_uval(UO_nl_after_func_body) > 0)
            {
               if (get_uval(UO_nl_after_func_body) != pc->nl_count)
               {
                  blank_line_set(pc, UO_nl_after_func_body);
               }
            }
         }
      }

      /* Add blanks after function prototypes */
      if (is_type_and_ptype(prev, CT_SEMICOLON, CT_FUNC_PROTO))
      {
         if (get_uval(UO_nl_after_func_proto) > pc->nl_count)
         {
            pc->nl_count = get_uval(UO_nl_after_func_proto);
            MARK_CHANGE();
         }
         if ((get_uval(UO_nl_after_func_proto_group) > pc->nl_count) &&
             (is_valid(next)) && not_ptype(next, CT_FUNC_PROTO))
         {
            blank_line_set(pc, UO_nl_after_func_proto_group);
         }
      }

      /* Add blanks after function class prototypes */
      if (is_type_and_ptype(prev, CT_SEMICOLON, CT_FUNC_CLASS_PROTO) )
      {
         if (get_uval(UO_nl_after_func_class_proto) > pc->nl_count)
         {
            pc->nl_count = get_uval(UO_nl_after_func_class_proto);
            MARK_CHANGE();
         }
         if ((get_uval(UO_nl_after_func_class_proto_group) > pc->nl_count) &&
             (is_valid(next)) && not_ptype(next, CT_FUNC_CLASS_PROTO))
         {
            blank_line_set(pc, UO_nl_after_func_class_proto_group);
         }
      }

      /* Add blanks after struct/enum/union/class */
      if (is_type (prev,    CT_SEMICOLON, CT_BRACE_CLOSE          ) &&
          is_ptype(prev, 4, CT_STRUCT, CT_ENUM, CT_UNION, CT_CLASS) )
      {
         if (is_ptype(prev, CT_CLASS))
         {
            if (get_uval(UO_nl_after_class) > pc->nl_count)
            {
               blank_line_set(pc, UO_nl_after_class);
            }
         }
         else
         {
            if (get_uval(UO_nl_after_struct) > pc->nl_count)
            {
               blank_line_set(pc, UO_nl_after_struct);
            }
         }
      }

      /* Change blanks between a function comment and body */
      if ((get_uval(UO_nl_comment_func_def) != 0)   &&
          is_type_and_ptype(pcmt, CT_COMMENT_MULTI, CT_COMMENT_WHOLE) &&
          is_ptype         (next, CT_FUNC_CLASS_DEF, CT_FUNC_DEF    ) )
      {
         if (get_uval(UO_nl_comment_func_def) != pc->nl_count)
         {
            blank_line_set(pc, UO_nl_comment_func_def);
         }
      }

      /* Change blanks after a try-catch-finally block */
      if ((get_uval(UO_nl_after_try_catch_finally) != 0           ) &&
          (get_uval(UO_nl_after_try_catch_finally) != pc->nl_count) &&
           are_valid(prev, next                                         ) )
      {
         if (is_type (prev, CT_BRACE_CLOSE      ) &&
             is_ptype(prev, CT_CATCH, CT_FINALLY) )
         {
            if (not_type(next, CT_BRACE_CLOSE, CT_CATCH, CT_FINALLY))
            {
               blank_line_set(pc, UO_nl_after_try_catch_finally);
            }
         }
      }

      /* Change blanks after a try-catch-finally block */
      if ((get_uval(UO_nl_between_get_set) > 0            ) &&
          (get_uval(UO_nl_between_get_set) != pc->nl_count) &&
           are_valid(prev, next))
      {
         if ( is_ptype(prev, CT_GETSET                   ) &&
              not_type(next, CT_BRACE_CLOSE              ) &&
              is_type (prev, CT_BRACE_CLOSE, CT_SEMICOLON) )
         {
            blank_line_set(pc, UO_nl_between_get_set);
         }
      }

      /* Change blanks after a try-catch-finally block */
      if ((get_uval(UO_nl_around_cs_property) > 0            ) &&
          (get_uval(UO_nl_around_cs_property) != pc->nl_count) &&
          (are_valid(prev, next)                                   ) )
      {
         if (is_type_and_ptype(prev, CT_BRACE_CLOSE, CT_CS_PROPERTY) &&
             not_type         (next, CT_BRACE_CLOSE                ) )
         {
            blank_line_set(pc, UO_nl_around_cs_property);
         }
         else if (is_ptype(next, CT_CS_PROPERTY) &&
                  is_flag (next, PCF_STMT_START) )
         {
            blank_line_set(pc, UO_nl_around_cs_property);
         }
      }

      if (line_added && pc->nl_count > 1)
      {
         --pc->nl_count;
      }

      if (old_nl != pc->nl_count)
      {
         LOG_FMT(LBLANK, "   -=> changed to %u\n", pc->nl_count);
      }
   }
}


void newlines_cleanup_dup(void)
{
   LOG_FUNC_ENTRY();

   chunk_t* pc   = chunk_get_head();
   chunk_t* next = pc;
   while (is_valid(pc))
   {
      next = chunk_get_next(next);
      if (are_types(next, pc, CT_NEWLINE))
      {
         next->nl_count = max(pc->nl_count, next->nl_count);
         chunk_del(pc);
         MARK_CHANGE();
      }
      pc = next;
   }
}


static void nl_enum_entries(chunk_t* open_brace, argval_t av)
{
   LOG_FUNC_ENTRY();
   chunk_t* pc = open_brace;

   while (((pc = get_next_nc(pc)) != nullptr) &&
          (pc->level > open_brace->level))
   {
      continue_if ((pc->level != (open_brace->level + 1)) ||
                   not_type(pc, CT_COMMA));

      nl_iarf(pc, av);
   }

   nl_iarf(open_brace, av);
}


static void nl_double_space_struct_enum_union(chunk_t* open_brace)
{
   LOG_FUNC_ENTRY();
   chunk_t* pc = open_brace;

   while (((pc = get_next_nc(pc)) != nullptr) &&
           (pc->level > open_brace->level   ) )
   {
      continue_if((pc->level != (open_brace->level + 1)) ||
                   not_type(pc, CT_NEWLINE))

      /* If the newline is NOT after a comment or a brace open and
       * it is before a comment, then make sure that the newline is
       * at least doubled */
      chunk_t* prev = chunk_get_prev(pc);
      assert(is_valid(prev));
      if ((!is_cmt (prev)              ) &&
           not_type(prev, CT_BRACE_OPEN) &&
           is_cmt  (chunk_get_next(pc) ) )
      {
         if (pc->nl_count < 2)
         {
            double_newline(pc);
         }
      }
   }
}


void annotations_nl(void)
{
   LOG_FUNC_ENTRY();

   chunk_t* next;
   chunk_t* pc = chunk_get_head();
   while (((pc   = get_next_type(pc, CT_ANNOTATION)) != nullptr) &&
          ((next = get_next_nnl (pc)               ) != nullptr) )
   {
      /* find the end of this annotation */
      /* TODO: control newline between annotation and '(' ? */
      /* last token of the annotation */
      chunk_t* ae = (is_paren_open(next)) ? chunk_skip_to_match(next) : pc;
      break_if(is_invalid(ae));

      LOG_FMT(LANNOT, "%s: %u:%u annotation '%s' end@%u:%u '%s'",
              __func__, pc->orig_line, pc->orig_col, pc->text(),
              ae->orig_line, ae->orig_col, ae->text());

      next = get_next_nnl(ae);

      uo_t type = (is_type(next, CT_ANNOTATION)) ?
            UO_nl_between_annotation : UO_nl_after_annotation;
      LOG_FMT(LANNOT, " -- %s\n", get_option_name(type));
      nl_iarf(ae, get_arg(type));
   }
}


int32_t newlines_between(chunk_t* pc_start, chunk_t* pc_end, scope_e scope)
{
   retval_if(are_invalid(pc_start, pc_end), INVALID_COUNT);

   uint32_t nl_count = 0;
   chunk_t* chunk = pc_start;
   for ( ; chunk != pc_end; chunk = chunk_get_next(chunk, scope))
   {
      nl_count += chunk->nl_count;
   }

   /* check that search did end before end of file */
   return((chunk == chunk_get_tail()) ? INVALID_COUNT : (int32_t)nl_count);
}
