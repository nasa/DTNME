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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/io/NetUtils.h>
#include <oasys/util/Time.h>
#include "DTNTunnel.h"
#include "TCPTunnel.h"

#include <linux/netfilter_ipv4.h>	// for SO_ORIGINAL_DST
#include <netinet/tcp.h>		// for SOL_TCP

namespace dtntunnel {

//----------------------------------------------------------------------
TCPTunnel::TCPTunnel()
    : IPTunnel("TCPTunnel", "/dtntunnel/tcp"),
      next_connection_id_(0)
{
}

//----------------------------------------------------------------------
void
TCPTunnel::add_listener(in_addr_t listen_addr, u_int16_t listen_port,
                        in_addr_t remote_addr, u_int16_t remote_port)
{
    new Listener(this, listen_addr, listen_port,
                 remote_addr, remote_port);
}

//----------------------------------------------------------------------
u_int32_t
TCPTunnel::next_connection_id()
{
    oasys::ScopeLock l(&lock_, "TCPTunnel::next_connection_id");
    return ++next_connection_id_;
}

//----------------------------------------------------------------------
void
TCPTunnel::new_connection(Connection* c)
{
    oasys::ScopeLock l(&lock_, "TCPTunnel::new_connection");
    
    ConnTable::iterator i;
    ConnKey key(c->dest_eid_,
                c->client_addr_,
                c->client_port_,
                c->remote_addr_,
                c->remote_port_,
                c->connection_id_);
    
    i = connections_.find(key);
    
    if (i != connections_.end()) {
        log_err("got duplicate connection *%p", c);
        return;
    }

    log_debug("added new connection to table *%p", c);
    
    connections_[key] = c;

    ASSERT(connections_.find(key) != connections_.end());
}

//----------------------------------------------------------------------
void
TCPTunnel::kill_connection(Connection* c)
{
    oasys::ScopeLock l(&lock_, "TCPTunnel::kill_connection");
    
    ConnTable::iterator i;
    ConnKey key(c->dest_eid_,
                c->client_addr_,
                c->client_port_,
                c->remote_addr_,
                c->remote_port_,
                c->connection_id_);
    
    i = connections_.find(key);

    if (i == connections_.end()) {
        log_err("can't find connection *%p in table", c);
        return;
    }

    // there's a chance that the connection was replaced by a
    // restarted one, in which case we leave the existing one in the
    // table and don't want to blow it away
    if (i->second == c) {
	log_debug("erasing connection from table *%p", c);
        connections_.erase(i);
    } else {
        log_notice("not erasing connection in table since already replaced");
    }

}

//----------------------------------------------------------------------
void
TCPTunnel::handle_bundle(dtn::APIBundle* bundle)
{
    oasys::ScopeLock l(&lock_, "TCPTunnel::handle_bundle");

    DTNTunnel::BundleHeader hdr;
    memcpy(&hdr, bundle->payload_.buf(), sizeof(hdr));
    hdr.connection_id_ = ntohl(hdr.connection_id_);
    hdr.seqno_ = ntohl(hdr.seqno_);
    hdr.client_port_ = ntohs(hdr.client_port_);
    hdr.remote_port_ = ntohs(hdr.remote_port_);

    log_debug("handle_bundle got %zu byte bundle from %s for %s:%d -> %s:%d (id %u seqno %u)",
              bundle->payload_.len(),
              bundle->spec_.source.uri,
              intoa(hdr.client_addr_),
              hdr.client_port_,
              intoa(hdr.remote_addr_),
              hdr.remote_port_,
              hdr.connection_id_,
              hdr.seqno_);
    
    Connection* conn = NULL;
    ConnTable::iterator i;
    ConnKey key(bundle->spec_.source,
                hdr.client_addr_,
                hdr.client_port_,
                hdr.remote_addr_,
                hdr.remote_port_,
                hdr.connection_id_);
    
    i = connections_.find(key);
    
    if (i == connections_.end()) {
        if (hdr.seqno_ == 0) {
            conn = new Connection(this, &bundle->spec_.source,
                                  hdr.client_addr_, hdr.client_port_,
                                  hdr.remote_addr_, hdr.remote_port_,
                                  hdr.connection_id_);

            log_info("new connection *%p", conn);
            conn->start();
            connections_[key] = conn;

        } else {
            // seqno != 0
            log_warn("got bundle with seqno %u but no connection... "
                     "postponing delivery",
                     hdr.seqno_);

            dtn::APIBundleVector* vec;
            NoConnBundleTable::iterator j = no_conn_bundles_.find(key);
            if (j == no_conn_bundles_.end()) {
                vec = new dtn::APIBundleVector();
                no_conn_bundles_[key] = vec;
            } else {
                vec = j->second;
            }
            vec->push_back(bundle);
            return;
        }
        
    } else {
        conn = i->second;
    }

    ASSERT(conn != NULL);
    conn->handle_bundle(bundle);

    NoConnBundleTable::iterator j = no_conn_bundles_.find(key);
    if (j != no_conn_bundles_.end()) {
        dtn::APIBundleVector* vec = j->second;
        no_conn_bundles_.erase(j);
        for (dtn::APIBundleVector::iterator k = vec->begin(); k != vec->end(); ++k) {
            log_debug("conn *%p handling postponed bundle", conn);
            conn->handle_bundle(*k);
        }
        delete vec;
    }
}

//----------------------------------------------------------------------
TCPTunnel::Listener::Listener(TCPTunnel* t,
                              in_addr_t listen_addr, u_int16_t listen_port,
                              in_addr_t remote_addr, u_int16_t remote_port)
    : TCPServerThread("TCPTunnel::Listener",
                      "/dtntunnel/tcp/listener",
                      Thread::DELETE_ON_EXIT),
      tcptun_(t),
      listen_addr_(listen_addr),
      listen_port_(listen_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port)
{
    transparent_ = DTNTunnel::instance()->transparent();
    
    // set IP_TRANSPARENT socket option in order to be able to intercept traffic
    // with the help of TProxy
    if (transparent_) {
#if defined(__linux__) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	this->init_socket();
        int on, err;
        on = 1;
        err = setsockopt(this->fd(), SOL_IP, IP_TRANSPARENT, &on, sizeof(on));
    	if (err) {
    	    log_err("setsockopt(IP_TRANSPARENT) error: %d, %s", err, strerror(errno));
    	} else {
	    log_debug("set IP_TRANSPARENT on listener socket");
	}
#endif
    }

    if (bind_listen_start(listen_addr, listen_port) != 0) {
        log_err("can't initialize listener socket, bailing");
        exit(1);
    }
}

//----------------------------------------------------------------------
void
TCPTunnel::Listener::force_close_socket(int fd)
{
    struct linger l;
    int off = 0;
    l.l_onoff = 1;
    l.l_linger = 0;
    int err;

    err = setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
    if (err) {
    	log_err("setsockopt(SO_LINGER) error: %d, %s", err, strerror(errno));
    }
    err = setsockopt(fd, SOL_TCP, TCP_NODELAY, &off, sizeof(off));
    if (err) {
    	log_err("setsockopt(TCP_NODELAY) error: %d, %s", err, strerror(errno));
    }
    ::write(fd, "", 1);
    ::close(fd);
}

//----------------------------------------------------------------------
void
TCPTunnel::Listener::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    // store original destination address and port
    if (transparent_) {
#if defined(__linux__) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        int rc;
        socklen_t socksize;
	struct sockaddr_in sock;
    
	// prepare socket for getting original destination address and port
        socksize = sizeof(sock);
        rc = getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, &sock, &socksize);

        if (rc != 0) {
    	    log_err("getting original destination failed, getsockopt(SO_ORIGINAL_DST) error: %s", 
		    strerror(errno));
        }
        else 
        if ((sock.sin_addr.s_addr != remote_addr_) || 
    	    (ntohs(sock.sin_port) != remote_port_)) {
	    // set remote address and port in TCPTunnel to their original values
	    remote_addr_ = sock.sin_addr.s_addr;
            remote_port_ = ntohs(sock.sin_port);
	    log_debug("connection redirected, original destination: %s:%u", 
			intoa(remote_addr_), remote_port_);
	}
#endif
    }

    dtn_endpoint_id_t dest_eid;
    dtn_copy_eid(&dest_eid, DTNTunnel::instance()->dest_eid());
    DTNTunnel::instance()->get_dest_eid(remote_addr_, &dest_eid);
    if (dest_eid.uri[0] == '\0') {
	log_warn("cannot find destination eid for address %s, ignoring connection", 
		 intoa(remote_addr_));
	// Notify sending application that the connection can not be established
	force_close_socket(fd);
	return;
    }

    Connection* c = new Connection(tcptun_, &dest_eid,
                                   fd, addr, port, remote_addr_, remote_port_,
                                   tcptun_->next_connection_id());
    tcptun_->new_connection(c);
    c->start();
}

//----------------------------------------------------------------------
TCPTunnel::Connection::Connection(TCPTunnel* t, dtn_endpoint_id_t* dest_eid,
                                  in_addr_t client_addr, u_int16_t client_port,
                                  in_addr_t remote_addr, u_int16_t remote_port,
                                  u_int32_t connection_id)
    : Thread("TCPTunnel::Connection", Thread::DELETE_ON_EXIT),
      Logger("TCPTunnel::Connection", "/dtntunnel/tcp/conn"),
      tcptun_(t),
      sock_("/dtntunnel/tcp/conn/sock"),
      queue_("/dtntunnel/tcp/conn"),
      next_seqno_(0),
      client_addr_(client_addr),
      client_port_(client_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port),
      connection_id_(connection_id)
{
    dtn_copy_eid(&dest_eid_, dest_eid);
}

//----------------------------------------------------------------------
TCPTunnel::Connection::Connection(TCPTunnel* t, dtn_endpoint_id_t* dest_eid,
                                  int fd,
                                  in_addr_t client_addr, u_int16_t client_port,
                                  in_addr_t remote_addr, u_int16_t remote_port,
                                  u_int32_t connection_id)
    : Thread("TCPTunnel::Connection", Thread::DELETE_ON_EXIT),
      Logger("TCPTunnel::Connection", "/dtntunnel/tcp/conn"),
      tcptun_(t),
      sock_(fd, client_addr, client_port, "/dtntunnel/tcp/conn/sock"),
      queue_("/dtntunnel/tcp/conn"),
      next_seqno_(0),
      client_addr_(client_addr),
      client_port_(client_port),
      remote_addr_(remote_addr),
      remote_port_(remote_port),
      connection_id_(connection_id)
{
    dtn_copy_eid(&dest_eid_, dest_eid);
}

//----------------------------------------------------------------------
TCPTunnel::Connection::~Connection()
{
    dtn::APIBundle* b;
    while(queue_.try_pop(&b)) {
        delete b;
    }
}

//----------------------------------------------------------------------
int
TCPTunnel::Connection::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz, "[%s %s:%d -> %s:%d (id %u)]",
                    dest_eid_.uri,
                    intoa(client_addr_),
                    client_port_,
                    intoa(remote_addr_),
                    remote_port_,
                    connection_id_);
}

//----------------------------------------------------------------------
void
TCPTunnel::Connection::run()
{
    DTNTunnel* tunnel = DTNTunnel::instance();
    u_int32_t send_seqno = 0;
    u_int32_t next_recv_seqno = 0;
    u_int32_t total_sent = 0;
    bool sock_eof = false;
    bool dtn_blocked = false;
    bool first = true;
    
    transparent_ = tunnel->transparent();

    // outgoing (tcp -> dtn) / incoming (dtn -> tcp) bundles
    dtn::APIBundle* b_xmit = NULL;
    dtn::APIBundle* b_recv = NULL;

    // time values to implement nagle
    oasys::Time tbegin, tnow;
    ASSERT(tbegin.sec_ == 0);
    
    // header for outgoing bundles
    DTNTunnel::BundleHeader hdr;
    hdr.eof_           = 0;
    hdr.protocol_      = IPPROTO_TCP;
    hdr.connection_id_ = htonl(connection_id_);
    hdr.seqno_         = 0;
    hdr.client_addr_   = client_addr_;
    hdr.client_port_   = htons(client_port_);
    hdr.remote_addr_   = remote_addr_;
    hdr.remote_port_   = htons(remote_port_);
    
    if (sock_.state() != oasys::IPSocket::ESTABLISHED) {
	// bind socket to non-local address of the sending tcp application 
	// in order to transmit data to the receiving tcp application transparrently
	if (transparent_) {
#if defined(__linux__) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	    sock_.init_socket();
    	    int on, err;
            on = 1;
	    err = setsockopt(sock_.fd(), SOL_IP, IP_TRANSPARENT, &on, sizeof(on));
    	    if (err) {
        	log_err("setsockopt(IP_TRANSPARENT) error: %d, %s", err, strerror(errno));
    	    } else {
		log_debug("set IP_TRANSPARENT on connection socket");
	    }
            err = sock_.bind(client_addr_, client_port_);
    	    if (err != 0) {
    	        log_err("can't bind to %s:%u, error: %d (%s)", intoa(client_addr_), client_port_, 
    	    		err, strerror(errno));
	        goto done;
    	    }
#endif
        }
        
        int err = sock_.connect(remote_addr_, remote_port_);
        if (err != 0) {
            log_err("error connecting to %s:%d",
                    intoa(remote_addr_), remote_port_);

            // send an empty bundle back
            dtn::APIBundle* b = new dtn::APIBundle();
            hdr.eof_ = 1;
            memcpy(b->payload_.buf(sizeof(hdr)), &hdr, sizeof(hdr));
            b->payload_.set_len(sizeof(hdr));
            int err;
            if ((err = tunnel->send_bundle(b, &dest_eid_)) != DTN_SUCCESS) {
                log_err("error sending connect reply bundle: %s",
                        dtn_strerror(err));
                tcptun_->kill_connection(this);
                exit(1);
            }
            delete b;
            goto done;
        }
    }

    while (1) {
        struct pollfd pollfds[2];

        struct pollfd* msg_poll  = &pollfds[0];
        msg_poll->fd             = queue_.read_fd();
        msg_poll->events         = POLLIN;
        msg_poll->revents        = 0;

        struct pollfd* sock_poll = &pollfds[1];
        sock_poll->fd            = sock_.fd();
        sock_poll->events        = POLLIN | POLLERR;
        sock_poll->revents       = 0;

        // if the socket already had an eof or if dtn is write
        // blocked, we just poll for activity on the message queue to
        // look for data that needs to be returned out the TCP socket
        int nfds = (sock_eof || dtn_blocked) ? 1 : 2;

        int timeout = -1;
        if (first || dtn_blocked) {
            timeout = 1000; // one second between retries
        } else if (tbegin.sec_ != 0) {
            timeout = tunnel->delay();
        }
        
        log_debug("blocking in poll... (timeout %d)", timeout);
        int nready = oasys::IO::poll_multiple(pollfds, nfds, timeout,
                                              NULL, logpath_);
        if (nready == oasys::IOERROR) {
            log_err("unexpected error in poll: %s", strerror(errno));
            goto done;
        }

        // check if we need to create a new bundle, either because
        // this is the first time through and we'll need to send an
        // initial bundle to create the connection on the remote side,
        // or because there's data on the socket.
        if ((first || sock_poll->revents != 0) && (b_xmit == NULL)) {
            first = false;
            b_xmit = new dtn::APIBundle();
            b_xmit->payload_.reserve(tunnel->max_size());
            hdr.seqno_ = ntohl(send_seqno++);
            memcpy(b_xmit->payload_.buf(), &hdr, sizeof(hdr));
            b_xmit->payload_.set_len(sizeof(hdr));
        }

        // now we check if there really is data on the socket
        if (sock_poll->revents != 0) {
            u_int payload_todo = tunnel->max_size() - b_xmit->payload_.len();

            if (payload_todo != 0) {
                tbegin.get_time();
                
                char* bp = b_xmit->payload_.end();
                int ret = sock_.read(bp, payload_todo);
                if (ret < 0) {
                    log_err("error reading from socket: %s", strerror(errno));
                    delete b_xmit;
                    goto done;
                }
                
                b_xmit->payload_.set_len(b_xmit->payload_.len() + ret);
                
                if (ret == 0) {
                    DTNTunnel::BundleHeader* hdrp =
                        (DTNTunnel::BundleHeader*)b_xmit->payload_.buf();
                    hdrp->eof_ = 1;
                    sock_eof = true;
                }
            }
        }

        // now check if we should send the outgoing bundle
        tnow.get_time();
        if ((b_xmit != NULL) &&
            ((sock_eof == true) ||
             (b_xmit->payload_.len() == tunnel->max_size()) ||
             ((tnow - tbegin).in_milliseconds() >= tunnel->delay())))
        {
            size_t len = b_xmit->payload_.len();
            int err = tunnel->send_bundle(b_xmit, &dest_eid_);
            if (err == DTN_SUCCESS) {
                total_sent += len;
                log_debug("sent %zu byte payload #%u to dtn (%u total)",
                          len, send_seqno, total_sent);
                delete b_xmit;
                b_xmit = NULL;
                tbegin.sec_ = 0;
                tbegin.usec_ = 0;
                dtn_blocked = false;
                
            } else if (err == DTN_ENOSPACE) {
                log_debug("no space for %zu byte payload... "
                          "setting dtn_blocked", len);
                dtn_blocked = true;
                continue;
            } else {
                log_err("error sending bundle: %s", dtn_strerror(err));
                exit(1);
            }
        }
        
        // now check for activity on the incoming bundle queue
        if (msg_poll->revents != 0) {
            b_recv = queue_.pop_blocking();

            // if a NULL bundle is put on the queue, then the main
            // thread is signalling that we should abort the
            // connection
            if (b_recv == NULL)
            {
                log_debug("got signal to abort connection");
                goto done;
            }

            DTNTunnel::BundleHeader* recv_hdr =
                (DTNTunnel::BundleHeader*)b_recv->payload_.buf();

            u_int32_t recv_seqno = ntohl(recv_hdr->seqno_);

            // check the seqno match -- reordering should have been
            // handled before the bundle was put on the blocking
            // message queue
            if (recv_seqno != next_recv_seqno) {
                log_err("got out of order bundle: seqno %d, expected %d",
                        recv_seqno, next_recv_seqno);
                delete b_recv;
                goto done;
            }
            ++next_recv_seqno;

            u_int len = b_recv->payload_.len() - sizeof(hdr);

	    // send data to the receiving application
            if (len != 0) {
                int cc = sock_.writeall(b_recv->payload_.buf() + sizeof(hdr), len);
                if (cc != (int)len) {
                    log_err("error writing payload to socket: %s", strerror(errno));
                    delete b_recv;
                    goto done;
                }

                log_debug("sent %d byte payload to client", len);
            }

            if (recv_hdr->eof_) {
                log_info("bundle had eof bit set... closing connection");

                if ( !sock_eof ) {
                    sock_eof = true;

                    dtn::APIBundle* b = new dtn::APIBundle();
                    hdr.eof_ = 1;
                    hdr.seqno_ = ntohl(send_seqno++);
                    memcpy(b->payload_.buf(sizeof(hdr)), &hdr, sizeof(hdr));
                    b->payload_.set_len(sizeof(hdr));
                    int err;
                    if ((err = tunnel->send_bundle(b, &dest_eid_)) != DTN_SUCCESS) {
                        log_err("error sending final socket closed packet bundle: %s",
                                dtn_strerror(err));
                        tcptun_->kill_connection(this);
                        exit(1);
                    }
                    delete b;
                }
                sock_.close();
                goto done;
            }
            
            delete b_recv;
        }
    }

 done:
    tcptun_->kill_connection(this);
}

//----------------------------------------------------------------------
void
TCPTunnel::Connection::handle_bundle(dtn::APIBundle* bundle)
{
    DTNTunnel::BundleHeader* hdr =
        (DTNTunnel::BundleHeader*)bundle->payload_.buf();
    
    u_int32_t recv_seqno = ntohl(hdr->seqno_);

    // if the seqno is in the past, then it's a duplicate delivery so
    // just ignore it
    if (recv_seqno < next_seqno_)
    {
        log_warn("got seqno %u, but already delivered up to %u: "
                 "ignoring bundle",
                 recv_seqno, next_seqno_);
        delete bundle;
        return;
    }
    
    // otherwise, if it's not the next one expected, put it on the
    // queue and wait for the one that's missing
    else if (recv_seqno != next_seqno_)
    {
        log_debug("got out of order bundle: expected seqno %d, got %d",
                  next_seqno_, recv_seqno);
        
        reorder_table_[recv_seqno] = bundle;
        return;
    }

    // deliver the one that just arrived
    log_debug("delivering %zu byte bundle with seqno %d",
              bundle->payload_.len(), recv_seqno);
    queue_.push_back(bundle);
    next_seqno_++;
    
    // once we get one that's in order, that might let us transfer
    // more bundles out of the reorder table and into the queue
    ReorderTable::iterator iter;
    while (1) {
        iter = reorder_table_.find(next_seqno_);
        if (iter == reorder_table_.end()) {
            break;
        }

        bundle = iter->second;
        log_debug("delivering %zu byte bundle with seqno %d (from reorder table)",
                  bundle->payload_.len(), next_seqno_);
        
        reorder_table_.erase(iter);
        next_seqno_++;
        
        queue_.push_back(bundle);
    }
}

} // namespace dtntunnel

