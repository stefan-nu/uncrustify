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
#include "ListManager.h"

/* \todo better use a class for all chunk related operations,
 *  then the following functions can be changed into member
 *  functions. The function  "chunk_is_comment(chunk)" would for instance
 *  become "chunk.is_comment()". This makes the usage of the chunks easier
 * and more intuitive. */


#define ANY_LEVEL    -1


typedef ListManager<chunk_t>::dir_e dir_e;

/**
 * Specifies which chunks should/should not be found.
 * ALL (default)
 *  - return the true next/prev
 *
 * PREPROC
 *  - If not in a preprocessor, skip over any encountered preprocessor stuff
 *  - If in a preprocessor, fail to leave (return nullptr)
 */
enum class scope_e : uint32_t
{
   ALL,      /**< search in all kind of chunks */
   PREPROC,  /**< search only in preprocessor chunks */
};


/**
 * check if a pointer is valid thus no nullptr
 */
bool ptr_is_valid(
   const void* const ptr /**< [in] pointer to check */
);


/**
 * check if all two pointers are valid thus no nullptr
 */
bool ptrs_are_valid(
   const void* const ptr1, /**< [in] pointer1 to check */
   const void* const ptr2  /**< [in] pointer2 to check */
);


/**
 * check if all three pointers are valid thus no nullptr
 */
bool ptrs_are_valid(
   const void* const ptr1, /**< [in] pointer1 to check */
   const void* const ptr2, /**< [in] pointer2 to check */
   const void* const ptr3  /**< [in] pointer3 to check */
);


/**
 * check if a pointer is invalid thus a nullptr
 */
bool ptr_is_invalid(
   const void* const ptr /**< [in] pointer to check */
);


/**
 * check if any of two pointers is invalid thus a nullptr
 */
bool ptrs_are_invalid(
   const void* const ptr1, /**< [in] pointer1 to check */
   const void* const ptr2  /**< [in] pointer2 to check */
);


/**
 * check if any of three pointers is invalid thus a nullptr
 */
bool ptrs_are_invalid(
   const void* const ptr1, /**< [in] pointer1 to check */
   const void* const ptr2, /**< [in] pointer2 to check */
   const void* const ptr3  /**< [in] pointer3 to check */
);



/**
 * check if a chunk is valid
 */
bool is_valid(
   const chunk_t* const pc /**< [in] chunk to check */
);


/**
 * checks if two chunks are valid
 */
bool are_valid(
   const chunk_t* const pc1, /**< [in] chunk1 to check */
   const chunk_t* const pc2  /**< [in] chunk2 to check */
);


/**
 * check if a chunk is not valid
 */
bool is_invalid(
   const chunk_t* const pc /**< [in] chunk to check */
);


/**
 * check if a chunk is invalid or is valid and has a given type
 */
bool is_invalid_or_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type     /**< [in] type to check for */
);


/**
 * check if a chunk is invalid or is valid and has a given parent type
 */
bool is_invalid_or_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t ptype    /**< [in] parent type to check for */
);



/**
 * check if a chunk is invalid or is valid and has a given flag combination set
 */
bool is_invalid_or_flag(
   const chunk_t* const pc,
   const uint64_t flags
);


/**
 * check if a chunk is invalid or is valid has not a given type
 */
bool is_invalid_or_not_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type     /**< [in] type to check for */
);


/**
 * check if any of two chunks is invalid
 */
bool are_invalid(
   const chunk_t* const pc1, /**< [in] chunk1 to check */
   const chunk_t* const pc2  /**< [in] chunk2 to check */
);


/**
 * check if any of three chunks is invalid
 */
bool are_invalid(
   const chunk_t* const pc1,  /**< [in] chunk1 to check */
   const chunk_t* const pc2,  /**< [in] chunk2 to check */
   const chunk_t* const pc3   /**< [in] chunk3 to check */
);


/**
 * check if both chunks are valid
 */
bool are_valid(
   const chunk_t* const pc1,  /**< [in] chunk1 to check */
   const chunk_t* const pc2   /**< [in] chunk2 to check */
);


/**
 * check if all three chunks are valid
 */
bool are_valid(
   const chunk_t* const pc1,  /**< [in] chunk1 to check */
   const chunk_t* const pc2,  /**< [in] chunk2 to check */
   const chunk_t* const pc3   /**< [in] chunk3 to check */
);


/**
 * check if a chunk and its following chunk is valid
 */
bool chunk_and_next_are_valid(
   const chunk_t* const pc /**< [in] chunk to check */
);


/**
 * check if a chunk and its preceding chunk is valid
 */
bool chunk_and_prev_are_valid(
   const chunk_t* const pc /**< [in] chunk to check */
);


/***************************************************************************//**
 * @brief prototype for a function that checks a chunk to have a given type
 *
 * @note this typedef defines the function type "check_t"
 * for a function pointer of type
 * bool function(chunk_t* pc)
 ******************************************************************************/
typedef bool (*check_t)(chunk_t* pc);


/***************************************************************************//**
 * \brief search for a chunk that satisfies a condition in a chunk list
 *
 * A generic function that traverses a chunks list either
 * in forward or reverse direction. The traversal continues until a
 * chunk satisfies the condition defined by the compare function.
 * Depending on the parameter cond the condition will either be
 * checked to be true or false.
 *
 * Whenever a chunk list traversal is to be performed this function
 * shall be used. This keeps the code clear and easy to understand.
 *
 * If there are performance issues this function might be worth to
 * be optimized as it is heavily used.
 *
 * @retval nullptr - no requested chunk was found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 ******************************************************************************/
chunk_t* chunk_search(
   chunk_t*      cur,                  /**< [in] chunk to start search at */
   const check_t check_fct,            /**< [in] compare function */
   const scope_e scope = scope_e::ALL, /**< [in] code parts to consider for search */
   const dir_e   dir   = dir_e::AFTER, /**< [in] search direction */
   const bool    cond  = true          /**< [in] success condition */
);


/**
 * \brief duplicate a chunk in a chunk list
 *
 */
chunk_t* chunk_dup(
   const chunk_t* const pc_in  /**< [in] chunk to duplicate */
);


/**
 * \brief Add a copy of a chunk to a chunk list after the given position
 *
 * \note If ref is nullptr, add at the tail of the chunk list
 *
 * @retval pointer to the added chunk
 */
chunk_t* chunk_add_after(
   const chunk_t* pc_in, /**< [in] chunk to add to list */
   chunk_t*       ref    /**< [in] chunk after which to insert */
);


/**
 * \brief Add a copy of a chunk to a chunk list before the given position
 *
 * \note If ref is nullptr, add at the head of the chunk list
 * \bug currently chunks are added to the tail
 *
 * @retval pointer to the added chunk
 */
chunk_t* chunk_add_before(
   const chunk_t* pc_in, /**< [in] chunk to add to list */
   chunk_t*       ref    /**< [in] chunk before which to insert */
);


/**
 * delete a chunk from a chunk list
 */
void chunk_del(
   chunk_t* pc /**< [in] chunk to delete */
);


/**
 * move a chunk to after the reference position in a chunk list
 */
void chunk_move_after(
   chunk_t* pc_in, /**< [in] chunk to move */
   chunk_t* ref    /**< [in] chunk after which to move */
);


/**
 * \brief returns the head of a chunk list
 *
 * @return pointer to the first chunk
 */
chunk_t* chunk_get_head(void);


/**
 * \brief returns the tail of a chunk list
 *
 * @return pointer to the last chunk
 */
chunk_t* chunk_get_tail(void);


/**
 * \brief returns the next chunk in a list of chunks
 *
 * @return pointer to next chunk or nullptr if no chunk was found
 */
chunk_t* chunk_get_next(
   chunk_t* cur,                      /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * \brief returns the previous chunk in a list of chunks
 *
 * @return pointer to previous chunk or nullptr if no chunk was found
 */
chunk_t* chunk_get_prev(
   chunk_t*      cur,                 /**< [in] chunk to use as start point */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Swaps two chunks
 */
void swap_chunks(
   chunk_t* pc1, /**< [in] the first  chunk */
   chunk_t* pc2  /**< [in] the second chunk */
);


/**
 * Swaps two lines that are started with the specified chunks.
 */
void swap_lines(
   chunk_t* pc1, /**< [in] The first chunk of line 1 */
   chunk_t* pc2  /**< [in] The first chunk of line 2 */
);


/**
 * Finds the first chunk of the line that pc is part of.
 * This backs up until a newline or nullptr is hit.
 *
 * chunk list: [ a - b - c - n1 - d - e - n2 ]
 * input:      [ a  => a ]
 * input:      [ b  => a ]
 * input:      [ c  => a ]
 * input:      [ n1 => a ]
 * input:      [ d  => d ]
 * input:      [ e  => d ]
 * input:      [ n2 => d ]
 *
 * @return pointer to first chunk of the line
 */
chunk_t* get_first_on_line(
   chunk_t* pc /**< [in] chunk to start with */
);


/**
 * tbd
 */
chunk_t* get_prev_category(
   chunk_t* pc /**< [in] chunk to start with */
);


/**
 * tbd
 */
chunk_t* get_next_scope(
   chunk_t* pc /**< [in] chunk to start with */
);


/**
 * tbd
 */
chunk_t* get_next_class(
   chunk_t* pc /**< [in] chunk to start with */
);


/**
 * tbd
 */
chunk_t* get_prev_oc_class(
   chunk_t* pc /**< [in] chunk to start with */
);


/**
 * Gets the previous function open brace
 */
chunk_t* get_prev_fparen_open(
   chunk_t*      pc,                  /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the previous chunk that is not a preprocessor
 */
chunk_t* get_prev_non_pp(
   chunk_t*      pc,                  /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the next function chunk
 */
chunk_t* get_next_function(
   chunk_t*      pc,                  /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the next NEWLINE chunk
 */
chunk_t* get_next_nl(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the next non-comment chunk
 */
chunk_t* get_next_nc(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the next non-NEWLINE and non-comment chunk
 */
chunk_t* get_next_nnl(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the next non-NEWLINE and non-comment chunk, non-preprocessor chunk
 */
chunk_t* get_next_ncnl(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the next chunk not in or part of balanced square
 * brackets. This handles stacked [] instances to accommodate
 * multi-dimensional array declarations
 *
 * @return nullptr or the next chunk not in or part of square brackets
 */
chunk_t* get_next_ncnlnp(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * tbd
 */
chunk_t* get_next_nisq(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the next non-blank chunk
 */
chunk_t* get_next_nblank(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the prev non-blank chunk
 */
chunk_t* get_prev_nblank(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the prev NEWLINE chunk
 */
chunk_t* get_prev_nl(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the prev COMMA chunk
 */
chunk_t* get_prev_comma(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the prev non-comment chunk
 */
chunk_t* get_prev_nc(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the prev non-NEWLINE chunk
 */
chunk_t* get_prev_nnl(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the prev non-NEWLINE and non-comment chunk
 */
chunk_t* get_prev_ncnl(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Gets the prev non-NEWLINE and non-comment chunk, non-preprocessor chunk
 */
chunk_t* get_prev_ncnlnp(
   chunk_t*      cur,                 /**< [in] chunk to start with */
   const scope_e scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Grabs the next chunk of the given type at the level.
 *
 * @return nullptr or the match
 */
chunk_t* get_next_type(
   chunk_t*        cur,   /**< [in] Starting chunk */
   const c_token_t type,  /**< [in] The type to look for */
   const int32_t   level, /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   const scope_e   scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * Grabs the prev chunk of the given type at the level.
 *
 * @return nullptr or the match
 */
chunk_t* get_prev_type(
   chunk_t*        cur,   /**< [in] Starting chunk */
   const c_token_t type,  /**< [in] The type to look for */
   const int32_t   level, /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   const scope_e   scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * \brief search forward through chunk list to find a chunk that holds a given string
 *
 * traverses a chunk list either in forward or backward direction.
 * The traversal continues until a chunk of a given category is found.
 *
 * @retval nullptr - no chunk found or invalid parameters provided
 * @retval chunk_t - pointer to the found chunk
 */
chunk_t* get_next_str(
   chunk_t*       cur,   /**< [in] Starting chunk */
   const char*    str,   /**< [in] string to search for */
   const uint32_t len,   /**< [in] length of string */
   const int32_t  level, /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   const scope_e  scope = scope_e::ALL /**< [in] code region to search in */
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
chunk_t* get_prev_str(
   chunk_t*       cur,   /**< [in] Starting chunk */
   const char*    str,   /**< [in] string to search for */
   const uint32_t len,   /**< [in] length of string */
   const int32_t  level, /**< [in] -1 or ANY_LEVEL (any level) or the level to match */
   const scope_e  scope = scope_e::ALL /**< [in] code region to search in */
);


/**
 * @return pointer to found chunk or nullptr if no chunk was found
 */
chunk_t* get_next_nvb(
   chunk_t*      cur,                 /**< [in] chunk to start search */
   const scope_e scope = scope_e::ALL /**< [in] chunk section to consider */
);


/**
 * \brief Gets the next non-vbrace chunk
 *
 * @return pointer to found chunk or nullptr if no chunk was found
 */
chunk_t* get_prev_nvb(
   chunk_t*      cur,                 /**< [in] chunk to start search */
   const scope_e scope = scope_e::ALL /**< [in] chunk section to consider */
);


/**
 * \brief Gets the next non-pointer chunk
 *
 * @return pointer to found chunk or nullptr if no chunk was found
 */
chunk_t* get_next_nptr(
   chunk_t*      cur,                 /**< [in] chunk to start search */
   const scope_e scope = scope_e::ALL /**< [in] chunk section to consider */
);



/**
 * defines the type of a chunk
 */
void set_type(
   chunk_t*        pc,  /**< [in] chunk to operate on */
   const c_token_t type /**< [in] value to set as chunk type */
);


/**
 * defines the parent type of a chunk
 */
void set_ptype(
   chunk_t*        pc,  /**< [in] chunk to operate on */
   const c_token_t type /**< [in] value to set as parent type */
);


/**
 * defines the type and parent type of a chunk
 */
void set_type_and_ptype(
   chunk_t*        pc,    /**< [in] chunk to operate on */
   const c_token_t type,  /**< [in] value to set as chunk  type */
   const c_token_t parent /**< [in] value to set as parent type */
);


void set_type_and_flag(
   chunk_t*        pc,   /**< [in] chunk to operate on */
   const c_token_t type, /**< [in] value to set as chunk type */
   const uint64_t  flag  /**< [in] flag bits to add */
);


void set_ptype_and_flag(
   chunk_t*        pc,   /**< [in] chunk to operate on */
   const c_token_t type, /**< [in] value to set as chunk type */
   const uint64_t  flag  /**< [in] flag bits to add */
);


/**
 * provides the flags of a chunk filtered by an optional mask
 */
uint64_t get_flags(
   chunk_t*       pc,               /**< [in] chunk to operate on */
   const uint64_t mask = UINT64_MAX /**< [in] mask to exclude some bits */
);


/**
 * defines the flags of a chunk
 */
void set_flags(
   chunk_t*       pc,      /**< [in] chunk to operate on */
   const uint64_t set_bits /**< [in] flag bits to add */
);


/**
 * clears flags of a chunk
 */
void clr_flags(
   chunk_t*       pc,      /**< [in] chunk to operate on */
   const uint64_t clr_bits /**< [in] flag bits to remove */
);


/**
 * updates the flags in a chunk
 */
void update_flags(
   chunk_t*        pc,       /**< [in] chunk to update */
   const uint64_t  clr_bits, /**< [in] flag bits to remove */
   const uint64_t  set_bits  /**< [in] flag bits to add */
);


/**
 * Skips to the closing match for the current paren/brace/square.
 */
chunk_t* chunk_skip_to_match(
   chunk_t*      cur,                 /**< [in] chunk to operate on */
   const scope_e scope = scope_e::ALL /**< [in]  */
);


/**
 * tbd
 */
chunk_t* chunk_skip_to_match_rev(
   chunk_t*      cur,                 /**< [in] chunk to operate on */
   const scope_e scope = scope_e::ALL /**< [in]  */
);


/**
 * Check if a chunk is valid and has a given level
 */
bool is_level(
   const chunk_t* pc,   /**< [in] chunk to check */
   const uint32_t level /**< [in] expected level */
);


/**
 * Check if a chunk is valid and has a level that is larger
 * than the reference level given as parameter
 */
bool exceeds_level(
   const chunk_t* pc, /**< [in] chunk to check */
   const uint32_t ref /**< [in] reference level to be exceeded */
);


/**
 * Check if a chunk is valid and has a given type and level
 */
bool is_type_and_level(
   const chunk_t*  pc,   /**< [in] chunk to check */
   const c_token_t type, /**< [in] expected type */
   const int32_t   level /**< [in] expected level or -1 to ignore level */
);


/**
 * Check to see if there is a newline between the two chunks
 */
bool is_newline_between(
   chunk_t* start, /**< [in] chunk where check starts */
   chunk_t* end    /**< [in] chunk where check ends */
);


/**
 * check if a chunk is valid and holds a pointer operator
 */
bool is_ptr_operator(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a newline
 */
bool is_nl(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a comma
 */
bool is_comma(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is a pointer
 */
bool is_ptr(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds an empty string
 */
bool chunk_empty(
chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds any part of a function
 */
bool chunk_is_function(
   chunk_t* pc /**< [in] chunk to check */
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
bool is_cmt(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * checks if a chunk is valid and either a comment or newline
 */
bool is_cmt_or_nl(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * tbd
 */
bool is_bal_square(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a part of a preprocessor region
 */
bool is_preproc(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and has a type that is not part of a preprocessor region
 */
bool is_no_preproc_type(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and is a comment or newline located in
 * a preprocessor region
 */
bool is_cmt_or_nl_in_preproc(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a newline or blank character
 */
bool is_cmt_nl_or_blank(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a comment, a newline or is
 * a preprocessor part
 */
bool is_cmt_nl_or_preproc(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a single line comment
 */
bool is_single_line_cmt(
   const chunk_t* const pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a semicolon
 */
bool is_semicolon(
   const chunk_t* const pc /**< [in] chunk to check */
);


/**
 * check if a chunk is valid and holds a variable type
 */
bool is_var_type(
   const chunk_t* const pc /**< [in] chunk to check */
);


/**
 * check if the given chunk is valid and holds a given token type
 * and  a given parent token type
 */
bool is_type_and_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type,    /**< [in] token type to check for */
   const c_token_t ptype    /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 * and is not a given parent token type
 */
bool is_type_and_not_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type,    /**< [in] token type to check for */
   const c_token_t ptype    /**< [in] token type to check for */
);


/**
 * check if either of two given chunks is valid and holds the
 * given token type
 */
bool any_is_type(
   const chunk_t* const pc1, /**< [in] chunk1 to check */
   const chunk_t* const pc2, /**< [in] chunk2 to check */
   const c_token_t type      /**< [in] token type to check for */
);


/**
 * check if the either of two given chunks is valid and hold
 * the respective given token type
 */
bool any_is_type(
   const chunk_t* const pc1,   /**< [in] chunk1 to check */
   const c_token_t      type1, /**< [in] token1 type to check for */
   const chunk_t* const pc2,   /**< [in] chunk2 to check */
   const c_token_t      type2  /**< [in] token2 type to check for */
);


/**
 * check if both chunks are valid but only the first has the
 * given type the second chunk has to be different from its
 * given type
 */
bool is_only_first_type(
   const chunk_t* const  pc1,   /**< [in] chunk1 to check */
   const c_token_t       type1, /**< [in] token1 type to check for */
   const chunk_t* const  pc2,   /**< [in] chunk2 to check */
   const c_token_t       type2  /**< [in] token2 type to check for */
);


/**
 * check if the two given chunks are valid and both hold the
 * same given token type
 */
bool are_types(
   const chunk_t* const pc1, /**< [in] chunk1 to check */
   const chunk_t* const pc2, /**< [in] chunk2 to check */
   const c_token_t type      /**< [in] token type to check for */
);


/**
 * check if the two given chunks are valid and both hold the
 * same given parent token type
 */
bool are_ptypes(
   const chunk_t* const pc1, /**< [in] chunk1 to check */
   const chunk_t* const pc2, /**< [in] chunk2 to check */
   const c_token_t      type /**< [in] token type to check for */
);


/**
 * check if the two given chunks are valid and hold a given
 * token type
 */
bool are_types(
   const chunk_t*  const pc1,   /**< [in] chunk1 to check */
   const c_token_t       type1, /**< [in] token1 type to check for */
   const chunk_t*  const pc2,   /**< [in] chunk2 to check */
   const c_token_t       type2  /**< [in] token2 type to check for */
);


/**
 * check if the two given chunks are valid and hold a given
 * parent token type
 */
bool are_ptypes(
   const chunk_t* const pc1,   /**< [in] chunk1 to check */
   c_token_t            type1, /**< [in] token1 type to check for */
   const chunk_t* const pc2,   /**< [in] chunk2 to check */
   c_token_t            type2  /**< [in] token2 type to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 */
bool is_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type     /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 * that corresponds either to type1 or type2
 */
bool is_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type1,   /**< [in] token type1 to check for */
   const c_token_t type2    /**< [in] token type2 to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 * that corresponds either to type1, type2, or type3
 */
bool is_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type1,   /**< [in] token type1 to check for */
   const c_token_t type2,   /**< [in] token type2 to check for */
   const c_token_t type3    /**< [in] token type3 to check for */
);


/**
 * check if the given chunk is valid and holds a given token type
 * that corresponds either to type1, type2, or type3
 */
bool is_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type1,   /**< [in] token type1 to check for */
   const c_token_t type2,   /**< [in] token type2 to check for */
   const c_token_t type3,   /**< [in] token type3 to check for */
   const c_token_t type4    /**< [in] token type4 to check for */
);


/**
 * check if the given token equals a given token type
 */
bool is_type(
   const c_token_t token, /**< [in] token to check */
   const c_token_t type   /**< [in] token type to check for */
);


/**
 * check if the given token equals any of two given token type
 */
bool is_type(
   const c_token_t token, /**< [in] token to check */
   const c_token_t type1, /**< [in] token type1 to check for */
   const c_token_t type2  /**< [in] token type2 to check for */
);


/**
 * check if the given token equals any of three given token type
 */
bool is_type(
   const c_token_t token, /**< [in] token to check */
   const c_token_t type1, /**< [in] token type1 to check for */
   const c_token_t type2, /**< [in] token type2 to check for */
   const c_token_t type3  /**< [in] token type3 to check for */
);


/**
 * check if the given chunk is valid and holds a given
 * parent token type
 */
bool is_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type     /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and holds a parent token type
 * that is either parent1 or parent2
 */
bool is_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type1,   /**< [in] token type1 to check for */
   const c_token_t type2    /**< [in] token type2 to check for */
);


/**
 * check if the given chunk is valid and holds a parent token type
 * that is either parent1, parent2 or parent3
 */
bool is_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type1,   /**< [in] token type1 to check for */
   const c_token_t type2,   /**< [in] token type2 to check for */
   const c_token_t type3    /**< [in] token type3 to check for */
);


/**
 * check if the given token differs from a given token type
 */
bool not_type(
   const c_token_t token, /**< [in] token to check */
   const c_token_t type   /**< [in] token type to check for */
);


/**
 * check if the given token differs all of two given token type
 */
bool not_type(
   const c_token_t token, /**< [in] token to check */
   const c_token_t type1, /**< [in] token type1 to check for */
   const c_token_t type2  /**< [in] token type2 to check for */
);


/**
 * check if the given token differs all of three given token type
 */
bool not_type(
   const c_token_t token, /**< [in] token to check */
   const c_token_t type1, /**< [in] token type1 to check for */
   const c_token_t type2, /**< [in] token type2 to check for */
   const c_token_t type3  /**< [in] token type3 to check for */
);


/**
 * check if the given chunk is valid and has a token type
 * different than a given one
 */
bool not_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type     /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and has a token type
 * different than token1 and token2
 */
bool not_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type1,   /**< [in] token type1 to check for */
   const c_token_t type2    /**< [in] token type2 to check for */
);


/**
 * check if the given chunk is valid and has a token type
 * different than token1, token2, and token3
 */
bool not_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type1,   /**< [in] token type1 to check for */
   const c_token_t type2,   /**< [in] token type2 to check for */
   const c_token_t type3    /**< [in] token type3 to check for */
);


/**
 * check if the given chunk is valid and holds a token type which is
 * part of a given list
 */
bool is_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   int32_t count,               /**< [in] number of token types to check */
   ...                      /**< [in] list of token types to check for */
);


/**
 * check if the given chunk is valid and holds a parent token
 * type which is part of a given list
 */
bool is_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   int32_t count,               /**< [in] number of token types to check */
   ...                      /**< [in] list of token types to check for */
);


/**
 * check if the given chunk is valid and holds a token type which is
 * different from all types in a given list
 */
bool not_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   int32_t count,               /**< [in] number of token types to check */
   ...                      /**< [in] list of token types to check for */
);


/**
 * check if the given chunk is valid and holds a parent token
 * type which is different from the given type
 */
bool not_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t ptype    /**< [in] token type to check for */
);


/**
 * check if the given chunk is valid and has a parent token type
 * different than ptoken1 and ptoken2
 */
bool not_type(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t ptype1,  /**< [in] token type1 to check for */
   const c_token_t ptype2   /**< [in] token type2 to check for */
);


/**
 * check if the given chunk is valid and has a parent token type
 * different than ptoken1, ptoken2, and ptoken3
 */
bool not_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t ptype1,  /**< [in] token type1 to check for */
   const c_token_t ptype2,  /**< [in] token type2 to check for */
   const c_token_t ptype3   /**< [in] token type3 to check for */
);


/**
 * check if the given chunk is valid and holds a parent token
 * type which is different from all types in a given list
 */
bool not_ptype(
   const chunk_t* const pc, /**< [in] chunk to check */
   int32_t count,               /**< [in] number of token types to check */
   ...                      /**< [in] list of parent token types to check for */
);


/**
 * Check if the given chunk is valid and has a given type
 * and a given flag combination set.
 */
bool is_type_and_flag(
   const chunk_t* const pc, /**< [in] chunk to check */
   const c_token_t type,    /**< [in] token type to check for */
   const uint64_t  flags    /**< [in] expected flags */
);


/**
 * Check if the given chunk is valid and has a given flag
 * set.
 *
 * \note Only check one flag at a time. Several flags cannot be
 * checked together as the flags are not defined as bitmask.
 */
bool is_flag(
   const chunk_t* const pc, /**< [in] chunk to check */
   const uint64_t flags     /**< [in] expected flags */
);


/**
 * Check if the given chunk is valid and has a given flag
 * combination not set.
 */
bool not_flag(
   const chunk_t* const pc, /**< [in] chunk to check */
   const uint64_t flags     /**< [in] expected flags */
);


/**
 * check if the given chunk is valid and holds a given string
 * The case of the string is considered.
 */
bool is_str(
   chunk_t*    pc, /**< [in] chunk to check */
   const char* str /**< [in] string to compare with */
);


/**
 * check if the given chunk is valid and holds a given string.
 * The case of the string is ignored.
 */
bool is_str_case(
   chunk_t*    pc, /**< [in] chunk to check */
   const char* str /**< [in] string to compare with */
);


/**
 * tbd
 */
 bool is_word(
   chunk_t* pc /**< [in] chunk to check */
);


 /**
  * tbd
  */
bool is_star(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * tbd
 */
bool is_addr(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * tbd
 */
bool is_msref(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is a member sign
 * thus is either "->" or "::"
 */
bool chunk_is_member(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is a real or virtual closing brace
 */
bool is_closing_brace(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is a real or virtual opening brace
 */
bool is_opening_brace(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is a opening or closing virtual brace
 */
bool is_vbrace(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is a function opening parenthese
 */
bool is_fparen_open(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is any kind of opening parenthesis
 */
bool is_paren_open(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * check if a chunk is any kind of closing parenthesis
 */
bool is_paren_close(
   chunk_t* pc /**< [in] chunk to check */
);


/**
 * Check if both chunks are invalid and both have the same preprocessor
 * flag. Thus if both are either part of a preprocessor block or both are not
 * part of a preprocessor block.
 */
bool are_same_pp(
   const chunk_t* const pc1, /**< [in] chunk 1 to compare */
   const chunk_t* const pc2  /**< [in] chunk 2 to compare */
);


/**
 * Check if both chunks are valid and have not the same preprocessor state.
 * Thus either chunk is part of a preprocessor block the other one not.
 */
bool are_different_pp(
   const chunk_t* const pc1, /**< [in] chunk 1 to compare */
   const chunk_t* const pc2  /**< [in] chunk 2 to compare */
);


/**
 * Returns true if it is safe to delete a newline
 * The prev and next chunks must have the same preprocessor flag AND
 * the newline can't be after a C++ comment.
 */
bool is_safe_to_del_nl(
   chunk_t* nl /**< [in] newline chunk to check */
);


/**
 * check if a chunk points to the opening parenthese of a
 * for(...in...) loop in Objective-C.
 *
 *@retval true  - the chunk is the opening parentheses of a for in loop
 *@retval false - no for(...in...) loop found
 */
bool is_forin(
   chunk_t* pc /**< [in] chunk to start search with */
);


#endif /* CHUNK_LIST_H_INCLUDED */
