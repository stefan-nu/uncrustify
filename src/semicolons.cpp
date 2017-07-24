/**
 * @file semicolons.cpp
 * Removes extra semicolons
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "semicolons.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "ChunkStack.h"
#include "uncrustify.h"

#include <cstdio>
#include <cstdlib>
#include "unc_ctype.h"


static void remove_semicolon(
   chunk_t* pc /**< [in]  */
);


/**
 * We are on a semicolon that is after an unidentified brace close.
 * Check for what is before the brace open.
 * Do not remove if it is a square close, word, type, or paren close.
 */
static void check_unknown_brace_close(
   chunk_t* semi,       /**< [in]  */
   chunk_t* brace_close /**< [in]  */
);


static void remove_semicolon(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   LOG_FMT(LDELSEMI, "%s: Removed semicolon at line %u, col %u",
           __func__, pc->orig_line, pc->orig_col);
   log_func_stack_inline(LDELSEMI);
   /* TODO: do we want to shift stuff back a column? */
   chunk_del(pc);
}


void remove_extra_semicolons(void)
{
   LOG_FUNC_ENTRY();

   chunk_t* pc = chunk_get_head();
   while (is_valid(pc))
   {
      chunk_t* next = get_next_ncnl(pc);
      chunk_t* prev;
      if ( is_type(pc, CT_SEMICOLON) &&
          !is_preproc(pc) &&
          ((prev = get_prev_ncnl(pc)) != nullptr))
      {
         LOG_FMT(LSCANSEMI, "Semicolon on %u:%u parent=%s, prev = '%s' [%s/%s]\n",
            pc->orig_line, pc->orig_col, get_token_name(pc->ptype), prev->text(),
            get_token_name(prev->type),  get_token_name(prev->ptype));

         if (is_ptype(pc, CT_TYPEDEF))
         {
            /* keep it */
         }
         else if (is_type (prev,    CT_BRACE_CLOSE) &&
                  is_ptype(prev, 11,CT_ELSEIF, CT_FUNC_DEF,   CT_IF,
                 CT_FUNC_CLASS_DEF, CT_SWITCH, CT_USING_STMT, CT_FOR,
                 CT_OC_MSG_DECL,    CT_WHILE,  CT_NAMESPACE,  CT_ELSE))
         {
            remove_semicolon(pc);
         }
         else if (is_type_and_ptype(prev, CT_BRACE_CLOSE, CT_NONE))
         {
            check_unknown_brace_close(pc, prev);
         }
         else if (         is_type_and_not_ptype(prev, CT_SEMICOLON, CT_FOR         ) ||
                 (is_lang(LANG_D   ) && is_ptype(prev, CT_ENUM, CT_UNION, CT_STRUCT)) ||
                 (is_lang(LANG_JAVA) && is_ptype(prev, CT_SYNCHRONIZED             )) ||
                 (                      is_type (prev, CT_BRACE_OPEN               )) )
         {
            remove_semicolon(pc);
         }
      }

      pc = next;
   }
}


static void check_unknown_brace_close(chunk_t* semi, chunk_t* brace_close)
{
   LOG_FUNC_ENTRY();
   chunk_t* pc = get_prev_type(brace_close, CT_BRACE_OPEN, (int32_t)brace_close->level);
   pc = get_prev_ncnl(pc);

   if (not_type(pc, 6, CT_ANGLE_CLOSE,  CT_RETURN,  CT_WORD,
                       CT_SQUARE_CLOSE, CT_TSQUARE, CT_TYPE) &&
       !is_paren_close(pc) )
   {
      remove_semicolon(semi);
   }
}
