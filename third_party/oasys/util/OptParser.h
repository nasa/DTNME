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

#ifndef _OASYS_OPTPARSER_H_
#define _OASYS_OPTPARSER_H_

#include <string>
#include <vector>
#include "Options.h"

namespace oasys {

/*
 * Utility class for parsing options from a single string or an array
 * of strings of the form var1=val1 var2=val2 ...
 */
class OptParser {
public:
    /**
     * Destructor, which also deletes any bound Opt classes.
     */
    virtual ~OptParser();
    
    /**
     * Register a new option binding. This assumes ownership of the
     * object and will call delete on it when the parser is destroyed.
     */
    void addopt(Opt* opt);

    /**
     * Parse the given argument string, processing all registered
     * opts.
     *
     * @return true if the argument string was successfully parsed,
     * false otherwise. If non-null, invalidp is set to point to the
     * invalid option string.
     */
    bool parse(const char* args, const char** invalidp = NULL);
    
    /**
     * Parse the given argument vector, processing all registered
     * opts.
     *
     * @return true if the argument string was successfully parsed,
     * false otherwise. If non-null, invalidp is set to point to the
     * invalid option string.
     */
    bool parse(int argc, const char* const argv[],
               const char** invalidp = NULL);

    /**
     * Parse any matching options from the given argument vector,
     * shifting any non-matching ones to be contiguous at the start of
     * the argv. If there is a matching option with an invalid value,
     * return -1 and set invalidp to point to the bogus argument.
     *
     * @return the number of parsed options
     */
    int parse_and_shift(int argc, const char* argv[],
                        const char** invalidp = NULL);

    /**
     * Parse the given argument vector, processing all registered
     * opts.
     *
     * @return true if the argument string was successfully parsed,
     * false otherwise. If non-null, invalidp is set to point to the
     * invalid option string.
     */
    bool parse(const std::vector<std::string>& args,
               const char** invalidp = NULL);

    /**
     * Parse a single option (or option=value) string.
     * @return true if valid, false otherwise
     */
    bool parse_opt(const char* opt, size_t len,
                   bool* invalid_value = NULL);
                  
protected:
    typedef std::vector<Opt*> OptList;
    OptList allopts_;
};

} // namespace oasys

#endif /* _OASYS_OPTPARSER_H_ */
