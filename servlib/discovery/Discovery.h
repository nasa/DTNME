/*
 *    Copyright 2006 Baylor University
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

#ifndef _DISCOVERY_H_
#define _DISCOVERY_H_

#include <oasys/debug/Log.h>
#include <string>
#include <list>
#include "naming/EndpointID.h"
#include "Announce.h"

namespace dtn {

/**
 * Abstraction of neighbor discovery agent.
 *
 * Much like Interface, Discovery is generally created by the
 * configuration file / console. Derived classes (such as 
 * IPDiscovery) typically bind to a UDP socket to listen for
 * neighbor beacons.  Bluetooth has built-in discovery mechanisms,
 * so BluetoothDiscovery polls via Inquiry instead of listen()ing
 * on a socket.
 *
 * To advertise a local convergence layer, register its local address
 * (and port) by calling "discovery add_cl".  For each registered CL,
 * Discovery will advertise (outbound) the CL's presence to neighbors,
 * and distribute (inbound) each event of neighbor discovery to each CL.
 */
class Discovery : public oasys::Logger {
public:

    /**
     * Name of this Discovery instance
     */
    const std::string& name() const { return name_; }

    /**
     * Address family represented by this Discovery instance
     */
    const std::string& af() const { return af_; }

    /**
     * Outbound address of advertisements sent by this Discovery instance
     */
    const std::string& to_addr() const { return to_addr_; }

    /**
     * Local address on which to listen for advertisements
     */
    const std::string& local_addr() const { return local_; }

    /**
     * Factory method for instantiating objects from the appropriate
     * derived class
     */
    static Discovery* create_discovery(const std::string& name,
                                       const std::string& afname,
                                       int argc, const char* argv[],
                                       const char** error);
    /**
     * Append snapshot of object state to StringBuffer
     */
    virtual void dump(oasys::StringBuffer* buf);

    /**
     * Close down listening socket and stop the thread.  Derived classes
     * should NOT auto-delete.
     */
    virtual void shutdown() = 0;

    /**
     * Register an Announce to advertise a local convergence
     * layer and to respond to advertisements from neighbors
     */
    virtual bool announce(const char* name, int argc, const char* argv[]);

    /**
     * Remove registration for named announce object 
     */
    virtual bool remove(const char* name);

    /**
     * Handle neighbor discovery out to registered DiscoveryInfo objects
     */
    void handle_neighbor_discovered(const std::string& cl_type,
                                    const std::string& cl_addr,
                                    const EndpointID& remote_eid);

    virtual ~Discovery();
protected:
    typedef std::list<Announce*> List;
    typedef std::list<Announce*>::iterator iterator;

    /**
     * Constructor
     */
    Discovery(const std::string& name,
              const std::string& af);

    /**
     * Configure this Discovery instance
     */
    virtual bool configure(int argc, const char* argv[]) = 0;

    /**
     * Optional handler for new Announce registration
     */
    virtual void handle_announce() {}

    /**
     * Find a registration by name
     */
    bool find(const char *name, iterator* iter);

    std::string name_;    ///< name of discovery agent
    std::string af_;      ///< address family
    std::string to_addr_; ///< outbound address of advertisements sent
    std::string local_;   ///< address of beacon listener
    List list_; 	  ///< registered Announce objects
private:
    Discovery(const Discovery&)
        : oasys::Logger("Discovery","/no/loggy/here")
    {}
}; // class Discovery

}; // namespace dtn

#endif // _DISCOVERY_H_
