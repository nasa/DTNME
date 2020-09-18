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

#ifndef _ROUTER_INFO_H_
#define _ROUTER_INFO_H_

#include <oasys/debug/Formatter.h>

namespace oasys {
class StringBuffer;
}

namespace dtn {

/**
 * Empty wrapper class to encapsulate router-specific data attached to
 * Links.
 */
class RouterInfo {
public:
    virtual ~RouterInfo();
    virtual void dump(oasys::StringBuffer* buf);
    virtual void dump_stats(oasys::StringBuffer* buf);
};

} // namespace dtn

#endif /* _ROUTER_INFO_H_ */
