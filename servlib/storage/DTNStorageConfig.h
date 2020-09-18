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

#ifndef _DTNSTORAGECONFIG_H_
#define _DTNSTORAGECONFIG_H_

#include <oasys/storage/StorageConfig.h>

namespace dtn {

/**
 * Subclass of the basic oasys storage config to add dtn-specific
 * configuration variables.
 */
class DTNStorageConfig : public oasys::StorageConfig {
public:
    DTNStorageConfig(
        const std::string& cmd,
        const std::string& type,
        const std::string& dbname,
        const std::string& dbdir)
        : StorageConfig(cmd, type, dbname, dbdir),
          payload_dir_(""),
          payload_quota_(0),
          payload_fd_cache_size_(32)
    {}

    /// Directory to store payload files
    std::string payload_dir_;

    /// Total quota for the payload storage
    u_int64_t payload_quota_;

    /// Number of payload file descriptors to keep open in a cache.
    u_int payload_fd_cache_size_;
};

} // namespace dtn

#endif /* _DTNSTORAGECONFIG_H_ */
