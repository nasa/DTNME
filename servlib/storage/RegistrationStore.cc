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

#include "RegistrationStore.h"
#include "reg/Registration.h"

namespace oasys {
    template<> dtn::RegistrationStore* oasys::Singleton<dtn::RegistrationStore, false>::instance_ = NULL;
}

namespace dtn {

RegistrationStore::RegistrationStore()
    : RegistrationStoreImpl("RegistrationStore", "/dtn/storage/registrations",
                            "registration", "registrations")
{
}

/**
 * Boot time initializer that takes as a parameter the storage
 * configuration to use.
 */
int
RegistrationStore::init(const oasys::StorageConfig& cfg,
                        oasys::DurableStore*        store) 
{
    if (instance_ != NULL) {
        PANIC("RegistrationStore::init called multiple times");
    }
    instance_ = new RegistrationStore();
    return instance_->do_init(cfg, store);
}


} // namespace dtn

