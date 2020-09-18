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
#  include <dtn-config.h>
#endif

#include <stdio.h>
#include <oasys/debug/Log.h>
#include <oasys/serialize/MarshalSerialize.h>
#include "bundling/Bundle.h"

using namespace dtn;
using namespace oasys;

int
main(int argc, const char** argv)
{
    Log::init();
    
    Bundle b, b2;
    b.bundleid_ = 100;
    ASSERT(!b.source_.valid());
    ASSERT(!b2.source_.valid());
    
    b.source_.assign("bundles://internet/tcp://foo");
    ASSERT(b.source_.valid());
    ASSERT(b.source_.region().compare("internet") == 0);
    ASSERT(b.source_.admin().compare("tcp://foo") == 0);

    MarshalSize s;
    if (s.action(&b) != 0) {
        logf("/test", LOG_ERR, "error in marshal sizing");
        exit(1);
    }

    size_t sz = s.size();
    logf("/test", LOG_INFO, "marshalled size is %zu", sz);

    u_char* buf = (u_char*)malloc(sz);

    Marshal m(buf, sz);
    if (m.action(&b) != 0) {
        logf("/test", LOG_ERR, "error in marshalling");
        exit(1);
    }

    Unmarshal u(buf, sz);
    if (u.action(&b2) != 0) {
        logf("/test", LOG_ERR, "error in unmarshalling");
        exit(1);
    }
    
    if (b.bundleid_ != b2.bundleid_) {
        logf("/test", LOG_ERR, "error: bundle id mismatch");
        exit(1);
    }

    if (!b2.source_.valid()) {
        logf("/test", LOG_ERR, "error: b2 source not valid");
        exit(1);
    }


#define COMPARE(x)                                                      \
    if (b.source_.x().compare(b2.source_.x()) != 0) {                   \
         logf("/test", LOG_ERR, "error: mismatch of %s: %s != %s",      \
              #x, b.source_.x().c_str(), b2.source_.x().c_str());       \
    }

    COMPARE(eid);
    COMPARE(region);
    COMPARE(admin);

    logf("/test", LOG_INFO, "simple marshal test passed");
}
