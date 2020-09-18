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

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include <debug/DebugUtils.h>
#include <debug/Log.h>
#include <thread/Thread.h>
#include <io/NetUtils.h>
#include <io/TCPClient.h>
#include <io/TCPServer.h>

using namespace oasys;

#define PORT 17600

// The client side socket that initiates the ping
class TestTcpPing : public TCPClient, public Thread {
public:
    TestTcpPing() : Thread("TestTcpPing")
    {
    }
    
protected:
    void run()
    {
        sleep(1);
        
        for (int j = 0; j < 3; ++j) {
            connect(htonl(INADDR_LOOPBACK), PORT);
            for (int i = 0; i < 5; ++i) {
                snprintf(outpkt_, sizeof(outpkt_), "ping %d", i);
                write(outpkt_, strlen(outpkt_));
                int cc = read(inpkt_, sizeof(inpkt_));
                log_info("got packet '%.*s' of size %d", cc, inpkt_, cc);
                sleep(1);
            }
            close();
        }
    }

public:
    char inpkt_[64];
    char outpkt_[64];
};

class TestTcpPong : public TCPClient, public Thread {
public:
    // The server side accept()ed socket that responds
    TestTcpPong(int fd, in_addr_t host, u_int16_t port) :
        TCPClient(fd, host, port), Thread("TestTcpPong", DELETE_ON_EXIT)
    {
        snprintf(outpkt_, sizeof(outpkt_), "pong");
    }

    ~TestTcpPong()
    {
        log_info("pong thread exiting");
    }

    void run()
    {
        while (1) {
            memset(inpkt_, 0, sizeof(inpkt_));
            int cc = read(inpkt_, sizeof(inpkt_));
            if (cc == 0) {
                close(); // eof
                return;
            }
            
            log_info("got packet '%s' of size %d", inpkt_, cc);
            write(outpkt_, strlen(outpkt_));
        }
    }
    
    char inpkt_[64];
    char outpkt_[64];
};

class TestTcpServer : public TCPServerThread {
public:
    TestTcpServer()
        : TCPServerThread("TCPServerThread", "/test-server", CREATE_JOINABLE)
    {
        log_info("starting up");
        bind(htonl(INADDR_LOOPBACK), PORT);
        listen();
        start();
    }

    void accepted(int fd, in_addr_t addr, u_int16_t port)
    {
        TestTcpPong* p = new TestTcpPong(fd, addr, port);
        p->start();
    }
};

int
main(int argc, const char** argv)
{
    in_addr_t addr;

    Log::init(LOG_INFO);

    log_info("/test", "testing gethostbyname");
    if (gethostbyname("10.0.0.1", &addr) != 0) {
        log_err("/test", "error: can't gethostbyname 10.0.0.1");
    }
    if (addr != inet_addr("10.0.0.1")) {
        log_err("/test", "error: gethostbyname 10.0.0.1 got %x, "
                        "not %x", (u_int32_t)addr,
                        (u_int32_t)inet_addr("10.0.0.1"));
    }
    
    if (gethostbyname("localhost", &addr) != 0) {
        log_err("/test", "error: can't gethostbyname localhost");
    }
    
    if (ntohl(addr) != INADDR_LOOPBACK) {
        log_err("/test", "error: gethostbyname(localhost) got %x, "
                        "not %x", (u_int32_t)addr,
                        (u_int32_t)INADDR_LOOPBACK);
    }

    TestTcpServer* s = new TestTcpServer();
    s->start();

    TestTcpPing* p = new TestTcpPing();
    p->start();

    s->join();
}
