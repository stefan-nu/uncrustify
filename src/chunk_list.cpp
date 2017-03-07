/**
 * @file chunk_list.cpp
 * Manages and navigates the list of chunks.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "chunk_list.h"
#include "char_table.h"
#include <cstring>
#include <cstdlib>
#include <stdarg.h>

#include "uncrustify_types.h"
#include "ListManager.h"
#include "uncrustify.h"
#include "space.h"


/** note
 *  throughout the code it is important to correctly recognize and name
 *  the found object. Here is some useful information.
 *
 *  parenthesis = ( )
 *  brace       = { } aka "curly brace"
 *  bracket     = [ ] aka "square bracket"
 */

/** use this enum to define in what direction or location an
 *  operation shall be performed. */
enum class dir_e : unsigned int
{
   BEFORE, /**< indicates a position or direction upwards   (=prev) */
   AFTER   /**< indicates a position or direction downwards (=next) */
};


/***************************************************************************//**
 * @brief prototype for a function that checks a chunk to have a given type
 *
 * @note this typedef defines the function type "check_t"
 * for a function pointer of type
 * bool function(chunk_t *pc)
 ******************************************************************************/
typedef bool (*check_t)(chunk_t *pc);



/***************************************************************************//**
 * @brief prototype for a function that searches through a chunk list
 *
 * @note this typedef defines the function type "search_t"
 * for a function pointer of type
 * chunk_t *function(chunk_t *cur, scope_t scope)
 ******************************************************************************/
typedef chunk_t * (*search_t)(chunk_t *cur, const scope_e scope);


typedef ListManager<chunk_t> ChunkList_t;



/***************************************************************************//**
 * \brief search for a chunk that satisfies a condition in a chunk list
 *
 * A generic function that traverses a chunks list either
 * in forward or reverse direction. The traversal continues until a
 * chunk satisfies the condition defined by the compare function.
 * Depending on the parameter cond the condition will either be
 * checked to be true or false.
 *
 * Whenever a chunk list traversal is to be performed this function
 * shall be used. This keeps the code clear and easy to understand.
 *
 * If there are performance issues this function might be worth to
 * be optimized as it is heavily used.
 *
 * @retval nullptr    - no requested chunk was found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
static chunk_t *chunk_search(
   chunk_t       *cur,                 /**< [in] chunk to start search at */
   const check_t check_fct,            /**< [in] compare function */
   const scope_e scope = scope_e::ALL, /**< [in] code parts to consider for search */
   const dir_e   dir   = dir_e::AFTER, /**< [in] search direction */
   const bool    cond  = true          /**< [in] success condition */
);


/***************************************************************************//**
 * \brief search a chunk of a given category in a chunk list
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * This function is a specialization of chunk_search.
 *
 * @retval nullptr    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
static chunk_t *chunk_search_type(
   chunk_t         *cur,                 /**< [in] chunk to start search at */
   const c_token_t type,                 /**< [in] category to search for */
   const scope_e   scope = scope_e::ALL, /**< [in] code parts to consider for search */
   const dir_e     dir   = dir_e::AFTER  /**< [in] search direction */
);


/***************************************************************************//**
 * \brief search a chunk of a given type and level
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * This function is a specialization of chunk_search.
 *
 * @retval nullptr    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
chunk_t *chunk_search_typelevel(
   chunk_t   *cur,                 /**< [in] chunk to start search at */
   c_token_t type,                 /**< [in] category to search for */
   scope_e   scope = scope_e::ALL, /**< [in] code parts to consider for search */
   dir_e     dir   = dir_e::AFTER, /**< [in] search direction */
   int       level = -1            /**< {in]  */
);


/***************************************************************************//**
 * \brief searches a chunk that is non-NEWLINE, non-comment and non-preprocessor
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * @retval nullptr    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
static chunk_t *chunk_get_ncnlnp(
   chunk_t       *cur,                 /**< [in] chunk to start search at */
   const scope_e scope = scope_e::ALL, /**< [in] code parts to consider for search */
   const dir_e   dir   = dir_e::AFTER  /**< [in] search direction */
);


/***************************************************************************//**
 * \brief searches a chunk that holds a given string
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * @retval nullptr    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
chunk_t *chunk_search_str(
   chunk_t    *cur,  /**< [in] chunk to start search at */
   const char *str,  /**< [in] string to search for */
   size_t     len,   /**< [in] length of string */
   scope_e    scope, /**< [in] code parts to consider for search */
   dir_e      dir,   /**< [in] search direction */
   int        level  /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
);


/***************************************************************************//**
 * \brief Add a new chunk after the given position in a chunk list
 *
 * \note If ref is nullptr:
 *       add at the head of the chunk list if position is BEFOR
 *       add at the tail of the chunk list if position is AFTER
 *
 * @return pointer to the added chunk
 ******************************************************************************/
static chunk_t *chunk_add(
   const chunk_t *pc_in,     /**< {in] chunk to add to list */
   chunk_t       *ref,       /**< [in] insert position in list */
   const dir_e   pos = dir_e::AFTER /**< [in] insert before or after */
);


/***************************************************************************//**
 * \brief Determines which chunk search function to use
 *
 * Depending on the required search direction return a pointer
 * to the corresponding chunk search function.
 *
 * @return pointer to chunk search function
 ******************************************************************************/
static search_t select_search_fct(
   const dir_e dir = dir_e::AFTER /**< [in] search direction */
);


static void chunk_log(
   chunk_t    *pc,
   const char *text
);


static bool is_expected_string_and_level(
   chunk_t    *pc,
   const char *str,
   int        level,
   size_t     len
);


bool ptr_is_valid(const void * const ptr)
{
   return (ptr != nullptr);
}


bool ptrs_are_valid(const void * const ptr1, const void * const ptr2)
{
   return ((ptr1 != nullptr) &&
           (ptr2 != nullptr) );
}


bool ptrs_are_valid(const void * const ptr1, const void * const ptr2, const void * const ptr3)
{
   return ((ptr1 != nullptr) &&
           (ptr2 != nullptr) &&
           (ptr3 != nullptr) );
}


bool ptr_is_invalid(const void * const ptr)
{
   return (ptr == nullptr);
}


bool ptrs_are_invalid(const void * const ptr1, const void * const ptr2)
{
   return ((ptr1 == nullptr) ||
           (ptr2 == nullptr) );
}


bool ptrs_are_invalid(const void * const ptr1, const void * const ptr2, const void * const ptr3)
{
   return ((ptr1 == nullptr) ||
           (ptr2 == nullptr) ||
           (ptr3 == nullptr) );
}


bool is_valid(const chunk_t * const pc)
{
   return (pc != nullptr);
}


bool are_valid(const chunk_t * const pc1, const chunk_t * const pc2)
{
   return ((pc1 != nullptr) &&
           (pc2 != nullptr) );
}


bool is_invalid(const chunk_t *pc)
{
   return (pc == nullptr);
}


bool is_invalid_or_not_type(const chunk_t* const pc, const c_token_t type)
{
   return (is_invalid (pc) || is_not_type(pc, type));
}


bool is_invalid_or_type(const chunk_t* const pc, const c_token_t type)
{
   return (is_invalid(pc) || is_type(pc, type));
}


bool is_invalid_or_flag(const chunk_t* const pc, const UINT64 flags)
{
   return (is_invalid(pc) || is_flag(pc, flags));
}


bool are_invalid(const chunk_t * const pc1, const chunk_t * const pc2)
{
   return ((pc1 == nullptr) ||
           (pc2 == nullptr) );
}


bool are_invalid(const chunk_t * const pc1, const chunk_t * const pc2, const chunk_t * const pc3)
{
   return ((pc1 == nullptr) ||
           (pc2 == nullptr) ||
           (pc3 == nullptr) );
}


bool are_valid(const chunk_t * const pc1, const chunk_t * const pc2, const chunk_t * const pc3)
{
   return ((pc1 != nullptr) &&
           (pc2 != nullptr) &&
           (pc3 != nullptr) );
}


bool chunk_and_next_are_valid(const chunk_t * const pc)
{
   return((pc       != nullptr) &&
          (pc->next != nullptr) );
}


bool chunk_and_prev_are_valid(const chunk_t * const pc)
{
   return((pc       != nullptr) &&
          (pc->prev != nullptr) );
}


static search_t select_search_fct(const dir_e dir)
{
   return((dir == dir_e::AFTER) ? chunk_get_next : chunk_get_prev);
}


chunk_t *chunk_search_prev_cat(chunk_t *pc, const c_token_t cat)
{
   return(chunk_search_type(pc, cat, scope_e::ALL, dir_e::BEFORE));
}


chunk_t *chunk_search_next_cat(chunk_t *pc, const c_token_t cat)
{
   return(chunk_search_type(pc, cat, scope_e::ALL, dir_e::AFTER));
}


static void set_chunk(chunk_t *pc, c_token_t token, log_sev_t what, const char *str);


ChunkList_t g_cl; /** global chunk list */ /*\todo should become a local variable */


static chunk_t *chunk_search_type(chunk_t *cur, const c_token_t type,
      const scope_e scope, const dir_e dir)
{
   const search_t search_function = select_search_fct(dir);
   chunk_t        *pc             = cur;

   do                                /* loop over the chunk list */
   {
      pc = search_function(pc, scope); /* in either direction while */
   } while ((is_valid(pc)) &&    /* the end of the list was not reached yet */
            (pc->type != type));       /* and the demanded chunk was not found either */
   return(pc);                         /* the latest chunk is the searched one */
}


chunk_t *chunk_search_typelevel(chunk_t *cur, c_token_t type, scope_e scope,
      dir_e dir, int level)
{
   const search_t search_function = select_search_fct(dir);
   chunk_t        *pc             = cur;

   do                                  /* loop over the chunk list */
   {
      pc = search_function(pc, scope); /* in either direction while */
   } while ((is_valid(pc)) &&    /* the end of the list was not reached yet */
            (is_type_and_level(pc, type, level) == false));
   return(pc);                       /* the latest chunk is the searched one */
}


chunk_t *chunk_search_str(chunk_t *cur, const char *str, size_t len, scope_e scope, dir_e dir, int level)
{
   const search_t search_function = select_search_fct(dir);
   chunk_t        *pc             = cur;

   do                                  /* loop over the chunk list */
   {
      pc = search_function(pc, scope); /* in either direction while */
   } while ((is_valid(pc)) &&    /* the end of the list was not reached yet */
            (is_expected_string_and_level(pc, str, level, len) == false));
   return(pc);                         /* the latest chunk is the searched one */
}


static chunk_t *chunk_search(chunk_t *cur, const check_t check_fct,
      const scope_e scope, const dir_e dir, const bool cond)
{
   const search_t search_function = select_search_fct(dir);
   chunk_t        *pc             = cur;

   do                                  /* loop over the chunk list */
   {
      pc = search_function(pc, scope); /* in either direction while */
   } while ((is_valid(pc)) &&    /* the end of the list was not reached yet */
            (check_fct(pc) != cond));  /* and the demanded chunk was not found either */
   return(pc);                         /* the latest chunk is the searched one */
}


bool is_level(const chunk_t *pc, const size_t level)
{
   return (is_valid(pc) && pc->level == level);
}


bool is_type_and_level(const chunk_t *pc, const c_token_t type, const int level)
{
   return (( pc->type  ==         type ) && /* the type is as expected and */
           ((pc->level == (size_t)level) || /* the level is as expected or */
            (level     <             0)) ); /* we don't care about the level */
}


static bool is_expected_string_and_level(chunk_t *pc, const char *str, int level, size_t len)
{
   return ((pc->len()  == len                ) &&  /* the length is as expected and */
           (memcmp(str, pc->text(), len) == 0) &&  /* the strings equals */
           ((pc->level     == (size_t)level  )||   /* the level is as expected or */
            (level          <             0) ) );  /* we don't care about the level */
}


/* \todo the following function shall be made similar to the search functions */
chunk_t *chunk_first_on_line(chunk_t *pc)
{
   chunk_t *first = pc;

   while (((pc = chunk_get_prev(pc)) != nullptr) &&
           (chunk_is_newline(pc)     == false  ) )
   {
      first = pc;
   }
   return(first);
}


/* \todo maybe it is better to combine chunk_get_next and chunk_get_prev
 * into a common function However this should be done with the preprocessor
 * to avoid addition check conditions that would be evaluated in the
 * while loop of the calling function */
chunk_t *chunk_get_next(chunk_t *cur, const scope_e scope)
{
   if (is_invalid(cur)) { return(cur); }

   chunk_t *pc = g_cl.GetNext(cur);
   if (is_invalid(pc) || (scope == scope_e::ALL))
   {
      return(pc);
   }
   if (cur->flags & PCF_IN_PREPROC)
   {
      /* If in a preproc, return nullptr if trying to leave */
      if ((pc->flags & PCF_IN_PREPROC) == 0)
      {
         return((chunk_t *)nullptr);
      }
      return(pc);
   }
   /* Not in a preproc, skip any preproc */
   while (is_flag(pc, PCF_IN_PREPROC))
   {
      pc = g_cl.GetNext(pc);
   }
   return(pc);
}


chunk_t *chunk_get_prev(chunk_t *cur, const scope_e scope)
{
   retval_if(is_invalid(cur), cur);

   chunk_t *pc = g_cl.GetPrev(cur);
   if (is_invalid(pc) || (scope == scope_e::ALL))
   {
      return(pc);
   }
   if (cur->flags & PCF_IN_PREPROC)
   {
      /* If in a preproc, return nullptr if trying to leave */
      if ((pc->flags & PCF_IN_PREPROC) == 0)
      {
         return((chunk_t *)nullptr);
      }
      return(pc);
   }
   /* Not in a preproc, skip any preproc */
   while (is_flag(pc, PCF_IN_PREPROC))
   {
      pc = g_cl.GetPrev(pc);
   }
   return(pc);
}


chunk_t *chunk_get_head(void)
{
   return(g_cl.GetHead());
}


chunk_t *chunk_get_tail(void)
{
   return(g_cl.GetTail());
}


chunk_t *chunk_dup(const chunk_t * const pc_in)
{
   chunk_t *const pc = new chunk_t; /* Allocate a new chunk */
   if (is_invalid(pc))
   {
      LOG_FMT(LERR, "Failed to allocate memory\n");
      exit(EXIT_FAILURE);
   }

   /* Copy all fields and then init the entry */
   *pc = *pc_in;
   g_cl.InitEntry(pc);

   return(pc);
}


static void chunk_log_msg(chunk_t *chunk, const log_sev_t log, const char *str)
{
   LOG_FMT(log, "%s %zu:%zu '%s' [%s]",
           str, chunk->orig_line, chunk->orig_col, chunk->text(),
           get_token_name(chunk->type));
}


static void chunk_log(chunk_t *pc, const char *text)
{
   if ((is_valid(pc)                          ) &&
       (cpd.unc_stage != unc_stage_e::TOKENIZE) &&
       (cpd.unc_stage != unc_stage_e::CLEANUP ) )
   {
      const log_sev_t log = LCHUNK;
      chunk_t *prev = chunk_get_prev(pc);
      chunk_t *next = chunk_get_next(pc);

      chunk_log_msg(pc, log, text);

      if (are_valid(prev, next)) { chunk_log_msg(prev, log, " @ between");
                                   chunk_log_msg(next, log, " and"      ); }
      else if (is_valid(next))   { chunk_log_msg(next, log, " @ before" ); }
      else if (is_valid(prev))   { chunk_log_msg(prev, log, " @ after"  ); }

      LOG_FMT(log, " stage=%d", cpd.unc_stage);
      log_func_stack_inline(log);
   }
}


static chunk_t *chunk_add(const chunk_t *pc_in, chunk_t *ref, const dir_e pos)
{
   chunk_t *pc = chunk_dup(pc_in);
   if (is_valid(pc))
   {
      switch(pos)
      {
         case(dir_e::AFTER ): (is_valid(ref)) ? g_cl.AddAfter (pc, ref) : g_cl.AddTail(pc); break;
         case(dir_e::BEFORE): (is_valid(ref)) ? g_cl.AddBefore(pc, ref) : g_cl.AddTail(pc); break; // \todo should be AddHead but tests fail
         default:              /* invalid position indication */                            break;
      }
      chunk_log(pc, "chunk_add");
   }
   return(pc); /* \todo what is returned here? */
}


chunk_t *chunk_add_after(const chunk_t *pc_in, chunk_t *ref)
{
   return(chunk_add(pc_in, ref, dir_e::AFTER));
}


chunk_t *chunk_add_before(const chunk_t *pc_in, chunk_t *ref)
{
   return(chunk_add(pc_in, ref, dir_e::BEFORE));
}


void chunk_del(chunk_t *pc)
{
   chunk_log(pc, "chunk_del");
   g_cl.Pop(pc);
   delete pc;
}


void chunk_move_after(chunk_t *pc_in, chunk_t *ref)
{
   LOG_FUNC_ENTRY();

   if (is_valid(pc_in) &&
       is_valid(ref  ) )
   {
      g_cl.Pop     (pc_in     );
      g_cl.AddAfter(pc_in, ref);

      /* HACK: Adjust the original column */
      pc_in->column       = ref->column + (size_t)space_col_align(ref, pc_in);
      pc_in->orig_col     = static_cast<UINT32>(pc_in->column);
      pc_in->orig_col_end = pc_in->orig_col + pc_in->len();
   }
}


chunk_t *get_prev_non_pp(chunk_t *pc, const scope_e scope)
{
   return (chunk_search(pc, chunk_is_preproc, scope, dir_e::BEFORE, false));
}


chunk_t *get_prev_fparen_open(chunk_t *pc, const scope_e scope)
{
   return (chunk_search(pc, chunk_is_fparen_open, scope, dir_e::BEFORE, true));
}


chunk_t *get_next_function(chunk_t *pc, scope_e scope)
{
   return (chunk_search(pc, chunk_is_function, scope, dir_e::AFTER, true));
}


chunk_t *get_next_class(chunk_t *pc)
{
   return(chunk_get_next(chunk_search_next_cat(pc, CT_CLASS)));
}


chunk_t *get_prev_category(chunk_t *pc)
{
   return(chunk_search_prev_cat(pc, CT_OC_CATEGORY));
}


chunk_t *get_next_scope(chunk_t *pc)
{
   return(chunk_search_next_cat(pc, CT_OC_SCOPE));
}


chunk_t *get_prev_oc_class(chunk_t *pc)
{
   return(chunk_search_prev_cat(pc, CT_OC_CLASS));
}


chunk_t *chunk_get_next_nl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_newline, scope, dir_e::AFTER, true));
}


chunk_t *chunk_get_prev_nl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_newline, scope, dir_e::BEFORE, true));
}


chunk_t *chunk_get_next_nnl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_newline, scope, dir_e::AFTER, false));
}


chunk_t *chunk_get_prev_nnl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_newline, scope, dir_e::BEFORE, false));
}


chunk_t *chunk_get_next_ncnl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_or_newline, scope, dir_e::AFTER, false));
}


chunk_t *chunk_get_next_ncnlnp(chunk_t *cur, scope_e scope)
{
   return(chunk_get_ncnlnp(cur, scope, dir_e::AFTER));
}


chunk_t *chunk_get_prev_ncnlnp(chunk_t *cur, scope_e scope)
{
   return(chunk_get_ncnlnp(cur, scope, dir_e::BEFORE));
}


chunk_t *chunk_get_next_nblank(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_newline_or_blank, scope, dir_e::AFTER, false));
}


chunk_t *chunk_get_prev_nblank(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_newline_or_blank, scope, dir_e::BEFORE, false));
}


chunk_t *chunk_get_next_nc(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment, scope, dir_e::AFTER, false));
}


chunk_t *chunk_get_next_nisq(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_balanced_square, scope, dir_e::AFTER, false));
}


chunk_t *chunk_get_prev_ncnl(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment_or_newline, scope, dir_e::BEFORE, false));
}


chunk_t *chunk_get_prev_nc(chunk_t *cur, scope_e scope)
{
   return(chunk_search(cur, chunk_is_comment, scope, dir_e::BEFORE, false));
}


chunk_t *chunk_get_next_nvb(chunk_t *cur, const scope_e scope)
{
   return(chunk_search(cur, chunk_is_vbrace, scope, dir_e::AFTER, false));
}


chunk_t *chunk_get_prev_nvb(chunk_t *cur, const scope_e scope)
{
   return(chunk_search(cur, chunk_is_vbrace, scope, dir_e::BEFORE, false));
}


chunk_t *chunk_get_next_type(chunk_t *cur, c_token_t type, int level, scope_e scope)
{
   return(chunk_search_typelevel(cur, type, scope, dir_e::AFTER, level));
}


chunk_t *chunk_get_prev_type(chunk_t *cur, c_token_t type, int level, scope_e scope)
{
   return(chunk_search_typelevel(cur, type, scope, dir_e::BEFORE, level));
}


chunk_t *chunk_get_next_str(chunk_t *cur, const char *str, size_t len, int level, scope_e scope)
{
   return(chunk_search_str(cur, str, len, scope, dir_e::AFTER, level));
}


chunk_t *chunk_get_prev_str(chunk_t *cur, const char *str, size_t len, int level, scope_e scope)
{
   return(chunk_search_str(cur, str, len, scope, dir_e::BEFORE, level));
}


bool chunk_is_newline_between(chunk_t *start, chunk_t *end)
{
   for (chunk_t *pc = start; pc != end; pc = chunk_get_next(pc))
   {
      retval_if(chunk_is_newline(pc), true);
   }
   return(false);
}


void chunk_swap(chunk_t *pc1, chunk_t *pc2)
{
   g_cl.Swap(pc1, pc2);
}


void chunk_swap_lines(chunk_t *pc1, chunk_t *pc2)
{
   /* to swap lines we need to find the first chunk of the lines */
   pc1 = chunk_first_on_line(pc1);
   pc2 = chunk_first_on_line(pc2);

   return_if(are_invalid(pc1, pc2) || (pc1 == pc2));

   /* Example start:
    * ? - start1 - a1 - b1 - nl1 - ? - ref2 - start2 - a2 - b2 - nl2 - ?
    *      ^- pc1                              ^- pc2 */
   chunk_t *ref2 = chunk_get_prev(pc2);

   /* Move the line started at pc2 before pc1 */
   while (( is_valid  (pc2)) &&
          (!chunk_is_newline(pc2)) )
   {
      chunk_t *tmp = chunk_get_next(pc2);
      g_cl.Pop(pc2);
      g_cl.AddBefore(pc2, pc1);
      pc2 = tmp;
   }

   /* Should now be:
    * ? - start2 - a2 - b2 - start1 - a1 - b1 - nl1 - ? - ref2 - nl2 - ?
    *                         ^- pc1                              ^- pc2 */

   /* Now move the line started at pc1 after ref2 */
   while (( is_valid  (pc1)) &&
          (!chunk_is_newline(pc1)) )
   {
      chunk_t *tmp = chunk_get_next(pc1);
      g_cl.Pop(pc1);
      if (is_valid(ref2)){ g_cl.AddAfter(pc1, ref2); }
      else               { g_cl.AddHead (pc1);       }
      ref2 = pc1;
      pc1  = tmp;
   }

   /* Should now be:
    * ? - start2 - a2 - b2 - nl1 - ? - ref2 - start1 - a1 - b1 - nl2 - ?
    *                         ^- pc1                              ^- pc2 */

   /* pc1 and pc2 should be the newlines for their lines.
    * swap the chunks and the nl_count so that the spacing remains the same. */
   if (are_valid(pc1, pc2))
   {
      SWAP(pc1->nl_count, pc2->nl_count);
      chunk_swap(pc1, pc2);
   }
}


static void set_chunk(chunk_t *pc, c_token_t token, log_sev_t what, const char *str)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   c_token_t       *where;
   const c_token_t *type;
   const c_token_t *parent_type;
   switch (what)
   {
      case (LSETTYP): where = &pc->type;  type = &token;    parent_type = &pc->ptype; break;
      case (LSETPAR): where = &pc->ptype; type = &pc->type; parent_type = &token;     break;
      default:  /* unexpected type */ return;
   }

   if (*where != token)
   {
      LOG_FMT(what, "%s: %zu:%zu '%s' %s:%s => %s:%s",
              str, pc->orig_line, pc->orig_col, pc->text(),
              get_token_name(pc->type), get_token_name(pc->ptype),
              get_token_name(*type), get_token_name(*parent_type));
      log_func_stack_inline(what);
      *where = token;
   }
}


void set_type_and_ptype(chunk_t *pc, c_token_t type, c_token_t parent)
{
   set_type (pc, type  );
   set_ptype(pc, parent);
}


void set_ptype_and_flag(chunk_t *pc, c_token_t type, UINT64 flag)
{
   set_ptype      (pc, type);
   chunk_flags_set(pc, flag);
}


void set_type(chunk_t *pc, c_token_t tt)
{
   LOG_FUNC_CALL();
   set_chunk(pc, tt, LSETTYP, "set_chunk_type");
}


void set_ptype(chunk_t *pc, c_token_t pt)
{
   LOG_FUNC_CALL();
   set_chunk(pc, pt, LSETPAR, "set_chunk_parent");
}


void chunk_flags_set(chunk_t *pc, UINT64 set_bits)
{
   LOG_FUNC_CALL();
   chunk_flags_update(pc, 0, set_bits);
}


void chunk_flags_clr(chunk_t *pc, UINT64 clr_bits)
{
   LOG_FUNC_CALL();
   chunk_flags_update(pc, clr_bits, 0);
}


void chunk_flags_update(chunk_t *pc, UINT64 clr_bits, UINT64 set_bits)
{
   LOG_FUNC_ENTRY();

   if (is_valid(pc))
   {
      const UINT64 nflags = (pc->flags & ~clr_bits) | set_bits;
      if (pc->flags != nflags)
      {
         LOG_FMT(LSETFLG, "set_chunk_flags: %016" PRIx64 "^%016" PRIx64 "=%016" PRIx64 " %zu:%zu '%s' %s:%s",
                 pc->flags, pc->flags ^ nflags, nflags, pc->orig_line, pc->orig_col, pc->text(),
                 get_token_name(pc->type), get_token_name(pc->ptype));
         log_func_stack_inline(LSETFLG);
         pc->flags = nflags;
      }
   }
}


static chunk_t *chunk_get_ncnlnp(chunk_t *cur, const scope_e scope, const dir_e dir)
{
   chunk_t *pc = cur;
   pc = (chunk_is_preproc(pc) == true) ?
        chunk_search(pc, chunk_is_comment_or_newline_in_preproc, scope, dir, false) :
        chunk_search(pc, chunk_is_comment_newline_or_preproc,    scope, dir, false);
   return(pc);
}


bool chunk_is_forin(chunk_t *pc)
{
   if ((cpd.lang_flags & LANG_OC         ) &&
       (is_type(pc, CT_SPAREN_OPEN)) )
   {
      chunk_t *prev = chunk_get_prev_ncnl(pc);
      if(is_type(prev, CT_FOR))
      {
         chunk_t *next = pc;
         while(is_not_type(next, 2, CT_IN, CT_SPAREN_CLOSE))
         {
            next = chunk_get_next_ncnl(next);
            retval_if(is_type(next, CT_IN), true);
         }
      }
   }
   return(false);
}


bool is_type_and_ptype(chunk_t *pc, c_token_t type, c_token_t parent)
{
   return( is_valid(pc)         &&
          (pc->type  == type  ) &&
          (pc->ptype == parent) );
}


bool is_type_and_not_ptype(chunk_t *pc, c_token_t type, c_token_t parent)
{
   return( is_valid(pc)         &&
          (pc->type  == type  ) &&
          (pc->ptype != parent) );
}


bool any_is_type(const chunk_t *pc1, const chunk_t *pc2, c_token_t type)
{
   return( is_type(pc1, type) ||
           is_type(pc2, type) );
}


bool any_is_type(const chunk_t *pc1, c_token_t type1, const chunk_t *pc2, c_token_t type2)
{
   return( is_type(pc1, type1) ||
           is_type(pc2, type2) );
}


bool are_types(const chunk_t *pc1, const chunk_t *pc2, c_token_t type)
{
   return( is_type(pc1, type) &&
           is_type(pc2, type) );
}


bool are_ptypes(const chunk_t *pc1, const chunk_t *pc2, c_token_t type)
{
   return( is_ptype(pc1, type) &&
           is_ptype(pc2, type) );
}


bool are_types(const chunk_t *pc1, c_token_t type1, const chunk_t *pc2, c_token_t type2)
{
   return( is_type(pc1, type1) &&
           is_type(pc2, type2) );
}


bool are_ptypes(const chunk_t *pc1, c_token_t type1, const chunk_t *pc2, c_token_t type2)
{
   return( is_ptype(pc1, type1) &&
           is_ptype(pc2, type2) );
}


bool is_type(const chunk_t *pc, const c_token_t type)
{
   return(is_valid(pc) && (pc->type == type));
}


bool is_type(const chunk_t *pc, const c_token_t type1, const c_token_t type2)
{
   return( is_valid(pc) &&
          ((pc->type == type1) || (pc->type == type2)) );
}

#if 1
bool is_type(const chunk_t *pc, const c_token_t type1, const c_token_t type2,
                                const c_token_t type3)
{
   return(is_valid(pc)           &&
          ((pc->type == type1) ||
           (pc->type == type2) ||
           (pc->type == type3) ) );
}

bool is_type(const chunk_t *pc, const c_token_t type1, const c_token_t type2,
                                const c_token_t type3, const c_token_t type4)
{
   return(is_valid(pc)           &&
          ((pc->type == type1) ||
           (pc->type == type2) ||
           (pc->type == type3) ||
           (pc->type == type4) ) );
}
#endif

bool is_ptype(const chunk_t * const pc, c_token_t parent)
{
   return(is_valid(pc) && (pc->ptype == parent));
}


bool is_only_first_type(const chunk_t *pc1, c_token_t type1, const chunk_t *pc2, c_token_t type2)
{
   return(is_type    (pc1, type1) &&
          is_not_type(pc2, type2) );

}


bool is_not_type(const chunk_t * const pc, c_token_t c_token)
{
   return(is_valid(pc) && (pc->type != c_token));
}


/* \todo check if using a variadic function has a large performance
 * penalty. If so use several overloaded functions, with varying
 * number of parameters */
bool is_type(const chunk_t *pc, int count, ... )
{
   va_list args;          /* define     argument list */
   va_start(args, count); /* initialize argument list */

   bool result = false;
   if(is_valid(pc))
   {
      for( ; count > 0; --count)
      {
         c_token_t type = (c_token_t)va_arg(args, int);

         if(pc->type == type)
         {
            result = true;
            break;
         }
      }
   }
   va_end(args);
   return result;
}


/* todo combine with is_type */
bool is_ptype(const chunk_t * const pc, int count, ... )
{
   va_list args;          /* define     argument list */
   va_start(args, count); /* initialize argument list */

   bool result = false;
   if(is_valid(pc))
   {
      for( ; count > 0; --count)
      {
         c_token_t type = (c_token_t)va_arg(args, int);
         if(pc->ptype == type)
         {
            result = true;
            break;
         }
      }
   }
   va_end(args);
   return result;
}


/* \todo combine with chunk_is_not_parent_type */
bool is_not_type(const chunk_t * const pc, int count, ... )
{
   va_list args;          /* define     argument list */
   va_start(args, count); /* initialize argument list */

   bool result = false;
   if(is_valid(pc))
   {
      result = true;
      for( ; count > 0; --count)
      {
         c_token_t type = (c_token_t)va_arg(args, int);
         if(pc->type == type)
         {
           result = false;
           break;
         }
      }
   }
   va_end(args);
   return result;
}


bool is_not_ptype(chunk_t *pc, int count, ... )
{
   va_list args;          /* define  argument list */
   va_start(args, count); /* initialize argument list */

   bool result = false;
   if(is_valid(pc))
   {
      result = true;
      for( ; count > 0; --count)
      {
         c_token_t type = (c_token_t)va_arg(args, int);
         if(pc->ptype == type)
         {
            result = false;
            break;
         }
      }
   }
   va_end(args);
   return result;
}


bool is_flag(const chunk_t * const pc, const UINT64 flags)
{
   return (is_valid(pc) && (pc->flags & flags) == flags);
}


bool is_not_flag(const chunk_t * const pc, const UINT64 flags)
{
   return (is_valid(pc) && (pc->flags & flags) == 0);
}


chunk_t *chunk_skip_to_match(chunk_t *cur, scope_e scope)
{
   if(is_type(cur, 8, CT_PAREN_OPEN,  CT_SPAREN_OPEN,
                      CT_FPAREN_OPEN, CT_TPAREN_OPEN,
                      CT_BRACE_OPEN,  CT_VBRACE_OPEN,
                      CT_ANGLE_OPEN,  CT_SQUARE_OPEN))
   {
      return(chunk_get_next_type(cur, get_inverse_type(cur->type),
                                 (int)cur->level, scope));
   }
   return(cur);
}


chunk_t *chunk_skip_to_match_rev(chunk_t *cur, scope_e scope)
{
   if(is_type(cur, 8, CT_PAREN_CLOSE,  CT_SPAREN_CLOSE,
                      CT_FPAREN_CLOSE, CT_TPAREN_CLOSE,
                      CT_BRACE_CLOSE,  CT_VBRACE_CLOSE,
                      CT_ANGLE_CLOSE,  CT_SQUARE_CLOSE))
   {
      return(chunk_get_prev_type(cur, get_inverse_type(cur->type),
                                 (int)cur->level, scope));
   }
   return(cur);
}


bool chunk_is_function(chunk_t *pc)
{
   return(is_type(pc, 5, CT_FUNC_CLASS_DEF,   CT_FUNC_PROTO,
                         CT_FUNC_CLASS_PROTO, CT_FUNC_DEF,
                         CT_OC_MSG_DECL));
}


bool chunk_is_comment(chunk_t *pc)
{
   return(is_type(pc, CT_COMMENT_MULTI, CT_COMMENT, CT_COMMENT_CPP));
}


bool chunk_is_newline(chunk_t *pc)
{
   return(is_type(pc, CT_NEWLINE, CT_NL_CONT));
}


bool chunk_is_empty(chunk_t *pc)
{
   return(is_valid(pc) && (pc->len() == 0));
}


bool chunk_is_comment_or_newline(chunk_t *pc)
{
   return(chunk_is_comment(pc) ||
          chunk_is_newline(pc) );
}


bool chunk_is_balanced_square(chunk_t *pc)
{
   return(is_type(pc, CT_SQUARE_OPEN, CT_TSQUARE, CT_SQUARE_CLOSE));
}


bool chunk_is_preproc(chunk_t *pc)
{
   return((is_valid(pc)) && (pc->flags & PCF_IN_PREPROC));
}


bool chunk_is_no_preproc_type(chunk_t *pc)
{
   return ((pc->type < CT_PP_DEFINE) ||
           (pc->type > CT_PP_OTHER ) );
}


bool chunk_is_comment_or_newline_in_preproc(chunk_t * pc)
{
   return(chunk_is_preproc           (pc) &&
          chunk_is_comment_or_newline(pc) );
}


bool chunk_is_comment_newline_or_preproc(chunk_t *pc)
{
   return(chunk_is_comment_or_newline(pc) ||
          chunk_is_preproc           (pc) );
}


bool chunk_is_comment_newline_or_blank(chunk_t * pc)
{
   return(chunk_is_comment_or_newline(pc) ||
          chunk_is_empty             (pc) );
}


bool chunk_is_single_line_comment(const chunk_t * const pc)
{
   return(is_type(pc, CT_COMMENT, CT_COMMENT_CPP));
}


bool chunk_is_semicolon(const chunk_t * const pc)
{
   return(is_type(pc, CT_SEMICOLON, CT_VSEMICOLON));
}


bool chunk_is_var_type(const chunk_t * const pc)
{
   return(is_type(pc, 8, CT_PTR_TYPE,  CT_TYPE,
                         CT_DC_MEMBER, CT_BYREF,
                         CT_QUALIFIER, CT_STRUCT,
                         CT_UNION,     CT_ENUM));
}


bool chunk_is_member(chunk_t *pc)
{
   return(is_type(pc, CT_DC_MEMBER, CT_MEMBER));
}

bool chunk_is_closing_brace(chunk_t *pc)
{
   return(is_type(pc, CT_BRACE_CLOSE, CT_VBRACE_CLOSE));
}


bool chunk_is_opening_brace(chunk_t *pc)
{
   return(is_type(pc, CT_BRACE_OPEN, CT_VBRACE_OPEN));
}


bool chunk_is_vbrace(chunk_t *pc)
{
   return(is_type(pc, CT_VBRACE_CLOSE, CT_VBRACE_OPEN));
}


bool chunk_is_fparen_open(chunk_t *pc)
{
   return(is_type(pc, CT_FPAREN_OPEN));
}


bool chunk_is_paren_open(chunk_t *pc)
{
   return(is_type(pc, CT_PAREN_OPEN,  CT_SPAREN_OPEN,
                      CT_TPAREN_OPEN, CT_FPAREN_OPEN));
}


bool chunk_is_paren_close(chunk_t *pc)
{
   return(is_type(pc, CT_PAREN_CLOSE,  CT_SPAREN_CLOSE,
                      CT_TPAREN_CLOSE, CT_FPAREN_CLOSE));
}


bool is_str(chunk_t *pc, const char *str, size_t len)
{
   return(is_valid(pc) &&  (pc->len() == len) && /* token size equals size parameter */
          (memcmp(pc->text(), str, len) == 0) ); /* strings are equal considering case */
}


/* \todo determine string size and remove len parameter */
bool chunk_is_str_case(chunk_t *pc, const char *str, size_t len)
{
   return(is_valid(pc) &&  (pc->len() == len) && /* token size equals size parameter */
          (strncasecmp(pc->text(), str, len) == 0)); /* strings are equal ignoring case */
}


bool chunk_is_word(chunk_t *pc)
{
   return(is_valid(pc) && (pc->len() >= 1u) &&
          (CharTable::IsKW1((size_t)pc->str[0])) );
}

#if 0
bool chunk_is_word(chunk_t *pc)
{
   return(is_valid(pc) && pc->type == CT_WORD);
}
#endif


bool chunk_is_star(chunk_t *pc)
{
   return(is_valid(pc) && (pc->len() == 1) &&
          (pc->str[0] == '*'            ) &&
          (pc->type   != CT_OPERATOR_VAL) );
}


bool chunk_is_addr(chunk_t *pc)
{
   if (  (is_valid(pc)           ) &&
       ( (pc->type   == CT_BYREF ) ||
        ((pc->len()  ==  1       ) &&
         (pc->str[0] == '&'      ) &&
         (pc->type   != CT_OPERATOR_VAL) )))
   {
      chunk_t *prev = chunk_get_prev(pc);

      if ((pc->flags & PCF_IN_TEMPLATE) &&
          (is_type(prev, CT_COMMA, CT_ANGLE_OPEN)) )
      {
//      (pos == dir_e::AFTER) ? g_cl.AddAfter(pc, ref) : g_cl.AddBefore(pc, ref);
         return(false);
      }
      return(true);
   }
   return(false);
}


// ms compilers for C++/CLI and WinRT use '^' instead of '*' for marking up reference types vs pointer types
bool chunk_is_msref(chunk_t *pc)
{
   return((cpd.lang_flags & LANG_CPP    ) &&
          (is_valid(pc)) && (pc->len() == 1) &&
          (pc->str[0] == '^'            ) &&
          (pc->type   != CT_OPERATOR_VAL) );
}


bool chunk_is_ptr_operator(chunk_t *pc)
{
   return(chunk_is_star (pc) ||
          chunk_is_addr (pc) ||
          chunk_is_msref(pc) );
}


bool chunk_same_preproc(chunk_t *pc1, chunk_t *pc2)
{
   return( are_invalid(pc1, pc2) ||
          ((pc1->flags & PCF_IN_PREPROC) == (pc2->flags & PCF_IN_PREPROC)));
}


bool chunk_different_preproc(chunk_t *pc1, chunk_t *pc2)
{
   return( are_valid(pc1, pc2) ||
          ((pc1->flags & PCF_IN_PREPROC) != (pc2->flags & PCF_IN_PREPROC)));
}


bool chunk_safe_to_del_nl(chunk_t *nl)
{
   chunk_t *tmp = chunk_get_prev(nl);
   if (is_type(tmp, CT_COMMENT_CPP))
   {
      return(false);
   }
   else
   {
      return(chunk_same_preproc(chunk_get_prev(nl), chunk_get_next(nl)));
   }
}
