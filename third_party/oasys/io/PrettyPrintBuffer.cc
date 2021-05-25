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

#include "PrettyPrintBuffer.h"
#include "util/StringBuffer.h"

namespace oasys {

const int PrettyPrintBuf::MAX_COL = 80;

//----------------------------------------------------------------------------
PrettyPrintBuf::PrettyPrintBuf(const char* buf, int len)
    : buf_(buf), cur_(0), len_(len)
{
    if(len_ == -1)
    {
        len_ = strlen(buf);
    }
}

//----------------------------------------------------------------------------
bool
PrettyPrintBuf::next_str(std::string* s)
{
    StringBuffer buf;

    int bound = std::min(cur_ + MAX_COL, len_);
    for(int i = cur_; i<bound; ++i, ++cur_)
    {
        switch(buf_[i])
        { 
        case '\n': buf.append("\\n"); break;
        case '\r': buf.append("\\r"); break;
        case '\t': buf.append("\\t"); break;
        case '\0': buf.append("\\0"); break;

        default:
            buf.append(buf_[i]);
        }
    }

    bool full = (bound == len_) ? true : false;
    s->assign(buf.c_str());
    return full;
}

} // namespace oasys
