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

#ifndef _SQL_REGISTRATION_STORE_H_
#define _SQL_REGISTRATION_STORE_H_

#include "RegistrationStore.h"

namespace oasys {
class SQLImplementation;
}

namespace dtn {

class SQLStore;

/**
 * Implementation of RegistrationStore that uses an underlying SQL database.
 */
class SQLRegistrationStore : public RegistrationStore {
public:
    /**
     * Constructor
     */
    SQLRegistrationStore(oasys::SQLImplementation* impl,
                         const char* table_name = "registration");

    /**
     * Destructor
     */
    virtual ~SQLRegistrationStore();

    /**
     * Load in the whole database of registrations, populating the
     * given list.
     */
    virtual void load(RegistrationList* reg_list);

    /**
     * Add a new registration to the database. Returns true if the
     * registration is successfully added, false on error.
     */
    virtual bool add(Registration* reg);
    
    /**
     * Remove the registration from the database, returns true if
     * successful, false on error.
     */
    virtual bool del(Registration* reg);
    
    /**
     * Update the registration in the database. Returns true on
     * success, false if there's no matching registration or on error.
     */
    virtual bool update(Registration* reg);

protected:
    SQLStore* store_;
};
} // namespace dtn

#endif /* _SQL_REGISTRATION_STORE_H_ */
