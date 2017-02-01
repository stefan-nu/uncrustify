/**
 * @file args.h
 * Parses command line arguments.
 *
 * This differs from the GNU/getopt way in that:
 *  - parameters cannot mixed "-e -f" is not the same as "-ef"
 *  - knowledge of the complete set of parameters is not required
 *  - this means you can handle args in multiple spots
 *  - it is more portable
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef ARGS_H_INCLUDED
#define ARGS_H_INCLUDED

#include "base_types.h"

class Args
{
private:
   /**
    * \brief calculates how many bytes are required to store a given number of bits
    */
   static size_t NumberOfBits(
      const int argc /**< [in] number of arguments */
   );


protected:
   size_t  m_count;      /**< number of command line arguments */
   char   **m_values;    /**< pointer array to each argument */
   UINT8  *m_used;       /**< bit array with one flag per argument */

public:

   /**
    * Initializes the argument library.
    * Store the values and allocate enough memory for the 'used' flags.
    * This keeps a reference to argv, so don't change it.
    */
   Args(
      int  argc,  /**< [in] number of command line parameter passed to main() */
      char **argv /**< [in] pointer array to command line parameters */
   );

   /** Standard destructor */
   ~Args();


   /* Copy constructor */
   //Args(const Args &ref);


   /**
    * Checks to see if an arg w/o a value is present.
    * Just scans the args looking for an exact match.
    *
    * "-c" matches "-c", but not "-call" or "-ec"
    *
    * @return  true/false - Whether the argument was present
    */
   bool Present(
      const char *token /**< [in] The token string to match */
   );


   /**
    * Just call arg_params() with an index of 0.
    *
    * Check for an argument with a given value.
    * Returns only the first match.
    *
    * Assuming the token "-c"...
    *   "-call" returns "all"
    *   "-c=all" returns "all"
    *   "-c", "all" returns "all"
    *   "-c=", "all" returns ""
    *
    * @return        NULL or the pointer to the string
    */
   const char *Param(
      const char *token /**< [in] The token string to match */
   );


   /**
    * Similar to arg_param, but can iterate over all matches.
    * Set index to 0 before the first call.
    *
    * @return        NULL or the pointer to the string.
    */
   const char *Params(
      const char *token, /**< [in] The token string to match */
      size_t     &index  /**< [in] Pointer to the index that you initialized to 0 */
   );


   /**
    * Marks an argument as being used.
    */
   void SetUsed(
      size_t idx /**< [in] The index of the argument */
   );


   /**
    * Gets whether an argument has been used, by index.
    */
   bool GetUsed(
      size_t idx /**< [in] The index of the argument */
   ) const;


   /**
    * This function retrieves all unused parameters.
    * You must set the index before the first call.
    * Set the index to 1 to skip argv[0].
    *
    * @return NULL (done) or the pointer to the string
    */
   const char *Unused(
      size_t &idx /**< [in] Pointer to the index */
   ) const;


   /**
    * Takes text and splits it into arguments.
    * args is an array of char pointers that will get populated.
    * num_args is the maximum number of args split off.
    * If there are more than num_args, the remaining text is ignored.
    * Note that text is modified (zeroes are inserted)
    *
    * @return The number of arguments parsed (always <= num_args)
    */
   static size_t SplitLine(
      char   *text,    /**< [in]  text to split, gets modified */
      char   *args[],  /**< [out] array of pointers to be populated */
      size_t num_args  /**< [in]  number of items in input string */
   );
};

#endif /* ARGS_H_INCLUDED */
