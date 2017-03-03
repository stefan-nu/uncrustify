/**
 * @file detect.cpp
 * Scans the parsed file and tries to determine options.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "detect.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "ChunkStack.h"
#include <cstdio>
#include <cstdlib>
#include "unc_ctype.h"


/**
 * Detect spacing options
 */
static void detect_space_options(void);

class sp_votes
{
protected:
   size_t   m_add;
   size_t   m_remove;
   size_t   m_force;
   argval_t *m_av;

public:
   explicit sp_votes(argval_t &av)
   {
      m_add    = 0;
      m_remove = 0;
      m_force  = 0;
      m_av     = &av;
   }

   ~sp_votes();

   void vote(chunk_t *first, chunk_t *second);
};


void sp_votes::vote(chunk_t *first, chunk_t *second)
{
   if (!are_valid(first, second) ||
        chunk_is_newline(first ) ||
        chunk_is_newline(second) )
   {
      return;
   }

   int col_dif = (int)second->column - (int)(first->column + first->len());

   if      (col_dif == 0) { m_remove++; }
   else if (col_dif == 1) { m_force++;  }
   else                   { m_add++;    }
}


/**
 * Figure out the result of the vote and maybe update *m_av
 */
sp_votes::~sp_votes()
{
   /* no change if no items were added */
   if ((m_remove == 0) &&
       (m_add    == 0) &&
       (m_force  == 0) )
   {
      return;
   }

   if (m_remove == 0)
   {
      *m_av = (m_force > m_add) ? AV_FORCE : AV_ADD;
   }
   else if ((m_force == 0) &&
            (m_add   == 0) )
   {
      *m_av = AV_REMOVE;
   }
   else
   {
      /* nothing conclusive. do not alter. */
   }
}


/* generates "vote_sp_xxx" variable name from uncrustify option name "UO_xxx" */
#define SP_VOTE_VAR(x)    sp_votes vote_ ## x(cpd.settings[UO_ ## x].a)


static void detect_space_options(void)
{
   SP_VOTE_VAR(sp_arith);
   SP_VOTE_VAR(sp_before_assign);
   SP_VOTE_VAR(sp_after_assign);
   SP_VOTE_VAR(sp_enum_before_assign);
   SP_VOTE_VAR(sp_enum_after_assign);
   SP_VOTE_VAR(sp_bool);
   SP_VOTE_VAR(sp_compare);
   SP_VOTE_VAR(sp_inside_paren);
   SP_VOTE_VAR(sp_paren_paren);
   SP_VOTE_VAR(sp_paren_brace);
   SP_VOTE_VAR(sp_before_ptr_star);
   SP_VOTE_VAR(sp_before_unnamed_ptr_star);
   SP_VOTE_VAR(sp_between_ptr_star);
   SP_VOTE_VAR(sp_after_ptr_star);
   SP_VOTE_VAR(sp_after_byref);
   SP_VOTE_VAR(sp_before_byref);
   SP_VOTE_VAR(sp_before_unnamed_byref);
   SP_VOTE_VAR(sp_after_type);
   SP_VOTE_VAR(sp_template_angle);
   SP_VOTE_VAR(sp_before_angle);
   SP_VOTE_VAR(sp_inside_angle);
   SP_VOTE_VAR(sp_after_angle);
   SP_VOTE_VAR(sp_angle_paren);
   SP_VOTE_VAR(sp_angle_word);
   SP_VOTE_VAR(sp_before_square);
   SP_VOTE_VAR(sp_before_squares);
   SP_VOTE_VAR(sp_inside_square);
   SP_VOTE_VAR(sp_before_sparen);
   SP_VOTE_VAR(sp_inside_sparen);
   SP_VOTE_VAR(sp_after_sparen);
   SP_VOTE_VAR(sp_sparen_brace);
   SP_VOTE_VAR(sp_special_semi);
   SP_VOTE_VAR(sp_before_semi);
   SP_VOTE_VAR(sp_before_semi_for);
   SP_VOTE_VAR(sp_before_semi_for_empty);
   SP_VOTE_VAR(sp_after_semi_for_empty);
   SP_VOTE_VAR(sp_after_comma);
   SP_VOTE_VAR(sp_before_comma);
   SP_VOTE_VAR(sp_after_class_colon);
   SP_VOTE_VAR(sp_before_class_colon);
   SP_VOTE_VAR(sp_inside_braces);
   SP_VOTE_VAR(sp_inside_braces_empty);
   SP_VOTE_VAR(sp_else_brace);
   SP_VOTE_VAR(sp_brace_else);
   SP_VOTE_VAR(sp_catch_brace);
   SP_VOTE_VAR(sp_brace_catch);
// SP_VOTE_VAR(sp_finally_brace);
   SP_VOTE_VAR(sp_brace_finally);
// SP_VOTE_VAR(sp_try_brace);
// SP_VOTE_VAR(sp_getset_brace);

   chunk_t *prev = chunk_get_head();
   chunk_t *pc   = chunk_get_next(prev);
   chunk_t *next;

   while (is_valid(pc))
   {
      next = chunk_get_next(pc);
      if (is_invalid(next)) { break; }

      if (is_type(pc, CT_ARITH))
      {
         vote_sp_arith.vote(pc, next);
         vote_sp_arith.vote(prev, pc);
      }
      if (is_type(pc, CT_ASSIGN))
      {
         if ((pc->flags & PCF_IN_ENUM) == 0) { vote_sp_before_assign.vote     (prev, pc); vote_sp_after_assign.vote     (pc, next); }
         else                                { vote_sp_enum_before_assign.vote(prev, pc); vote_sp_enum_after_assign.vote(pc, next); }
      }
      if (is_type(pc, CT_SQUARE_OPEN )) { vote_sp_before_square.vote (prev, pc); vote_sp_inside_square.vote(pc, next); }
      if (is_type(pc, CT_SQUARE_CLOSE)) { vote_sp_inside_square.vote (prev, pc); }
      if (is_type(pc, CT_TSQUARE     )) { vote_sp_before_squares.vote(prev, pc); }
      if (is_type(pc, CT_BOOL        )) { vote_sp_bool.vote          (prev, pc); vote_sp_bool.vote   (pc, next); }
      if (is_type(pc, CT_COMPARE     )) { vote_sp_compare.vote       (prev, pc); vote_sp_compare.vote(pc, next); }
      if (is_type(pc, CT_PAREN_CLOSE )) { vote_sp_inside_paren.vote  (prev, pc); }
      if (is_type(pc, CT_PAREN_OPEN  )) { vote_sp_inside_paren.vote  (pc, next); }

      if ((chunk_is_paren_open (pc) && chunk_is_paren_open (next)) ||
          (chunk_is_paren_close(pc) && chunk_is_paren_close(next)) )
      {
         vote_sp_paren_paren.vote(pc, next);
      }
      if (chunk_is_paren_close(pc) &&
          is_type(next, CT_BRACE_OPEN))
      {
         vote_sp_paren_brace.vote(pc, next);
      }

      if (is_type(pc, CT_PTR_TYPE))
      {
         if      (is_type    (prev, CT_PTR_TYPE)) { vote_sp_between_ptr_star.vote       (prev, pc); }
         else if (is_not_type(next, CT_WORD    )) { vote_sp_before_unnamed_ptr_star.vote(prev, pc); }
         else                                { vote_sp_before_ptr_star.vote        (prev, pc); }
         if (CharTable::IsKW1((size_t)next->str[0])) { vote_sp_after_ptr_star.vote  (pc, next); }
      }

      if (is_type(pc, CT_BYREF))
      {
         if (is_not_type(next, CT_WORD)) { vote_sp_before_unnamed_byref.vote(prev, pc); }
         else                                  { vote_sp_before_byref.vote        (prev, pc); }
         vote_sp_after_byref.vote(pc, next);
      }

      if (is_not_type(pc,      CT_PTR_TYPE          ) &&
          is_type    (prev, 2, CT_QUALIFIER, CT_TYPE) )
      {
         vote_sp_after_type.vote(prev, pc);
      }
      if (is_type(pc, CT_ANGLE_OPEN))
      {
         vote_sp_inside_angle.vote(pc, next);
         if (is_type(prev, CT_TEMPLATE)) { vote_sp_template_angle.vote(prev, pc); }
         else                                  { vote_sp_before_angle.vote  (prev, pc); }
      }

      if (is_type(pc, CT_ANGLE_CLOSE))
      {
                                                              vote_sp_inside_angle.vote(prev, pc);
         if      (chunk_is_paren_open(next))                { vote_sp_angle_paren.vote (prev, pc); }
         else if (is_type(next, CT_WORD) ||
               CharTable::IsKW1((size_t)next->str[0])) { vote_sp_angle_word.vote  (prev, pc); }
         else                                               { vote_sp_after_angle.vote (pc, next); }
      }

      if (is_type(pc, CT_SPAREN_OPEN))
      {
         vote_sp_before_sparen.vote(prev, pc);
         vote_sp_inside_sparen.vote(pc, next);
      }
      if (is_type(pc, CT_SPAREN_CLOSE))
      {
         vote_sp_inside_sparen.vote(prev, pc);
         if (is_type(next, CT_BRACE_OPEN)) { vote_sp_sparen_brace.vote(pc, next); }
         else                                    { vote_sp_after_sparen.vote(pc, next); }
      }
      if (is_type(pc, CT_SEMICOLON))
      {
         if (is_type(pc, CT_FOR))
         {
            if      (is_type(prev, CT_SPAREN_OPEN  )) { vote_sp_before_semi_for_empty.vote(prev, pc); } /* empty, ie for (;;) */
            else if (is_type(next, CT_SPAREN_CLOSE )) { vote_sp_after_semi_for_empty.vote (pc, next); } /* empty, ie for (;;) */
            else if (is_not_type(prev, CT_SEMICOLON)) { vote_sp_before_semi_for.vote      (prev, pc); }
         }
         else if (is_type(prev, CT_VBRACE_OPEN)) { vote_sp_special_semi.vote(chunk_get_prev(prev), pc); }
         else                                   { vote_sp_before_semi.vote (               prev,  pc); }
      }

      if (is_type(pc, CT_COMMA      )) { vote_sp_before_comma.vote      (prev, pc); vote_sp_after_comma.vote      (pc, next); }
      if (is_type(pc, CT_CLASS_COLON)) { vote_sp_before_class_colon.vote(prev, pc); vote_sp_after_class_colon.vote(pc, next); }

      if (is_type(pc, CT_BRACE_OPEN))
      {
         switch(prev->type)
         {
            case(CT_ELSE   ): { vote_sp_else_brace.vote (prev, pc); break; }
            case(CT_CATCH  ):   /* fall through */
            case(CT_FINALLY):   /* fall through */
            case(CT_TRY    ):   /* fall through */
            case(CT_GETSET ): { vote_sp_catch_brace.vote(prev, pc); break; }
            default:          { /* do nothing */                    break; }
         }

         if (is_type(next, CT_BRACE_CLOSE)) { vote_sp_inside_braces_empty.vote(pc, next); }
         else                                     { vote_sp_inside_braces.vote      (pc, next); }
      }
      if (is_type(pc, CT_BRACE_CLOSE))
      {
         vote_sp_inside_braces.vote(prev, pc);
         switch(next->type)
         {
            case(CT_ELSE   ): { vote_sp_brace_else.vote   (pc, next); break; }
            case(CT_CATCH  ): { vote_sp_brace_catch.vote  (pc, next); break; }
            case(CT_FINALLY): { vote_sp_brace_finally.vote(pc, next); break; }
            default:          { /* do nothing */                      break; }
         }
      }

      prev = pc;
      pc   = next;
   }
}


void detect_options(void)
{
   detect_space_options();
}
