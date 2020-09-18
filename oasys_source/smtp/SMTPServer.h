/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _OASYS_SMTP_SERVER_H_
#define _OASYS_SMTP_SERVER_H_

#include "../io/FdIOClient.h"
#include "../io/TCPServer.h"
#include "SMTP.h"

namespace oasys {

class SMTPServer;
class SMTPHandlerFactory;
class SMTPHandlerThread;

/**
 * Class to implement an SMTP server which creates a thread and an
 * SMTPHandler (using the factory interface) per connection.
 */
class SMTPServer : public oasys::TCPServerThread {
public:
    SMTPServer(const SMTP::Config& config,
               SMTPHandlerFactory* handler_factory,
               Notifier*           session_done = NULL);

private:
    void accepted(int fd, in_addr_t addr, u_int16_t port);
    
    SMTP::Config        config_;
    SMTPHandlerFactory* handler_factory_;
    Notifier*           session_done_;
};

/**
 * Simple interface for a class to create handler instances for
 * incoming SMTP connections that are then destroyed when connections
 * close.
 */
class SMTPHandlerFactory {
public:
    virtual ~SMTPHandlerFactory() {}
    virtual SMTPHandler* new_handler() = 0;
};

/**
 * Class for a single SMTP session.
 */
class SMTPHandlerThread : public Thread {
public:
    SMTPHandlerThread(SMTPHandler* handler,
                      int fd_in, int fd_out,
                      const SMTP::Config& config,
                      Notifier* session_done);

    virtual ~SMTPHandlerThread();
    void run();

private:
    SMTPHandler*   handler_;
    FdIOClient     fdio_in_;
    FdIOClient     fdio_out_;
    BufferedInput  in_;
    BufferedOutput out_;
    SMTP           smtp_;
    Notifier*      session_done_;
};

} // namespace oasys

#endif /* _OASYS_SMTP_SERVER_H_ */
