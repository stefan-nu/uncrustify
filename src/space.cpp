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
   chunk_t    *first,  /**< [in]  */
   chunk_t    *second, /**< [in]  */
   bool       complete /**< [in]  */
);


/**
 * Decides how to change inter-chunk spacing.
 * Note that the order of the if statements is VERY important.
 *
 * @return AV_IGNORE, AV_ADD, AV_REMOVE or AV_FORCE
 */
static argval_t do_space(
   chunk_t *first,   /**< [in]  The first chunk */
   chunk_t *second,  /**< [in]  The second chunk */
   int     &min_sp,  /**< [out] minimal required space */
   bool    complete  /**< [in]  */
);


struct no_space_table_t
{
   c_token_t first;   /**< [in]  */
   c_token_t second;  /**< [in]  */
};


/** this table lists all combos where a space should NOT be present
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



#define log_argval_and_return(argval)                              \
   do {                                                            \
      if (log_sev_on(LSPACE))                                      \
      { log_rule2(__LINE__, (argval2str(argval)), first, second, complete); } \
      return(argval);                                              \
   } while(0)

#define log_option_and_return(uo)                                  \
   do {                                                            \
      const option_map_value_t* my_option = get_option_name(uo);   \
      if (log_sev_on(LSPACE))                                      \
      { log_rule2(__LINE__, (my_option->name), first, second, complete); } \
      return(cpd.settings[uo].a);                                  \
   } while(0)

#define log_rule(rule)                                             \
   do {                                                            \
      if (log_sev_on(LSPACE))                                      \
      { log_rule2(__LINE__, (rule), first, second, complete); }    \
   } while (0)


static void log_rule2(size_t line, const char *rule, chunk_t *first, chunk_t *second, bool complete)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(first, second));

   if (is_not_type(second, CT_NEWLINE))
   {
      LOG_FMT(LSPACE, "Spacing: line %zu [%s/%s] '%s' <===> [%s/%s] '%s' : %s[%zu]%s",
              first->orig_line,
              get_token_name(first->type), get_token_name(first->parent_type),
              first->text(),
              get_token_name(second->type), get_token_name(second->parent_type),
              second->text(), rule, line,
              complete ? "\n" : "");
   }
}


/* \todo make min_sp a size_t */
static argval_t do_space(chunk_t *first, chunk_t *second, int &min_sp, bool complete = true)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(first, second));

   LOG_FMT(LSPACE, "%s: %zu:%zu %s %s\n", __func__, first->orig_line,
         first->orig_col, first->text(), get_token_name(first->type));

   min_sp = 1;
   if(any_is_type(first, second, CT_IGNORED  )) { log_argval_and_return(AV_REMOVE         ); }

   /* Leave spacing alone between PP_IGNORE tokens as we don't want the default behavior (which is ADD) */
   if(are_types  (first, second, CT_PP_IGNORE)) { log_argval_and_return(AV_IGNORE         ); }
   if(any_is_type(first, second, CT_PP       )) { log_option_and_return(UO_sp_pp_concat   ); }
   if(is_type    (first,         CT_POUND    )) { log_option_and_return(UO_sp_pp_stringify); }
   if(is_type(second, CT_POUND         ) &&
      (second->flags & PCF_IN_PREPROC  ) &&
      is_not_ptype(first, CT_MACRO_FUNC) )
   {
      log_rule("sp_before_pp_stringify");
      return(cpd.settings[UO_sp_before_pp_stringify].a);
   }
   if (any_is_type(first, second, CT_SPACE))
   {
      log_argval_and_return(AV_REMOVE);
//      log_rule("REMOVE");
//      return(AV_REMOVE);
   }
   if (is_type(second, 2, CT_NEWLINE, CT_VBRACE_OPEN))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }
   if (is_only_first_type(first, CT_VBRACE_OPEN, second, CT_NL_CONT))
   {
      log_rule("FORCE");
      return(AV_FORCE);
   }
   if (is_only_first_type(first, CT_VBRACE_CLOSE, second, CT_NL_CONT))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }
   if (is_type(second, CT_VSEMICOLON))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }
   if (is_type(first, CT_MACRO_FUNC))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }
   if (is_type(second, CT_NL_CONT))
   {
      log_rule("sp_before_nl_cont");
      return(cpd.settings[UO_sp_before_nl_cont].a);
   }
   if (any_is_type(first, second, CT_D_ARRAY_COLON))
   {
      log_rule("sp_d_array_colon");
      return(cpd.settings[UO_sp_d_array_colon].a);
   }
   if ( is_type(first, CT_CASE                    ) &&
       (CharTable::IsKeyword1((size_t)(second->str[0])) ||
        is_type(second, CT_NUMBER)                ) )
   {
      log_rule("sp_case_label");
      return(add_option(cpd.settings[UO_sp_case_label].a, AV_ADD));
   }
   if (is_type(first, CT_FOR_COLON))
   {
      log_rule("sp_after_for_colon");
      return(cpd.settings[UO_sp_after_for_colon].a);
   }
   if (is_type(second, CT_FOR_COLON))
   {
      log_rule("sp_before_for_colon");
      return(cpd.settings[UO_sp_before_for_colon].a);
   }
   if (are_types(first, CT_QUESTION, second, CT_COND_COLON))
   {
      if (is_not_option(cpd.settings[UO_sp_cond_ternary_short].a, AV_IGNORE))
      {
         return(cpd.settings[UO_sp_cond_ternary_short].a);
      }
   }
   if (any_is_type(first, second, CT_QUESTION))
   {
      if (is_type(second, CT_QUESTION) &&
          (is_not_option(cpd.settings[UO_sp_cond_question_before].a, AV_IGNORE)))
      {
         log_rule("sp_cond_question_before");
         return(cpd.settings[UO_sp_cond_question_before].a);
      }
      if (is_type(first, CT_QUESTION) &&
          (is_not_option(cpd.settings[UO_sp_cond_question_after].a, AV_IGNORE)))
      {
         log_rule("sp_cond_question_after");
         return(cpd.settings[UO_sp_cond_question_after].a);
      }
      if (is_not_option(cpd.settings[UO_sp_cond_question].a, AV_IGNORE))
      {
         log_rule("sp_cond_question");
         return(cpd.settings[UO_sp_cond_question].a);
      }
   }
   if (any_is_type(first, second, CT_COND_COLON))
   {
      if (is_type(second, CT_COND_COLON) &&
          (is_not_option(cpd.settings[UO_sp_cond_colon_before].a, AV_IGNORE)))
      {
         log_rule("sp_cond_colon_before");
         return(cpd.settings[UO_sp_cond_colon_before].a);
      }
      if (is_type(first, CT_COND_COLON) &&
          (is_not_option(cpd.settings[UO_sp_cond_colon_after].a, AV_IGNORE)))
      {
         log_rule("sp_cond_colon_after");
         return(cpd.settings[UO_sp_cond_colon_after].a);
      }
      if (is_not_option(cpd.settings[UO_sp_cond_colon].a, AV_IGNORE))
      {
         log_rule("sp_cond_colon");
         return(cpd.settings[UO_sp_cond_colon].a);
      }
   }
   if (any_is_type(first, second, CT_RANGE))
   {
      return(cpd.settings[UO_sp_range].a);
   }
   if (is_type_and_ptype(first, CT_COLON, CT_SQL_EXEC))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }
   /* Macro stuff can only return IGNORE, ADD, or FORCE */
   if (is_type(first, CT_MACRO))
   {
      log_rule("sp_macro");
      argval_t arg     = cpd.settings[UO_sp_macro].a;
      argval_t add_arg = is_not_option(arg, AV_IGNORE) ? AV_ADD : AV_IGNORE;
      return (add_option(arg, add_arg));
   }
   if (is_type_and_ptype(first, CT_FPAREN_CLOSE, CT_MACRO_FUNC))
   {
      log_rule("sp_macro_func");
      argval_t arg     = cpd.settings[UO_sp_macro_func].a;
      argval_t add_arg = is_not_option(arg, AV_IGNORE) ? AV_ADD : AV_IGNORE;
      return (add_option(arg, add_arg));
   }

   if (is_type(first, CT_PREPROC))
   {
      /* Remove spaces, unless we are ignoring. See indent_preproc() */
      if (cpd.settings[UO_pp_space].a == AV_IGNORE)
      {
         log_rule("IGNORE");
         return(AV_IGNORE);
      }
      log_rule("REMOVE");
      return(AV_REMOVE);
   }

   if (is_type(second, CT_SEMICOLON))
   {
      if (is_ptype(second, CT_FOR))
      {
         if ((is_not_option(cpd.settings[UO_sp_before_semi_for_empty].a, AV_IGNORE)) &&
             is_type(first, 2, CT_SPAREN_OPEN, CT_SEMICOLON) )
         {
            log_rule("sp_before_semi_for_empty");
            return(cpd.settings[UO_sp_before_semi_for_empty].a);
         }
         if (is_not_option(cpd.settings[UO_sp_before_semi_for].a, AV_IGNORE))
         {
            log_rule("sp_before_semi_for");
            return(cpd.settings[UO_sp_before_semi_for].a);
         }
      }

      argval_t arg = cpd.settings[UO_sp_before_semi].a;
#if 1
      if ((first->type        == CT_SPAREN_CLOSE) &&
          (first->parent_type != CT_WHILE_OF_DO ) )
#else
      // fails test 01011, 30711
      if (is_type     (first, CT_SPAREN_CLOSE) &&
          is_not_ptype(first, CT_WHILE_OF_DO ) )
#endif
      {
         log_rule("sp_before_semi|sp_special_semi");
         arg = (add_option(arg, cpd.settings[UO_sp_special_semi].a));
      }
      else
      {
         log_rule("sp_before_semi");
      }
      return(arg);
   }

   if (is_type(second,   CT_COMMENT             ) &&
       is_type(first, 2, CT_PP_ELSE, CT_PP_ENDIF) )
   {
      if (is_not_option(cpd.settings[UO_sp_endif_cmt].a, AV_IGNORE))
      {
         set_type(second, CT_COMMENT_ENDIF);
         log_rule("sp_endif_cmt");
         return(cpd.settings[UO_sp_endif_cmt].a);
      }
   }

   if ((is_not_option(cpd.settings[UO_sp_before_tr_emb_cmt].a, AV_IGNORE)) &&
       (is_ptype(second, 2, CT_COMMENT_END, CT_COMMENT_EMBED)            ) )
   {
      log_rule("sp_before_tr_emb_cmt");
      min_sp = cpd.settings[UO_sp_num_before_tr_emb_cmt].n;
      return(cpd.settings[UO_sp_before_tr_emb_cmt].a);
   }

   if (second->parent_type == CT_COMMENT_END)
   {
      switch (second->orig_prev_sp)
      {
         case 0:  log_rule("orig_prev_sp-REMOVE"); return(AV_REMOVE);
         case 1:  log_rule("orig_prev_sp-FORCE" ); return(AV_FORCE );
         default: log_rule("orig_prev_sp-ADD"   ); return(AV_ADD   );
      }
   }

   /* "for (;;)" vs "for (;; )" and "for (a;b;c)" vs "for (a; b; c)" */
   if (is_type(first, CT_SEMICOLON))
   {
      if (is_ptype(first, CT_FOR))
      {
         if ((is_not_option(cpd.settings[UO_sp_after_semi_for_empty].a, AV_IGNORE)) &&
             (is_type(second, CT_SPAREN_CLOSE)))
         {
            log_rule("sp_after_semi_for_empty");
            return(cpd.settings[UO_sp_after_semi_for_empty].a);
         }
         if (is_not_option(cpd.settings[UO_sp_after_semi_for].a, AV_IGNORE))
         {
            log_rule("sp_after_semi_for");
            return(cpd.settings[UO_sp_after_semi_for].a);
         }
      }
      else if (!chunk_is_comment(second) &&
               second->type != CT_BRACE_CLOSE)
      {
         log_rule("sp_after_semi");
         return(cpd.settings[UO_sp_after_semi].a);
      }
      /* Let the comment spacing rules handle this */
   }

   /* puts a space in the rare '+-' or '-+' */
   if (is_type(first,  3, CT_NEG, CT_POS, CT_ARITH) &&
       is_type(second, 3, CT_NEG, CT_POS, CT_ARITH) )
   {
      log_rule("ADD");
      return(AV_ADD);
   }

   /* "return(a);" vs "return (foo_t)a + 3;" vs "return a;" vs "return;" */
   if (is_type(first, CT_RETURN))
   {
      if (is_type_and_ptype(second, CT_PAREN_OPEN, CT_RETURN))
      {
         log_rule("sp_return_paren");
         return(cpd.settings[UO_sp_return_paren].a);
      }
      /* everything else requires a space */
      log_rule("FORCE");
      return(AV_FORCE);
   }

   /* "sizeof(foo_t)" vs "sizeof foo_t" */
   if (is_type(first, CT_SIZEOF))
   {
      if (is_type(second, CT_PAREN_OPEN))
      {
         log_rule("sp_sizeof_paren");
         return(cpd.settings[UO_sp_sizeof_paren].a);
      }
      log_rule("FORCE");
      return(AV_FORCE);
   }

   /* handle '::' */
   if (is_type(first, CT_DC_MEMBER))
   {
      log_rule("sp_after_dc");
      return(cpd.settings[UO_sp_after_dc].a);
   }

   // mapped_file_source abc((int) ::CW2A(sTemp));
   if (are_types(first, CT_PAREN_CLOSE, second, CT_DC_MEMBER) &&
       is_type(second->next, CT_FUNC_CALL) )
   {
      log_rule("REMOVE_889_A");
      return(AV_REMOVE);
   }
   if (is_type(second, CT_DC_MEMBER))
   {
      /* '::' at the start of an identifier is not member access, but global scope operator.
       * Detect if previous chunk is keyword
       */
      switch (first->type)
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
         case CT_USING:     log_rule("FORCE"); return(AV_FORCE);
         default:           break;
      }

      if (is_type(first, 3, CT_WORD, CT_TYPE, CT_PAREN_CLOSE) ||
          CharTable::IsKeyword1((size_t)(first->str[0])))
      {
         log_rule("sp_before_dc");
         return(cpd.settings[UO_sp_before_dc].a);
      }
   }

   /* "a,b" vs "a, b" */
   if (is_type(first, CT_COMMA))
   {
      if (is_ptype(first, CT_TYPE))
      {
         /* C# multidimensional array type: ',,' vs ', ,' or ',]' vs ', ]' */
         if (is_type(second, CT_COMMA))
         {
            log_rule("sp_between_mdatype_commas");
            return(cpd.settings[UO_sp_between_mdatype_commas].a);
         }
         else
         {
            log_rule("sp_after_mdatype_commas");
            return(cpd.settings[UO_sp_after_mdatype_commas].a);
         }
      }
      else
      {
         log_rule("sp_after_comma");
         return(cpd.settings[UO_sp_after_comma].a);
      }
   }
   // test if we are within a SIGNAL/SLOT call
   if (QT_SIGNAL_SLOT_found)
   {
      if (is_type(first,     CT_FPAREN_CLOSE          ) &&
          is_type(second, 2, CT_FPAREN_CLOSE, CT_COMMA) )
      {
         if (second->level == (size_t)(QT_SIGNAL_SLOT_level))
         {
            restoreValues = true;
         }
      }
   }
   if (is_type(second, CT_COMMA))
   {
      if (is_type_and_ptype(first, CT_SQUARE_OPEN, CT_TYPE))
      {
         log_rule("sp_before_mdatype_commas");
         return(cpd.settings[UO_sp_before_mdatype_commas].a);
      }
      if (is_type(first, CT_PAREN_OPEN) &&
          (cpd.settings[UO_sp_paren_comma].a != AV_IGNORE))
      {
         log_rule("sp_paren_comma");
         return(cpd.settings[UO_sp_paren_comma].a);
      }
      log_rule("sp_before_comma");
      return(cpd.settings[UO_sp_before_comma].a);
   }

   if (is_type(second, CT_ELLIPSIS))
   {
      /* non-punc followed by a ellipsis */
      if (((first->flags & PCF_PUNCTUATOR) == 0) &&
          (is_not_option(cpd.settings[UO_sp_before_ellipsis].a, AV_IGNORE)))
      {
         log_rule("sp_before_ellipsis");
         return(cpd.settings[UO_sp_before_ellipsis].a);
      }

      if (is_type(first, CT_TAG_COLON))
      {
         log_rule("FORCE");
         return(AV_FORCE);
      }
   }
   if (is_type(first, CT_ELLIPSIS) &&
       CharTable::IsKeyword1((size_t)(second->str[0])))
   {
      log_rule("FORCE");
      return(AV_FORCE);
   }
   if (is_type(first, CT_TAG_COLON))
   {
      log_rule("sp_after_tag");
      return(cpd.settings[UO_sp_after_tag].a);
   }
   if (is_type(second, CT_TAG_COLON))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }

   /* handle '~' */
   if (is_type(first, CT_DESTRUCTOR))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }

   /* "((" vs "( (" or "))" vs ") )" */
   if ((is_str(first, "(", 1) && is_str(second, "(", 1)) ||
       (is_str(first, ")", 1) && is_str(second, ")", 1)) )
   {
      log_rule("sp_paren_paren");
      return(cpd.settings[UO_sp_paren_paren].a);
   }

   if (are_types(first, CT_CATCH, second, CT_SPAREN_OPEN) &&
       (is_not_option(cpd.settings[UO_sp_catch_paren].a, AV_IGNORE)))
   {
      log_rule("sp_catch_paren");
      return(cpd.settings[UO_sp_catch_paren].a);
   }

   if (are_types(first, CT_D_VERSION_IF, second, CT_SPAREN_OPEN) &&
       (is_not_option(cpd.settings[UO_sp_version_paren].a, AV_IGNORE)))
   {
      log_rule("sp_version_paren");
      return(cpd.settings[UO_sp_version_paren].a);
   }

   if (are_types(first,  CT_D_SCOPE_IF, second, CT_SPAREN_OPEN) &&
       (is_not_option(cpd.settings[UO_sp_scope_paren].a, AV_IGNORE)))
   {
      log_rule("sp_scope_paren");
      return(cpd.settings[UO_sp_scope_paren].a);
   }

   /* "if (" vs "if(" */
   if (is_type(second, CT_SPAREN_OPEN))
   {
      log_rule("sp_before_sparen");
      return(cpd.settings[UO_sp_before_sparen].a);
   }

   if (any_is_type(first, second, CT_LAMBDA))
   {
      log_rule("sp_assign (lambda)");
      return(cpd.settings[UO_sp_assign].a);
   }

   // Handle the special lambda case for C++11:
   //    [=](Something arg){.....}
   if ((cpd.settings[UO_sp_cpp_lambda_assign].a != AV_IGNORE) &&
       ((is_type_and_ptype(first,  CT_SQUARE_OPEN,  CT_CPP_LAMBDA) && is_type(second, CT_ASSIGN) ) ||
        (is_type_and_ptype(second, CT_SQUARE_CLOSE, CT_CPP_LAMBDA) && is_type(first,  CT_ASSIGN) ) ) )
   {
      log_rule("UO_sp_cpp_lambda_assign");
      return(cpd.settings[UO_sp_cpp_lambda_assign].a);
   }

   // Handle the special lambda case for C++11:
   //    [](Something arg){.....}
   if ((cpd.settings[UO_sp_cpp_lambda_paren].a != AV_IGNORE    ) &&
       is_type_and_ptype(first,  CT_SQUARE_CLOSE, CT_CPP_LAMBDA) &&
       is_type          (second, CT_FPAREN_OPEN                ) )
   {
      log_rule("UO_sp_cpp_lambda_paren");
      return(cpd.settings[UO_sp_cpp_lambda_paren].a);
   }

   if (are_types(first, CT_ENUM, second, CT_FPAREN_OPEN))
   {
      if (is_not_option(cpd.settings[UO_sp_enum_paren].a, AV_IGNORE))
      {
         log_rule("sp_enum_paren");
         return(cpd.settings[UO_sp_enum_paren].a);
      }
   }

   if (is_type(second, CT_ASSIGN))
   {
      if (second->flags & PCF_IN_ENUM)
      {
         if (is_not_option(cpd.settings[UO_sp_enum_before_assign].a, AV_IGNORE))
         {
            log_rule("sp_enum_before_assign");
            return(cpd.settings[UO_sp_enum_before_assign].a);
         }
         log_rule("sp_enum_assign");
         return(cpd.settings[UO_sp_enum_assign].a);
      }
      if ((is_not_option(cpd.settings[UO_sp_assign_default].a, AV_IGNORE)) &&
          is_ptype(second, CT_FUNC_PROTO))
      {
         log_rule("sp_assign_default");
         return(cpd.settings[UO_sp_assign_default].a);
      }
      if (is_not_option(cpd.settings[UO_sp_before_assign].a, AV_IGNORE))
      {
         log_rule("sp_before_assign");
         return(cpd.settings[UO_sp_before_assign].a);
      }
      log_rule("sp_assign");
      return(cpd.settings[UO_sp_assign].a);
   }

   if (is_type(first, CT_ASSIGN))
   {
      if (first->flags & PCF_IN_ENUM)
      {
         if (is_not_option(cpd.settings[UO_sp_enum_after_assign].a, AV_IGNORE))
         {
            log_rule("sp_enum_after_assign");
            return(cpd.settings[UO_sp_enum_after_assign].a);
         }
         log_rule("sp_enum_assign");
         return(cpd.settings[UO_sp_enum_assign].a);
      }
      if ((is_not_option(cpd.settings[UO_sp_assign_default].a, AV_IGNORE)) &&
          is_ptype(first, CT_FUNC_PROTO) )
      {
         log_rule("sp_assign_default");
         return(cpd.settings[UO_sp_assign_default].a);
      }
      if (is_not_option(cpd.settings[UO_sp_after_assign].a, AV_IGNORE))
      {
         log_rule("sp_after_assign");
         return(cpd.settings[UO_sp_after_assign].a);
      }
      log_rule("sp_assign");
      return(cpd.settings[UO_sp_assign].a);
   }

   if (is_type(first, CT_BIT_COLON) &&
       (first->flags & PCF_IN_ENUM) )
   {
         log_rule("sp_enum_colon");
         return(cpd.settings[UO_sp_enum_colon].a);
   }

   if (is_type(second, CT_BIT_COLON))
   {
      if (second->flags & PCF_IN_ENUM)
      {
         log_rule("sp_enum_colon");
         return(cpd.settings[UO_sp_enum_colon].a);
      }
   }

   if (is_type(second, CT_OC_BLOCK_CARET))
   {
      log_rule("sp_before_oc_block_caret");
      return(cpd.settings[UO_sp_before_oc_block_caret].a);
   }
   if (is_type(first, CT_OC_BLOCK_CARET))
   {
      log_rule("sp_after_oc_block_caret");
      return(cpd.settings[UO_sp_after_oc_block_caret].a);
   }
   if (is_type(second, CT_OC_MSG_FUNC))
   {
      log_rule("sp_after_oc_msg_receiver");
      return(cpd.settings[UO_sp_after_oc_msg_receiver].a);
   }

   /* "a [x]" vs "a[x]" */
   if (is_type(second, CT_SQUARE_OPEN) &&
       (second->parent_type != CT_OC_MSG     ) )
   {
      log_rule("sp_before_square");
      return(cpd.settings[UO_sp_before_square].a);
   }

   /* "byte[]" vs "byte []" */
   if (is_type(second, CT_TSQUARE))
   {
      log_rule("sp_before_squares");
      return(cpd.settings[UO_sp_before_squares].a);
   }

   if ((cpd.settings[UO_sp_angle_shift].a != AV_IGNORE) &&
         are_types(first, second, CT_ANGLE_CLOSE))
   {
      log_rule("sp_angle_shift");
      return(cpd.settings[UO_sp_angle_shift].a);
   }

   /* spacing around template < > stuff */
   if (any_is_type(first, CT_ANGLE_OPEN, second, CT_ANGLE_CLOSE))
   {
      log_rule("sp_inside_angle");
      return(cpd.settings[UO_sp_inside_angle].a);
   }
   if (is_type(second, CT_ANGLE_OPEN))
   {
      if (is_type(first, CT_TEMPLATE) &&
          (is_not_option(cpd.settings[UO_sp_template_angle].a, AV_IGNORE)))
      {
         log_rule("sp_template_angle");
         return(cpd.settings[UO_sp_template_angle].a);
      }
      log_rule("sp_before_angle");
      return(cpd.settings[UO_sp_before_angle].a);
   }
   if (is_type(first, CT_ANGLE_CLOSE))
   {
      if (is_type(second, CT_WORD) ||
          CharTable::IsKeyword1((size_t)(second->str[0])))
      {
         if (is_not_option(cpd.settings[UO_sp_angle_word].a, AV_IGNORE))
         {
            log_rule("sp_angle_word");
            return(cpd.settings[UO_sp_angle_word].a);
         }
      }
      if (is_type(second, 2, CT_FPAREN_OPEN, CT_PAREN_OPEN ) )
      {
         chunk_t *next = chunk_get_next_ncnl(second);
         if (is_type(next, CT_FPAREN_CLOSE))
         {
            log_rule("sp_angle_paren_empty");
            return(cpd.settings[UO_sp_angle_paren_empty].a);
         }

         log_rule("sp_angle_paren");
         return(cpd.settings[UO_sp_angle_paren].a);
      }
      if (is_type(second, CT_DC_MEMBER))
      {
         log_rule("sp_before_dc");
         return(cpd.settings[UO_sp_before_dc].a);
      }
      if (is_not_type(second, 2, CT_BYREF, CT_PTR_TYPE))
      {
         log_rule("sp_after_angle");
         return(cpd.settings[UO_sp_after_angle].a);
      }
   }

   if (is_type(first, CT_BYREF) &&
       is_not_option(cpd.settings[UO_sp_after_byref_func].a, AV_IGNORE) &&
       is_ptype(first, 2, CT_FUNC_DEF, CT_FUNC_PROTO))
   {
      log_rule("sp_after_byref_func");
      return(cpd.settings[UO_sp_after_byref_func].a);
   }

   if (is_type(first, CT_BYREF                 ) &&
       CharTable::IsKeyword1((size_t)(second->str[0])) )
   {
      log_rule("sp_after_byref");
      return(cpd.settings[UO_sp_after_byref].a);
   }

   if (is_type(second, CT_BYREF))
   {
      if (is_not_option(cpd.settings[UO_sp_before_byref_func].a, AV_IGNORE))
      {
         chunk_t *next = chunk_get_next(second);
         if (is_type(next, 2, CT_FUNC_DEF, CT_FUNC_PROTO))
         {
            return(cpd.settings[UO_sp_before_byref_func].a);
         }
      }

      if (is_not_option(cpd.settings[UO_sp_before_unnamed_byref].a, AV_IGNORE))
      {
         chunk_t *next = chunk_get_next_nc(second);
         if (is_not_type(next, CT_WORD))
         {
            log_rule("sp_before_unnamed_byref");
            return(cpd.settings[UO_sp_before_unnamed_byref].a);
         }
      }
      log_rule("sp_before_byref");
      return(cpd.settings[UO_sp_before_byref].a);
   }

   if (is_type(first, CT_SPAREN_CLOSE))
   {
      if (is_type(second, CT_BRACE_OPEN) &&
          (cpd.settings[UO_sp_sparen_brace].a != AV_IGNORE))
      {
         log_rule("sp_sparen_brace");
         return(cpd.settings[UO_sp_sparen_brace].a);
      }
      if (!chunk_is_comment(second) &&
          (cpd.settings[UO_sp_after_sparen].a != AV_IGNORE))
      {
         log_rule("sp_after_sparen");
         return(cpd.settings[UO_sp_after_sparen].a);
      }
   }

   if (is_type (second, CT_FPAREN_OPEN) &&
       is_ptype(first,  CT_OPERATOR   ) &&
       (is_not_option(cpd.settings[UO_sp_after_operator_sym].a, AV_IGNORE)))
   {

      // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_after_operator_sym_empty].a, AV_IGNORE)) &&
          (is_type(second, CT_FPAREN_OPEN)))
      {
         const chunk_t *next = chunk_get_next_ncnl(second);
         if (is_type(next, CT_FPAREN_CLOSE))
         {
            log_rule("sp_after_operator_sym_empty");
            return(cpd.settings[UO_sp_after_operator_sym_empty].a);
         }
      }

      log_rule("sp_after_operator_sym");
      return(cpd.settings[UO_sp_after_operator_sym].a);
      // DRY1 end
   }

   /* spaces between function and open paren */
   if (is_type(first, 2, CT_FUNC_CALL, CT_FUNC_CTOR_VAR))
   {

      // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_func_call_paren_empty].a, AV_IGNORE)) &&
          (is_type(second, CT_FPAREN_OPEN)))
      {
         chunk_t *next = chunk_get_next_ncnl(second);
         if (is_type(next, CT_FPAREN_CLOSE) )
         {
            log_rule("sp_func_call_paren_empty");
            return(cpd.settings[UO_sp_func_call_paren_empty].a);
         }
      }
      log_rule("sp_func_call_paren");
      return(cpd.settings[UO_sp_func_call_paren].a);
      // DRY1 end

   }
   if (is_type(first, CT_FUNC_CALL_USER))
   {
      log_rule("sp_func_call_user_paren");
      return(cpd.settings[UO_sp_func_call_user_paren].a);
   }
   if (is_type(first, CT_ATTRIBUTE))
   {
      log_rule("sp_attribute_paren");
      return(cpd.settings[UO_sp_attribute_paren].a);
   }
   if (is_type(first, CT_FUNC_DEF))
   {

      // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_func_def_paren_empty].a, AV_IGNORE)) &&
          (is_type(second, CT_FPAREN_OPEN)))
      {
         const chunk_t *next = chunk_get_next_ncnl(second);
         if (is_type(next, CT_FPAREN_CLOSE))
         {
            log_rule("sp_func_def_paren_empty");
            return(cpd.settings[UO_sp_func_def_paren_empty].a);
         }
      }
      log_rule("sp_func_def_paren");
      return(cpd.settings[UO_sp_func_def_paren].a);
      // DRY1 end

   }
   if (is_type(first, 2, CT_CPP_CAST, CT_TYPE_WRAP))
   {
      log_rule("sp_cpp_cast_paren");
      return(cpd.settings[UO_sp_cpp_cast_paren].a);
   }

   if (are_types(first, CT_PAREN_CLOSE, second, CT_WHEN))
   {
      log_rule("FORCE");
      return(AV_FORCE); /* TODO: make this configurable? */
   }

   if (is_type(first,     CT_PAREN_CLOSE               ) &&
       is_type(second, 2, CT_PAREN_OPEN, CT_FPAREN_OPEN) )
   {
      /* "(int)a" vs "(int) a" or "cast(int)a" vs "cast(int) a" */
      if (is_ptype(first, 2, CT_C_CAST, CT_D_CAST))
      {
         log_rule("sp_after_cast");
         return(cpd.settings[UO_sp_after_cast].a);
      }

      /* Must be an indirect/chained function call? */
      log_rule("REMOVE");
      return(AV_REMOVE);  /* TODO: make this configurable? */
   }

   /* handle the space between parens in fcn type 'void (*f)(void)' */
   if (is_type(first, CT_TPAREN_CLOSE))
   {
      log_rule("sp_after_tparen_close");
      return(cpd.settings[UO_sp_after_tparen_close].a);
   }

   /* ")(" vs ") (" */
   if ((is_str(first, ")", 1) && is_str(second, "(", 1)) ||
       (chunk_is_paren_close(first) && chunk_is_paren_open(second)))
   {
      log_rule("sp_cparen_oparen");
      return(cpd.settings[UO_sp_cparen_oparen].a);
   }

   if (is_type          (first,                  CT_FUNC_PROTO ) ||
       is_type_and_ptype(second, CT_FPAREN_OPEN, CT_FUNC_PROTO ) )
   {

      // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_func_proto_paren_empty].a, AV_IGNORE)) &&
          (is_type(second, CT_FPAREN_OPEN)))
      {
         const chunk_t *next = chunk_get_next_ncnl(second);
         if (is_type(next, CT_FPAREN_CLOSE))
         {
            log_rule("sp_func_proto_paren_empty");
            return(cpd.settings[UO_sp_func_proto_paren_empty].a);
         }
      }
      log_rule("sp_func_proto_paren");
      return(cpd.settings[UO_sp_func_proto_paren].a);
      // DRY1 end

   }
   if (is_type(first, 2, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO))
   {
      // \todo DRY1 start
      if ((is_not_option(cpd.settings[UO_sp_func_class_paren_empty].a, AV_IGNORE)) &&
          is_type(second, CT_FPAREN_OPEN))
      {
         const chunk_t *next = chunk_get_next_ncnl(second);
         if (is_type(next, CT_FPAREN_CLOSE))
         {
            log_rule("sp_func_class_paren_empty");
            return(cpd.settings[UO_sp_func_class_paren_empty].a);
         }
      }
      log_rule("sp_func_class_paren");
      return(cpd.settings[UO_sp_func_class_paren].a);
      // DRY1 end

   }
   if (  is_type(first, CT_CLASS) &&
        !(first->flags & PCF_IN_OC_MSG) )
   {
      log_rule("FORCE");
      return(AV_FORCE);
   }

   if (are_types(first, CT_BRACE_OPEN, second, CT_BRACE_CLOSE))
   {
      log_rule("sp_inside_braces_empty");
      return(cpd.settings[UO_sp_inside_braces_empty].a);
   }

   if (is_type(second, CT_BRACE_CLOSE))
   {
      if (second->parent_type == CT_ENUM)
      {
         log_rule("sp_inside_braces_enum");
         return(cpd.settings[UO_sp_inside_braces_enum].a);
      }
      if (is_ptype(second, 2,CT_STRUCT, CT_UNION))
      {
         log_rule("sp_inside_braces_struct");
         return(cpd.settings[UO_sp_inside_braces_struct].a);
      }
      log_rule("sp_inside_braces");
      return(cpd.settings[UO_sp_inside_braces].a);
   }

   if (is_type(first, CT_D_CAST))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }

   if (are_types(first, CT_PP_DEFINED, second, CT_PAREN_OPEN))
   {
      log_rule("sp_defined_paren");
      return(cpd.settings[UO_sp_defined_paren].a);
   }

   if (is_type(first, CT_THROW))
   {
      if (is_type(second, CT_PAREN_OPEN))
      {
         log_rule("sp_throw_paren");
         return(cpd.settings[UO_sp_throw_paren].a);
      }
      log_rule("sp_after_throw");
      return(cpd.settings[UO_sp_after_throw].a);
   }

   if (are_types(first, CT_THIS, second, CT_PAREN_OPEN) )
   {
      log_rule("sp_this_paren");
      return(cpd.settings[UO_sp_this_paren].a);
   }

   if (are_types(first, CT_STATE, second, CT_PAREN_OPEN))
   {
      log_rule("ADD");
      return(AV_ADD);
   }

   if (are_types(first, CT_DELEGATE, second, CT_PAREN_OPEN))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }

   if (any_is_type(first, CT_MEMBER, second, CT_MEMBER))
   {
      log_rule("sp_member");
      return(cpd.settings[UO_sp_member].a);
   }

   if (is_type(first, CT_C99_MEMBER))
   {
      // always remove space(s) after then '.' of a C99-member
      log_rule("REMOVE");
      return(AV_REMOVE);
   }

   if (are_types(first, CT_SUPER, second, CT_PAREN_OPEN))
   {
      log_rule("sp_super_paren");
      return(cpd.settings[UO_sp_super_paren].a);
   }

   if (are_types(first, CT_FPAREN_CLOSE, second, CT_BRACE_OPEN))
   {
      if (second->parent_type == CT_DOUBLE_BRACE)
      {
         log_rule("sp_fparen_dbrace");
         return(cpd.settings[UO_sp_fparen_dbrace].a);
      }
      log_rule("sp_fparen_brace");
      return(cpd.settings[UO_sp_fparen_brace].a);
   }

   if (any_is_type(first, second, CT_D_TEMPLATE))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }

   if (are_types(first, CT_ELSE, second, CT_BRACE_OPEN))
   {
      log_rule("sp_else_brace");
      return(cpd.settings[UO_sp_else_brace].a);
   }

   if (are_types(first, CT_ELSE, second, CT_ELSEIF))
   {
      log_rule("FORCE");
      return(AV_FORCE);
   }

   if (are_types(first, CT_CATCH, second, CT_BRACE_OPEN))
   {
      log_rule("sp_catch_brace");
      return(cpd.settings[UO_sp_catch_brace].a);
   }

   if (are_types(first, CT_FINALLY, second, CT_BRACE_OPEN))
   {
      log_rule("sp_finally_brace");
      return(cpd.settings[UO_sp_finally_brace].a);
   }

   if (are_types(first, CT_TRY, second, CT_BRACE_OPEN))
   {
      log_rule("sp_try_brace");
      return(cpd.settings[UO_sp_try_brace].a);
   }

   if (are_types(first, CT_GETSET, second, CT_BRACE_OPEN))
   {
      log_rule("sp_getset_brace");
      return(cpd.settings[UO_sp_getset_brace].a);
   }

   if (are_types(first, CT_WORD, second, CT_BRACE_OPEN))
   {
      if (is_ptype(first, CT_NAMESPACE))
      {
         log_rule("sp_word_brace_ns");
         return(cpd.settings[UO_sp_word_brace_ns].a);
      }
      if (are_ptypes(first, second, CT_NONE) )
      {
         log_rule("sp_word_brace");
         return(cpd.settings[UO_sp_word_brace].a);
      }
   }

   if (is_type_and_ptype(second, CT_PAREN_OPEN, CT_INVARIANT))
   {
      log_rule("sp_invariant_paren");
      return(cpd.settings[UO_sp_invariant_paren].a);
   }

   if (is_type(first, CT_PAREN_CLOSE))
   {
      if (is_ptype(first, CT_D_TEMPLATE))
      {
         log_rule("FORCE");
         return(AV_FORCE);
      }

      if (is_ptype(first, CT_INVARIANT))
      {
         log_rule("sp_after_invariant_paren");
         return(cpd.settings[UO_sp_after_invariant_paren].a);
      }

      /* Arith after a cast comes first */
      if (is_type(second, 2, CT_ARITH, CT_CARET))
      {
         log_rule("sp_arith");
         return(cpd.settings[UO_sp_arith].a);
      }

      /* "(struct foo) {...}" vs "(struct foo){...}" */
      if (is_type(second, CT_BRACE_OPEN))
      {
         log_rule("sp_paren_brace");
         return(cpd.settings[UO_sp_paren_brace].a);
      }

      /* D-specific: "delegate(some thing) dg */
      if (is_ptype(first, CT_DELEGATE))
      {
         log_rule("ADD");
         return(AV_ADD);
      }

      /* PAWN-specific: "state (condition) next" */
      if (is_ptype(first, CT_STATE))
      {
         log_rule("ADD");
         return(AV_ADD);
      }
   }

   /* "foo(...)" vs "foo( ... )" */
   if (any_is_type(first, CT_FPAREN_OPEN, second, CT_FPAREN_CLOSE))
   {
      if (are_types(first, CT_FPAREN_OPEN, second, CT_FPAREN_CLOSE))
      {
         log_rule("sp_inside_fparens");
         return(cpd.settings[UO_sp_inside_fparens].a);
      }
      log_rule("sp_inside_fparen");
      return(cpd.settings[UO_sp_inside_fparen].a);
   }

   /* "foo(...)" vs "foo( ... )" */
   if (any_is_type(first, CT_TPAREN_OPEN, second, CT_TPAREN_CLOSE))
   {
      log_rule("sp_inside_tparen");
      return(cpd.settings[UO_sp_inside_tparen].a);
   }

   if (is_type(first, CT_PAREN_CLOSE))
   {
      if ((first->flags & PCF_OC_RTYPE) /*== CT_OC_RTYPE)*/ &&
          is_ptype(first, 2, CT_OC_MSG_DECL, CT_OC_MSG_SPEC))
      {
         log_rule("sp_after_oc_return_type");
         return(cpd.settings[UO_sp_after_oc_return_type].a);
      }
      else if (is_ptype(first, 2, CT_OC_MSG_SPEC, CT_OC_MSG_DECL))
      {
         log_rule("sp_after_oc_type");
         return(cpd.settings[UO_sp_after_oc_type].a);
      }
      else if (is_ptype   (first,  CT_OC_SEL      ) &&
               is_not_type(second, CT_SQUARE_CLOSE) )
      {
         log_rule("sp_after_oc_at_sel_parens");
         return(cpd.settings[UO_sp_after_oc_at_sel_parens].a);
      }
   }

   if (cpd.settings[UO_sp_inside_oc_at_sel_parens].a != AV_IGNORE)
   {
      if ((is_type(first,  CT_PAREN_OPEN ) && is_ptype(first,  2, CT_OC_SEL, CT_OC_PROTOCOL)) ||
          (is_type(second, CT_PAREN_CLOSE) && is_ptype(second, 2, CT_OC_SEL, CT_OC_PROTOCOL)) )
      {
         log_rule("sp_inside_oc_at_sel_parens");
         return(cpd.settings[UO_sp_inside_oc_at_sel_parens].a);
      }
   }

   if (is_type(second,   CT_PAREN_OPEN            ) &&
       is_type(first, 2, CT_OC_SEL, CT_OC_PROTOCOL) )
   {
      log_rule("sp_after_oc_at_sel");
      return(cpd.settings[UO_sp_after_oc_at_sel].a);
   }

   /* C cast:   "(int)"      vs "( int )"
    * D cast:   "cast(int)"  vs "cast( int )"
    * CPP cast: "int(a + 3)" vs "int( a + 3 )" */
   if (is_type(first, CT_PAREN_OPEN))
   {
      if (is_ptype(first, 3, CT_C_CAST, CT_CPP_CAST, CT_D_CAST))
      {
         log_rule("sp_inside_paren_cast");
         return(cpd.settings[UO_sp_inside_paren_cast].a);
      }
      log_rule("sp_inside_paren");
      return(cpd.settings[UO_sp_inside_paren].a);
   }

   if (is_type(second, CT_PAREN_CLOSE))
   {
      if (is_ptype(second, 3, CT_C_CAST, CT_CPP_CAST, CT_D_CAST))
      {
         log_rule("sp_inside_paren_cast");
         return(cpd.settings[UO_sp_inside_paren_cast].a);
      }
      log_rule("sp_inside_paren");
      return(cpd.settings[UO_sp_inside_paren].a);
   }

   /* "[3]" vs "[ 3 ]" */
   if (any_is_type(first, CT_SQUARE_OPEN, second, CT_SQUARE_CLOSE))
   {
      log_rule("sp_inside_square");
      return(cpd.settings[UO_sp_inside_square].a);
   }
   if (are_types(first, CT_SQUARE_CLOSE, second, CT_FPAREN_OPEN ))
   {
      log_rule("sp_square_fparen");
      return(cpd.settings[UO_sp_square_fparen].a);
   }

   /* "if(...)" vs "if( ... )" */
   if (is_type(second, CT_SPAREN_CLOSE) &&
       (is_not_option(cpd.settings[UO_sp_inside_sparen_close].a, AV_IGNORE)))
   {
      log_rule("sp_inside_sparen_close");
      return(cpd.settings[UO_sp_inside_sparen_close].a);
   }
   if (is_type(first, CT_SPAREN_OPEN) &&
       (is_not_option(cpd.settings[UO_sp_inside_sparen_open].a, AV_IGNORE)))
   {
      log_rule("sp_inside_sparen_open");
      return(cpd.settings[UO_sp_inside_sparen_open].a);
   }
   if (any_is_type(first, CT_SPAREN_OPEN, second, CT_SPAREN_CLOSE))
   {
      log_rule("sp_inside_sparen");
      return(cpd.settings[UO_sp_inside_sparen].a);
   }

   if ((is_not_option(cpd.settings[UO_sp_after_class_colon].a, AV_IGNORE)) &&
       (is_type(first, CT_CLASS_COLON)))
   {
      log_rule("sp_after_class_colon");
      return(cpd.settings[UO_sp_after_class_colon].a);
   }
   if ((is_not_option(cpd.settings[UO_sp_before_class_colon].a, AV_IGNORE)) &&
       (is_type(second, CT_CLASS_COLON)))
   {
      log_rule("sp_before_class_colon");
      return(cpd.settings[UO_sp_before_class_colon].a);
   }

   if ((is_not_option(cpd.settings[UO_sp_after_constr_colon].a, AV_IGNORE)) &&
       (is_type(first, CT_CONSTR_COLON)))
   {
      min_sp = cpd.settings[UO_indent_ctor_init_leading].n - 1; // default indent is 1 space

      log_rule("sp_after_constr_colon");
      return(cpd.settings[UO_sp_after_constr_colon].a);
   }
   if ((is_not_option(cpd.settings[UO_sp_before_constr_colon].a, AV_IGNORE)) &&
       (is_type(second, CT_CONSTR_COLON)))
   {
      log_rule("sp_before_constr_colon");
      return(cpd.settings[UO_sp_before_constr_colon].a);
   }

   if ((is_not_option(cpd.settings[UO_sp_before_case_colon].a, AV_IGNORE)) &&
       (is_type(second, CT_CASE_COLON)))
   {
      log_rule("sp_before_case_colon");
      return(cpd.settings[UO_sp_before_case_colon].a);
   }

   if (is_type(first, CT_DOT))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }
   if (is_type(second, CT_DOT))
   {
      log_rule("ADD");
      return(AV_ADD);
   }

   if (any_is_type(first, second, CT_NULLCOND))
   {
      log_rule("sp_member");
      return(cpd.settings[UO_sp_member].a);
   }

   if (is_type(first,  2, CT_ARITH, CT_CARET) ||
       is_type(second, 2, CT_ARITH, CT_CARET) )
   {
      log_rule("sp_arith");
      return(cpd.settings[UO_sp_arith].a);
   }

   if (any_is_type(first, second, CT_BOOL))
   {
      argval_t arg = cpd.settings[UO_sp_bool].a;
      if (is_not_token(cpd.settings[UO_pos_bool].tp, TP_IGNORE) &&
          (first->orig_line != second->orig_line) &&
          (arg != AV_REMOVE))
      {
         arg = static_cast<argval_t>(arg | AV_ADD);
      }
      log_rule("sp_bool");
      return(arg);
   }

   if (any_is_type(first, second, CT_COMPARE))
   {
      log_rule("sp_compare");
      return(cpd.settings[UO_sp_compare].a);
   }

   if (are_types(first, CT_PAREN_OPEN, second, CT_PTR_TYPE))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }

   if (is_type(first, CT_PTR_TYPE) &&
       (cpd.settings[UO_sp_ptr_star_paren].a != AV_IGNORE) &&
       (is_type(second, 2, CT_FPAREN_OPEN, CT_TPAREN_OPEN)))
   {
      log_rule("sp_ptr_star_paren");
      return(cpd.settings[UO_sp_ptr_star_paren].a);
   }

   if (are_types(first, second, CT_PTR_TYPE) &&
       (is_not_option(cpd.settings[UO_sp_between_ptr_star].a, AV_IGNORE)))
   {
      log_rule("sp_between_ptr_star");
      return(cpd.settings[UO_sp_between_ptr_star].a);
   }

   if ( is_type (first, CT_PTR_TYPE) &&
       (is_not_option(cpd.settings[UO_sp_after_ptr_star_func].a, AV_IGNORE)) &&
       (is_ptype(first, 3, CT_FUNC_DEF, CT_FUNC_PROTO, CT_FUNC_VAR)))
   {
      log_rule("sp_after_ptr_star_func");
      return(cpd.settings[UO_sp_after_ptr_star_func].a);
   }

   if (is_type(first, CT_PTR_TYPE                ) &&
       (CharTable::IsKeyword1((size_t)(second->str[0]))) )
   {
      chunk_t *prev = chunk_get_prev(first);
      if (is_type(prev, CT_IN))
      {
         log_rule("sp_deref");
         return(cpd.settings[UO_sp_deref].a);
      }

      if (is_type(second, CT_QUALIFIER) &&
          (is_not_option(cpd.settings[UO_sp_after_ptr_star_qualifier].a, AV_IGNORE)))
      {
         log_rule("sp_after_ptr_star_qualifier");
         return(cpd.settings[UO_sp_after_ptr_star_qualifier].a);
      }
      else if (is_not_option(cpd.settings[UO_sp_after_ptr_star].a, AV_IGNORE))
      {
         log_rule("sp_after_ptr_star");
         return(cpd.settings[UO_sp_after_ptr_star].a);
      }
   }

   if (is_only_first_type(second, CT_PTR_TYPE, first, CT_IN))
   {
      if (is_not_option(cpd.settings[UO_sp_before_ptr_star_func].a, AV_IGNORE))
      {
         /* Find the next non-'*' chunk */
         chunk_t *next = second;
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
         chunk_t *next = chunk_get_next_nc(second);
         while (is_type(next, CT_PTR_TYPE) )
         {
            next = chunk_get_next_nc(next);
         }
         if (is_not_type(next, CT_WORD) )
         {
            log_rule("sp_before_unnamed_ptr_star");
            return(cpd.settings[UO_sp_before_unnamed_ptr_star].a);
         }
      }
      if (is_not_option(cpd.settings[UO_sp_before_ptr_star].a, AV_IGNORE))
      {
         log_rule("sp_before_ptr_star");
         return(cpd.settings[UO_sp_before_ptr_star].a);
      }
   }

   if (is_type(first, CT_OPERATOR))
   {
      log_rule("sp_after_operator");
      return(cpd.settings[UO_sp_after_operator].a);
   }

   if (is_type(second, 2, CT_FUNC_PROTO, CT_FUNC_DEF))
   {
      if (is_not_type(first, CT_PTR_TYPE))
      {
         log_rule("sp_type_func|ADD");
         return(add_option(cpd.settings[UO_sp_type_func].a, AV_ADD));
      }
      log_rule("sp_type_func");
      return(cpd.settings[UO_sp_type_func].a);
   }

   /* "(int)a" vs "(int) a" or "cast(int)a" vs "cast(int) a" */
   if (is_ptype(first, 2, CT_C_CAST, CT_D_CAST) )
   {
      log_rule("sp_after_cast");
      return(cpd.settings[UO_sp_after_cast].a);
   }

   if (is_type(first, CT_BRACE_CLOSE))
   {
      if (is_type(second, CT_ELSE))
      {
         log_rule("sp_brace_else");
         return(cpd.settings[UO_sp_brace_else].a);
      }
      else if (is_type(second, CT_CATCH))
      {
         log_rule("sp_brace_catch");
         return(cpd.settings[UO_sp_brace_catch].a);
      }
      else if (is_type(second, CT_FINALLY))
      {
         log_rule("sp_brace_finally");
         return(cpd.settings[UO_sp_brace_finally].a);
      }
   }

   if (is_type(first, CT_BRACE_OPEN))
   {
      if (is_ptype(first, CT_ENUM))
      {
         log_rule("sp_inside_braces_enum");
         return(cpd.settings[UO_sp_inside_braces_enum].a);
      }
      else if (is_ptype(first, 2, CT_UNION, CT_STRUCT))
      {
         log_rule("sp_inside_braces_struct");
         return(cpd.settings[UO_sp_inside_braces_struct].a);
      }
      else if (!chunk_is_comment(second))
      {
         log_rule("sp_inside_braces");
         return(cpd.settings[UO_sp_inside_braces].a);
      }
   }

   if (is_type(second, CT_BRACE_CLOSE))
   {
      if (is_ptype(second, CT_ENUM))
      {
         log_rule("sp_inside_braces_enum");
         return(cpd.settings[UO_sp_inside_braces_enum].a);
      }
      else if (is_ptype(second, 2, CT_UNION, CT_STRUCT))
      {
         log_rule("sp_inside_braces_struct");
         return(cpd.settings[UO_sp_inside_braces_struct].a);
      }

      log_rule("sp_inside_braces");
      return(cpd.settings[UO_sp_inside_braces].a);
   }

   if ( is_type(first, CT_BRACE_CLOSE) &&
       (first->flags & PCF_IN_TYPEDEF) &&
       (is_ptype(first, 3, CT_ENUM, CT_STRUCT, CT_UNION)))
   {
      log_rule("sp_brace_typedef");
      return(cpd.settings[UO_sp_brace_typedef].a);
   }

   if (is_type(second, CT_SPAREN_OPEN))
   {
      log_rule("sp_before_sparen");
      return(cpd.settings[UO_sp_before_sparen].a);
   }

   if (is_type_and_ptype(second, CT_PAREN_OPEN, CT_TEMPLATE))
   {
      log_rule("UO_sp_before_template_paren");
      return(cpd.settings[UO_sp_before_template_paren].a);
   }

   if (is_not_type(second, CT_PTR_TYPE        ) &&
       is_type(first, 2, CT_QUALIFIER, CT_TYPE) )
   {
      argval_t arg = cpd.settings[UO_sp_after_type].a;
      log_rule("sp_after_type");
      return((arg != AV_REMOVE) ? arg : AV_FORCE);
   }

   if (is_type(first, 3, CT_MACRO_OPEN, CT_MACRO_CLOSE, CT_MACRO_ELSE))
   {
      if (is_type(second, CT_PAREN_OPEN))
      {
         log_rule("sp_func_call_paren");
         return(cpd.settings[UO_sp_func_call_paren].a);
      }
      log_rule("IGNORE");
      return(AV_IGNORE);
   }

   /* If nothing claimed the PTR_TYPE, then return ignore */
   if (any_is_type(first, second, CT_PTR_TYPE))
   {
      log_rule("IGNORE");
      return(AV_IGNORE);
   }

   if (is_type(first, CT_NOT))
   {
      log_rule("sp_not");
      return(cpd.settings[UO_sp_not].a);
   }
   if (is_type(first, CT_INV))
   {
      log_rule("sp_inv");
      return(cpd.settings[UO_sp_inv].a);
   }
   if (is_type(first, CT_ADDR))
   {
      log_rule("sp_addr");
      return(cpd.settings[UO_sp_addr].a);
   }
   if (is_type(first, CT_DEREF))
   {
      log_rule("sp_deref");
      return(cpd.settings[UO_sp_deref].a);
   }
   if (is_type(first, 2, CT_POS, CT_NEG) )
   {
      log_rule("sp_sign");
      return(cpd.settings[UO_sp_sign].a);
   }
   if (any_is_type(first, CT_INCDEC_BEFORE, second, CT_INCDEC_AFTER))
   {
      log_rule("sp_incdec");
      return(cpd.settings[UO_sp_incdec].a);
   }
   if (is_type(second, CT_CS_SQ_COLON))
   {
      log_rule("REMOVE");
      return(AV_REMOVE);
   }
   if (is_type(first, CT_CS_SQ_COLON))
   {
      log_rule("FORCE");
      return(AV_FORCE);
   }
   if (is_type(first, CT_OC_SCOPE))
   {
      log_rule("sp_after_oc_scope");
      return(cpd.settings[UO_sp_after_oc_scope].a);
   }
   if (is_type(first, CT_OC_DICT_COLON))
   {
      log_rule("sp_after_oc_dict_colon");
      return(cpd.settings[UO_sp_after_oc_dict_colon].a);
   }
   if (is_type(second, CT_OC_DICT_COLON))
   {
      log_rule("sp_before_oc_dict_colon");
      return(cpd.settings[UO_sp_before_oc_dict_colon].a);
   }
   if (is_type(first, CT_OC_COLON))
   {
      if (first->flags & PCF_IN_OC_MSG)
      {
         log_rule("sp_after_send_oc_colon");
         return(cpd.settings[UO_sp_after_send_oc_colon].a);
      }
      else
      {
         log_rule("sp_after_oc_colon");
         return(cpd.settings[UO_sp_after_oc_colon].a);
      }
   }
   if (is_type(second, CT_OC_COLON))
   {
      if ((first->flags & PCF_IN_OC_MSG  )   &&
          is_type(first, 2, CT_OC_MSG_FUNC, CT_OC_MSG_NAME) )
      {
         log_rule("sp_before_send_oc_colon");
         return(cpd.settings[UO_sp_before_send_oc_colon].a);
      }
      else
      {
         log_rule("sp_before_oc_colon");
         return(cpd.settings[UO_sp_before_oc_colon].a);
      }
   }

   if (is_type_and_ptype(second, CT_COMMENT, CT_COMMENT_EMBED))
   {
      log_rule("FORCE");
      return(AV_FORCE);
   }

   if (chunk_is_comment(second))
   {
      log_rule("IGNORE");
      return(AV_IGNORE);
   }

   if (is_type(first, CT_COMMENT))
   {
      log_rule("FORCE");
      return(AV_FORCE);
   }

   if (is_type(first,  CT_NEW       ) &&
       is_type(second, CT_PAREN_OPEN) )
   {
      log_rule("sp_between_new_paren");
      return(cpd.settings[UO_sp_between_new_paren].a);
   }
   if (is_type          (first, 2, CT_NEW,  CT_DELETE) ||
       is_type_and_ptype(first, CT_TSQUARE, CT_DELETE) )
   {
      log_rule("sp_after_new");
      return(cpd.settings[UO_sp_after_new].a);
   }

   if (is_type(first, CT_ANNOTATION) &&
       (chunk_is_paren_open(second) ) )
   {
      log_rule("sp_annotation_paren");
      return(cpd.settings[UO_sp_annotation_paren].a);
   }

   if (is_type(first, CT_OC_PROPERTY))
   {
      log_rule("sp_after_oc_property");
      return(cpd.settings[UO_sp_after_oc_property].a);
   }

   if (are_types(first, CT_EXTERN, second, CT_PAREN_OPEN))
   {
      log_rule("sp_extern_paren");
      return(cpd.settings[UO_sp_extern_paren].a);
   }

   // this table lists out all combos where a space should NOT be present
   // CT_UNKNOWN is a wildcard.
   for (auto it : no_space_table)
   {
      if ( ((it.first  == CT_UNKNOWN) || (it.first  == first->type ) ) &&
           ((it.second == CT_UNKNOWN) || (it.second == second->type) ) )
      {
         log_rule("REMOVE from no_space_table");
         return(AV_REMOVE);
      }
   }

   // mapped_file_source abc((int) A::CW2A(sTemp));
   if (are_types(first, CT_PAREN_CLOSE, second, CT_TYPE) &&
       are_types(second->next, CT_DC_MEMBER, second->next->next, CT_FUNC_CALL))
   {
      log_rule("REMOVE_889_B");
      return(AV_REMOVE);
   }

#ifdef DEBUG
   // these lines are only useful for debugging uncrustify itself
   LOG_FMT(LSPACE, "\n\n(%d) %s: WARNING: unrecognized do_space: first: %zu:%zu %s %s and second: %zu:%zu %s %s\n\n\n",
           __LINE__, __func__, first->orig_line, first->orig_col, first->text(), get_token_name(first->type),
           second->orig_line, second->orig_col, second->text(), get_token_name(second->type));
#endif
   log_rule("ADD as default value");
   return(AV_ADD);
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

         /**
          * Apply a general safety check
          * If the two chunks combined will tokenize differently, then we
          * must force a space.
          * Two chunks -- "()" and "[]" will always tokenize differently.
          * They are always safe to not have a space after them.
          */
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
               bool kw1 = CharTable::IsKeyword2((size_t)(pc->str[pc->len()-1]));
               bool kw2 = CharTable::IsKeyword1((size_t)(next->str[0]));
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
                          is_type(pc,   CT_ANGLE_CLOSE) &&
                          is_type(next, CT_ANGLE_CLOSE) )
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
                 ((next->parent_type != CT_COMMENT_END  ) &&
                  (next->parent_type != CT_COMMENT_EMBED) ) ) &&
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

      if (is_str(first, "(", 1) && is_str(next, "(", 1))
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
      case AV_ADD:     /* fallthrough */
      case AV_FORCE:   return((size_t)(max(1, min_sp)));
      case AV_REMOVE:  return(0);
      case AV_IGNORE:  /* fallthrough */
      default:         return(second->orig_col > (first->orig_col + first->len()));
   }
}


size_t space_col_align(chunk_t *first, chunk_t *second)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(first, second));

   LOG_FMT(LSPACE, "%s: %zu:%zu [%s/%s] '%s' <==> %zu:%zu [%s/%s] '%s'",
           __func__, first->orig_line, first->orig_col,
           get_token_name(first->type), get_token_name(first->parent_type),
           first->text(), second->orig_line, second->orig_col,
           get_token_name(second->type), get_token_name(second->parent_type),
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


void space_add_after(chunk_t *pc, size_t count)
{
   LOG_FUNC_ENTRY();
   //if (count <= 0) { return; }

   chunk_t *next = chunk_get_next(pc);

   /* don't add at the end of the file or before a newline */
   if (is_invalid(next) ||
       chunk_is_newline(next) ) { return; }

   /* Limit to 16 spaces */
   if (count > 16) { count = 16; }

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
