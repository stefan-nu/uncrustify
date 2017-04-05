/**
 * @file space.cpp
 * Adds or removes spaces between chunks.
 *
 * Informations
 *   "Ignore" means do not change it.
 *   "Add"    in the context of spaces means make sure there is at least 1.
 *   "Add"    elsewhere means make sure one is present.
 *   "Remove" mean remove the space/brace/newline/etc.
 *   "Force"  in the context of spaces means ensure that there is exactly 1.
 *   "Force"  in other contexts means the same as "add".
 *
 *   Remark: spaces = space + newline
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt October 2015, 2016
 * @author  Stefan Nunninger refactoring and optimization 2017
 * @license GPL v2+
 */

#include <forward_list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "space.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "punctuators.h"
#include "char_table.h"
#include "options_for_QT.h"
#include "unc_ctype.h"
#include "uncrustify.h"


/**
 * used abbreviations:
 * SCA = space check action
 * CT  = chunk type
 * ID  = identifier
 */


/** type that combines two chunk with a value that indicates how many
 * spaces are expected between the two chunks */
typedef struct chunks_s
{
   chunk_t*  a;  /**< first  chunk */
   chunk_t*  b;  /**< second chunk */
   uint32_t& sp; /**< number of expected spaces, typically 1 */
}chunks_t;


/** \brief log how and where a spacing decision was taken */
static void log_space(
   const uint32_t id,    /**< [in] id of space modification rule */
   const uo_t     opt,   /**< [in] option that was used for spacing decision */
   chunks_t*      c,     /**< [in] chunks for which the spacing applies */
   bool           add_nl /**< [in] if true log messages gets terminated with a newline */
);


/**
 * Decides how to change spaces between two successive chunks
 *
 * @return AV_IGNORE, AV_ADD, AV_REMOVE or AV_FORCE and the number
 * of expected space characters in min_sp
 */
static argval_t do_space(
   chunk_t* pc1,     /**< [in]  The first chunk */
   chunk_t* pc2,     /**< [in]  The second chunk */
   uint32_t &min_sp, /**< [out] minimal required space size, typically 1 */
   bool     complete /**< [in]  if true the log message gets terminated with a newline */
);


/** type that stores two chunks between those no space
 * shall occur */
struct no_space_table_t
{
   c_token_t first;   /**< [in] first  chunk */
   c_token_t second;  /**< [in] second chunk */
};


/**
 * this table lists all chunk combos where a space must NOT be present
 *
 * \note: CT_UNKNOWN is a wildcard for all chunks.
 * \todo: some of these are no longer needed, identify and remove them.
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

#define log_opt_return(uo)                    \
   do {log_space(__LINE__, uo, pc, complete); \
       return(get_arg(uo));                   \
   } while(0)

#define log_rule(rule)                            \
   do {log_space(__LINE__, (rule), pc, complete); \
   } while (0)


static void log_space(const uint32_t id, const uo_t opt, chunks_t* c, bool add_nl)
{
   LOG_FUNC_ENTRY();
   if (log_sev_on(LSPACE))
   {
      chunk_t* pc1 = c->a;
      chunk_t* pc2 = c->b;

      const option_map_value_t* rule = get_option_name(opt);
      if(is_invalid(pc1))      {LOG_FMT(LSPACE, "cannot log as pointer pc1  is invalid\n");return;};
      if(is_invalid(pc2))      {LOG_FMT(LSPACE, "cannot log as pointer pc2  is invalid\n");return;};
      if(ptr_is_invalid(rule)) {LOG_FMT(LSPACE, "cannot log as pointer rule is invalid\n");return;};
      if (not_type(pc2, CT_NEWLINE))
      {
         LOG_FMT(LSPACE, "Spacing: id %u [%s/%s] '%s' <===> [%s/%s] '%s' : %s[%u]%s",
                 pc1->orig_line, get_token_name(pc1->type),
                 get_token_name(pc1->ptype), pc1->text(),
                 get_token_name(pc2->type), get_token_name(pc2->ptype),
                 pc2->text(), rule->name, id, add_nl ? "\n" : "");
      }
   }
}


/* various conditions to determine where to places spaces
 * the conditions have been placed in individual functions to use
 * them as callback functions which will make the code easier
 * and more efficient. */
static bool sp_cond_0069(chunks_t* c) {return is_type_and_ptype(c->a, CT_SQUARE_CLOSE, CT_CPP_LAMBDA) &&
                                              is_type          (c->b, CT_FPAREN_OPEN); }
static bool sp_cond_0023(chunks_t* c) {return is_type_and_ptype(c->a, CT_COLON,        CT_SQL_EXEC  ); }
static bool sp_cond_0025(chunks_t* c) {return is_type_and_ptype(c->a, CT_FPAREN_CLOSE, CT_MACRO_FUNC); }
static bool sp_cond_0151(chunks_t* c) {return is_type_and_ptype(c->b, CT_PAREN_OPEN,   CT_INVARIANT ); }

static bool sp_cond_0008(chunks_t* c) {return is_only_first_type(c->a, CT_VBRACE_OPEN,  c->b, CT_NL_CONT); }
static bool sp_cond_0009(chunks_t* c) {return is_only_first_type(c->a, CT_VBRACE_CLOSE, c->b, CT_NL_CONT); }

static bool sp_cond_0161(chunks_t* c) {return any_is_type(c->a, CT_TPAREN_OPEN,   c->b, CT_TPAREN_CLOSE); }
static bool sp_cond_0178(chunks_t* c) {return any_is_type(c->a, CT_SPAREN_OPEN,   c->b, CT_SPAREN_CLOSE); }
static bool sp_cond_0174(chunks_t* c) {return any_is_type(c->a, CT_SQUARE_OPEN,   c->b, CT_SQUARE_CLOSE); }
static bool sp_cond_0223(chunks_t* c) {return any_is_type(c->a, CT_INCDEC_BEFORE, c->b, CT_INCDEC_AFTER); }
static bool sp_cond_0140(chunks_t* c) {return any_is_type(c->a, CT_MEMBER,        c->b, CT_MEMBER      ); }
static bool sp_cond_0085(chunks_t* c) {return any_is_type(c->a, CT_ANGLE_OPEN,    c->b, CT_ANGLE_CLOSE ); }

static bool sp_cond_1188(chunks_t* c) {return any_is_type(c->a, c->b, CT_BOOL) && (c->a->orig_line != c->b->orig_line     ); }
static bool sp_cond_0188(chunks_t* c) {return any_is_type(c->a, c->b, CT_BOOL         ); }
static bool sp_cond_0189(chunks_t* c) {return any_is_type(c->a, c->b, CT_COMPARE      ); }
static bool sp_cond_0186(chunks_t* c) {return any_is_type(c->a, c->b, CT_NULLCOND     ); }
static bool sp_cond_0067(chunks_t* c) {return any_is_type(c->a, c->b, CT_LAMBDA       ); }
static bool sp_cond_0001(chunks_t* c) {return any_is_type(c->a, c->b, CT_IGNORED      ); }
static bool sp_cond_0003(chunks_t* c) {return any_is_type(c->a, c->b, CT_PP           ); }
static bool sp_cond_0006(chunks_t* c) {return any_is_type(c->a, c->b, CT_SPACE        ); }
static bool sp_cond_0013(chunks_t* c) {return any_is_type(c->a, c->b, CT_D_ARRAY_COLON); }
static bool sp_cond_0217(chunks_t* c) {return any_is_type(c->a, c->b, CT_PTR_TYPE     ); }
static bool sp_cond_0018(chunks_t* c) {return any_is_type(c->a, c->b, CT_QUESTION     ); }
static bool sp_cond_0022(chunks_t* c) {return any_is_type(c->a, c->b, CT_RANGE        ); }
static bool sp_cond_0141(chunks_t* c) {return any_is_type(c->a, c->b, CT_D_TEMPLATE   ); }
static bool sp_cond_0021(chunks_t* c) {return any_is_type(c->a, c->b, CT_COND_COLON   ); }

static bool sp_cond_0019(chunks_t* c) {return is_type(c->b, CT_QUESTION); }
static bool sp_cond_0020(chunks_t* c) {return is_type(c->a, CT_QUESTION); }

static bool sp_cond_0084(chunks_t* c) {return are_types(c->a, c->b, CT_ANGLE_CLOSE); }
static bool sp_cond_0002(chunks_t* c) {return are_types(c->a, c->b, CT_PP_IGNORE  ); }
static bool sp_cond_0191(chunks_t* c) {return are_types(c->a, c->b, CT_PTR_TYPE   ); }

static bool sp_cond_0149(chunks_t* c) {return are_types(c->a, CT_WORD, c->b, CT_BRACE_OPEN) && is_ptype(c->a, CT_NAMESPACE); }
static bool sp_cond_0150(chunks_t* c) {return are_types(c->a, CT_WORD, c->b, CT_BRACE_OPEN) && are_ptypes(c->a, c->b, CT_NONE); }

static bool sp_cond_0235(chunks_t* c) {return are_types(c->a, CT_NEW,          c->b, CT_PAREN_OPEN  ); }
static bool sp_cond_0190(chunks_t* c) {return are_types(c->a, CT_PAREN_OPEN,   c->b, CT_PTR_TYPE    ); }

static bool sp_cond_0091(chunks_t* c) {return any_is_type(c->a, CT_FPAREN_OPEN, c->b, CT_FPAREN_CLOSE); }
static bool sp_cond_0160(chunks_t* c) {return are_types  (c->a, CT_FPAREN_OPEN, c->b, CT_FPAREN_CLOSE); }


static bool sp_cond_0201(chunks_t* c) {return are_types(c->a, CT_BRACE_CLOSE,  c->b, CT_ELSE        ); }
static bool sp_cond_0202(chunks_t* c) {return are_types(c->a, CT_BRACE_CLOSE,  c->b, CT_CATCH       ); }
static bool sp_cond_0203(chunks_t* c) {return are_types(c->a, CT_BRACE_CLOSE,  c->b, CT_FINALLY     ); }
static bool sp_cond_0175(chunks_t* c) {return are_types(c->a, CT_SQUARE_CLOSE, c->b, CT_FPAREN_OPEN ); }
static bool sp_cond_0064(chunks_t* c) {return are_types(c->a, CT_D_VERSION_IF, c->b, CT_SPAREN_OPEN ); }
static bool sp_cond_0065(chunks_t* c) {return are_types(c->a, CT_D_SCOPE_IF,   c->b, CT_SPAREN_OPEN ); }
static bool sp_cond_0017(chunks_t* c) {return are_types(c->a, CT_QUESTION,     c->b, CT_COND_COLON  ); }
static bool sp_cond_0129(chunks_t* c) {return are_types(c->a, CT_PP_DEFINED,   c->b, CT_PAREN_OPEN  ); }
static bool sp_cond_0132(chunks_t* c) {return are_types(c->a, CT_THIS,         c->b, CT_PAREN_OPEN  ); }
static bool sp_cond_0133(chunks_t* c) {return are_types(c->a, CT_STATE,        c->b, CT_PAREN_OPEN  ); }
static bool sp_cond_0134(chunks_t* c) {return are_types(c->a, CT_DELEGATE,     c->b, CT_PAREN_OPEN  ); }
static bool sp_cond_0112(chunks_t* c) {return are_types(c->a, CT_PAREN_CLOSE,  c->b, CT_WHEN        ); }
static bool sp_cond_0124(chunks_t* c) {return are_types(c->a, CT_BRACE_OPEN,   c->b, CT_BRACE_CLOSE ); }
static bool sp_cond_0142(chunks_t* c) {return are_types(c->a, CT_ELSE,         c->b, CT_BRACE_OPEN  ); }
static bool sp_cond_0143(chunks_t* c) {return are_types(c->a, CT_ELSE,         c->b, CT_ELSEIF      ); }
static bool sp_cond_0063(chunks_t* c) {return are_types(c->a, CT_CATCH,        c->b, CT_SPAREN_OPEN ); }
static bool sp_cond_0144(chunks_t* c) {return are_types(c->a, CT_CATCH,        c->b, CT_BRACE_OPEN  ); }
static bool sp_cond_0145(chunks_t* c) {return are_types(c->a, CT_FINALLY,      c->b, CT_BRACE_OPEN  ); }
static bool sp_cond_0146(chunks_t* c) {return are_types(c->a, CT_TRY,          c->b, CT_BRACE_OPEN  ); }
static bool sp_cond_0147(chunks_t* c) {return are_types(c->a, CT_GETSET,       c->b, CT_BRACE_OPEN  ); }
static bool sp_cond_0137(chunks_t* c) {return are_types(c->a, CT_SUPER,        c->b, CT_PAREN_OPEN  ); }
static bool sp_cond_0070(chunks_t* c) {return are_types(c->a, CT_ENUM,         c->b, CT_FPAREN_OPEN ); }
static bool sp_cond_0239(chunks_t* c) {return are_types(c->a, CT_EXTERN,       c->b, CT_PAREN_OPEN  ); }

static bool sp_cond_0248(chunks_t* c) {return(are_types(c->a,       CT_PAREN_CLOSE, c->b,             CT_TYPE     ) &&
                                              are_types(c->b->next, CT_DC_MEMBER,   c->b->next->next, CT_FUNC_CALL) ); }

static bool sp_cond_0045(chunks_t* c) {return are_types(c->a, CT_PAREN_CLOSE,  c->b, CT_DC_MEMBER) &&
                                                is_type(c->b->next, CT_FUNC_CALL); }

static bool sp_cond_0241(chunks_t* c) {return is_type(c->a, CT_PTR_TYPE    ) && is_ptype(c->a, CT_FUNC_DEF, CT_FUNC_PROTO, CT_FUNC_VAR); }
static bool sp_cond_0033(chunks_t* c) {return is_type(c->b, CT_COMMENT     ) && is_type(c->a, CT_PP_ELSE,      CT_PP_ENDIF   ); }
static bool sp_cond_0051(chunks_t* c) {return is_type(c->a, CT_FPAREN_CLOSE) && is_type(c->b, CT_FPAREN_CLOSE, CT_COMMA      ); }
static bool sp_cond_0167(chunks_t* c) {return is_type(c->b, CT_PAREN_OPEN  ) && is_type(c->a, CT_OC_PROTOCOL,  CT_OC_SEL     ); }
static bool sp_cond_0246(chunks_t* c) {return is_type(c->a, CT_PTR_TYPE    ) && is_type(c->b, CT_FPAREN_OPEN,  CT_TPAREN_OPEN); }
static bool sp_cond_0187(chunks_t* c) {return is_type(c->a, CT_ARITH, CT_CARET) || is_type(c->b, CT_ARITH, CT_CARET) ; }
static bool sp_cond_0039(chunks_t* c) {return is_type(c->a, CT_NEG,  CT_POS,  CT_ARITH) && is_type(c->b, CT_NEG, CT_POS, CT_ARITH); }
static bool sp_cond_0059(chunks_t* c) {return is_type(c->a, CT_ELLIPSIS   ) &&  CharTable::IsKW1((uint32_t)(c->b->str[0])); }
static bool sp_cond_0014(chunks_t* c) {return is_type(c->a, CT_CASE       ) && (CharTable::IsKW1((uint32_t)(c->b->str[0])) || is_type(c->b, CT_NUMBER)); }
static bool sp_cond_0005(chunks_t* c) {return is_type(c->b, CT_POUND      ) && is_preproc(c->b) && not_ptype(c->a, CT_MACRO_FUNC); }
static bool sp_cond_0123(chunks_t* c) {return is_type(c->a, CT_CLASS      ) && not_flag(c->a, PCF_IN_OC_MSG); }
static bool sp_cond_0096(chunks_t* c) {return is_type(c->a, CT_BYREF      ) && CharTable::IsKW1((uint32_t)(c->b->str[0])); }
static bool sp_cond_0077(chunks_t* c) {return(is_type(c->a, CT_BIT_COLON  ) && is_flag(c->a, PCF_IN_ENUM)) ||
                                             (is_type(c->b, CT_BIT_COLON  ) && is_flag(c->b, PCF_IN_ENUM)); }
static bool sp_cond_0082(chunks_t* c) {return is_type(c->b, CT_SQUARE_OPEN) && not_ptype(c->b, CT_OC_MSG); }

static bool sp_cond_0007(chunks_t* c) {return is_type(c->b, CT_NEWLINE,    CT_VBRACE_OPEN); }
static bool sp_cond_0111(chunks_t* c) {return is_type(c->a, CT_CPP_CAST,   CT_TYPE_WRAP  ); }

static bool sp_cond_0004(chunks_t* c) {return is_type(c->a, CT_POUND         ); }
static bool sp_cond_0010(chunks_t* c) {return is_type(c->b, CT_VSEMICOLON    ); }
static bool sp_cond_0011(chunks_t* c) {return is_type(c->a, CT_MACRO_FUNC    ); }
static bool sp_cond_0012(chunks_t* c) {return is_type(c->b, CT_NL_CONT       ); }
static bool sp_cond_0015(chunks_t* c) {return is_type(c->a, CT_FOR_COLON     ); }
static bool sp_cond_0016(chunks_t* c) {return is_type(c->b, CT_FOR_COLON     ); }
static bool sp_cond_0024(chunks_t* c) {return is_type(c->a, CT_MACRO         ); }
static bool sp_cond_0026(chunks_t* c) {return is_type(c->a, CT_PREPROC       ); }
static bool sp_cond_0027(chunks_t* c) {return is_type(c->b, CT_COND_COLON); }
static bool sp_cond_0028(chunks_t* c) {return is_type(c->a, CT_COND_COLON); }
static bool sp_cond_0044(chunks_t* c) {return is_type(c->a, CT_DC_MEMBER); }

static bool sp_cond_0029(chunks_t* c) {return is_type(c->b, CT_SEMICOLON     ); }
static bool sp_cond_0032(chunks_t* c) {return is_type  (c->b, CT_SEMICOLON   ) &&
                                              is_type  (c->a, CT_SPAREN_CLOSE) &&
                                              not_ptype(c->a, CT_WHILE_OF_DO ); }

static bool sp_cond_0030(chunks_t* c) {return is_type (c->b, CT_SEMICOLON) && is_ptype(c->b, CT_FOR); }
static bool sp_cond_0031(chunks_t* c) {return is_type (c->b, CT_SEMICOLON) && is_ptype(c->b, CT_FOR) &&
                                              is_type (c->a, CT_SPAREN_OPEN, CT_SEMICOLON); }

static bool sp_cond_0048(chunks_t* c) {return is_type (c->a, CT_COMMA);}
static bool sp_cond_0049(chunks_t* c) {return is_type (c->a, CT_COMMA) && is_ptype(c->a, CT_TYPE); }
static bool sp_cond_0050(chunks_t* c) {return is_type (c->a, CT_COMMA) && is_ptype(c->a, CT_TYPE) &&
                                              is_type (c->b, CT_COMMA); }

static bool sp_cond_0060(chunks_t* c) {return is_type(c->a, CT_TAG_COLON     ); }
static bool sp_cond_0061(chunks_t* c) {return is_type(c->b, CT_TAG_COLON     ); }
static bool sp_cond_0062(chunks_t* c) {return is_type(c->a, CT_DESTRUCTOR    ); }
static bool sp_cond_0066(chunks_t* c) {return is_type(c->b, CT_SPAREN_OPEN   ); }
static bool sp_cond_0079(chunks_t* c) {return is_type(c->b, CT_OC_BLOCK_CARET); }
static bool sp_cond_0080(chunks_t* c) {return is_type(c->a, CT_OC_BLOCK_CARET); }
static bool sp_cond_0081(chunks_t* c) {return is_type(c->b, CT_OC_MSG_FUNC   ); }
static bool sp_cond_0083(chunks_t* c) {return is_type(c->b, CT_TSQUARE       ); }
static bool sp_cond_0086(chunks_t* c) {return is_type(c->b, CT_ANGLE_OPEN    ); }
static bool sp_cond_0087(chunks_t* c) {return is_type(c->b, CT_ANGLE_OPEN) && is_type(c->a, CT_TEMPLATE); }
static bool sp_cond_0105(chunks_t* c) {return is_type(c->a, CT_FUNC_CALL_USER); }
static bool sp_cond_0106(chunks_t* c) {return is_type(c->a, CT_ATTRIBUTE     ); }

static bool sp_cond_0130(chunks_t* c) {return is_type(c->a, CT_THROW); }
static bool sp_cond_0131(chunks_t* c) {return is_type(c->a, CT_THROW) && is_type(c->b, CT_PAREN_OPEN); }

static bool sp_cond_0136(chunks_t* c) {return is_type(c->a, CT_C99_MEMBER    ); }
static bool sp_cond_0128(chunks_t* c) {return is_type(c->a, CT_D_CAST        ); }
static bool sp_cond_0115(chunks_t* c) {return is_type(c->a, CT_TPAREN_CLOSE  ); }

static bool sp_cond_0218(chunks_t* c) {return is_type(c->a, CT_NOT           ); }
static bool sp_cond_0219(chunks_t* c) {return is_type(c->a, CT_INV           ); }
static bool sp_cond_0220(chunks_t* c) {return is_type(c->a, CT_ADDR          ); }
static bool sp_cond_0221(chunks_t* c) {return is_type(c->a, CT_DEREF         ); }
static bool sp_cond_0222(chunks_t* c) {return is_type(c->a, CT_POS, CT_NEG   ); }
static bool sp_cond_0224(chunks_t* c) {return is_type(c->b, CT_CS_SQ_COLON   ); }
static bool sp_cond_0225(chunks_t* c) {return is_type(c->a, CT_CS_SQ_COLON   ); }
static bool sp_cond_0226(chunks_t* c) {return is_type(c->a, CT_OC_SCOPE      ); }
static bool sp_cond_0227(chunks_t* c) {return is_type(c->a, CT_OC_DICT_COLON ); }
static bool sp_cond_0228(chunks_t* c) {return is_type(c->b, CT_OC_DICT_COLON ); }
static bool sp_cond_0215(chunks_t* c) {return is_type(c->a, CT_MACRO_OPEN, CT_MACRO_CLOSE, CT_MACRO_ELSE); }
static bool sp_cond_0216(chunks_t* c) {return is_type(c->a, CT_MACRO_OPEN, CT_MACRO_CLOSE, CT_MACRO_ELSE) && is_type(c->b, CT_PAREN_OPEN); }
static bool sp_cond_0238(chunks_t* c) {return is_type(c->a, CT_OC_PROPERTY   ); }
static bool sp_cond_0176(chunks_t* c) {return is_type(c->b, CT_SPAREN_CLOSE  ); }
static bool sp_cond_0177(chunks_t* c) {return is_type(c->a, CT_SPAREN_OPEN   ); }
static bool sp_cond_0212(chunks_t* c) {return is_type(c->b, CT_SPAREN_OPEN   ); }
static bool sp_cond_0179(chunks_t* c) {return is_type(c->a, CT_CLASS_COLON   ); }
static bool sp_cond_0180(chunks_t* c) {return is_type(c->b, CT_CLASS_COLON   ); }
static bool sp_cond_0182(chunks_t* c) {return is_type(c->b, CT_CONSTR_COLON  ); }
static bool sp_cond_0183(chunks_t* c) {return is_type(c->b, CT_CASE_COLON    ); }
static bool sp_cond_0184(chunks_t* c) {return is_type(c->a, CT_DOT           ); }
static bool sp_cond_0185(chunks_t* c) {return is_type(c->b, CT_DOT           ); }
static bool sp_cond_0196(chunks_t* c) {return is_type(c->a, CT_OPERATOR      ); }
static bool sp_cond_0234(chunks_t* c) {return is_type(c->a, CT_COMMENT       ); }

static bool sp_cond_0089(chunks_t* c) {return is_type(c->a, CT_ANGLE_CLOSE) && (is_type(c->b, CT_WORD) || CharTable::IsKW1((uint32_t)(c->b->str[0]))); }
static bool sp_cond_0090(chunks_t* c) {return is_type(c->a, CT_ANGLE_CLOSE) && is_type (c->b, CT_FPAREN_OPEN, CT_PAREN_OPEN); }
static bool sp_cond_0245(chunks_t* c) {return is_type(c->a, CT_ANGLE_CLOSE) && is_type (c->b, CT_FPAREN_OPEN, CT_PAREN_OPEN) && is_type(get_next_ncnl (c->b), CT_FPAREN_CLOSE); }
static bool sp_cond_0092(chunks_t* c) {return is_type(c->a, CT_ANGLE_CLOSE) && is_type (c->b, CT_DC_MEMBER); }
static bool sp_cond_0093(chunks_t* c) {return is_type(c->a, CT_ANGLE_CLOSE) && not_type(c->b, CT_BYREF, CT_PTR_TYPE); }

static bool sp_cond_0192(chunks_t* c) {return is_type(c->a, CT_PTR_TYPE) && CharTable::IsKW1((uint32_t)(c->b->str[0])); }
static bool sp_cond_0194(chunks_t* c) {return is_type(c->a, CT_PTR_TYPE) && CharTable::IsKW1((uint32_t)(c->b->str[0])) && is_type(chunk_get_prev(c->a), CT_IN); }
static bool sp_cond_0195(chunks_t* c) {return is_type(c->a, CT_PTR_TYPE) && CharTable::IsKW1((uint32_t)(c->b->str[0])) && is_type(c->b, CT_QUALIFIER); }

static bool sp_cond_0097(chunks_t* c) {return is_type(c->b, CT_BYREF); }
static bool sp_cond_0244(chunks_t* c) {return is_type(c->b, CT_BYREF) && not_type(get_next_nc(c->b), CT_WORD); }
static bool sp_cond_0243(chunks_t* c) {return is_type(c->b, CT_BYREF) && is_type(chunk_get_next(c->b), CT_FUNC_DEF, CT_FUNC_PROTO); }

static bool sp_cond_0104(chunks_t* c) {return is_type(c->a, CT_FUNC_CALL, CT_FUNC_CTOR_VAR) &&
                                              is_type(c->b, CT_FPAREN_OPEN); }

static bool sp_cond_0103(chunks_t* c) {return is_type(c->a, CT_FUNC_CALL, CT_FUNC_CTOR_VAR) &&
                                              is_type(c->b, CT_FPAREN_OPEN) &&
                                              is_type(get_next_ncnl (c->b), CT_FPAREN_CLOSE); }

static bool sp_cond_0120(chunks_t* c) {return is_type(c->a, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO); }
static bool sp_cond_0121(chunks_t* c) {return is_type(c->a, CT_FUNC_CLASS_DEF, CT_FUNC_CLASS_PROTO) &&
                                              is_type(c->b, CT_FPAREN_OPEN) &&
                                              is_type(get_next_ncnl (c->b), CT_FPAREN_CLOSE); }

static bool sp_cond_0117(chunks_t* c) {return  is_type(c->a, CT_FUNC_PROTO) || is_type_and_ptype(c->b, CT_FPAREN_OPEN, CT_FUNC_PROTO); }
static bool sp_cond_0118(chunks_t* c) {return (is_type(c->a, CT_FUNC_PROTO) || is_type_and_ptype(c->b, CT_FPAREN_OPEN, CT_FUNC_PROTO)) && is_type(get_next_ncnl (c->b), CT_FPAREN_CLOSE); }

static bool sp_cond_0107(chunks_t* c) {return is_type(c->a, CT_FUNC_DEF); }
static bool sp_cond_0109(chunks_t* c) {return is_type(c->a, CT_FUNC_DEF) && is_type(c->b, CT_FPAREN_OPEN) && is_type(get_next_ncnl (c->b), CT_FPAREN_CLOSE); }

static bool sp_cond_0101(chunks_t* c) {return is_type(c->b, CT_FPAREN_OPEN ) && is_ptype(c->a, CT_OPERATOR); }
static bool sp_cond_0135(chunks_t* c) {return is_type(c->b, CT_FPAREN_OPEN ) && is_ptype(c->a, CT_OPERATOR) && is_type(get_next_ncnl (c->b), CT_FPAREN_CLOSE); }

static bool sp_cond_0047(chunks_t* c) {return is_type(c->b, CT_DC_MEMBER) &&
      (is_type(c->a, CT_WORD, CT_TYPE, CT_PAREN_CLOSE) || CharTable::IsKW1((uint32_t)(c->a->str[0]))); }

static bool sp_cond_0052(chunks_t* c) {return is_type(c->b, CT_DC_MEMBER) &&
      is_type(c->a, 22, CT_SBOOL, CT_SARITH, CT_CASE, CT_SASSIGN,  CT_NEW, CT_RETURN, CT_QUALIFIER,
      CT_TYPENAME,  CT_OPERATOR, CT_DELETE, CT_ARITH, CT_ARITH, CT_FRIEND, CT_PRIVATE, CT_UNION,
      CT_NAMESPACE, CT_SCOMPARE, CT_SIZEOF, CT_CLASS, CT_USING, CT_STRUCT, CT_TYPEDEF, CT_THROW); }

static bool sp_cond_0163(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_flag(c->a, PCF_OC_RTYPE ) && is_ptype(c->a, CT_OC_MSG_DECL, CT_OC_MSG_SPEC); }
static bool sp_cond_0164(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_ptype(c->a, CT_OC_MSG_SPEC, CT_OC_MSG_DECL); }
static bool sp_cond_0165(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_ptype(c->a, CT_OC_SEL) && not_type(c->b, CT_SQUARE_CLOSE); }

static bool sp_cond_0125(chunks_t* c) {return is_type(c->b, CT_BRACE_CLOSE); }
static bool sp_cond_0126(chunks_t* c) {return is_type(c->b, CT_BRACE_CLOSE) && is_ptype(c->b, CT_ENUM); }
static bool sp_cond_0127(chunks_t* c) {return is_type(c->b, CT_BRACE_CLOSE) && is_ptype(c->b, CT_STRUCT, CT_UNION); }

static bool sp_cond_0199(chunks_t* c) {return is_ptype(c->a, CT_C_CAST, CT_D_CAST); }

static bool sp_cond_0113(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_type(c->b, CT_PAREN_OPEN, CT_FPAREN_OPEN); }
static bool sp_cond_0114(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_type(c->b, CT_PAREN_OPEN, CT_FPAREN_OPEN) && is_ptype(c->a, CT_C_CAST, CT_D_CAST); }

static bool sp_cond_0138(chunks_t* c) {return are_types(c->a, CT_FPAREN_CLOSE, c->b, CT_BRACE_OPEN); }
static bool sp_cond_0139(chunks_t* c) {return are_types(c->a, CT_FPAREN_CLOSE, c->b, CT_BRACE_OPEN) && is_ptype(c->b, CT_DOUBLE_BRACE); }

static bool sp_cond_0153(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_ptype(c->a, CT_D_TEMPLATE); }
static bool sp_cond_0154(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_ptype(c->a, CT_INVARIANT); }
static bool sp_cond_0155(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_type (c->b, CT_ARITH, CT_CARET); }
static bool sp_cond_0156(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_type (c->b, CT_BRACE_OPEN); }
static bool sp_cond_0157(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_ptype(c->a, CT_DELEGATE); }
static bool sp_cond_0158(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_ptype(c->a, CT_STATE); }
static bool sp_cond_0159(chunks_t* c) {return is_type(c->a, CT_PAREN_CLOSE) && is_ptype(c->a, CT_NEW); }

static bool sp_cond_0168(chunks_t* c) {return is_type(c->a, CT_PAREN_OPEN); }
static bool sp_cond_0169(chunks_t* c) {return is_type(c->a, CT_PAREN_OPEN) && is_ptype(c->a, CT_C_CAST, CT_CPP_CAST, CT_D_CAST); }
static bool sp_cond_0170(chunks_t* c) {return is_type(c->a, CT_PAREN_OPEN) && is_ptype(c->a, CT_NEW); }

static bool sp_cond_0171(chunks_t* c) {return is_type(c->b, CT_PAREN_CLOSE); }
static bool sp_cond_0172(chunks_t* c) {return is_type(c->b, CT_PAREN_CLOSE) && is_ptype(c->b, CT_C_CAST, CT_CPP_CAST, CT_D_CAST); }
static bool sp_cond_0173(chunks_t* c) {return is_type(c->b, CT_PAREN_CLOSE) && is_ptype(c->b, CT_NEW); }

static bool sp_cond_0208(chunks_t* c) {return is_type(c->b, CT_BRACE_CLOSE); }
static bool sp_cond_0209(chunks_t* c) {return is_type(c->b, CT_BRACE_CLOSE) && is_ptype(c->b, CT_ENUM); }
static bool sp_cond_0210(chunks_t* c) {return is_type(c->b, CT_BRACE_CLOSE) && is_ptype(c->b, CT_UNION, CT_STRUCT); }

static bool sp_cond_0231(chunks_t* c) {return is_type(c->b, CT_OC_COLON); }
static bool sp_cond_0232(chunks_t* c) {return is_type(c->b, CT_OC_COLON) && is_flag(c->a, PCF_IN_OC_MSG) && is_type (c->a, CT_OC_MSG_FUNC, CT_OC_MSG_NAME); }

static bool sp_cond_0229(chunks_t* c) {return is_type(c->a, CT_OC_COLON); }
static bool sp_cond_0230(chunks_t* c) {return is_type(c->a, CT_OC_COLON) && is_flag(c->a, PCF_IN_OC_MSG); }

static bool sp_cond_0071(chunks_t* c) {return is_type(c->b, CT_ASSIGN); }
static bool sp_cond_0072(chunks_t* c) {return is_type(c->b, CT_ASSIGN) && is_flag (c->b, PCF_IN_ENUM  ); }
static bool sp_cond_0073(chunks_t* c) {return is_type(c->b, CT_ASSIGN) && is_ptype(c->b, CT_FUNC_PROTO); }

static bool sp_cond_0197(chunks_t* c) {return is_type (c->b, CT_FUNC_PROTO, CT_FUNC_DEF); }
static bool sp_cond_0198(chunks_t* c) {return is_type (c->b, CT_FUNC_PROTO, CT_FUNC_DEF) && not_type(c->a, CT_PTR_TYPE); }
static bool sp_cond_0214(chunks_t* c) {return not_type(c->b, CT_PTR_TYPE) && is_type(c->a, CT_QUALIFIER, CT_TYPE); }

static bool sp_cond_0247(chunks_t* c) {return   is_type_and_ptype(c->b, CT_COMMENT,      CT_COMMENT_EMBED); }
static bool sp_cond_0213(chunks_t* c) {return   is_type_and_ptype(c->b, CT_PAREN_OPEN,   CT_TEMPLATE); }
static bool sp_cond_0068(chunks_t* c) {return ((is_type_and_ptype(c->a, CT_SQUARE_OPEN,  CT_CPP_LAMBDA) && is_type(c->b, CT_ASSIGN)) ||
                                               (is_type_and_ptype(c->b, CT_SQUARE_CLOSE, CT_CPP_LAMBDA) && is_type(c->a, CT_ASSIGN)) ); }

static bool sp_cond_0205(chunks_t* c) {return is_type(c->a, CT_BRACE_OPEN) && is_ptype(c->a, CT_ENUM); }
static bool sp_cond_0206(chunks_t* c) {return is_type(c->a, CT_BRACE_OPEN) && is_ptype(c->a, CT_UNION, CT_STRUCT); }
static bool sp_cond_0207(chunks_t* c) {return is_type(c->a, CT_BRACE_OPEN) && !is_cmt(c->b); }

static bool sp_cond_0233(chunks_t* c) {return  is_cmt(c->b); }

static bool sp_cond_0240(chunks_t* c) {return ((is_str(c->a, "(") && is_str(c->b, "(")) ||
                                               (is_str(c->a, ")") && is_str(c->b, ")")) ); }
static bool sp_cond_0116(chunks_t* c) {return ( is_str(c->a, ")") && is_str(c->b, "(") ) ||
                                              (is_paren_close(c->a) && is_paren_open(c->b) ); }

static bool sp_cond_0166(chunks_t* c) {return ((is_type(c->a, CT_PAREN_OPEN ) && is_ptype(c->a, CT_OC_SEL, CT_OC_PROTOCOL)) ||
                                               (is_type(c->b, CT_PAREN_CLOSE) && is_ptype(c->b, CT_OC_SEL, CT_OC_PROTOCOL)) ); }

static bool sp_cond_0211(chunks_t* c) {return is_type (c->a, CT_BRACE_CLOSE) && is_flag(c->a, PCF_IN_TYPEDEF) &&
                                              is_ptype(c->a, CT_ENUM, CT_STRUCT, CT_UNION); }

static bool sp_cond_0236(chunks_t* c) {return is_type(c->a, CT_NEW, CT_DELETE) || is_type_and_ptype(c->a, CT_TSQUARE, CT_DELETE); }
static bool sp_cond_0237(chunks_t* c) {return is_type(c->a, CT_ANNOTATION) && is_paren_open(c->b); }

static bool sp_cond_0035(chunks_t* c) {return is_ptype(c->b, CT_COMMENT_END ) && (c->b->orig_prev_sp == 0); }
static bool sp_cond_1035(chunks_t* c) {return is_ptype(c->b, CT_COMMENT_END ) && (c->b->orig_prev_sp == 1); }
static bool sp_cond_2035(chunks_t* c) {return is_ptype(c->b, CT_COMMENT_END ) && (c->b->orig_prev_sp == 2); }

static bool sp_cond_0037(chunks_t* c) {return is_type (c->a, CT_SEMICOLON) && is_ptype(c->a, CT_FOR); }
static bool sp_cond_0038(chunks_t* c) {return is_type (c->a, CT_SEMICOLON) && is_ptype(c->a, CT_FOR) && is_type(c->b, CT_SPAREN_CLOSE); }
static bool sp_cond_0242(chunks_t* c) {return is_type(c->a, CT_SEMICOLON) && !is_cmt(c->b) && not_type(c->b, CT_BRACE_CLOSE); }

static bool sp_cond_0040(chunks_t* c) {return is_type(c->a, CT_RETURN); }
static bool sp_cond_0041(chunks_t* c) {return is_type(c->a, CT_RETURN) && is_type_and_ptype(c->b, CT_PAREN_OPEN, CT_RETURN); }

static bool sp_cond_0042(chunks_t* c) {return is_type(c->a, CT_SIZEOF); }
static bool sp_cond_0043(chunks_t* c) {return is_type(c->a, CT_SIZEOF) && is_type(c->b, CT_PAREN_OPEN); }

static bool sp_cond_0053(chunks_t* c) {return is_type(c->b, CT_COMMA); }
static bool sp_cond_0054(chunks_t* c) {return is_type(c->b, CT_COMMA) && is_type_and_ptype(c->a, CT_SQUARE_OPEN, CT_TYPE); }
static bool sp_cond_0055(chunks_t* c) {return is_type(c->b, CT_COMMA) && is_type(c->a, CT_PAREN_OPEN); }

static bool sp_cond_0057(chunks_t* c) {return is_type(c->b, CT_ELLIPSIS) && not_flag(c->a, PCF_PUNCTUATOR); }
static bool sp_cond_0058(chunks_t* c) {return is_type(c->b, CT_ELLIPSIS) && is_type(c->a, CT_TAG_COLON); }

static bool sp_cond_0074(chunks_t* c) {return is_type(c->a, CT_ASSIGN); }
static bool sp_cond_0075(chunks_t* c) {return is_type(c->a, CT_ASSIGN) && is_flag(c->a, PCF_IN_ENUM); }
static bool sp_cond_0076(chunks_t* c) {return is_type(c->a, CT_ASSIGN) && is_ptype(c->a, CT_FUNC_PROTO); }

static bool sp_cond_0095(chunks_t* c) {return is_type (c->a, CT_BYREF) && is_ptype(c->a, CT_FUNC_DEF, CT_FUNC_PROTO); }

static bool sp_cond_0099(chunks_t* c) {return is_type(c->a, CT_SPAREN_CLOSE) && is_type(c->b, CT_BRACE_OPEN    ); }
static bool sp_cond_0100(chunks_t* c) {return is_type(c->a, CT_SPAREN_CLOSE) && !is_cmt(c->b); }

static bool sp_cond_0193(chunks_t* c) {return is_only_first_type(c->b, CT_PTR_TYPE, c->a, CT_IN); }
static bool sp_cond_0250(chunks_t* c) {return is_only_first_type(c->b, CT_PTR_TYPE, c->a, CT_IN) &&  is_type(get_next_nptr(c->b), CT_FUNC_DEF, CT_FUNC_PROTO); }
static bool sp_cond_0251(chunks_t* c) {return is_only_first_type(c->b, CT_PTR_TYPE, c->a, CT_IN) && not_type(get_next_nptr(c->b), CT_WORD); }


static bool sp_qt_check(chunks_t* c)
{
   /* test if we are within a SIGNAL/SLOT call */
   if(QT_SIGNAL_SLOT_found && sp_cond_0051(c) &&
     is_level(c->b, (QT_SIGNAL_SLOT_level)))
   {
      restoreValues = true;
   }
   return false;
}


static bool sp_cond_0181(chunks_t* c)
{
   if (is_type(c->a, CT_CONSTR_COLON))
   {
      c->sp = get_ival(UO_indent_ctor_init_leading) - 1;
      return true;
   }
   else
   return false;
}


static bool sp_cond_0034(chunks_t* c)
{
   if(is_ptype(c->b, CT_COMMENT_END, CT_COMMENT_EMBED))
   {
      c->sp = get_ival(UO_sp_num_before_tr_emb_cmt);
      return true;
   }
   else
   {
      return false;
   }
}


/***************************************************************************//**
 * @brief prototype for a function that checks if a space action is
 * appropriate to be applied to two chunk
 ******************************************************************************/
typedef bool (*sp_check_t)(
   chunks_t* pc  /**< [in]  chunks to use for check and their spacing */
);


typedef void (*sp_act_t)(
   chunks_t* pc,
   uo_t*     opt
);


/***************************************************************************//**
 * @brief prototype for a function that determines if a space is to be
 * added or removed between two chunks
 ******************************************************************************/
typedef void (*sp_log_t)(
   uint32_t    id,      /**< [in] identifier of space check */
   const char* rule,    /**< [in] string that describes the check rule */
   chunks_t*   c,       /**< [in] chunks that where used for check */
   bool        complete /**< [in] flag indicates if it was the last check */
);


/** combines a check with an action function */
typedef struct space_check_action_s
{
   uint32_t   id;    /**< [in] identifier of this space check */
   sp_check_t check; /**< [in] a function that checks condition with two chunks */
   uo_t       opt;   /**< [in] option to derive return argument from */
}sca_t;


forward_list<sca_t> sca_list;   /**< array of virtual brace checks */
static uint32_t sca_count = 0;  /**< number of virtual brace checks */

static void add_sca(sca_t check)
{
   static forward_list<sca_t>::iterator insert_pos = sca_list.before_begin();
   insert_pos = sca_list.insert_after(insert_pos, check);
   sca_count++;
}


void init_space_check_action_array(void)
{
   /* \note the order of the if statements is VERY important. */
   add_sca({  1, sp_cond_0001, UO_always_remove          });
   add_sca({  2, sp_cond_0002, UO_always_ignore          });
   add_sca({  3, sp_cond_0003, UO_sp_pp_concat           });
   add_sca({  4, sp_cond_0004, UO_sp_pp_stringify        });
   add_sca({  5, sp_cond_0005, UO_sp_before_pp_stringify });
   add_sca({  6, sp_cond_0006, UO_always_remove          });
   add_sca({  7, sp_cond_0007, UO_always_remove          });
   add_sca({  8, sp_cond_0008, UO_always_force           });
   add_sca({  9, sp_cond_0009, UO_always_remove          });
   add_sca({ 10, sp_cond_0010, UO_always_remove          });
   add_sca({ 11, sp_cond_0011, UO_always_remove          });
   add_sca({ 12, sp_cond_0012, UO_sp_before_nl_cont      });
   add_sca({ 13, sp_cond_0013, UO_sp_d_array_colon       });

   if(not_ignore(UO_sp_case_label)) { set_arg(UO_sp_case_label, AV_FORCE); }
      add_sca({ 14, sp_cond_0014, UO_sp_case_label});

   add_sca({ 15, sp_cond_0015, UO_sp_after_for_colon });
   add_sca({ 16, sp_cond_0016, UO_sp_before_for_colon});

   if (not_ignore(UO_sp_cond_ternary_short)) {
      add_sca({ 17, sp_cond_0017, UO_sp_cond_ternary_short  }); }
   if (not_ignore(UO_sp_cond_question_before)) {
      add_sca({ 18, sp_cond_0019, UO_sp_cond_question_before}); }
   if (not_ignore(UO_sp_cond_question_after)) {
      add_sca({ 19, sp_cond_0020, UO_sp_cond_question_after }); }
   if (not_ignore(UO_sp_cond_question)) {
      add_sca({ 20, sp_cond_0018,  UO_sp_cond_question      }); }
   if (not_ignore(UO_sp_cond_colon_before)) {
      add_sca({ 21, sp_cond_0027, UO_sp_cond_colon_before   }); }
   if (not_ignore(UO_sp_cond_colon_after)) {
      add_sca({ 22, sp_cond_0028, UO_sp_cond_colon_after    }); }
   if (not_ignore(UO_sp_cond_colon)) {
      add_sca({ 23, sp_cond_0021, UO_sp_cond_colon          }); }
   add_sca({ 24, sp_cond_0022, UO_sp_range     });
   add_sca({ 25, sp_cond_0023, UO_always_remove});

   /* Macro stuff can only return IGNORE, ADD, or FORCE but not REMOVE */
   if(is_arg(UO_sp_macro     , AV_REMOVE)) { set_arg(UO_sp_macro,      AV_FORCE); }
   if(is_arg(UO_sp_macro_func, AV_REMOVE)) { set_arg(UO_sp_macro_func, AV_FORCE); }
      add_sca({ 26, sp_cond_0024, UO_sp_macro});
   add_sca({ 27, sp_cond_0025, UO_sp_macro_func});

   /* Remove spaces, unless we are ignoring. See indent_preproc() */
   if (is_ignore(UO_pp_space)) {
      add_sca({ 28, sp_cond_0026, UO_always_ignore}); }
   else {
      add_sca({ 29, sp_cond_0026, UO_always_remove}); }

   if (not_ignore(UO_sp_before_semi_for_empty)) {
      add_sca({ 30, sp_cond_0031, UO_sp_before_semi_for_empty}); }

   if (not_ignore(UO_sp_before_semi_for)) {
      add_sca({ 31, sp_cond_0030, UO_sp_before_semi_for}); }
   add_sca({ 32, sp_cond_0032, UO_sp_special_semi});
   add_sca({ 33, sp_cond_0029, UO_sp_before_semi });

   if(not_ignore(UO_sp_endif_cmt)) {
      add_sca({ 34, sp_cond_0033, UO_sp_endif_cmt}); }

   if(not_ignore(UO_sp_before_tr_emb_cmt)) {
      add_sca({ 35, sp_cond_0034, UO_sp_before_tr_emb_cmt}); }
   add_sca({ 36, sp_cond_0035, UO_always_remove});
   add_sca({ 37, sp_cond_1035, UO_always_force });
   add_sca({ 38, sp_cond_2035, UO_always_add   });

   /* "for (;;)" vs "for (;; )" and "for (a;b;c)" vs "for (a; b; c)" */
   if(not_ignore(UO_sp_after_semi_for_empty)) {
      add_sca({ 39, sp_cond_0038, UO_sp_after_semi_for_empty}); }

   if(not_ignore(UO_sp_after_semi_for)) {
      add_sca({ 40, sp_cond_0037, UO_sp_after_semi_for}); }
   add_sca({ 41, sp_cond_0242, UO_sp_after_semi});

   /* puts a space in the rare '+-' or '-+' */
   add_sca({ 42, sp_cond_0039, UO_always_add});

   /* "return(a);" vs "return (foo_t)a + 3;" vs "return a;" vs "return;" */
   add_sca({ 43, sp_cond_0041, UO_sp_return_paren});

   /* everything else requires a space */
   add_sca({ 44, sp_cond_0040, UO_always_force});

   /* "sizeof(foo_t)" vs "sizeof foo_t" */
   add_sca({ 45, sp_cond_0043, UO_sp_sizeof_paren});
   add_sca({ 46, sp_cond_0042, UO_always_force   });

   /* handle '::' */
   add_sca({ 47, sp_cond_0044, UO_sp_after_dc});

   /* mapped_file_source abc((int32_t) ::CW2A(sTemp)); */
   add_sca({ 48, sp_cond_0045, UO_always_remove});

   /* '::' at the start of an identifier is not member access,
    * but global scope operator. Detect if previous chunk is keyword */
   add_sca({ 49, sp_cond_0052, UO_always_force});
   add_sca({ 50, sp_cond_0047, UO_sp_before_dc});

   /* "a,b" vs "a, b" */
   /* C# multidimensional array type: ',,' vs ', ,' or ',]' vs ', ]' */
   add_sca({ 51, sp_cond_0050, UO_sp_between_mdatype_commas});
   add_sca({ 52, sp_cond_0049, UO_sp_after_mdatype_commas  });
   add_sca({ 53, sp_cond_0048, UO_sp_after_comma           });

   /* special check that test if we are within a SIGNAL/SLOT
    * if so sets restoreValues = true */
   add_sca({ 54, sp_qt_check, UO_always_ignore} );

   add_sca({ 55, sp_cond_0054, UO_sp_before_mdatype_commas} );
   if(not_ignore(UO_sp_paren_comma)) {
      add_sca({ 56, sp_cond_0055, UO_sp_paren_comma } ); }
   add_sca({ 57, sp_cond_0053, UO_sp_before_comma} );

   /* non-punctuator followed by a ellipsis */
   if(not_ignore(UO_sp_before_ellipsis)) {
      add_sca({ 58, sp_cond_0057, UO_sp_before_ellipsis} ); }
   add_sca({ 59, sp_cond_0058, UO_always_force } );
   add_sca({ 60, sp_cond_0059, UO_always_force } );
   add_sca({ 61, sp_cond_0060, UO_sp_after_tag } );
   add_sca({ 62, sp_cond_0061, UO_always_remove} );
   add_sca({ 63, sp_cond_0062, UO_always_remove} ); /* handle '~' */

   if(not_ignore(UO_sp_catch_paren)) {
      add_sca({ 64, sp_cond_0063, UO_sp_catch_paren} ); }
   if(not_ignore(UO_sp_version_paren)) {
      add_sca({ 65, sp_cond_0064, UO_sp_version_paren} ); }
   if(not_ignore(UO_sp_scope_paren)) {
      add_sca({ 66, sp_cond_0065, UO_sp_scope_paren} ); }

   /* "if (" vs "if(" */
   add_sca({ 67, sp_cond_0066, UO_sp_before_sparen} );
   add_sca({ 68, sp_cond_0067, UO_sp_assign       } );

   /* Handle the special lambda case for C++11: [=](Something arg){.....} */
   if(not_ignore(UO_sp_cpp_lambda_assign)) {
      add_sca({ 69, sp_cond_0068, UO_sp_cpp_lambda_assign} ); }

   /* Handle the special lambda case for C++11: [](Something arg){.....} */
   if(not_ignore(UO_sp_cpp_lambda_paren)) {
      add_sca({ 70, sp_cond_0069, UO_sp_cpp_lambda_paren}); }

   if(not_ignore(UO_sp_enum_paren)) {
      add_sca({ 71, sp_cond_0070, UO_sp_enum_paren}); }

   if(not_ignore(UO_sp_enum_before_assign)) {
      add_sca({ 72, sp_cond_0072, UO_sp_enum_before_assign}); }
   else {
      add_sca({ 73, sp_cond_0072, UO_sp_enum_assign}); }

   if(not_ignore(UO_sp_assign_default)) {
      add_sca({ 74, sp_cond_0073, UO_sp_assign_default}); }

   if(not_ignore(UO_sp_before_assign )) {
      add_sca({ 75, sp_cond_0071, UO_sp_before_assign}); }
   else {
      add_sca({ 76, sp_cond_0071, UO_sp_assign}); }

   if(not_ignore(UO_sp_enum_after_assign)) {
      add_sca({ 77, sp_cond_0075, UO_sp_enum_after_assign}); }
   else {
      add_sca({ 78, sp_cond_0075, UO_sp_enum_assign}); }

   if(not_ignore(UO_sp_assign_default)) {
      add_sca({ 79, sp_cond_0076, UO_sp_assign_default}); }

   if(not_ignore(UO_sp_after_assign)) {
      add_sca({ 80, sp_cond_0074, UO_sp_after_assign}); }
   add_sca({ 81, sp_cond_0074, UO_sp_assign               });
   add_sca({ 82, sp_cond_0077, UO_sp_enum_colon           });
   add_sca({ 83, sp_cond_0079, UO_sp_before_oc_block_caret});
   add_sca({ 84, sp_cond_0080, UO_sp_after_oc_block_caret });
   add_sca({ 85, sp_cond_0081, UO_sp_after_oc_msg_receiver});

   /* "a [x]" vs "a[x]" */
   add_sca({ 86, sp_cond_0082, UO_sp_before_square});

   /* "byte[]" vs "byte []" */
   add_sca({ 87, sp_cond_0083, UO_sp_before_squares});

   if(not_ignore(UO_sp_angle_shift)) {
      add_sca({ 88, sp_cond_0084, UO_sp_angle_shift}); }

   /* spacing around template < > stuff */
   add_sca({ 89, sp_cond_0085, UO_sp_inside_angle});

   if(not_ignore(UO_sp_template_angle)) {
      add_sca({ 90, sp_cond_0087, UO_sp_template_angle}); }
   add_sca({ 91, sp_cond_0086, UO_sp_before_angle});

   if(not_ignore(UO_sp_angle_word)) {
      add_sca({ 92, sp_cond_0089, UO_sp_angle_word}); }

   add_sca({ 93, sp_cond_0245, UO_sp_angle_paren_empty});
   add_sca({ 94, sp_cond_0090, UO_sp_angle_paren      });
   add_sca({ 95, sp_cond_0092, UO_sp_before_dc        });
   add_sca({ 96, sp_cond_0093, UO_sp_after_angle      });

   if(not_ignore(UO_sp_after_byref_func)) {
      add_sca({ 97, sp_cond_0095, UO_sp_after_byref_func}); }
   add_sca({ 98, sp_cond_0096, UO_sp_after_byref});

   if(not_ignore(UO_sp_before_byref_func)) {
      add_sca({ 99, sp_cond_0243, UO_sp_before_byref_func}); }

   if(not_ignore(UO_sp_before_unnamed_byref)) {
      add_sca({100, sp_cond_0244, UO_sp_before_unnamed_byref}); }
   add_sca({101, sp_cond_0097, UO_sp_before_byref});

   if (not_ignore(UO_sp_sparen_brace)) {
      add_sca({102, sp_cond_0099, UO_sp_sparen_brace}); }

   if (not_ignore(UO_sp_after_sparen)) {
      add_sca({103, sp_cond_0100, UO_sp_after_sparen}); }

   if(not_ignore(UO_sp_after_operator_sym)) {
      if(not_ignore(UO_sp_after_operator_sym_empty)) {
         add_sca({104, sp_cond_0135, UO_sp_after_operator_sym_empty}); }
      add_sca({105, sp_cond_0101, UO_sp_after_operator_sym}); }

   /* spaces between function and open parenthesis */
   if(not_ignore(UO_sp_func_call_paren_empty)){
      add_sca({106, sp_cond_0103, UO_sp_func_call_paren_empty}); }
   add_sca({107, sp_cond_0104, UO_sp_func_call_paren});

   add_sca({108, sp_cond_0105, UO_sp_func_call_user_paren});
   add_sca({109, sp_cond_0106, UO_sp_attribute_paren     });

   if(not_ignore(UO_sp_func_def_paren_empty)) {
      add_sca({110, sp_cond_0109, UO_sp_func_def_paren_empty}); }
   add_sca({111, sp_cond_0107, UO_sp_func_def_paren});

   add_sca({112, sp_cond_0111, UO_sp_cpp_cast_paren});
   add_sca({113, sp_cond_0112, UO_always_force});

   /* "(int32_t)a" vs "(int32_t) a" or "cast(int32_t)a" vs "cast(int32_t) a" */
   add_sca({114, sp_cond_0114, UO_sp_after_cast});

   /* Must be an indirect/chained function call? */
   add_sca({115, sp_cond_0113, UO_always_remove});

   /* handle the space between parenthesis in fcn type 'void (*f)(void)' */
   add_sca({116, sp_cond_0115, UO_sp_after_tparen_close});

   /* ")(" vs ") (" */
   add_sca({117, sp_cond_0116, UO_sp_cparen_oparen});

   if(not_ignore(UO_sp_func_proto_paren_empty)) {
      add_sca({118, sp_cond_0118, UO_sp_func_proto_paren_empty}); }
   add_sca({119, sp_cond_0117, UO_sp_func_proto_paren});

   if(not_ignore(UO_sp_func_class_paren_empty)) {
      add_sca({120, sp_cond_0121, UO_sp_func_class_paren_empty}); }
   add_sca({121, sp_cond_0120, UO_sp_func_class_paren});

   add_sca({122, sp_cond_0123, UO_always_force           });
   add_sca({123, sp_cond_0124, UO_sp_inside_braces_empty });
   add_sca({124, sp_cond_0126, UO_sp_inside_braces_enum  });
   add_sca({125, sp_cond_0127, UO_sp_inside_braces_struct});
   add_sca({126, sp_cond_0125, UO_sp_inside_braces       });
   add_sca({127, sp_cond_0128, UO_always_remove          });
   add_sca({128, sp_cond_0129, UO_sp_defined_paren       });
   add_sca({129, sp_cond_0131, UO_sp_throw_paren         });
   add_sca({130, sp_cond_0130, UO_sp_after_throw         });
   add_sca({131, sp_cond_0132, UO_sp_this_paren          });
   add_sca({132, sp_cond_0133, UO_always_add             });
   add_sca({133, sp_cond_0134, UO_always_remove          });
   add_sca({134, sp_cond_0140, UO_sp_member              });

   /* always remove space(s) after then '.' of a C99-member */
   add_sca({135, sp_cond_0136, UO_always_remove     });
   add_sca({136, sp_cond_0137, UO_sp_super_paren    });
   add_sca({137, sp_cond_0139, UO_sp_fparen_dbrace  });
   add_sca({138, sp_cond_0138, UO_sp_fparen_brace   });
   add_sca({139, sp_cond_0141, UO_always_remove     });
   add_sca({140, sp_cond_0142, UO_sp_else_brace     });
   add_sca({141, sp_cond_0143, UO_always_force      });
   add_sca({142, sp_cond_0144, UO_sp_catch_brace    });
   add_sca({143, sp_cond_0145, UO_sp_finally_brace  });
   add_sca({144, sp_cond_0146, UO_sp_try_brace      });
   add_sca({145, sp_cond_0147, UO_sp_getset_brace   });
   add_sca({146, sp_cond_0149, UO_sp_word_brace_ns  });
   add_sca({147, sp_cond_0150, UO_sp_word_brace     });
   add_sca({148, sp_cond_0151, UO_sp_invariant_paren});

   add_sca({149, sp_cond_0153, UO_always_force});
   add_sca({150, sp_cond_0154, UO_sp_after_invariant_paren});

   /* Arithmetics after a cast comes first */
   add_sca({151, sp_cond_0155, UO_sp_arith});

   /* "(struct foo) {...}" vs "(struct foo){...}" */
   add_sca({152, sp_cond_0156, UO_sp_paren_brace});

   /* D-specific: "delegate(some thing) dg */
   add_sca({153, sp_cond_0157, UO_always_add});

   /* PAWN-specific: "state (condition) next" */
   add_sca({154, sp_cond_0158, UO_always_add});

   /* C++ new operator: new(bar) Foo */
   add_sca({155, sp_cond_0159, UO_sp_after_newop_paren});

   /* "foo(...)" vs "foo( ... )" */
   add_sca({156, sp_cond_0160, UO_sp_inside_fparens});
   add_sca({157, sp_cond_0091, UO_sp_inside_fparen});

   /* "foo(...)" vs "foo( ... )" */
   add_sca({158, sp_cond_0161, UO_sp_inside_tparen});

   add_sca({159, sp_cond_0163, UO_sp_after_oc_return_type});
   add_sca({160, sp_cond_0164, UO_sp_after_oc_type});
   add_sca({161, sp_cond_0165, UO_sp_after_oc_at_sel_parens});

   if(not_ignore(UO_sp_inside_oc_at_sel_parens)) {
   add_sca({162, sp_cond_0166, UO_sp_inside_oc_at_sel_parens}); }
   add_sca({163, sp_cond_0167, UO_sp_after_oc_at_sel});

   /* C cast:   "(int32_t)"      vs "( int )"
    * D cast:   "cast(int32_t)"  vs "cast( int )"
    * CPP cast: "int(a + 3)" vs "int( a + 3 )" */
   add_sca({164, sp_cond_0169, UO_sp_inside_paren_cast});

   if(not_ignore(UO_sp_inside_newop_paren_open)) {
      add_sca({165, sp_cond_0170, UO_sp_inside_newop_paren_open}); }
   if(not_ignore(UO_sp_inside_newop_paren)) {
      add_sca({166, sp_cond_0170, UO_sp_inside_newop_paren}); }

   add_sca({167, sp_cond_0168, UO_sp_inside_paren});
   add_sca({168, sp_cond_0172, UO_sp_inside_paren_cast});

   if(not_ignore(UO_sp_inside_newop_paren_close)) {
      add_sca({169, sp_cond_0173, UO_sp_inside_newop_paren_close}); }
   if(not_ignore(UO_sp_inside_newop_paren)) {
      add_sca({170, sp_cond_0173, UO_sp_inside_newop_paren}); }

   add_sca({171, sp_cond_0171, UO_sp_inside_paren});

   /* "[3]" vs "[ 3 ]" */
   add_sca({172, sp_cond_0174, UO_sp_inside_square});
   add_sca({173, sp_cond_0175, UO_sp_square_fparen});

   /* "if(...)" vs "if( ... )" */
   if(not_ignore(UO_sp_inside_sparen_close)) {
      add_sca({174, sp_cond_0176, UO_sp_inside_sparen_close}); }
   if(not_ignore(UO_sp_inside_sparen_open)) {
      add_sca({175, sp_cond_0177, UO_sp_inside_sparen_open}); }

   add_sca({176, sp_cond_0178, UO_sp_inside_sparen});

   if(not_ignore(UO_sp_after_class_colon)) {
      add_sca({177, sp_cond_0179, UO_sp_after_class_colon}); }
   if(not_ignore(UO_sp_before_class_colon)) {
      add_sca({178, sp_cond_0180, UO_sp_before_class_colon}); }

   /* default indent is 1 space */
   if(not_ignore(UO_sp_after_constr_colon)) {
      add_sca({179, sp_cond_0181, UO_sp_after_constr_colon}); }

   if(not_ignore(UO_sp_before_constr_colon)) {
         add_sca({180, sp_cond_0182, UO_sp_before_constr_colon}); }
   if(not_ignore(UO_sp_before_case_colon)) {
         add_sca({181, sp_cond_0183, UO_sp_before_case_colon}); }

   add_sca({182, sp_cond_0184, UO_always_remove});
   add_sca({183, sp_cond_0185, UO_always_add   });
   add_sca({184, sp_cond_0186, UO_sp_member    });
   add_sca({185, sp_cond_0187, UO_sp_arith     });

   if(not_tok(UO_pos_bool, TP_IGNORE)) {
      /* if UP_pos_bool is different from ignore its AV_ADD option is required */
      if(not_arg(UO_sp_bool, AV_REMOVE)) { add_arg(UO_sp_bool, AV_ADD); }
      add_sca({186, sp_cond_1188, UO_sp_bool}); }
   add_sca({187, sp_cond_0188, UO_sp_bool});

   add_sca({188, sp_cond_0189, UO_sp_compare});
   add_sca({189, sp_cond_0190, UO_always_remove});

   if(not_ignore(UO_sp_ptr_star_paren)) {
         add_sca({190, sp_cond_0246, UO_sp_ptr_star_paren}); }
   if(not_ignore(UO_sp_between_pstar)) {
         add_sca({191, sp_cond_0191, UO_sp_between_pstar}); }
   if(not_ignore(UO_sp_after_ptr_star_func)) {
         add_sca({192, sp_cond_0241, UO_sp_after_ptr_star_func}); }

   add_sca({193, sp_cond_0194, UO_sp_deref});

   if(not_ignore(UO_sp_after_pstar_qualifier)) {
         add_sca({194, sp_cond_0195, UO_sp_after_pstar_qualifier}); }
   if(not_ignore(UO_sp_after_pstar)) {
         add_sca({195, sp_cond_0192, UO_sp_after_pstar}); }

   /* Find the next non-'*' chunk */
   if(not_ignore(UO_sp_before_ptr_star_func)) {
            add_sca({196, sp_cond_0250, UO_sp_before_ptr_star_func}); }

   if(not_ignore(UO_sp_before_unnamed_pstar)) {
            add_sca({197, sp_cond_0251, UO_sp_before_unnamed_pstar}); }

   if(not_ignore(UO_sp_before_ptr_star)) {
            add_sca({198, sp_cond_0193, UO_sp_before_ptr_star}); }

   add_sca({199, sp_cond_0196, UO_sp_after_operator});

   set_arg(UO_sp_type_func_add, add_option(get_arg(UO_sp_type_func), AV_ADD));
   add_sca({200, sp_cond_0198, UO_sp_type_func_add});
   add_sca({201, sp_cond_0197, UO_sp_type_func});

   /*"(int32_t)a" vs "(int32_t) a" or "cast(int32_t)a" vs "cast(int32_t) a" */
   add_sca({202, sp_cond_0199, UO_sp_after_cast});

   add_sca({203, sp_cond_0201, UO_sp_brace_else});
   add_sca({204, sp_cond_0202, UO_sp_brace_catch});
   add_sca({205, sp_cond_0203, UO_sp_brace_finally});

   add_sca({206, sp_cond_0205, UO_sp_inside_braces_enum});
   add_sca({207, sp_cond_0206, UO_sp_inside_braces_struct});
   add_sca({208, sp_cond_0207, UO_sp_inside_braces});

   add_sca({209, sp_cond_0209, UO_sp_inside_braces_enum});
   add_sca({210, sp_cond_0210, UO_sp_inside_braces_struct});
   add_sca({211, sp_cond_0208, UO_sp_inside_braces});

   add_sca({212, sp_cond_0211, UO_sp_brace_typedef});
   add_sca({213, sp_cond_0212, UO_sp_before_sparen});
   add_sca({214, sp_cond_0213, UO_sp_before_template_paren});

   if(not_arg(UO_sp_after_type, AV_REMOVE)) {
      add_sca({215, sp_cond_0214, UO_sp_after_type}); }
   else {
      add_sca({216, sp_cond_0214, UO_always_force}); }

   add_sca({217, sp_cond_0216, UO_sp_func_call_paren});
   add_sca({218, sp_cond_0215, UO_always_ignore});

   /* If nothing claimed the PTR_TYPE, then return ignore */
   add_sca({219, sp_cond_0217, UO_always_ignore});
   add_sca({220, sp_cond_0218, UO_sp_not});
   add_sca({221, sp_cond_0219, UO_sp_inv});
   add_sca({222, sp_cond_0220, UO_sp_addr});
   add_sca({223, sp_cond_0221, UO_sp_deref});
   add_sca({224, sp_cond_0222, UO_sp_sign});
   add_sca({225, sp_cond_0223, UO_sp_incdec});
   add_sca({226, sp_cond_0224, UO_always_remove});
   add_sca({227, sp_cond_0225, UO_always_force});
   add_sca({228, sp_cond_0226, UO_sp_after_oc_scope});
   add_sca({229, sp_cond_0227, UO_sp_after_oc_dict_colon});
   add_sca({230, sp_cond_0228, UO_sp_before_oc_dict_colon});

   add_sca({231, sp_cond_0230, UO_sp_after_send_oc_colon});
   add_sca({232, sp_cond_0229, UO_sp_after_oc_colon});
   add_sca({233, sp_cond_0232, UO_sp_before_send_oc_colon});
   add_sca({234, sp_cond_0231, UO_sp_before_oc_colon});
   add_sca({235, sp_cond_0247, UO_always_force});
   add_sca({236, sp_cond_0233, UO_always_ignore});
   add_sca({237, sp_cond_0234, UO_always_force});

   /* c# new Constraint, c++ new operator */
   add_sca({238, sp_cond_0235, UO_sp_between_new_paren});
   add_sca({239, sp_cond_0236, UO_sp_after_new});
   add_sca({240, sp_cond_0237, UO_sp_annotation_paren});
   add_sca({241, sp_cond_0238, UO_sp_after_oc_property});
   add_sca({242, sp_cond_0239, UO_sp_extern_paren});

   /* "((" vs "( (" or "))" vs ") )" */
   add_sca({243, sp_cond_0240, UO_sp_paren_paren});

   /* mapped_file_source abc((int32_t) A::CW2A(sTemp)); */
   add_sca({244, sp_cond_0248, UO_always_remove});
}


/* this function is called for every chunk in the input file.
 * Thus it is important to keep this function efficient */
static argval_t do_space(chunk_t* pc1, chunk_t* pc2, uint32_t& min_sp, bool complete = true)
{
   LOG_FUNC_ENTRY();
   min_sp = 1; /* default space count is 1 */
   retval_if(are_invalid(pc1, pc2), AV_IGNORE);

   LOG_FMT(LSPACE, "%s: %u:%u %s %s\n", __func__, pc1->orig_line,
         pc1->orig_col, pc1->text(), get_token_name(pc1->type));

   chunks_t chunks = {pc1, pc2, min_sp};
   chunks_t* pc    = &chunks;

   for (sca_t& sca: sca_list)
   {
      if(sca.check(pc))
      {
         log_space(sca.id, sca.opt, pc, complete);
         return(get_arg(sca.opt));
      }
   }

   /* this table lists out all combos where a space should NOT be
    * present CT_UNKNOWN is a wildcard. */
   for (auto it : no_space_table)
   {
      if (((it.first  == CT_UNKNOWN) || (it.first  == pc1->type)) &&
          ((it.second == CT_UNKNOWN) || (it.second == pc2->type)) )
       {
         log_opt_return(UO_always_remove);
       }
   }

#ifdef DEBUG
   LOG_FMT(LSPACE, "\n\n(%d) %s: WARNING: unrecognized do_space: first: %u:%u %s %s and second: %u:%u %s %s\n\n\n",
           __LINE__, __func__,
           pc1->orig_line, pc1->orig_col, pc1->text(), get_token_name(pc1->type),
           pc2->orig_line, pc2->orig_col, pc2->text(), get_token_name(pc2->type));
#endif

   /* if no condition was hit, assume a space shall be added */
   log_opt_return(UO_always_add);
}


void space_text(void)
{
   LOG_FUNC_ENTRY();

   chunk_t* pc = chunk_get_head();
   return_if(is_invalid(pc));

   chunk_t* next;
   uint32_t prev_column;
   uint32_t column = pc->column;
   while (is_valid(pc))
   {
      LOG_FMT(LSPACE, "%s: %u:%u %s %s\n",
              __func__, pc->orig_line, pc->orig_col, pc->text(), get_token_name(pc->type));
      if ((is_true(UO_use_options_overriding_for_qt_macros)) &&
          ((strcmp(pc->text(), "SIGNAL") == 0) ||
           (strcmp(pc->text(), "SLOT"  ) == 0) ) )
      {
         LOG_FMT(LSPACE, "%u: [%d] type %s SIGNAL/SLOT found\n",
                 pc->orig_line, __LINE__, get_token_name(pc->type));
         set_flags(pc, PCF_IN_QT_MACRO); /* flag the chunk for a second processing */

         save_set_options_for_QT(pc->level);
      }
      if (is_true(UO_sp_skip_vbrace_tokens))
      {
         next = chunk_get_next(pc);
         while ( ( chunk_empty(next)) &&
                 (!is_nl(next)) &&
                 is_type(next, CT_VBRACE_OPEN, CT_VBRACE_CLOSE))
         {
            assert(is_valid(next));
            LOG_FMT(LSPACE, "%s: %u:%u Skip %s (%u+%u)\n",
                    __func__, next->orig_line, next->orig_col, get_token_name(next->type),
                    pc->column, pc->str.size());
            next->column = pc->column + pc->str.size();
            next         = chunk_get_next(next);
         }
      }
      else { next = pc->next; }

      break_if(is_invalid(next));

      if (QT_SIGNAL_SLOT_found && is_true(UO_sp_bal_nested_parens))
      {
         if (is_type(next->next, CT_SPACE))
         {
            chunk_del(next->next); /* remove the space */
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
             !is_str(pc, "[]") &&
             !is_str(pc, "{{") &&
             !is_str(pc, "}}") &&
             !is_str(pc, "()") &&
             !pc->str.startswith("@\""))
         {
            /* Find the next non-empty chunk on this line */
            chunk_t *tmp = next;
            while (is_valid(tmp) &&
                  (tmp->len() == 0) && /* \todo better use chunk_search here */
                  !is_nl(tmp))
            {
               tmp = chunk_get_next(tmp);
            }
            if ((is_valid(tmp)) && (tmp->len() > 0))
            {
               bool kw1 = CharTable::IsKW2((uint32_t)(pc->str[pc->len()-1]));
               bool kw2 = CharTable::IsKW1((uint32_t)(next->str[0]));
               if ((kw1 == true) && (kw2 == true) )
               {
                  /* back-to-back words need a space */
                  set_flags(pc, PCF_FORCE_SPACE);
               }
               /* \todo what is the meaning of 4 */
               else if ( (kw1 == false) && (pc->len()   < 4) &&
                         (kw2 == false) && (next->len() < 4) )
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
                     if (((is_lang(cpd, LANG_CPP ) && is_true(UO_sp_permit_cpp11_shift)) ||
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

         uint32_t min_sp;
         argval_t av = do_space(pc, next, min_sp, false);
         min_sp = max(1u, min_sp);
         if (is_flag(pc, PCF_FORCE_SPACE))
         {
            LOG_FMT(LSPACE, " <force between '%s' and '%s'>", pc->text(), next->text());
            av = add_option(av, AV_ADD);
         }
         switch (av)
         {
            case AV_FORCE:
               column += min_sp; /* add exactly the specified number of spaces */
            break;

            case AV_ADD:
            {
               uint32_t delta = min_sp;
               if ((next->orig_col >= pc->orig_col_end) && (pc->orig_col_end != 0))
               {
                  /* Keep the same relative spacing, minimum 1 */
                  delta = next->orig_col - pc->orig_col_end;
                  delta = max(delta, min_sp);
               }
               column += delta;
            }
            break;

            case AV_IGNORE:
               /* Keep the same relative spacing, if possible */
               if ((next->orig_col >= pc->orig_col_end) && (pc->orig_col_end != 0))
               {
                  column += next->orig_col - pc->orig_col_end;
               }
            break;

            case AV_REMOVE:      /* the symbols will be back-to-back "a+3" */
            case AV_NOT_DEFINED: /* unknown argument value */
            default:
                                 /* do nothing */
            break;
         }


         if (is_cmt(next) && is_nl(chunk_get_next(next)) &&
             (column < next->orig_col) )
         {
            /* do some comment adjustments if sp_before_tr_emb_cmt and
             * sp_endif_cmt did not apply. */
            if ((is_ignore(UO_sp_before_tr_emb_cmt) || not_ptype(next, 2, CT_COMMENT_END, CT_COMMENT_EMBED) ) &&
                (is_ignore(UO_sp_endif_cmt        ) || not_type (pc,      CT_PP_ELSE,     CT_PP_ENDIF     ) ) )
            {
               if (is_true(UO_indent_rel_single_line_comments))
               {
                  /* Try to keep relative spacing between tokens */
                  LOG_FMT(LSPACE, " <relative adj>");
                  column = pc->column + 1 + (next->orig_col - pc->orig_col_end);
               }
               else
               {
                  /* If there was a space, we need to force one, otherwise
                   * try to keep the comment in the same column. */
                  uint32_t col_min = pc->column + pc->len() + ((next->orig_prev_sp > 0) ? 1 : 0);
                  column = next->orig_col;
                  column = max(column, col_min);
                  LOG_FMT(LSPACE, " <relative set>");
               }
            }
         }
         next->column = column;

         LOG_FMT(LSPACE, " = %s @ %u => %u\n", argval2str(av).c_str(),
                 column - prev_column, next->column);
         if (restoreValues) { restore_options_for_QT(); }
      }

      pc = next;
      if (QT_SIGNAL_SLOT_found)
      {
         /* flag the chunk for a second processing */
         set_flags(pc, PCF_IN_QT_MACRO);
      }
   }
}


void space_text_balance_nested_parens(void)
{
   LOG_FUNC_ENTRY();

   chunk_t* first = chunk_get_head();
   while (is_valid(first))
   {
      chunk_t* next = chunk_get_next(first);
      break_if(is_invalid(next));

      if (is_str(first, "(") && /* if there are two successive */
          is_str(next,  "(") )  /* opening parenthesis */
      {
         space_add_after(first, 1); /* insert a space between them */

         chunk_t* cur  = next;
         chunk_t* prev = next;
         while ((cur = chunk_get_next(cur)) != nullptr) /* find the closing parenthesis */
         {                                              /* that matches the */
            if (cur->level == first->level)             /* first open parenthesis */
            {
               space_add_after(prev, 1);                /* and force a space before it */
               break;
            }
            prev = cur;
         }
      }
      else if (is_str(first, ")") && /* if there are two successive */
               is_str(next,  ")") )  /* closing parenthesis */
      {
         space_add_after(first, 1);  /* insert a space between them */
      }

      first = next;
   }
}


uint32_t how_many_spaces_are_needed(chunk_t* first, chunk_t* second)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPACE, "%s\n", __func__);

   uint32_t min_sp;
   argval_t arg = do_space(first, second, min_sp);

   uint32_t num_spaces = second->orig_col > (first->orig_col + first->len());
   if     (is_arg_set(arg, AV_ADD)) { num_spaces = max(1u, min_sp); }
   else if(is_arg(arg, AV_REMOVE))  { num_spaces = 0; }
   return num_spaces;
}


uint32_t space_col_align(chunk_t* first, chunk_t* second)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(first, second));

   LOG_FMT(LSPACE, "%s: %u:%u [%s/%s] '%s' <==> %u:%u [%s/%s] '%s'",
           __func__, first->orig_line, first->orig_col,
           get_token_name(first->type), get_token_name(first->ptype),
           first->text(), second->orig_line, second->orig_col,
           get_token_name(second->type), get_token_name(second->ptype),
           second->text());
   log_func_stack_inline(LSPACE);

   uint32_t min_sp;
   argval_t av = do_space(first, second, min_sp);

   LOG_FMT(LSPACE, "%s: av=%d, ", __func__, av);
   uint32_t coldiff;
   if (first->nl_count > 0)
   {
      LOG_FMT(LSPACE, "nl_count=%u, orig_col_end=%u", first->nl_count, first->orig_col_end);
      coldiff = first->orig_col_end - 1u;
   }
   else
   {
      LOG_FMT(LSPACE, "len=%u", first->len());
      coldiff = first->len();
   }

   if( is_arg_set(av, AV_ADD   ) ||
      (is_arg    (av, AV_IGNORE) && (second->orig_col > (first->orig_col + first->len()))))
   {
      coldiff++;
   }

   LOG_FMT(LSPACE, " => %u\n", coldiff);
   return(coldiff);
}


#define MAX_SPACE_COUNT 16u
void space_add_after(chunk_t* pc, uint32_t count)
{
   LOG_FUNC_ENTRY();
   // return_if(count == 0);

   chunk_t* next = chunk_get_next(pc);

   /* don't add at the end of the file or before a newline */
   return_if(is_invalid(next) || is_nl(next));

   count = min(count, MAX_SPACE_COUNT); /* limit to max space count value */

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
