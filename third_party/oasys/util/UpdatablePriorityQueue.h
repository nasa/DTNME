/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _UPDATABLE_PRIORITY_QUEUE_H_
#define _UPDATABLE_PRIORITY_QUEUE_H_

#include <algorithm>
#include <queue>
#include <vector>

namespace oasys {

/**
 * The UpdatablePriorityQueue extends std::priority_queue with the
 * ability to adjust the underlying heap after an element's priority
 * has changed.
 */
template<typename _Tp,
         typename _Sequence = std::vector<_Tp>,
         typename _Compare = std::less<typename _Sequence::value_type> >

class UpdatablePriorityQueue : public ::std::priority_queue<_Tp,_Sequence,_Compare> {
public:
    typedef typename _Sequence::value_type value_type;
    typedef typename _Sequence::iterator   iterator;
    
    /**
     * Update the heap to reflect a change in the value of element
     * __x. Requires that __x exists in the queue.
     */
    void update(const value_type& __x)
    {
        typename _Sequence::iterator i;
        
        for (i = this->c.begin(); i != this->c.end(); ++i)
        {
            if ((*i) == __x) {
                this->update(i);
                return;
            }
        }
    }

    /**
     * Update the heap to reflect a change in the value at the given iterator.
     */
    void update(const iterator& i) {
        this->_update(i - this->c.begin());
    }

    /**
     * Accessor for the underlying sequence container, which may be
     * needed to find the element before adjusting its value.
     */
    _Sequence& sequence() { return this->c; }

protected:
    template <typename _Distance>
    void _update(_Distance __x) {
        // For now, we ignore __x and just remake the heap.
        // The more efficient implementation would look at the value
        // and then swap it up the heap or down the heap as needed
        (void)__x;
        std::make_heap(this->c.begin(), this->c.end(), this->comp);
    }
};

}

#endif /* _UPDATABLE_PRIORITY_QUEUE_H_ */
