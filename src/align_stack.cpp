/**
 * @file align_stack.cpp
 * Manages an align stack, which is just a pair of chunk stacks.
 * There can be at most 1 item per line in the stack.
 * The seqnum is actually a line counter.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "align_stack.h"
#include "chunk_list.h"
#include "indent.h"
#include "space.h"
#include "tabulator.h"


void AlignStack::Start(uint32_t span, uint32_t thresh)
{
   LOG_FMT(LAS, "Start(%zu, %zu)\n", span, thresh);

   m_aligned.Reset();
   m_skipped.Reset();
   m_span        = span;
   m_thresh      = thresh;
   m_min_col     = 9999;
   m_max_col     = 0;
   m_nl_seqnum   = 0;
   m_seqnum      = 0;
   m_gap         = 0;
   m_right_align = false;
   m_star_style  = SS_IGNORE;
   m_amp_style   = SS_IGNORE;
}


void AlignStack::ReAddSkipped(void)
{
   if (!m_skipped.Empty())
   {
      /* Make a copy of the ChunkStack and clear m_skipped */
      m_scratch.Set(m_skipped);
      m_skipped.Reset();

      /* Need to add them in order so that m_nl_seqnum is correct */
      for (size_t idx = 0; idx < m_scratch.Len(); idx++)
      {
         const ChunkStack::Entry *ce = m_scratch.Get(idx);
         return_if(ptr_is_invalid(ce));

         LOG_FMT(LAS, "ReAddSkipped [%zu] - ", ce->m_seqnum);
         Add(ce->m_pc, ce->m_seqnum);
      }

      NewLines(0); /* Check to see if we need to flush right away */
   }
}


void AlignStack::Add(chunk_t* start, uint32_t seqnum)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(start));

   /* Assign a seqnum if needed */
   if (seqnum == 0) { seqnum = m_seqnum; }

   m_last_added = 0;

   /* Check threshold limits */
   if ( (m_max_col == 0) ||
        (m_thresh  == 0) ||
        (( (start->column + m_gap) <= (m_thresh + m_max_col)) && /* don't use subtraction here to prevent underflow */
         (((start->column + m_gap + m_thresh) >= (m_max_col)) ||
           (start->column                     >= m_min_col )) ) )
   {
      /* we are adding it, so update the newline seqnum */
      if (seqnum > m_nl_seqnum) { m_nl_seqnum = seqnum; }

      /* SS_IGNORE: no special handling of '*' or '&', only 'foo' is aligned
       *     void     foo;  // gap=5, 'foo' is aligned
       *     char *   foo;  // gap=3, 'foo' is aligned
       *     foomatic foo;  // gap=1, 'foo' is aligned
       *  The gap is the columns between 'foo' and the previous token.
       *  [void - foo], ['*' - foo], etc
       *
       * SS_INCLUDE: - space between variable and '*' or '&' is eaten
       *     void     foo;  // gap=5, 'foo' is aligned
       *     char     *foo; // gap=5, '*' is aligned
       *     foomatic foo;  // gap=1, 'foo' is aligned
       *  The gap is the columns between the first '*' or '&' before foo
       *  and the previous token. [void - foo], [char - '*'], etc
       *
       * SS_DANGLE: - space between variable and '*' or '&' is eaten
       *     void     foo;  // gap=5
       *     char    *bar;  // gap=5, as the '*' doesn't count
       *     foomatic foo;  // gap=1
       *  The gap is the columns between 'foo' and the chunk before the first
       *  '*' or '&'. [void - foo], [char - bar], etc
       *
       * If the gap < m_gap, then the column is bumped out by the difference.
       * So, if m_gap is 2, then the above would be:
       * SS_IGNORE:
       *     void      foo;  // gap=6
       *     char *    foo;  // gap=4
       *     foomatic  foo;  // gap=2
       * SS_INCLUDE:
       *     void      foo;  // gap=6
       *     char      *foo; // gap=6
       *     foomatic  foo;  // gap=2
       * SS_DANGLE:
       *     void      foo;  // gap=6
       *     char     *bar;  // gap=6, as the '*' doesn't count
       *     foomatic  foo;  // gap=2
       * Right aligned numbers:
       *     #define A    -1
       *     #define B   631
       *     #define C     3
       * Left aligned numbers:
       *     #define A     -1
       *     #define B     631
       *     #define C     3
       *
       * In the code below, pc is set to the item that is aligned.
       * In the above examples, that is 'foo', '*', '-', or 63.
       *
       * Ref is set to the last part of the type.
       * In the above examples, that is 'void', 'char', 'foomatic', 'A', or 'B'.
       *
       * The '*' and '&' can float between the two.
       *
       * If align_on_tabstop=true, then SS_DANGLE is changed to SS_INCLUDE. */
      if (is_true(UO_align_on_tabstop) && (m_star_style == SS_DANGLE))
      {
         m_star_style = SS_INCLUDE;
      }

      /* Find ref. Back up to the real item that is aligned. */
      chunk_t* prev = start;
      while (((prev = chunk_get_prev(prev)) != nullptr) &&
             (is_ptr_operator(prev) || is_type(prev, CT_TPAREN_OPEN)))
      {
         /* do nothing - we want prev when this exits */
      }
      chunk_t* ref = prev;
      if (is_nl(ref))
      {
         ref = chunk_get_next(ref);
      }

      /* Find the item that we are going to align. */
      chunk_t* ali = start;
      if (m_star_style != SS_IGNORE)
      {
         /* back up to the first '*' or '^' preceding the token */
         prev = chunk_get_prev(ali);
         while (is_star(prev) || is_msref(prev))
         {
            ali  = prev;
            prev = chunk_get_prev(ali);
         }
         if (is_type(prev, CT_TPAREN_OPEN))
         {
            ali  = prev;
            prev = chunk_get_prev(ali);
            // this is correct, even Coverity says:
            // CID 76021 (#1 of 1): Unused value (UNUSED_VALUE)returned_pointer: Assigning value from
            // chunk_get_prev(ali, nav_e::ALL) to prev here, but that stored value is overwritten before it can be used.
         }
      }
      if (m_amp_style != SS_IGNORE)
      {
         /* back up to the first '&' preceding the token */
         prev = chunk_get_prev(ali);
         while (is_addr(prev))
         {
            ali  = prev;
            prev = chunk_get_prev(ali);
         }
      }

      return_if(are_invalid(ali, ref));

      chunk_t* tmp;
      /* Tighten down the spacing between ref and start */
      if (is_false(UO_align_keep_extra_space))
      {
         uint32_t tmp_col = ref->column;
         tmp = ref;
         while (tmp != start)
         {
            chunk_t* next = chunk_get_next(tmp);
            assert(is_valid(next));
            tmp_col += (size_t)space_col_align(tmp, next); // \todo ensure the result cannot become negative
            if (next->column != tmp_col)
            {
               align_to_column(next, tmp_col);
            }
            tmp = next;
         }
      }

      /* Set the column adjust and gap */
      uint32_t col_adj = 0; /* Amount the column is shifted for 'dangle' mode */
      uint32_t gap     = 0;
      if (ref != ali)
      {
         gap = ali->column - (ref->column + ref->len());
      }
      tmp = ali;
      if (is_type(tmp, CT_TPAREN_OPEN))
      {
         tmp = chunk_get_next(tmp);
      }
      if ((is_star (tmp) && (m_star_style == SS_DANGLE)) ||
          (is_addr (tmp) && (m_amp_style  == SS_DANGLE)) ||
          (is_msref(tmp) && (m_star_style == SS_DANGLE)) ) // TODO: add m_msref_style
      {
         col_adj = start->column - ali->column;
         gap     = start->column - (ref->column + ref->len());
      }

      /* See if this pushes out the max_col */
      size_t endcol = ali->column + col_adj;
      if (gap < m_gap)
      {
         endcol += m_gap - gap;
      }

      // LOG_FMT(LSYS, "[%p] line %d pc='%s' [%s] col:%d ali='%s' [%s] col:%d ref='%s' [%s] col:%d  col_adj=%d  endcol=%d, ss=%d as=%d, gap=%d\n",
      //         this,
      //         start->orig_line,
      //         start->text(), get_token_name(start->type), start->column,
      //         ali->text(), get_token_name(ali->type), ali->column,
      //         ref->text(), get_token_name(ref->type), ref->column,
      //         col_adj, endcol, m_star_style, m_amp_style, gap);

      ali->align.col_adj = (int)col_adj;
      ali->align.ref     = ref;
      ali->align.start   = start;
      m_aligned.Push_Back(ali, seqnum);
      m_last_added = 1;

      LOG_FMT(LAS, "Add-[%s]: line %zu, col %zu, adj %d : ref=[%s] endcol=%zu\n",
              ali->text(), ali->orig_line, ali->column, ali->align.col_adj,
              ref->text(), endcol);

      m_min_col = min(m_min_col, endcol);
      if (endcol > m_max_col)
      {
         LOG_FMT(LAS, "Add-aligned [%zu/%zu/%zu]: line %zu, col %zu : max_col old %zu, new %zu - min_col %zu\n",
                 seqnum, m_nl_seqnum, m_seqnum,
                 ali->orig_line, ali->column, m_max_col, endcol, m_min_col);
         m_max_col = endcol;

         /* If there were any entries that were skipped, re-add them as they
          * may now be within the threshold */
         if (!m_skipped.Empty()) { ReAddSkipped(); }
      }
      else
      {
         LOG_FMT(LAS, "Add-aligned [%zu/%zu/%zu]: line %zu, col %zu : col %zu <= %zu - min_col %zu\n",
                 seqnum, m_nl_seqnum, m_seqnum,
                 ali->orig_line, ali->column, endcol, m_max_col, m_min_col);
      }
   }
   else
   {
      /* The threshold check failed, so add it to the skipped list */
      m_skipped.Push_Back(start, seqnum);
      m_last_added = 2;

      LOG_FMT(LAS, "Add-skipped [%zu/%zu/%zu]: line %zu, col %zu <= %zu + %zu\n",
              seqnum, m_nl_seqnum, m_seqnum,
              start->orig_line, start->column, m_max_col, m_thresh);
   }
}


void AlignStack::NewLines(size_t cnt)
{
   if (!m_aligned.Empty())
   {
      m_seqnum += cnt;
      if (m_seqnum > (m_nl_seqnum + m_span))
      {
         LOG_FMT(LAS, "Newlines<%zu>-", cnt);
         Flush();
      }
      else
      {
         LOG_FMT(LAS, "Newlines<%zu>\n", cnt);
      }
   }
}


void AlignStack::Flush(void)
{
   LOG_FMT(LAS, "%s: m_aligned.Len()=%zu\n", __func__, m_aligned.Len());
   LOG_FMT(LAS, "Flush (min=%zu, max=%zu)\n", m_min_col, m_max_col);

   chunk_t* pc;
   if (m_aligned.Len() == 1)
   {
      /* check if we have *one* typedef in the line */
      assert(ptr_is_valid(m_aligned.Get(0)));
      pc = m_aligned.Get(0)->m_pc;
      chunk_t* temp = get_prev_type(pc, CT_TYPEDEF, (int)pc->level);
      if (is_valid(temp))
      {
         if (pc->orig_line == temp->orig_line)
         {
            /* reset the gap only for *this* stack */
            m_gap = 1;
         }
      }
   }

   m_last_added = 0;
   m_max_col    = 0;

   /* Recalculate the max_col - it may have shifted since the last Add() */
   for (size_t idx = 0; idx < m_aligned.Len(); idx++)
   {
      assert(ptr_is_valid(m_aligned.Get(idx)));
      pc = m_aligned.Get(idx)->m_pc;

      /* Set the column adjust and gap */
      int      col_adj = 0;
      uint32_t gap     = 0u;
      if (pc != pc->align.ref)
      {
         gap = pc->column - (pc->align.ref->column + pc->align.ref->len());
      }
      chunk_t* tmp = pc;
      if (is_type(tmp, CT_TPAREN_OPEN))
      {
         tmp = chunk_get_next(tmp);
      }
      if (is_ptr_operator(tmp) &&
         (m_star_style == SS_DANGLE))
      {
         col_adj = (int)pc->align.start->column - (int)pc->column;
         gap     =      pc->align.start->column - (pc->align.ref->column + pc->align.ref->len());
      }
      if (m_right_align)
      {
         /* Adjust the width for signed numbers */
         size_t start_len = pc->align.start->len();
         if (is_type(pc->align.start, CT_NEG))
         {
            tmp = chunk_get_next(pc->align.start);
            if(is_type(tmp, CT_NUMBER))
            {
               start_len += tmp->len();
            }
         }
         col_adj += (int)start_len;
      }

      pc->align.col_adj = col_adj;

      /* See if this pushes out the max_col */
      size_t endcol = pc->column + (size_t)col_adj;
      if (gap    < m_gap    ) { endcol += m_gap - gap; }
      if (endcol > m_max_col) { m_max_col = endcol;    }
   }

   if (is_true(UO_align_on_tabstop) && (m_aligned.Len() > 1))
   {
      m_max_col = align_tab_column(m_max_col);
   }

   const ChunkStack::Entry *ce = nullptr;
   LOG_FMT(LAS, "%s: m_aligned.Len()=%zu\n", __func__, m_aligned.Len());
   for (size_t idx = 0; idx < m_aligned.Len(); idx++)
   {
      ce = m_aligned.Get(idx);
      assert(ptr_is_valid(ce));
      pc = ce->m_pc;

      const uint32_t tmp_col = (size_t)((int)m_max_col - pc->align.col_adj);
      if (idx == 0)
      {
         if ((m_skip_first == true   ) &&
             (pc->column   != tmp_col) )
         {
            LOG_FMT(LAS, "%s: %zu:%zu dropping first item due to skip_first\n",
                    __func__, pc->orig_line, pc->orig_col);
            m_skip_first = false;
            m_aligned.Pop_Front();
            Flush();
            m_skip_first = true;
            return;
         }
         set_flags(pc, PCF_ALIGN_START);

         pc->align.right_align = m_right_align;
         pc->align.amp_style   = m_amp_style;
         pc->align.star_style  = m_star_style;
      }
      pc->align.gap  = m_gap;
      pc->align.next = m_aligned.GetChunk(idx + 1);

      /* Indent the token, taking col_adj into account */
      LOG_FMT(LAS, "%s: line %zu: '%s' to col %zu (adj=%d)\n",
              __func__, pc->orig_line, pc->text(), tmp_col, pc->align.col_adj);
      align_to_column(pc, tmp_col);
   }

   uint32_t last_seqnum = 0;
   if (ptr_is_valid(ce))
   {
      last_seqnum = ce->m_seqnum;
      m_aligned.Reset();
   }
   m_min_col = 9999; /* use unrealistic high numbers */
   m_max_col = 0;    /* as start value */

   if (m_skipped.Empty())
   {
      /* Nothing was skipped, sync the sequence numbers */
      m_nl_seqnum = m_seqnum;
   }
   else
   {
      /* Remove all items with seqnum < last_seqnum */
      for (size_t idx = 0; idx < m_skipped.Len(); idx++)
      {
         if (m_skipped.Get(idx)->m_seqnum < last_seqnum) /*lint !e613 */
         {
            m_skipped.Zap(idx);
         }
      }
      m_skipped.Collapse();

      ReAddSkipped();   /* Add all items from the skipped list */
   }
}


void AlignStack::Reset(void)
{
   m_aligned.Reset();
   m_skipped.Reset();
}


void AlignStack::End(void)
{
   if (!m_aligned.Empty())
   {
      LOG_FMT(LAS, "End-");
      Flush();
   }
   AlignStack::Reset();
}
