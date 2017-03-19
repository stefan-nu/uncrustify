/**
 * @file output.cpp
 * Does all the output & comment formatting.
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */
#include "output.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include "uncrustify.h"
#include "indent.h"
#include "braces.h"
#include "unicode.h"
#include "tabulator.h"
#include <cstdlib>


/** tbd */
struct cmt_reflow_t
{
   chunk_t  *pc;         /**< tbd  */
   size_t   column;      /**< Column of the comment start */
   size_t   brace_col;   /**< Brace column (for indenting with tabs) */
   size_t   base_col;    /**< Base column (for indenting with tabs) */
   size_t   word_count;  /**< number of words on this line */
   size_t   xtra_indent; /**< extra indent of non-first lines (0 or 1) */
   unc_text cont_text;   /**< fixed text to output at the start of a line (0 to 3 chars) */
   bool     reflow;      /**< reflow the current line */
};


/***************************************************************************//**
 * @brief prototype for a function that operates on a keyword
 *
 * @note this typedef defines the function type "kw_func_t"
 * for a function pointer of type
 * bool *function(chunk_t *cmt, unc_text &out_txt)
 ******************************************************************************/
typedef bool (*kw_func_t)(chunk_t *cmt, unc_text &out_txt);


struct kw_subst_t
{
   const char *tag; /**<   */
   kw_func_t  func; /**<   */
};


/**
 * A multiline comment
 * The only trick here is that we have to trim out whitespace characters
 * to get the comment to line up.
 */
static void output_comment_multi(
   chunk_t *pc  /**< [in]  */
);


/**
 * tbd
 */
static bool kw_fcn_filename(
   chunk_t  *cmt,     /**< [in]  */
   unc_text &out_txt  /**< [in]  */
);


/**
 * tbd
 */
static bool kw_fcn_class(
   chunk_t  *cmt,     /**< [in]  */
   unc_text &out_txt  /**< [in]  */
);


/**
 * tbd
 */
static bool kw_fcn_message(
   chunk_t  *cmt,    /**< [in]  */
   unc_text &out_txt /**< [in]  */
);


/**
 * tbd
 */
static bool kw_fcn_category(
   chunk_t  *cmt,     /**< [in]  */
   unc_text &out_txt  /**< [in]  */
);


/**
 * tbd
 */
static bool kw_fcn_scope(
   chunk_t  *cmt,    /**< [in]  */
   unc_text &out_txt /**< [in]  */
);


/**
 * tbd
 */
static bool kw_fcn_function(
   chunk_t  *cmt,    /**< [in]  */
   unc_text &out_txt /**< [in]  */
);


/**
 * Adds the javadoc-style @param and @return stuff, based on the params and
 * return value for pc.
 * If the arg list is '()' or '(void)', then no @params are added.
 * Likewise, if the return value is 'void', then no @return is added.
 */
static bool kw_fcn_javaparam(
   chunk_t  *cmt,    /**< [in]  */
   unc_text &out_txt /**< [in]  */
);


/**
 * tbd
 */
static bool kw_fcn_fclass(
   chunk_t  *cmt,    /**< [in]  */
   unc_text &out_txt /**< [in]  */
);


/**
 * Output a multiline comment without any reformatting other than shifting
 * it left or right to get the column right.
 * Trim trailing whitespace and do keyword substitution.
 */
static void output_comment_multi_simple(
   chunk_t *pc,      /**< [in]  */
   bool    kw_subst  /**< [in]  */
);


/**
 * This renders the #if condition to a string buffer.
 */
static void generate_if_conditional_as_text(
   unc_text &dst,    /**< [out] unc_text buffer to be filled */
   chunk_t  *ifdef   /**< [in]  if conditional as chunk list */
);


/**
 * Do keyword substitution on a comment.
 * NOTE: it is assumed that a comment will contain at most one of each type
 * of keyword.
 */
static void do_keyword_substitution(
   chunk_t *pc /**< [in]  */
);


/**
 * All output text is sent here, one char at a time.
 */
static void add_char(
   uint32_t ch /**< [in]  */
);


/**
 * tbd
 */
static void add_text(
   const char *ascii_text /**< [in]  */
);


/**
 * tbd
 */
static void add_text(
   const unc_text &text,     /**< [in]  */
   bool           is_ignored /**< [in]  */
);


/**
 * Count the number of characters to the end of the next chunk of text.
 * If it exceeds the limit, return true.
 */
static bool next_word_exceeds_limit(
   const unc_text &text,  /**< [in]  */
   size_t         idx     /**< [in]  */
);


/**
 * fill a line with empty characters until a specific column is reached
 */
static void fill_line(
   size_t column,    /**< [in] The column to advance to */
   bool   allow_tabs /**< [in] false=use only spaces, true=use tabs+spaces */
);


/**
 * fill a line with tab stops until the limit given by the parameter
 * column is reached as close as possible
 */
void fill_line_with_tabs(
   size_t column  /**< [in] column to not exceed with tab stops */
);


/**
 * fill a line with spaces until the limit given by the parameter
 * column is reached
 */
void fill_line_with_spaces(
   size_t column  /**< [in] column which determines where to stop */
);



/**
 * Output a comment to the column using indent_with_tabs and
 * indent_cmt_with_tabs as the rules.
 * base_col is the indent of the first line of the comment.
 * On the first line, column == base_col.
 * On subsequent lines, column >= base_col.
 *
 * @param brace_col the brace-level indent of the comment
 * @param base_col  the indent of the start of the comment (multiline)
 * @param column    the column that we should end up in
 */
static void cmt_output_indent(
   size_t brace_col, /**< [in]  */
   size_t base_col,  /**< [in]  */
   size_t column     /**< [in]  */
);


/**
 * Checks for and updates the lead chars.
 *
 * @param line the comment line
 * @return 0=not present, >0=number of chars that are part of the lead
 */
static size_t cmt_parse_lead(
   const unc_text &line,   /**< [in]  */
   bool           is_last  /**< [in]  */
);


/**
 * Scans a multiline comment to determine the following:
 *  - the extra indent of the non-first line (0 or 1)
 *  - the continuation text ('' or '* ')
 *
 * The decision is based on:
 *  - cmt_indent_multi
 *  - cmt_star_cont
 *  - cmt_multi_first_len_minimum
 *  - the first line length
 *  - the second line leader length
 *  - the last line length (without leading space/tab)
 *
 * If the first and last line are the same length and don't contain any alnum
 * chars and (the first line len > 2 or the second leader is the same as the
 * first line length), then the indent is 0.
 *
 * If the leader on the second line is 1 wide or missing, then the indent is 1.
 *
 * Otherwise, the indent is 0.
 *
 * @param str       The comment string
 * @param len       Length of the comment
 * @param start_col Starting column
 * @return          cmt.xtra_indent is set to 0 or 1
 */
static void calculate_comment_body_indent(
   cmt_reflow_t   &cmt, /**< [in]  */
   const unc_text &str  /**< [in]  */
);


/**
 * tbd
 */
static int next_up(
   const unc_text &text, /**< [in]  */
   size_t         idx,   /**< [in]  */
   const unc_text &tag   /**< [in]  */
);


/**
 * Outputs the C comment at pc.
 * C comment combining is done here
 *
 * @return the last chunk output'd
 */
static chunk_t *output_comment_c(
   chunk_t *pc /**< [in]  */
);


/**
 * Outputs the CPP comment at pc.
 * CPP comment combining is done here
 *
 * @return the last chunk output'd
 */
static chunk_t *output_comment_cpp(
   chunk_t *pc /**< [in]  */
);


/**
 * tbd
 */
static void cmt_trim_whitespace(
   unc_text &line,      /**< [in]  */
   bool     in_preproc  /**< [in]  */
);


/**
 * Outputs a comment. The initial opening '//' may be included in the text.
 * Subsequent openings (if combining comments), should not be included.
 * The closing (for C/D comments) should not be included.
 *
 * \TODO: If reflowing text, the comment should be added one word (or line) at a time.
 * A newline should only be sent if a blank line is encountered or if the next
 * line is indented beyond the current line (optional?).
 * If the last char on a line is a ':' or '.', then the next line won't be
 * combined.
 */
static void add_comment_text(
   const unc_text &text,     /**< [in]  */
   cmt_reflow_t   &cmt,      /**< [in]  */
   bool           esc_close  /**< [in]  */
);


/**
 * tbd
 */
static void output_cmt_start(
   cmt_reflow_t &cmt, /**< [in]  */
   chunk_t      *pc   /**< [in]  */
);


/**
 * Checks if the current comment can be combined with the next comment.
 *
 * Two comments can be combined if:
 *  1. They are the same type
 *  2. There is exactly one newline between them
 *  3. They are indented to the same level
 */
static bool can_combine_comment(
   chunk_t            *pc,   /**< [in] chunk with first comment */
   const cmt_reflow_t &cmt   /**< [in]  */
);


/**
 * check if a character combination is a DOS newline
 * thus if it is combination of carriage return CARRIAGERETURN
 * and line feed character
 *
 * \todo parameter should not be int type
 */
bool is_dos_newline(
   const int* pchar /**< [in] pointer to first character of line ending */
);


/**
 * Checks if a line of source code has a whitespace character
 * at its end. Whitespace is either a space or a tabstop.
 */
static bool line_has_trailing_whitespace(
   unc_text &line /**< [in] line to check */
);


#define LOG_CONTTEXT() \
   LOG_FMT(LCONTTEXT, "%s:%d set cont_text to '%s'\n", __func__, __LINE__, cmt.cont_text.c_str())


static const kw_subst_t kw_subst_table[] =
{
   { "$(filename)",  kw_fcn_filename  },
   { "$(class)",     kw_fcn_class     },
   { "$(message)",   kw_fcn_message   },
   { "$(category)",  kw_fcn_category  },
   { "$(scope)",     kw_fcn_scope     },
   { "$(function)",  kw_fcn_function  },
   { "$(javaparam)", kw_fcn_javaparam },
   { "$(fclass)",    kw_fcn_fclass    },
};


static void add_char(uint32_t ch)
{
   /* output a newline if ... */
   if ((cpd.last_char == CARRIAGERETURN) &&
       (ch            != LINEFEED      ) ) /* we did a CARRIAGERETURN not followed by a LINEFEED */
   {
      write_string(cpd.newline);
      cpd.column      = 1;
      cpd.did_newline = true;
      cpd.spaces      = 0;
   }

   /* convert a newline into the LF/CRLF/CR sequence */
   if (ch == LINEFEED)
   {
      write_string(cpd.newline);
      cpd.column      = 1;
      cpd.did_newline = true;
      cpd.spaces      = 0;
   }
   else if (ch == CARRIAGERETURN) /* do not output CARRIAGERETURN */
   {
      cpd.column      = 1;
      cpd.did_newline = true;
      cpd.spaces      = 0;
   }
   else if ((ch == TABSTOP) && cpd.output_tab_as_space)
   {
      size_t endcol = next_tab_column(cpd.column);
      while (cpd.column < endcol)
      {
         add_char(SPACE);
      }
      return;
   }
   else
   {
      /* explicitly disallow a tab after a space */
      if ((ch            == TABSTOP) &&
          (cpd.last_char == SPACE  ) )
      {
         size_t endcol = next_tab_column(cpd.column);
         fill_line_with_spaces(endcol);
         return;
      }
      else if ((ch == SPACE) && !cpd.output_trailspace)
      {
         cpd.spaces++;
         cpd.column++;
      }
      else
      {
         while (cpd.spaces > 0)
         {
            write_char(SPACE);
            cpd.spaces--;
         }
         write_char(ch);
         cpd.column = (ch == TABSTOP) ? next_tab_column(cpd.column) : cpd.column+1;
      }
   }
   cpd.last_char = ch; /*lint !e713 */
}


void fill_line_with_tabs(size_t column)
{
   while (next_tab_column(cpd.column) <= column)
   {
      add_text("\t");
   }
}


void fill_line_with_spaces(size_t column)
{
   while (cpd.column < column)
   {
      add_text(" ");
   }
}


static void fill_line(size_t column, bool allow_tabs)
{
   cpd.did_newline = false; // \todo what is this global flag used for?
   if (allow_tabs)
   {
      fill_line_with_tabs(column);   /* tab out as far as possible ... */
   }
   fill_line_with_spaces(column);    /* ...  and then use spaces */
}


static void add_text(const char *ascii_text)
{
   char ch;
   while ((ch = *ascii_text) != 0)
   {
      ascii_text++;
      add_char(ch); /*lint !e732 */
   }
}


static void add_text(const unc_text &text, bool is_ignored = false)
{
   for (size_t idx = 0; idx < text.size(); idx++)
   {
      int ch = text[idx];

      if( (is_ignored == true) &&
          (ch         >= 0   ) )
      {
         write_char((uint32_t)ch);
      }
      else
      {
         add_char(ch); /*lint !e732 */
      }
   }
}


static bool next_word_exceeds_limit(const unc_text &text, size_t idx)
{
   size_t length = 0;

   /* Count any whitespace */
   while ((idx < text.size()) && unc_isspace(text[idx]))
   {
      idx++;
      length++;
   }

   /* Count non-whitespace */
   while ((idx < text.size()) && !unc_isspace(text[idx]))
   {
      idx++;
      length++;
   }

   const bool result = ((cpd.column + length - 1) > cpd.settings[UO_cmt_width].u);
   return(result);
}


static void cmt_output_indent(size_t brace_col, size_t base_col, size_t column)
{
   size_t indent_with_tabs = cpd.settings[UO_indent_cmt_with_tabs].b ? 2 :
                            (cpd.settings[UO_indent_with_tabs    ].n ? 1 :
                                                                       0);

   size_t tab_col = (indent_with_tabs == 0) ? 0 :
                    (indent_with_tabs == 1) ? brace_col :
                  /*(indent_with_tabs == 2)*/ base_col;

   cpd.did_newline = false;
   if ( (                      indent_with_tabs == 2 ) ||
        ((cpd.column == 1) && (indent_with_tabs == 1)) )
   {
      fill_line_with_tabs(tab_col);
   }

   fill_line_with_spaces(column);
}


void output_parsed(FILE *pfile)
{
   // save_option_file(pfile, false);
   save_option_file_kernel(pfile, false, true);

   fprintf(pfile, "# -=====-\n");
   fprintf(pfile, "# Line              Tag           Parent          Columns Br/Lvl/pp     Flag   Nl  Text");
   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      fprintf(pfile, "\n# %3zu> %16.16s[%16.16s][%3zu/%3zu/%3u/%3u][%zu/%zu/%zu][%10" PRIx64 "][%zu-%d]",
              pc->orig_line,   get_token_name(pc->type),
              get_token_name(pc->ptype),         pc->column,
              pc->orig_col,    pc->orig_col_end, pc->orig_prev_sp,
              pc->brace_level, pc->level,        pc->pp_level,
              get_flags(pc),   pc->nl_count,     pc->after_tab);

      if (not_type(pc, CT_NEWLINE) &&
          (pc->len() != 0         ) )
      {
         for (size_t cnt = 0; cnt < pc->column; cnt++)
         {
            fprintf(pfile, " ");
         }

         if (not_type(pc, CT_NL_CONT)) { fprintf(pfile, "%s", pc->text()); }
         else                          { fprintf(pfile, "\\");             }
      }
   }
   fprintf(pfile, "\n# -=====-\n");
   fflush(pfile);
}


void output_text(FILE *pfile)
{
   cpd.fout        = pfile;
   cpd.did_newline = true;
   cpd.column      = 1;

   if (cpd.bom) { write_bom(); }

   chunk_t *pc;
   if (cpd.frag_cols > 0)
   {
      size_t indent = cpd.frag_cols - 1;

      /* loop over the whole chunk list */
      for (pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
      {
         pc->column        += indent;
         pc->column_indent += indent;
      }
      cpd.frag_cols = 0;
   }

   /* loop over the whole chunk list */
   for (pc = chunk_get_head(); is_valid(pc); pc = chunk_get_next(pc))
   {
      LOG_FMT(LOUTIND, "text() %s, type %s, col=%zu\n",
              pc->text(), get_token_name(pc->type), pc->orig_col);
      cpd.output_tab_as_space = (cpd.settings[UO_cmt_convert_tab_to_spaces].b &&
                                 is_cmt(pc));

      switch(pc->type)
      {
         case(CT_NEWLINE):
            for (size_t cnt = 0; cnt < pc->nl_count; cnt++)
            {
               add_char(LINEFEED);
            }
            cpd.did_newline = true;
            cpd.column      = 1;
            LOG_FMT(LOUTIND, " xx\n");
         break;

         case(CT_NL_CONT):
            /* FIXME: this really shouldn't be done here! */
            if (not_flag(pc, PCF_WAS_ALIGNED))
            {
               if (is_option_set(cpd.settings[UO_sp_before_nl_cont].a, AV_REMOVE))
               {
                  pc->column = cpd.column + (is_option(cpd.settings[UO_sp_before_nl_cont].a, AV_FORCE) ? 1 :0);
               }
               else
               {
                  /* Try to keep the same relative spacing */
                  chunk_t *prev = chunk_get_prev(pc);
                  while ((is_valid(prev)) &&
                         (prev->orig_col == 0 ) &&
                         (prev->nl_count == 0 ) )
                  {
                     prev = chunk_get_prev(prev);
                  }

                  if ((is_valid(prev)) &&
                      (prev->nl_count == 0 ) )
                  {
                     int orig_sp = (int)pc->orig_col - (int)prev->orig_col_end;
                     pc->column = (size_t)((int)cpd.column + orig_sp);
                     // the value might be negative --> use an int
                     int columnDiff = (int)cpd.column + orig_sp;
                     if ((cpd.settings[UO_sp_before_nl_cont].a != AV_IGNORE) &&
                         (columnDiff < (int)(cpd.column + 1u) ))
                     {
                        pc->column = cpd.column + 1;
                     }
                  }
               }
               fill_line(pc->column, false);
            }
            else
            {
               fill_line(pc->column, (cpd.settings[UO_indent_with_tabs].n == 2));
            }
            add_char(BACKSLASH);
            add_char(LINEFEED);
            cpd.did_newline = true;
            cpd.column      = 1;
            LOG_FMT(LOUTIND, " \\xx\n");
         break;

         case(CT_COMMENT_MULTI):
            if (cpd.settings[UO_cmt_indent_multi].b)
            {
               output_comment_multi(pc);
            }
            else
            {
               output_comment_multi_simple(pc, not_flag(pc, PCF_INSERTED));
            }
         break;

         case(CT_COMMENT_CPP):
         {
            bool tmp = cpd.output_trailspace;
            /* keep trailing spaces if they are still present in a chunk;
             * note that tokenize() already strips spaces in comments,
             * so if they made it up to here, they are to stay */
            cpd.output_trailspace = true;
            pc                    = output_comment_cpp(pc);
            cpd.output_trailspace = tmp;
         }
         break;

         case(CT_COMMENT):
            pc = output_comment_c(pc);
         break;

         case(CT_JUNK):
         case(CT_IGNORED):
            /* do not adjust the column for junk */
            add_text(pc->str, true);
         break;

         default:
            if (pc->len() == 0)
            {
               /* don't do anything for non-visible stuff */
               LOG_FMT(LOUTIND, " <%zu> -", pc->column);
            }
            else
            {
               bool allow_tabs;
               cpd.output_trailspace = is_type(pc, CT_STRING_MULTI);
               /* indent to the 'level' first */
               if (cpd.did_newline == true)
               {
                  if (cpd.settings[UO_indent_with_tabs].n == 1)
                  {
                     size_t lvlcol;
                     /* FIXME: it would be better to properly set column_indent in
                      * indent_text(), but this hack for '}' and ':' seems to work. */
                     if( (is_type(pc, CT_BRACE_CLOSE, CT_PREPROC)) ||
                         (is_str(pc, ":")                        ) )
                     {
                        lvlcol = pc->column;
                     }
                     else
                     {
                        lvlcol = pc->column_indent;
                        lvlcol = min(lvlcol, pc->column);
                     }

                     if (lvlcol > 1)
                     {
                        fill_line(lvlcol, true);
                     }
                  }
                  allow_tabs = (cpd.settings[UO_indent_with_tabs].n == 2) ||
                               (is_cmt(pc) &&
                               (cpd.settings[UO_indent_with_tabs].n != 0));

                  LOG_FMT(LOUTIND, "  %zu> col %zu/%zu/%u - ", pc->orig_line, pc->column, pc->column_indent, cpd.column);
               }
               else
               {
                  /* Reformatting multi-line comments can screw up the column.
                   * Make sure we don't mess up the spacing on this line.
                   * This has to be done here because comments are not formatted
                   * until the output phase. */
                  if (pc->column < cpd.column)
                  {
                     reindent_line(pc, cpd.column);
                  }

                  /* not the first item on a line */
                  chunk_t *prev = chunk_get_prev(pc); /* \todo einfacher wäre den vorherigen Chunk zwischenzuspeichern */
                  assert(is_valid(prev));
                  allow_tabs = (cpd.settings[UO_align_with_tabs].b &&
                                is_flag(pc, PCF_WAS_ALIGNED) &&
                                ((prev->column + prev->len() + 1) != pc->column));
                  if (cpd.settings[UO_align_keep_tabs].b)
                  {
                     allow_tabs = (pc->after_tab == true) ? true : allow_tabs;
                  }
                  LOG_FMT(LOUTIND, " %zu(%d) -", pc->column, allow_tabs);
               }

               fill_line(pc->column, allow_tabs);
               add_text(pc->str);
               if (is_type(pc, CT_PP_DEFINE))
               {
                  if (cpd.settings[UO_force_tab_after_define].b)
                  {
                     add_char(TABSTOP);
                  }
               }
               cpd.did_newline       = is_nl(pc);
               cpd.output_trailspace = false;
            break;
            }
      }
   }
}


static size_t cmt_parse_lead(const unc_text &line, bool is_last)
{
   size_t len = 0;

   while ((len < 32         ) &&
          (len < line.size()) )
   {
      if ((len         >  0 ) &&
           (line[len] == SLASH) )
      {
         /* ignore combined comments */
         size_t tmp = len + 1;
         while ((tmp < line.size()    ) &&
                 unc_isspace(line[tmp]) )
         {
            tmp++;
         }
         retval_if((tmp < line.size()) &&
                   (line[tmp] == SLASH), 1);
         break;
      }
      else if (strchr("*|\\#+", line[len]) == nullptr)
      {
         break;
      }
      len++;
   }

   retval_if(len > 30, 1);

   if(((len >  0) && ((len >= line.size()) || unc_isspace(line[len])) ) ||
      ((len >  0) && (is_last == true                               ) ) ||
      ((len == 1) && (line[0] == '*'                                ) ) )
   {
      return(len);
   }
   return(0);
}


bool is_dos_newline(const int* pchar)
{
   return((pchar[0] == CARRIAGERETURN) &&
          (pchar[1] == LINEFEED      ) );
}


bool is_part_of_newline(const int character)
{
   return((character == CARRIAGERETURN) ||
          (character == LINEFEED      ) );
}


bool is_space_or_tab(const int character)
{
   return((character == SPACE  ) ||
          (character == TABSTOP) );
}


static void calculate_comment_body_indent(cmt_reflow_t &cmt, const unc_text &str)
{
   return_if(!cpd.settings[UO_cmt_indent_multi].b);

   cmt.xtra_indent = 0;
   size_t idx      = 0;
   size_t len      = str.size();
   size_t last_len = 0;
   if (cpd.settings[UO_cmt_multi_check_last].b)
   {
      /* find the last line length */
      for (idx = len - 1; idx > 0; idx--)
      {
         if(is_part_of_newline(str[idx]))
         {
            idx++;
            while ( (idx      <  len    )   &&
                   ((str[idx] == SPACE  ) ||
                    (str[idx] == TABSTOP) ) )
            {
               idx++;
            }
            last_len = (len - idx);
            break;
         }
      }
   }

   /* find the first line length */
   size_t first_len = 0;
   for (idx = 0; idx < len; idx++)
   {
      if(is_part_of_newline(str[idx]))
      {
         first_len = idx;
         while ((str[first_len - 1] == SPACE  ) ||
                (str[first_len - 1] == TABSTOP) )
         {
            first_len--;
         }

         /* handle DOS endings */
#if 0
         if(is_dos_newline(&str[idx]))
#else
         if ((str[idx+0] == CARRIAGERETURN) &&
             (str[idx+1] == LINEFEED      ) )
#endif
         {
            idx++;
         }
         idx++;
         break;
      }
   }

   /* Scan the second line */
   size_t width = 0;
   for ( ; idx < len - 1; idx++)
   {
      if ((str[idx] == SPACE  ) ||
          (str[idx] == TABSTOP) )
      {
         break_if(width > 0);
         continue;
      }

      /* Done with second line */
      break_if(is_part_of_newline(str[idx]));

      /* Count the leading chars */
      if ((str[idx] == '*'      ) ||
          (str[idx] == '|'      ) ||
          (str[idx] == BACKSLASH) ||
          (str[idx] == '#'      ) ||
          (str[idx] == '+'      ) )
      {
         width++;
      }
      else
      {
         if ((width      != 1  ) ||
             (str[idx-1] != '*') )
         {
            width = 0;
         }
         break;
      }
   }

   // LOG_FMT(LSYS, "%s: first=%d last=%d width=%d\n", __func__, first_len, last_len, width);

   // If the first and last line are the same length and don't contain any alnum
   // chars and (the first line len > cmt_multi_first_len_minimum or
   // the second leader is the same as the first line length), then the indent is 0.
   return_if ( (first_len == last_len                                     )   &&
       ((first_len > cpd.settings[UO_cmt_multi_first_len_minimum].u) ||
        (first_len == width                                        ) ) );

   cmt.xtra_indent = ((width == 2) ? 0 : 1);
}


static int next_up(const unc_text &text, size_t idx, const unc_text &tag)
{
   size_t offs = 0;

   while ((idx < text.size()) && unc_isspace(text[idx]))
   {
      idx++;
      offs++;
   }

   retval_if (text.startswith(tag, idx), (int)offs);
   return(-1); // \todo is this a good solution to indicate an error?
}


static void add_comment_text(const unc_text &text, cmt_reflow_t &cmt, bool esc_close)
{
   bool   was_star  = false;
   bool   was_slash = false;
   bool   in_word   = false;
   size_t len       = text.size();
   size_t ch_cnt    = 0; /* chars since newline */

   /* If the '//' is included write it first else we may wrap an empty line */
   size_t idx = 0;

   if (text.startswith("//"))
   {
      add_text("//");
      idx += 2;
      while (unc_isspace(text[idx]))
      {
         add_char(text[idx++]);  /*lint !e732 */
      }
   }

   for ( ; idx < len; idx++)  /* \todo avoid modifying idx in loop */
   {
      /* Split the comment */
      if (text[idx] == LINEFEED)
      {
         in_word = false;
         add_char(LINEFEED);
         cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
         if (cmt.xtra_indent > 0)
         {
            add_char(SPACE);
         }
         /* hack to get escaped newlines to align and not dup the leading '//' */
         int tmp = next_up(text, idx + 1, cmt.cont_text);
         if (tmp < 0)
         {
            add_text(cmt.cont_text);
         }
         else
         {
            idx = (size_t)((int)idx + tmp);
         }
         ch_cnt = 0;

      }
      else if ((cmt.reflow == true              ) &&
               (text[idx]  == SPACE             ) &&
               (cpd.settings[UO_cmt_width].u > 0) &&
               ((cpd.column > cpd.settings[UO_cmt_width].u) ||
                ((ch_cnt > 1) && next_word_exceeds_limit(text, idx))))
      {
         in_word = false;
         add_char(LINEFEED);
         cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
         if (cmt.xtra_indent > 0) { add_char(SPACE); }

         add_text(cmt.cont_text);
         fill_line(cmt.column + cpd.settings[UO_cmt_sp_after_star_cont].u, false);
         ch_cnt = 0;

      }
      else
      {
         /* Escape a C closure in a CPP comment */
         if ( (esc_close               == true) &&
             ((was_star  && (text[idx] == SLASH)) ||
              (was_slash && (text[idx] == '*')) ) )
         {
            add_char(SPACE);
         }

         if (!in_word && !unc_isspace(text[idx]))
         {
            cmt.word_count++;
         }
         in_word = !unc_isspace(text[idx]);

         add_char    (text[idx]); /*lint !e732 */
         was_star  = (text[idx] == '*');
         was_slash = (text[idx] == SLASH);
         ch_cnt++;
      }
   }
} /*lint !e850 */


static void output_cmt_start(cmt_reflow_t &cmt, chunk_t *pc)
{
   cmt.pc          = pc;
   cmt.column      = pc->column;
   cmt.brace_col   = pc->column_indent;
   cmt.base_col    = pc->column_indent;
   cmt.word_count  = 0;
   cmt.xtra_indent = 0;
   cmt.cont_text.clear();
   cmt.reflow = false;

   if (is_flag(pc, PCF_INSERTED)) { do_keyword_substitution(pc); }

   if (cmt.brace_col == 0)
   {
      cmt.brace_col = 1u + (pc->brace_level * cpd.settings[UO_output_tab_size].u);
   }

   if (is_ptype(pc, CT_COMMENT_START, CT_COMMENT_WHOLE))
   {
      if ( (!cpd.settings[UO_indent_col1_comment].b) &&
           (pc->orig_col == 1                      ) &&
           (not_flag(pc, PCF_INSERTED)             ) )
      {
         cmt.column    = 1u;
         cmt.base_col  = 1u;
         cmt.brace_col = 1u;
      }
   }

   /* tab aligning code */
   if ( (cpd.settings[UO_indent_cmt_with_tabs].b)   &&
        (is_ptype(pc, CT_COMMENT_END, CT_COMMENT_WHOLE)))
   {
      cmt.column = align_tab_column(cmt.column - 1);
      pc->column = cmt.column;
   }
   cmt.base_col = cmt.column;

   /* Bump out to the column */
   cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
}


static bool can_combine_comment(chunk_t *pc, const cmt_reflow_t &cmt)
{
   /* We can't combine if ... */
   if ((is_invalid(pc)              ) || /* chunk is invalid or */
       is_ptype(pc, CT_COMMENT_START) )  /* there is something other than a newline next */
   {
      return(false);
   }

   /* next is a newline for sure, make sure it is a single newline */
   chunk_t *next = chunk_get_next(pc);
   if ((is_valid(next)) &&
       (next->nl_count == 1 ) )
   {
      /* Make sure the comment is the same type at the same column */
      next = chunk_get_next(next);
      if (  is_type(next, pc->type ) &&
          (((next->column ==            1) && (pc->column ==            1  )) ||
           ((next->column == cmt.base_col) && (pc->column == cmt.base_col  )) ||
           ((next->column  > cmt.base_col) && (pc->ptype  == CT_COMMENT_END)) ) )
      {
         return(true);
      }
   }
   return(false);
}


/**
 * tbd
 */
void combine_comment(
   unc_text     &tmp, /**< [in]  */
   chunk_t*     pc,   /**< [in]  */
   cmt_reflow_t &cmt  /**< [in]  */
);

/* \todo is this fct name correct? */
void combine_comment(unc_text &tmp, chunk_t *pc, cmt_reflow_t &cmt)
{
   tmp.set(pc->str, 2, pc->len() - 4);
   if ((cpd.last_char == '*'  ) &&
        (tmp[0]       == SLASH) )
   {
      add_text(" ");
   }
   add_comment_text(tmp, cmt, false);
}


static chunk_t *output_comment_c(chunk_t *first)
{
   retval_if(is_invalid(first), first);

   cmt_reflow_t cmt;
   output_cmt_start(cmt, first);
   cmt.reflow = (cpd.settings[UO_cmt_reflow_mode].n != 1);

   /* See if we can combine this comment with the next comment */
   if (!cpd.settings[UO_cmt_c_group].b ||
       !can_combine_comment(first, cmt))
   {
      /* Just add the single comment */
      cmt.cont_text = cpd.settings[UO_cmt_star_cont].b ? " * " : "   ";
      LOG_CONTTEXT();
      add_comment_text(first->str, cmt, false);
      return(first);
   }

   cmt.cont_text = cpd.settings[UO_cmt_star_cont].b ? " *" : "  ";
   LOG_CONTTEXT();

   add_text("/*"); /* comment start */
   if (cpd.settings[UO_cmt_c_nl_start].b)
   {
      add_comment_text("\n", cmt, false);
   }

   chunk_t  *pc = first;
   unc_text tmp;
   while (can_combine_comment(pc, cmt))
   {
      combine_comment(tmp, pc, cmt);
      add_comment_text("\n", cmt, false);
      pc = chunk_get_next(chunk_get_next(pc));
   }
   assert(is_valid(pc));
   combine_comment(tmp, pc, cmt);

   if (cpd.settings[UO_cmt_c_nl_end].b)
   {
      cmt.cont_text = " ";
      LOG_CONTTEXT();
      add_comment_text("\n", cmt, false);
   }
   add_comment_text("*/", cmt, false); /* comment end */
   return(pc);
}


/** \todo use more appropriate function name */
void do_something(
   int      &offs, /**< [in]  */
   chunk_t  *pc,   /**< [in]  */
   unc_text &tmp   /**< [in]  */
);


static chunk_t *output_comment_cpp(chunk_t *first)
{
   retval_if(is_invalid(first), first);

   cmt_reflow_t cmt;
   output_cmt_start(cmt, first);
   cmt.reflow = (cpd.settings[UO_cmt_reflow_mode].n != 1);

   unc_text leadin = "//";                    // default setting to keep previous behavior
   if (cpd.settings[UO_sp_cmt_cpp_doxygen].b) // special treatment for doxygen style comments (treat as unity)
   {
      const char *sComment = first->text();
      retval_if(ptr_is_invalid(sComment), first);

      bool grouping = (sComment[2] == '@');
      int  brace    = 3;
      if ((sComment[2] == SLASH) ||
          (sComment[2] == '!') ) // doxygen style found!
      {
         leadin += sComment[2];  // at least one additional char (either "///" or "//!")
         if (sComment[3] == '<') // and a further one (either "///<" or "//!<")
         {
            leadin += '<';
         }
         else
         {
            grouping = (sComment[3] == '@');  // or a further one (grouping)
            brace    = 4;
         }
      }
      if ( (grouping == true       )   &&
           ((sComment[brace] == '{') ||
            (sComment[brace] == '}') ) )
      {
         leadin += '@';
         leadin += sComment[brace];
      }
   }

   /* Special treatment for Qt translator or meta-data comments (treat as unity) */
   if (cpd.settings[UO_sp_cmt_cpp_qttr].b)
   {
      const int c = first->str[2];
      if ((c == ':') ||
          (c == '=') ||
          (c == '~') )
      {
         leadin += c;
      }
   }

   const argval_t sp_cmt_cpp_start = cpd.settings[UO_sp_cmt_cpp_start].a;

   /* CPP comments can't be grouped unless they are converted to C comments */
   if (!cpd.settings[UO_cmt_cpp_to_c].b)
   {
      cmt.cont_text = leadin;
      if (not_option(sp_cmt_cpp_start, AV_REMOVE))
      {
         cmt.cont_text += SPACE;
      }
      LOG_CONTTEXT();

      if (is_option(sp_cmt_cpp_start, AV_IGNORE))
      {
         add_comment_text(first->str, cmt, false);
      }
      else
      {
         size_t   iLISz = leadin.size();
         unc_text tmp(first->str, 0, iLISz);
         add_comment_text(tmp, cmt, false);

         tmp.set(first->str, iLISz, first->len() - iLISz);

         if (is_option_set(sp_cmt_cpp_start, AV_REMOVE))
         {
            while ((tmp.size() > 0    ) &&
                    unc_isspace(tmp[0]) )
            {
               tmp.pop_front();
            }
         }
         if (tmp.size() > 0)
         {
            if (is_option_set(sp_cmt_cpp_start, AV_ADD))
            {
               if (!unc_isspace(tmp[0]) &&
                   (tmp[0] != SLASH   ) )
               {
                  add_comment_text(" ", cmt, false);
               }
            }
            add_comment_text(tmp, cmt, false);
         }
      }
      return(first);
   }

   /* We are going to convert the CPP comments to C comments */
   cmt.cont_text = cpd.settings[UO_cmt_star_cont].b ? " * " : "   ";
   LOG_CONTTEXT();

   unc_text tmp;
   /* See if we can combine this comment with the next comment */
   if (!cpd.settings[UO_cmt_cpp_group].b ||
       !can_combine_comment(first, cmt))
   {
      /* nothing to group: just output a single line */
      add_text("/*");
      if( (!unc_isspace(first->str[2])            ) &&
          (is_option_set(sp_cmt_cpp_start, AV_ADD)) )
      {
         add_char(SPACE);
      }
      tmp.set(first->str, 2, first->len() - 2);
      add_comment_text(tmp, cmt, true);
      add_text(" */");
      return(first);
   }

   add_text("/*");
   if (cpd.settings[UO_cmt_cpp_nl_start].b)
   {
      add_comment_text("\n", cmt, false);
   }
   else
   {
      add_text(" ");
   }
   chunk_t *pc = first;
   int     offs;

   while (can_combine_comment(pc, cmt))
   {
      do_something(offs, pc, tmp);

      if ((cpd.last_char == '*'  ) &&
          (tmp[0]        == SLASH) )
      {
         add_text(" ");
      }
      add_comment_text(tmp,  cmt, true );
      add_comment_text("\n", cmt, false);
      pc = chunk_get_next(chunk_get_next(pc));
      assert(is_valid(pc));
   }
   do_something(offs, pc, tmp);

   add_comment_text(tmp, cmt, true);
   if (cpd.settings[UO_cmt_cpp_nl_end].b)
   {
      cmt.cont_text = "";
      LOG_CONTTEXT();
      add_comment_text("\n", cmt, false);
   }
   add_comment_text(" */", cmt, false);
   return(pc);
}


void do_something(int &offs, chunk_t *pc, unc_text &tmp)
{
   offs = unc_isspace(pc->str[2]) ? 1 : 0;
   tmp.set(pc->str, (size_t)(2 + offs), (size_t)((int)pc->len() - 2 + offs));
}


static bool line_has_trailing_whitespace(unc_text &line)
{
   return ((line.size() > 0       )   &&
          ((line.back() == SPACE  ) ||
           (line.back() == TABSTOP) ) );
}

static void cmt_trim_whitespace(unc_text &line, bool in_preproc)
{
   /* Remove trailing whitespace on the line */
   while(line_has_trailing_whitespace(line))
   {
      line.pop_back();
   }

   /* Shift back to the comment text, ... */
   if((in_preproc  == true     ) &&  /* if in a preproc ... */
      (line.size() >  1        ) &&  /* with a line that holds ... */
      (line.back() == BACKSLASH) )   /* a backslash-newline ... */
   {
      /* If there was any space before the backslash, change it to 1 space */
      line.pop_back();

      bool add_space = false;
      while(line_has_trailing_whitespace(line))
      {
         add_space = true;
         line.pop_back();
      }

      if (add_space) { line.append(SPACE); }
      line.append(BACKSLASH);
   }
}


static void output_comment_multi(chunk_t *pc)
{
   // \todo DRY 5 with output_comment_multi_simple

   cmt_reflow_t cmt;
   output_cmt_start(cmt, pc);
   cmt.reflow = (cpd.settings[UO_cmt_reflow_mode].n != 1);

   size_t cmt_col  = cmt.base_col;
   int    col_diff = (int)pc->orig_col - (int)cmt.base_col;

   calculate_comment_body_indent(cmt, pc->str);

   cmt.cont_text =  cpd.settings[UO_cmt_indent_multi].b == false ? ""   :
                   (cpd.settings[UO_cmt_star_cont   ].b == true  ? "* " : "  ");
   LOG_CONTTEXT();

   size_t   line_count = 0;
   size_t   cmt_idx    = 0;
   size_t   ccol       = pc->column; /* the col of subsequent comment lines */
   bool     nl_end     = false;
   unc_text line;
   line.clear();
   while (cmt_idx < pc->len())
   {
      int ch = pc->str[cmt_idx++];

      /* handle the CRLF and CR endings. convert both to LF */
      if (ch == CARRIAGERETURN)
      {
         ch = LINEFEED;
         if ((cmt_idx < pc->len()         ) &&
             (pc->str[cmt_idx] == LINEFEED) )
         {
            cmt_idx++;
         }
      }

      /* Find the start column */
      if (line.size() == 0)
      {
         nl_end = false;
         if (ch == SPACE)
         {
            ccol++;
            continue;
         }
         else if (ch == TABSTOP)
         {
            ccol = calc_next_tab_column(ccol, cpd.settings[UO_input_tab_size].u);
            continue;
         }
         else
         {
            // LOG_FMT(LSYS, "%d] Text starts in col %d\n", line_count, ccol);
         }
      }

      // DRY 5 end

      /* Now see if we need/must fold the next line with the current to enable
       * full reflow */
      if ((cpd.settings[UO_cmt_reflow_mode].n == 2) &&
          (ch      == LINEFEED     ) &&
          (cmt_idx <  pc->len()) )
      {
         int    prev_nonempty_line = -1;
         size_t nwidx              = line.size();
         bool   star_is_bullet     = false;

         /* strip trailing whitespace from the line collected so far */
         while (nwidx > 0)
         {
            nwidx--;
            if ((prev_nonempty_line < 0) &&
                !unc_isspace(line[nwidx]) &&
                (line[nwidx] != '*') &&     // block comment: skip '*' at end of line
                (is_preproc(pc) ?
                  (line[nwidx  ] !=  '\\') ||
                 ((line[nwidx+1] !=  'r' ) &&
                  (line[nwidx+1] != LINEFEED ) )
                 : true))
            {
               prev_nonempty_line = (int)nwidx; // last non-whitespace char in the previous line
            }
         }

         int    next_nonempty_line = -1;
         size_t remaining = pc->len() - cmt_idx;
         for (size_t nxt_len = 0;
              (nxt_len <= remaining    ) &&
              (pc->str[nxt_len] != 'r' ) && /* \todo should this be \r ? */
              (pc->str[nxt_len] != LINEFEED);
              nxt_len++)
         {
            if ((next_nonempty_line < 0) &&
                !unc_isspace(pc->str[nxt_len]) &&
                (pc->str[nxt_len] != '*') &&
                ((nxt_len == remaining) ||
                 (is_preproc(pc)
                  ? (pc->str[nxt_len] != '\\') ||
                  ((pc->str[nxt_len + 1] != 'r') && /* \todo should this be \r ? */
                   (pc->str[nxt_len + 1] != LINEFEED))
                  : true)))
            {
               next_nonempty_line = (int)nxt_len;      // first non-whitespace char in the next line
            }
         }

         /* see if we should fold up; usually that'd be a YES, but there are a few
          * situations where folding/reflowing by merging lines is frowned upon:
          *
          * - ASCII art in the comments (most often, these are drawings done in +-\/|.,*)
          *
          * - Doxygen/JavaDoc/etc. parameters: these often start with \ or @, at least
          *   something clearly non-alphanumeric (you see where we're going with this?)
          *
          * - bullet lists that are closely spaced: bullets are always non-alphanumeric
          *   characters, such as '-' or '+' (or, oh horror, '*' - that's bloody ambiguous
          *   to parse :-( ... with or without '*' comment start prefix, that's the
          *   question, then.)
          *
          * - semi-HTML formatted code, e.g. <pre>...</pre> comment sections (NDoc, etc.)
          *
          * - New lines which form a new paragraph without there having been added an
          *   extra empty line between the last sentence and the new one.
          *   A bit like this, really; so it is opportune to check if the last line ended
          *   in a terminal (that would be the set '.:;!?') and the new line starts with
          *   a capital.
          *   Though new lines starting with comment delimiters, such as '(', should be
          *   pulled up.
          *
          * So it bores down to this: the only folding (& reflowing) that's going to happen
          * is when the next line starts with an alphanumeric character AND the last
          * line didn't end with an non-alphanumeric character, except: ',' AND the next
          * line didn't start with a '*' all of a sudden while the previous one didn't
          * (the ambiguous '*'-for-bullet case!) */
         if ( (prev_nonempty_line >= 0) &&
              (next_nonempty_line >= 0) &&
              (((unc_isalnum(line   [(size_t)prev_nonempty_line]) || strchr(",)]", line   [(size_t)prev_nonempty_line])) &&
                (unc_isalnum(pc->str[(size_t)next_nonempty_line]) || strchr("([" , pc->str[(size_t)next_nonempty_line])) ) ||
                // dot followed by non-capital is NOT a new sentence start
              (('.' == line[(size_t)prev_nonempty_line]) && unc_isupper(pc->str[(size_t)next_nonempty_line]))) &&
              !star_is_bullet)
         {

            line.resize((size_t)(prev_nonempty_line + 1)); // rewind the line to the last non-alpha:

            cmt_idx =  (size_t)((int)cmt_idx + next_nonempty_line);  // roll the current line forward to the first non-alpha:

            ch = SPACE;  /* override the NL and make it a single whitespace: */
         }
      }

      line.append(ch);

      /* If we ... */
      if ((ch      == LINEFEED ) || /* hit an end of line sign or */
          (cmt_idx == pc->len()) )  /* hit an end-of-comment */
      {
         line_count++;

         /* strip trailing tabs and spaces before the newline */
         if (ch == LINEFEED)
         {
            nl_end = true;
            line.pop_back();
            cmt_trim_whitespace(line, is_preproc(pc));
         }

         // LOG_FMT(LSYS, "[%3d]%s\n", ccol, line);

         if (line_count == 1)
         {
            /* this is the first line - add unchanged */
            add_comment_text(line, cmt, false);
            if (nl_end == true) { add_char(LINEFEED); }
         }
         else
         {
            /* This is not the first line, so we need to indent to the
             * correct column. Each line is indented 0 or more spaces. */
            ccol = (size_t)((int)ccol - col_diff);
            if (ccol < (cmt_col + 3)) { ccol = cmt_col + 3; }

            if (line.size() == 0)
            {
               /* Empty line - just a LINEFEED */
               if (cpd.settings[UO_cmt_star_cont].b)
               {
                  cmt.column = cmt_col + cpd.settings[UO_cmt_sp_before_star_cont].u;
                  cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);

                  if (cmt.xtra_indent > 0) { add_char(SPACE); }
                  add_text(cmt.cont_text);
               }
               add_char(LINEFEED);
            }
            else
            {
               /* If this doesn't start with a '*' or '|'.
                * '\name' is a common parameter documentation thing. */
               if (cpd.settings[UO_cmt_indent_multi].b &&
                    (line[0] != '*' ) &&
                    (line[0] != '|' ) &&
                    (line[0] != '#' ) &&
                   ((line[0] != '\\') || unc_isalpha(line[1])) &&
                    (line[0] != '+' ) )
               {
                  size_t start_col = cmt_col + cpd.settings[UO_cmt_sp_before_star_cont].u;

                  if (cpd.settings[UO_cmt_star_cont].b)
                  {
                     cmt.column = start_col;
                     cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
                     if (cmt.xtra_indent > 0) { add_char(SPACE); }
                     add_text(cmt.cont_text);

                     size_t end_col = ccol + cpd.settings[UO_cmt_sp_after_star_cont].u;
                     fill_line(end_col, false);
                  }
                  else
                  {
                     cmt.column = ccol;
                     cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
                  }
               }
               else
               {
                  cmt.column = cmt_col + cpd.settings[UO_cmt_sp_before_star_cont].u;
                  cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
                  if (cmt.xtra_indent > 0) { add_char(SPACE); }

                  // Checks for and updates the lead chars.
                  // @return 0=not present, >0=number of chars that are part of the lead
                  size_t idx = cmt_parse_lead(line, (cmt_idx == pc->len()));
                  if (idx > 0)
                  {
                     // >0=number of chars that are part of the lead
                     cmt.cont_text.set(line, 0, idx);
                     LOG_CONTTEXT();
                     if ((line.size() >=  2   ) &&
                         (line[0]     == '*'  ) &&
                         (unc_isalnum(line[1])) )
                     {
                        line.insert(1, SPACE);
                     }
                  }
                  else
                  {
                     if (is_lang(cpd, LANG_D)) { add_text(cmt.cont_text); } // 0=no lead char present
                  }
               }

               add_comment_text(line, cmt, false);
               if (nl_end) { add_text("\n"); }
            }
         }
         line.clear();
         ccol = 1;
      }
   }
}


static bool kw_fcn_filename(chunk_t *cmt, unc_text &out_txt)
{
   UNUSED(cmt);
   out_txt.append(path_basename(cpd.filename));
   return(true);
}


static bool kw_fcn_class(chunk_t *cmt, unc_text &out_txt)
{
   chunk_t *tmp = nullptr;

   if (is_lang(cpd, LANG_CPP) &&
       is_lang(cpd, LANG_OC ) )
   {
      chunk_t *fcn = get_next_function(cmt);

      tmp = (is_type(fcn, CT_OC_MSG_DECL)) ? get_prev_oc_class(cmt) : get_next_class(cmt);
   }
   else if (is_lang(cpd, LANG_OC))
   {
      tmp = get_prev_oc_class(cmt);
   }

   if (is_invalid(tmp)) { tmp = get_next_class(cmt); }

   if (is_valid(tmp))
   {
      out_txt.append(tmp->str);
      if (cpd.lang_flags)
      {
         while ((tmp = chunk_get_next(tmp)) != nullptr)
         {
            break_if(not_type(tmp, CT_DC_MEMBER));
            tmp = chunk_get_next(tmp);
            if (tmp)
            {
               out_txt.append("::");
               out_txt.append(tmp->str);
            }
         }
      }
      return(true);
   }
   return(false);
}


static bool kw_fcn_message(chunk_t *cmt, unc_text &out_txt)
{
   chunk_t *fcn = get_next_function(cmt);
   retval_if(is_invalid(fcn), false);

   out_txt.append(fcn->str);

   chunk_t *tmp  = get_next_ncnl(fcn);
   const chunk_t *word = nullptr;
   while (is_valid(tmp))
   {
      break_if(is_type(tmp, CT_BRACE_OPEN, CT_SEMICOLON));

      if (is_type(tmp, CT_OC_COLON))
      {
         if (is_valid(word))
         {
            out_txt.append(word->str);
            word = nullptr;
         }
         out_txt.append(":");
      }
      if (is_type(tmp, CT_WORD)) { word = tmp; }
      tmp = get_next_ncnl(tmp);
   }
   return(true);
}


static bool kw_fcn_category(chunk_t *cmt, unc_text &out_txt)
{
   const chunk_t *category = get_prev_category(cmt);
   if (is_valid(category))
   {
      out_txt.append('(');
      out_txt.append(category->str);
      out_txt.append(')');
   }
   return(true);
}


static bool kw_fcn_scope(chunk_t *cmt, unc_text &out_txt)
{
   const chunk_t *scope = get_next_scope(cmt);
   if (is_valid(scope))
   {
      out_txt.append(scope->str);
      return(true);
   }
   return(false);
}


static bool kw_fcn_function(chunk_t *cmt, unc_text &out_txt)
{
   const chunk_t *fcn = get_next_function(cmt);
   if (is_valid(fcn))
   {
      out_txt.append_cond(is_ptype(fcn, CT_OPERATOR), "operator ");

      if(is_type(fcn->prev, CT_DESTRUCTOR))
      {
         out_txt.append('~');
      }
      out_txt.append(fcn->str);
      return(true);
   }
   return(false);
}


static bool kw_fcn_javaparam(chunk_t *cmt, unc_text &out_txt)
{
   chunk_t *fcn = get_next_function(cmt);
   retval_if(is_invalid(fcn), false);

   chunk_t *fpo;
   chunk_t *fpc;
   bool    has_param = true;
   bool    need_nl   = false;

   if (is_type(fcn, CT_OC_MSG_DECL))
   {
      chunk_t *tmp = get_next_ncnl(fcn);
      has_param = false;
      while (tmp)
      {
         break_if(is_type(tmp, CT_BRACE_OPEN, CT_SEMICOLON));

         if (has_param)
         {
            out_txt.append_cond(need_nl, "\n");
            need_nl = true;
            out_txt.append("@param %s TODO", tmp->str.c_str() );
         }

         has_param = false;
         if (is_type(tmp, CT_PAREN_CLOSE)) { has_param = true; }
         tmp = get_next_ncnl(tmp);
      }
      fpo = fpc = nullptr;
   }
   else
   {
      fpo = get_next_type(fcn, CT_FPAREN_OPEN, (int)fcn->level);
      retval_if(is_invalid(fpo), true);

      fpc = get_next_type(fpo, CT_FPAREN_CLOSE,(int)fcn->level);
      retval_if(is_invalid(fpc), true);
   }

   chunk_t *tmp;
   /* Check for 'foo()' and 'foo(void)' */
   if (get_next_ncnl(fpo) == fpc)
   {
      has_param = false;
   }
   else
   {
      tmp = get_next_ncnl(fpo);
      if ((tmp == get_prev_ncnl(fpc)) &&
          is_str(tmp, "void"))
      {
         has_param = false;
      }
   }

   if (has_param == true)
   {
      chunk_t *prev = nullptr;
      tmp = fpo;
      while ((tmp = chunk_get_next(tmp)) != nullptr)
      {
         if (is_type(tmp, CT_COMMA) || (tmp == fpc))
         {
            out_txt.append_cond(need_nl, "\n");
            need_nl = true;
            out_txt.append("@param");
            if (is_valid(prev))
            {
               out_txt.append(" %s TODO", prev->str.c_str() );
            }
            prev = nullptr;
            break_if(tmp == fpc);
         }

         if (is_type(tmp, CT_WORD)) { prev = tmp; }
      }
   }

   /* Do the return stuff */
   tmp = get_prev_ncnl(fcn);
   assert(is_valid(tmp));

   /* For Objective-C we need to go to the previous chunk */
   if (is_type_and_ptype(tmp, CT_PAREN_CLOSE, CT_OC_MSG_DECL))
   {
      tmp = get_prev_ncnl(tmp);
   }

   if (is_valid(tmp) && !is_str(tmp, "void"))
   {
      out_txt.append_cond(need_nl, "\n");
      out_txt.append("@return TODO");
   }

   return(true);
}


static bool kw_fcn_fclass(chunk_t *cmt, unc_text &out_txt)
{
   chunk_t *fcn = get_next_function(cmt);
   retval_if(is_invalid(fcn), false);

   if (is_flag(fcn, PCF_IN_CLASS))
   {
      /* if inside a class, we need to find to the class name */
      chunk_t *tmp = get_prev_type(fcn, CT_BRACE_OPEN, (int)(fcn->level - 1));
      assert(is_valid(tmp));
      tmp = get_prev_type(tmp, CT_CLASS, (int)tmp->level);
      tmp = get_next_ncnl(tmp);
      while (is_type(get_next_ncnl(tmp), CT_DC_MEMBER))
      {
         tmp = get_next_ncnl(tmp);
         tmp = get_next_ncnl(tmp);
      }

      if (is_valid(tmp))
      {
         out_txt.append(tmp->str);
         return(true);
      }
   }
   else
   {
      /* if outside a class, we expect "CLASS::METHOD(...)" */
      chunk_t *tmp = get_prev_ncnl(fcn);
      if(is_type(tmp, CT_OPERATOR))
      {
         tmp = get_prev_ncnl(tmp);
      }

      if(chunk_is_member(tmp))
      {
         tmp = get_prev_ncnl(tmp);
         assert(is_valid(tmp));
         out_txt.append(tmp->str);
         return(true);
      }
   }
   return(false);
}


static void do_keyword_substitution(chunk_t *pc)
{
   for (const auto &kw : kw_subst_table)
   {
      int idx = pc->str.find(kw.tag);
      continue_if(idx < 0);

      unc_text tmp_txt;
      tmp_txt.clear();
      if (kw.func(pc, tmp_txt))
      {
         /* if the replacement contains LINEFEED we need to fix the lead */
         if (tmp_txt.find("\n") >= 0)
         {
            size_t nl_idx = pc->str.rfind("\n", idx);
            if (nl_idx > 0)
            {
               unc_text nl_txt;
               nl_txt.append("\n");
               nl_idx++;
               while ((nl_idx < static_cast<size_t>(idx)) && !unc_isalnum(pc->str[nl_idx]))
               {
                  nl_txt.append(pc->str[nl_idx++]);
               }
               tmp_txt.replace("\n", nl_txt);
            }
         }
         pc->str.replace(kw.tag, tmp_txt);
      }
   }
}


static void output_comment_multi_simple(chunk_t *pc, bool kw_subst)
{
   UNUSED(kw_subst);

   // DRY 5 start with output_comment_multi

   cmt_reflow_t cmt;
   output_cmt_start(cmt, pc);

   int col_diff = (is_nl(chunk_get_prev(pc))) ?
      (int)pc->orig_col - (int)pc->column : 0;
   /* The comment should be indented correctly : The comment starts after something else */

   size_t   line_count = 0;
   size_t   cmt_idx    = 0;
   size_t   ccol       = pc->column;
   bool     nl_end     = false;
   unc_text line;
   line.clear();
   while (cmt_idx < pc->len())
   {
      int ch = pc->str[cmt_idx++];

      /* handle the CRLF and CR endings. convert both to LF */
      if (ch == CARRIAGERETURN)
      {
         ch = LINEFEED;
         if ((cmt_idx < pc->len()         ) &&
             (pc->str[cmt_idx] == LINEFEED) )
         {
            cmt_idx++;
         }
      }

      /* Find the start column */
      if (line.size() == 0)
      {
         nl_end = false;
         if (ch == SPACE)
         {
            ccol++;
            continue;
         }
         else if (ch == TABSTOP)
         {
            ccol = calc_next_tab_column(ccol, cpd.settings[UO_input_tab_size].u);
            continue;
         }
         else
         {
            // LOG_FMT(LSYS, "%d] Text starts in col %d, col_diff=%d, real=%d\n",
            //        line_count, ccol, col_diff, ccol - col_diff);
         }
      }

      // DRY 5 end

      line.append(ch);

      /* If we just hit an end of line OR we just hit end-of-comment... */
      if ((ch      == LINEFEED ) ||
          (cmt_idx == pc->len()) )
      {
         line_count++;

         /* strip trailing tabs and spaces before the newline */
         if (ch == LINEFEED)
         {
            line.pop_back();
            nl_end = true;

            /* Say we aren't in a preproc to prevent changing any bs-nl */
            cmt_trim_whitespace(line, false);
         }

         if (line_count > 1)
         {
            ccol = (size_t)((int)ccol - col_diff);
         }

         if (line.size() > 0)
         {
            cmt.column = ccol;
            cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
            add_text(line);
         }

         if (nl_end) { add_char(LINEFEED); }
         line.clear();
         ccol = 1;
      }
   }
}


static void generate_if_conditional_as_text(unc_text &dst, chunk_t *ifdef)
{
   int column = -1; /* \todo better use size_t avoid negative values for column */

   dst.clear();
   for (chunk_t *pc = ifdef; is_valid(pc); pc = chunk_get_next(pc))
   {
      if (column == -1) { column = (int)pc->column; }

      switch(pc->type)
      {
         case(CT_NEWLINE      ): /* fallthrough */
         case(CT_COMMENT_MULTI): /* fallthrough */
         case(CT_COMMENT_CPP  ): /* do nothing */  return;

         case(CT_COMMENT):       /* fallthrough */
         case(CT_COMMENT_EMBED): /* do nothing */  break;

         case(CT_NL_CONT):
            dst   += SPACE;
            column = -1;
         break;

         default: /* (CT_JUNK or everything  else */
            for (int spacing = (int)pc->column - column; spacing > 0; spacing--)
            {
               dst += SPACE;
               column++;
            }
            dst.append(pc->str);
            column += (int)pc->len();
         break;
      }
   }
}


void add_long_preprocessor_conditional_block_comment(void)
{
   const chunk_t *pp_start = nullptr;
   const chunk_t *pp_end   = nullptr;

   for (chunk_t *pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      /* just track the preproc level: */
      if (is_type(pc, CT_PREPROC))
      {
         pp_end   = pc;
         pp_start = pc;
      }

      continue_if(not_type(pc, CT_PP_IF) || is_invalid (pp_start ));

      chunk_t *br_close;
      chunk_t *br_open = pc;
      size_t  nl_count = 0;
      chunk_t *tmp     = pc;
      while ((tmp = chunk_get_next(tmp)) != nullptr)
      {
         assert(is_valid(tmp));
         /* just track the preproc level: */
         if (is_type(tmp, CT_PREPROC)) { pp_end = tmp; }

         assert(is_valid(pp_end));
         if (is_nl(tmp))
         {
            nl_count += tmp->nl_count;
         }
         else if ((pp_end->pp_level == pp_start->pp_level) &&
                   (is_type(tmp,     CT_PP_ENDIF) ||
                   (is_type(br_open, CT_PP_IF   ) ?
                    is_type(tmp,     CT_PP_ELSE ) : 0)))
         {
            br_close = tmp;
            const char* str = is_type(tmp, CT_PP_ENDIF) ? "#endif" : "#else";
            LOG_FMT(LPPIF, "found #if / %s section on lines %zu and %zu, nl_count=%zu\n",
                    str, br_open->orig_line, br_close->orig_line, nl_count);

            /* Found the matching #else or #endif - make sure a newline is next */
            tmp = chunk_get_next(tmp);

            LOG_FMT(LPPIF, "next item type %d (is %s)\n",   /* \todo use switch */
                    (is_valid(tmp) ? (int)tmp->type : -1),
                    (is_valid(tmp) ?
                     is_nl(tmp) ? "newline" : is_cmt(tmp) ?
                     "comment" : "other" : "---"));

            if (is_type(tmp, CT_NEWLINE))  /* chunk_is_newline(tmp) */
            {
               size_t nl_min = (is_type(br_close, CT_PP_ENDIF)) ?
                    cpd.settings[UO_mod_add_long_ifdef_endif_comment].u :
                    cpd.settings[UO_mod_add_long_ifdef_else_comment ].u;

               const char *txt = is_invalid(tmp)              ? "EOF" :
                                 is_type   (tmp, CT_PP_ENDIF) ? "#endif" :
                                                                "#else";

               LOG_FMT(LPPIF, "#if / %s section candidate for augmenting when over NL threshold %zu != 0 (nl_count=%zu)\n",
                       txt, nl_min, nl_count);

               if ((nl_min > 0) && (nl_count > nl_min))        /* nl_count is 1 too large at all times as #if line was counted too */
               {                                               /* determine the added comment style */
                  c_token_t style = (is_lang(cpd, LANG_CPPCS)) ? CT_COMMENT_CPP : CT_COMMENT;

                  unc_text str;
                  generate_if_conditional_as_text(str, br_open);

                  LOG_FMT(LPPIF, "#if / %s section over threshold %zu (nl_count=%zu) --> insert comment after the %s: %s\n",
                          txt, nl_min, nl_count, txt, str.c_str());

                  /* Add a comment after the close brace */
                  insert_comment_after(br_close, style, str);
               }
            }

            /* checks both the #else and #endif for a given level, only then look further in the main loop */
            break_if(is_type(br_close, CT_PP_ENDIF));
         }
      }
   }
}

