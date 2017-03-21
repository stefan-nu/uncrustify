/**
 * @file ListManager.h
 * Template class that manages items in a double-linked list.
 * If C++ could do it, this would just be a class that worked on an interface.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#ifndef LIST_MANAGER_H_INCLUDED
#define LIST_MANAGER_H_INCLUDED

#include "chunk_list.h"

/**
 * A simple list manager for a double-linked list.
 *
 * This template class can be used with any Class T
 * given that it defines pointers to 'next' and 'prev',
 * which must be pointers to type T.
 */

/* \todo why do we provide this template class? can't we use
 * a double linked list from the standard library ? */

template<class T> class ListManager
{
protected:
   T* first; /**< pointer to the head of list */
   T* last;  /**> pointer to tail of list */

private:
   /* Hide copy constructor */
   ListManager(const ListManager &ref)
   {
      first = nullptr;
      last  = nullptr;
   }

public:
   /** use this enum to define in what direction or location an
    *  operation shall be performed. */
   enum class dir_e : uint32_t
   {
      BEFORE, /**< indicates a position or direction upwards   (=prev) */
      AFTER,  /**< indicates a position or direction downwards (=next) */
   };


   ListManager()
   {
      first = nullptr;
      last  = nullptr;
   }

   /**
    * \brief return the first element of the linked list
    *
    * @return pointer to first element or nullptr if list is empty
    */
   T* GetHead(void)
   {
      return(first);
   }


   /**
    * \brief return the last element of the linked list
    *
    * @return pointer to last element or nullptr if list is empty
    */
   T* GetTail(void)
   {
      return(last);
   }


   /**
    *  \brief return the next element of the linked list
    *
    * @return pointer to next element or nullptr if no next element exists
    */
   T* GetNext(T* ref) /**< [in] pointer to current list element */
   {
      return(ptr_is_valid(ref) ? ref->next : nullptr);
   }


   /**
    * \brief return the previous element of the linked list
    *
    * @return pointer to previous element or nullptr if no previous element exists
    */
   T* GetPrev(T* ref) /**< [in] pointer to current list element */
   {
      return(ptr_is_valid(ref) ? ref->prev : nullptr);
   }


   /**
    * \brief return the the next element of the linked list
    *
    * @return pointer to next list element or nullptr if ref pointer is invalid
    */
   T* Get(
      T*    ref,      /**< [in] pointer to current list element */
      const dir_e dir /**< [in] direction to proceed in list */
   )
   {
      retval_if(ptr_is_invalid(ref), nullptr);
      return (dir == dir_e::BEFORE) ? ref->prev : ref->next;
   }


   /** \brief initialize the pointers of a new list element */
   void InitEntry(
      T* obj /**< [in] object to initialize */
   ) const
   {
      if (ptr_is_valid(obj))
      {
         obj->next = nullptr;
         obj->prev = nullptr;
      }
   }


   /** \brief remove an element from a linked list */
   void Pop(T* obj) /**< [in] element to remove from list */
   {
      if (ptr_is_valid(obj))
      {
         if (first == obj)            { first           = obj->next; }
         if (last  == obj)            { last            = obj->prev; }
         if (ptr_is_valid(obj->next)) { obj->next->prev = obj->prev; }
         if (ptr_is_valid(obj->prev)) { obj->prev->next = obj->next; }
         obj->next = nullptr;
         obj->prev = nullptr;
      }
   }


   /** swap two elements of a list */
   void Swap(
      T* obj1, /**< [in] first  object to swap */
      T* obj2  /**< [in] second object to swap */
   )
   {
      if (ptrs_are_valid(obj1, obj2))
      {
         if      (obj1->prev == obj2) { Pop(obj1); AddBefore(obj1, obj2); }
         else if (obj2->prev == obj1) { Pop(obj2); AddBefore(obj2, obj1); }
         else
         {
            T* prev1 = obj1->prev; Pop(obj1);
            T* prev2 = obj2->prev; Pop(obj2);
            AddAfter(obj1, prev2);
            AddAfter(obj2, prev1);
         }
      }
   }


   /** \brief add a new element to a list */
   void Push(
      T*    obj, /**< [in] new object to add to list */
      T*    ref, /**< [in] reference determines insert position */
      dir_e pos = dir_e::AFTER /**< [in] insert before or after reference */
   )
   {
      assert( (pos == dir_e::AFTER) || (pos == dir_e::BEFORE));
      return_if(ptr_is_invalid(obj));

      bool after = (pos == dir_e::AFTER);

      if (ptr_is_valid(ref))
      {
         Pop(obj); /* \todo is this necessary? */
         obj->next = (after) ? ref->next  : ref;
         obj->prev = (after) ? ref        : ref->prev;
         T** ins   = (after) ? &ref->next : &ref->prev;
         if (ptr_is_valid(*ins))
         {
            T** add = (after) ? &ref->next->prev : &ref->prev->next;
            *add    = obj;
         }
         else
         {
            T** end = (after) ? &last : &first;
            *end    = obj;
         }
         *ins = obj;
      }
      else /* and an element to an empty list */
      {
         if(after) /* add to tail */
         {
            obj->next = nullptr;
            obj->prev = last;
            if (last == nullptr)
            {
               last  = obj;
               first = obj;
            }
            else
            {
               last->next = obj;
            }
            last = obj;
         }
         else /* add to head */
         {
            obj->next = first;
            obj->prev = nullptr;
            if (first == nullptr)
            {
               last  = obj;
               first = obj;
            }
            else
            {
               first->prev = obj;
            }
            first = obj;
         }
      }
   }


   /** \brief add a new element after a reference position in a list */
   void AddAfter(
      T* obj, /**< [in] new element to add to list */
      T* ref  /**< [in] chunk after which to insert new object */
   )
   {
      Push(obj, ref, dir_e::AFTER);
   }


   /** \brief add a new element before a reference position in a list */
   void AddBefore(
      T* obj, /**< [in] new element to add to list */
      T* ref  /**< [in] chunk before to insert new object */
   )
   {
      Push(obj, ref, dir_e::BEFORE);
   }


   /** \brief add a new element to the tail of a list */
   void AddTail(T* obj) /**< [in] new element to add to list */
   {
      Push(obj, last, dir_e::AFTER);
   }


   /** \brief add a new element to the head of a list */
   void AddHead(T* obj) /**< [in] new element to add to list */
   {
      Push(obj, last, dir_e::BEFORE);
   }
};


#endif /* LIST_MANAGER_H_INCLUDED */
