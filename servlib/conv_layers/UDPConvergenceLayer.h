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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _UDP_CONVERGENCE_LAYER_H_
#define _UDP_CONVERGENCE_LAYER_H_

#include <oasys/io/UDPClient.h>
#include <oasys/thread/Thread.h>
#include <oasys/io/RateLimitedSocket.h>

#include "IPConvergenceLayer.h"

namespace dtn {

class UDPConvergenceLayer : public IPConvergenceLayer {
public:
    /**
     * Maximum bundle size
     */
    static const u_int MAX_BUNDLE_LEN = 65507;

    /**
     * Default port used by the udp cl.
     */
    static const u_int16_t UDPCL_DEFAULT_PORT = 4556;
    
    /**
     * Constructor.
     */
    UDPConvergenceLayer();
        
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
     * Dump out CL specific interface information.
     */
    void dump_interface(Interface* iface, oasys::StringBuffer* buf);

    /**
     * Create any CL-specific components of the Link.
     */
    bool init_link(const LinkRef& link, int argc, const char* argv[]);

    /**
     * Set default link options.
     */
    bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp);

    /**
     * Delete any CL-specific components of the Link.
     */
    void delete_link(const LinkRef& link);

    /**
     * Dump out CL specific link information.
     */
    void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
    
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
     * Send the bundle out the link.
     */
    void bundle_queued(const LinkRef& link, const BundleRef& bundle);

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
        virtual void serialize( oasys::SerializeAction *a );

        in_addr_t local_addr_;		///< Local address to bind to
        u_int16_t local_port_;		///< Local port to bind to
        in_addr_t remote_addr_;		///< Peer address to connect to
        u_int16_t remote_port_;		///< Peer port to connect to
        oasys::RateLimitedSocket::BUCKET_TYPE bucket_type_;         ///< bucket type for standard or leaky
        uint64_t  rate_;		///< Rate (in bps)
        uint64_t  bucket_depth_;	///< Token bucket depth (in bits)
        bool      wait_and_send_;       ///< Force the socket to wait until sent on rate socket only
    };
    
    /**
     * Default parameters.
     */
    static Params defaults_;

protected:
    bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);
    virtual CLInfo* new_link_params();

    in_addr_t next_hop_addr_;
    u_int16_t next_hop_port_;
    int       next_hop_flags_; 
    /**
     * Helper class (and thread) that listens on a registered
     * interface for incoming data.
     */
    class Receiver : public CLInfo,
                     public oasys::UDPClient,
                     public oasys::Thread
    {
    public:
        /**
         * Constructor.
         */
        Receiver(UDPConvergenceLayer::Params* params);

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

        UDPConvergenceLayer::Params params_;
        
    protected:
        /**
         * Handler to process an arrived packet.
         */
        void process_data(u_char* bp, size_t len);
    };

    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class Sender : public CLInfo, public Logger {
    public:
        /**
         * Destructor.
         */
        virtual ~Sender();

        /**
         * Initialize the sender (the "real" constructor).
         */
        bool init(Params* params, in_addr_t addr, u_int16_t port);
        
    private:
        friend class UDPConvergenceLayer;
        
        /**
         * Constructor.
         */
        Sender(const ContactRef& contact);
        
        /**
         * Send one bundle.
         * @return the length of the bundle sent or -1 on error
         */
        int send_bundle(const BundleRef& bundle, in_addr_t next_hop_addr_, u_int16_t next_hop_port_);

        /**
         * Pointer to the link parameters.
         */
        Params* params_;

        /**
         * The udp client socket.
         */
        oasys::UDPClient socket_;

        /**
         * Rate-limited socket that's optionally enabled.
         */
        oasys::RateLimitedSocket* rate_socket_;
        
        /**
         * The contact that we're representing.
         */
        ContactRef contact_;

        /**
         * Temporary buffer for formatting bundles. Note that the
         * fixed-length buffer is big enough since UDP packets can't
         * be any bigger than that.
         */
        u_char buf_[UDPConvergenceLayer::MAX_BUNDLE_LEN];

    };   
};

} // namespace dtn

#endif /* _UDP_CONVERGENCE_LAYER_H_ */
