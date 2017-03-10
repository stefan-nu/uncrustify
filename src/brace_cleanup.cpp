/**
 * @file brace_cleanup.cpp
 * Determines the brace level and parenthesis level.
 * Inserts virtual braces as needed.
 * Handles all that preprocessor stuff.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "brace_cleanup.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "unc_ctype.h"
#include "uncrustify.h"
#include "lang_pawn.h"
#include "parse_frame.h"
#include "keywords.h"
#include "token_enum.h"

/*
 * abbreviations used:
 * - sparen = tbd
 */


/**
 * tbd
 *
 * @return
 */
static size_t preproc_start(
   parse_frame_t *frm,  /**< [in]  */
   chunk_t       *pc    /**< [in]  */
);


/**
 * tbd
 */
static void print_stack(
   log_sev_t     logsev, /**< [in]  */
   const char    *str,   /**< [in]  */
   parse_frame_t *frm,   /**< [in]  */
   chunk_t       *pc     /**< [in]  */
);


/**
 * pc is a CT_WHILE.
 * Scan backwards to see if we find a brace/vbrace with the parent set to CT_DO
 */
static bool maybe_while_of_do(
   chunk_t *pc  /**< [in]  */
);


/**
 * tbd
 */
static void push_fmr_pse(
   parse_frame_t *frm,     /**< [in]  */
   chunk_t       *pc,      /**< [in]  */
   brace_stage_e stage,    /**< [in]  */
   const char    *logtext  /**< [in]  */
);


/**
 * the value of after determines:
 *   true:  insert_vbrace_close_after(pc, frm)
 *   false: insert_vbrace_open_before(pc, frm)
 */
static chunk_t *insert_vbrace(
   chunk_t       *pc,    /**< [in]  */
   bool          after,  /**< [in] true=close_after, false=open_before */
   parse_frame_t *frm    /**< [in]  */
);

#define insert_vbrace_close_after(pc, frm)    insert_vbrace(pc, true,  frm)
#define insert_vbrace_open_before(pc, frm)    insert_vbrace(pc, false, frm)


/**
 * tbd
 */
static void parse_cleanup(
   parse_frame_t *frm,  /**< [in]  */
   chunk_t       *pc    /**< [in]  */
);


/**
 * Called when a statement was just closed and the pse_tos was just
 * decremented.
 *
 * - if the TOS is now VBRACE, insert a CT_VBRACE_CLOSE and recurse.
 * - if the TOS is a complex statement, call handle_complex_close()
 *
 * @retval true  - done with this chunk
 * @retval false - keep processing
 */
static bool close_statement(
   parse_frame_t *frm,  /**< [in]  */
   chunk_t       *pc    /**< [in]  */
);


/**
 * Checks the progression of complex statements.
 * - checks for else after if
 * - checks for if after else
 * - checks for while after do
 * - checks for open brace       in BRACE2 and BRACE_DO stages, inserts open VBRACE
 * - checks for open parenthesis in PAREN1 and PAREN2 stages, complains
 *
 * @return true  - done with this chunk
 * @retval false - keep processing
 */
static bool check_complex_statements(
   parse_frame_t *frm,  /**< [in] the parse frame */
   chunk_t       *pc    /**< [in] the current chunk */
);


/**
 * Handles a close parenthesis or brace - just progress the stage, if the end
 * of the statement is hit, call close_statement()
 *
 * @return true  - done with this chunk
 * @retval false - keep processing
 */
static bool handle_complex_close(
   parse_frame_t *frm,  /**< [in] The parse frame */
   chunk_t       *pc    /**< [in] The current chunk */
);


static size_t preproc_start(parse_frame_t *frm, chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   size_t  pp_level = cpd.pp_level;

   /* Get the type of preprocessor and handle it */
   chunk_t *next = get_next_ncnl(pc);
   if (is_valid(next))
   {
      cpd.is_preproc = next->type;

      /* If we are in a define, push the frame stack. */
      if (cpd.is_preproc == CT_PP_DEFINE)
      {
         pf_push(frm);

         /* a preproc body starts a new, blank frame */
         memset(frm, 0, sizeof(*frm));
         frm->level       = 1;
         frm->brace_level = 1;

         /*TODO: not sure about the next 3 lines */
         frm->pse_tos                 = 1;
         frm->pse[frm->pse_tos].type  = CT_PP_DEFINE;
         frm->pse[frm->pse_tos].stage = brace_stage_e::NONE;
      }
      else
      {
         /* Check for #if, #else, #endif, etc */
         pp_level = pf_check(frm, pc);
      }
   }
   return(pp_level);
}


static void print_stack(log_sev_t logsev, const char *str,
                        parse_frame_t *frm, chunk_t *pc)
{
   UNUSED(pc);
   LOG_FUNC_ENTRY();
   if (log_sev_on(logsev))
   {
      log_fmt(logsev, "%8.8s", str);

      for (size_t idx = 1; idx <= frm->pse_tos; idx++)
      {
         if (frm->pse[idx].stage != brace_stage_e::NONE)
         {
            LOG_FMT(logsev, " [%s - %d]", get_token_name(frm->pse[idx].type),
                    (unsigned int)frm->pse[idx].stage);
         }
         else
         {
            LOG_FMT(logsev, " [%s]", get_token_name(frm->pse[idx].type));
         }
      }
      log_fmt(logsev, "\n");
   }
}


void brace_cleanup(void)
{
   LOG_FUNC_ENTRY();

   cpd.unc_stage = unc_stage_e::BRACE_CLEANUP;

   parse_frame_t frm;
   memset(&frm, 0, sizeof(frm));

   cpd.frame_count = 0;
   cpd.is_preproc  = CT_NONE;
   cpd.pp_level    = 0;

   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      /* Check for leaving a #define body */
      if ((cpd.is_preproc != CT_NONE) && !is_preproc(pc))
      {
         if (cpd.is_preproc == CT_PP_DEFINE)
         {
            /* out of the #define body, restore the frame */
            pf_pop(&frm);
         }

         cpd.is_preproc = CT_NONE;
      }

      /* Check for a preprocessor start */
      size_t pp_level = cpd.pp_level;
      if(is_type(pc, CT_PREPROC))
      {
         pp_level = preproc_start(&frm, pc);
      }

      /* Do before assigning stuff from the frame */
      if (cpd.lang_flags & LANG_PAWN)
      {
         if ((frm.pse[frm.pse_tos].type == CT_VBRACE_OPEN) &&
              is_type(pc,  CT_NEWLINE) )
         {
            pc = pawn_check_vsemicolon(pc);
         }
      }

      /* Assume the level won't change */
      assert(is_valid(pc));
      pc->level       = frm.level;
      pc->brace_level = frm.brace_level;
      pc->pp_level    = pp_level;


      /**
       * #define bodies get the full formatting treatment
       * Also need to pass in the initial '#' to close out any virtual braces.
       */
      if ((is_cmt(pc) == false       )   &&
          (is_nl(pc) == false       )   &&
          ((cpd.is_preproc      == CT_PP_DEFINE) ||
           (cpd.is_preproc      == CT_NONE     ) ) )
      {
         cpd.consumed = false;
         parse_cleanup(&frm, pc);
         const char* str = (is_type(pc, CT_VBRACE_CLOSE)) ? "Virt-}" : pc->str.c_str();
         print_stack(LBCSAFTER, str, &frm, pc);
      }
      pc = chunk_get_next(pc);
   }
}


static bool maybe_while_of_do(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   chunk_t *prev;

   prev = chunk_get_prev_ncnl(pc);
   retval_if(is_invalid(prev) || !is_preproc(prev), false);

   /* Find the chunk before the preprocessor */
#if 0
   prev = chunk_get_prev_ncnlnp(prev); // fails test 02300, 02301 why?
#else
   while (is_preproc(prev))
   {
      prev = chunk_get_prev_ncnl(prev);
   }
#endif


   return(is_ptype(prev, CT_DO) &&
          is_type (prev, CT_VBRACE_CLOSE, CT_BRACE_CLOSE)) ?
            true : false;
}


static void push_fmr_pse(parse_frame_t *frm, chunk_t *pc,
                         brace_stage_e stage, const char *logtext)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(pc));

   if (frm->pse_tos < ((int)ARRAY_SIZE(frm->pse) - 1))
   {
      frm->pse_tos++;
      frm->pse[frm->pse_tos].type  = pc->type;
      frm->pse[frm->pse_tos].stage = stage;
      frm->pse[frm->pse_tos].pc    = pc;

      print_stack(LBCSPUSH, logtext, frm, pc);
   }
   else
   {
      LOG_FMT(LWARN, "%s:%d Error: Frame stack overflow,  Unable to "
            "properly process this file.\n",  cpd.filename, cpd.line_number);
      cpd.error_count++;
   }
}


/**
 * At the heart of this algorithm are two stacks.
 * There is the Parenthesis Stack (PS) and the Frame stack.
 *
 * The PS (pse in the code) keeps track of braces, parenthesis,
 * if/else/switch/do/while/etc items -- anything that is nestable.
 * Complex statements go through stages.
 * Take this simple if statement as an example:
 *   if ( x ) { x--; }
 *
 * The stack would change like so: 'token' stack afterwards
 * 'if' [IF - 1]
 * '('  [IF - 1] [PAREN OPEN]
 * 'x'  [IF - 1] [PAREN OPEN]
 * ')'  [IF - 2]       <- note that the state was incremented
 * '{'  [IF - 2] [BRACE OPEN]
 * 'x'  [IF - 2] [BRACE OPEN]
 * '--' [IF - 2] [BRACE OPEN]
 * ';'  [IF - 2] [BRACE OPEN]
 * '}'  [IF - 3]
 *                             <- lack of else kills the IF, closes statement
 *
 * Virtual braces example:
 *   if ( x ) x--; else x++;
 *
 * 'if'   [IF - 1]
 * '('    [IF - 1] [PAREN OPEN]
 * 'x'    [IF - 1] [PAREN OPEN]
 * ')'    [IF - 2]
 * 'x'    [IF - 2] [VBRACE OPEN]   <- VBrace open inserted before because '{' was not next
 * '--'   [IF - 2] [VBRACE OPEN]
 * ';'    [IF - 3]                 <- VBrace close inserted after semicolon
 * 'else' [ELSE - 0]               <- IF changed into ELSE
 * 'x'    [ELSE - 0] [VBRACE OPEN] <- lack of '{' -> VBrace
 * '++'   [ELSE - 0] [VBRACE OPEN]
 * ';'    [ELSE - 0]               <- VBrace close inserted after semicolon
 *                                 <- ELSE removed after statement close
 *
 * The pse stack is kept on a frame stack.
 * The frame stack is need for languages that support preprocessors (C, C++, C#)
 * that can arbitrarily change code flow. It also isolates #define macros so
 * that they are indented independently and do not affect the rest of the program.
 *
 * When an #if is hit, a copy of the current frame is push on the frame stack.
 * When an #else/#elif is hit, a copy of the current stack is pushed under the
 * #if frame and the original (pre-#if) frame is copied to the current frame.
 * When #endif is hit, the top frame is popped.
 * This has the following effects:
 *  - a simple #if / #endif does not affect program flow
 *  - #if / #else /#endif - continues from the #if clause
 *
 * When a #define is entered, the current frame is pushed and cleared.
 * When a #define is exited, the frame is popped.
 */
static void parse_cleanup(parse_frame_t *frm, chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   assert(is_valid(pc));
   LOG_FMT(LTOK, "%s:%zu] %16s - tos:%zu/%16s TOS.stage:%d\n",
           __func__, pc->orig_line, get_token_name(pc->type),
           frm->pse_tos, get_token_name(frm->pse[frm->pse_tos].type),
           (unsigned int)frm->pse[frm->pse_tos].stage);

   /* Mark statement starts */
   if (((frm->stmt_count == 0) ||
        (frm->expr_count == 0)     ) &&
       (!is_semicolon(pc)    ) &&
         not_type(pc, CT_BRACE_CLOSE, CT_VBRACE_CLOSE) &&
       (!is_str(pc, ")", 1)  ) &&
       (!is_str(pc, "]", 1)  ) )
   {
      const char* type = is_flag(pc, PCF_STMT_START) ? "stmt" : "expr";
      set_flags(pc, PCF_EXPR_START | ((frm->stmt_count == 0) ? PCF_STMT_START : 0));
      LOG_FMT(LSTMT, "%zu] 1.marked %s as %s start st:%d ex:%d\n",
            pc->orig_line, pc->text(), type, frm->stmt_count, frm->expr_count);
   }
   frm->stmt_count++;
   frm->expr_count++;

   if (frm->sparen_count > 0)
   {
      set_flags(pc, PCF_IN_SPAREN);

      /* Mark everything in the for statement */
      for (int tmp = (int)frm->pse_tos - 1; tmp >= 0; tmp--)	/* tmp can become negative do not use size_t */
      {
         if (frm->pse[tmp].type == CT_FOR)
         {
            set_flags(pc, PCF_IN_FOR);
            break;
         }
      }

      /* Mark the parent on semicolons in for() statements */
      if (is_type(pc, CT_SEMICOLON) &&
          (frm->pse_tos > 1        ) &&
          (frm->pse[frm->pse_tos - 1].type == CT_FOR))
      {
         set_ptype(pc, CT_FOR);
      }
   }

   /* Check the progression of complex statements */
   if (frm->pse[frm->pse_tos].stage != brace_stage_e::NONE)
   {
      if (check_complex_statements(frm, pc)) { return; }
   }

   /** Check for a virtual brace statement close due to a semicolon.
    * The virtual brace will get handled the next time through.
    * The semicolon isn't handled at all.
    * TODO: may need to float VBRACE past comments until newline? */
   if (frm->pse[frm->pse_tos].type == CT_VBRACE_OPEN)
   {
      if (is_semicolon(pc))
      {
         cpd.consumed = true;
         close_statement(frm, pc);
      }
      else if (cpd.lang_flags & LANG_PAWN)
      {
         if (is_type(pc, CT_BRACE_CLOSE))
         {
            close_statement(frm, pc);
         }
      }
   }

   /* Handle close parenthesis, vbrace, brace, and square */

   if (is_type(pc, 6, CT_PAREN_CLOSE, CT_BRACE_CLOSE, CT_VBRACE_CLOSE,
                            CT_ANGLE_CLOSE, CT_MACRO_CLOSE, CT_SQUARE_CLOSE) )
   {
      /* Change CT_PAREN_CLOSE into CT_SPAREN_CLOSE or CT_FPAREN_CLOSE */
      if ( is_type(pc, CT_PAREN_CLOSE)   &&
          ((frm->pse[frm->pse_tos].type == CT_FPAREN_OPEN) ||
           (frm->pse[frm->pse_tos].type == CT_SPAREN_OPEN) ) )
      {
         set_type(pc, get_inverse_type(frm->pse[frm->pse_tos].type) );
         if (is_type(pc, CT_SPAREN_CLOSE))
         {
            frm->sparen_count--;
            clr_flags(pc, PCF_IN_SPAREN);
         }
      }

      /* Make sure the open / close match */
      if (pc->type != (c_token_t)((int)frm->pse[frm->pse_tos].type + 1))   // \todo why +1
//    if (pc->type != get_inverse_type(frm->pse[frm->pse_tos].type )) fails
      {
         if ((frm->pse[frm->pse_tos].type != CT_NONE     ) &&
             (frm->pse[frm->pse_tos].type != CT_PP_DEFINE) )
         {
            LOG_FMT(LWARN, "%s: %s:%zu Error: Unexpected '%s' for '%s', which was on line %zu\n",
                    __func__, cpd.filename, pc->orig_line, pc->text(),
                    get_token_name(frm->pse[frm->pse_tos].pc->type),
                    frm->pse[frm->pse_tos].pc->orig_line);
            print_stack(LBCSPOP, "=Error  ", frm, pc);
            cpd.error_count++;
         }
      }
      else
      {
         cpd.consumed = true;

         /* Copy the parent, update the parenthesis/brace levels */
         set_ptype(pc, frm->pse[frm->pse_tos].parent);
         frm->level--;
         if (is_type(pc, CT_BRACE_CLOSE, CT_VBRACE_CLOSE, CT_MACRO_CLOSE))
         {
            frm->brace_level--;
         }
         pc->level       = frm->level;
         pc->brace_level = frm->brace_level;

         /* Pop the entry */
         frm->pse_tos--;
         print_stack(LBCSPOP, "-Close  ", frm, pc);

         /* See if we are in a complex statement */
         if (frm->pse[frm->pse_tos].stage != brace_stage_e::NONE)
         {
            handle_complex_close(frm, pc);
         }
      }
   }

   /* In this state, we expect a semicolon, but we'll also hit the closing
    * sparen, so we need to check cpd.consumed to see if the close sparen was
    * already handled. */
   if (frm->pse[frm->pse_tos].stage == brace_stage_e::WOD_SEMI)
   {
      chunk_t *tmp = pc;

      if (cpd.consumed)
      {
         /* If consumed, then we are on the close sparen.
          * PAWN: Check the next chunk for a semicolon. If it isn't, then
          * add a virtual semicolon, which will get handled on the next pass. */
         if (cpd.lang_flags & LANG_PAWN)
         {
            tmp = get_next_ncnl(pc);
            assert(is_valid(tmp));
            if (not_type(tmp, CT_SEMICOLON, CT_VSEMICOLON))
            {
               pawn_add_vsemi_after(pc);
            }
         }
      }
      else
      {
         /* Complain if this ISN'T a semicolon, but close out WHILE_OF_DO anyway */
         if (is_type(pc, CT_SEMICOLON, CT_VSEMICOLON) )
         {
            cpd.consumed = true;
            set_ptype(pc, CT_WHILE_OF_DO);
         }
         else
         {
            LOG_FMT(LWARN, "%s:%zu: Error: Expected a semicolon for WHILE_OF_DO, but got '%s'\n",
                    cpd.filename, pc->orig_line, get_token_name(pc->type));
            cpd.error_count++;
         }
         handle_complex_close(frm, pc);
      }
   }

   /* Get the parent type for brace and parenthesis open */
   c_token_t parent = pc->ptype;
   if (is_type(pc, CT_PAREN_OPEN, CT_FPAREN_OPEN, CT_SPAREN_OPEN, CT_BRACE_OPEN))
   {
      chunk_t *prev = chunk_get_prev_ncnl(pc);
      if (is_valid(prev))
      {
         if (is_type(pc, CT_PAREN_OPEN, CT_FPAREN_OPEN, CT_SPAREN_OPEN))
         {
            /* Set the parent for parenthesis and change parenthesis type */
            if (frm->pse[frm->pse_tos].stage != brace_stage_e::NONE)
            {
               set_type(pc, CT_SPAREN_OPEN);
               parent = frm->pse[frm->pse_tos].type;
               frm->sparen_count++;
            }
            else if (is_type(prev, CT_FUNCTION))
            {
               set_type(pc, CT_FPAREN_OPEN);
               parent = CT_FUNCTION;
            }
            /* NS_ENUM and NS_OPTIONS are followed by a (type, name) pair */
            else if ((is_type(prev, CT_ENUM)) &&
                     (cpd.lang_flags & LANG_OC) )
            {
               /* Treat both as CT_ENUM since the syntax is identical */
               set_type(pc, CT_FPAREN_OPEN);
               parent = CT_ENUM;
            }
            else
            {
               /* no need to set parent */
            }
         }
         else  /* must be CT_BRACE_OPEN */
         {
            /* Set the parent for open braces */
            if (frm->pse[frm->pse_tos].stage != brace_stage_e::NONE)
            {
               parent = frm->pse[frm->pse_tos].type;
            }
            else if ((is_type(prev, CT_ASSIGN)) &&
                     (prev->str[0] == '='))
            {
               parent = CT_ASSIGN;
            }
            /*  Carry through CT_ENUM parent in NS_ENUM (type, name) { */
            else if (is_type_and_ptype(prev, CT_FPAREN_CLOSE, CT_ENUM) &&
                     (cpd.lang_flags & LANG_OC) )
            {
               parent = CT_ENUM;
            }
            else if (is_type(prev, CT_FPAREN_CLOSE))
            {
               parent = CT_FUNCTION;
            }
            else
            {
               /* no need to set parent */
            }
         }
      }
   }

   /** Adjust the level for opens & create a stack entry
    * Note that CT_VBRACE_OPEN has already been handled. */
   if (is_type(pc, 7, CT_BRACE_OPEN, CT_PAREN_OPEN, CT_FPAREN_OPEN,
            CT_SPAREN_OPEN, CT_ANGLE_OPEN, CT_MACRO_OPEN, CT_SQUARE_OPEN))
   {
      frm->level++;

      if(is_type(pc, CT_BRACE_OPEN, CT_MACRO_OPEN))
      {
         frm->brace_level++;
      }
      push_fmr_pse(frm, pc, brace_stage_e::NONE, "+Open   ");
      frm->pse[frm->pse_tos].parent = parent;
      set_ptype(pc, parent);
   }

   const pattern_class_e patcls = get_token_pattern_class(pc->type);

   /** Create a stack entry for complex statements: */
   /** if, elseif, switch, for, while, synchronized, using, lock, with, version, CT_D_SCOPE_IF */
   if (patcls == pattern_class_e::BRACED)
   {
      push_fmr_pse(frm, pc, (is_type(pc, CT_DO)) ?
            brace_stage_e::BRACE_DO : brace_stage_e::BRACE2, "+ComplexBraced");
   }
   else if (patcls == pattern_class_e::PBRACED)
   {
      brace_stage_e bs = brace_stage_e::PAREN1;

      if (is_type(pc, CT_WHILE) &&
          maybe_while_of_do(pc)       )
      {
         set_type(pc, CT_WHILE_OF_DO);
         bs = brace_stage_e::WOD_PAREN;
      }
      push_fmr_pse(frm, pc, bs, "+ComplexParenBraced");
   }
   else if (patcls == pattern_class_e::OPBRACED)
   {
      push_fmr_pse(frm, pc, brace_stage_e::OP_PAREN1, "+ComplexOpParenBraced");
   }
   else if (patcls == pattern_class_e::ELSE)
   {
      push_fmr_pse(frm, pc, brace_stage_e::ELSEIF, "+ComplexElse");
   }

   /* Mark simple statement/expression starts
    *  - after { or }
    *  - after ';', but not if the parenthesis stack top is a parenthesis
    *  - after '(' that has a parent type of CT_FOR */
   if ( is_type(pc, 5, CT_SQUARE_OPEN, CT_COLON, CT_OC_END,
                           CT_BRACE_CLOSE, CT_VBRACE_CLOSE) ||
       is_type_and_not_ptype(pc, CT_BRACE_OPEN,  CT_ASSIGN) ||
       is_type_and_ptype    (pc, CT_SPAREN_OPEN, CT_FOR   ) ||
       (is_semicolon(pc) &&
        (frm->pse[frm->pse_tos].type != CT_PAREN_OPEN ) &&
        (frm->pse[frm->pse_tos].type != CT_FPAREN_OPEN) &&
        (frm->pse[frm->pse_tos].type != CT_SPAREN_OPEN) ) )
   {
      LOG_FMT(LSTMT, "%s: %zu> reset1 statement on %s\n",
              __func__, pc->orig_line, pc->text());
      frm->stmt_count = 0;
      frm->expr_count = 0;
   }

   /* Mark expression starts */
   const chunk_t *tmp = get_next_ncnl(pc);
   if (is_type(pc, 23, CT_PAREN_OPEN,  CT_ARITH,  CT_CASE, CT_COMPARE,
                             CT_ANGLE_CLOSE, CT_MINUS,  CT_PLUS, CT_QUESTION,
                             CT_ANGLE_OPEN,  CT_ASSIGN, CT_BOOL, CT_CONTINUE,
                             CT_FPAREN_OPEN, CT_CARET,  CT_GOTO, CT_THROW,
                             CT_SPAREN_OPEN, CT_COMMA,  CT_NOT,  CT_COLON,
                             CT_BRACE_OPEN,  CT_INV,    CT_RETURN)    ||
        is_semicolon(pc)                                        ||
       (is_type(pc, CT_STAR) && not_type(tmp, CT_STAR)))
   {
      frm->expr_count = 0;
      LOG_FMT(LSTMT, "%s: %zu> reset expr on %s\n", __func__, pc->orig_line, pc->text());
   }

   else if (is_type(pc, CT_BRACE_CLOSE) &&
            cpd.consumed     == false         &&
            cpd.unc_off_used == false         )
   {
      /* fatal error */
      fprintf(stderr, "Unmatched BRACE_CLOSE\nat line=%zu, column=%zu\n",
              pc->orig_line, pc->orig_col);
      exit(EXIT_FAILURE);
   }
}


static bool check_complex_statements(parse_frame_t *frm, chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   c_token_t parent;

   assert(is_valid(pc));
   /* Turn an optional parenthesis into either a real parenthesis or a brace */
   if (frm->pse[frm->pse_tos].stage == brace_stage_e::OP_PAREN1)
   {
      frm->pse[frm->pse_tos].stage = (not_type(pc, CT_PAREN_OPEN)) ?
            brace_stage_e::BRACE2 : brace_stage_e::PAREN1;
   }

   /* Check for CT_ELSE after CT_IF */
   while (frm->pse[frm->pse_tos].stage == brace_stage_e::ELSE)
   {
      if (is_type(pc, CT_ELSE))
      {
         /* Replace CT_IF with CT_ELSE on the stack & we are done */
         frm->pse[frm->pse_tos].type  = CT_ELSE;
         frm->pse[frm->pse_tos].stage = brace_stage_e::ELSEIF;
         print_stack(LBCSSWAP, "=Swap   ", frm, pc);
         return(true);
      }

      /* Remove the CT_IF and close the statement */
      frm->pse_tos--;
      print_stack(LBCSPOP, "-IF-CCS ", frm, pc);
      if (close_statement(frm, pc)) { return(true); }
   }

   /* Check for CT_IF after CT_ELSE */
   if (frm->pse[frm->pse_tos].stage == brace_stage_e::ELSEIF)
   {
      if (is_type(pc, CT_IF))
      {
         if (!cpd.settings[UO_indent_else_if].b ||
             !is_nl(chunk_get_prev_nc(pc)))
         {
            /* Replace CT_ELSE with CT_IF */
            set_type(pc, CT_ELSEIF);
            frm->pse[frm->pse_tos].type  = CT_ELSEIF;
            frm->pse[frm->pse_tos].stage = brace_stage_e::PAREN1;
            return(true);
         }
      }

      /* Jump to the 'expecting brace' stage */
      frm->pse[frm->pse_tos].stage = brace_stage_e::BRACE2;
   }

   /* Check for CT_CATCH or CT_FINALLY after CT_TRY or CT_CATCH */
   while (frm->pse[frm->pse_tos].stage == brace_stage_e::CATCH)
   {
      if (is_type(pc, CT_CATCH, CT_FINALLY))
      {
         /* Replace CT_TRY with CT_CATCH on the stack & we are done */
         frm->pse[frm->pse_tos].type  = pc->type;
         frm->pse[frm->pse_tos].stage = (is_type(pc, CT_CATCH)) ?
               brace_stage_e::CATCH_WHEN : brace_stage_e::BRACE2;
         print_stack(LBCSSWAP, "=Swap   ", frm, pc);
         return(true);
      }

      /* Remove the CT_TRY and close the statement */
      frm->pse_tos--;
      print_stack(LBCSPOP, "-TRY-CCS ", frm, pc);
      retval_if(close_statement(frm, pc), true);
   }

   /* Check for optional parenthesis and optional CT_WHEN after CT_CATCH */
   if (frm->pse[frm->pse_tos].stage == brace_stage_e::CATCH_WHEN)
   {
      if (is_type(pc, CT_PAREN_OPEN))
      {
         /* Replace CT_PAREN_OPEN with CT_SPAREN_OPEN */
         set_type(pc, CT_SPAREN_OPEN);
         frm->pse[frm->pse_tos].type  = pc->type;
         frm->pse[frm->pse_tos].stage = brace_stage_e::PAREN1;
         return(false);
      }
      else if (is_type(pc, CT_WHEN))
      {
         frm->pse[frm->pse_tos].type  = pc->type;
         frm->pse[frm->pse_tos].stage = brace_stage_e::OP_PAREN1;
         return(true);
      }
      else if (is_type(pc, CT_BRACE_OPEN))
      {
         frm->pse[frm->pse_tos].stage = brace_stage_e::BRACE2;
         return(false);
      }
   }

   /* Check for CT_WHILE after the CT_DO */
   if (frm->pse[frm->pse_tos].stage == brace_stage_e::WHILE)
   {
      if (is_type(pc, CT_WHILE))
      {
         set_type(pc, CT_WHILE_OF_DO);
         frm->pse[frm->pse_tos].type  = CT_WHILE_OF_DO; //CT_WHILE;
         frm->pse[frm->pse_tos].stage = brace_stage_e::WOD_PAREN;
         return(true);
      }

      LOG_FMT(LWARN, "%s:%zu Error: Expected 'while', got '%s'\n",
              cpd.filename, pc->orig_line, pc->text());
      frm->pse_tos--;
      print_stack(LBCSPOP, "-Error  ", frm, pc);
      cpd.error_count++;
   }

   /* Insert a CT_VBRACE_OPEN, if needed */
   if ( not_type(pc, CT_BRACE_OPEN                     )   &&
       ((frm->pse[frm->pse_tos].stage == brace_stage_e::BRACE2  ) ||
        (frm->pse[frm->pse_tos].stage == brace_stage_e::BRACE_DO) ) )
   {
      if ((cpd.lang_flags & LANG_CS       ) &&
           is_type(pc, CT_USING_STMT) &&
          (!cpd.settings[UO_indent_using_block].b))
      {
         // don't indent the using block
      }
      else
      {
         parent = frm->pse[frm->pse_tos].type;

         chunk_t *vbrace = insert_vbrace_open_before(pc, frm);
         set_ptype(vbrace, parent);

         frm->level++;
         frm->brace_level++;

         push_fmr_pse(frm, vbrace, brace_stage_e::NONE, "+VBrace ");
         frm->pse[frm->pse_tos].parent = parent;

         /* update the level of pc */
         pc->level       = frm->level;
         pc->brace_level = frm->brace_level;

         /* Mark as a start of a statement */
         frm->stmt_count = 0;
         frm->expr_count = 0;
         set_flags(pc, PCF_STMT_START | PCF_EXPR_START);
         frm->stmt_count = 1;
         frm->expr_count = 1;
         LOG_FMT(LSTMT, "%zu] 2.marked %s as statement start\n", pc->orig_line, pc->text());
      }
   }

   /* Verify open parenthesis in complex statement */
   if ( not_type(pc, CT_PAREN_OPEN                      )   &&
       ((frm->pse[frm->pse_tos].stage == brace_stage_e::PAREN1   ) ||
        (frm->pse[frm->pse_tos].stage == brace_stage_e::WOD_PAREN) ) )
   {
      LOG_FMT(LWARN, "%s:%zu Error: Expected '(', got '%s' for '%s'\n",
              cpd.filename, pc->orig_line, pc->text(),
              get_token_name(frm->pse[frm->pse_tos].type));

      /* Throw out the complex statement */
      frm->pse_tos--;
      print_stack(LBCSPOP, "-Error  ", frm, pc);
      cpd.error_count++;
   }

   return(false);
}


static bool handle_complex_close(parse_frame_t *frm, chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(pc));
   chunk_t *next;

   if (frm->pse[frm->pse_tos].stage == brace_stage_e::PAREN1)
   {
      if(is_type(pc->next, CT_WHEN))
      {
         frm->pse[frm->pse_tos].type  = pc->type;
         frm->pse[frm->pse_tos].stage = brace_stage_e::CATCH_WHEN;
         return(true);
      }
      else
      {
         /* PAREN1 always => BRACE2 */
         frm->pse[frm->pse_tos].stage = brace_stage_e::BRACE2;
      }
   }
   else if (frm->pse[frm->pse_tos].stage == brace_stage_e::BRACE2)
   {
      /* BRACE2: IF => ELSE, anything else => close */
      if ((frm->pse[frm->pse_tos].type == CT_IF    ) ||
          (frm->pse[frm->pse_tos].type == CT_ELSEIF) )
      {
         frm->pse[frm->pse_tos].stage = brace_stage_e::ELSE;

         /* If the next chunk isn't CT_ELSE, close the statement */
         next = get_next_ncnl(pc);
         if (not_type(next, CT_ELSE))
         {
            frm->pse_tos--;
            print_stack(LBCSPOP, "-IF-HCS ", frm, pc);
            retval_if(close_statement(frm, pc), true);
         }
      }
      else if ((frm->pse[frm->pse_tos].type == CT_TRY  ) ||
               (frm->pse[frm->pse_tos].type == CT_CATCH) )
      {
         frm->pse[frm->pse_tos].stage = brace_stage_e::CATCH;

         /* If the next chunk isn't CT_CATCH or CT_FINALLY, close the statement */
         next = get_next_ncnl(pc);
         if(not_type(next, CT_CATCH, CT_FINALLY))
         {
            frm->pse_tos--;
            print_stack(LBCSPOP, "-TRY-HCS ", frm, pc);
            retval_if(close_statement(frm, pc), true);
         }
      }
      else
      {
         LOG_FMT(LNOTE, "%s: close_statement on %s brace_stage_e::BRACE2\n", __func__,
                 get_token_name(frm->pse[frm->pse_tos].type));
         frm->pse_tos--;
         print_stack(LBCSPOP, "-HCC B2 ", frm, pc);
         retval_if(close_statement(frm, pc), true);
      }
   }
   else if (frm->pse[frm->pse_tos].stage == brace_stage_e::BRACE_DO)
   {
      frm->pse[frm->pse_tos].stage = brace_stage_e::WHILE;
   }
   else if (frm->pse[frm->pse_tos].stage == brace_stage_e::WOD_PAREN)
   {
      LOG_FMT(LNOTE, "%s: close_statement on %s brace_stage_e::WOD_PAREN\n", __func__,
              get_token_name(frm->pse[frm->pse_tos].type));
      frm->pse[frm->pse_tos].stage = brace_stage_e::WOD_SEMI;
      print_stack(LBCSPOP, "-HCC WoDP ", frm, pc);
   }
   else if (frm->pse[frm->pse_tos].stage == brace_stage_e::WOD_SEMI)
   {
      LOG_FMT(LNOTE, "%s: close_statement on %s brace_stage_e::WOD_SEMI\n", __func__,
              get_token_name(frm->pse[frm->pse_tos].type));
      frm->pse_tos--;
      print_stack(LBCSPOP, "-HCC WoDS ", frm, pc);

      retval_if(close_statement(frm, pc), true);
   }
   else
   {
      /* PROBLEM */
      LOG_FMT(LWARN, "%s:%zu Error: TOS.type='%s' TOS.stage=%d\n",
              cpd.filename, pc->orig_line,
              get_token_name(frm->pse[frm->pse_tos].type),
              (unsigned int)frm->pse[frm->pse_tos].stage);
      cpd.error_count++;
   }
   return(false);
}


static chunk_t *insert_vbrace(chunk_t *pc, bool after, parse_frame_t *frm)
{
   LOG_FUNC_ENTRY();

   retval_if(is_invalid(pc), pc);

   chunk_t chunk;
   chunk.orig_line   = pc->orig_line;
   chunk.ptype = frm->pse[frm->pse_tos].type;
   chunk.level       = frm->level;
   chunk.brace_level = frm->brace_level;
   set_flags(&chunk, get_flags(pc, PCF_COPY_FLAGS));

   chunk.str         = "";
   chunk_t *rv;
   if (after)
   {
      chunk.type = CT_VBRACE_CLOSE;
      rv         = chunk_add_after(&chunk, pc);
   }
   else
   {
      chunk_t *ref = chunk_get_prev(pc);
      if (!is_preproc(ref))
      {
         clr_flags(&chunk, PCF_IN_PREPROC);
      }

      while(is_cmt_or_nl(ref))
      {
         ref->level++;
         ref->brace_level++;
         ref = chunk_get_prev(ref);
      }

      /* Don't back into a preprocessor */
      if (!is_preproc(pc ) &&
           is_preproc(ref) )
      {
         const bool is_pp_body = is_type(ref, CT_PREPROC_BODY);
         ref = (is_pp_body) ? get_prev_non_pp(ref): chunk_get_next (ref);
      }

      assert(is_valid(ref));
      chunk.orig_line = ref->orig_line;
      chunk.column    = ref->column + ref->len() + 1;
      chunk.type      = CT_VBRACE_OPEN;
      rv              = chunk_add_after(&chunk, ref);
   }
   return(rv);
}


static bool close_statement(parse_frame_t *frm, chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(pc));
   chunk_t *vbc = pc;

   LOG_FMT(LTOK, "%s:%zu] %s '%s' type %s stage %d\n", __func__,
           pc->orig_line, get_token_name(pc->type), pc->text(),
           get_token_name(frm->pse[frm->pse_tos].type),
           (unsigned int)frm->pse[frm->pse_tos].stage);

   if (cpd.consumed)
   {
      frm->stmt_count = 0;
      frm->expr_count = 0;
      LOG_FMT(LSTMT, "%s: %zu> reset2 statement on %s\n",
              __func__, pc->orig_line, pc->text());
   }

   /* Insert a CT_VBRACE_CLOSE, if needed:
    * If we are in a virtual brace and we are not ON a CT_VBRACE_CLOSE add one */
   if (frm->pse[frm->pse_tos].type == CT_VBRACE_OPEN)
   {
      /* If the current token has already been consumed, then add after it */
      if (cpd.consumed)
      {
         insert_vbrace_close_after(pc, frm);
      }
      else
      {
         /* otherwise, add before it and consume the vbrace */
         vbc = chunk_get_prev_ncnl(pc);
         vbc = insert_vbrace_close_after(vbc, frm);
         set_ptype(vbc, frm->pse[frm->pse_tos].parent);

         frm->level--;
         frm->brace_level--;
         frm->pse_tos--;

         /* Update the token level */
         pc->level       = frm->level;
         pc->brace_level = frm->brace_level;

         print_stack(LBCSPOP, "-CS VB  ", frm, pc);

         /* And repeat the close */
         close_statement(frm, pc);
         return(true);
      }
   }

   /* See if we are done with a complex statement */
   if (frm->pse[frm->pse_tos].stage != brace_stage_e::NONE)
   {
      retval_if(handle_complex_close(frm, vbc), true);
   }
   return(false);
}
