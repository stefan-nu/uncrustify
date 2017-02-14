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
      Entry()
         : m_seqnum(0)
         , m_pc(0)
      {
      }


      Entry(const Entry &ref)
         : m_seqnum(ref.m_seqnum)
         , m_pc(ref.m_pc)
      {
      }


      Entry(size_t sn, chunk_t *pc)
         : m_seqnum(sn)
         , m_pc(pc)
      {
      }
      size_t  m_seqnum;
      chunk_t *m_pc;
   };


protected:
   deque<Entry> m_cse;
   size_t       m_seqnum; // current sequence number


public:

   /**
    * Constructor
    */
   ChunkStack()
      : m_seqnum(0)
   {
   }


   /**
    * Constructor
    */
   ChunkStack(const ChunkStack &cs)
   {
      Set(cs);
   }


   /**
    * Destructor
    */
   virtual ~ChunkStack()
   {
   }


   /**
    * tbd
    */
   void Set(
      const ChunkStack &cs
   );


   /**
    * tbd
    */
   void Push_Back(chunk_t *pc)
   {
      Push_Back(pc, ++m_seqnum);
   }


   /**
    * tbd
    */
   bool Empty() const
   {
      return(m_cse.empty());
   }


   /**
    * tbd
    */
   size_t Len() const
   {
      return(m_cse.size());
   }


   /**
    * tbd
    */
   const Entry *Top() const;


   /**
    * tbd
    */
   const Entry *Get(
      size_t idx
   ) const;


   /**
    * tbd
    */
   chunk_t *GetChunk(
      size_t idx
   ) const;


   /**
    * tbd
    */
   chunk_t *Pop_Back();


   /**
    * tbd
    */
   void Push_Back(
      chunk_t *pc,
      size_t  seqnum
   );


   /**
    * tbd
    */
   chunk_t *Pop_Front();


   /**
    * tbd
    */
   void Reset()
   {
      m_cse.clear();
   }


   /**
    * Mark an entry to be removed by Collapse()
    */
   void Zap(
      size_t idx /**< [in] item to remove */
   );


   /**
    * Compresses down the stack by removing dead entries
    */
   void Collapse();
};


#endif   /* CHUNKSTACK_H_INCLUDED */
