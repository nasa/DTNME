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

#include "../debug/DebugUtils.h"
#include "Regex.h"

namespace oasys {

Regex::Regex(const char* regex, int cflags)
{
    compilation_err_ = regcomp(&regex_, regex, cflags);
}

Regex::~Regex()
{
    if (compilation_err_ == 0)
        regfree(&regex_);
}

int
Regex::match(const char* str, int flags)
{
    if (compilation_err_ != 0) {
        return compilation_err_;
    }
    
    return regexec(&regex_, str, MATCH_LIMIT, matches_, flags);
}

int 
Regex::match(const char* regex, const char* str, int cflags, int rflags)
{
    Regex r(regex, cflags);
    return r.match(str, rflags);
}

int
Regex::num_matches()
{
    for(size_t i = 0; i<MATCH_LIMIT; ++i) {
        if (matches_[i].rm_so == -1) {
            return i;
        }
    }

    return MATCH_LIMIT;
}

const regmatch_t&
Regex::get_match(size_t i)
{
    ASSERT(i <= MATCH_LIMIT);
    return matches_[i];
}

std::string
Regex::regerror_str(int err)
{
    char buf[1024];
    size_t len = regerror(err, &regex_, buf, sizeof(buf));
    return std::string(buf, len);
}

Regsub::Regsub(const char* regex, const char* sub_spec, int flags)
    : Regex(regex, flags), sub_spec_(sub_spec)
{
}

Regsub::~Regsub()
{
}

int
Regsub::subst(const char* str, std::string* result, int flags)
{
    int match_err = match(str, flags);
    if (match_err != 0) {
        return match_err;
    }

    size_t len = sub_spec_.length();
    size_t i = 0;
    int nmatches = num_matches();

    result->clear();
    
    while (i < len) {
        if (sub_spec_[i] == '\\') {

            // safe since there's a trailing null in sub_spec
            char c = sub_spec_[i + 1];

            // handle '\\'
            if (c == '\\') {
                result->push_back('\\');
                result->push_back('\\');
                i += 2;
                continue;
            }

            // handle \0, \1, etc
            int match_num = c - '0';
            if ((match_num >= 0) && (match_num < nmatches))
            {
                regmatch_t* match = &matches_[match_num];
                result->append(str + match->rm_so, match->rm_eo - match->rm_so);
                i += 2;
                continue;
            }
            else
            {
                // out of range
                result->clear();
                return REG_ESUBREG;;
            }
            
        } else {
            // just copy the character
            result->push_back(sub_spec_[i]);
            ++i;
        }
    }

    return 0;
}

int
Regsub::subst(const char* regex, const char* str,
              const char* sub_spec, std::string* result,
              int cflags, int rflags)
{
    Regsub r(regex, sub_spec, cflags);
    return r.subst(str, result, rflags);
}

} // namespace oasys
