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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <string.h>
#include "debug/DebugUtils.h"
#include "debug/Log.h"
#include "StreamBuffer.h"

namespace oasys {

/***************************************************************************
 *
 * StreamBuffer
 *
 **************************************************************************/
StreamBuffer::StreamBuffer(size_t size) : 
    start_(0), end_(0), size_(size)
{
    if (size_ == 0)
        size_ = 4;

    buf_ = static_cast<char*>(malloc(size_));
    ASSERT(buf_);
}

StreamBuffer::~StreamBuffer()
{
    if (buf_) 
    {
        free(buf_);
        buf_ = 0;
    }
}

void
StreamBuffer::set_size(size_t size)
{
    ASSERT(fullbytes() <= size);
    moveup();
    
    realloc(size);
}

char*
StreamBuffer::start()
{
    return &buf_[start_];
}

char*
StreamBuffer::end()
{
    return &buf_[end_];
}

void
StreamBuffer::reserve(size_t amount)
{
    if (amount <= tailbytes())
    {
        // do nothing
    } 
    else if (amount <= (start_ + tailbytes())) 
    {
        moveup();
    } 
    else
    {
        moveup();
        realloc(((amount + fullbytes())> size_*2) ? 
                (amount + fullbytes()): (size_*2));
    }

    ASSERT(amount <= tailbytes());
}

void
StreamBuffer::fill(size_t amount)
{
    ASSERT(amount <= tailbytes());
    
    end_ += amount;
}

void
StreamBuffer::consume(size_t amount)
{
    ASSERT(amount <= fullbytes());

    start_ += amount;
    if (start_ == end_)
    {
        start_ = end_ = 0;
    }
}

void
StreamBuffer::clear()
{
    start_ = end_ = 0;
}

size_t
StreamBuffer::fullbytes() 
{
    return end_ - start_;
}

size_t
StreamBuffer::tailbytes() 
{
    return size_ - end_;
}

void
StreamBuffer::realloc(size_t size)
{
    buf_ = (char*)::realloc(buf_, size);
    if (buf_ == 0)
    {
        logf("/StreamBuffer", LOG_CRIT, "Out of memory");
        ASSERT(0);
    }
    
    size_ = size;
}

void
StreamBuffer::moveup()
{
    if (start_ == 0) {
        return;
    }
            
    memmove(&buf_[0], &buf_[start_], end_ - start_);
    end_   = end_ - start_;
    start_ = 0;
}

} // namespace oasys
