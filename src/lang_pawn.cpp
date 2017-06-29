/**
 * @file lang_pawn.cpp
 * Special functions for pawn stuff
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "lang_pawn.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "ChunkStack.h"
#include "uncrustify.h"


/**
 * Checks to see if a token continues a statement to the next line.
 * We need to check for 'open' braces/paren/etc because the level doesn't
 * change until the token after the open.
 */
static bool pawn_continued(
   chunk_t* pc,      /**< [in]  */
   uint32_t br_level /**< [in]  */
);


/**
 * Functions prototypes and definitions can only appear in level 0.
 *
 * Function prototypes start with "native", "forward", or are just a function
 * with a trailing semicolon instead of a open brace (or something else)
 *
 * somefunc(params)              <-- def
 * stock somefunc(params)        <-- def
 * somefunc(params);             <-- proto
 * forward somefunc(params)      <-- proto
 * native somefunc[rect](params) <-- proto
 *
 * Functions start with 'stock', 'static', 'public', or '@' (on level 0)
 *
 * Variable definitions start with 'stock', 'static', 'new', or 'public'.
 */
static chunk_t* pawn_process_line(
   chunk_t* start /**< [in]  */
);


/**
 * We are on a level 0 function proto of def
 */
static chunk_t* pawn_mark_function0(
   chunk_t* start, /**< [in]  */
   chunk_t* fcn    /**< [in]  */
);


/**
 * follows a variable definition at level 0 until the end.
 * Adds a semicolon at the end, if needed.
 */
static chunk_t* pawn_process_variable(
   chunk_t* start /**< [in]  */
);


/**
 * tbd
 */
static chunk_t* pawn_process_func_def(
   chunk_t* pc /**< [in]  */
);


chunk_t* pawn_add_vsemi_after(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   retval_if((is_invalid(pc) || is_semicolon(pc)), pc);

   chunk_t* next = get_next_nc(pc);
   retval_if(is_semicolon(next), pc);

   chunk_t chunk = *pc;
   chunk.type    = CT_VSEMICOLON;
   chunk.str     = is_true(UO_mod_pawn_semicolon) ? ";" : "";
   chunk.column += pc->len();
   chunk.ptype   = CT_NONE;

   LOG_FMT(LPVSEMI, "%s: Added VSEMI on line %u, prev='%s' [%s]\n",
           __func__, pc->orig_line, pc->text(), get_token_name(pc->type));

   return(chunk_add_after(&chunk, pc));
}


void pawn_scrub_vsemi(void)
{
   LOG_FUNC_ENTRY();
   return_if(is_false(UO_mod_pawn_semicolon));

   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if(not_type(pc, CT_VSEMICOLON));
      chunk_t* prev = get_prev_ncnl(pc);

      if (is_type (prev,    CT_BRACE_CLOSE) &&
          is_ptype(prev, 5, CT_ELSE, CT_IF, CT_SWITCH,
                            CT_CASE, CT_WHILE_OF_DO))
      {
         pc->str.clear();
      }
   }
}


static bool pawn_continued(chunk_t* pc, uint32_t br_level)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(pc), false);

   if ((pc->level > br_level) ||
         is_type(pc, 15, CT_ARITH, CT_CARET,  CT_QUESTION, CT_BRACE_OPEN,
                         CT_BOOL,  CT_ASSIGN, CT_COMMA,    CT_VBRACE_OPEN,
                         CT_IF,    CT_ELSE,   CT_COMPARE,  CT_FPAREN_OPEN,
                         CT_DO,    CT_WHILE,  CT_SWITCH                  ) ||
         is_ptype(pc, 9, CT_IF,    CT_ELSE,   CT_ELSEIF,   CT_FUNC_DEF,
                         CT_FOR,   CT_WHILE,  CT_SWITCH,   CT_DO, CT_ENUM) ||
       is_flag(pc, (PCF_IN_ENUM | PCF_IN_STRUCT)) ||
       is_str(pc, ":") ||
       is_str(pc, "+") ||
       is_str(pc, "-") )
   {
      return(true);
   }
   return(false);
}


void pawn_prescan(void)
{
   LOG_FUNC_ENTRY();

   /* Start at the beginning and step through the entire file, and clean up
    * any questionable stuff */
   bool    did_nl = true;
   chunk_t* pc    = chunk_get_head();
   while (is_valid(pc))
   {
      if( (did_nl == true) && is_level(pc, 0) &&
          not_type(pc, CT_PREPROC, CT_NEWLINE, CT_NL_CONT))
      {
         /* pc now points to the start of a line */
         pc = pawn_process_line(pc);
      }
      /* note that continued lines are ignored */
      if (is_valid(pc))
      {
         did_nl = (is_type(pc, CT_NEWLINE));
      }

      pc = get_next_nc(pc);
   }
}


static chunk_t* pawn_process_line(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   if(is_type(start, CT_NEW ) ||
      is_str (start, "const") )
   {
      return(pawn_process_variable(start));
   }

   /* if a open paren is found before an assign, then this is a function */
   chunk_t* fcn = nullptr;
   if(is_type(start, CT_WORD))
   {
      fcn = start;
   }
   chunk_t* pc = start;
   while (((pc = get_next_nc(pc)) != nullptr) &&
          !is_str(pc, "(") &&
          not_type(pc, CT_ASSIGN, CT_NEWLINE))
   {
      if ((pc->level == 0) &&
           is_type(pc, CT_FUNCTION, CT_WORD, CT_OPERATOR_VAL))
      {
         fcn = pc;
      }
   }
   retval_if(is_type(pc, CT_ASSIGN), pawn_process_variable(pc));

   if (is_valid(fcn))
   {
      //LOG_FMT(LSYS, "FUNCTION: %s\n", fcn->text());
      return(pawn_mark_function0(start, fcn));
   }

   if (is_type(start, CT_ENUM))
   {
      pc = get_next_type(start, CT_BRACE_CLOSE, (int32_t)start->level);
      return(pc);
   }

   return(start);
}


static chunk_t* pawn_process_variable(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   chunk_t* prev = nullptr;
   chunk_t* pc   = start;
   while ((pc = get_next_nc(pc)) != nullptr)
   {
      if (is_type (pc, CT_NEWLINE                     ) &&
          (pawn_continued(prev, start->level) == false) )
      {
         if(not_type(prev, CT_SEMICOLON, CT_VSEMICOLON))
         {
            pawn_add_vsemi_after(prev);
         }
         break;
      }
      prev = pc;
   }
   return(pc);
}


void pawn_add_virtual_semicolons(void)
{
   LOG_FUNC_ENTRY();

   /** Add Pawn virtual semicolons */
   if (is_lang(LANG_PAWN))
   {
      chunk_t* prev = nullptr;
      chunk_t* pc   = chunk_get_head();
      while ((pc = chunk_get_next(pc)) != nullptr)
      {
         if (!is_cmt_or_nl(pc) &&
             (not_type(pc, CT_VBRACE_CLOSE, CT_VBRACE_OPEN)))
         {
            prev = pc;
         }

         continue_if((is_invalid(prev)) ||
                 not_type(pc, CT_NEWLINE, CT_BRACE_CLOSE, CT_VBRACE_CLOSE));

         /* we just hit a newline and we have a previous token */
         if ((!is_preproc(prev)) && not_flag(prev, (PCF_IN_ENUM | PCF_IN_STRUCT)) &&
              not_type(prev, CT_VSEMICOLON, CT_SEMICOLON) &&
             !pawn_continued(prev, prev->brace_level))
         {
            pawn_add_vsemi_after(prev);
            prev = nullptr;
         }
      }
   }
}


static chunk_t* pawn_mark_function0(chunk_t* start, chunk_t* fcn)
{
   LOG_FUNC_ENTRY();

   /* handle prototypes */
   if (start == fcn)
   {
      chunk_t* last = get_next_type(fcn, CT_PAREN_CLOSE, (int32_t)fcn->level);
      last = chunk_get_next(last);

      if(is_type(last, CT_SEMICOLON))
      {
         LOG_FMT(LPFUNC, "%s: %u] '%s' proto due to semicolon\n",
                 __func__, fcn->orig_line, fcn->text());
         set_type(fcn, CT_FUNC_PROTO);
         return(last);
      }
   }
   else
   {
      if (is_type(start, CT_FORWARD, CT_NATIVE))
      {
         LOG_FMT(LPFUNC, "%s: %u] '%s' [%s] proto due to %s\n",
                 __func__, fcn->orig_line, fcn->text(),
                 get_token_name(fcn->type), get_token_name(start->type));
         set_type(fcn, CT_FUNC_PROTO);
         return(get_next_nc(fcn));
      }
   }

   /* Not a prototype, so it must be a function def */
   return(pawn_process_func_def(fcn));
}


static chunk_t* pawn_process_func_def(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   /* We are on a function definition */
   set_type(pc, CT_FUNC_DEF);

   LOG_FMT(LPFUNC, "%s: %u:%u %s\n",
           __func__, pc->orig_line, pc->orig_col, pc->text());

   /* If we don't have a brace open right after the close fparen, then
    * we need to add virtual braces around the function body.
    */
   chunk_t* clp  = get_next_str(pc, ")", 1, 0);
   chunk_t* last = get_next_ncnl(clp);

   if (is_valid(last))
   {
      LOG_FMT(LPFUNC, "%s: %u] last is '%s' [%s]\n",
              __func__, last->orig_line, last->text(), get_token_name(last->type));
   }

   /* See if there is a state clause after the function */
   if (is_str(last, "<"))
   {
      LOG_FMT(LPFUNC, "%s: %u] '%s' has state angle open %s\n",
              __func__, pc->orig_line, pc->text(), get_token_name(last->type));

      set_type_and_ptype(last, CT_ANGLE_OPEN, CT_FUNC_DEF);
      while (((last = chunk_get_next(last)) != nullptr) &&
             !is_str(last, ">"))
      {
         /* do nothing just search, \todo use search_chunk */
      }

      if (is_valid(last))
      {
         LOG_FMT(LPFUNC, "%s: %u] '%s' has state angle close %s\n",
                 __func__, pc->orig_line, pc->text(), get_token_name(last->type));
         set_type_and_ptype(last, CT_ANGLE_CLOSE, CT_FUNC_DEF);
      }
      last = get_next_ncnl(last);
   }

   retval_if(is_invalid(last), last);

   if (is_type(last, CT_BRACE_OPEN))
   {
      set_ptype(last, CT_FUNC_DEF);
      last = get_next_type(last, CT_BRACE_CLOSE, (int32_t)last->level);
      if (is_valid(last))
      {
         set_ptype(last, CT_FUNC_DEF);
      }
   }
   else
   {
      LOG_FMT(LPFUNC, "%s: %u] '%s' fdef: expected brace open: %s\n",
              __func__, pc->orig_line, pc->text(), get_token_name(last->type));

      /* do not insert a vbrace before a preproc */
      retval_if(is_preproc(last), last);

      chunk_t chunk = *last;
      chunk.str.clear();
      chunk.type  = CT_VBRACE_OPEN;
      chunk.ptype = CT_FUNC_DEF;

      chunk_t* prev = chunk_add_before(&chunk, last);
      last = prev;

      /* find the next newline at level 0 */
      prev = get_next_ncnl(prev);
      assert(is_valid(prev));
      do
      {
         LOG_FMT(LPFUNC, "%s:%u] check %s, level %u\n",
                 __func__, prev->orig_line, get_token_name(prev->type), prev->level);
         if (is_type(prev, CT_NEWLINE) &&
             (prev->level == 0       ) )
         {
            chunk_t* next = get_next_ncnl(prev);
            break_if(not_type(next, CT_ELSE, CT_WHILE_OF_DO));
         }
         prev->level++;
         prev->brace_level++;
         last = prev;
      } while ((prev = chunk_get_next(prev)) != nullptr);

      if (is_valid(last))
      {
         LOG_FMT(LPFUNC, "%s:%u] ended on %s, level %u\n",
                 __func__, last->orig_line, get_token_name(last->type), last->level);
      }

      assert(is_valid(last));
      chunk = *last;
      chunk.str.clear();
      chunk.column     += last->len();
      chunk.type        = CT_VBRACE_CLOSE;
      chunk.level       = 0;
      chunk.brace_level = 0;
      chunk.ptype       = CT_FUNC_DEF;
      last              = chunk_add_after(&chunk, last);
   }
   return(last);
}


chunk_t* pawn_check_vsemicolon(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   /* Grab the open VBrace */
   const chunk_t* vb_open = get_prev_type(pc, CT_VBRACE_OPEN);

   /** Grab the item before the newline
    * Don't do anything if:
    *  - the only thing previous is the V-Brace open
    *  - in a preprocessor
    *  - level > (vb_open->level + 1) -- ie, in () or []
    *  - it is something that needs a continuation
    *    + arith, assign, bool, comma, compare */
   chunk_t* prev = get_prev_ncnl(pc);
   if ((is_invalid(prev) ) ||
       (prev == vb_open  ) ||
       is_preproc(prev) ||
       pawn_continued(prev, vb_open->level + 1u))
   {
      if (is_valid(prev))
      {
         LOG_FMT(LPVSEMI, "%s:  no  VSEMI on line %u, prev='%s' [%s]\n",
                 __func__, prev->orig_line, prev->text(), get_token_name(prev->type));
      }
      return(pc);
   }

   return(pawn_add_vsemi_after(prev));
}
