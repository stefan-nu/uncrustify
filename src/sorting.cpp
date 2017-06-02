/**
 * @file sorting.cpp
 * Sorts chunks and imports
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "sorting.h"
#include "chunk_list.h"
#include <regex>


struct include_category
{
   include_category(const char* pattern)
      : regex(pattern)
   {}
   std::regex regex;
};

enum
{
   kIncludeCategoriesCount = UO_include_category_last - UO_include_category_first + 1
};


/**
 * Compare two series of chunks, starting with the given ones.
 *
 * \todo explain the return values, see unc_text::compare
 * @retval == 0 - both text elements are equal
 * @retval  > 0 - tbd
 * @retval  < 0 - tbd
 */
static int32_t compare_chunks(
   chunk_t* pc1, /**< [in] chunk 1 to compare */
   chunk_t* pc2  /**< [in] chunk 2 to compare */
);


/**
 * Sorting should be pretty rare and should usually only include a few chunks.
 * We need to minimize the number of swaps, as those are expensive.
 * So, we do a min sort.
 */
static void do_the_sort(
   chunk_t**      chunks,    /**< [in]  */
   const uint32_t num_chunks /**< [in]  */
);


/** tbd */
static int32_t get_chunk_priority(
   chunk_t* pc /**< [in]  */
);


/** tbd */
static void prepare_categories(void);


/** tbd */
static void cleanup_categories(void);


include_category* include_categories[kIncludeCategoriesCount];


static void prepare_categories(void)
{
   for (uint32_t i = 0; i < kIncludeCategoriesCount; i++)
   {
      if (cpd.settings[UO_include_category_first + i].str != nullptr)
      {
         include_categories[i] = new include_category(cpd.settings[UO_include_category_first + i].str);
      }
      else
      {
         include_categories[i] = nullptr;
      }
   }
}


static void cleanup_categories(void)
{
   for (auto& include_category : include_categories)
   {
      continue_if(ptr_is_invalid(include_category));
      delete include_category;
      include_category = NULL;
   }
}


static int32_t get_chunk_priority(chunk_t* pc)
{
   for (uint32_t i = 0; i < kIncludeCategoriesCount; i++)
   {
      if (ptr_is_valid(include_categories[i]))
      {
         if (std::regex_match(pc->text(), include_categories[i]->regex))
         {
            return((int32_t)i);
         }
      }
   }
   return((int32_t)kIncludeCategoriesCount);
}


static int32_t compare_chunks(chunk_t* pc1, chunk_t* pc2)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSORT, "\n@begin pc1->len=%u, line=%u, column=%u\n", pc1->len(), pc1->orig_line, pc1->orig_col);
   LOG_FMT(LSORT,   "@begin pc2->len=%u, line=%u, column=%u\n", pc2->len(), pc2->orig_line, pc2->orig_col);
   retval_if((pc1 == pc2), 0); /* same chunk is always identical thus return 0 differences */
   while (are_valid(pc1, pc2)) /* ensure there are two valid pointers */
   {
      const int32_t ppc1 = get_chunk_priority(pc1);
      const int32_t ppc2 = get_chunk_priority(pc2);

      if (ppc1 != ppc2) { return(ppc1 - ppc2); }

      LOG_FMT(LSORT, "text=%s, pc1->len=%u, line=%u, column=%u\n", pc1->text(), pc1->len(), pc1->orig_line, pc1->orig_col);
      LOG_FMT(LSORT, "text=%s, pc2->len=%u, line=%u, column=%u\n", pc2->text(), pc2->len(), pc2->orig_line, pc2->orig_col);
      const uint32_t min_len = min(pc1->len(), pc2->len());
      const int32_t  ret_val = unc_text::compare(pc1->str, pc2->str, min_len);
      LOG_FMT(LSORT, "ret_val=%d\n", ret_val);

      if (ret_val    != 0         ) { return(ret_val); }
      if (pc1->len() != pc2->len()) { return((long)pc1->len() - (long)pc2->len()); }

      /* Same word, same length. Step to the next chunk. */
      pc1 = chunk_get_next(pc1);
      if (is_valid(pc1))
      {
         LOG_FMT(LSORT, "text=%s, pc1->len=%u, line=%u, column=%u\n", pc1->text(), pc1->len(), pc1->orig_line, pc1->orig_col);
         if (is_type(pc1, CT_MEMBER))
         {
            pc1 = chunk_get_next(pc1);
            if(is_valid(pc1))
            {
               LOG_FMT(LSORT, "text=%s, pc1->len=%u, line=%u, column=%u\n",    pc1->text(), pc1->len(), pc1->orig_line, pc1->orig_col);
            }
         }
      }
      pc2 = chunk_get_next(pc2);
      if(is_valid(pc2))
      {
         LOG_FMT(LSORT, "text=%s, pc2->len=%u, line=%u, column=%u\n", pc2->text(), pc2->len(), pc2->orig_line, pc2->orig_col);
         if (is_type(pc2, CT_MEMBER))
         {
            pc2 = chunk_get_next(pc2);
            assert(is_valid(pc2));
            LOG_FMT(LSORT, "text=%s, pc2->len=%u, line=%u, column=%u\n", pc2->text(), pc2->len(), pc2->orig_line, pc2->orig_col);
         }
      }

      /* If we hit a newline or nullptr, we are done */
      break_if(are_invalid(pc1, pc2) ||
               is_nl(pc1) ||
               is_nl(pc2) );
   }

   retval_if((is_invalid(pc1) || !is_nl(pc2)), -1);
   retval_if(!is_nl(pc1), 1);
   return(0);
}


static void do_the_sort(chunk_t** chunks, const uint32_t num_chunks)
{
   LOG_FUNC_ENTRY();

   LOG_FMT(LSORT, "%s: %u chunks:", __func__, num_chunks);
   for (uint32_t idx = 0; idx < num_chunks; idx++)
   {
      LOG_FMT(LSORT, " [%s]", chunks[idx]->text());
   }
   LOG_FMT(LSORT, "\n");

   uint32_t start_idx;
   for (start_idx = 0; start_idx < (num_chunks - 1); start_idx++)
   {
      /* Find the index of the minimum value */
      uint32_t min_idx = start_idx;
      for (uint32_t idx = start_idx + 1; idx < num_chunks; idx++)
      {
         if (compare_chunks(chunks[idx], chunks[min_idx]) < 0)
         {
            min_idx = idx;
         }
      }

      /* Swap the lines if the minimum isn't the first entry */
      if (min_idx != start_idx)
      {
         swap_lines(chunks[start_idx], chunks[min_idx]);

         /* Don't need to swap, since we only want the side-effects */
         chunks[min_idx] = chunks[start_idx];
      }
   }
}


#define MAX_NUMBER_TO_SORT 256 /* \todo better use a list with dynamic size */
void sort_imports(void)
{
   LOG_FUNC_ENTRY();
   chunk_t* chunks[MAX_NUMBER_TO_SORT];
   uint32_t num_chunks = 0;
   chunk_t* p_last     = nullptr;
   chunk_t* p_imp      = nullptr;

   prepare_categories();

   chunk_t* pc = chunk_get_head();
   while (is_valid(pc))
   {
      chunk_t* next = chunk_get_next(pc);

      if (is_nl(pc))
      {
         bool did_import = false;

         if (are_valid(p_imp, p_last                             ) &&
             (is_type(p_last, CT_SEMICOLON ) || is_preproc(p_imp)) )
         {
            if (num_chunks < MAX_NUMBER_TO_SORT)
            {
               LOG_FMT(LSORT, "p_imp %s\n", p_imp->text());
               chunks[num_chunks++] = p_imp;
            }
            else
            {
               fprintf(stderr, "Number of 'import' to be sorted is too big \
                     for the current value %d.\n", MAX_NUMBER_TO_SORT);
               fprintf(stderr, "Please make a report.\n");
               log_flush(true);
               cpd.error_count++;
               exit(2);
            }
            did_import = true;
         }
         if ((did_import == false) ||
             (pc->nl_count > 1   ) ||
             (is_invalid(next)   ) )
         {
            if (num_chunks > 1)
            {
               do_the_sort(chunks, num_chunks);
            }
            num_chunks = 0;
         }
         p_imp  = nullptr;
         p_last = nullptr;
      }
      else if((is_type(pc, CT_IMPORT) && is_true(UO_mod_sort_import)) ||
              (is_type(pc, CT_USING ) && is_true(UO_mod_sort_using )) )
      {
         p_imp = chunk_get_next(pc);
      }
      else if (is_type(pc, CT_PP_INCLUDE) && is_true(UO_mod_sort_include))
      {
         p_imp  = chunk_get_next(pc);
         p_last = pc;
      }
      else if (!is_cmt(pc))
      {
         p_last = pc;
      }
      pc = next;
   }

   cleanup_categories();
}
