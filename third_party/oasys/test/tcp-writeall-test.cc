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
#include "debug/Log.h"
#include <thread/Thread.h>
#include <io/NetUtils.h>

#include <io/TCPClient.h>
#include <io/TCPServer.h>


using namespace oasys;

#define PORT 17600

/*
 * Test for the writeall and writevall functions. Starts up two
 * threads, one sender, one receiver.
 *
 * Linux's loopback networking code is messed up seriously so I don't
 * really understand why this does what I think it does, but it seems
 * to work...
 */

// 64K buffers
int alldone = 0;
char sndbuf[65536];
char rcvbuf[65536];

class TestTcpWriter : public TCPClient, public Thread {
 public:
    TestTcpWriter() : Thread("TestTcpWriter")
    {
        logpathf("/testtcpwriter");

        // force a small send buffer of 4K
        params_.send_bufsize_ = 4097;
    }

    void run()
    {
        while(state() != ESTABLISHED) {
            connect(htonl(INADDR_LOOPBACK), PORT);
        }

        configure();
        sleep(1);

        int* bufp = (int*)sndbuf;
        for (unsigned int i = 0; i < sizeof(sndbuf) / 4; ++i) {
            bufp[i] = i;
        }
        
        writeall(sndbuf, sizeof(sndbuf));

        sleep(2);

        // set up the iovec to make sure that on the first call to
        // write (which sends 16K-1, don't ask), all of the first iov
        // and part of the second gets written, then on the second
        // call, only part of the first one gets written, then test a
        // clean write (i.e. 4K), then just the rest
        struct iovec iov[4];
        iov[0].iov_base = sndbuf;
        iov[0].iov_len  = 4096;
        iov[1].iov_base = &sndbuf[4096];
        iov[1].iov_len  = 32768;
        iov[2].iov_base = &sndbuf[4096 + 32768];
        iov[2].iov_len  = 4096;
        iov[3].iov_base = &sndbuf[4096 + 32768 + 4096];
        iov[3].iov_len  = 24576;

        writevall(iov, 4);

        alldone = 1;
    }
};

class TestTcpReader : public TCPClient, public Thread {
public:
    TestTcpReader(int fd, in_addr_t host, u_int16_t port) :
        TCPClient(fd, host, port), Thread("TestTcpReader", 
                                          DELETE_ON_EXIT | CREATE_JOINABLE)
    {
        logpathf("/testtcpreader");

        params_.recv_bufsize_ = 4097;
    }

    void run()
    {
        configure();
        int done = 0;
        while (1) {
            int cc = read(&rcvbuf[done], 4096);
            if (cc <= 0) {
                close(); // eof
                return;
            }
            log_debug("read %d/%d into %p", cc, 4096, &rcvbuf[done]);

            done += cc;
            log_debug("done %d", done);

            if (done == 65536) {
                done = 0;
                for (int i = 0; i < 65536; ++i) {
                    if (sndbuf[i] != rcvbuf[i]) {
                        log_err("buffer mismatch at byte %d!!!", i);
                        abort();
                    }
                }

                if (alldone) {
                    log_info("all done!");
                    exit(0);
                    NOTREACHED;
                }
            }
            
            usleep(250);
        }
    }
    
    char inpkt_[64];
    char outpkt_[64];
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
    Log::init(LOG_INFO);

    TestTcpServer* s = new TestTcpServer();
    s->start();

    TestTcpWriter* w = new TestTcpWriter();
    w->start();

    s->join();
}
