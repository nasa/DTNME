/*
 *    Copyright 2007 Darren Long.
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

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>


#include "../oasys-config.h"

#include <debug/DebugUtils.h>
#include <debug/Log.h>
#include <thread/Thread.h>
#include <io/NetUtils.h>

#include "../ax25/AX25ConnectedModeClient.h"
#include "../ax25/AX25ConnectedModeServer.h"

#include <iostream>

using namespace oasys;
using namespace std;

#include <sstream>

#define APE_AXPORT "pk232"
#define APE_TEST_CLIENT_CALL "MYCALL" // SSID appended in main, when spawning clients
#define APE_TEST_SERVER_CALL "MYCALL-5"
#define APE_TEST_DIGI_CALL "DIGI-7"

const int connect_count = 3;
const int connect_interval = 10;
const int ping_count = 1000;
const int pong_length = 255;

// The client side socket that initiates the ping
class TestAX25Ping : public AX25ConnectedModeClient, public Thread {
public:
    TestAX25Ping(std::string& local) : Thread("TestAX25Ping"),local_(local)
    {
    }

protected:
    std::string local_;

    void run()
    {
        sleep(1);
        std::string port = APE_AXPORT;
        std::string remote = APE_TEST_SERVER_CALL;
        std::vector<std::string> ss;
        assert(ss.size() == 0);
        ss.push_back(APE_TEST_DIGI_CALL);
        assert(ss.size() == 1);

        memset(outpkt_, 0x24, pong_length);
        for (int j = 0; j < connect_count; ++j) {
            bind(port,local_);
            if(timeout_connect(port,remote, ss,10000)==0){
                for (int i = 0; i < ping_count; ++i) {
                    //snprintf(outpkt_, sizeof(outpkt_), "ping %d", i);
                    log_info("pinging, round %i",i);
                    write(outpkt_, strlen(outpkt_));
                    memset(inpkt_, 0, sizeof(inpkt_));
                    int cc = read(inpkt_, sizeof(inpkt_));
                    if(cc == -1) {
                        log_err("received error in read: %s",strerror(errno));
                    }
                    else {
                        log_info("got packet of size %d:\n '%.*s'", cc, cc, inpkt_);
                    }
                    ASSERT(cc == pong_length || cc == 0);
                    //usleep(100000);
                }
                close();
                usleep(1000000);
            }
            else{
                usleep(4000000);
                j--;
            }
        }
    }

public:
    char inpkt_[pong_length];
    char outpkt_[pong_length];
};

class TestAX25Pong : public AX25ConnectedModeClient, public Thread {public:
    // The server side accept()ed socket that responds
    TestAX25Pong(int fd, std::string addr) :
        AX25ConnectedModeClient(fd, addr), Thread("TestAX25Pong", DELETE_ON_EXIT)
    {
        //snprintf(outpkt_, sizeof(outpkt_), "pong");
        //snprintf(outpkt_, strlen("pong"), "pong");
        memset(outpkt_, 0x23, pong_length);        
    }

    ~TestAX25Pong()
    {
        log_info("pong thread exiting");
    }

    void run()
    {
        while (1) {
            memset(inpkt_, 0, sizeof(inpkt_));
            int cc = read(inpkt_, sizeof(inpkt_));
            if(cc == -1) {
                log_err("received error in read: %s",strerror(errno));
            }
            else {
                log_info("got packet of size %d:\n '%.*s'", cc, cc, inpkt_);
            }
            ASSERT(cc == pong_length || cc == 0);
            if (cc == 0) {
                close(); // eof
                return;
            }
            write(outpkt_, strlen(outpkt_));
        }
    }

    char inpkt_[pong_length];
    char outpkt_[pong_length];
};

class TestAX25Server : public AX25ConnectedModeServerThread {
public:
    TestAX25Server()
        : AX25ConnectedModeServerThread("AX25ServerThread", "/test-server", CREATE_JOINABLE)
    {
        std::string port = APE_AXPORT;
        std::string local = APE_TEST_SERVER_CALL;
        log_info("starting up");
        bind(port,local);
        listen();
        start();
    }

    void accepted(int fd, const std::string& addr)
    {
        TestAX25Pong* p = new TestAX25Pong(fd, addr);
        p->start();
    }
};

int
main(int argc, const char** argv)
{
    //in_addr_t addr;

    std::stringstream blackhole;
    blackhole<<argc<<argv;

    Log::init(LOG_DEBUG);

    TestAX25Server* s = new TestAX25Server();
    s->start();

    sleep(1);

    std::string client1 = APE_TEST_CLIENT_CALL;
    client1.append("-1");
    TestAX25Ping* p = new TestAX25Ping(client1);
    p->start();
/*
    sleep(1);

    std::string client2 = APE_TEST_CLIENT_CALL;
    client2.append("-2");    
    TestAX25Ping* p2 = new TestAX25Ping(client2);
    p2->start();
*/
    printf("sleeping for %i seconds\n",connect_interval);
    sleep(connect_interval);

    s->join();
}
