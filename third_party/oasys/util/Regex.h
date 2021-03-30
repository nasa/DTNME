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


#ifndef __OASYS_REGEX_H__
#define __OASYS_REGEX_H__

#include <string>
#include <sys/types.h>
#include <regex.h>

namespace oasys {

class Regex {
public:
    static const size_t MATCH_LIMIT = 8;

    static int match(const char* regex, const char* str, 
                     int cflags = 0, int rflags = 0);

    Regex(const char* regex, int cflags = 0);
    virtual ~Regex();
    
    int match(const char* str, int flags = 0);

    bool valid() { return compilation_err_ == 0; }
    
    int num_matches();
    const regmatch_t& get_match(size_t i);

    std::string regerror_str(int err);
    
protected:
    int compilation_err_;

    regex_t    regex_;
    regmatch_t matches_[MATCH_LIMIT];
};

class Regsub : public Regex {
public:
    static int subst(const char* regex, const char* str,
                     const char* sub_spec, std::string* result,
                     int cflags = 0, int rflags = 0);
    
    Regsub(const char* regex, const char* sub_spec, int flags = 0);
    ~Regsub();
        
    int subst(const char* str, std::string* result, int flags = 0);

protected:
    std::string sub_spec_;
};
    
} // namespace oasys

#endif /* __OASYS_REGEX_H__ */
