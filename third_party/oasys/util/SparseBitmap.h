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

#ifndef _OASYS_SPARSE_BITMAP_H_
#define _OASYS_SPARSE_BITMAP_H_

#include <vector>

#include "../debug/DebugUtils.h"
#include "../debug/Formatter.h"
#include "../serialize/Serialize.h"
#include "../serialize/SerializableVector.h"
#include "../util/StringAppender.h"

namespace oasys {

/**
 * This class represents a bitmap, where the implementation is biased
 * towards space efficiency rather than lookup time. To that end, the
 * implementation uses a vector of bitmap entries, each storing a start
 * and an end value.
 *
 * Note that in all cases, ranges must be well-formed, i.e. the end
 * must always be greater than start.
 *
 * XXX/bowei -- this isn't a bitmap. that still needs to be
 * implemented.
 */
template <typename _inttype_t>
class SparseBitmap : public Formatter,
                     public SerializableObject {
public:
    /**
     * Default constructor.
     */
    SparseBitmap();

    /**
     * Builder constructor.
     */
    SparseBitmap(const Builder&);

    /**
     * Set the given bit range to true.
     */
    void set(_inttype_t start, _inttype_t len = 1);

    /**
     * Set the given bit range to false.
     */
    void clear(_inttype_t start, _inttype_t len = 1);

    /**
     * Check if the given range is set.
     */
    bool is_set(_inttype_t start, _inttype_t len = 1);

    /**
     * Array operator overload for syntactic sugar.
     */
    bool operator[](_inttype_t val)
    {
        return is_set(val, 1);
    }

    /**
     * Return the total number of set bits.
     */
    _inttype_t num_set();

    /**
     * Return whether or not the bitmap is empty
     */
    bool empty() { return bitmap_.empty(); }

    /**
     * Return the number of range entries (for testing only).
     */
    size_t num_entries() { return bitmap_.size(); }

    /**
     * Clear the whole bitmap.
     */
    void clear() { bitmap_.clear(); }
    
    /**
     * Return the total number of contiguous bits on the left of the
     * range.
     */
    _inttype_t num_contiguous();

    /**
     * Virtual from Formatter.
     */
    int format(char* bp, size_t len) const;
    
    /**
     * Virtual from SerializableObject
     */
    void serialize(SerializeAction* a);
    
protected:
    class Range : public SerializableObject {
    public:
        /// Basic constructor
        Range(_inttype_t start, _inttype_t end)
            : start_(start), end_(end) {}

        /// Builder constructor
        Range(const Builder&)
            : start_(0), end_(0) {}
        
        /// virtual from SerializableObject
        void serialize(SerializeAction* a);
        
        _inttype_t start_;
        _inttype_t end_;
    };
    
    typedef SerializableVector<Range> RangeVector;
    RangeVector bitmap_;
    
    void validate();

public:
    /**
     * An STL-like iterator class. However, to keep in-line with the
     * sparse nature of the class, incrementing the iterator advances
     * only through over the set bits in the bitmap, and the
     * dereference operator returns the offset of the set bit.
     *
     * For example, if bits 1, 5, and 10 are set, then dereferencing
     * the return from ::begin() returns 1, incrementing and
     * dereferencing returns 5, etc.
     */
    class iterator {
    public:
        /**
         * Constructor to initialize an empty iterator.
         */
        iterator();

        /**
         * Dereference operator returns the current bit offset.
         */
        _inttype_t operator*();

        /**
         * Prefix increment operator.
         */
        iterator& operator++();

        /**
         * Postfix increment operator.
         */
        iterator operator++(int);

        /**
         * Addition operator.
         */
        iterator operator+(unsigned int diff);

        /**
         * Addition and assigment operator.
         */
        iterator& operator+=(unsigned int diff);

        /**
         * Prefix decrement operator.
         */
        iterator& operator--();

        /**
         * Postfix decrement operator.
         */
        iterator operator--(int);
        
        /**
         * Subtraction operator.
         */
        iterator operator-(unsigned int diff);

        /**
         * Subtraction and assigment operator.
         */
        iterator& operator-=(unsigned int diff);

        /**
         * Equality operator.
         */
        bool operator==(const iterator& other);

        /**
         * Inequality operator.
         */
        bool operator!=(const iterator& other);

        /**
         * Advance past any contiguous bits, returning an iterator at
         * the last contiguous bit that's set. The iterator must not
         * be pointing at end() for this to be called.
         */
        iterator& skip_contiguous();

    private:
        friend class SparseBitmap<_inttype_t>;

        /// Private constructor used by begin() and end()
        iterator(typename RangeVector::iterator iter,
                 _inttype_t offset);

        /// iterator to the current Range
        typename RangeVector::iterator iter_;
        
        /// offset from start_ in the range
        _inttype_t offset_;
    };

    /**
     * Return an iterator at the start of the vector
     */
    iterator begin();

    /**
     * Return an iterator at the end of the vector
     */
    iterator end();

    /**
     * Syntactic sugar to get the first bit set.
     */
    _inttype_t first() { return *begin(); }

    /**
     * Syntactic sugar to get the last bit set.
     */
    _inttype_t last() { return *--end(); }
};

#include "SparseBitmap.tcc"

} // namespace oasys

#endif /* _OASYS_SPARSE_BITMAP_H_ */
