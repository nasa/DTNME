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

#ifndef _FILE_CONVERGENCE_LAYER_H_
#define _FILE_CONVERGENCE_LAYER_H_

#include "ConvergenceLayer.h"
#include <oasys/thread/Thread.h>

namespace dtn {

class FileConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    FileConvergenceLayer();

    /// @{
    
    virtual bool interface_up(Interface* iface,
                              int argc, const char* argv[]);
    virtual bool interface_down(Interface* iface);
    virtual bool open_contact(const ContactRef& contact);
    virtual bool close_contact(const ContactRef& contact);
    virtual void send_bundle(const ContactRef& contact, Bundle* bundle);

protected:
    /**
     * Current version of the file cl protocol.
     */
    static const int CURRENT_VERSION = 0x1;
     
    /**
     * Framing header at the beginning of each bundle file.
     */
    struct FileHeader {
        u_int8_t version;		///< framing protocol version
        u_int8_t pad;			///< unused
        u_int16_t header_length;	///< length of the bundle header
        u_int32_t bundle_length;	///< length of the bundle + headers
    } __attribute__((packed));
    
    /**
     * Pull a filesystem directory out of the next hop admin address.
     */
    bool extract_dir(const char* nexthop, std::string* dirp);
    
    /**
     * Validate that a given directory exists and that the permissions
     * are correct.
     */
    bool validate_dir(const std::string& dir);
        
    /**
     * Helper class (and thread) that periodically scans a directory
     * for new bundle files.
     */
    class Scanner : public CLInfo, public oasys::Logger, public oasys::Thread {
    public:
        /**
         * Constructor.
         */
        Scanner(int secs_per_scan, const std::string& dir);

        /**
         * Set the flag to ask it to stop next loop.
         */
        void stop();

        /**
         * Virtual from SerializableObject
         */
        using CLInfo::serialize;
        virtual void serialize( oasys::SerializableObject *) {}

    protected:
        /**
         * Main thread function.
         */
        void run();
        
        int secs_per_scan_;	///< scan interval
        std::string dir_;	///< directory to scan for bundles.
        bool run_;              ///< keep running?
    };
};

} // namespace dtn

#endif /* _FILE_CONVERGENCE_LAYER_H_ */
