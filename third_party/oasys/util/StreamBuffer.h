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

#ifndef _OASYS_STREAM_BUFFER_H_
#define _OASYS_STREAM_BUFFER_H_

#include <stdlib.h>

namespace oasys {

/**
 * @brief Stream oriented resizable buffer.
 *
 * StreamBuffer is a resizable buffer which is designed to efficiently
 * support growing at the end of the buffer and consumption of the
 * buffer from the front.
 *
 * XXX/demmer rework to use ExpandableBuffer
 *
 */
class StreamBuffer {
public:
    /**
     * Create a new StreamBuffer with initial size
     */
    StreamBuffer(size_t size = DEFAULT_BUFSIZE);
    ~StreamBuffer();

    /**
     * Set the size of the buffer. New size should not be smaller than
     * size of data in the StreamBuffer.
     */
    void set_size(size_t size);

    /** @return Pointer to the beginning of the total buffer
     * (including potentially consumed parts */
    char* data() { return &buf_[0]; }
    
    /** @return Pointer to the beginning of the data. */
    char* start();
    
    /** @return Pointer to the end of the data. */
    char* end();
    
    /** Reserve amount bytes in the buffer */
    void reserve(size_t amount);

    /** Fill amount bytes, e.g. move the end ptr up by that amount */
    void fill(size_t amount);
    
    /** Consume amount bytes from the front of the buffer */
    void consume(size_t amount);

    /** Clear all data from the buffer */
    void clear();

    /** Total size of the buffer, including full and empty bytes */
    size_t size()   { return size_; }
    
    /** Amount of bytes stored in the stream buffer */
    size_t fullbytes();

    /** Amount of free bytes available at the end of the buffer */
    size_t tailbytes();

private:
    /** Resize buffer to size. */
    void realloc(size_t size);

    /** Compact buffer, moving all data up to the start of the
     * dynamically allocated buffer, eliminating the empty space at
     * the front. */
    void moveup();

    size_t start_;
    size_t end_;
    size_t size_;

    char* buf_;

    static const size_t DEFAULT_BUFSIZE = 512;
};

} // namespace oasys
 
#endif //_OASYS_STREAM_BUFFER_H_
