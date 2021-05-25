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

#include "debug/Log.h"
#include "StringUtils.h"

namespace oasys {

int
tokenize(const std::string& str,
         const std::string& sep,
         std::vector<std::string>* tokens)
{
    size_t start, end;

    tokens->clear();

    start = str.find_first_not_of(sep);
    if (start == std::string::npos || start == str.length()) {
        return 0; // nothing to do
    }
    
    while (1) {
        end = str.find_first_of(sep, start);
        if (end == std::string::npos) {
            end = str.length();
        }
        
        tokens->push_back(str.substr(start, end - start));
        
        if (end == str.length()) {
            break; // all done
        }
        
        start = str.find_first_not_of(sep, end);
        if (start == std::string::npos) {
            break; // all done
        }
    }

    return tokens->size();
}

void
StringSet::dump(const char* log) const
{
    logf(log, LOG_DEBUG, "dumping string set...");
    for (iterator i = begin(); i != end(); ++i) {
        logf(log, LOG_DEBUG, "\t%s", i->c_str());
    }
}

void
StringHashSet::dump(const char* log) const
{
    logf(log, LOG_DEBUG, "dumping string set...");
    //dzdebug for (iterator i = begin(); i != end(); ++i) {
    for (auto i = begin(); i != end(); ++i) {
        logf(log, LOG_DEBUG, "\t%s", i->c_str());
    }
}

//----------------------------------------------------------------------------
const char*
bool_to_str(bool b)
{
    if (b) 
    {
        return "true";
    }
    
    return "false";
}

//----------------------------------------------------------------------------
const char*
str_if(bool b, const char* true_str, const char* false_str)
{
    if (b) 
    {
        return true_str;
    }
    else
    {
        return false_str;
    }
}

} // namespace oasys
