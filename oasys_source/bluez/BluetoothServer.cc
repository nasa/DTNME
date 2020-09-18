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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include "Bluetooth.h"
#include "BluetoothServer.h"
#include "debug/Log.h"

#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace oasys {

BluetoothServer::BluetoothServer(int socktype,
                                 BluetoothSocket::proto_t proto,
                                 char* logbase)
    : BluetoothSocket(socktype, proto, logbase)
{
}

int
BluetoothServer::listen()
{
    // In Bluetooth, piconets are formed by one Master
    // connecting to up to seven Slaves.  The device
    // performing connect() is the Master ... thus the
    // device performing listen()/accept() is the Slave.
    logf(LOG_DEBUG, "listening");
    ASSERT(fd_ != -1);

    if (::listen(fd_, SOMAXCONN) == -1) {
        logf(LOG_ERR, "error in listen(): %s",strerror(errno));
        return -1;
    }
    
    set_state(LISTENING);
    return 0;
}
    
int
BluetoothServer::accept(int *fd, bdaddr_t *addr, u_int8_t *channel)
{
    ASSERTF(state_ == LISTENING,
            "accept() expected state LISTENING, not %s", statetoa(state_));
    
    struct sockaddr sa;
    socklen_t sl = sizeof(sa);
    memset(&sa,0,sl);

    int ret = ::accept(fd_,&sa,&sl);
    if (ret == -1) {
        logf(LOG_ERR, "error in accept(): %s",strerror(errno));
        return ret;
    }
    
    *fd = ret;

    switch(proto_) {
    case RFCOMM:
        rc_ = (struct sockaddr_rc*) &sa;
        bacpy(addr,&rc_->rc_bdaddr);
        *channel = rc_->rc_channel;
        break;
    default:
        ASSERTF(0,"not implemented for %s",prototoa((proto_t)proto_));
        break;
    }

    monitor(IO::ACCEPT, 0); // XXX/bowei

    return 0;
}

int
BluetoothServer::timeout_accept(int *fd, bdaddr_t *addr, u_int8_t *channel,
                                int timeout_ms)
{
    int ret = poll_sockfd(POLLIN, NULL, timeout_ms);

    if (ret != 1) return ret;
    ASSERT(ret == 1);

    ret = accept(fd, addr, channel);

    if (ret < 0) {
        return IOERROR;
    }

    monitor(IO::ACCEPT, 0); // XXX/bowei

    return 0;
}

void
BluetoothServerThread::run()
{
    int fd;
    bdaddr_t addr;
    u_int8_t channel;

    while (1) {
        // check if someone has told us to quit by setting the
        // should_stop flag. if so, we're all done.
        if (should_stop())
            break;

        // check the accept_timeout parameter to see if we should
        // block or poll when calling accept
        int ret;
        if (accept_timeout_ == -1) {
            ret = accept(&fd, &addr, &channel);
        } else {
            ret = timeout_accept(&fd, &addr, &channel, accept_timeout_);
            if (ret == IOTIMEOUT)
                continue;
        }

        if (ret != 0) {
            if (errno == EINTR || ret == IOINTR) 
                continue;

            logf(LOG_ERR, "error %d in accept(): %d %s",
                 ret, errno, strerror(errno));
            close();

            ASSERT(errno != 0);

            break;
        }
        
        logf(LOG_DEBUG, "accepted connection fd %d from %s(%d)",
             fd, bd2str(addr), channel);

        set_remote_addr(addr);

        accepted(fd, addr, channel);
    }

    log_debug("server thread %p exiting",this);
}

void
BluetoothServerThread::stop()
{
    set_should_stop();

    if (! is_stopped()) {
        interrupt_from_io();

        // wait for 10 seconds (i.e. 20 sleep periods)
        for (int i = 0; i < 20; i++) {
            if (is_stopped())
                return;

            usleep(500000);
        }

        log_err("bluetooth server thread didn't die after 10 seconds");
    }
}

int
BluetoothServerThread::bind_listen_start(bdaddr_t local_addr,
                                  u_int8_t local_channel)
{
    if(bind(local_addr, local_channel) != 0)
        return -1;

    if(listen() != 0)
        return -1;

    start();

    return 0;
}

} // namespace oasys
#endif /* OASYS_BLUETOOTH_ENABLED */
