/**
 * @file combine.h
 * prototypes for combine.c
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef COMBINE_H_INCLUDED
#define COMBINE_H_INCLUDED

#include "uncrustify_types.h"
#include "chunk_list.h"


/**
 * \brief Re-type chunks and combine chunks
 *
 * Change CT_INCDEC_AFTER + WORD to CT_INCDEC_BEFORE
 * Change number/word + CT_ADDR to CT_ARITH
 * Change number/word + CT_STAR to CT_ARITH
 * Change number/word + CT_NEG  to CT_ARITH
 * Change word + ( to a CT_FUNCTION
 * Change struct/union/enum + CT_WORD => CT_TYPE
 * Force parenthesis on return.
 *
 * TODO: This could be done earlier.
 *
 * Patterns detected:
 *   STRUCT/ENUM/UNION + WORD :: WORD => TYPE
 *   WORD + '('               :: WORD => FUNCTION
 */
void fix_symbols(void);


/**
 * Examines the whole file and changes
 * CT_COLON => CT_Q_COLON, CT_LABEL_COLON, or CT_CASE_COLON.
 * CT_WORD before CT_LABEL_COLON => CT_LABEL.
 *
 * Look at all colons ':' and mark labels, :? sequences, etc.
 */
void combine_labels(void);


/**
 * This is called on every chunk.
 * First on all non-preprocessor chunks and then on each preprocessor chunk.
 * It does all the detection and classifying.
 * This is only called by fix_symbols.
 * The three parameters never get the value nullptr.
 * it is not necessary to test.
 */
void do_symbol_check(
   chunk_t* prev, /**< [in]  */
   chunk_t* pc,   /**< [in]  */
   chunk_t* next  /**< [in]  */
);


/**
 * help function for mark_variable_definition...
 */
bool go_on(
   chunk_t* pc,   /**< [in]  */
   chunk_t* start /**< [in]  */
);


/**
 * Sets the parent of the open parenthesis/brace/square/angle and the closing.
 * Note - it is assumed that pc really does point to an open item and the
 * close must be open + 1.
 *
 * @param start   The open parenthesis
 * @param parent  The type to assign as the parent
 * @return        The chunk after the close parenthesis
 */
chunk_t* set_paren_parent(
   chunk_t*  start, /**< [in]  */
   c_token_t parent  /**< [in]  */
);


/**
 * Sets the parent for comments.
 */
void mark_comments(void);


/**
 * tbd
 */
void make_type(
   chunk_t* pc /**< [in]  */
);


/**
 * tbd
 */
void flag_series(
   chunk_t* start,             /**< [in]  */
   chunk_t* end,               /**< [in]  */
   uint64_t set_flags,         /**< [in]  */
   uint64_t clr_flags = 0,     /**< [in]  */
   scope_e  nav = scope_e::ALL /**< [in]  */
);


/**
 * Sets the parent of the open parenthesis/brace/square/angle and the closing.
 * Note - it is assumed that pc really does point to an open item and the
 * close must be open + 1.
 *
 * @param start   The open parenthesis
 * @param parent  The type to assign as the parent
 * @return        The chunk after the close parenthesis
 */
chunk_t* set_paren_parent(
   chunk_t*  start, /**< [in]  */
   c_token_t parent  /**< [in]  */
);


/**
 * This is called on every chunk.
 * First on all non-preprocessor chunks and then on each preprocessor chunk.
 * It does all the detection and classifying.
 * This is only called by fix_symbols.
 * The three parameters never get the value nullptr.
 * it is not necessary to test.
 */
void do_symbol_check(
   chunk_t* prev, /**< [in]  */
   chunk_t* pc,   /**< [in]  */
   chunk_t* next  /**< [in]  */
);


/**
 * Skips over the rest of the template if ang_open is indeed a CT_ANGLE_OPEN.
 * Points to the chunk after the CT_ANGLE_CLOSE.
 * If the chunk isn't an CT_ANGLE_OPEN, then it is returned.
 */
chunk_t* skip_template_next(
   chunk_t* ang_open /**< [in]  */
);


/**
 * Skips over the rest of the template if ang_close is indeed a CT_ANGLE_CLOSE.
 * Points to the chunk before the CT_ANGLE_OPEN
 * If the chunk isn't an CT_ANGLE_CLOSE, then it is returned.
 */
chunk_t* skip_template_prev(
   chunk_t* ang_close /**< [in]  */
);


/**
 * Skips the rest of the array definitions if ary_def is indeed a
 * CT_TSQUARE or CT_SQUARE_OPEN
 */
chunk_t* skip_tsquare_next(
   chunk_t* ary_def /**< [in]  */
);


/**
 * If attr is CT_ATTRIBUTE, then skip it and the parens and return the chunk
 * after the CT_FPAREN_CLOSE.
 * If the chunk isn't an CT_ATTRIBUTE, then it is returned.
 */
chunk_t* skip_attribute_next(
   chunk_t* attr /**< [in]  */
);


/**
 * If fp_close is a CT_FPAREN_CLOSE with a parent of CT_ATTRIBUTE, then skip it
 * and the '__attribute__' thingy and return the chunk before CT_ATTRIBUTE.
 * Otherwise return fp_close.
 */
chunk_t* skip_attribute_prev(
   chunk_t* fp_close /**< [in]  */
);


/**
 * \brief Remove unnecessary returns
 * that is remove 'return;' that appears as the last statement in a function
 */
void remove_extra_returns(void);


#endif /* COMBINE_H_INCLUDED */
