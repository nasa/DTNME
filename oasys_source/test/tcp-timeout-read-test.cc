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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include <debug/DebugUtils.h>
#include <debug/Log.h>
#include <thread/Thread.h>
#include <io/NetUtils.h>
#include <io/TCPClient.h>
#include <io/TCPServer.h>

#define PORT 17600

using namespace oasys;

/*
 * Test for the timeout read call. The reader repeatedly tries to read
 * 64 bytes with a 1 second timeout until it gets an eof. The writer
 * waits for five seconds, then writes, then waits five more seconds,
 * writes again, then quits.
 */

class TestTcpWriter : public TCPClient, public Thread {
 public:
    TestTcpWriter() : Thread("TestTcpWriter")
    {
    }

    void run()
    {
        char buf[64];
            
        while(state() != ESTABLISHED)
            connect(htonl(INADDR_LOOPBACK), PORT);

        sleep(5);
        write(buf, 64);
        sleep(5);
        write(buf, 64);
        close();
    }
};

class TestTcpReader : public TCPClient, public Thread {
public:
    TestTcpReader(int fd, in_addr_t host, u_int16_t port) :
        TCPClient(fd, host, port), Thread("TestTcpReader", DELETE_ON_EXIT | CREATE_JOINABLE)
    {
    }

    void run()
    {
        char buf[64];
        while (1) {
            int cc = timeout_read(buf, 64, 1000);
            if (cc == IOTIMEOUT) {
                log_debug("timeout_read return timeout");
                continue;
            } else if (cc == IOEOF) {
                log_debug("timeout_read return eof");
                close();
                return;
            } else if (cc < 0) {
                ASSERT(0); // unexpected IOERROR
            }
            
            log_debug("timeout_read returned %d", cc);
        }
    }
};

class TestTcpServer : public TCPServerThread {
public:
    TestTcpServer()
        : TCPServerThread("TestTcpServer", "/test-server", CREATE_JOINABLE)
    {
        log_info("starting up");
        bind(htonl(INADDR_LOOPBACK), PORT);
        listen();
        start();
    }

    void accepted(int fd, in_addr_t addr, u_int16_t port)
    {
        TestTcpReader* p = new TestTcpReader(fd, addr, port);
        p->start();
        p->join();
        set_should_stop();
    }
};

int
main(int argc, const char** argv)
{
    Log::init(LOG_DEBUG);

    TestTcpServer* s = new TestTcpServer();
    s->start();

    TestTcpWriter* w = new TestTcpWriter();
    w->start();

    s->join();
}
