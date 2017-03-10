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
 * Class T must define 'next' and 'prev', which must be pointers to type T.
 */

template<class T> class ListManager
{
protected:
   /* Pointers to the head and tail.
    * They are either both nullptr or both non-nullptr.
    */
   T *first;
   T *last;

private:
   /* Hide copy constructor */
   ListManager(const ListManager &ref)
   {
      first = nullptr;
      last  = nullptr;
   }

public:
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
   T *GetHead()
   {
      return(first);
   }


   /**
    * \brief return the last element of the linked list
    *
    * @return pointer to last element or nullptr if list is empty
    */
   T *GetTail()
   {
      return(last);
   }


   /**
    *  \brief return the next element of the linked list
    *
    * @param [in] ref pointer to current list element
    * @return pointer to next element or nullptr if no next element exists
    */
   T *GetNext(T *ref)
   {
      return(ptr_is_valid(ref) ? ref->next : nullptr);
   }


   /**
    * \brief return the previous element of the linked list
    *
    * @param [in] ref pointer to current list element
    * @return pointer to previous element or nullptr if no previous element exists
    */
   T *GetPrev(T *ref)
   {
      return(ptr_is_valid(ref) ? ref->prev : nullptr);
   }


   /**
    * tbd
    */
   void InitEntry(T *obj) const
   {
      if (ptr_is_valid(obj))
      {
         obj->next = nullptr;
         obj->prev = nullptr;
      }
   }


   /**
    * \brief remove an element from a linked list
    *
    * @param [in] obj list element to remove
    */
   void Pop(T *obj)
   {
      if (ptr_is_valid(obj))
      {
         if (first == obj)            { first = obj->next; }
         if (last  == obj)            { last  = obj->prev; }
         if (ptr_is_valid(obj->next)) { obj->next->prev = obj->prev; }
         if (ptr_is_valid(obj->prev)) { obj->prev->next = obj->next; }
         obj->next = nullptr;
         obj->prev = nullptr;
      }
   }


   /**
    * tbd
    */
   void Swap(T *obj1, T *obj2)
   {
      if (ptrs_are_valid(obj1, obj2))
      {
         if      (obj1->prev == obj2) { Pop(obj1); AddBefore(obj1, obj2); }
         else if (obj2->prev == obj1) { Pop(obj2); AddBefore(obj2, obj1); }
         else
         {
            T *prev1 = obj1->prev;
            Pop(obj1);

            T *prev2 = obj2->prev;
            Pop(obj2);

            AddAfter(obj1, prev2);
            AddAfter(obj2, prev1);
         }
      }
   }


   /** use this enum to define in what direction or location an
    *  operation shall be performed. */
   enum class loc_e : unsigned int
   {
      BEFORE, /**< indicates a position or direction upwards   (=prev) */
      AFTER,  /**< indicates a position or direction downwards (=next) */
   };


   /** add a new element to a list */
   void Add(
      T *obj, /**< [in] new object to add to list */
      T *ref, /**< [in] reference determines insert position */
      loc_e pos = loc_e::AFTER /**< [in] insert before or after reference */
   )
   {
      assert( (pos == loc_e::AFTER) || (pos == loc_e::BEFORE));
      return_if(ptr_is_invalid(obj));

      bool after = (pos == loc_e::AFTER);

      if (ptr_is_valid(ref))
      {
         Pop(obj); /* \todo is this necessary? */
         obj->next = (after) ? ref->next  : ref;
         obj->prev = (after) ? ref        : ref->prev;
         T **ins   = (after) ? &ref->next : &ref->prev;
         if (ptr_is_valid(*ins))
         {
            T **add = (after) ? &ref->next->prev : &ref->prev->next;
            *add    = obj;
         }
         else
         {
            T **end = (after) ? &last : &first;
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


   /** add a new element after a reference position in a list */
   void AddAfter(
      T *obj, /**< [in] new object to add to list */
      T *ref  /**< [in] chunk after which to insert new object */
   )
   {
      Add(obj, ref, loc_e::AFTER);
   }


   /** add a new element before a reference position in a list */
   void AddBefore(
      T *obj, /**< [in] new object to add to list */
      T *ref  /**< [in] chunk before to insert new object */
   )
   {
      Add(obj, ref, loc_e::BEFORE);
   }


   /** add a new element to the tail of a list */
   void AddTail(T *obj)
   {
      Add(obj, last, loc_e::AFTER);
#if 0
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
#endif
   }


   /** add a new element to the head of a list */
   void AddHead(T *obj)
   {
      Add(obj, last, loc_e::BEFORE);
#if 0
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
#endif
   }
};


#endif /* LIST_MANAGER_H_INCLUDED */
