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
#  include <dtn-config.h>
#endif

#include "SchemeTable.h"
#include "DTNScheme.h"
#include "EthernetScheme.h"
#include "IPNScheme.h"
#include "SessionScheme.h"
#include "StringScheme.h"
#include "WildcardScheme.h"

namespace oasys {
    template <> dtn::SchemeTable* oasys::Singleton<dtn::SchemeTable>::instance_ = 0;
}

namespace dtn {

//----------------------------------------------------------------------
SchemeTable::SchemeTable()
{
    table_["dtn"] = DTNScheme::instance();
    table_["ipn"] = IPNScheme::instance();
    table_["str"] = StringScheme::instance();
    table_["*"]   = WildcardScheme::instance();
#ifdef __linux__
    table_["eth"] = EthernetScheme::instance();
#endif
    table_["dtn-session"] = SessionScheme::instance();
}

//----------------------------------------------------------------------
SchemeTable::~SchemeTable()
{
    table_.clear();
}

//----------------------------------------------------------------------
void
SchemeTable::register_scheme(const std::string& scheme_str,
                             Scheme* scheme)
{
    table_[scheme_str] = scheme;
}

//----------------------------------------------------------------------
Scheme*
SchemeTable::lookup(const std::string& scheme_str)
{
    SchemeMap::iterator iter = table_.find(scheme_str);
    if (iter == table_.end()) {
        return NULL;
    }
    
    return (*iter).second;
}

}
