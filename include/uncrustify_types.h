/**
 * @file uncrustify_types.h
 *
 * Defines some types for the uncrustify program
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef UNCRUSTIFY_TYPES_H_INCLUDED
#define UNCRUSTIFY_TYPES_H_INCLUDED

#include <vector>
#include <deque>
using namespace std;

#include "base_types.h"
#include "options.h"
#include "token_enum.h"
#include "log_levels.h"
#include "logger.h"
#include "unc_text.h"
#include <cstdio>
#include <assert.h>
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif


/** special strings to mark a part of the input file where
 *  uncrustify shall not change anything */
#define UNCRUSTIFY_OFF_TEXT    " *INDENT-OFF*"
#define UNCRUSTIFY_ON_TEXT     " *INDENT-ON*"


/**
 * @brief Macro to inform the compiler that a variable is intentionally
 * not in use.
 *
 * @param [in] variableName: The unused variable.
 */
#define UNUSED(variableName)    ((void)variableName)


/** swaps two elements of the same type */
#define SWAP(a, b) { __typeof__(a) x = (a); (a) = (b); (b) = x; }


/** Brace stage enum used in brace_cleanup */
enum class brace_stage_e : unsigned int
{
   NONE,
   PAREN1,     /**< if/for/switch/while/synchronized */
   OP_PAREN1,  /**< optional parenthesis: catch () { */
   WOD_PAREN,  /**< while of do parenthesis */
   WOD_SEMI,   /**< semicolon after while of do */
   BRACE_DO,   /**< do */
   BRACE2,     /**< if/else/for/switch/while */
   ELSE,       /**< expecting 'else' after 'if' */
   ELSEIF,     /**< expecting 'if' after 'else' */
   WHILE,      /**< expecting 'while' after 'do' */
   CATCH,      /**< expecting 'catch' or 'finally' after 'try' */
   CATCH_WHEN  /**< optional 'when' after 'catch' */
};


enum class char_encoding_e : unsigned int
{
   ASCII,      /**< 0-127 */
   BYTE,       /**< 0-255, not UTF-8 */
   UTF8,       /**< UTF-8 bit wide */
   UTF16_LE,   /**< UTF-16 bit wide, little endian */
   UTF16_BE    /**< UTF-16 bit wide, big endian */
};


struct chunk_t;   /**< forward declaration */

/**
 * Sort of like the aligning stuff, but the token indent is relative to the
 * indent of another chunk. This is needed, as that chunk may be aligned and
 * so the indent cannot be determined in the indent code.
 */
struct indent_ptr_t
{
   chunk_t *ref;   /**<  */
   int     delta;  /**<  */
};


/** Structure for counting nested level */
struct paren_stack_entry_t
{
   c_token_t     type;         /**< the type that opened the entry */
   size_t        level;        /**< Level of opening type */
   size_t        open_line;    /**< line that open symbol is on */
   chunk_t       *pc;          /**< Chunk that opened the level */
   int           brace_indent; /**< indent for braces - may not relate to indent */
   size_t        indent;       /**< indent level (depends on use) */
   size_t        indent_tmp;   /**< temporary indent level (depends on use) */
   size_t        indent_tab;   /**< the 'tab' indent (always <= real column) */
   bool          indent_cont;  /**< indent_continue was applied */
   int           ref;          /**<  */
   c_token_t     parent;       /**< if, for, function, etc */
   brace_stage_e stage;        /**<  */
   bool          in_preproc;   /**< whether this was created in a preprocessor */
   size_t        ns_cnt;       /**<  */
   bool          non_vardef;   /**< Hit a non-vardef line */
   indent_ptr_t  ip;
};


/* TODO: put this on a linked list */
struct parse_frame_t
{
   int                 ref_no;
   size_t              level;           /**< level of parens/square/angle/brace */
   size_t              brace_level;     /**< level of brace/vbrace */
   size_t              pp_level;        /**< level of preproc #if stuff */

   int                 sparen_count;    /**<  */

   paren_stack_entry_t pse[128];        /**<  */
   size_t              pse_tos;         /**<  */
   int                 paren_count;     /**<  */

   c_token_t           in_ifdef;        /**<  */
   int                 stmt_count;      /**<  */
   int                 expr_count;      /**<  */

   bool                maybe_decl;      /**<  */
   bool                maybe_cast;      /**<  */
};


#define PCF_BIT(b)    (1ULL << b)

/* Copy flags are in the lower 16 bits */
#define PCF_COPY_FLAGS         0x0000ffff   /**<  */
#define PCF_IN_PREPROC         PCF_BIT(0)   /**< in a preprocessor */
#define PCF_IN_STRUCT          PCF_BIT(1)   /**< in a struct */
#define PCF_IN_ENUM            PCF_BIT(2)   /**< in enum */
#define PCF_IN_FCN_DEF         PCF_BIT(3)   /**< inside function def parenthesis */
#define PCF_IN_FCN_CALL        PCF_BIT(4)   /**< inside function call parenthesis */
#define PCF_IN_SPAREN          PCF_BIT(5)   /**< inside for/if/while/switch parenthesis */
#define PCF_IN_TEMPLATE        PCF_BIT(6)   /**<  */
#define PCF_IN_TYPEDEF         PCF_BIT(7)   /**<  */
#define PCF_IN_CONST_ARGS      PCF_BIT(8)   /**<  */
#define PCF_IN_ARRAY_ASSIGN    PCF_BIT(9)   /**<  */
#define PCF_IN_CLASS           PCF_BIT(10)  /**<  */
#define PCF_IN_CLASS_BASE      PCF_BIT(11)  /**<  */
#define PCF_IN_NAMESPACE       PCF_BIT(12)  /**<  */
#define PCF_IN_FOR             PCF_BIT(13)  /**<  */
#define PCF_IN_OC_MSG          PCF_BIT(14)  /**<  */

/* Non-Copy flags are in the upper 48 bits */
#define PCF_FORCE_SPACE        PCF_BIT(16)  /**< must have a space after this token */
#define PCF_STMT_START         PCF_BIT(17)  /**< marks the start of a statement */
#define PCF_EXPR_START         PCF_BIT(18)  /**<  */
#define PCF_DONT_INDENT        PCF_BIT(19)  /**< already aligned! */
#define PCF_ALIGN_START        PCF_BIT(20)  /**<  */
#define PCF_WAS_ALIGNED        PCF_BIT(21)  /**<  */
#define PCF_VAR_TYPE           PCF_BIT(22)  /**< part of a variable def type */
#define PCF_VAR_DEF            PCF_BIT(23)  /**< variable name in a variable def */
#define PCF_VAR_1ST            PCF_BIT(24)  /**< 1st variable def in a statement */
#define PCF_VAR_1ST_DEF        (PCF_VAR_DEF | PCF_VAR_1ST)
#define PCF_VAR_INLINE         PCF_BIT(25)  /* type was an inline struct/enum/union */
#define PCF_RIGHT_COMMENT      PCF_BIT(26)  /**<  */
#define PCF_OLD_FCN_PARAMS     PCF_BIT(27)  /**<  */
#define PCF_LVALUE             PCF_BIT(28)  /* left of assignment */
#define PCF_ONE_LINER          PCF_BIT(29)  /**<  */
#define PCF_ONE_CLASS          (PCF_ONE_LINER | PCF_IN_CLASS)
#define PCF_EMPTY_BODY         PCF_BIT(30)  /**<  */
#define PCF_ANCHOR             PCF_BIT(31)  /**< aligning anchor */
#define PCF_PUNCTUATOR         PCF_BIT(32)  /**<  */
#define PCF_INSERTED           PCF_BIT(33)  /**< chunk was inserted from another file */
#define PCF_LONG_BLOCK         PCF_BIT(34)  /**< the block is 'long' by some measure */
#define PCF_OC_BOXED           PCF_BIT(35)  /**< inside OC boxed expression */
#define PCF_KEEP_BRACE         PCF_BIT(36)  /**< do not remove brace */
#define PCF_OC_RTYPE           PCF_BIT(37)  /**< inside OC return type */
#define PCF_OC_ATYPE           PCF_BIT(38)  /**< inside OC arg type */
#define PCF_WF_ENDIF           PCF_BIT(39)  /**< #endif for whole file ifdef */
#define PCF_IN_QT_MACRO        PCF_BIT(40)  /**< in a QT-macro, i.e. SIGNAL, SLOT */


typedef enum StarStyle_e
{
   SS_IGNORE,   /**< don't look for prev stars */
   SS_INCLUDE,  /**< include prev * before add */
   SS_DANGLE    /**< include prev * after  add */
}StarStyle_t;


typedef struct align_ptr_s
{
   chunk_t      *next;       /**< nullptr or the chunk that should be under this one */
   bool         right_align; /**< AlignStack.m_right_align */
   StarStyle_t  star_style;  /**< AlignStack.m_star_style */
   StarStyle_t  amp_style;   /**< AlignStack.m_amp_style */
   size_t       gap;         /**< AlignStack.m_gap */

   /* col_adj is the amount to alter the column for the token.
    * For example, a dangling '*' would be set to -1.
    * A right-aligned word would be a positive value. */
   int     col_adj;          /**<  */
   chunk_t *ref;             /**<  */
   chunk_t *start;           /**<  */
}align_ptr_t;


/** This is the main type of this program */
struct chunk_t
{
   /** constructor for chunk_t */
   chunk_t()
   {
      reset();
   }


   /** sets all elements of the struct to their default value */
   void reset()
   {
      memset(&align,  0, sizeof(align ));
      memset(&indent, 0, sizeof(indent));
      next          = 0;
      prev          = 0;
      type          = CT_NONE;
      ptype   = CT_NONE;
      orig_line     = 0;
      orig_col      = 0;
      orig_col_end  = 0;
      orig_prev_sp  = 0;
      flags         = 0;
      column        = 0;
      column_indent = 0;
      nl_count      = 0;
      level         = 0;
      brace_level   = 0;
      pp_level      = 0;
      after_tab     = false;
      str.clear();
   }


   /** provides the number of characters of string */
   size_t len()
   {
      return(str.size());
   }


   /** provides the content of a string as zero terminated character pointer */
   const char *text()
   {
      return(str.c_str());
   }

   chunk_t      *next;         /**< pointer to next chunk in list */
   chunk_t      *prev;         /**< pointer to previous chunk in list */
   align_ptr_t  align;         /**<  */
   indent_ptr_t indent;        /**<  */
   c_token_t    type;          /**< type of the chunk itself */
   c_token_t    ptype;         /**< type of the parent chunk usually CT_NONE */
   size_t       orig_line;     /**< line number of chunk in input file */
   size_t       orig_col;      /**< column where chunk started in input file, is always > 0 */
   uint32_t     orig_col_end;  /**< column where chunk ended in input file, is always > 1 */
   uint32_t     orig_prev_sp;  /**< whitespace before this token */
   uint64_t     flags;         /**< see PCF_xxx */
   size_t       column;        /**< column of chunk */
   size_t       column_indent; /**< if 1st on a line, set to the 'indent'
                                *   column, which may be less than the real column
                                *   used to indent with tabs */
   size_t       nl_count;      /**< number of newlines in CT_NEWLINE */
   size_t       level;         /**< nest level in {, (, or [ */
   size_t       brace_level;   /**< nest level in braces only */
   size_t       pp_level;      /**< nest level in preprocessor */
   bool         after_tab;     /**< whether this token was after a tab */
   unc_text     str;           /**< the token text */
};


/** list of all programming languages known to uncrustify */
typedef enum lang_e
{
   LANG_NONE  = 0x0000,     /**< no language */
   LANG_C     = 0x0001,     /**< plain C */
   LANG_CPP   = 0x0002,     /**< C++ */
   LANG_D     = 0x0004,     /**< D */
   LANG_CS    = 0x0008,     /**< C# or C-sharp */
   LANG_JAVA  = 0x0010,     /**< Java */
   LANG_OC    = 0x0020,     /**< Objective C */
   LANG_VALA  = 0x0040,     /**< Vala, like C# */
   LANG_PAWN  = 0x0080,     /**< Pawn a compubase language */
   LANG_ECMA  = 0x0100,     /**< ECMA script based on Java script */
   LANG_ALL   = 0x0fff,     /**< applies to all languages */
   FLAG_DIG   = 0x4000,     /**< digraph/trigraph */
   FLAG_PP    = 0x8000,     /**< only appears in a preprocessor */

   /* various language combinations are defined to avoid compiler errors due to int/enum conversions */
   LANG_CCPPDCSJVE= LANG_C   | LANG_CPP  | LANG_D    | LANG_CS  | LANG_JAVA |             LANG_VALA | LANG_ECMA,
   LANG_CCPPDCSV  = LANG_C   | LANG_CPP  | LANG_D    | LANG_CS  |                         LANG_VALA,
   LANG_CCPPDCSVE = LANG_C   | LANG_CPP  | LANG_D    | LANG_CS  |                         LANG_VALA | LANG_ECMA,
   LANG_CCPPD     = LANG_C   | LANG_CPP  | LANG_D,
   LANG_CCPPDJP   = LANG_C   | LANG_CPP  | LANG_D    |            LANG_JAVA |                                    LANG_PAWN,
   LANG_CCPPDE    = LANG_C   | LANG_CPP  | LANG_D    |                                                LANG_ECMA,
   LANG_CCPPCS    = LANG_C   | LANG_CPP  |             LANG_CS,
   LANG_CCPPCSJE  = LANG_C   | LANG_CPP  |             LANG_CS  | LANG_JAVA |                         LANG_ECMA,
   LANG_CCPPCSVP  = LANG_C   | LANG_CPP  |             LANG_CS                          | LANG_VALA |            LANG_PAWN,
   LANG_CCPPDIG   = LANG_C   | LANG_CPP  |                                                                                   FLAG_DIG,
   LANG_CCPP      = LANG_C   | LANG_CPP,
   LANG_CCPPF     = LANG_C   | LANG_CPP  |                                                                                   FLAG_PP,
   LANG_CCPPO     = LANG_C   | LANG_CPP  |                                     LANG_OC,
   LAGN_CCPPDO    = LANG_C   | LANG_CPP  | LANG_D    |                         LANG_OC,
   LANG_ALLC      = LANG_C   | LANG_CPP  | LANG_D    | LANG_CS  | LANG_JAVA | LANG_OC   | LANG_VALA | LANG_ECMA,
   LANG_ALLCPP    = LANG_C   | LANG_CPP  | LANG_D    | LANG_CS  | LANG_JAVA | LANG_OC   | LANG_VALA | LANG_ECMA|             FLAG_PP,
   LANG_CCPPDCSOV = LANG_C   | LANG_CPP  | LANG_D    | LANG_CS  |             LANG_OC   | LANG_VALA,
   LANG_CCPPPP    = LANG_C   | LANG_CPP  |                                                                       LANG_PAWN | FLAG_PP,
   LANG_CPPDCSJVE =            LANG_CPP  | LANG_D    | LANG_CS  | LANG_JAVA |             LANG_VALA | LANG_ECMA, // xxx
   LANG_CPPD      =            LANG_CPP  | LANG_D,
   LANG_CPPDE     =            LANG_CPP  | LANG_D    |                                                LANG_ECMA,
   LANG_CPPDVE    =            LANG_CPP  | LANG_D    |                                    LANG_VALA | LANG_ECMA,
   LANG_CPPDCSJV  =            LANG_CPP  | LANG_D    | LANG_CS  | LANG_JAVA |             LANG_VALA,
   LANG_CPPDCSJVEP=            LANG_CPP  | LANG_D    | LANG_CS  | LANG_JAVA |             LANG_VALA | LANG_ECMA| LANG_PAWN,
   LANG_CPPCSV    =            LANG_CPP  |             LANG_CS  |                         LANG_VALA,
   LANG_CPPDIG    =            LANG_CPP                                                                        |             FLAG_DIG,
   LANG_CPPCSP    =            LANG_CPP  |             LANG_CS                                      |            LANG_PAWN,
   LANG_DE        =                        LANG_D    |                                                LANG_ECMA,
   LANG_DCSJV     =                        LANG_D    | LANG_CS  | LANG_JAVA |             LANG_VALA,
   LANG_DCSJVE    =                        LANG_D    | LANG_CS  | LANG_JAVA |             LANG_VALA | LANG_ECMA,
   LANG_DCSJVEX   =                        LANG_D    | LANG_CS  | LANG_JAVA |             LANG_VALA | LANG_ECMA,
   LANG_DCSOVE    =                        LANG_D    | LANG_CS  |              LANG_OC  | LANG_VALA | LANG_ECMA,
   LANG_DCSV      =                        LANG_D    | LANG_CS  |                         LANG_VALA,
   LANG_DP        =                        LANG_D    |                                                           LANG_PAWN,
   LANG_DJE       =                        LANG_D    |            LANG_JAVA |                         LANG_ECMA,
   LANG_DJP       =                        LANG_D    |            LANG_JAVA |                                    LANG_PAWN,
   LANG_CSDJE     =                        LANG_D    | LANG_CS  | LANG_JAVA |                         LANG_ECMA,
   LANG_CSV       =                                    LANG_CS  |                         LANG_VALA,
   LANG_CSPP      =                                    LANG_CS  |                                                            FLAG_PP,
   LANG_JE        =                                               LANG_JAVA |                         LANG_ECMA,
   LANG_JVE       =                                               LANG_JAVA |             LANG_VALA | LANG_ECMA,
   LANG_VE        =                                                                       LANG_VALA | LANG_ECMA,
   LANG_OCPP      =            LANG_CPP  |                                     LANG_OC,
   LANG_PPP       =                                                                                              LANG_PAWN | FLAG_PP,
   LANG_ALLPP     = LANG_ALL |                                                                                               FLAG_PP,
   LANG_ALLNJE    = LANG_ALL & ~(LANG_JAVA | LANG_ECMA),
}lang_t;


/** Pattern classes for special keywords */
enum class pattern_class_e : unsigned int
{
   NONE,
   BRACED,   // keyword + braced statement:
             //    do, try, finally, body, unittest, unsafe, volatile
             //    add, get, remove, set
   PBRACED,  // keyword + parenthesis + braced statement:
             //    if, elseif, switch, for, while, synchronized,
             //    using, lock, with, version, CT_D_SCOPE_IF
   OPBRACED, // keyword + optional parenthesis + braced statement:
             //    catch, version, debug
   VBRACED,  // keyword + value + braced statement:
             //    namespace
   PAREN,    // keyword + parenthesis:
             //    while-of-do
   OPPAREN,  // keyword + optional parenthesis: invariant (D lang)
   ELSE,     // Special case of pattern_class_e::BRACED for handling CT_IF
             //    else
};


struct chunk_tag_t
{
   const char *tag;         /**<  */
   c_token_t  type;         /**<  */
   lang_t     lang_flags;   /**<  */
};


struct lookup_entry_t
{
   char              ch;              /**<  */
   char              left_in_group;   /**<  */
   uint16_t          next_idx;        /**<  */
   const chunk_tag_t *tag;            /**<  */
};


struct align_t
{
   size_t    col;    /**<  */
   c_token_t type;   /**<  */
   size_t    len;    /**< of the token + space */
};


/** holds information and data of a file */
struct file_mem_t
{
   vector<uint8_t> raw;   /**< raw content of file  */
   deque<int>      data;  /**< processed content of file  */
   bool            bom;   /**<  */
   char_encoding_e enc;   /**< character encoding of file ASCII, utf, etc. */
#ifdef HAVE_UTIME_H
   struct utimbuf  utb;   /**<  */
#endif
};

enum class unc_stage_e : unsigned int
{
   TOKENIZE,
   HEADER,
   TOKENIZE_CLEANUP,
   BRACE_CLEANUP,
   FIX_SYMBOLS,
   MARK_COMMENTS,
   COMBINE_LABELS,
   OTHER,

   CLEANUP
};

#define MAX_OPTION_NAME_LEN    32 /* sets a limit to the name padding */

struct cp_data_t
{
   deque<uint8_t>  *bout;          /**<  */
   FILE            *fout;          /**<  */
   int             last_char;      /**<  */
   bool            do_check;       /**<  */
   unc_stage_e     unc_stage;      /**<  */
   int             check_fail_cnt; /**< total failures */
   bool            if_changed;     /**<  */

   uint32_t        error_count;    /**< counts how many errors occurred so far */
   const char      *filename;      /**<  */

   file_mem_t      file_hdr;       /**< for cmt_insert_file_header */
   file_mem_t      file_ftr;       /**< for cmt_insert_file_footer */
   file_mem_t      func_hdr;       /**< for cmt_insert_func_header */
   file_mem_t      oc_msg_hdr;     /**< for cmt_insert_oc_msg_header */
   file_mem_t      class_hdr;      /**< for cmt_insert_class_header */

   lang_t          lang_flags;     /**< defines the language of the source input LANG_xxx */
   bool            lang_forced;    /**<  */

   bool            unc_off;        /**<  */
   bool            unc_off_used;   /**< to check if "unc_off" is used */
   uint32_t        line_number;    /**<  */
   uint32_t        column;         /**< column for parsing */
   uint32_t        spaces;         /**< space count on output */

   int             ifdef_over_whole_file;

   bool            frag;           /**< activates code fragment option */
   uint32_t        frag_cols;      /**<  */

   // stuff to auto-detect line endings
   uint32_t        le_counts[LE_AUTO];  /**<  */
   unc_text        newline;             /**<  */

   bool            consumed;            /**<  */

   bool            did_newline;         /**< flag indicates if a newline was added or converted */
   c_token_t       is_preproc;          /**<  */
   int             preproc_ncnl_count;  /**<  */
   bool            output_trailspace;   /**<  */
   bool            output_tab_as_space; /**<  */

   bool            bom;                 /**<  */
   char_encoding_e enc;                 /**<  */

   // bumped up when a line is split or indented
   int             changes;             /**<  */
   int             pass_count;          /**<  */

   align_t         al[80];              /**<  */
   size_t          al_cnt;              /**<  */
   bool            al_c99_array;        /**<  */

   bool            warned_unable_string_replace_tab_chars;    /**<  */

   // Here are all the settings
   op_val_t        settings[UO_option_count]; /**<  */

   parse_frame_t   frames[16];                /**<  */
   int             frame_count;               /**<  */
   size_t          pp_level;                  /**< \todo can this ever be -1 */

   // the default values for settings
   op_val_t        defaults[UO_option_count];   /**<  */
};


extern cp_data_t cpd;   /* \todo can we avoid this external variable? */


#endif /* UNCRUSTIFY_TYPES_H_INCLUDED */
