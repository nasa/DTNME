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

#include "util/UnitTest.h"
#include "serialize/StreamSerialize.h"
#include "io/ByteStream.h"

using namespace oasys;

struct Out : public OutByteStream {
    Out() : offset_(0) {}

    char buf_[512];
    
    int begin() { return 0; }
    int end()   { return 0; }
    
    int write(const u_char* buf, size_t len)
    {
        memcpy(buf_ + offset_, buf, len);
        offset_ += len;
        return 0;
    }
    
    size_t offset_;
};

struct In : public InByteStream {
    In() : offset_(0) {}

    char buf_[512];
    
    int begin() { return 0; }
    int end()   { return 0; }

    int read(u_char* buf, size_t len) const
    {
        memcpy(buf, buf_ + offset_, len);
        offset_ += len;
        return 0;
    }
    
    mutable size_t offset_;
};

struct TestObj : public SerializableObject {
    int i_;
    char str[256];
    char str2[256];
    
    void serialize(SerializeAction* action)
    {
        action->process("i", &i_);
        
        if (action->action_code() == Serialize::UNMARSHAL)
        {
            BufferCarrier<char> uc;
            action->process("str", &uc);
            memset(str, 0, 256);
            memcpy(str, uc.buf(), uc.len());

            uc.reset();
            action->process("str2", &uc);
            memset(str2, 0, 256);
            memcpy(str2, uc.buf(), uc.len());
        }
        else
        {
            BufferCarrier<char> uc(str, strlen(str), false);
            action->process("str", &uc);
            uc.set_buf(str2, strlen(str2), false);
            action->process("str2", &uc);
        }
    }
};

DECLARE_TEST(Test1) {
    TestObj obj, copy;
    obj.i_ = 42;
    strcpy(obj.str, "hello, world!");
    strcpy(obj.str2, "|some data and stuff|");

    Out out;

    StreamSerialize ss(&out, Serialize::CONTEXT_LOCAL);
    ss.action(&obj);

    CHECK(! ss.error());
    
    In in;
    memcpy(in.buf_, out.buf_, 512);
    
    StreamUnserialize su(&in, Serialize::CONTEXT_LOCAL);
    su.action(&copy);
    
    CHECK(obj.i_ == 42);
    CHECK(obj.i_ == copy.i_);
    CHECK_EQUALSTR(obj.str, copy.str);
    CHECK_EQUALSTR(copy.str, "hello, world!");
    CHECK_EQUALSTR(obj.str2, copy.str2);
    CHECK_EQUALSTR(copy.str2, "|some data and stuff|");

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(Test) {
    ADD_TEST(Test1);
}

DECLARE_TEST_FILE(Test, "Stream serialize test");
