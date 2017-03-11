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


 enum class align_mode_e : unsigned int
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
   chunk_t *pc,  /**< [in] The comment, which is the first item on a line */
   size_t  col   /**< [in] The column if this is to be put at indent level */
);


/**
 * Starts a new entry
 */
static void indent_pse_push(
   parse_frame_t &frm,  /**< [in] The parse frame */
   chunk_t       *pc    /**< [in] The chunk causing the push */
);


/**
 * Removes the top entry
 */
static void indent_pse_pop(
   parse_frame_t &frm,  /**< [in] The parse frame */
   chunk_t       *pc    /**< [in] The chunk causing the push */
);


/**
 * tbd
 */
static size_t token_indent(
   c_token_t type  /**< [in]  */
);


/**
 * tbd
 */
static size_t calc_indent_continue(
   const parse_frame_t &frm,    /**< [in] The parse frame */
   size_t              pse_tos  /**< [in]  */
);


/**
 * We are on a '{' that has parent = OC_BLOCK_EXPR
 * find the column of the param tag
 */
static chunk_t *oc_msg_block_indent(
   chunk_t *pc,       /**< [in]  */
   bool from_brace,   /**< [in]  */
   bool from_caret,   /**< [in]  */
   bool from_colon,   /**< [in]  */
   bool from_keyword  /**< [in]  */
);


/**
 * We are on a '{' that has parent = OC_BLOCK_EXPR
 */
static chunk_t *oc_msg_prev_colon(
   chunk_t *pc   /**< [in]  */
);


/**
 * returns true if forward scan reveals only single newlines or comments
 * stops when hits code
 * false if next thing hit is a closing brace, also if 2 newlines in a row
 */
static bool single_line_comment_indent_rule_applies(
   chunk_t *start  /**< [in]  */
);


/**
 * tbd
 */
void log_and_reindent(
   chunk_t      *pc,   /**< [in] chunk to operate with */
   const size_t val,   /**< [in] value to print */
   const char   *str   /**< [in] string to print */
);


/**
 * tbd
 */
void log_and_indent_comment(
   chunk_t      *pc,   /**< [in] chunk to operate with */
   const size_t val,   /**< [in] value to print */
   const char   *str   /**< [in] string to print */
);


static const char *get_align_mode_name(const align_mode_e align_mode);


static const char *get_align_mode_name(const align_mode_e align_mode)
{
   switch(align_mode)
   {
      case(align_mode_e::SHIFT   ): return ("sft"  );
      case(align_mode_e::KEEP_ABS): return ("abs"  );
      case(align_mode_e::KEEP_REL): return ("rel"  );
      default:                      return ("error");
   }
}


void indent_to_column(chunk_t *pc, size_t column)
{
   LOG_FUNC_ENTRY();
   column = max(column, pc->column);
   reindent_line(pc, column);
}


void align_to_column(chunk_t *pc, size_t column)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc) || (column == pc->column));

   LOG_FMT(LINDLINE, "%s(%d): %zu] col %zu on %s [%s] => %zu\n",
           __func__, __LINE__, pc->orig_line, pc->column, pc->text(),
           get_token_name(pc->type), column);

   size_t col_delta = column - pc->column;
   size_t min_col   = column;
   pc->column = column;
   do
   {
      align_mode_e almod = align_mode_e::SHIFT;

      chunk_t *next = chunk_get_next(pc);
      break_if(is_invalid(next));

      int min_delta = (int)space_col_align(pc, next);
      min_col  = (size_t)((int)min_col + min_delta);
      chunk_t  *prev = pc;
      pc       = next;

      if (is_cmt(pc) &&
         (pc->ptype != CT_COMMENT_EMBED))
      {
         almod = (is_single_line_cmt(pc) &&
                  cpd.settings[UO_indent_rel_single_line_comments].b) ?
                 align_mode_e::KEEP_REL : align_mode_e::KEEP_ABS;
      }

      switch(almod)
      {
         case(align_mode_e::KEEP_ABS):
            /* Keep same absolute column */
            pc->column = pc->orig_col;
            pc->column = max(min_col, pc->column);
            break;

         case(align_mode_e::KEEP_REL):
         {
            /* Keep same relative column */
            int orig_delta = (int)pc->orig_col - (int)prev->orig_col;
            orig_delta = max(min_delta, orig_delta);
            pc->column = (size_t)((int)prev->column + orig_delta);
            break;
         }

         case(align_mode_e::SHIFT):
            /* Shift by the same amount */
            pc->column += col_delta;
            pc->column = max(min_col, pc->column);
            break;

         default: /* unknown enum, do nothing */ break;
      }


      LOG_FMT(LINDLINED, "   %s set column of %s on line %zu to col %zu (orig %zu)\n",
            get_align_mode_name(almod), get_token_name(pc->type), pc->orig_line,
            pc->column, pc->orig_col);
   } while ( is_valid(pc) &&
            (pc->nl_count == 0) );
}


void reindent_line(chunk_t *pc, size_t column)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(pc));
   LOG_FMT(LINDLINE, "%s(%d): %zu] col %zu on '%s' [%s/%s] => %zu",
           __func__, __LINE__, pc->orig_line, pc->column, pc->text(),
           get_token_name(pc->type), get_token_name(pc->ptype), column);
   log_func_stack_inline(LINDLINE);

   return_if(column == pc->column);

   size_t col_delta = column - pc->column;
   size_t min_col   = column;

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
      chunk_t *next = chunk_get_next(pc);
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
                        cpd.settings[UO_indent_rel_single_line_comments].b;

      if ((is_comment      == true      ) &&
          (pc->ptype != CT_COMMENT_EMBED) &&
          (keep            == false     ) )
      {
         pc->column = pc->orig_col;
         pc->column = max(min_col, pc->column);

         LOG_FMT(LINDLINE, "%s(%d): set comment on line %zu to col %zu (orig %zu)\n",
                 __func__, __LINE__, pc->orig_line, pc->column, pc->orig_col);
      }
      else
      {
         pc->column += col_delta;
         pc->column = max(min_col, pc->column);

         LOG_FMT(LINDLINED, "   set column of '%s' to %zu (orig %zu)\n",
         (is_type(pc, CT_NEWLINE)) ? "newline" : pc->text(), pc->column, pc->orig_col);
      }
   } while (is_valid(pc) && (pc->nl_count == 0));
}


static void indent_pse_push(parse_frame_t &frm, chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(pc));

   /* check the stack depth */
   if (frm.pse_tos < (ARRAY_SIZE(frm.pse) - 1))
   {
      static size_t ref = 0;
      /* Bump up the index and initialize it */
      frm.pse_tos++;
      LOG_FMT(LINDLINE, "%s(%d): line=%zu, pse_tos=%zu, type=%s\n",
              __func__, __LINE__, pc->orig_line, frm.pse_tos, get_token_name(pc->type));

      size_t index = frm.pse_tos;
      memset(&frm.pse[index], 0, sizeof(paren_stack_entry_t));

      //LOG_FMT(LINDPSE, "%s(%d):%d] (pp=%d) OPEN  [%d,%s] level=%d\n",
      //        __func__, __LINE__, pc->orig_line, cpd.pp_level, frm.pse_tos, get_token_name(pc->type), pc->level);

      frm.pse[frm.pse_tos].pc          = pc;
      frm.pse[frm.pse_tos].type        = pc->type;
      frm.pse[frm.pse_tos].level       = pc->level;
      frm.pse[frm.pse_tos].open_line   = pc->orig_line;
      frm.pse[frm.pse_tos].ref         = (int)++ref;
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


static void indent_pse_pop(parse_frame_t &frm, chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   /* Bump up the index and initialize it */
   if (frm.pse_tos > 0)
   {
      if (is_valid(pc))
      {
         LOG_FMT(LINDPSE, "%4zu] (pp=%zu) CLOSE [%zu,%s] on %s, started on line %zu, level=%zu/%zu\n",
                 pc->orig_line, cpd.pp_level, frm.pse_tos,
                 get_token_name(frm.pse[frm.pse_tos].type), get_token_name(pc->type),
                 frm.pse[frm.pse_tos].open_line, frm.pse[frm.pse_tos].level, pc->level);
      }
      else
      {
         LOG_FMT(LINDPSE, " EOF] CLOSE [%zu,%s], started on line %zu\n",
                 frm.pse_tos, get_token_name(frm.pse[frm.pse_tos].type),
                 frm.pse[frm.pse_tos].open_line);
      }

      /* Don't clear the stack entry because some code 'cheats' and uses the
       * just-popped indent values */
      frm.pse_tos--;
      LOG_FMT(LINDLINE, "(%d) ", __LINE__);
      if (is_valid(pc))
      {
         LOG_FMT(LINDLINE, "%s(%d): orig_line=%zu, pse_tos=%zu, type=%s\n",
                 __func__, __LINE__, pc->orig_line, frm.pse_tos, get_token_name(pc->type));
      }
      else
      {
         LOG_FMT(LINDLINE, "%s(%d): ------------------- pse_tos=%zu\n",
                 __func__, __LINE__, frm.pse_tos);
      }
   }
   else
   {
      /* fatal error */
      fprintf(stderr, "the stack index is already zero\n");
      assert(is_valid(pc));
      fprintf(stderr, "at line=%zu, type is %s\n",
              pc->orig_line, get_token_name(pc->type));
      exit(EXIT_FAILURE);
   }
}


static size_t token_indent(c_token_t type)
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
      LOG_FMT(LINDENT2, "%s:[line %d], orig_line=%zu, indent_column = %zu\n", \
              __func__, __LINE__, pc->orig_line, indent_column);              \
   } while (0)


static size_t calc_indent_continue(const parse_frame_t &frm, size_t pse_tos)
{
   int ic = cpd.settings[UO_indent_continue].n;

   if ((ic < 0) &&
       frm.pse[pse_tos].indent_cont)
   {
      return(frm.pse[pse_tos].indent);
   }
   return(frm.pse[pse_tos].indent + (size_t)abs(ic));
}


static chunk_t *oc_msg_block_indent(chunk_t *pc, bool from_brace,
      bool from_caret, bool from_colon,  bool from_keyword)
{
   LOG_FUNC_ENTRY();
   chunk_t *tmp = chunk_get_prev_nc(pc);

   retval_if(from_brace, pc);

   if (is_paren_close(tmp))
   {
      tmp = chunk_get_prev_nc(chunk_skip_to_match_rev(tmp));
   }
   retval_if(is_invalid_or_not_type(tmp, CT_OC_BLOCK_CARET), nullptr);
   retval_if(from_caret, tmp);

   tmp = chunk_get_prev_nc(tmp);
   retval_if(is_invalid_or_not_type(tmp, CT_OC_COLON), nullptr);
   retval_if(from_colon, tmp);

   tmp = chunk_get_prev_nc(tmp);
   if (is_invalid (tmp                                ) ||
       not_type(tmp, CT_OC_MSG_NAME, CT_OC_MSG_FUNC) )
   {
      return(nullptr);
   }
   retval_if(from_keyword, tmp);
   return(nullptr);
}


static chunk_t *oc_msg_prev_colon(chunk_t *pc)
{
   return(get_prev_type(pc, CT_OC_COLON, (int)pc->level, scope_e::ALL));
}


void indent_text(void)
{
   LOG_FUNC_ENTRY();
   chunk_t       *pc;
   chunk_t       *next;
   chunk_t       *prev       = nullptr;
   bool          my_did_newline = true;
   int           idx;
   size_t        vardefcol    = 0;
   size_t        shiftcontcol = 0;
   size_t        indent_size  = cpd.settings[UO_indent_columns].u;
   parse_frame_t frm;
   bool          in_preproc = false;
   size_t        indent_column;
   size_t        parent_token_indent = 0;
   int           xml_indent          = 0;
   bool          token_used;
   size_t        sql_col      = 0;
   size_t        sql_orig_col = 0;
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
         LOG_FMT(LINDLINE, "%s(%d): %zu NEWLINE\n",
                 __func__, __LINE__, pc->orig_line);
      }
      else if (pc->type == CT_NL_CONT)
      {
         LOG_FMT(LINDLINE, "%s(%d): %zu CT_NL_CONT\n",
                 __func__, __LINE__, pc->orig_line);
      }
      else
      {
         LOG_FMT(LINDLINE, "%s(%d): %zu:%zu pc->text() %s\n",
                 __func__, __LINE__, pc->orig_line, pc->orig_col, pc->text());
         log_pcf_flags(LINDLINE, get_flags(pc));
      }
      if ((cpd.settings[UO_use_options_overriding_for_qt_macros].b) &&
          ((strcmp(pc->text(), "SIGNAL") == 0) ||
           (strcmp(pc->text(), "SLOT"  ) == 0) ) )
      {
         LOG_FMT(LINDLINE, "%s(%d): orig_line=%zu: type %s SIGNAL/SLOT found\n",
                 __func__, __LINE__, pc->orig_line, get_token_name(pc->type));
      }
      /* Handle preprocessor transitions */
      in_preproc = is_preproc(pc);

      if (cpd.settings[UO_indent_brace_parent].b)
      {
         parent_token_indent = token_indent(pc->ptype);
      }

      /* Handle "force indentation of function definition to start in column 1" */
      if (cpd.settings[UO_indent_func_def_force_col1].b)
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
              is_ptype(pc, CT_PP_ENDIF, CT_PP_ELSE     ) )
         {
            indent_pse_pop(frm, pc);
         }

         pf_check(&frm, pc);

         /* Indent the body of a #region here */
         if (cpd.settings[UO_pp_region_indent_code].b &&
             (pc->ptype == CT_PP_REGION))
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
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }

         /* Indent the body of a #if here */
         if (cpd.settings[UO_pp_if_indent_code].b &&
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
            size_t extra = ((pc->pp_level == 0) && ifdef_over_whole_file()) ? 0 : indent_size;
            frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent     + extra;
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos-1].indent_tab + extra;
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
            frm.pse[frm.pse_tos].in_preproc = false;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }

         /* Transition into a preproc by creating a dummy indent */
         frm.level++;
         indent_pse_push(frm, chunk_get_next(pc));

         if ((pc->ptype == CT_PP_DEFINE) ||
             (pc->ptype == CT_PP_UNDEF ) )
         {
            frm.pse[frm.pse_tos].indent_tmp = cpd.settings[UO_pp_define_at_level].b ?
                                              frm.pse[frm.pse_tos-1].indent_tmp : 1;
            frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos  ].indent_tmp + indent_size;
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos  ].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }
         else if ((pc->ptype == CT_PP_PRAGMA) &&
                  cpd.settings[UO_pp_define_at_level].b)
         {
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos-1].indent_tmp;
            frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos  ].indent_tmp + indent_size;
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos  ].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent);
            if ((pc->ptype == CT_PP_REGION   ) ||
                (pc->ptype == CT_PP_ENDREGION) )
            {
               int val = cpd.settings[UO_pp_indent_region].n;
               if (val > 0)
               {
                  frm.pse[frm.pse_tos].indent = (size_t)val;
               }
               else
               {
                  frm.pse[frm.pse_tos].indent = (size_t)((int)frm.pse[frm.pse_tos].indent + val);
               }
            }
            else if (is_ptype(pc, CT_PP_IF, CT_PP_ELSE, CT_PP_ENDIF))
            {
               int val = cpd.settings[UO_pp_indent_if].n;
               if (val > 0)
               {
                  frm.pse[frm.pse_tos].indent = (size_t)val;
               }
               else
               {
                  frm.pse[frm.pse_tos].indent = (size_t)((int)frm.pse[frm.pse_tos].indent + val);
               }
            }
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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

      /**
       * Handle non-brace closures
       */
      LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
              __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

      token_used = false;
      size_t old_pse_tos;
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
            if (((frm.pse[frm.pse_tos].type == CT_ASSIGN_NL) ||
                 (frm.pse[frm.pse_tos].type == CT_ASSIGN   ) )               &&
                 (is_semicolon(pc)                                   ||
                  is_type(pc, CT_COMMA, CT_BRACE_OPEN, CT_SPAREN_CLOSE) ||
                  is_type_and_ptype(pc, CT_SQUARE_OPEN, CT_OC_AT         ) ||
                  is_type_and_ptype(pc, CT_SQUARE_OPEN, CT_ASSIGN        ) ) &&
                  (pc->ptype != CT_CPP_LAMBDA)                               )
            {
               indent_pse_pop(frm, pc);
            }

            /* End any assign operations with a semicolon on the same level */
            if (is_semicolon(pc)                      &&
                ((frm.pse[frm.pse_tos].type == CT_IMPORT) ||
                 (frm.pse[frm.pse_tos].type == CT_USING ) ) )
            {
               indent_pse_pop(frm, pc);
            }

            /* End any custom macro-based open/closes */
            if (!token_used &&
                (frm.pse[frm.pse_tos].type == CT_MACRO_OPEN) &&
                 is_type(pc, CT_MACRO_CLOSE))
            {
               token_used = true;
               indent_pse_pop(frm, pc);
            }

            /* End any CPP/ObjC class colon stuff */
            if (((frm.pse[frm.pse_tos].type == CT_CLASS_COLON) ||
                 (frm.pse[frm.pse_tos].type == CT_CONSTR_COLON)) &&
                (is_type(pc, CT_BRACE_OPEN, CT_OC_END, CT_OC_SCOPE, CT_OC_PROPERTY) ||
                      is_semicolon(pc) ) )
            {
               indent_pse_pop(frm, pc);
            }
            /* End ObjC class colon stuff inside of generic definition (like Test<T1: id<T3>>) */
            if ((frm.pse[frm.pse_tos].type == CT_CLASS_COLON) &&
                 is_type_and_ptype(pc, CT_ANGLE_CLOSE, CT_OC_GENERIC_SPEC))
            {
               indent_pse_pop(frm, pc);
            }

            /* a case is ended with another case or a close brace */
            if ((frm.pse[frm.pse_tos].type == CT_CASE) &&
                (is_type(pc, CT_BRACE_CLOSE, CT_CASE)))
            {
               indent_pse_pop(frm, pc);
            }

            /* a class scope is ended with another class scope or a close brace */
            if (cpd.settings[UO_indent_access_spec_body].b &&
                (frm.pse[frm.pse_tos].type == CT_PRIVATE) &&
                (is_type(pc, CT_BRACE_CLOSE, CT_PRIVATE)))
            {
               indent_pse_pop(frm, pc);
            }

            /* return & throw are ended with a semicolon */
            if (is_semicolon(pc) &&
                ((frm.pse[frm.pse_tos].type == CT_RETURN) ||
                 (frm.pse[frm.pse_tos].type == CT_THROW ) ) )
            {
               indent_pse_pop(frm, pc);
            }

            /* an OC SCOPE ('-' or '+') ends with a semicolon or brace open */
            if ((frm.pse[frm.pse_tos].type == CT_OC_SCOPE) &&
                (is_semicolon(pc) ||
                 is_type(pc, CT_BRACE_OPEN)))
            {
               indent_pse_pop(frm, pc);
            }

            /* a typedef and an OC SCOPE ('-' or '+') ends with a semicolon or
             * brace open */
            if ((frm.pse[frm.pse_tos].type == CT_TYPEDEF) &&
                (is_semicolon (pc) ||
                 is_paren_open(pc) ||
                 is_type(pc, CT_BRACE_OPEN)))
            {
               indent_pse_pop(frm, pc);
            }

            /* an SQL EXEC is ended with a semicolon */
            if ((frm.pse[frm.pse_tos].type == CT_SQL_EXEC) &&
                  is_semicolon(pc))
            {
               indent_pse_pop(frm, pc);
            }

            /* an CLASS is ended with a semicolon or brace open */
            if ((frm.pse[frm.pse_tos].type == CT_CLASS) &&
                (is_type(pc, CT_CLASS_COLON, CT_BRACE_OPEN) ||
                      is_semicolon(pc)))
            {
               indent_pse_pop(frm, pc);
            }

            /* Close out parenthesis and squares */
            if ((frm.pse[frm.pse_tos].type == get_inverse_type(pc->type) ) &&
                (is_type(pc, 5, CT_PAREN_CLOSE,  CT_SPAREN_CLOSE,
                     CT_FPAREN_CLOSE, CT_SQUARE_CLOSE, CT_ANGLE_CLOSE) ) )
            {
               indent_pse_pop(frm, pc);
               frm.paren_count--;
            }
         }
      } while (old_pse_tos > frm.pse_tos);

      /* Grab a copy of the current indent */
      indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
      LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
              __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

      if (!is_nl(pc) && !is_cmt(pc) && log_sev_on(LINDPC))
      {
         LOG_FMT(LINDPC, " -=[ %zu:%zu %s ]=-\n",
                 pc->orig_line, pc->orig_col, pc->text());
         for (size_t ttidx = frm.pse_tos; ttidx > 0; ttidx--)
         {
            LOG_FMT(LINDPC, "     [%zu %zu:%zu %s %s/%s tmp=%zu ind=%zu bri=%d tab=%zu cont=%d lvl=%zu blvl=%zu]\n",
                    ttidx,
                    frm.pse[ttidx].pc->orig_line,
                    frm.pse[ttidx].pc->orig_col,
                    frm.pse[ttidx].pc->text(),
                    get_token_name(frm.pse[ttidx].type),
                    get_token_name(frm.pse[ttidx].pc->ptype),
                    frm.pse[ttidx].indent_tmp,
                    frm.pse[ttidx].indent,
                    frm.pse[ttidx].brace_indent,
                    frm.pse[ttidx].indent_tab,
                    (int)frm.pse[ttidx].indent_cont,
                    frm.pse[ttidx].level,
                    frm.pse[ttidx].pc->brace_level);
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
         brace_indent = (( cpd.settings[UO_indent_braces          ].b                                    ) &&
                         (!cpd.settings[UO_indent_braces_no_func  ].b || (pc->ptype != CT_FUNC_DEF      )) &&
                         (!cpd.settings[UO_indent_braces_no_func  ].b || (pc->ptype != CT_FUNC_CLASS_DEF)) &&
                         (!cpd.settings[UO_indent_braces_no_class ].b || (pc->ptype != CT_CLASS         )) &&
                         (!cpd.settings[UO_indent_braces_no_struct].b || (pc->ptype != CT_STRUCT        )) );
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

         size_t iMinIndent = cpd.settings[UO_indent_min_vbrace_open].u;
         if (indent_size > iMinIndent)
         {
            iMinIndent = indent_size;
         }
         size_t iNewIndent = frm.pse[frm.pse_tos - 1].indent + iMinIndent;
         if (cpd.settings[UO_indent_vbrace_open_on_tabstop].b)
         {
            iNewIndent = next_tab_column(iNewIndent);
         }
         frm.pse[frm.pse_tos].indent     = iNewIndent;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
         frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

         /* Always indent on virtual braces */
         indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
      }
      else if (is_type    (pc,       CT_BRACE_OPEN) &&
               not_type(pc->next, CT_NAMESPACE ) )
      {
         frm.level++;
         indent_pse_push(frm, pc);

         if (cpd.settings[UO_indent_cpp_lambda_body].b &&
             is_ptype(pc, CT_CPP_LAMBDA))
         {
            // DRY6
            frm.pse[frm.pse_tos  ].brace_indent = (int)frm.pse[frm.pse_tos-1].indent;
            indent_column                       = frm.pse[frm.pse_tos  ].brace_indent;
            frm.pse[frm.pse_tos  ].indent       = indent_column + indent_size;
            frm.pse[frm.pse_tos  ].indent_tab   = frm.pse[frm.pse_tos  ].indent;
            frm.pse[frm.pse_tos  ].indent_tmp   = frm.pse[frm.pse_tos  ].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            frm.pse[frm.pse_tos-1].indent_tmp   = frm.pse[frm.pse_tos  ].indent_tmp;
         }
         else if (is_lang(cpd, LANG_CS) &&
                   cpd.settings[UO_indent_cs_delegate_brace].b &&
                   is_ptype(pc, CT_LAMBDA, CT_DELEGATE))
         {
            // DRY6
            frm.pse[frm.pse_tos  ].brace_indent = (int)(1 + ((pc->brace_level+1) * indent_size));
            indent_column                       = frm.pse[frm.pse_tos].brace_indent;
            frm.pse[frm.pse_tos  ].indent       = indent_column + indent_size;
            frm.pse[frm.pse_tos  ].indent_tab   = frm.pse[frm.pse_tos].indent;
            frm.pse[frm.pse_tos  ].indent_tmp   = frm.pse[frm.pse_tos].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            frm.pse[frm.pse_tos-1].indent_tmp   = frm.pse[frm.pse_tos].indent_tmp;
         }
         /* any '{' that is inside of a '(' overrides the '(' indent */
         else if (!cpd.settings[UO_indent_paren_open_brace].b &&
                  is_paren_open(frm.pse[frm.pse_tos-1].pc) &&
                  is_nl(get_next_nc(pc)))
         {
            /* FIXME: I don't know how much of this is necessary, but it seems to work */
            // DRY6
            frm.pse[frm.pse_tos  ].brace_indent = (int)(1 + (pc->brace_level * indent_size));
            indent_column                       = frm.pse[frm.pse_tos].brace_indent;
            frm.pse[frm.pse_tos  ].indent       = indent_column + indent_size;
            frm.pse[frm.pse_tos  ].indent_tab   = frm.pse[frm.pse_tos].indent;
            frm.pse[frm.pse_tos  ].indent_tmp   = frm.pse[frm.pse_tos].indent;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            frm.pse[frm.pse_tos-1].indent_tmp   = frm.pse[frm.pse_tos].indent_tmp;
         }
         else if (frm.paren_count != 0)
         {
            if (frm.pse[frm.pse_tos].pc->ptype == CT_OC_BLOCK_EXPR)
            {
               if (is_flag(pc, PCF_IN_OC_MSG) &&
                   cpd.settings[UO_indent_oc_block_msg].u)
               {
                  frm.pse[frm.pse_tos].ip.ref   = oc_msg_block_indent(pc, false, false, false, true);
                  frm.pse[frm.pse_tos].ip.delta = cpd.settings[UO_indent_oc_block_msg].n;
               }

               if (cpd.settings[UO_indent_oc_block                ].b ||
                   cpd.settings[UO_indent_oc_block_msg_xcode_style].b)
               {
                  bool in_oc_msg           = (is_flag(pc, PCF_IN_OC_MSG));
                  bool indent_from_keyword = cpd.settings[UO_indent_oc_block_msg_from_keyword].b && in_oc_msg;
                  bool indent_from_colon   = cpd.settings[UO_indent_oc_block_msg_from_colon  ].b && in_oc_msg;
                  bool indent_from_caret   = cpd.settings[UO_indent_oc_block_msg_from_caret  ].b && in_oc_msg;
                  bool indent_from_brace   = cpd.settings[UO_indent_oc_block_msg_from_brace  ].b && in_oc_msg;

                  // In "Xcode indent mode", we want to indent:
                  //  - if the colon is aligned (namely, if a newline has been
                  //    added before it), indent_from_brace
                  //  - otherwise, indent from previous block (the "else" statement here)
                  if (cpd.settings[UO_indent_oc_block_msg_xcode_style].b)
                  {
                     chunk_t *colon        = oc_msg_prev_colon(pc);
                     chunk_t *param_name   = chunk_get_prev(colon);
                     const chunk_t *before_param = chunk_get_prev(param_name);
                     const bool is_newline = is_type(before_param, CT_NEWLINE);
                     indent_from_keyword   = (is_newline) ? true  : false;
                     indent_from_brace     = false; // (is_newline) ? false : true;
                     indent_from_colon     = false;
                     indent_from_caret     = false;

                  }

                  const chunk_t *ref = oc_msg_block_indent(pc, indent_from_brace,
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
                 CT_TRY, CT_CATCH, CT_DO, CT_WHILE, CT_SWITCH, CT_SYNCHRONIZED) )
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
            else if (pc->ptype == CT_CASE)
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
               LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                       __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            }
            else if (is_ptype(pc, CT_CLASS) &&
                     !cpd.settings[UO_indent_class].b)
            {
               frm.pse[frm.pse_tos].indent -= indent_size;
            }
            else if (pc->ptype == CT_NAMESPACE)
            {
               if (cpd.settings[UO_indent_namespace              ].b &&
                   cpd.settings[UO_indent_namespace_single_indent].b)
               {
                  if (frm.pse[frm.pse_tos].ns_cnt)
                  {
                     /* undo indent on all except the first namespace */
                     frm.pse[frm.pse_tos].indent -= indent_size;
                  }
                  indent_column_set((frm.pse_tos <= 1) ? 1 : frm.pse[frm.pse_tos-1].brace_indent);
               }
               else if (is_flag(pc, PCF_LONG_BLOCK) ||
                        !cpd.settings[UO_indent_namespace].b)
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
            else if (is_ptype(pc, CT_EXTERN) &&
                     !cpd.settings[UO_indent_extern].b)
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
               if (cpd.settings[UO_indent_token_after_brace].b)
               {
                  frm.pse[frm.pse_tos].indent = next->column;
               }
            }
            frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos].indent;
            frm.pse[frm.pse_tos].open_line  = pc->orig_line;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

            /* Update the indent_column if needed */
            if (brace_indent || (parent_token_indent != 0))
            {
               indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
               LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                       __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            }
         }

         /* Save the brace indent */
         frm.pse[frm.pse_tos].brace_indent = (int)indent_column;
      }
      else if (is_type(pc, CT_SQL_END))
      {
         if (frm.pse[frm.pse_tos].type == CT_SQL_BEGIN)
         {
            indent_pse_pop(frm, pc);
            frm.level--;
            indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
      }
      else if (is_type(pc, CT_SQL_EXEC))
      {
         frm.level++;
         indent_pse_push(frm, pc);
         frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent + indent_size;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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
         size_t tmp = frm.pse[frm.pse_tos].indent + cpd.settings[UO_indent_switch_case].u;

         indent_pse_push(frm, pc);

         frm.pse[frm.pse_tos].indent     = tmp;
         frm.pse[frm.pse_tos].indent_tmp = tmp - indent_size + cpd.settings[UO_indent_case_shift].u;
         frm.pse[frm.pse_tos].indent_tab = tmp;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

         /* Always set on case statements */
         indent_column_set(frm.pse[frm.pse_tos].indent_tmp);

         /* comments before 'case' need to be aligned with the 'case' */
         chunk_t *pct = pc;
         while (((pct = get_prev_nnl(pct)) != nullptr) &&
                is_cmt(pct))
         {
            chunk_t *t2 = chunk_get_prev(pct);
            if (is_nl(t2))
            {
               pct->column        = frm.pse[frm.pse_tos].indent_tmp;
               pct->column_indent = pct->column;
            }
         }
      }
      else if (is_type(pc, CT_BREAK))
      {
         prev = chunk_get_prev_ncnl(pc);
         if (is_type_and_ptype(prev, CT_BRACE_CLOSE, CT_CASE))
         {
            const chunk_t *temp = get_prev_type(pc, CT_BRACE_OPEN, (int)pc->level);
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
                   (cpd.settings[UO_indent_label].n + static_cast<int>(pc->len()) + 2 <= static_cast<int>(frm.pse[frm.pse_tos].indent)))
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
         if (cpd.settings[UO_indent_access_spec_body].b)
         {
            size_t tmp = frm.pse[frm.pse_tos].indent + indent_size;

            indent_pse_push(frm, pc);

            frm.pse[frm.pse_tos].indent     = tmp;
            frm.pse[frm.pse_tos].indent_tmp = tmp - indent_size;
            frm.pse[frm.pse_tos].indent_tab = tmp;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
      }
      else if (is_type(pc, CT_CLASS_COLON, CT_CONSTR_COLON))
      {
         /* just indent one level */
         indent_pse_push(frm, pc);
         frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent_tmp + indent_size;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
         frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos  ].indent;
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

         indent_column_set(frm.pse[frm.pse_tos].indent_tmp);

         if ((cpd.settings[UO_indent_class_colon ].b && is_type(pc, CT_CLASS_COLON )) ||
             (cpd.settings[UO_indent_constr_colon].b && is_type(pc, CT_CONSTR_COLON)) )
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
                  LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                          __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
                  indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
               }
            }
            else
            {
               if (cpd.settings[UO_indent_class_on_colon].b && is_type(pc, CT_CLASS_COLON))
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
                        LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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
            LOG_FMT(LINDENT, "%s[line %d]: %zu] indent => %zu [%s]\n",
                    __func__, __LINE__, pc->orig_line, indent_column, pc->text());
            reindent_line(pc, indent_column);
         }
         frm.pse[frm.pse_tos].indent = pc->column + pc->len();

         if (is_type(pc, CT_SQUARE_OPEN) && is_lang(cpd, LANG_D))
         {
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
         }

         if ((is_type(pc, CT_FPAREN_OPEN, CT_ANGLE_OPEN)) &&
             ((cpd.settings[UO_indent_func_call_param    ].b && (is_ptype(pc, CT_FUNC_CALL,      CT_FUNC_CALL_USER  ))) ||
              (cpd.settings[UO_indent_func_proto_param   ].b && (is_ptype(pc, CT_FUNC_PROTO,     CT_FUNC_CLASS_PROTO))) ||
              (cpd.settings[UO_indent_func_class_param   ].b && (is_ptype(pc, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO))) ||
              (cpd.settings[UO_indent_template_param     ].b && (is_ptype(pc, CT_TEMPLATE                           ))) ||
              (cpd.settings[UO_indent_func_ctor_var_param].b && (is_ptype(pc, CT_FUNC_CTOR_VAR                      ))) ||
              (cpd.settings[UO_indent_func_def_param     ].b && (is_ptype(pc, CT_FUNC_DEF)                          ))) )
         {
            /* Skip any continuation indents */
            idx = (int)frm.pse_tos - 1;
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
            if (cpd.settings[UO_indent_func_param_double].b)
            {
               frm.pse[frm.pse_tos].indent += indent_size;
            }
            frm.pse[frm.pse_tos].indent_tab = frm.pse[frm.pse_tos].indent;
         }

         else if ((is_str(pc, "(", 1) && !cpd.settings[UO_indent_paren_nl ].b) ||
                  (is_str(pc, "<", 1) && !cpd.settings[UO_indent_paren_nl ].b) || /* TODO: add indent_angle_nl? */
                  (is_str(pc, "[", 1) && !cpd.settings[UO_indent_square_nl].b) )
         {
            next = get_next_nc(pc);
            break_if(is_invalid(next));

            if (is_nl(next))
            {
               size_t sub = 1;
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
         if ((pc->ptype != CT_OC_AT) && (cpd.settings[UO_indent_continue].n != 0) && (!skipped))
         {
            frm.pse[frm.pse_tos].indent = frm.pse[frm.pse_tos - 1].indent;
            if ((pc->level == pc->brace_level) &&
                (is_type(pc, CT_FPAREN_OPEN, CT_SPAREN_OPEN)))
            {
               //frm.pse[frm.pse_tos].indent += abs(cpd.settings[UO_indent_continue].n);
               //   frm.pse[frm.pse_tos].indent      = calc_indent_continue(frm, frm.pse_tos);
               //   frm.pse[frm.pse_tos].indent_cont = true;
               if ((cpd.settings[UO_use_indent_continue_only_once].b) &&
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
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
            indent_column_set(frm.pse[frm.pse_tos].indent_tmp);
            LOG_FMT(LINDENT, "%s(%d): %zu] assign => %zu [%s]\n",
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
               if ((pc->level == pc->brace_level) &&
                   (not_type(pc, CT_ASSIGN) ||
                    ((pc->ptype != CT_FUNC_PROTO) && (pc->ptype != CT_FUNC_DEF))))
               {
                  //frm.pse[frm.pse_tos].indent += abs(cpd.settings[UO_indent_continue].n);
                  //   frm.pse[frm.pse_tos].indent      = calc_indent_continue(frm, frm.pse_tos);
                  //   frm.pse[frm.pse_tos].indent_cont = true;
                  if ((cpd.settings[UO_use_indent_continue_only_once].b) &&
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
            else if (is_nl(next) || !cpd.settings[UO_indent_align_assign].b)
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
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                    __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);
         }
      }
      else if (is_type(pc, CT_OC_SCOPE, CT_TYPEDEF))
      {
         indent_pse_push(frm, pc);
         // Issue # 405
         frm.pse[frm.pse_tos].indent     = frm.pse[frm.pse_tos-1].indent;
         frm.pse[frm.pse_tos].indent_tmp = frm.pse[frm.pse_tos  ].indent;
         LOG_FMT(LINDLINE, "%s(%d): .indent=%zu, .indent_tmp=%zu\n",
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
      if ( (cpd.settings[UO_indent_shift].b    ) &&
            not_flag(pc, PCF_IN_ENUM) &&
           (pc->ptype != CT_OPERATOR) &&
           not_type(pc, 4, CT_COMMENT,       CT_COMMENT_CPP,
                              CT_COMMENT_MULTI, CT_BRACE_OPEN) &&
           (pc->level > 0 ) &&
           (!chunk_empty(pc) ) )
      {
         bool in_shift    = false;
         bool is_operator = false;

         /* Are we in such an expression? Go both forwards and backwards. */
         chunk_t *tmp = pc;
         do // \todo DRY see below
         {
            if (is_str(tmp, "<<", 2) ||
                is_str(tmp, ">>", 2) )
            {
               in_shift = true;
               tmp = chunk_get_prev_ncnl(tmp);
               if (is_type(tmp, CT_OPERATOR)) { is_operator = true; }
               break;
            }
            tmp = chunk_get_prev_ncnl(tmp);
         } while ((in_shift == false) &&
               not_type(tmp, 6, CT_BRACE_CLOSE, CT_SPAREN_CLOSE, CT_COMMA,
                     CT_SEMICOLON, CT_BRACE_OPEN,  CT_SPAREN_OPEN));

         tmp = pc;
         do
         {
            tmp = get_next_ncnl(tmp);
            if (is_str(tmp, "<<", 2) ||
                is_str(tmp, ">>", 2) )
            {
               in_shift = true;
               tmp = chunk_get_prev_ncnl(tmp);
               if (is_type(tmp, CT_OPERATOR)) { is_operator = true; }
               break;
            }
         } while ((in_shift == false) &&
                  not_type(tmp, 6, CT_BRACE_CLOSE, CT_SPAREN_CLOSE,
                        CT_SEMICOLON, CT_BRACE_OPEN, CT_COMMA, CT_SPAREN_OPEN));

         chunk_t *prev_nonl = chunk_get_prev_ncnl(pc);
         chunk_t *prev2     = chunk_get_prev_nc  (pc);

         if (is_type(prev_nonl, 8, CT_SEMICOLON,  CT_VBRACE_CLOSE,
                   CT_BRACE_OPEN,  CT_VSEMICOLON, CT_BRACE_CLOSE,
                   CT_VBRACE_OPEN, CT_CASE_COLON, CT_COMMA) ||
               are_different_preproc(prev_nonl, pc) ||
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
            size_t sub             = 0;
            for (int i = (int)frm.pse_tos; i >= 0; i--)
            {
               if ( (frm.pse[i].type == CT_RETURN) ||
                    (frm.pse[i].type == CT_ASSIGN) )
               {
                  need_workaround = true;
                  sub             = (size_t)(frm.pse_tos + 1u - i); /*lint !e737 */
                  break;
               }
            }

            if (need_workaround)
            {
               shiftcontcol = calc_indent_continue(frm, frm.pse_tos - (size_t)sub);
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
         else if (cpd.settings[UO_indent_var_def_cont].b ||
                  is_nl(chunk_get_prev(pc)))
         {
            vardefcol = frm.pse[frm.pse_tos].indent + indent_size;
         }
         else
         {
            vardefcol = pc->column;
            /* need to skip backward over any '*' */
            chunk_t *tmp = chunk_get_prev_nc(pc);
            while (is_type(tmp, CT_PTR_TYPE))
            {
               assert(is_valid(tmp));
               vardefcol = tmp->column;
               tmp       = chunk_get_prev_nc(tmp);
            }
         }
      }
      if (is_semicolon(pc) || is_type_and_ptype(pc, CT_BRACE_OPEN, CT_FUNCTION))
      {
         vardefcol = 0;
      }

      /* Indent the line if needed */
      if ((my_did_newline       == true ) &&
          (is_nl(pc) == false) &&
          (pc->len()      != 0   ) )
      {
         pc->column_indent = frm.pse[frm.pse_tos].indent_tab;

         if (frm.pse[frm.pse_tos].ip.ref)
         {
            pc->indent.ref   = frm.pse[frm.pse_tos].ip.ref;
            pc->indent.delta = frm.pse[frm.pse_tos].ip.delta;
         }

         LOG_FMT(LINDENT2, "%s(%d): %zu] %zu/%zu for %s\n",
                 __func__, __LINE__, pc->orig_line, pc->column_indent, indent_column, pc->text());

         /* Check for special continuations.
          * Note that some of these could be done as a stack item like
          * everything else */
         prev = chunk_get_prev_ncnl(pc);
         next = get_next_ncnl(pc);

         bool do_vardefcol = false;
         if ((vardefcol  > 0              ) &&
             (pc->level == pc->brace_level) &&
             is_type(prev, CT_COMMA, CT_TYPE, CT_PTR_TYPE, CT_WORD))
         {
            chunk_t *tmp = pc;
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
         else if ((pc->ptype == CT_SQL_EXEC) &&
                  cpd.settings[UO_indent_preserve_sql].b)
         {
            reindent_line(pc, sql_col + (pc->orig_col - sql_orig_col));
            LOG_FMT(LINDENT, "Indent SQL: [%s] to %zu (%zu/%zu)\n",
                                pc->text(), pc->column, sql_col, sql_orig_col);
         }
         else if (not_flag(pc, PCF_STMT_START)           &&
                  (is_type(pc,   CT_MEMBER, CT_DC_MEMBER) ||
                   is_type(prev, CT_MEMBER, CT_DC_MEMBER) ) )
         {
            size_t tmp = cpd.settings[UO_indent_member].u + indent_column;

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
         else if (is_type(pc, CT_NAMESPACE)                   &&
                  cpd.settings[UO_indent_namespace              ].b &&
                  cpd.settings[UO_indent_namespace_single_indent].b &&
                  frm.pse[frm.pse_tos].ns_cnt)
         {
            log_and_reindent(pc, frm.pse[frm.pse_tos].brace_indent, "Namespace");
         }
         else if (are_types(pc, prev, CT_STRING) &&
                  cpd.settings[UO_indent_align_string].b)
         {
            size_t tmp = (xml_indent != 0) ? (size_t)xml_indent : prev->column;
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
            LOG_FMT(LINDLINE, "%s(%d): indent_column is %zu\n",
                    __func__, __LINE__, indent_column);
            if (frm.pse[frm.pse_tos + 1].type == get_inverse_type(pc->type) )
            {
               // Issue # 405
               LOG_FMT(LINDLINE, "%s(%d): [%zu:%zu] [%s:%s]\n",
                       __func__, __LINE__, pc->orig_line, pc->orig_col, pc->text(), get_token_name(pc->type));
               chunk_t *ck1 = frm.pse[frm.pse_tos + 1].pc;
               chunk_t *ck2 = chunk_get_prev(ck1);

               assert(is_valid(ck2));
               /* If the open paren was the first thing on the line or we are
                * doing mode 1, then put the close paren in the same column */
               if (is_nl(ck2) ||
                   (cpd.settings[UO_indent_paren_close].u == 1))
               {
                  LOG_FMT(LINDLINE, "%s(%d): [%zu:%zu] indent_paren_close is 1\n",
                          __func__, __LINE__, ck2->orig_line, ck2->orig_col);
                  indent_column_set(ck1->column);
                  LOG_FMT(LINDLINE, "%s(%d): [%zu:%zu] indent_column set to %zu\n",
                          __func__, __LINE__, ck2->orig_line, ck2->orig_col, indent_column);
               }
               else
               {
                  if (cpd.settings[UO_indent_paren_close].u != 2)
                  {
                     // 0 or 1
                     LOG_FMT(LINDLINE, "%s(%d): [%zu:%zu] indent_paren_close is 0 or 1\n",
                             __func__, __LINE__, ck2->orig_line, ck2->orig_col);
                     indent_column_set(frm.pse[frm.pse_tos + 1].indent_tmp);

                     LOG_FMT(LINDLINE, "%s(%d): [%zu:%zu] indent_column set to %zu\n",
                             __func__, __LINE__, ck2->orig_line, ck2->orig_col, indent_column);
                     pc->column_indent = frm.pse[frm.pse_tos + 1].indent_tab;
                     if (cpd.settings[UO_indent_paren_close].u == 1)
                     {
                        LOG_FMT(LINDLINE, "%s(%d): [%zu:%zu] indent_paren_close is 1\n",
                                __func__, __LINE__, ck2->orig_line, ck2->orig_col);
                        indent_column--;
                        LOG_FMT(LINDLINE, "%s(%d): [%zu:%zu] indent_column set to %zu\n",
                                __func__, __LINE__, ck2->orig_line, ck2->orig_col, indent_column);
                     }
                  }
                  else
                  {
                     // 2
                     LOG_FMT(LINDLINE, "%s(%d): [%zu:%zu] indent_paren_close is 2\n",
                             __func__, __LINE__, ck2->orig_line, ck2->orig_col);
                  }
               }
            }

            log_and_reindent(pc, indent_column, "cl_paren");
         }
         else if (is_type(pc, CT_COMMA))
         {
            if (cpd.settings[UO_indent_comma_paren].b &&
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
            if (cpd.settings[UO_indent_bool_paren].b &&
                is_paren_open(frm.pse[frm.pse_tos].pc))
            {
               indent_column_set(frm.pse[frm.pse_tos].pc->column);
               if (cpd.settings[UO_indent_first_bool_expr].b)
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
            chunk_t *tmp = get_prev_type(prev, CT_QUESTION, -1);
            tmp = get_next_ncnl(tmp);
            assert(is_valid(tmp));
            log_and_reindent(pc, tmp->column, "ternarydefcol");
         }
         else if ((cpd.settings[UO_indent_ternary_operator].u == 2) &&
                  is_type(pc, CT_COND_COLON))
         {
            const chunk_t *tmp = get_prev_type(pc, CT_QUESTION, -1);
            assert(is_valid(tmp));
            log_and_reindent(pc, tmp->column, "ternarydefcol");
         }
         else
         {
            bool   use_ident = true;
            size_t ttidx     = frm.pse_tos;
            if (ttidx > 0)
            {
               //if (strcasecmp(get_token_name(frm.pse[ttidx].pc->parent_type), "FUNC_CALL") == 0)
               if ((frm.pse[ttidx].pc)->ptype == CT_FUNC_CALL)
               {
                  LOG_FMT(LINDPC, "FUNC_CALL OK [%d]\n", __LINE__);

                  use_ident = cpd.settings[UO_use_indent_func_call_param].b;
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
                  LOG_FMT(LINDENT, "%s(%d): %zu] don't indent this line [%d]\n",
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
               size_t tmp = indent_column;
               if (cpd.settings[UO_indent_var_def_blk].n > 0)
               {
                  tmp = cpd.settings[UO_indent_var_def_blk].u;
               }
               else
               {
                  tmp += cpd.settings[UO_indent_var_def_blk].u;
               }
               reindent_line(pc, tmp);
               LOG_FMT(LINDENT, "%s(%d): %zu] var_type indent => %zu [%s]\n",
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
         LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
                 __func__, __LINE__, frm.pse_tos, frm.pse[frm.pse_tos].indent_tmp);

         /* Handle the case of a multi-line #define w/o anything on the
          * first line (indent_tmp will be 1 or 0) */
         if (is_type(pc, CT_NL_CONT) &&
             (frm.pse[frm.pse_tos].indent_tmp <= indent_size))
         {
            frm.pse[frm.pse_tos].indent_tmp = indent_size + 1;
            LOG_FMT(LINDLINE, "%s(%d): frm.pse_tos=%zu, ... indent_tmp=%zu\n",
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
                  xml_indent = (int)pc->column;
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

   for (size_t idx_temp = 1; idx_temp <= frm.pse_tos; idx_temp++)
   {
      LOG_FMT(LWARN, "%s:%zu Unmatched %s\n", cpd.filename,
            frm.pse[idx_temp].open_line, get_token_name(frm.pse[idx_temp].type));
      cpd.error_count++;
   }

   quick_align_again();
   quick_indent_again();
}


void log_and_reindent(chunk_t *pc, const size_t val, const char* str)
{
   LOG_FMT(LINDENT, "%s(%d): %zu] %s => %zu\n",
           __func__, __LINE__, pc->orig_line, str, val);
   reindent_line(pc, val);
}


void log_and_indent_comment(chunk_t *pc, const size_t val, const char* str)
{
   LOG_FMT(LINDENT, "%s(%d): %zu] %s => %zu\n",
           __func__, __LINE__, pc->orig_line, str, val);
   indent_comment(pc, val);
}


static bool single_line_comment_indent_rule_applies(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   retval_if(!is_single_line_cmt(start), false);

   /* scan forward, if only single newlines and comments before next line of
    * code, we want to apply */
   chunk_t *pc = start;
   size_t nl_count = 0;
   while ((pc = chunk_get_next(pc)) != nullptr)
   {
      if (is_nl(pc))
      {
         if ((nl_count > 0) || (pc->nl_count > 1))
         {
            return(false);
         }
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
            if (is_cmt(pc) || is_closing_brace(pc))
            {
               return(false);
            }
            return(true);
         }
      }
   }

   return(false);
}


static void indent_comment(chunk_t *pc, size_t col)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LCMTIND, "%s(%d): orig_line %zu, orig_col %zu, level %zu: ",
           __func__, __LINE__, pc->orig_line, pc->orig_col, pc->level);

   /* force column 1 comment to column 1 if not changing them */
   if ((pc->orig_col == 1) && !cpd.settings[UO_indent_col1_comment].b &&
       not_flag(pc, PCF_INSERTED))
   {
      LOG_FMT(LCMTIND, "rule 1 - keep in col 1\n");
      reindent_line(pc, 1);
      return;
   }

   chunk_t *nl = chunk_get_prev(pc);

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

   chunk_t *prev = chunk_get_prev(nl);
   if((is_cmt(prev)) && (is_valid(nl)) && (nl->nl_count == 1))
   {
      assert(is_valid(prev));
      int     coldiff = (int)prev->orig_col - (int)pc->orig_col;
      chunk_t *pp     = chunk_get_prev(prev);

      /* Here we want to align comments that are relatively close one to another
       * but not when the previous comment is on the same line with a preproc */
      if ((coldiff <=  3      ) &&
          (coldiff >= -3      ) &&
          !is_preproc(pp) )
      {
         reindent_line(pc, prev->column);
         LOG_FMT(LCMTIND, "rule 3 - prev comment, coldiff = %d, now in %zu\n",
                 coldiff, pc->column);
         return;
      }
   }

   /* check if special single line comment rule applies */
   if ((cpd.settings[UO_indent_sing_line_comments].u > 0) &&
       single_line_comment_indent_rule_applies(pc))
   {
      reindent_line(pc, col + cpd.settings[UO_indent_sing_line_comments].u);
      LOG_FMT(LCMTIND, "rule 4 - single line comment indent, now in %zu\n", pc->column);
      return;
   }
   LOG_FMT(LCMTIND, "rule 5 - fall-through, stay in %zu\n", col);

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

   size_t        stage   = 0;
   chunk_t       *end_pp = nullptr;
   const chunk_t *next;
   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if(is_cmt_or_nl(pc));

      if (stage == 0)
      {
         /* Check the first PP, make sure it is an #if type */
         break_if(not_type(pc, CT_PREPROC));
         next = chunk_get_next(pc);
         break_if(is_invalid_or_not_type(next, CT_PP_IF));
         stage = 1;
      }
      else if (stage == 1)
      {
         /* Scan until a PP at level 0 is found - the close to the #if */
         if (is_type(pc, CT_PREPROC) &&
             (pc->pp_level == 0))
         {
            stage  = 2;
            end_pp = pc;
         }
         continue;
      }
      else if (stage == 2)
      {
         /* We should only see the rest of the preprocessor */
         if (is_type (pc, CT_PREPROC) || !is_preproc(pc))
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
   chunk_t *next;
   int     pp_level;
   int     pp_level_sub = 0;

   /* Scan to see if the whole file is covered by one #ifdef */
   if (ifdef_over_whole_file())
   {
      pp_level_sub = 1;
   }

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      continue_if(not_type(pc, CT_PREPROC));

      next = get_next_ncnl(pc);
      break_if(is_invalid(next));

      pp_level = (int)pc->pp_level - pp_level_sub;
      pp_level = max(pp_level, 0);

      /* Adjust the indent of the '#' */
      if (is_option_set(cpd.settings[UO_pp_indent].a, AV_ADD))
      {
         reindent_line(pc, 1 + (size_t)pp_level * cpd.settings[UO_pp_indent_count].u);
      }
      else if (is_option_set(cpd.settings[UO_pp_indent].a, AV_REMOVE))
      {
         reindent_line(pc, 1);
      }

      /* Add spacing by adjusting the length */
      if ((cpd.settings[UO_pp_space].a != AV_IGNORE) &&  (is_valid(next)))
      {
         if (is_option_set(cpd.settings[UO_pp_space].a, AV_ADD))
         {
            size_t mult = cpd.settings[UO_pp_space_count].u;
            mult = max(mult, (size_t)1u);

            reindent_line(next, (size_t)((int)pc->column + (int)pc->len() + (pp_level * (int)mult)));
         }
         else if (is_option_set(cpd.settings[UO_pp_space].a, AV_REMOVE))
         {
            reindent_line(next, pc->column + pc->len());
         }
      }

      /* Mark as already handled if not region stuff or in column 1 */
      if ((!cpd.settings[UO_pp_indent_at_level].b ||
           (pc->brace_level <= ((pc->ptype == CT_PP_DEFINE) ? 1 : 0))) &&
           (pc->ptype != CT_PP_REGION   ) &&
           (pc->ptype != CT_PP_ENDREGION) )
      {
         if (!cpd.settings[UO_pp_define_at_level].b ||
#if 1
             (pc->ptype != CT_PP_DEFINE))
#else
//           error
             is_not_ptype(pc, CT_PP_DEFINE))
#endif
         {
            set_flags(pc, PCF_DONT_INDENT);
         }
      }

      LOG_FMT(LPPIS, "%s(%d): orig_line %zu to %d (len %zu, next->col %d)\n",
              __func__, __LINE__, pc->orig_line, 1 + pp_level, pc->len(),
              next ? (int)next->column : -1);
   }
}
