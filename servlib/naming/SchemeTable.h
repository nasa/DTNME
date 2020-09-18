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

#ifndef _SCHEME_TABLE_H_
#define _SCHEME_TABLE_H_

#include <oasys/util/Singleton.h>
#include <oasys/util/StringUtils.h>

namespace dtn {

class Scheme;

/**
 * The table of registered endpoint id schemes.
 */
class SchemeTable : public oasys::Singleton<SchemeTable> {
private:
    friend class oasys::Singleton<SchemeTable>;
    
    /**
     * Constructor -- instantiates and registers all known schemes.
     * Called from the singleton instance() method the first time the
     * table is accessed.
     */
    SchemeTable();

    /**
     * Destructor cleans up the known schemes and is called at
     * shutdown time.
     */
    virtual ~SchemeTable();

public:
    /**
     * Register the given scheme.
     */
    void register_scheme(const std::string& scheme_str, Scheme* scheme);
    
    /**
     * Find the appropriate Scheme instance based on the URI
     * scheme of the endpoint id scheme.
     *
     * @return the instance if it exists or NULL if there's no match
     */ 
    Scheme* lookup(const std::string& scheme_str);

protected:
    static SchemeTable* instance_;
    typedef oasys::StringHashMap<Scheme*> SchemeMap;
    SchemeMap table_;
};

}

#endif /* _SCHEME_TABLE_H_ */
