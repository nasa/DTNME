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

#ifndef _OASYS_PRETTY_PRINT_BUFFER_H_
#define _OASYS_PRETTY_PRINT_BUFFER_H_

#include <string>
#include <utility>              // for std::pair

namespace oasys {

/**
 * Class for generating pretty printed text. Somewhat inefficient.
 */
class PrettyPrintBuf {
public:
    PrettyPrintBuf(const char* buf, int len = -1);
 
    bool next_str(std::string* s);

private:
    static const int MAX_COL;

    const char* buf_;
    int cur_;
    int len_;
};

} // namespace oasys

#endif /* _OASYS_PRETTY_PRINT_BUFFER_H_ */
