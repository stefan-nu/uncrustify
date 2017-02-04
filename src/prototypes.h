/**
 * @file prototypes.h
 * Big jumble of prototypes used in Uncrustify.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef C_PARSE_PROTOTYPES_H_INCLUDED
#define C_PARSE_PROTOTYPES_H_INCLUDED

#include "uncrustify_types.h"
#include "chunk_list.h"

#include <string>
#include <deque>


/*
 *  options.cpp
 */
void unc_begin_group(uncrustify_groups id, const char *short_desc, const char *long_desc = NULL);
void register_options(void);
void set_option_defaults(void);
void process_option_line(char *configLine, const char *filename);
int load_option_file(const char *filename);
int save_option_file(FILE *pfile, bool withDoc);
int save_option_file_kernel(FILE *pfile, bool withDoc, bool only_not_default);
int set_option_value(const char *name, const char *value);
const group_map_value *get_group_name(size_t ug);
const option_map_value *get_option_name(uncrustify_options uo);
void print_options(FILE *pfile);


/*
 *  punctuators.cpp
 */
const chunk_tag_t *find_punctuator(const char *str, int lang_flags);


/**
 * Advances to the next tab stop.
 * Column 1 is the left-most column.
 *
 * @param col     The current column
 * @param tabsize The tabsize
 * @return the next tabstop column
 */
static_inline
size_t calc_next_tab_column(size_t col, size_t tabsize)
{
   if (col == 0)
   {
      col = 1;
   }
   if (cpd.frag_cols > 0)
   {
      col += cpd.frag_cols - 1;
   }
   col = 1 + ((((col - 1) / tabsize) + 1) * tabsize);
   if (cpd.frag_cols > 0)
   {
      col -= cpd.frag_cols - 1;
   }
   return(col);
}


/**
 * Advances to the next tab stop for output.
 *
 * @param col  The current column
 * @return the next tabstop column
 */
static_inline
size_t next_tab_column(size_t col)
{
   return(calc_next_tab_column(col, cpd.settings[UO_output_tab_size].u));
}


/**
 * Advances to the next tab stop if not currently on one.
 *
 * @param col  The current column
 * @return the next tabstop column
 */
static_inline
size_t align_tab_column(size_t col)
{
   //if (col <= 0)
   if (col == 0)
   {
      col = 1;
   }
   if ((col % cpd.settings[UO_output_tab_size].u) != 1)
   {
      col = next_tab_column(col);
   }
   return(col);
}

#endif /* C_PARSE_PROTOTYPES_H_INCLUDED */
