/**
 * @file ChunkStack.h
 * Manages a simple stack of chunks
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#ifndef CHUNKSTACK_H_INCLUDED
#define CHUNKSTACK_H_INCLUDED

#include "uncrustify_types.h"
#include <deque>

class ChunkStack
{
public:
   struct Entry
   {
      Entry(void)
         : m_seqnum(0)
         , m_pc(0)
      { }


      Entry(const Entry& ref)
         : m_seqnum(ref.m_seqnum)
         , m_pc(ref.m_pc)
      { }


      Entry(uint32_t sn, chunk_t* pc)
         : m_seqnum(sn)
         , m_pc(pc)
      { }
      uint32_t m_seqnum;
      chunk_t* m_pc;
   };


protected:
   deque<Entry> m_cse;    /**<   */
   uint32_t     m_seqnum; /**< current sequence number */


public:

   /** Constructor */
   ChunkStack(void)
      : m_seqnum(0)
   { }


   /** Constructor */
   ChunkStack(const ChunkStack& cs) /**< [in]  */
   {
      Set(cs);
   }


   /** Destructor */
   virtual ~ChunkStack(void) { }


   /** tbd */
   void Set(
      const ChunkStack& cs /**< [in]  */
   );


   /** tbd */
   void Push_Back(chunk_t* pc)
   {
      Push_Back(pc, ++m_seqnum);
   }


   /** tbd */
   bool Empty(void) const
   {
      return(m_cse.empty());
   }


   /** tbd */
   uint32_t Len(void) const
   {
      return(m_cse.size());
   }


   /** tbd */
   const Entry* Top(void) const;


   /** tbd */
   const Entry* Get(
      uint32_t idx /**< [in]  */
   ) const;


   /** tbd */
   chunk_t* GetChunk(
      uint32_t idx /**< [in]  */
   ) const;


   /** tbd */
   chunk_t* Pop_Back(void);


   /** tbd */
   void Push_Back(
      chunk_t* pc,    /**< [in]  */
      uint32_t seqnum /**< [in]  */
   );


   /** tbd */
   chunk_t* Pop_Front(void);


   /** tbd */
   void Reset(void)
   {
      m_cse.clear();
   }


   /** Mark an entry to be removed by Collapse() */
   void Zap(
      uint32_t idx /**< [in] item to remove */
   );


   /** Compresses down the stack by removing dead entries */
   void Collapse(void);
};


#endif   /* CHUNKSTACK_H_INCLUDED */
