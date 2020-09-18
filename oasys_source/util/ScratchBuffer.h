/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef __STATIC_SCRATCH_BUFFER__
#define __STATIC_SCRATCH_BUFFER__

#include <cstdlib>
#include <cstring>

#include "../debug/DebugUtils.h"
#include "../util/ExpandableBuffer.h"

namespace oasys {

/*!
 * ScratchBuffer template.
 *
 * @param _memory_t    Memory type to return from buf()
 *
 * @param _static_size Size of the buffer to allocate statically from
 *     the stack. This is useful where the size is commonly small, but
 *     unusual and large sizes need to be handled as well. Specifying 
 *     this value will can potentially save many calls to malloc().
 */
template<typename _memory_t = void*, size_t _static_size = 0>
class ScratchBuffer;

/*!
 * ScratchBuffer class that doesn't use a static stack to begin with.
 */
template<typename _memory_t>
class ScratchBuffer<_memory_t, 0> : public ExpandableBuffer {
public:
    ScratchBuffer(size_t size = 0) : ExpandableBuffer(size) {}
    ScratchBuffer(const ScratchBuffer& other) : ExpandableBuffer(other) {}
    
    //! @return Pointer of buffer of size, otherwise 0
    _memory_t buf(size_t size = 0) {
        if (size > buf_len_) {
            reserve(size);
        }
        return reinterpret_cast<_memory_t>(buf_);
    }

    //! @return Read-only pointer to buffer
    const _memory_t buf() const {
        return reinterpret_cast<const _memory_t>(buf_);
    }

};

/*!
 * ScratchBuffer class that uses a static stack allocated to begin
 * with.
 */
template<typename _memory_t, size_t _static_size>
class ScratchBuffer : public ExpandableBuffer {
public:
    //! Standard default constructor
    ScratchBuffer(size_t size = 0)
        : ExpandableBuffer(0)
    {
        buf_     = static_buf_;
        buf_len_ = _static_size;

        if (size > buf_len_) {
            reserve(size);
        }
    }

    //! We need to implement our own copy constructor to make sure to
    //! call the right reserve() call
    ScratchBuffer(const ScratchBuffer& other)
        : ExpandableBuffer(0)
    {
        buf_     = static_buf_;
        buf_len_ = _static_size;
        
        reserve(other.buf_len_);
        memcpy(buf_, other.buf_, other.buf_len_);
        len_ = other.len_;

        ASSERT(using_malloc() == other.using_malloc());
    }

    //! We need to implement the assignment operator for the benefit of
    //! vector operations; otherwise buf_ gets assigned to other.buf_
    //! which is incorrect
    ScratchBuffer& operator=(ScratchBuffer& other)
    {
        if (&other == this)
        {
            return *this;
        }
        
        reserve(other.buf_len_);
        memcpy(buf_, other.buf_, other.buf_len_);
        len_ = other.len_;

        return *this;
    }

    //! The destructor clears the buf_ pointer if pointing at the
    //! static segment so ExpandableBuffer doesn't try to delete it
    virtual ~ScratchBuffer() {
        if (! using_malloc()) {
            buf_ = 0;
        }
    }

    //! @return Pointer of buffer of size, otherwise 0
    _memory_t buf(size_t size = 0) {
        if (size != 0) {
            reserve(size);
        }
        return reinterpret_cast<_memory_t>(buf_);
    }

    //! @return Read-only pointer to buffer
    const _memory_t buf() const {
        return reinterpret_cast<const _memory_t>(buf_);
    }

    //! virtual from ExpandableBuffer
    virtual void reserve(size_t size = 0) {
        if (size == 0) {
            size = (buf_len_ == 0) ? 1 : (buf_len_ * 2);
        }     

        if (size <= buf_len_) {
            return;
        }

        if (! using_malloc()) 
        {
            ASSERT(size > _static_size);
            buf_ = 0;
            size_t old_buf_len = buf_len_;

            ExpandableBuffer::reserve(size);
            memcpy(buf_, static_buf_, old_buf_len);
        } 
        else 
        {
            ExpandableBuffer::reserve(size);
        }
    }

private:
    char static_buf_[_static_size];
    bool using_malloc() const {
        return buf_ != static_buf_;
    }
};

} // namespace oasys

#endif //__STATIC_SCRATCH_BUFFER_H__
