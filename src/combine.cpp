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
 * @return     The token after the close parenthesis
 */
static chunk_t *flag_parens(
   chunk_t   *po,       /**< [in] Pointer to the open parenthesis */
   UINT64    flags,     /**< [in] flags to add */
   c_token_t opentype,  /**< [in]  */
   c_token_t parenttype,/**< [in]  */
   bool      parent_all /**< [in]  */
);


/**
 * Mark the parenthesis and colons in:
 *   asm volatile ( "xx" : "xx" (l), "yy"(h) : ...  );
 */
static void flag_asm(
   chunk_t *pc  /**< [in] the CT_ASM item */
);


/**
 * Scan backwards to see if we might be on a type declaration
 */
static bool chunk_ends_type(
   chunk_t *start  /**< [in]  */
);


/**
 * skip to the final word/type in a :: chain
 * pc is either a word or a ::
 */
static chunk_t *skip_dc_member(
   chunk_t *start  /**< [in]  */
);


/**
 * Skips to the start of the next statement.
 */
static chunk_t *skip_to_next_statement(
   chunk_t *pc  /**< [in]  */
);


/**
 * Skips everything until a comma or semicolon at the same level.
 * Returns the semicolon, comma, or close brace/parenthesis or nullptr.
 */
static chunk_t *skip_expression(
   chunk_t *start  /**< [in]  */
);


/**
 * Skips the D 'align()' statement and the colon, if present.
 *    align(2) int foo;  -- returns 'int'
 *    align(4):          -- returns 'int'
 *    int bar;
 */
static chunk_t *skip_align(
   chunk_t *start  /**< [in]  */
);

/**
 * Combines two tokens into {{ and }} if inside parenthesis and nothing is between
 * either pair.
 */
static void check_double_brace_init(
   chunk_t *bo1  /**< [in]  */
);

/**
 * Simply change any STAR to PTR_TYPE and WORD to TYPE
 */
static void fix_fcn_def_params(
   chunk_t *pc  /**< [in] points to the function's open parenthesis */
);


/**
 * We are on a typedef.
 * If the next word is not enum/union/struct, then the last word before the
 * next ',' or ';' or '__attribute__' is a type.
 *
 * typedef [type...] [*] type [, [*]type] ;
 * typedef <return type>([*]func)();
 * typedef <return type>([*]func)(params);
 * typedef <return type>(__stdcall *func)(); Bug # 633    MS-specific extension
 *                                           include the config-file "test/config/MS-calling_conventions.cfg"
 * typedef <return type>func(params);
 * typedef <enum/struct/union> [type] [*] type [, [*]type] ;
 * typedef <enum/struct/union> [type] { ... } [*] type [, [*]type] ;
 */
static void fix_typedef(
   chunk_t *pc  /**< [in]  */
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
   chunk_t *pc  /**< [in]  */
);


/**
 * Checks to see if the current parenthesis is part of a cast.
 * We already verified that this doesn't follow function, TYPE, IF, FOR,
 * SWITCH, or WHILE and is followed by WORD, TYPE, STRUCT, ENUM, or UNION.
 */
static void fix_casts(
   chunk_t *pc  /**< [in]  */
);


/**
 * CT_TYPE_CAST follows this pattern:
 * dynamic_cast<...>(...)
 *
 * Mark everything between the <> as a type and set the paren parent
 */
static void fix_type_cast(
   chunk_t *pc  /**< [in]  */
);


/**
 * We are on the start of a sequence that could be a var def
 *  - FPAREN_OPEN (parent == CT_FOR)
 *  - BRACE_OPEN
 *  - SEMICOLON
 */
static chunk_t *fix_var_def(
   chunk_t *pc  /**< [in]  */
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
   chunk_t *pc  /**< [in]  */
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
   chunk_t *start,  /**< [in] the first chunk to look at */
   chunk_t *end     /**< [in] the chunk after the last one to look at */
);


/**
 * Changes the return type to type and set the parent.
 */
static void mark_function_return_type(
   chunk_t   *fname,      /**< [in]  */
   chunk_t   *pc,         /**< [in] the last chunk of the return type */
   c_token_t parent_type  /**< [in] CT_NONE (no change) or the new parent type */
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
   chunk_t *pc  /**< [in] Points to the first closing parenthesis */
);


/**
 * Examines the stuff between braces { }.
 * There should only be variable definitions and methods.
 * Skip the methods, as they will get handled elsewhere.
 */
static void mark_struct_union_body(
   chunk_t *start  /**< [in]  */
);


/**
 * We are on the first word of a variable definition.
 * Mark all the variable names with PCF_VAR_1ST and PCF_VAR_DEF as appropriate.
 * Also mark any '*' encountered as a CT_PTR_TYPE.
 * Skip over []. Go until a ';' is hit.
 *
 * Example input:
 * int   a = 3, b, c = 2;              ## called with 'a'
 * foo_t f = {1, 2, 3}, g = {5, 6, 7}; ## called with 'f'
 * struct {...} *a, *b;                ## called with 'a' or '*'
 * myclass a(4);
 */
static chunk_t *mark_variable_definition(
   chunk_t *start  /**< [in]  */
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
 * Processes a return statement, labeling the parenthesis and marking the parent.
 * May remove or add parens around the return statement
 */
static chunk_t *process_return(
   chunk_t *pc  /**< [in] Pointer to the return chunk */
);


/**
 * We're on a 'class' or 'struct'.
 * Scan for CT_FUNCTION with a string that matches pclass->str
 */
static void mark_class_ctor(
   chunk_t *pclass  /**< [in]  */
);


/**
 * We're on a 'namespace' skip the word and then set the parent of the braces.
 */
static void mark_namespace(
   chunk_t *pns  /**< [in]  */
);


/**
 * tbd
 */
static void mark_cpp_constructor(
   chunk_t *pc  /**< [in]  */
);


/**
 *  Just hit an assign. Go backwards until we hit an open brace/parenthesis/square or
 * semicolon (TODO: other limiter?) and mark as a LValue.
 */
static void mark_lvalue(
   chunk_t *pc  /**< [in]  */
);


/**
 * We are on a word followed by a angle open which is part of a template.
 * If the angle close is followed by a open paren, then we are on a template
 * function def or a template function call:
 *   Vector2<float>(...) [: ...[, ...]] { ... }
 * Or we could be on a variable def if it's followed by a word:
 *   Renderer<rgb32> rend;
 */
static void mark_template_func(
   chunk_t *pc,      /**< [in]  */
   chunk_t *pc_next  /**< [in]  */
);


/**
 * Just mark every CT_WORD until a semicolon as CT_SQL_WORD.
 * Adjust the levels if pc is CT_SQL_BEGIN
 */
static void mark_exec_sql(
   chunk_t *pc  /**< [in]  */
);


/**
 * Process an ObjC 'class'
 * pc is the chunk after '@implementation' or '@interface' or '@protocol'.
 * Change colons, etc. Processes stuff until '@end'.
 * Skips anything in braces.
 */
static void handle_oc_class(
   chunk_t *pc  /**< [in]  */
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
   chunk_t *pc  /**< [in] points to the '^' */
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
   chunk_t *pc  /**< [in] points to the '^' */
);


/**
 * Process an ObjC message spec/dec
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
 * -(void) foo: (int) arg: {  }
 * -(void) foo: (int) arg: {  }
 * -(void) insertObject:(id)anObject atIndex:(int)index
 */
static void handle_oc_message_decl(
   chunk_t *pc  /**< [in]  */
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
   chunk_t *pc  /**< [in] points to the open square '[' */
);


/**
 * Process @Property values and re-arrange them if necessary
 */
static void handle_oc_property_decl(
   chunk_t *pc  /**< [in]  */
);


/**
 * Process a type that is enclosed in parenthesis in message declarations.
 * TODO: handle block types, which get special formatting
 *
 * @param pc points to the open parenthesis
 * @return the chunk after the type
 */
static chunk_t *handle_oc_md_type(
   chunk_t  *paren_open, /**< [in]  */
   c_token_t ptype,      /**< [in]  */
   UINT64    flags,      /**< [in]  */
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
   chunk_t *pc  /**< [in] points to the open square '[' */
);


/**
 * We are on a brace open that is preceded by a word or square close.
 * Set the brace parent to CT_CS_PROPERTY and find the first item in the
 * property and set its parent, too.
 */
static void handle_cs_property(
   chunk_t *pc  /**< [in]  */
);


/**
 * We hit a ']' followed by a WORD. This may be a multidimensional array type.
 * Example: int[,,] x;
 * If there is nothing but commas between the open and close, then mark it.
 */
static void handle_cs_array_type(
   chunk_t *pc  /**< [in]  */
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
   chunk_t *pc  /**< [in]  */
);


/**
 * Verify and then mark C++ lambda expressions.
 * The expected format is '[...](...){...}' or '[...](...) -> type {...}'
 * sq_o is '[' CT_SQUARE_OPEN or '[]' CT_TSQUARE
 * Split the '[]' so we can control the space
 */
static void handle_cpp_lambda(
   chunk_t *pc  /**< [in]  */
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
   chunk_t *pc  /**< [in]  */
);


/**
 * A func wrap chunk and what follows should be treated as a function name.
 * Create new text for the chunk and call it a CT_FUNCTION.
 *
 * A type wrap chunk and what follows should be treated as a simple type.
 * Create new text for the chunk and call it a CT_TYPE.
 */
static void handle_wrap(
   chunk_t *pc  /**< [in]  */
);


/**
 * A prototype wrap chunk and what follows should be treated as a function prototype.
 *
 * RETTYPE PROTO_WRAP( NAME, PARAMS ); or RETTYPE PROTO_WRAP( NAME, (PARAMS) );
 * RETTYPE gets changed with make_type().
 * PROTO_WRAP is marked as CT_FUNC_PROTO or CT_FUNC_DEF.
 * NAME is marked as CT_WORD.
 * PARAMS is all marked as prototype parameters.
 */
static void handle_proto_wrap(
   chunk_t *pc  /**< [in]  */
);


/**
 * tbd
 */
static bool is_oc_block(
   chunk_t *pc  /**< [in]  */
);


/**
 * Java assert statements are: "assert EXP1 [: EXP2] ;"
 * Mark the parent of the colon and semicolon
 */
static void handle_java_assert(
   chunk_t *pc  /**< [in]  */
);


/**
 * Parse off the types in the D template args, adds to C-sharp
 * returns the close_paren
 */
static chunk_t *get_d_template_types(
   ChunkStack &cs,         /**< [in]  */
   chunk_t    *open_paren  /**< [in]  */
);


/**
 * tbd
 */
static bool chunkstack_match(
   const   ChunkStack &cs, /**< [in]  */
   chunk_t *pc             /**< [in]  */
);


void make_type(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   if (chunk_is_valid(pc))
   {
      if      (chunk_is_type (pc, CT_WORD)) { set_chunk_type(pc, CT_TYPE    ); }
      else if (chunk_is_star (pc) ||
               chunk_is_msref(pc))          { set_chunk_type(pc, CT_PTR_TYPE); }
      else if (chunk_is_addr (pc))          { set_chunk_type(pc, CT_BYREF   ); }
   }
}


void flag_series(chunk_t *start, chunk_t *end, UINT64 set_flags, UINT64 clr_flags, scope_e nav)
{
   LOG_FUNC_ENTRY();
   while ((chunk_is_valid(start)) &&
          (start != end ) )
   {
      chunk_flags_update(start, clr_flags, set_flags);
      start = chunk_get_next(start, nav);
   }

   if (chunk_is_valid(end))
   {
      chunk_flags_update(end, clr_flags, set_flags);
   }
}


static chunk_t *flag_parens(chunk_t *po, UINT64 flags, c_token_t opentype,
                            c_token_t parenttype, bool parent_all)
{
   LOG_FUNC_ENTRY();
   chunk_t *paren_close = chunk_skip_to_match(po, scope_e::PREPROC);
   if (chunk_is_invalid(paren_close))
   {
      assert(chunk_is_valid(po));
      LOG_FMT(LERR, "flag_parens: no match for [%s] at [%zu:%zu]",
              po->text(), po->orig_line, po->orig_col);
      log_func_stack_inline(LERR);
      cpd.error_count++;
      return(nullptr);
   }

   assert(chunk_is_valid(po));
   LOG_FMT(LFLPAREN, "flag_parens: %zu:%zu [%s] and %zu:%zu [%s] type=%s ptype=%s",
           po->orig_line, po->orig_col, po->text(),
           paren_close->orig_line, paren_close->orig_col, paren_close->text(),
           get_token_name(opentype), get_token_name(parenttype));
   //log_func_stack_inline(LSETTYP);
   log_func_stack_inline(LFLPAREN);

   if (po != paren_close)
   {
      if ((flags != 0) ||
          (parent_all && (parenttype != CT_NONE)))
      {
         chunk_t *pc;
         for (pc = chunk_get_next(po, scope_e::PREPROC);
              pc != paren_close;
              pc = chunk_get_next(pc, scope_e::PREPROC))
         {
            chunk_flags_set(pc, flags);
            if (parent_all)
            {
               set_chunk_parent(pc, parenttype);
            }
         }
      }

      if (opentype != CT_NONE)
      {
         set_chunk_type(po, opentype);
         set_chunk_type(paren_close, get_inverse_type(opentype));
      }

      if (parenttype != CT_NONE)
      {
         set_chunk_parent(po, parenttype);
         set_chunk_parent(paren_close, parenttype);
      }
   }
   return(chunk_get_next_ncnl(paren_close, scope_e::PREPROC));
}


chunk_t *set_paren_parent(chunk_t *start, c_token_t parent)
{
   LOG_FUNC_ENTRY();
   chunk_t *end;

   end = chunk_skip_to_match(start, scope_e::PREPROC);
   if (chunk_is_valid(end))
   {
      assert(chunk_is_valid(start));
      LOG_FMT(LFLPAREN, "set_paren_parent: %zu:%zu [%s] and %zu:%zu [%s] type=%s ptype=%s",
              start->orig_line, start->orig_col, start->text(),
              end->orig_line, end->orig_col, end->text(),
              get_token_name(start->type), get_token_name(parent));
      log_func_stack_inline(LFLPAREN);
      set_chunk_parent(start, parent);
      set_chunk_parent(end, parent);
   }
   return(chunk_get_next_ncnl(end, scope_e::PREPROC));
}


static void flag_asm(chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   chunk_t *tmp = chunk_get_next_ncnl(pc, scope_e::PREPROC);
   if (!chunk_is_type(tmp, CT_QUALIFIER)) { return; }

   chunk_t *po = chunk_get_next_ncnl(tmp, scope_e::PREPROC);
   if (!chunk_is_paren_open(po)) { return; }

   chunk_t *end = chunk_skip_to_match(po, scope_e::PREPROC);
   if (chunk_is_invalid(end)) { return; }

   set_chunk_parent(po, CT_ASM);
   set_chunk_parent(end, CT_ASM);
   for (tmp = chunk_get_next_ncnl(po, scope_e::PREPROC);
        tmp != end;
        tmp = chunk_get_next_ncnl(tmp, scope_e::PREPROC))
   {
      assert(chunk_is_valid(tmp));
      if (chunk_is_type(tmp, CT_COLON))
      {
         set_chunk_type(tmp, CT_ASM_COLON);
      }
      else if (chunk_is_type(tmp, CT_DC_MEMBER))
      {
         /* if there is a string on both sides, then this is two ASM_COLONs */
         if (chunk_is_type(chunk_get_next_ncnl(tmp, scope_e::PREPROC), CT_STRING) &&
             chunk_is_type(chunk_get_prev_ncnl(tmp, scope_e::PREPROC), CT_STRING) )
         {
            chunk_t nc;

            nc = *tmp;

            tmp->str.resize(1);
            tmp->orig_col_end = tmp->orig_col + 1;
            set_chunk_type(tmp, CT_ASM_COLON);

            nc.type = tmp->type;
            nc.str.pop_front();
            nc.orig_col++;
            nc.column++;
            chunk_add_after(&nc, tmp);
         }
      }
   }
   tmp = chunk_get_next_ncnl(end, scope_e::PREPROC);
   if (chunk_is_type(tmp, CT_SEMICOLON))
   {
      set_chunk_parent(tmp, CT_ASM);
   }
}


static bool chunk_ends_type(chunk_t *start)
{
   LOG_FUNC_ENTRY();

   if(chunk_is_invalid(start)) { return false; }

   bool    ret       = false;
   size_t  cnt       = 0;
   bool    last_lval = false;

   chunk_t *pc       = start;
   for ( ; chunk_is_valid(pc); pc = chunk_get_prev_ncnl(pc))
   {
      LOG_FMT(LFTYPE, "%s: [%s] %s flags %" PRIx64 " on line %zu, col %zu\n",
              __func__, get_token_name(pc->type), pc->text(),
              pc->flags, pc->orig_line, pc->orig_col);

      if(chunk_is_type(pc, 6, CT_QUALIFIER, CT_WORD, CT_STRUCT,
                              CT_DC_MEMBER, CT_TYPE, CT_PTR_TYPE))
      {
         cnt++;
         last_lval = (pc->flags & PCF_LVALUE) != 0;   // forcing value to bool
         continue;
      }

      if (((chunk_is_semicolon(pc)      ) &&
         ((pc->flags & PCF_IN_FOR) == 0)) ||
          chunk_is_type(pc, 3, CT_TYPEDEF, CT_BRACE_OPEN, CT_BRACE_CLOSE) ||
          (chunk_is_forin(pc)          ) ||
          ((pc->type  == CT_SPAREN_OPEN) &&
           (last_lval == true)         ) )
      {
         ret = cnt > 0;
      }
      break;
   }

   if (chunk_is_invalid(pc)) { ret = true; } /* first token */
   LOG_FMT(LFTYPE, "%s verdict: %s\n", __func__, ret ? "yes" : "no");
   return(ret);
}


static chunk_t *skip_dc_member(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   if (chunk_is_invalid(start)) { return(nullptr); }

   chunk_t *pc   = start;
   chunk_t *next = (chunk_is_type(pc, CT_DC_MEMBER)) ? pc : chunk_get_next_ncnl(pc);
   while (chunk_is_type(next, CT_DC_MEMBER))
   {
      pc   = chunk_get_next_ncnl(next);
      next = chunk_get_next_ncnl(pc);
   }
   return(pc);
}


void do_symbol_check(chunk_t *prev, chunk_t *pc, chunk_t *next)
{
   LOG_FUNC_ENTRY();
   chunk_t *tmp;

#ifdef DEBUG
   LOG_FMT(LGUY, "(%d) ", __LINE__);
#endif

#if 1
   LOG_FMT(LGUY, "%s: %zu:%zu %s:%s\n", __func__, pc->orig_line, pc->orig_col,
                                        pc->text(), get_token_name(pc->type));
#else
   switch(pc->type)
   {  /* \todo why can the default case not handle all cases? */
      case(CT_NEWLINE     ): { LOG_FMT(LGUY, "%s: %zu:%zu CT_NEWLINE\n",      __func__, pc->orig_line, pc->orig_col);  break;}
      case(CT_VBRACE_OPEN ): { LOG_FMT(LGUY, "%s: %zu:%zu CT_VBRACE_OPEN\n",  __func__, pc->orig_line, pc->orig_col);  break;}
      case(CT_VBRACE_CLOSE): { LOG_FMT(LGUY, "%s: %zu:%zu CT_VBRACE_CLOSE\n", __func__, pc->orig_line, pc->orig_col);  break;}
      default:               { LOG_FMT(LGUY, "%s: %zu:%zu %s:%s\n",           __func__, pc->orig_line, pc->orig_col, pc->text(), get_token_name(pc->type)); break;}
   }
#endif
   // LOG_FMT(LSYS, " %3d > ['%s' %s] ['%s' %s] ['%s' %s]\n",
   //         pc->orig_line,
   //         prev->text(), get_token_name(prev->type),
   //         pc->text(), get_token_name(pc->type),
   //         next->text(), get_token_name(next->type));

   if (chunk_is_type(pc, CT_OC_AT))
   {
      if(chunk_is_type(next, 3, CT_PAREN_OPEN, CT_BRACE_OPEN, CT_SQUARE_OPEN) )
      {
         flag_parens(next, PCF_OC_BOXED, next->type, CT_OC_AT, false);
      }
      else
      {
         set_chunk_parent(next, CT_OC_AT);
      }
   }

   /* D stuff */
   if ((cpd.lang_flags & LANG_D         ) &&
       chunk_is_type(pc,   CT_QUALIFIER ) &&
       chunk_is_str (pc,   "const", 5   ) &&
       chunk_is_type(next, CT_PAREN_OPEN) )
   {
      set_chunk_type(pc, CT_D_CAST);
      set_paren_parent(next, pc->type);
   }

   if ( chunk_is_type(next,  CT_PAREN_OPEN                   ) &&
        chunk_is_type(pc, 3, CT_D_CAST, CT_DELEGATE, CT_ALIGN) )
   {
      /* mark the parenthesis parent */
      tmp = set_paren_parent(next, pc->type);

      /* For a D cast - convert the next item */
      if ((pc->type == CT_D_CAST) &&
           chunk_is_valid(tmp)    )
      {
         switch(tmp->type)
         {
            case(CT_STAR ): { set_chunk_type(tmp, CT_DEREF); break;}
            case(CT_AMP  ): { set_chunk_type(tmp, CT_ADDR ); break;}
            case(CT_MINUS): { set_chunk_type(tmp, CT_NEG  ); break;}
            case(CT_PLUS ): { set_chunk_type(tmp, CT_POS  ); break;}
            default:        { /* ignore unexpected type */   break;}
         }
      }

      /* For a delegate, mark previous words as types and the item after the
       * close paren as a variable def
       */
      if (chunk_is_type(pc, CT_DELEGATE))
      {
         if (chunk_is_valid(tmp))
         {
            set_chunk_parent(tmp, CT_DELEGATE);
            if (tmp->level == tmp->brace_level)
            {
               chunk_flags_set(tmp, PCF_VAR_1ST_DEF);
            }
         }

         for (tmp = chunk_get_prev_ncnl(pc); chunk_is_valid(tmp); tmp = chunk_get_prev_ncnl(tmp))
         {
            if ((chunk_is_semicolon(tmp)    ) ||
                chunk_is_type(tmp, 2,CT_BRACE_OPEN, CT_VBRACE_OPEN) )
            {
               break;
            }
            make_type(tmp);
         }
      }

      if (chunk_is_type(pc, CT_ALIGN))
      {
         if      (chunk_is_type(tmp, CT_BRACE_OPEN)) { set_paren_parent(tmp, pc->type); }
         else if (chunk_is_type(tmp, CT_COLON     )) { set_chunk_parent(tmp, pc->type); }
      }
   } /* paren open + cast/align/delegate */

   if (chunk_is_type(pc, CT_INVARIANT))
   {
      if (chunk_is_type(next, CT_PAREN_OPEN))
      {
         set_chunk_parent(next, pc->type);
         tmp = chunk_get_next(next);
         while (chunk_is_valid(tmp))
         {
            if (chunk_is_type(tmp, CT_PAREN_CLOSE))
            {
               set_chunk_parent(tmp, pc->type);
               break;
            }
            make_type(tmp);
            tmp = chunk_get_next(tmp);
         }
      }
      else
      {
         set_chunk_type(pc, CT_QUALIFIER);
      }
   }

   if (chunk_is_type(prev, CT_BRACE_OPEN   ) &&
       (prev->parent_type != CT_CS_PROPERTY) &&
       (chunk_is_type(pc, 2, CT_GETSET, CT_GETSET_EMPTY) ) )
   {
      flag_parens(prev, 0, CT_NONE, CT_GETSET, false);
   }

   if (chunk_is_type(pc, CT_ASM)) { flag_asm(pc); }

   /* Objective C stuff */
   if (cpd.lang_flags & LANG_OC)
   {
      /* Check for message declarations */
      if (pc->flags & PCF_STMT_START)
      {
         if ((chunk_is_str(pc,   "-", 1) ||
              chunk_is_str(pc,   "+", 1) ) &&
              chunk_is_str(next, "(", 1)   )
         {
            handle_oc_message_decl(pc);
         }
      }
      if (pc->flags & PCF_EXPR_START)
      {
         if (chunk_is_type(pc, CT_SQUARE_OPEN)) { handle_oc_message_send (pc); }
         if (chunk_is_type(pc, CT_CARET      )) { handle_oc_block_literal(pc); }
      }

      if (chunk_is_type(pc, CT_OC_PROPERTY))
      {
         handle_oc_property_decl(pc);
      }
   }


   /* C# stuff */
   if (cpd.lang_flags & LANG_CS)
   {
      /* '[assembly: xxx]' stuff */
      if ((pc->flags & PCF_EXPR_START) &&
          (chunk_is_type(pc, CT_SQUARE_OPEN)))
      {
         handle_cs_square_stmt(pc);
      }

      if ((next->type        == CT_BRACE_OPEN  ) &&
          (next->parent_type == CT_NONE        ) &&
          (chunk_is_type(pc, 3, CT_SQUARE_CLOSE, CT_ANGLE_CLOSE, CT_WORD)))
      {
         handle_cs_property(next);
      }

      if (chunk_is_type(pc,   CT_SQUARE_CLOSE) &&
          chunk_is_type(next, CT_WORD        ) )
      {
         handle_cs_array_type(pc);
      }

      if (((chunk_is_type(pc, CT_LAMBDA ) ||
            (pc->type   == CT_DELEGATE  ) ) ) &&
            (chunk_is_type(next, CT_BRACE_OPEN) ))
      {
         set_paren_parent(next, pc->type);
      }

      if (chunk_is_type(pc, CT_WHEN        ) &&
          (pc->next->type != CT_SPAREN_OPEN) )
      {
         set_chunk_type(pc, CT_WORD);
      }
   }

   if (chunk_is_type(pc, CT_NEW))
   {
      chunk_t *ts = nullptr;
      tmp = next;
      if (chunk_is_type(tmp, CT_TSQUARE))
      {
         ts  = tmp;
         tmp = chunk_get_next_ncnl(tmp);
      }
      if (chunk_is_type(tmp, CT_BRACE_OPEN))
      {
         set_paren_parent(tmp, pc->type);
         if (chunk_is_valid(ts))
         {
            ts->parent_type = pc->type;
         }
      }
   }

   /* C++11 Lambda stuff */
   if ((cpd.lang_flags & LANG_CPP  )   &&
       (chunk_is_type(pc, 2, CT_SQUARE_OPEN, CT_TSQUARE)))
   {
      handle_cpp_lambda(pc);
   }

   /* FIXME: which language does this apply to? */
   if (chunk_is_type(pc,   CT_ASSIGN     ) &&
       chunk_is_type(next, CT_SQUARE_OPEN) )
   {
      set_paren_parent(next, CT_ASSIGN);

      /* Mark one-liner assignment */
      tmp = next;
      while ((tmp = chunk_get_next_nc(tmp)) != nullptr)
      {
         if (chunk_is_newline(tmp)) { break; }

         if (chunk_is_type(tmp, CT_SQUARE_CLOSE) &&
             (next->level == tmp->level))
         {
            chunk_flags_set(tmp,  PCF_ONE_LINER);
            chunk_flags_set(next, PCF_ONE_LINER);
            break;
         }
      }
   }

   if (chunk_is_type(pc, CT_ASSERT))
   {
      handle_java_assert(pc);
   }
   if (chunk_is_type(pc, CT_ANNOTATION))
   {
      tmp = chunk_get_next_ncnl(pc);
      if (chunk_is_paren_open(tmp))
      {
         set_paren_parent(tmp, CT_ANNOTATION);
      }
   }

   /* A [] in C# and D only follows a type */
   if ((pc->type == CT_TSQUARE) &&
       (cpd.lang_flags & (LANG_D | LANG_CS | LANG_VALA)))
   {
      if (chunk_is_type(prev, CT_WORD)) { set_chunk_type(prev, CT_TYPE);          }
      if (chunk_is_type(next, CT_WORD)) { chunk_flags_set(next, PCF_VAR_1ST_DEF); }
   }

   if (chunk_is_type(pc, 3, CT_SQL_EXEC, CT_SQL_BEGIN, CT_SQL_END))
   {
      mark_exec_sql(pc);
   }

   if (chunk_is_type(pc, CT_PROTO_WRAP))
   {
      handle_proto_wrap(pc);
   }

   /* Handle the typedef */
   if (chunk_is_type(pc, CT_TYPEDEF))
   {
      fix_typedef(pc);
   }
   if (chunk_is_type(pc, 3, CT_ENUM, CT_STRUCT, CT_UNION))
   {
      if (chunk_is_not_type(prev, CT_TYPEDEF))
      {
         fix_enum_struct_union(pc);
      }
   }

   if (chunk_is_type(pc, CT_EXTERN))
   {
      if (chunk_is_paren_open(next))
      {
         tmp = flag_parens(next, 0, CT_NONE, CT_EXTERN, true);
         if (tmp && (tmp->type == CT_BRACE_OPEN))
         {
            set_paren_parent(tmp, CT_EXTERN);
         }
      }
      else
      {
         /* next likely is a string (see tokenize_cleanup.cpp) */
         set_chunk_parent(next, CT_EXTERN);
         tmp = chunk_get_next_ncnl(next);
         if (tmp && (tmp->type == CT_BRACE_OPEN))
         {
            set_paren_parent(tmp, CT_EXTERN);
         }
      }
   }

   if (chunk_is_type(pc, CT_TEMPLATE))
   {
      if (cpd.lang_flags & LANG_D) { handle_d_template  (pc); }
      else                         { handle_cpp_template(pc); }
   }

   if (chunk_is_type (pc,   CT_WORD      ) &&
       chunk_is_type (next, CT_ANGLE_OPEN) &&
       chunk_is_ptype(next, CT_TEMPLATE  ) )
   {
      mark_template_func(pc, next);
   }

   if (chunk_is_type(pc,   CT_SQUARE_CLOSE) &&
       chunk_is_type(next, CT_PAREN_OPEN  ) )
   {
      flag_parens(next, 0, CT_FPAREN_OPEN, CT_NONE, false);
   }

   if (chunk_is_type(pc, CT_TYPE_CAST))
   {
      fix_type_cast(pc);
   }

   if (chunk_is_ptype(pc,    CT_ASSIGN                    ) &&
       chunk_is_type (pc, 2, CT_BRACE_OPEN, CT_SQUARE_OPEN) )
   {
      /* Mark everything in here as in assign */
      flag_parens(pc, PCF_IN_ARRAY_ASSIGN, pc->type, CT_NONE, false);
   }

   if (chunk_is_type(pc, CT_D_TEMPLATE))
   {
      set_paren_parent(next, pc->type);
   }

   /**
    * A word before an open paren is a function call or definition.
    * CT_WORD => CT_FUNC_CALL or CT_FUNC_DEF
    */
   if (chunk_is_type(next, CT_PAREN_OPEN))
   {
      tmp = chunk_get_next_ncnl(next);
      if ((cpd.lang_flags & LANG_OC) &&
            chunk_is_type(tmp, CT_CARET))
      {
         handle_oc_block_type(tmp);

         // This is the case where a block literal is passed as the first argument of a C-style method invocation.
         assert(chunk_is_valid(tmp));
         if (chunk_is_type(tmp, CT_OC_BLOCK_CARET) &&
             chunk_is_type(pc,  CT_WORD          ) )
         {
            set_chunk_type(pc, CT_FUNC_CALL);
         }
      }
      else if (chunk_is_type(pc, 2, CT_WORD, CT_OPERATOR_VAL))
      {
         set_chunk_type(pc, CT_FUNCTION);
      }
      else if (chunk_is_type(pc, CT_TYPE))
      {
         /**
          * If we are on a type, then we are either on a C++ style cast, a
          * function or we are on a function type.
          * The only way to tell for sure is to find the close paren and see
          * if it is followed by an open paren.
          * "int(5.6)"
          * "int()"
          * "int(foo)(void)"
          *
          * FIXME: this check can be done better...
          */
         tmp = chunk_get_next_type(next, CT_PAREN_CLOSE, (int)next->level);
         tmp = chunk_get_next(tmp);
         if (chunk_is_type(tmp, CT_PAREN_OPEN) )
         {
            /* we have "TYPE(...)(" */
            set_chunk_type(pc, CT_FUNCTION);
         }
         else
         {
            if (chunk_is_ptype(pc, CT_NONE) &&
                ((pc->flags & PCF_IN_TYPEDEF) == 0))
            {
               tmp = chunk_get_next_ncnl(next);
               if (chunk_is_type(tmp, CT_PAREN_CLOSE) )
               {
                  /* we have TYPE() */
                  set_chunk_type(pc, CT_FUNCTION);
               }
               else
               {
                  /* we have TYPE(...) */
                  set_chunk_type  (pc,   CT_CPP_CAST);
                  set_paren_parent(next, CT_CPP_CAST);
               }
            }
         }
      }
      else if (chunk_is_type(pc, CT_ATTRIBUTE))
      {
         flag_parens(next, 0, CT_FPAREN_OPEN, CT_ATTRIBUTE, false);
      }
   }
   if (cpd.lang_flags & LANG_PAWN)
   {
      if (chunk_is_type(pc, CT_FUNCTION) &&
          (pc->brace_level > 0))
      {
         set_chunk_type(pc, CT_FUNC_CALL);
      }
      if (chunk_is_type(pc,   CT_STATE     ) &&
          chunk_is_type(next, CT_PAREN_OPEN) )
      {
         set_paren_parent(next, pc->type);
      }
   }
   else
   {
      if ( chunk_is_type (pc, CT_FUNCTION     ) &&
          (chunk_is_ptype(pc, CT_OC_BLOCK_EXPR) || !is_oc_block(pc)))
      {
         mark_function(pc);
      }
   }

   /* Detect C99 member stuff */
   if (chunk_is_type(pc,      CT_MEMBER              ) &&
       chunk_is_type(prev, 2, CT_COMMA, CT_BRACE_OPEN) )
   {
      set_chunk_type  (pc,   CT_C99_MEMBER);
      set_chunk_parent(next, CT_C99_MEMBER);
   }

   /* Mark function parens and braces */
   if (chunk_is_type(pc, 4, CT_FUNC_CALL, CT_FUNC_CALL_USER,
                            CT_FUNC_DEF,  CT_FUNC_PROTO) )
   {
      tmp = next;
      if (chunk_is_type(tmp, CT_SQUARE_OPEN))
      {
         tmp = set_paren_parent(tmp, pc->type);
      }
      else if (chunk_is_type (tmp, CT_TSQUARE ) ||
               chunk_is_ptype(tmp, CT_OPERATOR) )
      {
         tmp = chunk_get_next_ncnl(tmp);
      }

      if (chunk_is_valid(tmp))
      {
         if (chunk_is_paren_open(tmp))
         {
            tmp = flag_parens(tmp, 0, CT_FPAREN_OPEN, pc->type, false);
            if (chunk_is_valid(tmp))
            {
               if (chunk_is_type(tmp, CT_BRACE_OPEN))
               {
                  if ((tmp->parent_type != CT_DOUBLE_BRACE) &&
                      ((pc->flags & PCF_IN_CONST_ARGS) == 0))
                  {
                     set_paren_parent(tmp, pc->type);
                  }
               }
               else if (chunk_is_semicolon(tmp) && (pc->type == CT_FUNC_PROTO))
               {
                  set_chunk_parent(tmp, pc->type);
               }
            }
         }
      }
   }

   /* Mark the parameters in catch() */
   if (chunk_is_type(pc,   CT_CATCH      ) &&
       chunk_is_type(next, CT_SPAREN_OPEN) )
   {
      fix_fcn_def_params(next);
   }

   if (chunk_is_type(pc,   CT_THROW       ) &&
       chunk_is_type(prev, CT_FPAREN_CLOSE) )
   {
      set_chunk_parent(pc, prev->parent_type);
      if (chunk_is_type(next, CT_PAREN_OPEN))
      {
         set_paren_parent(next, CT_THROW);
      }
   }

   /* Mark the braces in: "for_each_entry(xxx) { }" */
   if (chunk_is_type(pc, CT_BRACE_OPEN          ) &&
       (pc->parent_type    != CT_DOUBLE_BRACE   ) &&
       chunk_is_type(prev, CT_FPAREN_CLOSE      ) &&
       ((prev->parent_type == CT_FUNC_CALL      ) ||
        (prev->parent_type == CT_FUNC_CALL_USER)) &&
       ((pc->flags & PCF_IN_CONST_ARGS) == 0))
   {
      set_paren_parent(pc, CT_FUNC_CALL);
   }

   /* Check for a close paren followed by an open paren, which means that
    * we are on a function type declaration (C/C++ only?).
    * Note that typedefs are already taken care of.
    */
   if (((pc->flags & (PCF_IN_TYPEDEF | PCF_IN_TEMPLATE)) == 0) &&
       (pc->parent_type != CT_CPP_CAST   ) &&
       (pc->parent_type != CT_C_CAST     ) &&
       ((pc->flags & PCF_IN_PREPROC) == 0) &&
       (!is_oc_block(pc)                 ) &&
       (pc->parent_type != CT_OC_MSG_DECL) &&
       (pc->parent_type != CT_OC_MSG_SPEC) &&
       chunk_is_str(pc, ")", 1           ) &&
       chunk_is_str(next, "(", 1         ) )
   {
      if (cpd.lang_flags & LANG_D)
      {
         flag_parens(next, 0, CT_FPAREN_OPEN, CT_FUNC_CALL, false);
      }
      else
      {
         mark_function_type(pc);
      }
   }

   if (chunk_is_type(pc, 2, CT_CLASS, CT_STRUCT) &&
       (pc->level == pc->brace_level           ) )
   {
      if (chunk_is_not_type(pc, CT_STRUCT) ||
          ((cpd.lang_flags & LANG_C) == 0) )
      {
         mark_class_ctor(pc);
      }
   }

   if (chunk_is_type(pc, CT_OC_CLASS )) { handle_oc_class(pc); }
   if (chunk_is_type(pc, CT_NAMESPACE)) { mark_namespace (pc); }

   /*TODO: Check for stuff that can only occur at the start of an statement */

   if ((cpd.lang_flags & LANG_D) == 0)
   {
      /**
       * Check a paren pair to see if it is a cast.
       * Note that SPAREN and FPAREN have already been marked.
       */
      if (chunk_is_type(pc, CT_PAREN_OPEN) &&
          (chunk_is_ptype(pc,   3, CT_NONE, CT_OC_MSG, CT_OC_BLOCK_EXPR)) &&
          (chunk_is_type (next, 8, CT_WORD,   CT_TYPE, CT_QUALIFIER, CT_STRUCT,
                                   CT_MEMBER, CT_ENUM, CT_DC_MEMBER, CT_UNION)) &&
           (prev->type != CT_SIZEOF) &&
           (prev->parent_type != CT_OPERATOR) &&
          ((pc->flags & PCF_IN_TYPEDEF) == 0))
      {
         fix_casts(pc);
      }
   }


   /* Check for stuff that can only occur at the start of an expression */
   if (pc->flags & PCF_EXPR_START)
   {
      /* Change STAR, MINUS, and PLUS in the easy cases */
      if (chunk_is_type(pc, CT_STAR))
      {
         // issue #596
         // [0x100062020:IN_SPAREN,IN_FOR,STMT_START,EXPR_START,PUNCTUATOR]
         // prev->type is CT_COLON ==> CT_DEREF
         if      (prev->type == CT_ANGLE_CLOSE) { set_chunk_type(pc, CT_PTR_TYPE); }
         else if (prev->type == CT_COLON      ) { set_chunk_type(pc, CT_DEREF);    }
         else                                   { set_chunk_type(pc, CT_DEREF);    }
      }
      if ((cpd.lang_flags & LANG_CPP       )&&
         chunk_is_type(pc,   CT_CARET      )&&
         chunk_is_type(prev, CT_ANGLE_CLOSE)) { set_chunk_type(pc, CT_PTR_TYPE);      }
      if (chunk_is_type(pc, CT_MINUS       )) { set_chunk_type(pc, CT_NEG);           }
      if (chunk_is_type(pc, CT_PLUS        )) { set_chunk_type(pc, CT_POS);           }
      if (chunk_is_type(pc, CT_INCDEC_AFTER)) { set_chunk_type(pc, CT_INCDEC_BEFORE); }
      if (chunk_is_type(pc, CT_AMP         )) { set_chunk_type(pc, CT_ADDR);          }

      if (chunk_is_type(pc, CT_CARET))
      {
         if (cpd.lang_flags & LANG_OC)
         {
            /* This is likely the start of a block literal */
            handle_oc_block_literal(pc);
         }
      }
   }

   /* Detect a variable definition that starts with struct/enum/union/class */
   if (((pc->flags & PCF_IN_TYPEDEF) == 0  ) &&
       (prev->parent_type != CT_CPP_CAST   ) &&
       ((prev->flags & PCF_IN_FCN_DEF) == 0) &&
       (chunk_is_type(pc, 4, CT_STRUCT, CT_UNION, CT_CLASS, CT_ENUM) ) )
   {
      tmp = skip_dc_member(next);
      if (chunk_is_type(tmp, 2, CT_TYPE, CT_WORD))
      {
         set_chunk_parent(tmp, pc->type);
         set_chunk_type  (tmp, CT_TYPE);

         tmp = chunk_get_next_ncnl(tmp);
      }
      if (chunk_is_type(tmp, CT_BRACE_OPEN) )
      {
         tmp = chunk_skip_to_match(tmp);
         tmp = chunk_get_next_ncnl(tmp);
      }
      if (chunk_is_ptr_operator(tmp ) ||
          chunk_is_type(tmp, CT_WORD) )
      {
         mark_variable_definition(tmp);
      }
   }

   /**
    * Change the paren pair after a function/macro-function
    * CT_PAREN_OPEN => CT_FPAREN_OPEN
    */
   if (chunk_is_type(pc, CT_MACRO_FUNC))
   {
      flag_parens(next, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_MACRO_FUNC, false);
   }

   if (chunk_is_type(pc, 3, CT_MACRO_OPEN, CT_MACRO_ELSE, CT_MACRO_CLOSE) )
   {
      if (chunk_is_type(next, CT_PAREN_OPEN))
      {
         flag_parens(next, 0, CT_FPAREN_OPEN, pc->type, false);
      }
   }

   if (chunk_is_type(pc,   CT_DELETE ) &&
       chunk_is_type(next, CT_TSQUARE) )
   {
      set_chunk_parent(next, CT_DELETE);
   }

   /* Change CT_STAR to CT_PTR_TYPE or CT_ARITH or CT_DEREF */
   if (chunk_is_type(pc, CT_STAR                             ) ||
       ((pc->type == CT_CARET) && (cpd.lang_flags & LANG_CPP)) )
   {
      if (chunk_is_paren_close(next) ||
         (chunk_is_type(next, CT_COMMA)))
      {
         set_chunk_type(pc, CT_PTR_TYPE);
      }
      else if ((cpd.lang_flags & LANG_OC) &&
               (chunk_is_type(next, CT_STAR)))
      {
         /* Change pointer-to-pointer types in OC_MSG_DECLs
          * from ARITH <===> DEREF to PTR_TYPE <===> PTR_TYPE */
         set_chunk_type  (pc, CT_PTR_TYPE);
         set_chunk_parent(pc, prev->parent_type);

         set_chunk_type  (next, CT_PTR_TYPE);
         set_chunk_parent(next, pc->parent_type);
      }
      else if ((pc->type == CT_STAR) && ((prev->type == CT_SIZEOF) || (prev->type == CT_DELETE)))
      {
         set_chunk_type(pc, CT_DEREF);
      }
      else if ((chunk_is_type(prev, CT_WORD) && chunk_ends_type(prev)) ||
                chunk_is_type(prev, 2, CT_DC_MEMBER, CT_PTR_TYPE)      )
      {
         set_chunk_type(pc, CT_PTR_TYPE);
      }
      else if (chunk_is_type(next, CT_SQUARE_OPEN) &&
               !(cpd.lang_flags & LANG_OC))                // issue # 408
      {
         set_chunk_type(pc, CT_PTR_TYPE);
      }
      else if (chunk_is_type(pc, CT_STAR))
      {
         /* most PCF_PUNCTUATOR chunks except a paren close would make this
          * a deref. A paren close may end a cast or may be part of a macro fcn.
          */
         if (chunk_is_type(prev, CT_TYPE))
         {
            set_chunk_type(pc, CT_PTR_TYPE);
         }
         else
         {
            set_chunk_type(pc,
                           ((prev->flags & PCF_PUNCTUATOR) &&
                            (!chunk_is_paren_close(prev) ||
                             (prev->parent_type == CT_MACRO_FUNC)) &&
                            (prev->type != CT_SQUARE_CLOSE) &&
                            (prev->type != CT_DC_MEMBER)) ? CT_DEREF : CT_ARITH);
         }
      }
   }

   if (chunk_is_type(pc, CT_AMP))
   {
      if      (chunk_is_type(prev, CT_DELETE)) { set_chunk_type(pc, CT_ADDR ); }
      else if (chunk_is_type(prev, CT_TYPE  )) { set_chunk_type(pc, CT_BYREF); }
      else if (chunk_is_type(next, 2, CT_FPAREN_CLOSE, CT_COMMA))
      {
         // fix the bug #654
         // connect(&mapper, SIGNAL(mapped(QString &)), this, SLOT(onSomeEvent(QString &)));
         set_chunk_type(pc, CT_BYREF);
      }
      else
      {
         set_chunk_type(pc, CT_ARITH);
         if (chunk_is_type(prev, CT_WORD))
         {
            tmp = chunk_get_prev_ncnl(prev);
            if (chunk_is_type(tmp, 4, CT_SEMICOLON,  CT_VSEMICOLON,
                                      CT_BRACE_OPEN, CT_QUALIFIER))
            {
               set_chunk_type(prev, CT_TYPE);
               set_chunk_type(pc,   CT_ADDR);
               chunk_flags_set(next, PCF_VAR_1ST);
            }
         }
      }
   }

   if (chunk_is_type(pc, 2, CT_MINUS, CT_PLUS ) )
   {
      if      (chunk_is_type(prev, 2, CT_POS, CT_NEG)) { set_chunk_type(pc, (chunk_is_type(pc, CT_MINUS)) ? CT_NEG : CT_POS); }
      else if (chunk_is_type(prev,    CT_OC_CLASS   )) { set_chunk_type(pc, (chunk_is_type(pc, CT_MINUS)) ? CT_NEG : CT_POS); }
      else                                             { set_chunk_type(pc, CT_ARITH); }
   }

   /* Bug # 634
    * Check for extern "C" NSString* i;
    * NSString is a type
    * change CT_WORD => CT_TYPE     for pc
    * change CT_STAR => CT_PTR_TYPE for pc-next */
   if (chunk_is_type(pc, CT_WORD))      // here NSString
   {
      if (chunk_is_type(pc->next, CT_STAR)) // here *
      {
         if (chunk_is_type(pc->prev, CT_STRING))
         {
            /* compare text with "C" to find extern "C" instructions */
            if (unc_text::compare(pc->prev->text(), "\"C\"") == 0)
            {
               if (chunk_is_type(pc->prev->prev, CT_EXTERN))
               {
                  set_chunk_type(pc,       CT_TYPE    ); // change CT_WORD => CT_TYPE
                  set_chunk_type(pc->next, CT_PTR_TYPE); // change CT_STAR => CT_PTR_TYPE
               }
            }
         }
         // Issue #322 STDMETHOD(GetValues)(BSTR bsName, REFDATA** pData);
         if (chunk_is_type(pc->next->next, CT_STAR))
         {
            // change CT_STAR => CT_PTR_TYPE
            set_chunk_type(pc->next,       CT_PTR_TYPE);
            set_chunk_type(pc->next->next, CT_PTR_TYPE);
         }
         // Issue #222 whatever3 *(func_ptr)( whatever4 *foo2, ...
         if (chunk_is_type(pc->next->next, CT_WORD) &&
             (pc->flags & PCF_IN_FCN_DEF     ) )
         {
            set_chunk_type(pc->next, CT_PTR_TYPE);
         }
      }
   }

   /* Bug # 634
    * Check for __attribute__((visibility ("default"))) NSString* i;
    * NSString is a type
    * change CT_WORD => CT_TYPE     for pc
    * change CT_STAR => CT_PTR_TYPE for pc-next */
   if (chunk_is_type(pc, CT_WORD))      // here NSString
   {
      if (chunk_is_type(pc->next, CT_STAR)) // here *
      {
         tmp = pc;
         while (chunk_is_valid(tmp))
         {
            if (chunk_is_type(tmp, CT_ATTRIBUTE))
            {
               LOG_FMT(LGUY, "ATTRIBUTE found %s:%s\n", get_token_name(tmp->type), tmp->text());
               LOG_FMT(LGUY, "for token %s:%s\n",       get_token_name(pc->type ), pc->text() );

               set_chunk_type(pc,       CT_TYPE    ); // change CT_WORD => CT_TYPE
               set_chunk_type(pc->next, CT_PTR_TYPE); // change CT_STAR => CT_PTR_TYPE
            }
            if (tmp->flags & PCF_STMT_START)
            {
               // we are at beginning of the line
               break;
            }
            tmp = chunk_get_prev(tmp);
         }
      }
   }
}


static void check_double_brace_init(chunk_t *bo1)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LJDBI, "%s: %zu:%zu", __func__, bo1->orig_line, bo1->orig_col);
   chunk_t *pc = chunk_get_prev_ncnl(bo1);
   if (chunk_is_paren_close(pc))
   {
      chunk_t *bo2 = chunk_get_next(bo1);
      if (chunk_is_type(bo2, CT_BRACE_OPEN))
      {
         /* found a potential double brace */
         chunk_t *bc2 = chunk_skip_to_match(bo2);
         chunk_t *bc1 = chunk_get_next     (bc2);
         if (chunk_is_type(bc1, CT_BRACE_CLOSE))
         {
            LOG_FMT(LJDBI, " - end %zu:%zu\n", bc2->orig_line, bc2->orig_col);
            /* delete bo2 and bc1 */
            assert(chunk_is_valid(bo2));
            bo1->str         += bo2->str;
            bo1->orig_col_end = bo2->orig_col_end;
            chunk_del(bo2);
            set_chunk_parent(bo1, CT_DOUBLE_BRACE);

            assert(chunk_is_valid(bc1));
            bc2->str         += bc1->str;
            bc2->orig_col_end = bc1->orig_col_end;
            chunk_del(bc1);
            set_chunk_parent(bc2, CT_DOUBLE_BRACE);
            return;
         }
      }
   }
   LOG_FMT(LJDBI, " - no\n");
}


void fix_symbols(void)
{
   LOG_FUNC_ENTRY();
   chunk_t *pc;
   chunk_t dummy;

   cpd.unc_stage = unc_stage_e::FIX_SYMBOLS;

   mark_define_expressions();

   bool is_java = (cpd.lang_flags & LANG_JAVA) != 0;   // forcing value to bool
   for (pc = chunk_get_head(); chunk_is_valid(pc); pc = chunk_get_next_ncnl(pc))
   {
      if (chunk_is_type(pc, 2, CT_FUNC_WRAP, CT_TYPE_WRAP))
      {
         handle_wrap(pc);
      }

      if (chunk_is_type(pc, CT_ASSIGN)) { mark_lvalue(pc); }

      if ((is_java == true) &&
          chunk_is_type(pc, CT_BRACE_OPEN)) { check_double_brace_init(pc); }
   }

   pc = chunk_get_head();
   if(chunk_is_comment_or_newline(pc))
   {
      pc = chunk_get_next_ncnl(pc);
   }
   while (chunk_is_valid(pc))
   {
      chunk_t *prev = chunk_get_prev_ncnl(pc, scope_e::PREPROC);
      if (chunk_is_invalid(prev)) { prev = &dummy; }

      chunk_t *next = chunk_get_next_ncnl(pc, scope_e::PREPROC);
      if (chunk_is_invalid(next)) { next = &dummy; }

      do_symbol_check(prev, pc, next);
      pc = chunk_get_next_ncnl(pc);
   }

   pawn_add_virtual_semicolons();
   process_returns();

   /* 2nd pass - handle variable definitions
    * REVISIT: We need function params marked to do this (?) */
   pc = chunk_get_head();
   int square_level = -1;
   while (chunk_is_valid(pc))
   {
      /* Can't have a variable definition inside [ ] */
      if (square_level < 0)
      {
         if (chunk_is_type(pc, CT_SQUARE_OPEN))
         {
            square_level = (int)pc->level;
         }
      }
      else
      {
         if (pc->level <= static_cast<size_t>(square_level))
         {
            square_level = -1;
         }
      }

      /* A variable definition is possible after at the start of a statement
       * that starts with: QUALIFIER, TYPE, or WORD */
      if ((square_level < 0          ) &&
          (pc->flags & PCF_STMT_START) &&
          (chunk_is_type(pc, 4, CT_QUALIFIER, CT_TYPE, CT_TYPENAME, CT_WORD)) &&
          (pc->parent_type != CT_ENUM) &&
          ((pc->flags & PCF_IN_ENUM) == 0))
      {
         pc = fix_var_def(pc);
      }
      else
      {
         pc = chunk_get_next_ncnl(pc);
      }
   }
}


static void mark_lvalue(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   if (pc->flags & PCF_IN_PREPROC) { return; }

   chunk_t *prev;
   for (prev = chunk_get_prev_ncnl(pc);
        chunk_is_valid(prev);
        prev = chunk_get_prev_ncnl(prev))
   {
      if ((prev->level < pc->level) ||
          chunk_is_type(prev, 3, CT_ASSIGN, CT_COMMA, CT_BOOL) ||
          chunk_is_semicolon(prev)  ||
          chunk_is_str(prev, "(", 1) ||
          chunk_is_str(prev, "{", 1) ||
          chunk_is_str(prev, "[", 1) ||
          (prev->flags & PCF_IN_PREPROC))
      {
         break;
      }
      chunk_flags_set(prev, PCF_LVALUE);
      if ((prev->level == pc->level) &&
          chunk_is_str(prev, "&", 1))
      {
         make_type(prev);
      }
   }
}


static void mark_function_return_type(chunk_t *fname, chunk_t *start, c_token_t parent_type)
{
   LOG_FUNC_ENTRY();
   assert(chunks_are_valid(fname, start));
   chunk_t *pc = start;

   if (chunk_is_valid(pc))
   {
      /* Step backwards from pc and mark the parent of the return type */
      LOG_FMT(LFCNR, "%s: (backwards) return type for '%s' @ %zu:%zu",
              __func__, fname->text(), fname->orig_line, fname->orig_col);

      chunk_t *first = pc;
      while (chunk_is_valid(pc))
      {
         if ((!chunk_is_var_type(pc) &&
               chunk_is_not_type(pc, 3, CT_OPERATOR, CT_WORD, CT_ADDR)) ||
             (pc->flags & PCF_IN_PREPROC))
         {
            break;
         }
         if (!chunk_is_ptr_operator(pc))
         {
            first = pc;
         }
         pc = chunk_get_prev_ncnl(pc);
      }

      pc = first;
      while (chunk_is_valid(pc))
      {
         LOG_FMT(LFCNR, " [%s|%s]", pc->text(), get_token_name(pc->type));

         if (parent_type != CT_NONE) { set_chunk_parent(pc, parent_type); }
         make_type(pc);
         if (pc == start) { break; }
         pc = chunk_get_next_ncnl(pc);
      }
      LOG_FMT(LFCNR, "\n");
   }
}


static bool mark_function_type(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   assert(chunk_is_valid(pc));
   LOG_FMT(LFTYPE, "%s: [%s] %s @ %zu:%zu\n", __func__,
           get_token_name(pc->type), pc->text(), pc->orig_line, pc->orig_col);

   size_t    star_count = 0;
   size_t    word_count = 0;
   chunk_t   *ptrcnk    = nullptr;
   chunk_t   *tmp;
   chunk_t   *apo;
   chunk_t   *apc;
   chunk_t   *aft;
   bool      anon = false;
   c_token_t pt, ptp;

   /* Scan backwards across the name, which can only be a word and single star */
   chunk_t *varcnk = chunk_get_prev_ncnl(pc);
   if (!chunk_is_word(varcnk))
   {
      if ((cpd.lang_flags & LANG_OC) &&
           chunk_is_str(varcnk, "^", 1) &&
          chunk_is_paren_open(chunk_get_prev_ncnl(varcnk)))
      {
         /* anonymous ObjC block type -- RTYPE (^)(ARGS) */
         anon = true;
      }
      else
      {
         assert(chunk_is_valid(varcnk));
         LOG_FMT(LFTYPE, "%s: not a word '%s' [%s] @ %zu:%zu\n",
                 __func__, varcnk->text(), get_token_name(varcnk->type),
                 varcnk->orig_line, varcnk->orig_col);
         goto nogo_exit;
      }
   }

   apo = chunk_get_next_ncnl(pc);
   apc = chunk_skip_to_match(apo);
   if (!chunk_is_paren_open(apo) ||
       ((apc = chunk_skip_to_match(apo)) == nullptr))
   {
      LOG_FMT(LFTYPE, "%s: not followed by parens\n", __func__);
      goto nogo_exit;
   }
   aft = chunk_get_next_ncnl(apc);
   if (chunk_is_type(aft, CT_BRACE_OPEN))
   {
      pt = CT_FUNC_DEF;
   }
   else if (chunk_is_type(aft, 2, CT_SEMICOLON, CT_ASSIGN))
   {
      pt = CT_FUNC_PROTO;
   }
   else
   {
      LOG_FMT(LFTYPE, "%s: not followed by '{' or ';'\n", __func__);
      goto nogo_exit;
   }
   ptp = (pc->flags & PCF_IN_TYPEDEF) ? CT_FUNC_TYPE : CT_FUNC_VAR;

   tmp = pc;
   while ((tmp = chunk_get_prev_ncnl(tmp)) != nullptr)
   {
      LOG_FMT(LFTYPE, " -- [%s] %s on line %zu, col %zu",
              get_token_name(tmp->type), tmp->text(),
              tmp->orig_line, tmp->orig_col);

      if (chunk_is_star(tmp                          ) ||
          chunk_is_type(tmp, 2, CT_PTR_TYPE, CT_CARET) )
      {
         star_count++;
         ptrcnk = tmp;
         LOG_FMT(LFTYPE, " -- PTR_TYPE\n");
      }
      else if (chunk_is_word(tmp                     ) ||
               chunk_is_type(tmp, 2, CT_WORD, CT_TYPE) )
      {
         word_count++;
         LOG_FMT(LFTYPE, " -- TYPE(%s)\n", tmp->text());
      }
      else if (chunk_is_type(tmp, CT_DC_MEMBER))
      {
         word_count = 0;
         LOG_FMT(LFTYPE, " -- :: reset word_count\n");
      }
      else if (chunk_is_str(tmp, "(", 1))
      {
         LOG_FMT(LFTYPE, " -- open paren (break)\n");
         break;
      }
      else
      {
         LOG_FMT(LFTYPE, " --  unexpected token [%s] %s on line %zu, col %zu\n",
                 get_token_name(tmp->type), tmp->text(),
                 tmp->orig_line, tmp->orig_col);
         goto nogo_exit;
      }
   }

   if ((star_count > 1) ||
       (word_count > 1) ||
       ((star_count + word_count) == 0))
   {
      LOG_FMT(LFTYPE, "%s: bad counts word:%zu, star:%zu\n", __func__,
              word_count, star_count);
      goto nogo_exit;
   }

   /* make sure what appears before the first open paren can be a return type */
   if (!chunk_ends_type(chunk_get_prev_ncnl(tmp)))
   {
      goto nogo_exit;
   }

   if (ptrcnk)
   {
      set_chunk_type(ptrcnk, CT_PTR_TYPE);
   }
   if (!anon)
   {
      if (pc->flags & PCF_IN_TYPEDEF)
      {
         set_chunk_type(varcnk, CT_TYPE);
      }
      else
      {
         set_chunk_type(varcnk, CT_FUNC_VAR);
         chunk_flags_set(varcnk, PCF_VAR_1ST_DEF);
      }
   }
   set_chunk_type(pc, CT_TPAREN_CLOSE);
   set_chunk_parent(pc, ptp);

   set_chunk_type    (apo, CT_FPAREN_OPEN);
   set_chunk_parent  (apo, pt);
   set_chunk_type    (apc, CT_FPAREN_CLOSE);
   set_chunk_parent  (apc, pt);
   fix_fcn_def_params(apo);

   if (chunk_is_semicolon(aft))
   {
      assert(chunk_is_valid(aft));
      set_chunk_parent(aft, (aft->flags & PCF_IN_TYPEDEF) ? CT_TYPEDEF : CT_FUNC_VAR);
   }
   else if (chunk_is_type(aft, CT_BRACE_OPEN))
   {
      flag_parens(aft, 0, CT_NONE, pt, false);
   }

   /* Step backwards to the previous open paren and mark everything a
    */
   tmp = pc;
   while ((tmp = chunk_get_prev_ncnl(tmp)) != nullptr)
   {
      LOG_FMT(LFTYPE, " ++ [%s] %s on line %zu, col %zu\n",
              get_token_name(tmp->type), tmp->text(),
              tmp->orig_line, tmp->orig_col);

      if (*tmp->str.c_str() == '(')
      {
         if ((pc->flags & PCF_IN_TYPEDEF) == 0)
         {
            chunk_flags_set(tmp, PCF_VAR_1ST_DEF);
         }
         set_chunk_type(tmp, CT_TPAREN_OPEN);
         set_chunk_parent(tmp, ptp);

         tmp = chunk_get_prev_ncnl(tmp);
         if (chunk_is_type(tmp, 5, CT_FUNCTION, CT_FUNC_CALL,
                CT_FUNC_CALL_USER, CT_FUNC_DEF, CT_FUNC_PROTO))
         {
            set_chunk_type(tmp, CT_TYPE);
            chunk_flags_clr(tmp, PCF_VAR_1ST_DEF);
         }
         mark_function_return_type(varcnk, tmp, ptp);
         break;
      }
   }
   return(true);

nogo_exit:
   tmp = chunk_get_next_ncnl(pc);
   if (chunk_is_paren_open(tmp))
   {
      assert(chunk_is_valid(tmp));
      LOG_FMT(LFTYPE, "%s:%d setting FUNC_CALL on %zu:%zu\n",
              __func__, __LINE__, tmp->orig_line, tmp->orig_col);
      flag_parens(tmp, 0, CT_FPAREN_OPEN, CT_FUNC_CALL, false);
   }
   return(false);
}


static void process_returns(void)
{
   LOG_FUNC_ENTRY();
   chunk_t *pc = chunk_get_head();
   while (chunk_is_valid(pc))
   {
      if ((pc->type != CT_RETURN) ||
          (pc->flags & PCF_IN_PREPROC))
      {
         pc = chunk_get_next_type(pc, CT_RETURN, -1);
         continue;
      }
      pc = process_return(pc);
   }
}


static chunk_t *process_return(chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   /* grab next and bail if it is a semicolon */
   chunk_t *next = chunk_get_next_ncnl(pc);
   if (chunk_is_invalid    (next) ||
        chunk_is_semicolon(next) )
   {
      return(next);
   }

   if (cpd.settings[UO_nl_return_expr].a != AV_IGNORE)
   {
      newline_iarf(pc, cpd.settings[UO_nl_return_expr].a);
   }

   chunk_t *temp;
   chunk_t *semi;
   chunk_t *cpar;
   chunk_t chunk;
   if (chunk_is_type(next, CT_PAREN_OPEN))
   {
      /* See if the return is fully paren'd */
      cpar = chunk_get_next_type(next, CT_PAREN_CLOSE, (int)next->level);
      semi = chunk_get_next_ncnl(cpar);
      assert(chunk_is_valid(semi));
      if (chunk_is_semicolon(semi))
      {
         if (cpd.settings[UO_mod_paren_on_return].a == AV_REMOVE)
         {
            LOG_FMT(LRETURN, "%s: removing parens on line %zu\n",
                    __func__, pc->orig_line);

            /* lower the level of everything */
            for (temp = next; temp != cpar; temp = chunk_get_next(temp))
            {
               temp->level--;
            }

            /* delete the parens */
            chunk_del(next);
            chunk_del(cpar);

            /* back up the semicolon */
            semi->column--;
            semi->orig_col--;
            semi->orig_col_end--;
         }
         else
         {
            LOG_FMT(LRETURN, "%s: keeping parens on line %zu\n",
                    __func__, pc->orig_line);

            /* mark & keep them */
            set_chunk_parent(next, CT_RETURN);
            set_chunk_parent(cpar, CT_RETURN);
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
      if ((chunk_is_semicolon(semi) &&
          (pc->level == semi->level)) ||
          (semi->level < pc->level))
      {
         break;
      }
   }
   assert(chunk_is_valid(semi));
   if ((chunk_is_semicolon(semi)) &&
       (pc->level == semi->level) )
   {
      /* add the parens */
      chunk.type        = CT_PAREN_OPEN;
      chunk.str         = "(";
      chunk.level       = pc->level;
      chunk.brace_level = pc->brace_level;
      chunk.orig_line   = pc->orig_line;
      chunk.parent_type = CT_RETURN;
      chunk.flags       = pc->flags & PCF_COPY_FLAGS;
      chunk_add_before(&chunk, next);

      chunk.type      = CT_PAREN_CLOSE;
      chunk.str       = ")";

      assert(chunk_is_valid(semi));
      chunk.orig_line = semi->orig_line;
      cpar            = chunk_add_before(&chunk, semi);

      LOG_FMT(LRETURN, "%s: added parens on line %zu\n",
              __func__, pc->orig_line);

      for (temp = next; temp != cpar; temp = chunk_get_next(temp))
      {
         temp->level++;
      }
   }
   return(semi);
}


static bool is_ucase_str(const char *str, size_t len)
{
   while (len-- > 0)
   {
      if (unc_toupper(*str) != *str)
      {
         return(false);
      }
      str++;
   }
   return(true);
}


static bool is_oc_block(chunk_t *pc)
{
   return(chunk_is_ptype(pc, 4, CT_OC_BLOCK_TYPE, CT_OC_BLOCK_EXPR,
                                      CT_OC_BLOCK_ARG,  CT_OC_BLOCK   ) ||
           chunk_is_type(pc,       CT_OC_BLOCK_CARET) ||
           chunk_is_type(pc->next, CT_OC_BLOCK_CARET) ||
           chunk_is_type(pc->prev, CT_OC_BLOCK_CARET));
}


static void fix_casts(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   return_if_invalid(start);

   LOG_FMT(LCASTS, "%s:line %zu, col %zu:", __func__, start->orig_line, start->orig_col);

   chunk_t *prev = chunk_get_prev_ncnl(start);
   if (chunk_is_type(prev, CT_PP_DEFINED))
   {
      LOG_FMT(LCASTS, " -- not a cast - after defined\n");
      return;
   }

   chunk_t    *first;
   chunk_t    *after;
   chunk_t    *last = nullptr;
   chunk_t    *paren_close;
   const char *verb      = "likely";
   const char *detail    = "";
   size_t     count      = 0;
   int        word_count = 0;
   bool       nope;
   bool       doubtful_cast = false;

   /* Make sure there is only WORD, TYPE, and '*' or '^' before the close paren */
   chunk_t *pc = chunk_get_next_ncnl(start);
   first = pc;
   while (chunk_is_var_type(pc) ||
          chunk_is_type(pc, 7, CT_WORD, CT_QUALIFIER, CT_DC_MEMBER,
                 CT_STAR, CT_CARET, CT_TSQUARE, CT_AMP))
   {
      LOG_FMT(LCASTS, " [%s]", get_token_name(pc->type));

      if      (chunk_is_type(pc, CT_WORD     )) { word_count++; }
      else if (chunk_is_type(pc, CT_DC_MEMBER)) { word_count--; }

      last = pc;
      pc   = chunk_get_next_ncnl(pc);
      count++;
   }

   assert(chunk_is_valid(prev));
   if (chunk_is_not_type(pc,   CT_PAREN_CLOSE) ||
       chunk_is_type    (prev, CT_OC_CLASS   ) )
   {
      LOG_FMT(LCASTS, " -- not a cast, hit [%s]\n",
              (chunk_is_invalid(pc)) ? "nullptr"  : get_token_name(pc->type));
      return;
   }

   if (word_count > 1)
   {
      LOG_FMT(LCASTS, " -- too many words: %d\n", word_count);
      return;
   }
   paren_close = pc;

   if (chunk_is_invalid(last)) { return; }

   /* If last is a type or star/caret, we have a cast for sure */
   if (chunk_is_type(last, 4, CT_STAR, CT_CARET, CT_PTR_TYPE, CT_TYPE) )
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
      else if ((cpd.lang_flags & LANG_OC) &&
                 chunk_is_str(last, "id", 2))
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
       * For this to be a cast, the close paren must be followed by:
       *  - constant (number or string)
       *  - paren open
       *  - word
       *
       * Find the next non-open paren item. */
      pc    = chunk_get_next_ncnl(paren_close);
      after = pc;
      do
      {
         after = chunk_get_next_ncnl(after);
      } while (chunk_is_type(after, CT_PAREN_OPEN));

      if (chunk_is_invalid(after))
      {
         LOG_FMT(LCASTS, " -- not a cast - hit nullptr\n");
         return;
      }

      assert(chunk_is_valid(pc));
      nope = false;
      if (chunk_is_ptr_operator(pc))
      {
         /* star (*) and address (&) are ambiguous */
         if (chunk_is_type(after, 3, CT_NUMBER_FP, CT_NUMBER, CT_STRING) ||
             doubtful_cast                 )
         {
            nope = true;
         }
      }
      else if (chunk_is_type(pc, CT_MINUS))
      {
         /* (UINT8)-1 or (foo)-1 or (FOO)-'a' */
         if (chunk_is_type(after, CT_STRING) ||
              doubtful_cast)
         {
            nope = true;
         }
      }
      else if (chunk_is_type(pc, CT_PLUS))
      {
         /* (UINT8)+1 or (foo)+1 */
         if (((after->type != CT_NUMBER   ) &&
              (after->type != CT_NUMBER_FP)) || doubtful_cast)
         {
            nope = true;
         }
      }
      else if ((pc->type != CT_NUMBER_FP     ) &&
               (pc->type != CT_NUMBER        ) &&
               (pc->type != CT_WORD          ) &&
               (pc->type != CT_THIS          ) &&
               (pc->type != CT_TYPE          ) &&
               (pc->type != CT_PAREN_OPEN    ) &&
               (pc->type != CT_STRING        ) &&
               (pc->type != CT_SIZEOF        ) &&
               (pc->type != CT_FUNC_CALL     ) &&
               (pc->type != CT_FUNC_CALL_USER) &&
               (pc->type != CT_FUNCTION      ) &&
               (pc->type != CT_BRACE_OPEN    ) &&
               (!((pc->type == CT_SQUARE_OPEN) &&
                  (cpd.lang_flags & LANG_OC  ))))
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

   /* if the 'cast' is followed by a semicolon, comma or close paren, it isn't */
   pc = chunk_get_next_ncnl(paren_close);
   if (chunk_is_semicolon(pc)      ||
       chunk_is_type(pc, CT_COMMA) ||
       chunk_is_paren_close(pc)    )
   {
      assert(chunk_is_valid(pc));
      LOG_FMT(LCASTS, " -- not a cast - followed by %s\n", get_token_name(pc->type));
      return;
   }

   set_chunk_parent(start, CT_C_CAST);
   set_chunk_parent(paren_close, CT_C_CAST);

   LOG_FMT(LCASTS, " -- %s c-cast: (", verb);

   for (pc = first; pc != paren_close; pc = chunk_get_next_ncnl(pc))
   {
      assert(chunk_is_valid(pc));
      set_chunk_parent(pc, CT_C_CAST);
      make_type(pc);
      LOG_FMT(LCASTS, " %s", pc->text());
   }
   LOG_FMT(LCASTS, " )%s\n", detail);

   /* Mark the next item as an expression start */
   pc = chunk_get_next_ncnl(paren_close);
   if (chunk_is_valid(pc))
   {
      chunk_flags_set(pc, PCF_EXPR_START);
      if (chunk_is_opening_brace(pc))
      {
         set_paren_parent(pc, start->parent_type);
      }
   }
}


static void fix_type_cast(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   chunk_t *pc;

   pc = chunk_get_next_ncnl(start);
   if ((chunk_is_invalid(pc)) ||
       (pc->type != CT_ANGLE_OPEN))
   {
      return;
   }

   while (((pc = chunk_get_next_ncnl(pc)) != nullptr) &&
          (pc->level >= start->level))
   {
      if ((pc->level == start->level) && (pc->type == CT_ANGLE_CLOSE))
      {
         pc = chunk_get_next_ncnl(pc);
         if (chunk_is_str(pc, "(", 1))
         {
            set_paren_parent(pc, CT_TYPE_CAST);
         }
         return;
      }
      make_type(pc);
   }
}


static void fix_enum_struct_union(chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   if ( (pc == nullptr               ) || /* invalid parameter */
        (pc->parent_type == CT_C_CAST) )  /* Make sure this wasn't a cast */
   {
      return;
   }

   chunk_t *prev        = nullptr;
   size_t  flags        = PCF_VAR_1ST_DEF;
   size_t  in_fcn_paren = pc->flags & PCF_IN_FCN_DEF;

   /* the next item is either a type or open brace */
   chunk_t *next = chunk_get_next_ncnl(pc);
   // the enum-key might be enum, enum class or enum struct (TODO)
   if (chunk_is_type(next, CT_ENUM_CLASS))
   {
      // get the next one
      next = chunk_get_next_ncnl(next);
   }
   /* the next item is either a type, an attribute (TODO), an identifier, a colon or open brace */
   if (chunk_is_type(next, CT_TYPE) )
   {
      // i.e. "enum xyz : unsigned int { ... };"
      // i.e. "enum class xyz : unsigned int { ... };"
      // xyz is a type
      set_chunk_parent(next, pc->type);
      prev = next;
      next = chunk_get_next_ncnl(next);
//  \todo is this needed ? set_chunk_parent(next, pc->type);

      /* next up is either a colon, open brace, or open paren (pawn) */
      if (chunk_is_invalid(next)) { return; }

      else if ((cpd.lang_flags & LANG_PAWN) &&
               (chunk_is_type(next, CT_PAREN_OPEN)))
      {
         next = set_paren_parent(next, CT_ENUM);
      }
      else if ((pc->type   == CT_ENUM ) &&
               (chunk_is_type(next, CT_COLON) ))
      {
         /* enum TYPE : INT_TYPE { ... }; */
         next = chunk_get_next_ncnl(next);
         if (next)
         {
            make_type(next);
            next = chunk_get_next_ncnl(next);
         }
      }
   }
   if (chunk_is_type(next, CT_BRACE_OPEN))
   {
      /* \todo which function is the right one? */
//      flag_series(pc, next, (pc->type == CT_ENUM) ? PCF_IN_ENUM : PCF_IN_STRUCT);
      flag_parens(next, (pc->type == CT_ENUM) ? PCF_IN_ENUM : PCF_IN_STRUCT,
                  CT_NONE, CT_NONE, false);

      if (chunk_is_type(pc, 2, CT_UNION, CT_STRUCT) )
      {
         mark_struct_union_body(next);
      }

      /* Skip to the closing brace */
      set_chunk_parent(next, pc->type);
      next   = chunk_get_next_type(next, CT_BRACE_CLOSE, (int)pc->level);
      flags |= PCF_VAR_INLINE;
      if (chunk_is_valid(next))
      {
         set_chunk_parent(next, pc->type);
         next = chunk_get_next_ncnl(next);
      }
      prev = nullptr;
   }
   /* reset var name parent type */
   else if (next && prev)
   {
      set_chunk_parent(prev, CT_NONE);
   }

   if (chunk_is_type(next, CT_PAREN_CLOSE) )
   {
      return;
   }

   if (!chunk_is_semicolon(next))
   {
      /* Pawn does not require a semicolon after an enum */
      if (cpd.lang_flags & LANG_PAWN) { return; }

      /* D does not require a semicolon after an enum, but we add one to make
       * other code happy.*/
      if (cpd.lang_flags & LANG_D)
      {
         next = pawn_add_vsemi_after(chunk_get_prev_ncnl(next));
      }
   }

   /* We are either pointing to a ';' or a variable */
   while (chunk_is_not_type(next, CT_ASSIGN) &&
         !chunk_is_semicolon(next) &&
          ((in_fcn_paren ^ (next->flags & PCF_IN_FCN_DEF)) == 0))
   {
      if (next->level == pc->level)
      {
         if (chunk_is_type(next, CT_WORD))
         {
            chunk_flags_set(next, flags);
            flags &= ~PCF_VAR_1ST;       /* clear the first flag for the next items */
         }

         if (next->type == CT_STAR ||
             ((cpd.lang_flags & LANG_CPP) &&
              (chunk_is_type(next, CT_CARET))))
         {
            set_chunk_type(next, CT_PTR_TYPE);
         }

         /* If we hit a comma in a function param, we are done */
         if (((next->type == CT_COMMA       ) ||
              (next->type == CT_FPAREN_CLOSE) ) &&
             (next->flags & (PCF_IN_FCN_DEF | PCF_IN_FCN_CALL)))
         {
            return;
         }
      }

      next = chunk_get_next_ncnl(next);
   }

   if (chunk_is_type(next, CT_SEMICOLON) &&
       chunk_is_invalid(prev           ) )
   {
      set_chunk_parent(next, pc->type);
   }
}


static void fix_typedef(chunk_t *start)
{
   LOG_FUNC_ENTRY();

   if (chunk_is_invalid(start)) { return; }

   LOG_FMT(LTYPEDEF, "%s: typedef @ %zu:%zu\n", __func__, start->orig_line, start->orig_col);

   chunk_t *the_type = nullptr;
   chunk_t *last_op  = nullptr;
   chunk_t *open_paren;

   /* Mark everything in the typedef and scan for ")(", which makes it a
    * function type */
   chunk_t *next = start;
   while (((next = chunk_get_next_ncnl(next, scope_e::PREPROC)) != nullptr) &&
          (next->level >= start->level))
   {
      chunk_flags_set(next, PCF_IN_TYPEDEF);
      if (start->level == next->level)
      {
         if (chunk_is_semicolon(next))
         {
            set_chunk_parent(next, CT_TYPEDEF);
            break;
         }
         if (chunk_is_type(next, CT_ATTRIBUTE))
         {
            break;
         }
         if ((cpd.lang_flags & LANG_D) &&
             (chunk_is_type(next, CT_ASSIGN) ))
         {
            set_chunk_parent(next, CT_TYPEDEF);
            break;
         }
         make_type(next);
         if (chunk_is_type(next, CT_TYPE))
         {
            the_type = next;
         }
         chunk_flags_clr(next, PCF_VAR_1ST_DEF);
         if (*next->str.c_str() == '(')
         {
            last_op = next;
         }
      }
   }

   /* avoid interpreting typedef NS_ENUM (NSInteger, MyEnum) as a function def */
   if (last_op && !((cpd.lang_flags & LANG_OC) &&
                    (last_op->parent_type == CT_ENUM)))
   {
      flag_parens(last_op, 0, CT_FPAREN_OPEN, CT_TYPEDEF, false);
      fix_fcn_def_params(last_op);

      open_paren = nullptr;
      the_type   = chunk_get_prev_ncnl(last_op, scope_e::PREPROC);
      if (chunk_is_paren_close(the_type))
      {
         open_paren = chunk_skip_to_match_rev(the_type);
         mark_function_type(the_type);
         the_type = chunk_get_prev_ncnl(the_type, scope_e::PREPROC);
      }
      else
      {
         /* must be: "typedef <return type>func(params);" */
         set_chunk_type(the_type, CT_FUNC_TYPE);
      }
      set_chunk_parent(the_type, CT_TYPEDEF);

      assert(chunk_is_valid(the_type));
      LOG_FMT(LTYPEDEF, "%s: fcn typedef [%s] on line %zu\n",
              __func__, the_type->text(), the_type->orig_line);

      /* If we are aligning on the open paren, grab that instead */
      if (open_paren && (cpd.settings[UO_align_typedef_func].u == 1))
      {
         the_type = open_paren;
      }
      if (cpd.settings[UO_align_typedef_func].u != 0)
      {
         LOG_FMT(LTYPEDEF, "%s:  -- align anchor on [%s] @ %zu:%zu\n",
                 __func__, the_type->text(), the_type->orig_line, the_type->orig_col);
         chunk_flags_set(the_type, PCF_ANCHOR);
      }

      /* already did everything we need to do */
      return;
   }

   /* Skip over enum/struct/union stuff, as we know it isn't a return type
    * for a function type */
   next = chunk_get_next_ncnl(start, scope_e::PREPROC);
   if (chunk_is_invalid(next)) { return; }

   if ((next->type != CT_ENUM  ) &&
       (next->type != CT_STRUCT) &&
       (next->type != CT_UNION ) )
   {
      if (chunk_is_valid(the_type))
      {
         /* We have just a regular typedef */
         LOG_FMT(LTYPEDEF, "%s: regular typedef [%s] on line %zu\n",
                 __func__, the_type->text(), the_type->orig_line);
         chunk_flags_set(the_type, PCF_ANCHOR);
      }
      return;
   }

   /* We have a struct/union/enum type, set the parent */
   c_token_t tag = next->type;

   /* the next item should be either a type or { */
   next = chunk_get_next_ncnl(next, scope_e::PREPROC);
   if (chunk_is_invalid(next)) { return; }

   if (chunk_is_type(next, CT_TYPE))
   {
      next = chunk_get_next_ncnl(next, scope_e::PREPROC);
   }
   if (chunk_is_invalid(next)) { return; }

   if (chunk_is_type(next, CT_BRACE_OPEN))
   {
      set_chunk_parent(next, tag);
      /* Skip to the closing brace */
      next = chunk_get_next_type(next, CT_BRACE_CLOSE, (int)next->level, scope_e::PREPROC);
      if (chunk_is_valid(next)) { set_chunk_parent(next, tag); }
   }

   if (chunk_is_valid(the_type))
   {
      LOG_FMT(LTYPEDEF, "%s: %s typedef [%s] on line %zu\n",
              __func__, get_token_name(tag), the_type->text(), the_type->orig_line);
      chunk_flags_set(the_type, PCF_ANCHOR);
   }
}


static bool cs_top_is_question(const ChunkStack &cs, size_t level)
{
#if 0
   chunk_t *pc = cs.Empty() ? nullptr : cs.Top()->m_pc;
#else
   chunk_t *pc;
   if(cs.Empty()) { pc = nullptr;        }
   else           { pc = cs.Top()->m_pc; }
#endif

   return(chunk_is_type(pc, CT_QUESTION) &&
          (pc->level == level      ) );
}


void combine_labels(void)
{
   LOG_FUNC_ENTRY();

   chunk_t *tmp;
   bool    hit_case  = false;
   bool    hit_class = false;

   cpd.unc_stage = unc_stage_e::COMBINE_LABELS;

   // need a stack to handle nesting inside of OC messages, which reset the scope
   ChunkStack cs;

   chunk_t *prev = chunk_get_head();
   chunk_t *cur  = chunk_get_next_nc(prev);
   chunk_t *next = chunk_get_next_nc(cur);

   /* unlikely that the file will start with a label... */
   while (chunk_is_valid(next))
   {
#ifdef DEBUG
      LOG_FMT(LGUY, "(%d) ", __LINE__);
#endif

      assert(chunk_is_valid(cur));
      if      (cur->type == CT_NEWLINE     ) { LOG_FMT(LGUY, "%s: %zu:%zu CT_NEWLINE\n",      __func__, cur->orig_line, cur->orig_col); }
      else if (cur->type == CT_VBRACE_OPEN ) { LOG_FMT(LGUY, "%s: %zu:%zu CT_VBRACE_OPEN\n",  __func__, cur->orig_line, cur->orig_col); }
      else if (cur->type == CT_VBRACE_CLOSE) { LOG_FMT(LGUY, "%s: %zu:%zu CT_VBRACE_CLOSE\n", __func__, cur->orig_line, cur->orig_col); }
      else                                   { LOG_FMT(LGUY, "%s: %zu:%zu %s\n",              __func__, cur->orig_line, cur->orig_col, cur->text()); }

      if (!(next->flags & PCF_IN_OC_MSG) && /* filter OC case of [self class] msg send */
          (chunk_is_type(next, 3, CT_CLASS, CT_OC_CLASS, CT_TEMPLATE) ) )
      {
         hit_class = true;
      }
      if (chunk_is_semicolon(next) ||
         (chunk_is_type(next, CT_BRACE_OPEN)))
      {
         hit_class = false;
      }

      if (chunk_is_type_and_ptype(prev, CT_SQUARE_OPEN, CT_OC_MSG))
      {
         cs.Push_Back(prev);
      }
      else if (chunk_is_type_and_ptype(next, CT_SQUARE_CLOSE, CT_OC_MSG))
      {
         /* pop until we hit '[' */
         while (!cs.Empty())
         {
            chunk_t *t2 = cs.Top()->m_pc; /*lint !e613 */
            cs.Pop_Back();
            if (chunk_is_type(t2, CT_SQUARE_OPEN)) { break; }
         }
      }

      if (chunk_is_type(next, CT_QUESTION))
      {
         cs.Push_Back(next);
      }
      else if (chunk_is_type(next, CT_CASE))
      {
         if (chunk_is_type(cur, CT_GOTO))
         {
            /* handle "goto case x;" */
            set_chunk_type(next, CT_QUALIFIER);
         }
         else
         {
            hit_case = true;
         }
      }
      else if ( chunk_is_type(next, 2, CT_COLON, CT_OC_COLON) &&
                cs_top_is_question(cs, next->level)           )
      {
         if (chunk_is_type(cur, CT_DEFAULT))
         {
            set_chunk_type(cur, CT_CASE);
            hit_case = true;
         }
         if (cs_top_is_question(cs, next->level))
         {
            set_chunk_type(next, CT_COND_COLON);
            cs.Pop_Back();
         }
         else if (hit_case)
         {
            hit_case = false;
            set_chunk_type(next, CT_CASE_COLON);
            tmp = chunk_get_next_ncnl(next);
            if (chunk_is_type(tmp, CT_BRACE_OPEN) )
            {
               set_chunk_parent(tmp, CT_CASE);
               tmp = chunk_get_next_type(tmp, CT_BRACE_CLOSE, (int)tmp->level);
               if (chunk_is_valid(tmp))
               {
                  set_chunk_parent(tmp, CT_CASE);
               }
            }
         }
         else
         {
            chunk_t *nextprev = chunk_get_prev_ncnl(next);

            if (cpd.lang_flags & LANG_PAWN)
            {
               if (chunk_is_type(cur, 2, CT_WORD, CT_BRACE_CLOSE) )
               {
                  c_token_t new_type = CT_TAG;

                  tmp = chunk_get_next_nc(next);
                  if (chunk_is_newline(prev) &&
                      chunk_is_newline(tmp ) )
                  {
                     new_type = CT_LABEL;
                     set_chunk_type(next, CT_LABEL_COLON);
                  }
                  else
                  {
                     set_chunk_type(next, CT_TAG_COLON);
                  }
                  if (chunk_is_type(cur, CT_WORD))
                  {
                     set_chunk_type(cur, new_type);
                  }
               }
            }
            else if (next->flags & PCF_IN_ARRAY_ASSIGN) { set_chunk_type(next, CT_D_ARRAY_COLON); }
            else if (next->flags & PCF_IN_FOR         ) { set_chunk_type(next, CT_FOR_COLON    ); }
            else if (next->flags & PCF_OC_BOXED       ) { set_chunk_type(next, CT_OC_DICT_COLON); }
            else if (chunk_is_type(cur, CT_WORD))
            {
               tmp = chunk_get_next_nc(next, scope_e::PREPROC);
               assert(chunk_is_valid(tmp));

               LOG_FMT(LGUY, "%s: %zu:%zu, tmp=%s\n", __func__, tmp->orig_line,
                       tmp->orig_col, (tmp->type == CT_NEWLINE) ? "<NL>" : tmp->text());
               log_pcf_flags(LGUY, tmp->flags);
               if (next->flags & PCF_IN_FCN_CALL)
               {
                  /* Must be a macro thingy, assume some sort of label */
                  set_chunk_type(next, CT_LABEL_COLON);
               }
               else if (((tmp->type != CT_NUMBER ) &&
                         (tmp->type != CT_SIZEOF ) &&
                        !(tmp->flags & (PCF_IN_STRUCT | PCF_IN_CLASS))) ||
                         (chunk_is_type(tmp, CT_NEWLINE) ))
               {
                  /* the CT_SIZEOF isn't great - test 31720 happens to use a sizeof expr,
                   * but this really should be able to handle any constant expr */
                  set_chunk_type(cur,  CT_LABEL);
                  set_chunk_type(next, CT_LABEL_COLON);
               }
               else if (next->flags & (PCF_IN_STRUCT | PCF_IN_CLASS | PCF_IN_TYPEDEF))
               {
                  set_chunk_type(next, CT_BIT_COLON);

                  tmp = chunk_get_next(next);
                  while ((tmp = chunk_get_next(tmp)) != nullptr)
                  {
                     if (chunk_is_type(tmp, CT_SEMICOLON)) { break; }
                     if (tmp->type == CT_COLON    ) { set_chunk_type(tmp, CT_BIT_COLON); }
                  }
               }
            }
            else if (chunk_is_type(nextprev, CT_FPAREN_CLOSE)) { set_chunk_type(next, CT_CLASS_COLON); /* it's a class colon */ }
            else if (chunk_is_type(cur, CT_TYPE))              { set_chunk_type(next, CT_BIT_COLON); }
            else if (chunk_is_type(cur, 3, CT_ENUM, CT_PRIVATE, CT_QUALIFIER) ||
                     (cur->parent_type == CT_ALIGN))             { /* ignore it - bit field, align or public/private, etc */ }
            else if ((cur->type == CT_ANGLE_CLOSE) || hit_class) { /* ignore it - template thingy */ }
            else if (cur->parent_type == CT_SQL_EXEC)            { /* ignore it - SQL variable name */ }
            else if (next->parent_type == CT_ASSERT)             { /* ignore it - Java assert thing */ }
            else if (next->level > next->brace_level)            { /* ignore it, as it is inside a paren */ }
            else
            {
               tmp = chunk_get_next_ncnl(next);
               if (chunk_is_type(tmp, 2, CT_BASE, CT_THIS))
               {
                  /* ignore it, as it is a C# base thingy */
               }
               else
               {
                  LOG_FMT(LWARN, "%s:%zu unexpected colon in col %zu n-parent=%s c-parent=%s l=%zu bl=%zu\n",
                          cpd.filename, next->orig_line, next->orig_col,
                          get_token_name(next->parent_type),
                          get_token_name(cur->parent_type),
                          next->level, next->brace_level);
                  cpd.error_count++;
               }
            }
         }
      }
      prev = cur;
      cur  = next;
      next = chunk_get_next_nc(cur);
   }
}


static void mark_variable_stack(ChunkStack &cs, log_sev_t sev)
{
   UNUSED(sev);
   LOG_FUNC_ENTRY();

   /* throw out the last word and mark the rest */
   chunk_t *var_name = cs.Pop_Back();
   if (var_name && (var_name->prev->type == CT_DC_MEMBER))
   {
      cs.Push_Back(var_name);
   }

   if (chunk_is_valid(var_name))
   {
      LOG_FMT(LFCNP, "%s: parameter on line %zu :", __func__, var_name->orig_line);

      size_t  word_cnt = 0;
      chunk_t *word_type;
      while ((word_type = cs.Pop_Back()) != nullptr)
      {
         if (chunk_is_type(word_type, 2, CT_WORD, CT_TYPE) )
         {
            LOG_FMT(LFCNP, " <%s>", word_type->text());

            set_chunk_type(word_type, CT_TYPE);
            chunk_flags_set(word_type, PCF_VAR_TYPE);
         }
         word_cnt++;
      }

      if (chunk_is_type(var_name, CT_WORD))
      {
         if (word_cnt > 0)
         {
            LOG_FMT(LFCNP, " [%s]\n", var_name->text());
            chunk_flags_set(var_name, PCF_VAR_DEF);
         }
         else
         {
            LOG_FMT(LFCNP, " <%s>\n", var_name->text());
            set_chunk_type(var_name, CT_TYPE);
            chunk_flags_set(var_name, PCF_VAR_TYPE);
         }
      }
   }
}


static void fix_fcn_def_params(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   if (chunk_is_invalid(start)) { return; }

   LOG_FMT(LFCNP, "%s: %s [%s] on line %zu, level %zu\n",
           __func__, start->text(), get_token_name(start->type), start->orig_line, start->level);

   while ( chunk_is_valid     (start) &&
          !chunk_is_paren_open(start) )
   {
      start = chunk_get_next_ncnl(start);
   }

   if (chunk_is_invalid(start)) { return; }

   /* ensure start chunk holds a single '(' character */
   size_t len        = start->len();
   char   first_char = start->str[0]; /*lint !e734 */
   assert( (len == 1) && (first_char == '(' ));

   ChunkStack cs;
   size_t     level = start->level + 1;
   chunk_t    *pc   = start;

   while ((pc = chunk_get_next_ncnl(pc)) != nullptr)
   {
      if (((start->len()  == 1   ) &&
           (start->str[0] == ')')) ||
           (pc->level < level))
      {
         LOG_FMT(LFCNP, "%s: bailed on %s on line %zu\n",
                 __func__, pc->text(), pc->orig_line);
         break;
      }

      LOG_FMT(LFCNP, "%s: %s %s on line %zu, level %zu\n",
              __func__, (pc->level > level) ? "skipping" : "looking at",
              pc->text(), pc->orig_line, pc->level);

      if (pc->level > level) { continue; }

      if (chunk_is_star (pc) ||
          chunk_is_msref(pc) )
      {
         set_chunk_type(pc, CT_PTR_TYPE);
         cs.Push_Back(pc);
      }
      else if (chunk_is_type(pc, CT_AMP) ||
               ((cpd.lang_flags & LANG_CPP) &&
                 chunk_is_str(pc, "&&", 2)))
      {
         set_chunk_type(pc, CT_BYREF);
         cs.Push_Back(pc);
      }
      else if (chunk_is_type(pc, CT_TYPE_WRAP))
      {
         cs.Push_Back(pc);
      }
      else if (chunk_is_type(pc, 2, CT_WORD, CT_TYPE))
      {
         cs.Push_Back(pc);
      }
      else if (chunk_is_type(pc, 2, CT_COMMA, CT_ASSIGN))
      {
         mark_variable_stack(cs, LFCNP);
         if (chunk_is_type(pc, CT_ASSIGN))
         {
            /* Mark assignment for default param spacing */
            set_chunk_parent(pc, CT_FUNC_PROTO);
         }
      }
   }
   mark_variable_stack(cs, LFCNP);
}


static chunk_t *skip_to_next_statement(chunk_t *pc)
{
   while (chunk_is_not_type(pc, 2, CT_BRACE_OPEN, CT_BRACE_CLOSE) &&
          !chunk_is_semicolon(pc) )
   {
      pc = chunk_get_next_ncnl(pc);
   }
   return(pc);
}


static chunk_t *fix_var_def(chunk_t *start)
{
   LOG_FUNC_ENTRY();

   if(chunk_is_invalid(start)) { return start; };

   chunk_t *pc = start;

   LOG_FMT(LFVD, "%s: start[%zu:%zu]", __func__, pc->orig_line, pc->orig_col);

   ChunkStack cs;

   /* Scan for words and types and stars oh my! */
   while (chunk_is_type(pc, 6, CT_TYPE, CT_MEMBER,   CT_QUALIFIER,
                               CT_WORD, CT_TYPENAME, CT_DC_MEMBER) ||
          chunk_is_ptr_operator(pc) )
   {
      LOG_FMT(LFVD, " %s[%s]", pc->text(), get_token_name(pc->type));
      cs.Push_Back(pc);
      pc = chunk_get_next_ncnl(pc);

      /* Skip templates and attributes */
      pc = skip_template_next(pc);
      pc = skip_attribute_next(pc);
      if (cpd.lang_flags & LANG_JAVA)
      {
         pc = skip_tsquare_next(pc);
      }
   }
   chunk_t *end = pc;

   LOG_FMT(LFVD, " end=[%s]\n", (chunk_is_valid(end)) ? get_token_name(end->type) : "nullptr");

   if (chunk_is_invalid(end)) { return(end); }

   /* Function defs are handled elsewhere */
   if ((cs.Len()  <= 1                  ) ||
        chunk_is_type(end, 5, CT_OPERATOR, CT_FUNC_DEF, CT_FUNC_PROTO,
                              CT_FUNC_CLASS_PROTO, CT_FUNC_CLASS_DEF) )
   {
      return(skip_to_next_statement(end));
   }

   /* ref_idx points to the alignable part of the var def */
   int ref_idx = (int)cs.Len() - 1;
   chunk_t *tmp_pc;

   assert(ptr_is_valid(cs.Get(0)));
   /* Check for the '::' stuff: "char *Engine::name" */
   if ((cs.Len() >= 3) &&
       ((cs.Get(cs.Len() - 2)->m_pc->type == CT_MEMBER   ) || /*lint !e613 */
        (cs.Get(cs.Len() - 2)->m_pc->type == CT_DC_MEMBER)))  /*lint !e613 */
   {
      int idx = (int)cs.Len() - 2;
      while (idx > 0)
      {
         tmp_pc = cs.Get((size_t)idx)->m_pc; /*lint !e613 */
         if (chunk_is_not_type(tmp_pc, 2, CT_DC_MEMBER, CT_MEMBER)) { break; }

         idx--;
         tmp_pc = cs.Get((size_t)idx)->m_pc; /*lint !e613 */
         if (chunk_is_not_type(tmp_pc, 2, CT_WORD, CT_TYPE)) { break; }

         make_type(tmp_pc);
         idx--;
      }
      ref_idx = idx + 1;
   }
   tmp_pc = cs.Get((size_t)ref_idx)->m_pc;  /*lint !e613 */
   LOG_FMT(LFVD, " ref_idx(%d) => %s\n", ref_idx, tmp_pc->text());

   /* No type part found! */
   if (ref_idx <= 0)
   {
      return(skip_to_next_statement(end));
   }

   LOG_FMT(LFVD2, "%s:%zu TYPE : ", __func__, start->orig_line);
   for (size_t idxForCs = 0; idxForCs < cs.Len() - 1; idxForCs++)
   {
      tmp_pc = cs.Get(idxForCs)->m_pc; /*lint !e613 */
      make_type(tmp_pc);
      chunk_flags_set(tmp_pc, PCF_VAR_TYPE);
      LOG_FMT(LFVD2, " %s[%s]", tmp_pc->text(), get_token_name(tmp_pc->type));
   }
   LOG_FMT(LFVD2, "\n");

   /* OK we have two or more items, mark types up to the end. */
   mark_variable_definition(cs.Get(cs.Len() - 1)->m_pc); /*lint !e613 */
   if (chunk_is_type(end, CT_COMMA))
   {
      return(chunk_get_next_ncnl(end));
   }
   return(skip_to_next_statement(end));
}


static chunk_t *skip_expression(chunk_t *start)
{
   chunk_t *pc = start;

   while ((chunk_is_valid(pc)       ) &&
          (pc->level >= start->level) )
   {
      if ((pc->level == start->level) &&
          (chunk_is_semicolon(pc) ||
          (chunk_is_type(pc, CT_COMMA))))
      {
         return(pc);
      }
      pc = chunk_get_next_ncnl(pc);
   }
   return(pc);
}


bool go_on(chunk_t *pc, chunk_t *start)
{
   if ((chunk_is_invalid(pc   )  ) ||
       (chunk_is_invalid(start)  ) ||
       (pc->level != start->level) )
   {
      return(false);
   }
   if (pc->flags & PCF_IN_FOR)
   {
      return((!chunk_is_semicolon(pc)) &&
             (!(pc->type == CT_COLON)));
   }
   else
   {
      return(!chunk_is_semicolon(pc));
   }
}


static chunk_t *mark_variable_definition(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   retval_if_invalid(start, start);

   chunk_t *pc   = start;
   size_t  flags = PCF_VAR_1ST_DEF;

   LOG_FMT(LVARDEF, "%s: line %zu, col %zu '%s' type %s\n",
           __func__, pc->orig_line, pc->orig_col, pc->text(),
           get_token_name(pc->type));

   pc = start;
   // issue #596
   while (go_on(pc, start))
   {
      if (chunk_is_type(pc, 2, CT_WORD, CT_FUNC_CTOR_VAR))
      {
         UINT64 flg = pc->flags;
         if ((pc->flags & PCF_IN_ENUM) == 0)
         {
            chunk_flags_set(pc, flags);
         }
         flags &= ~PCF_VAR_1ST;

         LOG_FMT(LVARDEF, "%s:%zu marked '%s'[%s] in col %zu flags: %#" PRIx64 " -> %#" PRIx64 "\n",
                 __func__, pc->orig_line, pc->text(),
                 get_token_name(pc->type), pc->orig_col, flg, pc->flags);
      }
      else if (chunk_is_star (pc) ||
               chunk_is_msref(pc) )
      {
         set_chunk_type(pc, CT_PTR_TYPE);
      }
      else if (chunk_is_addr(pc))
      {
         set_chunk_type(pc, CT_BYREF);
      }
      else if (chunk_is_type(pc, 2, CT_SQUARE_OPEN, CT_ASSIGN))
      {
         pc = skip_expression(pc);
         continue;
      }
      pc = chunk_get_next_ncnl(pc);
   }
   return(pc);
}


static bool can_be_full_param(chunk_t *start, chunk_t *end)
{
   LOG_FUNC_ENTRY();

   LOG_FMT(LFPARAM, "%s:", __func__);

   int     word_cnt   = 0;
   size_t  type_count = 0;
   chunk_t *pc;

   for (pc = start; pc != end; pc = chunk_get_next_ncnl(pc, scope_e::PREPROC))
   {
      assert(chunk_is_valid(pc));
      LOG_FMT(LFPARAM, " [%s]", pc->text());

      if (chunk_is_type(pc, 5, CT_QUALIFIER, CT_STRUCT, CT_ENUM,
                               CT_TYPENAME,  CT_UNION) )
      {
         LOG_FMT(LFPARAM, " <== %s! (yes)\n", get_token_name(pc->type));
         return(true);
      }

      /* \todo better use a switch */
      if (chunk_is_type(pc, 2, CT_WORD, CT_TYPE))
      {
         word_cnt++;
         if (chunk_is_type(pc, CT_TYPE))
         {
            type_count++;
         }
      }
      else if (chunk_is_type(pc, 2, CT_MEMBER, CT_DC_MEMBER))
      {
         if (word_cnt > 0)
         {
            word_cnt--;
         }
      }
      else if ((pc != start) && chunk_is_ptr_operator(pc))
      {
         /* chunk is OK */
      }
      else if (chunk_is_type(pc, CT_ASSIGN))
      {
         /* chunk is OK (default values) */
         break;
      }
      else if (chunk_is_type(pc, CT_ANGLE_OPEN))
      {
         LOG_FMT(LFPARAM, " <== template\n");
         return(true);
      }
      else if (chunk_is_type(pc, CT_ELLIPSIS))
      {
         LOG_FMT(LFPARAM, " <== elipses\n");
         return(true);
      }
      else if ((word_cnt == 0) &&
               chunk_is_type(pc, CT_PAREN_OPEN))
      {
         /* Check for old-school func proto param '(type)' */
         chunk_t *tmp1 = chunk_skip_to_match(pc,   scope_e::PREPROC);
         chunk_t *tmp2 = chunk_get_next_ncnl(tmp1, scope_e::PREPROC);

         if (chunk_is_type(tmp2, CT_COMMA) ||
             chunk_is_paren_close(tmp2))
         {
            do
            {
               pc = chunk_get_next_ncnl(pc, scope_e::PREPROC);
               assert(chunk_is_valid(pc));
               LOG_FMT(LFPARAM, " [%s]", pc->text());
            } while (pc != tmp1);

            /* reset some vars to allow [] after parens */
            word_cnt   = 1;
            type_count = 1;
         }
         else
         {
            LOG_FMT(LFPARAM, " <== [%s] not fcn type!\n", get_token_name(pc->type));
            return(false);
         }
      }
      else if (((word_cnt == 1) ||
               (static_cast<size_t>(word_cnt) == type_count)) &&
               (chunk_is_type(pc, CT_PAREN_OPEN)))
      {
         /* Check for func proto param 'void (*name)' or 'void (*name)(params)' */
         chunk_t *tmp1 = chunk_get_next_ncnl(pc,   scope_e::PREPROC);
         chunk_t *tmp2 = chunk_get_next_ncnl(tmp1, scope_e::PREPROC);
         chunk_t *tmp3 = chunk_get_next_ncnl(tmp2, scope_e::PREPROC);

         if (!chunk_is_str     (tmp3, ")", 1 ) ||
             !chunk_is_str     (tmp1, "*", 1 ) ||
              chunk_is_not_type(tmp2, CT_WORD) )
         {
            LOG_FMT(LFPARAM, " <== [%s] not fcn type!\n", get_token_name(pc->type));
            return(false);
         }
         LOG_FMT(LFPARAM, " <skip fcn type>");
         tmp1 = chunk_get_next_ncnl(tmp3, scope_e::PREPROC);
         if (chunk_is_str(tmp1, "(", 1))
         {
            tmp3 = chunk_skip_to_match(tmp1, scope_e::PREPROC);
         }
         pc = tmp3;

         /* reset some vars to allow [] after parens */
         word_cnt   = 1;
         type_count = 1;
      }
      else if (chunk_is_type(pc, CT_TSQUARE))
      {
         /* ignore it */
      }
      else if ((word_cnt == 1) &&
               chunk_is_type(pc, CT_SQUARE_OPEN))
      {
         /* skip over any array stuff */
         pc = chunk_skip_to_match(pc, scope_e::PREPROC);
      }
      else if ((word_cnt == 2) &&
                chunk_is_type(pc, CT_SQUARE_OPEN))
      {
         /* Bug #671: is it such as: bool foo[FOO_MAX] */
         pc = chunk_skip_to_match(pc, scope_e::PREPROC);
      }
      else if ((word_cnt == 1) &&
               (cpd.lang_flags & LANG_CPP) &&
               chunk_is_str(pc, "&&", 2))
      {
         /* ignore possible 'move' operator */
      }
      else
      {
         LOG_FMT(LFPARAM, " <== [%s] no way! tc=%zu wc=%d\n",
                 get_token_name(pc->type), type_count, word_cnt);
         return(false);
      }
   }

   chunk_t *last = chunk_get_prev_ncnl(pc);
   if (chunk_is_ptr_operator(last))
   {
      if (chunk_is_valid(pc))
      {
         LOG_FMT(LFPARAM, " <== [%s] sure!\n", get_token_name(pc->type));
      }
      return(true);
   }

   bool ret = ( (word_cnt >= 2) ||
               ((word_cnt == 1) && (type_count == 1)));

   if (chunk_is_valid(pc))
   {
      LOG_FMT(LFPARAM, " <== [%s] %s!\n",
              get_token_name(pc->type), ret ? "Yup" : "Unlikely");
   }
   return(ret);
}


static void mark_function(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   assert(chunk_is_valid(pc));
   chunk_t *prev = chunk_get_prev_ncnlnp(pc);
   chunk_t *next = chunk_get_next_ncnlnp(pc);
   return_if_invalid(next);

   chunk_t *tmp;
   chunk_t *semi = nullptr;
   chunk_t *paren_open;
   chunk_t *paren_close;

   /* Find out what is before the operator */
   if (pc->parent_type == CT_OPERATOR)
   {
      const chunk_t *pc_op = chunk_get_prev_type(pc, CT_OPERATOR, (int)pc->level);
      if ((chunk_is_valid(pc_op)) &&
          (pc_op->flags & PCF_EXPR_START))
      {
         set_chunk_type(pc, CT_FUNC_CALL);
      }
      if (cpd.lang_flags & LANG_CPP)
      {
         tmp = pc;
         while ((tmp = chunk_get_prev_ncnl(tmp)) != nullptr)
         {
#if 1
            if ((tmp->type == CT_BRACE_CLOSE) ||
                (tmp->type == CT_BRACE_OPEN) ||  // Issue 575
                (tmp->type == CT_SEMICOLON)) {                                   break; }
            if (chunk_is_paren_open(tmp)   ) { set_chunk_type(pc, CT_FUNC_CALL); break; }
            if (tmp->type == CT_ASSIGN     ) { set_chunk_type(pc, CT_FUNC_CALL); break; }
            if (tmp->type == CT_TEMPLATE   ) { set_chunk_type(pc, CT_FUNC_DEF ); break; }
            if (tmp->type == CT_BRACE_OPEN)
            {
               if (tmp->parent_type == CT_FUNC_DEF)
               {
                  set_chunk_type(pc, CT_FUNC_CALL);
               }
               if ((tmp->parent_type == CT_CLASS ) ||
                   (tmp->parent_type == CT_STRUCT) )
               {
                  set_chunk_type(pc, CT_FUNC_DEF);
               }
               break;
            }
#else
            // \todo fails test 30109
            switch(tmp->type)
            {
               case(CT_BRACE_CLOSE):   /* fallthrough */
               case(CT_BRACE_OPEN ):   /* fallthrough */
               case(CT_SEMICOLON  ): { /* do nothing */                     break; }
               case(CT_PAREN_OPEN ):   /* fallthrough */
               case(CT_SPAREN_OPEN):   /* fallthrough */
               case(CT_TPAREN_OPEN):   /* fallthrough */
               case(CT_FPAREN_OPEN): { set_chunk_type(pc, CT_FUNC_CALL);    break; }
               case(CT_ASSIGN     ): { set_chunk_type(pc, CT_FUNC_CALL);    break; }
               case(CT_TEMPLATE   ): { set_chunk_type(pc, CT_FUNC_DEF );    break; }
               case(CT_BRACE_OPEN ): // \todo double BRACE_OPEN case
               {
                  switch(tmp->parent_type)
                  {
                     case(CT_FUNC_DEF): { set_chunk_type(pc, CT_FUNC_CALL); break; }
                     case(CT_CLASS   ): /* fallthrough */
                     case(CT_STRUCT  ): { set_chunk_type(pc, CT_FUNC_DEF ); break; }
                     default:           { /* ignore unexpected type */      break; }
                  }
                  break;
               }
               default:              { /* ignore unexpected type */         break; }
            }
#endif
         }

         if (chunk_is_valid   (tmp             ) &&
             chunk_is_not_type(pc, CT_FUNC_CALL) )
         {
            /* Mark the return type */
            while ((tmp = chunk_get_next_ncnl(tmp)) != pc)
            {
               make_type(tmp);
            }
         }
      }
   }

   if (chunk_is_ptr_operator(next))
   {
      next = chunk_get_next_ncnlnp(next);
      return_if_invalid(next);
   }

   LOG_FMT(LFCN, "%s: orig_line=%zu] %s[%s] - parent=%s level=%zu/%zu, next=%s[%s] - level=%zu\n",
           __func__, pc->orig_line, pc->text(),
           get_token_name(pc->type), get_token_name(pc->parent_type),
           pc->level, pc->brace_level,
           next->text(), get_token_name(next->type), next->level);

   if (pc->flags & PCF_IN_CONST_ARGS)
   {
      set_chunk_type(pc, CT_FUNC_CTOR_VAR);

      LOG_FMT(LFCN, "  1) Marked [%s] as FUNC_CTOR_VAR on line %zu col %zu\n",
              pc->text(), pc->orig_line, pc->orig_col);
      next = skip_template_next(next);
      return_if_invalid(next);

      flag_parens(next, 0, CT_FPAREN_OPEN, pc->type, true);
      return;
   }

   /* Skip over any template and attribute madness */
   next = skip_template_next (next); return_if_invalid(next);
   next = skip_attribute_next(next); return_if_invalid(next);

   /* Find the open and close paren */
   paren_open  = chunk_get_next_str(pc, "(", 1, (int)pc->level);
   paren_close = chunk_get_next_str(paren_open, ")", 1, (int)pc->level);

   if (chunk_is_invalid(paren_open ) ||
       chunk_is_invalid(paren_close) )
   {
#ifdef DEBUG
      LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
      LOG_FMT(LFCN, "No parens found for [%s] on line %zu col %zu\n",
              pc->text(), pc->orig_line, pc->orig_col);
      return;
   }

   /**
    * This part detects either chained function calls or a function ptr definition.
    * MYTYPE (*func)(void);
    * mWriter( "class Clst_"c )( somestr.getText() )( " : Cluster {"c ).newline;
    *
    * For it to be a function variable def, there must be a '*' followed by a
    * single word.
    *
    * Otherwise, it must be chained function calls.
    */
   tmp = chunk_get_next_ncnl(paren_close);
   if (chunk_is_str(tmp, "(", 1))
   {
      chunk_t *tmp1, *tmp2, *tmp3;

      /* skip over any leading class/namespace in: "T(F::*A)();" */
      tmp1 = chunk_get_next_ncnl(next);
      while (tmp1)
      {
         tmp2 = chunk_get_next_ncnl(tmp1);
         if (chunk_is_word(tmp1              ) == false ||
             chunk_is_type(tmp2, CT_DC_MEMBER) == false )
         {
            break;
         }
         tmp1 = chunk_get_next_ncnl(tmp2);
      }

      tmp2 = chunk_get_next_ncnl(tmp1);
      if (chunk_is_str(tmp2, ")", 1))
      {
         tmp3 = tmp2;
         tmp2 = nullptr;
      }
      else
      {
         tmp3 = chunk_get_next_ncnl(tmp2);
      }

      if (chunk_is_str(tmp3, ")", 1) &&
          (chunk_is_star (tmp1) ||
           chunk_is_msref(tmp1) ||
           ((cpd.lang_flags & LANG_OC) && chunk_is_type(tmp1, CT_CARET)))
          &&
          ((tmp2       == nullptr) ||
           (tmp2->type == CT_WORD) ) )
      {
         if (tmp2)
         {
#ifdef DEBUG
            LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
            LOG_FMT(LFCN, "%s: [%zu/%zu] function variable [%s], changing [%s] into a type\n",
                    __func__, pc->orig_line, pc->orig_col, tmp2->text(), pc->text());
            set_chunk_type(tmp2, CT_FUNC_VAR);
            flag_parens(paren_open, 0, CT_PAREN_OPEN, CT_FUNC_VAR, false);

#ifdef DEBUG
            LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
            LOG_FMT(LFCN, "%s: paren open @ %zu:%zu\n",
                    __func__, paren_open->orig_line, paren_open->orig_col);
         }
         else
         {
#ifdef DEBUG
            LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
            LOG_FMT(LFCN, "%s: [%zu/%zu] function type, changing [%s] into a type\n",
                    __func__, pc->orig_line, pc->orig_col, pc->text());
            if (tmp2)
            {
               set_chunk_type(tmp2, CT_FUNC_TYPE);
            }
            flag_parens(paren_open, 0, CT_PAREN_OPEN, CT_FUNC_TYPE, false);
         }

         set_chunk_type(pc, CT_TYPE);
         set_chunk_type(tmp1, CT_PTR_TYPE);
         chunk_flags_clr(pc, PCF_VAR_1ST_DEF);
         if (chunk_is_valid(tmp2))
         {
            chunk_flags_set(tmp2, PCF_VAR_1ST_DEF);
         }
         flag_parens(tmp, 0, CT_FPAREN_OPEN, CT_FUNC_PROTO, false);
         fix_fcn_def_params(tmp);
         return;
      }

#ifdef DEBUG
      LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
      LOG_FMT(LFCN, "%s: chained function calls? [%zu.%zu] [%s]\n",
              __func__, pc->orig_line, pc->orig_col, pc->text());
   }

   /* Assume it is a function call if not already labeled */
   if (chunk_is_type(pc, CT_FUNCTION))
   {
#ifdef DEBUG
      LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
      LOG_FMT(LFCN, "%s: examine [%zu.%zu] [%s], type %s\n",
              __func__, pc->orig_line, pc->orig_col, pc->text(), get_token_name(pc->type));
      // look for an assigment. Issue 575
      chunk_t *temp = chunk_get_next_type(pc, CT_ASSIGN, (int)pc->level);
      if (chunk_is_valid(temp))
      {
         LOG_FMT(LFCN, "%s: assigment found [%zu.%zu] [%s]\n",
                 __func__, temp->orig_line, temp->orig_col, temp->text());
         set_chunk_type(pc, CT_FUNC_CALL);
      }
      else
      {
         set_chunk_type(pc, (pc->parent_type == CT_OPERATOR) ? CT_FUNC_DEF : CT_FUNC_CALL);
      }
   }

   /* Check for C++ function def */
   if (chunk_is_type(pc,      CT_FUNC_CLASS_DEF   ) ||
       chunk_is_type(prev, 2, CT_DC_MEMBER, CT_INV) )
   {
      const chunk_t *destr = nullptr;
      assert(chunk_is_valid(prev));
      if (chunk_is_type(prev, CT_INV))
      {
         /* TODO: do we care that this is the destructor? */
         set_chunk_type  (prev, CT_DESTRUCTOR    );
         set_chunk_type  (pc,   CT_FUNC_CLASS_DEF);
         set_chunk_parent(pc,   CT_DESTRUCTOR    );

         destr = prev;
         prev  = chunk_get_prev_ncnlnp(prev);
      }

      if (chunk_is_type(prev, CT_DC_MEMBER))
      {
         prev = chunk_get_prev_ncnlnp(prev);
         // LOG_FMT(LSYS, "%s: prev1 = %s (%s)\n", __func__,
         //         get_token_name(prev->type), prev->text());
         prev = skip_template_prev(prev);
         prev = skip_attribute_prev(prev);
         // LOG_FMT(LSYS, "%s: prev2 = %s [%d](%s) pc = %s [%d](%s)\n", __func__,
         //         get_token_name(prev->type), prev->len, prev->text(),
         //         get_token_name(pc->type), pc->len, pc->text());
         if (chunk_is_type(prev, 2, CT_WORD, CT_TYPE))
         {
            if (pc->str.equals(prev->str))
            {
               set_chunk_type(pc, CT_FUNC_CLASS_DEF);
#ifdef DEBUG
               LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
               LOG_FMT(LFCN, "(%d) %zu:%zu - FOUND %sSTRUCTOR for %s[%s]\n", __LINE__,
                       prev->orig_line, prev->orig_col,
                       (chunk_is_valid(destr)) ? "DE" : "CON",
                       prev->text(), get_token_name(prev->type));

               mark_cpp_constructor(pc);
               return;
            }
            else
            {
               /* Point to the item previous to the class name */
               prev = chunk_get_prev_ncnlnp(prev);
            }
         }
      }
   }

   /* Determine if this is a function call or a function def/proto
    * We check for level==1 to allow the case that a function prototype is
    * wrapped in a macro: "MACRO(void foo(void));"
    */
   if (chunk_is_type(pc, CT_FUNC_CALL) &&
       ((pc->level == pc->brace_level) || (pc->level == 1)) &&
       ((pc->flags & PCF_IN_ARRAY_ASSIGN) == 0))
   {
      bool isa_def  = false;
      bool hit_star = false;
#ifdef DEBUG
      LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
      LOG_FMT(LFCN, "  Checking func call: prev=%s", (chunk_is_invalid(prev)) ? "<null>" : get_token_name(prev->type));

      // if (!chunk_ends_type(prev))
      // {
      //    goto bad_ret_type;
      // }

      /**
       * REVISIT:
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
       * static const char * const fizz(); -- fcn def
       */
      while (chunk_is_valid(prev))
      {
         if (prev->flags & PCF_IN_PREPROC)
         {
            prev = chunk_get_prev_ncnlnp(prev);
            continue;
         }

         /* Some code slips an attribute between the type and function */
         if (chunk_is_type(prev, CT_FPAREN_CLOSE) &&
             (prev->parent_type == CT_ATTRIBUTE))
         {
            prev = skip_attribute_prev(prev);
            continue;
         }

         /* skip const(TYPE) */
         if (chunk_is_type(prev, CT_PAREN_CLOSE) &&
             (prev->parent_type == CT_D_CAST))
         {
#ifdef DEBUG
            LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
            LOG_FMT(LFCN, " --> For sure a prototype or definition\n");
            isa_def = true;
            break;
         }

         /** Skip the word/type before the '.' or '::' */
         if (chunk_is_type(prev, 2, CT_DC_MEMBER, CT_MEMBER))
         {
            prev = chunk_get_prev_ncnlnp(prev);
            if (chunk_is_not_type(prev, 3, CT_WORD, CT_TYPE, CT_THIS))
            {
#ifdef DEBUG
               LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
               LOG_FMT(LFCN, " --? Skipped MEMBER and landed on %s\n",
                       (prev == nullptr) ? "<null>" : get_token_name(prev->type));
               set_chunk_type(pc, CT_FUNC_CALL);
               isa_def = false;
               break;
            }
            LOG_FMT(LFCN, " <skip %s>", prev->text());
            prev = chunk_get_prev_ncnlnp(prev);
            continue;
         }

         /* If we are on a TYPE or WORD, then we must be on a proto or def */
         if (chunk_is_type(prev, 2, CT_TYPE, CT_WORD))
         {
            if (hit_star == false)
            {
#ifdef DEBUG
               LOG_FMT(LFCN, "\n(%d) ", __LINE__);
#endif
               LOG_FMT(LFCN, " --> For sure a prototype or definition\n");
               isa_def = true;
               break;
            }
#ifdef DEBUG
            LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
            LOG_FMT(LFCN, " --> maybe a proto/def\n");
            isa_def = true;
         }

         if (chunk_is_ptr_operator(prev))
         {
            hit_star = true;
         }

         if (chunk_is_not_type(prev, 6, CT_OPERATOR,  CT_TSQUARE,     CT_WORD,
                                        CT_QUALIFIER, CT_ANGLE_CLOSE, CT_TYPE) &&
             !chunk_is_ptr_operator(prev))
         {
#ifdef DEBUG
            LOG_FMT(LFCN, "\n(%d) ", __LINE__);
#endif
            LOG_FMT(LFCN, " --> Stopping on %s [%s]\n",
                    prev->text(), get_token_name(prev->type));
            /* certain tokens are unlikely to precede a proto or def */
            if (chunk_is_type(prev, 7, CT_ARITH, CT_ASSIGN, CT_COMMA,
                  CT_STRING, CT_STRING_MULTI, CT_NUMBER, CT_NUMBER_FP))
            {
               isa_def = false;
            }
            break;
         }

         /* Skip over template and attribute stuff */
         if (chunk_is_type(prev, CT_ANGLE_CLOSE))
         {
            prev = skip_template_prev(prev);
         }
         else
         {
            prev = chunk_get_prev_ncnlnp(prev);
         }
      }

      //LOG_FMT(LFCN, " -- stopped on %s [%s]\n",
      //        prev->text(), get_token_name(prev->type));

      if ((isa_def == true) &&
          ((chunk_is_paren_close(prev) &&
           (prev->parent_type != CT_D_CAST)) ||
           chunk_is_type(prev, 2, CT_ASSIGN, CT_RETURN)))
      {
#ifdef DEBUG
         LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
         LOG_FMT(LFCN, " -- overriding DEF due to %s [%s]\n",
                 prev->text(), get_token_name(prev->type));
         isa_def = false;
      }
      if (isa_def)
      {
         set_chunk_type(pc, CT_FUNC_DEF);
#ifdef DEBUG
         LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
         LOG_FMT(LFCN, "%s: '%s' is FCN_DEF:", __func__, pc->text());
         if (chunk_is_invalid(prev))
         {
            prev = chunk_get_head();
         }
         for (tmp = prev; tmp != pc; tmp = chunk_get_next_ncnl(tmp))
         {
            LOG_FMT(LFCN, " %s[%s]",
                    tmp->text(), get_token_name(tmp->type));
            make_type(tmp);
         }
         LOG_FMT(LFCN, "\n");
      }
   }

   if (pc->type != CT_FUNC_DEF)
   {
#ifdef DEBUG
      LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
      LOG_FMT(LFCN, "  Detected %s '%s' on line %zu col %zu\n",
              get_token_name(pc->type),
              pc->text(), pc->orig_line, pc->orig_col);

      tmp = flag_parens(next, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_FUNC_CALL, false);
      if (tmp && (tmp->type == CT_BRACE_OPEN) && (tmp->parent_type != CT_DOUBLE_BRACE))
      {
         set_paren_parent(tmp, pc->type);
      }
      return;
   }

   /* We have a function definition or prototype
    * Look for a semicolon or a brace open after the close paren to figure
    * out whether this is a prototype or definition
    */

   /* See if this is a prototype or implementation */

   /* FIXME: this doesn't take the old K&R parameter definitions into account */

   /* Scan tokens until we hit a brace open (def) or semicolon (proto) */
   tmp = paren_close;
   while ((tmp = chunk_get_next_ncnl(tmp)) != nullptr)
   {
      /* Only care about brace or semi on the same level */
      if (tmp->level < pc->level)
      {
         /* No semicolon - guess that it is a prototype */
         set_chunk_type(pc, CT_FUNC_PROTO);
         break;
      }
      else if (tmp->level == pc->level)
      {
         if (chunk_is_type(tmp, CT_BRACE_OPEN)) { break; }/* its a function def for sure */

         else if (chunk_is_semicolon(tmp))
         {
            /* Set the parent for the semi for later */
            semi = tmp;
            set_chunk_type(pc, CT_FUNC_PROTO);
#ifdef DEBUG
            LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
            LOG_FMT(LFCN, "  2) Marked [%s] as FUNC_PROTO on line %zu col %zu\n",
                    pc->text(), pc->orig_line, pc->orig_col);
            break;
         }
         else if (chunk_is_type(pc, CT_COMMA))
         {
            set_chunk_type(pc, CT_FUNC_CTOR_VAR);
#ifdef DEBUG
            LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
            LOG_FMT(LFCN, "  2) Marked [%s] as FUNC_CTOR_VAR on line %zu col %zu\n",
                    pc->text(), pc->orig_line, pc->orig_col);
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
   if ((cpd.lang_flags & LANG_CPP) &&
       (chunk_is_type(pc, CT_FUNC_PROTO)) &&
       (pc->parent_type != CT_OPERATOR))
   {
#ifdef DEBUG
      LOG_FMT(LFPARAM, "(%d) ", __LINE__);
#endif
      LOG_FMT(LFPARAM, "%s :: checking '%s' for constructor variable %s %s\n",
              __func__, pc->text(),
              get_token_name(paren_open->type),
              get_token_name(paren_close->type));

      /* Scan the parameters looking for:
       *  - constant strings
       *  - numbers
       *  - non-type fields
       *  - function calls
       */
      chunk_t *ref = chunk_get_next_ncnl(paren_open);
      chunk_t *tmp2;
      bool    is_param = true;
      tmp = ref;
      while (tmp != paren_close)
      {
         tmp2 = chunk_get_next_ncnl(tmp);
         if (chunk_is_type(tmp, CT_COMMA           ) &&
             (tmp->level == (paren_open->level + 1)) )
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
         set_chunk_type(pc, CT_FUNC_CTOR_VAR);
#ifdef DEBUG
         LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
         LOG_FMT(LFCN, "  3) Marked [%s] as FUNC_CTOR_VAR on line %zu col %zu\n",
                 pc->text(), pc->orig_line, pc->orig_col);
      }
      else if (pc->brace_level > 0)
      {
         chunk_t *br_open = chunk_get_prev_type(pc, CT_BRACE_OPEN, (int)pc->brace_level - 1);

         if (chunk_is_not_ptype(br_open, 2, CT_EXTERN, CT_NAMESPACE))
         {
            /* Do a check to see if the level is right */
            prev = chunk_get_prev_ncnl(pc);
            if (!chunk_is_str(prev, "*", 1) && !chunk_is_str(prev, "&", 1))
            {
               chunk_t *p_op = chunk_get_prev_type(pc, CT_BRACE_OPEN, (int)pc->brace_level - 1);
               if (chunk_is_not_ptype(p_op, 3, CT_CLASS, CT_STRUCT, CT_NAMESPACE))
               {
                  set_chunk_type(pc, CT_FUNC_CTOR_VAR);
#ifdef DEBUG
                  LOG_FMT(LFCN, "(%d) ", __LINE__);
#endif
                  LOG_FMT(LFCN, "  4) Marked [%s] as FUNC_CTOR_VAR on line %zu col %zu\n",
                          pc->text(), pc->orig_line, pc->orig_col);
               }
            }
         }
      }
   }

   if (chunk_is_valid(semi)) { set_chunk_parent(semi, pc->type); }

   flag_parens(paren_open, PCF_IN_FCN_DEF, CT_FPAREN_OPEN, pc->type, false);

   if (chunk_is_type(pc, CT_FUNC_CTOR_VAR))
   {
      chunk_flags_set(pc, PCF_VAR_1ST_DEF);
      return;
   }

   if (chunk_is_type(next, CT_TSQUARE))
   {
      next = chunk_get_next_ncnl(next);
      return_if_invalid(next);
   }

   /* Mark parameters and return type */
   fix_fcn_def_params(next);
   mark_function_return_type(pc, chunk_get_prev_ncnl(pc), pc->type);

   /* Find the brace pair and set the parent */
   if (chunk_is_type(pc, CT_FUNC_DEF))
   {
      tmp = chunk_get_next_ncnl(paren_close);
      while (chunk_is_not_type(tmp, CT_BRACE_OPEN))
      {
         //LOG_FMT(LSYS, "%s: set parent to FUNC_DEF on line %d: [%s]\n", __func__, tmp->orig_line, tmp->text());
         set_chunk_parent(tmp, CT_FUNC_DEF);
         if (!chunk_is_semicolon(tmp))
         {
            chunk_flags_set(tmp, PCF_OLD_FCN_PARAMS);
         }
         tmp = chunk_get_next_ncnl(tmp);
      }
      if (chunk_is_type(tmp, CT_BRACE_OPEN))
      {
         set_chunk_parent(tmp, CT_FUNC_DEF);
         tmp = chunk_skip_to_match(tmp);
         if (chunk_is_valid(tmp))
         {
            set_chunk_parent(tmp, CT_FUNC_DEF);
         }
      }
   }
}


static void mark_cpp_constructor(chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   if(chunk_is_invalid(pc)) { return; }

   bool    is_destr = false;
   chunk_t *tmp     = chunk_get_prev_ncnl(pc);
   assert(chunk_is_valid(tmp));
   if (chunk_is_type(tmp, 2, CT_INV, CT_DESTRUCTOR))
   {
      set_chunk_type  (tmp, CT_DESTRUCTOR);
      set_chunk_parent(pc,  CT_DESTRUCTOR);
      is_destr = true;
   }

   LOG_FMT(LFTOR, "(%d) %zu:%zu FOUND %sSTRUCTOR for %s[%s] prev=%s[%s]",
           __LINE__, pc->orig_line, pc->orig_col, is_destr ? "DE" : "CON",
           pc->text(), get_token_name(pc->type), tmp->text(), get_token_name(tmp->type));

   chunk_t *paren_open = skip_template_next(chunk_get_next_ncnl(pc));
   if (!chunk_is_str(paren_open, "(", 1))
   {
      assert(chunk_is_valid(paren_open));
      LOG_FMT(LWARN, "%s:%zu Expected '(', got: [%s]\n", cpd.filename,
              paren_open->orig_line, paren_open->text());
      return;
   }

   /* Mark parameters */
   fix_fcn_def_params(paren_open);
   chunk_t *after = flag_parens(paren_open, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_FUNC_CLASS_PROTO, false);

   assert(chunk_is_valid(after));
   LOG_FMT(LFTOR, "[%s]\n", after->text());

   /* Scan until the brace open, mark everything */
   chunk_t *var;
   tmp = paren_open;
   bool hit_colon = false;
   while ( chunk_is_not_type(tmp, CT_BRACE_OPEN) &&
          !chunk_is_semicolon(tmp) )
   {
      chunk_flags_set(tmp, PCF_IN_CONST_ARGS);
      tmp = chunk_get_next_ncnl(tmp);
      assert(chunks_are_valid(paren_open, tmp));
      if (chunk_is_str(tmp, ":", 1) && (tmp->level == paren_open->level))
      {
         set_chunk_type(tmp, CT_CONSTR_COLON);
         hit_colon = true;
      }
      if (hit_colon &&
          (chunk_is_paren_open(tmp) ||
           chunk_is_opening_brace(tmp)) &&
          (tmp->level == paren_open->level))
      {
         var = skip_template_prev(chunk_get_prev_ncnl(tmp));
         assert(chunk_is_valid(var));
         if (chunk_is_type(var, 2, CT_TYPE, CT_WORD))
         {
            set_chunk_type(var, CT_FUNC_CTOR_VAR);
            flag_parens(tmp, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_FUNC_CTOR_VAR, false);
         }
      }
   }
   if (chunk_is_valid(tmp))
   {
      if (chunk_is_type(tmp, CT_BRACE_OPEN))
      {
         set_paren_parent(paren_open, CT_FUNC_CLASS_DEF);
         set_paren_parent(tmp, CT_FUNC_CLASS_DEF);
      }
      else
      {
         set_chunk_parent(tmp, CT_FUNC_CLASS_PROTO);
         set_chunk_type(pc, CT_FUNC_CLASS_PROTO);
         LOG_FMT(LFCN, "  2) Marked [%s] as FUNC_CLASS_PROTO on line %zu col %zu\n",
                 pc->text(), pc->orig_line, pc->orig_col);
      }
   }
}


static void mark_class_ctor(chunk_t *start)
{
   LOG_FUNC_ENTRY();

   chunk_t *pclass = chunk_get_next_ncnl(start, scope_e::PREPROC);
   if (chunk_is_invalid (pclass                     ) ||
       chunk_is_not_type(pclass, 2, CT_TYPE, CT_WORD) )
   {
      return;
   }

   chunk_t *next = chunk_get_next_ncnl(pclass, scope_e::PREPROC);
   while (chunk_is_type(next, 3, CT_TYPE, CT_WORD, CT_DC_MEMBER))
   {
      pclass = next;
      next   = chunk_get_next_ncnl(next, scope_e::PREPROC);
   }

   chunk_t *pc   = chunk_get_next_ncnl(pclass, scope_e::PREPROC);
   size_t  level = pclass->brace_level + 1;

   if (chunk_is_invalid(pc))
   {
      LOG_FMT(LFTOR, "%s: Called on %s on line %zu. Bailed on nullptr\n",
              __func__, pclass->text(), pclass->orig_line);
      return;
   }

   /* Add the class name */
   ChunkStack cs;
   cs.Push_Back(pclass);

   LOG_FMT(LFTOR, "%s: Called on %s on line %zu (next='%s')\n",
           __func__, pclass->text(), pclass->orig_line, pc->text());

   /* detect D template class: "class foo(x) { ... }" */
   if (chunk_is_valid(next))
   {
      if ((cpd.lang_flags & LANG_D          ) &&
           chunk_is_type(next, CT_PAREN_OPEN) )
      {
         set_chunk_parent(next, CT_TEMPLATE);

         next = get_d_template_types(cs, next);
         if (chunk_is_type(next, CT_PAREN_CLOSE))
         {
            set_chunk_parent(next, CT_TEMPLATE);
         }
      }
   }

   /* Find the open brace, abort on semicolon */
   size_t flags = 0;
   while (chunk_is_not_type(pc, CT_BRACE_OPEN))
   {
      LOG_FMT(LFTOR, " [%s]", pc->text());

      if (chunk_is_str(pc, ":", 1))
      {
         set_chunk_type(pc, CT_CLASS_COLON);
         flags |= PCF_IN_CLASS_BASE;
         LOG_FMT(LFTOR, "%s: class colon on line %zu\n",
                 __func__, pc->orig_line);
      }

      if (chunk_is_semicolon(pc))
      {
         LOG_FMT(LFTOR, "%s: bailed on semicolon on line %zu\n",
                 __func__, pc->orig_line);
         return;
      }
      chunk_flags_set(pc, flags);
      pc = chunk_get_next_ncnl(pc, scope_e::PREPROC);
   }

   if (chunk_is_invalid(pc))
   {
      LOG_FMT(LFTOR, "%s: bailed on nullptr\n", __func__);
      return;
   }

   set_paren_parent(pc, start->type);

   pc = chunk_get_next_ncnl(pc, scope_e::PREPROC);
   while (chunk_is_valid(pc))
   {
      chunk_flags_set(pc, PCF_IN_CLASS);

      if ((pc->brace_level > level) ||
          (pc->level > pc->brace_level) ||
          (pc->flags & PCF_IN_PREPROC))
      {
         pc = chunk_get_next_ncnl(pc);
         continue;
      }

      if ((pc->type == CT_BRACE_CLOSE) && (pc->brace_level < level))
      {
         LOG_FMT(LFTOR, "%s: %zu] Hit brace close\n", __func__, pc->orig_line);
         pc = chunk_get_next_ncnl(pc, scope_e::PREPROC);
         if (pc && (pc->type == CT_SEMICOLON))
         {
            set_chunk_parent(pc, start->type);
         }
         return;
      }

      next = chunk_get_next_ncnl(pc, scope_e::PREPROC);
      if (chunkstack_match(cs, pc))
      {
         if ((next != nullptr) && (next->len() == 1) && (next->str[0] == '('))
         {
            set_chunk_type(pc, CT_FUNC_CLASS_DEF);
            LOG_FMT(LFTOR, "(%d) %zu] Marked CTor/DTor %s\n", __LINE__, pc->orig_line, pc->text());
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


static void mark_namespace(chunk_t *pns)
{
   LOG_FUNC_ENTRY();

   bool is_using = false;

   chunk_t *pc = chunk_get_prev_ncnl(pns);
   if (chunk_is_type(pc, CT_USING))
   {
      is_using = true;
      set_chunk_parent(pns, CT_USING);
   }

   chunk_t *br_close;
   pc = chunk_get_next_ncnl(pns);
   while (chunk_is_valid(pc))
   {
      set_chunk_parent(pc, CT_NAMESPACE);
      if (pc->type != CT_BRACE_OPEN)
      {
         if (chunk_is_type(pc, CT_SEMICOLON))
         {
            if (is_using)
            {
               set_chunk_parent(pc, CT_USING);
            }
            return;
         }
         pc = chunk_get_next_ncnl(pc);
         continue;
      }

      if ((cpd.settings[UO_indent_namespace_limit].u > 0) &&
          ((br_close = chunk_skip_to_match(pc)) != nullptr))
      {
         // br_close->orig_line is always >= pc->orig_line;
         size_t diff = br_close->orig_line - pc->orig_line;

         if (diff > cpd.settings[UO_indent_namespace_limit].u)
         {
            chunk_flags_set(pc, PCF_LONG_BLOCK);
            chunk_flags_set(br_close, PCF_LONG_BLOCK);
         }
      }
      flag_parens(pc, PCF_IN_NAMESPACE, CT_NONE, CT_NAMESPACE, false);
      return;
   }
}


static chunk_t *skip_align(chunk_t *start)
{
   assert(chunk_is_valid(start));
   chunk_t *pc = start;

   if (chunk_is_type(pc, CT_ALIGN))
   {
      pc = chunk_get_next_ncnl(pc);
      assert(chunk_is_valid(pc));
      if (chunk_is_type(pc, CT_PAREN_OPEN))
      {
         pc = chunk_get_next_type(pc, CT_PAREN_CLOSE, (int)pc->level);
         pc = chunk_get_next_ncnl(pc);
         assert(chunk_is_valid(pc));
         if (chunk_is_type(pc, CT_COLON))
         {
            pc = chunk_get_next_ncnl(pc);
         }
      }
   }
   return(pc);
}


static void mark_struct_union_body(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   chunk_t *pc = start;

   while ((pc != nullptr) &&
          (pc->level >= start->level) &&
          !((pc->level == start->level) && (pc->type == CT_BRACE_CLOSE)))
   {
      // LOG_FMT(LSYS, "%s: %d:%d %s:%s\n", __func__, pc->orig_line, pc->orig_col,
      //         pc->text(), get_token_name(pc->parent_type));
      if ((pc->type == CT_BRACE_OPEN) ||
          (pc->type == CT_BRACE_CLOSE) ||
          (pc->type == CT_SEMICOLON))
      {
         pc = chunk_get_next_ncnl(pc);
         break_if_invalid(pc);
      }
      if (chunk_is_type(pc, CT_ALIGN))
      {
         pc = skip_align(pc); // "align(x)" or "align(x):"
         break_if_invalid(pc);
      }
      else
      {
         pc = fix_var_def(pc);
         return_if_invalid(pc);
      }
   }
}


void mark_comments(void)
{
   LOG_FUNC_ENTRY();

   cpd.unc_stage = unc_stage_e::MARK_COMMENTS;

   bool    prev_nl = true;
   chunk_t *cur    = chunk_get_head();

   while (chunk_is_valid(cur))
   {
      chunk_t *next   = chunk_get_next_nvb(cur);
      bool    next_nl = (chunk_is_invalid(next)) || chunk_is_newline(next);

      if (chunk_is_comment(cur))
      {
         if      (next_nl && prev_nl) { set_chunk_parent(cur, CT_COMMENT_WHOLE); }
         else if (next_nl)            { set_chunk_parent(cur, CT_COMMENT_END  ); }
         else if (prev_nl)            { set_chunk_parent(cur, CT_COMMENT_START); }
         else                         { set_chunk_parent(cur, CT_COMMENT_EMBED); }
      }

      prev_nl = chunk_is_newline(cur);
      cur     = next;
   }
}


static void mark_define_expressions(void)
{
   LOG_FUNC_ENTRY();

   bool    in_define = false;
   bool    first     = true;
   chunk_t *pc       = chunk_get_head();
   chunk_t *prev     = pc;

   while (chunk_is_valid(pc))
   {
      if (!in_define)
      {
         if ((pc->type == CT_PP_DEFINE) ||
             (pc->type == CT_PP_IF) ||
             (pc->type == CT_PP_ELSE))
         {
            in_define = true;
            first     = true;
         }
      }
      else
      {
         if (((pc->flags & PCF_IN_PREPROC) == 0) || (pc->type == CT_PREPROC))
         {
            in_define = false;
         }
         else
         {
            if ((pc->type != CT_MACRO) &&
                (first ||
                 (prev->type == CT_PAREN_OPEN) ||
                 (prev->type == CT_ARITH) ||
                 (prev->type == CT_CARET) ||
                 (prev->type == CT_ASSIGN) ||
                 (prev->type == CT_COMPARE) ||
                 (prev->type == CT_RETURN) ||
                 (prev->type == CT_GOTO) ||
                 (prev->type == CT_CONTINUE) ||
                 (prev->type == CT_FPAREN_OPEN) ||
                 (prev->type == CT_SPAREN_OPEN) ||
                 (prev->type == CT_BRACE_OPEN) ||
                 chunk_is_semicolon(prev) ||
                 (prev->type == CT_COMMA) ||
                 (prev->type == CT_COLON) ||
                 (prev->type == CT_QUESTION)))
            {
               chunk_flags_set(pc, PCF_EXPR_START);
               first = false;
            }
         }
      }

      prev = pc;
      pc   = chunk_get_next(pc);
   }
}


static void handle_cpp_template(chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   chunk_t *tmp = chunk_get_next_ncnl(pc);
   assert(chunk_is_valid(tmp));
   if (tmp->type != CT_ANGLE_OPEN) { return; }

   set_chunk_parent(tmp, CT_TEMPLATE);

   size_t level = tmp->level;

   while ((tmp = chunk_get_next(tmp)) != nullptr)
   {
      if ((tmp->type == CT_CLASS) ||
          (tmp->type == CT_STRUCT))
      {
         set_chunk_type(tmp, CT_TYPE);
      }
      else if ((tmp->type == CT_ANGLE_CLOSE) && (tmp->level == level))
      {
         set_chunk_parent(tmp, CT_TEMPLATE);
         break;
      }
   }
   if (chunk_is_valid(tmp))
   {
      tmp = chunk_get_next_ncnl(tmp);
      if (chunk_is_type(tmp, 2, CT_CLASS, CT_STRUCT))
      {
         set_chunk_parent(tmp, CT_TEMPLATE);

         /* REVISIT: This may be a bit risky - might need to track the { }; */
         tmp = chunk_get_next_type(tmp, CT_SEMICOLON, (int)tmp->level);
         if (chunk_is_valid(tmp))
         {
            set_chunk_parent(tmp, CT_TEMPLATE);
         }
      }
   }
}


static void handle_cpp_lambda(chunk_t *sq_o)
{
   LOG_FUNC_ENTRY();

   chunk_t *ret = nullptr;

   chunk_t *sq_c = sq_o; /* assuming '[]' */
   if (chunk_is_type(sq_o, CT_SQUARE_OPEN))
   {
      /* make sure there is a ']' */
      sq_c = chunk_skip_to_match(sq_o);
      if (chunk_is_invalid(sq_c)) { return; }
   }

   /* Make sure a '(' is next */
   chunk_t *pa_o = chunk_get_next_ncnl(sq_c);
   if (chunk_is_invalid(pa_o) ||
       (pa_o->type != CT_PAREN_OPEN))
   {
      return;
   }
   /* and now find the ')' */
   chunk_t *pa_c = chunk_skip_to_match(pa_o);
   if (chunk_is_invalid(pa_c)) { return; }

   /* Check if keyword 'mutable' is before '->' */
   chunk_t *br_o = chunk_get_next_ncnl(pa_c);
   assert(chunk_is_valid(br_o));
   if (chunk_is_str(br_o, "mutable", 7))
   {
      br_o = chunk_get_next_ncnl(br_o);
   }
   assert(chunk_is_valid(br_o));

   /* Make sure a '{' or '->' is next */
   if (chunk_is_str(br_o, "->", 2))
   {
      ret = br_o;
      /* REVISIT: really should check the stuff we are skipping */
      br_o = chunk_get_next_type(br_o, CT_BRACE_OPEN, (int)br_o->level);
   }
   if (!br_o ||
       (br_o->type != CT_BRACE_OPEN))
   {
      return;
   }
   /* and now find the '}' */
   chunk_t *br_c = chunk_skip_to_match(br_o);
   if (!br_c)
   {
      return;
   }

   /* This looks like a lambda expression */
   if (chunk_is_type(sq_o, CT_TSQUARE))
   {
      /* split into two chunks */
      chunk_t nc;

      nc = *sq_o;
      set_chunk_type(sq_o, CT_SQUARE_OPEN);
      sq_o->str.resize(1);
      // bug # 664
      // The original orig_col of CT_SQUARE_CLOSE is stored at orig_col_end of CT_TSQUARE.
      // CT_SQUARE_CLOSE orig_col and orig_col_end values are calculate from orig_col_end of CT_TSQUARE.
      nc.orig_col        = sq_o->orig_col_end - 1;
      nc.column          = static_cast<int>(nc.orig_col);
      nc.orig_col_end    = sq_o->orig_col_end;
      sq_o->orig_col_end = sq_o->orig_col + 1;

      nc.type = CT_SQUARE_CLOSE;
      nc.str.pop_front();
      sq_c = chunk_add_after(&nc, sq_o);
   }
   set_chunk_parent(sq_o, CT_CPP_LAMBDA  );
   set_chunk_parent(sq_c, CT_CPP_LAMBDA  );
   set_chunk_type  (pa_o, CT_FPAREN_OPEN );
   set_chunk_parent(pa_o, CT_CPP_LAMBDA  );
   set_chunk_type  (pa_c, CT_FPAREN_CLOSE);
   set_chunk_parent(pa_c, CT_CPP_LAMBDA  );
   set_chunk_parent(br_o, CT_CPP_LAMBDA  );
   set_chunk_parent(br_c, CT_CPP_LAMBDA  );

   if (chunk_is_valid(ret))
   {
      set_chunk_type(ret, CT_CPP_LAMBDA_RET);
      ret = chunk_get_next_ncnl(ret);
      while (ret != br_o)
      {
         make_type(ret);
         ret = chunk_get_next_ncnl(ret);
      }
   }

   fix_fcn_def_params(pa_o);
}


static chunk_t *get_d_template_types(ChunkStack &cs, chunk_t *open_paren)
{
   LOG_FUNC_ENTRY();
   chunk_t *tmp       = open_paren;
   bool    maybe_type = true;

   while (((tmp = chunk_get_next_ncnl(tmp)) != nullptr) &&
          (tmp->level > open_paren->level))
   {
      if ((tmp->type == CT_TYPE) ||
          (tmp->type == CT_WORD))
      {
         if (maybe_type)
         {
            make_type(tmp);
            cs.Push_Back(tmp);
         }
         maybe_type = false;
      }
      else if (chunk_is_type(tmp, CT_COMMA))
      {
         maybe_type = true;
      }
   }
   return(tmp);
}


static bool chunkstack_match(const ChunkStack &cs, chunk_t *pc)
{
   for (size_t idx = 0; idx < cs.Len(); idx++)
   {
      const chunk_t *tmp = cs.GetChunk(idx);
      assert(chunk_is_valid(tmp));
      if (pc->str.equals(tmp->str))
      {
         return(true);
      }
   }
   return(false);
}


static void handle_d_template(chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   chunk_t *name = chunk_get_next_ncnl(pc);
   chunk_t *po   = chunk_get_next_ncnl(name);
   //if (!name || ((name->type != CT_WORD) && (name->type != CT_WORD)))  Coverity CID 76000 Same on both sides, 2016-03-16
   if (!name ||
       (name->type != CT_WORD))
   {
      /* TODO: log an error, expected NAME */
      return;
   }
   if (!po ||
       (po->type != CT_PAREN_OPEN))
   {
      /* TODO: log an error, expected '(' */
      return;
   }

   set_chunk_type  (name, CT_TYPE    );
   set_chunk_parent(name, CT_TEMPLATE);
   set_chunk_parent(po,   CT_TEMPLATE);

   ChunkStack cs;
   chunk_t    *tmp = get_d_template_types(cs, po);

   if (!tmp ||
       (tmp->type != CT_PAREN_CLOSE))
   {
      /* TODO: log an error, expected ')' */
      return;
   }
   set_chunk_parent(tmp, CT_TEMPLATE);

   tmp = chunk_get_next_ncnl(tmp);
   assert(chunk_is_valid(tmp));
   if (tmp->type != CT_BRACE_OPEN)
   {
      /* TODO: log an error, expected '{' */
      return;
   }
   set_chunk_parent(tmp, CT_TEMPLATE);
   po = tmp;

   tmp = po;
   while (((tmp = chunk_get_next_ncnl(tmp)) != nullptr) &&
          (tmp->level > po->level))
   {
      if ((tmp->type == CT_WORD) && chunkstack_match(cs, tmp))
      {
         set_chunk_type(tmp, CT_TYPE);
      }
   }
   assert(chunk_is_valid(tmp));
   if (tmp->type != CT_BRACE_CLOSE)
   {
      /* TODO: log an error, expected '}' */
   }
   set_chunk_parent(tmp, CT_TEMPLATE);
}


static void mark_template_func(chunk_t *pc, chunk_t *pc_next)
{
   LOG_FUNC_ENTRY();

   /* We know angle_close must be there... */
   chunk_t *angle_close = chunk_get_next_type(pc_next, CT_ANGLE_CLOSE, (int)pc->level);
   chunk_t *after       = chunk_get_next_ncnl(angle_close);
   if (chunk_is_valid(after))
   {
      if (chunk_is_str(after, "(", 1))
      {
         assert(chunk_is_valid(angle_close));
         if (angle_close->flags & PCF_IN_FCN_CALL)
         {
            LOG_FMT(LTEMPFUNC, "%s: marking '%s' in line %zu as a FUNC_CALL\n",
                    __func__, pc->text(), pc->orig_line);
            set_chunk_type(pc, CT_FUNC_CALL);
            flag_parens(after, PCF_IN_FCN_CALL, CT_FPAREN_OPEN, CT_FUNC_CALL, false);
         }
         else
         {
            /* Might be a function def. Must check what is before the template:
             * Func call:
             *   BTree.Insert(std::pair<int, double>(*it, double(*it) + 1.0));
             *   a = Test<int>(j);
             *   std::pair<int, double>(*it, double(*it) + 1.0));
             */

            LOG_FMT(LTEMPFUNC, "%s: marking '%s' in line %zu as a FUNC_CALL 2\n",
                    __func__, pc->text(), pc->orig_line);
            // its a function!!!
            set_chunk_type(pc, CT_FUNC_CALL);
            mark_function(pc);
         }
      }
      else if (chunk_is_type(after, CT_WORD))
      {
         // its a type!
         set_chunk_type(pc, CT_TYPE);
         chunk_flags_set(pc, PCF_VAR_TYPE);
         chunk_flags_set(after, PCF_VAR_DEF);
      }
   }
}


static void mark_exec_sql(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   chunk_t *tmp;

   /* Change CT_WORD to CT_SQL_WORD */
   for (tmp = chunk_get_next(pc); chunk_is_valid(tmp); tmp = chunk_get_next(tmp))
   {
      set_chunk_parent(tmp, pc->type);
      if (chunk_is_type(tmp, CT_WORD))
      {
         set_chunk_type(tmp, CT_SQL_WORD);
      }
      if (chunk_is_type(tmp, CT_SEMICOLON)) { break; }
   }

   if ((pc->type != CT_SQL_BEGIN) ||
       (chunk_is_invalid(tmp)) ||
       (tmp->type != CT_SEMICOLON))
   {
      return;
   }

   for (tmp = chunk_get_next(tmp);
        chunk_is_not_type(tmp, CT_SQL_END);
        tmp = chunk_get_next(tmp))
   {
      tmp->level++;
   }
}


chunk_t *skip_template_next(chunk_t *ang_open)
{
   if (chunk_is_type(ang_open, CT_ANGLE_OPEN))
   {
      chunk_t *pc = chunk_get_next_type(ang_open, CT_ANGLE_CLOSE, (int)ang_open->level);
      return(chunk_get_next_ncnl(pc));
   }
   return(ang_open);
}


chunk_t *skip_template_prev(chunk_t *ang_close)
{
   if (chunk_is_type(ang_close, CT_ANGLE_CLOSE))
   {
      chunk_t *pc = chunk_get_prev_type(ang_close, CT_ANGLE_OPEN, (int)ang_close->level);
      return(chunk_get_prev_ncnl(pc));
   }
   return(ang_close);
}


chunk_t *skip_tsquare_next(chunk_t *ary_def)
{
   if (chunk_is_type(ary_def, 2,  CT_SQUARE_OPEN, CT_TSQUARE))
   {
      return(chunk_get_next_nisq(ary_def));
   }
   return(ary_def);
}


chunk_t *skip_attribute_next(chunk_t *attr)
{
   if (chunk_is_type(attr, CT_ATTRIBUTE))
   {
      chunk_t *pc = chunk_get_next(attr);
      if (chunk_is_type(pc, CT_FPAREN_OPEN))
      {
         pc = chunk_get_next_type(attr, CT_FPAREN_CLOSE, (int)attr->level);
         return(chunk_get_next_ncnl(pc));
      }
      return(pc);
   }
   return(attr);
}


chunk_t *skip_attribute_prev(chunk_t *fp_close)
{
   if (chunk_is_type       (fp_close, CT_FPAREN_CLOSE) &&
       chunk_is_ptype(fp_close, CT_ATTRIBUTE   ) )
   {
      chunk_t *pc = chunk_get_prev_type(fp_close, CT_ATTRIBUTE, (int)fp_close->level);
      return(chunk_get_prev_ncnl(pc));
   }
   return(fp_close);
}


static void handle_oc_class(chunk_t *pc)
{
   enum class angle_state_e : unsigned int
   {
      NONE  = 0,
      OPEN  = 1, // '<' found
      CLOSE = 2  // '>' found
   };

   LOG_FUNC_ENTRY();
   chunk_t       *tmp;
   bool          hit_scope     = false;
   bool          passed_name   = false; // Did we pass the name of the class and now there can be only protocols, not generics
   int           generic_level = 0;     // level of depth of generic
   angle_state_e as            = angle_state_e::NONE;

   LOG_FMT(LOCCLASS, "%s: start [%s] [%s] line %zu\n",
           __func__, pc->text(), get_token_name(pc->parent_type), pc->orig_line);

   if (pc->parent_type == CT_OC_PROTOCOL)
   {
      tmp = chunk_get_next_ncnl(pc);
      if (chunk_is_semicolon(tmp))
      {
         set_chunk_parent(tmp, pc->parent_type);
         LOG_FMT(LOCCLASS, "%s:   bail on semicolon\n", __func__);
         return;
      }
   }

   tmp = pc;
   while ((tmp = chunk_get_next_nnl(tmp)) != nullptr)
   {
      LOG_FMT(LOCCLASS, "%s:       %zu [%s]\n",
              __func__, tmp->orig_line, tmp->text());

      if (chunk_is_type(tmp, CT_OC_END))
      {
         break;
      }
      if (chunk_is_type(tmp, CT_PAREN_OPEN))
      {
         passed_name = true;
      }
      if (chunk_is_str(tmp, "<", 1))
      {
         set_chunk_type(tmp, CT_ANGLE_OPEN);
         if (passed_name)
         {
            set_chunk_parent(tmp, CT_OC_PROTO_LIST);
         }
         else
         {
            set_chunk_parent(tmp, CT_OC_GENERIC_SPEC);
            generic_level++;
         }
         as = angle_state_e::OPEN;
      }
      if (chunk_is_str(tmp, ">", 1))
      {
         set_chunk_type(tmp, CT_ANGLE_CLOSE);
         if (passed_name)
         {
            set_chunk_parent(tmp, CT_OC_PROTO_LIST);
            as = angle_state_e::CLOSE;
         }
         else
         {
            set_chunk_parent(tmp, CT_OC_GENERIC_SPEC);
            generic_level--;
            if (generic_level == 0)
            {
               as = angle_state_e::CLOSE;
            }
         }
      }
      if (chunk_is_str(tmp, ">>", 2))
      {
         set_chunk_type(tmp, CT_ANGLE_CLOSE);
         set_chunk_parent(tmp, CT_OC_GENERIC_SPEC);
         split_off_angle_close(tmp);
         generic_level -= 1;
         if (generic_level == 0)
         {
            as = angle_state_e::CLOSE;
         }
      }
      if (chunk_is_type(tmp, CT_BRACE_OPEN))
      {
         as = angle_state_e::CLOSE;
         set_chunk_parent(tmp, CT_OC_CLASS);
         tmp = chunk_get_next_type(tmp, CT_BRACE_CLOSE, (int)tmp->level);
         if (chunk_is_valid(tmp))
         {
            set_chunk_parent(tmp, CT_OC_CLASS);
         }
      }
      else if (chunk_is_type(tmp, CT_COLON))
      {
         if (as != angle_state_e::OPEN)
         {
            passed_name = true;
         }
         set_chunk_type(tmp, hit_scope ? CT_OC_COLON : CT_CLASS_COLON);
         if (chunk_is_type(tmp, CT_CLASS_COLON))
         {
            set_chunk_parent(tmp, CT_OC_CLASS);
         }
      }
      else if (chunk_is_str(tmp, "-", 1) || chunk_is_str(tmp, "+", 1))
      {
         as = angle_state_e::CLOSE;
         if (chunk_is_newline(chunk_get_prev(tmp)))
         {
            set_chunk_type(tmp, CT_OC_SCOPE);
            chunk_flags_set(tmp, PCF_STMT_START);
            hit_scope = true;
         }
      }
      if (as == angle_state_e::OPEN)
      {
         if (passed_name) { set_chunk_parent(tmp, CT_OC_PROTO_LIST  ); }
         else             { set_chunk_parent(tmp, CT_OC_GENERIC_SPEC); }
      }
   }

   if (chunk_is_type(tmp, CT_BRACE_OPEN))
   {
      tmp = chunk_get_next_type(tmp, CT_BRACE_CLOSE, (int)tmp->level);
      if (chunk_is_valid(tmp))
      {
         set_chunk_parent(tmp, CT_OC_CLASS);
      }
   }
}


static void handle_oc_block_literal(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   const chunk_t *prev = chunk_get_prev_ncnl(pc);
   chunk_t *next = chunk_get_next_ncnl(pc);

   if (!pc || !prev || !next)
   {
      return; /* let's be paranoid */
   }

   /* block literal: '^ RTYPE ( ARGS ) { }'
    * RTYPE and ARGS are optional
    */
   LOG_FMT(LOCBLK, "%s: block literal @ %zu:%zu\n", __func__, pc->orig_line, pc->orig_col);

   chunk_t *apo = nullptr; /* arg paren open */
   chunk_t *bbo = nullptr; /* block brace open */
   chunk_t *bbc;           /* block brace close */

   LOG_FMT(LOCBLK, "%s:  + scan", __func__);
   chunk_t *tmp;
   for (tmp = next; tmp; tmp = chunk_get_next_ncnl(tmp))
   {
      LOG_FMT(LOCBLK, " %s", tmp->text());
      if ((tmp->level < pc->level   ) ||
          (chunk_is_type(tmp, CT_SEMICOLON) ))
      {
         LOG_FMT(LOCBLK, "[DONE]");
         break;
      }
      if (tmp->level == pc->level)
      {
         if (chunk_is_paren_open(tmp))
         {
            LOG_FMT(LOCBLK, "[PAREN]");
            apo = tmp;
         }
         if (chunk_is_type(tmp, CT_BRACE_OPEN))
         {
            LOG_FMT(LOCBLK, "[BRACE]");
            bbo = tmp;
            break;
         }
      }
   }

   /* make sure we have braces */
   bbc = chunk_skip_to_match(bbo);
   if (!chunks_are_valid(bbo, bbc))
   {
      LOG_FMT(LOCBLK, " -- no braces found\n");
      return;
   }
   LOG_FMT(LOCBLK, "\n");

   /* we are on a block literal for sure */
   set_chunk_type(pc, CT_OC_BLOCK_CARET);
   set_chunk_parent(pc, CT_OC_BLOCK_EXPR);

   /* handle the optional args */
   chunk_t *lbp; /* last before paren - end of return type, if any */
   if (apo)
   {
      chunk_t *apc = chunk_skip_to_match(apo);  /* arg paren close */
      if (chunk_is_paren_close(apc))
      {
         LOG_FMT(LOCBLK, " -- marking parens @ %zu:%zu and %zu:%zu\n",
                 apo->orig_line, apo->orig_col, apc->orig_line, apc->orig_col);
         flag_parens(apo, PCF_OC_ATYPE, CT_FPAREN_OPEN, CT_OC_BLOCK_EXPR, true);
         fix_fcn_def_params(apo);
      }
      lbp = chunk_get_prev_ncnl(apo);
   }
   else
   {
      lbp = chunk_get_prev_ncnl(bbo);
   }

   /* mark the return type, if any */
   while (lbp != pc)
   {
      assert(chunk_is_valid(lbp));
      LOG_FMT(LOCBLK, " -- lbp %s[%s]\n", lbp->text(), get_token_name(lbp->type));
      make_type(lbp);
      chunk_flags_set(lbp, PCF_OC_RTYPE);
      set_chunk_parent(lbp, CT_OC_BLOCK_EXPR);
      lbp = chunk_get_prev_ncnl(lbp);
   }
   /* mark the braces */
   set_chunk_parent(bbo, CT_OC_BLOCK_EXPR);
   set_chunk_parent(bbc, CT_OC_BLOCK_EXPR);
}


static void handle_oc_block_type(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   if (!pc)
   {
      return;
   }

   if (pc->flags & PCF_IN_TYPEDEF)
   {
      LOG_FMT(LOCBLK, "%s: skip block type @ %zu:%zu -- in typedef\n",
              __func__, pc->orig_line, pc->orig_col);
      return;
   }

   /* make sure we have '( ^' */
   chunk_t *tpo = chunk_get_prev_ncnl(pc); /* type paren open */
   if (chunk_is_paren_open(tpo))
   {
      /* block type: 'RTYPE (^LABEL)(ARGS)'
       * LABEL is optional.
       */
      chunk_t *tpc = chunk_skip_to_match(tpo);  /* type close paren (after '^') */
      chunk_t *nam = chunk_get_prev_ncnl(tpc);  /* name (if any) or '^' */
      chunk_t *apo = chunk_get_next_ncnl(tpc);  /* arg open paren */
      chunk_t *apc = chunk_skip_to_match(apo);  /* arg close paren */

      // If this is a block literal instead of a block type, 'nam' will actually
      // be the closing bracket of the block.
      // We run into this situation if a block literal is enclosed in parentheses.
      if (chunk_is_closing_brace(nam))
      {
         return(handle_oc_block_literal(pc));
      }

      if (chunk_is_paren_close(apc))
      {
         chunk_t   *aft = chunk_get_next_ncnl(apc);
         c_token_t pt;

         if (chunk_is_str(nam, "^", 1))
         {
            set_chunk_type(nam, CT_PTR_TYPE);
            pt = CT_FUNC_TYPE;
         }
         else if (chunk_is_type(aft, 2, CT_ASSIGN, CT_SEMICOLON))
         {
            set_chunk_type(nam, CT_FUNC_VAR);
            pt = CT_FUNC_VAR;
         }
         else
         {
            set_chunk_type(nam, CT_FUNC_TYPE);
            pt = CT_FUNC_TYPE;
         }
         assert(chunk_is_valid(nam));
         LOG_FMT(LOCBLK, "%s: block type @ %zu:%zu (%s)[%s]\n",
                 __func__, pc->orig_line, pc->orig_col, nam->text(), get_token_name(nam->type));
         set_chunk_type  (pc, CT_PTR_TYPE);
         set_chunk_parent(pc, pt);  //CT_OC_BLOCK_TYPE;
         set_chunk_type  (tpo, CT_TPAREN_OPEN);
         set_chunk_parent(tpo, pt); //CT_OC_BLOCK_TYPE;
         set_chunk_type  (tpc, CT_TPAREN_CLOSE);
         set_chunk_parent(tpc, pt); //CT_OC_BLOCK_TYPE;
         set_chunk_type  (apo, CT_FPAREN_OPEN);
         set_chunk_parent(apo, CT_FUNC_PROTO);
         set_chunk_type  (apc, CT_FPAREN_CLOSE);
         set_chunk_parent(apc, CT_FUNC_PROTO);
         fix_fcn_def_params(apo);
         mark_function_return_type(nam, chunk_get_prev_ncnl(tpo), pt);
      }
   }
}


static chunk_t *handle_oc_md_type(chunk_t *paren_open, c_token_t ptype, UINT64 flags, bool &did_it)
{
   chunk_t *paren_close;

   if (!chunk_is_paren_open(paren_open) ||
       ((paren_close = chunk_skip_to_match(paren_open)) == nullptr))
   {
      did_it = false;
      return(paren_open);
   }

   did_it = true;

   set_chunk_parent(paren_open, ptype);
   chunk_flags_set(paren_open, flags);
   set_chunk_parent(paren_close, ptype);
   chunk_flags_set(paren_close, flags);

   for (chunk_t *cur = chunk_get_next_ncnl(paren_open);
        cur != paren_close;
        cur = chunk_get_next_ncnl(cur))
   {
      assert(chunk_is_valid(cur));
      LOG_FMT(LOCMSGD, " <%s|%s>", cur->text(), get_token_name(cur->type));
      chunk_flags_set(cur, flags);
      make_type(cur);
   }

   /* returning the chunk after the paren close */
   return(chunk_get_next_ncnl(paren_close));
}


static void handle_oc_message_decl(chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   /* Figure out if this is a spec or declaration */
   chunk_t *tmp = pc;
   while ((tmp = chunk_get_next(tmp)) != nullptr)
   {
      if (tmp->level < pc->level) { return; } /* should not happen */

      if (chunk_is_type(tmp, 2, CT_SEMICOLON, CT_BRACE_OPEN)) { break; }
   }
   if (chunk_is_invalid(tmp)) { return; }

   c_token_t pt = (chunk_is_type(tmp, CT_SEMICOLON)) ? CT_OC_MSG_SPEC : CT_OC_MSG_DECL;

   set_chunk_type(pc, CT_OC_SCOPE);
   set_chunk_parent(pc, pt);

   LOG_FMT(LOCMSGD, "%s: %s @ %zu:%zu -", __func__, get_token_name(pt), pc->orig_line, pc->orig_col);

   /* format: -(TYPE) NAME [: (TYPE)NAME */

   /* handle the return type */
   bool did_it;
   tmp = handle_oc_md_type(chunk_get_next_ncnl(pc), pt, PCF_OC_RTYPE, did_it);
   if (did_it == false)              { LOG_FMT(LOCMSGD, " -- missing type parens\n"); return; }
   if (!chunk_is_type(tmp, CT_WORD)) { LOG_FMT(LOCMSGD, " -- missing method name\n"); return; } /* expect the method name/label */

   chunk_t *label = tmp;
   set_chunk_type  (tmp, pt);
   set_chunk_parent(tmp, pt);
   pc = chunk_get_next_ncnl(tmp);
   assert(chunk_is_valid(pc));
   LOG_FMT(LOCMSGD, " [%s]%s", pc->text(), get_token_name(pc->type));

   /* if we have a colon next, we have args */
   if (chunk_is_type(pc,2, CT_COLON, CT_OC_COLON))
   {
      pc = label;

      while (true)
      {
         /* skip optional label */
         if (chunk_is_type(pc, 2, CT_WORD, pt))
         {
            set_chunk_parent(pc, pt);
            pc = chunk_get_next_ncnl(pc);
         }
         /* a colon must be next */
         if (!chunk_is_str(pc, ":", 1)) { break; }

         set_chunk_type  (pc, CT_OC_COLON);
         set_chunk_parent(pc, pt);
         pc = chunk_get_next_ncnl(pc);
         assert(chunk_is_valid(pc));

         /* next is the type in parens */
         LOG_FMT(LOCMSGD, "  (%s)", pc->text());
         tmp = handle_oc_md_type(pc, pt, PCF_OC_ATYPE, did_it);
         if (!did_it)
         {
            LOG_FMT(LWARN, "%s: %zu:%zu expected type\n", __func__, pc->orig_line, pc->orig_col);
            break;
         }
         pc = tmp;
         assert(chunk_is_valid(pc));
         /* we should now be on the arg name */
         chunk_flags_set(pc, PCF_VAR_DEF);
         LOG_FMT(LOCMSGD, " arg[%s]", pc->text());
         pc = chunk_get_next_ncnl(pc);
      }
   }

   assert(chunk_is_valid(pc));
   LOG_FMT(LOCMSGD, " end[%s]", pc->text());

   if (chunk_is_type(pc, CT_BRACE_OPEN))
   {
      set_chunk_parent(pc, pt);
      pc = chunk_skip_to_match(pc);
      if (chunk_is_valid(pc)) { set_chunk_parent(pc, pt); }
   }
   else if (chunk_is_type(pc, CT_SEMICOLON)) { set_chunk_parent(pc, pt); }

   LOG_FMT(LOCMSGD, "\n");
}


static void handle_oc_message_send(chunk_t *os)
{
   LOG_FUNC_ENTRY();

   chunk_t *cs = chunk_get_next(os);

   while ((chunk_is_valid(cs)) &&
           (cs->level > os->level))
   {
      cs = chunk_get_next(cs);
   }

   if (chunk_is_invalid   (cs                 ) ||
        chunk_is_not_type(cs, CT_SQUARE_CLOSE) )
   {
      return;
   }

   LOG_FMT(LOCMSG, "%s: line %zu, col %zu\n", __func__, os->orig_line, os->orig_col);

   chunk_t *tmp = chunk_get_next_ncnl(cs);
   if (chunk_is_semicolon(tmp))
   {
      set_chunk_parent(tmp, CT_OC_MSG);
   }

   set_chunk_parent(os, CT_OC_MSG); chunk_flags_set (os, PCF_IN_OC_MSG);
   set_chunk_parent(cs, CT_OC_MSG); chunk_flags_set (cs, PCF_IN_OC_MSG);

   /* expect a word first thing or [...] */
   tmp = chunk_get_next_ncnl(os);
   assert(chunk_is_valid(tmp));
   if (chunk_is_type(tmp, 2, CT_SQUARE_OPEN, CT_PAREN_OPEN))
   {
      tmp = chunk_skip_to_match(tmp);
   }
   else if ((tmp->type != CT_WORD) && (tmp->type != CT_TYPE) && (tmp->type != CT_STRING))
   {
      LOG_FMT(LOCMSG, "%s: %zu:%zu expected identifier, not '%s' [%s]\n",
              __func__, tmp->orig_line, tmp->orig_col,
              tmp->text(), get_token_name(tmp->type));
      return;
   }
   else
   {
      chunk_t *tt = chunk_get_next_ncnl(tmp);
      if (chunk_is_paren_open(tt))
      {
         set_chunk_type(tmp, CT_FUNC_CALL);
         tmp = chunk_get_prev_ncnl(set_paren_parent(tt, CT_FUNC_CALL));
      }
      else
      {
         set_chunk_type(tmp, CT_OC_MSG_CLASS);
      }
   }

   /* handle '< protocol >' */
   tmp = chunk_get_next_ncnl(tmp);
   if (chunk_is_str(tmp, "<", 1))
   {
      chunk_t *ao = tmp;
      assert(chunk_is_valid(ao));
      chunk_t *ac = chunk_get_next_str(ao, ">", 1, (int)ao->level);

      if (ac)
      {
         set_chunk_and_parent_type(ao, CT_ANGLE_OPEN,  CT_OC_PROTO_LIST);
         set_chunk_and_parent_type(ac, CT_ANGLE_CLOSE, CT_OC_PROTO_LIST);
         for (tmp = chunk_get_next(ao); tmp != ac; tmp = chunk_get_next(tmp))
         {
            assert(chunk_is_valid(tmp));
            tmp->level += 1;
            set_chunk_parent(tmp, CT_OC_PROTO_LIST);
         }
      }
      tmp = chunk_get_next_ncnl(ac);
   }
   /* handle 'object.property' and 'collection[index]' */
   else
   {
      while (chunk_is_valid(tmp))
      {
         if (chunk_is_type(tmp, CT_MEMBER))  /* move past [object.prop1.prop2  */
         {
            chunk_t *typ = chunk_get_next_ncnl(tmp);
            if (chunk_is_type(typ, 2, CT_WORD, CT_TYPE))
            {
               tmp = chunk_get_next_ncnl(typ);
            }
            else { break; }
         }
         else if (chunk_is_type(tmp, CT_SQUARE_OPEN))  /* move past [collection[index]  */
         {
            chunk_t *tcs = chunk_get_next_ncnl(tmp);
            while ((chunk_is_valid(tcs)) &&
                   (tcs->level > tmp->level) )
            {
               tcs = chunk_get_next_ncnl(tcs);
            }
            if (chunk_is_type(tcs, CT_SQUARE_CLOSE))
            {
               tmp = chunk_get_next_ncnl(tcs);
            }
            else { break; }
         }
         else { break; }
      }
   }

   if (chunk_is_type(tmp, 2, CT_WORD, CT_TYPE)) { set_chunk_type(tmp, CT_OC_MSG_FUNC); }

   chunk_t *prev = nullptr;

   for (tmp = chunk_get_next(os); tmp != cs; tmp = chunk_get_next(tmp))
   {
      assert(chunk_is_valid(tmp));
      chunk_flags_set(tmp, PCF_IN_OC_MSG);
      if (tmp->level == cs->level + 1)
      {
         if (chunk_is_type(tmp, CT_COLON))
         {
            set_chunk_type(tmp, CT_OC_COLON);
            if (chunk_is_type(prev, 2, CT_WORD, CT_TYPE))
            {
               /* Might be a named param, check previous block */
               chunk_t *pp = chunk_get_prev(prev);
               if (chunk_is_not_type(pp, 3, CT_OC_COLON, CT_ARITH, CT_CARET) )
               {
                  set_chunk_type  (prev, CT_OC_MSG_NAME);
                  set_chunk_parent(tmp,  CT_OC_MSG_NAME);
               }
            }
         }
      }
      prev = tmp;
   }
}


static void handle_oc_property_decl(chunk_t *os)
{
   if (cpd.settings[UO_mod_sort_oc_properties].b)
   {
      typedef std::vector<chunk_t *> ChunkGroup;

      chunk_t *open_paren = nullptr;
      chunk_t *next       = chunk_get_next(os);
      assert(chunk_is_valid(next));

      std::vector<ChunkGroup> thread_chunks;      // atomic/nonatomic
      std::vector<ChunkGroup> readwrite_chunks;   // readwrite, readonly
      std::vector<ChunkGroup> ref_chunks;         // retain, copy, assign, weak, strong, unsafe_unretained
      std::vector<ChunkGroup> getter_chunks;      // getter
      std::vector<ChunkGroup> setter_chunks;      // setter
      std::vector<ChunkGroup> nullability_chunks; // nonnull/nullable


      if (chunk_is_type(next, CT_PAREN_OPEN))
      {
         open_paren = next;
         next       = chunk_get_next(next);

         // Determine location of the property attributes
         // NOTE: Did not do this in the combine.cpp do_symbol_check as I was not sure
         // what the ramifications of adding a new type for each of the below types would
         // be. It did break some items when I attempted to add them so this is my hack for
         // now.
         while (chunk_is_not_type(next, CT_PAREN_CLOSE))
         {
            if (chunk_is_type(next, CT_WORD))
            {  /* \todo convert into switch */
               if (chunk_is_str(next, "atomic", 6))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  thread_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "nonatomic", 9))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  thread_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "readonly", 8))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  readwrite_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "readwrite", 9))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  readwrite_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "assign", 6))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  ref_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "retain", 6))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  ref_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "copy", 4))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  ref_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "strong", 6))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  ref_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "weak", 4))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  ref_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "unsafe_unretained", 17))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  ref_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "getter", 6))
               {
                  ChunkGroup chunkGroup;
                  do
                  {
                     chunkGroup.push_back(next);
                     next = chunk_get_next(next);
                  } while (chunk_is_not_type(next, 2, CT_COMMA, CT_PAREN_CLOSE));
                  assert(chunk_is_valid(next));
                  next = next->prev;
                  if (chunk_is_invalid(next)) { break; }

                  getter_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "setter", 6))
               {
                  ChunkGroup chunkGroup;
                  do
                  {
                     chunkGroup.push_back(next);
                     next = chunk_get_next(next);
                  } while (chunk_is_not_type(next, 2, CT_COMMA, CT_PAREN_CLOSE));
                  assert(chunk_is_valid(next));
                  next = next->prev;
                  if (chunk_is_invalid(next)) { break; }

                  setter_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "nullable", 8))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  nullability_chunks.push_back(chunkGroup);
               }
               else if (chunk_is_str(next, "nonnull", 7))
               {
                  ChunkGroup chunkGroup;
                  chunkGroup.push_back(next);
                  nullability_chunks.push_back(chunkGroup);
               }
            }
            next = chunk_get_next(next);
         }

         int thread_w      = cpd.settings[UO_mod_sort_oc_property_thread_safe_weight].n;
         int readwrite_w   = cpd.settings[UO_mod_sort_oc_property_readwrite_weight  ].n;
         int ref_w         = cpd.settings[UO_mod_sort_oc_property_reference_weight  ].n;
         int getter_w      = cpd.settings[UO_mod_sort_oc_property_getter_weight     ].n;
         int setter_w      = cpd.settings[UO_mod_sort_oc_property_setter_weight     ].n;
         int nullability_w = cpd.settings[UO_mod_sort_oc_property_nullability_weight].n;

         std::multimap<int, std::vector<ChunkGroup> > sorted_chunk_map;
         sorted_chunk_map.insert(pair<int, std::vector<ChunkGroup> >(thread_w,      thread_chunks));
         sorted_chunk_map.insert(pair<int, std::vector<ChunkGroup> >(readwrite_w,   readwrite_chunks));
         sorted_chunk_map.insert(pair<int, std::vector<ChunkGroup> >(ref_w,         ref_chunks));
         sorted_chunk_map.insert(pair<int, std::vector<ChunkGroup> >(getter_w,      getter_chunks));
         sorted_chunk_map.insert(pair<int, std::vector<ChunkGroup> >(setter_w,      setter_chunks));
         sorted_chunk_map.insert(pair<int, std::vector<ChunkGroup> >(nullability_w, nullability_chunks));

         chunk_t *curr_chunk = open_paren;
         for (multimap<int, std::vector<ChunkGroup> >::reverse_iterator it = sorted_chunk_map.rbegin(); it != sorted_chunk_map.rend(); ++it)
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

               /* add the parens */
               assert(chunk_is_valid(curr_chunk));
               chunk_t endchunk;
               endchunk.type        = CT_COMMA;
               endchunk.str         = ", ";
               endchunk.level       = curr_chunk->level;
               endchunk.brace_level = curr_chunk->brace_level;
               endchunk.orig_line   = curr_chunk->orig_line;
               endchunk.column      = static_cast<int>(curr_chunk->orig_col_end) + 1u;
               endchunk.parent_type = curr_chunk->parent_type;
               endchunk.flags       = curr_chunk->flags & PCF_COPY_FLAGS;
               chunk_add_after(&endchunk, curr_chunk);
               curr_chunk = curr_chunk->next;
            }
         }

         // Remove the extra comma's that we did not move
         while (curr_chunk && curr_chunk->type != CT_PAREN_CLOSE)
         {
            chunk_t *rm_chunk = curr_chunk;
            curr_chunk = chunk_get_next(curr_chunk);
            chunk_del(rm_chunk);
         }
      }
   }


   chunk_t *tmp = chunk_get_next_ncnl(os);
   if (chunk_is_paren_open(tmp))
   {
      tmp = chunk_get_next_ncnl(chunk_skip_to_match(tmp));
   }
   fix_var_def(tmp);
}


static void handle_cs_square_stmt(chunk_t *os)
{
   LOG_FUNC_ENTRY();

   chunk_t *cs = chunk_get_next(os);
   while ((chunk_is_valid(cs)) &&
          (cs->level > os->level))
   {
      cs = chunk_get_next(cs);
   }

   if ((chunk_is_invalid(cs)) ||
        (cs->type != CT_SQUARE_CLOSE))
   {
      return;
   }

   set_chunk_parent(os, CT_CS_SQ_STMT);
   set_chunk_parent(cs, CT_CS_SQ_STMT);

   chunk_t *tmp;
   for (tmp = chunk_get_next(os); tmp != cs; tmp = chunk_get_next(tmp))
   {
      assert(chunk_is_valid(tmp));
      set_chunk_parent(tmp, CT_CS_SQ_STMT);
      if (chunk_is_type(tmp, CT_COLON)) { set_chunk_type(tmp, CT_CS_SQ_COLON); }
   }

   tmp = chunk_get_next_ncnl(cs);
   if (chunk_is_valid(tmp)) { chunk_flags_set(tmp, PCF_STMT_START | PCF_EXPR_START); }
}


static void handle_cs_property(chunk_t *bro)
{
   LOG_FUNC_ENTRY();

   set_paren_parent(bro, CT_CS_PROPERTY);

   bool    did_prop = false;
   chunk_t *pc      = bro;
   while ((pc = chunk_get_prev_ncnl(pc)) != nullptr)
   {
      if (pc->level == bro->level)
      {
         if ((did_prop == false) &&
             (chunk_is_type(pc, 2, CT_WORD, CT_THIS)))
         {
            set_chunk_type(pc, CT_CS_PROPERTY);
            did_prop = true;
         }
         else
         {
            set_chunk_parent(pc, CT_CS_PROPERTY);
            make_type(pc);
         }
         if (pc->flags & PCF_STMT_START) { break; }
      }
   }
}


static void handle_cs_array_type(chunk_t *pc)
{
   chunk_t *prev;

   for (prev = chunk_get_prev(pc);
        chunk_is_type(prev, CT_COMMA);
        prev = chunk_get_prev(prev))
   {
      /* empty */
   }

   if (chunk_is_type(prev, CT_SQUARE_OPEN))
   {
      while (pc != prev)
      {
         pc->parent_type = CT_TYPE;
         pc              = chunk_get_prev(pc);
      }
      prev->parent_type = CT_TYPE;
   }
}


void remove_extra_returns(void)
{
   LOG_FUNC_ENTRY();

   chunk_t *pc = chunk_get_head();
   while (chunk_is_valid(pc))
   {
      if ((pc->type == CT_RETURN) && ((pc->flags & PCF_IN_PREPROC) == 0))
      {
         chunk_t *semi  = chunk_get_next_ncnl(pc);
         chunk_t *cl_br = chunk_get_next_ncnl(semi);

         if (chunk_is_type       (semi,     CT_SEMICOLON                  ) &&
             chunk_is_type       (cl_br,    CT_BRACE_CLOSE                ) &&
             chunk_is_ptype(cl_br, 2, CT_FUNC_DEF, CT_FUNC_CLASS_DEF) )
         {
            LOG_FMT(LRMRETURN, "Removed 'return;' on line %zu\n", pc->orig_line);
            chunk_del(pc);
            chunk_del(semi);
            pc = cl_br;
         }
      }

      pc = chunk_get_next(pc);
   }
}


static void handle_wrap(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   chunk_t  *opp  = chunk_get_next(pc);
   chunk_t  *name = chunk_get_next(opp);
   chunk_t  *clp  = chunk_get_next(name);

   assert(chunks_are_valid(opp, name, clp));

   argval_t pav = (pc->type == CT_FUNC_WRAP) ?
                  cpd.settings[UO_sp_func_call_paren].a :
                  cpd.settings[UO_sp_cpp_cast_paren ].a;

   const argval_t av = (pc->type == CT_FUNC_WRAP) ?
                 cpd.settings[UO_sp_inside_fparen    ].a :
                 cpd.settings[UO_sp_inside_paren_cast].a;

   if (chunk_is_type(clp,     CT_PAREN_CLOSE  ) &&
       chunk_is_type(opp,     CT_PAREN_OPEN   ) &&
       chunk_is_type(name, 2, CT_WORD, CT_TYPE) )
   {
      const char *psp = is_option_set(pav, AV_ADD) ? " " : "";
      const char *fsp = is_option_set(av,  AV_ADD) ? " " : "";

      pc->str.append(psp);
      pc->str.append("(");
      pc->str.append(fsp);
      pc->str.append(name->str);
      pc->str.append(fsp);
      pc->str.append(")");

      set_chunk_type(pc, chunk_is_type(pc, CT_FUNC_WRAP) ? CT_FUNCTION : CT_TYPE);

      pc->orig_col_end = pc->orig_col + pc->len();

      chunk_del(opp);
      chunk_del(name);
      chunk_del(clp);
   }
}


static void handle_proto_wrap(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   chunk_t *opp  = chunk_get_next_ncnl(pc);
   chunk_t *name = chunk_get_next_ncnl(opp);
   chunk_t *tmp  = chunk_get_next_ncnl(chunk_get_next_ncnl(name));
   chunk_t *clp  = chunk_skip_to_match(opp);
   const chunk_t *cma  = chunk_get_next_ncnl(clp);

   if (chunks_are_invalid(opp, name    ) ||
       chunks_are_invalid(clp, cma, tmp) ||
       chunk_is_not_type (name, 2, CT_WORD, CT_TYPE) ||
       chunk_is_not_type (opp,     CT_PAREN_OPEN   ) )
   {
      return;
   }
   switch(cma->type)
   {
      case(CT_SEMICOLON ): set_chunk_type(pc, CT_FUNC_PROTO); break;
      case(CT_BRACE_OPEN): set_chunk_type(pc, CT_FUNC_DEF  ); break;
      default:             /* unexpected chunk type */        return;
   }

   set_chunk_parent(opp, pc->type);
   set_chunk_parent(clp, pc->type);

   set_chunk_parent(tmp, CT_PROTO_WRAP);

   if (chunk_is_type(tmp, CT_PAREN_OPEN)) { fix_fcn_def_params(tmp); }
   else                                   { fix_fcn_def_params(opp); set_chunk_type(name, CT_WORD); }

   tmp = chunk_skip_to_match(tmp);
   if (chunk_is_valid(tmp)) { set_chunk_parent(tmp, CT_PROTO_WRAP); }

   /* Mark return type (TODO: move to own function) */
   tmp = pc;
   while ((tmp = chunk_get_prev_ncnl(tmp)) != nullptr)
   {
      if (!chunk_is_var_type(tmp) &&
           chunk_is_not_type(tmp, 3, CT_OPERATOR, CT_WORD, CT_ADDR))
      {
         break;
      }
      set_chunk_parent(tmp, pc->type);
      make_type(tmp);
   }
}


static void handle_java_assert(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   bool    did_colon = false;
   chunk_t *tmp      = pc;

   while ((tmp = chunk_get_next(tmp)) != nullptr)
   {
      if (tmp->level == pc->level)
      {
         if ((did_colon == false   ) &&
             (chunk_is_type(tmp, CT_COLON)) )
         {
            did_colon = true;
            set_chunk_parent(tmp, pc->type);
         }
         if (chunk_is_type(tmp, CT_SEMICOLON))
         {
            set_chunk_parent(tmp, pc->type);
            break;
         }
      }
   }
}
