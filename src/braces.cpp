/**
 * @file braces.cpp
 * Adds or removes braces.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "braces.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include <cstdio>
#include <cstdlib>
#include "unc_ctype.h"
#include "uncrustify.h"
#include "combine.h"
#include "newlines.h"
#include "options.h"
#include "token_enum.h"


/**
 * Abbreviations used:
 * - vbrace = virtual brace
 * - vbopen = virtual opening brace
 */

/**
 * Converts a single brace into a virtual brace
 */
static void convert_brace(
   chunk_t *br /**< [in]  */
);


/**
 * Converts a single virtual brace into a brace
 */
static void convert_vbrace(
   chunk_t *br /**< [in]  */
);


/**
 * tbd
 */
static void convert_all_vbrace_to_brace(void);


/**
 * Go backwards to honor brace newline removal limits
 */
static void examine_braces(void);


/**
 * Step forward and count the number of semicolons at the current level.
 * Abort if more than 1 or if we enter a preprocessor
 */
static void examine_brace(
   chunk_t *bopen /**< [in]  */
);


/**
 * tbd
 */
static void move_case_break(void);


/**
 * tbd
 */
static void mod_case_brace(void);


/**
 * tbd
 */
static void mod_full_brace_if_chain(void);


/**
 * Checks to see if the braces can be removed.
 *  - less than a certain length
 *  - doesn't mess up if/else stuff
 */
static bool can_remove_braces(
   chunk_t *bopen /**< [in]  */
);


/**
 * Checks to see if the virtual braces should be converted to real braces.
 *  - over a certain length
 *
 * @param vbopen Virtual Brace Open chunk
 * @return true (convert to real braces) or false (leave alone)
 */
static bool should_add_braces(
   chunk_t *vbopen /**< [in]  */
);


/**
 * Collect the text into txt that contains the full tag name.
 * Mainly for collecting namespace 'a.b.c' or function 'foo::bar()' names.
 */
static void append_tag_name(
   unc_text &txt, /**< [in]  */
   chunk_t* pc    /**< [in]  */
);


/**
 * Remove the case brace, if allowable.
 */
static chunk_t *mod_case_brace_remove(
   chunk_t *br_open /**< [in]  */
);


/**
 * Add the case brace, if allowable.
 */
static chunk_t *mod_case_brace_add(
   chunk_t *cl_colon /**< [in]  */
);


/**
 * Traverse the if chain and see if all can be removed
 */
static void process_if_chain(
   chunk_t *br_start /**< [in]  */
);

/* defines to make checking options easier \todo can be  */
#define is_opt(option, value) is_option_set(cpd.settings[option].a, (value)  )
#define is_opt_add   (option) is_option_set(cpd.settings[option].a, AV_ADD   )
#define is_opt_remove(option) is_option_set(cpd.settings[option].a, AV_REMOVE)
#define is_opt_force (option) is_option_set(cpd.settings[option].a, AV_FORCE )
#define is_opt_ignore(option) is_option_set(cpd.settings[option].a, AV_IGNORE)


void do_braces(void)
{
   LOG_FUNC_ENTRY();

   if (cpd.settings[UO_mod_full_brace_if_chain     ].b ||
       cpd.settings[UO_mod_full_brace_if_chain_only].b)
   {
      mod_full_brace_if_chain();
   }

   if (is_opt(UO_mod_full_brace_if   , AV_REMOVE) ||
       is_opt(UO_mod_full_brace_do   , AV_REMOVE) ||
       is_opt(UO_mod_full_brace_for  , AV_REMOVE) ||
       is_opt(UO_mod_full_brace_using, AV_REMOVE) ||
       is_opt(UO_mod_full_brace_while, AV_REMOVE) )
   {
      examine_braces();
   }

   /* convert vbraces if needed */
   if ( is_opt(UO_mod_full_brace_if      , AV_ADD) ||
        is_opt(UO_mod_full_brace_do      , AV_ADD) ||
        is_opt(UO_mod_full_brace_for     , AV_ADD) ||
        is_opt(UO_mod_full_brace_function, AV_ADD) ||
        is_opt(UO_mod_full_brace_using   , AV_ADD) ||
        is_opt(UO_mod_full_brace_while   , AV_ADD) )
   {
      convert_all_vbrace_to_brace();
   }

   /* Mark one-liners */
   chunk_t *pc = chunk_get_head();
   while ((pc = get_next_ncnl(pc)) != nullptr)
   {
      continue_if(not_type(pc, CT_BRACE_OPEN, CT_VBRACE_OPEN));
      chunk_t *br_open = pc;
      const c_token_t brc_type = get_inverse_type(pc->type); /* corresponds to closing type */

      /* Detect empty bodies */
      chunk_t *tmp = get_next_ncnl(pc);
      if (is_type(tmp, brc_type))
      {
         set_flags(br_open, PCF_EMPTY_BODY);
         set_flags(tmp,     PCF_EMPTY_BODY);
      }

      /* Scan for the brace close or a newline */
      tmp = br_open;
      while ((tmp = get_next_nc(tmp)) != nullptr)
      {
         break_if(is_nl(tmp));
         if ((is_type(tmp, brc_type)) &&
             (br_open->level == tmp->level) )
         {
            flag_series(br_open, tmp, PCF_ONE_LINER);
            break;
         }
      }
   }

   if (cpd.settings[UO_mod_case_brace].a != AV_IGNORE) { mod_case_brace();  }
   if (cpd.settings[UO_mod_move_case_break].b        ) { move_case_break(); }
}


static void examine_braces(void)
{
   LOG_FUNC_ENTRY();

   chunk_t *pc = chunk_get_tail();
   while (is_valid(pc))
   {
      chunk_t *prev = get_prev_type(pc, CT_BRACE_OPEN, -1);
      if (is_type(pc, CT_BRACE_OPEN ) &&
          !is_preproc(pc))
      {
         switch(pc->ptype)
         {
            case(CT_IF        ): /* fallthrough */
            case(CT_ELSE      ): /* fallthrough */
            case(CT_ELSEIF    ): if(is_option(cpd.settings[UO_mod_full_brace_if   ].a, AV_REMOVE)) examine_brace(pc); break;
            case(CT_DO        ): if(is_option(cpd.settings[UO_mod_full_brace_do   ].a, AV_REMOVE)) examine_brace(pc); break;
            case(CT_FOR       ): if(is_option(cpd.settings[UO_mod_full_brace_for  ].a, AV_REMOVE)) examine_brace(pc); break;
            case(CT_USING_STMT): if(is_option(cpd.settings[UO_mod_full_brace_using].a, AV_REMOVE)) examine_brace(pc); break;
            case(CT_WHILE     ): if(is_option(cpd.settings[UO_mod_full_brace_while].a, AV_REMOVE)) examine_brace(pc); break;
            default:             /* do nothing */ break;
         }
      }
      pc = prev;
   }
}


static bool should_add_braces(chunk_t *vbopen)
{
   LOG_FUNC_ENTRY();
   const size_t nl_max = cpd.settings[UO_mod_full_brace_nl].u;
   retval_if(nl_max == 0, false);

   LOG_FMT(LBRDEL, "%s: start on %zu : ", __func__, vbopen->orig_line);

   size_t  nl_count = 0;
   chunk_t *pc = get_next_nc(vbopen, scope_e::PREPROC);

   while( (is_valid(pc)        ) && /* chunk is valid */
          (pc->level >  vbopen->level) )  /* tbd */
   {
      if (is_nl(pc))
      {
         nl_count += pc->nl_count;
      }
      pc = get_next_nc(pc, scope_e::PREPROC);
   }

   if ((is_valid(pc)                    ) &&
       (nl_count         >  nl_max      ) &&
       (vbopen->pp_level == pc->pp_level) )
   {
      LOG_FMT(LBRDEL, " exceeded %zu newlines\n", nl_max);
      return(true);
   }
   return(false);
}


// DRY with examine_brace
static bool can_remove_braces(chunk_t *bopen)
{
   LOG_FUNC_ENTRY();

   /* Cannot remove braces inside a preprocessor */
   retval_if(is_invalid(bopen) || is_preproc(bopen), false);

   chunk_t *pc = get_next_ncnl(bopen, scope_e::PREPROC);
   /* Can't remove empty statement */
   retval_if(is_invalid_or_type(pc, CT_BRACE_CLOSE), false); /* Can't remove empty statement */

   // DRY 7 start
   LOG_FMT(LBRDEL, "%s: start on %zu : ", __func__, bopen->orig_line);

   chunk_t *prev = nullptr;
   size_t        semi_count = 0;
   const size_t  level      = bopen->level + 1;
   bool          hit_semi   = false;
   bool          was_fcn    = false;
   const size_t  nl_max     = cpd.settings[UO_mod_full_brace_nl].u;
   size_t        nl_count   = 0;
   size_t        if_count   = 0;
   int           br_count   = 0;

   pc = get_next_nc(bopen, scope_e::ALL);
   while ((is_valid(pc)) && (pc->level >= level))
   {
      if (is_preproc(pc))
      {  /* Cannot remove braces that contain a preprocessor */
         LOG_FMT(LBRDEL, " PREPROC\n");
         return(false);
      }

      if (is_nl(pc))
      {
         nl_count += pc->nl_count;
         if ((nl_max > 0) && (nl_count > nl_max))
         {
            LOG_FMT(LBRDEL, " exceeded %zu newlines\n", nl_max);
            return(false);
         }
      }
      else
      {
         switch(pc->type)
         {
            case(CT_BRACE_OPEN ): br_count++;                        break;
            case(CT_BRACE_CLOSE): br_count--;                        break;
            case(CT_IF         ): /* fallthrough */
            case(CT_ELSEIF     ): if(br_count == 0) { if_count++; }  break;
            default:              /* do nothing */                   break;
         }

         if (pc->level == level)
         {
            if ((semi_count >  0   ) &&
                (hit_semi   == true) )
            {
               /* should have bailed due to close brace level drop */
               LOG_FMT(LBRDEL, " no close brace\n");
               return(false);
            }

            LOG_FMT(LBRDEL, " [%s %zu-%zu]", pc->text(), pc->orig_line, semi_count);
            if (is_type(pc, CT_ELSE))
            {
               LOG_FMT(LBRDEL, " bailed on %s on line %zu\n",
                       pc->text(), pc->orig_line);
               return(false);
            }

            was_fcn = is_type(prev, CT_FPAREN_CLOSE);

            if (( is_semicolon(pc)   ) ||
                ( is_type(pc, 7, CT_IF, CT_ELSEIF, CT_BRACE_OPEN, CT_FOR,
                                 CT_DO, CT_WHILE,  CT_USING_STMT) &&
                   was_fcn == true))
            {
               // DRY with line 485
               const bool is_semi = is_semicolon(pc);
               hit_semi = (is_semi) ? true : hit_semi;
               if (++semi_count > 1)
               {
                  LOG_FMT(LBRDEL, " bailed on %zu because of %s on line %zu\n",
                          bopen->orig_line, pc->text(), pc->orig_line);
                  return(false);
               }
            }
         }
      }
      prev = pc;
      pc   = get_next_nc(pc);
   }

   if (is_invalid(pc))
   {
      LOG_FMT(LBRDEL, " nullptr\n");
      return(false);
   }
   // DRY 7 end
   if (is_type_and_ptype(pc, CT_BRACE_CLOSE, CT_IF))
   {
      chunk_t *next = get_next_ncnl(pc, scope_e::PREPROC);

      prev = get_prev_ncnl(pc, scope_e::PREPROC);
      assert(is_valid(prev));
      if ( is_type (next, CT_ELSE                        ) &&
           is_type (prev, CT_BRACE_CLOSE, CT_VBRACE_CLOSE) &&
           is_ptype(prev, CT_IF                          ) )
      {
         LOG_FMT(LBRDEL, " - bailed on '%s'[%s] on line %zu due to 'if' and 'else' sequence\n",
                 get_token_name(pc->type), get_token_name(pc->ptype),
                 pc->orig_line);
         return(false);
      }
   }

   LOG_FMT(LBRDEL, " - end on '%s' on line %zu. if_count=%zu semi_count=%zu\n",
           get_token_name(pc->type), pc->orig_line, if_count, semi_count);

   return(is_type(pc, CT_BRACE_CLOSE      ) &&
          (pc->pp_level == bopen->pp_level) );
}


// DRY with can_remove_braces
static void examine_brace(chunk_t *bopen)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(bopen));

   // DRY7 start
   LOG_FMT(LBRDEL, "%s: start on %zu : ", __func__, bopen->orig_line);

   chunk_t       *prev      = nullptr;
   size_t        semi_count = 0;
   const size_t  level      = bopen->level + 1;
   bool          hit_semi   = false;
   bool          was_fcn    = false;
   const size_t  nl_max     = cpd.settings[UO_mod_full_brace_nl].u;
   size_t        nl_count   = 0;
   size_t        if_count   = 0;
   int           br_count   = 0;

   chunk_t *pc = get_next_nc(bopen);
   while ( is_valid(pc) &&
          (pc->level >= level))
   {
      if (is_preproc(pc))
      {
         /* Cannot remove braces that contain a preprocessor */
         LOG_FMT(LBRDEL, " PREPROC\n");
         return;
      }

      if (is_nl(pc))
      {
         nl_count += pc->nl_count;
         if ((nl_max > 0) && (nl_count > nl_max))
         {
            LOG_FMT(LBRDEL, " exceeded %zu newlines\n", nl_max);
            return;
         }
      }
      else
      {
         if (is_type(pc, CT_BRACE_OPEN))
         {
            br_count++;
         }
         else if (is_type(pc, CT_BRACE_CLOSE))
         {
            br_count--;
#if 0
            \\todo SN disabled, enable code after if it does not fail any test
            if (br_count == 0)
            {
               next = get_next_ncnl(pc, scope_e::PREPROC);
               if (is_invalid(next) || (next->type != CT_BRACE_CLOSE))
               {
                  LOG_FMT(LBRDEL, " junk after close brace\n");
                  return;
               }
            }
#endif
         }

         else if (is_type(pc, CT_IF, CT_ELSEIF))
         {
            if (br_count == 0) { if_count++; }
         }

         if (is_level(pc, level))
         {
            if ((semi_count > 0  ) &&
                (hit_semi == true) )
            {
               /* should have bailed due to close brace level drop */
               LOG_FMT(LBRDEL, " no close brace\n");
               return;
            }

            LOG_FMT(LBRDEL, " [%s %zu-%zu]", pc->text(), pc->orig_line, semi_count);

            if (is_type(pc, CT_ELSE))
            {
               LOG_FMT(LBRDEL, " bailed on %s on line %zu\n",
                       pc->text(), pc->orig_line);
               return;
            }

            was_fcn = is_type(prev, CT_FPAREN_CLOSE);

            if ( is_type(pc, 9, CT_IF, CT_FOR,   CT_SEMICOLON,  CT_ELSEIF,
                       CT_USING_STMT, CT_DO, CT_WHILE, CT_VSEMICOLON, CT_SWITCH) ||
                 (is_type(pc, CT_BRACE_OPEN) && (was_fcn == true)) )
            {
               // DRY with line 345
               const bool is_semi = is_semicolon(pc);
               hit_semi = (is_semi) ? true : hit_semi;
               if (++semi_count > 1)
               {
                  LOG_FMT(LBRDEL, " bailed on %zu because of %s on line %zu\n",
                          bopen->orig_line, pc->text(), pc->orig_line);
                  return;
               }
            }
         }
      }
      prev = pc;
      pc   = get_next_nc(pc);
   }

   if (is_invalid(pc))
   {
      LOG_FMT(LBRDEL, " nullptr\n");
      return;
   }
   // DRY7 end

   LOG_FMT(LBRDEL, " - end on '%s' on line %zu. if_count=%zu semi_count=%zu\n",
           get_token_name(pc->type), pc->orig_line, if_count, semi_count);

   if (is_type(pc, CT_BRACE_CLOSE))
   {
      chunk_t *next = get_next_ncnl(pc);
      while (is_type(next, CT_VBRACE_CLOSE) )
      {
         next = get_next_ncnl(next);
      }

      assert(is_valid(next));
      LOG_FMT(LBRDEL, " next is '%s'\n", get_token_name(next->type));
      if ((if_count > 0 ) && is_type(next, CT_ELSE, CT_ELSEIF))
      {
         LOG_FMT(LBRDEL, " bailed on because 'else' is next and %zu ifs\n", if_count);
         return;
      }

      if (semi_count > 0)
      {
         if (is_ptype(bopen, CT_ELSE))
         {
            next = get_next_ncnl(bopen);
            if (is_type(next, CT_IF))
            {
               prev = get_prev_ncnl(bopen);
               LOG_FMT(LBRDEL, " else-if removing braces on line %zu and %zu\n",
                       bopen->orig_line, pc->orig_line);

               chunk_del(bopen);
               chunk_del(pc   );
               newline_del_between(prev, next);
               if (is_option_set(cpd.settings[UO_nl_else_if].a, AV_ADD))
               {
                  newline_add_between(prev, next);
               }
               return;
            }
         }

         /* we have a pair of braces with only 1 statement inside */
         convert_brace(bopen);
         convert_brace(pc   );

         LOG_FMT(LBRDEL, " removing braces on line %zu and %zu\n",
                 bopen->orig_line, pc->orig_line);
      }
      else
      {
         LOG_FMT(LBRDEL, " empty statement\n");
      }
   }
   else
   {
      LOG_FMT(LBRDEL, " not a close brace? - '%s'\n", pc->text());
   }
}


static void convert_brace(chunk_t *br)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid_or_flag(br, PCF_KEEP_BRACE));

   chunk_t *tmp;
   switch(br->type)
   {
      case(CT_BRACE_OPEN ): { set_type(br, CT_VBRACE_OPEN ); br->str.clear(); tmp = chunk_get_prev(br); break; }
      case(CT_BRACE_CLOSE): { set_type(br, CT_VBRACE_CLOSE); br->str.clear(); tmp = chunk_get_next(br); break; }
      default:              { /* unexpected type */   return;     }
   }

   if (is_nl(tmp))
   {
      if (tmp->nl_count > 1) { tmp->nl_count--; }
      else
      {
         if (is_safe_to_del_nl(tmp))
         {
            chunk_del(tmp);
         }
      }
   }
}


static void convert_vbrace(chunk_t *vbr)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(vbr));

   if (is_type(vbr, CT_VBRACE_OPEN))
   {
      set_type(vbr, CT_BRACE_OPEN);
      vbr->str = "{";

      /* If the next chunk is a preprocessor, then move the open brace after the
       * preprocessor. */
      chunk_t *tmp = chunk_get_next(vbr);
      if (is_type(tmp, CT_PREPROC))
      {
         tmp = chunk_get_next(vbr, scope_e::PREPROC);
         chunk_move_after(vbr, tmp);
         newline_add_after(vbr);
      }
   }
   else if (is_type(vbr, CT_VBRACE_CLOSE))
   {
      set_type(vbr, CT_BRACE_CLOSE);
      vbr->str = "}";

      /* If the next chunk is a comment, followed by a newline, then
       * move the brace after the newline and add another newline after
       * the close brace. */
      chunk_t *tmp = chunk_get_next(vbr);
      if (is_cmt(tmp))
      {
         tmp = chunk_get_next(tmp);
         if (is_nl(tmp))
         {
            chunk_move_after(vbr, tmp);
            newline_add_after(vbr);
         }
      }
   }
}


static void convert_vbrace_to_brace(chunk_t *pc)
{
   bool in_preproc = is_preproc(pc);

   /* Find the matching vbrace close */
   chunk_t *vbc = nullptr;
   chunk_t *tmp = pc;
   while ((tmp = chunk_get_next(tmp)) != nullptr)
   {
      /* Can't leave a preprocessor */
      break_if ((in_preproc == true  ) &&
                !is_preproc(tmp) )

      if ((pc->brace_level == tmp->brace_level) &&
           is_type (tmp, CT_VBRACE_CLOSE      ) &&
           is_ptype(pc,  tmp->ptype           ) &&
           are_same_pp(tmp, pc           ) )
      {
         vbc = tmp;
         break;
      }
   }
   if (is_valid(vbc))
   {
      convert_vbrace(pc);
      convert_brace(vbc);
   }
}


static void convert_all_vbrace_to_brace(void)
{
   LOG_FUNC_ENTRY();

   /* Find every vbrace open */
   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      continue_if(not_type(pc, CT_VBRACE_OPEN));

#if 0 /* \todo fix this */
      switch(pc->ptype)
      {
         case(CT_IF        ): /* fallthrough */
         case(CT_ELSE      ): /* fallthrough */
         case(CT_ELSEIF    ): if(is_option_set(cpd.settings[UO_mod_full_brace_if      ].a, AV_ADD)) convert_vbrace_to_brace(pc); break;
         case(CT_DO        ): if(is_option_set(cpd.settings[UO_mod_full_brace_do      ].a, AV_ADD)) convert_vbrace_to_brace(pc); break;
         case(CT_FOR       ): if(is_option_set(cpd.settings[UO_mod_full_brace_for     ].a, AV_ADD)) convert_vbrace_to_brace(pc); break;
         case(CT_WHILE     ): if(is_option_set(cpd.settings[UO_mod_full_brace_while   ].a, AV_ADD)) convert_vbrace_to_brace(pc); break;
         case(CT_USING_STMT): if(is_option_set(cpd.settings[UO_mod_full_brace_using   ].a, AV_ADD)) convert_vbrace_to_brace(pc); break;
         case(CT_FUNC_DEF  ): if(is_option_set(cpd.settings[UO_mod_full_brace_function].a, AV_ADD)) convert_vbrace_to_brace(pc); break;
         default:             /* do nothing */ break;
      }
#else
      bool in_preproc = is_preproc(pc);

      if ((is_ptype(pc, CT_IF,
                    CT_ELSE, CT_ELSEIF ) && (is_option_set(cpd.settings[UO_mod_full_brace_if      ].a, AV_ADD)) &&  !cpd.settings[UO_mod_full_brace_if_chain].b) ||
           ((pc->ptype == CT_FOR       ) && (is_option_set(cpd.settings[UO_mod_full_brace_for     ].a, AV_ADD))) ||
           ((pc->ptype == CT_DO        ) && (is_option_set(cpd.settings[UO_mod_full_brace_do      ].a, AV_ADD))) ||
           ((pc->ptype == CT_WHILE     ) && (is_option_set(cpd.settings[UO_mod_full_brace_while   ].a, AV_ADD))) ||
           ((pc->ptype == CT_USING_STMT) && (is_option_set(cpd.settings[UO_mod_full_brace_using   ].a, AV_ADD))) ||
           ((pc->ptype == CT_FUNC_DEF  ) && (is_option_set(cpd.settings[UO_mod_full_brace_function].a, AV_ADD))) )
      {
         /* Find the matching vbrace close */
         chunk_t *vbc = nullptr;
         chunk_t *tmp = pc;
         while ((tmp = chunk_get_next(tmp)) != nullptr)
         {
            /* Can't leave a preprocessor */
            break_if((in_preproc == true) && !is_preproc(tmp))

            if ((pc->brace_level == tmp->brace_level) &&
                 is_type (tmp, CT_VBRACE_CLOSE      ) &&
                 is_ptype(pc,  tmp->ptype           ) &&
                 are_same_pp(tmp, pc           ) )
            {
               vbc = tmp;
               break;
            }
         }
         continue_if (is_invalid(vbc));

         convert_vbrace(pc);
         convert_vbrace(vbc);
      }
#endif
   }
}


chunk_t *insert_comment_after(chunk_t *ref, c_token_t cmt_type,
                              const unc_text &cmt_text)
{
   LOG_FUNC_ENTRY();

   chunk_t new_cmt = *ref;
   new_cmt.prev  = nullptr;
   new_cmt.next  = nullptr;
   set_flags(&new_cmt, get_flags(ref, PCF_COPY_FLAGS));
   new_cmt.type  = cmt_type;
   new_cmt.str.clear();
   if (cmt_type == CT_COMMENT_CPP)
   {
      new_cmt.str.append("// ");
      new_cmt.str.append(cmt_text);
   }
   else
   {
      if (is_type(ref, CT_PP_ELSE))
      {
         new_cmt.str.append(" ");
      }
#if 1
      new_cmt.str.append("/* ");
      new_cmt.str.append(cmt_text);
      new_cmt.str.append(" */");
#else
      new_cmt.str.append("/* %s */", cmt_text.c_str());
#endif
   }
   /* TODO: expand comment type to cover other comment styles? */

   new_cmt.column   = ref->column + ref->len() + 1;
   new_cmt.orig_col = new_cmt.column;

   return(chunk_add_after(&new_cmt, ref));
}


static void append_tag_name(unc_text &txt, chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   /* step backwards over all a::b stuff */
   chunk_t *tmp = pc;
   while ((tmp = get_prev_ncnl(tmp)) != nullptr)
   {
      break_if(not_type(tmp, CT_DC_MEMBER, CT_MEMBER));
      tmp = get_prev_ncnl(tmp);
      pc  = tmp;
      break_if(!is_word(tmp));
   }

   assert(is_valid(pc));
   txt += pc->str;
   while ((pc = get_next_ncnl(pc)) != nullptr)
   {
      break_if(not_type(pc, CT_DC_MEMBER, CT_MEMBER));
      txt += pc->str;
      pc   = get_next_ncnl(pc);
      if (is_valid(pc)) { txt += pc->str; }
   }
}


void add_long_closebrace_comment(void)
{
   LOG_FUNC_ENTRY();
   chunk_t* br_close;
   chunk_t* fcn_pc     = nullptr;
   chunk_t* sw_pc      = nullptr;
   chunk_t* ns_pc      = nullptr;
   chunk_t* cl_pc      = nullptr;
   chunk_t* cl_semi_pc = nullptr;
   unc_text xstr;

   for (chunk_t *pc = chunk_get_head(); pc; pc = get_next_ncnl(pc))
   {
#if 1
      switch(pc->type)
      {
         case(CT_FUNC_DEF   ):  /* fallthrough */
         case(CT_OC_MSG_DECL): fcn_pc = pc; break;
         case(CT_SWITCH     ): sw_pc  = pc; break; /* pointless, since it always has the text "switch" */
         case(CT_NAMESPACE  ): ns_pc  = pc; break;
         case(CT_CLASS      ): cl_pc  = pc; break;
         default: /* unexpected type */     break;
      }
#else
      if(is_type(pc, CT_FUNC_DEF, CT_OC_MSG_DECL)) { fcn_pc = pc; }
      /* kind of pointless, since it always has the text "switch" */
      else if (is_type(pc, CT_SWITCH            )) { sw_pc = pc; }
      else if (is_type(pc, CT_NAMESPACE         )) { ns_pc = pc; }
      else if (is_type(pc, CT_CLASS             )) { cl_pc = pc; }
#endif

      continue_if(not_type(pc, CT_BRACE_OPEN) || is_preproc(pc));

      size_t   nl_count = 0;
      chunk_t* br_open  = pc;
      chunk_t* tmp      = pc;
      while ((tmp = chunk_get_next(tmp)) != nullptr)
      {
         if (is_nl(tmp))
         {
            nl_count += tmp->nl_count;
         }

         else if (is_type_and_level(tmp, CT_BRACE_CLOSE, br_open->level))
         {
            br_close = tmp;

            //LOG_FMT(LSYS, "found brace pair on lines %d and %d, nl_count=%d\n",
            //        br_open->orig_line, br_close->orig_line, nl_count);

            /* Found the matching close brace - make sure a newline is next */
            tmp = chunk_get_next(tmp);

            /* Check for end of class */
            if(is_type_and_ptype(tmp, CT_SEMICOLON, CT_CLASS))
            {
               cl_semi_pc = tmp;
               tmp        = chunk_get_next(tmp);
               if (is_valid(tmp) && !is_nl(tmp))
               {
                  tmp        = cl_semi_pc;
                  cl_semi_pc = nullptr;
               }
            }
            if (is_invalid(tmp) || is_nl(tmp))
            {
               size_t   nl_min = 0;
               chunk_t* tag_pc = nullptr;

               if(is_ptype(br_open, CT_SWITCH))
               {
                  nl_min = cpd.settings[UO_mod_add_long_switch_closebrace_comment].u;
                  tag_pc = sw_pc;
                  xstr   = (is_valid(sw_pc)) ? sw_pc->str : "";
               }
               else if(is_ptype(br_open, CT_FUNC_DEF, CT_OC_MSG_DECL))
               {
                  nl_min = cpd.settings[UO_mod_add_long_function_closebrace_comment].u;
                  tag_pc = fcn_pc;
                  xstr.clear();
                  append_tag_name(xstr, tag_pc);
               }
               else if (is_ptype(br_open, CT_NAMESPACE))
               {
                  nl_min = cpd.settings[UO_mod_add_long_namespace_closebrace_comment].u;
                  tag_pc = ns_pc;

                  /* obtain the next chunk, normally this is the name of the namespace
                   * and append it to generate "namespace xyz" */
                  assert(is_valid(ns_pc));
                  xstr = ns_pc->str;
                  xstr.append(" ");
                  append_tag_name(xstr, chunk_get_next(ns_pc));
               }
               else if ( is_ptype (br_open, CT_CLASS) &&
                         are_valid(cl_pc, cl_semi_pc) )
               {
                  nl_min = cpd.settings[UO_mod_add_long_class_closebrace_comment].u;
                  tag_pc = cl_pc;
                  xstr   = tag_pc->str;
                  xstr.append(" ");
                  append_tag_name(xstr, chunk_get_next(cl_pc));
                  br_close   = cl_semi_pc;
                  cl_semi_pc = nullptr;
                  cl_pc      = nullptr;
               }

               if ((nl_min   >  0     ) &&
                   (nl_count >= nl_min) &&
                   is_valid(tag_pc)     )
               {
                  /* determine the added comment style */
                  const c_token_t style = (is_lang(cpd, LANG_CPPCS)) ?
                                    CT_COMMENT_CPP : CT_COMMENT;

                  /* Add a comment after the close brace */
                  insert_comment_after(br_close, style, xstr);
               }
            }
            break;
         }
      }
   }
}


static void move_case_break(void)
{
   LOG_FUNC_ENTRY();
   chunk_t *prev = nullptr;

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      if ( is_type          (pc,   CT_BREAK               ) &&
           is_type_and_ptype(prev, CT_BRACE_CLOSE, CT_CASE) )
      {
         if (is_nl(chunk_get_prev(pc  )) &&
             is_nl(chunk_get_prev(prev)) )
         {
            swap_lines(prev, pc);
         }
      }
      prev = pc;
   }
}


static chunk_t *mod_case_brace_remove(chunk_t *br_open)
{
   LOG_FUNC_ENTRY();
   chunk_t *next = get_next_ncnl(br_open, scope_e::PREPROC);

   LOG_FMT(LMCB, "%s: line %zu", __func__, br_open->orig_line);

   /* Find the matching brace close */
   chunk_t *br_close = get_next_type(br_open, CT_BRACE_CLOSE, (int)br_open->level, scope_e::PREPROC);
   if (is_invalid(br_close))
   {
      LOG_FMT(LMCB, " - no close\n");
      return(next);
   }

   /* Make sure 'break', 'return', 'goto', 'case' or '}' is after the close brace */
   chunk_t *pc = get_next_ncnl(br_close, scope_e::PREPROC);
   if (not_type(pc, 5, CT_CASE, CT_BREAK, CT_BRACE_CLOSE, CT_GOTO, CT_RETURN))
   {
      LOG_FMT(LMCB, " - after '%s'\n", (is_invalid(pc)) ? "<null>" : get_token_name(pc->type));
      return(next);
   }

   /* scan to make sure there are no definitions at brace level between braces */
   for (pc = br_open; pc != br_close; pc = get_next_ncnl(pc, scope_e::PREPROC))
   {
      if ((pc->level == (br_open->level + 1)) &&
          is_flag(pc, PCF_VAR_DEF))
      {
         LOG_FMT(LMCB, " - vardef on line %zu: '%s'\n", pc->orig_line, pc->text());
         return(next);
      }
   }
   LOG_FMT(LMCB, " - removing braces on lines %zu and %zu\n",
           br_open->orig_line, br_close->orig_line);

   for (pc = br_open; pc != br_close; pc = get_next_ncnl(pc, scope_e::PREPROC))
   {
      pc->brace_level--;
      pc->level--;
   }
   next = chunk_get_prev(br_open, scope_e::PREPROC);
   chunk_del(br_open );
   chunk_del(br_close);
   return(chunk_get_next(next, scope_e::PREPROC));
}


static chunk_t *mod_case_brace_add(chunk_t *cl_colon)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(cl_colon), cl_colon);

   chunk_t *pc = cl_colon;
   LOG_FMT(LMCB, "%s: line %zu", __func__, pc->orig_line);

   chunk_t *last = nullptr;
   chunk_t *next = get_next_ncnl(cl_colon, scope_e::PREPROC);

   while ((pc = get_next_ncnl(pc, scope_e::PREPROC)) != nullptr)
   {
      if (pc->level < cl_colon->level)
      {
         LOG_FMT(LMCB, " - level drop\n");
         return(next);
      }

      if (is_level(pc, cl_colon->level  ) &&
          is_type (pc, CT_CASE, CT_BREAK) )
      {
         last = pc;
         //if (is_type(pc, CT_BREAK))
         //{
         //   /* Step past the semicolon */
         //   last = chunk_get_next_ncnl(chunk_get_next_ncnl(last));
         //}
         break;
      }
   }

   if (is_invalid(last))
   {
      LOG_FMT(LMCB, " - nullptr last\n");
      return(next);
   }

   LOG_FMT(LMCB, " - adding before '%s' on line %zu\n", last->text(), last->orig_line);

   assert(is_valid(pc));

   chunk_t chunk;
   chunk.type        = CT_BRACE_OPEN;
   chunk.ptype       = CT_CASE;
   chunk.orig_line   = cl_colon->orig_line;
   chunk.level       = cl_colon->level;
   chunk.brace_level = cl_colon->brace_level;
   chunk.str         = "{";
   set_flags(&chunk, get_flags(pc, PCF_COPY_FLAGS));

   chunk_t *br_open = chunk_add_after(&chunk, cl_colon);

   chunk.type      = CT_BRACE_CLOSE;
   chunk.orig_line = last->orig_line;
   chunk.str       = "}";

   const chunk_t *br_close = chunk_add_before(&chunk, last);
   newline_add_before(last);

   for (pc = chunk_get_next(br_open, scope_e::PREPROC);
        pc != br_close;
        pc = chunk_get_next(pc, scope_e::PREPROC))
   {
      assert(is_valid(pc));
      pc->level++;
      pc->brace_level++;
   }

   return(br_open);
}


static void mod_case_brace(void)
{
   LOG_FUNC_ENTRY();

   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      chunk_t *next = get_next_ncnl(pc, scope_e::PREPROC);
      return_if(is_invalid(next));

      if ((cpd.settings[UO_mod_case_brace].a == AV_REMOVE) &&
          is_type_and_ptype(pc, CT_BRACE_OPEN, CT_CASE))
      {
         pc = mod_case_brace_remove(pc);
      }
      else if ((is_option_set(cpd.settings[UO_mod_case_brace].a, AV_ADD) ) &&
                is_type      (pc,   CT_CASE_COLON                        ) &&
                not_type  (next, CT_BRACE_OPEN, CT_BRACE_CLOSE, CT_CASE  ) )
      {
         pc = mod_case_brace_add(pc);
      }
      else
      {
         pc = get_next_ncnl(pc, scope_e::PREPROC);
      }
   }
}


static void process_if_chain(chunk_t *br_start)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(br_start));

   chunk_t *braces[256];
   int     br_cnt           = 0;
   bool    must_have_braces = false;

   chunk_t *pc = br_start;

   LOG_FMT(LBRCH, "%s: if starts on line %zu\n", __func__, br_start->orig_line);

   while (is_valid(pc))
   {
      if (is_type(pc, CT_BRACE_OPEN))
      {
         bool tmp = can_remove_braces(pc);
         LOG_FMT(LBRCH, "  [%d] line %zu - can%s remove %s\n",
                 br_cnt, pc->orig_line, tmp ? "" : "not",
                 get_token_name(pc->type));
         if (tmp == false) { must_have_braces = true; }
      }
      else
      {
         const bool tmp = should_add_braces(pc);
         if (tmp == true) { must_have_braces = true; }
         LOG_FMT(LBRCH, "  [%d] line %zu - %s %s\n",
                 br_cnt, pc->orig_line, tmp ? "should add" : "ignore",
                 get_token_name(pc->type));
      }

      braces[br_cnt++] = pc;
      chunk_t *br_close = chunk_skip_to_match(pc, scope_e::PREPROC);
      break_if(is_invalid(br_close));

      braces[br_cnt++] = br_close;

      pc = get_next_ncnl(br_close, scope_e::PREPROC);
      break_if(is_invalid_or_not_type(pc, CT_ELSE));

      if (cpd.settings[UO_mod_full_brace_if_chain_only].b)
      {
         // There is an 'else' - we want full braces.
         must_have_braces = true;
      }

      pc = get_next_ncnl(pc, scope_e::PREPROC);
      if(is_type(pc, CT_ELSEIF))
      {
         while(not_type(pc, CT_VBRACE_OPEN, CT_BRACE_OPEN))
         {
            pc = get_next_ncnl(pc, scope_e::PREPROC);
         }
      }
      break_if(is_invalid(pc));
      break_if(not_type(pc, CT_VBRACE_OPEN, CT_BRACE_OPEN));
   }

   if (must_have_braces)
   {
      LOG_FMT(LBRCH, "%s: add braces on lines[%d]:", __func__, br_cnt);
      while (--br_cnt >= 0)
      {
         set_flags(braces[br_cnt], PCF_KEEP_BRACE);
         if (is_type(braces[br_cnt], CT_VBRACE_OPEN, CT_VBRACE_CLOSE))
         {
            LOG_FMT(LBRCH, " %zu", braces[br_cnt]->orig_line);
            convert_vbrace(braces[br_cnt]);
         }
         else
         {
            LOG_FMT(LBRCH, " {%zu}", braces[br_cnt]->orig_line);
         }
         braces[br_cnt] = nullptr;
      }
      LOG_FMT(LBRCH, "\n");
   }
   else if (cpd.settings[UO_mod_full_brace_if_chain].b)
   {
      // This might run because either UO_mod_full_brace_if_chain or UO_mod_full_brace_if_chain_only is used.
      // We only want to remove braces if the first one is active.
      LOG_FMT(LBRCH, "%s: remove braces on lines[%d]:", __func__, br_cnt);
      while (--br_cnt >= 0)
      {
         LOG_FMT(LBRCH, " {%zu}", braces[br_cnt]->orig_line);
         if (is_type(braces[br_cnt], CT_BRACE_OPEN, CT_BRACE_CLOSE))
         {
            convert_brace(braces[br_cnt]);
         }
         braces[br_cnt] = nullptr;
      }
      LOG_FMT(LBRCH, "\n");
   }
}


static void mod_full_brace_if_chain(void)
{
   LOG_FUNC_ENTRY();

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if(is_type (pc, CT_BRACE_OPEN, CT_VBRACE_OPEN) &&
         is_ptype(pc, CT_IF))
      {
         process_if_chain(pc);
      }
   }
}
