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

/*
 * Not intended to be run directly, but rather invoked from Tcl script
 * in DTNME/test/bluez-test.tcl using the Tcl testing fromework
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

bdaddr_t remote = {{ 0, 0, 0, 0, 0, 0 }};

class TestRFCOMMPing : public RFCOMMClient, public Thread {
public:
    TestRFCOMMPing(int num, bdaddr_t addr, u_int8_t channel = CHANNEL)
        : Thread("TestRFCOMMPing"),
          num_(num)
    {
        set_remote_addr(addr);
        set_channel(channel);
    }

    void run() 
    {
        log_info("connecting to %s - %d",bd2str(remote_addr_),channel_);
        sleep(1);

        if (connect() == -1) {
            log_err("failed on connect to %s - %d",
                    bd2str(remote_addr_),channel_);
            return;
        }
        log_info("connected to %s - %d",bd2str(remote_addr_),channel_);
        int i = 0;
        while (i++ < num_) {
            snprintf(outpkt_,sizeof(outpkt_), "ping %d", i);
            write(outpkt_,strlen(outpkt_));
            int cc = read(inpkt_, sizeof(inpkt_));
            if (cc <= 0) {
                log_err("failed on read");
                exit(1);
            }
            log_info("got packet '%.*s' of size %d", cc, inpkt_, cc);
            sleep(1);
        }
        num_ -= i - 1; // subtract send attempts
        close();
    }

    int num_;
    char inpkt_[64];
    char outpkt_[64];
};

DECLARE_TEST(RFCOMMClient) {
    log_info_p("/test","Client test starting up");

    // bdaddr_t remote;  // declared global scope, above
    log_info_p("/test","opening active client to  peer %s",bd2str(remote));
    TestRFCOMMPing* p = new TestRFCOMMPing(ITERATIONS,remote);
    p->run();
    CHECK_EQUAL(p->num_,0);
    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(RFCOMMClientTest) {
    ADD_TEST(RFCOMMClient);
}

//DECLARE_TEST_FILE(RFCOMMClientTest, "bluetooth rfcomm socket test");

int main(int argc, const char* argv[]) {

    OptParser p;
    const char* invalid = NULL;

    // whether bluetooth address of server is set
    bool remote_set = false;

    p.addopt(new BdAddrOpt("remote",&remote,"Bluetooth address of server"
                           " (required for server mode)","",&remote_set));

    // no command line parameters were set, this is an error
    if (argc == 1 ) {
        fprintf(stderr,"\n"
                "remote=<bdaddr>: Bluetooth address of server (required)\n\n");
        exit(1);
    }

    // parse through command line parameters
    if (!p.parse(argc-1,argv+1,&invalid)) {
        fprintf(stderr,"bad parameter %s\n",invalid);
        exit(1);
    }

    if (remote_set) {
        RUN_TESTER(RFCOMMClientTest,"bluetooth rfcomm client test",argc-1,argv);
    } else {
        fprintf(stderr,"\n"
                "remote=<bdaddr>: Bluetooth address of server (required)\n\n");
    }
}
