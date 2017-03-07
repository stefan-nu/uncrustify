/**
 * @file chunk_list.h
 * Manages and navigates the list of chunks.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef CHUNK_LIST_H_INCLUDED
#define CHUNK_LIST_H_INCLUDED

#include "uncrustify_types.h"
#include "char_table.h"


/* for backward compatibility
 * \todo remove later */
//#define set_chunk_parent set_chunk_ptype


/* \todo better use a class for all chunk related operations,
 *  then the following functions can be changed into member
 *  functions. The function  "chunk_is_comment(chunk)" would for instance
 *  become "chunk.is_comment()". This makes the usage of the chunks easier
 * and more intuitive. */


#define ANY_LEVEL    -1

/* some useful defines that perform typical checks and corresponding
 * reactions. */
#define return_if(cond)          if (cond) {return;        }
#define retval_if(cond, retval)  if (cond) {return(retval);}
#define break_if(cond)           if (cond) {break;         }
#define continue_if(cond)        if (cond) {continue;      }

/**
 * Specifies which chunks should/should not be found.
 * ALL (default)
 *  - return the true next/prev
 *
 * PREPROC
 *  - If not in a preprocessor, skip over any encountered preprocessor stuff
 *  - If in a preprocessor, fail to leave (return nullptr)
 */
enum class scope_e : unsigned int
{
   ALL,      /**< search in all kind of chunks */
   PREPROC,  /**< search only in preprocessor chunks */
};


/**
 * check if a pointer is valid thus no nullptr
 */
bool ptr_is_valid(
   const void * const ptr /**< [in] pointer to check */
);


/**
 * check if all two pointers are valid thus no nullptr
 */
bool ptrs_are_valid(
   const void * const ptr1, /**< [in] pointer1 to check */
   const void * const ptr2  /**< [in] pointer2 to check */
);


/**
 * check if all three pointers are valid thus no nullptr
 */
bool ptrs_are_valid(
   const void * const ptr1, /**< [in] pointer1 to check */
   const void * const ptr2, /**< [in] pointer2 to check */
   const void * const ptr3  /**< [in] pointer3 to check */
);


/**
 * check if a pointer is invalid thus a nullptr
 */
bool ptr_is_invalid(
   const void * const ptr /**< [in] pointer to check */
);


/**
 * check if any of two pointers is invalid thus a nullptr
 */
bool ptrs_are_invalid(
   const void * const ptr1, /**< [in] pointer1 to check */
   const void * const ptr2  /**< [in] pointer2 to check */
);


/**
 * check if any of three pointers is invalid thus a nullptr
 */
bool ptrs_are_invalid(
   const void * const ptr1, /**< [in] pointer1 to check */
   const void * const ptr2, /**< [in] pointer2 to check */
   const void * const ptr3  /**< [in] pointer3 to check */
);



/**
 * check if a chunk is valid
 */
bool is_valid(
   const chunk_t * const pc /**< [in] chunk to check */
);


/**
 * checks if two chunks are valid
 */
bool are_valid(
   const chunk_t * const pc1, /**< [in] chunk1 to check */
   const chunk_t * const pc2  /**< [in] chunk2 to check */
);


/**
 * check if a chunk is not valid
 */
bool is_invalid(
   const chunk_t * const pc /**< [in] chunk to check */
);


/**
 * check if a chunk is invalid or has a given type
 */
bool is_invalid_or_type(
   const chunk_t* const pc,  /**< [in] chunk to check */
   const c_token_t type      /**< [in] type to check for */
);


/**
 * check if a chunk is invalid or has a given flag combination set
 */
bool is_invalid_or_flag(
   const chunk_t* const pc,
   const UINT64 flags
);


/**
 * check if a chunk is invalid or has not a given type
 */
bool is_invalid_or_not_type(
   const chunk_t* const pc,  /**< [in] chunk to check */
   const c_token_t type      /**< [in] type to check for */
);


/**
 * check if any of two chunks is invalid
 */
bool are_invalid(
   const chunk_t * const pc1, /**< [in] chunk1 to check */
   const chunk_t * const pc2  /**< [in] chunk2 to check */
);


/**
 * check if any of three chunks is invalid
 */
bool are_invalid(
   const chunk_t * const pc1,  /**< [in] chunk1 to check */
   const chunk_t * const pc2,  /**< [in] chunk2 to check */
   const chunk_t * const pc3   /**< [in] chunk3 to check */
);


/**
 * check if both chunks are valid
 */
bool are_valid(
   const chunk_t * const pc1,  /**< [in] chunk1 to check */
   const chunk_t * const pc2   /**< [in] chunk2 to check */
);


/**
 * check if all three chunks are valid
 */
bool are_valid(
   const chunk_t * const pc1,  /**< [in] chunk1 to check */
   const chunk_t * const pc2,  /**< [in] chunk2 to check */
   const chunk_t * const pc3   /**< [in] chunk3 to check */
);


/**
 * check if a chunk and its following chunk is valid
 */
bool chunk_and_next_are_valid(
   const chunk_t * const pc /**< [in] chunk to check */
);


/**
 * check if a chunk and its preceding chunk is valid
 */
bool chunk_and_prev_are_valid(
   const chunk_t * const pc /**< [in] chunk to check */
);


/**
 * \brief duplicate a chunk in a chunk list
 *
 */
chunk_t *chunk_dup(
   const chunk_t * const pc_in  /**< [in] chunk to duplicate */
);


/**
 * \brief Add a copy of a chunk to a chunk list after the given position
 *
 * \note If ref is nullptr, add at the tail of the chunk list
 *
 * @retval pointer to the added chunk
 */
chunk_t *chunk_add_after(
   const chunk_t *pc_in,   /**< [in] chunk to add to list */
   chunk_t       *ref      /**< [in] chunk after which to insert */
);


/**
 * \brief Add a copy of a chunk to a chunk list before the given position
 *
 * \note If ref is nullptr, add at the head of the chunk list
 * \bug currently chunks are added to the tail
 *
 * @retval pointer to the added chunk
 */
chunk_t *chunk_add_before(
   const chunk_t *pc_in,   /**< [in] chunk to add to list */
   chunk_t       *ref      /**< [in] chunk before which to insert */
);


/**
 * delete a chunk from a chunk list
 */
void chunk_del(
   chunk_t *pc  /**< [in] chunk to delete */
);


/**
 * move a chunk to after the reference position in a chunk list
 */
void chunk_move_after(
   chunk_t *pc_in,  /**< [in] chunk to move */
   chunk_t *ref     /**< [in] chunk after which to move */
);


/**
 * \brief returns the head of a chunk list
 *
 * @return pointer to the first chunk
 */
chunk_t *chunk_get_head(void);


/**
 * \brief returns the tail of a chunk list
 *
 * @return pointer to the last chunk
 */
chunk_t *chunk_get_tail(void);


/**
 * \brief returns the next chunk in a list of chunks
 *
 * @return pointer to next chunk or nullptr if no chunk was found
 */
chunk_t *chunk_get_next(
   chunk_t *cur,                       /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * \brief returns the previous chunk in a list of chunks
 *
 * @return pointer to previous chunk or nullptr if no chunk was found
 */
chunk_t *chunk_get_prev(
   chunk_t    *cur,                   /**< [in] chunk to use as start point */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Swaps two chunks
 */
void chunk_swap(
   chunk_t *pc1, /**< [in] the first  chunk */
   chunk_t *pc2  /**< [in] the second chunk */
);


/**
 * Swaps two lines that are started with the specified chunks.
 */
void chunk_swap_lines(
   chunk_t *pc1, /**< [in] The first chunk of line 1 */
   chunk_t *pc2  /**< [in] The first chunk of line 2 */
);


/**
 * Finds the first chunk on the line that pc is on.
 * This just backs up until a newline or nullptr is hit.
 *
 * given: [ a - b - c - n1 - d - e - n2 ]
 * input: [ a | b | c | n1 ] => a
 * input: [ d | e | n2 ]     => d
 */
chunk_t *chunk_first_on_line(
   chunk_t *pc  /**< [in] chunk to start with */
);


/**
 * tbd
 */
chunk_t *get_prev_category(
   chunk_t *pc /**< [in] chunk to start with */
);


/**
 * tbd
 */
chunk_t *get_next_scope(
   chunk_t *pc  /**< [in] chunk to start with */
);


/**
 * tbd
 */
chunk_t *get_next_class(
   chunk_t *pc  /**< [in] chunk to start with */
);


/**
 * tbd
 */
chunk_t *get_prev_oc_class(
   chunk_t *pc  /**< [in] chunk to start with */
);


/**
 * Gets the previous function open brace
 */
chunk_t *get_prev_fparen_open(
   chunk_t     *pc,              /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the previous chunk that is not a preprocessor
 */
chunk_t *get_prev_non_pp(
   chunk_t *pc,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the next function chunk
 */
chunk_t *get_next_function(
   chunk_t     *pc,            /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the next NEWLINE chunk
 */
chunk_t *chunk_get_next_nl(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the next non-comment chunk
 */
chunk_t *chunk_get_next_nc(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the next non-NEWLINE and non-comment chunk
 */
chunk_t *chunk_get_next_nnl(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the next non-NEWLINE and non-comment chunk, non-preprocessor chunk
 */
chunk_t *chunk_get_next_ncnl(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the next chunk not in or part of balanced square
 * brackets. This handles stacked [] instances to accommodate
 * multi-dimensional array declarations
 *
 * @return nullptr or the next chunk not in or part of square brackets
 */
chunk_t *chunk_get_next_ncnlnp(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * tbd
 */
chunk_t *chunk_get_next_nisq(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the next non-blank chunk
 */
chunk_t *chunk_get_next_nblank(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the prev non-blank chunk
 */
chunk_t *chunk_get_prev_nblank(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the prev NEWLINE chunk
 */
chunk_t *chunk_get_prev_nl(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the prev non-comment chunk
 */

chunk_t *chunk_get_prev_nc(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the prev non-NEWLINE chunk
 */
chunk_t *chunk_get_prev_nnl(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the prev non-NEWLINE and non-comment chunk
 */
chunk_t *chunk_get_prev_ncnl(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Gets the prev non-NEWLINE and non-comment chunk, non-preprocessor chunk
 */
chunk_t *chunk_get_prev_ncnlnp(
   chunk_t *cur,           /**< [in] chunk to start with */
   scope_e scope = scope_e::ALL  /**< [in] code region to search in */
);


/**
 * Grabs the next chunk of the given type at the level.
 *
 * @return nullptr or the match
 */
chunk_t *chunk_get_next_type(
   chunk_t *cur,        /**< [in] Starting chunk */
   c_token_t type,      /**< [in] The type to look for */
   int level,           /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Grabs the prev chunk of the given type at the level.
 *
 * @return nullptr or the match
 */
chunk_t *chunk_get_prev_type(
   chunk_t   *cur,          /**< [in] Starting chunk */
   c_token_t type,          /**< [in] The type to look for */
   int       level,         /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * \brief search forward through chunk list to find a chunk that holds a given string
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * @retval nullptr    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 */
chunk_t *chunk_get_next_str(
   chunk_t    *cur,          /**< [in] Starting chunk */
   const char *str,          /**< [in] string to search for */
   size_t     len,           /**< [in] length of string */
   int        level,         /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * \brief search backward through chunk list to find a chunk that holds a given string
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * @retval nullptr    - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 */
chunk_t *chunk_get_prev_str(
   chunk_t    *cur,          /**< [in] Starting chunk */
   const char *str,          /**< [in] string to search for */
   size_t     len,           /**< [in] length of string */
   int        level,         /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * @return pointer to found chunk or nullptr if no chunk was found
 */
chunk_t *chunk_get_next_nvb(
   chunk_t     *cur,          /**< [in] chunk to start search */
   const scope_e scope = scope_e::ALL /**< [in] chunk section to consider */
);


/**
 * \brief Gets the next non-vbrace chunk
 *
 * \brief Gets the previous non-vbrace chunk
 *
 * @return pointer to found chunk or nullptr if no chunk was found
 */
chunk_t *chunk_get_prev_nvb(
   chunk_t     *cur,          /**< [in] chunk to start search */
   const scope_e scope = scope_e::ALL /**< [in] chunk section to consider */
);


/**
 * \brief reverse search a chunk of a given category in a chunk list
 *
 * @retval nullptr    - no object found, or invalid parameters provided
 * @retval chunk_t - pointer to the found object
 */
chunk_t *chunk_search_prev_cat(
   chunk_t         *pc, /**< [in] chunk to start search with */
   const c_token_t cat  /**< [in] category to search for */
);


/**
 * \brief forward search a chunk of a given category in a chunk list
 *
 * @retval nullptr    - no object found, or invalid parameters provided
 * @retval chunk_t - pointer to the found object
 */
chunk_t *chunk_search_next_cat(
   chunk_t         *pc, /**< [in] chunk to start search with */
   const c_token_t cat  /**< [in] category to search for */
);


/**
 * defines the type of a chunk
 */
void set_type(
   chunk_t   *pc,  /**< [in] chunk to operate on */
   c_token_t tt    /**< [in] value to set as chunk type */
);


/**
 * defines the parent type of a chunk
 */
void set_ptype(
   chunk_t   *pc,  /**< [in] chunk to operate on */
   c_token_t tt    /**< [in] value to set as parent type */
);


/**
 * defines the type and parent type of a chunk
 */
void set_type_and_ptype(
   chunk_t   *pc,   /**< [in] chunk to operate on */
   c_token_t type,  /**< [in] value to set as chunk  type */
   c_token_t parent /**< [in] value to set as parent type */
);


void set_ptype_and_flag(
   chunk_t   *pc,   /**< [in] chunk to operate on */
   c_token_t type,  /**< [in] value to set as chunk  type */
   UINT64    flag   /**< [in] flag bits to add */
);


/**
 * defines the flags of a chunk
 */
void chunk_flags_set(
   chunk_t *pc,      /**< [in] chunk to operate on */
   UINT64  set_bits  /**< [in] flag bits to add */
);


/**
 * clears flags of a chunk
 */
void chunk_flags_clr(
   chunk_t *pc,      /**< [in] chunk to operate on */
   UINT64  clr_bits  /**< [in] flag bits to remove */
);


/**
 * updates the flags in a chunk
 */
void chunk_flags_update(
   chunk_t *pc,      /**< [in] chunk to update */
   UINT64  clr_bits, /**< [in] flag bits to remove */
   UINT64  set_bits  /**< [in] flag bits to add */
);


/**
 * Skips to the closing match for the current paren/brace/square.
 */
chunk_t *chunk_skip_to_match(
   chunk_t *cur,
   scope_e scope = scope_e::ALL
);


/**
 * Check if a chunk is valid and has a given level
 */
bool is_level(
   const chunk_t *pc,  /**< [in] chunk to check */
   const size_t  level /**< [in] expected level */
);


/**
 * Check if a chunk is valid and has a given type and level
 */
bool is_type_and_level(
   const chunk_t   *pc,  /**< [in] chunk to check */
   const c_token_t type, /**< [in] expected type */
   const int       level /**< [in] expected level or -1 to ignore level */
);


/**
 * Check to see if there is a newline between the two chunks
 */
bool chunk_is_newline_between(
   chunk_t *start,  /**< [in] chunk where check starts */
   chunk_t *end     /**< [in] chunk where check ends */
);


/**
 * tbd
 */
chunk_t *chunk_skip_to_match_rev(
   chunk_t *cur,
   scope_e scope = scope_e::ALL
);


/**
 * check if a chunk is valid and holds a pointer operator
 */
bool chunk_is_ptr_operator(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a newline
 */
bool chunk_is_newline(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds an empty string
 */
bool chunk_is_empty(
chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds any part of a function
 */
bool chunk_is_function(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * checks if a chunk is valid and is a comment
 *
 * comment means any kind of
 * - single line comment
 * - multiline comment
 * - C comment
 * - C++ comment
 */
bool chunk_is_comment(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * checks if a chunk is valid and either a comment or newline
 */
bool chunk_is_comment_or_newline(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_balanced_square(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a part of a preprocessor region
 */
bool chunk_is_preproc(
   chunk_t * pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and has a type that is not part of a preprocessor region
 */
bool chunk_is_no_preproc_type(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and is a comment or newline located in
 * a preprocessor region
 */
bool chunk_is_comment_or_newline_in_preproc(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a newline or blank character
 */
bool chunk_is_comment_newline_or_blank(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a comment, a newline or is
 * a preprocessor part
 */
bool chunk_is_comment_newline_or_preproc(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a single line comment
 */
bool chunk_is_single_line_comment(
   const chunk_t * const pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a semicolon
 */
bool chunk_is_semicolon(
   const chunk_t * const pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a variable type
 */
bool chunk_is_var_type(
   const chunk_t * const pc  /**< [in] chunk to check */
);


/**
 * check if the given chunk is valid and holds a given token type
 * and  a given parent token type
 */
bool is_type_and_ptype(
   chunk_t   *pc,   /**< [in] chunk to check */
   c_token_t type,  /**< [in] token type to check for */
   c_token_t parent /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 * and is not a given parent token type
 */
bool is_type_and_not_ptype(
   chunk_t   *pc,   /**< [in] chunk to check */
   c_token_t type,  /**< [in] token type to check for */
   c_token_t parent /**< [in] token type to check for */
);


/**
 * check if either of two given chunks is valid and holds the
 * given token type
 */
bool any_is_type(
   const chunk_t *pc1,  /**< [in] chunk1 to check */
   const chunk_t *pc2,  /**< [in] chunk2 to check */
   c_token_t     type   /**< [in] token type to check for */
);


/**
 * check if the either of two given chunks is valid and hold
 * the respective given token type
 */
bool any_is_type(
   const chunk_t *pc1,  /**< [in] chunk1 to check */
   c_token_t     type1, /**< [in] token1 type to check for */
   const chunk_t *pc2,  /**< [in] chunk2 to check */
   c_token_t     type2  /**< [in] token2 type to check for */
);


/**
 * check if both chunks are valid but only the first has the
 * given type the second chunk has to be different from its
 * given type
 */
bool is_only_first_type(
   const chunk_t *pc1,  /**< [in] chunk1 to check */
   c_token_t     type1, /**< [in] token1 type to check for */
   const chunk_t *pc2,  /**< [in] chunk2 to check */
   c_token_t     type2  /**< [in] token2 type to check for */
);


/**
 * check if the two given chunks are valid and both hold the
 * same given token type
 */
bool are_types(
   const chunk_t *pc1,  /**< [in] chunk1 to check */
   const chunk_t *pc2,  /**< [in] chunk2 to check */
   c_token_t     type   /**< [in] token type to check for */
);


/**
 * check if the two given chunks are valid and both hold the
 * same given parent token type
 */
bool are_ptypes(
   const chunk_t *pc1,  /**< [in] chunk1 to check */
   const chunk_t *pc2,  /**< [in] chunk2 to check */
   c_token_t     type   /**< [in] token type to check for */
);


/**
 * check if the two given chunks are valid and hold a given
 * token type
 */
bool are_types(
   const chunk_t *pc1,  /**< [in] chunk1 to check */
   c_token_t     type1,  /**< [in] token1 type to check for */
   const chunk_t *pc2,  /**< [in] chunk2 to check */
   c_token_t     type2  /**< [in] token2 type to check for */
);


/**
 * check if the two given chunks are valid and hold a given
 * parent token type
 */
bool are_ptypes(
   const chunk_t *pc1,  /**< [in] chunk1 to check */
   c_token_t     type1,  /**< [in] token1 type to check for */
   const chunk_t *pc2,  /**< [in] chunk2 to check */
   c_token_t     type2  /**< [in] token2 type to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 */
bool is_type(
   const chunk_t   *pc,  /**< [in] chunk to check */
   const c_token_t type  /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 * that corresponds either to type1 or type2
 */
bool is_type(
   const chunk_t   *pc,   /**< [in] chunk to check */
   const c_token_t type1, /**< [in] token type1 to check for */
   const c_token_t type2  /**< [in] token type2 to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 * that corresponds either to type1, type2, or type3
 */
bool is_type(
   const chunk_t   *pc,   /**< [in] chunk to check */
   const c_token_t type1, /**< [in] token type1 to check for */
   const c_token_t type2, /**< [in] token type2 to check for */
   const c_token_t type3  /**< [in] token type3 to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 * that corresponds either to type1, type2, or type3
 */
bool is_type(
   const chunk_t   *pc,   /**< [in] chunk to check */
   const c_token_t type1, /**< [in] token type1 to check for */
   const c_token_t type2, /**< [in] token type2 to check for */
   const c_token_t type3, /**< [in] token type3 to check for */
   const c_token_t type4  /**< [in] token type4 to check for */
);


/**
 * check if the given chunk is valid and holds a given
 * parent token type
 */
bool is_ptype(
   const chunk_t * const pc, /**< [in] chunk to check */
   const c_token_t parent    /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and holds a parent token type
 * that is either parent1 or parent2
 */
bool is_ptype(
   const chunk_t * const pc, /**< [in] chunk to check */
   const c_token_t parent1,  /**< [in] token type1 to check for */
   const c_token_t parent2   /**< [in] token type2 to check for */
);


/**
 * check if the given chunk is valid and holds a parent token type
 * that is either parent1, parent2 or parent3
 */
bool is_ptype(
   const chunk_t * const pc, /**< [in] chunk to check */
   const c_token_t parent1,  /**< [in] token type1 to check for */
   const c_token_t parent2,  /**< [in] token type2 to check for */
   const c_token_t parent3   /**< [in] token type3 to check for */
);


/**
 * check if the given chunk is valid and has a token type different
 * than a given one
 */
bool is_not_type(
   const chunk_t * const pc, /**< [in] chunk to check */
   c_token_t c_token         /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and holds a token type which is
 * part of a given list
 */
bool is_type(
   const chunk_t *pc,     /**< [in] chunk to check */
   int            count,  /**< [in] number of token types to check */
   ...                    /**< [in] list of token types to check for */
);


/**
 * check if the given chunk is valid and holds a parent token
 * type which is part of a given list
 */
bool is_ptype(
   const chunk_t * const pc,   /**< [in] chunk to check */
   int count,     /**< [in] number of token types to check */
   ...            /**< [in] list of token types to check for */
);


/**
 * check if the given chunk is valid and holds a token type which is
 * different from all types in a given list
 */
bool is_not_type(
   const chunk_t * const pc, /**< [in] chunk to check */
   int count,                /**< [in] number of token types to check */
   ...                       /**< [in] list of token types to check for */
);


/**
 * check if the given chunk is valid and holds a parent token
 * type which is different from all types in a given list
 */
bool is_not_ptype(
   const chunk_t * const pc, /**< [in] chunk to check */
   int count,     /**< [in] number of token types to check */
   ...            /**< [in] list of parent token types to check for */
);


/**
 * Check if the given chunk is valid and has a given flag
 * combination set. Other flags are ignored during the check.
 */
bool is_flag(
   const chunk_t * const pc, /**< [in] chunk to check */
   const UINT64 flags        /**< [in] expected flags */
);


/**
 * Check if the given chunk is valid and has a given flag
 * combination not set Other flags are ignored during the check.
 */
bool is_not_flag(
   const chunk_t * const pc, /**< [in] chunk to check */
   const UINT64 flags        /**< [in] expected flags */
);


/**
 * check if the given chunk is valid and holds a given string
 * The case of the string is considered.
 */
bool is_str(
   chunk_t *pc,     /**< [in] chunk to check */
   const char *str, /**< [in] string to compare with */
   size_t len       /**< [in] number of characters in string to compare */
);


/**
 * check if the given chunk is valid and holds a given string.
 * The case of the string is ignored.
 */
bool chunk_is_str_case(
   chunk_t *pc,      /**< [in] chunk to check */
   const char *str,  /**< [in] string to compare with */
   size_t len        /**< [in] number of characters in string to compare */
);


/**
 * tbd
 */
 bool chunk_is_word(
   chunk_t *pc  /**< [in] chunk to check */
);


 /**
  * tbd
  */
bool chunk_is_star(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_addr(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_msref(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * check if a chunk is a member sign
 * thus is either "->" or "::"
 */
bool chunk_is_member(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_closing_brace(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_opening_brace(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_vbrace(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_fparen_open(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_paren_open(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * tbd
 */
bool chunk_is_paren_close(
   chunk_t *pc  /**< [in] chunk to check */
);


/**
 * Check if any of two chunks is invalid or both have the same preprocessor
 * flag. Thus if both are either part of a preprocessor block or both are not
 * part of a preprocessor block.
 * If the result is true, a newline/nl_cont between the two may be removed.
 */
bool chunk_same_preproc(
   chunk_t *pc1, /**< [in] chunk 1 to compare */
   chunk_t *pc2  /**< [in] chunk 2 to compare */
);


/**
 * Check if both chunks are valid and have not the same preprocessor state.
 * Thus either chunk is part of a preprocessor block the other one not.
 */
bool chunk_different_preproc(
   chunk_t *pc1, /**< [in] chunk 1 to compare */
   chunk_t *pc2  /**< [in] chunk 2 to compare */
);


/**
 * Returns true if it is safe to delete the newline token.
 * The prev and next chunks must have the same PCF_IN_PREPROC flag AND
 * the newline can't be after a C++ comment.
 */
bool chunk_safe_to_del_nl(
   chunk_t *nl
);


/**
 * check if a chunk points to the opening parenthese of a
 * for(...in...) loop in Objective-C.
 *
 *@retval true  - the chunk is the opening parentheses of a for in loop
 *@retval false - no for(...in...) loop found
 */
bool chunk_is_forin(
   chunk_t *pc /**< [in] chunk to start search with */
);


#endif /* CHUNK_LIST_H_INCLUDED */
