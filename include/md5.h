/**
 * @file md5.h
 * A simple class for MD5 calculation
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef MD5_H_INCLUDED
#define MD5_H_INCLUDED

#include "base_types.h"

#define M_IN_SIZE   64
#define M_BITS_SIZE  2
#define M_BUF_SIZE   4

class MD5
{
public:
   MD5(void);


   ~MD5(void)
   {
   }

   /**
    * tbd
    */
   void Init(void);


   /**
    * blocks the data and converts bytes into longwords for
    * the transform routine.
    */
   void Update(
      const void* data, /**< [in]  */
      uint32_t    len   /**< [in]  */
   );


   /**
    * Final wrap-up - pad to 64-byte boundary with the bit pattern
    * 1 0* (64-bit count of bits processed, MSB-first)
    */
   void Final(
      uint8_t digest[16] /**< [out] calculated MD5 checksum */
   );


   /*
    * The core of the MD5 algorithm, this alters an existing MD5 hash to
    * reflect the addition of 16 longwords of new data. MD5::Update blocks
    * the data and converts bytes into longwords for this routine.
    */
   static void Transform(
      uint32_t       buf[4],     /**< [in]  */
      const uint32_t in_data[16] /**< [in]  */
   );


   /**
    * Calculates MD5 for a block of data
    */
   static void Calc(
      const void*    data,      /**< [in] data to calculate MD5 for */
      const uint32_t length,    /**< [in] number of bytes in data */
      uint8_t        digest[16] /**< [out] calculated MD5 checksum */
   );


private:
   uint32_t m_buf [M_BUF_SIZE];
   uint32_t m_bits[M_BITS_SIZE];
   uint8_t  m_in  [M_IN_SIZE];
   bool     m_need_byteswap;
   bool     m_big_endian;

   /**
    * Reverse the bytes in 32-bit chunks.
    * 'buf' might not be word-aligned.
    */
   void reverse_u32(
      uint8_t* buf,  /**< [in] The byte array to reverse */
      uint32_t n_u32 /**< [in] The number of uint32_t's in the data */
   ) const;
};

#endif /* MD5_H_INCLUDED */
