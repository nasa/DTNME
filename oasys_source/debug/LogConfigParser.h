/*
 *    Copyright 2006 Intel Corporation
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

#ifndef __LOGCONFIGPARSER_H__
#define __LOGCONFIGPARSER_H__

#include "../util/RuleSet.h"

namespace oasys {

class LogConfigParser { 
public:
    struct Option {
        const char* option_str_;
        int         flag_value_;
    };

    /*! 
     * @param rs RuleSet to add parsed logging entries into.
     *
     * @param options Set of options to parse. End of options has
     * option_str_ == 0.
     *
     */
    LogConfigParser(const char* filename, RuleSet* rs, Option* opts);
    
    /*!
     * @return 0 if there is no errors in parsing the file.
     */
    int parse();

    /*!
     * @return flags Options that were set in the configuration file
     */
    int flags() { return flags_; }

private:
    const char* filename_;
    RuleSet*    rs_;
    Option*     opts_;
    int         flags_;
};

} // namespace oasys

#endif /* __LOGCONFIGPARSER_H__ */
