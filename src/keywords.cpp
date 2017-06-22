/**
 * @file keywords.cpp
 * Manages the table of keywords.
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */
#include <cstring>
#include <cstdlib>
#include <map>
#include "args.h"
#include "cfg_file.h"
#include "char_table.h"
#include "chunk_list.h"
#include "keywords.h"
#include "unc_ctype.h"
#include "uncrustify.h"
#include "uncrustify_types.h"

using namespace std;

/* Dynamic keyword map */
typedef map<string, c_token_t> dkwmap;
static dkwmap dkwm;


/**
 * Compares two chunk_tag_t entries using strcmp on the strings
 *
 * @retval ==0 - if both keywords are equal
 * @retval < 0 - p1 is smaller than p2
 * @retval > 0 - p2 is smaller than p1
 */
static int32_t kw_compare(
   const void* p1,  /**< [in] The 'left'  entry */
   const void* p2   /**< [in] The 'right' entry */
);


/**
 * search in static keywords for first occurrence of a given tag
 */
static const chunk_tag_t* kw_static_first(
   const chunk_tag_t* tag  /**< [in] tag/keyword to search for */
);


static const chunk_tag_t* kw_static_match(
   const chunk_tag_t* tag  /**< [in] tag/keyword to search for */
);


/**
 * interesting static keywords - keep sorted.
 * Table includes the Name, Type, and Language flags.
 */
// \todo it might be useful if users could add there custom keywords to
// this list
static const chunk_tag_t keywords[] =
{
   { "@catch",           CT_CATCH,         LANG_CCPPO      },
   { "@dynamic",         CT_OC_DYNAMIC,    LANG_CCPPO      },
   { "@end",             CT_OC_END,        LANG_CCPPO      },
   { "@finally",         CT_FINALLY,       LANG_CCPPO      },
   { "@implementation",  CT_OC_IMPL,       LANG_CCPPO      },
   { "@interface",       CT_OC_INTF,       LANG_CCPPO      },
   { "@interface",       CT_CLASS,         LANG_JAVA       },
   { "@private",         CT_PRIVATE,       LANG_CCPPO      },
   { "@property",        CT_OC_PROPERTY,   LANG_CCPPO      },
   { "@protocol",        CT_OC_PROTOCOL,   LANG_CCPPO      },
   { "@selector",        CT_OC_SEL,        LANG_CCPPO      },
   { "@synthesize",      CT_OC_DYNAMIC,    LANG_CCPPO      },
   { "@throw",           CT_THROW,         LANG_OC         },
   { "@try",             CT_TRY,           LANG_CCPPO      },
   { "NS_ENUM",          CT_ENUM,          LANG_OC         },
   { "NS_OPTIONS",       CT_ENUM,          LANG_OC         },
   { "Q_EMIT",           CT_Q_EMIT,        LANG_CPP        },
   { "Q_FOREACH",        CT_FOR,           LANG_CPP        },
   { "Q_FOREVER",        CT_Q_FOREVER,     LANG_CPP        },
   { "Q_GADGET",         CT_Q_GADGET,      LANG_CPP        },
   { "Q_OBJECT",         CT_COMMENT_EMBED, LANG_CPP        },
   { "_Bool",            CT_TYPE,          LANG_CPP        },
   { "_Complex",         CT_TYPE,          LANG_CPP        },
   { "_Imaginary",       CT_TYPE,          LANG_CPP        },
   { "__DI__",           CT_DI,            LANG_CCPP       },
   { "__HI__",           CT_HI,            LANG_CCPP       },
   { "__QI__",           CT_QI,            LANG_CCPP       },
   { "__SI__",           CT_SI,            LANG_CCPP       },
   { "__asm__",          CT_ASM,           LANG_CCPP       },
   { "__attribute__",    CT_ATTRIBUTE,     LANG_CCPP       },
   { "__block",          CT_QUALIFIER,     LANG_OC         },
   { "__const__",        CT_QUALIFIER,     LANG_CCPP       },
   { "__except",         CT_CATCH,         LANG_CCPP       },
   { "__finally",        CT_FINALLY,       LANG_CCPP       },
   { "__inline__",       CT_QUALIFIER,     LANG_CCPP       },
   { "__nothrow__",      CT_NOTHROW,       LANG_CCPP       },
   { "__restrict",       CT_QUALIFIER,     LANG_CCPP       },
   { "__signed__",       CT_TYPE,          LANG_CCPP       },
   { "__thread",         CT_QUALIFIER,     LANG_CCPP       },
   { "__traits",         CT_QUALIFIER,     LANG_D          },
   { "__try",            CT_TRY,           LANG_CCPP       },
   { "__typeof__",       CT_SIZEOF,        LANG_CCPP       },
   { "__volatile__",     CT_QUALIFIER,     LANG_CCPP       },
   { "__word__",         CT_WORD_,         LANG_CCPP       },
   { "abstract",         CT_QUALIFIER,     LANG_DCSJVE     },
   { "add",              CT_GETSET,        LANG_CS         },
   { "alias",            CT_QUALIFIER,     LANG_D          },
   { "align",            CT_ALIGN,         LANG_D          },
   { "alignof",          CT_SIZEOF,        LANG_CCPP       },
   { "and",              CT_SBOOL,         LANG_CCPPF      },
   { "and_eq",           CT_SASSIGN,       LANG_CCPP       },
   { "as",               CT_AS,            LANG_CSV        },
   { "asm",              CT_ASM,           LANG_CCPPD,     },
   { "asm",              CT_PP_ASM,        LANG_ALLPP      },
   { "assert",           CT_ASSERT,        LANG_JAVA       },
   { "assert",           CT_FUNCTION,      LANG_DP         },
   { "assert",           CT_PP_ASSERT,     LANG_PPP        },
   { "auto",             CT_TYPE,          LANG_CCPPD,     },
   { "base",             CT_BASE,          LANG_CSV        },
   { "bit",              CT_TYPE,          LANG_D          },
   { "bitand",           CT_ARITH,         LANG_CCPP       },
   { "bitor",            CT_ARITH,         LANG_CCPP       },
   { "body",             CT_BODY,          LANG_D          },
   { "bool",             CT_TYPE,          LANG_CPPCSV     },
   { "boolean",          CT_TYPE,          LANG_JE         },
   { "break",            CT_BREAK,         LANG_ALL        },
   { "byte",             CT_TYPE,          LANG_CSDJE      },
   { "callback",         CT_QUALIFIER,     LANG_VALA       },
   { "case",             CT_CASE,          LANG_ALL        },
   { "cast",             CT_D_CAST,        LANG_D          },
   { "catch",            CT_CATCH,         LANG_CPPDCSJVE  },
   { "cdouble",          CT_TYPE,          LANG_D          },
   { "cent",             CT_TYPE,          LANG_D          },
   { "cfloat",           CT_TYPE,          LANG_D          },
   { "char",             CT_CHAR,          LANG_PAWN       },
   { "char",             CT_TYPE,          LANG_ALLC       },
   { "checked",          CT_QUALIFIER,     LANG_CS         },
   { "class",            CT_CLASS,         LANG_CPPDCSJVE  },
   { "compl",            CT_ARITH,         LANG_CCPP       },
   { "const",            CT_QUALIFIER,     LANG_ALL        },
   { "const_cast",       CT_TYPE_CAST,     LANG_CPP        },
   { "constexpr",        CT_QUALIFIER,     LANG_CPP        },
   { "construct",        CT_CONSTRUCT,     LANG_VALA       },
   { "continue",         CT_CONTINUE,      LANG_ALL        },
   { "creal",            CT_TYPE,          LANG_D          },
   { "dchar",            CT_TYPE,          LANG_D          },
   { "debug",            CT_DEBUG,         LANG_D          },
   { "debugger",         CT_DEBUGGER,      LANG_ECMA       },
   { "decltype",         CT_SIZEOF,        LANG_CPP        },
   { "default",          CT_DEFAULT,       LANG_ALL        },
   { "define",           CT_PP_DEFINE,     LANG_ALLPP      },
   { "defined",          CT_DEFINED,       LANG_PAWN       },
   { "defined",          CT_PP_DEFINED,    LANG_ALLCPP     },
   { "delegate",         CT_DELEGATE,      LANG_DCSV       },
   { "delete",           CT_DELETE,        LANG_CPPDVE     },
   { "deprecated",       CT_QUALIFIER,     LANG_D          },
   { "do",               CT_DO,            LANG_ALL        },
   { "double",           CT_TYPE,          LANG_ALLC       },
   { "dynamic_cast",     CT_TYPE_CAST,     LANG_CPP        },
   { "elif",             CT_PP_ELSE,       LANG_ALLCPP     },
   { "else",             CT_ELSE,          LANG_ALL        },
   { "else",             CT_PP_ELSE,       LANG_ALLPP      },
   { "elseif",           CT_PP_ELSE,       LANG_PPP        },
   { "emit",             CT_PP_EMIT,       LANG_PPP        },
   { "endif",            CT_PP_ENDIF,      LANG_ALLPP      },
   { "endinput",         CT_PP_ENDINPUT,   LANG_PPP        },
   { "endregion",        CT_PP_ENDREGION,  LANG_ALLPP      },
   { "endscript",        CT_PP_ENDINPUT,   LANG_PPP        },
   { "enum",             CT_ENUM,          LANG_ALL        },
   { "error",            CT_PP_ERROR,      LANG_PPP        },
   { "event",            CT_TYPE,          LANG_CS         },
   { "exit",             CT_FUNCTION,      LANG_PAWN       },
   { "explicit",         CT_TYPE,          LANG_CCPPCS     },
   { "export",           CT_EXPORT,        LANG_CCPPDE     },
   { "extends",          CT_QUALIFIER,     LANG_JE         },
   { "extern",           CT_EXTERN,        LANG_CCPPDCSV   },
   { "false",            CT_WORD,          LANG_CPPDCSJV   },
   { "file",             CT_PP_FILE,       LANG_PPP        },
   { "final",            CT_QUALIFIER,     LANG_CPPDE      },
   { "finally",          CT_FINALLY,       LANG_DCSJVE     },
   { "flags",            CT_TYPE,          LANG_VALA       },
   { "float",            CT_TYPE,          LANG_ALLC       },
   { "for",              CT_FOR,           LANG_ALL        },
   { "foreach",          CT_FOR,           LANG_DCSV       },
   { "foreach_reverse",  CT_FOR,           LANG_D          },
   { "forward",          CT_FORWARD,       LANG_PAWN       },
   { "friend",           CT_FRIEND,        LANG_CPP        },
   { "function",         CT_FUNCTION,      LANG_DE         },
   { "get",              CT_GETSET,        LANG_CSV        },
   { "goto",             CT_GOTO,          LANG_ALL        },
   { "idouble",          CT_TYPE,          LANG_D          },
   { "if",               CT_IF,            LANG_ALL        },
   { "if",               CT_PP_IF,         LANG_ALLPP      },
   { "ifdef",            CT_PP_IF,         LANG_ALLCPP     },
   { "ifloat",           CT_TYPE,          LANG_D          },
   { "ifndef",           CT_PP_IF,         LANG_ALLCPP     },
   { "implements",       CT_QUALIFIER,     LANG_JE         },
   { "implicit",         CT_QUALIFIER,     LANG_CS         },
   { "import",           CT_IMPORT,        LANG_DJE        }, /* fudged to get indenting */
   { "import",           CT_PP_INCLUDE,    LANG_OPP        }, /* #import = ObjectiveC version of #include */
   { "in",               CT_IN,            LANG_DCSOVE     },
   { "include",          CT_PP_INCLUDE,    LANG_CCPPPP     },
   { "inline",           CT_QUALIFIER,     LANG_CCPP       },
   { "inout",            CT_QUALIFIER,     LANG_D          },
   { "instanceof",       CT_SIZEOF,        LANG_JE         },
   { "int",              CT_TYPE,          LANG_ALLC       },
   { "interface",        CT_CLASS,         LANG_CCPPDCSJVE },
   { "internal",         CT_QUALIFIER,     LANG_CS         },
   { "invariant",        CT_INVARIANT,     LANG_D          },
   { "ireal",            CT_TYPE,          LANG_D          },
   { "is",               CT_SCOMPARE,      LANG_DCSV       },
   { "lazy",             CT_LAZY,          LANG_D          },
   { "line",             CT_PP_LINE,       LANG_PPP        },
   { "lock",             CT_LOCK,          LANG_CSV        },
   { "long",             CT_TYPE,          LANG_ALLC       },
   { "macro",            CT_D_MACRO,       LANG_D          },
   { "mixin",            CT_CLASS,         LANG_D          }, // may need special handling
   { "module",           CT_D_MODULE,      LANG_D          },
   { "mutable",          CT_QUALIFIER,     LANG_CCPP       },
   { "namespace",        CT_NAMESPACE,     LANG_CPPCSV     },
   { "native",           CT_NATIVE,        LANG_PAWN       },
   { "native",           CT_QUALIFIER,     LANG_JE         },
   { "new",              CT_NEW,           LANG_CPPDCSJVEP },
   { "not",              CT_SARITH,        LANG_CCPP       },
   { "not_eq",           CT_SCOMPARE,      LANG_CCPP       },
   { "null",             CT_TYPE,          LANG_DCSJV      },
   { "object",           CT_TYPE,          LANG_CS         },
   { "operator",         CT_OPERATOR,      LANG_CPPCSP     },
   { "or",               CT_SBOOL,         LANG_CCPPF      },
   { "or_eq",            CT_SASSIGN,       LANG_CCPP       },
   { "out",              CT_QUALIFIER,     LANG_DCSV       },
   { "override",         CT_QUALIFIER,     LANG_DCSV       },
   { "package",          CT_PRIVATE,       LANG_D          },
   { "package",          CT_PACKAGE,       LANG_JE         },
   { "params",           CT_TYPE,          LANG_CSV        },
   { "pragma",           CT_PP_PRAGMA,     LANG_ALLPP      },
   { "private",          CT_PRIVATE,       LANG_ALLC       }, // not C
   { "property",         CT_PP_PROPERTY,   LANG_CSPP       },
   { "protected",        CT_PRIVATE,       LANG_ALLC       }, // not C
   { "public",           CT_PRIVATE,       LANG_ALL        }, // not C
   { "readonly",         CT_QUALIFIER,     LANG_CS         },
   { "real",             CT_TYPE,          LANG_D          },
   { "ref",              CT_QUALIFIER,     LANG_CSV        },
   { "region",           CT_PP_REGION,     LANG_ALLPP      },
   { "register",         CT_QUALIFIER,     LANG_CCPP       },
   { "reinterpret_cast", CT_TYPE_CAST,     LANG_CCPP       },
   { "remove",           CT_GETSET,        LANG_CS         },
   { "restrict",         CT_QUALIFIER,     LANG_CCPP       },
   { "return",           CT_RETURN,        LANG_ALL        },
   { "sbyte",            CT_TYPE,          LANG_CS         },
   { "scope",            CT_D_SCOPE,       LANG_D          },
   { "sealed",           CT_QUALIFIER,     LANG_CS         },
   { "section",          CT_PP_SECTION,    LANG_PPP        },
   { "set",              CT_GETSET,        LANG_CSV        },
   { "short",            CT_TYPE,          LANG_ALLC       },
   { "signal",           CT_PRIVATE,       LANG_VALA       },
   { "signals",          CT_PRIVATE,       LANG_CPP        },
   { "signed",           CT_TYPE,          LANG_CCPP       },
   { "sizeof",           CT_SIZEOF,        LANG_CCPPCSVP   },
   { "sleep",            CT_SIZEOF,        LANG_PAWN       },
   { "stackalloc",       CT_NEW,           LANG_CS         },
   { "state",            CT_STATE,         LANG_PAWN       },
   { "static",           CT_QUALIFIER,     LANG_ALL        },
   { "static_cast",      CT_TYPE_CAST,     LANG_CPP        },
   { "stock",            CT_STOCK,         LANG_PAWN       },
   { "strictfp",         CT_QUALIFIER,     LANG_JAVA       },
   { "string",           CT_TYPE,          LANG_CSV        },
   { "struct",           CT_STRUCT,        LANG_CCPPDCSV   },
   { "super",            CT_SUPER,         LANG_DJE        },
   { "switch",           CT_SWITCH,        LANG_ALL        },
   { "synchronized",     CT_QUALIFIER,     LANG_DE         },
   { "synchronized",     CT_SYNCHRONIZED,  LANG_JAVA       },
   { "tagof",            CT_TAGOF,         LANG_PAWN       },
   { "template",         CT_TEMPLATE,      LANG_CPPD       },
   { "this",             CT_THIS,          LANG_CPPDCSJVE  },
   { "throw",            CT_THROW,         LANG_CPPDCSJVE  },
   { "throws",           CT_QUALIFIER,     LANG_JVE        },
   { "transient",        CT_QUALIFIER,     LANG_JE         },
   { "true",             CT_WORD,          LANG_CPPDCSJV   },
   { "try",              CT_TRY,           LANG_CPPDCSJVE  },
   { "tryinclude",       CT_PP_INCLUDE,    LANG_PPP        },
   { "typedef",          CT_TYPEDEF,       LAGN_CCPPDO     },
   { "typeid",           CT_SIZEOF,        LANG_CCPPD      },
   { "typename",         CT_TYPENAME,      LANG_CPP        },
   { "typeof",           CT_SIZEOF,        LANG_CCPPDCSVE  },
   { "ubyte",            CT_TYPE,          LANG_D          },
   { "ucent",            CT_TYPE,          LANG_D          },
   { "uint",             CT_TYPE,          LANG_DCSV       },
   { "ulong",            CT_TYPE,          LANG_DCSV       },
   { "unchecked",        CT_QUALIFIER,     LANG_CS         },
   { "undef",            CT_PP_UNDEF,      LANG_ALLPP      },
   { "union",            CT_UNION,         LANG_CCPPD      },
   { "unittest",         CT_UNITTEST,      LANG_D          },
   { "unsafe",           CT_UNSAFE,        LANG_CS         },
   { "unsigned",         CT_TYPE,          LANG_CCPP       },
   { "ushort",           CT_TYPE,          LANG_DCSV       },
   { "using",            CT_USING,         LANG_CPPCSV     },
   { "var",              CT_TYPE,          LANG_VE         },
   { "version",          CT_D_VERSION,     LANG_D          },
   { "virtual",          CT_QUALIFIER,     LANG_CPPCSV     },
   { "void",             CT_TYPE,          LANG_ALLC       },
   { "volatile",         CT_QUALIFIER,     LANG_CCPPCSJE   },
   { "volatile",         CT_VOLATILE,      LANG_D          },
   { "wchar",            CT_TYPE,          LANG_D          },
   { "wchar_t",          CT_TYPE,          LANG_CCPP       },
   { "weak",             CT_QUALIFIER,     LANG_VALA       },
   { "when",             CT_WHEN,          LANG_CS         },
   { "while",            CT_WHILE,         LANG_ALL        },
   { "with",             CT_D_WITH,        LANG_DE         },
   { "xor",              CT_SARITH,        LANG_CCPP       },
   { "xor_eq",           CT_SASSIGN,       LANG_CCPP       },
};


void init_keywords(void)
{
   /* nothing to do here */
}


static int32_t kw_compare(const void* p1, const void* p2)
{
   const chunk_tag_t* t1 = static_cast<const chunk_tag_t*>(p1);
   const chunk_tag_t* t2 = static_cast<const chunk_tag_t*>(p2);

   return(strcmp(t1->tag, t2->tag));
}


bool keywords_are_sorted(void)
{
   for (uint32_t idx = 1; idx < ARRAY_SIZE(keywords); idx++)
   {
      if (kw_compare(&keywords[idx - 1], &keywords[idx]) > 0)
      {
         fprintf(stderr, "%s: bad sort order at idx %u, words '%s' and '%s'\n",
                 __func__, idx - 1, keywords[idx - 1].tag, keywords[idx].tag);
         log_flush(true);
         cpd.error_count++;
         return(false);
      }
   }
   return(true);
}


void add_keyword(const char* tag, c_token_t type)
{
   string ss = tag;

   /* See if the keyword has already been added */
   dkwmap::iterator it = dkwm.find(ss);

   if (it != dkwm.end())
   {
      LOG_FMT(LDYNKW, "%s: changed '%s' to %d\n", __func__, tag, type);
      (*it).second = type;
      return;
   }

   /* Insert the keyword */
   dkwm.insert(dkwmap::value_type(ss, type));
   LOG_FMT(LDYNKW, "%s: added '%s' as %d\n", __func__, tag, type);
}


static const chunk_tag_t* kw_static_first(const chunk_tag_t* tag)
{
   const chunk_tag_t* prev = tag - 1;

   /* loop over static keyword array while ... */
   // \todo avoid pointer arithmetics
   while (((uint32_t)prev >= (uint32_t)&keywords[0]) && /* not at beginning of keyword array */
          (strcmp(prev->tag, tag->tag) == 0    ) )      /* tags match */
   {
      tag = prev;
      prev--;
   }
   return(tag);
}


static const chunk_tag_t* kw_static_match(const chunk_tag_t* tag)
{
   bool in_preproc = not_type(cpd.is_preproc, CT_NONE, CT_PP_DEFINE);

   const chunk_tag_t* end_adr = &keywords[ARRAY_SIZE(keywords)];
   const chunk_tag_t* iter    = kw_static_first(tag);
   for ( ; (uint32_t)iter < (uint32_t)end_adr; iter++)
   {
      bool pp_iter = (iter->lang_flags & FLAG_PP) != 0;
      if ((strcmp(iter->tag, tag->tag) == 0 ) &&
          (cpd.lang_flags & iter->lang_flags) &&
          (in_preproc == pp_iter))
      {
         return(iter);
      }
   }
   return(nullptr);
}


c_token_t find_keyword_type(const char* word, uint32_t len)
{
   retval_if((len == 0), CT_NONE);

   /* check the dynamic word list first */
   string           ss(word, len);
   dkwmap::iterator it = dkwm.find(ss);
   if (it != dkwm.end()) { return((*it).second); }

   chunk_tag_t key;
   key.tag = ss.c_str();

   /* check the static word list */
   const void* pos          = bsearch(&key, keywords, ARRAY_SIZE(keywords), sizeof(keywords[0]), kw_compare);
   const chunk_tag_t* p_ret = static_cast<const chunk_tag_t*>(pos);

   if (ptr_is_valid(p_ret))
   {
      p_ret = kw_static_match(p_ret);
   }
   return(ptr_is_valid(p_ret) ? p_ret->type : CT_WORD);
}


int32_t load_keyword_file(const char* filename, const uint32_t max_line_size)
{
   return load_from_file(filename, max_line_size, false);
}


void print_keywords(FILE* pfile)
{
   for (const auto& keyword_pair : dkwm)
   {
      c_token_t tt = keyword_pair.second;
      // \todo combine into one fprintf statement
      if      (tt == CT_TYPE       ) { fprintf(pfile, "type %*.s%s\n",        MAX_OPTION_NAME_LEN -  4, " ", keyword_pair.first.c_str()); }
      else if (tt == CT_MACRO_OPEN ) { fprintf(pfile, "macro-open %*.s%s\n",  MAX_OPTION_NAME_LEN - 11, " ", keyword_pair.first.c_str()); }
      else if (tt == CT_MACRO_CLOSE) { fprintf(pfile, "macro-close %*.s%s\n", MAX_OPTION_NAME_LEN - 12, " ", keyword_pair.first.c_str()); }
      else if (tt == CT_MACRO_ELSE ) { fprintf(pfile, "macro-else %*.s%s\n",  MAX_OPTION_NAME_LEN - 11, " ", keyword_pair.first.c_str()); }
      else
      {
         const char* tn = get_token_name(tt);
         fprintf(pfile, "set %s %*.s%s\n", tn, int32_t(MAX_OPTION_NAME_LEN - (4 + strlen(tn))), " ", keyword_pair.first.c_str());
      }
   }
}


void clear_keyword_file(void)
{
   dkwm.clear();
}


pattern_class_e get_token_pattern_class(c_token_t tok)
{
   // \todo instead of this switch better assign the pattern class to each statement
   switch (tok)
   {
      case CT_IF:           /* fallthrough */
      case CT_ELSEIF:       /* fallthrough */
      case CT_SWITCH:       /* fallthrough */
      case CT_FOR:          /* fallthrough */
      case CT_WHILE:        /* fallthrough */
      case CT_SYNCHRONIZED: /* fallthrough */
      case CT_USING_STMT:   /* fallthrough */
      case CT_LOCK:         /* fallthrough */
      case CT_D_WITH:       /* fallthrough */
      case CT_D_VERSION_IF: /* fallthrough */
      case CT_D_SCOPE_IF:   return(pattern_class_e::PBRACED );
      case CT_ELSE:         return(pattern_class_e::ELSE    );
      case CT_DO:           /* fallthrough */
      case CT_TRY:          /* fallthrough */
      case CT_FINALLY:      /* fallthrough */
      case CT_BODY:         /* fallthrough */
      case CT_UNITTEST:     /* fallthrough */
      case CT_UNSAFE:       /* fallthrough */
      case CT_VOLATILE:     /* fallthrough */
      case CT_GETSET:       return(pattern_class_e::BRACED  );
      case CT_CATCH:        /* fallthrough */
      case CT_D_VERSION:    /* fallthrough */
      case CT_DEBUG:        return(pattern_class_e::OPBRACED);
      case CT_NAMESPACE:    return(pattern_class_e::VBRACED );
      case CT_WHILE_OF_DO:  return(pattern_class_e::PAREN   );
      case CT_INVARIANT:    return(pattern_class_e::OPPAREN );
      default:              return(pattern_class_e::NONE    );
   }
}
