/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef __SERIALIZESTREAM_H__
#define __SERIALIZESTREAM_H__

#include "../io/IOClient.h"

namespace oasys {

//----------------------------------------------------------------------------
/*!
 * Stream is the consumer/producer of bits which the Serialization
 * adaptors use to reconstitute objects.
 */
class SerializeStream {
public:
    virtual ~SerializeStream() {}
    
    //! Allow const buffers to be passed in
    int process_bits(const char* buf, size_t size) {
        return process_bits(const_cast<char*>(buf), size);
    }

    /*!
     * Process size bits into/out of the stream.
     *
     * @return Bytes processed, less than 0 on error.
     */
    virtual int process_bits(char* buf, size_t size) = 0;
};


//----------------------------------------------------------------------------
/*!
 * A stream which wraps writing byte array.
 */
template<typename _Copy>
class MemoryStream : public SerializeStream {
public:
    MemoryStream(const char* buf, size_t size)
        : buf_(const_cast<char*>(buf)), 
          size_(size), pos_(0) {}

    virtual ~MemoryStream() {}

    //! virtual from SerializeStream
    int process_bits(char* buf, size_t size) {
        if (pos_ + size > size_) {
            return -1;
        }
        _Copy::op(buf, buf_ + pos_, size);            
        pos_ += size;

        return size;
    }
    
private:
    char*  buf_;
    size_t size_;
    size_t pos_;                //!< Current position in the stream
};


//----------------------------------------------------------------------------
template<typename _IO_Op>
class IOStream : public SerializeStream {
public:
    IOStream(IOClient* io) : io_(io) {}
    
    virtual ~IOStream() {}
    
    //! virtual from SerializeStream
    int process_bits(char* buf, size_t size) {
        return _IO_Op::op(io_, buf, size);
    }

private:
    IOClient* io_;
};


//----------------------------------------------------------------------------
namespace StreamOps {

struct CopyTo {
    static void* op(void* dest, void* src, size_t n) {
        return memcpy(dest, src, n);
    }
};
struct CopyFrom {
    static void* op(void* src, void* dest, size_t n) {
        return memcpy(dest, src, n);
    }
};

struct Read {
    static int op(IOClient* io, char* buf, size_t size) {
        return io->readall(buf, size);
    }
};

struct Write {
    static int op(IOClient* io, char* buf, size_t size) {
        return io->readall(buf, size);
    }
};

} // namespace StreamOps

typedef MemoryStream<StreamOps::CopyTo>   ReadMemoryStream;
typedef MemoryStream<StreamOps::CopyFrom> WriteMemoryStream;
typedef IOStream<StreamOps::Read>         ReadIOStream;
typedef IOStream<StreamOps::Write>        WriteIOStream;


//----------------------------------------------------------------------------
class WriteBase64Stream : public SerializeStream {
public:
    WriteBase64Stream(SerializeStream* stream);

    int process_bits(char* buf, size_t size);

private:
    SerializeStream* stream_;
};

} // namespace oasys

#endif /* __SERIALIZESTREAM_H__ */
