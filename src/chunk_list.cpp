/**
 * @file chunk_list.cpp
 * Manages and navigates the list of chunks.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "chunk_list.h"
#include <cstring>
#include <cstdlib>

#include "uncrustify_types.h"
#include "ListManager.h"
#include "uncrustify.h"
#include "space.h"


typedef ListManager<chunk_t> ChunkList_t;

/** use this enum to define in what direction or location an
 *  operation shall be performed. */
enum loc_t
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
 * chunk_t *function(chunk_t *cur, nav_t nav)
 ******************************************************************************/
typedef chunk_t * (*search_t)(chunk_t *cur, nav_t nav);


typedef ListManager<chunk_t> ChunkList_t;


/* \todo if we use C++ we can overload the following two functions
 * and thus name them equally */

/**
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
 * @retval NULL    - no requested chunk was found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 */
static chunk_t *chunk_search(
   chunk_t       *cur,           /**< [in] chunk to start search at */
   const check_t check_fct,      /**< [in] compare function */
   const nav_t   nav = CNAV_ALL, /**< [in] code parts to consider for search */
   const loc_t   dir = AFTER,    /**< [in] search direction */
   const bool    cond = true     /**< [in] success condition */
);


/**
 * \brief search a chunk of a given category in a chunk list
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * This function is a specialization of chunk_search.
 *
 * @retval NULL    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 */
static chunk_t *chunk_search_type(
   chunk_t         *cur,           /**< [in] chunk to start search at */
   const c_token_t type,           /**< [in] category to search for */
   const nav_t     nav = CNAV_ALL, /**< [in] code parts to consider for search */
   const loc_t     dir = AFTER     /**< [in] search direction */
);


/**
 * \brief search a chunk of a given type and level
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * This function is a specialization of chunk_search.
 *
 * @retval NULL    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 */
chunk_t *chunk_search_typelevel(
   chunk_t   *cur,           /**< [in] chunk to start search at */
   c_token_t type,           /**< [in] category to search for */
   nav_t     nav = CNAV_ALL, /**< [in] code parts to consider for search */
   loc_t     dir = AFTER,    /**< [in] search direction */
   int       level = -1      /**< {in]  */
);


/**
 * \brief searches a chunk that is non-NEWLINE, non-comment and non-preprocessor
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * @retval NULL    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 */
static chunk_t *chunk_get_ncnlnp(
   chunk_t     *cur,           /**< [in] chunk to start search at */
   const nav_t nav = CNAV_ALL, /**< [in] code parts to consider for search */
   const loc_t dir = AFTER     /**< [in] search direction */
);


/**
 * \brief searches a chunk that holds a given string
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * @retval NULL    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 */
chunk_t *chunk_search_str(
   chunk_t    *cur,  /**< [in] chunk to start search at */
   const char *str,  /**< [in] string to search for */
   nav_t      nav,   /**< [in] code parts to consider for search */
   loc_t      dir,   /**< [in] search direction */
   int        level, /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   size_t     len    /**< [in] length of string */
);


/**
 * \brief Add a new chunk after the given position in a chunk list
 *
 * \note If ref is NULL:
 *       add at the head of the chunk list if position is BEFOR
 *       add at the tail of the chunk list if position is AFTER
 *
 * @return pointer to the added chunk
 */
static chunk_t *chunk_add(
   const chunk_t *pc_in,     /**< {in] chunk to add to list */
   chunk_t       *ref,       /**< [in] insert position in list */
   const loc_t   pos = AFTER /**< [in] insert before or after */
);


/**
 * \brief Determines which chunk search function to use
 *
 * Depending on the required search direction return a pointer
 * to the corresponding chunk search function.
 *
 * @return pointer to chunk search function
 */
static search_t select_search_fct(
   const loc_t dir = AFTER /**< [in] search direction */
);


static void chunk_log(
   chunk_t    *pc,
   const char *text
);


static bool is_expected_type_and_level(
   chunk_t   *pc,
   c_token_t type,
   int       level
);


static bool is_expected_string_and_level(
   chunk_t    *pc,
   const char *str,
   int        level,
   size_t     len
);


static search_t select_search_fct(const loc_t dir)
{
   return((dir == AFTER) ? chunk_get_next : chunk_get_prev);
}


chunk_t *chunk_search_prev_cat(chunk_t *pc, const c_token_t cat)
{
   return(chunk_search_type(pc, cat, CNAV_ALL, BEFORE));
}


chunk_t *chunk_search_next_cat(chunk_t *pc, const c_token_t cat)
{
   return(chunk_search_type(pc, cat, CNAV_ALL, AFTER));
}


static void set_chunk(chunk_t *pc, c_token_t token, log_sev_t what, const char *str);


ChunkList_t g_cl; /** global chunk list */


static chunk_t
*chunk_search_type(chunk_t *cur, const c_token_t type, const nav_t nav, const loc_t dir)
{
   const search_t search_function = select_search_fct(dir);
   chunk_t        *pc             = cur;

   do                                /* loop over the chunk list */
   {
      pc = search_function(pc, nav); /* in either direction while */
   } while ((pc != NULL) &&          /* the end of the list was not reached yet */
            (pc->type != type));     /* and the demanded chunk was not found either */
   return(pc);                       /* the latest chunk is the searched one */
}


chunk_t *chunk_search_typelevel(chunk_t *cur, c_token_t type, nav_t nav, loc_t dir, int level)
{
   const search_t search_function = select_search_fct(dir);
   chunk_t        *pc             = cur;

   do                                /* loop over the chunk list */
   {
      pc = search_function(pc, nav); /* in either direction while */
   } while ((pc != NULL) &&          /* the end of the list was not reached yet */
            (is_expected_type_and_level(pc, type, level) == false));
   return(pc);                       /* the latest chunk is the searched one */
}


chunk_t *chunk_search_str(chunk_t *cur, const char *str, nav_t nav, loc_t dir, int level, size_t len)
{
   const search_t search_function = select_search_fct(dir);
   chunk_t        *pc             = cur;

   do                                /* loop over the chunk list */
   {
      pc = search_function(pc, nav); /* in either direction while */
   } while ((pc != NULL) &&          /* the end of the list was not reached yet */
            (is_expected_string_and_level(pc, str, level, len) == false));
   return(pc);                       /* the latest chunk is the searched one */
}


static chunk_t
*chunk_search(chunk_t *cur, const check_t check_fct, const nav_t nav,
              const loc_t dir, const bool cond)
{
   const search_t search_function = select_search_fct(dir);
   chunk_t        *pc             = cur;

   do                                 /* loop over the chunk list */
   {
      pc = search_function(pc, nav);  /* in either direction while */
   } while ((pc != NULL) &&           /* the end of the list was not reached yet */
            (check_fct(pc) != cond)); /* and the demanded chunk was not found either */
   return(pc);                        /* the latest chunk is the searched one */
}


static bool is_expected_type_and_level(chunk_t *pc, c_token_t type, int level)
{
   return ((pc->type  ==          type ) && /* the type is as expected and */
           ((pc->level == (size_t)level) || /* the level is as expected or */
            (level     <              0)) );                 /* we don't care about the level */
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

   while (((pc = chunk_get_prev(pc)) != NULL) && !chunk_is_newline(pc))
   {
      first = pc;
   }

   return(first);
}


/* \todo maybe it is better to combine chunk_get_next and chunk_get_prev
 * into a common function However this should be done with the preprocessor
 * to avoid addition check conditions that would be evaluated in the
 * while loop of the calling function */
chunk_t *chunk_get_next(chunk_t *cur, nav_t nav)
{
   if (cur == NULL) { return(cur); }

   chunk_t *pc = g_cl.GetNext(cur);
   if ((pc == NULL) || (nav == CNAV_ALL))
   {
      return(pc);
   }
   if (cur->flags & PCF_IN_PREPROC)
   {
      /* If in a preproc, return NULL if trying to leave */
      if ((pc->flags & PCF_IN_PREPROC) == 0)
      {
         return((chunk_t *)NULL);
      }
      return(pc);
   }
   /* Not in a preproc, skip any preproc */
   while ((pc != NULL) && (pc->flags & PCF_IN_PREPROC))
   {
      pc = g_cl.GetNext(pc);
   }
   return(pc);
}


chunk_t *chunk_get_prev(chunk_t *cur, nav_t nav)
{
   if (cur == NULL) { return(cur); }

   chunk_t *pc = g_cl.GetPrev(cur);
   if ((pc == NULL) || (nav == CNAV_ALL))
   {
      return(pc);
   }
   if (cur->flags & PCF_IN_PREPROC)
   {
      /* If in a preproc, return NULL if trying to leave */
      if ((pc->flags & PCF_IN_PREPROC) == 0)
      {
         return((chunk_t *)NULL);
      }
      return(pc);
   }
   /* Not in a preproc, skip any preproc */
   while ((pc != NULL) && (pc->flags & PCF_IN_PREPROC))
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


chunk_t *chunk_dup(const chunk_t *pc_in)
{
   chunk_t *const pc = new chunk_t; /* Allocate a new chunk */

   if (pc == NULL)
   {
      /* @todo clean up properly before crashing */
      LOG_FMT(LERR, "Failed to allocate memory\n");
      exit(EXIT_FAILURE);
   }

   /* Copy all fields and then init the entry */
   *pc = *pc_in;  /* \todo what happens if pc_in == NULL? */
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
   if ((pc            != NULL       ) &&
       (cpd.unc_stage != US_TOKENIZE) &&
       (cpd.unc_stage != US_CLEANUP ) )
   {
      const log_sev_t log   = LCHUNK;
      chunk_t         *prev = chunk_get_prev(pc);
      chunk_t         *next = chunk_get_next(pc);

      chunk_log_msg(pc, log, text);

      if ((prev != NULL) && (next != NULL))
      {
         chunk_log_msg(prev, log, " @ between");
         chunk_log_msg(next, log, " and");
      }
      else if (next != NULL)
      {
         chunk_log_msg(next, log, " @ before");
      }
      else if (prev != NULL)
      {
         chunk_log_msg(prev, log, " @ after");
      }
      LOG_FMT(log, " stage=%d", cpd.unc_stage);
      log_func_stack_inline(log);
   }
}


static chunk_t *chunk_add(const chunk_t *pc_in, chunk_t *ref, const loc_t pos)
{
   chunk_t *pc = chunk_dup(pc_in);
   if (pc != NULL)
   {
      switch(pos)
      {
         case(AFTER):  (ref != NULL) ? g_cl.AddAfter (pc, ref) : g_cl.AddTail(pc); break;
         case(BEFORE): (ref != NULL) ? g_cl.AddBefore(pc, ref) : g_cl.AddTail(pc); break; // should be AddHead but tests fail
         default:      /* invalid position indication */                           break;
      }
      chunk_log(pc, "chunk_add");
   }
   return(pc); /* \todo what is returned here? */
}


chunk_t *chunk_add_after(const chunk_t *pc_in, chunk_t *ref)
{
   return(chunk_add(pc_in, ref, AFTER));
}


chunk_t *chunk_add_before(const chunk_t *pc_in, chunk_t *ref)
{
   return(chunk_add(pc_in, ref, BEFORE));
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

   if ((pc_in != NULL) && (ref != NULL))
   {
      g_cl.Pop(pc_in);
      g_cl.AddAfter(pc_in, ref);

      /* HACK: Adjust the original column */
      pc_in->column       = ref->column + (size_t)space_col_align(ref, pc_in);
      pc_in->orig_col     = (UINT32)pc_in->column;
      pc_in->orig_col_end = pc_in->orig_col + pc_in->len();
   }
}


chunk_t *chunk_get_next_nl(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_newline, nav, AFTER, true));
}


chunk_t *chunk_get_prev_nl(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_newline, nav, BEFORE, true));
}


chunk_t *chunk_get_next_nnl(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_newline, nav, AFTER, false));
}


chunk_t *chunk_get_prev_nnl(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_newline, nav, BEFORE, false));
}


chunk_t *chunk_get_next_ncnl(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_comment_or_newline, nav, AFTER, false));
}


chunk_t *chunk_get_next_ncnlnp(chunk_t *cur, nav_t nav)
{
   return(chunk_get_ncnlnp(cur, nav, AFTER));
}


chunk_t *chunk_get_prev_ncnlnp(chunk_t *cur, nav_t nav)
{
   return(chunk_get_ncnlnp(cur, nav, BEFORE));
}


chunk_t *chunk_get_next_nblank(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_comment_newline_or_blank, nav, AFTER, false));
}


chunk_t *chunk_get_prev_nblank(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_comment_newline_or_blank, nav, BEFORE, false));
}


chunk_t *chunk_get_next_nc(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_comment, nav, AFTER, false));
}


chunk_t *chunk_get_next_nisq(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_balanced_square, nav, AFTER, false));
}


chunk_t *chunk_get_prev_ncnl(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_comment_or_newline, nav, BEFORE, false));
}


chunk_t *chunk_get_prev_nc(chunk_t *cur, nav_t nav)
{
   return(chunk_search(cur, chunk_is_comment, nav, BEFORE, false));
}


chunk_t *chunk_get_next_nvb(chunk_t *cur, const nav_t nav)
{
   return(chunk_search(cur, chunk_is_vbrace, nav, AFTER, false));
}


chunk_t *chunk_get_prev_nvb(chunk_t *cur, const nav_t nav)
{
   return(chunk_search(cur, chunk_is_vbrace, nav, BEFORE, false));
}


chunk_t *chunk_get_next_type(chunk_t *cur, c_token_t type, int level, nav_t nav)
{
   return(chunk_search_typelevel(cur, type, nav, AFTER, level));
}


chunk_t *chunk_get_prev_type(chunk_t *cur, c_token_t type, int level, nav_t nav)
{
   return(chunk_search_typelevel(cur, type, nav, BEFORE, level));
}


chunk_t *chunk_get_next_str(chunk_t *cur, const char *str, size_t len, int level, nav_t nav)
{
   return(chunk_search_str(cur, str, nav, AFTER, level, len));
}


chunk_t *chunk_get_prev_str(chunk_t *cur, const char *str, size_t len, int level, nav_t nav)
{
   return(chunk_search_str(cur, str, nav, BEFORE, level, len));
}


bool chunk_is_newline_between(chunk_t *start, chunk_t *end)
{
   for (chunk_t *pc = start; pc != end; pc = chunk_get_next(pc))
   {
      if (chunk_is_newline(pc))
      {
         return(true);
      }
   }
   return(false);
}


void chunk_swap(chunk_t *pc1, chunk_t *pc2)
{
   g_cl.Swap(pc1, pc2);
}


/* \todo this function needs some cleanup */
void chunk_swap_lines(chunk_t *pc1, chunk_t *pc2)
{
   pc1 = chunk_first_on_line(pc1);
   pc2 = chunk_first_on_line(pc2);

   if ((pc1 == NULL) ||
       (pc2 == NULL) ||
       (pc1 == pc2 ) )
   {
      return;
   }

   /* Example start:
    * ? - start1 - a1 - b1 - nl1 - ? - ref2 - start2 - a2 - b2 - nl2 - ?
    *      ^- pc1                              ^- pc2 */
   chunk_t *ref2 = chunk_get_prev(pc2);

   /* Move the line started at pc2 before pc1 */
   while ((pc2 != NULL) && !chunk_is_newline(pc2))
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
   while ((pc1 != NULL) && !chunk_is_newline(pc1))
   {
      chunk_t *tmp = chunk_get_next(pc1);
      g_cl.Pop(pc1);
      if (ref2 != NULL) { g_cl.AddAfter(pc1, ref2); }
      else              { g_cl.AddHead (pc1);       }
      ref2 = pc1;
      pc1  = tmp;
   }

   /* Should now be:
    * ? - start2 - a2 - b2 - nl1 - ? - ref2 - start1 - a1 - b1 - nl2 - ?
    *                         ^- pc1                              ^- pc2 */

   /* pc1 and pc2 should be the newlines for their lines.
    * swap the chunks and the nl_count so that the spacing remains the same.
    */
   if ((pc1 != NULL) && (pc2 != NULL))
   {
#if 0
      SWAP(pc1->nl_count, pc2->nl_count); /* \todo check this */
#else
      size_t nl_count = pc1->nl_count;

      pc1->nl_count = pc2->nl_count;
      pc2->nl_count = nl_count;
#endif
      chunk_swap(pc1, pc2);
   }
}


static void set_chunk(chunk_t *pc, c_token_t token, log_sev_t what, const char *str)
{
   LOG_FUNC_ENTRY();

   assert(pc != NULL);

   c_token_t       *where;
   const c_token_t *type;
   const c_token_t *parent_type;

   switch (what)
   {
   case (LSETTYP): where = &pc->type;
      type               = &token;
      parent_type        = &pc->parent_type;
      break;

   case (LSETPAR): where = &pc->parent_type;
      type               = &pc->type;
      parent_type        = &token;
      break;

   default:
      return;
   }

   if ((pc != NULL) && (*where != token))
   {
      LOG_FMT(what, "%s: %zu:%zu '%s' %s:%s => %s:%s",
              str, pc->orig_line, pc->orig_col, pc->text(),
              get_token_name(pc->type), get_token_name(pc->parent_type),
              get_token_name(*type), get_token_name(*parent_type));
      log_func_stack_inline(what);
      *where = token;
   }
}


void set_chunk_type(chunk_t *pc, c_token_t tt)
{
   LOG_FUNC_CALL();
   set_chunk(pc, tt, LSETTYP, "set_chunk_type");
}


void set_chunk_parent(chunk_t *pc, c_token_t pt)
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

   if (pc != NULL)
   {
      const UINT64 nflags = (pc->flags & ~clr_bits) | set_bits;
      if (pc->flags != nflags)
      {
         LOG_FMT(LSETFLG, "set_chunk_flags: %016" PRIx64 "^%016" PRIx64 "=%016" PRIx64 " %zu:%zu '%s' %s:%s",
                 pc->flags, pc->flags ^ nflags, nflags,
                 pc->orig_line, pc->orig_col, pc->text(),
                 get_token_name(pc->type), get_token_name(pc->parent_type));
         log_func_stack_inline(LSETFLG);
         pc->flags = nflags;
      }
   }
}


static chunk_t *chunk_get_ncnlnp(chunk_t *cur, const nav_t nav, const loc_t dir)
{
   chunk_t *pc = cur;

   pc = (chunk_is_preproc(pc) == true) ?
        chunk_search(pc, chunk_is_comment_or_newline_in_preproc, nav, dir, false) :
        chunk_search(pc, chunk_is_comment_newline_or_preproc, nav, dir, false);
   return(pc);
}


#if 1
bool chunk_is_forin(chunk_t *pc)
{
   if ((cpd.lang_flags & LANG_OC) && pc && (pc->type == CT_SPAREN_OPEN))
   {
      chunk_t *prev = chunk_get_prev_ncnl(pc);
      if (prev->type == CT_FOR)
      {
         chunk_t *next = pc;
         while ( (next       != NULL           ) &&
                 (next->type != CT_SPAREN_CLOSE) &&
                 (next->type != CT_IN          ) )
         {
            next = chunk_get_next_ncnl(next);
            if (next->type == CT_IN)
            {
               return(true);
            }
         }
      }
   }
   return(false);
}
#else
bool chunk_is_forin(chunk_t *pc)
{
   /* only start search if ... */
   if ( (pc      !=            NULL) &&   /* start chunk is valid and */
        (pc->type == CT_SPAREN_OPEN) &&   /* is a open parenthesis "(" and */
        (cpd.lang_flags & LANG_OC  ) )    /* language is objective C */
   {
      /* check if the preceding chunk is a "for" instruction */
      chunk_t *prev = chunk_get_prev_ncnl(pc);
      if (prev->type == CT_FOR)
      {
         chunk_t *next = pc;
         do
         {
            next = chunk_get_next_ncnl(next);
            if(next == NULL)
            {
               return false; /* reach end of chunk list */
            }

            if (next->type == CT_IN)
            {
               return(true); /* found "in" chunk */
            }
         } /* go on until closing parentheses occurs */
         while(next->type != CT_SPAREN_CLOSE);
      }
   }
   return(false); /* no "in" in for loop found */
}
#endif
