/*
 *    Copyright 2007-2008 Darren Long, darren.long@mac.com
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

#ifndef _OASYS_AX25CONNECTEDMODESERVER_H_
#define _OASYS_AX25CONNECTEDMODESERVER_H_

// If ax25 support found at configure time...
#ifdef OASYS_AX25_ENABLED

// for AX25 support
#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include "AX25Socket.h"

// for oasys integration
#include "../io/IO.h"
#include "../io/IOClient.h"
#include "../compat/inttypes.h"
//#include "../debug/Log.h"


#include "../thread/Thread.h"

namespace oasys {


/*****************************************************************************/
//                  Class AX25ConnectedModeServer Specification
/*****************************************************************************/
/**
 *
 * Wrapper class for a AX25 server socket.
 */
class AX25ConnectedModeServer : public AX25Socket {
public:
    AX25ConnectedModeServer(const char* logbase = "/oasys/AX25server");

    //@{
    /// System call wrapper
    int listen();
    int accept(int *fd, std::string* remote_addr);
    //@}

    /**
     * @brief Try to accept a new connection, but don't block for more
     * than the timeout milliseconds.
     *
     * @return 0 on timeout, 1 on success, -1 on error
     */
    int timeout_accept(int *fd, std::string* remote_addr, 
                       int timeout_ms);
};

/**
 * \class AX25ServerThread
 *
 * Simple class that implements a thread of control that loops,
 * blocking on accept(), and issuing the accepted() callback when new
 * connections arrive.
 */
class AX25ConnectedModeServerThread : public AX25ConnectedModeServer, public Thread {
public:
     /**
     * Constructor initializes but doesn't start the thread.
     */
    AX25ConnectedModeServerThread(  const char* name,
                                    const char* logbase = "/oasys/AX25server",
                                    int         flags = 0);

    /**
     * Destructor stops the thread.
     */
    virtual ~AX25ConnectedModeServerThread();
    
    
    /**
     * Virtual callback hook that gets called when new connections
     * arrive.
     */
    virtual void accepted(int fd, const std::string& addr) = 0;

    /**
     * Loop forever, issuing blocking calls to AX25Server::accept(),
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
     * Most uses of AX25ServerThread will simply call these functions
     * in sequence, so this helper function is to merge such
     * redundancy.
     *
     * @return -1 on error, 0 otherwise.
     */
    int bind_listen_start(std::string& axport, std::string& local_call);

};



} // namespace oasys
#endif /* #ifdef OASYS_AX25_ENABLED  */
#endif /* #ifndef _OASYS_AX25CONNECTEDMODESERVER_H_ */
