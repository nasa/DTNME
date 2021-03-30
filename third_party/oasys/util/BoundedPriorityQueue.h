/*
 *    Copyright 2006 Baylor University
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef BOUNDED_PRIORITY_QUEUE
#define BOUNDED_PRIORITY_QUEUE

#include <vector>
#include <queue>
#include <algorithm>

namespace oasys {
/**
 * BoundedPriorityQueue inherits from the std::priority_queue template
 * to include capacity detection and to impose an arbitrary bound.
 *
 * The default capacity is object size, but this can be overridden by
 * supplying a TSize function pattern to read a different capacity from
 * the T object.
 *
 * Elements are sorted according to the priority provided by TCompare,
 * which defaults to using the < operator, if the object defines one.
 * This too can be overridden by supplying a comparator that detects a 
 * different priority from the T object.
 *
 * Member function current() returns the current capacity.  The maximum
 * capacity is set by template variable, but can be updated by passing
 * the new value to the constructor or set_max() method.
 *
 * Bounds are enforced by the constructors and by push().  When a push()
 * operation causes the BoundedPriorityQueue to exceed its maximum
 * capacity, elements of least priority are dropped (by calling
 * pop_back() on the underlying container) until the bound is no longer
 * violated. 
 *
 * Other than the above exceptions, the default behavior is that of
 * priority_queue
 */
template <class T, class TSize,
          class TCompare = std::less<T> >
class BoundedPriorityQueue :
      public std::priority_queue<T,std::vector<T>,TCompare> {

public:

    // Grab a shortcut, PriorityQueue, for scoping to base class below 
    typedef std::vector<T> Sequence;
    typedef std::priority_queue<T,Sequence,TCompare> PriorityQueue;

    /**
     * Default constructor
     * @param max defaults to template parameter, max_size
     */
    BoundedPriorityQueue(u_int max_size) :
        PriorityQueue(),
        cur_size_(0),
        max_size_(max_size)
    {
        init();
    }

    /**
     * Alternate constructor
     * @param comp comparator object
     * @param max defaults to template parametr, max_size
     */
    BoundedPriorityQueue(const TCompare& comp, u_int max_size) :
        PriorityQueue(comp),
        cur_size_(0),
        max_size_(max_size)
    {
        init();
    }

    /**
     * Utilize priority_queue's default behavior, then update
     * bounds tracking with new entry's TSize(), then enforce_bound
     */
    void push(const T& obj)
    {
        PriorityQueue::push(obj);
        cur_size_ += TSize()(obj);
        enforce_bound();
    }

    /**
     * Decrement bounds tracking by this entry's TSize(), then
     * utilize priority_queue's default pop() behavior
     */
    void pop()
    {
        cur_size_ -= TSize()(PriorityQueue::c.front());
        PriorityQueue::pop();
    }

    /**
     * Report current bound status
     */
    u_int current() const
    {
        return cur_size_;
    }

    /**
     * Report maximum bound allowed 
     */
    u_int max() const
    {
        return max_size_;
    }

    /**
     * Update to new maximum bound
     */
    void set_max(u_int max_size)
    {
        max_size_ = max_size;
        enforce_bound();
    }

#ifndef _DEBUG
protected:
#endif
    const Sequence& container() const
    {
        return PriorityQueue::c;
    }

    typedef typename Sequence::const_iterator const_iterator;
    typedef typename Sequence::iterator iterator;

    /**
     * Initialize bounds tracking, then enforce_bound
     */
    void init() {
        const_iterator it = (const_iterator) PriorityQueue::c.begin();
        while(it != (const_iterator) PriorityQueue::c.end()) {
            cur_size_ += TSize()(*it);
        }
        enforce_bound();
    }

    /**
     * Create a stub for derived classes to impose other policy
     * upon object removal
     */
    void erase_member(iterator pos)
    {
        PriorityQueue::c.erase(pos);
    }

    /**
     * Drop lowest priority elements until maximum bounds is no
     * longer exceeded
     */
    void enforce_bound() {
        iterator minObj;
        while(cur_size_ > max_size_) {
            // find the minimum object
            minObj = std::min_element (
                        PriorityQueue::c.begin(),
                        PriorityQueue::c.end(),
                        PriorityQueue::comp);
            // decrement by this node's capacity value
            cur_size_ -= TSize()(*minObj);
            // remove the leaf but break the heap
            erase_member(minObj);
        }
        // re-heapify
        make_heap(PriorityQueue::c.begin(),
                  PriorityQueue::c.end(),
                  PriorityQueue::comp);
    }

    u_int cur_size_; ///< Current bound status
    u_int max_size_; ///< Maximum bound allowed
};

} // oasys
#endif // BOUNDED_PRIORITY_QUEUE
