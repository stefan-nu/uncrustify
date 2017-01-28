/**
 * @file sorting.cpp
 * Sorts chunks and imports
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "sorting.h"
#include "chunk_list.h"

/**
 * Compare two series of chunks, starting with the given ones.
 */
static int compare_chunks(chunk_t *pc1, chunk_t *pc2);


/**
 * Sorting should be pretty rare and should usually only include a few chunks.
 * We need to minimize the number of swaps, as those are expensive.
 * So, we do a min sort.
 */
static void do_the_sort(chunk_t **chunks, size_t num_chunks);


static int compare_chunks(chunk_t *pc1, chunk_t *pc2)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSORT, "\n@begin pc1->len=%zu, line=%zu, column=%zu\n", pc1->len(), pc1->orig_line, pc1->orig_col);
   LOG_FMT(LSORT, "@begin pc2->len=%zu, line=%zu, column=%zu\n", pc2->len(), pc2->orig_line, pc2->orig_col);
   if (pc1 == pc2)
   {
      return(0);
   }

   while ((pc1 != NULL) && (pc2 != NULL))
   {
      LOG_FMT(LSORT, "text=%s, pc1->len=%zu, line=%zu, column=%zu\n", pc1->text(), pc1->len(), pc1->orig_line, pc1->orig_col);
      LOG_FMT(LSORT, "text=%s, pc2->len=%zu, line=%zu, column=%zu\n", pc2->text(), pc2->len(), pc2->orig_line, pc2->orig_col);
      size_t min_len = (pc1->len() < pc2->len()) ? pc1->len() : pc2->len();
      int    ret_val = unc_text::compare(pc1->str, pc2->str, min_len);
      LOG_FMT(LSORT, "ret_val=%d\n", ret_val);

      if (ret_val != 0)
      {
         return(ret_val);
      }
      if (pc1->len() != pc2->len())
      {
         return((long)pc1->len() - (long)pc2->len());
      }

      /* Same word, same length. Step to the next chunk. */
      pc1 = chunk_get_next(pc1);
      LOG_FMT(LSORT, "text=%s, pc1->len=%zu, line=%zu, column=%zu\n", pc1->text(), pc1->len(), pc1->orig_line, pc1->orig_col);
      if (pc1->type == CT_MEMBER)
      {
         pc1 = chunk_get_next(pc1);
         LOG_FMT(LSORT, "text=%s, pc1->len=%zu, line=%zu, column=%zu\n", pc1->text(), pc1->len(), pc1->orig_line, pc1->orig_col);
      }
      pc2 = chunk_get_next(pc2);
      LOG_FMT(LSORT, "text=%s, pc2->len=%zu, line=%zu, column=%zu\n", pc2->text(), pc2->len(), pc2->orig_line, pc2->orig_col);
      if (pc2->type == CT_MEMBER)
      {
         pc2 = chunk_get_next(pc2);
         LOG_FMT(LSORT, "text=%s, pc2->len=%zu, line=%zu, column=%zu\n", pc2->text(), pc2->len(), pc2->orig_line, pc2->orig_col);
      }
      LOG_FMT(LSORT, ">>>text=%s, pc1->len=%zu, line=%zu, column=%zu\n", pc1->text(), pc1->len(), pc1->orig_line, pc1->orig_col);
      LOG_FMT(LSORT, ">>>text=%s, pc2->len=%zu, line=%zu, column=%zu\n", pc2->text(), pc2->len(), pc2->orig_line, pc2->orig_col);

      /* If we hit a newline or NULL, we are done */
      if ((pc1 == NULL) || chunk_is_newline(pc1) ||
          (pc2 == NULL) || chunk_is_newline(pc2) )
      {
         break;
      }
   }

   if ((pc1 == NULL) || !chunk_is_newline(pc2))
   {
      return(-1);
   }
   if (!chunk_is_newline(pc1))
   {
      return(1);
   }
   return(0);
} // compare_chunks


static void do_the_sort(chunk_t **chunks, size_t num_chunks)
{
   LOG_FUNC_ENTRY();

   LOG_FMT(LSORT, "%s: %zu chunks:", __func__, num_chunks);
   for (size_t idx = 0; idx < num_chunks; idx++)
   {
      LOG_FMT(LSORT, " [%s]", chunks[idx]->text());
   }
   LOG_FMT(LSORT, "\n");

   size_t start_idx;
   for (start_idx = 0; start_idx < (num_chunks - 1); start_idx++)
   {
      /* Find the index of the minimum value */
      size_t min_idx = start_idx;
      for (size_t idx = start_idx + 1; idx < num_chunks; idx++)
      {
         if (compare_chunks(chunks[idx], chunks[min_idx]) < 0)
         {
            min_idx = idx;
         }
      }

      /* Swap the lines if the minimum isn't the first entry */
      if (min_idx != start_idx)
      {
         chunk_swap_lines(chunks[start_idx], chunks[min_idx]);

         /* Don't need to swap, since we only want the side-effects */
         chunks[min_idx] = chunks[start_idx];
      }
   }
}


void sort_imports(void)
{
   LOG_FUNC_ENTRY();
   chunk_t *chunks[MAX_NUMBER_TO_SORT];  /* MAX_NUMBER_TO_SORT should be enough, right? */
   size_t  num_chunks = 0;
   const chunk_t *p_last    = NULL;
   chunk_t *p_imp     = NULL;

   chunk_t *pc = chunk_get_head();
   while (pc != NULL)
   {
      chunk_t *next = chunk_get_next(pc);

      if (chunk_is_newline(pc))
      {
         bool did_import = false;

         if ((p_imp != NULL) && (p_last != NULL) &&
             ((p_last->type == CT_SEMICOLON) ||
              (p_imp->flags & PCF_IN_PREPROC)))
         {
            if (num_chunks < MAX_NUMBER_TO_SORT)
            {
               LOG_FMT(LSORT, "p_imp %s\n", p_imp->text());
               chunks[num_chunks++] = p_imp;
            }
            else
            {
               fprintf(stderr, "Number of 'import' to be sorted is too big for the current value %d.\n", MAX_NUMBER_TO_SORT);
               fprintf(stderr, "Please make a report.\n");
               cpd.error_count++;
               exit(2);
            }
            did_import = true;
         }
         if (!did_import || (pc->nl_count > 1))
         {
            if (num_chunks > 1)
            {
               do_the_sort(chunks, num_chunks);
            }
            num_chunks = 0;
            memset(chunks, 0, sizeof(chunks));
         }
         p_imp  = NULL;
         p_last = NULL;
      }
      else if (pc->type == CT_IMPORT)
      {
         if (cpd.settings[UO_mod_sort_import].b)
         {
            p_imp = chunk_get_next(pc);
         }
      }
      else if (pc->type == CT_USING)
      {
         if (cpd.settings[UO_mod_sort_using].b)
         {
            p_imp = chunk_get_next(pc);
         }
      }
      else if (pc->type == CT_PP_INCLUDE)
      {
         if (cpd.settings[UO_mod_sort_include].b)
         {
            p_imp  = chunk_get_next(pc);
            p_last = pc;
         }
      }
      else if (!chunk_is_comment(pc))
      {
         p_last = pc;
      }
      pc = next;
   }
} // sort_imports
