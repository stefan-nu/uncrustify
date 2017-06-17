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
 *  parenthesis = ( ) aka "round brace"
 *  brace       = { } aka "curly brace"
 *  bracket     = [ ] aka "square bracket"
 */

/**
 * abbreviations use
 *
 * vbrace = virtual brace
 * rbrace = real brace
 */


/***************************************************************************//**
 * @brief prototype for a function that searches through a chunk list
 *
 * @note this typedef defines the function type "search_t"
 * for a function pointer of type
 * chunk_t* function(chunk_t* cur, scope_t scope)
 ******************************************************************************/
typedef chunk_t* (*search_t)(chunk_t* cur, const scope_e scope);


typedef ListManager<chunk_t> ChunkList_t;


/***************************************************************************//**
 * \brief search a chunk of a given category in a chunk list
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * This function is a specialization of chunk_search.
 *
 * @retval nullptr - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
static chunk_t* chunk_search_type(
   chunk_t*        cur,                  /**< [in] chunk to start search at */
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
 * @retval nullptr - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
chunk_t* chunk_search_typelevel(
   chunk_t*        cur,                  /**< [in] chunk to start search at */
   const c_token_t type,                 /**< [in] category to search for */
   const scope_e   scope = scope_e::ALL, /**< [in] code parts to consider for search */
   const dir_e     dir   = dir_e::AFTER, /**< [in] search direction */
   const int32_t   level = ANY_LEVEL     /**< {in] -1 or ANY_LEVEL or the level to match */
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
static chunk_t* get_ncnlnp(
   chunk_t*      cur,                  /**< [in] chunk to start search at */
   const scope_e scope = scope_e::ALL, /**< [in] code parts to consider for search */
   const dir_e   dir   = dir_e::AFTER  /**< [in] search direction */
);


/***************************************************************************//**
 * \brief searches a chunk that holds a given string
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * @retval nullptr - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
chunk_t* chunk_search_str(
   chunk_t*       cur,   /**< [in] chunk to start search at */
   const char*    str,   /**< [in] string to search for */
   const uint32_t len,   /**< [in] length of string */
   const scope_e  scope = scope_e::ALL, /**< [in] code parts to consider for search */
   const dir_e    dir   = dir_e::AFTER, /**< [in] search direction */
   const int32_t  level = ANY_LEVEL     /**< [in] -1 or ANY_LEVEL or the level to match */
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
static chunk_t* chunk_add(
   const chunk_t* pc_in,             /**< {in] chunk to add to list */
   chunk_t*       ref,               /**< [in] insert position in list */
   const dir_e    pos = dir_e::AFTER /**< [in] insert before or after */
);


static void chunk_log(
   chunk_t*    pc,  /**< [in]  */
   const char* text /**< [in]  */
);


static bool is_expected_string_and_level(
   chunk_t*       pc,    /**< [in]  */
   const char*    str,   /**< [in]  */
   const int32_t  level, /**< [in]  */
   const uint32_t len    /**< [in]  */
);


bool ptr_is_valid(const void* const ptr)
{
   return(ptr != nullptr);
}


bool ptrs_are_valid(const void* const ptr1, const void* const ptr2)
{
   return((ptr1 != nullptr) &&
          (ptr2 != nullptr) );
}


bool ptrs_are_valid(const void* const ptr1, const void* const ptr2,
                    const void* const ptr3)
{
   return((ptr1 != nullptr) &&
          (ptr2 != nullptr) &&
          (ptr3 != nullptr) );
}


bool ptr_is_invalid(const void* const ptr)
{
   return(ptr == nullptr);
}


bool ptrs_are_invalid(const void* const ptr1, const void* const ptr2)
{
   return((ptr1 == nullptr) ||
          (ptr2 == nullptr) );
}


bool ptrs_are_invalid(const void* const ptr1, const void* const ptr2,
                      const void* const ptr3)
{
   return ((ptr1 == nullptr) ||
           (ptr2 == nullptr) ||
           (ptr3 == nullptr) );
}


bool is_valid(const chunk_t* const pc)
{
   return(pc != nullptr);
}


bool are_valid(const chunk_t* const pc1, const chunk_t* const pc2)
{
   return ((pc1 != nullptr) &&
           (pc2 != nullptr) );
}


bool is_invalid(const chunk_t* const pc)
{
   return(pc == nullptr);
}


bool is_invalid_or_not_type(const chunk_t* const pc, const c_token_t type)
{
   return (is_invalid (pc) || not_type(pc, type));
}


bool is_invalid_or_type(const chunk_t* const pc, const c_token_t type)
{
   return (is_invalid(pc) || is_type(pc, type));
}


bool is_invalid_or_ptype(const chunk_t* const pc, const c_token_t ptype)
{
   return (is_invalid(pc) || is_ptype(pc, ptype));
}


bool is_invalid_or_flag(const chunk_t* const pc, const uint64_t flags)
{
   return (is_invalid(pc) || is_flag(pc, flags));
}


bool are_invalid(const chunk_t* const pc1, const chunk_t* const pc2)
{
   return ((pc1 == nullptr) ||
           (pc2 == nullptr) );
}


bool are_invalid(const chunk_t* const pc1, const chunk_t* const pc2,
                 const chunk_t* const pc3)
{
   return ((pc1 == nullptr) ||
           (pc2 == nullptr) ||
           (pc3 == nullptr) );
}


bool are_valid(const chunk_t* const pc1, const chunk_t* const pc2,
               const chunk_t* const pc3)
{
   return ((pc1 != nullptr) &&
           (pc2 != nullptr) &&
           (pc3 != nullptr) );
}


bool chunk_and_next_are_valid(const chunk_t* const pc)
{
   return((pc       != nullptr) &&
          (pc->next != nullptr) );
}


bool chunk_and_prev_are_valid(const chunk_t* const pc)
{
   return((pc       != nullptr) &&
          (pc->prev != nullptr) );
}


static void set_chunk(chunk_t* pc, c_token_t token, log_sev_t val, const char* str);


ChunkList_t g_cl; /** g_cl = global chunk list, \todo should become a local variable */


static chunk_t* chunk_search_type(chunk_t* cur, const c_token_t type,
      const scope_e scope, const dir_e dir)
{
   chunk_t* pc = cur;
   do                                 /* loop over the chunk list */
   {
      pc = chunk_get(pc, scope, dir); /* in either direction while */
   } while (not_type(pc, type));      /* and the demanded chunk was not found either */
   return(pc);                        /* the latest chunk is the searched one */
}


chunk_t* chunk_search_typelevel(chunk_t* cur, const c_token_t type,
      const scope_e scope, const dir_e dir, const int32_t level)
{
   chunk_t* pc = cur;
   do /* loop over the chunk list */
   {
      pc = chunk_get(pc, scope, dir);  /* in either direction while */
   } while ((is_valid(pc)) &&          /* the end of the list was not reached */
            (is_type_and_level(pc, type, level) == false)); /* and chunk not found yet */
   return(pc);                         /* the latest chunk is the searched one */
}


chunk_t* chunk_search_str(chunk_t* cur, const char* str, const uint32_t len,
      const scope_e scope, const dir_e dir, const int32_t level)
{
   chunk_t* pc = cur;
   do /* loop over the chunk list */
   {
      pc = chunk_get(pc, scope, dir);  /* in either direction while */
   } while ((is_valid(pc)) &&          /* the end of the list was not reached yet */
            (is_expected_string_and_level(pc, str, level, len) == false));
   return(pc);                         /* the latest chunk is the searched one */
}


chunk_t* chunk_search(chunk_t* cur, const check_t check_fct,
      const scope_e scope, const dir_e dir, const bool cond)
{
   chunk_t* pc = cur;
   do /* loop over the chunk list */
   {
      pc = chunk_get(pc, scope, dir);  /* in either direction while */
   } while ((is_valid(pc)) &&          /* the end of the list was not reached yet */
            (check_fct(pc) != cond));  /* and the demanded chunk was not found either */
   return(pc);                         /* the latest chunk is the searched one */
}


bool is_level(const chunk_t* pc, const uint32_t level)
{
   return (is_valid(pc) && pc->level == level);
}


bool exceeds_level(const chunk_t* pc, const uint32_t ref)
{
   return (is_valid(pc) && pc->level > ref);
}


bool is_type_and_level(const chunk_t* pc, const c_token_t type, const int32_t level)
{
   return (is_type(pc, type) && /* the chunk is valid and has the expected type */
           ((pc->level == (uint32_t)level) || /* the level is as expected or */
            (level     <             0)) ); /* we don't care about the level */
}


static bool is_expected_string_and_level(chunk_t* pc, const char* str,
      const int32_t level, const uint32_t len)
{
   return ((pc->len()  == len                ) &&  /* the length is as expected and */
           (memcmp(str, pc->text(), len) == 0) &&  /* the strings equals */
           ((pc->level     == (uint32_t)level  )||   /* the level is as expected or */
            (level          <             0) ) );  /* we don't care about the level */
}


chunk_t* get_first_on_line(chunk_t* pc)
{
   chunk_t* first = pc;
   while (((pc = chunk_get_prev(pc)) != nullptr) &&
           !is_nl(pc) )
   {
      first = pc;
   }
   return(first);
}


chunk_t* chunk_get(chunk_t* cur, const scope_e scope, const dir_e dir)
{
   retval_if(is_invalid(cur), cur);

   chunk_t* pc = g_cl.Get(cur, dir);
   retval_if((is_invalid(pc) || (scope == scope_e::ALL)), pc);

   if (is_preproc(cur))
   {
      /* If in a preproc, return nullptr if trying to leave */
      return(!is_preproc(pc)) ? (chunk_t*)nullptr : pc;
   }
   /* Not in a preproc, skip any preproc */
   while (is_preproc(pc))
   {
      pc = g_cl.Get(pc, dir);
   }
   return(pc);
}


chunk_t* chunk_get_next(chunk_t* cur, const scope_e scope)
{
   return chunk_get(cur, scope, dir_e::AFTER);
}


chunk_t* chunk_get_prev(chunk_t* cur, const scope_e scope)
{
   return chunk_get(cur, scope, dir_e::BEFORE);
}


chunk_t* chunk_get_head(void) { return(g_cl.GetHead()); }
chunk_t* chunk_get_tail(void) { return(g_cl.GetTail()); }


bool are_corresponding(chunk_t* chunk1, chunk_t* chunk2)
{
   return((chunk1->brace_level == chunk2->brace_level) &&
           is_ptype(chunk1, chunk2->ptype ) &&
           are_same_pp(chunk2, chunk1     ) );
}


chunk_t* chunk_dup(const chunk_t* const pc_in)
{
   chunk_t* const pc = new chunk_t; /* Allocate a new chunk */
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


static void chunk_log_msg(chunk_t* chunk, const log_sev_t log, const char* str)
{
   LOG_FMT(log, "%s %u:%u '%s' [%s]",
           str, chunk->orig_line, chunk->orig_col, chunk->text(),
           get_token_name(chunk->type));
}


static void chunk_log(chunk_t* pc, const char* text)
{
   if ((is_valid(pc)                          ) &&
       (cpd.unc_stage != unc_stage_e::TOKENIZE) &&
       (cpd.unc_stage != unc_stage_e::CLEANUP ) )
   {
      const log_sev_t log = LCHUNK;
      chunk_t* prev = chunk_get_prev(pc);
      chunk_t* next = chunk_get_next(pc);

      chunk_log_msg(pc, log, text);

      if (are_valid(prev, next)) { chunk_log_msg(prev, log, " @ between");
                                   chunk_log_msg(next, log, " and"      ); }
      else if (is_valid(next))   { chunk_log_msg(next, log, " @ before" ); }
      else if (is_valid(prev))   { chunk_log_msg(prev, log, " @ after"  ); }

      LOG_FMT(log, " stage=%d", (int32_t)cpd.unc_stage);
      log_func_stack_inline(log);
   }
}


static chunk_t* chunk_add(const chunk_t* pc_in, chunk_t* ref, const dir_e pos)
{
   chunk_t* pc = chunk_dup(pc_in);
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
   return(pc);
}


chunk_t* chunk_add_after(const chunk_t* pc_in, chunk_t* ref)
{
   return(chunk_add(pc_in, ref, dir_e::AFTER));
}


chunk_t* chunk_add_before(const chunk_t* pc_in, chunk_t* ref)
{
   return(chunk_add(pc_in, ref, dir_e::BEFORE));
}


void chunk_del(chunk_t* pc)
{
   chunk_log(pc, "chunk_del");
   g_cl.Pop(pc);
   delete pc;
}


void chunk_move_after(chunk_t* pc_in, chunk_t* ref)
{
   LOG_FUNC_ENTRY();
   if(are_valid(pc_in, ref))
   {
      g_cl.Pop     (pc_in     );
      g_cl.AddAfter(pc_in, ref);

      /* HACK: Adjust the original column */
      pc_in->column       = ref->column + space_col_align(ref, pc_in);
      pc_in->orig_col     = pc_in->column;
      pc_in->orig_col_end = pc_in->orig_col + pc_in->len();
   }
}


chunk_t* get_next_opening_vbrace(chunk_t* pc, const scope_e scope)
{
   return (chunk_search(pc, is_opening_vbrace, scope, dir_e::AFTER, true));
}


chunk_t* get_next_closing_vbrace(chunk_t* pc, const scope_e scope)
{
   return (chunk_search(pc, is_closing_vbrace, scope, dir_e::AFTER, true));
}


chunk_t* get_prev_non_pp(chunk_t* pc, const scope_e scope)
{
   return (chunk_search(pc, is_preproc, scope, dir_e::BEFORE, false));
}


chunk_t* get_prev_fparen_open(chunk_t* pc, const scope_e scope)
{
   return (chunk_search(pc, is_fparen_open, scope, dir_e::BEFORE, true));
}


chunk_t* get_next_function(chunk_t* pc, const scope_e scope)
{
   return (chunk_search(pc, chunk_is_function, scope, dir_e::AFTER, true));
}


chunk_t* get_next_class(chunk_t* pc)
{
   return(chunk_get_next(chunk_search_type(pc, CT_CLASS)));
}


chunk_t* get_prev_category(chunk_t* pc)
{
   return(chunk_search_type(pc, CT_OC_CATEGORY, scope_e::ALL, dir_e::BEFORE));
}


chunk_t* get_next_scope(chunk_t* pc)
{
   return(chunk_search_type(pc, CT_OC_SCOPE));
}


chunk_t* get_prev_oc_class(chunk_t* pc)
{
   return(chunk_search_type(pc, CT_OC_CLASS, scope_e::ALL, dir_e::BEFORE));
}


chunk_t* get_next_nl(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_nl, scope, dir_e::AFTER, true));
}


chunk_t* get_prev_nl(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_nl, scope, dir_e::BEFORE, true));
}


chunk_t* get_prev_comma(chunk_t* cur, const scope_e scope, const bool cond)
{
   return(chunk_search(cur, is_comma, scope, dir_e::BEFORE, cond));
}


chunk_t* get_next_nnl(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_nl, scope, dir_e::AFTER, false));
}


chunk_t* get_prev_nnl(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_nl, scope, dir_e::BEFORE, false));
}


chunk_t* get_next_ncnl(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_cmt_or_nl, scope, dir_e::AFTER, false));
}


chunk_t* get_next_ncnlnp(chunk_t* cur, const scope_e scope)
{
   return(get_ncnlnp(cur, scope, dir_e::AFTER));
}


chunk_t* get_prev_ncnlnp(chunk_t* cur, const scope_e scope)
{
   return(get_ncnlnp(cur, scope, dir_e::BEFORE));
}


chunk_t* get_next_nblank(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_cmt_nl_or_blank, scope, dir_e::AFTER, false));
}


chunk_t* get_prev_nblank(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_cmt_nl_or_blank, scope, dir_e::BEFORE, false));
}


chunk_t* get_next_nc(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_cmt, scope, dir_e::AFTER, false));
}


chunk_t* get_next_nisq(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_bal_square, scope, dir_e::AFTER, false));
}


chunk_t* get_prev_ncnl(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_cmt_or_nl, scope, dir_e::BEFORE, false));
}


chunk_t* get_prev_nc(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_cmt, scope, dir_e::BEFORE, false));
}


chunk_t* get_next_nvb(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_vbrace, scope, dir_e::AFTER, false));
}


chunk_t* get_prev_nvb(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_vbrace, scope, dir_e::BEFORE, false));
}


chunk_t* get_next_nptr(chunk_t* cur, const scope_e scope)
{
   return(chunk_search(cur, is_ptr, scope, dir_e::AFTER, false));
}


chunk_t* get_next_type(chunk_t* cur, const c_token_t type, const int32_t level,
      const scope_e scope)
{
   return(chunk_search_typelevel(cur, type, scope, dir_e::AFTER, level));
}


chunk_t* get_prev_type(chunk_t* cur, const c_token_t type, const int32_t level,
      const scope_e scope)
{
   return(chunk_search_typelevel(cur, type, scope, dir_e::BEFORE, level));
}


chunk_t* get_next_str(chunk_t* cur, const char* str, const uint32_t len,
                      const int32_t level, const scope_e scope)
{
   return(chunk_search_str(cur, str, len, scope, dir_e::AFTER, level));
}


chunk_t* get_prev_str(chunk_t* cur, const char* str, const uint32_t len,
                      const int32_t level, const scope_e scope)
{
   return(chunk_search_str(cur, str, len, scope, dir_e::BEFORE, level));
}


bool is_newline_between(chunk_t* start, chunk_t* end)
{
   for (chunk_t* pc = start; pc != end; pc = chunk_get_next(pc))
   {
      retval_if(is_nl(pc), true);
   }
   return(false);
}


void swap_chunks(chunk_t* pc1, chunk_t* pc2)
{
   g_cl.Swap(pc1, pc2);
}


void swap_lines(chunk_t* pc1, chunk_t* pc2)
{
   /* to swap lines we need to find the first chunk of the lines */
   pc1 = get_first_on_line(pc1);
   pc2 = get_first_on_line(pc2);

   return_if(are_invalid(pc1, pc2) || (pc1 == pc2));

   /* Example start:
    * ? - start1 - a1 - b1 - nl1 - ? - ref2 - start2 - a2 - b2 - nl2 - ?
    *      ^- pc1                              ^- pc2 */
   chunk_t* ref2 = chunk_get_prev(pc2);

   /* Move the line started at pc2 before pc1 */
   while (is_valid(pc2) && !is_nl(pc2))
   {
      chunk_t* tmp = chunk_get_next(pc2);
      g_cl.Pop(pc2);
      g_cl.AddBefore(pc2, pc1);
      pc2 = tmp;
   }

   /* Should now be:
    * ? - start2 - a2 - b2 - start1 - a1 - b1 - nl1 - ? - ref2 - nl2 - ?
    *                         ^- pc1                              ^- pc2 */

   /* Now move the line started at pc1 after ref2 */
   while (is_valid(pc1) && !is_nl(pc1))
   {
      chunk_t* tmp = chunk_get_next(pc1);
      g_cl.Pop(pc1);
      if (is_valid(ref2)){ g_cl.AddAfter(pc1, ref2); }
      else               { g_cl.AddHead (pc1);       }
      ref2 = pc1;
      pc1  = tmp;
   }

   /* Should now be:
    * ? - start2 - a2 - b2 - nl1 - ? - ref2 - start1 - a1 - b1 - nl2 - ?
    *                         ^- pc1                              ^- pc2 */

   /* pc1 and pc2 should be the newlines for their lines. swap the chunks
    * and the nl_count so that the spacing remains the same. */
   if (are_valid(pc1, pc2))
   {
      SWAP(pc1->nl_count, pc2->nl_count);
      swap_chunks(pc1, pc2);
   }
}


static void set_chunk(chunk_t* pc, c_token_t token, log_sev_t val, const char* str)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   c_token_t*       what; /* object to update */
   const c_token_t* newt; /* new value for chunk type */
   const c_token_t* newp; /* new value for chunk parent */
   switch (val)
   {
      case (LSETTYP): what = &pc->type;  newt = &token;    newp = &pc->ptype; break;
      case (LSETPAR): what = &pc->ptype; newt = &pc->type; newp = &token;     break;
      default:  /* unexpected type */ return;
   }

   if (*what != token)
   {
      LOG_FMT(val, "set %s: %u:%u '%s' %s:%s => %s:%s",
              str, pc->orig_line, pc->orig_col, pc->text(),
              get_token_name(pc->type), get_token_name(pc->ptype),
              get_token_name(*newt), get_token_name(*newp));
      log_func_stack_inline(val);
      *what = token;
   }
}


void set_type(chunk_t* pc, const c_token_t type)
{
   LOG_FUNC_CALL();
   set_chunk(pc, type, LSETTYP, "chunk_type");
}


void set_ptype(chunk_t* pc, const c_token_t type)
{
   LOG_FUNC_CALL();
   set_chunk(pc, type, LSETPAR, "chunk_parent");
}


void set_type_and_ptype(chunk_t* pc, const c_token_t type, const c_token_t parent)
{
   set_type (pc, type  );
   set_ptype(pc, parent);
}


void set_type_and_flag(chunk_t* pc, const c_token_t type, const uint64_t flag)
{
   set_type (pc, type);
   set_flags(pc, flag);
}


void set_ptype_and_flag(chunk_t* pc, const c_token_t type, const uint64_t flag)
{
   set_ptype(pc, type);
   set_flags(pc, flag);
}


uint64_t get_flags(chunk_t* pc, const uint64_t mask)
{
   return(pc->flags & mask);
}


void set_flags(chunk_t* pc, const uint64_t set_bits)
{
   LOG_FUNC_CALL();
   update_flags(pc, 0, set_bits);
}


void clr_flags(chunk_t* pc, const uint64_t clr_bits)
{
   LOG_FUNC_CALL();
   update_flags(pc, clr_bits, 0);
}


void update_flags(chunk_t* pc, const uint64_t clr_bits, const uint64_t set_bits)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   const uint64_t new_flags = (pc->flags & ~clr_bits) | set_bits;
   if (pc->flags != new_flags)
   {
      LOG_FMT(LSETFLG, "set_chunk_flags: %016" PRIx64 "^%016" PRIx64 "=%016" PRIx64 " %u:%u '%s' %s:%s",
              pc->flags, pc->flags ^ new_flags, new_flags, pc->orig_line, pc->orig_col, pc->text(),
              get_token_name(pc->type), get_token_name(pc->ptype));
      log_func_stack_inline(LSETFLG);
      pc->flags = new_flags;
   }
}


static chunk_t* get_ncnlnp(chunk_t* cur, const scope_e scope, const dir_e dir)
{
   chunk_t* pc = cur;
   pc = (is_preproc(pc) == true) ?
        chunk_search(pc, is_cmt_or_nl_in_preproc, scope, dir, false) :
        chunk_search(pc, is_cmt_nl_or_preproc,    scope, dir, false);
   return(pc);
}


bool is_forin(chunk_t* pc)
{
   if (is_lang(cpd, LANG_OC) && (is_type(pc, CT_SPAREN_OPEN)) )
   {
      chunk_t* prev = get_prev_ncnl(pc);
      if(is_type(prev, CT_FOR))
      {
         chunk_t* next = pc;
         while(not_type(next, CT_IN, CT_SPAREN_CLOSE))
         {
            next = get_next_ncnl(next);
            retval_if(is_type(next, CT_IN), true);
         }
      }
   }
   return(false);
}


bool is_if_else_elseif(const chunk_t* const pc) { return is_ptype(pc, CT_IF, CT_ELSE, CT_ELSEIF); }
bool is_for           (const chunk_t* const pc) { return is_ptype(pc, CT_FOR                   ); }
bool is_do            (const chunk_t* const pc) { return is_ptype(pc, CT_DO                    ); }
bool is_while         (const chunk_t* const pc) { return is_ptype(pc, CT_WHILE                 ); }
bool is_using         (const chunk_t* const pc) { return is_ptype(pc, CT_USING_STMT            ); }
bool is_fct           (const chunk_t* const pc) { return is_ptype(pc, CT_FUNC_DEF              ); }


bool is_type_and_ptype(const chunk_t* const pc, const c_token_t type,
                                                const c_token_t ptype)
{
   return(is_valid(pc) && (pc->type  == type ) &&
                          (pc->ptype == ptype) );
}


bool is_type_and_not_ptype(const chunk_t* const pc, const c_token_t type,
                                                    const c_token_t ptype)
{
   return(is_valid(pc) && (pc->type  == type ) &&
                          (pc->ptype != ptype) );
}


bool any_is_type(const chunk_t* const pc1,
                 const chunk_t* const pc2, const c_token_t type)
{
   return(is_type(pc1, type) ||
          is_type(pc2, type) );
}


bool any_is_type(const chunk_t* pc1, const c_token_t type1,
                 const chunk_t* pc2, const c_token_t type2)
{
   return(is_type(pc1, type1) ||
          is_type(pc2, type2) );
}


bool are_types(const chunk_t* const pc1,
               const chunk_t* const pc2, const c_token_t type)
{
   return(is_type(pc1, type) &&
          is_type(pc2, type) );
}


bool are_ptypes(const chunk_t* const pc1,
                const chunk_t* const pc2, const c_token_t type)
{
   return(is_ptype(pc1, type) &&
          is_ptype(pc2, type) );
}


bool are_types(const chunk_t* pc1, const c_token_t type1,
               const chunk_t* pc2, const c_token_t type2)
{
   return(is_type(pc1, type1) &&
          is_type(pc2, type2) );
}


bool are_ptypes(const chunk_t* pc1, const c_token_t type1,
                const chunk_t* pc2, const c_token_t type2)
{
   return(is_ptype(pc1, type1) &&
          is_ptype(pc2, type2) );
}


bool is_type(const c_token_t token, const c_token_t type)
{
   return(token == type);
}


bool is_type(const c_token_t token, const c_token_t type1,
                                    const c_token_t type2)
{
   return((token == type1) || (token == type2));
}


bool is_type(const c_token_t token, const c_token_t type1,
             const c_token_t type2, const c_token_t type3)
{
   return((token == type1) || (token == type2) || (token == type3));
}


bool not_type(const c_token_t token, const c_token_t type)
{
   return(token != type);
}


bool not_type(const c_token_t token, const c_token_t type1,
                                     const c_token_t type2)
{
   return((token != type1) && (token != type2));
}


bool not_type(const c_token_t token, const c_token_t type1,
              const c_token_t type2, const c_token_t type3)
{
   return((token != type1) && (token != type2) && (token != type3));
}

#if 0
// use variadic template to unify overloaded and variadic functions
template<typename T>
bool nis_type(const chunk_t* const pc, const T type)
{
   retval_if(is_invalid(pc), false);
   return(pc->type == type);
}

template<typename T, typename... Args>
bool nis_type(const chunk_t* const pc, const T type, Args...args)
{
   return (pc->type == type) && nis_type(pc, args...);
}
#endif

bool is_type(const chunk_t* const pc, const c_token_t type)
{
#if 0
   return nis_type(pc, type);
#else
   return(is_valid(pc) && (pc->type == type));
#endif
}


bool is_type(const chunk_t* const pc, const c_token_t type1,
                                      const c_token_t type2)
{
#if 0
   return nis_type(pc, type1, type2);
#else
   return(is_valid(pc) && ((pc->type == type1) || (pc->type == type2)));
#endif
}


bool is_type(const chunk_t* const pc, const c_token_t type1,
             const c_token_t type2,   const c_token_t type3)
{
#if 0
   return nis_type(pc, type1, type2, type3);
#else
   retval_if(is_invalid(pc), false);
   return((pc->type == type1) ||
          (pc->type == type2) ||
          (pc->type == type3) ) ;
#endif
}

#if 1
bool is_type(const chunk_t* const pc, const c_token_t type1, const c_token_t type2,
                                      const c_token_t type3, const c_token_t type4)
{
   retval_if(is_invalid(pc), false);
   return((pc->type == type1) || (pc->type == type2) ||
          (pc->type == type3) || (pc->type == type4) );
}
#endif

#if 0
bool is_type(const chunk_t* const pc, const c_token_t type1,
      const c_token_t type2=CT_IGNORE,
      const c_token_t type3=CT_IGNORE,
      const c_token_t type4=CT_IGNORE,
      const c_token_t type5=CT_IGNORE,
      const c_token_t type6=CT_IGNORE,
      const c_token_t type7=CT_IGNORE,
      const c_token_t type8=CT_IGNORE,
      const c_token_t type9=CT_IGNORE)
{
   if (!pc) return false;
   if (pc->type == type1) return true;
   if (type2==CT_IGNORE) return false; if (pc->type == type2) return true;
   if (type3==CT_IGNORE) return false; if (pc->type == type3) return true;
   if (type4==CT_IGNORE) return false; if (pc->type == type4) return true;
   if (type5==CT_IGNORE) return false; if (pc->type == type5) return true;
   if (type6==CT_IGNORE) return false; if (pc->type == type6) return true;
   if (type7==CT_IGNORE) return false; if (pc->type == type7) return true;
   if (type8==CT_IGNORE) return false; if (pc->type == type8) return true;
   if (type9==CT_IGNORE) return false; if (pc->type == type9) return true;
}
#endif


bool is_ptype(const chunk_t* const pc, const c_token_t type)
{
   retval_if(is_invalid(pc), false);
   return(pc->ptype == type);
}


bool is_ptype(const chunk_t* const pc, const c_token_t type1,
                                       const c_token_t type2)
{
   retval_if(is_invalid(pc), false);
   return((pc->ptype == type1) ||
          (pc->ptype == type2) );
}


bool is_ptype(const chunk_t* const pc, const c_token_t type1,
              const c_token_t type2,   const c_token_t type3)
{
   retval_if(is_invalid(pc), false);
   return ((pc->ptype == type1) ||
           (pc->ptype == type2) ||
           (pc->ptype == type3) );
}


bool is_only_first_type(const chunk_t* pc1, const c_token_t type1,
                        const chunk_t* pc2, const c_token_t type2)
{
   return(is_type(pc1, type1) && not_type(pc2, type2));

}


bool not_type(const chunk_t* const pc, const c_token_t type)
{
   retval_if(is_invalid(pc), false);
   return(pc->type != type);
}


bool not_type(const chunk_t* const pc, const c_token_t type1,
                                       const c_token_t type2)
{
   retval_if(is_invalid(pc), false);
   return((pc->type != type1) &&
          (pc->type != type2) );
}


bool not_type(const chunk_t* const pc, const c_token_t type1,
              const c_token_t type2,   const c_token_t type3)
{
   retval_if(is_invalid(pc), false);
   return((pc->type != type1) &&
          (pc->type != type2) &&
          (pc->type != type3) );
}


bool not_ptype(const chunk_t* const pc, const c_token_t ptype)
{
   retval_if(is_invalid(pc), false);
   return(pc->ptype != ptype);
}


bool not_ptype(const chunk_t* const pc, const c_token_t ptype1,
                                        const c_token_t ptype2)
{
   retval_if(is_invalid(pc), false);
   return((pc->ptype != ptype1) &&
          (pc->ptype != ptype2) );
}


bool not_ptype(const chunk_t* const pc, const c_token_t ptype1,
               const c_token_t ptype2,  const c_token_t ptype3)
{
   retval_if(is_invalid(pc), false);
   return(is_valid(pc) && (pc->ptype != ptype1) &&
                          (pc->ptype != ptype2) &&
                          (pc->ptype != ptype3) );
}


bool is_type(const chunk_t* const pc, uint32_t count, ... )
{
   va_list args;          /* determine list of arguments ... */
   va_start(args, count); /* ... that follow after parameter count */

   bool result = false;
   if(is_valid(pc))
   {
      for( ; count != 0; --count)
      {
         c_token_t type = (c_token_t)va_arg(args, int32_t); /* get next argument */

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
bool is_ptype(const chunk_t* const pc, uint32_t count, ... )
{
   va_list args;          /* determine list of arguments ... */
   va_start(args, count); /* ... that follow after parameter count */

   bool result = false;
   if(is_valid(pc))
   {
      for( ; count != 0; --count)
      {
         c_token_t type = (c_token_t)va_arg(args, int32_t); /* get next argument */
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
bool not_type(const chunk_t* const pc, uint32_t count, ... )
{
   va_list args;          /* determine list of arguments ... */
   va_start(args, count); /* ... that follow after parameter count */

   bool result = false;
   if(is_valid(pc))
   {
      result = true;
      for( ; count != 0; --count)
      {
         c_token_t type = (c_token_t)va_arg(args, int32_t); /* get next argument */
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


bool not_ptype(const chunk_t* const pc, uint32_t count, ... )
{
   va_list args;          /* determine list of arguments ... */
   va_start(args, count); /* ... that follow after parameter count */

   bool result = false;
   if(is_valid(pc))
   {
      result = true;
      for( ; count != 0; --count)
      {
         c_token_t type = (c_token_t)va_arg(args, int32_t); /* get next argument */
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


bool is_type_and_flag(const chunk_t* const pc, const c_token_t type,
                                               const uint64_t  flags)
{
   return (is_valid(pc) && (pc->ptype          == type ) &&
                           (pc->flags & flags) == flags);
}


bool is_flag (const chunk_t* const pc, const uint64_t flags) { return (is_valid(pc) && (pc->flags & flags) == flags); }
bool not_flag(const chunk_t* const pc, const uint64_t flags) { return (is_valid(pc) && (pc->flags & flags) == 0    ); }


chunk_t* chunk_skip_to_match(chunk_t* cur, const scope_e scope)
{
   if(is_type(cur, 8, CT_PAREN_OPEN,  CT_SPAREN_OPEN,
                      CT_FPAREN_OPEN, CT_TPAREN_OPEN,
                      CT_BRACE_OPEN,  CT_VBRACE_OPEN,
                      CT_ANGLE_OPEN,  CT_SQUARE_OPEN))
   {
      return(get_next_type(cur, get_inverse_type(cur->type),
                                 (int32_t)cur->level, scope));
   }
   return(cur);
}


chunk_t* chunk_skip_to_match_rev(chunk_t* cur, const scope_e scope)
{
   if(is_type(cur, 8, CT_PAREN_CLOSE,  CT_SPAREN_CLOSE,
                      CT_FPAREN_CLOSE, CT_TPAREN_CLOSE,
                      CT_BRACE_CLOSE,  CT_VBRACE_CLOSE,
                      CT_ANGLE_CLOSE,  CT_SQUARE_CLOSE))
   {
      return(get_prev_type(cur, get_inverse_type(cur->type),
                                 (int32_t)cur->level, scope));
   }
   return(cur);
}


bool chunk_is_function(const chunk_t* const pc)
{
   return(is_type(pc, 5, CT_FUNC_CLASS_DEF,   CT_FUNC_PROTO,
                         CT_FUNC_CLASS_PROTO, CT_FUNC_DEF,
                         CT_OC_MSG_DECL));
}


bool is_cmt(const chunk_t* const pc)
{
   return(is_type(pc, CT_COMMENT_MULTI, CT_COMMENT, CT_COMMENT_CPP));
}


bool is_nl(const chunk_t* const pc)
{
   return(is_type(pc, CT_NEWLINE, CT_NL_CONT));
}

bool is_comma(const chunk_t* const pc) { return(is_type(pc, CT_COMMA   )); }
bool is_ptr  (const chunk_t* const pc) { return(is_type(pc, CT_PTR_TYPE)); }


bool chunk_empty(const chunk_t* const pc)
{
   return(is_valid(pc) && (pc->str.size() == 0));
}


bool is_cmt_or_nl           (const chunk_t* const pc) { return(is_cmt      (pc) || is_nl       (pc)); }
bool is_cmt_or_nl_in_preproc(const chunk_t* const pc) { return(is_preproc  (pc) && is_cmt_or_nl(pc)); }
bool is_cmt_nl_or_preproc   (const chunk_t* const pc) { return(is_cmt_or_nl(pc) || is_preproc  (pc)); }
bool is_cmt_nl_or_blank     (const chunk_t* const pc) { return(is_cmt_or_nl(pc) || chunk_empty (pc)); }


bool is_doxygen_cmt(chunk_t* pc)
{
   retval_if(!is_cmt(pc), false);

   /* check the third character */
   const char*  sComment = pc->text();
   const size_t len      = strlen(sComment);
   retval_if((len < 3), false);
   return((sComment[2] == '/') ||
          (sComment[2] == '!') ||
          (sComment[2] == '@') );
}


bool is_bal_square(const chunk_t* const pc)
{
   return(is_type(pc, CT_SQUARE_OPEN, CT_TSQUARE, CT_SQUARE_CLOSE));
}


bool is_single_line_cmt(const chunk_t* const pc) { return(is_type(pc, CT_COMMENT, CT_COMMENT_CPP )); }
bool is_semicolon      (const chunk_t* const pc) { return(is_type(pc, CT_SEMICOLON, CT_VSEMICOLON)); }
bool chunk_is_member   (const chunk_t* const pc) { return(is_type(pc, CT_DC_MEMBER, CT_MEMBER    )); }


bool is_var_type(const chunk_t* const pc)
{
   return(is_type(pc, 8, CT_DC_MEMBER, CT_PTR_TYPE, CT_TYPE,  CT_BYREF,
                         CT_QUALIFIER, CT_STRUCT,   CT_UNION, CT_ENUM));
}


bool is_opening_brace_of_if(const chunk_t* const pc)
{
   return (is_opening_brace(pc) && is_ptype(pc, CT_IF));
}

bool is_any_brace     (const chunk_t* const pc) { return(is_rbrace(pc) || is_vbrace(pc)); }
bool is_closing_brace (const chunk_t* const pc) { return(is_type(pc, CT_BRACE_CLOSE,  CT_VBRACE_CLOSE)); }
bool is_opening_brace (const chunk_t* const pc) { return(is_type(pc, CT_BRACE_OPEN,   CT_VBRACE_OPEN )); }
bool is_rbrace        (const chunk_t* const pc) { return(is_type(pc, CT_BRACE_CLOSE,  CT_BRACE_OPEN  )); }
bool is_vbrace        (const chunk_t* const pc) { return(is_type(pc, CT_VBRACE_CLOSE, CT_VBRACE_OPEN )); }
bool is_opening_vbrace(const chunk_t* const pc) { return(is_type(pc,                  CT_VBRACE_OPEN )); }
bool is_opening_rbrace(const chunk_t* const pc) { return(is_type(pc,                  CT_BRACE_OPEN  )); }
bool is_closing_rbrace(const chunk_t* const pc) { return(is_type(pc, CT_BRACE_CLOSE                  )); }
bool is_closing_vbrace(const chunk_t* const pc) { return(is_type(pc, CT_VBRACE_CLOSE                 )); }


bool is_fparen_open(const chunk_t* const pc)
{
   return(is_type(pc, CT_FPAREN_OPEN));
}


bool is_paren_open(const chunk_t* const pc)
{
   return(is_type(pc, CT_PAREN_OPEN,  CT_SPAREN_OPEN,
                      CT_TPAREN_OPEN, CT_FPAREN_OPEN));
}


bool is_paren_close(const chunk_t* const pc)
{
   return(is_type(pc, CT_PAREN_CLOSE,  CT_SPAREN_CLOSE,
                      CT_TPAREN_CLOSE, CT_FPAREN_CLOSE));
}


bool is_str(chunk_t* pc, const char* str)
{
   retval_if(ptrs_are_invalid(pc, str), false);
   uint32_t len = strlen(str);
   return((pc->str.size() == len) && /* string length has to be equal */
          (memcmp(pc->text(), str, len) == 0) ); /* strings are equal considering case */
}


bool is_str_case(chunk_t* pc, const char* str)
{
   retval_if(ptrs_are_invalid(pc, str), false);
   uint32_t len = strlen(str);
   return((pc->str.size() == len) && /* string length has to be equal */
          (strncasecmp(pc->text(), str, len) == 0)); /* strings are equal ignoring case */
}


bool is_word(const chunk_t* const pc)
{
   return(is_valid(pc) && (pc->str.size() >= 1u) &&
          (CharTable::IsKW1((uint32_t)pc->str[0])) );
}

bool is_addr(chunk_t* pc)
{
   if (  (is_valid(pc)           ) &&
       ( (pc->type   == CT_BYREF ) ||
        ((pc->str.size() == 1    ) &&
         (pc->str[0] == '&'      ) &&
         not_type(pc, CT_OPERATOR_VAL) )))
   {
      chunk_t* prev = chunk_get_prev(pc);

      if (is_flag(pc,   PCF_IN_TEMPLATE        ) &&
          is_type(prev, CT_COMMA, CT_ANGLE_OPEN) )
      {
//      (pos == dir_e::AFTER) ? g_cl.AddAfter(pc, ref) : g_cl.AddBefore(pc, ref);
         return(false);
      }
      return(true);
   }
   return(false);
}


bool is_star(const chunk_t* const pc)
{
   return(is_valid(pc) && (pc->str.size() == 1) &&
          (pc->str[0] == '*'            ) &&
          (pc->type   != CT_OPERATOR_VAL) );
}


/* ms compilers for C++/CLI and WinRT use '^' instead of '*'
 * for marking up reference types vs pointer types */
bool is_msref(const chunk_t* const pc)
{
   return(is_lang(cpd, LANG_CPP       ) &&
          (is_valid(pc)               ) &&
          (pc->str.size() == 1        ) &&
          (pc->str[0] == '^'          ) &&
          not_type(pc, CT_OPERATOR_VAL) );
}


bool is_ptr_operator(chunk_t* pc)
{
   return(is_star (pc) ||
          is_addr (pc) ||
          is_msref(pc) );
}


bool is_preproc(const chunk_t* const pc)
{
   return(is_flag(pc, PCF_IN_PREPROC));
}


bool is_no_preproc_type(const chunk_t* const pc)
{
   return ((pc->type < CT_PP_DEFINE) ||
           (pc->type > CT_PP_OTHER ) );
}


bool are_same_pp(const chunk_t* const pc1, const chunk_t* const pc2)
{
   return are_valid(pc1, pc2) && (is_preproc(pc1) == is_preproc(pc2));
}


bool are_different_pp(const chunk_t* const pc1, const chunk_t* const pc2)
{
   return( are_valid(pc1, pc2) &&
          (is_flag(pc1, PCF_IN_PREPROC) != is_flag(pc2, PCF_IN_PREPROC)));
}


bool is_safe_to_del_nl(chunk_t* nl)
{
   chunk_t* tmp = chunk_get_prev(nl);
   return (is_type(tmp, CT_COMMENT_CPP)) ?
    false : are_same_pp(chunk_get_prev(nl), chunk_get_next(nl));
}
