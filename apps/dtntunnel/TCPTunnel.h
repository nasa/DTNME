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

#ifndef _TCPTUNNEL_H_
#define _TCPTUNNEL_H_

#include <map>
#include <dtn_api.h>

#include <oasys/debug/Log.h>
#include <oasys/io/TCPClient.h>
#include <oasys/io/TCPServer.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/ExpandableBuffer.h>

#include "IPTunnel.h"

namespace dtntunnel {

/**
 * Class to manage TCP <-> DTN tunnels.
 */
class TCPTunnel : public IPTunnel {
public:
    /// Constructor for the bundle receiving side of the TCPTunnel
    TCPTunnel();
    
    /// Add a new listening to from the given listening
    /// address/port to the given remote address/port
    void add_listener(in_addr_t listen_addr, u_int16_t listen_port,
                      in_addr_t remote_addr, u_int16_t remote_port);
    
    /// Handle a newly arriving bundle
    void handle_bundle(dtn::APIBundle* bundle);

protected:
    /// Helper class to accept incoming TCP connections
    class Listener : public oasys::TCPServerThread {
    public:
        Listener(TCPTunnel* t,
                 in_addr_t listen_addr, u_int16_t listen_port,
                 in_addr_t remote_addr, u_int16_t remote_port);
        
        void accepted(int fd, in_addr_t addr, u_int16_t port);

    protected:
        TCPTunnel* tcptun_;

        /// @{
        /// Proxy parameters
        in_addr_t listen_addr_;
        u_int16_t listen_port_;
        in_addr_t remote_addr_;
        u_int16_t remote_port_;
        /// @}

        /// Proxy application traffic transparently (Linux only)
        bool transparent_;

	/// Force close a TCP socket
	void force_close_socket(int fd);
    };

    /// Helper class to handle an actively proxied connection
    class Connection : public oasys::Formatter,
                       public oasys::Thread,
                       public oasys::Logger
    {
    public:
        /// Constructor called to initiate a connection due to an
        /// arriving bundle request
        Connection(TCPTunnel* t, dtn_endpoint_id_t* dest_eid,
                   in_addr_t client_addr, u_int16_t client_port,
                   in_addr_t remote_addr, u_int16_t remote_port,
                   u_int32_t connection_id);

        /// Constructor called when a new connection was accepted
        Connection(TCPTunnel* t, dtn_endpoint_id_t* dest_eid, int fd,
                   in_addr_t client_addr, u_int16_t client_port,
                   in_addr_t remote_addr, u_int16_t remote_port,
                   u_int32_t connection_id);

        /// Destructor
        ~Connection();

        /// Virtual from Formatter
        int format(char* buf, size_t sz) const;

        /// Handle a newly arriving bundle
        void handle_bundle(dtn::APIBundle* bundle);
        
    protected:
        friend class TCPTunnel;
        
        /// virtual run method
        void run();

        /// The tcp tunnel object
        TCPTunnel* tcptun_;
        
        /// The tcp socket
        oasys::TCPClient sock_;

        /// Queue for bundles on this connection
        dtn::APIBundleQueue queue_;

        /// Table for out-of-order bundles
        typedef std::map<u_int32_t, dtn::APIBundle*> ReorderTable;
        ReorderTable reorder_table_;

        /// Running sequence number counter
        u_int32_t next_seqno_;

        /// Parameters for the connection
        dtn_endpoint_id_t dest_eid_;
        in_addr_t         client_addr_;
        u_int16_t         client_port_;
        in_addr_t         remote_addr_;
        u_int16_t         remote_port_;
        u_int32_t         connection_id_;

        /// Proxy application traffic transparently (Linux only)
        bool transparent_;
    };

    /// Return the next connection id
    u_int32_t next_connection_id();

    /// Hook called by the listener when a new connection comes in
    void new_connection(Connection* c);

    /// Hook called when a new connection dies
    void kill_connection(Connection* c);

    /// Helper struct used as the index key into the connection table
    struct ConnKey {
        ConnKey()
            : endpoint_id_(""),
              client_addr_(INADDR_NONE), client_port_(0),
              remote_addr_(INADDR_NONE), remote_port_(0),
              connection_id_(0) {}

        ConnKey(const dtn_endpoint_id_t& eid,
                in_addr_t client_addr, u_int16_t client_port,
                in_addr_t remote_addr, u_int16_t remote_port,
                u_int32_t connection_id)
            : endpoint_id_(eid.uri),
              client_addr_(client_addr),
              client_port_(client_port),
              remote_addr_(remote_addr),
              remote_port_(remote_port),
              connection_id_(connection_id) {}

        bool operator<(const ConnKey& other) const
        {
            #define COMPARE(_x) if (_x != other._x) return _x < other._x;
            COMPARE(connection_id_);
            COMPARE(client_addr_);
            COMPARE(client_port_);
            COMPARE(remote_addr_);
            COMPARE(remote_port_);
            #undef COMPARE

            return endpoint_id_ < other.endpoint_id_;
        }

        std::string endpoint_id_;
        in_addr_t   client_addr_;
        u_int16_t   client_port_;
        in_addr_t   remote_addr_;
        u_int16_t   remote_port_;
        u_int32_t   connection_id_;
    };

    /// Table of connection classes indexed by the remote address/port
    typedef std::map<ConnKey, Connection*> ConnTable;
    ConnTable connections_;

    /// Table of bundles that arrive out of order before the
    /// connection is set up
    typedef std::map<ConnKey, dtn::APIBundleVector*> NoConnBundleTable;
    NoConnBundleTable no_conn_bundles_;

    /// Lock to protect the connections table
    oasys::SpinLock lock_;

    /// Increasing counter for connection identifiers
    u_int32_t next_connection_id_;
};

} // namespace dtntunnel

#endif /* _TCPTUNNEL_H_ */
