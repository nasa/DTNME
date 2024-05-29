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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "SMTPServer.h"

namespace oasys {

//----------------------------------------------------------------------------
SMTPServer::SMTPServer(const SMTP::Config& config,
                       SMTPHandlerFactory* handler_factory,
                       Notifier*           session_done)
    : TCPServerThread("SMTPServer", "/smtp/server", 0),
      config_(config),
      handler_factory_(handler_factory),
      session_done_(session_done)
{
    logpathf("/smtp/server/%s:%d", intoa(config.addr_), config.port_);
    bind_listen_start(config.addr_, config.port_);
}

//----------------------------------------------------------------------------
void
SMTPServer::accepted(int fd, in_addr_t addr, u_int16_t port)
{
    (void)addr;
    (void)port;
    SMTPHandler* handler = handler_factory_->new_handler();
    SMTPHandlerThread* thread =
        new SMTPHandlerThread(handler, fd, fd, config_, session_done_);
    thread->start();
}

//----------------------------------------------------------------------------
SMTPHandlerThread::SMTPHandlerThread(SMTPHandler* handler,
                                     int fd_in, int fd_out,
                                     const SMTP::Config& config,
                                     Notifier* session_done)
    : Thread("/smtp/server", DELETE_ON_EXIT),
      handler_(handler),
      fdio_in_(fd_in), fdio_out_(fd_out),
      in_(&fdio_in_), out_(&fdio_out_),
      smtp_(&in_, &out_, config, "/smtp/server"),
      session_done_(session_done)
{
}

//----------------------------------------------------------------------------
SMTPHandlerThread::~SMTPHandlerThread()
{
    delete handler_;
    handler_ = 0;
}

//----------------------------------------------------------------------------
void
SMTPHandlerThread::run()
{
    smtp_.server_session(handler_);
    if (session_done_) {
        session_done_->notify();
    }
}

} // namespace oasys
