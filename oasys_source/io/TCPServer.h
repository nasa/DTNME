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


#ifndef _OASYS_TCP_SERVER_H_
#define _OASYS_TCP_SERVER_H_

#include "IPSocket.h"
#include "../thread/Thread.h"

namespace oasys {

/**
 * \class TCPServer
 *
 * Wrapper class for a tcp server socket.
 */
class TCPServer : public IPSocket {
public:
    TCPServer(const char* logbase = "/oasys/tcpserver");

    //@{
    /// System call wrapper
    int listen();
    int accept(int *fd, in_addr_t *addr, u_int16_t *port);
    //@}

    /**
     * @brief Try to accept a new connection, but don't block for more
     * than the timeout milliseconds.
     *
     * @return 0 on timeout, 1 on success, -1 on error
     */
    int timeout_accept(int *fd, in_addr_t *addr, u_int16_t *port, 
                       int timeout_ms);
};

/**
 * \class TCPServerThread
 *
 * Simple class that implements a thread of control that loops,
 * blocking on accept(), and issuing the accepted() callback when new
 * connections arrive.
 */
class TCPServerThread : public TCPServer, public Thread {
public:
    /**
     * Constructor initializes but doesn't start the thread.
     */
    TCPServerThread(const char* name,
                    const char* logbase = "/oasys/tcpserver",
                    int         flags = 0);

    /**
     * Destructor stops the thread.
     */
    virtual ~TCPServerThread();

    /**
     * Virtual callback hook that gets called when new connections
     * arrive.
     */
    virtual void accepted(int fd, in_addr_t addr, u_int16_t port) = 0;

    /**
     * Loop forever, issuing blocking calls to TCPServer::accept(),
     * then calling the accepted() function when new connections
     * arrive
     * 
     * Note that unlike in the Thread base class, this run() method is
     * public in case we don't want to actually create a new thread
     * for this guy, but instead just want to run the main loop.
     */
    void run();

    /**
     * Stop the thread by posting on the notifier, which causes it to
     * wake up from poll and then exit.
     */
    void stop();

    /**
     * handler to do subclass-specific shutdown, e.g. free resources. 
     * Called after thread is stopped. 
     */
    virtual void shutdown_hook() { /* nothing */ }

    /**
     * @brief Bind to an address, open the port for listening and
     * start the thread.
     *
     * Most uses of TcpServerThread will simply call these functions
     * in sequence, so this helper function is to merge such
     * redundancy.
     *
     * @return -1 on error, 0 otherwise.
     */
    int bind_listen_start(in_addr_t local_addr, u_int16_t local_port);
};

} // namespace oasys
                        
#endif /* _OASYS_TCP_SERVER_H_ */
