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

#ifndef __RULESET_H__
#define __RULESET_H__

namespace oasys {

/*!
 * Note this rules storage is static because we don't want behavior to change 
 * when we switch rules around and memory access patterns change. 
 */
struct RuleStorage {
    static const unsigned int MAX_RULES       = 128;
    static const unsigned int MAX_RULE_LENGTH = 256;

    struct Item {
        int  flags_;
        int  log_level_;
        int  priority_; // larger is higher priority
        char rule_[MAX_RULE_LENGTH];
    } items_[MAX_RULES];
};

//----------------------------------------------------------------------------
/*! 
 * A RuleSet is a set of hierarchical rules which define a debugging
 * set. Abstracted out because this kind of thing can be useful in
 * other contexts.
 */
class RuleSet {
public:
    enum {
        //! Prefix matches, priority is string length
        PREFIX    = 1, 
        //! Glob matching (e.g. /foo*)
        GLOB      = 2,
    };

    //! Initialize with empty storage.
    RuleSet(RuleStorage* rs);
    
    //! @return Log level for matching rule that is in effect
    RuleStorage::Item* match_rule(char* rule);
 
    void add_prefix_rule(char* rule, int log_level);
    void add_glob_rule(char* rule, int log_level, int priority);
 
private:
    RuleStorage* rules_;    
    unsigned int num_rules_;

    void add_rule(int flags, char* rule, int log_level, int priority_);
    bool do_match(char* rule, RuleStorage::Item* item);
};

} // namespace oasys


#endif /* __RULESET_H__ */
