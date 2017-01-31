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
   MD5();


   ~MD5()
   {
   }

   void Init();
   void Update(const void *data, UINT32 len);


   /**
    * Final wrapup - pad to 64-byte boundary with the bit pattern
    * 1 0* (64-bit count of bits processed, MSB-first)
    */
   void Final(UINT8 digest[16]);

   /* internal function */
   static void Transform(UINT32 buf[4], UINT32 in_data[16]);

   static void Calc(const void *data, UINT32 length, UINT8 digest[16]);

private:
   UINT32 m_buf[M_BUF_SIZE];
   UINT32 m_bits[M_BITS_SIZE];
   UINT8  m_in[M_IN_SIZE];
   bool   m_need_byteswap;
   bool   m_big_endian;

   void reverse_u32(UINT8 *buf, int n_u32) const;
};

#endif /* MD5_H_INCLUDED */
