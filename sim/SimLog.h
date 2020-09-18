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

#ifndef _SIMLOG_H_
#define _SIMLOG_H_

#include <oasys/io/FileIOClient.h>
#include <oasys/util/Singleton.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {
class Bundle;
}

using namespace dtn;

namespace dtnsim {

class Node;

/**
 * Class for more structured logging of bundle generation / reception.
 */
class SimLog : public oasys::Singleton<SimLog> {
public:
    SimLog();

    void log_gen(Node* n, Bundle* b)     { log_entry("GEN", n, b); }
    void log_recv(Node* n, Bundle* b)    { log_entry("RECV", n, b); }
    void log_xmit(Node* n, Bundle* b)    { log_entry("XMIT", n, b); }
    void log_arrive(Node* n, Bundle* b)  { log_entry("ARR", n, b); }
    void log_dup(Node* n, Bundle* b)     { log_entry("DUP", n, b); }
    void log_expire(Node* n, Bundle* b)  { log_entry("EXP", n, b); }
    void log_inqueue(Node* n, Bundle* b) { log_entry("INQ", n, b); }

    void flush();
    
protected:
    void log_entry(const char* what, Node* n, Bundle* b);
    
    oasys::FileIOClient* file_;
    oasys::StringBuffer buf_;
};

} // namespace dtnsim

#endif /* _SIMLOG_H_ */
