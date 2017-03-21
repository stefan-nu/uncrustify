/**
 * @file parens.cpp
 * Adds or removes parenthesis.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "parens.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include <cstdio>
#include <cstdlib>
#include "unc_ctype.h"
#include "uncrustify.h"


/**
 * Add an open parenthesis after first and add a close parenthesis before the last
 */
static void add_parens_between(
   chunk_t *first,  /**< [in]  */
   chunk_t *last    /**< [in]  */
);


/**
 * Scans between two parenthesis and adds additional parenthesis if needed.
 * This function is recursive. If it hits another open parenthesis, it'll call itself
 * with the new bounds.
 *
 * Adds optional parenthesis in an IF or SWITCH conditional statement.
 *
 * This basically just checks for a CT_COMPARE that isn't surrounded by parenthesis.
 * The edges for the compare are the open, close and any CT_BOOL tokens.
 *
 * This only handles VERY simple patterns:
 *   (!a && b)         => (!a && b)          -- no change
 *   (a && b == 1)     => (a && (b == 1))
 *   (a == 1 || b > 2) => ((a == 1) || (b > 2))
 *
 * FIXME: we really should bail if we transition between a preprocessor and
 *        a non-preprocessor
 */
static void check_bool_parens(
   chunk_t *popen,  /**< [in]  */
   chunk_t *pclose, /**< [in]  */
   int32_t     nest     /**< [in]  */
);


void do_parens(void)
{
   LOG_FUNC_ENTRY();

   if (is_true(UO_mod_full_paren_if_bool))
   {
      chunk_t *pc = chunk_get_head();
      while ((pc = get_next_ncnl(pc)) != nullptr)
      {
         continue_if (not_type (pc, CT_SPAREN_OPEN             ) ||
                      not_ptype(pc, 3, CT_IF, CT_ELSEIF, CT_SWITCH) );

         /* Grab the close sparen */
         chunk_t *pclose = get_next_type(pc, CT_SPAREN_CLOSE, (int32_t)pc->level, scope_e::PREPROC);
         if (is_valid(pclose))
         {
            check_bool_parens(pc, pclose, 0);
            pc = pclose;
         }
      }
   }
}


static void add_parens_between(chunk_t *first, chunk_t *last)
{
   LOG_FUNC_ENTRY();

   if (is_invalid(first) ||
       is_invalid(last ) ) { return; }

   LOG_FMT(LPARADD, "%s: line %u between %s [lvl=%u] and %s [lvl=%u]\n",
           __func__, first->orig_line,
           first->text(), first->level, last->text(),  last->level);

   /* Don't do anything if we have a bad sequence, ie "&& )" */
   chunk_t *first_n = get_next_ncnl(first);
   assert(is_valid(first_n));
   return_if(first_n == last);

   chunk_t pc;
   pc.type        = CT_PAREN_OPEN;
   pc.str         = "(";
   set_flags(&pc, get_flags(first_n, PCF_COPY_FLAGS));
   pc.level       = first_n->level;
   pc.pp_level    = first_n->pp_level;
   pc.brace_level = first_n->brace_level;

   chunk_add_before(&pc, first_n);

   chunk_t *last_p = get_prev_ncnl(last, scope_e::PREPROC);
   assert(is_valid(last_p));
   pc.type        = CT_PAREN_CLOSE;
   pc.str         = ")";
   set_flags(&pc, get_flags(last_p, PCF_COPY_FLAGS));
   pc.level       = last_p->level;
   pc.pp_level    = last_p->pp_level;
   pc.brace_level = last_p->brace_level;

   chunk_add_after(&pc, last_p);

   for (chunk_t *tmp = first_n;
        tmp != last_p;
        tmp = get_next_ncnl(tmp))
   {
      tmp->level++;
   }
   last_p->level++;
}


static void check_bool_parens(chunk_t *popen, chunk_t *pclose, int32_t nest)
{
   LOG_FUNC_ENTRY();

   chunk_t *ref        = popen;
   bool    hit_compare = false;

   LOG_FMT(LPARADD, "%s(%d): popen on %u, col %u, pclose on %u, col %u, level=%u\n",
           __func__, nest, popen->orig_line, popen->orig_col,
           pclose->orig_line, pclose->orig_col, popen->level);

   chunk_t *pc = popen;
   while (((pc = get_next_ncnl(pc)) != nullptr) && (pc != pclose))
   {
      if (is_preproc(pc))
      {
         LOG_FMT(LPARADD2, " -- bail on PP %s [%s] at line %u col %u, level %u\n",
                 get_token_name(pc->type),
                 pc->text(), pc->orig_line, pc->orig_col, pc->level);
         return;
      }

      if (is_type(pc, CT_BOOL, CT_QUESTION, CT_COND_COLON, CT_COMMA))
      {
         LOG_FMT(LPARADD2, " -- %s [%s] at line %u col %u, level %u\n",
                 get_token_name(pc->type),
                 pc->text(), pc->orig_line, pc->orig_col, pc->level);
         if (hit_compare)
         {
            hit_compare = false;
            add_parens_between(ref, pc);
         }
         ref = pc;
      }
      else if (is_type(pc, CT_COMPARE))
      {
         LOG_FMT(LPARADD2, " -- compare [%s] at line %u col %u, level %u\n",
                 pc->text(), pc->orig_line, pc->orig_col, pc->level);
         hit_compare = true;
      }
      else if (is_paren_open(pc))
      {
         chunk_t *next = chunk_skip_to_match(pc);
         if (is_valid(next))
         {
            check_bool_parens(pc, next, nest + 1);
            pc = next;
         }
      }
      else if (is_type(pc, CT_BRACE_OPEN, CT_SQUARE_OPEN, CT_ANGLE_OPEN))
      {
         /* Skip [], {}, and <> */
         pc = chunk_skip_to_match(pc);
      }
   }

   if (hit_compare && (ref != popen))
   {
      add_parens_between(ref, pclose);
   }
}
