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
#  include <dtn-config.h>
#endif

#if SQL_ENABLED

#include "SQLRegistrationStore.h"
#include "SQLStore.h"

namespace dtn {

/**
 * Constructor
 */
SQLRegistrationStore::SQLRegistrationStore(oasys::SQLImplementation* impl,
                                           const char* table_name)
{
    store_ = new SQLStore(table_name, impl);
}

/**
 * Destructor
 */
SQLRegistrationStore::~SQLRegistrationStore()
{
    NOTREACHED;
}

/**
 * Load in the whole database of registrations, populating the
 * given list.
 */
void
SQLRegistrationStore::load(RegistrationList* reg_list)
{
    // NOTIMPLEMENTED
}

/**
 * Add a new registration to the database. Returns true if the
 * registration is successfully added, false on error.
 */
bool
SQLRegistrationStore::add(Registration* reg)
{
    //NOTIMPLEMENTED;
    return true;
}

/**
 * Remove the registration from the database, returns true if
 * successful, false on error.
 */
bool
SQLRegistrationStore::del(Registration* reg)
{
    NOTIMPLEMENTED;
}
    
/**
 * Update the registration in the database. Returns true on
 * success, false if there's no matching registration or on error.
 */
bool
SQLRegistrationStore::update(Registration* reg)
{
    NOTIMPLEMENTED;
}

} // namespace dtn

#endif /* SQL_ENABLED */
