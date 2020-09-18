/*
 *    Copyright 2006 Intel Corporation
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
#ifndef __ITERATORS_H__
#define __ITERATORS_H__

#include "../debug/DebugUtils.h"

namespace oasys {

/*! 
 * A wrapper iterator for counting integers. Useless? I think not!
 */
template<typename Ret>
class CountIterator {
public:
    CountIterator() {}
    CountIterator(Ret begin, Ret end)
        : cur_(begin), end_(end) 
    {
        if (cur_ > end_)
        {
            cur_ = end_;
        }
    }
    
    // Use default assignment, copy
    
    bool operator==(const CountIterator& other) const 
    {
        return cur_ == other.cur_;
    }

    bool operator!=(const CountIterator& other) const 
    {
        return cur_ != other.cur_;
    }    

    CountIterator operator++()
    {
        if (cur_ != end_)
        {
            ++cur_;
        }
        return *this;
    }

    Ret operator*() const 
    {
        return cur_;
    }

    /*!
     * Get the begin and end iterator positions for a specific range
     * of numbers.
     */
    static void get_endpoints(CountIterator* begin, CountIterator* end,
                              Ret r_begin, Ret r_end)
    {
        *begin = CountIterator(r_begin, r_end);
        *end   = CountIterator(r_end, r_end);
    }
    
private:
    Ret cur_;
    Ret end_;
};

/*! 
 * An iterator which combines two iterators. It will first iterate on
 * itr_A, then go to itr_B.
 */
template<typename itr_A, typename itr_B, typename Ret>
class CombinedIterator {
public:
    CombinedIterator() {}
    CombinedIterator(itr_A a_end, itr_B b_end) 
        : a_cur_(a_end), a_end_(a_end), 
          b_cur_(b_end), b_end_(b_end)
    {}
    CombinedIterator(itr_A a_begin, itr_A a_end, itr_B b_begin, itr_B b_end) 
        : a_cur_(a_begin), a_end_(a_end), 
          b_cur_(b_begin), b_end_(b_end)
    {}

    // use default copy and assignment

    bool operator==(const CombinedIterator& other) const
    {
        return a_cur_ == other.a_cur_ && b_cur_ == other.b_cur_;
    }

    bool operator!=(const CombinedIterator& other) const
    {
        return ! operator==(other);
    }    

    CombinedIterator operator++()
    {
        if (a_cur_ != a_end_)
        {
            ++a_cur_;
        }
        else if (b_cur_ != b_end_)
        {
            ++b_cur_;
        }

        return *this;
    }
    
    Ret operator*() const
    { 
        if (a_cur_ != a_end_)
        {
            return *a_cur_;
        }
        if (b_cur_ != b_end_)
        {
            return *b_cur_;
        }
        PANIC("CombinedIterator: deref off end of iteration");
    }
    
    /*!
     * Get the start and end of the composed iterators.
     */
    static void get_endpoints(CombinedIterator* begin, CombinedIterator* end,
                              itr_A a_begin, itr_A a_end, 
                              itr_B b_begin, itr_B b_end)
    {
        *begin = CombinedIterator(a_begin, a_end, b_begin, b_end);
        *end   = CombinedIterator(a_end, b_end);
    }
    
private:
    itr_A a_cur_;
    itr_A a_end_;
    itr_B b_cur_;
    itr_B b_end_;
};

/*!
 * An iterator which iterates over only those items that satisfy the
 * predicate function.
 */  
template<typename Itr, typename Ret, typename Pred>
class PredicateIterator {
public:
    PredicateIterator(Itr begin, Itr end, Pred predicate)
        : cur_(begin), end_(end), predicate_(predicate)
    {
        find_next();
    }
    
    // Use default assignment, copy

    bool operator==(const PredicateIterator& other) const
    {
        return cur_ == other.cur_;        
    }
    bool operator!=(const PredicateIterator& other) const
    {
        return ! operator==(other);
    }

    bool operator==(const Itr& other) const
    {
        return cur_ == other;        
    }
    bool operator!=(const Itr& other) const
    {
        return ! operator==(other);
    }

    PredicateIterator operator++()
    {
        ++cur_;
        find_next();
        return *this;
    }
    
    Ret operator*() const
    {
        return *cur_;
    }
    
private:
    Itr  cur_;
    Itr  end_;
    Pred predicate_;

    /*!
     * Advance the pointer if needed to the next item satisfying the
     * predicate.
     */
    void find_next() 
    {
        while(cur_ != end_ && ! predicate_(*cur_))
        {
            ++cur_;
        }
    }
};

} // namespace oasys

#endif /* __ITERATORS_H__ */
