/**
 * @file align.cpp
 * Does all the aligning stuff.
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */
#include "align.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "ChunkStack.h"
#include "align_stack.h"
#include <cstdio>
#include <cstdlib>
#include "unc_ctype.h"
#include "uncrustify.h"
#include "indent.h"
#include "space.h"
#include "tabulator.h"


 /**  */
enum class comment_align_e : unsigned int
{
   REGULAR, /**<  */
   BRACE,   /**<  */
   ENDIF    /**<  */
};


/*
 *   Here are the items aligned:
 *
 *   - enum value assignments
 *     enum {
 *        cat  = 1,
 *        fred = 2,
 *     };
 *
 *   - struct/union variable & bit definitions
 *     struct foo {
 *        char cat;
 *        int  id       : 5;
 *        int  name_len : 6;
 *        int  height   : 12;
 *     };
 *
 *   - variable definitions & assignments in normal code
 *     const char *cat = "feline";
 *     int        id   = 4;
 *     a   = 5;
 *     bat = 14;
 *
 *   - simple array initializers
 *     int a[] = {
 *        1, 2, 3, 4, 5,
 *        6, 7, 8, 9, 10
 *     };
 *
 *   - c99 array initializers
 *     const char *name[] = {
 *        [FRED]  = "fred",
 *        [JOE]   = "joe",
 *        [PETER] = "peter",
 *     };
 *     struct foo b[] = {
 *        { .id = 1,   .name = "text 1" },
 *        { .id = 567, .name = "text 2" },
 *     };
 *     struct foo_t bars[] =
 *     {
 *        [0] = { .name = "bar",
 *                .age  = 21 },
 *        [1] = { .name = "barley",
 *                .age  = 55 },
 *     };
 *
 *   - compact array initializers
 *     struct foo b[] = {
 *        { 3, "dog" },      { 6, "spider" },
 *        { 8, "elephant" }, { 3, "cat" },
 *     };
 *
 *   - multiline array initializers (2nd line indented, not aligned)
 *     struct foo b[] = {
 *        { AD_NOT_ALLOWED, "Sorry, you failed to guess the password.",
 *          "Try again?", "Yes", "No" },
 *        { AD_SW_ERROR,    "A software error has occured.", "Bye!", nullptr, nullptr },
 *     };
 *
 *   - Trailing comments
 *
 *   - Back-slash newline groups
 *
 *   - Function prototypes
 *     int  foo();
 *     void bar();
 *
 *   - Preprocessors
 *     #define FOO_VAL        15
 *     #define MAX_TIMEOUT    60
 *     #define FOO(x)         ((x) * 65)
 *
 *   - typedefs
 *     typedef uint8_t     BYTE;
 *     typedef int32_t     int32_t;
 *     typedef uint32_t    uint32_t;
 */


/**
 * Aligns everything in the chunk stack to a particular column.
 * The stack is empty after this function.
 */
static void align_stack(
   ChunkStack &cs,          /**< [in]  */
   size_t     col,          /**< [in] the column */
   bool       align_single, /**< [in] indicates if to align even if there is only one item on the stack */
   log_sev_t  sev           /**< [in]  */
);


/**
 * Adds an item to the align stack and adjust the nl_count and max_col.
 * Adjust max_col as needed
 */
static void align_add(
   ChunkStack &cs,      /**< [in]  */
   chunk_t    *pc,      /**< [in] the item to add */
   size_t     &max_col, /**< [in] pointer to the column variable */
   size_t     min_pad,  /**< [in]  */
   bool       squeeze   /**< [in]  */
);


/**
 * Scan everything at the current level until the close brace and find the
 * variable def align column.  Also aligns bit-colons, but that assumes that
 * bit-types are the same! But that should always be the case...
 */
static chunk_t *align_var_def_brace(
   chunk_t *pc,       /**< [in]  */
   size_t  span,      /**< [in]  */
   size_t  *nl_count  /**< [in]  */
);


static comment_align_e get_comment_align_type(
   chunk_t *cmt  /**< [in]  */
);


/**
 * For a series of lines ending in a comment, align them.
 * The series ends when more than align_right_cmt_span newlines are found.
 *
 * Interesting info:
 *  - least physically allowed column
 *  - intended column
 *  - least original cmt column
 *
 * min_col is the minimum allowed column (based on prev token col/size)
 * cmt_col less than
 *
 * @param start   Start point
 * @return        pointer the last item looked at
 */
static chunk_t *align_trailing_comments(
   chunk_t *start  /**< [in]  */
);


/**
 * Shifts out all columns by a certain amount.
 *
 * @param idx  The index to start shifting
 * @param num  The number of columns to shift
 */
static void ib_shift_out(
   size_t idx,  /**< [in]  */
   size_t num   /**< [in]  */
);


/**
 * If sq_open is CT_SQUARE_OPEN and the matching close is followed by '=',
 * then return the chunk after the '='.  Otherwise, return nullptr.
 */
static chunk_t *skip_c99_array(
   chunk_t *sq_open  /**< [in]  */
);


/**
 * Scans a line for stuff to align on.
 *
 * We trigger on BRACE_OPEN, FPAREN_OPEN, ASSIGN, and COMMA.
 * We want to align the NEXT item.
 */
static chunk_t *scan_ib_line(
   chunk_t *start,     /**< [in]  */
   bool    first_pass  /**< [in]  */
);


/**
 * tbd
 */
static void align_log_al(
   log_sev_t sev,  /**< [in]  */
   size_t    line  /**< [in]  */
);


/**
 * Generically aligns on '=', '{', '(' and item after ','
 * It scans the first line and picks up the location of those tags.
 * It then scans subsequent lines and adjusts the column.
 * Finally it does a second pass to align everything.
 *
 * Aligns all the '=' signs in structure assignments.
 * a = {
 *    .a    = 1;
 *    .type = fast;
 * };
 *
 * And aligns on '{', numbers, strings, words.
 * colors[] = {
 *    {"red",   {255, 0,   0}}, {"blue",   {  0, 255, 0}},
 *    {"green", {  0, 0, 255}}, {"purple", {255, 255, 0}},
 * };
 *
 * For the C99 indexed array assignment, the leading []= is skipped (no aligning)
 * struct foo_t bars[] =
 * {
 *    [0] = { .name = "bar",
 *            .age  = 21 },
 *    [1] = { .name = "barley",
 *            .age  = 55 },
 * };
 *
 * NOTE: this assumes that spacing is at the minimum correct spacing (ie force)
 *       if it isn't, some extra spaces will be inserted.
 *
 * @param start   Points to the open brace chunk
 */
static void align_init_brace(
   chunk_t *start  /**< [in]  */
);


/**  */
static void align_func_params(void);


/**  */
static void align_params(
   chunk_t          *start,  /**< [in]  */
   deque<chunk_t *> &chunks  /**< [in]  */
);


/**  */
static void align_same_func_call_params(void);


/**  */
static chunk_t *step_back_over_member(
   chunk_t *pc  /**< [in]  */
);


/**
 * Aligns all function prototypes in the file.
 */
static void align_func_proto(
   size_t span  /**< [in]  */
);


/**
 * Aligns all function prototypes in the file.
 */
static void align_oc_msg_spec(
   size_t span  /**< [in]  */
);


/**
 * tbd
 */
static chunk_t *align_func_param(
   chunk_t *start  /**< [in]  */
);


/**
 * Aligns simple typedefs that are contained on a single line each.
 * This should be called after the typedef target is marked as a type.
 *
 * typedef int        foo_t;
 * typedef char       bar_t;
 * typedef const char cc_t;
 */
static void align_typedefs(
   size_t span  /**< [in]  */
);


/**
 * Align '<<' (CT_ARITH?)
 */
static void align_left_shift(void);


/**
 * Aligns OC messages
 */
static void align_oc_msg_colons(void);


/**
 * Aligns an OC message
 *
 * @param so   the square open of the message
 */
static void align_oc_msg_colon(
   chunk_t *so  /**< [in]  */
);


/**
 * Aligns OC declarations on the colon
 * -(void) doSomething: (NSString*) param1
 *                with: (NSString*) param2
 */
static void align_oc_decl_colon(void);


/**
 * Aligns asm declarations on the colon
 * asm volatile (
 *    "xxx"
 *    : "x"(h),
 *      "y"(l),
 *    : "z"(h)
 *    );
 */
static void align_asm_colon(void);


static void align_stack(ChunkStack &cs, size_t col, bool align_single, log_sev_t sev)
{
   LOG_FUNC_ENTRY();

   if (is_true(UO_align_on_tabstop)) { col = align_tab_column(col); }

   if ( (cs.Len()  > 1) ||
       ((cs.Len() == 1) && align_single))
   {
      LOG_FMT(sev, "%s: max_col=%zu\n", __func__, col);
      chunk_t *pc;
      while ((pc = cs.Pop_Back()) != nullptr)
      {
         align_to_column(pc, col);
         set_flags(pc, PCF_WAS_ALIGNED);

         LOG_FMT(sev, "%s: indented [%s] on line %zu to %zu\n",
                 __func__, pc->text(), pc->orig_line, pc->column);
      }
   }
   cs.Reset();
}


static void align_add(ChunkStack &cs, chunk_t *pc, size_t &max_col, size_t min_pad, bool squeeze)
{
   LOG_FUNC_ENTRY();

   size_t  min_col;
   chunk_t *prev = chunk_get_prev(pc);
   if (is_invalid (prev) || is_nl(prev) )
   {
      min_col = squeeze ? 1 : pc->column;
      LOG_FMT(LALADD, "%s: pc->orig_line=%zu, pc->col=%zu max_col=%zu min_pad=%zu min_col=%zu\n",
              __func__, pc->orig_line, pc->column, max_col, min_pad, min_col);
   }
   else
   {
      if (is_type(prev, CT_COMMENT_MULTI)) { min_col = prev->orig_col_end +               min_pad; }
      else                                 { min_col = prev->column       + prev->len() + min_pad; }

      if (squeeze == false) { min_col = max(min_col, pc->column); }

      const bool  is_multi = is_type(prev, CT_COMMENT_MULTI);
      const char* type = (is_multi) ? "Y" : "N";
      const size_t col = (is_multi) ? prev->orig_col_end : (uint32_t)prev->column;

      LOG_FMT(LALADD, "%s: pc->orig_line=%zu, pc->col=%zu max_col=%zu min_pad=%zu \
            min_col=%zu multi:%s prev->col=%u prev->len()=%zu %s\n",
              __func__, pc->orig_line, pc->column, max_col, min_pad, min_col,
              type, col, prev->len(), get_token_name(prev->type));
   }

   if (cs.Empty()) { max_col = 0; }

   cs.Push_Back(pc);
   max_col = max(min_col, max_col);
}


void quick_align_again(void)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LALAGAIN, "%s:\n", __func__);
   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if (is_valid(pc->align.next     ) &&
          is_flag (pc, PCF_ALIGN_START) )
      {
         AlignStack as;
         as.Start(100, 0);
         as.m_right_align = pc->align.right_align;
         as.m_star_style  = static_cast<StarStyle_t>(pc->align.star_style);
         as.m_amp_style   = static_cast<StarStyle_t>(pc->align.amp_style );
         as.m_gap         = pc->align.gap;

         LOG_FMT(LALAGAIN, "   [%s:%zu]", pc->text(), pc->orig_line);
         as.Add(pc->align.start);
         set_flags(pc, PCF_WAS_ALIGNED);
         for (chunk_t *tmp = pc->align.next; is_valid(tmp); tmp = tmp->align.next)
         {
            set_flags(tmp, PCF_WAS_ALIGNED);
            as.Add(tmp->align.start);
            LOG_FMT(LALAGAIN, " => [%s:%zu]", tmp->text(), tmp->orig_line);
         }
         LOG_FMT(LALAGAIN, "\n");
         as.End();
      }
   }
}


void quick_indent_again(void)
{
   LOG_FUNC_ENTRY();

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if (pc->indent.ref)
      {
         if (is_nl(chunk_get_prev(pc)))
         {
            const size_t col = (size_t)((int)pc->indent.ref->column + pc->indent.delta);

            indent_to_column(pc, col);
            LOG_FMT(LINDENTAG, "%s: [%zu] indent [%s] to %zu based on [%s] @ %zu:%zu\n",
                    __func__, pc->orig_line, pc->text(), col,
                    pc->indent.ref->text(),
                    pc->indent.ref->orig_line, pc->indent.ref->column);
         }
      }
   }
}


void align_all(void)
{
   LOG_FUNC_ENTRY();
   if (cpd.settings[UO_align_typedef_span     ].u > 0) { align_typedefs(cpd.settings[UO_align_typedef_span].u); }
   if (is_true(UO_align_left_shift)                  ) { align_left_shift();                                    }
   if (cpd.settings[UO_align_oc_msg_colon_span].u > 0) { align_oc_msg_colons();                                 }

   /* Align variable definitions */
   if ((cpd.settings[UO_align_var_def_span   ].u > 0) ||
       (cpd.settings[UO_align_var_struct_span].u > 0) ||
       (cpd.settings[UO_align_var_class_span ].u > 0) )
   {
      align_var_def_brace(chunk_get_head(), cpd.settings[UO_align_var_def_span].u, nullptr);
   }

   /* Align assignments */
   align_assign(chunk_get_head(),
                cpd.settings[UO_align_assign_span  ].u,
                cpd.settings[UO_align_assign_thresh].u,
                nullptr);

   /* Align structure initializers */
   if (cpd.settings[UO_align_struct_init_span].u > 0)  { align_struct_initializers(); }

   /* Align function prototypes */
   if ((cpd.settings[UO_align_func_proto_span].u > 0) &&
       is_false(UO_align_mix_var_proto)) { align_func_proto(cpd.settings[UO_align_func_proto_span].u); }

   /* Align function prototypes */
   if (cpd.settings[UO_align_oc_msg_spec_span].u > 0)  { align_oc_msg_spec(cpd.settings[UO_align_oc_msg_spec_span].u); }

   /* Align OC colons */
   if (is_true(UO_align_oc_decl_colon)) { align_oc_decl_colon(); }
   if (is_true(UO_align_asm_colon    )) { align_asm_colon();     }

   /* Align variable definitions in function prototypes */
   if (is_true(UO_align_func_params          )) { align_func_params();           }
   if (is_true(UO_align_same_func_call_params)) { align_same_func_call_params(); }

   /* Just in case something was aligned out of order... do it again */
   quick_align_again();
}


static void align_oc_msg_spec(size_t span)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LALIGN, "%s\n", __func__);

   AlignStack as;
   as.Start(span, 0);

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if      (is_nl  (pc)) { as.NewLines(pc->nl_count); }
      else if (is_type(pc, CT_OC_MSG_SPEC)) { as.Add(pc); }
   }
   as.End();
}


void align_backslash_newline(void)
{
   LOG_FUNC_ENTRY();
   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      if (not_type(pc, CT_NL_CONT))
      {
         pc = get_next_type(pc, CT_NL_CONT, -1);
         continue;
      }
      pc = align_nl_cont(pc);
   }
}


void align_right_comments(void)
{
   LOG_FUNC_ENTRY();

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if(is_type(pc, CT_COMMENT, CT_COMMENT_CPP, CT_COMMENT_MULTI))
      {
         if(is_ptype(pc, CT_COMMENT_END))
         {
            bool    skip  = false;
            chunk_t *prev = chunk_get_prev(pc);
            assert(is_valid(prev));
            if (pc->orig_col < (size_t)((int)prev->orig_col_end + cpd.settings[UO_align_right_cmt_gap].n))
            {
               // note the use of -5 here (-1 would probably have worked as well) to force
               // comments which are stuck to the previous token (gap=0) into alignment with the
               // others. Not the major feature, but a nice find. (min_val/max_val in
               // options.cpp isn't validated against, it seems; well, I don't mind! :-) )
               LOG_FMT(LALTC, "NOT changing END comment on line %zu (%zu <= %u + %d)\n",
                pc->orig_line, pc->orig_col, prev->orig_col_end, cpd.settings[UO_align_right_cmt_gap].n);
               skip = true;
            }
            if (skip == false)
            {
               LOG_FMT(LALTC, "Changing END comment on line %zu into a RIGHT-comment\n",
                       pc->orig_line);
               set_flags(pc, PCF_RIGHT_COMMENT);
            }
         }

         /* Change certain WHOLE comments into RIGHT-alignable comments */
         if (is_ptype(pc, CT_COMMENT_WHOLE))
         {
            const size_t max_col = pc->column_indent + cpd.settings[UO_input_tab_size].u;

            /* If the comment is further right than the brace level... */
            if (pc->column >= max_col)
            {
               LOG_FMT(LALTC, "Changing WHOLE comment on line %zu into a RIGHT-comment (col=%zu col_ind=%zu max_col=%zu)\n",
                       pc->orig_line, pc->column, pc->column_indent, max_col);

               set_flags(pc, PCF_RIGHT_COMMENT);
            }
         }
      }
   }

   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      if (is_flag(pc, PCF_RIGHT_COMMENT)) { pc = align_trailing_comments(pc); }
      else                                { pc = chunk_get_next         (pc); }
   }
}


void align_struct_initializers(void)
{
   LOG_FUNC_ENTRY();
   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      chunk_t *prev = get_prev_ncnl(pc);
      if ( is_type(prev, CT_ASSIGN     ) &&
          (is_type(pc,   CT_BRACE_OPEN ) ||
          (is_type(pc,   CT_SQUARE_OPEN) && is_lang(cpd, LANG_D) )))
      {
         align_init_brace(pc);
      }
      pc = get_next_type(pc, CT_BRACE_OPEN, -1);
   }
}


void align_preprocessor(void)
{
   LOG_FUNC_ENTRY();

   AlignStack as;    /* value macros */
   as.Start(  cpd.settings[UO_align_pp_define_span].u);
   as.m_gap = cpd.settings[UO_align_pp_define_gap ].u;
   AlignStack *cur_as = &as;

   AlignStack asf;   /* function macros */
   asf.Start(  cpd.settings[UO_align_pp_define_span].u);
   asf.m_gap = cpd.settings[UO_align_pp_define_gap ].u;

   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      /* Note: not counting back-slash newline combos */
      if (is_type(pc, CT_NEWLINE))
      {
         as.NewLines (pc->nl_count);
         asf.NewLines(pc->nl_count);
      }

      /* If we aren't on a 'define', then skip to the next non-comment */
      if (not_type(pc, CT_PP_DEFINE))
      {
         pc = get_next_nc(pc);
         continue;
      }

      /* step past the 'define' */
      pc = get_next_nc(pc);
      break_if(is_invalid(pc));

      LOG_FMT(LALPP, "%s: define (%s) on line %zu col %zu\n",
              __func__, pc->text(), pc->orig_line, pc->orig_col);

      cur_as = &as;
      if (is_type(pc, CT_MACRO_FUNC))
      {
         if (is_false(UO_align_pp_define_together))
         {
            cur_as = &asf;
         }

         /* Skip to the close parenthesis */
         pc = get_next_nc  (pc); // point to open (
         pc = get_next_type(pc, CT_FPAREN_CLOSE, (int)pc->level);
         assert(is_valid(pc));

         LOG_FMT(LALPP, "%s: jumped to (%s) on line %zu col %zu\n",
                 __func__, pc->text(), pc->orig_line, pc->orig_col);
      }

      /* step to the value past the close parenthesis or the macro name */
      pc = chunk_get_next(pc);
      break_if(is_invalid(pc));

      /* don't align anything if the first line ends with a newline before
       * a value is given */
      if (!is_nl(pc))
      {
         LOG_FMT(LALPP, "%s: align on '%s', line %zu col %zu\n",
                 __func__, pc->text(), pc->orig_line, pc->orig_col);

         cur_as->Add(pc);
      }
   }

   as.End();
   asf.End();
}


chunk_t *align_assign(chunk_t *first, size_t span, size_t thresh, size_t *p_nl_count)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(first), first);

   const size_t my_level = first->level;
   retval_if(span == 0, chunk_get_next(first));

   LOG_FMT(LALASS, "%s[%zu]: checking %s on line %zu - span=%zu threshold=%zu\n",
           __func__, my_level, first->text(), first->orig_line, span, thresh);

   /* If we are aligning on a tabstop, we shouldn't right-align */
   AlignStack as;    // regular assigns
   as.Start(span, thresh);
   as.m_right_align = is_false(UO_align_on_tabstop);

   AlignStack vdas;  // variable def assigns
   vdas.Start(span, thresh);
   vdas.m_right_align = as.m_right_align;

   size_t  var_def_cnt = 0;
   size_t  equ_count   = 0;
   size_t  tmp;
   chunk_t *pc = first;
   while (is_valid(pc))
   {
      /* Don't check inside parenthesis or SQUARE groups */
      if (is_type(pc, CT_SPAREN_OPEN, CT_FPAREN_OPEN,
                      CT_SQUARE_OPEN, CT_PAREN_OPEN))
      {
         tmp = pc->orig_line;
         pc  = chunk_skip_to_match(pc);
         if (is_valid(pc))
         {
            as.NewLines  (pc->orig_line - tmp);
            vdas.NewLines(pc->orig_line - tmp);
         }
         continue;
      }

      /* Recurse if a brace set is found */
      if(is_type(pc, CT_BRACE_OPEN, CT_VBRACE_OPEN))
      {
         const bool is_enum     = is_ptype(pc, CT_ENUM);
         const uo_t span_type   = is_enum ? UO_align_enum_equ_span   : UO_align_assign_span;
         const uo_t thresh_type = is_enum ? UO_align_enum_equ_thresh : UO_align_assign_thresh;

         size_t myspan   = cpd.settings[span_type  ].u;
         size_t mythresh = cpd.settings[thresh_type].u;
         size_t sub_nl_count = 0;

         pc = align_assign(get_next_ncnl(pc), myspan, mythresh, &sub_nl_count);
         if (sub_nl_count > 0)
         {
            as.NewLines(sub_nl_count);
            vdas.NewLines(sub_nl_count);
            if (p_nl_count != nullptr)
            {
               *p_nl_count += sub_nl_count;
            }
         }
         continue;
      }

      /* Done with this brace set? */
      if (is_type(pc, CT_BRACE_CLOSE, CT_VBRACE_CLOSE))
      {
         pc = chunk_get_next(pc);
         break;
      }

      if (is_nl(pc))
      {
         as.NewLines  (pc->nl_count);
         vdas.NewLines(pc->nl_count);
         if (p_nl_count != nullptr)
         {
            *p_nl_count += pc->nl_count;
         }

         var_def_cnt = 0;
         equ_count   = 0;
      }
      else if (is_flag(pc, PCF_VAR_DEF))
      {
         var_def_cnt++;
      }
      else if (var_def_cnt > 1)
      {
         /* we hit the second variable def - don't look for assigns, don't align */
         vdas.Reset();
      }
      else if ((equ_count == 0             ) &&
               is_type    (pc, CT_ASSIGN   ) &&
               not_flag(pc, PCF_IN_TEMPLATE) )
      {
         equ_count++;
         if (var_def_cnt != 0) { vdas.Add(pc); }
         else                  { as.Add  (pc); }
      }
      pc = chunk_get_next(pc);
   }

   as.End();
   vdas.End();

   const bool   valid = is_valid(pc);
   const char*  str   = valid ? pc->text()    : "nullptr";
   const size_t line  = valid ? pc->orig_line : 0;
   LOG_FMT(LALASS, "%s: done on %s on line %zu\n", __func__, str, line);

   return(pc);
}


static chunk_t* align_func_param(chunk_t *start)
{
   LOG_FUNC_ENTRY();

   AlignStack as;
   as.Start(2, 0);
   as.m_star_style = static_cast<StarStyle_t>(cpd.settings[UO_align_var_def_star_style].u);
   as.m_amp_style  = static_cast<StarStyle_t>(cpd.settings[UO_align_var_def_amp_style ].u);

   bool    did_this_line = false;
   size_t  comma_count   = 0;
   size_t  chunk_count   = 0;

   chunk_t *pc = start;
   while ((pc = chunk_get_next(pc)) != nullptr)
   {
      chunk_count++;
      if (is_nl(pc))
      {
         did_this_line = false;
         comma_count   = 0;
         chunk_count   = 0;
      }
      else if (pc->level <= start->level) { break; }

      else if ((did_this_line == false ) &&
                is_flag(pc, PCF_VAR_DEF) )
      {
         if (chunk_count > 1) { as.Add(pc); }
         did_this_line = true;
      }
      else if (comma_count > 0)
      {
         if (!is_cmt(pc))
         {
            comma_count = 2;
            break;
         }
      }
      else if (is_type(pc, CT_COMMA)) { comma_count++; }
   }

   if (comma_count <= 1) { as.End(); }

   return(pc);
}


static void align_func_params(void)
{
   LOG_FUNC_ENTRY();
   chunk_t *pc = chunk_get_head();
   while ((pc = chunk_get_next(pc)) != nullptr)
   {
      continue_if(not_type (pc,    CT_FPAREN_OPEN) ||
                  not_ptype(pc, 5, CT_FUNC_PROTO, CT_FUNC_DEF,
                    CT_FUNC_CLASS_PROTO, CT_FUNC_CLASS_DEF, CT_TYPEDEF));
      /* We're on a open parenthesis of a prototype */
      pc = align_func_param(pc);
   }
}


static void align_params(chunk_t *start, deque<chunk_t *> &chunks)
{
   LOG_FUNC_ENTRY();

   chunks.clear();

   bool    hit_comma = true;
   chunk_t *pc       = get_next_type(start, CT_FPAREN_OPEN, (int)start->level);
   while ((pc = chunk_get_next(pc)) != nullptr)
   {
      break_if(is_type(pc, CT_NEWLINE, CT_NL_CONT, CT_SEMICOLON) ||
               is_type_and_level(pc, CT_FPAREN_CLOSE, start->level));

      if (pc->level == (start->level + 1))
      {
         if (hit_comma)
         {
            chunks.push_back(pc);
            hit_comma = false;
         }
         else if (is_type(pc, CT_COMMA)) { hit_comma = true; }
      }
   }
}


static void log_align(const uint32_t line, const char* str)
{
   LOG_FMT(LASFCP, "(%d) align_fnc_name [%s]\n", line, str);
}


static void align_same_func_call_params(void)
{
   LOG_FUNC_ENTRY();
   chunk_t           *pc;
   chunk_t           *align_root = nullptr;
   chunk_t           *align_cur  = nullptr;
   size_t            align_len   = 0;
   chunk_t           *align_fcn;
   unc_text          align_fcn_name;
   unc_text          align_root_name;
   deque<chunk_t *>  chunks;
   deque<AlignStack> as;
   AlignStack        fcn_as;
   const char        *add_str;

   fcn_as.Start(3);

   for (pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if (not_type(pc, CT_FUNC_CALL))
      {
         if (is_nl(pc))
         {
            for (auto &as_v : as)
            {
               as_v.NewLines(pc->nl_count);
            }
            fcn_as.NewLines(pc->nl_count);
         }
         else
         {
            /* if we drop below the brace level that started it, we are done */
            if (is_valid(align_root) &&
                (align_root->brace_level > pc->brace_level))
            {
               LOG_FMT(LASFCP, "  ++ (drop) Ended with %zu functions\n", align_len);

               /* Flush it all! */
               fcn_as.Flush();
               for (auto &as_v : as)
               {
                  as_v.Flush();
               }
               align_root = nullptr;
            }
         }
         continue;
      }

      /* Only align function calls that are right after a newline */
      chunk_t *prev = chunk_get_prev(pc);
      while(is_type(prev, CT_MEMBER, CT_DC_MEMBER))
      {
         chunk_t *tprev = chunk_get_prev(prev);
         if (not_type(tprev, CT_TYPE))
         {
            prev = tprev;
            break;
         }
         prev = chunk_get_prev(tprev);
      }
      continue_if(!is_nl(prev));

      prev      = chunk_get_next(prev);
      align_fcn = prev;
      align_fcn_name.clear();
      log_align(__LINE__, align_fcn_name.c_str());
      while (prev != pc)
      {
         log_align(__LINE__, align_fcn_name.c_str());
         assert(is_valid(prev));
         align_fcn_name += prev->str;
         log_align(__LINE__, align_fcn_name.c_str());
         prev = chunk_get_next(prev);
      }
      log_align(__LINE__, align_fcn_name.c_str());
      align_fcn_name += pc->str;
      log_align(__LINE__, align_fcn_name.c_str());
      assert(is_valid(align_fcn));
      LOG_FMT(LASFCP, "Func Call @ %zu:%zu [%s]\n", align_fcn->orig_line,
            align_fcn->orig_col, align_fcn_name.c_str());

      add_str = nullptr;
      if (is_valid(align_root))
      {
         /* can only align functions on the same brace level */
         if ((align_root->brace_level == pc->brace_level) &&
             align_fcn_name.equals(align_root_name))
         {
            fcn_as.Add(pc);
            assert(is_valid(align_cur));
            align_cur->align.next = pc;
            align_cur             = pc;
            align_len++;
            add_str = "  Add";
         }
         else
         {
            LOG_FMT(LASFCP, "  ++ Ended with %zu fcns\n", align_len);

            /* Flush it all! */
            fcn_as.Flush();
            for (auto &as_v : as)
            {
               as_v.Flush();
            }
            align_root = nullptr;
         }
      }

      if (is_invalid(align_root))
      {
         fcn_as.Add(pc);
         align_root      = align_fcn;
         align_root_name = align_fcn_name;
         align_cur       = pc;
         align_len       = 1;
         add_str         = "Start";
      }

      if (ptr_is_valid(add_str))
      {
         LOG_FMT(LASFCP, "%s '%s' on line %zu -",
                 add_str, align_fcn_name.c_str(), pc->orig_line);
         align_params(pc, chunks);
         LOG_FMT(LASFCP, " %d items:", (int)chunks.size());

         for (size_t idx = 0; idx < chunks.size(); idx++)
         {
            LOG_FMT(LASFCP, " [%s]", chunks[idx]->text());
            if (idx >= as.size())
            {
               as.resize(idx + 1);
               as[idx].Start(3);
               if (is_false(UO_align_number_left))
               {
                  if (is_type(chunks[idx], CT_NUMBER_FP, CT_POS,
                                           CT_NUMBER,    CT_NEG) )
                  {
                     as[idx].m_right_align = is_false(UO_align_on_tabstop);
                  }
               }
            }
            as[idx].Add(chunks[idx]);
         }
         LOG_FMT(LASFCP, "\n");
      }
   }

   if (align_len > 1)
   {
      LOG_FMT(LASFCP, "  ++ Ended with %zu fcns\n", align_len);
      fcn_as.End();
      for (auto &as_v : as)
      {
         as_v.End();
      }
   }
}


static chunk_t *step_back_over_member(chunk_t *pc)
{
   /* Skip over any class stuff: bool CFoo::bar() */
   chunk_t *tmp;
   while (((tmp = get_prev_ncnl(pc)) != nullptr) &&
           is_type(tmp, CT_DC_MEMBER))
   {
      pc = get_prev_ncnl(tmp);
   }
   return(pc);
}


static void align_func_proto(size_t span)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LALIGN, "%s\n", __func__);

   AlignStack as;
   as.Start(span, 0);
   as.m_gap        = cpd.settings[UO_align_func_proto_gap].u;
   as.m_star_style = static_cast<StarStyle_t>(cpd.settings[UO_align_var_def_star_style].u);
   as.m_amp_style  = static_cast<StarStyle_t>(cpd.settings[UO_align_var_def_amp_style ].u);

   AlignStack as_br;
   as_br.Start(span, 0);
   as_br.m_gap = cpd.settings[UO_align_single_line_brace_gap].u;

   bool    look_bro = false;
   chunk_t *toadd;

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if (is_nl(pc))
      {
         look_bro = false;
         as.NewLines(pc->nl_count);
         as_br.NewLines(pc->nl_count);
      }
      else if ( is_type(pc, CT_FUNC_PROTO) ||
               (is_type(pc, CT_FUNC_DEF  ) && is_true(UO_align_single_line_func)))
      {
         if (is_ptype(pc, CT_OPERATOR) && is_true(UO_align_on_operator))
         {
            toadd = get_prev_ncnl(pc);
         }
         else
         {
            toadd = pc;
         }
         as.Add(step_back_over_member(toadd));
         look_bro = (is_type(pc, CT_FUNC_DEF) && is_true(UO_align_single_line_brace));
      }
      else if ((look_bro == true) &&
               is_type(pc, CT_BRACE_OPEN) &&
               is_flag(pc, PCF_ONE_LINER) )
      {
         as_br.Add(pc);
         look_bro = false;
      }
   }
   as.End();
   as_br.End();
}


static chunk_t *align_var_def_brace(chunk_t *start, size_t span, size_t *p_nl_count)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(start), start);

   chunk_t *next;
   size_t  myspan   = span;
   size_t  mythresh = 0;
   size_t  mygap    = 0;

   /* Override the span, if this is a struct/union */
   switch(start->ptype)
   {
      case(CT_STRUCT):  /* fallthrough */
      case(CT_UNION ):  myspan   = cpd.settings[UO_align_var_struct_span  ].u;
                        mythresh = cpd.settings[UO_align_var_struct_thresh].u;
                        mygap    = cpd.settings[UO_align_var_struct_gap   ].u; break;
      case(CT_CLASS):   myspan   = cpd.settings[UO_align_var_class_span   ].u;
                        mythresh = cpd.settings[UO_align_var_class_thresh ].u;
                        mygap    = cpd.settings[UO_align_var_class_gap    ].u; break;
      default:          mythresh = cpd.settings[UO_align_var_def_thresh   ].u;
                        mygap    = cpd.settings[UO_align_var_def_gap      ].u; break;
   }

   /* can't be any variable definitions in a "= {" block */
   chunk_t *prev = get_prev_ncnl(start);
   if(is_type(prev, CT_ASSIGN))
   {
      LOG_FMT(LAVDB, "%s: start=%s [%s] on line %zu (abort due to assign)\n", __func__,
              start->text(), get_token_name(start->type), start->orig_line);

      chunk_t *pc = get_next_type(start, CT_BRACE_CLOSE, (int)start->level);
      return(get_next_ncnl(pc));
   }

   LOG_FMT(LAVDB, "%s: start=%s [%s] on line %zu\n", __func__,
           start->text(), get_token_name(start->type), start->orig_line);

   uint64_t align_mask = PCF_IN_FCN_DEF | PCF_VAR_1ST;
   if (is_false(UO_align_var_def_inline))
   {
      align_mask |= PCF_VAR_INLINE;
   }

   /* Set up the var/proto/def aligner */
   AlignStack as;
   as.Start(myspan, mythresh);
   as.m_gap        = mygap;
   as.m_star_style = static_cast<StarStyle_t>(cpd.settings[UO_align_var_def_star_style].u);
   as.m_amp_style  = static_cast<StarStyle_t>(cpd.settings[UO_align_var_def_amp_style].u);

   /* Set up the bit colon aligner */
   AlignStack as_bc;
   as_bc.Start(myspan, 0);
   as_bc.m_gap = cpd.settings[UO_align_var_def_colon_gap].u;

   AlignStack as_at; /* attribute */
   as_at.Start(myspan, 0);

   /* Set up the brace open aligner */
   AlignStack as_br;
   as_br.Start(myspan, mythresh);
   as_br.m_gap = cpd.settings[UO_align_single_line_brace_gap].u;

   bool       fp_look_bro   = false;
   bool       did_this_line = false;
   const bool fp_active     = is_true(UO_align_mix_var_proto);
   chunk_t    *pc           = chunk_get_next(start);
   while ((is_valid(pc)) &&
          ((pc->level >= start->level) || (pc->level == 0)))
   {
      LOG_FMT(LGUY, "%s: pc->text()=%s, pc->orig_line=%zu, pc->orig_col=%zu\n",
              __func__, pc->text(), pc->orig_line, pc->orig_col);
      if (is_cmt(pc))
      {
         if (pc->nl_count > 0)
         {
            as.NewLines   (pc->nl_count);
            as_bc.NewLines(pc->nl_count);
            as_at.NewLines(pc->nl_count);
            as_br.NewLines(pc->nl_count);
         }
         pc = chunk_get_next(pc);
         continue;
      }

      if ((fp_active == true) &&
         !is_flag(pc, PCF_IN_CLASS_BASE))
      {
         if (is_type(pc, CT_FUNC_PROTO, CT_FUNC_DEF) && is_true(UO_align_single_line_func))
         {
            LOG_FMT(LAVDB, "    add=[%s] line=%zu col=%zu level=%zu\n",
                    pc->text(), pc->orig_line, pc->orig_col, pc->level);

            as.Add(pc);
            fp_look_bro = (is_type(pc, CT_FUNC_DEF)) && is_true(UO_align_single_line_brace);
         }
         else if ((fp_look_bro == true     ) &&
                  is_type(pc, CT_BRACE_OPEN) &&
                  is_flag(pc, PCF_ONE_LINER) )
         {
            as_br.Add(pc);
            fp_look_bro = false;
         }
      }

      /* process nested braces */
      if (is_type(pc, CT_BRACE_OPEN))
      {
         size_t sub_nl_count = 0;

         pc = align_var_def_brace(pc, span, &sub_nl_count);
         if (sub_nl_count > 0)
         {
            fp_look_bro   = false;
            did_this_line = false;
            as.NewLines   (sub_nl_count);
            as_bc.NewLines(sub_nl_count);
            as_at.NewLines(sub_nl_count);
            as_br.NewLines(sub_nl_count);
            if (ptr_is_valid(p_nl_count))
            {
               *p_nl_count += sub_nl_count;
            }
         }
         continue;
      }

      /* Done with this brace set? */
      if (is_type(pc, CT_BRACE_CLOSE))
      {
         pc = chunk_get_next(pc);
         break;
      }

      if (is_nl(pc))
      {
         fp_look_bro   = false;
         did_this_line = false;
         as.NewLines   (pc->nl_count);
         as_bc.NewLines(pc->nl_count);
         as_at.NewLines(pc->nl_count);
         as_br.NewLines(pc->nl_count);
         if (ptr_is_valid(p_nl_count))
         {
            *p_nl_count += pc->nl_count;
         }
      }

      /* don't align stuff inside parens/squares/angles */
      if (pc->level > pc->brace_level)
      {
         pc = chunk_get_next(pc);
         continue;
      }

      /* If this is a variable def, update the max_col */
      if (!is_flag(pc, PCF_IN_CLASS_BASE) &&
          not_type(pc, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO) &&
          (get_flags(pc, align_mask) == PCF_VAR_1ST) &&
          ((pc->level == (start->level + 1)) ||
           (pc->level == 0)) &&
           not_type(pc->prev, CT_MEMBER))
      {
         if (!did_this_line)
         {
            if (is_ptype(start, CT_STRUCT) &&
                (as.m_star_style  == SS_INCLUDE) )
            {
               // we must look after the previous token
               chunk_t *prev_local = pc->prev;
               while (is_type(prev_local, CT_PTR_TYPE, CT_ADDR))
               {
                  LOG_FMT(LAVDB, "    prev_local=%s, prev_local->type=%s\n",
                          prev_local->text(), get_token_name(prev_local->type));
                  prev_local = prev_local->prev;
               }
               pc = prev_local->next;
            }
            LOG_FMT(LAVDB, "    add=[%s] line=%zu col=%zu level=%zu\n",
                    pc->text(), pc->orig_line, pc->orig_col, pc->level);

            as.Add(step_back_over_member(pc));

            if (is_true(UO_align_var_def_colon))
            {
               next = get_next_nc(pc);
               if (is_type(next, CT_BIT_COLON))
               {
                  as_bc.Add(next);
               }
            }
            if (is_true(UO_align_var_def_attribute))
            {
               next = pc;
               while ((next = get_next_nc(next)) != nullptr)
               {
                  if(is_type(next, CT_ATTRIBUTE))
                  {
                     as_at.Add(next);
                     break;
                  }
                  if (is_type   (next, CT_SEMICOLON) ||
                      is_nl(next        ) )
                  {
                     break;
                  }
               }
            }
         }
         did_this_line = true;
      }
      else if (is_type(pc, CT_BIT_COLON))
      {
         if (did_this_line == false)
         {
            as_bc.Add(pc);
            did_this_line = true;
         }
      }
      pc = chunk_get_next(pc);
   }

   as.End();
   as_bc.End();
   as_at.End();
   as_br.End();

   return(pc);
}


chunk_t *align_nl_cont(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LALNLC, "%s: start on [%s] on line %zu\n", __func__,
           get_token_name(start->type), start->orig_line);

   /* Find the max column */
   ChunkStack cs;
   size_t     max_col = 0;
   chunk_t    *pc     = start;

   while(not_type(pc, CT_NEWLINE, CT_COMMENT_MULTI) )
   {
      if (is_type(pc, CT_NL_CONT))
      {
         align_add(cs, pc, max_col, 1, true);
      }
      pc = chunk_get_next(pc);
   }

   /* NL_CONT is always the last thing on a line */
   chunk_t *tmp;
   while ((tmp = cs.Pop_Back()) != nullptr)
   {
      set_flags(tmp, (uint64_t)PCF_WAS_ALIGNED);
      tmp->column = max_col;
   }

   return(pc);
}


static comment_align_e get_comment_align_type(chunk_t *cmt)
{
   chunk_t         *prev;
   comment_align_e cmt_type = comment_align_e::REGULAR;

   if (is_false(UO_align_right_cmt_mix) &&
       ((prev = chunk_get_prev(cmt)) != nullptr))
   {
      if(is_type(prev, CT_PP_ENDIF, CT_PP_ELSE, CT_ELSE, CT_BRACE_CLOSE))
      {
         /* REVISIT: someone may want this configurable */
         if ((cmt->column - (prev->column + prev->len())) < 3)
         {
            cmt_type = (is_type(prev, CT_PP_ENDIF)) ?
                  comment_align_e::ENDIF : comment_align_e::BRACE;
         }
      }
   }
   return(cmt_type);
}


static chunk_t *align_trailing_comments(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   size_t          min_col  = 0;
   size_t          min_orig = 0;
   chunk_t         *pc      = start;
   size_t          nl_count = 0;
   ChunkStack      cs;
   size_t          col;
   const size_t         intended_col = cpd.settings[UO_align_right_cmt_at_col].u;
   comment_align_e cmt_type_cur;
   comment_align_e cmt_type_start = get_comment_align_type(pc);

   LOG_FMT(LALADD, "%s: start on line=%zu\n",
           __func__, pc->orig_line);

   /* Find the max column */
   while ((is_valid(pc)) &&
           (nl_count < cpd.settings[UO_align_right_cmt_span].u))
   {
      if (is_flag(pc, PCF_RIGHT_COMMENT) && (pc->column > 1))
      {
         cmt_type_cur = get_comment_align_type(pc);

         if (cmt_type_cur == cmt_type_start)
         {
            col = 1 + (pc->brace_level * cpd.settings[UO_indent_columns].u);
            LOG_FMT(LALADD, "%s: line=%zu col=%zu min_col=%zu pc->col=%zu pc->len=%zu %s\n",
                    __func__, pc->orig_line, col, min_col, pc->column, pc->len(),
                    get_token_name(pc->type));
            if ((min_orig == 0        ) ||
                (min_orig > pc->column) )
            {
               min_orig = pc->column;
            }

            pc->column = max(pc->column, col);
            align_add(cs, pc, min_col, 1, true); // (intended_col < col));
            nl_count = 0;
         }
      }
      if (is_nl(pc))
      {
         nl_count += pc->nl_count;
      }
      pc = chunk_get_next(pc);
   }

   /* Start with the minimum original column */
   col = min_orig;
   /* fall back to the intended column */
   if ((col > intended_col) && (intended_col > 0))
   {
      col = intended_col;
   }
   /* if less than allowed, bump it out */
   col = max(col, min_col);

   /* bump out to the intended column */
   col = max(col, intended_col);

   LOG_FMT(LALADD, "%s:  -- min_orig=%zu intended_col=%zu min_allowed=%zu ==> col=%zu\n",
           __func__, min_orig, intended_col, min_col, col);
   if ((cpd.frag_cols >  0  ) &&
       (cpd.frag_cols <= col) )
   {
      col -= cpd.frag_cols;
   }
   align_stack(cs, col, (intended_col != 0), LALTC);

   return(chunk_get_next(pc));
}


static void ib_shift_out(size_t idx, size_t num)
{
   while (idx < cpd.al_cnt)
   {
      cpd.al[idx].col += num;
      idx++;
   }
}


static chunk_t *skip_c99_array(chunk_t *sq_open)
{
   if (is_type(sq_open, CT_SQUARE_OPEN))
   {
      chunk_t *tmp = get_next_nc(chunk_skip_to_match(sq_open));
      if (is_type(tmp, CT_ASSIGN))
      {
         return(get_next_nc(tmp));
      }
   }
   return(nullptr);
}


static chunk_t *scan_ib_line(chunk_t *start, bool first_pass)
{
   UNUSED(first_pass);
   LOG_FUNC_ENTRY();

   retval_if(is_invalid(start), start);

   const chunk_t *prev_match = nullptr;
   size_t        idx         = 0;

   /* Skip past C99 "[xx] =" stuff */
   chunk_t *tmp = skip_c99_array(start);
   if (is_valid(tmp))
   {
      set_ptype(start, CT_TSQUARE);
      start            = tmp;
      cpd.al_c99_array = true;
   }

   chunk_t *pc = start;
   if (is_valid(pc))
   {
      LOG_FMT(LSIB, "%s: start=%s col %zu/%zu line %zu\n",
              __func__, get_token_name(pc->type), pc->column, pc->orig_col, pc->orig_line);
   }

   while ( is_valid  (pc) &&
          !is_nl(pc) &&
          (pc->level >= start->level))
   {
      //LOG_FMT(LSIB, "%s:     '%s'   col %d/%d line %zu\n", __func__,
      //        pc->text(), pc->column, pc->orig_col, pc->orig_line);

      chunk_t *next = chunk_get_next(pc);
      if (is_invalid(next) ||
          is_cmt(next) )
      {
         /* do nothing */
      }
      else if(is_type(pc, CT_ASSIGN, CT_BRACE_OPEN, CT_BRACE_CLOSE, CT_COMMA))
      {
         const size_t token_width = space_col_align(pc, next);

         /* Is this a new entry? */
         if (idx >= cpd.al_cnt)
         {
            LOG_FMT(LSIB, " - New   [%zu] %.2zu/%zu - %10.10s\n",
                    idx, pc->column, token_width, get_token_name(pc->type));
            cpd.al[cpd.al_cnt].type = pc->type;
            cpd.al[cpd.al_cnt].col  = pc->column;
            cpd.al[cpd.al_cnt].len  = token_width;
            cpd.al_cnt++;
            idx++;
         }
         else
         {
            /* expect to match stuff */
            if (cpd.al[idx].type == pc->type)
            {
               LOG_FMT(LSIB, " - Match [%zu] %.2zu/%zu - %10.10s",
                       idx, pc->column, token_width, get_token_name(pc->type));

               /* Shift out based on column */
               if (is_invalid(prev_match))
               {
                  if (pc->column > cpd.al[idx].col)
                  {
                     LOG_FMT(LSIB, " [ pc->col(%zu) > col(%zu) ] ",
                             pc->column, cpd.al[idx].col);

                     ib_shift_out(idx, pc->column - cpd.al[idx].col);
                     cpd.al[idx].col = pc->column;
                  }
               }
               else if (idx > 0)
               {
                  const int min_col_diff = (int)pc->column - (int)prev_match->column;
                  const int cur_col_diff = (int)cpd.al[idx].col - (int)cpd.al[idx - 1].col;
                  if (cur_col_diff < min_col_diff)
                  {
                     LOG_FMT(LSIB, " [ min_col_diff(%d) > cur_col_diff(%d) ] ",
                             min_col_diff, cur_col_diff);
                     assert(min_col_diff - cur_col_diff >= 0);  /* assure par2 is always positive */
                     ib_shift_out(idx, (size_t)(min_col_diff - cur_col_diff));
                  }
               }
               LOG_FMT(LSIB, " - now col %zu, len %zu\n", cpd.al[idx].col, cpd.al[idx].len);
               idx++;
            }
         }
         prev_match = pc;
      }
      pc = get_next_nc(pc);
   }
   return(pc);
}


static void align_log_al(log_sev_t sev, size_t line)
{
   if (log_sev_on(sev))
   {
      log_fmt(sev, "%s: line %zu, %zu)", __func__, line, cpd.al_cnt);
      for (size_t idx = 0; idx < cpd.al_cnt; idx++)
      {
         log_fmt(sev, " %zu/%zu=%s", cpd.al[idx].col, cpd.al[idx].len,
                 get_token_name(cpd.al[idx].type));
      }
      log_fmt(sev, "\n");
   }
}


static void align_init_brace(chunk_t *start)
{
   LOG_FUNC_ENTRY();

   chunk_t *num_token = nullptr;
   cpd.al_cnt         = 0;
   cpd.al_c99_array   = false;

   LOG_FMT(LALBR, "%s: start @ %zu:%zu\n", __func__, start->orig_line, start->orig_col);

   chunk_t *pc = get_next_ncnl(start);
   pc = scan_ib_line(pc, true);

   /* single line - nothing to do */
   if(is_type_and_ptype(pc, CT_BRACE_CLOSE, CT_ASSIGN)) { return; }

   do
   {
      pc = scan_ib_line(pc, false);
      assert(is_valid(pc));

      /* debug dump the current frame */
      align_log_al(LALBR, pc->orig_line);

      while (is_nl(pc))
      {
         pc = chunk_get_next(pc);
      }
   } while ((is_valid(pc)       ) &&
            (pc->level >  start->level) );

   /* debug dump the current frame */
   align_log_al(LALBR, start->orig_line);

   if (is_true(UO_align_on_tabstop ) &&
       (cpd.al_cnt >= 1            ) &&
       (cpd.al[0].type == CT_ASSIGN) )
   {
      cpd.al[0].col = align_tab_column(cpd.al[0].col);
   }

   pc = chunk_get_next(start);
   size_t idx = 0;
   do
   {
      chunk_t *tmp;
      if ((idx == 0) &&
          ((tmp = skip_c99_array(pc)) != nullptr))
      {
         pc = tmp;
         if (is_valid(pc))
         {
            LOG_FMT(LALBR, " -%zu- skipped '[] =' to %s\n",
                    pc->orig_line, get_token_name(pc->type));
         }
         continue;
      }

      assert(is_valid(pc));
      chunk_t *next = pc;
      if (idx < cpd.al_cnt)
      {
         LOG_FMT(LALBR, " (%zu) check %s vs %s -- ",
                 idx, get_token_name(pc->type), get_token_name(cpd.al[idx].type));
         if (is_type(pc, cpd.al[idx].type))
         {
            if ((idx == 0) && cpd.al_c99_array)
            {
               chunk_t *prev = chunk_get_prev(pc);
               if (is_nl(prev))
               {
                  set_flags(pc, (uint64_t)PCF_DONT_INDENT);
               }
            }
            LOG_FMT(LALBR, " [%s] to col %zu\n", pc->text(), cpd.al[idx].col);

            if (is_valid(num_token))
            {
               const int col_diff = (int)pc->column - (int)num_token->column;
               assert((int)cpd.al[idx].col - col_diff >= 0);
               reindent_line(num_token, (size_t)((int)cpd.al[idx].col - col_diff));
               //LOG_FMT(LSYS, "-= %zu =- NUM indent [%s] col=%d diff=%d\n",
               //        num_token->orig_line,
               //        num_token->text(), cpd.al[idx - 1].col, col_diff);

               set_flags(num_token, (uint64_t)PCF_WAS_ALIGNED);
               num_token = nullptr;
            }

            /* Comma's need to 'fall back' to the previous token */
            if (is_type(pc, CT_COMMA))
            {
               next = chunk_get_next(pc);
               if ((is_valid(next)) && !is_nl(next))
               {
                  //LOG_FMT(LSYS, "-= %zu =- indent [%s] col=%d len=%d\n",
                  //        next->orig_line,
                  //        next->text(), cpd.al[idx].col, cpd.al[idx].len);

                  if ((idx < (cpd.al_cnt - 1)) &&
                      is_true(UO_align_number_left) &&
                      (is_type(next, CT_NUMBER_FP, CT_NUMBER, CT_POS, CT_NEG)))
                  {
                     /* Need to wait until the next match to indent numbers */
                     num_token = next;
                  }
                  else if (idx < (cpd.al_cnt - 1))
                  {
                     reindent_line(next, cpd.al[idx].col + cpd.al[idx].len);
                     set_flags(next, (uint64_t)PCF_WAS_ALIGNED);
                  }
               }
            }
            else
            {
               /* first item on the line */
               reindent_line(pc, cpd.al[idx].col);
               set_flags(pc, (uint64_t)PCF_WAS_ALIGNED);

               /* see if we need to right-align a number */
               if ((idx < (cpd.al_cnt - 1)) && is_true(UO_align_number_left))
               {
                  next = chunk_get_next(pc);
                  if (!is_nl(next) &&
                      (is_type(next, CT_NUMBER_FP, CT_NUMBER, CT_POS, CT_NEG)))
                  {
                     /* Need to wait until the next match to indent numbers */
                     num_token = next;
                  }
               }
            }
            idx++;
         }
         else { LOG_FMT(LALBR, " no match\n"); }
      }
      if (is_nl(pc  ) ||
          is_nl(next) )
      {
         idx = 0;
      }
      pc = chunk_get_next(pc);
   } while ((is_valid(pc)       ) &&
            (pc->level >  start->level) );
}


static void align_typedefs(size_t span)
{
   LOG_FUNC_ENTRY();

   AlignStack as;
   as.Start(span);
   as.m_gap        = cpd.settings[UO_align_typedef_gap].u;
   as.m_star_style = static_cast<StarStyle_t>(cpd.settings[UO_align_typedef_star_style].u);
   as.m_amp_style  = static_cast<StarStyle_t>(cpd.settings[UO_align_typedef_amp_style ].u);

   const chunk_t *c_typedef = nullptr;
   chunk_t *pc        = chunk_get_head();
   while (is_valid(pc))
   {
      if (is_nl(pc))
      {
         as.NewLines(pc->nl_count);
         c_typedef = nullptr;
      }
      else if (is_valid(c_typedef))
      {
         if (is_flag(pc, PCF_ANCHOR))
         {
            as.Add(pc);
            LOG_FMT(LALTD, "%s: typedef @ %zu:%zu, tag '%s' @ %zu:%zu\n",
                    __func__, c_typedef->orig_line, c_typedef->orig_col,
                    pc->text(), pc->orig_line, pc->orig_col);
            c_typedef = nullptr;
         }
      }
      else
      {
         if (is_type(pc, CT_TYPEDEF)) { c_typedef = pc; }
      }

      pc = chunk_get_next(pc);
   }

   as.End();
}


static void align_left_shift(void)
{
   LOG_FUNC_ENTRY();

   const chunk_t *start = nullptr;
   AlignStack    as;
   as.Start(255);

   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      if(are_different_pp(pc, start))
      {
         /* a change in preproc status restarts the aligning */
         as.Flush();
         start = nullptr;
      }
      else if (is_nl(pc))
      {
         as.NewLines(pc->nl_count);
      }
      else if (is_valid(start) && (pc->level < start->level))
      {
         /* A drop in level restarts the aligning */
         as.Flush();
         start = nullptr;
      }
      else if (is_valid(start) && (pc->level > start->level))
      {
         /* Ignore any deeper levels when aligning */
      }
      else if (is_type(pc, CT_SEMICOLON))
      {
         /* A semicolon at the same level flushes */
         as.Flush();
         start = nullptr;
      }
      else if (not_flag(pc, PCF_IN_ENUM) && is_str(pc, "<<"))
      {
         if (is_ptype(pc, CT_OPERATOR))
         {
            /* Ignore operator<< */
         }
         else if (as.m_aligned.Empty())
         {
            /* check if the first one is actually on a blank line and then
             * indent it. Eg:
             *
             *      cout
             *          << "something"; */
            chunk_t *prev = chunk_get_prev(pc);
            if (is_nl(prev))
            {
               indent_to_column(pc, pc->column_indent + cpd.settings[UO_indent_columns].u);
               pc->column_indent = pc->column;
               set_flags(pc, PCF_DONT_INDENT);
            }

            /* first one can be anywhere */
            as.Add(pc);
            start = pc;
         }
         else if (is_nl(chunk_get_prev(pc)))
         {
            /* subsequent ones must be after a newline */
            as.Add(pc);
         }
      }
      else if (!as.m_aligned.Empty())
      {
         /* check if the given statement is on a line of its own, immediately following <<
          * and then it. Eg:
          *
          *      cout <<
          *          "something"; */
         chunk_t *prev = chunk_get_prev(pc);
         if (is_nl(prev))
         {
            indent_to_column(pc, pc->column_indent + cpd.settings[UO_indent_columns].u);
            pc->column_indent = pc->column;
            set_flags(pc, PCF_DONT_INDENT);
         }
      }

      pc = chunk_get_next(pc);
   }
   as.End();
}


static void align_oc_msg_colon(chunk_t *so)
{
   LOG_FUNC_ENTRY();

   AlignStack nas;   /* for the parameter tag */
   nas.Reset();
   nas.m_right_align = is_false(UO_align_on_tabstop);

   AlignStack cas;   /* for the colons */
   const size_t     span = cpd.settings[UO_align_oc_msg_colon_span].u;
   cas.Start(span);

   const size_t  level = so->level;
   chunk_t *pc   = get_next_ncnl(so, scope_e::PREPROC);

   bool    did_line  = false;
   bool    has_colon = false;
   size_t  lcnt      = 0; /* line count with no colon for span */

   while (is_valid(pc) && (pc->level > level))
   {
      if (pc->level > (level + 1)) { /* do nothing */ }
      else if (is_nl(pc))
      {
         if (has_colon == false)
         {
            ++lcnt;
         }
         did_line  = false;
         has_colon = !has_colon;
      }
      else if ( (did_line == false) &&
                (lcnt < span + 1  ) &&
                 is_type(pc, CT_OC_COLON))
      {
         has_colon = true;
         cas.Add(pc);
         chunk_t *tmp = chunk_get_prev(pc);
         if (is_type(tmp, CT_OC_MSG_FUNC, CT_OC_MSG_NAME))
         {
            nas.Add(tmp);
            set_flags(tmp, (uint64_t)PCF_DONT_INDENT);
         }
         did_line = true;
      }
      pc = chunk_get_next(pc, scope_e::PREPROC);
   }

   nas.m_skip_first = is_false(UO_align_oc_msg_colon_first);
   cas.m_skip_first = is_false(UO_align_oc_msg_colon_first);

   /* find the longest args that isn't the first one */
   size_t  first_len = 0;
   size_t  mlen      = 0;
   chunk_t *longest  = nullptr;

   size_t  len = nas.m_aligned.Len();
   for (size_t idx = 0; idx < len; idx++)
   {
      chunk_t *tmp = nas.m_aligned.GetChunk(idx);
      assert(is_valid(tmp));

      const size_t  tlen = tmp->str.size();
      if (tlen > mlen)
      {
         mlen = tlen;
         if (idx != 0) { longest = tmp; }
      }
      if (idx == 0) { first_len = tlen + 1; }
   }

   /* add spaces before the longest arg */
   len = cpd.settings[UO_indent_oc_msg_colon].u;
   const size_t len_diff    = mlen - first_len;
   const size_t indent_size = cpd.settings[UO_indent_columns].u;
   /* Align with first colon if possible by removing spaces */
   if (is_valid(longest) &&
       is_true(UO_indent_oc_msg_prioritize_first_colon) &&
       (len_diff > 0) &&
       ((longest->column - len_diff) > (longest->brace_level * indent_size)))
   {
      longest->column -= len_diff;
   }
   else if (longest && (len > 0))
   {
      chunk_t chunk;
      chunk.type        = CT_SPACE;
      chunk.orig_line   = longest->orig_line;
      chunk.ptype = CT_NONE;
      chunk.level       = longest->level;
      chunk.brace_level = longest->brace_level;
      set_flags(&chunk, get_flags(longest, PCF_COPY_FLAGS));

      /* start at one since we already indent for the '[' */
      for (size_t idx = 1; idx < len; idx++)
      {
         chunk.str.append(' ');
      }
      chunk_add_before(&chunk, longest);
   }
   nas.End();
   cas.End();
}


static void align_oc_msg_colons(void)
{
   LOG_FUNC_ENTRY();

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      if (is_type_and_ptype(pc, CT_SQUARE_OPEN, CT_OC_MSG))
      {
         align_oc_msg_colon(pc);
      }
   }
}


static void align_oc_decl_colon(void)
{
   LOG_FUNC_ENTRY();

   bool       did_line;
   AlignStack cas;   /* for the colons */
   AlignStack nas;   /* for the parameter label */
   cas.Start(4);
   nas.Start(4);
   nas.m_right_align = is_false(UO_align_on_tabstop);

   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      if (not_type(pc, CT_OC_SCOPE))
      {
         pc = chunk_get_next(pc);
         continue;
      }

      nas.Reset();
      cas.Reset();

      const size_t level = pc->level;
      pc = get_next_ncnl(pc, scope_e::PREPROC);

      did_line = false;

      while (is_valid(pc) &&
             (pc->level >= level))
      {
         /* The declaration ends with an open brace or semicolon */
         break_if (is_type(pc, CT_BRACE_OPEN) || is_semicolon(pc));

         if (is_nl(pc))
         {
            nas.NewLines(pc->nl_count);
            cas.NewLines(pc->nl_count);
            did_line = false;
         }
         else if ((did_line == false      ) &&
                  is_type(pc, CT_OC_COLON) )
         {
            cas.Add(pc);

            chunk_t *tmp  = chunk_get_prev(pc, scope_e::PREPROC);
            chunk_t *tmp2 = get_prev_ncnl(tmp, scope_e::PREPROC);

            /* Check for an un-labeled parameter */
            if (is_type(tmp,  CT_WORD, CT_TYPE, CT_OC_MSG_DECL, CT_OC_MSG_SPEC) &&
                is_type(tmp2, CT_WORD, CT_TYPE, CT_PAREN_CLOSE                ) )
            {
               nas.Add(tmp);
            }
            did_line = true;
         }
         pc = chunk_get_next(pc, scope_e::PREPROC);
      }
      nas.End();
      cas.End();
   }
}


static void align_asm_colon(void)
{
   LOG_FUNC_ENTRY();

   bool       did_nl;
   AlignStack cas;   /* for the colons */
   cas.Start(4);

   chunk_t *pc = chunk_get_head();
   while (is_valid(pc))
   {
      if (not_type(pc, CT_ASM_COLON))
      {
         pc = chunk_get_next(pc);
         continue;
      }

      cas.Reset();

      pc = get_next_ncnl(pc, scope_e::PREPROC);
      const size_t level = pc ? pc->level : 0;
      did_nl = true;
      while (is_valid(pc) &&
             (pc->level >= level))
      {
         if (is_nl(pc))
         {
            cas.NewLines(pc->nl_count);
            did_nl = true;
         }
         else if (is_type(pc, CT_ASM_COLON))
         {
            cas.Flush();
            did_nl = true;
         }
         else if (did_nl == true)
         {
            did_nl = false;
            cas.Add(pc);
         }
         pc = get_next_nc(pc, scope_e::PREPROC);
      }
      cas.End();
   }
}
