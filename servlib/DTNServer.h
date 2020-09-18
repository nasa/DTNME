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

#ifndef _DTNSERVER_H_
#define _DTNSERVER_H_

#include <oasys/debug/Logger.h>
#include <oasys/thread/Atomic.h>
#include <oasys/storage/DurableStore.h>

namespace dtn {

class DTNStorageConfig;

/*!
 * Encapsulation class for the "guts" of the server library.
 */
class DTNServer : public oasys::Logger {
public:
    DTNServer(const char* logpath,
              DTNStorageConfig* storage_config);
    ~DTNServer();
    
    DTNStorageConfig* storage_config() { return storage_config_; }

    /*! Initialize storage, components
     *
     * NOTE: This needs to be called with thread barrier and timer
     * system off because of initialization ordering constraints.
     */
    void init();

    //! Initialize the datastore
    bool init_datastore();

    //! Close and sync the data store.
    void close_datastore();
    
    //! Start DTN daemon
    void start();

    //! Parse the conf file
    bool parse_conf_file(std::string& conf_file,
                         bool         conf_file_set);

    //! Shut down the server
    void shutdown();

    //! Typedef for a shutdown procedure
    typedef void (*ShutdownProc) (void* args);

    //! Set an application-specific shutdown handler.
    void set_app_shutdown(ShutdownProc proc, void* data);

private:
    oasys::atomic_t in_shutdown_;

    DTNStorageConfig*     storage_config_;
    oasys::DurableStore*  store_;

    bool init_dir(const char* dirname);
    bool tidy_dir(const char* dirname);
    bool validate_dir(const char* dirname);    

    /**
     * Initialize and register all the server related dtn commands.
     */
    void init_commands();
    
    /**
     * Initialize all components before modifying any configuration.
     */
    void init_components();
};

} // namespace dtn

#endif /* _DTNSERVER_H_ */
