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

#ifndef _REGISTRATION_STORE_H_
#define _REGISTRATION_STORE_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/TypeShims.h>
#include <oasys/storage/InternalKeyDurableTable.h>
#include <oasys/util/Singleton.h>

#include "reg/APIRegistration.h"

namespace dtn {

/**
 * Convenience typedef for the oasys template parent class.
 */
typedef oasys::InternalKeyDurableTable<
    oasys::UIntShim, u_int32_t, APIRegistration> RegistrationStoreImpl;

/**
 * The class for registration storage is simply an instantiation of the
 * generic oasys durable table interface.
 */
class RegistrationStore : public oasys::Singleton<RegistrationStore, false>,
                          public RegistrationStoreImpl {
public:
    /**
     * Boot time initializer that takes as a parameter the storage
     * configuration to use.
     */
    static int init(const oasys::StorageConfig& cfg,
                    oasys::DurableStore*        store);
        
    /**
     * Constructor.
     */
    RegistrationStore();

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance() != NULL); }
};

} // namespace dtn

#endif /* _REGISTRATION_STORE_H_ */
