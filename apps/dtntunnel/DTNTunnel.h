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

#ifndef _DTNTUNNEL_H_
#define _DTNTUNNEL_H_


// if linux then include the version macros to only include the transparent
// feature if the kernel version is greater than or equal to 2.6.31  
#ifdef __linux__
#    include <linux/version.h>
#endif

#include <map>

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include <dtn_api.h>
#include <APIBundleQueue.h>
#include <oasys/debug/Log.h>
#include <oasys/thread/Mutex.h>
#include <oasys/util/App.h>
#include <oasys/util/Singleton.h>

namespace dtntunnel {

class TCPTunnel;
class UDPTunnel;

/**
 * Main wrapper class for the DTN Tunnel.
 */
class DTNTunnel : public oasys::App,
                  public oasys::Singleton<DTNTunnel>
{
public:
    /// Constructor
    DTNTunnel();

    /// Struct to encapsulate the header sent with each tunneled
    /// bundle. Note that since it is declared as a packed struct, it
    /// can be sent over the wire as-is.
    ///
    /// XXX/demmer if this is used for non-IP tunnels, the address
    /// fields will need to be union'd or something like that
    struct BundleHeader {
        BundleHeader()
        {
            memset(this, 0, sizeof(BundleHeader));
        }
        
        BundleHeader(u_int8_t  protocol,
                     u_int8_t  eof,
                     u_int32_t connection_id,
                     u_int32_t seqno,
                     u_int32_t client_addr,
                     u_int32_t remote_addr,
                     u_int16_t client_port,
                     u_int16_t remote_port)
            : protocol_(protocol),
              eof_(eof),
              connection_id_(connection_id),
              seqno_(seqno),
              client_addr_(client_addr),
              remote_addr_(remote_addr),
              client_port_(client_port),
              remote_port_(remote_port)
        {
        }

        u_int8_t  protocol_;
        u_int8_t  eof_;
        u_int32_t connection_id_;
        u_int32_t seqno_;
        u_int32_t client_addr_;
        u_int32_t remote_addr_;
        u_int16_t client_port_;
        u_int16_t remote_port_;
                             
    } __attribute__((packed));

    /// Hook for various tunnel classes to send a bundle. Assumes
    /// ownership of the passed-in bundle
    ///
    /// @return DTN_SUCCESS on success, a DTN_ERRNO value on error
    int send_bundle(dtn::APIBundle* bundle, dtn_endpoint_id_t* dest_eid);

    /// Called for arriving bundles
    int handle_bundle(dtn_bundle_spec_t* spec,
                      dtn_bundle_payload_t* payload);

    /// Main application loop
    int main(int argc, char* argv[]);

    /// Virtual from oasys::App
    void fill_options();
    void validate_options(int argc, char* const argv[], int remainder);

    /// output the statistics
    void output_throughput_stats();

    /// Get destination eid from the dest_eid_table
    void get_dest_eid(in_addr_t remote_addr, dtn_endpoint_id_t* dest_eid);

    /// Accessors
    u_int max_size()              { return max_size_; }
    u_int delay()                 { return delay_; }
    u_int delay_set()             { return delay_set_; }
    bool reorder_udp()            { return reorder_udp_; }
    bool transparent()            { return transparent_; }
    dtn_endpoint_id_t* dest_eid() { return &dest_eid_; }
    u_int32_t recv_bufsize()      { return socket_recv_bufsize_; }
    u_int32_t send_bufsize()      { return socket_send_bufsize_; }

protected:
    UDPTunnel*          udptunnel_;
    TCPTunnel*          tcptunnel_;

    dtn_handle_t 	recv_handle_;
    dtn_handle_t 	send_handle_;
    oasys::Mutex	send_lock_;
    bool                listen_;
    dtn_endpoint_id_t 	local_eid_;
    dtn_endpoint_id_t 	dest_eid_;
    bool		custody_;
    u_int		expiration_;
    bool                tcp_;
    bool                udp_;
    bool                reorder_udp_;
    in_addr_t		local_addr_;
    u_int16_t		local_port_;
    in_addr_t		remote_addr_;
    u_int16_t		remote_port_;
    u_int		delay_;
    bool		delay_set_;
    u_int		max_size_;
    std::string	        tunnel_spec_;
    bool	        tunnel_spec_set_;
    u_int32_t           socket_recv_bufsize_;
    u_int32_t           socket_send_bufsize_;
    bool		transparent_;
    std::string         api_ip_;
    u_int               api_port_;
    bool                api_ip_set_;

    // ECOS values
    bool        ecos_enabled_;
    bool        ecos_critical_;
    bool        ecos_streaming_;
    bool        ecos_reliable_;
    bool        ecos_flow_label_set_;
    u_int32_t   ecos_flags_;
    u_int32_t   ecos_ordinal_;
    u_int32_t   ecos_flow_label_;


    /// Helper class to output statistics
    class StatusThread : public oasys::Thread, public oasys::Logger {
    public:
        /// Constructor
        StatusThread();

    protected:
        /// Main listen loop
        void run();
    };
    
    /// Statistics structure definition
    struct ThroughputStats {
        u_int64_t bundles_per_sec_;
        u_int64_t bytes_per_sec_;
        u_int64_t total_bundles_;
        u_int64_t seq_ctr_;
        u_int64_t seq_ctr_jumps_;
        u_int64_t seq_ctr_jumpsize_;
        u_int64_t seq_ctr_backfills_;
        u_int64_t seq_ctr_missed_;
    };
    ThroughputStats throughput_stats_;
    bool throughput_stats_enabled_;
    bool throughput_stats_seqctr_enabled_;
    mutable oasys::SpinLock throughput_stats_lock_;

    /// Helper struct for network IP address in CIDR notation
    struct CIDR {
	CIDR()
	    : addr_(0),
	      bits_(0) {}
	
	CIDR(u_int32_t addr, u_int16_t bits)
	    : addr_(addr),
	      bits_(bits) {}

        bool operator<(const CIDR& other) const
        {
            #define COMPARE(_x) if (_x != other._x) return _x < other._x;
            COMPARE(addr_);
            #undef COMPARE

	    return bits_ < other.bits_;
        }

	u_int32_t addr_;
	u_int16_t bits_;
    };

    /// Table for IP network address <-> EID matching
    typedef std::map<DTNTunnel::CIDR, char*> NetworkEidTable;
    NetworkEidTable dest_eid_table_;

    void init_tunnel();
    void init_registration();
    void load_dest_eid_table(char* filename);
};

} // namespace dtntunnel

#endif /* _DTNTUNNEL_H_ */
