/**
 * @file combine.cpp
 * Labels the chunks as needed.
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */
#include "combine.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "ChunkStack.h"
#include "uncrustify.h"
#include "lang_pawn.h"
#include "newlines.h"
#include "tokenize_cleanup.h"

#include <cstdio>
#include <cstdlib>
#include "unc_ctype.h"
#include <cassert>


/**
 * Flags all chunks from the open parenthesis to the close parenthesis.
 *
 * @return The token after the close parenthesis
 */
static chunk_t* flag_parens(
   chunk_t*  po,        /**< [in] Pointer to the open parenthesis */
   uint64_t  flags,     /**< [in] flags to add */
   c_token_t opentype,  /**< [in]  */
   c_token_t ptype,     /**< [in]  */
   bool      parent_all /**< [in]  */
);


/**
 * Mark the parenthesis and colons in:
 *   asm volatile ( "xx" : "xx" (l), "yy"(h) : ...  );
 */
static void flag_asm(
   chunk_t* pc  /**< [in] the CT_ASM item */
);


/**
 * Scan backwards to see if we might be on a type declaration
 */
static bool chunk_ends_type(
   chunk_t* start  /**< [in]  */
);


/**
 * skip to the final word/type in a :: chain
 * pc is either a word or a ::
 */
static chunk_t* skip_dc_member(
   chunk_t* start  /**< [in]  */
);


/**
 * Skips to the start of the next statement.
 */
static chunk_t* skip_to_next_statement(
   chunk_t* pc  /**< [in]  */
);


/**
 * Skips everything until a comma or semicolon at the same level.
 * Returns the semicolon, comma, or close brace/parenthesis or nullptr.
 */
static chunk_t* skip_expression(
   chunk_t* start  /**< [in]  */
);


/**
 * Skips the D 'align()' statement and the colon, if present.
 *    align(2) int foo;  -- returns 'int'
 *    align(4):          -- returns 'int'
 *    int32_t bar;
 */
static chunk_t* skip_align(
   chunk_t* start  /**< [in]  */
);

/**
 * Combines two tokens into {{ and }} if inside parenthesis and nothing is between
 * either pair.
 */
static void check_double_brace_init(
   chunk_t* bo1  /**< [in]  */
);

/**
 * Simply change any STAR to PTR_TYPE and WORD to TYPE
 */
static void fix_fcn_def_params(
   chunk_t* pc  /**< [in] points to the function's open parenthesis */
);


/**
 * We are on a typedef.
 * If the next word is not enum/union/struct, then the last word before the
 * next ',' or ';' or '__attribute__' is a type.
 *
 * typedef [type...] [*] type [, [*]type] ;
 * typedef <return type>([*]func)();
 * typedef <return type>([*]func)(params);
 * typedef <return type>(__stdcall *func)(); Bug # 633 MS-specific extension
 *         include the config-file "test/config/MS-calling_conventions.cfg"
 * typedef <return type>func(params);
 * typedef <enum/struct/union> [type] [*] type [, [*]type] ;
 * typedef <enum/struct/union> [type] { ... } [*] type [, [*]type] ;
 */
static void fix_typedef(
   chunk_t* pc  /**< [in]  */
);


/**
 * We are on an enum/struct/union tag that is NOT inside a typedef.
 * If there is a {...} and words before the ';', then they are variables.
 *
 * tag { ... } [*] word [, [*]word] ;
 * tag [word/type] { ... } [*] word [, [*]word] ;
 * enum [word/type [: int_type]] { ... } [*] word [, [*]word] ;
 * tag [word/type] [word]; -- this gets caught later.
 * fcn(tag [word/type] [word])
 * a = (tag [word/type] [*])&b;
 *
 * REVISIT: should this be consolidated with the typedef code?
 */
static void fix_enum_struct_union(
   chunk_t* pc  /**< [in]  */
);


/**
 * Checks to see if the current parenthesis is part of a cast.
 * We already verified that this doesn't follow function, TYPE, IF, FOR,
 * SWITCH, or WHILE and is followed by WORD, TYPE, STRUCT, ENUM, or UNION.
 */
static void fix_casts(
   chunk_t* pc  /**< [in]  */
);


/**
 * CT_TYPE_CAST follows this pattern:
 * dynamic_cast<...>(...)
 *
 * Mark everything between the <> as a type and set the paren parent
 */
static void fix_type_cast(
   chunk_t* pc  /**< [in]  */
);


/**
 * We are on the start of a sequence that could be a var def
 *  - FPAREN_OPEN (parent == CT_FOR)
 *  - BRACE_OPEN
 *  - SEMICOLON
 */
static chunk_t* fix_var_def(
   chunk_t* pc  /**< [in]  */
);


/**
 * We are on a function word. we need to:
 *  - find out if this is a call or prototype or implementation
 *  - mark return type
 *  - mark parameter types
 *  - mark brace pair
 *
 * REVISIT:
 * This whole function is a mess.
 * It needs to be reworked to eliminate duplicate logic and determine the
 * function type more directly.
 *  1. Skip to the close parenthesis and see what is after.
 *     a. semicolon - function call or function prototype
 *     b. open brace - function call (i.e., list_for_each) or function def
 *     c. open paren - function type or chained function call
 *     d. qualifier - function def or prototype, continue to semicolon or open brace
 *  2. Examine the 'parameters' to see if it can be a prototype/definition
 *  3. Examine what is before the function name to see if it is a prototype or call
 * Constructor/destructor detection should have already been done when the
 * 'class' token was encountered (see mark_class_ctor).
 */
static void mark_function(
   chunk_t* pc  /**< [in]  */
);


/**
 * Checks to see if a series of chunks could be a C++ parameter
 * FOO foo(5, &val);
 *
 * WORD means CT_WORD or CT_TYPE
 *
 * "WORD WORD"          ==> true
 * "QUALIFIER ??"       ==> true
 * "TYPE"               ==> true
 * "WORD"               ==> true
 * "WORD.WORD"          ==> true
 * "WORD::WORD"         ==> true
 * "WORD * WORD"        ==> true
 * "WORD & WORD"        ==> true
 * "NUMBER"             ==> false
 * "STRING"             ==> false
 * "OPEN PAREN"         ==> false
 */
static bool can_be_full_param(
   chunk_t* start,  /**< [in] the first chunk to look at */
   chunk_t* end     /**< [in] the chunk after the last one to look at */
);


/**
 * Changes the return type to type and set the parent.
 */
static void mark_function_return_type(
   chunk_t   *fname,     /**< [in]  */
   chunk_t   *pc,        /**< [in] the last chunk of the return type */
   c_token_t parent_type /**< [in] CT_NONE (no change) or the new parent type */
);


/**
 * Process a function type that is not in a typedef.
 * pc points to the first close parenthesis.
 *
 * void (*func)(params);
 * const char * (*func)(params);
 * const char * (^func)(params);   -- Objective C
 *
 * @return whether a function type was processed
 */
static bool mark_function_type(
   chunk_t* pc  /**< [in] Points to the first closing parenthesis */
);


/**
 * Examines the stuff between braces { }.
 * There should only be variable definitions and methods.
 * Skip the methods, as they will get handled elsewhere.
 */
static void mark_struct_union_body(
   chunk_t* start  /**< [in]  */
);


/**
 * We are on the first word of a variable definition.
 * Mark all the variable names with PCF_VAR_1ST and PCF_VAR_DEF as
 * appropriate. Also mark any '*' encountered as a CT_PTR_TYPE.
 * Skip over []. Go until a ';' is hit.
 *
 * Example input:
 * int   a = 3, b, c = 2;              ## called with 'a'
 * foo_t f = {1, 2, 3}, g = {5, 6, 7}; ## called with 'f'
 * struct {...} *a, *b;                ## called with 'a' or '*'
 * myclass a(4);
 */
static chunk_t* mark_variable_definition(
   chunk_t* start  /**< [in]  */
);


/**
 * Marks statement starts in a macro body.
 * REVISIT: this may already be done
 */
static void mark_define_expressions(void);


/**
 * tbd
 */
static void process_returns(void);


/**
 * Processes a return statement, labeling the parenthesis and marking
 * the parent. May remove or add parenthesis around the return statement
 */
static chunk_t* process_return(
   chunk_t* pc  /**< [in] Pointer to the return chunk */
);


/**
 * We're on a 'class' or 'struct'.
 * Scan for CT_FUNCTION with a string that matches pclass->str
 */
static void mark_class_ctor(
   chunk_t* pclass  /**< [in]  */
);


/**
 * We're on a 'namespace' skip the word and then set the parent of the braces.
 */
static void mark_namespace(
   chunk_t* pns  /**< [in]  */
);


/**
 * tbd
 */
static void mark_cpp_constructor(
   chunk_t* pc  /**< [in]  */
);


/**
 * Just hit an assign. Go backwards until we hit an open
 * brace/parenthesis/square or semicolon
 * (TODO: other limiter?) and mark as a LValue.
 */
static void mark_lvalue(
   chunk_t* pc  /**< [in]  */
);


/**
 * We are on a word followed by a angle open which is part of a template.
 * If the angle close is followed by a open paren, then we are on a
 * template function def or a template function call:
 *   Vector2<float>(...) [: ...[, ...]] { ... }
 * Or we could be on a variable def if it's followed by a word:
 *   Renderer<rgb32> rend;
 */
static void mark_template_func(
   chunk_t* pc,      /**< [in]  */
   chunk_t* pc_next  /**< [in]  */
);


/**
 * Just mark every CT_WORD until a semicolon as CT_SQL_WORD.
 * Adjust the levels if pc is CT_SQL_BEGIN
 */
static void mark_exec_sql(
   chunk_t* pc  /**< [in]  */
);


/**
 * Process an ObjC 'class'
 * pc is the chunk after '@implementation' or '@interface' or '@protocol'.
 * Change colons, etc. Processes stuff until '@end'.
 * Skips anything in braces.
 */
static void handle_oc_class(
   chunk_t* pc  /**< [in]  */
);


/**
 *  Mark Objective-C blocks (aka lambdas or closures)
 *  The syntax and usage is exactly like C function pointers
 *  but instead of an asterisk they have a caret as pointer symbol.
 *  Although it may look expensive this functions is only triggered
 *  on appearance of an OC_BLOCK_CARET for LANG_OC.
 *  repeat(10, ^{ putc('0'+d); });
 *  typedef void (^workBlk_t)(void);
 */
static void handle_oc_block_literal(
   chunk_t* pc  /**< [in] points to the '^' */
);


/**
 * Mark Objective-C block types.
 * The syntax and usage is exactly like C function pointers
 * but instead of an asterisk they have a caret as pointer symbol.
 *  typedef void (^workBlk_t)(void);
 *  const char * (^workVar)(void);
 *  -(void)Foo:(void(^)())blk { }
 *
 * This is triggered when the sequence '(' '^' is found.
 */
static void handle_oc_block_type(
   chunk_t* pc  /**< [in] points to the '^' */
);


/**
 * Process an ObjC message specification/declaration
 *
 * Specs:
 * -(void) foo ARGS;
 *
 * Declaration:
 * -(void) foo ARGS {  }
 *
 * LABEL : (ARGTYPE) ARGNAME
 *
 * ARGS is ': (ARGTYPE) ARGNAME [MOREARGS...]'
 * MOREARGS is ' [ LABEL] : (ARGTYPE) ARGNAME '
 * -(void) foo: (int32_t) arg: {  }
 * -(void) foo: (int32_t) arg: {  }
 * -(void) insertObject:(id)anObject atIndex:(int32_t)index
 */
static void handle_oc_message_decl(
   chunk_t* pc  /**< [in]  */
);


/**
 * Process an ObjC message send statement:
 * [ class func: val1 name2: val2 name3: val3] ; // named params
 * [ class func: val1      : val2      : val3] ; // unnamed params
 * [ class <proto> self method ] ; // with protocol
 * [[NSMutableString alloc] initWithString: @"" ] // class from msg
 * [func(a,b,c) lastObject ] // class from func
 *
 * Mainly find the matching ']' and ';' and mark the colons.
 */
static void handle_oc_message_send(
   chunk_t* pc  /**< [in] points to the open square '[' */
);


/**
 * Process @Property values and re-arrange them if necessary
 */
static void handle_oc_property_decl(
   chunk_t* pc  /**< [in]  */
);


/**
 * Process a type that is enclosed in parenthesis in message declarations.
 * TODO: handle block types, which get special formatting
 *
 * @param pc points to the open parenthesis
 * @return the chunk after the type
 */
static chunk_t* handle_oc_md_type(
   chunk_t*  paren_open, /**< [in]  */
   c_token_t ptype,      /**< [in]  */
   uint64_t  flags,      /**< [in]  */
   bool      &did_it     /**< [in]  */
);

/**
 * Process an C# [] thingy:
 *    [assembly: xxx]
 *    [AttributeUsage()]
 *    [@X]
 *
 * Set the next chunk to a statement start after the close ']'
 */
static void handle_cs_square_stmt(
   chunk_t* pc  /**< [in] points to the open square '[' */
);


/**
 * We are on a brace open that is preceded by a word or square close.
 * Set the brace parent to CT_CS_PROPERTY and find the first item in the
 * property and set its parent, too.
 */
static void handle_cs_property(
   chunk_t* pc  /**< [in]  */
);


/**
 * We hit a ']' followed by a WORD. This may be a multidimensional array type.
 * Example: int[,,] x;
 * If there is nothing but commas between the open and close, then mark it.
 */
static void handle_cs_array_type(
   chunk_t* pc  /**< [in]  */
);


/**
 * We are on the C++ 'template' keyword.
 * What follows should be the following:
 *
 * template <class identifier> function_declaration;
 * template <typename identifier> function_declaration;
 * template <class identifier> class class_declaration;
 * template <typename identifier> class class_declaration;
 *
 * Change the 'class' inside the <> to CT_TYPE.
 * Set the parent to the class after the <> to CT_TEMPLATE.
 * Set the parent of the semicolon to CT_TEMPLATE.
 */
static void handle_cpp_template(
   chunk_t* pc  /**< [in]  */
);


/**
 * Verify and then mark C++ lambda expressions.
 * The expected format is '[...](...){...}' or '[...](...) -> type {...}'
 * sq_o is '[' CT_SQUARE_OPEN or '[]' CT_TSQUARE
 * Split the '[]' so we can control the space
 */
static void handle_cpp_lambda(
   chunk_t* pc  /**< [in]  */
);


/**
 * We are on the D 'template' keyword.
 * What follows should be the following:
 *
 * template NAME ( TYPELIST ) { BODY }
 *
 * Set the parent of NAME to template, change NAME to CT_TYPE.
 * Set the parent of the parens and braces to CT_TEMPLATE.
 * Scan the body for each type in TYPELIST and change the type to CT_TYPE.
 */
static void handle_d_template(
   chunk_t* pc  /**< [in]  */
);


/**
 * A func wrap chunk and what follows should be treated as a function name.
 * Create new text for the chunk and call it a CT_FUNCTION.
 *
 * A type wrap chunk and what follows should be treated as a simple type.
 * Create new text for the chunk and call it a CT_TYPE.
 */
static void handle_wrap(
   chunk_t* pc  /**< [in]  */
);


/**
 * A prototype wrap chunk and what follows should be treated as a
 * function prototype.
 *
 * RETTYPE PROTO_WRAP(NAME, PARAMS); or RETTYPE PROTO_WRAP(NAME, (PARAMS));
 * RETTYPE gets changed with make_type().
 * PROTO_WRAP is marked as CT_FUNC_PROTO or CT_FUNC_DEF.
 * NAME is marked as CT_WORD.
 * PARAMS is all marked as prototype parameters.
 */
static void handle_proto_wrap(
   chunk_t* pc  /**< [in]  */
);


/**
 * tbd
 */
static bool is_oc_block(
   chunk_t* pc  /**< [in]  */
);


/**
 * Java assert statements are: "assert EXP1 [: EXP2] ;"
 * Mark the parent of the colon and semicolon
 */
static void handle_java_assert(
   chunk_t* pc  /**< [in]  */
);


/**
 * Parse off the types in the D template args, adds to C-sharp
 * returns the close_paren
 */
static chunk_t* get_d_template_types(
   ChunkStack &cs,         /**< [in]  */
   chunk_t    *open_paren  /**< [in]  */
);


/**
 * tbd
 */
static bool chunkstack_match(
   const   ChunkStack &cs, /**< [in]  */
   chunk_t* pc             /**< [in]  */
);


void make_type(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   if(is_type (pc, CT_WORD)) { set_type(pc, CT_TYPE    ); }
   else if (is_star (pc) ||
            is_msref(pc))    { set_type(pc, CT_PTR_TYPE); }
   else if (is_addr (pc))    { set_type(pc, CT_BYREF   ); }
}


void flag_series(chunk_t* start, chunk_t* end, uint64_t set_flags,
                 uint64_t clr_flags, scope_e nav)
{
   LOG_FUNC_ENTRY();
   while ((is_valid(start)) && (start != end ) )
   {
      update_flags(start, clr_flags, set_flags);
      start = chunk_get_next(start, nav);
   }

   if (is_valid(end))
   {
      update_flags(end, clr_flags, set_flags);
   }
}


static chunk_t* flag_parens(chunk_t* po, uint64_t flags, c_token_t opentype,
                            c_token_t ptype, bool parent_all)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(po));

   chunk_t* paren_close = chunk_skip_to_match(po, scope_e::PREPROC);
   if (is_invalid(paren_close))
   {
      LOG_FMT(LERR, "flag_parens: no match for [%s] at [%u:%u]",
              po->text(), po->orig_line, po->orig_col);
      log_func_stack_inline(LERR);
      cpd.error_count++;
      return(nullptr);
   }

   LOG_FMT(LFLPAREN, "flag_parens: %u:%u [%s] and %u:%u [%s] type=%s ptype=%s",
           po->orig_line, po->orig_col, po->text(),
           paren_close->orig_line, paren_close->orig_col, paren_close->text(),
           get_token_name(opentype), get_token_name(ptype));
   log_func_stack_inline(LFLPAREN);

   if (po != paren_close)
   {
      if ((flags != 0) ||
          (parent_all && (ptype != CT_NONE)))
      {
         chunk_t* pc;
         for (pc = chunk_get_next(po, scope_e::PREPROC);
              pc != paren_close;
              pc = chunk_get_next(pc, scope_e::PREPROC))
         {
            set_flags(pc, flags);
            if (parent_all)
            {
               set_ptype(pc, ptype);
            }
         }
      }

      if (opentype != CT_NONE)
      {
         set_type(po, opentype);
         set_type(paren_close, get_inverse_type(opentype));
      }

      if (ptype != CT_NONE)
      {
         set_ptype(po,          ptype);
         set_ptype(paren_close, ptype);
      }
   }
   return(get_next_ncnl(paren_close, scope_e::PREPROC));
}


chunk_t* set_paren_parent(chunk_t* start, c_token_t parent)
{
   LOG_FUNC_ENTRY();
   chunk_t* end;

   end = chunk_skip_to_match(start, scope_e::PREPROC);
   if (is_valid(end))
   {
      assert(is_valid(start));
      LOG_FMT(LFLPAREN, "set_paren_parent: %u:%u [%s] and %u:%u [%s] type=%s ptype=%s",
              start->orig_line, start->orig_col, start->text(),
              end->orig_line,   end->orig_col,   end->text(),
              get_token_name(start->type), get_token_name(parent));
      log_func_stack_inline(LFLPAREN);
      set_ptype(start, parent);
      set_ptype(end,   parent);
   }
   return(get_next_ncnl(end, scope_e::PREPROC));
}


static void flag_asm(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   chunk_t* tmp = get_next_ncnl(pc, scope_e::PREPROC);
   return_if(!is_type(tmp, CT_QUALIFIER));

   chunk_t* po = get_next_ncnl(tmp, scope_e::PREPROC);
   return_if(!is_paren_open(po));

   chunk_t* end = chunk_skip_to_match(po, scope_e::PREPROC);
   return_if(is_invalid(end));

   set_ptype(po,  CT_ASM);
   set_ptype(end, CT_ASM);
   for (tmp = get_next_ncnl(po,  scope_e::PREPROC);
        tmp != end;
        tmp = get_next_ncnl(tmp, scope_e::PREPROC))
   {
      assert(is_valid(tmp));
      if (is_type(tmp, CT_COLON))
      {
         set_type(tmp, CT_ASM_COLON);
      }
      else if (is_type(tmp, CT_DC_MEMBER))
      {
         /* if there is a string on both sides, then this is two ASM_COLONs */
         if (is_type(get_next_ncnl(tmp, scope_e::PREPROC), CT_STRING) &&
             is_type(get_prev_ncnl(tmp, scope_e::PREPROC), CT_STRING) )
         {
            chunk_t nc = *tmp;
            tmp->str.resize(1);
            tmp->orig_col_end = tmp->orig_col + 1;
            set_type(tmp, CT_ASM_COLON);

            nc.type = tmp->type;
            nc.str.pop_front();
            nc.orig_col++;
            nc.column++;
            chunk_add_after(&nc, tmp);
         }
      }
   }
   tmp = get_next_ncnl(end, scope_e::PREPROC);
   if (is_type(tmp, CT_SEMICOLON))
   {
      set_ptype(tmp, CT_ASM);
   }
}


static bool chunk_ends_type(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(start), false);

   bool    ret       = false;
   uint32_t  cnt       = 0;
   bool    last_lval = false;

   chunk_t* pc = start;
   for ( ; is_valid(pc); pc = get_prev_ncnl(pc))
   {
      LOG_FMT(LFTYPE, "%s: [%s] %s flags %" PRIx64 " on line %u, col %u\n",
              __func__, get_token_name(pc->type), pc->text(),
              get_flags(pc), pc->orig_line, pc->orig_col);

      if(is_type(pc, 6, CT_QUALIFIER, CT_WORD, CT_STRUCT,
                        CT_DC_MEMBER, CT_TYPE, CT_PTR_TYPE))
      {
         cnt++;
         last_lval = is_flag(pc, PCF_LVALUE);
         continue;
      }

      if ((is_semicolon(pc) && not_flag(pc, PCF_IN_FOR)) ||
          is_type(pc, CT_TYPEDEF, CT_BRACE_OPEN, CT_BRACE_CLOSE) ||
          is_forin(pc) ||
          (is_type(pc, CT_SPAREN_OPEN) &&
          (last_lval == true)        ) )
      {
         ret = cnt > 0;
      }
      break;
   }

   if (is_invalid(pc)) { ret = true; } /* first token */
   LOG_FMT(LFTYPE, "%s verdict: %s\n", __func__, ret ? "yes" : "no");
   return(ret);
}


static chunk_t* skip_dc_member(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(start), start);

   chunk_t* pc   = start;
   chunk_t* next = (is_type(pc, CT_DC_MEMBER)) ? pc : get_next_ncnl(pc);
   while (is_type(next, CT_DC_MEMBER))
   {
      pc   = get_next_ncnl(next);
      next = get_next_ncnl(pc);
   }
   return(pc);
}


void do_symbol_check(chunk_t* prev, chunk_t* pc, chunk_t* next)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LGUY, "%s: %u:%u %s:%s\n", __func__, pc->orig_line, pc->orig_col,
                                        pc->text(), get_token_name(pc->type));

   if (is_type(pc, CT_OC_AT))
   {
      if(is_type(next, CT_PAREN_OPEN, CT_BRACE_OPEN, CT_SQUARE_OPEN))
      {
         flag_parens(next, PCF_OC_BOXED, next->type, CT_OC_AT, false);
      }
      else
      {
         set_ptype(next, CT_OC_AT);
      }
   }

   /* D stuff */
   if (is_lang(cpd,  LANG_D       ) &&
       is_type(pc,   CT_QUALIFIER ) &&
       is_str (pc,   "const"  ) &&
       is_type(next, CT_PAREN_OPEN) )
   {
      set_type(pc, CT_D_CAST);
      set_paren_parent(next, pc->type);
   }

   if ( is_type(next, CT_PAREN_OPEN                   ) &&
        is_type(pc,   CT_D_CAST, CT_DELEGATE, CT_ALIGN) )
   {
      /* mark the parenthesis parent */
      chunk_t* tmp = set_paren_parent(next, pc->type);

      /* For a D cast - convert the next item */
      if (is_type (pc, CT_D_CAST) && is_valid(tmp))
      {
         switch(tmp->type)
         {
            case(CT_STAR ): set_type(tmp, CT_DEREF); break;
            case(CT_AMP  ): set_type(tmp, CT_ADDR ); break;
            case(CT_MINUS): set_type(tmp, CT_NEG  ); break;
            case(CT_PLUS ): set_type(tmp, CT_POS  ); break;
            default:       /*ignore unexpected type*/break;
         }
      }

      /* For a delegate, mark previous words as types and the item after the
       * close paren as a variable def
       */
      if (is_type(pc, CT_DELEGATE))
      {
         if (is_valid(tmp))
         {
            set_ptype(tmp, CT_DELEGATE);
            if (is_level(tmp, tmp->brace_level))
            {
               set_flags(tmp, PCF_VAR_1ST_DEF);
            }
         }

         for (tmp = get_prev_ncnl(pc); is_valid(tmp); tmp = get_prev_ncnl(tmp))
         {
            break_if ((is_semicolon(tmp)) ||
                      is_type(tmp, CT_BRACE_OPEN, CT_VBRACE_OPEN));
            make_type(tmp);
         }
      }

      if (is_type(pc, CT_ALIGN))
      {
         if (is_type(tmp, CT_BRACE_OPEN)) { set_paren_parent(tmp, pc->type); }
         if (is_type(tmp, CT_COLON     )) { set_ptype       (tmp, pc->type); }
      }
   }

   if (is_type(pc, CT_INVARIANT))
   {
      if (is_type(next, CT_PAREN_OPEN))
      {
         set_ptype(next, pc->type);
         chunk_t* tmp = chunk_get_next(next);
         while (is_valid(tmp))
         {
            if (is_type(tmp, CT_PAREN_CLOSE))
            {
               set_ptype(tmp, pc->type);
               break;
            }
            make_type(tmp);
            tmp = chunk_get_next(tmp);
         }
      }
      else
      {
         set_type(pc, CT_QUALIFIER);
      }
   }

   if (is_type_and_not_ptype(prev, CT_BRACE_OPEN, CT_CS_PROPERTY) &&
       (is_type(pc, CT_GETSET, CT_GETSET_EMPTY) ) )
   {
      flag_parens(prev, 0, CT_NONE, CT_GETSET, false);
   }

   if (is_type(pc, CT_ASM)) { flag_asm(pc); }

   /* Objective C stuff */
   if(is_lang(cpd, LANG_OC))
   {
      /* Check for message declarations */
      if (is_flag(pc, PCF_STMT_START))
      {
         if ((is_str(pc,   "-") ||
              is_str(pc,   "+") ) &&
              is_str(next, "(")   )
         {
            handle_oc_message_decl(pc);
         }
      }
      if (is_flag(pc, PCF_EXPR_START))
      {
         if (is_type(pc, CT_SQUARE_OPEN)) { handle_oc_message_send (pc); }
         if (is_type(pc, CT_CARET      )) { handle_oc_block_literal(pc); }
      }
      if (is_type(pc, CT_OC_PROPERTY))
      {
         handle_oc_property_decl(pc);
      }
   }


   /* C# stuff */
   if (is_lang(cpd, LANG_CS))
   {
      /* '[assembly: xxx]' stuff */
      if (is_type_and_flag(pc, CT_SQUARE_OPEN, PCF_EXPR_START))
      {
         handle_cs_square_stmt(pc);
      }

      if (is_type_and_ptype(next,  CT_BRACE_OPEN,   CT_NONE                ) &&
          is_type          (pc, 3, CT_SQUARE_CLOSE, CT_ANGLE_CLOSE, CT_WORD) )
      {
         handle_cs_property(next);
      }
      if (are_types(pc, CT_SQUARE_CLOSE, next, CT_WORD))
      {
         handle_cs_array_type(pc);
      }

      if (is_type(pc,   CT_LAMBDA, CT_DELEGATE) &&
          is_type(next, CT_BRACE_OPEN         ) )
      {
         set_paren_parent(next, pc->type);
      }

      if (is_type (pc,       CT_WHEN       ) &&
          not_type(pc->next, CT_SPAREN_OPEN) )
      {
         set_type(pc, CT_WORD);
      }
   }

   if (is_type(pc, CT_NEW))
   {
      chunk_t* ts  = nullptr;
      chunk_t* tmp = next;
      if (is_type(tmp, CT_TSQUARE))
      {
         ts  = tmp;
         tmp = get_next_ncnl(tmp);
      }
      if (is_type(tmp, CT_BRACE_OPEN, CT_PAREN_OPEN))
      {
         set_paren_parent(tmp, pc->type);
         if (is_valid(ts))
         {
            ts->ptype = pc->type;
         }
      }
   }

   /* C++11 Lambda stuff */
   if (is_lang(cpd, LANG_CPP) &&
       (is_type(pc, CT_SQUARE_OPEN, CT_TSQUARE)))
   {
      handle_cpp_lambda(pc);
   }

   /* FIXME: which language does this apply to? */
   if (are_types(pc, CT_ASSIGN, next, CT_SQUARE_OPEN))
   {
      set_paren_parent(next, CT_ASSIGN);

      /* Mark one-liner assignment */
      chunk_t* tmp = next;
      while ((tmp = get_next_nc(tmp)) != nullptr)
      {
         break_if (is_nl(tmp));

         if (is_type(tmp, CT_SQUARE_CLOSE) &&
             (next->level == tmp->level))
         {
            set_flags(tmp,  PCF_ONE_LINER);
            set_flags(next, PCF_ONE_LINER);
            break;
         }
      }
   }

   if (is_type(pc, CT_ASSERT))
   {
      handle_java_assert(pc);
   }
   if (is_type(pc, CT_ANNOTATION))
   {
      chunk_t* tmp = get_next_ncnl(pc);
      if (is_paren_open(tmp))
      {
         set_paren_parent(tmp, CT_ANNOTATION);
      }
   }

   /* A [] in C# and D only follows a type */
   if (is_type(pc, CT_TSQUARE) && is_lang(cpd, LANG_DCSV))
   {
      if (is_type(prev, CT_WORD)) { set_type(prev, CT_TYPE);          }
      if (is_type(next, CT_WORD)) { set_flags(next, PCF_VAR_1ST_DEF); }
   }

   if (is_type(pc, 3, CT_SQL_EXEC, CT_SQL_BEGIN, CT_SQL_END))
   {
      mark_exec_sql(pc);
   }

   if (is_type(pc, CT_PROTO_WRAP))
   {
      handle_proto_wrap(pc);
   }

   /* Handle the typedef */
   if (is_type(pc, CT_TYPEDEF))
   {
      fix_typedef(pc);
   }
   if (is_type(pc, CT_ENUM, CT_STRUCT, CT_UNION))
   {
      if (not_type(prev, CT_TYPEDEF))
      {
         fix_enum_struct_union(pc);
      }
   }

   if (is_type(pc, CT_EXTERN))
   {
      if (is_paren_open(next))
      {
         chunk_t* tmp = flag_parens(next, 0, CT_NONE, CT_EXTERN, true);
         if (is_type(tmp, CT_BRACE_OPEN))
         {
            set_paren_parent(tmp, CT_EXTERN);
         }
      }
      else
      {
         /* next likely is a string (see tokenize_cleanup.cpp) */
         set_ptype(next, CT_EXTERN);
         chunk_t* tmp = get_next_ncnl(next);
         if (is_type(tmp, CT_BRACE_OPEN))
         {
            set_paren_parent(tmp, CT_EXTERN);
         }
      }
   }

   if (is_type(pc, CT_TEMPLATE))
   {
      if (is_lang(cpd, LANG_D)) { handle_d_template  (pc); }
      else                      { handle_cpp_template(pc); }
   }

   if (is_type          (pc,   CT_WORD) &&
       is_type_and_ptype(next, CT_ANGLE_OPEN, CT_TEMPLATE))
   {
      mark_template_func(pc, next);
   }

   if (are_types(pc, CT_SQUARE_CLOSE, next, CT_PAREN_OPEN))
   {
      flag_parens(next, 0, CT_FPAREN_OPEN, CT_NONE, false);
   }

   if (is_type(pc, CT_TYPE_CAST))
   {
      fix_type_cast(pc);
   }

   if (is_ptype(pc, CT_ASSIGN                    ) &&
       is_type (pc, CT_BRACE_OPEN, CT_SQUARE_OPEN) )
   {
      /* Mark everything in here as in assign */
      flag_parens(pc, PCF_IN_ARRAY_ASSIGN, pc->type, CT_NONE, false);
   }

   if (is_type(pc, CT_D_TEMPLATE))
   {
      set_paren_parent(next, pc->type);
   }

   /* A word before an open paren is a function call or definition.
    * CT_WORD => CT_FUNC_CALL or CT_FUNC_DEF */
   if (is_type(next, CT_PAREN_OPEN))
   {
      chunk_t* tmp = get_next_ncnl(next);
      if (is_lang(cpd, LANG_OC) && is_type(tmp, CT_CARET))
      {
         handle_oc_block_type(tmp);

         /* This is the case where a block literal is passed as the
          * first argument of a C-style method invocation. */
         assert(is_valid(tmp));

         if (are_types(tmp, CT_OC_BLOCK_CARET, pc, CT_WORD))
         {
            set_type(pc, CT_FUNC_CALL);
         }
      }
      else if (is_type(pc, CT_WORD, CT_OPERATOR_VAL))
      {
         set_type(pc, CT_FUNCTION);
      }
      else if (is_type(pc, CT_TYPE))
      {
         /* If we are on a type, then we are either on a C++ style cast, a
          * function or we are on a function type.
          * The only way to tell for sure is to find the close paren and see
          * if it is followed by an open paren.
          * "int(5.6)"
          * "int()"
          * "int(foo)(void)"
          *
          * FIXME: this check can be done better... */
         chunk_t* tmp = get_next_type(next, CT_PAREN_CLOSE, (int32_t)next->level);
         tmp = chunk_get_next(tmp);
         if (is_type(tmp, CT_PAREN_OPEN) )
         {
            /* we have "TYPE(...)(" */
            set_type(pc, CT_FUNCTION);
         }
         else
         {
            if (is_ptype(pc, CT_NONE       ) &&
                not_flag(pc, PCF_IN_TYPEDEF) )
            {
               tmp = get_next_ncnl(next);
               if (is_type(tmp, CT_PAREN_CLOSE) )
               {
                  /* we have TYPE() */
                  set_type(pc, CT_FUNCTION);
               }
               else
               {
                  /* we have TYPE(...) */
                  set_type        (pc,   CT_CPP_CAST);
                  set_paren_parent(next, CT_CPP_CAST);
               }
            }
         }
      }
      else if (is_type(pc, CT_ATTRIBUTE))
      {
         flag_parens(next, 0, CT_FPAREN_OPEN, CT_ATTRIBUTE, false);
      }
   }
   if (is_lang(cpd, LANG_PAWN))
   {
      if (is_type(pc, CT_FUNCTION) && (pc->brace_level > 0))
      {
         set_type(pc, CT_FUNC_CALL);
      }
      if (are_types(pc, CT_STATE, next, CT_PAREN_OPEN))
      {
         set_paren_parent(next, pc->type);
      }
   }
   else
   {
      if ( is_type (pc, CT_FUNCTION     ) &&
          (is_ptype(pc, CT_OC_BLOCK_EXPR) || !is_oc_block(pc)))
      {
         mark_function(pc);
      }
   }

   /* Detect C99 member stuff */
   if (is_type(pc,   CT_MEMBER              ) &&
       is_type(prev, CT_COMMA, CT_BRACE_OPEN) )
   {
      set_type (pc,   CT_C99_MEMBER);
      set_ptype(next, CT_C99_MEMBER);
   }

   /* Mark function parenthesis and braces */
   if (is_type(pc, CT_FUNC_CALL, CT_FUNC_CALL_USER,
                   CT_FUNC_DEF,  CT_FUNC_PROTO))
   {
      chunk_t* tmp = next;
      if (is_type(tmp, CT_SQUARE_OPEN))
      {
         tmp = set_paren_parent(tmp, pc->type);
      }
      else if (is_type (tmp, CT_TSQUARE ) ||
               is_ptype(tmp, CT_OPERATOR) )
      {
         tmp = get_next_ncnl(tmp);
      }

      if (is_valid(tmp))
      {
         if (is_paren_open(tmp))
         {
            tmp = flag_parens(tmp, 0, CT_FPAREN_OPEN, pc->type, false);
            if (is_valid(tmp))
            {
               if (is_type(tmp, CT_BRACE_OPEN))
               {
                  if (not_ptype(tmp, CT_DOUBLE_BRACE) &&
                      not_flag(pc, PCF_IN_CONST_ARGS) )
                  {
                     set_paren_parent(tmp, pc->type);
                  }
               }
               else if (is_semicolon(tmp) && is_type(pc, CT_FUNC_PROTO))
               {
                  set_ptype(tmp, pc->type);
               }
            }
         }
      }
   }

   /* Mark the parameters in catch() */
   if (are_types(pc, CT_CATCH, next, CT_SPAREN_OPEN))
   {
      fix_fcn_def_params(next);
   }

   if (are_types(pc, CT_THROW, prev, CT_FPAREN_CLOSE))
   {
      set_ptype(pc, prev->ptype);
      if (is_type(next, CT_PAREN_OPEN))
      {
         set_paren_parent(next, CT_THROW);
      }
   }

   /* Mark the braces in: "for_each_entry(xxx) { }" */
   if (is_type_and_not_ptype(pc, CT_BRACE_OPEN, CT_DOUBLE_BRACE) &&
       is_type (prev, CT_FPAREN_CLOSE                ) &&
       is_ptype(prev, CT_FUNC_CALL, CT_FUNC_CALL_USER) &&
       not_flag(pc, PCF_IN_CONST_ARGS                ) )
   {
      set_paren_parent(pc, CT_FUNC_CALL);
   }

   /* Check for a close parenthesis followed by an open parenthesis,
    * which means that we are on a function type declaration (C/C++ only?).
    * Note that typedefs are already taken care of. */
   if (not_flag(pc, (PCF_IN_TYPEDEF | PCF_IN_TEMPLATE)) &&
       not_ptype(pc, 2, CT_CPP_CAST, CT_C_CAST) &&
       not_ptype(pc, 2, CT_OC_MSG_DECL, CT_OC_MSG_SPEC) &&
       !is_preproc (pc) &&
       !is_oc_block(pc) &&

       is_str(pc,   ")"            ) &&
       is_str(next, "("            ) )
   {
      if (is_lang(cpd, LANG_D))
      {
         flag_parens(next, 0, CT_FPAREN_OPEN, CT_FUNC_CALL, false);
      }
      else
      {
         mark_function_type(pc);
      }
   }

   if (is_type (pc, CT_CLASS, CT_STRUCT) &&
       is_level(pc, pc->brace_level    ) )
   {
      if (not_type(pc, CT_STRUCT) ||
          ((cpd.lang_flags & LANG_C) == 0) )
      {
         mark_class_ctor(pc);
      }
   }

   if (is_type(pc, CT_OC_CLASS )) { handle_oc_class(pc); }
   if (is_type(pc, CT_NAMESPACE)) { mark_namespace (pc); }

   /*TODO: Check for stuff that can only occur at the start of an statement */

   if ((cpd.lang_flags & LANG_D) == 0)
   {
      /* Check a parenthesis pair to see if it is a cast.
       * Note that SPAREN and FPAREN have already been marked. */
      if (is_type (pc,      CT_PAREN_OPEN                              ) &&
          is_ptype(pc,      CT_NONE, CT_OC_MSG, CT_OC_BLOCK_EXPR       ) &&
          is_type (next, 8, CT_WORD,   CT_TYPE, CT_QUALIFIER, CT_STRUCT,
                            CT_MEMBER, CT_ENUM, CT_DC_MEMBER, CT_UNION ) &&
          not_type (prev, CT_SIZEOF                                    ) &&
          not_ptype(prev, CT_OPERATOR                                  ) &&
          not_flag (pc,   PCF_IN_TYPEDEF                               ) )
      {
         fix_casts(pc);
      }
   }


   /* Check for stuff that can only occur at the start of an expression */
   if (is_flag(pc, PCF_EXPR_START))
   {
      switch(pc->type)
      {
         case(CT_STAR): /* Change STAR, MINUS, and PLUS in the easy cases */
         // [0x100062020:IN_SPAREN,IN_FOR,STMT_START,EXPR_START,PUNCTUATOR]
         // prev->type is CT_COLON ==> CT_DEREF
            switch(prev->type)
            {
               case(CT_ANGLE_CLOSE): set_type(pc, CT_PTR_TYPE); break;
               case(CT_COLON      ): set_type(pc, CT_DEREF   ); break;
               default:              set_type(pc, CT_DEREF   ); break;
            }
         break;

         case(CT_CARET):
            if (is_lang(cpd, LANG_CPP) && is_type(prev, CT_ANGLE_CLOSE))
            {
               set_type(pc, CT_PTR_TYPE);
            }
            if (is_lang(cpd, LANG_OC))
            {
               /* This is likely the start of a block literal */
               handle_oc_block_literal(pc);
            }
         break;

         case(CT_MINUS       ): set_type(pc, CT_NEG          ); break;
         case(CT_PLUS        ): set_type(pc, CT_POS          ); break;
         case(CT_INCDEC_AFTER): set_type(pc, CT_INCDEC_BEFORE); break;
         case(CT_AMP         ): set_type(pc, CT_ADDR         ); break;
         default:               /* unexpected type */           break;
      }
   }

   /* Detect a variable definition that starts with struct/enum/union/class */
   if (not_flag (pc,   PCF_IN_TYPEDEF) &&
       not_ptype(prev, CT_CPP_CAST   ) &&
       not_flag (prev, PCF_IN_FCN_DEF) &&
       is_type(pc, CT_STRUCT, CT_UNION, CT_CLASS, CT_ENUM) )
   {
      chunk_t* tmp = skip_dc_member(next);
      if (is_type(tmp, CT_TYPE, CT_WORD))
      {
         set_type_and_ptype(tmp, CT_TYPE, pc->type);

         tmp = get_next_ncnl(tmp);
      }
      if (is_type(tmp, CT_BRACE_OPEN) )
      {
         tmp = chunk_skip_to_match(tmp);
         tmp = get_next_ncnl(tmp);
      }
      if (is_ptr_operator(tmp) || is_type(tmp, CT_WORD) )
      {
         mark_variable_definition(tmp);
      }
   }

   /* Change the parenthesis pair after a function/macro-function
    * CT_PAREN_OPEN => CT_FPAREN_OPEN */
   if (is_type(pc, CT_MACRO_FUNC))
   {
      flag_parens(next, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_MACRO_FUNC, false);
   }

   if (is_type(pc, CT_MACRO_OPEN, CT_MACRO_ELSE, CT_MACRO_CLOSE) )
   {
      if (is_type(next, CT_PAREN_OPEN))
      {
         flag_parens(next, 0, CT_FPAREN_OPEN, pc->type, false);
      }
   }

   if (are_types(pc, CT_DELETE, next, CT_TSQUARE))
   {
      set_ptype(next, CT_DELETE);
   }

   /* Change CT_STAR to CT_PTR_TYPE or CT_ARITH or CT_DEREF */
   if ( is_type(pc, CT_STAR                            ) ||
       (is_type(pc, CT_CARET) && is_lang(cpd, LANG_CPP)) )
   {
      if (is_paren_close(next) ||
         (is_type(next, CT_COMMA)))
      {
         set_type(pc, CT_PTR_TYPE);
      }
      else if (is_lang(cpd, LANG_OC) && (is_type(next, CT_STAR)))
      {
         /* Change pointer-to-pointer types in OC_MSG_DECLs
          * from ARITH <===> DEREF to PTR_TYPE <===> PTR_TYPE */
         set_type_and_ptype(pc,   CT_PTR_TYPE, prev->ptype);
         set_type_and_ptype(next, CT_PTR_TYPE, pc->ptype  );
      }
      else if (is_type(pc,   CT_STAR             ) &&
               is_type(prev, CT_SIZEOF, CT_DELETE) )
      {
         set_type(pc, CT_DEREF);
      }
      else if ((is_type(prev, CT_WORD) && chunk_ends_type(prev)) ||
                is_type(prev, CT_DC_MEMBER, CT_PTR_TYPE)         )
      {
         set_type(pc, CT_PTR_TYPE);
      }
      else if (is_type(next, CT_SQUARE_OPEN) &&
               !is_lang(cpd, LANG_OC))
      {
         set_type(pc, CT_PTR_TYPE);
      }
      else if (is_type(pc, CT_STAR))
      {
         /* A star can have three meanings
          * 1. CT_DEREF    = pointer dereferencation
          * 2. CT_PTR_TYPE = pointer definition
          * 3. CT_ARITH    = arithmetic multiplication */
         if (is_type(prev, CT_TYPE))
         {
            set_type(pc, CT_PTR_TYPE);
         }
         else
         {
            const c_token_t type = ( is_flag(prev, PCF_PUNCTUATOR)                    &&
                             (!is_paren_close(prev) || is_ptype(prev, CT_MACRO_FUNC)) &&
                               not_type(prev, CT_SQUARE_CLOSE, CT_DC_MEMBER)          ) ?
                               CT_DEREF : CT_ARITH;
            set_type(pc, type);
         }
      }
   }

   if (is_type(pc, CT_AMP))
   {
      if      (is_type(prev, CT_DELETE)) { set_type(pc, CT_ADDR ); }
      else if (is_type(prev, CT_TYPE  )) { set_type(pc, CT_BYREF); }
      else if (is_type(next, CT_FPAREN_CLOSE, CT_COMMA))
      {
         // connect(&mapper, SIGNAL(mapped(QString &)), this, SLOT(onSomeEvent(QString &)));
         set_type(pc, CT_BYREF);
      }
      else
      {
         set_type(pc, CT_ARITH);
         if (is_type(prev, CT_WORD))
         {
            chunk_t* tmp = get_prev_ncnl(prev);
            if (is_type(tmp, CT_SEMICOLON,  CT_VSEMICOLON,
                             CT_BRACE_OPEN, CT_QUALIFIER))
            {
               set_type(prev, CT_TYPE);
               set_type(pc,   CT_ADDR);
               set_flags(next, PCF_VAR_1ST);
            }
         }
      }
   }

   if (is_type(pc, CT_MINUS, CT_PLUS ) )
   {
      const bool pc_is_minus = is_type(pc, CT_MINUS);
      const c_token_t pc_new_type = (pc_is_minus) ? CT_NEG : CT_POS;
      switch(prev->type)
      {
         case(CT_POS     ): /* fallthrough */
         case(CT_NEG     ): /* fallthrough */
         case(CT_OC_CLASS): set_type(pc, pc_new_type); break;
         default:           set_type(pc, CT_ARITH   ); break;
      }
   }

   /* Check for extern "C" NSString* i;
    * NSString is a type
    * change CT_WORD => CT_TYPE     for pc
    * change CT_STAR => CT_PTR_TYPE for pc-next */
   if (is_type(pc, CT_WORD))      // here NSString
   {
      if (is_type(pc->next, CT_STAR)) // here *
      {
         if (is_type(pc->prev, CT_STRING))
         {
            /* compare text with "C" to find extern "C" instructions */
            if (unc_text::compare(pc->prev->text(), "\"C\"") == 0)
            {
               if (is_type(pc->prev->prev, CT_EXTERN))
               {
                  set_type(pc,       CT_TYPE    ); // change CT_WORD => CT_TYPE
                  set_type(pc->next, CT_PTR_TYPE); // change CT_STAR => CT_PTR_TYPE
               }
            }
         }
         // Issue #322 STDMETHOD(GetValues)(BSTR bsName, REFDATA** pData);
         if (is_type(pc->next->next, CT_STAR) &&
             is_flag(pc, PCF_IN_CONST_ARGS  ) )
         {
            // change CT_STAR => CT_PTR_TYPE
            set_type(pc->next,       CT_PTR_TYPE);
            set_type(pc->next->next, CT_PTR_TYPE);
         }
         // Issue #222 whatever3 *(func_ptr)( whatever4 *foo2, ...
         if (is_type(pc->next->next, CT_WORD) &&
             is_flag(pc, PCF_IN_FCN_DEF     ) )
         {
            set_type(pc->next, CT_PTR_TYPE);
         }
      }
   }

   /* Check for __attribute__((visibility ("default"))) NSString* i;
    * NSString is a type
    * change CT_WORD => CT_TYPE     for pc
    * change CT_STAR => CT_PTR_TYPE for pc-next */
   if (is_type(pc, CT_WORD))      // here NSString
   {
      if (is_type(pc->next, CT_STAR)) // here *
      {
         chunk_t* tmp = pc;
         while (is_valid(tmp))
         {
            if (is_type(tmp, CT_ATTRIBUTE))
            {
               LOG_FMT(LGUY, "ATTRIBUTE found %s:%s\n", get_token_name(tmp->type), tmp->text());
               LOG_FMT(LGUY, "for token %s:%s\n",       get_token_name(pc->type ), pc->text() );

               set_type(pc,       CT_TYPE    ); // change CT_WORD => CT_TYPE
               set_type(pc->next, CT_PTR_TYPE); // change CT_STAR => CT_PTR_TYPE
            }
            break_if(is_flag(tmp, PCF_STMT_START));  // we are at beginning of the line

            tmp = chunk_get_prev(tmp);
         }
      }
   }
}


static void check_double_brace_init(chunk_t* bo1)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LJDBI, "%s: %u:%u", __func__, bo1->orig_line, bo1->orig_col);
   chunk_t* pc = get_prev_ncnl(bo1);
   if (is_paren_close(pc))
   {
      chunk_t* bo2 = chunk_get_next(bo1);
      if (is_type(bo2, CT_BRACE_OPEN))
      {
         /* found a potential double brace */
         chunk_t* bc2 = chunk_skip_to_match(bo2);
         chunk_t* bc1 = chunk_get_next     (bc2);
         if (is_type(bc1, CT_BRACE_CLOSE))
         {
            LOG_FMT(LJDBI, " - end %u:%u\n", bc2->orig_line, bc2->orig_col);
            /* delete bo2 and bc1 */
            assert(is_valid(bo2));
            bo1->str         += bo2->str;
            bo1->orig_col_end = bo2->orig_col_end;
            chunk_del(bo2);
            set_ptype(bo1, CT_DOUBLE_BRACE);

            assert(is_valid(bc1));
            bc2->str         += bc1->str;
            bc2->orig_col_end = bc1->orig_col_end;
            chunk_del(bc1);
            set_ptype(bc2, CT_DOUBLE_BRACE);
            return;
         }
      }
   }
   LOG_FMT(LJDBI, " - no\n");
}


void fix_symbols(void)
{
   LOG_FUNC_ENTRY();

   cpd.unc_stage = unc_stage_e::FIX_SYMBOLS;
   mark_define_expressions();

   bool is_java = (cpd.lang_flags & LANG_JAVA) != 0;
   chunk_t* pc;
   for (pc = chunk_get_head(); is_valid(pc); pc = get_next_ncnl(pc))
   {
      if (is_type(pc, CT_FUNC_WRAP, CT_TYPE_WRAP))
      {
         handle_wrap(pc);
      }

      if (             is_type(pc, CT_ASSIGN    )) { mark_lvalue            (pc); }
      if ((is_java) && is_type(pc, CT_BRACE_OPEN)) { check_double_brace_init(pc); }
   }

   pc = chunk_get_head();
   if(is_cmt_or_nl(pc))
   {
      pc = get_next_ncnl(pc);
   }
   chunk_t dummy;
   while (is_valid(pc))
   {
      chunk_t* prev = get_prev_ncnl(pc, scope_e::PREPROC);
      if (is_invalid(prev)) { prev = &dummy; }

      chunk_t* next = get_next_ncnl(pc, scope_e::PREPROC);
      if (is_invalid(next)) { next = &dummy; }

      do_symbol_check(prev, pc, next);
      pc = get_next_ncnl(pc);
   }

   pawn_add_virtual_semicolons();
   process_returns();

   /* 2nd pass - handle variable definitions
    * REVISIT: We need function params marked to do this (?) */
   pc = chunk_get_head();
   int32_t square_level = -1;
   while (is_valid(pc))
   {
      /* Can't have a variable definition inside [ ] */
      if (square_level < 0)
      {
         if (is_type(pc, CT_SQUARE_OPEN))
         {
            square_level = (int32_t)pc->level;
         }
      }
      else
      {
         if (pc->level <= static_cast<uint32_t>(square_level))
         {
            square_level = -1;
         }
      }

      /* A variable definition is possible after at the start of a statement
       * that starts with: QUALIFIER, TYPE, or WORD */
      if ((square_level < 0          ) &&
           is_flag(pc, PCF_STMT_START) &&
          (is_type(pc, CT_QUALIFIER, CT_TYPE, CT_TYPENAME, CT_WORD)) &&
           not_ptype(pc, CT_ENUM    ) &&
           not_flag (pc, PCF_IN_ENUM) )
      {
         pc = fix_var_def(pc);
      }
      else
      {
         pc = get_next_ncnl(pc);
      }
   }
}


static void mark_lvalue(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   return_if(is_preproc(pc));

   chunk_t* prev;
   for (prev = get_prev_ncnl(pc);
        is_valid(prev);
        prev = get_prev_ncnl(prev))
   {
      if ((prev->level < pc->level) ||
          is_type(prev, CT_ASSIGN, CT_COMMA, CT_BOOL) ||
          is_semicolon(prev) ||
          is_str(prev, "(" ) ||
          is_str(prev, "{" ) ||
          is_str(prev, "[" ) ||
          is_preproc(prev))
      {
         break;
      }
      set_flags(prev, PCF_LVALUE);
      if (is_level(prev, pc->level) &&
          is_str  (prev, "&"      ) )
      {
         make_type(prev);
      }
   }
}


static void mark_function_return_type(chunk_t* fname, chunk_t* start, c_token_t parent_type)
{
   LOG_FUNC_ENTRY();
   assert(are_valid(fname, start));
   chunk_t* pc = start;

   if (is_valid(pc))
   {
      /* Step backwards from pc and mark the parent of the return type */
      LOG_FMT(LFCNR, "%s: (backwards) return type for '%s' @ %u:%u",
              __func__, fname->text(), fname->orig_line, fname->orig_col);
#ifdef DEBUG
      LOG_FMT(LFCN, "\n");
#endif

      chunk_t* first = pc;
      chunk_t* save;
      while (is_valid(pc))
      {
         LOG_FMT(LFCNR, "%s(%d): pc: %s, type is %s\n", __func__, __LINE__, pc->text(), get_token_name(pc->type));
#ifdef DEBUG
         log_pcf_flags(LFCNR, pc->flags);
#endif
         break_if((!is_var_type(pc) &&
                   not_type(pc, CT_OPERATOR, CT_WORD, CT_ADDR)) ||
                   is_preproc(pc));

         if (!is_ptr_operator(pc))
         {
            first = pc;
         }
         save = pc; // keep a copy
         pc   = get_prev_ncnl(pc);
         if (pc != nullptr)
         {
            log_pcf_flags(LFCNR, pc->flags);
            // Issue #1027
            if ((pc->flags & PCF_IN_TEMPLATE) &&
                (pc->type == CT_ANGLE_CLOSE))
            {
               // look for the opening angle
               pc = get_prev_type(pc, CT_ANGLE_OPEN, save->level);
               if (pc != nullptr)
               {
                  // get the prev
                  pc = chunk_get_prev(pc);
                  if (pc != nullptr)
                  {
                     if (pc->type == CT_TYPE)
                     {
                        first = save;
                        break;
                     }
                  }
               }
            }
         }
      }

      pc = first;
      while (is_valid(pc))
      {
         LOG_FMT(LFCNR, " [%s|%s]", pc->text(), get_token_name(pc->type));

         if (parent_type != CT_NONE) { set_ptype(pc, parent_type); }
         make_type(pc);
         break_if(pc == start);
         pc = get_next_ncnl(pc);
      }
      LOG_FMT(LFCNR, "\n");
   }
}


static bool mark_function_type(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   assert(is_valid(pc));
   LOG_FMT(LFTYPE, "%s: [%s] %s @ %u:%u\n", __func__,
           get_token_name(pc->type), pc->text(), pc->orig_line, pc->orig_col);

   /* note cannot move variable definitions below due to switch */
   uint32_t    star_count = 0;
   uint32_t    word_count = 0;
   chunk_t   *ptrcnk    = nullptr;
   chunk_t   *tmp;
   chunk_t   *apo;
   chunk_t   *apc;
   chunk_t   *aft;
   bool      anon = false;
   c_token_t pt, ptp;

   /* Scan backwards across the name, which can only be a word and single star */
   chunk_t* varcnk = get_prev_ncnl(pc);
   if (!is_word(varcnk))
   {
      if (is_lang(cpd, LANG_OC) &&
           is_str(varcnk, "^" ) &&
           is_paren_open(get_prev_ncnl(varcnk)))
      {
         /* anonymous ObjC block type -- RTYPE (^)(ARGS) */
         anon = true;
      }
      else
      {
         assert(is_valid(varcnk));
         LOG_FMT(LFTYPE, "%s: not a word '%s' [%s] @ %u:%u\n",
                 __func__, varcnk->text(), get_token_name(varcnk->type),
                 varcnk->orig_line, varcnk->orig_col);
         goto nogo_exit;
      }
   }

   apo = get_next_ncnl(pc);
   apc = chunk_skip_to_match(apo);
   if (!is_paren_open(apo) ||
       ((apc = chunk_skip_to_match(apo)) == nullptr))
   {
      LOG_FMT(LFTYPE, "%s: not followed by parens\n", __func__);
      goto nogo_exit;
   }
   aft = get_next_ncnl(apc);

   switch(aft->type)
   {
      case(CT_BRACE_OPEN): pt = CT_FUNC_DEF;   break;
      case(CT_SEMICOLON ): /* fallthrough */
      case(CT_ASSIGN    ): pt = CT_FUNC_PROTO; break;
      default:
         LOG_FMT(LFTYPE, "%s: not followed by '{' or ';'\n", __func__);
         goto nogo_exit;
   }

   ptp = (is_flag(pc, PCF_IN_TYPEDEF)) ? CT_FUNC_TYPE : CT_FUNC_VAR;

   tmp = pc;
   while ((tmp = get_prev_ncnl(tmp)) != nullptr)
   {
      LOG_FMT(LFTYPE, " -- [%s] %s on line %u, col %u",
              get_token_name(tmp->type), tmp->text(),
              tmp->orig_line, tmp->orig_col);

      if (is_star(tmp) || is_type(tmp, CT_PTR_TYPE, CT_CARET))
      {
         star_count++;
         ptrcnk = tmp;
         LOG_FMT(LFTYPE, " -- PTR_TYPE\n");
      }
      else if (is_word(tmp) || is_type(tmp, CT_WORD, CT_TYPE))
      {
         word_count++;
         LOG_FMT(LFTYPE, " -- TYPE(%s)\n", tmp->text());
      }
      else if (is_type(tmp, CT_DC_MEMBER))
      {
         word_count = 0;
         LOG_FMT(LFTYPE, " -- :: reset word_count\n");
      }
      else if (is_str(tmp, "("))
      {
         LOG_FMT(LFTYPE, " -- open parenthesis (break)\n");
         break;
      }
      else
      {
         LOG_FMT(LFTYPE, " --  unexpected token [%s] %s on line %u, col %u\n",
                 get_token_name(tmp->type), tmp->text(),
                 tmp->orig_line, tmp->orig_col);
         goto nogo_exit;
      }
   }

   if ((star_count > 1) ||
       (word_count > 1) ||
       ((star_count + word_count) == 0))
   {
      LOG_FMT(LFTYPE, "%s: bad counts word:%u, star:%u\n", __func__,
              word_count, star_count);
      goto nogo_exit;
   }

   /* make sure what appears before the first open paren can be a return type */
   if (!chunk_ends_type(get_prev_ncnl(tmp)))
   {
      goto nogo_exit;
   }

   if (ptrcnk) { set_type(ptrcnk, CT_PTR_TYPE); }

   if (anon == false)
   {
      if (is_flag(pc, PCF_IN_TYPEDEF))
      {
         set_type(varcnk, CT_TYPE);
      }
      else
      {
         set_type(varcnk, CT_FUNC_VAR);
         set_flags(varcnk, PCF_VAR_1ST_DEF);
      }
   }
   set_type_and_ptype(pc,  CT_TPAREN_CLOSE, ptp);
   set_type_and_ptype(apo, CT_FPAREN_OPEN,  pt);
   set_type_and_ptype(apc, CT_FPAREN_CLOSE, pt);
   fix_fcn_def_params(apo);

   if (is_semicolon(aft))
   {
      set_ptype(aft, (is_flag(aft, PCF_IN_TYPEDEF)) ? CT_TYPEDEF : CT_FUNC_VAR);
   }
   else if (is_type(aft, CT_BRACE_OPEN))
   {
      flag_parens(aft, 0, CT_NONE, pt, false);
   }

   /* Step backwards to the previous open paren and mark everything a */
   tmp = pc;
   while ((tmp = get_prev_ncnl(tmp)) != nullptr)
   {
      LOG_FMT(LFTYPE, " ++ [%s] %s on line %u, col %u\n",
              get_token_name(tmp->type), tmp->text(),
              tmp->orig_line, tmp->orig_col);

      if (*tmp->str.c_str() == '(')
      {
         if (not_flag(pc, PCF_IN_TYPEDEF))
         {
            set_flags(tmp, PCF_VAR_1ST_DEF);
         }
         set_type_and_ptype(tmp, CT_TPAREN_OPEN, ptp);

         tmp = get_prev_ncnl(tmp);
         if (is_type(tmp, 5, CT_FUNCTION, CT_FUNC_CALL, CT_FUNC_CALL_USER,
                             CT_FUNC_DEF, CT_FUNC_PROTO))
         {
            set_type       (tmp, CT_TYPE);
            clr_flags(tmp, PCF_VAR_1ST_DEF);
         }
         mark_function_return_type(varcnk, tmp, ptp);
         break;
      }
   }
   return(true);

nogo_exit:
   tmp = get_next_ncnl(pc);
   if (is_paren_open(tmp))
   {
      assert(is_valid(tmp));
      LOG_FMT(LFTYPE, "%s:%d setting FUNC_CALL on %u:%u\n",
              __func__, __LINE__, tmp->orig_line, tmp->orig_col);
      flag_parens(tmp, 0, CT_FPAREN_OPEN, CT_FUNC_CALL, false);
   }
   return(false);
}


static void process_returns(void)
{
   LOG_FUNC_ENTRY();
   chunk_t* pc = chunk_get_head();
   while (is_valid(pc))
   {
      if (not_type(pc, CT_RETURN) || is_preproc(pc))
      {
         pc = get_next_type(pc, CT_RETURN, -1);
         continue;
      }
      pc = process_return(pc);
   }
}


static chunk_t* process_return(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   /* grab next and bail if it is a semicolon */
   chunk_t* next = get_next_ncnl(pc);
   retval_if((is_invalid(next) || is_semicolon(next)), next);

   if (not_ignore(UO_nl_return_expr))
   {
      nl_iarf(pc, get_arg(UO_nl_return_expr));
   }

   chunk_t* temp;
   chunk_t* semi;
   chunk_t* cpar;
   chunk_t chunk;
   if (is_type(next, CT_PAREN_OPEN))
   {
      /* See if the return is fully paren'd */
      cpar = get_next_type(next, CT_PAREN_CLOSE, (int32_t)next->level);
      semi = get_next_ncnl(cpar);
      assert(is_valid(semi));
      if (is_semicolon(semi))
      {
         if (cpd.settings[UO_mod_paren_on_return].a == AV_REMOVE)
         {
            LOG_FMT(LRETURN, "%s: removing parens on line %u\n",
                    __func__, pc->orig_line);

            /* lower the level of everything */
            for (temp = next; temp != cpar; temp = chunk_get_next(temp))
            {
               temp->level--;
            }

            /* delete the parenthesis */
            chunk_del(next);
            chunk_del(cpar);

            /* back up the semicolon */
            semi->column--;
            semi->orig_col--;
            semi->orig_col_end--;
         }
         else
         {
            LOG_FMT(LRETURN, "%s: keeping parenthesis on line %u\n",
                    __func__, pc->orig_line);

            /* mark & keep them */
            set_ptype(next, CT_RETURN);
            set_ptype(cpar, CT_RETURN);
         }
         return(semi);
      }
   }

   /* We don't have a fully paren'd return. Should we add some? */
   if ((cpd.settings[UO_mod_paren_on_return].a & AV_ADD) == 0) /*lint !e641 !e655 */
   {
      return(next);
   }

   /* find the next semicolon on the same level */
   semi = next;
   while ((semi = chunk_get_next(semi)) != nullptr)
   {
      break_if((is_semicolon(semi) && is_level(pc, semi->level)) ||
               (semi->level < pc->level));
   }
   assert(is_valid(semi));
   if (is_semicolon(semi) && is_level(pc, semi->level))
   {
      /* add the parens */
      chunk.type        = CT_PAREN_OPEN;
      chunk.str         = "(";
      chunk.level       = pc->level;
      chunk.brace_level = pc->brace_level;
      chunk.orig_line   = pc->orig_line;
      chunk.ptype = CT_RETURN;
      set_flags(&chunk, get_flags(pc, PCF_COPY_FLAGS));
      chunk_add_before(&chunk, next);

      chunk.type      = CT_PAREN_CLOSE;
      chunk.str       = ")";

      assert(is_valid(semi));
      chunk.orig_line = semi->orig_line;
      cpar            = chunk_add_before(&chunk, semi);

      LOG_FMT(LRETURN, "%s: added parens on line %u\n",
              __func__, pc->orig_line);

      for (temp = next; temp != cpar; temp = chunk_get_next(temp))
      {
         temp->level++;
      }
   }
   return(semi);
}


static bool is_ucase_str(const char *str, uint32_t len)
{
   while (len-- > 0)
   {
      retval_if(unc_toupper(*str) != *str, false);
      str++;
   }
   return(true);
}


static bool is_oc_block(chunk_t* pc)
{
   return(is_ptype(pc, 4, CT_OC_BLOCK_TYPE, CT_OC_BLOCK_EXPR,
                          CT_OC_BLOCK_ARG,  CT_OC_BLOCK) ||
           is_type(pc,       CT_OC_BLOCK_CARET) ||
           is_type(pc->next, CT_OC_BLOCK_CARET) ||
           is_type(pc->prev, CT_OC_BLOCK_CARET));
}


static void fix_casts(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(start));
   LOG_FMT(LCASTS, "%s:line %u, col %u:", __func__, start->orig_line, start->orig_col);

   chunk_t* prev = get_prev_ncnl(start);
   if (is_type(prev, CT_PP_DEFINED))
   {
      LOG_FMT(LCASTS, " -- not a cast - after defined\n");
      return;
   }

   chunk_t*   first;
   chunk_t*   after;
   chunk_t*   last = nullptr;
   chunk_t*   paren_close;
   const char *verb      = "likely";
   const char *detail    = "";
   uint32_t     count      = 0;
   int        word_count = 0;
   bool       nope;
   bool       doubtful_cast = false;

   /* Make sure there is only WORD, TYPE, and '*' or '^' before the close parenthesis */
   chunk_t* pc = get_next_ncnl(start);
   first = pc;
   while (is_var_type(pc) ||
          is_type(pc, 7, CT_WORD, CT_QUALIFIER, CT_AMP,
           CT_DC_MEMBER, CT_STAR, CT_TSQUARE,   CT_CARET))
   {
      LOG_FMT(LCASTS, " [%s]", get_token_name(pc->type));

      if      (is_type(pc, CT_WORD     )) { word_count++; }
      else if (is_type(pc, CT_DC_MEMBER)) { word_count--; }

      last = pc;
      pc   = get_next_ncnl(pc);
      count++;
   }

   assert(is_valid(prev));
   if (not_type(pc,   CT_PAREN_CLOSE) ||
       is_type (prev, CT_OC_CLASS   ) )
   {
      LOG_FMT(LCASTS, " -- not a cast, hit [%s]\n",
              (is_invalid(pc)) ? "nullptr"  : get_token_name(pc->type));
      return;
   }

   if (word_count > 1)
   {
      LOG_FMT(LCASTS, " -- too many words: %d\n", word_count);
      return;
   }
   paren_close = pc;

   return_if(is_invalid(last));

   /* If last is a type or star/caret, we have a cast for sure */
   if (is_type(last, CT_STAR, CT_CARET, CT_PTR_TYPE, CT_TYPE) )
   {
      verb = "for sure";
   }
   else if (count == 1)
   {
      /* We are on a potential cast of the form "(word)".
       * We don't know if the word is a type. So lets guess based on some
       * simple rules:
       *  - if all caps, likely a type
       *  - if it ends in _t, likely a type
       *  - if it's objective-c and the type is id, likely valid */
      verb = "guessed";
      if ((last->len() > 3) &&
          (last->str[last->len() - 2] == '_') &&
          (last->str[last->len() - 1] == 't') )
      {
         detail = " -- '_t'";
      }
      else if (is_ucase_str(last->text(), last->len()))
      {
         detail = " -- upper case";
      }
      else if (is_lang(cpd, LANG_OC) && is_str(last, "id"))
      {
         detail = " -- Objective-C id";
      }
      else
      {
         /* If we can't tell for sure whether this is a cast, decide against it */
         detail        = " -- mixed case";
         doubtful_cast = true;
      }

      /* If the next item is a * or &, the next item after that can't be a
       * number or string.
       *
       * If the next item is a +, the next item has to be a number.
       * If the next item is a -, the next item can't be a string.
       *
       * For this to be a cast, the close parenthesis must be followed by:
       *  - constant (number or string)
       *  - parenthesis open
       *  - word
       *
       * Find the next non-open parenthesis item. */
      pc    = get_next_ncnl(paren_close);
      after = pc;
      do
      {
         after = get_next_ncnl(after);
      } while (is_type(after, CT_PAREN_OPEN));

      if (is_invalid(after))
      {
         LOG_FMT(LCASTS, " -- not a cast - hit nullptr\n");
         return;
      }

      assert(is_valid(pc));
      nope = false;
      if (is_ptr_operator(pc))
      {
         /* star (*) and address (&) are ambiguous */
         if (is_type(after, CT_NUMBER_FP, CT_NUMBER, CT_STRING) ||
             doubtful_cast)
         {
            nope = true;
         }
      }
      else if (is_type(pc, CT_MINUS))
      {
         /* (uint8_t)-1 or (foo)-1 or (FOO)-'a' */
         if(is_type(after, CT_STRING) || doubtful_cast)
         {
            nope = true;
         }
      }
      else if (is_type(pc, CT_PLUS))
      {
         /* (uint8_t)+1 or (foo)+1 */
         if(not_type(after, CT_NUMBER, CT_NUMBER_FP) || doubtful_cast)
         {
            nope = true;
         }
      }
      else if (not_type(pc, 12, CT_FUNC_CALL_USER,     CT_WORD, CT_STRING,
            CT_BRACE_OPEN, CT_NUMBER_FP, CT_FUNC_CALL, CT_THIS, CT_SIZEOF,
            CT_PAREN_OPEN, CT_FUNCTION,  CT_NUMBER,    CT_TYPE) &&
            (!(is_type(pc, CT_SQUARE_OPEN) && is_lang(cpd, LANG_OC))))
      {
         LOG_FMT(LCASTS, " -- not a cast - followed by '%s' %s\n",
                 pc->text(), get_token_name(pc->type));
         return;
      }

      if (nope)
      {
         LOG_FMT(LCASTS, " -- not a cast - '%s' followed by %s\n",
                 pc->text(), get_token_name(after->type));
         return;
      }
   }

   /* if the 'cast' is followed by a semicolon, comma or close parenthesis, it isn't */
   pc = get_next_ncnl(paren_close);
   if (is_semicolon(pc) || is_type(pc, CT_COMMA) || is_paren_close(pc))
   {
      assert(is_valid(pc));
      LOG_FMT(LCASTS, " -- not a cast - followed by %s\n", get_token_name(pc->type));
      return;
   }

   set_ptype(start,       CT_C_CAST);
   set_ptype(paren_close, CT_C_CAST);

   LOG_FMT(LCASTS, " -- %s c-cast: (", verb);

   for (pc = first; pc != paren_close; pc = get_next_ncnl(pc))
   {
      assert(is_valid(pc));
      set_ptype(pc, CT_C_CAST);
      make_type(pc);
      LOG_FMT(LCASTS, " %s", pc->text());
   }
   LOG_FMT(LCASTS, " )%s\n", detail);

   /* Mark the next item as an expression start */
   pc = get_next_ncnl(paren_close);
   if (is_valid(pc))
   {
      set_flags(pc, PCF_EXPR_START);
      if (is_opening_brace(pc))
      {
         set_paren_parent(pc, start->ptype);
      }
   }
}


static void fix_type_cast(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   chunk_t* pc;

   pc = get_next_ncnl(start);
   return_if(is_invalid_or_not_type(pc, CT_ANGLE_OPEN));

   while (((pc = get_next_ncnl(pc)) != nullptr) &&
          (pc->level >= start->level))
   {
      if (is_type_and_level(pc, CT_ANGLE_CLOSE, start->level))
      {
         pc = get_next_ncnl(pc);
         if (is_str(pc, "("))
         {
            set_paren_parent(pc, CT_TYPE_CAST);
         }
         return;
      }
      make_type(pc);
   }
}


static void fix_enum_struct_union(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   /* Make sure this wasn't a cast */
   return_if(is_invalid_or_type(pc, CT_C_CAST));

   chunk_t* prev        = nullptr;
   uint32_t  flags        = PCF_VAR_1ST_DEF;
   uint32_t  in_fcn_paren = get_flags(pc, PCF_IN_FCN_DEF);

   /* the next item is either a type or open brace */
   chunk_t* next = get_next_ncnl(pc);
   /* the enum-key might be enum, enum class or enum struct (TODO) */
   if (is_type(next, CT_ENUM_CLASS))
   {
      next = get_next_ncnl(next); // get the next one
   }
   /* the next item is either a type, an attribute (TODO), an identifier, a colon or open brace */
   if (is_type(next, CT_TYPE) )
   {
      // i.e. "enum xyz : unsigned int { ... };"
      // i.e. "enum class xyz : unsigned int { ... };"
      // xyz is a type
      set_ptype(next, pc->type);
      prev = next;
      next = get_next_ncnl(next);
//  \todo SN is this needed ? set_chunk_parent(next, pc->type);

      /* next up is either a colon, open brace, or open parenthesis (pawn) */
      return_if(is_invalid(next));

      if (is_lang(cpd, LANG_PAWN) && is_type(next, CT_PAREN_OPEN))
      {
         next = set_paren_parent(next, CT_ENUM);
      }
      else if (are_types(pc, CT_ENUM, next, CT_COLON))
      {
         /* enum TYPE : INT_TYPE { ... }; */
         next = get_next_ncnl(next);
         if (is_valid(next))
         {
            make_type(next);
            next = get_next_ncnl(next);
         }
      }
   }
   if (is_type(next, CT_BRACE_OPEN))
   {
      /* \todo SN which function is the right one? */
//    flag_series(pc, next, (is_type(pc, CT_ENUM)) ? PCF_IN_ENUM : PCF_IN_STRUCT);
      flag_parens(next, (is_type(pc, CT_ENUM)) ? PCF_IN_ENUM : PCF_IN_STRUCT,
                  CT_NONE, CT_NONE, false);

      if (is_type(pc, CT_UNION, CT_STRUCT) )
      {
         mark_struct_union_body(next);
      }

      /* Skip to the closing brace */
      set_ptype(next, pc->type);
      next   = get_next_type(next, CT_BRACE_CLOSE, (int32_t)pc->level);
      flags |= PCF_VAR_INLINE;
      if (is_valid(next))
      {
         set_ptype(next, pc->type);
         next = get_next_ncnl(next);
      }
      prev = nullptr;
   }
   /* reset var name parent type */
   else if (are_valid(next, prev))
   {
      set_ptype(prev, CT_NONE);
   }

   return_if(is_type(next, CT_PAREN_CLOSE));

   if (!is_semicolon(next))
   {
      /* Pawn does not require a semicolon after an enum */
      return_if(is_lang(cpd, LANG_PAWN));

      /* D does not require a semicolon after an enum, but we add one to make
       * other code happy.*/
      if (is_lang(cpd, LANG_D))
      {
         next = pawn_add_vsemi_after(get_prev_ncnl(next));
      }
   }

   /* We are either pointing to a ';' or a variable */
   while (not_type(next, CT_ASSIGN) && !is_semicolon(next) &&
          (in_fcn_paren ^ (not_flag(next, PCF_IN_FCN_DEF))))
   {
      if (is_level(next, pc->level))
      {
         if (is_type(next, CT_WORD))
         {
            set_flags(next, flags);
            flags &= ~PCF_VAR_1ST;       /* clear the first flag for the next items */
         }

         if ((is_type(next, CT_STAR )                                ) ||
             (is_type(next, CT_CARET) && is_lang(cpd, LANG_CPP)) )
         {
            set_type(next, CT_PTR_TYPE);
         }

         /* If we hit a comma in a function param, we are done */
         return_if(is_type(next, CT_COMMA, CT_FPAREN_CLOSE ) &&
                   is_flag(next, (PCF_IN_FCN_DEF | PCF_IN_FCN_CALL)));
      }

      next = get_next_ncnl(next);
   }

   if (is_type(next, CT_SEMICOLON) && is_invalid(prev))
   {
      set_ptype(next, pc->type);
   }
}


static void fix_typedef(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(start));
   LOG_FMT(LTYPEDEF, "%s: typedef @ %u:%u\n", __func__,
         start->orig_line, start->orig_col);

   chunk_t* the_type = nullptr;
   chunk_t* last_op  = nullptr;
   chunk_t* open_paren;

   /* Mark everything in the typedef and scan for ")(", which makes it a
    * function type */
   chunk_t* next = start;
   while (((next = get_next_ncnl(next, scope_e::PREPROC)) != nullptr) &&
          (next->level >= start->level))
   {
      set_flags(next, PCF_IN_TYPEDEF);
      if (is_level(start, next->level))
      {
         if (is_semicolon(next))
         {
            set_ptype(next, CT_TYPEDEF);
            break;
         }
         break_if (is_type(next, CT_ATTRIBUTE));

         if (is_lang(cpd, LANG_D) && is_type(next, CT_ASSIGN))
         {
            set_ptype(next, CT_TYPEDEF);
            break;
         }
         make_type(next);
         if (is_type(next, CT_TYPE)) { the_type = next; }

         clr_flags(next, PCF_VAR_1ST_DEF);
         if (*next->str.c_str() == '(') { last_op = next; }
      }
   }

   /* avoid interpreting typedef NS_ENUM (NSInteger, MyEnum) as a function def */
   if ( is_valid(last_op)                                     &&
       !(is_lang(cpd, LANG_OC) && is_ptype(last_op, CT_ENUM)) )
   {
      flag_parens(last_op, 0, CT_FPAREN_OPEN, CT_TYPEDEF, false);
      fix_fcn_def_params(last_op);

      open_paren = nullptr;
      the_type   = get_prev_ncnl(last_op, scope_e::PREPROC);
      if (is_paren_close(the_type))
      {
         open_paren = chunk_skip_to_match_rev(the_type);
         mark_function_type(the_type);
         the_type = get_prev_ncnl(the_type, scope_e::PREPROC);
      }
      else
      {
         /* must be: "typedef <return type>func(params);" */
         set_type(the_type, CT_FUNC_TYPE);
      }
      set_ptype(the_type, CT_TYPEDEF);

      assert(is_valid(the_type));
      LOG_FMT(LTYPEDEF, "%s: function typedef [%s] on line %u\n",
              __func__, the_type->text(), the_type->orig_line);

      /* If we are aligning on the open parenthesis, grab that instead */
      if (is_valid(open_paren) &&
         (get_uval(UO_align_typedef_func) == 1))
      {
         the_type = open_paren;
      }
      if (get_uval(UO_align_typedef_func) != 0)
      {
         LOG_FMT(LTYPEDEF, "%s:  -- align anchor on [%s] @ %u:%u\n",
                 __func__, the_type->text(), the_type->orig_line, the_type->orig_col);
         set_flags(the_type, PCF_ANCHOR);
      }
      return; /* already did everything we need to do */
   }

   /* Skip over enum/struct/union stuff, as we know it isn't
    * a return type for a function type */
   next = get_next_ncnl(start, scope_e::PREPROC);
   return_if(is_invalid(next));

   if (not_type(next, CT_ENUM, CT_STRUCT, CT_UNION ))
   {
      if (is_valid(the_type))
      {
         /* We have just a regular typedef */
         LOG_FMT(LTYPEDEF, "%s: regular typedef [%s] on line %u\n",
                 __func__, the_type->text(), the_type->orig_line);
         set_flags(the_type, PCF_ANCHOR);
      }
      return;
   }

   /* We have a struct/union/enum type, set the parent */
   c_token_t tag = next->type;

   /* the next item should be either a type or { */
   next = get_next_ncnl(next, scope_e::PREPROC);
   return_if(is_invalid(next));

   if (is_type(next, CT_TYPE))
   {
      next = get_next_ncnl(next, scope_e::PREPROC);
   }
   return_if(is_invalid(next));

   if (is_type(next, CT_BRACE_OPEN))
   {
      set_ptype(next, tag);
      /* Skip to the closing brace */
      next = get_next_type(next, CT_BRACE_CLOSE, (int32_t)next->level, scope_e::PREPROC);
      if (is_valid(next)) { set_ptype(next, tag); }
   }

   if (is_valid(the_type))
   {
      LOG_FMT(LTYPEDEF, "%s: %s typedef [%s] on line %u\n",
              __func__, get_token_name(tag), the_type->text(), the_type->orig_line);
      set_flags(the_type, PCF_ANCHOR);
   }
}


static bool cs_top_is_question(const ChunkStack &cs, uint32_t level)
{
   chunk_t* pc = cs.Empty() ? nullptr : cs.Top()->m_pc;
   return(is_type_and_level(pc, CT_QUESTION, level));
}


void combine_labels(void)
{
   LOG_FUNC_ENTRY();

   bool hit_case  = false;
   bool hit_class = false;
   cpd.unc_stage = unc_stage_e::COMBINE_LABELS;

   ChunkStack cs; /* stack to handle nesting inside of OC messages, which reset the scope */
   chunk_t* prev = chunk_get_head();
   chunk_t* cur  = get_next_nc(prev);
   chunk_t* next = get_next_nc(cur);

   /* unlikely that the file will start with a label... */
   while (is_valid(next))
   {  /* \todo better use a switch for next->type */
      assert(is_valid(cur));
      LOG_FMT(LGUY, "%s: %u:%u %s\n", __func__,
            cur->orig_line, cur->orig_col, get_token_name(cur->type));

      if (not_flag(next, PCF_IN_OC_MSG) && /* filter OC case of [self class] msg send */
          is_type(next, CT_CLASS, CT_OC_CLASS, CT_TEMPLATE))
      {
         hit_class = true;
      }
      if (is_semicolon(next) || is_type(next, CT_BRACE_OPEN))
      {
         hit_class = false;
      }
      if (is_type_and_ptype(prev, CT_SQUARE_OPEN, CT_OC_MSG))
      {
         cs.Push_Back(prev);
      }
      else if (is_type_and_ptype(next, CT_SQUARE_CLOSE, CT_OC_MSG))
      {
         /* pop until we hit '[' */
         while (!cs.Empty())
         {
            chunk_t* t2 = cs.Top()->m_pc; /*lint !e613 */
            cs.Pop_Back();
            break_if(is_type(t2, CT_SQUARE_OPEN));
         }
      }

      if (is_type(next, CT_QUESTION))
      {
         cs.Push_Back(next);
      }
      else if (is_type(next, CT_CASE))
      {
         if (is_type(cur, CT_GOTO)) { set_type(next, CT_QUALIFIER); } /* handle "goto case x;" */
         else                       { hit_case = true; }
      }
      else if ( is_type(next, CT_COLON                                           ) ||
               (is_type(next, CT_OC_COLON) && cs_top_is_question(cs, next->level)) )
      {
         if (is_type(cur, CT_DEFAULT))
         {
            set_type(cur, CT_CASE);
            hit_case = true;
         }
         if (cs_top_is_question(cs, next->level))
         {
            set_type(next, CT_COND_COLON);
            cs.Pop_Back();
         }
         else if (hit_case)
         {
            hit_case = false;
            set_type(next, CT_CASE_COLON);
            chunk_t* tmp = get_next_ncnl(next);
            if (is_type(tmp, CT_BRACE_OPEN))
            {
               set_ptype(tmp, CT_CASE);
               tmp = get_next_type(tmp, CT_BRACE_CLOSE, (int32_t)tmp->level);
               if (is_valid(tmp))
               {
                  set_ptype(tmp, CT_CASE);
               }
            }
         }
         else
         {
            chunk_t* nextprev = get_prev_ncnl(next);

            if (is_lang(cpd, LANG_PAWN))
            {
               if (is_type(cur, CT_WORD, CT_BRACE_CLOSE) )
               {
                  c_token_t new_type = CT_TAG;

                  chunk_t* tmp = get_next_nc(next);
                  if (is_nl(prev) && is_nl(tmp))
                  {
                     new_type = CT_LABEL;
                     set_type(next, CT_LABEL_COLON);
                  }
                  else
                  {
                     set_type(next, CT_TAG_COLON);
                  }
                  if (is_type(cur, CT_WORD))
                  {
                     set_type(cur, new_type);
                  }
               }
            }
            else if (is_flag(next, PCF_IN_ARRAY_ASSIGN)) { set_type(next, CT_D_ARRAY_COLON); }
            else if (is_flag(next, PCF_IN_FOR         )) { set_type(next, CT_FOR_COLON    ); }
            else if (is_flag(next, PCF_OC_BOXED       )) { set_type(next, CT_OC_DICT_COLON); }
            else if (is_type(cur, CT_WORD))
            {
               chunk_t* tmp = get_next_nc(next, scope_e::PREPROC);
               assert(is_valid(tmp));

               LOG_FMT(LGUY, "%s: %u:%u, tmp=%s\n", __func__, tmp->orig_line,
                       tmp->orig_col, (is_type(tmp, CT_NEWLINE)) ? "<NL>" : tmp->text());
               log_pcf_flags(LGUY, get_flags(tmp));
               if (is_flag(next, PCF_IN_FCN_CALL))
               {
                  /* Must be a macro thingy, assume some sort of label */
                  set_type(next, CT_LABEL_COLON);
               }
               else if ( is_invalid(tmp) ||
                         is_type (tmp, CT_NEWLINE) ||
                        (not_type(tmp, CT_NUMBER, CT_SIZEOF) && not_flag(tmp, (PCF_IN_STRUCT | PCF_IN_CLASS))))
               {
                  /* the CT_SIZEOF isn't great - test 31720 happens to use a sizeof expr,
                   * but this really should be able to handle any constant expr */
                  set_type(cur,  CT_LABEL      );
                  set_type(next, CT_LABEL_COLON);
               }
               else if (is_flag(next, PCF_IN_STRUCT ) ||
                        is_flag(next, PCF_IN_CLASS  ) ||
                        is_flag(next, PCF_IN_TYPEDEF))
               {
                  set_type(next, CT_BIT_COLON);

                  tmp = chunk_get_next(next);
                  while ((tmp = chunk_get_next(tmp)) != nullptr)
                  {
                     if (is_type(tmp, CT_SEMICOLON)) { break; }
                     if (is_type(tmp, CT_COLON    )) { set_type(tmp, CT_BIT_COLON); }
                  }
               }
            }
            else if (is_type (nextprev, CT_FPAREN_CLOSE))        { set_type(next, CT_CLASS_COLON); /* it's a class colon */ }
            else if (is_type (cur, CT_TYPE))                     { set_type(next, CT_BIT_COLON); }
            else if (is_type (cur, CT_ENUM, CT_PRIVATE, CT_QUALIFIER) ||
                     is_ptype(cur, CT_ALIGN))                    { /* ignore it - bit field, align or public/private, etc */ }
            else if (is_type (cur, CT_ANGLE_CLOSE) || hit_class) { /* ignore it - template thingy */ }
            else if (is_ptype(cur, CT_SQL_EXEC))                 { /* ignore it - SQL variable name */ }
            else if (is_ptype(next,CT_ASSERT))                   { /* ignore it - Java assert thing */ }
            else if (next->level > next->brace_level)            { /* ignore it, as it is inside a parenthesis */ }
            else
            {
               chunk_t* tmp = get_next_ncnl(next);
               if (is_type(tmp, CT_BASE, CT_THIS))
               {
                  /* ignore it, as it is a C# base thingy */
               }
               else
               {
                  LOG_FMT(LWARN, "%s line %u unexpected colon in column %u next-parent=%s current-parent=%s level=%u bracelevel=%u\n",
                          cpd.filename, next->orig_line, next->orig_col, get_token_name(next->ptype),
                          get_token_name(cur->ptype), next->level, next->brace_level);
                  cpd.error_count++;
               }
            }
         }
      }
      prev = cur;
      cur  = next;
      next = get_next_nc(cur);
   }
}


static void mark_variable_stack(ChunkStack &cs, log_sev_t sev)
{
   UNUSED(sev);
   LOG_FUNC_ENTRY();

   /* throw out the last word and mark the rest */
   chunk_t* var_name = cs.Pop_Back();
   if (is_valid(var_name) && is_type(var_name->prev, CT_DC_MEMBER))
   {
      cs.Push_Back(var_name);
   }

   if (is_valid(var_name))
   {
      LOG_FMT(LFCNP, "%s: parameter on line %u :",
            __func__, var_name->orig_line);

      uint32_t  word_cnt = 0;
      chunk_t* word_type;
      while ((word_type = cs.Pop_Back()) != nullptr)
      {
         if (is_type(word_type, CT_WORD, CT_TYPE) )
         {
            LOG_FMT(LFCNP, " <%s>", word_type->text());
            set_type(word_type, CT_TYPE);
            set_flags(word_type, PCF_VAR_TYPE);
         }
         word_cnt++;
      }

      if (is_type(var_name, CT_WORD))
      {
         LOG_FMT(LFCNP, " [%s]\n", var_name->text());
         if (word_cnt > 0)
         {
            set_flags(var_name, PCF_VAR_DEF);
         }
         else
         {
            set_type (var_name, CT_TYPE);
            set_flags(var_name, PCF_VAR_TYPE);
         }
      }
   }
}


static void fix_fcn_def_params(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(start));

   LOG_FMT(LFCNP, "%s: %s [%s] on line %u, level %u\n",
         __func__, start->text(), get_token_name(start->type),
         start->orig_line, start->level);

   while (is_valid(start) && !is_paren_open(start))
   {
      start = get_next_ncnl(start);
   }

   return_if(is_invalid(start));

   /* ensure start chunk holds a single '(' character */
   uint32_t len        = start->len();
   char   first_char = start->str[0]; /*lint !e734 */
   return_if((len != 1) || (first_char != '(' ));

   ChunkStack cs;
   uint32_t     level = start->level + 1;
   chunk_t    *pc   = start;

   while ((pc = get_next_ncnl(pc)) != nullptr)
   {
      if (((start->len()  == 1   ) &&
           (start->str[0] == ')')) ||
           (pc->level < level))
      {
         LOG_FMT(LFCNP, "%s: bailed on %s on line %u\n",
                 __func__, pc->text(), pc->orig_line);
         break;
      }

      LOG_FMT(LFCNP, "%s: %s %s on line %u, level %u\n",
              __func__, (pc->level > level) ? "skipping" : "looking at",
              pc->text(), pc->orig_line, pc->level);

      continue_if(pc->level > level);

      if (is_star (pc) ||
          is_msref(pc) )
      {
         set_type(pc, CT_PTR_TYPE);
         cs.Push_Back(pc);
      }
      else if (is_type(pc, CT_AMP) ||
               (is_lang(cpd, LANG_CPP) && is_str(pc, "&&")))
      {
         set_type(pc, CT_BYREF);
         cs.Push_Back(pc);
      }
      else if (is_type(pc, CT_TYPE_WRAP       )) { cs.Push_Back(pc); }
      else if (is_type(pc, CT_WORD,  CT_TYPE  )) { cs.Push_Back(pc); }
      else if (is_type(pc, CT_COMMA, CT_ASSIGN))
      {
         mark_variable_stack(cs, LFCNP);
         if (is_type(pc, CT_ASSIGN))
         {
            /* Mark assignment for default param spacing */
            set_ptype(pc, CT_FUNC_PROTO);
         }
      }
   }
   mark_variable_stack(cs, LFCNP);
}


static chunk_t* skip_to_next_statement(chunk_t* pc)
{
   while ( not_type (pc, CT_BRACE_OPEN, CT_BRACE_CLOSE) &&
          !is_semicolon(pc                            ) )
   {
      pc = get_next_ncnl(pc);
   }
   return(pc);
}


static chunk_t* fix_var_def(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(start), start);

   chunk_t* pc = start;
   LOG_FMT(LFVD, "%s: start[%u:%u]",
         __func__, pc->orig_line, pc->orig_col);

   ChunkStack cs;

   /* Scan for words and types and stars oh my! */
   while (is_type(pc, 6, CT_TYPE, CT_MEMBER,   CT_QUALIFIER,
                         CT_WORD, CT_TYPENAME, CT_DC_MEMBER) ||
          is_ptr_operator(pc) )
   {
      LOG_FMT(LFVD, " %s[%s]", pc->text(), get_token_name(pc->type));
      cs.Push_Back(pc);
      pc = get_next_ncnl(pc);

      /* Skip templates and attributes */
      pc = skip_template_next(pc);
      pc = skip_attribute_next(pc);
      if (is_lang(cpd, LANG_JAVA))
      {
         pc = skip_tsquare_next(pc);
      }
   }
   chunk_t* end = pc;

   LOG_FMT(LFVD, " end=[%s]\n",
         (is_valid(end)) ? get_token_name(end->type) : "nullptr");
   retval_if(is_invalid(end), end);

   /* Function defs are handled elsewhere */
   if ((cs.Len()  <= 1) ||
        is_type(end, 5, CT_OPERATOR, CT_FUNC_DEF, CT_FUNC_PROTO,
                        CT_FUNC_CLASS_PROTO, CT_FUNC_CLASS_DEF) )
   {
      return(skip_to_next_statement(end));
   }

   /* ref_idx points to the alignable part of the var def */
   int32_t ref_idx = (int32_t)cs.Len() - 1;
   chunk_t* tmp_pc;

   assert(ptr_is_valid(cs.Get(0)));
   /* Check for the '::' stuff: "char *Engine::name" */
   if ((cs.Len() >= 3) &&
       ((cs.Get(cs.Len() - 2)->m_pc->type == CT_MEMBER   ) || /*lint !e613 */
        (cs.Get(cs.Len() - 2)->m_pc->type == CT_DC_MEMBER)))  /*lint !e613 */
   {
      int32_t idx = (int32_t)cs.Len() - 2;
      while (idx > 0)
      {
         tmp_pc = cs.Get((uint32_t)idx)->m_pc; /*lint !e613 */
         break_if(not_type(tmp_pc, CT_DC_MEMBER, CT_MEMBER));

         idx--;
         tmp_pc = cs.Get((uint32_t)idx)->m_pc; /*lint !e613 */
         break_if(not_type(tmp_pc, CT_WORD, CT_TYPE));

         make_type(tmp_pc);
         idx--;
      }
      ref_idx = idx + 1;
   }
   tmp_pc = cs.Get((uint32_t)ref_idx)->m_pc;  /*lint !e613 */
   LOG_FMT(LFVD, " ref_idx(%d) => %s\n", ref_idx, tmp_pc->text());

   /* No type part found! */
   retval_if(ref_idx <= 0, skip_to_next_statement(end));

   LOG_FMT(LFVD2, "%s:%u TYPE : ", __func__, start->orig_line);
   for (uint32_t idxForCs = 0; idxForCs < cs.Len() - 1; idxForCs++)
   {
      tmp_pc = cs.Get(idxForCs)->m_pc; /*lint !e613 */
      make_type(tmp_pc);
      set_flags(tmp_pc, PCF_VAR_TYPE);
      LOG_FMT(LFVD2, " %s[%s]", tmp_pc->text(), get_token_name(tmp_pc->type));
   }
   LOG_FMT(LFVD2, "\n");

   /* OK we have two or more items, mark types up to the end. */
   mark_variable_definition(cs.Get(cs.Len() - 1)->m_pc); /*lint !e613 */
   return (is_type(end, CT_COMMA)) ?
         get_next_ncnl   (end) :
         skip_to_next_statement(end);
}


static chunk_t* skip_expression(chunk_t* start)
{
   chunk_t* pc = start;
   while (is_valid(pc) && (pc->level >= start->level))
   {
      if (is_level(pc, start->level) &&
          is_type (pc, CT_COMMA, CT_SEMICOLON, CT_VSEMICOLON))
      {
         return(pc);
      }
      pc = get_next_ncnl(pc);
   }
   return(pc);
}


bool go_on(chunk_t* pc, chunk_t* start)
{
   if ((are_invalid(pc, start)) || (pc->level != start->level))
   {
      return(false);
   }
   if (is_flag(pc, PCF_IN_FOR))
   {
      return(!is_semicolon(pc) && not_type (pc, CT_COLON));
   }
   else
   {
      return(!is_semicolon(pc));
   }
}


static chunk_t* mark_variable_definition(chunk_t* start)
{
   LOG_FUNC_ENTRY();
   retval_if(is_invalid(start), start);

   chunk_t* pc   = start;
   uint32_t  flags = PCF_VAR_1ST_DEF;

   LOG_FMT(LVARDEF, "%s: line %u, col %u '%s' type %s\n", __func__,
         pc->orig_line, pc->orig_col, pc->text(), get_token_name(pc->type));

   pc = start;
   while (go_on(pc, start))
   {
      if (is_type(pc, CT_WORD, CT_FUNC_CTOR_VAR))
      {
         uint64_t flg = get_flags(pc);
         if (not_flag(pc, PCF_IN_ENUM))
         {
            set_flags(pc, flags);
         }
         flags &= ~PCF_VAR_1ST;

         LOG_FMT(LVARDEF, "%s:%u marked '%s'[%s] in col %u flags: %#" PRIx64 " -> %#" PRIx64 "\n",
                 __func__, pc->orig_line, pc->text(),
                 get_token_name(pc->type), pc->orig_col, flg, get_flags(pc));
      }
      else if (is_star (pc) ||
               is_msref(pc) )
      {
         set_type(pc, CT_PTR_TYPE);
      }
      else if (is_addr(pc))
      {
         set_type(pc, CT_BYREF);
      }
      else if (is_type(pc, CT_SQUARE_OPEN, CT_ASSIGN))
      {
         pc = skip_expression(pc);
         continue;
      }
      pc = get_next_ncnl(pc);
   }
   return(pc);
}


static bool can_be_full_param(chunk_t* start, chunk_t* end)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LFPARAM, "%s:", __func__);

   int32_t     word_cnt   = 0;
   uint32_t  type_count = 0;
   chunk_t* pc;
   for (pc = start; pc != end; pc = get_next_ncnl(pc, scope_e::PREPROC))
   {
      assert(is_valid(pc));
      LOG_FMT(LFPARAM, " [%s]", pc->text());

      switch(pc->type)
      {
         case(CT_QUALIFIER):  /* fallthrough */
         case(CT_STRUCT):     /* fallthrough */
         case(CT_ENUM):       /* fallthrough */
         case(CT_TYPENAME):   /* fallthrough */
         case(CT_UNION):
            LOG_FMT(LFPARAM, " <== %s! (yes)\n", get_token_name(pc->type));
            return(true);
         break;

         case(CT_WORD):       /* fallthrough */
         case(CT_TYPE):
            word_cnt++;
            if (is_type(pc, CT_TYPE)) { type_count++; }
         break;

         case(CT_MEMBER):     /* fallthrough */
         case(CT_DC_MEMBER):
            if (word_cnt > 0) { word_cnt--; }
         break;

         case(CT_ASSIGN):
            goto end_of_loop;  /* chunk is OK (default values) */
         break;

         case(CT_ANGLE_OPEN):
            LOG_FMT(LFPARAM, " <== template\n");
            return(true);
         break;

         case(CT_ELLIPSIS):
            LOG_FMT(LFPARAM, " <== ellipses\n");
            return(true);
         break;

         case(CT_PAREN_OPEN):
            if (word_cnt == 0)
            {
               /* Check for old-school func proto param '(type)' */
               chunk_t* tmp1 = chunk_skip_to_match(pc,   scope_e::PREPROC);
               chunk_t* tmp2 = get_next_ncnl(tmp1, scope_e::PREPROC);

               if (is_type(tmp2, CT_COMMA) ||
                   is_paren_close(tmp2))
               {
                  do
                  {
                     pc = get_next_ncnl(pc, scope_e::PREPROC);
                     assert(is_valid(pc));
                     LOG_FMT(LFPARAM, " [%s]", pc->text());
                  } while (pc != tmp1);

                  /* reset some variables to allow [] after parenthesis */
                  word_cnt   = 1;
                  type_count = 1;
               }
               else
               {
                  LOG_FMT(LFPARAM, " <== [%s] not function type!\n", get_token_name(pc->type));
                  return(false);
               }
            }
            else if (((word_cnt == 1) ||
                     (static_cast<uint32_t>(word_cnt) == type_count)))
            {
               /* Check for func proto param 'void (*name)' or 'void (*name)(params)' */
               chunk_t* tmp1 = get_next_ncnl(pc,   scope_e::PREPROC);
               chunk_t* tmp2 = get_next_ncnl(tmp1, scope_e::PREPROC);
               chunk_t* tmp3 = get_next_ncnl(tmp2, scope_e::PREPROC);

               if (!is_str  (tmp3, ")"    ) ||
                   !is_str  (tmp1, "*"    ) ||
                    not_type(tmp2, CT_WORD) )
               {
                  LOG_FMT(LFPARAM, " <== [%s] not fcn type!\n", get_token_name(pc->type));
                  return(false);
               }
               LOG_FMT(LFPARAM, " <skip fcn type>");
               tmp1 = get_next_ncnl(tmp3, scope_e::PREPROC);
               if (is_str(tmp1, "("))
               {
                  tmp3 = chunk_skip_to_match(tmp1, scope_e::PREPROC);
               }
               pc = tmp3;

               /* reset some variables to allow [] after parens */
               word_cnt   = 1;
               type_count = 1;
            }
         break;

         case(CT_TSQUARE):
            /* ignore it */
         break;

         case(CT_SQUARE_OPEN): /* skip over any array stuff */
            if (word_cnt == 1)
            {
               pc = chunk_skip_to_match(pc, scope_e::PREPROC);
            }
            else if (word_cnt == 2)
            {
               /* seems to be something like: bool foo[FOO_MAX] */
               pc = chunk_skip_to_match(pc, scope_e::PREPROC);
            }
         break;

         default:
            if ((pc != start) && is_ptr_operator(pc))
            {
               /* chunk is OK */
            }
            else if ((word_cnt == 1) &&
                  is_lang(cpd, LANG_CPP) &&
                is_str(pc, "&&"))
            {
               /* ignore possible 'move' operator */
            }
            else
            {
               /* unexpected type found */
               LOG_FMT(LFPARAM, " <== [%s] no way! tc=%u wc=%d\n",
                       get_token_name(pc->type), type_count, word_cnt);
               return(false);
            }
         break;
      }
   }
end_of_loop:

   chunk_t* last = get_prev_ncnl(pc);
   if (is_ptr_operator(last))
   {
      if (is_valid(pc))
      {
         LOG_FMT(LFPARAM, " <== [%s] sure!\n", get_token_name(pc->type));
      }
      return(true);
   }

   bool ret = ( (word_cnt >= 2) ||
               ((word_cnt == 1) && (type_count == 1)));

   if (is_valid(pc))
   {
      LOG_FMT(LFPARAM, " <== [%s] %s!\n",
              get_token_name(pc->type), ret ? "Yup" : "Unlikely");
   }
   return(ret);
}


void set_type_and_log(chunk_t* pc, const c_token_t type, const uint32_t num)
{
   set_type(pc, type);
   LOG_FMT(LFCN, "  %u) Marked [%s] as %s on line %u col %u\n",
           num, pc->text(), get_token_name(type), pc->orig_line, pc->orig_col);
}


static void mark_function(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));
   chunk_t* prev = get_prev_ncnlnp(pc);
   chunk_t* next = get_next_ncnlnp(pc);
   return_if(is_invalid(next));

   /* Find out what is before the operator */
   chunk_t* tmp;
   if (is_ptype(pc, CT_OPERATOR))
   {
      const chunk_t* pc_op = get_prev_type(pc, CT_OPERATOR, (int32_t)pc->level);
      if (is_flag(pc_op, PCF_EXPR_START))
      {
         set_type(pc, CT_FUNC_CALL);
      }

      if (is_lang(cpd, LANG_CPP))
      {
         tmp = pc;
         while ((tmp = get_prev_ncnl(tmp)) != nullptr)
         {
            switch(tmp->type)
            {
               case(CT_BRACE_CLOSE):    /* fallthrough */
               case(CT_SEMICOLON  ):    /* do nothing */            goto exit_loop;
               case(CT_PAREN_OPEN ):    /* fallthrough */
               case(CT_SPAREN_OPEN):    /* fallthrough */
               case(CT_TPAREN_OPEN):    /* fallthrough */
               case(CT_FPAREN_OPEN):    /* fallthrough */
               case(CT_ASSIGN     ):    set_type(pc, CT_FUNC_CALL); goto exit_loop;
               case(CT_TEMPLATE   ):    set_type(pc, CT_FUNC_DEF ); goto exit_loop;
               case(CT_BRACE_OPEN ):
               {
                  switch(tmp->ptype)
                  {
                     case(CT_FUNC_DEF): set_type(pc, CT_FUNC_CALL); goto exit_loop;
                     case(CT_CLASS   ): /* fallthrough */
                     case(CT_STRUCT  ): set_type(pc, CT_FUNC_DEF ); goto exit_loop;
                     default:   /* ignore unexpected parent type */ goto exit_loop;
                  }
                  break;
               }
               default:                 /* go on with loop */       break;
            }
         }

exit_loop:
         if (is_valid(tmp) && not_type(pc, CT_FUNC_CALL))
         {
            while ((tmp = get_next_ncnl(tmp)) != pc)
            {
               make_type(tmp); /* Mark the return type */
            }
         }
      }
   }

   if (is_ptr_operator(next))
   {
      next = get_next_ncnlnp(next);
      return_if(is_invalid(next));
   }

   LOG_FMT(LFCN, "%s: orig_line=%u] %s[%s] - parent=%s level=%u/%u, "
         "next=%s[%s] - level=%u\n",
           __func__, pc->orig_line, pc->text(),
           get_token_name(pc->type), get_token_name(pc->ptype),
           pc->level, pc->brace_level,
           next->text(), get_token_name(next->type), next->level);

   if (is_flag(pc, PCF_IN_CONST_ARGS))
   {
      set_type_and_log(pc, CT_FUNC_CTOR_VAR, 1);
      next = skip_template_next(next);
      return_if(is_invalid(next));

      flag_parens(next, 0, CT_FPAREN_OPEN, pc->type, true);
      return;
   }

   /* Skip over any template and attribute madness */
   next = skip_template_next (next); return_if(is_invalid(next));
   next = skip_attribute_next(next); return_if(is_invalid(next));

   /* Find the open and close parenthesis */
   chunk_t* popen  = get_next_str(pc,    "(", 1, (int32_t)pc->level);
   chunk_t* pclose = get_next_str(popen, ")", 1, (int32_t)pc->level);

   if (are_invalid(popen, pclose))
   {
      LOG_FMT(LFCN, "No parens found for [%s] on line %u col %u\n",
              pc->text(), pc->orig_line, pc->orig_col);
      return;
   }

   /* This part detects either chained function calls or a function ptr definition.
    * MYTYPE (*func)(void);
    * mWriter( "class Clst_"c )( somestr.getText() )( " : Cluster {"c ).newline;
    *
    * For it to be a function variable def, there must be a '*' followed by a
    * single word.
    *
    * Otherwise, it must be chained function calls. */
   tmp = get_next_ncnl(pclose);
   if (is_str(tmp, "("))
   {
      chunk_t* tmp2;
      chunk_t* tmp3;

      /* skip over any leading class/namespace in: "T(F::*A)();" */
      chunk_t* tmp1 = get_next_ncnl(next);
      while (is_valid(tmp1))
      {
         tmp2 = get_next_ncnl(tmp1);
         break_if (!is_word(tmp1              ) ||
                   !is_type(tmp2, CT_DC_MEMBER) );
         tmp1 = get_next_ncnl(tmp2);
      }

      tmp2 = get_next_ncnl(tmp1);
      if (is_str(tmp2, ")"))
      {
         tmp3 = tmp2;
         tmp2 = nullptr;
      }
      else
      {
         tmp3 = get_next_ncnl(tmp2);
      }

      if ( is_str(tmp3, ")") &&
          (is_star (tmp1) ||
           is_msref(tmp1) ||
           (is_lang(cpd, LANG_OC) && is_type(tmp1, CT_CARET))) &&
           is_invalid_or_type(tmp2, CT_WORD) )
      {
         if (is_valid(tmp2))
         {
            LOG_FMT(LFCN, "%s: [%u/%u] function variable [%s], changing [%s] into a type\n",
                    __func__, pc->orig_line, pc->orig_col, tmp2->text(), pc->text());
            set_type(tmp2, CT_FUNC_VAR);
            flag_parens(popen, 0, CT_PAREN_OPEN, CT_FUNC_VAR, false);
            LOG_FMT(LFCN, "%s: paren open @ %u:%u\n",
                    __func__, popen->orig_line, popen->orig_col);
         }
         else
         {
            LOG_FMT(LFCN, "%s: [%u/%u] function type, changing [%s] into a type\n",
                    __func__, pc->orig_line, pc->orig_col, pc->text());
            if (is_valid(tmp2))
            {
               set_type(tmp2, CT_FUNC_TYPE);
            }
            flag_parens(popen, 0, CT_PAREN_OPEN, CT_FUNC_TYPE, false);
         }

         set_type(pc,   CT_TYPE    );
         set_type(tmp1, CT_PTR_TYPE);
         clr_flags(pc,   PCF_VAR_1ST_DEF);
         set_flags(tmp2, PCF_VAR_1ST_DEF);
         flag_parens(tmp, 0, CT_FPAREN_OPEN, CT_FUNC_PROTO, false);
         fix_fcn_def_params(tmp);
         return;
      }
      LOG_FMT(LFCN, "%s: chained function calls? [%u.%u] [%s]\n",
              __func__, pc->orig_line, pc->orig_col, pc->text());
   }

   /* Assume it is a function call if not already labeled */
   if (is_type(pc, CT_FUNCTION))
   {
      LOG_FMT(LFCN, "%s: examine [%u.%u] [%s], type %s\n", __func__,
            pc->orig_line, pc->orig_col, pc->text(), get_token_name(pc->type));
      /* look for an assignment */
      chunk_t* temp = get_next_type(pc, CT_ASSIGN, (int32_t)pc->level);
      if (is_valid(temp))
      {
         LOG_FMT(LFCN, "%s: assignment found [%u.%u] [%s]\n",
                 __func__, temp->orig_line, temp->orig_col, temp->text());
         set_type(pc, CT_FUNC_CALL);
      }
      else
      {
         set_type(pc, is_ptype(pc, CT_OPERATOR) ? CT_FUNC_DEF : CT_FUNC_CALL);
      }
   }

   /* Check for C++ function def */
   if (is_type(pc,   CT_FUNC_CLASS_DEF   ) ||
       is_type(prev, CT_DC_MEMBER, CT_INV) )
   {
      const chunk_t* destr = nullptr;
      assert(is_valid(prev));
      if (is_type(prev, CT_INV))
      {
         /* TODO: do we care that this is the destructor? */
         set_type          (prev,                  CT_DESTRUCTOR);
         set_type_and_ptype(pc, CT_FUNC_CLASS_DEF, CT_DESTRUCTOR);

         destr = prev;
         prev  = get_prev_ncnlnp(prev);
      }

      if (is_type(prev, CT_DC_MEMBER))
      {
         prev = get_prev_ncnlnp(prev);
         // LOG_FMT(LSYS, "%s: prev1 = %s (%s)\n", __func__,
         //         get_token_name(prev->type), prev->text());
         prev = skip_template_prev(prev);
         prev = skip_attribute_prev(prev);
         // LOG_FMT(LSYS, "%s: prev2 = %s [%d](%s) pc = %s [%d](%s)\n", __func__,
         //         get_token_name(prev->type), prev->len, prev->text(),
         //         get_token_name(pc->type), pc->len, pc->text());
         if (is_type(prev, CT_WORD, CT_TYPE))
         {
            if (pc->str.equals(prev->str))
            {
               set_type(pc, CT_FUNC_CLASS_DEF);
               LOG_FMT(LFCN, "(%d) %u:%u - FOUND %sSTRUCTOR for %s[%s]\n",
                       __LINE__, prev->orig_line, prev->orig_col,
                       (is_valid(destr)) ? "DE" : "CON",
                       prev->text(), get_token_name(prev->type));

               mark_cpp_constructor(pc);
               return;
            }
            else
            {
               /* Point to the item previous to the class name */
               prev = get_prev_ncnlnp(prev);
            }
         }
      }
   }

   /* Determine if this is a function call or a function def/proto
    * We check for level==1 to allow the case that a function prototype is
    * wrapped in a macro: "MACRO(void foo(void));" */
   if ( is_type (pc, CT_FUNC_CALL) &&
       (is_level(pc, pc->brace_level) || is_level(pc, 1)) &&
        not_flag(pc, PCF_IN_ARRAY_ASSIGN))
   {
      LOG_FMT(LFCN, "  Checking func call: prev=%s", (is_invalid(prev)) ?
            "<null>" : get_token_name(prev->type));
#ifdef DEBUG
      LOG_FMT(LFCN, "\n");
#endif

      /* REVISIT:
       * a function def can only occur at brace level, but not inside an
       * assignment, structure, enum, or union.
       * The close paren must be followed by an open brace, with an optional
       * qualifier (const) in between.
       * There can be all sorts of template stuff and/or '[]' in the type.
       * This hack mostly checks that.
       *
       * Examples:
       * foo->bar(maid);                   -- fcn call
       * FOO * bar();                      -- fcn proto or class variable
       * FOO foo();                        -- fcn proto or class variable
       * FOO foo(1);                       -- class variable
       * a = FOO * bar();                  -- fcn call
       * a.y = foo() * bar();              -- fcn call
       * static const char * const fizz(); -- fcn def */
      bool isa_def  = false;
      bool hit_star = false;
      while (is_valid(prev))
      {
         if (is_preproc(prev))
         {
            prev = get_prev_ncnlnp(prev);
            continue;
         }

         /* Some code slips an attribute between the type and function */
         if (is_type_and_ptype(prev, CT_FPAREN_CLOSE, CT_ATTRIBUTE))
         {
            prev = skip_attribute_prev(prev);
            continue;
         }

         /* skip const(TYPE) */
         if (is_type_and_ptype(prev, CT_PAREN_CLOSE, CT_D_CAST))
         {
            LOG_FMT(LFCN, " --> For sure a prototype or definition\n");
            isa_def = true;
            break;
         }

         /* Skip the word/type before the '.' or '::' */
         if (is_type(prev, CT_DC_MEMBER, CT_MEMBER))
         {
            prev = get_prev_ncnlnp(prev);
            if (not_type(prev, CT_WORD, CT_TYPE, CT_THIS))
            {
               LOG_FMT(LFCN, " --? Skipped MEMBER and landed on %s\n",
                       (is_invalid(prev)) ? "<null>" : get_token_name(prev->type));
               set_type(pc, CT_FUNC_CALL);
               isa_def = false;
               break;
            }
            LOG_FMT(LFCN, " <skip %s>", prev->text());
            prev = get_prev_ncnlnp(prev);
            continue;
         }

         /* If we are on a TYPE or WORD, then we must be on a proto or def */
         if (is_type(prev, CT_TYPE, CT_WORD))
         {
            if (hit_star == false)
            {
               LOG_FMT(LFCN, " --> For sure a prototype or definition\n");
               isa_def = true;
               break;
            }
            LOG_FMT(LFCN, " --> maybe a prototype or definition\n");
            isa_def = true;
         }

         if (is_ptr_operator(prev))
         {
            hit_star = true;
         }

         if (not_type(prev, 6, CT_OPERATOR,  CT_TSQUARE,     CT_WORD,
                               CT_QUALIFIER, CT_ANGLE_CLOSE, CT_TYPE) &&
             !is_ptr_operator(prev))
         {
            LOG_FMT(LFCN, " --> Stopping on %s [%s]\n",
                    prev->text(), get_token_name(prev->type));
            /* certain tokens are unlikely to precede a prototype or definition */
            if (is_type(prev, 7, CT_ARITH,  CT_ASSIGN, CT_STRING_MULTI,
                      CT_STRING, CT_NUMBER, CT_COMMA,  CT_NUMBER_FP))
            {
               isa_def = false;
            }
            break;
         }

         /* Skip over template and attribute stuff */
         prev = (is_type(prev, CT_ANGLE_CLOSE)) ?
             skip_template_prev   (prev) :
             get_prev_ncnlnp(prev);
      }

      if ((isa_def == true) &&
          ((is_paren_close(prev) && not_ptype(prev, CT_D_CAST) ) ||
           is_type  (prev, CT_ASSIGN, CT_RETURN) ))
      {
         LOG_FMT(LFCN, " -- overriding DEF due to %s [%s]\n",
                 prev->text(), get_token_name(prev->type));
         isa_def = false;
      }
      if (isa_def)
      {
         set_type(pc, CT_FUNC_DEF);
         LOG_FMT(LFCN, "%s: '%s' is FCN_DEF:", __func__, pc->text());
         if (is_invalid(prev))
         {
            prev = chunk_get_head();
         }
         for (tmp = prev; tmp != pc; tmp = get_next_ncnl(tmp))
         {
            LOG_FMT(LFCN, " %s[%s]", tmp->text(), get_token_name(tmp->type));
            make_type(tmp);
         }
         LOG_FMT(LFCN, "\n");
      }
   }

   if (not_type(pc, CT_FUNC_DEF))
   {
      LOG_FMT(LFCN, "  Detected %s '%s' on line %u col %u\n",
            get_token_name(pc->type), pc->text(), pc->orig_line, pc->orig_col);

      tmp = flag_parens(next, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_FUNC_CALL, false);
      if(is_type_and_not_ptype(tmp, CT_BRACE_OPEN, CT_DOUBLE_BRACE))
      {
         set_paren_parent(tmp, pc->type);
      }
      return;
   }

   /* We have a function definition or prototype
    * Look for a semicolon or a brace open after the close parenthesis to figure
    * out whether this is a prototype or definition */
   /* See if this is a prototype or implementation */
   /* FIXME: this doesn't take the old K&R parameter definitions into account */
   /* Scan tokens until we hit a brace open (def) or semicolon (prototype) */
   chunk_t* semi = nullptr;
   tmp = pclose;
   while ((tmp = get_next_ncnl(tmp)) != nullptr)
   {
      /* Only care about brace or semicolon on the same level */
      if (tmp->level < pc->level)
      {
         /* No semicolon - guess that it is a prototype */
         set_type(pc, CT_FUNC_PROTO);
         break;
      }
      else if (is_level(tmp, pc->level))
      {
         /* its a function def for sure */
         break_if(is_type(tmp, CT_BRACE_OPEN));

         if (is_semicolon(tmp))
         {
            /* Set the parent for the semicolon for later */
            semi = tmp;
            set_type(pc, CT_FUNC_PROTO);
            LOG_FMT(LFCN, "  2) Marked [%s] as FUNC_PROTO on line %u col %u\n",
                    pc->text(), pc->orig_line, pc->orig_col);
            break;
         }

         else if (is_type(pc, CT_COMMA))
         {
            set_type_and_log(pc, CT_FUNC_CTOR_VAR, 2);
            break;
         }
      }
   }

   /* C++ syntax is wacky. We need to check to see if a prototype is really a
    * variable definition with parameters passed into the constructor.
    * Unfortunately, the only mostly reliable way to do so is to guess that
    * it is a constructor variable if inside a function body and scan the
    * 'parameter list' for items that are not allowed in a prototype.
    * We search backwards and checking the parent of the containing open braces.
    * If the parent is a class or namespace, then it probably is a prototype */
   if (is_lang  (cpd, LANG_CPP     ) &&
       is_type  (pc,  CT_FUNC_PROTO) &&
       not_ptype(pc,  CT_OPERATOR  ) )
   {
      LOG_FMT(LFPARAM, "%s :: checking '%s' for constructor variable %s %s\n",
              __func__, pc->text(),
              get_token_name(popen->type ),
              get_token_name(pclose->type));

      /* Scan the parameters looking for:
       *  - constant strings
       *  - numbers
       *  - non-type fields
       *  - function calls */
      chunk_t* ref = get_next_ncnl(popen);
      chunk_t* tmp2;
      bool    is_param = true;
      tmp = ref;
      while (tmp != pclose)
      {
         tmp2 = get_next_ncnl(tmp);
         if (is_type(tmp, CT_COMMA            ) &&
             (tmp->level == (popen->level + 1)) )
         {
            if (!can_be_full_param(ref, tmp))
            {
               is_param = false;
               break;
            }
            ref = tmp2;
         }
         tmp = tmp2;
      }
      if (is_param && (ref != tmp))
      {
         if (!can_be_full_param(ref, tmp))
         {
            is_param = false;
         }
      }
      if (!is_param)
      {
         set_type_and_log(pc, CT_FUNC_CTOR_VAR, 3);
      }
      else if (pc->brace_level > 0)
      {
         chunk_t* br_open = get_prev_type(pc, CT_BRACE_OPEN, (int32_t)pc->brace_level - 1);

         if (not_ptype(br_open, 2, CT_EXTERN, CT_NAMESPACE))
         {
            /* Do a check to see if the level is right */
            prev = get_prev_ncnl(pc);
            if (!is_str(prev, "*") && !is_str(prev, "&"))
            {
               chunk_t* p_op = get_prev_type(pc, CT_BRACE_OPEN, (int32_t)pc->brace_level - 1);
               if (not_ptype(p_op, 3, CT_CLASS, CT_STRUCT, CT_NAMESPACE))
               {
                  set_type_and_log(pc, CT_FUNC_CTOR_VAR, 4);
               }
            }
         }
      }
   }

   if (is_valid(semi)) { set_ptype(semi, pc->type); }

   flag_parens(popen, PCF_IN_FCN_DEF, CT_FPAREN_OPEN, pc->type, false);

   if (is_type(pc, CT_FUNC_CTOR_VAR))
   {
      set_flags(pc, PCF_VAR_1ST_DEF);
      return;
   }

   if (is_type(next, CT_TSQUARE))
   {
      next = get_next_ncnl(next);
      return_if(is_invalid(next));
   }

   /* Mark parameters and return type */
   fix_fcn_def_params(next);
   mark_function_return_type(pc, get_prev_ncnl(pc), pc->type);

   /* Find the brace pair and set the parent */
   if (is_type(pc, CT_FUNC_DEF))
   {
      tmp = get_next_ncnl(pclose);
      while (not_type(tmp, CT_BRACE_OPEN))
      {
         //LOG_FMT(LSYS, "%s: set parent to FUNC_DEF on line %d: [%s]\n",
         //     __func__, tmp->orig_line, tmp->text());
         set_ptype(tmp, CT_FUNC_DEF);
         if (!is_semicolon(tmp))
         {
            set_flags(tmp, PCF_OLD_FCN_PARAMS);
         }
         tmp = get_next_ncnl(tmp);
      }
      if (is_type(tmp, CT_BRACE_OPEN))
      {
         set_ptype(tmp, CT_FUNC_DEF);
         tmp = chunk_skip_to_match(tmp);
         if (is_valid(tmp))
         {
            set_ptype(tmp, CT_FUNC_DEF);
         }
      }
   }
}


static void mark_cpp_constructor(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   bool    is_destr = false;
   chunk_t* tmp     = get_prev_ncnl(pc);
   assert(is_valid(tmp));
   if (is_type(tmp, CT_INV, CT_DESTRUCTOR))
   {
      set_type (tmp, CT_DESTRUCTOR);
      set_ptype(pc,  CT_DESTRUCTOR);
      is_destr = true;
   }

   LOG_FMT(LFTOR, "(%d) %u:%u FOUND %sSTRUCTOR for %s[%s] prev=%s[%s]",
           __LINE__, pc->orig_line, pc->orig_col, is_destr ? "DE" : "CON",
           pc->text(), get_token_name(pc->type), tmp->text(), get_token_name(tmp->type));

   chunk_t* paren_open = skip_template_next(get_next_ncnl(pc));
   if (!is_str(paren_open, "("))
   {
      assert(is_valid(paren_open));
      LOG_FMT(LWARN, "%s:%u Expected '(', got: [%s]\n", cpd.filename,
              paren_open->orig_line, paren_open->text());
      return;
   }

   /* Mark parameters */
   fix_fcn_def_params(paren_open);
   chunk_t* after = flag_parens(paren_open, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_FUNC_CLASS_PROTO, false);

   assert(is_valid(after));
   LOG_FMT(LFTOR, "[%s]\n", after->text());

   /* Scan until the brace open, mark everything */
   chunk_t* var;
   tmp = paren_open;
   bool hit_colon = false;
   while (not_type(tmp, CT_BRACE_OPEN) && !is_semicolon(tmp))
   {
      set_flags(tmp, PCF_IN_CONST_ARGS);
      tmp = get_next_ncnl(tmp);
      assert(are_valid(paren_open, tmp));
      if (is_str(tmp, ":") && is_level(tmp, paren_open->level))
      {
         set_type(tmp, CT_CONSTR_COLON);
         hit_colon = true;
      }
      if (hit_colon &&
          (is_paren_open(tmp) || is_opening_brace(tmp)) &&
          is_level(tmp, paren_open->level))
      {
         var = skip_template_prev(get_prev_ncnl(tmp));
         assert(is_valid(var));
         if (is_type(var, CT_TYPE, CT_WORD))
         {
            set_type(var, CT_FUNC_CTOR_VAR);
            flag_parens(tmp, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_FUNC_CTOR_VAR, false);
         }
      }
   }
   if (is_valid(tmp))
   {
      if (is_type(tmp, CT_BRACE_OPEN))
      {
         set_paren_parent(paren_open, CT_FUNC_CLASS_DEF);
         set_paren_parent(tmp,        CT_FUNC_CLASS_DEF);
      }
      else
      {
         set_ptype(tmp, CT_FUNC_CLASS_PROTO);
         set_type (pc,  CT_FUNC_CLASS_PROTO);
         LOG_FMT(LFCN, "  2) Marked [%s] as FUNC_CLASS_PROTO on line %u col %u\n",
                 pc->text(), pc->orig_line, pc->orig_col);
      }
   }
}


static void mark_class_ctor(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   chunk_t* pclass = get_next_ncnl(start, scope_e::PREPROC);
   return_if(is_invalid(pclass) || not_type(pclass, CT_TYPE, CT_WORD));

   chunk_t* next = get_next_ncnl(pclass, scope_e::PREPROC);
   while (is_type(next, CT_TYPE, CT_WORD, CT_DC_MEMBER))
   {
      pclass = next;
      next   = get_next_ncnl(next, scope_e::PREPROC);
   }

   chunk_t* pc   = get_next_ncnl(pclass, scope_e::PREPROC);
   uint32_t  level = pclass->brace_level + 1;

   if (is_invalid(pc))
   {
      LOG_FMT(LFTOR, "%s: Called on %s on line %u. Bailed on nullptr\n",
              __func__, pclass->text(), pclass->orig_line);
      return;
   }

   /* Add the class name */
   ChunkStack cs;
   cs.Push_Back(pclass);

   LOG_FMT(LFTOR, "%s: Called on %s on line %u (next='%s')\n",
           __func__, pclass->text(), pclass->orig_line, pc->text());

   /* detect D template class: "class foo(x) { ... }" */
   if (is_valid(next))
   {
      if (is_lang(cpd, LANG_D) && is_type(next, CT_PAREN_OPEN))
      {
         set_ptype(next, CT_TEMPLATE);

         next = get_d_template_types(cs, next);
         if (is_type(next, CT_PAREN_CLOSE))
         {
            set_ptype(next, CT_TEMPLATE);
         }
      }
   }

   /* Find the open brace, abort on semicolon */
   uint32_t flags = 0;
   while (not_type(pc, CT_BRACE_OPEN))
   {
      LOG_FMT(LFTOR, " [%s]", pc->text());

      if (is_str(pc, ":"))
      {
         set_type(pc, CT_CLASS_COLON);
         flags |= PCF_IN_CLASS_BASE;
         LOG_FMT(LFTOR, "%s: class colon on line %u\n",
                 __func__, pc->orig_line);
      }

      if (is_semicolon(pc))
      {
         LOG_FMT(LFTOR, "%s: bailed on semicolon on line %u\n",
                 __func__, pc->orig_line);
         return;
      }
      set_flags(pc, flags);
      pc = get_next_ncnl(pc, scope_e::PREPROC);
   }

   if (is_invalid(pc))
   {
      LOG_FMT(LFTOR, "%s: bailed on nullptr\n", __func__);
      return;
   }

   set_paren_parent(pc, start->type);

   pc = get_next_ncnl(pc, scope_e::PREPROC);
   while (is_valid(pc))
   {
      set_flags(pc, PCF_IN_CLASS);

      if ((pc->brace_level > level) ||
          (pc->level > pc->brace_level) ||
          is_preproc(pc))
      {
         pc = get_next_ncnl(pc);
         continue;
      }

      if (is_type(pc, CT_BRACE_CLOSE) &&
          (pc->brace_level < level  ) )
      {
         LOG_FMT(LFTOR, "%s: %u] Hit brace close\n", __func__, pc->orig_line);
         pc = get_next_ncnl(pc, scope_e::PREPROC);
         if (is_type(pc, CT_SEMICOLON))
         {
            set_ptype(pc, start->type);
         }
         return;
      }

      next = get_next_ncnl(pc, scope_e::PREPROC);
      if (chunkstack_match(cs, pc))
      {
         if (is_valid(next) && (next->len() == 1) && (next->str[0] == '('))
         {
            set_type(pc, CT_FUNC_CLASS_DEF);
            LOG_FMT(LFTOR, "(%d) %u] Marked CTor/DTor %s\n", __LINE__,
                  pc->orig_line, pc->text());
            mark_cpp_constructor(pc);
         }
         else
         {
            make_type(pc);
         }
      }
      pc = next;
   }
}


static void mark_namespace(chunk_t* pns)
{
   LOG_FUNC_ENTRY();

   bool is_using = false;

   chunk_t* pc = get_prev_ncnl(pns);
   if (is_type(pc, CT_USING))
   {
      is_using = true;
      set_ptype(pns, CT_USING);
   }

   chunk_t* br_close;
   pc = get_next_ncnl(pns);
   while (is_valid(pc))
   {
      set_ptype(pc, CT_NAMESPACE);
      if (not_type(pc, CT_BRACE_OPEN))
      {
         if (is_type(pc, CT_SEMICOLON))
         {
            if (is_using)
            {
               set_ptype(pc, CT_USING);
            }
            return;
         }
         pc = get_next_ncnl(pc);
         continue;
      }

      if ((get_uval(UO_indent_namespace_limit) > 0) &&
          ((br_close = chunk_skip_to_match(pc)) != nullptr))
      {
         uint32_t diff = br_close->orig_line - pc->orig_line;

         if (diff > get_uval(UO_indent_namespace_limit))
         {
            set_flags(pc,       PCF_LONG_BLOCK);
            set_flags(br_close, PCF_LONG_BLOCK);
         }
      }
      flag_parens(pc, PCF_IN_NAMESPACE, CT_NONE, CT_NAMESPACE, false);
      return;
   }
}


static chunk_t* skip_align(chunk_t* start)
{
   assert(is_valid(start));

   chunk_t* pc = start;
   if (is_type(pc, CT_ALIGN))
   {
      pc = get_next_ncnl(pc);
      assert(is_valid(pc));
      if (is_type(pc, CT_PAREN_OPEN))
      {
         pc = get_next_type(pc, CT_PAREN_CLOSE, (int32_t)pc->level);
         pc = get_next_ncnl(pc);
         assert(is_valid(pc));
         if (is_type(pc, CT_COLON))
         {
            pc = get_next_ncnl(pc);
         }
      }
   }
   return(pc);
}


static void mark_struct_union_body(chunk_t* start)
{
   LOG_FUNC_ENTRY();

   chunk_t* pc = start;
   while (   is_valid(pc)         &&
            (pc->level >= start->level) &&
          !((pc->level == start->level) && is_type(pc, CT_BRACE_CLOSE)))
   {
      // LOG_FMT(LSYS, "%s: %d:%d %s:%s\n", __func__, pc->orig_line,
      // pc->orig_col, pc->text(), get_token_name(pc->parent_type));
      if (is_type(pc, 3, CT_BRACE_OPEN, CT_BRACE_CLOSE, CT_SEMICOLON))
      {
         pc = get_next_ncnl(pc);
         break_if(is_invalid(pc));
      }
      if (is_type(pc, CT_ALIGN))
      {
         pc = skip_align(pc); // "align(x)" or "align(x):"
         break_if(is_invalid(pc));
      }
      else
      {
         pc = fix_var_def(pc);
         return_if(is_invalid(pc));
      }
   }
}


void mark_comments(void)
{
   LOG_FUNC_ENTRY();

   cpd.unc_stage = unc_stage_e::MARK_COMMENTS;

   bool    prev_nl = true;
   chunk_t* cur    = chunk_get_head();

   while (is_valid(cur))
   {
      chunk_t* next   =  get_next_nvb(cur);
      bool    next_nl = (is_invalid(next) || is_nl(next));

      if (is_cmt(cur))
      {
         if (prev_nl && next_nl) { set_ptype(cur, CT_COMMENT_WHOLE); }
         else if (next_nl)       { set_ptype(cur, CT_COMMENT_END  ); }
         else if (prev_nl)       { set_ptype(cur, CT_COMMENT_START); }
         else                    { set_ptype(cur, CT_COMMENT_EMBED); }
      }

      prev_nl = is_nl(cur);
      cur     = next;
   }
}


static void mark_define_expressions(void)
{
   LOG_FUNC_ENTRY();

   bool    in_define = false;
   bool    first     = true;
   chunk_t* pc       = chunk_get_head();
   chunk_t* prev     = pc;

   while (is_valid(pc))
   {
      if (in_define == false)
      {
         if (is_type(pc, CT_PP_DEFINE, CT_PP_IF, CT_PP_ELSE))
         {
            in_define = true;
            first     = true;
         }
      }
      else
      {
         if (!is_preproc(pc) || is_type(pc, CT_PREPROC))
         {
            in_define = false;
         }
         else
         {
            if ( not_type(pc, CT_MACRO) &&
                 ((first == true) ||
                  is_type(prev, 16, CT_CARET,  CT_CONTINUE,   CT_ARITH, CT_GOTO,
                    CT_SPAREN_OPEN, CT_ASSIGN, CT_SEMICOLON,  CT_RETURN,
                    CT_FPAREN_OPEN, CT_COMMA,  CT_BRACE_OPEN, CT_COMPARE,
                    CT_PAREN_OPEN,  CT_COLON,  CT_VSEMICOLON, CT_QUESTION)) )
            {
               set_flags(pc, PCF_EXPR_START);
               first = false;
            }
         }
      }

      prev = pc;
      pc   = chunk_get_next(pc);
   }
}


static void handle_cpp_template(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   chunk_t* tmp = get_next_ncnl(pc);
   return_if(not_type(tmp, CT_ANGLE_OPEN));

   set_ptype(tmp, CT_TEMPLATE);
   uint32_t level = tmp->level;

   while ((tmp = chunk_get_next(tmp)) != nullptr)
   {
      if (is_type(tmp, CT_CLASS, CT_STRUCT))
      {
         set_type(tmp, CT_TYPE);
      }
      else if(is_type_and_level(tmp, CT_ANGLE_CLOSE, level))
      {
         set_ptype(tmp, CT_TEMPLATE);
         break;
      }
   }
   if (is_valid(tmp))
   {
      tmp = get_next_ncnl(tmp);
      if (is_type(tmp, CT_CLASS, CT_STRUCT))
      {
         set_ptype(tmp, CT_TEMPLATE);

         /* REVISIT: This may be a bit risky - might need to track the { }; */
         tmp = get_next_type(tmp, CT_SEMICOLON, (int32_t)tmp->level);
         set_ptype(tmp, CT_TEMPLATE);
      }
   }
}


static void handle_cpp_lambda(chunk_t* sq_o)
{
   LOG_FUNC_ENTRY();

   chunk_t* sq_c = sq_o; /* assuming '[]' */
   if (is_type(sq_o, CT_SQUARE_OPEN))
   {
      /* make sure there is a ']' */
      sq_c = chunk_skip_to_match(sq_o);
      return_if(is_invalid(sq_c));
   }

   /* Make sure a '(' is next */
   chunk_t* pa_o = get_next_ncnl(sq_c);

   return_if(is_invalid_or_not_type(pa_o, CT_PAREN_OPEN));

   /* and now find the ')' */
   chunk_t* pa_c = chunk_skip_to_match(pa_o);
   return_if(is_invalid(pa_c));

   /* Check if keyword 'mutable' is before '->' */
   chunk_t* br_o = get_next_ncnl(pa_c);
   if (is_str(br_o, "mutable"))
   {
      br_o = get_next_ncnl(br_o);
   }

   /* Make sure a '{' or '->' is next */
   chunk_t* ret = nullptr;
   if (is_str(br_o, "->"))
   {
      ret = br_o;
      /* REVISIT: really should check the stuff we are skipping */
      br_o = get_next_type(br_o, CT_BRACE_OPEN, (int32_t)br_o->level);
   }
   return_if(is_invalid_or_not_type(br_o, CT_BRACE_OPEN));

   /* and now find the '}' */
   chunk_t* br_c = chunk_skip_to_match(br_o);
   return_if(is_invalid(br_c));

   /* This looks like a lambda expression */
   if (is_type(sq_o, CT_TSQUARE))
   {
      /* split into two chunks */
      chunk_t nc = *sq_o;
      set_type(sq_o, CT_SQUARE_OPEN);
      sq_o->str.resize(1);

      /* The original orig_col of CT_SQUARE_CLOSE is stored at orig_col_end
       * of CT_TSQUARE. CT_SQUARE_CLOSE orig_col and orig_col_end values
       * are calculate from orig_col_end of CT_TSQUARE. */
      nc.orig_col        = sq_o->orig_col_end - 1;
      nc.column          = static_cast<int32_t>(nc.orig_col);
      nc.orig_col_end    = sq_o->orig_col_end;
      sq_o->orig_col_end = sq_o->orig_col + 1;

      nc.type = CT_SQUARE_CLOSE;
      nc.str.pop_front();
      sq_c = chunk_add_after(&nc, sq_o);
   }
   set_ptype         (sq_o,                  CT_CPP_LAMBDA);
   set_ptype         (sq_c,                  CT_CPP_LAMBDA);
   set_ptype         (br_o,                  CT_CPP_LAMBDA);
   set_ptype         (br_c,                  CT_CPP_LAMBDA);
   set_type_and_ptype(pa_o, CT_FPAREN_OPEN,  CT_CPP_LAMBDA);
   set_type_and_ptype(pa_c, CT_FPAREN_CLOSE, CT_CPP_LAMBDA);

   if (is_valid(ret))
   {
      set_type(ret, CT_CPP_LAMBDA_RET);
      ret = get_next_ncnl(ret);
      while (ret != br_o)
      {
         make_type(ret);
         ret = get_next_ncnl(ret);
      }
   }

   fix_fcn_def_params(pa_o);
}


static chunk_t* get_d_template_types(ChunkStack &cs, chunk_t* open_paren)
{
   LOG_FUNC_ENTRY();
   chunk_t* tmp       = open_paren;
   bool    maybe_type = true;

   while (((tmp = get_next_ncnl(tmp)) != nullptr) &&
          (tmp->level > open_paren->level))
   {
      if (is_type(tmp, CT_TYPE, CT_WORD))
      {
         if (maybe_type)
         {
            make_type(tmp);
            cs.Push_Back(tmp);
         }
         maybe_type = false;
      }
      else if (is_type(tmp, CT_COMMA))
      {
         maybe_type = true;
      }
   }
   return(tmp);
}


static bool chunkstack_match(const ChunkStack &cs, chunk_t* pc)
{
   for (uint32_t idx = 0; idx < cs.Len(); idx++)
   {
      const chunk_t* tmp = cs.GetChunk(idx);
      assert(is_valid(tmp));
      retval_if(pc->str.equals(tmp->str), true);
   }
   return(false);
}


static void handle_d_template(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   chunk_t* name = get_next_ncnl(pc);
   chunk_t* po   = get_next_ncnl(name);

   if(is_invalid_or_not_type(name, CT_WORD))
   {
      LOG_FMT(LERR, "%s: expected a NAME \n", __func__);
      return;
   }

   if(is_invalid_or_not_type(po, CT_PAREN_OPEN))
   {
      LOG_FMT(LERR, "%s: expected a '(' \n", __func__);
      return;
   }

   set_type_and_ptype(name, CT_TYPE, CT_TEMPLATE);
   set_ptype         (po,            CT_TEMPLATE);

   ChunkStack cs;
   chunk_t    *tmp = get_d_template_types(cs, po);

   if(is_invalid_or_not_type(tmp, CT_PAREN_CLOSE))
   {
      LOG_FMT(LERR, "%s: expected a ')' \n", __func__);
      return;
   }
   set_ptype(tmp, CT_TEMPLATE);

   tmp = get_next_ncnl(tmp);
   assert(is_valid(tmp));
   if (not_type(tmp, CT_BRACE_OPEN))
   {
      LOG_FMT(LERR, "%s: expected a '{' \n", __func__);
      return;
   }
   set_ptype(tmp, CT_TEMPLATE);
   po = tmp;

   tmp = po;
   while (((tmp = get_next_ncnl(tmp)) != nullptr) &&
          (tmp->level > po->level))
   {
      if (is_type(tmp, CT_WORD) &&
          chunkstack_match(cs, tmp))
      {
         set_type(tmp, CT_TYPE);
      }
   }
   assert(is_valid(tmp));
   if (not_type(tmp, CT_BRACE_CLOSE))
   {
      LOG_FMT(LERR, "%s: expected a '}' \n", __func__);
   }
   set_ptype(tmp, CT_TEMPLATE);
}


static void mark_template_func(chunk_t* pc, chunk_t* pc_next)
{
   LOG_FUNC_ENTRY();

   /* We know angle_close must be there... */
   chunk_t* angle_close = get_next_type(pc_next, CT_ANGLE_CLOSE, (int32_t)pc->level);
   chunk_t* after       = get_next_ncnl(angle_close);
   if (is_valid(after))
   {
      if (is_str(after, "("))
      {
         assert(is_valid(angle_close));
         if (is_flag(angle_close, PCF_IN_FCN_CALL))
         {
            LOG_FMT(LTEMPFUNC, "%s: marking '%s' in line %u as a FUNC_CALL\n",
                    __func__, pc->text(), pc->orig_line);
            set_type(pc, CT_FUNC_CALL);
            flag_parens(after, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_FUNC_CALL, false);
         }
         else
         {
            /* Might be a function def. Must check what is before the template:
             * Func call:
             *   BTree.Insert(std::pair<int, double>(*it, double(*it) + 1.0));
             *   a = Test<int>(j);
             *   std::pair<int, double>(*it, double(*it) + 1.0)); */
            LOG_FMT(LTEMPFUNC, "%s: marking '%s' in line %u as a FUNC_CALL 2\n",
                    __func__, pc->text(), pc->orig_line);

            set_type(pc, CT_FUNC_CALL); /* its a function */
            mark_function(pc);
         }
      }
      else if (is_type(after, CT_WORD))
      {
         set_type_and_flag(pc, CT_TYPE, PCF_VAR_TYPE); /* its a type */
         set_flags(after, PCF_VAR_DEF );
      }
   }
}


static void mark_exec_sql(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   chunk_t* tmp;

   /* Change CT_WORD to CT_SQL_WORD */
   for (tmp = chunk_get_next(pc); is_valid(tmp); tmp = chunk_get_next(tmp))
   {
      set_ptype(tmp, pc->type);
      if (is_type(tmp, CT_WORD))
      {
         set_type(tmp, CT_SQL_WORD);
      }
      break_if(is_type(tmp, CT_SEMICOLON));
   }

   return_if(not_type              (pc,  CT_SQL_BEGIN) ||
             is_invalid_or_not_type(tmp, CT_SEMICOLON) );
   for (tmp = chunk_get_next(tmp);
        not_type(tmp, CT_SQL_END);
        tmp = chunk_get_next(tmp))
   {
      tmp->level++;
   }
}


chunk_t* skip_template_next(chunk_t* ang_open)
{
   if (is_type(ang_open, CT_ANGLE_OPEN))
   {
      chunk_t* pc = get_next_type(ang_open, CT_ANGLE_CLOSE, (int32_t)ang_open->level);
      return(get_next_ncnl(pc));
   }
   return(ang_open);
}


chunk_t* skip_template_prev(chunk_t* ang_close)
{
   if (is_type(ang_close, CT_ANGLE_CLOSE))
   {
      chunk_t* pc = get_prev_type(ang_close, CT_ANGLE_OPEN, (int32_t)ang_close->level);
      return(get_prev_ncnl(pc));
   }
   return(ang_close);
}


chunk_t* skip_tsquare_next(chunk_t* ary_def)
{
   if (is_type(ary_def, CT_SQUARE_OPEN, CT_TSQUARE))
   {
      return(get_next_nisq(ary_def));
   }
   return(ary_def);
}


chunk_t* skip_attribute_next(chunk_t* attr)
{
   if (is_type(attr, CT_ATTRIBUTE))
   {
      chunk_t* pc = chunk_get_next(attr);
      if (is_type(pc, CT_FPAREN_OPEN))
      {
         pc = get_next_type(attr, CT_FPAREN_CLOSE, (int32_t)attr->level);
         return(get_next_ncnl(pc));
      }
      return(pc);
   }
   return(attr);
}


chunk_t* skip_attribute_prev(chunk_t* fp_close)
{
   if (is_type_and_ptype(fp_close, CT_FPAREN_CLOSE, CT_ATTRIBUTE))
   {
      chunk_t* pc = get_prev_type(fp_close, CT_ATTRIBUTE, (int32_t)fp_close->level);
      return(get_prev_ncnl(pc));
   }
   return(fp_close);
}


static void handle_oc_class(chunk_t* pc)
{
   enum class angle_state_e : uint32_t
   {
      NONE  = 0,
      OPEN  = 1, // '<' found
      CLOSE = 2  // '>' found
   };

   LOG_FUNC_ENTRY();
   chunk_t* tmp;
   bool    hit_scope     = false;
   bool    passed_name   = false; // Did we pass the name of the class and now there can be only protocols, not generics
   int32_t     generic_level = 0;     // level of depth of generic
   angle_state_e as      = angle_state_e::NONE;

   LOG_FMT(LOCCLASS, "%s: start [%s] [%s] line %u\n",
           __func__, pc->text(), get_token_name(pc->ptype), pc->orig_line);

   if (is_ptype(pc, CT_OC_PROTOCOL))
   {
      tmp = get_next_ncnl(pc);
      if (is_semicolon(tmp))
      {
         set_ptype(tmp, pc->ptype);
         LOG_FMT(LOCCLASS, "%s:   bail on semicolon\n", __func__);
         return;
      }
   }

   tmp = pc;
   while ((tmp = get_next_nnl(tmp)) != nullptr)
   {
      LOG_FMT(LOCCLASS, "%s:       %u [%s]\n",
              __func__, tmp->orig_line, tmp->text());

      break_if(is_type(tmp, CT_OC_END));
      if (is_type(tmp, CT_PAREN_OPEN))
      {
         passed_name = true;
      }
      if (is_str(tmp, "<"))
      {
         set_type(tmp, CT_ANGLE_OPEN);
         if (passed_name)
         {
            set_ptype(tmp, CT_OC_PROTO_LIST);
         }
         else
         {
            set_ptype(tmp, CT_OC_GENERIC_SPEC);
            generic_level++;
         }
         as = angle_state_e::OPEN;
      }
      if (is_str(tmp, ">"))
      {
         set_type(tmp, CT_ANGLE_CLOSE);
         if (passed_name)
         {
            set_ptype(tmp, CT_OC_PROTO_LIST);
            as = angle_state_e::CLOSE;
         }
         else
         {
            set_ptype(tmp, CT_OC_GENERIC_SPEC);
            generic_level--;
            if (generic_level == 0)
            {
               as = angle_state_e::CLOSE;
            }
         }
      }
      if (is_str(tmp, ">>"))
      {
         set_type_and_ptype(tmp, CT_ANGLE_CLOSE, CT_OC_GENERIC_SPEC);
         split_off_angle_close(tmp);
         generic_level -= 1;
         if (generic_level == 0)
         {
            as = angle_state_e::CLOSE;
         }
      }
      if (is_type(tmp, CT_BRACE_OPEN))
      {
         as = angle_state_e::CLOSE;
         set_ptype(tmp, CT_OC_CLASS);
         tmp = get_next_type(tmp, CT_BRACE_CLOSE, (int32_t)tmp->level);
         if (is_valid(tmp))
         {
            set_ptype(tmp, CT_OC_CLASS);
         }
      }
      else if (is_type(tmp, CT_COLON))
      {
         if (as != angle_state_e::OPEN)
         {
            passed_name = true;
         }
         set_type(tmp, hit_scope ? CT_OC_COLON : CT_CLASS_COLON);
         if (is_type(tmp, CT_CLASS_COLON))
         {
            set_ptype(tmp, CT_OC_CLASS);
         }
      }
      else if (is_str(tmp, "-") ||
               is_str(tmp, "+") )
      {
         as = angle_state_e::CLOSE;
         if (is_nl(chunk_get_prev(tmp)))
         {
            set_type(tmp, CT_OC_SCOPE);
            set_flags(tmp, PCF_STMT_START);
            hit_scope = true;
         }
      }
      if (as == angle_state_e::OPEN)
      {
         const c_token_t type = passed_name ? CT_OC_PROTO_LIST : CT_OC_GENERIC_SPEC;
         set_ptype(tmp, type);
      }
   }

   if (is_type(tmp, CT_BRACE_OPEN))
   {
      tmp = get_next_type(tmp, CT_BRACE_CLOSE, (int32_t)tmp->level);
      if (is_valid(tmp))
      {
         set_ptype(tmp, CT_OC_CLASS);
      }
   }
}


static void handle_oc_block_literal(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   const chunk_t* prev = get_prev_ncnl(pc);
   chunk_t       *next = get_next_ncnl(pc);
   return_if(are_invalid(pc, prev, next));

   /* block literal: '^ RTYPE ( ARGS ) { }'
    * RTYPE and ARGS are optional */
   LOG_FMT(LOCBLK, "%s: block literal @ %u:%u\n",
         __func__, pc->orig_line, pc->orig_col);

   chunk_t* apo = nullptr; /* arg paren open */
   chunk_t* bbo = nullptr; /* block brace open */
   chunk_t* bbc;           /* block brace close */

   LOG_FMT(LOCBLK, "%s:  + scan", __func__);
   chunk_t* tmp;
   for (tmp = next; is_valid(tmp); tmp = get_next_ncnl(tmp))
   {
      LOG_FMT(LOCBLK, " %s", tmp->text());
      if ((tmp->level < pc->level   ) ||
          (is_type(tmp, CT_SEMICOLON) ))
      {
         LOG_FMT(LOCBLK, "[DONE]");
         break;
      }
      if (tmp->level == pc->level)
      {
         if (is_paren_open(tmp))
         {
            LOG_FMT(LOCBLK, "[PAREN]");
            apo = tmp;
         }
         if (is_type(tmp, CT_BRACE_OPEN))
         {
            LOG_FMT(LOCBLK, "[BRACE]");
            bbo = tmp;
            break;
         }
      }
   }

   /* make sure we have braces */
   bbc = chunk_skip_to_match(bbo);
   if (are_invalid(bbo, bbc))
   {
      LOG_FMT(LOCBLK, " -- no braces found\n");
      return;
   }
   LOG_FMT(LOCBLK, "\n");

   /* we are on a block literal for sure */
   set_type_and_ptype(pc, CT_OC_BLOCK_CARET, CT_OC_BLOCK_EXPR );

   /* handle the optional args */
   chunk_t* lbp; /* last before parenthesis - end of return type, if any */
   if (apo)
   {
      chunk_t* apc = chunk_skip_to_match(apo);  /* arg parenthesis close */
      if (is_paren_close(apc))
      {
         LOG_FMT(LOCBLK, " -- marking parens @ %u:%u and %u:%u\n",
                 apo->orig_line, apo->orig_col, apc->orig_line, apc->orig_col);
         flag_parens(apo, PCF_OC_ATYPE, CT_FPAREN_OPEN, CT_OC_BLOCK_EXPR, true);
         fix_fcn_def_params(apo);
      }
      lbp = get_prev_ncnl(apo);
   }
   else
   {
      lbp = get_prev_ncnl(bbo);
   }

   /* mark the return type, if any */
   while (lbp != pc)
   {
      assert(is_valid(lbp));
      LOG_FMT(LOCBLK, " -- lbp %s[%s]\n", lbp->text(), get_token_name(lbp->type));
      make_type(lbp);
      set_flags(lbp, PCF_OC_RTYPE    );
      set_ptype(lbp, CT_OC_BLOCK_EXPR);
      lbp = get_prev_ncnl(lbp);
   }
   /* mark the braces */
   set_ptype(bbo, CT_OC_BLOCK_EXPR);
   set_ptype(bbc, CT_OC_BLOCK_EXPR);
}


static void handle_oc_block_type(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   return_if(is_invalid(pc));

   if (is_flag(pc, PCF_IN_TYPEDEF))
   {
      LOG_FMT(LOCBLK, "%s: skip block type @ %u:%u -- in typedef\n",
              __func__, pc->orig_line, pc->orig_col);
      return;
   }

   /* make sure we have '( ^' */
   chunk_t* tpo = get_prev_ncnl(pc); /* type paren open */
   if (is_paren_open(tpo))
   {
      /* block type: 'RTYPE (^LABEL)(ARGS)'
       * LABEL is optional. */
      chunk_t* tpc = chunk_skip_to_match(tpo); /* type close parenthesis (after '^') */
      chunk_t* nam = get_prev_ncnl(tpc); /* name (if any) or '^' */
      chunk_t* apo = get_next_ncnl(tpc); /* arg open parenthesis */
      chunk_t* apc = chunk_skip_to_match(apo); /* arg close parenthesis */

      /* If this is a block literal instead of a block type, 'nam'
       * will actually be the closing bracket of the block. We run into
       * this situation if a block literal is enclosed in parentheses. */
      if (is_closing_brace(nam))
      {
         return(handle_oc_block_literal(pc));
      }

      if (is_paren_close(apc))
      {
         chunk_t*  aft = get_next_ncnl(apc);
         c_token_t pt;

         if (is_str(nam, "^"))
         {
            set_type(nam, CT_PTR_TYPE);
            pt = CT_FUNC_TYPE;
         }
         else if (is_type(aft, CT_ASSIGN, CT_SEMICOLON))
         {
            set_type(nam, CT_FUNC_VAR);
            pt = CT_FUNC_VAR;
         }
         else
         {
            set_type(nam, CT_FUNC_TYPE);
            pt = CT_FUNC_TYPE;
         }
         assert(is_valid(nam));
         LOG_FMT(LOCBLK, "%s: block type @ %u:%u (%s)[%s]\n",
                 __func__, pc->orig_line, pc->orig_col, nam->text(), get_token_name(nam->type));
         set_type_and_ptype(pc,  CT_PTR_TYPE,     pt); //CT_OC_BLOCK_TYPE;
         set_type_and_ptype(tpo, CT_TPAREN_OPEN,  pt); //CT_OC_BLOCK_TYPE;
         set_type_and_ptype(tpc, CT_TPAREN_CLOSE, pt); //CT_OC_BLOCK_TYPE;
         set_type_and_ptype(apo, CT_FPAREN_OPEN,  CT_FUNC_PROTO);
         set_type_and_ptype(apc, CT_FPAREN_CLOSE, CT_FUNC_PROTO);
         fix_fcn_def_params(apo);
         mark_function_return_type(nam, get_prev_ncnl(tpo), pt);
      }
   }
}


static chunk_t* handle_oc_md_type(chunk_t* paren_open, c_token_t ptype, uint64_t flags, bool &did_it)
{
   chunk_t* paren_close;

   if (!is_paren_open(paren_open) ||
       ((paren_close = chunk_skip_to_match(paren_open)) == nullptr))
   {
      did_it = false;
      return(paren_open);
   }

   did_it = true;

   set_ptype_and_flag(paren_open,  ptype, flags);
   set_ptype_and_flag(paren_close, ptype, flags);

   for (chunk_t* cur = get_next_ncnl(paren_open);
        cur != paren_close;
        cur = get_next_ncnl(cur))
   {
      assert(is_valid(cur));
      LOG_FMT(LOCMSGD, " <%s|%s>", cur->text(), get_token_name(cur->type));
      set_flags(cur, flags);
      make_type(cur);
   }

   /* returning the chunk after the paren close */
   return(get_next_ncnl(paren_close));
}


static void handle_oc_message_decl(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   /* Figure out if this is a specification or declaration */
   chunk_t* tmp = pc;
   while ((tmp = chunk_get_next(tmp)) != nullptr)
   {
      return_if(tmp->level < pc->level); /* should not happen */
      break_if(is_type(tmp, CT_SEMICOLON, CT_BRACE_OPEN));
   }
   return_if(is_invalid(tmp));

   c_token_t pt = (is_type(tmp, CT_SEMICOLON)) ? CT_OC_MSG_SPEC : CT_OC_MSG_DECL;
   set_type_and_ptype(pc, CT_OC_SCOPE, pt);
   LOG_FMT(LOCMSGD, "%s: %s @ %u:%u -", __func__, get_token_name(pt), pc->orig_line, pc->orig_col);

   /* format: -(TYPE) NAME [: (TYPE)NAME */

   /* handle the return type */
   bool did_it;
   tmp = handle_oc_md_type(get_next_ncnl(pc), pt, PCF_OC_RTYPE, did_it);
   if (did_it == false)        { LOG_FMT(LOCMSGD, " -- missing type parens\n"); return; }
   if (!is_type(tmp, CT_WORD)) { LOG_FMT(LOCMSGD, " -- missing method name\n"); return; } /* expect the method name/label */

   chunk_t* label = tmp;
   set_type_and_ptype(tmp, pt, pt);
   pc = get_next_ncnl(tmp);
   assert(is_valid(pc));
   LOG_FMT(LOCMSGD, " [%s]%s", pc->text(), get_token_name(pc->type));

   /* if we have a colon next, we have args */
   if (is_type(pc, CT_COLON, CT_OC_COLON))
   {
      pc = label;

      while (true)
      {
         /* skip optional label */
         if (is_type(pc, CT_WORD, pt))
         {
            set_ptype(pc, pt);
            pc = get_next_ncnl(pc);
         }
         /* a colon must be next */
         break_if (!is_str(pc, ":"));

         set_type_and_ptype(pc, CT_OC_COLON, pt);
         pc = get_next_ncnl(pc);
         assert(is_valid(pc));

         /* next is the type in parens */
         LOG_FMT(LOCMSGD, "  (%s)", pc->text());
         tmp = handle_oc_md_type(pc, pt, PCF_OC_ATYPE, did_it);
         if (did_it == false)
         {
            LOG_FMT(LWARN, "%s: %u:%u expected type\n",
                  __func__, pc->orig_line, pc->orig_col);
            break;
         }
         pc = tmp;
         assert(is_valid(pc));
         /* we should now be on the arg name */
         set_flags(pc, PCF_VAR_DEF);
         LOG_FMT(LOCMSGD, " arg[%s]", pc->text());
         pc = get_next_ncnl(pc);
      }
   }

   assert(is_valid(pc));
   LOG_FMT(LOCMSGD, " end[%s]", pc->text());

   if (is_type(pc, CT_BRACE_OPEN))
   {
      set_ptype(pc, pt);
      pc = chunk_skip_to_match(pc);
      if (is_valid(pc)) { set_ptype(pc, pt); }
   }
   else if (is_type(pc, CT_SEMICOLON)) { set_ptype(pc, pt); }

   LOG_FMT(LOCMSGD, "\n");
}


static void handle_oc_message_send(chunk_t* os)
{
   LOG_FUNC_ENTRY();

   chunk_t* cs = chunk_get_next(os);
   while ((is_valid(cs)) && (cs->level > os->level))
   {
      cs = chunk_get_next(cs);
   }

   return_if(is_invalid_or_not_type(cs, CT_SQUARE_CLOSE));
   LOG_FMT(LOCMSG, "%s: line %u, col %u\n",
         __func__, os->orig_line, os->orig_col);

   chunk_t* tmp = get_next_ncnl(cs);
   if (is_semicolon(tmp))
   {
      set_ptype(tmp, CT_OC_MSG);
   }

   set_ptype_and_flag(os, CT_OC_MSG, PCF_IN_OC_MSG);
   set_ptype_and_flag(cs, CT_OC_MSG, PCF_IN_OC_MSG);

   /* expect a word first thing or [...] */
   tmp = get_next_ncnl(os);
   assert(is_valid(tmp));
   if (is_type(tmp, CT_SQUARE_OPEN, CT_PAREN_OPEN))
   {
      tmp = chunk_skip_to_match(tmp);
   }
   else if (not_type(tmp, CT_WORD, CT_TYPE, CT_STRING))
   {
      LOG_FMT(LOCMSG, "%s: %u:%u expected identifier, not '%s' [%s]\n",
              __func__, tmp->orig_line, tmp->orig_col,
              tmp->text(), get_token_name(tmp->type));
      return;
   }
   else
   {
      chunk_t* tt = get_next_ncnl(tmp);
      if (is_paren_open(tt))
      {
         set_type(tmp, CT_FUNC_CALL);
         tmp = get_prev_ncnl(set_paren_parent(tt, CT_FUNC_CALL));
      }
      else
      {
         set_type(tmp, CT_OC_MSG_CLASS);
      }
   }

   /* handle '< protocol >' */
   tmp = get_next_ncnl(tmp);
   if (is_str(tmp, "<"))
   {
      chunk_t* ao = tmp;
      assert(is_valid(ao));
      chunk_t* ac = get_next_str(ao, ">", 1, (int32_t)ao->level);

      if (is_valid(ac))
      {
         set_type_and_ptype(ao, CT_ANGLE_OPEN,  CT_OC_PROTO_LIST);
         set_type_and_ptype(ac, CT_ANGLE_CLOSE, CT_OC_PROTO_LIST);
         for (tmp = chunk_get_next(ao); tmp != ac; tmp = chunk_get_next(tmp))
         {
            assert(is_valid(tmp));
            tmp->level += 1;
            set_ptype(tmp, CT_OC_PROTO_LIST);
         }
      }
      tmp = get_next_ncnl(ac);
   }
   /* handle 'object.property' and 'collection[index]' */
   else
   {
      while (is_valid(tmp))
      {
         if (is_type(tmp, CT_MEMBER))  /* move past [object.prop1.prop2  */
         {
            chunk_t* typ = get_next_ncnl(tmp);
            if (is_type(typ, CT_WORD, CT_TYPE))
            {
               tmp = get_next_ncnl(typ);
            }
            else { break; }
         }
         else if (is_type(tmp, CT_SQUARE_OPEN))  /* move past [collection[index]  */
         {
            chunk_t* tcs = get_next_ncnl(tmp);
            while ((is_valid(tcs)) && (tcs->level > tmp->level))
            {
               tcs = get_next_ncnl(tcs);
            }
            if (is_type(tcs, CT_SQUARE_CLOSE))
            {
               tmp = get_next_ncnl(tcs);
            }
            else { break; }
         }
         else { break; }
      }
   }

   if (is_type(tmp, CT_WORD, CT_TYPE)) { set_type(tmp, CT_OC_MSG_FUNC); }

   chunk_t* prev = nullptr;
   for (tmp = chunk_get_next(os); tmp != cs; tmp = chunk_get_next(tmp))
   {
      assert(is_valid(tmp));
      set_flags(tmp, PCF_IN_OC_MSG);
      if (tmp->level == cs->level + 1)
      {
         if (is_type(tmp, CT_COLON))
         {
            set_type(tmp, CT_OC_COLON);
            if (is_type(prev, CT_WORD, CT_TYPE))
            {
               /* Might be a named param, check previous block */
               chunk_t* pp = chunk_get_prev(prev);
               if (not_type(pp, CT_OC_COLON, CT_ARITH, CT_CARET) )
               {
                  set_type (prev, CT_OC_MSG_NAME);
                  set_ptype(tmp,  CT_OC_MSG_NAME);
               }
            }
         }
      }
      prev = tmp;
   }
}


static void handle_oc_property_decl(chunk_t* os)
{
   if (is_true(UO_mod_sort_oc_properties))
   {
      typedef std::vector<chunk_t* > ChunkGroup;
      std::vector<ChunkGroup> thread_chunks;      // atomic/nonatomic
      std::vector<ChunkGroup> readwrite_chunks;   // readwrite, readonly
      std::vector<ChunkGroup> ref_chunks;         // retain, copy, assign, weak, strong, unsafe_unretained
      std::vector<ChunkGroup> getter_chunks;      // getter
      std::vector<ChunkGroup> setter_chunks;      // setter
      std::vector<ChunkGroup> nullability_chunks; // nonnull/nullable

      chunk_t* open_paren = nullptr;
      chunk_t* next       = chunk_get_next(os);
      assert(is_valid(next));
      if (is_type(next, CT_PAREN_OPEN))
      {
         open_paren = next;
         next       = chunk_get_next(next);

         /* Determine location of the property attributes
          * NOTE: Did not do this in the combine.cpp do_symbol_check as
          * I was not sure what the ramifications of adding a new type
          * for each of the below types would be. It did break some items
          * when I attempted to add them so this is my hack for now. */
         while (not_type(next, CT_PAREN_CLOSE))
         {
            if (is_type(next, CT_WORD))
            {
               ChunkGroup chunkGroup;
               if      (is_str(next, "atomic"   ) ||
                        is_str(next, "nonatomic") )
               {
                  chunkGroup.push_back(next);
                  thread_chunks.push_back(chunkGroup);
               }
               else if (is_str(next, "readonly" ) ||
                        is_str(next, "readwrite") )
               {
                  chunkGroup.push_back(next);
                  readwrite_chunks.push_back(chunkGroup);
               }
               else if (is_str(next, "assign"           ) ||
                        is_str(next, "retain"           ) ||
                        is_str(next, "copy"             ) ||
                        is_str(next, "strong"           ) ||
                        is_str(next, "weak"             ) ||
                        is_str(next, "unsafe_unretained") )
               {
                  chunkGroup.push_back(next);
                  ref_chunks.push_back(chunkGroup);
               }
               else if (is_str(next, "getter"))
               {
                  do
                  {
                     chunkGroup.push_back(next);
                     next = chunk_get_next(next);
                  } while (not_type(next, CT_COMMA, CT_PAREN_CLOSE));
                  assert(is_valid(next));
                  next = next->prev;
                  break_if(is_invalid(next));

                  getter_chunks.push_back(chunkGroup);
               }
               else if (is_str(next, "setter"))
               {
                  do
                  {
                     chunkGroup.push_back(next);
                     next = chunk_get_next(next);
                  } while (not_type(next, CT_COMMA, CT_PAREN_CLOSE));
                  assert(is_valid(next));
                  next = next->prev;
                  break_if(is_invalid(next));

                  setter_chunks.push_back(chunkGroup);
               }
               else if (is_str(next, "nullable") ||
                        is_str(next, "nonnull" ) )
               {
                  chunkGroup.push_back(next);
                  nullability_chunks.push_back(chunkGroup);
               }
            }
            next = chunk_get_next(next);
         }

         int32_t thread_w      = get_ival(UO_mod_sort_oc_property_thread_safe_weight);
         int32_t readwrite_w   = get_ival(UO_mod_sort_oc_property_readwrite_weight  );
         int32_t ref_w         = get_ival(UO_mod_sort_oc_property_reference_weight  );
         int32_t getter_w      = get_ival(UO_mod_sort_oc_property_getter_weight     );
         int32_t setter_w      = get_ival(UO_mod_sort_oc_property_setter_weight     );
         int32_t nullability_w = get_ival(UO_mod_sort_oc_property_nullability_weight);

         std::multimap<int32_t, std::vector<ChunkGroup>> sorted_chunk_map;
         sorted_chunk_map.insert(pair<int32_t, std::vector<ChunkGroup> >(thread_w,      thread_chunks));
         sorted_chunk_map.insert(pair<int32_t, std::vector<ChunkGroup> >(readwrite_w,   readwrite_chunks));
         sorted_chunk_map.insert(pair<int32_t, std::vector<ChunkGroup> >(ref_w,         ref_chunks));
         sorted_chunk_map.insert(pair<int32_t, std::vector<ChunkGroup> >(getter_w,      getter_chunks));
         sorted_chunk_map.insert(pair<int32_t, std::vector<ChunkGroup> >(setter_w,      setter_chunks));
         sorted_chunk_map.insert(pair<int32_t, std::vector<ChunkGroup> >(nullability_w, nullability_chunks));

         chunk_t* curr_chunk = open_paren;
         for (multimap<int32_t, std::vector<ChunkGroup>>::reverse_iterator it = sorted_chunk_map.rbegin();
              it != sorted_chunk_map.rend();
              ++it)
         {
            std::vector<ChunkGroup> chunk_groups = (*it).second;
            for (auto chunk_group : chunk_groups)
            {
               for (auto chunk : chunk_group)
               {
                  chunk->orig_prev_sp = 0;
                  if (chunk != curr_chunk)
                  {
                     chunk_move_after(chunk, curr_chunk);
                     curr_chunk = chunk;
                  }
                  else
                  {
                     curr_chunk = chunk_get_next(curr_chunk);
                  }
               }

               /* add the parenthesis */
               assert(is_valid(curr_chunk));
               chunk_t endchunk;
               endchunk.type        = CT_COMMA;
               endchunk.str         = ", ";
               endchunk.level       = curr_chunk->level;
               endchunk.brace_level = curr_chunk->brace_level;
               endchunk.orig_line   = curr_chunk->orig_line;
               endchunk.column      = static_cast<int32_t>(curr_chunk->orig_col_end) + 1u;
               endchunk.ptype       = curr_chunk->ptype;
               set_flags(&endchunk, get_flags(curr_chunk, PCF_COPY_FLAGS));
               chunk_add_after(&endchunk, curr_chunk);
               curr_chunk = curr_chunk->next;
            }
         }

         /* Remove the extra comma's that we did not move */
         while (not_type(curr_chunk, CT_PAREN_CLOSE))
         {
            chunk_t* rm_chunk = curr_chunk;
            curr_chunk = chunk_get_next(curr_chunk);
            chunk_del(rm_chunk);
         }
      }
   }


   chunk_t* tmp = get_next_ncnl(os);
   if (is_paren_open(tmp))
   {
      tmp = get_next_ncnl(chunk_skip_to_match(tmp));
   }
   fix_var_def(tmp);
}


static void handle_cs_square_stmt(chunk_t* os)
{
   LOG_FUNC_ENTRY();

   chunk_t* cs = chunk_get_next(os);
   while (is_valid(cs) && (cs->level > os->level))
   {
      cs = chunk_get_next(cs);
   }

   return_if(is_invalid_or_not_type(cs, CT_SQUARE_CLOSE));

   set_ptype(os, CT_CS_SQ_STMT);
   set_ptype(cs, CT_CS_SQ_STMT);

   chunk_t* tmp;
   for (tmp = chunk_get_next(os); tmp != cs; tmp = chunk_get_next(tmp))
   {
      assert(is_valid(tmp));
      set_ptype(tmp, CT_CS_SQ_STMT);
      if (is_type(tmp, CT_COLON)) { set_type(tmp, CT_CS_SQ_COLON); }
   }

   tmp = get_next_ncnl(cs);
   if (is_valid(tmp)) { set_flags(tmp, PCF_STMT_START | PCF_EXPR_START); }
}


static void handle_cs_property(chunk_t* bro)
{
   LOG_FUNC_ENTRY();

   set_paren_parent(bro, CT_CS_PROPERTY);

   bool    did_prop = false;
   chunk_t* pc      = bro;
   while ((pc = get_prev_ncnl(pc)) != nullptr)
   {
      if (pc->level == bro->level)
      {
         if ((did_prop == false) &&
              is_type(pc, CT_WORD, CT_THIS))
         {
            set_type(pc, CT_CS_PROPERTY);
            did_prop = true;
         }
         else
         {
            set_ptype(pc, CT_CS_PROPERTY);
            make_type(pc);
         }
         break_if(is_flag(pc, PCF_STMT_START));
      }
   }
}


static void handle_cs_array_type(chunk_t* pc)
{
   chunk_t* prev;

#if 0
   prev = chunk_get_prev_comma(pc);
   /* \todo better use search function here */
#else
   for (prev = chunk_get_prev(pc);
        is_type(prev, CT_COMMA);
        prev = chunk_get_prev(prev))
   {
      /* empty */
   }
#endif

   if (is_type(prev, CT_SQUARE_OPEN))
   {
      while (pc != prev)
      {
         pc->ptype = CT_TYPE;
         pc        = chunk_get_prev(pc);
      }
      prev->ptype = CT_TYPE;
   }
}


void remove_extra_returns(void)
{
   LOG_FUNC_ENTRY();

   chunk_t* pc = chunk_get_head();
   while (is_valid(pc))
   {
      if (is_type(pc, CT_RETURN) && !is_preproc(pc))
      {
         chunk_t* semi  = get_next_ncnl(pc);
         chunk_t* cl_br = get_next_ncnl(semi);

         if (are_types(semi, CT_SEMICOLON, cl_br, CT_BRACE_CLOSE) &&
             is_ptype(cl_br, CT_FUNC_DEF, CT_FUNC_CLASS_DEF  ) )
         {
            LOG_FMT(LRMRETURN, "Removed 'return;' on line %u\n", pc->orig_line);
            chunk_del(pc);
            chunk_del(semi);
            pc = cl_br;
         }
      }
      pc = chunk_get_next(pc);
   }
}


static void handle_wrap(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   chunk_t  *opp  = chunk_get_next(pc);
   chunk_t  *name = chunk_get_next(opp);
   chunk_t  *clp  = chunk_get_next(name);
   assert(are_valid(opp, name, clp));

   argval_t pav = (is_type(pc, CT_FUNC_WRAP)) ?
                  get_arg(UO_sp_func_call_paren) :
                  get_arg(UO_sp_cpp_cast_paren );

   const argval_t av = (is_type(pc, CT_FUNC_WRAP)) ?
                 get_arg(UO_sp_inside_fparen    ) :
                 get_arg(UO_sp_inside_paren_cast);

   if (is_type(clp,  CT_PAREN_CLOSE  ) &&
       is_type(opp,  CT_PAREN_OPEN   ) &&
       is_type(name, CT_WORD, CT_TYPE) )
   {
      const char *psp = is_opt_set(pav, AV_ADD) ? " " : "";
      const char *fsp = is_opt_set(av,  AV_ADD) ? " " : "";
      pc->str.append("%s(%s%s%s)", psp, fsp, name->str.c_str(), fsp);
      const c_token_t new_type = is_type(pc, CT_FUNC_WRAP) ? CT_FUNCTION : CT_TYPE;
      set_type(pc, new_type);

      pc->orig_col_end = pc->orig_col + pc->len();

      chunk_del(opp);
      chunk_del(name);
      chunk_del(clp);
   }
}


static void handle_proto_wrap(chunk_t* pc)
{
   LOG_FUNC_ENTRY();
   chunk_t* opp  = get_next_ncnl(pc);
   chunk_t* name = get_next_ncnl(opp);
   chunk_t* tmp  = get_next_ncnl(get_next_ncnl(name));
   chunk_t* clp  = chunk_skip_to_match(opp);
   const chunk_t* cma = get_next_ncnl(clp);

   return_if (are_invalid(opp, name    ) ||
              are_invalid(clp, cma, tmp) ||
              not_type(name, CT_WORD, CT_TYPE) ||
              not_type(opp,  CT_PAREN_OPEN   ) );

   switch(cma->type)
   {
      case(CT_SEMICOLON ): set_type(pc, CT_FUNC_PROTO); break;
      case(CT_BRACE_OPEN): set_type(pc, CT_FUNC_DEF  ); break;
      default:             /* unexpected chunk type */  return;
   }

   set_ptype(opp, pc->type);
   set_ptype(clp, pc->type);
   set_ptype(tmp, CT_PROTO_WRAP);

   if (is_type(tmp, CT_PAREN_OPEN)) { fix_fcn_def_params(tmp); }
   else                             { fix_fcn_def_params(opp); set_type(name, CT_WORD); }

   tmp = chunk_skip_to_match(tmp);
   set_ptype(tmp, CT_PROTO_WRAP);

   /* Mark return type (TODO: move to own function) */
   tmp = pc;
   while ((tmp = get_prev_ncnl(tmp)) != nullptr)
   {
      break_if (!is_var_type(tmp) &&
                not_type(tmp, CT_OPERATOR, CT_WORD, CT_ADDR));
      set_ptype(tmp, pc->type);
      make_type(tmp);
   }
}


static void handle_java_assert(chunk_t* pc)
{
   LOG_FUNC_ENTRY();

   bool     did_colon = false;
   chunk_t* tmp       = pc;
   while ((tmp = chunk_get_next(tmp)) != nullptr)
   {
      if (is_level(tmp, pc->level))
      {
         if ((did_colon == false  ) &&
             is_type(tmp, CT_COLON) )
         {
            did_colon = true;
            set_ptype(tmp, pc->type);
         }
         if (is_type(tmp, CT_SEMICOLON))
         {
            set_ptype(tmp, pc->type);
            break;
         }
      }
   }
}

