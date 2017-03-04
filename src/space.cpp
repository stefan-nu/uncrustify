/**
 * @file space.cpp
 * Adds or removes inter-chunk spaces.
 *
 * Informations
 *   "Ignore" means do not change it.
 *   "Add" in the context of spaces means make sure there is at least 1.
 *   "Add" elsewhere means make sure one is present.
 *   "Remove" mean remove the space/brace/newline/etc.
 *   "Force" in the context of spaces means ensure that there is exactly 1.
 *   "Force" in other contexts means the same as "add".
 *
 *   Rmk: spaces = space + nl
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */
#include "space.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "punctuators.h"
#include "char_table.h"
#include "options_for_QT.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "unc_ctype.h"
#include "uncrustify.h"


static void log_rule2(
   size_t     line,    /**< [in]  */
   const char *rule,   /**< [in]  */
   chunk_t    *pc1,  /**< [in]  */
   chunk_t    *pc2, /**< [in]  */
   bool       complete /**< [in]  */
);


/**
 * Decides how to change inter-chunk spacing.
 * Note that the order of the if statements is VERY important.
 *
 * @return AV_IGNORE, AV_ADD, AV_REMOVE or AV_FORCE
 */
static argval_t do_space(
   chunk_t *pc1,    /**< [in]  The first chunk */
   chunk_t *pc2,    /**< [in]  The second chunk */
   int     &min_sp, /**< [out] minimal required space */
   bool    complete /**< [in]  */
);


struct no_space_table_t
{
   c_token_t first;   /**< [in]  */
   c_token_t second;  /**< [in]  */
};


/**
 * this table lists all combos where a space should NOT be present
 * CT_UNKNOWN is a wildcard.
 *
 * TODO: some of these are no longer needed.
 */
const no_space_table_t no_space_table[] =
{
   { CT_OC_AT,          CT_UNKNOWN       },
   { CT_INCDEC_BEFORE,  CT_WORD          },
   { CT_UNKNOWN,        CT_INCDEC_AFTER  },
   { CT_UNKNOWN,        CT_LABEL_COLON   },
   { CT_UNKNOWN,        CT_PRIVATE_COLON },
   { CT_UNKNOWN,        CT_SEMICOLON     },
   { CT_UNKNOWN,        CT_D_TEMPLATE    },
   { CT_D_TEMPLATE,     CT_UNKNOWN       },
   { CT_MACRO_FUNC,     CT_FPAREN_OPEN   },
   { CT_PAREN_OPEN,     CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_PAREN_CLOSE   },
   { CT_FPAREN_OPEN,    CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_SPAREN_CLOSE  },
   { CT_SPAREN_OPEN,    CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_FPAREN_CLOSE  },
   { CT_UNKNOWN,        CT_COMMA         },
   { CT_POS,            CT_UNKNOWN       },
   { CT_STAR,           CT_UNKNOWN       },
   { CT_VBRACE_CLOSE,   CT_UNKNOWN       },
   { CT_VBRACE_OPEN,    CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_VBRACE_CLOSE  },
   { CT_UNKNOWN,        CT_VBRACE_OPEN   },
   { CT_PREPROC,        CT_UNKNOWN       },
   { CT_PREPROC_INDENT, CT_UNKNOWN       },
   { CT_NEG,            CT_UNKNOWN       },
   { CT_UNKNOWN,        CT_SQUARE_OPEN   },
   { CT_UNKNOWN,        CT_SQUARE_CLOSE  },
   { CT_SQUARE_OPEN,    CT_UNKNOWN       },
   { CT_PAREN_CLOSE,    CT_WORD          },
   { CT_PAREN_CLOSE,    CT_FUNC_DEF      },
   { CT_PAREN_CLOSE,    CT_FUNC_CALL     },
   { CT_PAREN_CLOSE,    CT_ADDR          },
   { CT_PAREN_CLOSE,    CT_FPAREN_OPEN   },
   { CT_OC_SEL_NAME,    CT_OC_SEL_NAME   },
   { CT_TYPENAME,       CT_TYPE          },
};



#define log_argval_and_return(a)                                    \
   do {                                                             \
      if (log_sev_on(LSPACE))                                       \
      { log_rule2(__LINE__, (argval2str(a)), pc1, pc2, complete); } \
      return(a);                                                    \
   } while(0)

#define log_option_and_return(uo)                                 \
   do {                                                           \
      const option_map_value_t* my_uo = get_option_name(uo);      \
      if (log_sev_on(LSPACE))                                     \
      { log_rule2(__LINE__, (my_uo->name), pc1, pc2, complete); } \
      return(cpd.settings[uo].a);                                 \
   } while(0)

#define log_rule(rule)                                        \
   do {                                                       \
      if (log_sev_on(LSPACE))                                 \
      { log_rule2(__LINE__, (rule), pc1, pc2, complete); }    \
   } while (0)


static void log_rule2(size_t line, const char *rule, chunk_t *pc1, chunk_t *pc2, bool complete)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(pc1, pc2));

   if (is_not_type(pc2, CT_NEWLINE))
   {
      LOG_FMT(LSPACE, "Spacing: line %zu [%s/%s] '%s' <===> [%s/%s] '%s' : %s[%zu]%s",
              pc1->orig_line, get_token_name(pc1->type),
              get_token_name(pc1->ptype), pc1->text(),
              get_token_name(pc2->type), get_token_name(pc2->ptype),
              pc2->text(), rule, line, complete ? "\n" : "");
   }
}


/* \todo make min_sp a size_t */
static argval_t do_space(chunk_t *pc1, chunk_t *pc2, int &min_sp, bool complete = true)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(pc1, pc2));

   LOG_FMT(LSPACE, "%s: %zu:%zu %s %s\n", __func__, pc1->orig_line,
         pc1->orig_col, pc1->text(), get_token_name(pc1->type));

   min_sp = 1;
   if(any_is_type(pc1, pc2, CT_IGNORED  ))   { log_argval_and_return(AV_REMOVE                ); }
   /* Leave spacing alone between PP_IGNORE tokens as we don't want the default behavior (which is ADD) */
   if(are_types  (pc1, pc2, CT_PP_IGNORE))   { log_argval_and_return(AV_IGNORE                ); }
   if(any_is_type(pc1, pc2, CT_PP       ))   { log_option_and_return(UO_sp_pp_concat          ); }
   if(is_type    (pc1, CT_POUND    ))        { log_option_and_return(UO_sp_pp_stringify       ); }
   if(is_type    (pc2, CT_POUND  ) && (pc2->flags & PCF_IN_PREPROC  ) &&
      is_not_ptype(pc1, CT_MACRO_FUNC))      { log_option_and_return(UO_sp_before_pp_stringify); }
   if (any_is_type(pc1, pc2, CT_SPACE))      { log_argval_and_return(AV_REMOVE                ); }
   if (is_type(pc2, 2, CT_NEWLINE, CT_VBRACE_OPEN))
                                             { log_argval_and_return(AV_REMOVE                ); }
   if (is_only_first_type(pc1, CT_VBRACE_OPEN, pc2, CT_NL_CONT))
                                             { log_argval_and_return(AV_FORCE                 ); }
   if (is_only_first_type(pc1, CT_VBRACE_CLOSE, pc2, CT_NL_CONT))
                                             { log_argval_and_return(AV_REMOVE                ); }
   if (is_type(pc2, CT_VSEMICOLON))          { log_argval_and_return(AV_REMOVE                ); }
   if (is_type(pc1, CT_MACRO_FUNC))          { log_argval_and_return(AV_REMOVE                ); }
   if (is_type(pc2, CT_NL_CONT))             { log_option_and_return(UO_sp_before_nl_cont     ); }
   if (any_is_type(pc1, pc2, CT_D_ARRAY_COLON))
                                             { log_option_and_return(UO_sp_d_array_colon      ); }
   if ( is_type(pc1, CT_CASE                  ) &&
       (CharTable::IsKW1((size_t)(pc2->str[0])) ||
        is_type(pc2, CT_NUMBER)               ) )
   {
      log_rule("sp_case_label");
      return(add_option(cpd.settings[UO_sp_case_label].a, AV_ADD));
   }
   if (is_type  (pc1, CT_FOR_COLON))         { log_option_and_return(UO_sp_after_for_colon    ); }
   if (is_type  (pc2, CT_FOR_COLON))         { log_option_and_return(UO_sp_before_for_colon   ); }
   if (are_types(pc1, CT_QUESTION, pc2, CT_COND_COLON))
   {  if (is_not_option(cpd.settings[UO_sp_cond_ternary_short].a, AV_IGNORE))
                                             { log_option_and_return(UO_sp_cond_ternary_short ); }}
   if (any_is_type(pc1, pc2, CT_QUESTION))
   {  if (is_type(pc2, CT_QUESTION) &&
         (is_not_option(cpd.settings[UO_sp_cond_question_before].a, AV_IGNORE)))
                                             { log_option_and_return(UO_sp_cond_question_before); }
      if (is_type(pc1, CT_QUESTION) &&
          (is_not_option(cpd.settings[UO_sp_cond_question_after].a, AV_IGNORE)))
                                             { log_option_and_return(UO_sp_cond_question_after); }
      if (is_not_option(cpd.settings[UO_sp_cond_question].a, AV_IGNORE))
                                             { log_option_and_return(UO_sp_cond_question); } }
   if (any_is_type(pc1, pc2, CT_COND_COLON))
   {  if (is_type(pc2, CT_COND_COLON) &&
          is_not_option(cpd.settings[UO_sp_cond_colon_before].a, AV_IGNORE))
                                             { log_option_and_return(UO_sp_cond_colon_before); }
      if (is_type(pc1, CT_COND_COLON) &&
          (is_not_option(cpd.settings[UO_sp_cond_colon_after].a, AV_IGNORE)))
                                             { log_option_and_return(UO_sp_cond_colon_after); }
      if (is_not_option(cpd.settings[UO_sp_cond_colon].a, AV_IGNORE))
                                             { log_option_and_return(UO_sp_cond_colon); }
   }
   if (any_is_type(pc1, pc2, CT_RANGE)) { log_option_and_return(UO_sp_range); }
   if (is_type_and_ptype(pc1, CT_COLON, CT_SQL_EXEC)) { log_argval_and_return(AV_REMOVE); }
   /* Macro stuff can only return IGNORE, ADD, or FORCE */
   if (is_type(pc1, CT_MACRO))
   {  log_rule("sp_macro");
      argval_t arg     = cpd.settings[UO_sp_macro].a;
      argval_t add_arg = is_not_option(arg, AV_IGNORE) ? AV_ADD : AV_IGNORE;
      return (add_option(arg, add_arg));
   }
   if (is_type_and_ptype(pc1, CT_FPAREN_CLOSE, CT_MACRO_FUNC))
   {
      log_rule("sp_macro_func");
      argval_t arg     = cpd.settings[UO_sp_macro_func].a;
      argval_t add_arg = is_not_option(arg, AV_IGNORE) ? AV_ADD : AV_IGNORE;
      return (add_option(arg, add_arg));
   }

   if (is_type(pc1, CT_PREPROC))
   {
      /* Remove spaces, unless we are ignoring. See indent_preproc() */
      if (cpd.settings[UO_pp_space].a == AV_IGNORE) { log_argval_and_return(AV_IGNORE); }
      log_argval_and_return(AV_REMOVE);
   }

   if (is_type(pc2, CT_SEMICOLON))
   {
      if (is_ptype(pc2, CT_FOR))
      {
         if ((is_not_option(cpd.settings[UO_sp_before_semi_for_empty].a, AV_IGNORE)) &&
             is_type(pc1, 2, CT_SPAREN_OPEN, CT_SEMICOLON) )
         {
            log_option_and_return(UO_sp_before_semi_for_empty);
         }
         if (is_not_option(cpd.settings[UO_sp_before_semi_for].a, AV_IGNORE))
         {
            log_option_and_return(UO_sp_before_semi_for);
         }
      }
      argval_t arg = cpd.settings[UO_sp_before_semi].a;
#if 1
      if ((pc1->type  == CT_SPAREN_CLOSE) &&
          (pc1->ptype != CT_WHILE_OF_DO ) )
#else
      // fails test 01011, 30711
      if (is_type     (pc1, CT_SPAREN_CLOSE) &&
          is_not_ptype(pc1, CT_WHILE_OF_DO ) )
#endif
      {
         log_rule("sp_before_semi|sp_special_semi");
         arg = (add_option(arg, cpd.settings[UO_sp_special_semi].a));
      }
      else { log_rule("sp_before_semi"); }
      return(arg);
   }

   if (is_type(pc2,    CT_COMMENT             ) &&
       is_type(pc1, 2, CT_PP_ELSE, CT_PP_ENDIF) )
   {
      if (is_not_option(cpd.settings[UO_sp_endif_cmt].a, AV_IGNORE))
      {
         set_type(pc2, CT_COMMENT_ENDIF);
         log_option_and_return(UO_sp_endif_cmt);
      }
   }

   if ((is_not_option(cpd.settings[UO_sp_before_tr_emb_cmt].a, AV_IGNORE)) &&
       (is_ptype(pc2, 2, CT_COMMENT_END, CT_COMMENT_EMBED)            ) )
   {
      min_sp = cpd.settings[UO_sp_num_before_tr_emb_cmt].n;
      log_option_and_return(UO_sp_before_tr_emb_cmt);
   }

   if (is_ptype(pc2, CT_COMMENT_END))
   {
      switch (pc2->orig_prev_sp)
      {
         case 0:  log_argval_and_return(AV_REMOVE); break;
         case 1:  log_argval_and_return(AV_FORCE ); break;
         default: log_argval_and_return(AV_ADD   ); break;
      }
   }

   /* "for (;;)" vs "for (;; )" and "for (a;b;c)" vs "for (a; b; c)" */
   if (is_type(pc1, CT_SEMICOLON))
   {
      if (is_ptype(pc1, CT_FOR))
      {
         if ((is_not_option(cpd.settings[UO_sp_after_semi_for_empty].a, AV_IGNORE)) &&
             (is_type(pc2, CT_SPAREN_CLOSE)))
         {
            log_option_and_return(UO_sp_after_semi_for_empty);
         }
         if (is_not_option(cpd.settings[UO_sp_after_semi_for].a, AV_IGNORE))
         {
            log_option_and_return(UO_sp_after_semi_for);
         }
      }
      else if (!chunk_is_comment(pc2) &&
               pc2->type != CT_BRACE_CLOSE) { log_option_and_return(UO_sp_after_semi); }
      /* Let the comment spacing rules handle this */
   }

   /* puts a space in the rare '+-' or '-+' */
   if (is_type(pc1, 3, CT_NEG, CT_POS, CT_ARITH) &&
       is_type(pc2, 3, CT_NEG, CT_POS, CT_ARITH) ) { log_argval_and_return(AV_ADD); }

   /* "return(a);" vs "return (foo_t)a + 3;" vs "return a;" vs "return;" */
   if (is_type(pc1, CT_RETURN))
   {
      if (is_type_and_ptype(pc2, CT_PAREN_OPEN, CT_RETURN)) { log_option_and_return(UO_sp_return_paren); }
      else                                                  { log_argval_and_return(AV_FORCE); }      /* everything else requires a space */
   }

   /* "sizeof(foo_t)" vs "sizeof foo_t" */
   if (is_type(pc1, CT_SIZEOF))
   {
      if (is_type(pc2, CT_PAREN_OPEN)) { log_option_and_return(UO_sp_sizeof_paren); }
      else                             { log_argval_and_return(AV_FORCE); }
   }

   /* handle '::' */
   if (is_type(pc1, CT_DC_MEMBER)) { log_option_and_return(UO_sp_after_dc); }

   // mapped_file_source abc((int) ::CW2A(sTemp));
   if (are_types(pc1, CT_PAREN_CLOSE, pc2, CT_DC_MEMBER) &&
       is_type(pc2->next, CT_FUNC_CALL) ) { log_argval_and_return(AV_REMOVE); }
   if (is_type(pc2, CT_DC_MEMBER))
   {
      /* '::' at the start of an identifier is not member access, but global scope operator.
       * Detect if previous chunk is keyword
       */
      switch (pc1->type)
      {
         case CT_SBOOL:     /* fallthrough */
         case CT_SASSIGN:   /* fallthrough */
         case CT_ARITH:     /* fallthrough */
         case CT_CASE:      /* fallthrough */
         case CT_CLASS:     /* fallthrough */
         case CT_DELETE:    /* fallthrough */
         case CT_FRIEND:    /* fallthrough */
         case CT_NAMESPACE: /* fallthrough */
         case CT_NEW:       /* fallthrough */
         case CT_SARITH:    /* fallthrough */
         case CT_SCOMPARE:  /* fallthrough */
         case CT_OPERATOR:  /* fallthrough */
         case CT_PRIVATE:   /* fallthrough */
         case CT_QUALIFIER: /* fallthrough */
         case CT_RETURN:    /* fallthrough */
         case CT_SIZEOF:    /* fallthrough */
         case CT_STRUCT:    /* fallthrough */
         case CT_THROW:     /* fallthrough */
         case CT_TYPEDEF:   /* fallthrough */
         case CT_TYPENAME:  /* fallthrough */
         case CT_UNION:     /* fallthrough */
         case CT_USING:     log_argval_and_return(AV_FORCE); break;
         default:           break;
      }

      if (is_type(pc1, 3, CT_WORD, CT_TYPE, CT_PAREN_CLOSE) ||
          CharTable::IsKW1((size_t)(pc1->str[0])))
                                     { log_option_and_return(UO_sp_before_dc); }
   }

   /* "a,b" vs "a, b" */
   if (is_type(pc1, CT_COMMA))
   {  if (is_ptype(pc1, CT_TYPE))
      {  /* C# multidimensional array type: ',,' vs ', ,' or ',]' vs ', ]' */
         if (is_type(pc2, CT_COMMA)) { log_option_and_return(UO_sp_between_mdatype_commas); }
         else                        { log_option_and_return(UO_sp_after_mdatype_commas); } }
      else                           { log_option_and_return(UO_sp_after_comma); }
   }
   // test if we are within a SIGNAL/SLOT call
   if (QT_SIGNAL_SLOT_found)
   {
      if (is_type(pc1,    CT_FPAREN_CLOSE          ) &&
          is_type(pc2, 2, CT_FPAREN_CLOSE, CT_COMMA) )
      {
         if (pc2->level == (size_t)(QT_SIGNAL_SLOT_level))
         {
            restoreValues = true;
         }
      }
   }
   if (is_type(pc2, CT_COMMA))
   {  if (is_type_and_ptype(pc1, CT_SQUARE_OPEN, CT_TYPE))
                                         { log_option_and_return(UO_sp_before_mdatype_commas); }
      if (is_type(pc1, CT_PAREN_OPEN) &&
          (cpd.settings[UO_sp_paren_comma].a != AV_IGNORE))
                                         { log_option_and_return(UO_sp_paren_comma); }
                                           log_option_and_return(UO_sp_before_comma); }
   if (is_type(pc2, CT_ELLIPSIS))
   {  /* non-punc followed by a ellipsis */
      if (((pc1->flags & PCF_PUNCTUATOR) == 0) &&
          (is_not_option(cpd.settings[UO_sp_before_ellipsis].a, AV_IGNORE)))
                                         { log_option_and_return(UO_sp_before_ellipsis); }
      if (is_type(pc1, CT_TAG_COLON))    { log_argval_and_return(AV_FORCE); } }
   if (is_type(pc1, CT_ELLIPSIS) &&
       CharTable::IsKW1((size_t) (pc2->str[0])))
                                         { log_argval_and_return(AV_FORCE); }
   if (is_type(pc1, CT_TAG_COLON))       { log_option_and_return(UO_sp_after_tag); }
   if (is_type(pc2, CT_TAG_COLON ))      { log_argval_and_return(AV_REMOVE); }
   if (is_type(pc1, CT_DESTRUCTOR))      { log_argval_and_return(AV_REMOVE); }   /* handle '~' */

   /* "((" vs "( (" or "))" vs ") )" */
   if ((is_str(pc1, "(", 1) && is_str(pc2, "(", 1)) ||
       (is_str(pc1, ")", 1) && is_str(pc2, ")", 1)) )
                                         { log_option_and_return(UO_sp_paren_paren); }
   if (are_types(pc1, CT_CATCH, pc2, CT_SPAREN_OPEN) &&
       (is_not_option(cpd.settings[UO_sp_catch_paren].a, AV_IGNORE)))
                                         { log_option_and_return(UO_sp_catch_paren); }
   if (are_types(pc1, CT_D_VERSION_IF, pc2, CT_SPAREN_OPEN) &&
       (is_not_option(cpd.settings[UO_sp_version_paren].a, AV_IGNORE)))
                                         { log_option_and_return(UO_sp_version_paren); }
   if (are_types(pc1,  CT_D_SCOPE_IF, pc2, CT_SPAREN_OPEN) &&
       (is_not_option(cpd.settings[UO_sp_scope_paren].a, AV_IGNORE)))
                                         { log_option_and_return(UO_sp_scope_paren); }
   /* "if (" vs "if(" */
   if (is_type    (pc2, CT_SPAREN_OPEN)) { log_option_and_return(UO_sp_before_sparen); }
   if (any_is_type(pc1, pc2, CT_LAMBDA)) { log_option_and_return(UO_sp_assign); }

   // Handle the special lambda case for C++11:
   //    [=](Something arg){.....}
   if ((cpd.settings[UO_sp_cpp_lambda_assign].a != AV_IGNORE) &&
       ((is_type_and_ptype(pc1, CT_SQUARE_OPEN,  CT_CPP_LAMBDA) && is_type(pc2, CT_ASSIGN) ) ||
        (is_type_and_ptype(pc2, CT_SQUARE_CLOSE, CT_CPP_LAMBDA) && is_type(pc1,  CT_ASSIGN) ) ) )
                                        { log_option_and_return(UO_sp_cpp_lambda_assign); }

   // Handle the special lambda case for C++11:
   //    [](Something arg){.....}
   if ((cpd.settings[UO_sp_cpp_lambda_paren].a != AV_IGNORE ) &&
       is_type_and_ptype(pc1, CT_SQUARE_CLOSE, CT_CPP_LAMBDA) &&
       is_type          (pc2, CT_FPAREN_OPEN                ) )
                                        { log_option_and_return(UO_sp_cpp_lambda_paren); }
   if (are_types(pc1, CT_ENUM, pc2, CT_FPAREN_OPEN))
   {  if (is_not_option(cpd.settings[UO_sp_enum_paren].a, AV_IGNORE))
                                        { log_option_and_return(UO_sp_enum_paren); } }
   if (is_type(pc2, CT_ASSIGN))
   {  if (pc2->flags & PCF_IN_ENUM)
      {  if (is_not_option(cpd.settings[UO_sp_enum_before_assign].a, AV_IGNORE))
                                        { log_option_and_return(UO_sp_enum_before_assign); }
         else                           { log_option_and_return(UO_sp_enum_assign); } }
      if ((is_not_option(cpd.settings[UO_sp_assign_default].a, AV_IGNORE)) &&
          is_ptype(pc2, CT_FUNC_PROTO)) { log_option_and_return(UO_sp_assign_default); }
      if (is_not_option(cpd.settings[UO_sp_before_assign].a, AV_IGNORE))
                                        { log_option_and_return(UO_sp_before_assign); }
      else                              { log_option_and_return(UO_sp_assign); } }
   if (is_type(pc1, CT_ASSIGN))
   {  if (pc1->flags & PCF_IN_ENUM)
      {  if (is_not_option(cpd.settings[UO_sp_enum_after_assign].a, AV_IGNORE))
                                        { log_option_and_return(UO_sp_enum_after_assign); }
         else                           { log_option_and_return(UO_sp_enum_assign); } }
      if ((is_not_option(cpd.settings[UO_sp_assign_default].a, AV_IGNORE)) &&
          is_ptype(pc1, CT_FUNC_PROTO)) { log_option_and_return(UO_sp_assign_default); }
      if (is_not_option(cpd.settings[UO_sp_after_assign].a, AV_IGNORE))
                                        { log_option_and_return(UO_sp_after_assign); }
      else                              { log_option_and_return(UO_sp_assign); } }
   if (is_type(pc1, CT_BIT_COLON) &&
       (pc1->flags & PCF_IN_ENUM))      { log_option_and_return(UO_sp_enum_colon); }
   if (is_type(pc2, CT_BIT_COLON) &&
       (pc2->flags & PCF_IN_ENUM))      { log_option_and_return(UO_sp_enum_colon); }
   if (is_type(pc2, CT_OC_BLOCK_CARET)) { log_option_and_return(UO_sp_before_oc_block_caret); }
   if (is_type(pc1, CT_OC_BLOCK_CARET)) { log_option_and_return(UO_sp_after_oc_block_caret); }
   if (is_type(pc2, CT_OC_MSG_FUNC))    { log_option_and_return(UO_sp_after_oc_msg_receiver); }
   if (is_type(pc2, CT_SQUARE_OPEN) &&  /* "a [x]" vs "a[x]" */
       (pc2->ptype != CT_OC_MSG   ) )   { log_option_and_return(UO_sp_before_square); }
   /* "byte[]" vs "byte []" */
   if (is_type(pc2, CT_TSQUARE))        { log_option_and_return(UO_sp_before_squares); }
   if ((cpd.settings[UO_sp_angle_shift].a != AV_IGNORE) &&
         are_types(pc1, pc2, CT_ANGLE_CLOSE))
                                        { log_option_and_return(UO_sp_angle_shift); }
   if (any_is_type(pc1, CT_ANGLE_OPEN,  /* spacing around template < > stuff */
                   pc2, CT_ANGLE_CLOSE)){ log_option_and_return(UO_sp_inside_angle); }
   if (is_type(pc2, CT_ANGLE_OPEN))
   {  if (is_type(pc1, CT_TEMPLATE) &&
          (is_not_option(cpd.settings[UO_sp_template_angle].a, AV_IGNORE)))
                                        { log_option_and_return(UO_sp_template_angle); }
      else                              { log_option_and_return(UO_sp_before_angle); } }
   if (is_type(pc1, CT_ANGLE_CLOSE))
   {  if (is_type(pc2, CT_WORD) ||
          CharTable::IsKW1((size_t)(pc2->str[0])))
      {
         if (is_not_option(cpd.settings[UO_sp_angle_word].a, AV_IGNORE))
                                        { log_option_and_return(UO_sp_angle_word); } }
      if (is_type(pc2, 2, CT_FPAREN_OPEN, CT_PAREN_OPEN ) )
      {  chunk_t *next = chunk_get_next_ncnl(pc2);
         if (is_type(next, CT_FPAREN_CLOSE))
                                        { log_option_and_return(UO_sp_angle_paren_empty); }
         else                           { log_option_and_return(UO_sp_angle_paren); }
      }
      if (is_type(pc2, CT_DC_MEMBER)) { log_option_and_return(UO_sp_before_dc); }
      if (is_not_type(pc2, 2, CT_BYREF, CT_PTR_TYPE)) { log_option_and_return(UO_sp_after_angle); }
   }

   if (is_type(pc1, CT_BYREF) &&
       is_not_option(cpd.settings[UO_sp_after_byref_func].a, AV_IGNORE) &&
       is_ptype(pc1, 2, CT_FUNC_DEF, CT_FUNC_PROTO))
                                          { log_option_and_return(UO_sp_after_byref_func); }
   if (is_type(pc1, CT_BYREF) && CharTable::IsKW1((size_t)(pc2->str[0])) )
                                          { log_option_and_return(UO_sp_after_byref); }

   if (is_type(pc2, CT_BYREF))
   {
      if (is_not_option(cpd.settings[UO_sp_before_byref_func].a, AV_IGNORE))
      {
         chunk_t *next = chunk_get_next(pc2);
         if (is_type(next, 2, CT_FUNC_DEF, CT_FUNC_PROTO))
         {
            return(cpd.settings[UO_sp_before_byref_func].a);
         }
      }

      if (is_not_option(cpd.settings[UO_sp_before_unnamed_byref].a, AV_IGNORE))
      {
         chunk_t *next = chunk_get_next_nc(pc2);
         if (is_not_type(next, CT_WORD)) { log_option_and_return(UO_sp_before_unnamed_byref); }
      }
      log_option_and_return(UO_sp_before_byref);
   }

   if (is_type(pc1, CT_SPAREN_CLOSE))
   {
      if (is_type(pc2, CT_BRACE_OPEN) &&
          (cpd.settings[UO_sp_sparen_brace].a != AV_IGNORE))
      {
         log_option_and_return(UO_sp_sparen_brace);
      }
      if (!chunk_is_comment(pc2) &&
          (cpd.settings[UO_sp_after_sparen].a != AV_IGNORE))
      {
         log_option_and_return(UO_sp_after_sparen);
      }
   }

   if (is_type (pc2, CT_FPAREN_OPEN) &&
       is_ptype(pc1, CT_OPERATOR   ) &&
       (is_not_option(cpd.settings[UO_sp_after_operator_sym].a, AV_IGNORE)))
   {
      // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_after_operator_sym_empty].a, AV_IGNORE)) &&
          (is_type(pc2, CT_FPAREN_OPEN)))
      {
         const chunk_t *next = chunk_get_next_ncnl(pc2);
         if (is_type(next, CT_FPAREN_CLOSE)) { log_option_and_return(UO_sp_after_operator_sym_empty); }
      }
      else { log_option_and_return(UO_sp_after_operator_sym); } // DRY1 end
   }

   /* spaces between function and open paren */
   if (is_type(pc1, 2, CT_FUNC_CALL, CT_FUNC_CTOR_VAR))
   {
      // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_func_call_paren_empty].a, AV_IGNORE)) &&
          (is_type(pc2, CT_FPAREN_OPEN)))
      {
         chunk_t *next = chunk_get_next_ncnl(pc2);
         if (is_type(next, CT_FPAREN_CLOSE) ) { log_option_and_return(UO_sp_func_call_paren_empty); }
      }
      else { log_option_and_return(UO_sp_func_call_paren); } // DRY1 end
   }
   if (is_type(pc1, CT_FUNC_CALL_USER)) { log_option_and_return(UO_sp_func_call_user_paren); }
   if (is_type(pc1, CT_ATTRIBUTE     )) { log_option_and_return(UO_sp_attribute_paren); }
   if (is_type(pc1, CT_FUNC_DEF))
   {  // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_func_def_paren_empty].a, AV_IGNORE)) &&
          (is_type(pc2, CT_FPAREN_OPEN)))
      {
         const chunk_t *next = chunk_get_next_ncnl(pc2);
         if (is_type(next, CT_FPAREN_CLOSE)) { log_option_and_return(UO_sp_func_def_paren_empty); }
      }
      log_rule("sp_func_def_paren");
      return(cpd.settings[UO_sp_func_def_paren].a);
      // DRY1 end
   }
   if (is_type(pc1, 2, CT_CPP_CAST,
                       CT_TYPE_WRAP))  { log_option_and_return(UO_sp_cpp_cast_paren); }

   if (are_types(pc1, CT_PAREN_CLOSE,
                 pc2, CT_WHEN))        { log_argval_and_return(AV_FORCE); }/* TODO: make this configurable? */

   if (is_type(pc1,    CT_PAREN_CLOSE               ) &&
       is_type(pc2, 2, CT_PAREN_OPEN, CT_FPAREN_OPEN) )
   {
      /* "(int)a" vs "(int) a" or "cast(int)a" vs "cast(int) a" */
      if (is_ptype(pc1, 2, CT_C_CAST, CT_D_CAST))
                                       { log_option_and_return(UO_sp_after_cast); }

      /* Must be an indirect/chained function call? */
      log_argval_and_return(AV_REMOVE); /* TODO: make this configurable? */
   }

   /* handle the space between parens in fcn type 'void (*f)(void)' */
   if (is_type(pc1, CT_TPAREN_CLOSE)) { log_option_and_return(UO_sp_after_tparen_close); }

   /* ")(" vs ") (" */
   if ((is_str(pc1, ")", 1) && is_str(pc2, "(", 1)) ||
       (chunk_is_paren_close(pc1) && chunk_is_paren_open(pc2)))
                                      { log_option_and_return(UO_sp_cparen_oparen); }

   if (is_type          (pc1,                 CT_FUNC_PROTO ) ||
       is_type_and_ptype(pc2, CT_FPAREN_OPEN, CT_FUNC_PROTO ) )
   {  // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_func_proto_paren_empty].a, AV_IGNORE)) &&
          (is_type(pc2, CT_FPAREN_OPEN)))
      {
         const chunk_t *next = chunk_get_next_ncnl(pc2);
         if (is_type(next, CT_FPAREN_CLOSE)) { log_option_and_return(UO_sp_func_proto_paren_empty); }}
      else                                   { log_option_and_return(UO_sp_func_proto_paren      ); }
   }
   if (is_type(pc1, 2, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO))
   {  // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_func_class_paren_empty].a, AV_IGNORE)) &&
          is_type(pc2, CT_FPAREN_OPEN))
      {  const chunk_t *next = chunk_get_next_ncnl(pc2);
         if (is_type(next, CT_FPAREN_CLOSE)) { log_option_and_return(UO_sp_func_class_paren_empty); }}
      else                                   { log_option_and_return(UO_sp_func_class_paren      ); }}
   if (  is_type(pc1, CT_CLASS) &&
        !(pc1->flags & PCF_IN_OC_MSG))       { log_argval_and_return(AV_FORCE                    ); }
   if (are_types(pc1, CT_BRACE_OPEN, pc2, CT_BRACE_CLOSE))
                                             { log_option_and_return(UO_sp_inside_braces_empty   ); }
   if (is_type(pc2, CT_BRACE_CLOSE))
   {  if (pc2->ptype == CT_ENUM)             { log_option_and_return(UO_sp_inside_braces_enum    ); }
      if (is_ptype(pc2, 2, CT_STRUCT, CT_UNION))
                                             { log_option_and_return(UO_sp_inside_braces_struct  ); }
      else                                   { log_option_and_return(UO_sp_inside_braces         ); }}
   if (is_type  (pc1, CT_D_CAST))            { log_argval_and_return(AV_REMOVE                   ); }
   if (are_types(pc1, CT_PP_DEFINED,
                 pc2, CT_PAREN_OPEN))        { log_option_and_return(UO_sp_defined_paren         ); }
   if (is_type(pc1, CT_THROW))
   {  if (is_type(pc2, CT_PAREN_OPEN))       { log_option_and_return(UO_sp_throw_paren           ); }
      else                                   { log_option_and_return(UO_sp_after_throw           ); }
   }
   if (are_types(pc1, CT_THIS,
                 pc2, CT_PAREN_OPEN) )       { log_option_and_return(UO_sp_this_paren            ); }
   if (are_types(pc1, CT_STATE,
                 pc2, CT_PAREN_OPEN))        { log_argval_and_return(AV_ADD                      ); }
   if (are_types(pc1, CT_DELEGATE,
                 pc2, CT_PAREN_OPEN))        { log_argval_and_return(AV_REMOVE                   ); }
   if (any_is_type(pc1, CT_MEMBER, pc2, CT_MEMBER))
                                             { log_option_and_return(UO_sp_member                ); }

   // always remove space(s) after then '.' of a C99-member
   if    (is_type(pc1, CT_C99_MEMBER))       { log_argval_and_return(AV_REMOVE                   ); }
   if    (are_types(pc1, CT_SUPER, pc2, CT_PAREN_OPEN))
                                             { log_option_and_return(UO_sp_super_paren           ); }
   if    (are_types(pc1, CT_FPAREN_CLOSE, pc2, CT_BRACE_OPEN))
   {
      if (pc2->ptype == CT_DOUBLE_BRACE)     { log_option_and_return(UO_sp_fparen_dbrace         ); }
      log_rule("sp_fparen_brace");
      return(cpd.settings[UO_sp_fparen_brace].a); }
   if    (any_is_type(pc1, pc2, CT_D_TEMPLATE))
                                             { log_argval_and_return(AV_REMOVE                   ); }
   if    (are_types  (pc1, CT_ELSE, pc2, CT_BRACE_OPEN))
                                             { log_option_and_return(UO_sp_else_brace            ); }
   if    (are_types  (pc1, CT_ELSE, pc2, CT_ELSEIF))
                                             { log_argval_and_return(AV_FORCE                    ); }
   if    (are_types  (pc1, CT_CATCH, pc2, CT_BRACE_OPEN))
                                             { log_option_and_return(UO_sp_catch_brace           ); }
   if    (are_types  (pc1, CT_FINALLY, pc2, CT_BRACE_OPEN))
                                             { log_option_and_return(UO_sp_finally_brace         ); }
   if    (are_types  (pc1, CT_TRY, pc2, CT_BRACE_OPEN))
                                             { log_option_and_return(UO_sp_try_brace             ); }
   if    (are_types  (pc1, CT_GETSET, pc2, CT_BRACE_OPEN))
                                             { log_option_and_return(UO_sp_getset_brace          ); }
   if    (are_types (pc1, CT_WORD, pc2, CT_BRACE_OPEN)) {
      if (is_ptype  (pc1, CT_NAMESPACE))     { log_option_and_return(UO_sp_word_brace_ns         ); }
      if (are_ptypes(pc1, pc2, CT_NONE))     { log_option_and_return(UO_sp_word_brace            ); }}
   if    (is_type_and_ptype(pc2, CT_PAREN_OPEN, CT_INVARIANT))
                                             { log_option_and_return(UO_sp_invariant_paren       ); }
   if    (is_type (pc1, CT_PAREN_CLOSE))
   {  if (is_ptype(pc1, CT_D_TEMPLATE))      { log_argval_and_return(AV_FORCE                    ); }
      if (is_ptype(pc1, CT_INVARIANT ))      { log_option_and_return(UO_sp_after_invariant_paren ); }
      if (is_type (pc2, 2, CT_ARITH, CT_CARET)) /* Arith after a cast comes first */
                                             { log_option_and_return(UO_sp_arith                 ); }
      /* "(struct foo) {...}" vs "(struct foo){...}" */
      if (is_type (pc2, CT_BRACE_OPEN))       { log_option_and_return(UO_sp_paren_brace           ); }
      /* D-specific: "delegate(some thing) dg */
      if (is_ptype(pc1, CT_DELEGATE))        { log_argval_and_return(AV_ADD                      ); }
      /* PAWN-specific: "state (condition) next" */
      if (is_ptype(pc1, CT_STATE))           { log_argval_and_return(AV_ADD                      ); }}
   if    (any_is_type(pc1, CT_FPAREN_OPEN, pc2, CT_FPAREN_CLOSE)) { /* "foo(...)" vs "foo( ... )" */
      if (are_types(pc1, CT_FPAREN_OPEN, pc2, CT_FPAREN_CLOSE))
                                             { log_option_and_return(UO_sp_inside_fparens        ); }
      else                                   { log_option_and_return(UO_sp_inside_fparen         ); }}
   /* "foo(...)" vs "foo( ... )" */
   if (any_is_type(pc1, CT_TPAREN_OPEN, pc2, CT_TPAREN_CLOSE))
                                             { log_option_and_return(UO_sp_inside_tparen); }
   if (is_type(pc1, CT_PAREN_CLOSE)) {
      if ((pc1->flags & PCF_OC_RTYPE) /*== CT_OC_RTYPE)*/ &&
          is_ptype(pc1, 2, CT_OC_MSG_DECL, CT_OC_MSG_SPEC))
                                             { log_option_and_return(UO_sp_after_oc_return_type); }
      else if (is_ptype(pc1, 2, CT_OC_MSG_SPEC, CT_OC_MSG_DECL))
                                             { log_option_and_return(UO_sp_after_oc_type); }
      else if (is_ptype   (pc1, CT_OC_SEL) &&
               is_not_type(pc2, CT_SQUARE_CLOSE))
                                             { log_option_and_return(UO_sp_after_oc_at_sel_parens); } }
   if (cpd.settings[UO_sp_inside_oc_at_sel_parens].a != AV_IGNORE) {
      if ((is_type(pc1, CT_PAREN_OPEN ) && is_ptype(pc1, 2, CT_OC_SEL, CT_OC_PROTOCOL)) ||
          (is_type(pc2, CT_PAREN_CLOSE) && is_ptype(pc2, 2, CT_OC_SEL, CT_OC_PROTOCOL)) )
                                             { log_option_and_return(UO_sp_inside_oc_at_sel_parens); } }
   if (is_type(pc2, CT_PAREN_OPEN) &&
       is_type(pc1, 2, CT_OC_SEL, CT_OC_PROTOCOL))
                                             { log_option_and_return(UO_sp_after_oc_at_sel); }
   /* C cast:   "(int)"      vs "( int )"
    * D cast:   "cast(int)"  vs "cast( int )"
    * CPP cast: "int(a + 3)" vs "int( a + 3 )" */
   if (is_type(pc1, CT_PAREN_OPEN))
   {
      if (is_ptype(pc1, 3, CT_C_CAST, CT_CPP_CAST, CT_D_CAST))
                                              { log_option_and_return(UO_sp_inside_paren_cast); }
      else                                    { log_option_and_return(UO_sp_inside_paren); } }
   if (is_type(pc2, CT_PAREN_CLOSE))
   {  if (is_ptype(pc2, 3, CT_C_CAST, CT_CPP_CAST, CT_D_CAST))
                                              { log_option_and_return(UO_sp_inside_paren_cast); }
      else                                    { log_option_and_return(UO_sp_inside_paren); } }
   /* "[3]" vs "[ 3 ]" */
   if (any_is_type(pc1, CT_SQUARE_OPEN, pc2, CT_SQUARE_CLOSE))
                                              { log_option_and_return(UO_sp_inside_square); }
   if (are_types(pc1, CT_SQUARE_CLOSE, pc2, CT_FPAREN_OPEN ))
                                              { log_option_and_return(UO_sp_square_fparen); }
   /* "if(...)" vs "if( ... )" */
   if (is_type(pc2, CT_SPAREN_CLOSE) &&
       (is_not_option(cpd.settings[UO_sp_inside_sparen_close].a, AV_IGNORE)))
                                              { log_option_and_return(UO_sp_inside_sparen_close); }
   if (is_type(pc1, CT_SPAREN_OPEN) &&
       (is_not_option(cpd.settings[UO_sp_inside_sparen_open].a, AV_IGNORE)))
                                              { log_option_and_return(UO_sp_inside_sparen_open); }
   if (any_is_type(pc1, CT_SPAREN_OPEN, pc2, CT_SPAREN_CLOSE))
                                              { log_option_and_return(UO_sp_inside_sparen); }
   if ((is_not_option(cpd.settings[UO_sp_after_class_colon].a, AV_IGNORE)) &&
       (is_type(pc1, CT_CLASS_COLON)))
                                              { log_option_and_return(UO_sp_after_class_colon); }
   if ((is_not_option(cpd.settings[UO_sp_before_class_colon].a, AV_IGNORE)) &&
       (is_type(pc2, CT_CLASS_COLON)))
                                              { log_option_and_return(UO_sp_before_class_colon); }
   if ((is_not_option(cpd.settings[UO_sp_after_constr_colon].a, AV_IGNORE)) &&
       (is_type(pc1, CT_CONSTR_COLON)))
   {  min_sp = cpd.settings[UO_indent_ctor_init_leading].n - 1; // default indent is 1 space
      log_option_and_return(UO_sp_after_constr_colon); }
   if ((is_not_option(cpd.settings[UO_sp_before_constr_colon].a, AV_IGNORE)) &&
       (is_type(pc2, CT_CONSTR_COLON)))
                                              { log_option_and_return(UO_sp_before_constr_colon); }
   if ((is_not_option(cpd.settings[UO_sp_before_case_colon].a, AV_IGNORE)) &&
       (is_type(pc2, CT_CASE_COLON)))
                                              { log_option_and_return(UO_sp_before_case_colon); }

   if (is_type(pc1, CT_DOT))                  { log_argval_and_return(AV_REMOVE); }
   if (is_type(pc2, CT_DOT))                  { log_argval_and_return(AV_ADD); }
   if (any_is_type(pc1, pc2, CT_NULLCOND))    { log_option_and_return(UO_sp_member); }
   if (is_type(pc1, 2, CT_ARITH, CT_CARET) ||
       is_type(pc2, 2, CT_ARITH, CT_CARET) )  { log_option_and_return(UO_sp_arith); }
   if (any_is_type(pc1, pc2, CT_BOOL))
   {  argval_t arg = cpd.settings[UO_sp_bool].a;
      if (is_not_token(cpd.settings[UO_pos_bool].tp, TP_IGNORE) &&
          (pc1->orig_line != pc2->orig_line) &&
          (arg != AV_REMOVE))
      {
         arg = static_cast<argval_t>(arg | AV_ADD);
      }
      log_rule("sp_bool");
      return(arg); }
   if (any_is_type(pc1, pc2, CT_COMPARE))     { log_option_and_return(UO_sp_compare); }
   if (are_types(pc1, CT_PAREN_OPEN, pc2, CT_PTR_TYPE))
                                              { log_argval_and_return(AV_REMOVE); }
   if (is_type(pc1, CT_PTR_TYPE) &&
       (cpd.settings[UO_sp_ptr_star_paren].a != AV_IGNORE) &&
       (is_type(pc2, 2, CT_FPAREN_OPEN, CT_TPAREN_OPEN)))
                                              { log_option_and_return(UO_sp_ptr_star_paren); }
   if (are_types(pc1, pc2, CT_PTR_TYPE) &&
       (is_not_option(cpd.settings[UO_sp_between_ptr_star].a, AV_IGNORE)))
                                              { log_option_and_return(UO_sp_between_ptr_star); }
   if ( is_type (pc1, CT_PTR_TYPE) &&
       (is_not_option(cpd.settings[UO_sp_after_ptr_star_func].a, AV_IGNORE)) &&
       (is_ptype(pc1, 3, CT_FUNC_DEF, CT_FUNC_PROTO, CT_FUNC_VAR)))
                                              { log_option_and_return(UO_sp_after_ptr_star_func); }
   if (is_type(pc1, CT_PTR_TYPE                ) &&
       (CharTable::IsKW1((size_t)(pc2->str[0]))) ){
      chunk_t *prev = chunk_get_prev(pc1);
      if (is_type(prev, CT_IN))               { log_option_and_return(UO_sp_deref); }
      if (is_type(pc2, CT_QUALIFIER) &&
          (is_not_option(cpd.settings[UO_sp_after_ptr_star_qualifier].a, AV_IGNORE)))
                                              { log_option_and_return(UO_sp_after_ptr_star_qualifier); }
      else if (is_not_option(cpd.settings[UO_sp_after_ptr_star].a, AV_IGNORE))
                                              { log_option_and_return(UO_sp_after_ptr_star); }}
   if (is_only_first_type(pc2, CT_PTR_TYPE, pc1, CT_IN))
   {
      if (is_not_option(cpd.settings[UO_sp_before_ptr_star_func].a, AV_IGNORE))
      {
         /* Find the next non-'*' chunk */
         chunk_t *next = pc2;
         do
         {
            next = chunk_get_next(next);
         } while (is_type(next, CT_PTR_TYPE) );

         if (is_type(next, 2, CT_FUNC_DEF, CT_FUNC_PROTO) )
         {
            return(cpd.settings[UO_sp_before_ptr_star_func].a);
         }
      }

      if (is_not_option(cpd.settings[UO_sp_before_unnamed_ptr_star].a, AV_IGNORE))
      {
         chunk_t *next = chunk_get_next_nc(pc2);
         while (is_type(next, CT_PTR_TYPE) )
         {
            next = chunk_get_next_nc(next);
         }

         if (is_not_type(next, CT_WORD)) { log_option_and_return(UO_sp_before_unnamed_ptr_star); }}
      if (is_not_option(cpd.settings[UO_sp_before_ptr_star].a, AV_IGNORE))
                                         { log_option_and_return(UO_sp_before_ptr_star     ); }}
   if (is_type(pc1, CT_OPERATOR))        { log_option_and_return(UO_sp_after_operator      ); }
   if (is_type(pc2, 2, CT_FUNC_PROTO, CT_FUNC_DEF)) {
      if (is_not_type(pc1, CT_PTR_TYPE)) {
         log_rule("sp_type_func|ADD");
         return(add_option(cpd.settings[UO_sp_type_func].a, AV_ADD)); }
      log_option_and_return(UO_sp_type_func);}
   /* "(int)a" vs "(int) a" or "cast(int)a" vs "cast(int) a" */
   if (is_ptype(pc1, 2, CT_C_CAST, CT_D_CAST))
                                         { log_option_and_return(UO_sp_after_cast          ); }
   if (is_type(pc1, CT_BRACE_CLOSE))     {
      if      (is_type(pc2, CT_ELSE   )) { log_option_and_return(UO_sp_brace_else          ); }
      else if (is_type(pc2, CT_CATCH  )) { log_option_and_return(UO_sp_brace_catch         ); }
      else if (is_type(pc2, CT_FINALLY)) { log_option_and_return(UO_sp_brace_finally       ); } }
   if (is_type(pc1, CT_BRACE_OPEN)) {
      if (is_ptype(pc1, CT_ENUM))        { log_option_and_return(UO_sp_inside_braces_enum  ); }
      else if (is_ptype(pc1, 2, CT_UNION, CT_STRUCT))
                                         { log_option_and_return(UO_sp_inside_braces_struct); }
      else if (!chunk_is_comment(pc2))   { log_option_and_return(UO_sp_inside_braces       ); } }
   if (is_type(pc2, CT_BRACE_CLOSE)) {
      if (is_ptype(pc2, CT_ENUM))        { log_option_and_return(UO_sp_inside_braces_enum  ); }
      else if (is_ptype(pc2, 2, CT_UNION, CT_STRUCT))
                                         { log_option_and_return(UO_sp_inside_braces_struct); }
      else                               { log_option_and_return(UO_sp_inside_braces       ); } }
   if ( is_type(pc1, CT_BRACE_CLOSE) &&
       (pc1->flags & PCF_IN_TYPEDEF) &&
       (is_ptype(pc1, 3, CT_ENUM, CT_STRUCT, CT_UNION)))
                                         { log_option_and_return(UO_sp_brace_typedef); }
   if (is_type(pc2, CT_SPAREN_OPEN))     { log_option_and_return(UO_sp_before_sparen); }
   if (is_type_and_ptype(pc2, CT_PAREN_OPEN, CT_TEMPLATE))
                                         { log_option_and_return(UO_sp_before_template_paren); }
   if (is_not_type(pc2, CT_PTR_TYPE) &&
       is_type(pc1, 2, CT_QUALIFIER, CT_TYPE))
   {
      argval_t arg = cpd.settings[UO_sp_after_type].a;
      log_rule("sp_after_type");
      return((arg != AV_REMOVE) ? arg : AV_FORCE);
   }

   if (is_type(pc1, 3, CT_MACRO_OPEN, CT_MACRO_CLOSE, CT_MACRO_ELSE)) {
      if (is_type(pc2, CT_PAREN_OPEN))     { log_option_and_return(UO_sp_func_call_paren); }
      else                                 { log_argval_and_return(AV_IGNORE   ); }}
   /* If nothing claimed the PTR_TYPE, then return ignore */
   if (any_is_type(pc1, pc2, CT_PTR_TYPE)) { log_argval_and_return(AV_IGNORE   ); }
   if (is_type(pc1, CT_NOT              )) { log_option_and_return(UO_sp_not   ); }
   if (is_type(pc1, CT_INV              )) { log_option_and_return(UO_sp_inv   ); }
   if (is_type(pc1, CT_ADDR             )) { log_option_and_return(UO_sp_addr  ); }
   if (is_type(pc1, CT_DEREF            )) { log_option_and_return(UO_sp_deref ); }
   if (is_type(pc1, 2, CT_POS, CT_NEG   )) { log_option_and_return(UO_sp_sign  ); }
   if (any_is_type(pc1, CT_INCDEC_BEFORE, pc2, CT_INCDEC_AFTER ))
                                           { log_option_and_return(UO_sp_incdec); }
   if (is_type(pc2, CT_CS_SQ_COLON      )) { log_argval_and_return(AV_REMOVE   ); }
   if (is_type(pc1, CT_CS_SQ_COLON      )) { log_argval_and_return(AV_FORCE    ); }
   if (is_type(pc1, CT_OC_SCOPE         )) { log_option_and_return(UO_sp_after_oc_scope); }
   if (is_type(pc1, CT_OC_DICT_COLON    )) { log_option_and_return(UO_sp_after_oc_dict_colon); }
   if (is_type(pc2, CT_OC_DICT_COLON    )) { log_option_and_return(UO_sp_before_oc_dict_colon); }
   if (is_type(pc1, CT_OC_COLON         )) {
      if (pc1->flags & PCF_IN_OC_MSG)      { log_option_and_return(UO_sp_after_send_oc_colon); }
      else { log_option_and_return(UO_sp_after_oc_colon); }}
   if (is_type(pc2, CT_OC_COLON)) {
      if ((pc1->flags & PCF_IN_OC_MSG) &&
          is_type(pc1, 2, CT_OC_MSG_FUNC, CT_OC_MSG_NAME) )
                                           { log_option_and_return(UO_sp_before_send_oc_colon); }
      else                                 { log_option_and_return(UO_sp_before_oc_colon); }}
   if (is_type_and_ptype(pc2, CT_COMMENT, CT_COMMENT_EMBED)) { log_argval_and_return(AV_FORCE); }
   if (chunk_is_comment(pc2)) { log_argval_and_return(AV_IGNORE); }
   if (is_type(pc1, CT_COMMENT)) { log_argval_and_return(AV_FORCE); }
   if (are_types(pc1, CT_NEW, pc2, CT_PAREN_OPEN)) { log_option_and_return(UO_sp_between_new_paren);; }
   if (is_type          (pc1, 2, CT_NEW,  CT_DELETE) ||
       is_type_and_ptype(pc1, CT_TSQUARE, CT_DELETE) ) { log_option_and_return(UO_sp_after_new); }
   if (is_type(pc1, CT_ANNOTATION) &&
       (chunk_is_paren_open(pc2) ) ) { log_option_and_return(UO_sp_annotation_paren); }
   if (is_type(pc1, CT_OC_PROPERTY)) { log_option_and_return(UO_sp_after_oc_property); }
   if (are_types(pc1, CT_EXTERN, pc2, CT_PAREN_OPEN)) { log_option_and_return(UO_sp_extern_paren); }

   // this table lists out all combos where a space should NOT be present
   // CT_UNKNOWN is a wildcard.
   for (auto it : no_space_table)
   {
      if ( ((it.first  == CT_UNKNOWN) || (it.first  == pc1->type ) ) &&
           ((it.second == CT_UNKNOWN) || (it.second == pc2->type) ) )
      {
         log_argval_and_return(AV_REMOVE);
      }
   }

   // mapped_file_source abc((int) A::CW2A(sTemp));
   if (are_types(pc1, CT_PAREN_CLOSE, pc2, CT_TYPE) &&
       are_types(pc2->next, CT_DC_MEMBER,
                 pc2->next->next, CT_FUNC_CALL)) { log_argval_and_return(AV_REMOVE); }

#ifdef DEBUG
   // these lines are only useful for debugging uncrustify itself
   LOG_FMT(LSPACE, "\n\n(%d) %s: WARNING: unrecognized do_space: first: %zu:%zu %s %s and second: %zu:%zu %s %s\n\n\n",
           __LINE__, __func__, pc1->orig_line, pc1->orig_col, pc1->text(), get_token_name(pc1->type),
           pc2->orig_line, pc2->orig_col, pc2->text(), get_token_name(pc2->type));
#endif
   log_argval_and_return(AV_ADD);
}


void space_text(void)
{
   LOG_FUNC_ENTRY();

   chunk_t *pc = chunk_get_head();
   if (is_invalid(pc)) { return; }

   chunk_t *next;
   size_t  prev_column;
   size_t  column = pc->column;
   while (is_valid(pc))
   {
      LOG_FMT(LSPACE, "%s: %zu:%zu %s %s\n",
              __func__, pc->orig_line, pc->orig_col, pc->text(), get_token_name(pc->type));
      if ((cpd.settings[UO_use_options_overriding_for_qt_macros].b) &&
          ((strcmp(pc->text(), "SIGNAL") == 0) ||
           (strcmp(pc->text(), "SLOT"  ) == 0) ) )
      {
         LOG_FMT(LSPACE, "%zu: [%d] type %s SIGNAL/SLOT found\n",
                 pc->orig_line, __LINE__, get_token_name(pc->type));
         // flag the chunk for a second processing
         chunk_flags_set(pc, PCF_IN_QT_MACRO);

         // save the values
         save_set_options_for_QT(pc->level);
      }
      if (cpd.settings[UO_sp_skip_vbrace_tokens].b)
      {
         next = chunk_get_next(pc);
         while ( ( chunk_is_empty  (next)) &&
                 (!chunk_is_newline(next)) &&
                 is_type(next, 2, CT_VBRACE_OPEN, CT_VBRACE_CLOSE))
         {
            assert(is_valid(next));
            LOG_FMT(LSPACE, "%s: %zu:%zu Skip %s (%zu+%zu)\n",
                    __func__, next->orig_line, next->orig_col, get_token_name(next->type),
                    pc->column, pc->str.size());
            next->column = pc->column + pc->str.size();
            next         = chunk_get_next(next);
         }
      }
      else { next = pc->next; }

      if (is_invalid(next)) { break; }

      if ((QT_SIGNAL_SLOT_found) &&
          (cpd.settings[UO_sp_balance_nested_parens].b))
      {
         if (is_type(next->next, CT_SPACE))
         {
            chunk_del(next->next); // remove the space
         }
      }

      /* If the current chunk contains a newline, do not change the column
       * of the next item */
      if (is_type(pc, 3, CT_NEWLINE, CT_NL_CONT, CT_COMMENT_MULTI))
      {
         column = next->column;
      }
      else
      {
         /* Set to the minimum allowed column */
         if (pc->nl_count == 0) { column += pc->len();        }
         else                   { column  = pc->orig_col_end; }
         prev_column = column;

         /* Apply a general safety check
          * If the two chunks combined will tokenize differently, then we
          * must force a space.
          * Two chunks -- "()" and "[]" will always tokenize differently.
          * They are always safe to not have a space after them. */
         chunk_flags_clr(pc, PCF_FORCE_SPACE);
         if ((pc->len() > 0) &&
             !is_str(pc, "[]", 2) &&
             !is_str(pc, "{{", 2) &&
             !is_str(pc, "}}", 2) &&
             !is_str(pc, "()", 2) &&
             !pc->str.startswith("@\""))
         {
            /* Find the next non-empty chunk on this line */
            chunk_t *tmp = next;
            while (is_valid(tmp) &&
                  (tmp->len() == 0) &&
                  !chunk_is_newline(tmp))
            {
               tmp = chunk_get_next(tmp);
            }
            if ((is_valid(tmp)) &&
                (tmp->len() > 0))
            {
               bool kw1 = CharTable::IsKW2((size_t)(pc->str[pc->len()-1]));
               bool kw2 = CharTable::IsKW1((size_t)(next->str[0]));
               if ((kw1 == true) &&
                   (kw2 == true) )
               {
                  /* back-to-back words need a space */
                  chunk_flags_set(pc, PCF_FORCE_SPACE);
               }
               else if ( (kw1 == false   ) &&
                         (kw2 == false   ) &&
                         (pc->len()   < 4) &&
                         (next->len() < 4) )
               {
                  /* We aren't dealing with keywords. concat and try punctuators */
                  char buf[9];
                  memcpy(buf,             pc->text(),   pc->len()  );
                  memcpy(buf + pc->len(), next->text(), next->len());
                  buf[pc->len() + next->len()] = 0;

                  const chunk_tag_t *ct = find_punctuator(buf, cpd.lang_flags);
                  if ( ptr_is_valid(ct)  &&
                      (strlen(ct->tag) != pc->len()) )
                  {
                     /* punctuator parsed to a different size.. */

                     /* C++11 allows '>>' to mean '> >' in templates:
                      *   some_func<vector<string>>();*/
                     if ((((cpd.lang_flags & LANG_CPP ) && cpd.settings[UO_sp_permit_cpp11_shift].b) ||
                          ((cpd.lang_flags & LANG_JAVA) || (cpd.lang_flags & LANG_CS))) &&
                          are_types(pc, next, CT_ANGLE_CLOSE) )
                     {
                        /* allow '>' and '>' to become '>>' */
                     }
                     else if (strcmp(ct->tag, "[]") == 0)
                     {
                        /* this is OK */
                     }
                     else
                     {
                        chunk_flags_set(pc, PCF_FORCE_SPACE);
                     }
                  }
               }
            }
         }

         int      min_sp;
         argval_t av = do_space(pc, next, min_sp, false);
         if (pc->flags & PCF_FORCE_SPACE)
         {
            LOG_FMT(LSPACE, " <force between '%s' and '%s'>", pc->text(), next->text());
            av = (argval_t)(av | AV_ADD); /*lint !e655 */
         }
         min_sp = max(1, min_sp);
         switch (av)
         {
         case AV_FORCE:
            /* add exactly the specified # of spaces */
            column = (size_t)((int)column + min_sp);
            break;

         case AV_ADD:
         {
            int delta = min_sp;
            if ((next->orig_col >= pc->orig_col_end) &&
                (pc->orig_col_end != 0))
            {
               /* Keep the same relative spacing, minimum 1 */
               delta = (int)next->orig_col - (int)pc->orig_col_end;
               delta = max(delta, min_sp);
            }
            column = (size_t)((int)column + delta);
            break;
         }

         case AV_REMOVE:  /* the symbols will be back-to-back "a+3" */
         case AV_NOT_DEFINED:
            break;

         case AV_IGNORE:
            /* Keep the same relative spacing, if possible */
            if ((next->orig_col >= pc->orig_col_end) && (pc->orig_col_end != 0))
            {
               column += next->orig_col - pc->orig_col_end;
            }
            break;
         }

         if (chunk_is_comment(next                ) &&
             chunk_is_newline(chunk_get_next(next)) &&
             (column < next->orig_col))
         {
            /* do some comment adjustments if sp_before_tr_emb_cmt and
             * sp_endif_cmt did not apply. */
            if (((is_option(cpd.settings[UO_sp_before_tr_emb_cmt].a, AV_IGNORE)) ||
                 ((next->ptype != CT_COMMENT_END  ) &&
                  (next->ptype != CT_COMMENT_EMBED) ) ) &&
                (is_option(cpd.settings[UO_sp_endif_cmt].a, AV_IGNORE) ||
                 is_not_type(pc, 2,CT_PP_ELSE, CT_PP_ENDIF     ) ) )
            {
               if (cpd.settings[UO_indent_relative_single_line_comments].b)
               {
                  /* Try to keep relative spacing between tokens */
                  LOG_FMT(LSPACE, " <relative adj>");
                  column = pc->column + 1 + (next->orig_col - pc->orig_col_end);
               }
               else
               {
                  /* If there was a space, we need to force one, otherwise
                   * try to keep the comment in the same column. */
                  size_t col_min = pc->column + pc->len() + ((next->orig_prev_sp > 0) ? 1 : 0);
                  column = next->orig_col;
                  column = max(column, col_min);
                  LOG_FMT(LSPACE, " <relative set>");
               }
            }
         }
         next->column = column;

         LOG_FMT(LSPACE, " = %s @ %zu => %zu\n", argval2str(av),
                 column - prev_column, next->column);
         if (restoreValues) { restore_options_for_QT(); }
      }

      pc = next;
      if (QT_SIGNAL_SLOT_found)
      {
         // flag the chunk for a second processing
         chunk_flags_set(pc, PCF_IN_QT_MACRO);
      }
   }
}


void space_text_balance_nested_parens(void)
{
   LOG_FUNC_ENTRY();

   chunk_t *first = chunk_get_head();
   while (is_valid(first))
   {
      chunk_t *next = chunk_get_next(first);
      break_if_invalid(next);

      if (is_str(first, "(", 1) &&
          is_str(next,  "(", 1) )
      {
         /* insert a space between the two opening parens */
         space_add_after(first, 1);

         /* find the closing paren that matches the 'first' open paren and force
          * a space before it */
         chunk_t *cur  = next;
         chunk_t *prev = next;
         while ((cur = chunk_get_next(cur)) != nullptr)
         {
            if (cur->level == first->level)
            {
               space_add_after(prev, 1);
               break;
            }
            prev = cur;
         }
      }
      else if (is_str(first, ")", 1) &&
               is_str(next,  ")", 1) )
      {
         /* insert a space between the two closing parens */
         space_add_after(first, 1);
      }

      first = next;
   }
}


size_t space_needed(chunk_t *first, chunk_t *second)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPACE, "%s\n", __func__);

   int min_sp;
   switch (do_space(first, second, min_sp))
   {
      case AV_ADD:    /* fallthrough */
      case AV_FORCE:  return((size_t)(max(1, min_sp)));
      case AV_REMOVE: return(0);
      case AV_IGNORE: /* fallthrough */
      default:        return(second->orig_col > (first->orig_col + first->len()));
   }
}


size_t space_col_align(chunk_t *first, chunk_t *second)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(first, second));

   LOG_FMT(LSPACE, "%s: %zu:%zu [%s/%s] '%s' <==> %zu:%zu [%s/%s] '%s'",
           __func__, first->orig_line, first->orig_col,
           get_token_name(first->type), get_token_name(first->ptype),
           first->text(), second->orig_line, second->orig_col,
           get_token_name(second->type), get_token_name(second->ptype),
           second->text());
   log_func_stack_inline(LSPACE);

   int      min_sp;
   argval_t av = do_space(first, second, min_sp);

   LOG_FMT(LSPACE, "%s: av=%d, ", __func__, av);
   size_t coldiff;
   if (first->nl_count > 0)
   {
      LOG_FMT(LSPACE, "nl_count=%zu, orig_col_end=%u", first->nl_count, first->orig_col_end);
      coldiff = first->orig_col_end - 1u;
   }
   else
   {
      LOG_FMT(LSPACE, "len=%zu", first->len());
      coldiff = first->len();
   }
   switch (av)
   {
      case AV_ADD:        /* fallthrough */
      case AV_FORCE:
         coldiff++;
         break;

      case AV_IGNORE:
         if (second->orig_col > (first->orig_col + first->len()))
         {
            coldiff++;
         }
         break;

      case AV_REMOVE:      /* fallthrough */
      case AV_NOT_DEFINED: /* fallthrough */
      default:             /* do nothing */ break;
   }
   LOG_FMT(LSPACE, " => %zu\n", coldiff);
   return(coldiff);
}


#define MAX_SPACE_COUNT 16u
void space_add_after(chunk_t *pc, size_t count)
{
   LOG_FUNC_ENTRY();
   //if (count == 0) { return; }

   chunk_t *next = chunk_get_next(pc);

   /* don't add at the end of the file or before a newline */
   if (is_invalid(next) ||
       chunk_is_newline(next) ) { return; }

   count = min(count, MAX_SPACE_COUNT);

   /* Two CT_SPACE in a row -- use the max of the two */
   if (is_type(next, CT_SPACE))
   {
      if (next->len() < count)
      {
         while (next->len() < count)
         {
            next->str.append(' ');
         }
      }
      return;
   }

   chunk_t sp;
   sp.flags       = pc->flags & PCF_COPY_FLAGS;
   sp.type        = CT_SPACE;
   sp.str         = "                "; // 16 spaces
   sp.str.resize(count);
   sp.level       = pc->level;
   sp.brace_level = pc->brace_level;
   sp.pp_level    = pc->pp_level;
   sp.column      = pc->column + pc->len();
   sp.orig_line   = pc->orig_line;

   chunk_add_after(&sp, pc);
}
