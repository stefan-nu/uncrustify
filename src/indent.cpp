/**
 * @file indent.cpp
 * Does all the indenting stuff.
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */
#include "indent.h"
#include <algorithm>
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "options_for_QT.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "unc_ctype.h"
#include "uncrustify.h"
#include "align.h"
#include "options.h"
#include "space.h"
#include "parse_frame.h"
#include "tabulator.h"
#include "token_enum.h"


/**
 * General indenting approach:
 * Indenting levels are put into a stack.
 *
 * The stack entries contain:
 *  - opening type
 *  - brace column
 *  - continuation column
 *
 * Items that start a new stack item:
 *  - preprocessor (new parse frame)
 *  - Brace Open (Virtual brace also)
 *  - Paren, Square, Angle open
 *  - Assignments
 *  - C++ '<<' operator (ie, cout << "blah")
 *  - case
 *  - class colon
 *  - return
 *  - types
 *  - any other continued statement
 *
 * Note that the column of items marked 'PCF_WAS_ALIGNED' is not changed.
 *
 * For an open brace:
 *  - indent increases by indent_columns
 *  - if part of if/else/do/while/switch/etc, an extra indent may be applied
 *  - if in a paren, then cont-col is set to column + 1, ie "({ some code })"
 *
 * Open paren/square/angle:
 * cont-col is set to the column of the item after the open paren, unless
 * followed by a newline, then it is set to (brace-col + indent_columns).
 * Examples:
 *    a_really_long_funcion_name(
 *       param1, param2);
 *    a_really_long_funcion_name(param1,
 *                               param2);
 *
 * Assignments:
 * Assignments are continued aligned with the first item after the assignment,
 * unless the assign is followed by a newline.
 * Examples:
 *    some.variable = asdf + asdf +
 *                    asdf;
 *    some.variable =
 *       asdf + asdf + asdf;
 *
 * C++ << operator:
 * Handled the same as assignment.
 * Examples:
 *    cout << "this is test number: "
 *         << test_number;
 *
 * case:
 * Started with case or default.
 * Terminated with close brace at level or another case or default.
 * Special indenting according to various rules.
 *  - indent of case label
 *  - indent of case body
 *  - how to handle optional braces
 * Examples:
 * {
 * case x: {
 *    a++;
 *    break;
 *    }
 * case y:
 *    b--;
 *    break;
 * default:
 *    c++;
 *    break;
 * }
 *
 * Class colon:
 * Indent continuation by indent_columns:
 * class my_class :
 *    baseclass1,
 *    baseclass2
 * {
 *
 * Return: same as assignments
 * If the return statement is not fully paren'd, then the indent continues at
 * the column of the item after the return. If it is paren'd, then the paren
 * rules apply.
 * return somevalue +
 *        othervalue;
 *
 * Type: pretty much the same as assignments
 * Examples:
 * int foo,
 *     bar,
 *     baz;
 *
 * Any other continued item:
 * There shouldn't be anything not covered by the above cases, but any other
 * continued item is indented by indent_columns:
 * Example:
 * somereallycrazylongname.with[lotsoflongstuff].
 *    thatreallyannoysme.whenIhavetomaintain[thecode] = 3;
 */


 enum class align_mode_e : uint32_t
 {
    SHIFT,     /**< shift relative to the current column */
    KEEP_ABS,  /**< try to keep the original absolute column */
    KEEP_REL   /**< try to keep the original gap */
 };


/**
 * REVISIT: This needs to be re-checked, maybe cleaned up
 *
 * Indents comments in a (hopefully) smart manner.
 *
 * There are two type of comments that get indented:
 *  - stand alone (ie, no tokens on the line before the comment)
 *  - trailing comments (last token on the line apart from a linefeed)
 *    + note that a stand-alone comment is a special case of a trailing
 *
 * The stand alone comments will get indented in one of three ways:
 *  - column 1:
 *    + There is an empty line before the comment AND the indent level is 0
 *    + The comment was originally in column 1
 *
 *  - Same column as trailing comment on previous line (ie, aligned)
 *    + if originally within TBD (3) columns of the previous comment
 *
 *  - syntax indent level
 *    + doesn't fit in the previous categories
 *
 * Options modify this behavior:
 *  - keep original column (don't move the comment, if possible)
 *  - keep relative column (move out the same amount as first item on line)
 *  - fix trailing comment in column TBD
 */
static void indent_comment(
   chunk_t* pc,  /**< [in] The comment, which is the first item on a line */
   uint32_t  col   /**< [in] The column if this is to be put at indent level */
);


/**
 * Starts a new entry
 */
static void indent_pse_push(
   parse_frame_t &frm, /**< [in] The parse frame */
   chunk_t*      pc    /**< [in] The chunk causing the push */
);


/**
 * Removes the top entry
 */
static void indent_pse_pop(
   parse_frame_t &frm, /**< [in] The parse frame */
   chunk_t*      pc    /**< [in] The chunk causing the push */
);


/**
 * tbd
 */
static uint32_t token_indent(
   c_token_t type  /**< [in]  */
);


/**
 * tbd
 */
static uint32_t calc_indent_continue(
   const parse_frame_t &frm,    /**< [in] The parse frame */
   uint32_t              pse_tos  /**< [in]  */
);


/**
 * We are on a '{' that has parent = OC_BLOCK_EXPR
 * find the column of the param tag
 */
static chunk_t* oc_msg_block_indent(
   chunk_t* pc,      /**< [in]  */
   bool from_brace,  /**< [in]  */
   bool from_caret,  /**< [in]  */
   bool from_colon,  /**< [in]  */
   bool from_keyword /**< [in]  */
);


/**
 * We are on a '{' that has parent = OC_BLOCK_EXPR
 */
static chunk_t* oc_msg_prev_colon(
   chunk_t* pc /**< [in]  */
);


/**
 * returns true if forward scan reveals only single newlines or comments
 * stops when hits code
 * false if next thing hit is a closing brace, also if 2 newlines in a row
 */
static bool single_line_comment_indent_rule_applies(
   chunk_t* start /**< [in]  */
);


/**
 * tbd
 */
void log_and_reindent(
   chunk_t*     pc,  /**< [in] chunk to operate with */
   const uint32_t val, /**< [in] value to print */
   const char*  str  /**< [in] string to print */
);


/**
 * tbd
 */
void log_and_indent_comment(
   chunk_t*      pc, /**< [in] chunk to operate with */
   const uint32_t val, /**< [in] value to print */
   const char*  str  /**< [in] string to print */
);


/**   */
static const char* get_align_mode_name(
   const align_mode_e align_mode /**< [in]  */
);


static const char* get_align_mode_name(const align_mode_e align_mode)
{
   switch(align_mode)
   {
      case(align_mode_e::SHIFT   ): return ("sft"  );
      case(align_mode_e::KEEP_ABS): return ("abs"  );
      case(align_mode_e::KEEP_REL): return ("rel"  );
      default:                      return ("error");
   }
}


void indent_to_column(chunk_t* pc, uint32_t column)
{
   LOG_FUNC_ENTRY();
   column = max(column, pc->column);
   reindent_line(pc, column);
}


void align_to_column(chunk_t* pc, uint32_t column)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc) || (column == pc->column));

   LOG_FMT(LINDLINE, "%s(%d): %u] col %u on %s [%s] => %u\n",
           __func__, __LINE__, pc->orig_line, pc->column, pc->text(),
           get_token_name(pc->type), column);

   uint32_t col_delta = column - pc->column;
   uint32_t min_col   = column;
   pc->column = column;
   do
   {
      align_mode_e almod = align_mode_e::SHIFT;

      chunk_t* next = chunk_get_next(pc);
      break_if(is_invalid(next));

      int32_t min_delta = (int32_t)space_col_align(pc, next);
      min_col  = (uint32_t)((int32_t)min_col + min_delta);
      chunk_t  *prev = pc;
      pc       = next;

      if (is_cmt(pc) && not_ptype(pc, CT_COMMENT_EMBED))
      {
         almod = (is_single_line_cmt(pc) &&
                  is_true(UO_indent_rel_single_line_comments)) ?
                  align_mode_e::KEEP_REL : align_mode_e::KEEP_ABS;
      }

      switch(almod)
      {
         case(align_mode_e::KEEP_ABS): /* Keep same absolute column */
            pc->column = pc->orig_col;
            pc->column = max(min_col, pc->column);
         break;

         case(align_mode_e::KEEP_REL): { /* Keep same relative column */
            int32_t orig_delta = (int32_t)pc->orig_col - (int32_t)prev->orig_col;
            orig_delta = max(min_delta, orig_delta);
            pc->column = (uint32_t)((int32_t)prev->column + orig_delta); }
         break;

         case(align_mode_e::SHIFT): /* Shift by the same amount */
            pc->column += col_delta;
            pc->column = max(min_col, pc->column);
         break;

         default: /* unknown enum, do nothing */ break;
      }


      LOG_FMT(LINDLINED, "   %s set column of %s on line %u to col %u (orig %u)\n",
            get_align_mode_name(almod), get_token_name(pc->type), pc->orig_line,
            pc->column, pc->orig_col);
   } while (is_valid(pc) && (pc->nl_count == 0));
}


void reindent_line(chunk_t* pc, uint32_t column)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));
   LOG_FMT(LINDLINE, "%s(%d): %u] col %u on '%s' [%s/%s] => %u",
           __func__, __LINE__, pc->orig_line, pc->column, pc->text(),
           get_token_name(pc->type), get_token_name(pc->ptype), column);
   log_func_stack_inline(LINDLINE);

   return_if(column == pc->column);

   uint32_t col_delta = column - pc->column;
   uint32_t min_col   = column;

   pc->column = column;
   do
   {
      if (QT_SIGNAL_SLOT_found)
      {
         // connect(&mapper, SIGNAL(mapped(QString &)), this, SLOT(onSomeEvent(QString &)));
         // look for end of SIGNAL/SLOT block
         if (not_flag(pc, PCF_IN_QT_MACRO))
         {
            LOG_FMT(LINDLINE, "FLAGS is NOT set: PCF_IN_QT_MACRO\n");
            restore_options_for_QT();
         }
      }
      else
      {
         /* look for begin of SIGNAL/SLOT block */
         if (is_flag(pc, PCF_IN_QT_MACRO))
         {
            LOG_FMT(LINDLINE, "FLAGS is set: PCF_IN_QT_MACRO\n");
            save_set_options_for_QT(pc->level);
         }
      }
      chunk_t* next = chunk_get_next(pc);
      break_if (is_invalid(next));

      if (pc->nl_count > 0)
      {
         min_col   = 0;
         col_delta = 0;
      }
      min_col += space_col_align(pc, next);
      pc       = next;

      bool is_comment = is_cmt(pc);
      bool keep       = is_comment && is_single_line_cmt(pc) &&
                        is_true(UO_indent_rel_single_line_comments);

      if ((is_comment      == true      ) &&
          not_ptype(pc, CT_COMMENT_EMBED) &&
          (keep            == false     ) )
      {
         pc->column = pc->orig_col;
         pc->column = max(min_col, pc->column);

         LOG_FMT(LINDLINE, "%s(%d): set comment on line %u to col %u (orig %u)\n",
                 __func__, __LINE__, pc->orig_line, pc->column, pc->orig_col);
      }
      else
      {
         pc->column += col_delta;
         pc->column = max(min_col, pc->column);

         LOG_FMT(LINDLINED, "   set column of '%s' to %u (orig %u)\n",
         (is_type(pc, CT_NEWLINE)) ? "newline" : pc->text(), pc->column, pc->orig_col);
      }
   } while (is_valid(pc) && (pc->nl_count == 0));
}


static void indent_pse_push(parse_frame_t &frm, chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(pc));

   /* check the stack depth */
   if (frm.pse_tos < (ARRAY_SIZE(frm.pse) - 1))
   {
      static uint32_t ref = 0;
      /* Bump up the index and initialize it */
      frm.pse_tos++;
      LOG_FMT(LINDLINE, "%s(%d): line=%u, pse_tos=%u, type=%s\n",
              __func__, __LINE__, pc->orig_line, frm.pse_tos, get_token_name(pc->type));

      uint32_t index = frm.pse_tos;
      memset(&frm.pse[index], 0, sizeof(paren_stack_entry_t));

      //LOG_FMT(LINDPSE, "%s(%d):%d] (pp=%d) OPEN  [%d,%s] level=%d\n",
      //        __func__, __LINE__, pc->orig_line, cpd.pp_level, frm.pse_tos, get_token_name(pc->type), pc->level);

      frm.pse[frm.pse_tos].pc          = pc;
      frm.pse[frm.pse_tos].type        = pc->type;
      frm.pse[frm.pse_tos].level       = pc->level;
      frm.pse[frm.pse_tos].open_line   = pc->orig_line;
      frm.pse[frm.pse_tos].ref         = (int32_t)++ref;
      frm.pse[frm.pse_tos].in_preproc  = is_preproc(pc);
      frm.pse[frm.pse_tos].indent_tab  = frm.pse[frm.pse_tos-1].indent_tab;
      frm.pse[frm.pse_tos].indent_cont = frm.pse[frm.pse_tos-1].indent_cont;
      frm.pse[frm.pse_tos].non_vardef  = false;
      frm.pse[frm.pse_tos].ns_cnt      = frm.pse[frm.pse_tos-1].ns_cnt;
      memcpy(&frm.pse[frm.pse_tos].ip,  &frm.pse[frm.pse_tos-1].ip, sizeof(frm.pse[frm.pse_tos].ip));
   }
   else
   {
      /* the stack depth is too small */
      /* fatal error */
      fprintf(stderr, "the stack depth is too small\n");
      exit(EXIT_FAILURE);
   }
}


static void indent_pse_pop(parse_frame_t &frm, chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   /* Bump up the index and initialize it */
   if (frm.pse_tos > 0)
   {
      if (is_valid(pc))
      {
         LOG_FMT(LINDPSE, "%4u] (pp=%u) CLOSE [%u,%s] on %s, started on line %u, level=%u/%u\n",
                 pc->orig_line, cpd.pp_level, frm.pse_tos,
                 get_token_name(frm.pse[frm.pse_tos].type), get_token_name(pc->type),
                 frm.pse[frm.pse_tos].open_line, frm.pse[frm.pse_tos].level, pc->level);
      }
      else
      {
         LOG_FMT(LINDPSE, " EOF] CLOSE [%u,%s], started on line %u\n",
                 frm.pse_tos, get_token_name(frm.pse[frm.pse_tos].type),
                 frm.pse[frm.pse_tos].open_line);
      }

      /* Don't clear the stack entry because some code 'cheats' and uses the
       * just-popped indent values */
      frm.pse_tos--;
      LOG_FMT(LINDLINE, "(%d) ", __LINE__);
      if (is_valid(pc))
      {
         LOG_FMT(LINDLINE, "%s(%d): orig_line=%u, pse_tos=%u, type=%s\n",
                 __func__, __LINE__, pc->orig_line, frm.pse_tos, get_token_name(pc->type));
      }
      else
      {
         LOG_FMT(LINDLINE, "%s(%d): ------------------- pse_tos=%u\n",
                 __func__, __LINE__, frm.pse_tos);
      }
   }
   else
   {
      /* fatal error */
      fprintf(stderr, "the stack index is already zero\n");
      assert(is_valid(pc));
      fprintf(stderr, "at line=%u, type is %s\n",
              pc->orig_line, get_token_name(pc->type));
      exit(EXIT_FAILURE);
   }
}


static uint32_t token_indent(c_token_t type)
{
   switch (type)
   {
      case CT_IF:           /* falltrough */
      case CT_DO:           return(3);
      case CT_FOR:          /* falltrough */
      case CT_ELSE:         return(4);  /* wacky, but that's what is wanted */
      case CT_WHILE:        /* falltrough */
      case CT_USING_STMT:   return(6);
      case CT_SWITCH:       return(7);
      case CT_ELSEIF:       return(8);
      case CT_SYNCHRONIZED: return(13);
      default:              return(0);
   }
}


#define indent_column_set(X)                                                  \
   do {                                                                       \
      indent_column = (X);                                                    \
      LOG_FMT(LINDENT2, "%s:[line %d], orig_line=%u, indent_column = %u\n", \
              __func__, __LINE__, pc->orig_line, indent_column);              \
   } while (0)


static uint32_t calc_indent_continue(const parse_frame_t &frm, uint32_t pse_tos)
{
   int32_t ic = cpd.settings[UO_indent_continue].n;

   if ((ic < 0) &&
       frm.pse[pse_tos].indent_cont)
   {
      return(frm.pse[pse_tos].indent);
   }
   return(frm.pse[pse_tos].indent + (uint32_t)abs(ic));
}


static chunk_t* oc_msg_block_indent(chunk_t* pc, bool from_brace,
      bool from_caret, bool from_colon,  bool from_keyword)
{
   LOG_FUNC_ENTRY();
   chunk_t* tmp = get_prev_nc(pc);

   retval_if(from_brace, pc);

   if (is_paren_close(tmp))
   {
      tmp = get_prev_nc(chunk_skip_to_match_rev(tmp));
   }
   retval_if(is_invalid_or_not_type(tmp, CT_OC_BLOCK_CARET), nullptr);
   retval_if(from_caret, tmp);

   tmp = get_prev_nc(tmp);
   retval_if(is_invalid_or_not_type(tmp, CT_OC_COLON), nullptr);
   retval_if(from_colon, tmp);

   tmp = get_prev_nc(tmp);
   if (is_invalid (tmp                                ) ||
       not_type(tmp, CT_OC_MSG_NAME, CT_OC_MSG_FUNC) )
   {
      return(nullptr);
   }
   retval_if(from_keyword, tmp);
   return(nullptr);
}


static chunk_t* oc_msg_prev_colon(chunk_t* pc)
{
   return(get_prev_type(pc, CT_OC_COLON, (int32_t)pc->level, scope_e::ALL));
}


void indent_text(void)
{
   LOG_FUNC_ENTRY();
   chunk_t*      pc;
   chunk_t*      next;
   chunk_t*      prev           = nullptr;
   bool          my_did_newline = true;
   int32_t       idx;
   uint32_t      vardefcol    = 0;
   uint32_t      shiftcontcol = 0;
   uint32_t      indent_size  = cpd.settings[UO_indent_columns].u;
   parse_frame_t frm;
   bool          in_preproc = false;
   uint32_t      indent_column;
   uint32_t      parent_token_indent = 0;
   int32_t       xml_indent          = 0;
   bool          token_used;
   uint32_t      sql_col      = 0;
   uint32_t      sql_orig_col = 0;
   bool          in_func_def  = false;
   c_token_t     memtype;

   memset(&frm, 0, sizeof(frm));
   cpd.frame_count = 0;

   /* dummy top-level entry */
   frm.pse[0].indent     = 1;
   frm.pse[0].indent_tmp = 1;
   frm.pse[0].indent_tab = 1;
   frm.pse[0].type       = CT_EOF;

   pc = chunk_get_head();
   while (is_valid(pc))
   {
      if (is_type(pc, CT_NEWLINE))
      {
         LOG_FMT(LINDLINE, "%s(%d): %u NEWLINE\n",
                 __func__, __LINE__, pc->orig_line);
      }
      else if (pc->type == CT_NL_CONT)
      {
         LOG_FMT(LINDLINE, "%s(%d): %u CT_NL_CONT\n",
                 __func__, __LINE__, pc->orig_line);
      }
      else
      {
         LOG_FMT(LINDLINE, "%s(%d): %u:%u pc->text() %s\n",
                 __func__, __LINE__, pc->orig_line, pc->orig_col, pc->text());
         log_pcf_flags(LINDLINE, get_flags(pc));
      }
      if ((is_true(UO_use_options_overriding_for_qt_macros)) &&
          ((strcmp(pc->text(), "SIGNAL") == 0) ||
           (strcmp(pc->text(), "SLOT"  ) == 0) ) )
      {
         LOG_FMT(LINDLINE, "%s(%d): orig_line=%u: type %s SIGNAL/SLOT found\n",
                 __func__, __LINE__, pc->orig_line, get_token_name(pc->type));
      }
      /* Handle preprocessor transitions */
      in_preproc = is_preproc(pc);

      if (is_true(UO_indent_brace_parent))
      {
         parent_token_indent = token_indent(pc->ptype);
      }

      /* Handle "force indentation of function definition to start in column 1" */
      if (is_true(UO_indent_func_def_force_col1))
      {
         if (!in_func_def)
         {
            next = get_next_ncnl(pc);
            if (is_ptype(pc, CT_FUNC_DEF) ||
               (is_type (pc, CT_COMMENT ) && is_ptype(next, CT_FUNC_DEF)))
            {
               in_func_def = true;
               indent_pse_push(frm, pc);
               frm.pse[frm.pse_tos].indent_tmp = 1;
               frm.pse[frm.pse_tos].indent     = 1;
               frm.pse[frm.pse_tos].indent_tab = 1;
            }
         }
         else
         {
            prev = chunk_get_prev(pc);
            assert(is_valid(prev));
            if (is_type_and_ptype(prev, CT_BRACE_CLOSE, CT_FUNC_DEF))
            {
               in_func_def = false;
               indent_pse_pop(frm, pc);
            }
         }
      }

      /* Clean up after a #define, etc */
      if (!in_preproc)
      {
         while ((frm.pse_tos > 0) &&
                 frm.pse[frm.pse_tos].in_preproc)
         {
            c_token_t type = frm.pse[frm.pse_tos].type;
            indent_pse_pop(frm, pc);

            /* If we just removed an #endregion, then check to see if a
             * PP_REGION_INDENT entry is right below it */
            if ((type == CT_PP_ENDREGION) &&
                (frm.pse[frm.pse_tos].type == CT_PP_REGION_INDENT))
            {
               indent_pse_pop(frm, pc);
            }
         }
      }
      else if (is_type(pc, CT_PREPROC))
      {
         /* Close out PP_IF_INDENT before playing with the parse frames */
         if ((frm.pse[frm.pse_tos].type == CT_PP_IF_INDENT) &&
              is_ptype(pc, CT_PP_ENDIF, CT_PP_ELSE        ) )
         {
            indent_pse_pop(frm, pc);
         }

         pf_check(&frm, pc);

         /* Indent the body of a #region here */
         if (is_true(UO_pp_region_indent_code) && is_ptype(pc, CT_PP_REGION))
         {
            next = chunk_get_next(pc);
            break_if (is_invalid(next));

            /* Hack to get the logs to look right */
            set_type(next, CT_PP_REGION_INDENT);
            indent_pse_push(frm, next);
            set_type(next, CT_PP_REGION);

            /* Indent one level */
            frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent     + indent_size;
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos-1].indent_tab + indent_size;
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
            frm.pse[frm.pse_tos].in_preproc = false;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }

         /* Indent the body of a #if here */
         if (is_true(UO_pp_if_indent_code) &&
             is_ptype(pc, CT_PP_IF, CT_PP_ELSE))
         {
            next = chunk_get_next(pc);
            break_if(is_invalid(next));
            /* Hack to get the logs to look right */
            memtype = next->type;
            set_type(next, CT_PP_IF_INDENT);
            indent_pse_push(frm, next);
            set_type(next, memtype);

            /* Indent one level except if the #if is a #include guard */
            uint32_t extra = ((pc->pp_level == 0) && ifdef_over_whole_file()) ? 0 : indent_size;
            frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent     + extra;
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos-1].indent_tab + extra;
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
            frm.pse[frm.pse_tos].in_preproc = false;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }

         /* Transition into a preproc by creating a dummy indent */
         frm.level++;
         indent_pse_push(frm, chunk_get_next(pc));

         if (is_ptype(pc, CT_PP_DEFINE, CT_PP_UNDEF))
         {
            frm.pse[frm.pse_tos].indent_tmp = is_true(UO_pp_define_at_level) ?
                                              frm.pse[frm.pse_tos-1].indent_tmp : 1;
            frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos  ].indent_tmp + indent_size;
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos  ].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }
         else if (is_ptype(pc, CT_PP_PRAGMA) && is_true(UO_pp_define_at_level))
         {
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos-1].indent_tmp;
            frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos  ].indent_tmp + indent_size;
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos  ].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }
         else
         {
            if ( (frm.pse[frm.pse_tos - 1].type == CT_PP_REGION_INDENT) ||
                ((frm.pse[frm.pse_tos - 1].type == CT_PP_IF_INDENT    ) &&
                 (frm.pse[frm.pse_tos].type != CT_PP_ENDIF)))
            {
               frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 2].indent;
            }
            else
            {
               frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 1].indent;
            }
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent);
            if (is_ptype(pc, CT_PP_REGION, CT_PP_ENDREGION))
            {
               int32_t val = cpd.settings[UO_pp_indent_region].n;
               if (val > 0)
               {
                  frm.pse[frm.pse_tos].indent = (uint32_t)val;
               }
               else
               {
                  frm.pse[frm.pse_tos].indent = (uint32_t)((int32_t)frm.pse[frm.pse_tos].indent + val);
               }
            }
            else if (is_ptype(pc, CT_PP_IF, CT_PP_ELSE, CT_PP_ENDIF))
            {
               int32_t val = cpd.settings[UO_pp_indent_if].n;
               if (val > 0)
               {
                  frm.pse[frm.pse_tos].indent = (uint32_t)val;
               }
               else
               {
                  frm.pse[frm.pse_tos].indent = (uint32_t)((int32_t)frm.pse[frm.pse_tos].indent + val);
               }
            }
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }
      }

      /* Check for close XML tags "</..." */
      if (cpd.settings[UO_indent_xml_string].u > 0)
      {
         if (is_type(pc, CT_STRING))
         {
            if ((pc->len()   >  4 ) &&
                (xml_indent  >  0 ) &&
                (pc->str[1] == '<') &&
                (pc->str[2] == '/') )
            {
               xml_indent -= cpd.settings[UO_indent_xml_string].n;
            }
         }
         else
         {
            if (!is_cmt(pc) && !is_nl(pc))
            {
               xml_indent = 0;
            }
         }
      }

      /* Handle non-brace closures */
      LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
              __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

      token_used = false;
      uint32_t old_pse_tos;
      do
      {
         old_pse_tos = frm.pse_tos;

         /* End anything that drops a level */
         if (!is_nl(pc) &&
             !is_cmt(pc) &&
             (frm.pse[frm.pse_tos].level > pc->level))
         {
            indent_pse_pop(frm, pc);
         }

         if (frm.pse[frm.pse_tos].level >= pc->level)
         {
            /* process virtual braces closes (no text output) */
            if (is_type(pc, CT_VBRACE_CLOSE) &&
                (frm.pse[frm.pse_tos].type == CT_VBRACE_OPEN))
            {
               indent_pse_pop(frm, pc);
               frm.level--;
               pc = chunk_get_next(pc);
               if (!pc)
               {
                  /* need to break out of both the do and while loops */
                  goto null_pc;
               }
            }

            /* End any assign operations with a semicolon on the same level */
            if ( is_type(frm.pse[frm.pse_tos].type, CT_ASSIGN_NL, CT_ASSIGN) &&
                (is_semicolon(pc)                                      ||
                 is_type(pc, CT_COMMA, CT_BRACE_OPEN, CT_SPAREN_CLOSE) ||
                 is_type_and_ptype(pc, CT_SQUARE_OPEN, CT_OC_AT      ) ||
                 is_type_and_ptype(pc, CT_SQUARE_OPEN, CT_ASSIGN     ) ) &&
                 not_ptype(pc, CT_CPP_LAMBDA)                            )
            {
               indent_pse_pop(frm, pc);
            }

            /* End any assign operations with a semicolon on the same level */
            if (is_semicolon(pc) &&
                is_type(frm.pse[frm.pse_tos].type, CT_IMPORT, CT_USING))
            {
               indent_pse_pop(frm, pc);
            }

            /* End any custom macro-based open/closes */
            if (!token_used &&
                is_type(frm.pse[frm.pse_tos].type, CT_MACRO_OPEN) &&
                is_type(pc, CT_MACRO_CLOSE))
            {
               token_used = true;
               indent_pse_pop(frm, pc);
            }

            /* End any CPP/ObjC class colon stuff */
            if ( is_type(frm.pse[frm.pse_tos].type, CT_CLASS_COLON, CT_CONSTR_COLON) &&
                (is_type(pc, CT_BRACE_OPEN, CT_OC_END, CT_OC_SCOPE, CT_OC_PROPERTY) || is_semicolon(pc)))
            {
               indent_pse_pop(frm, pc);
            }
            /* End ObjC class colon stuff inside of generic definition (like Test<T1: id<T3>>) */
            if (is_type(frm.pse[frm.pse_tos].type, CT_CLASS_COLON) &&
                is_type_and_ptype(pc, CT_ANGLE_CLOSE, CT_OC_GENERIC_SPEC))
            {
               indent_pse_pop(frm, pc);
            }

            /* a case is ended with another case or a close brace */
            if (is_type(frm.pse[frm.pse_tos].type, CT_CASE) &&
                is_type(pc, CT_BRACE_CLOSE, CT_CASE))
            {
               indent_pse_pop(frm, pc);
            }

            /* a class scope is ended with another class scope or a close brace */
            if (is_true(UO_indent_access_spec_body) &&
                is_type(frm.pse[frm.pse_tos].type, CT_PRIVATE) &&
                is_type(pc, CT_BRACE_CLOSE, CT_PRIVATE))
            {
               indent_pse_pop(frm, pc);
            }

            /* return & throw are ended with a semicolon */
            if (is_semicolon(pc) &&
                is_type(frm.pse[frm.pse_tos].type, CT_RETURN, CT_THROW))
            {
               indent_pse_pop(frm, pc);
            }

            /* an OC SCOPE ('-' or '+') ends with a semicolon or brace open */
            if (is_type(frm.pse[frm.pse_tos].type, CT_OC_SCOPE) &&
                (is_semicolon(pc) || is_type(pc, CT_BRACE_OPEN)))
            {
               indent_pse_pop(frm, pc);
            }

            /* a typedef and an OC SCOPE ('-' or '+') ends with a semicolon or
             * brace open */
            if (is_type(frm.pse[frm.pse_tos].type, CT_TYPEDEF) &&
                (is_semicolon (pc) || is_paren_open(pc) || is_type(pc, CT_BRACE_OPEN)))
            {
               indent_pse_pop(frm, pc);
            }

            /* an SQL EXEC is ended with a semicolon */
            if (is_type(frm.pse[frm.pse_tos].type, CT_SQL_EXEC) &&
                  is_semicolon(pc))
            {
               indent_pse_pop(frm, pc);
            }

            /* an CLASS is ended with a semicolon or brace open */
            if ( is_type(frm.pse[frm.pse_tos].type, CT_CLASS) &&
                (is_type(pc, CT_CLASS_COLON, CT_BRACE_OPEN) || is_semicolon(pc)))
            {
               indent_pse_pop(frm, pc);
            }

            /* Close out parenthesis and squares */
            if (is_type(frm.pse[frm.pse_tos].type, get_inverse_type(pc->type) ) &&
                (is_type(pc, 5, CT_PAREN_CLOSE,  CT_SPAREN_CLOSE, CT_FPAREN_CLOSE,
                                CT_SQUARE_CLOSE, CT_ANGLE_CLOSE)))
            {
               indent_pse_pop(frm, pc);
               frm.paren_count--;
            }
         }
      } while (old_pse_tos > frm.pse_tos);

      /* Grab a copy of the current indent */
      indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
      LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
              __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

      if (!is_nl(pc) && !is_cmt(pc) && log_sev_on(LINDPC))
      {
         LOG_FMT(LINDPC, " -=[ %u:%u %s ]=-\n",
                 pc->orig_line, pc->orig_col, pc->text());
         for (uint32_t ttidx = frm.pse_tos; ttidx > 0; ttidx--)
         {
            LOG_FMT(LINDPC, "     [%u %u:%u %s %s/%s tmp=%u ind=%u bri=%d tab=%u cont=%d lvl=%u blvl=%u]\n",
                    ttidx, frm.pse[ttidx].pc->orig_line, frm.pse[ttidx].pc->orig_col,
                    frm.pse[ttidx].pc->text(), get_token_name(frm.pse[ttidx].type),
                    get_token_name(frm.pse[ttidx].pc->ptype), frm.pse[ttidx].indent_tmp,
                    frm.pse[ttidx].indent, frm.pse[ttidx].brace_indent,
                    frm.pse[ttidx].indent_tab, (int32_t)frm.pse[ttidx].indent_cont,
                    frm.pse[ttidx].level, frm.pse[ttidx].pc->brace_level);
         }
      }

      /* Handle stuff that can affect the current indent:
       *  - brace close
       *  - vbrace open
       *  - brace open
       *  - case         (immediate)
       *  - labels       (immediate)
       *  - class colons (immediate)
       *
       * And some stuff that can't
       *  - open paren
       *  - open square
       *  - assignment
       *  - return */
      bool brace_indent = false;
      if (is_type(pc, CT_BRACE_CLOSE, CT_BRACE_OPEN ) )
      {
         brace_indent = (( is_true(UO_indent_braces          )                                    ) &&
                         (is_false(UO_indent_braces_no_func  ) || not_ptype(pc, CT_FUNC_DEF      )) &&
                         (is_false(UO_indent_braces_no_func  ) || not_ptype(pc, CT_FUNC_CLASS_DEF)) &&
                         (is_false(UO_indent_braces_no_class ) || not_ptype(pc, CT_CLASS         )) &&
                         (is_false(UO_indent_braces_no_struct) || not_ptype(pc, CT_STRUCT        )) );
      }

      if (is_type(pc, CT_BRACE_CLOSE))
      {
         if (frm.pse[frm.pse_tos].type == CT_BRACE_OPEN)
         {
            /* Indent the brace to match the open brace */
            indent_column_set(frm.pse[frm.pse_tos].brace_indent);

            if (frm.pse[frm.pse_tos].ip.ref)
            {
               pc->indent.ref   = frm.pse[frm.pse_tos].ip.ref;
               pc->indent.delta = 0;
            }

            indent_pse_pop(frm, pc);
            frm.level--;
         }
      }
      else if (is_type(pc, CT_VBRACE_OPEN))
      {
         frm.level++;
         indent_pse_push(frm, pc);

         uint32_t iMinIndent = cpd.settings[UO_indent_min_vbrace_open].u;
         if (indent_size > iMinIndent)
         {
            iMinIndent = indent_size;
         }
         uint32_t iNewIndent = frm.pse[frm.pse_tos - 1].indent + iMinIndent;
         if (is_true(UO_indent_vbrace_open_on_tabstop))
         {
            iNewIndent = next_tab_column(iNewIndent);
         }
         frm.pse[frm.pse_tos].indent     = iNewIndent;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
         frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

         /* Always indent on virtual braces */
         indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
      }
      else if (is_type    (pc,       CT_BRACE_OPEN) &&
               not_type(pc->next, CT_NAMESPACE ) )
      {
         frm.level++;
         indent_pse_push(frm, pc);

         if (is_true(UO_indent_cpp_lambda_body) &&
             is_ptype(pc, CT_CPP_LAMBDA))
         {
            // DRY6
            frm.pse[frm.pse_tos  ].brace_indent = (int32_t)frm.pse[frm.pse_tos-1].indent;
            indent_column                       = frm.pse[frm.pse_tos  ].brace_indent;
            frm.pse[frm.pse_tos  ].indent       = indent_column + indent_size;
            frm.pse[frm.pse_tos  ].indent_tab   = frm.pse[frm.pse_tos  ].indent;
            frm.pse[frm.pse_tos  ].indent_tmp   = frm.pse[frm.pse_tos  ].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            frm.pse[frm.pse_tos-1].indent_tmp   = frm.pse[frm.pse_tos  ].indent_tmp;
         }
         else if (is_lang(cpd, LANG_CS) &&
                   is_true(UO_indent_cs_delegate_brace) &&
                   is_ptype(pc, CT_LAMBDA, CT_DELEGATE))
         {
            // DRY6
            frm.pse[frm.pse_tos  ].brace_indent = (int32_t)(1 + ((pc->brace_level+1) * indent_size));
            indent_column                       = frm.pse[frm.pse_tos].brace_indent;
            frm.pse[frm.pse_tos  ].indent       = indent_column + indent_size;
            frm.pse[frm.pse_tos  ].indent_tab   = frm.pse[frm.pse_tos].indent;
            frm.pse[frm.pse_tos  ].indent_tmp   = frm.pse[frm.pse_tos].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            frm.pse[frm.pse_tos-1].indent_tmp   = frm.pse[frm.pse_tos].indent_tmp;
         }
         /* any '{' that is inside of a '(' overrides the '(' indent */
         else if (is_false(UO_indent_paren_open_brace) &&
                  is_paren_open(frm.pse[frm.pse_tos-1].pc) &&
                  is_nl(get_next_nc(pc)))
         {
            /* FIXME: I don't know how much of this is necessary, but it seems to work */
            // DRY6
            frm.pse[frm.pse_tos  ].brace_indent = (int32_t)(1 + (pc->brace_level * indent_size));
            indent_column                       = frm.pse[frm.pse_tos].brace_indent;
            frm.pse[frm.pse_tos  ].indent       = indent_column + indent_size;
            frm.pse[frm.pse_tos  ].indent_tab   = frm.pse[frm.pse_tos].indent;
            frm.pse[frm.pse_tos  ].indent_tmp   = frm.pse[frm.pse_tos].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            frm.pse[frm.pse_tos-1].indent_tmp   = frm.pse[frm.pse_tos].indent_tmp;
         }
         else if (frm.paren_count != 0)
         {
            if (is_ptype(frm.pse[frm.pse_tos].pc, CT_OC_BLOCK_EXPR))
            {
               if (is_flag(pc, PCF_IN_OC_MSG) &&
                   cpd.settings[UO_indent_oc_block_msg].u)
               {
                  frm.pse[frm.pse_tos].ip.ref   = oc_msg_block_indent(pc, false, false, false, true);
                  frm.pse[frm.pse_tos].ip.delta = cpd.settings[UO_indent_oc_block_msg].n;
               }

               if (is_true(UO_indent_oc_block                ) ||
                   is_true(UO_indent_oc_block_msg_xcode_style) )
               {
                  bool in_oc_msg           = (is_flag(pc, PCF_IN_OC_MSG));
                  bool indent_from_keyword = is_true(UO_indent_oc_block_msg_from_keyword) && in_oc_msg;
                  bool indent_from_colon   = is_true(UO_indent_oc_block_msg_from_colon  ) && in_oc_msg;
                  bool indent_from_caret   = is_true(UO_indent_oc_block_msg_from_caret  ) && in_oc_msg;
                  bool indent_from_brace   = is_true(UO_indent_oc_block_msg_from_brace  ) && in_oc_msg;

                  // In "Xcode indent mode", we want to indent:
                  //  - if the colon is aligned (namely, if a newline has been
                  //    added before it), indent_from_brace
                  //  - otherwise, indent from previous block (the "else" statement here)
                  if (is_true(UO_indent_oc_block_msg_xcode_style))
                  {
                     chunk_t* colon        = oc_msg_prev_colon(pc);
                     chunk_t* param_name   = chunk_get_prev(colon);
                     const chunk_t* before_param = chunk_get_prev(param_name);
                     const bool is_newline = is_type(before_param, CT_NEWLINE);
                     indent_from_keyword   = (is_newline) ? true  : false;
                     indent_from_brace     = false; // (is_newline) ? false : true;
                     indent_from_colon     = false;
                     indent_from_caret     = false;

                  }

                  const chunk_t* ref = oc_msg_block_indent(pc, indent_from_brace,
                        indent_from_caret, indent_from_colon, indent_from_keyword);
                  if (ref)
                  {
                     frm.pse[frm.pse_tos].indent = indent_size + ref->column;
                     indent_column_set(frm.pse[frm.pse_tos].indent - indent_size);
                  }
                  else
                  {
                     frm.pse[frm.pse_tos].indent = 1 + ((pc->brace_level+1) * indent_size);
                     indent_column_set(frm.pse[frm.pse_tos].indent - indent_size);
                  }
               }
               else
               {
                  frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos-1].indent_tmp + indent_size;
               }
            }
            else
            {
               /* We are inside ({ ... }) -- indent one tab from the paren */
               frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos-1].indent_tmp + indent_size;
            }
         }
         else
         {
            /* Use the prev indent level + indent_size. */
            frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos-1].indent + indent_size;

            /* If this brace is part of a statement, bump it out by indent_brace */
            if (is_ptype(pc, 11, CT_IF, CT_ELSEIF, CT_ELSE, CT_USING_STMT, CT_FOR,
                 CT_TRY, CT_CATCH, CT_DO, CT_WHILE, CT_SWITCH, CT_SYNCHRONIZED))
            {
               if (parent_token_indent != 0)
               {
                  frm.pse[frm.pse_tos].indent += parent_token_indent - indent_size;
               }
               else
               {
                  frm.pse[frm.pse_tos].indent     += cpd.settings[UO_indent_brace].u;
                  indent_column_set(indent_column +  cpd.settings[UO_indent_brace].u);
               }
            }
            else if (is_ptype(pc, CT_CASE))
            {
               /* An open brace with the parent of case does not indent by default
                * UO_indent_case_brace can be used to indent the brace.
                * So we need to take the CASE indent, subtract off the
                * indent_size that was added above and then add indent_case_brace.
                * may take negative value */
               indent_column_set(frm.pse[frm.pse_tos-1].indent - indent_size +
                                 cpd.settings[UO_indent_case_brace].u);

               /* Stuff inside the brace still needs to be indented */
               frm.pse[frm.pse_tos].indent     = indent_column + indent_size;
               frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
               LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                       __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            }
            else if (is_ptype(pc, CT_CLASS) && is_false(UO_indent_class))
            {
               frm.pse[frm.pse_tos].indent -= indent_size;
            }
            else if (is_ptype(pc, CT_NAMESPACE))
            {
               if (is_true(UO_indent_namespace              ) &&
                   is_true(UO_indent_namespace_single_indent) )
               {
                  if (frm.pse[frm.pse_tos].ns_cnt)
                  {
                     /* undo indent on all except the first namespace */
                     frm.pse[frm.pse_tos].indent -= indent_size;
                  }
                  indent_column_set((frm.pse_tos <= 1) ? 1 : frm.pse[frm.pse_tos-1].brace_indent);
               }
               else if (is_flag(pc, PCF_LONG_BLOCK) || is_false(UO_indent_namespace))
               {
                  /* don't indent long blocks */
                  frm.pse[frm.pse_tos].indent -= indent_size;
               }
               else /* indenting 'short' namespace */
               {
                  if (cpd.settings[UO_indent_namespace_level].u > 0)
                  {
                     frm.pse[frm.pse_tos].indent -= indent_size;
                     frm.pse[frm.pse_tos].indent +=
                        cpd.settings[UO_indent_namespace_level].u;
                  }
               }
               frm.pse[frm.pse_tos].ns_cnt++;
            }
            else if (is_ptype(pc, CT_EXTERN) && is_false(UO_indent_extern))
            {
               frm.pse[frm.pse_tos].indent -= indent_size;
            }

            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
         }

         if (is_flag(pc, PCF_DONT_INDENT))
         {
            frm.pse[frm.pse_tos].indent = pc->column;
            indent_column_set(pc->column);
         }
         else
         {
            /* If there isn't a newline between the open brace and the next
             * item, just indent to wherever the next token is.
             * This covers this sort of stuff:
             * { a++;
             *   b--; }; */
            next = get_next_ncnl(pc);
            break_if (is_invalid(next));

            if (!is_newline_between(pc, next))
            {
               if (is_true(UO_indent_token_after_brace))
               {
                  frm.pse[frm.pse_tos].indent = next->column;
               }
            }
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
            frm.pse[frm.pse_tos].open_line  = pc->orig_line;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

            /* Update the indent_column if needed */
            if (brace_indent || (parent_token_indent != 0))
            {
               indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
               LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                       __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            }
         }

         /* Save the brace indent */
         frm.pse[frm.pse_tos].brace_indent = (int32_t)indent_column;
      }
      else if (is_type(pc, CT_SQL_END))
      {
         if (frm.pse[frm.pse_tos].type == CT_SQL_BEGIN)
         {
            indent_pse_pop(frm, pc);
            frm.level--;
            indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }
      }
      else if(is_type(pc, CT_SQL_BEGIN, CT_MACRO_OPEN))
      {
         frm.level++;
         indent_pse_push(frm, pc);
         frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent + indent_size;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
         frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos  ].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
      }
      else if (is_type(pc, CT_SQL_EXEC))
      {
         frm.level++;
         indent_pse_push(frm, pc);
         frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent + indent_size;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
      }
      else if (is_type(pc, CT_MACRO_ELSE))
      {
         if (frm.pse[frm.pse_tos].type == CT_MACRO_OPEN)
         {
            indent_column_set(frm.pse[frm.pse_tos - 1].indent);
         }
      }
      else if (is_type(pc, CT_CASE))
      {
         /* Start a case - indent UO_indent_switch_case from the switch level */
         uint32_t tmp = frm.pse[frm.pse_tos].indent + cpd.settings[UO_indent_switch_case].u;

         indent_pse_push(frm, pc);

         frm.pse[frm.pse_tos].indent     = tmp;
         frm.pse[frm.pse_tos].indent_tmp = tmp - indent_size + cpd.settings[UO_indent_case_shift].u;
         frm.pse[frm.pse_tos].indent_tab = tmp;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

         /* Always set on case statements */
         indent_column_set(frm.pse[frm.pse_tos].indent_tmp);

         /* comments before 'case' need to be aligned with the 'case' */
         chunk_t* pct = pc;
         while (((pct = get_prev_nnl(pct)) != nullptr) &&
                is_cmt(pct))
         {
            chunk_t* t2 = chunk_get_prev(pct);
            if (is_nl(t2))
            {
               pct->column        = frm.pse[frm.pse_tos].indent_tmp;
               pct->column_indent = pct->column;
            }
         }
      }
      else if (is_type(pc, CT_BREAK))
      {
         prev = get_prev_ncnl(pc);
         if (is_type_and_ptype(prev, CT_BRACE_CLOSE, CT_CASE))
         {
            const chunk_t* temp = get_prev_type(pc, CT_BRACE_OPEN, (int32_t)pc->level);
            assert(is_valid(temp));
            /* This only affects the 'break', so no need for a stack entry */
            indent_column_set(temp->column);
         }
      }
      else if (is_type(pc, CT_LABEL))
      {
         /* Labels get sent to the left or backed up */
         if (cpd.settings[UO_indent_label].n > 0)
         {
            indent_column_set(cpd.settings[UO_indent_label].u);

            next = chunk_get_next(pc);   /* colon */
            if (is_valid(next))
            {
               next = chunk_get_next(next); /* possible statement */

               if ( is_valid  (next) &&
                   !is_nl(next) &&
                   /* label (+ 2, because there is colon and space after it) must fit into indent */
                   (cpd.settings[UO_indent_label].n + static_cast<int32_t>(pc->len()) + 2 <= static_cast<int32_t>(frm.pse[frm.pse_tos].indent)))
               {
                  reindent_line(next, frm.pse[frm.pse_tos].indent);
               }
            }
         }
         else
         {
            indent_column_set(frm.pse[frm.pse_tos].indent +
                              cpd.settings[UO_indent_label].u);
         }
      }
      else if (is_type(pc, CT_PRIVATE))
      {
         if (is_true(UO_indent_access_spec_body))
         {
            uint32_t tmp = frm.pse[frm.pse_tos].indent + indent_size;

            indent_pse_push(frm, pc);

            frm.pse[frm.pse_tos].indent     = tmp;
            frm.pse[frm.pse_tos].indent_tmp = tmp - indent_size;
            frm.pse[frm.pse_tos].indent_tab = tmp;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

            /* If we are indenting the body, then we must leave the access spec
             * indented at brace level */
            indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
         }
         else
         {
            /* Access spec labels get sent to the left or backed up */
            if (cpd.settings[UO_indent_access_spec].n > 0)
            {
               indent_column_set(cpd.settings[UO_indent_access_spec].u);
            }
            else
            {
               indent_column_set(frm.pse[frm.pse_tos].indent +
                                 cpd.settings[UO_indent_access_spec].u);
            }
         }
      }
      else if (is_type(pc, CT_CLASS))
      {
         frm.level++;
         indent_pse_push(frm, pc);
         frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent + indent_size;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
         frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos  ].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
      }
      else if (is_type(pc, CT_CLASS_COLON, CT_CONSTR_COLON))
      {
         /* just indent one level */
         indent_pse_push(frm, pc);
         frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent_tmp + indent_size;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
         frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos  ].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

         indent_column_set(frm.pse[frm.pse_tos].indent_tmp);

         if ((is_true(UO_indent_class_colon ) && is_type(pc, CT_CLASS_COLON )) ||
             (is_true(UO_indent_constr_colon) && is_type(pc, CT_CONSTR_COLON)) )
         {
            prev = chunk_get_prev(pc);
            if (is_nl(prev))
            {
               frm.pse[frm.pse_tos].indent += cpd.settings[UO_indent_ctor_init_leading].u;

               if (cpd.settings[UO_indent_ctor_init].u != 0)
               {
                  frm.pse[frm.pse_tos].indent     += cpd.settings[UO_indent_ctor_init].u;
                  frm.pse[frm.pse_tos].indent_tmp += cpd.settings[UO_indent_ctor_init].u;
                  frm.pse[frm.pse_tos].indent_tab += cpd.settings[UO_indent_ctor_init].u;
                  LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                          __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
                  indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
               }
            }
            else
            {
               if (is_true(UO_indent_class_on_colon) && is_type(pc, CT_CLASS_COLON))
               {
                  frm.pse[frm.pse_tos].indent = pc->column;
               }
               else
               {
                  next = chunk_get_next(pc);
                  if (is_valid(next))
                  {
                     if (cpd.settings[UO_indent_ctor_init].u != 0)
                     {
                        frm.pse[frm.pse_tos].indent     += cpd.settings[UO_indent_ctor_init].u;
                        frm.pse[frm.pse_tos].indent_tmp += cpd.settings[UO_indent_ctor_init].u;
                        frm.pse[frm.pse_tos].indent_tab += cpd.settings[UO_indent_ctor_init].u;
                        LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                                __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
                        indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
                     }
                     else if (!is_nl(next))
                     {
                        frm.pse[frm.pse_tos].indent = next->column;
                     }
                  }
               }
            }
         }
      }
      else if (is_type(pc, 5, CT_PAREN_OPEN,  CT_SPAREN_OPEN,
                    CT_FPAREN_OPEN, CT_SQUARE_OPEN, CT_ANGLE_OPEN ))
      {
         /* Open parenthesis and squares - never update indent_column, unless right
          * after a newline. */
         bool skipped = false;

         indent_pse_push(frm, pc);
         if (is_nl(chunk_get_prev(pc)) &&
             (pc->column != indent_column))
         {
            LOG_FMT(LINDENT, "%s[line %d]: %u] indent => %u [%s]\n",
                    __func__, __LINE__, pc->orig_line, indent_column, pc->text());
            reindent_line(pc, indent_column);
         }
         frm.pse[frm.pse_tos].indent = pc->column + pc->len();

         if (is_type(pc, CT_SQUARE_OPEN) && is_lang(cpd, LANG_D))
         {
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
         }

         if ((is_type(pc, CT_FPAREN_OPEN, CT_ANGLE_OPEN)) &&
             ((is_true(UO_indent_func_call_param    ) && (is_ptype(pc, CT_FUNC_CALL,      CT_FUNC_CALL_USER  ))) ||
              (is_true(UO_indent_func_proto_param   ) && (is_ptype(pc, CT_FUNC_PROTO,     CT_FUNC_CLASS_PROTO))) ||
              (is_true(UO_indent_func_class_param   ) && (is_ptype(pc, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO))) ||
              (is_true(UO_indent_template_param     ) && (is_ptype(pc, CT_TEMPLATE                           ))) ||
              (is_true(UO_indent_func_ctor_var_param) && (is_ptype(pc, CT_FUNC_CTOR_VAR                      ))) ||
              (is_true(UO_indent_func_def_param     ) && (is_ptype(pc, CT_FUNC_DEF)                          ))) )
         {
            /* Skip any continuation indents */
            idx = (int32_t)frm.pse_tos - 1;
            assert(idx >= 0);
            while ((idx > 0) &&
                   (frm.pse[idx].type != CT_BRACE_OPEN  ) &&
                   (frm.pse[idx].type != CT_VBRACE_OPEN ) &&
                   (frm.pse[idx].type != CT_PAREN_OPEN  ) &&
                   (frm.pse[idx].type != CT_FPAREN_OPEN ) &&
                   (frm.pse[idx].type != CT_SPAREN_OPEN ) &&
                   (frm.pse[idx].type != CT_SQUARE_OPEN ) &&
                   (frm.pse[idx].type != CT_ANGLE_OPEN  ) &&
                   (frm.pse[idx].type != CT_CLASS_COLON ) &&
                   (frm.pse[idx].type != CT_CONSTR_COLON) &&
                   (frm.pse[idx].type != CT_ASSIGN_NL   ) )
            {
               idx--;
               skipped = true;
            }
            // PR#381
            if (cpd.settings[UO_indent_param].u != 0)
            {
               frm.pse[frm.pse_tos].indent = frm.pse[idx].indent + cpd.settings[UO_indent_param].u;
            }
            else
            {
               frm.pse[frm.pse_tos].indent = frm.pse[idx].indent + indent_size;
            }
            if (is_true(UO_indent_func_param_double))
            {
               frm.pse[frm.pse_tos].indent += indent_size;
            }
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
         }

         else if ((is_str(pc, "(") && is_false(UO_indent_paren_nl )) ||
                  (is_str(pc, "<") && is_false(UO_indent_paren_nl )) || /* TODO: add indent_angle_nl? */
                  (is_str(pc, "[") && is_false(UO_indent_square_nl)) )
         {
            next = get_next_nc(pc);
            break_if(is_invalid(next));

            if (is_nl(next))
            {
               uint32_t sub = 1;
               if ((frm.pse[frm.pse_tos - 1].type == CT_ASSIGN) ||
                   (frm.pse[frm.pse_tos - 1].type == CT_RETURN))
               {
                  sub = 2;
               }
               frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos - sub].indent + indent_size;
               frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
               skipped                         = true;
            }
            else
            {
               if ( is_valid(next) &&
                   !is_cmt(next) )
               {
                  if (is_type(next, CT_SPACE))
                  {
                     next = get_next_nc(next);
                     break_if (is_invalid(next));
                  }
                  frm.pse[frm.pse_tos].indent = next->column;
               }
            }
         }

         if ( is_type(pc, CT_FPAREN_OPEN)    &&
              is_nl(chunk_get_prev(pc)) &&
             !is_nl(chunk_get_next(pc)) )
         {
            frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 1].indent + indent_size;
            indent_column_set(frm.pse[frm.pse_tos].indent);
         }
         if (not_ptype(pc, CT_OC_AT) && (cpd.settings[UO_indent_continue].n != 0) && (!skipped))
         {
            frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 1].indent;
            if ((pc->level == pc->brace_level) &&
                (is_type(pc, CT_FPAREN_OPEN, CT_SPAREN_OPEN)))
            {
               //frm.pse[frm.pse_tos].indent += abs(cpd.settings[UO_indent_continue].n);
               //   frm.pse[frm.pse_tos].indent      = calc_indent_continue(frm, frm.pse_tos);
               //   frm.pse[frm.pse_tos].indent_cont = true;
               if ((is_true(UO_use_indent_continue_only_once)) &&
                   (frm.pse[frm.pse_tos].indent_cont) &&
                   (vardefcol != 0))
               {
                  // The value of the indentation for a continuation line is calculate
                  // differently if the line is:
                  //   a declaration :your case with QString fileName ...
                  //   an assignment  :your case with pSettings = new QSettings( ...
                  // At the second case the option value might be used twice:
                  //   at the assignment
                  //   at the function call (if present)
                  // If you want to prevent the double use of the option value
                  // you may use the new option :
                  //   use_indent_continue_only_once
                  // with the value "true".
                  // use/don't use indent_continue once Guy 2016-05-16

                  // if vardefcol isn't zero, use it
                  frm.pse[frm.pse_tos].indent = vardefcol;
               }
               else
               {
                  frm.pse[frm.pse_tos].indent      = calc_indent_continue(frm, frm.pse_tos);
                  frm.pse[frm.pse_tos].indent_cont = true;
               }
            }
         }
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         frm.paren_count++;
      }
      else if (is_type(pc, CT_ASSIGN, CT_IMPORT, CT_USING ) )
      {
         /* if there is a newline after the '=' or the line starts with a '=',
          * just indent one level,
          * otherwise align on the '='. */
         if (is_type(pc, CT_ASSIGN) &&
             is_nl(chunk_get_prev(pc)))
         {
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent + indent_size;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
            LOG_FMT(LINDENT, "%s(%d): %u] assign => %u [%s]\n",
                    __func__, __LINE__, pc->orig_line, indent_column, pc->text());
            reindent_line(pc, frm.pse[frm.pse_tos].indent_tmp);
         }

         next = chunk_get_next(pc);
         if (is_valid(next))
         {
            indent_pse_push(frm, pc);
            if (cpd.settings[UO_indent_continue].n != 0)
            {
               frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 1].indent;
               if (is_level(pc, pc->brace_level) &&
                   (not_type(pc, CT_ASSIGN) ||
                    not_ptype(pc, 2, CT_FUNC_PROTO, CT_FUNC_DEF)))
               {
                  //frm.pse[frm.pse_tos].indent += abs(cpd.settings[UO_indent_continue].n);
                  //   frm.pse[frm.pse_tos].indent      = calc_indent_continue(frm, frm.pse_tos);
                  //   frm.pse[frm.pse_tos].indent_cont = true;
                  if ((is_true(UO_use_indent_continue_only_once)) &&
                      (frm.pse[frm.pse_tos].indent_cont) &&
                      (vardefcol != 0))
                  {
                     // if vardefcol isn't zero, use it
                     frm.pse[frm.pse_tos].indent = vardefcol;
                  }
                  else
                  {
                     frm.pse[frm.pse_tos].indent      = calc_indent_continue(frm, frm.pse_tos);
                     vardefcol                        = frm.pse[frm.pse_tos].indent; // use the same variable for the next line
                     frm.pse[frm.pse_tos].indent_cont = true;
                  }
               }
            }
            else if (is_nl(next) || is_false(UO_indent_align_assign))
            {
               frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 1].indent_tmp + indent_size;
               if (is_type(pc, CT_ASSIGN))
               {
                  frm.pse[frm.pse_tos].type       = CT_ASSIGN_NL;
                  frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
               }
            }
            else
            {
               frm.pse[frm.pse_tos].indent = pc->column + pc->len() + 1;
            }
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }
      }
      else if (is_type (pc, CT_RETURN, CT_THROW ) &&
               is_ptype(pc, CT_NONE             ) )
      {
         /* don't count returns inside a () or [] */
         if (pc->level == pc->brace_level)
         {
            indent_pse_push(frm, pc);
            if (is_nl(chunk_get_next(pc)))
            {
               frm.pse[frm.pse_tos].indent  = frm.pse[frm.pse_tos-1].indent + indent_size;
            }
            else
            {
               frm.pse[frm.pse_tos].indent  = frm.pse[frm.pse_tos-1].indent + pc->len() + 1;
            }
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos-1].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }
      }
      else if (is_type(pc, CT_OC_SCOPE, CT_TYPEDEF))
      {
         indent_pse_push(frm, pc);
         // Issue # 405
         frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
         LOG_FMT(LINDLINE, "%s(%d): .indent=%u, .indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse[frm.pse_tos].indent, frm.pse[frm.pse_tos].indent_tmp);
         if (cpd.settings[UO_indent_continue].n != 0)
         {
            //frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 1].indent +
            //                              abs(cpd.settings[UO_indent_continue].n);
            frm.pse[frm.pse_tos].indent      = calc_indent_continue(frm, frm.pse_tos - 1);
            frm.pse[frm.pse_tos].indent_cont = true;
         }
         else
         {
            frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 1].indent + indent_size;
         }
      }
      else if (is_type(pc, CT_C99_MEMBER)) { /* nothing to do  */ }
      else                                       { /* anything else? */ }

      /* Handle shift expression continuation indenting */
      shiftcontcol = 0;
      if ( (is_true(UO_indent_shift)    ) &&
            not_flag(pc, PCF_IN_ENUM) &&
            not_ptype(pc, CT_OPERATOR) &&
            not_type (pc, 4, CT_COMMENT_CPP, CT_COMMENT_MULTI,
                             CT_BRACE_OPEN,  CT_COMMENT) &&
           (pc->level > 0 ) &&
           (!chunk_empty(pc) ) )
      {
         bool in_shift    = false;
         bool is_operator = false;

         /* Are we in such an expression? Go both forwards and backwards. */
         chunk_t* tmp = pc;
         do // \todo DRY see below
         {
            if (is_str(tmp, "<<") ||
                is_str(tmp, ">>") )
            {
               in_shift = true;
               tmp = get_prev_ncnl(tmp);
               if (is_type(tmp, CT_OPERATOR)) { is_operator = true; }
               break;
            }
            tmp = get_prev_ncnl(tmp);
         } while ((in_shift == false) &&
               not_type(tmp, 6, CT_BRACE_CLOSE, CT_SPAREN_CLOSE, CT_COMMA,
                  CT_SEMICOLON, CT_BRACE_OPEN,  CT_SPAREN_OPEN));

         tmp = pc;
         do
         {
            tmp = get_next_ncnl(tmp);
            if (is_str(tmp, "<<") ||
                is_str(tmp, ">>") )
            {
               in_shift = true;
               tmp = get_prev_ncnl(tmp);
               if (is_type(tmp, CT_OPERATOR)) { is_operator = true; }
               break;
            }
         } while ((in_shift == false) &&
                  not_type(tmp, 6, CT_BRACE_CLOSE, CT_SPAREN_CLOSE, CT_COMMA,
                     CT_SEMICOLON, CT_BRACE_OPEN,  CT_SPAREN_OPEN));

         chunk_t* prev_nonl = get_prev_ncnl(pc);
         chunk_t* prev2     = get_prev_nc  (pc);

         if (is_type(prev_nonl, 8, CT_SEMICOLON,  CT_VBRACE_CLOSE,
                   CT_BRACE_OPEN,  CT_VSEMICOLON, CT_BRACE_CLOSE,
                   CT_VBRACE_OPEN, CT_CASE_COLON, CT_COMMA) ||
               are_different_pp(prev_nonl, pc) ||
              (is_operator == true) )
         {
            in_shift = false;
         }

         if (is_type(prev2, CT_NEWLINE) && (in_shift == true))
         {
            shiftcontcol                     = calc_indent_continue(frm, frm.pse_tos);
            frm.pse[frm.pse_tos].indent_cont = true;

            /* Work around the doubly increased indent in RETURNs and assignments */
            bool   need_workaround = false;
            uint32_t sub             = 0;
            for (uint32_t i = frm.pse_tos; i != 0; i--)
            {
               if(is_type(frm.pse[i].type, CT_RETURN, CT_ASSIGN))
               {
                  need_workaround = true;
                  sub             = (frm.pse_tos + 1u - i); /*lint !e737 */
                  break;
               }
            }

            if (need_workaround)
            {
               shiftcontcol = calc_indent_continue(frm, frm.pse_tos - (uint32_t)sub);
            }
         }
      }

      /* Handle variable definition continuation indenting */
      if ((vardefcol == 0                         ) &&
          is_type(pc, CT_WORD, CT_FUNC_CTOR_VAR) &&
          not_flag(pc, PCF_IN_FCN_DEF ) &&
          is_flag    (pc, PCF_VAR_1ST_DEF) )
      {
         if (cpd.settings[UO_indent_continue].n != 0)
         {
            //vardefcol = frm.pse[frm.pse_tos].indent +
            //            abs(cpd.settings[UO_indent_continue].n);
            vardefcol                        = calc_indent_continue(frm, frm.pse_tos);
            frm.pse[frm.pse_tos].indent_cont = true;
         }
         else if (is_true(UO_indent_var_def_cont) ||
                  is_nl(chunk_get_prev(pc)))
         {
            vardefcol = frm.pse[frm.pse_tos].indent + indent_size;
         }
         else
         {
            vardefcol = pc->column;
            /* need to skip backward over any '*' */
            chunk_t* tmp = get_prev_nc(pc);
            while (is_type(tmp, CT_PTR_TYPE))
            {
               assert(is_valid(tmp));
               vardefcol = tmp->column;
               tmp       = get_prev_nc(tmp);
            }
         }
      }
      if (is_semicolon(pc) || is_type_and_ptype(pc, CT_BRACE_OPEN, CT_FUNCTION))
      {
         vardefcol = 0;
      }

      /* Indent the line if needed */
      if ((my_did_newline == true ) &&
          (is_nl(pc)      == false) &&
          (pc->len()      != 0    ) )
      {
         pc->column_indent = frm.pse[frm.pse_tos].indent_tab;

         if (frm.pse[frm.pse_tos].ip.ref)
         {
            pc->indent.ref   = frm.pse[frm.pse_tos].ip.ref;
            pc->indent.delta = frm.pse[frm.pse_tos].ip.delta;
         }

         LOG_FMT(LINDENT2, "%s(%d): %u] %u/%u for %s\n",
                 __func__, __LINE__, pc->orig_line, pc->column_indent, indent_column, pc->text());

         /* Check for special continuations.
          * Note that some of these could be done as a stack item like
          * everything else */
         prev = get_prev_ncnl(pc);
         next = get_next_ncnl(pc);

         bool do_vardefcol = false;
         if ((vardefcol  > 0              ) &&
             (pc->level == pc->brace_level) &&
             is_type(prev, CT_COMMA, CT_TYPE, CT_PTR_TYPE, CT_WORD))
         {
            chunk_t* tmp = pc;
            while (is_type(tmp, CT_PTR_TYPE))
            {
               tmp = get_next_ncnl(tmp);
            }
            if(is_type(tmp, CT_WORD, CT_FUNC_CTOR_VAR) &&
               is_flag(tmp, PCF_VAR_DEF              ) )
            {
               do_vardefcol = true;
            }
         }

         if (is_flag(pc, PCF_DONT_INDENT))
         {
            /* no change */
         }
         else if (is_ptype(pc, CT_SQL_EXEC) && is_true(UO_indent_preserve_sql))
         {
            reindent_line(pc, sql_col + (pc->orig_col - sql_orig_col));
            LOG_FMT(LINDENT, "Indent SQL: [%s] to %u (%u/%u)\n",
                                pc->text(), pc->column, sql_col, sql_orig_col);
         }
         else if (not_flag(pc, PCF_STMT_START)           &&
                  (is_type(pc,   CT_MEMBER, CT_DC_MEMBER) ||
                   is_type(prev, CT_MEMBER, CT_DC_MEMBER) ) )
         {
            uint32_t tmp = cpd.settings[UO_indent_member].u + indent_column;

            log_and_reindent(pc, tmp, "member");
         }
         else if (do_vardefcol)
         {
            log_and_reindent(pc, vardefcol, "Vardefcol");
         }
         else if (shiftcontcol > 0)
         {
            log_and_reindent(pc, shiftcontcol, "indent_shift");
         }
         else if (is_type(pc, CT_NAMESPACE   ) &&
                  is_true(UO_indent_namespace) &&
                  is_true(UO_indent_namespace_single_indent) &&
                  frm.pse[frm.pse_tos].ns_cnt)
         {
            log_and_reindent(pc, frm.pse[frm.pse_tos].brace_indent, "Namespace");
         }
         else if (are_types(pc, prev, CT_STRING) &&
                  is_true(UO_indent_align_string))
         {
            uint32_t tmp = (xml_indent != 0) ? (uint32_t)xml_indent : prev->column;
            log_and_reindent(pc, tmp, "String");
         }
         else if (is_cmt(pc))
         {
            log_and_indent_comment(pc, frm.pse[frm.pse_tos].indent_tmp, "comment");
         }
         else if (is_type(pc, CT_PREPROC))
         {
            log_and_reindent(pc, indent_column, "pp-indent");
         }
         else if (is_paren_close(pc) ||
                  is_type(pc, CT_ANGLE_CLOSE))
         {
            /* This is a big hack. We assume that since we hit a paren close,
             * that we just removed a paren open */
            LOG_FMT(LINDLINE, "%s(%d): indent_column is %u\n",
                    __func__, __LINE__, indent_column);
            if (frm.pse[frm.pse_tos + 1].type == get_inverse_type(pc->type) )
            {
               // Issue # 405
               LOG_FMT(LINDLINE, "%s(%d): [%u:%u] [%s:%s]\n",
                       __func__, __LINE__, pc->orig_line, pc->orig_col, pc->text(), get_token_name(pc->type));
               chunk_t* ck1 = frm.pse[frm.pse_tos + 1].pc;
               chunk_t* ck2 = chunk_get_prev(ck1);

               assert(is_valid(ck2));
               /* If the open paren was the first thing on the line or we are
                * doing mode 1, then put the close paren in the same column */
               if (is_nl(ck2) ||
                   (cpd.settings[UO_indent_paren_close].u == 1))
               {
                  LOG_FMT(LINDLINE, "%s(%d): [%u:%u] indent_paren_close is 1\n",
                          __func__, __LINE__, ck2->orig_line, ck2->orig_col);
                  indent_column_set(ck1->column);
                  LOG_FMT(LINDLINE, "%s(%d): [%u:%u] indent_column set to %u\n",
                          __func__, __LINE__, ck2->orig_line, ck2->orig_col, indent_column);
               }
               else
               {
                  if (cpd.settings[UO_indent_paren_close].u != 2)
                  {
                     // 0 or 1
                     LOG_FMT(LINDLINE, "%s(%d): [%u:%u] indent_paren_close is 0 or 1\n",
                             __func__, __LINE__, ck2->orig_line, ck2->orig_col);
                     indent_column_set(frm.pse[frm.pse_tos + 1].indent_tmp);

                     LOG_FMT(LINDLINE, "%s(%d): [%u:%u] indent_column set to %u\n",
                             __func__, __LINE__, ck2->orig_line, ck2->orig_col, indent_column);
                     pc->column_indent = frm.pse[frm.pse_tos + 1].indent_tab;
                     if (cpd.settings[UO_indent_paren_close].u == 1)
                     {
                        LOG_FMT(LINDLINE, "%s(%d): [%u:%u] indent_paren_close is 1\n",
                                __func__, __LINE__, ck2->orig_line, ck2->orig_col);
                        indent_column--;
                        LOG_FMT(LINDLINE, "%s(%d): [%u:%u] indent_column set to %u\n",
                                __func__, __LINE__, ck2->orig_line, ck2->orig_col, indent_column);
                     }
                  }
                  else
                  {
                     // 2
                     LOG_FMT(LINDLINE, "%s(%d): [%u:%u] indent_paren_close is 2\n",
                             __func__, __LINE__, ck2->orig_line, ck2->orig_col);
                  }
               }
            }

            log_and_reindent(pc, indent_column, "cl_paren");
         }
         else if (is_type(pc, CT_COMMA))
         {
            if (is_true(UO_indent_comma_paren) &&
                is_paren_open(frm.pse[frm.pse_tos].pc))
            {
               indent_column_set(frm.pse[frm.pse_tos].pc->column);
            }
            log_and_reindent(pc, indent_column, "comma");
         }
         else if ( cpd.settings[UO_indent_func_const].u              &&
                  is_type(pc, CT_QUALIFIER)                    &&
                  (strncasecmp(pc->text(), "const", pc->len()) == 0) &&
                  (is_invalid(next) ||
                   is_type(next, 6, CT_BRACED, CT_BRACE_OPEN, CT_THROW,
                        CT_NEWLINE, CT_SEMICOLON, CT_VBRACE_OPEN))   )
         {
            // indent const - void GetFoo(void)\n const\n { return (m_Foo); }
            indent_column_set(cpd.settings[UO_indent_func_const].u);

            log_and_reindent(pc, indent_column, "const");
         }
         else if (cpd.settings[UO_indent_func_throw].u &&
                  is_type_and_not_ptype(pc, CT_THROW, CT_NONE))
         {
            // indent throw - void GetFoo(void)\n throw()\n { return (m_Foo); }
            indent_column_set(cpd.settings[UO_indent_func_throw].u);

            log_and_reindent(pc, indent_column, "throw");
         }
         else if (is_type(pc, CT_BOOL))
         {
            if (is_true(UO_indent_bool_paren) &&
                is_paren_open(frm.pse[frm.pse_tos].pc))
            {
               indent_column_set(frm.pse[frm.pse_tos].pc->column);
               if (is_true(UO_indent_first_bool_expr))
               {
                  reindent_line(chunk_get_next(frm.pse[frm.pse_tos].pc),
                                indent_column + pc->len() + 1);
               }
            }

            log_and_reindent(pc, indent_column, "bool");
         }
         else if ((cpd.settings[UO_indent_ternary_operator].u == 1) &&
                  ((is_type(pc, 6, CT_NUMBER, CT_ADDR, CT_WORD, CT_DEREF,
                                   CT_STRING, CT_PAREN_OPEN )) &&
                    is_type(prev,  CT_COND_COLON) ) )
         {
            chunk_t* tmp = get_prev_type(prev, CT_QUESTION, -1);
            tmp = get_next_ncnl(tmp);
            assert(is_valid(tmp));
            log_and_reindent(pc, tmp->column, "ternarydefcol");
         }
         else if ((cpd.settings[UO_indent_ternary_operator].u == 2) &&
                  is_type(pc, CT_COND_COLON))
         {
            const chunk_t* tmp = get_prev_type(pc, CT_QUESTION, -1);
            assert(is_valid(tmp));
            log_and_reindent(pc, tmp->column, "ternarydefcol");
         }
         else
         {
            bool   use_ident = true;
            uint32_t ttidx     = frm.pse_tos;
            if (ttidx > 0)
            {
               //if (strcasecmp(get_token_name(frm.pse[ttidx].pc->parent_type), "FUNC_CALL") == 0)
               if (is_ptype(frm.pse[ttidx].pc, CT_FUNC_CALL))
               {
                  LOG_FMT(LINDPC, "FUNC_CALL OK [%d]\n", __LINE__);

                  use_ident = is_true(UO_use_indent_func_call_param);
                  LOG_FMT(LINDPC, "use is %s [%d]\n", bool2str(use_ident).c_str(), __LINE__);
               }
            }
            if (pc->column != indent_column)
            {
               if (use_ident &&
                   not_type(pc, CT_PP_IGNORE)) // Leave indentation alone for PP_IGNORE tokens
               {
                  log_and_reindent(pc, indent_column, "indent");
               }
               else
               {  // do not indent this line
                  LOG_FMT(LINDENT, "%s(%d): %u] don't indent this line [%d]\n",
                          __func__, __LINE__, pc->orig_line, __LINE__);
               }
            }
         }
         my_did_newline = false;

         if (is_type(pc, CT_SQL_EXEC, CT_SQL_BEGIN, CT_SQL_END))
         {
            sql_col      = pc->column;
            sql_orig_col = pc->orig_col;
         }

         /* Handle indent for variable defs at the top of a block of code */
         if (is_flag(pc, PCF_VAR_TYPE))
         {
            if ((!frm.pse[frm.pse_tos].non_vardef           ) &&
                ( frm.pse[frm.pse_tos].type == CT_BRACE_OPEN) )
            {
               uint32_t tmp = indent_column;
               if (cpd.settings[UO_indent_var_def_blk].n > 0)
               {
                  tmp = cpd.settings[UO_indent_var_def_blk].u;
               }
               else
               {
                  tmp += cpd.settings[UO_indent_var_def_blk].u;
               }
               reindent_line(pc, tmp);
               LOG_FMT(LINDENT, "%s(%d): %u] var_type indent => %u [%s]\n",
                       __func__, __LINE__, pc->orig_line, tmp, pc->text());
            }
         }
         else
         {
            if (pc != frm.pse[frm.pse_tos].pc)
            {
               frm.pse[frm.pse_tos].non_vardef = true;
            }
         }
      }

      /* if we hit a newline, reset indent_tmp */
      if (is_nl(pc)         ||
          is_type(pc, CT_COMMENT_MULTI, CT_COMMENT_CPP))
      {
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

         /* Handle the case of a multi-line #define w/o anything on the
          * first line (indent_tmp will be 1 or 0) */
         if (is_type(pc, CT_NL_CONT) &&
             (frm.pse[frm.pse_tos].indent_tmp <= indent_size))
         {
            frm.pse[frm.pse_tos].indent_tmp = indent_size + 1;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%u, ... indent_tmp=%u\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }

         /* Get ready to indent the next item */
         my_did_newline = true;
      }

      /* Check for open XML tags "</..." */
      if (cpd.settings[UO_indent_xml_string].u > 0)
      {
         if (is_type(pc, CT_STRING))
         {
            if ((pc->len()               >  4 ) &&
                (pc->str[1]             == '<') &&
                (pc->str[2]             != '/') &&
                (pc->str[pc->len() - 3] != '/') )
            {
               if (xml_indent <= 0)
               {
                  xml_indent = (int32_t)pc->column;
               }
               xml_indent += cpd.settings[UO_indent_xml_string].n;
            }
         }
      }

      if(!is_cmt_or_nl(pc))
      {
         prev = pc;
      }
      pc = chunk_get_next(pc);
   }

null_pc:

   /* Throw out any stuff inside a preprocessor - no need to warn */
   while ((frm.pse_tos > 0) && frm.pse[frm.pse_tos].in_preproc)
   {
      indent_pse_pop(frm, pc);
   }

   /* Throw out any VBRACE_OPEN at the end - implied with the end of file  */
   while ((frm.pse_tos > 0) && (frm.pse[frm.pse_tos].type == CT_VBRACE_OPEN))
   {
      indent_pse_pop(frm, pc);
   }

   for (uint32_t idx_temp = 1; idx_temp <= frm.pse_tos; idx_temp++)
   {
      LOG_FMT(LWARN, "%s:%u Unmatched %s\n", cpd.filename,
            frm.pse[idx_temp].open_line, get_token_name(frm.pse[idx_temp].type));
      cpd.error_count++;
   }

   quick_align_again();
   quick_indent_again();
}


void log_and_reindent(chunk_t* pc, const uint32_t val, const char* str)
{
   LOG_FMT(LINDENT, "%s(%d): %u] %s => %u\n",
           __func__, __LINE__, pc->orig_line, str, val);
   reindent_line(pc, val);
}


void log_and_indent_comment(chunk_t* pc, const uint32_t val, const char* str)
{
   LOG_FMT(LINDENT, "%s(%d): %u] %s => %u\n",
           __func__, __LINE__, pc->orig_line, str, val);
   indent_comment(pc, val);
}


static bool single_line_comment_indent_rule_applies(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   retval_if(!is_single_line_cmt(start), false);

   /* scan forward, if only single newlines and comments before next line of
    * code, we want to apply */
   chunk_t* pc = start;
   uint32_t nl_count = 0;
   while ((pc = chunk_get_next(pc)) != nullptr)
   {
      if (is_nl(pc))
      {
         retval_if ((nl_count > 0) || (pc->nl_count > 1), false);
         nl_count++;
      }
      else
      {
         nl_count = 0;
         if (!is_single_line_cmt(pc))
         {
            /* here we check for things to run into that we wouldn't want to
             * indent the comment for. for example, non-single line comment,
             * closing brace */
            return (is_cmt(pc) || is_closing_brace(pc)) ? false : true;
         }
      }
   }

   return(false);
}


static void indent_comment(chunk_t* pc, uint32_t col)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LCMTIND, "%s(%d): orig_line %u, orig_col %u, level %u: ",
           __func__, __LINE__, pc->orig_line, pc->orig_col, pc->level);

   /* force column 1 comment to column 1 if not changing them */
   if ((pc->orig_col == 1) && is_false(UO_indent_col1_comment) &&
       not_flag(pc, PCF_INSERTED))
   {
      LOG_FMT(LCMTIND, "rule 1 - keep in col 1\n");
      reindent_line(pc, 1);
      return;
   }

   chunk_t* nl = chunk_get_prev(pc);

   /* outside of any expression or statement? */
   if (pc->level == 0)
   {
      if (is_valid(nl) && (nl->nl_count > 1 ))
      {
         LOG_FMT(LCMTIND, "rule 2 - level 0, nl before\n");
         reindent_line(pc, 1);
         return;
      }
   }

   chunk_t* prev = chunk_get_prev(nl);
   if((is_cmt(prev)) && (is_valid(nl)) && (nl->nl_count == 1))
   {
      assert(is_valid(prev));
      int32_t     coldiff = (int32_t)prev->orig_col - (int32_t)pc->orig_col;
      chunk_t* pp     = chunk_get_prev(prev);

      /* Here we want to align comments that are relatively close one to another
       * but not when the previous comment is on the same line with a preproc */
      if ((coldiff <=  3) &&
          (coldiff >= -3) &&
          !is_preproc(pp) )
      {
         reindent_line(pc, prev->column);
         LOG_FMT(LCMTIND, "rule 3 - prev comment, coldiff = %d, now in %u\n",
                 coldiff, pc->column);
         return;
      }
   }

   /* check if special single line comment rule applies */
   if ((cpd.settings[UO_indent_sing_line_comments].u > 0) &&
       single_line_comment_indent_rule_applies(pc))
   {
      reindent_line(pc, col + cpd.settings[UO_indent_sing_line_comments].u);
      LOG_FMT(LCMTIND, "rule 4 - single line comment indent, now in %u\n", pc->column);
      return;
   }
   LOG_FMT(LCMTIND, "rule 5 - fall-through, stay in %u\n", col);

   reindent_line(pc, col);
}


bool ifdef_over_whole_file(void)
{
   LOG_FUNC_ENTRY();

   /* the results for this file are cached */
   if (cpd.ifdef_over_whole_file)
   {
      return(cpd.ifdef_over_whole_file > 0);
   }

   uint32_t         stage   = 0;
   chunk_t*       end_pp = nullptr;
   const chunk_t* next;
   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if(is_cmt_or_nl(pc));

      if (stage == 0)
      {
         /* Check the first preprocessor, make sure it is an #if type */
         break_if(not_type(pc, CT_PREPROC));
         next = chunk_get_next(pc);
         break_if(is_invalid_or_not_type(next, CT_PP_IF));
         stage = 1;
      }
      else if (stage == 1)
      {
         /* Scan until a preprocessor at level 0 is found - the close to the #if */
         if(is_type(pc, CT_PREPROC) && (pc->pp_level == 0))
         {
            stage  = 2;
            end_pp = pc;
         }
         continue;
      }
      else if (stage == 2)
      {
         /* We should only see the rest of the preprocessor */
         if(is_type (pc, CT_PREPROC) || !is_preproc(pc))
         {
            stage = 0;
            break;
         }
      }
   }

   cpd.ifdef_over_whole_file = (stage == 2) ? 1 : -1;
   if (cpd.ifdef_over_whole_file > 0)
   {
      set_flags(end_pp, PCF_WF_ENDIF);
   }
   LOG_FMT(LNOTE, "The whole file is%s covered by a #IF\n",
           (cpd.ifdef_over_whole_file > 0) ? "" : " NOT");
   return(cpd.ifdef_over_whole_file > 0);
}


void indent_preproc(void)
{
   LOG_FUNC_ENTRY();
   chunk_t* next;
   int32_t     pp_level;
   int32_t     pp_level_sub = 0;

   /* Scan to see if the whole file is covered by one #ifdef */
   if (ifdef_over_whole_file())
   {
      pp_level_sub = 1;
   }

   for (chunk_t* pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if(not_type(pc, CT_PREPROC));

      next = get_next_ncnl(pc);
      break_if(is_invalid(next));

      pp_level = (int32_t)pc->pp_level - pp_level_sub;
      pp_level = max(pp_level, 0);

      /* Adjust the indent of the '#' */
      if (is_opt_set(UO_pp_indent, AV_ADD))
      {
         reindent_line(pc, 1 + (uint32_t)pp_level * cpd.settings[UO_pp_indent_count].u);
      }
      else if (is_opt_set(cpd.settings[UO_pp_indent].a, AV_REMOVE))
      {
         reindent_line(pc, 1);
      }

      /* Add spacing by adjusting the length */
      if ((cpd.settings[UO_pp_space].a != AV_IGNORE) &&  (is_valid(next)))
      {
         if (is_opt_set(UO_pp_space, AV_ADD))
         {
            uint32_t mult = cpd.settings[UO_pp_space_count].u;
            mult = max(mult, (uint32_t)1u);

            reindent_line(next, (uint32_t)((int32_t)pc->column + (int32_t)pc->len() + (pp_level * (int32_t)mult)));
         }
         else if (is_opt_set(cpd.settings[UO_pp_space].a, AV_REMOVE))
         {
            reindent_line(next, pc->column + pc->len());
         }
      }

      /* Mark as already handled if not region stuff or in column 1 */
      if ((is_false(UO_pp_indent_at_level) ||
           (pc->brace_level <= (is_ptype(pc, CT_PP_DEFINE) ? 1 : 0))) &&
           not_ptype(pc, 2, CT_PP_REGION, CT_PP_ENDREGION) )
      {
         if (is_false(UO_pp_define_at_level) || not_ptype(pc, CT_PP_DEFINE))
         {
            set_flags(pc, PCF_DONT_INDENT);
         }
      }

      LOG_FMT(LPPIS, "%s(%d): orig_line %u to %d (len %u, next->col %d)\n",
              __func__, __LINE__, pc->orig_line, 1 + pp_level, pc->len(),
              next ? (int32_t)next->column : -1);
   }
}
