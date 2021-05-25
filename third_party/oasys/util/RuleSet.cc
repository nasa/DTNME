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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <cstring>
#include <algorithm>

#include "Glob.h"
#include "RuleSet.h"

namespace oasys {

//----------------------------------------------------------------------------
RuleSet::RuleSet(RuleStorage* rs)
    : rules_(rs), num_rules_(0)
{}

//----------------------------------------------------------------------------
RuleStorage::Item*  
RuleSet::match_rule(char* rule)
{
    RuleStorage::Item* match = 0;
    int cur_priority = -1000;

    for (unsigned int i = 0; i < num_rules_; ++i)
    {
        RuleStorage::Item* item = &rules_->items_[i];
        if (do_match(rule, item) && item->priority_ > cur_priority) 
        {
            match        = item;
            cur_priority = item->priority_;
        }
    }

    return match;
}

//----------------------------------------------------------------------------
void
RuleSet::add_prefix_rule(char* rule, int log_level)
{
    add_rule(PREFIX, rule, log_level, strlen(rule));
}

//----------------------------------------------------------------------------
void
RuleSet::add_glob_rule(char* rule, int log_level, int priority)
{
    add_rule(GLOB, rule, log_level, priority);
}

//----------------------------------------------------------------------------
void 
RuleSet::add_rule(int flags, char* rule, int log_level, int priority)
{ 
    // Just ignore if we have too many rules
    if (num_rules_ > rules_->MAX_RULES) {
        return;
    }    

    RuleStorage::Item* item;

    item = &rules_->items_[num_rules_];
    memcpy(item->rule_, rule, std::min(rules_->MAX_RULE_LENGTH, strlen(rule)));
    item->flags_     = flags;
    item->log_level_ = log_level;
    item->priority_  = priority;

    num_rules_++;
}
  
//----------------------------------------------------------------------------
bool 
RuleSet::do_match(char* rule, RuleStorage::Item* item)
{
    switch (item->flags_) {
    case PREFIX:
        if (strstr(rule, item->rule_) == rule) {
            return true;
        }
        break;
    case GLOB:
        if (Glob::fixed_glob(item->rule_, rule)) {
            return true;
        }
        break;
    }

    return false;
}   
    
} // namespace oasys

#if 0

#include <stdio.h>

void
test(oasys::RuleSet* rs, char* rule) 
{
    oasys::RuleStorage::Item* item;

    item = rs->match_rule(rule);
    if (item == 0) { 
        printf("%s - no match\n", rule);
    } else {
        printf("%s:%s - %d\n", rule, item->rule_, item->log_level_);
    }
}

int
main()
{
    oasys::RuleStorage s;
    oasys::RuleSet rs(&s);

    rs.add_prefix_rule("/foo", 1);
    rs.add_prefix_rule("/foo/bar", 2);
    rs.add_glob_rule("*baz", 3, 1000);
    rs.add_glob_rule("*middle*", 4, 1000);

    test(&rs, "/foo");
    test(&rs, "/foo/gar");
    test(&rs, "/foo/bar"); 
    test(&rs, "/foo/bart"); 
    test(&rs, "/foo/bar/boo"); 
    test(&rs, "/foo/baz");
    test(&rs, "/this/is/a");
    test(&rs, "/this/is/a/long/baz");
    test(&rs, "/thismiddle");
}

#endif
