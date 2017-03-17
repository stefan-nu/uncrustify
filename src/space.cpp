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



#define log_arg_return(a)                              \
   do {                                                \
      if (log_sev_on(LSPACE))                          \
      {log_rule2(__LINE__, (argval2str(a).c_str()),    \
                            pc1, pc2, complete); }     \
      return(a);                                       \
   } while(0)

#define log_opt_return(uo)                                       \
   do {                                                          \
      const option_map_value_t* my_uo = get_option_name(uo);     \
      if (log_sev_on(LSPACE))                                    \
      {log_rule2(__LINE__, (my_uo->name), pc1, pc2, complete); } \
      return(cpd.settings[uo].a);                                \
   } while(0)

#define log_rule(rule)                                   \
   do {                                                  \
      if (log_sev_on(LSPACE))                            \
      {log_rule2(__LINE__, (rule), pc1, pc2, complete);} \
   } while (0)


static void log_rule2(size_t line, const char *rule, chunk_t *pc1, chunk_t *pc2, bool complete)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(pc1, pc2));

   if (not_type(pc2, CT_NEWLINE))
   {
      LOG_FMT(LSPACE, "Spacing: line %zu [%s/%s] '%s' <===> [%s/%s] '%s' : %s[%zu]%s",
              pc1->orig_line, get_token_name(pc1->type),
              get_token_name(pc1->ptype), pc1->text(),
              get_token_name(pc2->type), get_token_name(pc2->ptype),
              pc2->text(), rule, line, complete ? "\n" : "");
   }
}

/** type that combines two chunk pointers
 * this is used to keep the function prototypes short */
typedef struct chunks_s
{
   chunk_t* a; /**< first  chunk */
   chunk_t* b; /**< second chunk */
}chunks_t;


/* various conditions to determine where to places spaces
 * the conditions have been placed in individual functions to use
 * them in arrays of functions which will make the code easier
 * and more efficient.  */

static bool sp_cond_0101(chunks_t* c) { return is_type(c->b, CT_FPAREN_OPEN) && is_ptype(c->a, CT_OPERATOR); }


static bool sp_cond_0069(chunks_t* c) { return is_type_and_ptype(c->a, CT_SQUARE_CLOSE, CT_CPP_LAMBDA) && is_type(c->b, CT_FPAREN_OPEN); }
static bool sp_cond_0023(chunks_t* c) { return is_type_and_ptype(c->a, CT_COLON,        CT_SQL_EXEC  ); }
static bool sp_cond_0025(chunks_t* c) { return is_type_and_ptype(c->a, CT_FPAREN_CLOSE, CT_MACRO_FUNC); }
static bool sp_cond_0041(chunks_t* c) { return is_type_and_ptype(c->b, CT_PAREN_OPEN,   CT_RETURN    ); }
static bool sp_cond_0054(chunks_t* c) { return is_type_and_ptype(c->a, CT_SQUARE_OPEN,  CT_TYPE      ); }
static bool sp_cond_0151(chunks_t* c) { return is_type_and_ptype(c->b, CT_PAREN_OPEN,   CT_INVARIANT ); }

static bool sp_cond_0008(chunks_t* c) { return is_only_first_type(c->a, CT_VBRACE_OPEN,  c->b, CT_NL_CONT); }
static bool sp_cond_0009(chunks_t* c) { return is_only_first_type(c->a, CT_VBRACE_CLOSE, c->b, CT_NL_CONT); }

static bool sp_cond_0140(chunks_t* c) { return any_is_type(c->a, CT_MEMBER,     c->b, CT_MEMBER     ); }
static bool sp_cond_0085(chunks_t* c) { return any_is_type(c->a, CT_ANGLE_OPEN, c->b, CT_ANGLE_CLOSE); }
static bool sp_cond_0067(chunks_t* c) { return any_is_type(c->a, c->b, CT_LAMBDA       ); }
static bool sp_cond_0001(chunks_t* c) { return any_is_type(c->a, c->b, CT_IGNORED      ); }
static bool sp_cond_0003(chunks_t* c) { return any_is_type(c->a, c->b, CT_PP           ); }
static bool sp_cond_0006(chunks_t* c) { return any_is_type(c->a, c->b, CT_SPACE        ); }
static bool sp_cond_0013(chunks_t* c) { return any_is_type(c->a, c->b, CT_D_ARRAY_COLON); }
static bool sp_cond_0018(chunks_t* c) { return any_is_type(c->a, c->b, CT_QUESTION     ); }
static bool sp_cond_0021(chunks_t* c) { return any_is_type(c->a, c->b, CT_COND_COLON   ); }
static bool sp_cond_0022(chunks_t* c) { return any_is_type(c->a, c->b, CT_RANGE        ); }
static bool sp_cond_0141(chunks_t* c) { return any_is_type(c->a, c->b, CT_D_TEMPLATE   ); }

static bool sp_cond_0084(chunks_t* c) { return are_types(c->a, c->b, CT_ANGLE_CLOSE); }
static bool sp_cond_0002(chunks_t* c) { return are_types(c->a, c->b, CT_PP_IGNORE  ); }

static bool sp_cond_0150(chunks_t* c) { return are_ptypes(c->a, c->b, CT_NONE); }

static bool sp_cond_0063(chunks_t* c) { return are_types(c->a, CT_CATCH,        c->b, CT_SPAREN_OPEN); }
static bool sp_cond_0064(chunks_t* c) { return are_types(c->a, CT_D_VERSION_IF, c->b, CT_SPAREN_OPEN); }
static bool sp_cond_0065(chunks_t* c) { return are_types(c->a, CT_D_SCOPE_IF,   c->b, CT_SPAREN_OPEN); }
static bool sp_cond_0017(chunks_t* c) { return are_types(c->a, CT_QUESTION,     c->b, CT_COND_COLON ); }
static bool sp_cond_0129(chunks_t* c) { return are_types(c->a, CT_PP_DEFINED,   c->b, CT_PAREN_OPEN ); }
static bool sp_cond_0132(chunks_t* c) { return are_types(c->a, CT_THIS,         c->b, CT_PAREN_OPEN ); }
static bool sp_cond_0133(chunks_t* c) { return are_types(c->a, CT_STATE,        c->b, CT_PAREN_OPEN ); }
static bool sp_cond_0134(chunks_t* c) { return are_types(c->a, CT_DELEGATE,     c->b, CT_PAREN_OPEN ); }
static bool sp_cond_0112(chunks_t* c) { return are_types(c->a, CT_PAREN_CLOSE,  c->b, CT_WHEN       ); }
static bool sp_cond_0124(chunks_t* c) { return are_types(c->a, CT_BRACE_OPEN,   c->b, CT_BRACE_CLOSE); }
static bool sp_cond_0142(chunks_t* c) { return are_types(c->a, CT_ELSE,         c->b, CT_BRACE_OPEN ); }
static bool sp_cond_0143(chunks_t* c) { return are_types(c->a, CT_ELSE,         c->b, CT_ELSEIF     ); }
static bool sp_cond_0144(chunks_t* c) { return are_types(c->a, CT_CATCH,        c->b, CT_BRACE_OPEN ); }
static bool sp_cond_0145(chunks_t* c) { return are_types(c->a, CT_FINALLY,      c->b, CT_BRACE_OPEN ); }
static bool sp_cond_0146(chunks_t* c) { return are_types(c->a, CT_TRY,          c->b, CT_BRACE_OPEN ); }
static bool sp_cond_0147(chunks_t* c) { return are_types(c->a, CT_GETSET,       c->b, CT_BRACE_OPEN ); }
static bool sp_cond_0148(chunks_t* c) { return are_types(c->a, CT_WORD,         c->b, CT_BRACE_OPEN ); }
static bool sp_cond_0137(chunks_t* c) { return are_types(c->a, CT_SUPER,        c->b, CT_PAREN_OPEN ); }
static bool sp_cond_0138(chunks_t* c) { return are_types(c->a, CT_FPAREN_CLOSE, c->b, CT_BRACE_OPEN ); }
static bool sp_cond_0070(chunks_t* c) { return are_types(c->a, CT_ENUM,         c->b, CT_FPAREN_OPEN); }

static bool sp_cond_0045(chunks_t* c) { return are_types(c->a, CT_PAREN_CLOSE,  c->b, CT_DC_MEMBER) && is_type(c->b->next, CT_FUNC_CALL); }

static bool sp_cond_0059(chunks_t* c) { return is_type(c->a, CT_ELLIPSIS   ) &&  CharTable::IsKW1((size_t)(c->b->str[0])); }
static bool sp_cond_0014(chunks_t* c) { return is_type(c->a, CT_CASE       ) && (CharTable::IsKW1((size_t)(c->b->str[0])) || is_type(c->b, CT_NUMBER)); }
static bool sp_cond_0005(chunks_t* c) { return is_type(c->b, CT_POUND      ) && is_preproc(c->b) && not_ptype(c->a, CT_MACRO_FUNC); }
static bool sp_cond_0123(chunks_t* c) { return is_type(c->a, CT_CLASS      ) && not_flag(c->a, PCF_IN_OC_MSG); }
static bool sp_cond_0113(chunks_t* c) { return is_type(c->a, CT_PAREN_CLOSE) && is_type(c->b, CT_PAREN_OPEN, CT_FPAREN_OPEN); }
static bool sp_cond_0096(chunks_t* c) { return is_type(c->a, CT_BYREF      ) && CharTable::IsKW1((size_t)(c->b->str[0])); }
static bool sp_cond_0089(chunks_t* c) { return is_type(c->b, CT_WORD       ) || CharTable::IsKW1((size_t)(c->b->str[0])); }
static bool sp_cond_0117(chunks_t* c) { return is_type(c->a, CT_FUNC_PROTO ) || is_type_and_ptype(c->b, CT_FPAREN_OPEN, CT_FUNC_PROTO ); }

static bool sp_cond_0007(chunks_t* c) { return is_type(c->b, CT_NEWLINE,        CT_VBRACE_OPEN     ); }
static bool sp_cond_0031(chunks_t* c) { return is_type(c->a, CT_SPAREN_OPEN,    CT_SEMICOLON       ); }
static bool sp_cond_0155(chunks_t* c) { return is_type(c->b, CT_ARITH,          CT_CARET           ); }
static bool sp_cond_0111(chunks_t* c) { return is_type(c->a, CT_CPP_CAST,       CT_TYPE_WRAP       ); }
static bool sp_cond_0104(chunks_t* c) { return is_type(c->a, CT_FUNC_CALL,      CT_FUNC_CTOR_VAR   ); }
static bool sp_cond_0090(chunks_t* c) { return is_type(c->b, CT_FPAREN_OPEN,    CT_PAREN_OPEN      ); }
static bool sp_cond_0120(chunks_t* c) { return is_type(c->a, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO); }

static bool sp_cond_0004(chunks_t* c) { return is_type(c->a, CT_POUND         ); }
static bool sp_cond_0010(chunks_t* c) { return is_type(c->b, CT_VSEMICOLON    ); }
static bool sp_cond_0011(chunks_t* c) { return is_type(c->a, CT_MACRO_FUNC    ); }
static bool sp_cond_0012(chunks_t* c) { return is_type(c->b, CT_NL_CONT       ); }
static bool sp_cond_0015(chunks_t* c) { return is_type(c->a, CT_FOR_COLON     ); }
static bool sp_cond_0016(chunks_t* c) { return is_type(c->b, CT_FOR_COLON     ); }
static bool sp_cond_0019(chunks_t* c) { return is_type(c->b, CT_QUESTION      ); }
static bool sp_cond_0020(chunks_t* c) { return is_type(c->a, CT_QUESTION      ); }
static bool sp_cond_0024(chunks_t* c) { return is_type(c->a, CT_MACRO         ); }
static bool sp_cond_0026(chunks_t* c) { return is_type(c->a, CT_PREPROC       ); }
static bool sp_cond_0027(chunks_t* c) { return is_type(c->b, CT_COND_COLON    ); }
static bool sp_cond_0028(chunks_t* c) { return is_type(c->a, CT_COND_COLON    ); }
static bool sp_cond_0029(chunks_t* c) { return is_type(c->b, CT_SEMICOLON     ); }
static bool sp_cond_0032(chunks_t* c) { return is_type(c->a, CT_SPAREN_CLOSE  ); }
static bool sp_cond_0036(chunks_t* c) { return is_type(c->a, CT_SEMICOLON     ); }
static bool sp_cond_0038(chunks_t* c) { return is_type(c->b, CT_SPAREN_CLOSE  ); }
static bool sp_cond_0040(chunks_t* c) { return is_type(c->a, CT_RETURN        ); }
static bool sp_cond_0042(chunks_t* c) { return is_type(c->a, CT_SIZEOF        ); }
static bool sp_cond_0043(chunks_t* c) { return is_type(c->b, CT_PAREN_OPEN    ); }
static bool sp_cond_0044(chunks_t* c) { return is_type(c->a, CT_DC_MEMBER     ); }
static bool sp_cond_0046(chunks_t* c) { return is_type(c->b, CT_DC_MEMBER     ); }
static bool sp_cond_0048(chunks_t* c) { return is_type(c->a, CT_COMMA         ); }
static bool sp_cond_0053(chunks_t* c) { return is_type(c->b, CT_COMMA         ); }
static bool sp_cond_0055(chunks_t* c) { return is_type(c->a, CT_PAREN_OPEN    ); }
static bool sp_cond_0056(chunks_t* c) { return is_type(c->b, CT_ELLIPSIS      ); }
static bool sp_cond_0050(chunks_t* c) { return is_type(c->b, CT_COMMA         ); }
static bool sp_cond_0058(chunks_t* c) { return is_type(c->a, CT_TAG_COLON     ); }
static bool sp_cond_0060(chunks_t* c) { return is_type(c->a, CT_TAG_COLON     ); }
static bool sp_cond_0061(chunks_t* c) { return is_type(c->b, CT_TAG_COLON     ); }
static bool sp_cond_0062(chunks_t* c) { return is_type(c->a, CT_DESTRUCTOR    ); }
static bool sp_cond_0066(chunks_t* c) { return is_type(c->b, CT_SPAREN_OPEN   ); }
static bool sp_cond_0071(chunks_t* c) { return is_type(c->b, CT_ASSIGN        ); }
static bool sp_cond_0074(chunks_t* c) { return is_type(c->a, CT_ASSIGN        ); }
static bool sp_cond_0079(chunks_t* c) { return is_type(c->b, CT_OC_BLOCK_CARET); }
static bool sp_cond_0080(chunks_t* c) { return is_type(c->a, CT_OC_BLOCK_CARET); }
static bool sp_cond_0081(chunks_t* c) { return is_type(c->b, CT_OC_MSG_FUNC   ); }
static bool sp_cond_0083(chunks_t* c) { return is_type(c->b, CT_TSQUARE       ); }
static bool sp_cond_0086(chunks_t* c) { return is_type(c->b, CT_ANGLE_OPEN    ); }
static bool sp_cond_0087(chunks_t* c) { return is_type(c->a, CT_TEMPLATE      ); }
static bool sp_cond_0088(chunks_t* c) { return is_type(c->a, CT_ANGLE_CLOSE   ); }
static bool sp_cond_0092(chunks_t* c) { return is_type(c->b, CT_DC_MEMBER     ); }
static bool sp_cond_0094(chunks_t* c) { return is_type(c->a, CT_BYREF         ); }
static bool sp_cond_0097(chunks_t* c) { return is_type(c->b, CT_BYREF         ); }
static bool sp_cond_0098(chunks_t* c) { return is_type(c->a, CT_SPAREN_CLOSE  ); }
static bool sp_cond_0099(chunks_t* c) { return is_type(c->b, CT_BRACE_OPEN    ); }
static bool sp_cond_0102(chunks_t* c) { return is_type(c->b, CT_FPAREN_OPEN   ); }
static bool sp_cond_0105(chunks_t* c) { return is_type(c->a, CT_FUNC_CALL_USER); }
static bool sp_cond_0106(chunks_t* c) { return is_type(c->a, CT_ATTRIBUTE     ); }
static bool sp_cond_0107(chunks_t* c) { return is_type(c->a, CT_FUNC_DEF      ); }
static bool sp_cond_0108(chunks_t* c) { return is_type(c->b, CT_FPAREN_OPEN   ); }
static bool sp_cond_0110(chunks_t* c) { return is_type(c->b, CT_FPAREN_OPEN   ); }
static bool sp_cond_0121(chunks_t* c) { return is_type(c->b, CT_FPAREN_OPEN   ); }
static bool sp_cond_0152(chunks_t* c) { return is_type(c->a, CT_PAREN_CLOSE   ); }
static bool sp_cond_0156(chunks_t* c) { return is_type(c->b, CT_BRACE_OPEN    ); }
static bool sp_cond_0130(chunks_t* c) { return is_type(c->a, CT_THROW         ); }
static bool sp_cond_0131(chunks_t* c) { return is_type(c->b, CT_PAREN_OPEN    ); }
static bool sp_cond_0136(chunks_t* c) { return is_type(c->a, CT_C99_MEMBER    ); }
static bool sp_cond_0125(chunks_t* c) { return is_type(c->b, CT_BRACE_CLOSE   ); }
static bool sp_cond_0128(chunks_t* c) { return is_type(c->a, CT_D_CAST        ); }
static bool sp_cond_0115(chunks_t* c) { return is_type(c->a, CT_TPAREN_CLOSE  ); }
static bool sp_cond_0118(chunks_t* c) { return is_type(c->b, CT_FPAREN_OPEN   ); }

static bool sp_cond_0033(chunks_t* c) { return is_type(c->b, CT_COMMENT     ) && is_type(c->a, CT_PP_ELSE,      CT_PP_ENDIF); }
static bool sp_cond_0051(chunks_t* c) { return is_type(c->a, CT_FPAREN_CLOSE) && is_type(c->b, CT_FPAREN_CLOSE, CT_COMMA   ); }
static bool sp_cond_0039(chunks_t* c) { return is_type(c->a, CT_NEG,  CT_POS,  CT_ARITH) && is_type(c->b, CT_NEG, CT_POS, CT_ARITH); }
static bool sp_cond_0047(chunks_t* c) { return is_type(c->a, CT_WORD, CT_TYPE, CT_PAREN_CLOSE) || CharTable::IsKW1((size_t)(c->a->str[0])); }
static bool sp_cond_0052(chunks_t* c) { return is_type(c->a, 22, CT_SBOOL, CT_SARITH,  CT_SASSIGN,  CT_CASE, CT_NEW,
      CT_QUALIFIER, CT_RETURN,   CT_DELETE, CT_ARITH, CT_ARITH, CT_FRIEND, CT_PRIVATE, CT_OPERATOR, CT_UNION,
      CT_NAMESPACE, CT_SCOMPARE, CT_SIZEOF, CT_CLASS, CT_USING, CT_STRUCT, CT_TYPEDEF, CT_TYPENAME, CT_THROW); }

static bool sp_cond_0095(chunks_t* c) { return is_ptype(c->a, CT_FUNC_DEF,    CT_FUNC_PROTO   ); }
static bool sp_cond_0034(chunks_t* c) { return is_ptype(c->b, CT_COMMENT_END, CT_COMMENT_EMBED); }
static bool sp_cond_0127(chunks_t* c) { return is_ptype(c->b, CT_STRUCT,      CT_UNION        ); }
static bool sp_cond_0114(chunks_t* c) { return is_ptype(c->a, CT_C_CAST,      CT_D_CAST       ); }

static bool sp_cond_0139(chunks_t* c) { return is_ptype(c->b, CT_DOUBLE_BRACE); }
static bool sp_cond_0126(chunks_t* c) { return is_ptype(c->b, CT_ENUM        ); }
static bool sp_cond_0035(chunks_t* c) { return is_ptype(c->b, CT_COMMENT_END ); }
static bool sp_cond_0030(chunks_t* c) { return is_ptype(c->b, CT_FOR         ); }
static bool sp_cond_0037(chunks_t* c) { return is_ptype(c->a, CT_FOR         ); }
static bool sp_cond_0049(chunks_t* c) { return is_ptype(c->a, CT_TYPE        ); }
static bool sp_cond_0149(chunks_t* c) { return is_ptype(c->a, CT_NAMESPACE   ); }
static bool sp_cond_0153(chunks_t* c) { return is_ptype(c->a, CT_D_TEMPLATE  ); }
static bool sp_cond_0154(chunks_t* c) { return is_ptype(c->a, CT_INVARIANT   ); }
static bool sp_cond_0157(chunks_t* c) { return is_ptype(c->a, CT_DELEGATE    ); }
static bool sp_cond_0158(chunks_t* c) { return is_ptype(c->a, CT_STATE       ); }
static bool sp_cond_0159(chunks_t* c) { return is_ptype(c->a, CT_NEW         ); }
static bool sp_cond_0073(chunks_t* c) { return is_ptype(c->b, CT_FUNC_PROTO  ); }
static bool sp_cond_0076(chunks_t* c) { return is_ptype(c->a, CT_FUNC_PROTO  ); }

static bool sp_cond_0075(chunks_t* c) { return is_flag(c->a, PCF_IN_ENUM); }
static bool sp_cond_0072(chunks_t* c) { return is_flag(c->b, PCF_IN_ENUM); }

static bool sp_cond_0057(chunks_t* c) { return not_flag(c->a, PCF_PUNCTUATOR); }
static bool sp_cond_0093(chunks_t* c) { return not_type(c->b, CT_BYREF, CT_PTR_TYPE); }

static bool sp_cond_0068(chunks_t* c) { return ((is_type_and_ptype(c->a, CT_SQUARE_OPEN,  CT_CPP_LAMBDA) && is_type(c->b, CT_ASSIGN)) ||
                                                (is_type_and_ptype(c->b, CT_SQUARE_CLOSE, CT_CPP_LAMBDA) && is_type(c->a, CT_ASSIGN)) ); }


static bool sp_cond_0077(chunks_t* c) { return is_type(c->a, CT_BIT_COLON  ) && is_flag(c->a, PCF_IN_ENUM); }
static bool sp_cond_0078(chunks_t* c) { return is_type(c->b, CT_BIT_COLON  ) && is_flag(c->b, PCF_IN_ENUM); }
static bool sp_cond_0082(chunks_t* c) { return is_type(c->b, CT_SQUARE_OPEN) && (c->b->ptype != CT_OC_MSG); }


static bool sp_cond_0100(chunks_t* c) { return !is_cmt(c->b); }

static bool sp_cond_0103(chunks_t* c) { return is_type(get_next_ncnl(c->b), CT_FPAREN_CLOSE); }
static bool sp_cond_0122(chunks_t* c) { return is_type(get_next_ncnl(c->b), CT_FPAREN_CLOSE); }
static bool sp_cond_0119(chunks_t* c) { return is_type(get_next_ncnl(c->b), CT_FPAREN_CLOSE); }
static bool sp_cond_0109(chunks_t* c) { return is_type(get_next_ncnl(c->b), CT_FPAREN_CLOSE); }
static bool sp_cond_0135(chunks_t* c) { return is_type(get_next_ncnl(c->b), CT_FPAREN_CLOSE); }

static bool sp_cond_0116(chunks_t* c) { return (is_str(c->a, ")", 1) && is_str(c->b, "(", 1)) ||
                                               (is_paren_close(c->a) && is_paren_open(c->b)); }

static bool sp_cond_0091(chunks_t* c) { return any_is_type(c->a, CT_FPAREN_OPEN, c->b, CT_FPAREN_CLOSE); }
static bool sp_cond_0160(chunks_t* c) { return are_types  (c->a, CT_FPAREN_OPEN, c->b, CT_FPAREN_CLOSE); }
static bool sp_cond_0161(chunks_t* c) { return any_is_type(c->a, CT_TPAREN_OPEN, c->b, CT_TPAREN_CLOSE); }
static bool sp_cond_0162(chunks_t* c) { return is_type(c->a, CT_PAREN_CLOSE); }
static bool sp_cond_0163(chunks_t* c) { return is_flag(c->a, PCF_OC_RTYPE) && is_ptype(c->a, CT_OC_MSG_DECL, CT_OC_MSG_SPEC); }
static bool sp_cond_0164(chunks_t* c) { return is_ptype(c->a, CT_OC_MSG_SPEC, CT_OC_MSG_DECL); }
static bool sp_cond_0165(chunks_t* c) { return is_ptype(c->a, CT_OC_SEL) && not_type(c->b, CT_SQUARE_CLOSE); }
static bool sp_cond_0166(chunks_t* c) { return ((is_type(c->a, CT_PAREN_OPEN ) && is_ptype(c->a, CT_OC_SEL, CT_OC_PROTOCOL)) ||
                                                (is_type(c->b, CT_PAREN_CLOSE) && is_ptype(c->b, CT_OC_SEL, CT_OC_PROTOCOL)) ); }

static bool sp_cond_0167(chunks_t* c) { return is_type(c->b, CT_PAREN_OPEN) && is_type(c->a, CT_OC_SEL, CT_OC_PROTOCOL); }
static bool sp_cond_0168(chunks_t* c) { return is_type(c->a, CT_PAREN_OPEN); }
static bool sp_cond_0169(chunks_t* c) { return is_ptype(c->a, CT_C_CAST, CT_CPP_CAST, CT_D_CAST); }
static bool sp_cond_0170(chunks_t* c) { return is_ptype(c->a, CT_NEW); }
static bool sp_cond_0171(chunks_t* c) { return is_type(c->b, CT_PAREN_CLOSE); }
static bool sp_cond_0172(chunks_t* c) { return is_ptype(c->b, CT_C_CAST, CT_CPP_CAST, CT_D_CAST); }
static bool sp_cond_0173(chunks_t* c) { return is_ptype(c->b, CT_NEW); }
static bool sp_cond_0174(chunks_t* c) { return any_is_type(c->a, CT_SQUARE_OPEN, c->b, CT_SQUARE_CLOSE); }
static bool sp_cond_0175(chunks_t* c) { return are_types(c->a, CT_SQUARE_CLOSE, c->b, CT_FPAREN_OPEN); }
static bool sp_cond_0176(chunks_t* c) { return is_type(c->b, CT_SPAREN_CLOSE); }
static bool sp_cond_0177(chunks_t* c) { return is_type(c->a, CT_SPAREN_OPEN); }
static bool sp_cond_0178(chunks_t* c) { return any_is_type(c->a, CT_SPAREN_OPEN, c->b, CT_SPAREN_CLOSE); }
static bool sp_cond_0179(chunks_t* c) { return is_type(c->a, CT_CLASS_COLON); }
static bool sp_cond_0180(chunks_t* c) { return is_type(c->b, CT_CLASS_COLON); }
static bool sp_cond_0181(chunks_t* c) { return is_type(c->a, CT_CONSTR_COLON); }
static bool sp_cond_0182(chunks_t* c) { return is_type(c->b, CT_CONSTR_COLON); }
static bool sp_cond_0183(chunks_t* c) { return is_type(c->b, CT_CASE_COLON); }
static bool sp_cond_0184(chunks_t* c) { return is_type(c->a, CT_DOT); }
static bool sp_cond_0185(chunks_t* c) { return is_type(c->b, CT_DOT); }
static bool sp_cond_0186(chunks_t* c) { return any_is_type(c->a, c->b, CT_NULLCOND); }
static bool sp_cond_0187(chunks_t* c) { return is_type(c->a, CT_ARITH, CT_CARET) || is_type(c->b, CT_ARITH, CT_CARET) ; }
static bool sp_cond_0188(chunks_t* c) { return any_is_type(c->a, c->b, CT_BOOL); }
static bool sp_cond_0189(chunks_t* c) { return false; }



/* \todo make min_sp a size_t */
/* Note that the order of the if statements is VERY important. */
static argval_t do_space(chunk_t *pc1, chunk_t *pc2, int &min_sp, bool complete = true)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(pc1, pc2));

   chunks_t chunks = {pc1, pc2};
   chunks_t* pc = &chunks;

   LOG_FMT(LSPACE, "%s: %zu:%zu %s %s\n", __func__, pc1->orig_line,
         pc1->orig_col, pc1->text(), get_token_name(pc1->type));

   min_sp = 1;

   if(sp_cond_0001(pc)) { log_arg_return(AV_REMOVE                ); }
   if(sp_cond_0002(pc)) { log_arg_return(AV_IGNORE                ); } /* Leave spacing alone between PP_IGNORE tokens as we don't want the default behavior (which is ADD) */
   if(sp_cond_0003(pc)) { log_opt_return(UO_sp_pp_concat          ); }
   if(sp_cond_0004(pc)) { log_opt_return(UO_sp_pp_stringify       ); }
   if(sp_cond_0005(pc)) { log_opt_return(UO_sp_before_pp_stringify); }
   if(sp_cond_0006(pc)) { log_arg_return(AV_REMOVE                ); }
   if(sp_cond_0007(pc)) { log_arg_return(AV_REMOVE                ); }
   if(sp_cond_0008(pc)) { log_arg_return(AV_FORCE                 ); }
   if(sp_cond_0009(pc)) { log_arg_return(AV_REMOVE                ); }
   if(sp_cond_0010(pc)) { log_arg_return(AV_REMOVE                ); }
   if(sp_cond_0011(pc)) { log_arg_return(AV_REMOVE                ); }
   if(sp_cond_0012(pc)) { log_opt_return(UO_sp_before_nl_cont     ); }
   if(sp_cond_0013(pc)) { log_opt_return(UO_sp_d_array_colon      ); }
   if(sp_cond_0014(pc)) { log_rule("sp_case_label"); return(add_option(cpd.settings[UO_sp_case_label].a, AV_ADD)); }
   if(sp_cond_0015(pc)) { log_opt_return(UO_sp_after_for_colon    ); }
   if(sp_cond_0016(pc)) { log_opt_return(UO_sp_before_for_colon   ); }
   if(sp_cond_0017(pc)) {
      if (is_not_option(cpd.settings[UO_sp_cond_ternary_short  ].a, AV_IGNORE)) { log_opt_return(UO_sp_cond_ternary_short  ); }
   }
   if(sp_cond_0018(pc)) {
      if(sp_cond_0019(pc) && is_not_option(cpd.settings[UO_sp_cond_question_before].a, AV_IGNORE)) { log_opt_return(UO_sp_cond_question_before); }
      if(sp_cond_0020(pc) && is_not_option(cpd.settings[UO_sp_cond_question_after ].a, AV_IGNORE)) { log_opt_return(UO_sp_cond_question_after ); }
      if(                    is_not_option(cpd.settings[UO_sp_cond_question       ].a, AV_IGNORE)) { log_opt_return(UO_sp_cond_question       ); }
   }
   if(sp_cond_0021(pc))
   {  if(sp_cond_0027(pc) && is_not_option(cpd.settings[UO_sp_cond_colon_before   ].a, AV_IGNORE)) { log_opt_return(UO_sp_cond_colon_before   ); }
      if(sp_cond_0028(pc) && is_not_option(cpd.settings[UO_sp_cond_colon_after    ].a, AV_IGNORE)) { log_opt_return(UO_sp_cond_colon_after    ); }
      if(                    is_not_option(cpd.settings[UO_sp_cond_colon          ].a, AV_IGNORE)) { log_opt_return(UO_sp_cond_colon          ); }
   }
   if(sp_cond_0022(pc)) { log_opt_return(UO_sp_range); }
   if(sp_cond_0023(pc)) { log_arg_return(AV_REMOVE  ); }
   /* Macro stuff can only return IGNORE, ADD, or FORCE */
   if(sp_cond_0024(pc)) { log_rule("sp_macro"     ); argval_t arg = cpd.settings[UO_sp_macro     ].a; argval_t add_arg = is_not_option(arg, AV_IGNORE) ? AV_ADD : AV_IGNORE; return (add_option(arg, add_arg)); }
   if(sp_cond_0025(pc)) { log_rule("sp_macro_func"); argval_t arg = cpd.settings[UO_sp_macro_func].a; argval_t add_arg = is_not_option(arg, AV_IGNORE) ? AV_ADD : AV_IGNORE; return (add_option(arg, add_arg)); }
   if(sp_cond_0026(pc)) {
      if (cpd.settings[UO_pp_space].a == AV_IGNORE) { log_arg_return(AV_IGNORE); } log_arg_return(AV_REMOVE); /* Remove spaces, unless we are ignoring. See indent_preproc() */
   }
   if (sp_cond_0029(pc)) {
      if (sp_cond_0030(pc)) {
         if (is_not_option(cpd.settings[UO_sp_before_semi_for_empty].a, AV_IGNORE) && sp_cond_0031(pc)) { log_opt_return(UO_sp_before_semi_for_empty); }
         if (is_not_option(cpd.settings[UO_sp_before_semi_for      ].a, AV_IGNORE)                    ) { log_opt_return(UO_sp_before_semi_for      ); }
      }
      argval_t arg = cpd.settings[UO_sp_before_semi].a;

      if (sp_cond_0032(pc) &&
#if 1
          (pc1->ptype != CT_WHILE_OF_DO ) )
#else
      // fails test 01011, 30711
          not_ptype(pc1, CT_WHILE_OF_DO ) )
#endif
      {
         log_rule("sp_before_semi|sp_special_semi");
         arg = (add_option(arg, cpd.settings[UO_sp_special_semi].a));
      }
      else { log_rule("sp_before_semi"); }
      return(arg);
   }

   if (sp_cond_0033(pc)) {
      if (is_not_option(cpd.settings[UO_sp_endif_cmt].a, AV_IGNORE)) {
         set_type(pc2, CT_COMMENT_ENDIF);
         log_opt_return(UO_sp_endif_cmt);
      }
   }

   if (is_not_option(cpd.settings[UO_sp_before_tr_emb_cmt].a, AV_IGNORE) && sp_cond_0034(pc)) {
      min_sp = cpd.settings[UO_sp_num_before_tr_emb_cmt].n;
      log_opt_return(UO_sp_before_tr_emb_cmt);
   }

   if (sp_cond_0035(pc)) {
      switch (pc2->orig_prev_sp) {
         case 0:  log_arg_return(AV_REMOVE); break;
         case 1:  log_arg_return(AV_FORCE ); break;
         default: log_arg_return(AV_ADD   ); break;
      }
   }
   /* "for (;;)" vs "for (;; )" and "for (a;b;c)" vs "for (a; b; c)" */
   if (sp_cond_0036(pc)) {
      if (sp_cond_0037(pc)) {
         if (is_not_option(cpd.settings[UO_sp_after_semi_for_empty].a, AV_IGNORE) && sp_cond_0038(pc)) { log_opt_return(UO_sp_after_semi_for_empty); }
         if (is_not_option(cpd.settings[UO_sp_after_semi_for      ].a, AV_IGNORE)                    ) { log_opt_return(UO_sp_after_semi_for      ); }
      }
      else if (!is_cmt(pc2) &&
               pc2->type != CT_BRACE_CLOSE) { log_opt_return(UO_sp_after_semi); }
      /* Let the comment spacing rules handle this */
   }
   if (sp_cond_0039(pc)) { log_arg_return(AV_ADD); } /* puts a space in the rare '+-' or '-+' */

   /* "return(a);" vs "return (foo_t)a + 3;" vs "return a;" vs "return;" */
   if (sp_cond_0040(pc)) {
      if (sp_cond_0041(pc)) { log_opt_return(UO_sp_return_paren); }
      else                  { log_arg_return(AV_FORCE          ); } /* everything else requires a space */
   }
   if(sp_cond_0042(pc)) { /* "sizeof(foo_t)" vs "sizeof foo_t" */
      if (sp_cond_0043(pc)) { log_opt_return(UO_sp_sizeof_paren); }
      else                  { log_arg_return(AV_FORCE          ); }
   }
   if(sp_cond_0044(pc)) { log_opt_return(UO_sp_after_dc); } /* handle '::' */
   if(sp_cond_0045(pc)) { log_arg_return(AV_REMOVE     ); } /* mapped_file_source abc((int) ::CW2A(sTemp)); */
   if(sp_cond_0046(pc)) {
      /* '::' at the start of an identifier is not member access,
       * but global scope operator. Detect if previous chunk is keyword */
      if(sp_cond_0052(pc)) { log_arg_return(AV_FORCE       ); }
      if(sp_cond_0047(pc)) { log_opt_return(UO_sp_before_dc); }
   }

   /* "a,b" vs "a, b" */
   if (sp_cond_0048(pc))
   {  if (sp_cond_0049(pc))
      {  /* C# multidimensional array type: ',,' vs ', ,' or ',]' vs ', ]' */
         if (sp_cond_0050(pc)) { log_opt_return(UO_sp_between_mdatype_commas); }
         else                  { log_opt_return(UO_sp_after_mdatype_commas  ); }
      }
      else                     { log_opt_return(UO_sp_after_comma           ); }
   }
   /* test if we are within a SIGNAL/SLOT call */
   if (QT_SIGNAL_SLOT_found) {
      if (sp_cond_0051(pc)) {
         if (pc2->level == (size_t)(QT_SIGNAL_SLOT_level)) {
            restoreValues = true;
         }
      }
   }
   if(sp_cond_0053(pc)) {
      if (sp_cond_0054(pc)) { log_opt_return(UO_sp_before_mdatype_commas); }
      if (sp_cond_0055(pc) && (cpd.settings[UO_sp_paren_comma].a != AV_IGNORE))
                                         { log_opt_return(UO_sp_paren_comma); }
                                           log_opt_return(UO_sp_before_comma); }
   if(sp_cond_0056(pc))
   {  /* non-punc followed by a ellipsis */
      if (sp_cond_0057(pc) && is_not_option(cpd.settings[UO_sp_before_ellipsis].a, AV_IGNORE))
                                         { log_opt_return(UO_sp_before_ellipsis); }
      if (sp_cond_0058(pc))              { log_arg_return(AV_FORCE             ); }
   }
   if(sp_cond_0059(pc)) { log_arg_return(AV_FORCE       ); }
   if(sp_cond_0060(pc)) { log_opt_return(UO_sp_after_tag); }
   if(sp_cond_0061(pc)) { log_arg_return(AV_REMOVE      ); }
   if(sp_cond_0062(pc)) { log_arg_return(AV_REMOVE      ); }   /* handle '~' */
   if(sp_cond_0063(pc) && (is_not_option(cpd.settings[UO_sp_catch_paren  ].a, AV_IGNORE))) { log_opt_return(UO_sp_catch_paren  ); }
   if(sp_cond_0064(pc) && (is_not_option(cpd.settings[UO_sp_version_paren].a, AV_IGNORE))) { log_opt_return(UO_sp_version_paren); }
   if(sp_cond_0065(pc) && (is_not_option(cpd.settings[UO_sp_scope_paren  ].a, AV_IGNORE))) { log_opt_return(UO_sp_scope_paren  ); }
   if(sp_cond_0066(pc)                                                          ) { log_opt_return(UO_sp_before_sparen    ); } /* "if (" vs "if(" */
   if(sp_cond_0067(pc)                                                          ) { log_opt_return(UO_sp_assign           ); }
   if(sp_cond_0068(pc) && (cpd.settings[UO_sp_cpp_lambda_assign].a != AV_IGNORE)) { log_opt_return(UO_sp_cpp_lambda_assign); } /* Handle the special lambda case for C++11: [=](Something arg){.....} */
   if(sp_cond_0069(pc) && (cpd.settings[UO_sp_cpp_lambda_paren ].a != AV_IGNORE)) { log_opt_return(UO_sp_cpp_lambda_paren ); } /* Handle the special lambda case for C++11: [](Something arg){.....} */
   if(sp_cond_0070(pc)) {
      if (is_not_option(cpd.settings[UO_sp_enum_paren].a, AV_IGNORE)) { log_opt_return(UO_sp_enum_paren); }
   }
   if (sp_cond_0071(pc)) {
      if (sp_cond_0072(pc)) {
         if (is_not_option(cpd.settings[UO_sp_enum_before_assign].a, AV_IGNORE))
                                        { log_opt_return(UO_sp_enum_before_assign); }
         else                           { log_opt_return(UO_sp_enum_assign       ); }
      }
      if(is_not_option(cpd.settings[UO_sp_assign_default].a, AV_IGNORE) && sp_cond_0073(pc)) { log_opt_return(UO_sp_assign_default); }
      if(is_not_option(cpd.settings[UO_sp_before_assign ].a, AV_IGNORE))
                                        { log_opt_return(UO_sp_before_assign     ); }
      else                              { log_opt_return(UO_sp_assign            ); } }
   if (sp_cond_0074(pc)) {
      if (sp_cond_0075(pc)) {
         if (is_not_option(cpd.settings[UO_sp_enum_after_assign].a, AV_IGNORE))
                                        { log_opt_return(UO_sp_enum_after_assign); }
         else                           { log_opt_return(UO_sp_enum_assign); } }
      if ((is_not_option(cpd.settings[UO_sp_assign_default].a, AV_IGNORE)) &&
            sp_cond_0076(pc)) { log_opt_return(UO_sp_assign_default); }
      if (is_not_option(cpd.settings[UO_sp_after_assign].a, AV_IGNORE))
                                        { log_opt_return(UO_sp_after_assign); }
      else                              { log_opt_return(UO_sp_assign); } }
   if(sp_cond_0077(pc)) { log_opt_return(UO_sp_enum_colon); }
   if(sp_cond_0078(pc)) { log_opt_return(UO_sp_enum_colon); }
   if(sp_cond_0079(pc)) { log_opt_return(UO_sp_before_oc_block_caret); }
   if(sp_cond_0080(pc)) { log_opt_return(UO_sp_after_oc_block_caret ); }
   if(sp_cond_0081(pc)) { log_opt_return(UO_sp_after_oc_msg_receiver); }
   if(sp_cond_0082(pc)) { log_opt_return(UO_sp_before_square); }  /* "a [x]" vs "a[x]" */
   if(sp_cond_0083(pc)) { log_opt_return(UO_sp_before_squares); }    /* "byte[]" vs "byte []" */
   if(sp_cond_0084(pc) && (cpd.settings[UO_sp_angle_shift].a != AV_IGNORE)) { log_opt_return(UO_sp_angle_shift); }
   if(sp_cond_0085(pc)) { log_opt_return(UO_sp_inside_angle); }  /* spacing around template < > stuff */
   if(sp_cond_0086(pc)) {
      if(sp_cond_0087(pc) && is_not_option(cpd.settings[UO_sp_template_angle].a, AV_IGNORE))
                                        { log_opt_return(UO_sp_template_angle); }
      else                              { log_opt_return(UO_sp_before_angle); }
   }
   if(sp_cond_0088(pc)) {
      if(sp_cond_0089(pc)) {
         if (is_not_option(cpd.settings[UO_sp_angle_word].a, AV_IGNORE)) { log_opt_return(UO_sp_angle_word); }
      }
      if (sp_cond_0090(pc)) {
         chunk_t *next = get_next_ncnl(pc2);
         if (is_type(next, CT_FPAREN_CLOSE))
                                        { log_opt_return(UO_sp_angle_paren_empty); }
         else                           { log_opt_return(UO_sp_angle_paren      ); }
      }
      if(sp_cond_0092(pc)) { log_opt_return(UO_sp_before_dc  ); }
      if(sp_cond_0093(pc)) { log_opt_return(UO_sp_after_angle); }
   }
   if(sp_cond_0094(pc) && sp_cond_0095(pc) && is_not_option(cpd.settings[UO_sp_after_byref_func].a, AV_IGNORE)) { log_opt_return(UO_sp_after_byref_func); }
   if(sp_cond_0096(pc)) { log_opt_return(UO_sp_after_byref     ); }
   if(sp_cond_0097(pc)) {
      if (is_not_option(cpd.settings[UO_sp_before_byref_func].a, AV_IGNORE))
      {
         chunk_t* next = chunk_get_next(pc2);
         if (is_type(next, CT_FUNC_DEF, CT_FUNC_PROTO)) { return(cpd.settings[UO_sp_before_byref_func].a); }
      }

      if(is_not_option(cpd.settings[UO_sp_before_unnamed_byref].a, AV_IGNORE))
      {
         chunk_t* next = get_next_nc(pc2);
         if (not_type(next, CT_WORD)) { log_opt_return(UO_sp_before_unnamed_byref); }
      }
      log_opt_return(UO_sp_before_byref);
   }

   if(sp_cond_0098(pc))
   {
      if (sp_cond_0099(pc) && (cpd.settings[UO_sp_sparen_brace].a != AV_IGNORE)) { log_opt_return(UO_sp_sparen_brace); }
      if (sp_cond_0100(pc) && (cpd.settings[UO_sp_after_sparen].a != AV_IGNORE)) { log_opt_return(UO_sp_after_sparen); }
   }

   if(sp_cond_0101(pc) && is_not_option(cpd.settings[UO_sp_after_operator_sym].a, AV_IGNORE)) {
      if(sp_cond_0102(pc) && is_not_option(cpd.settings[UO_sp_after_operator_sym_empty].a, AV_IGNORE)) {
         if(sp_cond_0135(pc)) { log_opt_return(UO_sp_after_operator_sym_empty); }
      }
      log_opt_return(UO_sp_after_operator_sym);
   }

   /* spaces between function and open paren */
   if(sp_cond_0104(pc)) {
      if(sp_cond_0110(pc) && is_not_option(cpd.settings[UO_sp_func_call_paren_empty].a, AV_IGNORE)) {
         if(sp_cond_0103(pc)) { log_opt_return(UO_sp_func_call_paren_empty); }
      }
      log_opt_return(UO_sp_func_call_paren);
   }
   if(sp_cond_0105(pc)) { log_opt_return(UO_sp_func_call_user_paren); }
   if(sp_cond_0106(pc)) { log_opt_return(UO_sp_attribute_paren     ); }
   if(sp_cond_0107(pc)) {
      if(sp_cond_0108(pc) && is_not_option(cpd.settings[UO_sp_func_def_paren_empty].a, AV_IGNORE)) {
         if(sp_cond_0109(pc)) { log_opt_return(UO_sp_func_def_paren_empty); }
      }
      log_rule("sp_func_def_paren");
      return(cpd.settings[UO_sp_func_def_paren].a);
   }
   if(sp_cond_0111(pc)) { log_opt_return(UO_sp_cpp_cast_paren); }
   if(sp_cond_0112(pc)) { log_arg_return(AV_FORCE            ); } /* TODO: make this configurable? */
   if(sp_cond_0113(pc)) {
      if(sp_cond_0114(pc)) { log_opt_return(UO_sp_after_cast); } /* "(int)a" vs "(int) a" or "cast(int)a" vs "cast(int) a" */

      /* Must be an indirect/chained function call? */
      log_arg_return(AV_REMOVE); /* TODO: make this configurable? */
   }
   if(sp_cond_0115(pc)) { log_opt_return(UO_sp_after_tparen_close); } /* handle the space between parens in fcn type 'void (*f)(void)' */
   if(sp_cond_0116(pc)) { log_opt_return(UO_sp_cparen_oparen     ); } /* ")(" vs ") (" */

   if (sp_cond_0117(pc)) {
      if (sp_cond_0118(pc) && is_not_option(cpd.settings[UO_sp_func_proto_paren_empty].a, AV_IGNORE)) {
         if (sp_cond_0119(pc)) { log_opt_return(UO_sp_func_proto_paren_empty ); }
      }
                               { log_opt_return(UO_sp_func_proto_paren       ); }
   }
   if(sp_cond_0120(pc)) {
      if(sp_cond_0121(pc) && is_not_option(cpd.settings[UO_sp_func_class_paren_empty].a, AV_IGNORE)) {
         if(sp_cond_0122(pc)) { log_opt_return(UO_sp_func_class_paren_empty ); }
      }
                                log_opt_return(UO_sp_func_class_paren       );
   }
   if(sp_cond_0123(pc)) { log_arg_return(AV_FORCE                     ); }
   if(sp_cond_0124(pc)) { log_opt_return(UO_sp_inside_braces_empty    ); }
   if(sp_cond_0125(pc))
   {  if(sp_cond_0126(pc)) { log_opt_return(UO_sp_inside_braces_enum     ); }
      if(sp_cond_0127(pc)) { log_opt_return(UO_sp_inside_braces_struct   ); }
      else                 { log_opt_return(UO_sp_inside_braces          ); }
   }
   if(sp_cond_0128(pc)) { log_arg_return(AV_REMOVE                    ); }
   if(sp_cond_0129(pc)) { log_opt_return(UO_sp_defined_paren          ); }
   if(sp_cond_0130(pc)) {
      if(sp_cond_0131(pc)) { log_opt_return(UO_sp_throw_paren          ); }
                             log_opt_return(UO_sp_after_throw          );
   }
   if(sp_cond_0132(pc)) { log_opt_return(UO_sp_this_paren             ); }
   if(sp_cond_0133(pc)) { log_arg_return(AV_ADD                       ); }
   if(sp_cond_0134(pc)) { log_arg_return(AV_REMOVE                    ); }
   if(sp_cond_0140(pc)) { log_opt_return(UO_sp_member                 ); }

   // always remove space(s) after then '.' of a C99-member
   if(sp_cond_0136(pc)) { log_arg_return(AV_REMOVE                    ); }
   if(sp_cond_0137(pc)) { log_opt_return(UO_sp_super_paren            ); }
   if(sp_cond_0138(pc)) {
      if(sp_cond_0139(pc)) { log_opt_return(UO_sp_fparen_dbrace          ); }
      log_rule("sp_fparen_brace");
      return(cpd.settings[UO_sp_fparen_brace].a); }
   if(sp_cond_0141(pc)) { log_arg_return(AV_REMOVE                    ); }
   if(sp_cond_0142(pc)) { log_opt_return(UO_sp_else_brace             ); }
   if(sp_cond_0143(pc)) { log_arg_return(AV_FORCE                     ); }
   if(sp_cond_0144(pc)) { log_opt_return(UO_sp_catch_brace            ); }
   if(sp_cond_0145(pc)) { log_opt_return(UO_sp_finally_brace          ); }
   if(sp_cond_0146(pc)) { log_opt_return(UO_sp_try_brace              ); }
   if(sp_cond_0147(pc)) { log_opt_return(UO_sp_getset_brace           ); }
   if(sp_cond_0148(pc)) {
      if(sp_cond_0149(pc)) { log_opt_return(UO_sp_word_brace_ns          ); }
      if(sp_cond_0150(pc)) { log_opt_return(UO_sp_word_brace             ); }
   }
   if(sp_cond_0151(pc)) { log_opt_return(UO_sp_invariant_paren        ); }
   if(sp_cond_0152(pc)) {
      if(sp_cond_0153(pc)) { log_arg_return(AV_FORCE                     ); }
      if(sp_cond_0154(pc)) { log_opt_return(UO_sp_after_invariant_paren  ); }
      if(sp_cond_0155(pc)) { log_opt_return(UO_sp_arith                  ); } /* Arith after a cast comes first */
      if(sp_cond_0156(pc)) { log_opt_return(UO_sp_paren_brace            ); } /* "(struct foo) {...}" vs "(struct foo){...}" */
      if(sp_cond_0157(pc)) { log_arg_return(AV_ADD                       ); } /* D-specific: "delegate(some thing) dg */
      if(sp_cond_0158(pc)) { log_arg_return(AV_ADD                       ); } /* PAWN-specific: "state (condition) next" */
      if(sp_cond_0159(pc)) { log_opt_return(UO_sp_after_newop_paren      ); } /* C++ new operator: new(bar) Foo */
   }

   if(sp_cond_0091(pc)) { /* "foo(...)" vs "foo( ... )" */
      if(sp_cond_0160(pc)) { log_opt_return(UO_sp_inside_fparens         ); }
                             log_opt_return(UO_sp_inside_fparen          ); }
   /* "foo(...)" vs "foo( ... )" */
   if(sp_cond_0161(pc)) { log_opt_return(UO_sp_inside_tparen          ); }
   if(sp_cond_0162(pc)) {
      if (sp_cond_0163(pc)) { log_opt_return(UO_sp_after_oc_return_type   ); }
      else if (sp_cond_0164(pc)) { log_opt_return(UO_sp_after_oc_type          ); }
      else if (sp_cond_0165(pc)) { log_opt_return(UO_sp_after_oc_at_sel_parens ); }
   }
   if (cpd.settings[UO_sp_inside_oc_at_sel_parens].a != AV_IGNORE) {
      if(sp_cond_0166(pc)) { log_opt_return(UO_sp_inside_oc_at_sel_parens); }
   }
   if(sp_cond_0167(pc)) { log_opt_return(UO_sp_after_oc_at_sel        ); }
   /* C cast:   "(int)"      vs "( int )"
    * D cast:   "cast(int)"  vs "cast( int )"
    * CPP cast: "int(a + 3)" vs "int( a + 3 )" */
   if (sp_cond_0168(pc)) {
      if (sp_cond_0169(pc)) { log_opt_return(UO_sp_inside_paren_cast  ); }
      if (sp_cond_0170(pc)) {
         if(cpd.settings[UO_sp_inside_newop_paren_open].a != AV_IGNORE) { log_opt_return(UO_sp_inside_newop_paren_open); }
         if(cpd.settings[UO_sp_inside_newop_paren     ].a != AV_IGNORE) { log_opt_return(UO_sp_inside_newop_paren     ); }
      }
                                              { log_opt_return(UO_sp_inside_paren       ); }
   }
   if (sp_cond_0171(pc)) {
      if(sp_cond_0172(pc)) { log_opt_return(UO_sp_inside_paren_cast  ); }
      if(sp_cond_0173(pc)) {
         if(cpd.settings[UO_sp_inside_newop_paren_close].a != AV_IGNORE) { log_opt_return(UO_sp_inside_newop_paren_close); }
         if(cpd.settings[UO_sp_inside_newop_paren      ].a != AV_IGNORE) { log_opt_return(UO_sp_inside_newop_paren      ); }
      }
                                                log_opt_return(UO_sp_inside_paren       );
   }
   /* "[3]" vs "[ 3 ]" */
   if(sp_cond_0174(pc)) { log_opt_return(UO_sp_inside_square      ); }
   if(sp_cond_0175(pc)) { log_opt_return(UO_sp_square_fparen      ); }
   /* "if(...)" vs "if( ... )" */
   if(sp_cond_0176(pc) && is_not_option(cpd.settings[UO_sp_inside_sparen_close].a, AV_IGNORE)) { log_opt_return(UO_sp_inside_sparen_close); }
   if(sp_cond_0177(pc) && is_not_option(cpd.settings[UO_sp_inside_sparen_open ].a, AV_IGNORE)) { log_opt_return(UO_sp_inside_sparen_open ); }
   if(sp_cond_0178(pc))                                                                        { log_opt_return(UO_sp_inside_sparen      ); }
   if(sp_cond_0179(pc) && is_not_option(cpd.settings[UO_sp_after_class_colon  ].a, AV_IGNORE)) { log_opt_return(UO_sp_after_class_colon  ); }
   if(sp_cond_0180(pc) && is_not_option(cpd.settings[UO_sp_before_class_colon ].a, AV_IGNORE)) { log_opt_return(UO_sp_before_class_colon ); }
   if(sp_cond_0181(pc) && is_not_option(cpd.settings[UO_sp_after_constr_colon ].a, AV_IGNORE))
   { min_sp = cpd.settings[UO_indent_ctor_init_leading].n - 1; // default indent is 1 space
     log_opt_return(UO_sp_after_constr_colon); }
   if(sp_cond_0182(pc) && is_not_option(cpd.settings[UO_sp_before_constr_colon].a, AV_IGNORE)) { log_opt_return(UO_sp_before_constr_colon); }
   if(sp_cond_0183(pc) && is_not_option(cpd.settings[UO_sp_before_case_colon  ].a, AV_IGNORE)) { log_opt_return(UO_sp_before_case_colon  ); }
   if(sp_cond_0184(pc)) { log_arg_return(AV_REMOVE                ); }
   if(sp_cond_0185(pc)) { log_arg_return(AV_ADD                   ); }
   if(sp_cond_0186(pc)) { log_opt_return(UO_sp_member             ); }
   if(sp_cond_0187(pc)) { log_opt_return(UO_sp_arith              ); }
   if(sp_cond_0188(pc)) {
      argval_t arg = cpd.settings[UO_sp_bool].a;
     if (is_not_token(cpd.settings[UO_pos_bool].tp, TP_IGNORE) &&
         (pc1->orig_line != pc2->orig_line) && (arg != AV_REMOVE)) {
        arg = static_cast<argval_t>(arg | AV_ADD); }
        log_rule("sp_bool");
     return(arg);
   }
   if(any_is_type(pc1, pc2, CT_COMPARE))     { log_opt_return(UO_sp_compare            ); }
   if(are_types(pc1, CT_PAREN_OPEN, pc2, CT_PTR_TYPE))
                                              { log_arg_return(AV_REMOVE                ); }
   if (is_type(pc1, CT_PTR_TYPE) &&
       (cpd.settings[UO_sp_ptr_star_paren].a != AV_IGNORE) &&
       (is_type(pc2, CT_FPAREN_OPEN, CT_TPAREN_OPEN)))
                                              { log_opt_return(UO_sp_ptr_star_paren     ); }
   if (are_types(pc1, pc2, CT_PTR_TYPE) &&
       (is_not_option(cpd.settings[UO_sp_between_pstar].a, AV_IGNORE)))
                                              { log_opt_return(UO_sp_between_pstar   ); }
   if ( is_type (pc1, CT_PTR_TYPE) &&
       (is_not_option(cpd.settings[UO_sp_after_ptr_star_func].a, AV_IGNORE)) &&
       (is_ptype(pc1, CT_FUNC_DEF, CT_FUNC_PROTO, CT_FUNC_VAR)))
                                              { log_opt_return(UO_sp_after_ptr_star_func); }
   if (is_type(pc1, CT_PTR_TYPE) && (CharTable::IsKW1((size_t)(pc2->str[0]))) ){
      chunk_t *prev = chunk_get_prev(pc1);
      if (is_type(prev, CT_IN))               { log_opt_return(UO_sp_deref              ); }
      if (is_type(pc2, CT_QUALIFIER) &&
          (is_not_option(cpd.settings[UO_sp_after_pstar_qualifier].a, AV_IGNORE)))
                                              { log_opt_return(UO_sp_after_pstar_qualifier); }
      else if (is_not_option(cpd.settings[UO_sp_after_pstar].a, AV_IGNORE))
                                              { log_opt_return(UO_sp_after_pstar     ); }}
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

         if (is_type(next, CT_FUNC_DEF, CT_FUNC_PROTO) )
         {
            log_opt_return(UO_sp_before_ptr_star_func);
            //return(cpd.settings[UO_sp_before_ptr_star_func].a);
         }
      }

      if (is_not_option(cpd.settings[UO_sp_before_unnamed_pstar].a, AV_IGNORE))
      {
         chunk_t *next = get_next_nc(pc2);
         while (is_type(next, CT_PTR_TYPE) )
         {
            next = get_next_nc(next);
         }

         if (not_type(next, CT_WORD)) { log_opt_return(UO_sp_before_unnamed_pstar); }}
      if (is_not_option(cpd.settings[UO_sp_before_ptr_star].a, AV_IGNORE))
                                         { log_opt_return(UO_sp_before_ptr_star     ); }}
   if (is_type(pc1, CT_OPERATOR))        { log_opt_return(UO_sp_after_operator      ); }
   if (is_type(pc2, CT_FUNC_PROTO, CT_FUNC_DEF)) {
      if (not_type(pc1, CT_PTR_TYPE)) {
         log_rule("sp_type_func|ADD");
         return(add_option(cpd.settings[UO_sp_type_func].a, AV_ADD)); }
      log_opt_return(UO_sp_type_func);}
   /* "(int)a" vs "(int) a" or "cast(int)a" vs "cast(int) a" */
   if (is_ptype(pc1, CT_C_CAST, CT_D_CAST))
                                         { log_opt_return(UO_sp_after_cast           ); }
   if (is_type(pc1, CT_BRACE_CLOSE))     {
      if      (is_type(pc2, CT_ELSE   )) { log_opt_return(UO_sp_brace_else           ); }
      else if (is_type(pc2, CT_CATCH  )) { log_opt_return(UO_sp_brace_catch          ); }
      else if (is_type(pc2, CT_FINALLY)) { log_opt_return(UO_sp_brace_finally        ); } }
   if (is_type(pc1, CT_BRACE_OPEN)) {
      if (is_ptype(pc1, CT_ENUM))        { log_opt_return(UO_sp_inside_braces_enum   ); }
      else if (is_ptype(pc1, CT_UNION, CT_STRUCT))
                                         { log_opt_return(UO_sp_inside_braces_struct ); }
      else if (!is_cmt(pc2))       { log_opt_return(UO_sp_inside_braces        ); } }
   if (is_type(pc2, CT_BRACE_CLOSE)) {
      if (is_ptype(pc2, CT_ENUM))        { log_opt_return(UO_sp_inside_braces_enum   ); }
      else if (is_ptype(pc2, CT_UNION, CT_STRUCT))
                                         { log_opt_return(UO_sp_inside_braces_struct ); }
                                           log_opt_return(UO_sp_inside_braces        ); }
   if (is_type(pc1, CT_BRACE_CLOSE) && is_flag(pc1, PCF_IN_TYPEDEF) &&
       is_ptype(pc1, CT_ENUM, CT_STRUCT, CT_UNION))
                                         { log_opt_return(UO_sp_brace_typedef        ); }
   if (is_type(pc2, CT_SPAREN_OPEN))     { log_opt_return(UO_sp_before_sparen        ); }
   if (is_type_and_ptype(pc2, CT_PAREN_OPEN, CT_TEMPLATE))
                                         { log_opt_return(UO_sp_before_template_paren); }
   if (not_type(pc2, CT_PTR_TYPE) && is_type(pc1, 2, CT_QUALIFIER, CT_TYPE))
   {
      argval_t arg = cpd.settings[UO_sp_after_type].a;
      log_rule("sp_after_type");
      return((arg != AV_REMOVE) ? arg : AV_FORCE);
   }
   if (is_type(pc1, CT_MACRO_OPEN, CT_MACRO_CLOSE, CT_MACRO_ELSE)) {
      if (is_type(pc2, CT_PAREN_OPEN))     { log_opt_return(UO_sp_func_call_paren     ); }
                                             log_arg_return(AV_IGNORE                 ); }
   /* If nothing claimed the PTR_TYPE, then return ignore */
   if (any_is_type(pc1, pc2, CT_PTR_TYPE)) { log_arg_return(AV_IGNORE                 ); }
   if (is_type(pc1, CT_NOT              )) { log_opt_return(UO_sp_not                 ); }
   if (is_type(pc1, CT_INV              )) { log_opt_return(UO_sp_inv                 ); }
   if (is_type(pc1, CT_ADDR             )) { log_opt_return(UO_sp_addr                ); }
   if (is_type(pc1, CT_DEREF            )) { log_opt_return(UO_sp_deref               ); }
   if (is_type(pc1, CT_POS, CT_NEG   )) { log_opt_return(UO_sp_sign                ); }
   if (any_is_type(pc1, CT_INCDEC_BEFORE, pc2, CT_INCDEC_AFTER ))
                                           { log_opt_return(UO_sp_incdec              ); }
   if (is_type(pc2, CT_CS_SQ_COLON      )) { log_arg_return(AV_REMOVE                 ); }
   if (is_type(pc1, CT_CS_SQ_COLON      )) { log_arg_return(AV_FORCE                  ); }
   if (is_type(pc1, CT_OC_SCOPE         )) { log_opt_return(UO_sp_after_oc_scope      ); }
   if (is_type(pc1, CT_OC_DICT_COLON    )) { log_opt_return(UO_sp_after_oc_dict_colon ); }
   if (is_type(pc2, CT_OC_DICT_COLON    )) { log_opt_return(UO_sp_before_oc_dict_colon); }
   if (is_type(pc1, CT_OC_COLON         )) {
      if (is_flag(pc1, PCF_IN_OC_MSG))     { log_opt_return(UO_sp_after_send_oc_colon ); }
      else                                 { log_opt_return(UO_sp_after_oc_colon      ); }}
   if (is_type(pc2, CT_OC_COLON)) {
      if (is_flag(pc1, PCF_IN_OC_MSG) && is_type(pc1, 2, CT_OC_MSG_FUNC, CT_OC_MSG_NAME     ) )
                                           { log_opt_return(UO_sp_before_send_oc_colon); }
      else                                 { log_opt_return(UO_sp_before_oc_colon     ); }}
   if (is_type_and_ptype(pc2, CT_COMMENT, CT_COMMENT_EMBED))
                                           { log_arg_return(AV_FORCE                  ); }
   if (is_cmt(pc2)) { log_arg_return(AV_IGNORE); }
   if (is_type(pc1, CT_COMMENT)) { log_arg_return(AV_FORCE); }
   /* c# new Constraint, c++ new operator */
   if (are_types(pc1, CT_NEW, pc2, CT_PAREN_OPEN))
                                           { log_opt_return(UO_sp_between_new_paren   ); }
   if (is_type          (pc1, CT_NEW,  CT_DELETE) ||
       is_type_and_ptype(pc1, CT_TSQUARE, CT_DELETE) )
                                           { log_opt_return(UO_sp_after_new           ); }
   if (is_type(pc1, CT_ANNOTATION) &&
       (is_paren_open(pc2) ) )       { log_opt_return(UO_sp_annotation_paren    ); }
   if (is_type(pc1, CT_OC_PROPERTY))       { log_opt_return(UO_sp_after_oc_property   ); }
   if (are_types(pc1, CT_EXTERN, pc2, CT_PAREN_OPEN))
                                           { log_opt_return(UO_sp_extern_paren        ); }
   /* "((" vs "( (" or "))" vs ") )" */
   if ((is_str(pc1, "(", 1) && is_str(pc2, "(", 1)) ||
       (is_str(pc1, ")", 1) && is_str(pc2, ")", 1)) )
                                           { log_opt_return(UO_sp_paren_paren); }
   // this table lists out all combos where a space should NOT be present CT_UNKNOWN is a wildcard.
   // CT_UNKNOWN is a wildcard.
   for (auto it : no_space_table)
   {   if (((it.first  == CT_UNKNOWN) || (it.first  == pc1->type) ) &&
           ((it.second == CT_UNKNOWN) || (it.second == pc2->type) ) )
                                           { log_arg_return(AV_REMOVE                 ); }}
   // mapped_file_source abc((int) A::CW2A(sTemp));
   if (are_types(pc1, CT_PAREN_CLOSE, pc2, CT_TYPE) &&
       are_types(pc2->next, CT_DC_MEMBER, pc2->next->next, CT_FUNC_CALL))
                                           { log_arg_return(AV_REMOVE                 ); }
#ifdef DEBUG
   // these lines are only useful for debugging uncrustify itself
   LOG_FMT(LSPACE, "\n\n(%d) %s: WARNING: unrecognized do_space: first: %zu:%zu %s %s and second: %zu:%zu %s %s\n\n\n",
           __LINE__, __func__, pc1->orig_line, pc1->orig_col, pc1->text(), get_token_name(pc1->type),
           pc2->orig_line, pc2->orig_col, pc2->text(), get_token_name(pc2->type));
#endif
   log_arg_return(AV_ADD);
}


void space_text(void)
{
   LOG_FUNC_ENTRY();

   chunk_t *pc = chunk_get_head();
   return_if(is_invalid(pc));

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
         set_flags(pc, PCF_IN_QT_MACRO); /* flag the chunk for a second processing */

         save_set_options_for_QT(pc->level);
      }
      if (cpd.settings[UO_sp_skip_vbrace_tokens].b)
      {
         next = chunk_get_next(pc);
         while ( ( chunk_empty(next)) &&
                 (!is_nl(next)) &&
                 is_type(next, CT_VBRACE_OPEN, CT_VBRACE_CLOSE))
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

      break_if(is_invalid(next));

      if ((QT_SIGNAL_SLOT_found) &&
          (cpd.settings[UO_sp_bal_nested_parens].b))
      {
         if (is_type(next->next, CT_SPACE))
         {
            chunk_del(next->next); // remove the space
         }
      }

      /* If the current chunk contains a newline, do not change the column
       * of the next item */
      if (is_type(pc, CT_NEWLINE, CT_NL_CONT, CT_COMMENT_MULTI))
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
         clr_flags(pc, PCF_FORCE_SPACE);
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
                  !is_nl(tmp))
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
                  set_flags(pc, PCF_FORCE_SPACE);
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
                     if (((is_lang(cpd, LANG_CPP ) && cpd.settings[UO_sp_permit_cpp11_shift].b) ||
                          (is_lang(cpd, LANG_JAVA) || is_lang(cpd, LANG_CS))) &&
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
                        set_flags(pc, PCF_FORCE_SPACE);
                     }
                  }
               }
            }
         }

         int      min_sp;
         argval_t av = do_space(pc, next, min_sp, false);
         if (is_flag(pc, PCF_FORCE_SPACE))
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
               }
            break;

            case AV_REMOVE:  /* the symbols will be back-to-back "a+3" */
            case AV_NOT_DEFINED:
               /* do nothing */
            break;

            case AV_IGNORE:
               /* Keep the same relative spacing, if possible */
               if ((next->orig_col >= pc->orig_col_end) && (pc->orig_col_end != 0))
               {
                  column += next->orig_col - pc->orig_col_end;
               }
            break;
         }


         if (is_cmt(next                ) &&
             is_nl(chunk_get_next(next)) &&
             (column < next->orig_col))
         {
            /* do some comment adjustments if sp_before_tr_emb_cmt and
             * sp_endif_cmt did not apply. */
            if (((is_option(cpd.settings[UO_sp_before_tr_emb_cmt].a, AV_IGNORE)) ||
                 ((next->ptype != CT_COMMENT_END  ) &&
                  (next->ptype != CT_COMMENT_EMBED) ) ) &&
                (is_option(cpd.settings[UO_sp_endif_cmt].a, AV_IGNORE) ||
                 not_type(pc, CT_PP_ELSE, CT_PP_ENDIF     ) ) )
            {
               if (cpd.settings[UO_indent_rel_single_line_comments].b)
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

         LOG_FMT(LSPACE, " = %s @ %zu => %zu\n", argval2str(av).c_str(),
                 column - prev_column, next->column);
         if (restoreValues) { restore_options_for_QT(); }
      }

      pc = next;
      if (QT_SIGNAL_SLOT_found)
      {
         // flag the chunk for a second processing
         set_flags(pc, PCF_IN_QT_MACRO);
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
      break_if(is_invalid(next));

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
   return_if(is_invalid(next) || is_nl(next));

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
   set_flags(&sp, get_flags(pc, PCF_COPY_FLAGS));
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
