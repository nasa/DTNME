/*
 *    Copyright 2007 Baylor University
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
#  include <config.h>
#endif

#include <util/UnitTest.h>
#include <util/OptParser.h>

#include <debug/DebugUtils.h>
#include <debug/Log.h>
#include <thread/Thread.h>

#include <bluez/BluetoothInquiry.h>
#include <bluez/RFCOMMClient.h>
#include <bluez/RFCOMMServer.h>

using namespace oasys;

#define CHANNEL 9
#define ITERATIONS 5

class TestRFCOMMPong : public RFCOMMClient, public Thread {
public:
    TestRFCOMMPong(int num, int fd, bdaddr_t addr, u_int8_t channel) :
        RFCOMMClient(fd,addr,channel), 
        Thread("TestRFCOMMPong", DELETE_ON_EXIT),
        num_(num)
    {
        snprintf(outpkt_, sizeof(outpkt_), "pong");
    }

    ~TestRFCOMMPong()
    {
        log_info("pong thread exiting");
    }

    void run()
    {
        while (1) {
            memset(inpkt_, 0, sizeof(inpkt_));
            int cc = read(inpkt_, sizeof(inpkt_));
            if (cc <= 0) { 
                close(); // eof
                return;
            }

            num_--;
            log_info("got packet '%s' of size %d", inpkt_, cc);
            write(outpkt_, strlen(outpkt_));
            if (num_ < 0) {
                log_err("received more packets than expected");
                num_ = -1;
                break;
            }
        }
    }

    int num_;
    char inpkt_[64];
    char outpkt_[64];
};

class TestRFCOMMServer : public RFCOMMServerThread {
public:
    TestRFCOMMServer(int num)
        : RFCOMMServerThread("/test-server",CREATE_JOINABLE),
          num_(num)
    {
        log_info_p("/test","starting up");
        Bluetooth::hci_get_bdaddr(&local_);
        set_notifier(new Notifier("/test/notifier",true));
    }

    void accepted(int fd, bdaddr_t addr, u_int8_t channel)
    {
        TestRFCOMMPong* p = new TestRFCOMMPong(num_,fd,addr,channel);
        p->run();
        num_ = p->num_;
        if (num_ != 0) {
            log_err("didn't receive expected number of packets (%d != 0)",num_);
        }
        set_should_stop();
        interrupt_from_io();
    }

    bdaddr_t local_;
    int num_;
};

DECLARE_TEST(RFCOMMServer) {
    log_info_p("/test","Server test starting up");
    TestRFCOMMServer* s = new TestRFCOMMServer(ITERATIONS);
    bdaddr_t addr;
    s->local_addr(addr);
    s->bind_listen_start(addr,CHANNEL);
    s->join();
    CHECK_EQUAL(s->num_,0);
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(RFCOMMServerTest) {
    ADD_TEST(RFCOMMServer);
}

DECLARE_TEST_FILE(RFCOMMServerTest, "bluetooth rfcomm server test");
