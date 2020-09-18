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

#ifndef _ETH_CONVERGENCE_LAYER_H_
#define _ETH_CONVERGENCE_LAYER_H_

// Only works on Linux (for now)
#ifdef __linux__ 

#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>

#include <oasys/thread/Thread.h>
#include <oasys/thread/Timer.h>
#include <oasys/serialize/Serialize.h>

#include "ConvergenceLayer.h"
#include "naming/EthernetScheme.h" // for eth_addr_t

/** 
 *   The EthConvergenceLayer provides access to any ethernet interfaces that
 *  support RAW sockets. It periodically sends beacons out on each interface
 *  to support neighbor discovery. (this may change later). 
 *
 *   To add an ethernet interface in your config file, use:
 *
 *   interface add eth string://eth0
 *
 *   Theoretically, any router type should work, but so far, the only one I've
 *   been using is the NeighborhoodRouter. To use this router, add this to dtn.conf
 *  
 *   route set type neighborhood
 *
 */
namespace dtn {

class EthConvergenceLayer : public ConvergenceLayer {

public:
    class BeaconTimer;

    /**
     * Current version of the protocol.
     */
    static const u_int8_t  ETHCL_VERSION = 0x01;
    static const u_int16_t ETHERTYPE_DTN = 0xd710;

    static const u_int8_t  ETHCL_BEACON = 0x01;
    static const u_int8_t  ETHCL_BUNDLE = 0x02;

    static const u_int16_t MAX_ETHER_PACKET = 1518;

    /**
     * Maximum bundle size
     */
    static const u_int MAX_BUNDLE_LEN = 65507;
    
    /**
     * The basic Eth header structure.
     */
    struct EthCLHeader {
        u_int8_t  version;              ///< ethcl protocol version
        u_int8_t  type;                 ///< 
        u_int16_t _padding2;            ///< 
        u_int32_t bundle_id;            ///< bundle identifier at sender
    } __attribute__((packed));

    /**
     * Constructor.
     */
    EthConvergenceLayer();
        
    /**
     * Bring up a new interface.
     */
    bool interface_up(Interface* iface, int argc, const char* argv[]);
    void interface_activate(Interface* iface);

    /**
     * Bring down the interface.
     */
    bool interface_down(Interface* iface);

    /**
     * Open the connection to a given contact and send/listen for 
     * bundles over this contact.
     */
    bool open_contact(const ContactRef& contact);
    
    /**
     * Close the connnection to the contact.
     */
    bool close_contact(const ContactRef& contact);

    /**
     * Initialize any CL-specific components of the Link.
     */
    bool init_link(const LinkRef& link, int argc, const char* argv[]);

    /**
     * Set default link options.
     */
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);

    /**
     * Dump out CL specific link information.
     */
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);

    /**
     * Delete any CL-specific components of the Link.
     */
    void delete_link(const LinkRef& link);

    /**
     * Send the bundle to the contact
     */
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);

    /**
     * Report if the given bundle is queued on the given link.
     */
    bool is_queued(const LinkRef& contact, Bundle* bundle);
    
    /**
     * Tunable parameter structure.
     *
     * Per-link and per-interface settings are configurable via
     * arguments to the 'link add' and 'interface add' commands.
     *
     * The parameters are stored in each Link's CLInfo slot, as well
     * as part of the Receiver helper class.
     */
    class Params : public CLInfo {
    public:
        /**
         * Virtual from SerializableObject
         */
        virtual void serialize(oasys::SerializeAction *a);

        std::string if_name_;             ///< Interface name to bind sender to
    };
    
    /**
     * Default parameters.
     */
    static Params defaults_;
    
    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */
    class Receiver : public CLInfo,
                     public oasys::Logger,
                     public oasys::Thread
    {
    public:
        /**
         * Constructor.
         */
        Receiver(const char *if_name, EthConvergenceLayer::Params* params);

        /**
         * Destructor.
         */
        virtual ~Receiver() {}
        
        /**
         * Loop forever, issuing blocking calls to IPSocket::recvfrom(),
         * then calling the process_data function when new data does
         * arrive
         * 
         * Note that unlike in the Thread base class, this run() method is
         * public in case we don't want to actually create a new thread
         * for this guy, but instead just want to run the main loop.
         */
        void run();
        
    protected:
        /**
         * Handler to process an arrived packet.
         */
        void process_data(u_char* bp, size_t len);
        char if_name_[IFNAMSIZ];
    };



    /**
     * Helper class (and thread) that manages an "established"
     * connection with a peer daemon (virtual connection in the
     * case of Eth).
     *
     * Only the sender side of the connection needs to initiate
     * a connection. The receiver will just receive data. Therefore,
     * we don't need a passive side of a connection
     */
    class Sender : public CLInfo,
                   public oasys::Logger
    {
    public:
        /**
         * Constructor for the active connection side of a connection.
         */
        Sender(const char* if_name, const ContactRef& contact);

        /**
         * Destructor.
         */
        virtual ~Sender() {}
        
    protected:
        friend class EthConvergenceLayer;
        
        /**
         * Send one bundle.
         */
        bool send_bundle(const BundleRef& bundle);

        /// The contact that we're representing
        ContactRef contact_;
        
        /// Socket identifier
        int sock_;

        /// MAC address of the interface used for this contact
        eth_addr_t src_hw_addr_;
        eth_addr_t dst_hw_addr_; 

        /// The name of the interface the next_hop is behind
        char if_name_[IFNAMSIZ]; 
        
        char canary_[7];

       /**
        * Temporary buffer for formatting bundles. Note that the
        * fixed-length buffer is big enough since UDP packets can't
        * be any bigger than that.
        */
        u_char buf_[EthConvergenceLayer::MAX_BUNDLE_LEN];
    };

protected:
    /**
     * Parses parameters during EthConvegenceLayer initialization
     */
    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);

    virtual CLInfo* new_link_params();
};


} // namespace dtn

#endif // __linux

#endif /* _ETH_CONVERGENCE_LAYER_H_ */
