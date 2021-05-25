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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <cstring>

#include "OptParser.h"

namespace oasys {

//----------------------------------------------------------------------
OptParser::~OptParser()
{
    for (u_int i = 0; i < allopts_.size(); ++i)
    {
        delete allopts_[i];
    }
    allopts_.clear();
}

//----------------------------------------------------------------------
void
OptParser::addopt(Opt* opt)
{
    allopts_.push_back(opt);
}

//----------------------------------------------------------------------
bool
OptParser::parse_opt(const char* opt_str, size_t len, bool* invalid_value)
{
    Opt* opt;
    const char* val_str;
    size_t opt_len, val_len;

    if (invalid_value) {
        *invalid_value = false;
    }

    opt_len = strcspn(opt_str, "= \t\r\n");
    if (opt_len == 0 || opt_len > len) {
        return false;
    }

    if (opt_str[opt_len] != '=') {
        val_str = NULL;
        val_len = 0;
    } else {
        val_str = opt_str + opt_len + 1;
	val_len = len - (opt_len + 1);

        // regardless of what the option is, if there's no value
        // supplied it's invalid
        if (val_len == 0) {
            if (invalid_value) {
                *invalid_value = true;
            }
            return false;
        }
    }
    
    int nopts = allopts_.size(); 
    for (int i = 0; i < nopts; ++i)
    {
        opt = allopts_[i];
        
        if (strncmp(opt_str, opt->longopt_, opt_len) == 0)
        {
            if (opt->needval_ && val_str == NULL) {
                if (invalid_value) {
                    *invalid_value = true;
                }
                return false; // missing value
            }

            if (opt->set(val_str, val_len) != 0) {
                if (invalid_value) {
                    *invalid_value = true;
                }
                return false; // error in set
            }
            
            return true; // all set
        }
    }

    return false; // no matching option
}

//----------------------------------------------------------------------
bool
OptParser::parse(const char* args, const char** invalidp)
{
    const char* opt;
    size_t opt_len;

    opt = args;
    while (1) {
        opt_len = strcspn(opt, " \t\r\n");
        if (opt_len == 0) {
            return true; // all done
        }

        if (parse_opt(opt, opt_len) == false) {
            if (invalidp)
                *invalidp = opt;
            return false;
        }

        // skip past the arg and all other whitespace
        opt = opt + opt_len;
        opt += strspn(opt, " \t\r\n");
    }
}

//----------------------------------------------------------------------
bool
OptParser::parse(int argc, const char* const argv[], const char** invalidp)
{
    for (int i = 0; i < argc; ++i) {
        if (parse_opt(argv[i], strlen(argv[i])) == false) {
            *invalidp = argv[i];
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------
int
OptParser::parse_and_shift(int argc, const char* argv[], const char** invalidp)
{
    int last_slot = 0;
    int valid_count = 0;
    bool invalid_value;

    for (int i = 0; i < argc; ++i) {
        
        if (parse_opt(argv[i], strlen(argv[i]), &invalid_value) == true) {
            ++valid_count;
            
        } else {
            argv[last_slot] = argv[i];
            last_slot++;
            
            if (invalid_value) {
                if (invalidp)
                    *invalidp = argv[i];
                
                return -1; // stop parsing
            }
        }
    }
    
    return valid_count;
}

//----------------------------------------------------------------------
bool
OptParser::parse(const std::vector<std::string>& args,
                 const char** invalidp)
{
    std::vector<std::string>::const_iterator iter;
    for (iter = args.begin(); iter != args.end(); ++iter) {
        if (parse_opt(iter->c_str(), iter->length()) == false) {
            *invalidp = iter->c_str();
            return false;
        }
    }

    return true;
}


} // namespace oasys
