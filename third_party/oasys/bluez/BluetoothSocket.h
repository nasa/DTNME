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

#ifndef _OASYS_BT_SOCKET_H_
#define _OASYS_BT_SOCKET_H_

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/rfcomm.h>

#include "../io/IO.h"
#include "Bluetooth.h"
#include "../debug/Log.h"

#include "../thread/SpinLock.h"

namespace oasys {

#ifndef BDADDR_ANY
#define BDADDR_ANY   (&(bdaddr_t) {{0, 0, 0, 0, 0, 0}})
#endif

/**
 * BluetoothSocket is a base class that wraps around a Bluetooth socket. It 
 * is a base class for RFCOMMClient (possibly others to follow?).
 */ 
class BluetoothSocket : public Logger,
                        virtual public IOHandlerBase {
public:
    /**
      * from <bluetooth/bluetooth.h>:
      * #define BTPROTO_L2CAP   0
      * #define BTPROTO_HCI     1
      * #define BTPROTO_SCO     2
      * #define BTPROTO_RFCOMM  3
      * #define BTPROTO_BNEP    4
      * #define BTPROTO_CMTP    5
      * #define BTPROTO_HIDP    6
      * #define BTPROTO_AVDTP   7
      */
    enum proto_t {
        L2CAP=0,
        HCI,
        SCO,
        RFCOMM,
        BNEP,
        CMTP,
        HIDP,
        AVDTP
    };

    BluetoothSocket(int socktype, proto_t proto, const char* logbase);
    BluetoothSocket(int socktype, proto_t proto, int fd, bdaddr_t remote_addr,
                    u_int8_t channel, const char* logbase);
    virtual ~BluetoothSocket();

    /// Set the socket parameters
    void configure();

    //@{
    /// System call wrappers
    virtual int bind(bdaddr_t local_addr, u_int8_t channel);
    virtual int bind();
    virtual int connect(bdaddr_t remote_addr, u_int8_t channel);
    virtual int connect();
    virtual int close();
    virtual int shutdown(int how);

    virtual int send(const char* bp, size_t len, int flags);
    virtual int recv(char* bp, size_t len, int flags);

    //@}

    /// In case connect() was called on a nonblocking socket and
    /// returned EINPROGRESS, this fn returns the errno result of the
    /// connect attempt. It also sets the socket state appropriately
    int async_connect_result();

    /// Wrapper around poll() for this socket's fd
    virtual int poll_sockfd(int events, int* revents, int timeout_ms);


    /// Socket State values
    enum state_t {
        INIT,           ///< initial state
        LISTENING,      ///< server socket, called listen()
        CONNECTING,     ///< client socket, called connect()
        ESTABLISHED,    ///< connected socket, data can flow
        RDCLOSED,       ///< shutdown(SHUT_RD) called, writing still enabled
        WRCLOSED,       ///< shutdown(SHUT_WR) called, reading still enabled
        CLOSED,         ///< shutdown called for both read and write
        FINI            ///< close() called on the socket
    };

    enum sockaddr_t {
        ZERO,
        LOCAL,
        REMOTE
    };

    // cf <bits/socket.h>
    static const char* socktypetoa(int socktype) {
        switch(socktype) {
            case SOCK_STREAM:    return "SOCK_STREAM";
            case SOCK_DGRAM:     return "SOCK_DGRAM";
            case SOCK_RAW:       return "SOCK_RAW";
            case SOCK_RDM:       return "SOCK_RDM";
            case SOCK_SEQPACKET: return "SOCK_SEQPACKET";
            case SOCK_PACKET:    return "SOCK_PACKET";
            default:             return "UNKNOWN SOCKET TYPE";
        };
    }

    /**
      * Return the current state.
      */
    state_t state() { return state_; }

    /**
     * Socket parameters are public fields that should be set after
     * creating the socket but before the socket is used.
     */
    struct bluetooth_socket_params {
        bluetooth_socket_params() :
            reuseaddr_      (true),
            silent_connect_ (false),
            recv_bufsize_   (0),
            send_bufsize_   (0) {}
        bool reuseaddr_;      // default: on
        bool silent_connect_; // default: off
        int recv_bufsize_;    // default: system setting
        int send_bufsize_;    // default: system setting
    } params_;

    /// The socket file descriptor
    int fd();

    /// The local address that the socket is bound to
    void local_addr(bdaddr_t& addr);

    /// The channel that the socket is bound to
    u_int8_t channel();

    /// The remote address that the socket is bound to
    void remote_addr(bdaddr_t& addr);

    /// Set the local address that the socket is bound to
    void set_local_addr(bdaddr_t& addr);

    /// Set the remote address that the socket is bound to
    void set_remote_addr(bdaddr_t& addr);

    /// Set the channel that the socket is bound to
    void set_channel(u_int8_t channel);

    // logfd can be set to false to disable the appending of the
    /// socket file descriptor
    void set_logfd(bool logfd) { logfd_ = logfd; }

    void init_socket(); 
protected:
    void set_state(state_t state);
    const char* statetoa(state_t state); 
    void set_proto(proto_t proto);
    const char* prototoa(proto_t proto); 
    void get_local();
    void get_remote(); 

    static int abort_on_error_; 
    int fd_;
    int socktype_;
    state_t state_;
    int proto_;
    bool logfd_; 
    bdaddr_t local_addr_;
    bdaddr_t remote_addr_;
    u_int8_t channel_; 
    struct sockaddr_rc* rc_;  /* BTPROTO_RFCOMM */
};

} // namespace oasys

#endif // OASYS_BLUETOOTH_ENABLED
#endif // _OASYS_BT_SOCKET_H_
