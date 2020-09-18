/*
 *    Copyright 2012 Raytheon BBN Technologies
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
 *
 * ipnd-sb-tlv-test.cc
 *
 * Unit tests for IPND-SD-TLV encoding/decoding.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/util/UnitTest.h>
#include <oasys/util/HexDumpBuffer.h>
#include "discovery/IPNDService.h"
#include "discovery/ipnd_sd_tlv/Primitives.h"
#include "discovery/ipnd_srvc/IPClaService.h"
#include "discovery/ipnd_srvc/IPNDServiceFactory.h"

#include <vector>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace oasys;
using namespace dtn;

namespace dtn {
    /*
     * Define a custom service impl for testing
     *
     * 128 FooService {
     *   Key (string),
     *   Timeout (fixed16),
     *   Extra (BarType)
     * }
     *
     * 129 BarType {
     *   Age (uint64),
     *   Hash (bytes)
     * }
     *
     */

    class BarType: public ipndtlv::ConstructedType {
    public:
        BarType() : ipndtlv::ConstructedType(129, "bartype") {
            addChild(new ipndtlv::Primitives::UInt64());
            addChild(new ipndtlv::Primitives::Bytes());
        }
        BarType(uint64_t num) : ipndtlv::ConstructedType(129, "bartype") {
            addChild(new ipndtlv::Primitives::UInt64(num));
            addChild(new ipndtlv::Primitives::Bytes());
        }
        virtual ~BarType() {}
    };

    class FooService: public dtn::IPNDService {
    public:
        FooService() : dtn::IPNDService(128, "fooservice") {
            addChild(new ipndtlv::Primitives::String());
            addChild(new ipndtlv::Primitives::Fixed16());
            addChild(new BarType());
        }
        FooService(std::string key) : dtn::IPNDService(128, "fooservice") {
            addChild(new ipndtlv::Primitives::String(key));
            addChild(new ipndtlv::Primitives::Fixed16(0xc001));
            addChild(new BarType(255));
        }
        virtual ~FooService() {}

        std::string getKey() const {
            ipndtlv::Primitives::String *key =
                    static_cast<ipndtlv::Primitives::String*>(
                            getChild(ipndtlv::IPNDSDTLV::P_STRING));

            return key->getValue();
        }
    };

    class FooPlugin: public dtn::IPNDServiceFactory::Plugin {
    public:
        FooPlugin() : dtn::IPNDServiceFactory::Plugin("foo") {}
        virtual ~FooPlugin() {}

        IPNDService *configureService(const std::string &type, const int argc,
                const char *argv[]) const {
            // check service type for one we recognize
            if(type == "foo-svc") {
                // extract other params from argv

                // in reality, this would call a constructor with the params
                // extracted from argv
                return new FooService("fookey");
            } else {
                return NULL;
            }
        }

        IPNDService *readService(const uint8_t tag, const u_char **buf,
                const unsigned int len_remain, int *num_read) const {
            // check the tag for one we recognize
            if(tag == 128) {
                FooService *fs = new FooService();
                *num_read = fs->read(buf, len_remain);

                // in reality you'd want to check *num_read for errors first
                return fs;
            } else {
                *num_read = 0;
                return NULL;
            }
        }
    };
}

/**
 * Clears the given buffer (i.e. writes zeros)
 */
void clearBuf(u_char *buf, unsigned int len) {
    for(unsigned int i = 0; i < len; i++) {
        buf[i] = '\0';
    }
}

/**
 * Tests generic write/read of datatypes
 */
int writeReadTest(ipndtlv::IPNDSDTLV::BaseDatatype &t1,
                  ipndtlv::IPNDSDTLV::BaseDatatype &t2,
                  const unsigned int expectedLen,
                  u_char *buf,
                  const unsigned int bufLen) {
    int rwLen = -1;
    int errno_; const char* strerror_;

    // write to buffer
    rwLen = t1.write((const u_char**)&buf, bufLen);
    log_always_p("ipnd-tlv/test", "t1 wrote %i bytes", rwLen);
    CHECK_EQUAL(rwLen, expectedLen);

    // dump the buffer to string
    oasys::HexDumpBuffer dump(rwLen);
    dump.append(buf, rwLen);
    log_always_p("ipnd-tlv/test", "Raw TLV Buffer:\n%s", dump.hexify().c_str());

    // read from buffer
    rwLen = t2.read((const u_char**)&buf, bufLen);
    log_always_p("ipnd-tlv/test", "t2 read %i bytes", rwLen);
    CHECK_EQUAL(rwLen, expectedLen);

    return UNIT_TEST_PASSED;
}

/**
 * Checks encode/decode for all primitives
 */
DECLARE_TEST(PrimEncDec) {
    unsigned int bLen = 16;
    u_char tlvBuf[bLen];
    u_char *ptr = tlvBuf;

    clearBuf(tlvBuf, bLen);

    /*** Test Boolean *********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::Boolean @@@@@@");
    ipndtlv::Primitives::Boolean b1(true);
    ipndtlv::Primitives::Boolean b2;
    CHECK(writeReadTest(b1, b2, 2, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK(b2.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test UInt64 **********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::UInt64 @@@@@@");
    ipndtlv::Primitives::UInt64 u1(200);
    ipndtlv::Primitives::UInt64 u2;
    CHECK(writeReadTest(u1, u2, 3, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK_EQUAL_U64(u1.getValue(), u2.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test SInt64 **********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::SInt64 @@@@@@");
    ipndtlv::Primitives::SInt64 s1(-200);
    ipndtlv::Primitives::SInt64 s2;
    CHECK(writeReadTest(s1, s2, 11, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK_EQUAL((int)s1.getValue(), (int)s2.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test Fixed16 *********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::Fixed16 @@@@@@");
    ipndtlv::Primitives::Fixed16 f1(0xbeef);
    ipndtlv::Primitives::Fixed16 f2;
    CHECK(writeReadTest(f1, f2, 3, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK_EQUAL((int)f1.getValue(), (int)f2.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test Fixed32 *********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::Fixed32 @@@@@@");
    ipndtlv::Primitives::Fixed32 f3(0xdeadbeef);
    ipndtlv::Primitives::Fixed32 f4;
    CHECK(writeReadTest(f3, f4, 5, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK_EQUAL((int)f3.getValue(), (int)f4.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test Fixed64 *********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::Fixed64 @@@@@@");
    ipndtlv::Primitives::Fixed64 f5(0xdeadbeefdeadbeef);
    ipndtlv::Primitives::Fixed64 f6;
    CHECK(writeReadTest(f5, f6, 9, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK(f5.getValue() == f6.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test Float ***********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::Float @@@@@@");
    ipndtlv::Primitives::Float fl1((float)10.0/3);
    ipndtlv::Primitives::Float fl2;
    CHECK(writeReadTest(fl1, fl2, 5, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK(fl1.getValue() == fl2.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test Double **********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::Double @@@@@@");
    ipndtlv::Primitives::Double dl1((double)10.0/3);
    ipndtlv::Primitives::Double dl2;
    CHECK(writeReadTest(dl1, dl2, 9, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK(dl1.getValue() == dl2.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test String **********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::String @@@@@@");
    ipndtlv::Primitives::String str1("foobar");
    ipndtlv::Primitives::String str2;
    CHECK(writeReadTest(str1, str2, 8, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK(str1.getValue() == str2.getValue());
    clearBuf(tlvBuf, bLen);

    /*** Test Bytes ***********************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Primitives::Bytes @@@@@@");
    u_char arr[4] = { 0xde, 0xad, 0xbe, 0xef };
    ipndtlv::Primitives::Bytes by1(arr, 4);
    ipndtlv::Primitives::Bytes by2;
    CHECK(writeReadTest(by1, by2, 6, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK_EQUAL((int)by1.getValueLen(), (int)by2.getValueLen());
    for(unsigned int i = 0; i < by1.getValueLen(); i++) {
        CHECK(by1.getValue()[i] == by2.getValue()[i]);
    }
    clearBuf(tlvBuf, bLen);

    return UNIT_TEST_PASSED;
}

/**
 * Checks encode/decode for constructed types
 */
DECLARE_TEST(ConstrEncDec) {
    unsigned int bLen = 32;
    u_char tlvBuf[bLen];
    u_char *ptr = tlvBuf;

    clearBuf(tlvBuf, bLen);

    in_addr addr4;
    CHECK_SYS(inet_aton("128.64.128.64", &addr4) != 0);

    in6_addr addr6;
    CHECK_SYS(inet_pton(AF_INET6, "beef::dead:1", &addr6) == 1);

    /*** Test cla-tcp-v4 ******************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Constructed: TcpV4ClaService @@@@@@");
    TcpV4ClaService t4Cla1(addr4.s_addr, 0xaa04);
    TcpV4ClaService t4Cla2;
    CHECK(writeReadTest(t4Cla1, t4Cla2, 10, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK_EQUAL((int)t4Cla1.getIpAddr(), (int)t4Cla2.getIpAddr());
    CHECK_EQUAL((int)t4Cla1.getPort(), (int)t4Cla2.getPort());
    CHECK_EQUAL((int)t4Cla2.getIpAddr(), (int)addr4.s_addr);
    CHECK_EQUAL((int)t4Cla2.getPort(), 0xaa04);
    clearBuf(tlvBuf, bLen);

    /*** Test cla-udp-v4 ******************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Constructed: UdpV4ClaService @@@@@@");
    UdpV4ClaService u4Cla1(addr4.s_addr, 0xbb04);
    UdpV4ClaService u4Cla2;
    CHECK(writeReadTest(u4Cla1, u4Cla2, 10, ptr, bLen) == UNIT_TEST_PASSED);
    CHECK_EQUAL((int)u4Cla1.getIpAddr(), (int)u4Cla2.getIpAddr());
    CHECK_EQUAL((int)u4Cla1.getPort(), (int)u4Cla2.getPort());
    CHECK_EQUAL((int)u4Cla2.getIpAddr(), (int)addr4.s_addr);
    CHECK_EQUAL((int)u4Cla2.getPort(), 0xbb04);
    clearBuf(tlvBuf, bLen);

    /*** Test cla-tcp-v6 ******************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Constructed: TcpV6ClaService @@@@@@");
    TcpV6ClaService t6Cla1(addr6, 0xaa06);
    TcpV6ClaService t6Cla2;
    CHECK(writeReadTest(t6Cla1, t6Cla2, 23, ptr, bLen) == UNIT_TEST_PASSED);
    for(unsigned int i = 0; i < 16; i++) {
        log_always_p("ipnd-tlv/test", "Testing IPv6 addr[%u] -> "
                "{0x%02x == 0x%02x}?", i,
                t6Cla1.getIpAddr().s6_addr[i], t6Cla2.getIpAddr().s6_addr[i]);
        CHECK(t6Cla1.getIpAddr().s6_addr[i] == t6Cla2.getIpAddr().s6_addr[i]);
    }
    CHECK_EQUAL((int)t6Cla1.getPort(), (int)t6Cla2.getPort());
    clearBuf(tlvBuf, bLen);

    /*** Test cla-udp-v6 ******************************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Constructed: UdpV6ClaService @@@@@@");
    UdpV6ClaService u6Cla1(addr6, 0xbb06);
    UdpV6ClaService u6Cla2;
    CHECK(writeReadTest(u6Cla1, u6Cla2, 23, ptr, bLen) == UNIT_TEST_PASSED);
    for(unsigned int i = 0; i < 16; i++) {
        log_always_p("ipnd-tlv/test", "Testing IPv6 addr[%u] -> "
                "{0x%02x == 0x%02x}?", i,
                u6Cla1.getIpAddr().s6_addr[i], u6Cla2.getIpAddr().s6_addr[i]);
        CHECK(u6Cla1.getIpAddr().s6_addr[i] == u6Cla2.getIpAddr().s6_addr[i]);
    }
    CHECK_EQUAL((int)u6Cla1.getPort(), (int)u6Cla2.getPort());
    clearBuf(tlvBuf, bLen);

    return UNIT_TEST_PASSED;
}

/**
 * Checks encode/decode for buffer with multiple types
 */
DECLARE_TEST(MultiEncDec) {
    unsigned int bLen = 64;
    u_char tlvBuf[bLen];
    u_char *ptr = tlvBuf;
    int len = -1;
    unsigned int len_rem = bLen, tot_len = 0;

    clearBuf(tlvBuf, bLen);

    in_addr addr4;
    CHECK_SYS(inet_aton("128.64.128.64", &addr4) != 0);

    in6_addr addr6;
    CHECK_SYS(inet_pton(AF_INET6, "beef::dead:1", &addr6) == 1);

    /*** Test writing multiple structures *************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Multiple Structures: Write @@@@@@");
    TcpV4ClaService t4(addr4.s_addr, 0xaa01);
    TcpV6ClaService t6(addr6, 0xaa01);
    UdpV4ClaService u4(addr4.s_addr, 0xaa02);

    len = t4.write((const u_char**)&ptr, len_rem);
    ptr += len; len_rem -= len; tot_len += len;
    len = t6.write((const u_char**)&ptr, len_rem);
    ptr += len; len_rem -= len; tot_len += len;
    len = u4.write((const u_char**)&ptr, len_rem);
    ptr += len; len_rem -= len; tot_len += len;

    // dump the buffer to string
    oasys::HexDumpBuffer dump(tot_len);
    dump.append(tlvBuf, tot_len);
    log_always_p("ipnd-tlv/test", "Raw TLV Buffer:\n%s", dump.hexify().c_str());
    CHECK_EQUAL((int)len_rem, (int)(bLen - 10 - 23 - 10));
    ptr = tlvBuf; len_rem = bLen; tot_len = 0;

    /*** Test reading multiple structures *************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Multiple Structures: Read @@@@@@");
    TcpV4ClaService t4r;
    TcpV6ClaService t6r;
    UdpV4ClaService u4r;

    len = t4r.read((const u_char**)&ptr, len_rem);
    ptr += len; len_rem -= len; tot_len += len;
    len = t6r.read((const u_char**)&ptr, len_rem);
    ptr += len; len_rem -= len; tot_len += len;
    len = u4r.read((const u_char**)&ptr, len_rem);
    ptr += len; len_rem -= len; tot_len += len;

    CHECK_EQUAL((int)len_rem, (int)(bLen - 10 - 23 - 10));

    // check continuity
    CHECK_EQUAL((int)t4.getIpAddr(), (int)t4r.getIpAddr());
    CHECK_EQUAL((int)t4.getPort(), (int)t4r.getPort());

    CHECK_EQUAL((int)u4.getIpAddr(), (int)u4r.getIpAddr());
    CHECK_EQUAL((int)u4.getPort(), (int)u4r.getPort());

    for(unsigned int i = 0; i < 16; i++) {
        log_always_p("ipnd-tlv/test", "Testing IPv6 addr[%u] -> "
                "{0x%02x == 0x%02x}?", i,
                t6.getIpAddr().s6_addr[i], t6r.getIpAddr().s6_addr[i]);
        CHECK(t6.getIpAddr().s6_addr[i] == t6r.getIpAddr().s6_addr[i]);
    }
    CHECK_EQUAL((int)t6.getPort(), (int)t6r.getPort());

    return UNIT_TEST_PASSED;
}

/**
 * Checks error handling
 */
DECLARE_TEST(ErrHandling) {
    unsigned int bLen = 64;
    u_char tlvBuf[bLen];
    u_char *ptr = tlvBuf;
    int len = -1;
    unsigned int len_rem = bLen;

    clearBuf(tlvBuf, bLen);

    in_addr addr4;
    CHECK_SYS(inet_aton("128.64.128.64", &addr4) != 0);

    /*** Test tag mismatch error handling *************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Error Handling: Tags @@@@@@");
    TcpV4ClaService t4(addr4.s_addr, 0xaa01);
    UdpV4ClaService u4r;

    t4.write((const u_char**)&ptr, len_rem);
    len = u4r.read((const u_char**)&ptr, len_rem);
    CHECK_EQUAL(len, (int)ipndtlv::IPNDSDTLV::ERR_TAG);

    /*** Test length remaining error handling *********************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Error Handling: Length @@@@@@");
    len = u4r.read((const u_char**)&ptr, 8);
    CHECK_EQUAL(len, (int)ipndtlv::IPNDSDTLV::ERR_LENGTH);

    clearBuf(tlvBuf, bLen);

    /*** Test bad construction error handling *********************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing Error Handling: Processing @@@@@@");
    uint8_t tmp[10] = {0x41, 0x08, 0x03, 0x04, 0xbb, 0x05, 0x80, 0x40, 0x80, 0x40};
    u_char *ptr2 = (u_char*)tmp;
    len = u4r.read((const u_char**)&ptr2, 10);
    CHECK_EQUAL(len, (int)ipndtlv::IPNDSDTLV::ERR_PROCESSING);

    return UNIT_TEST_PASSED;
}

/**
 * Checks service factory functionality
 */
DECLARE_TEST(ServiceFactory) {
    unsigned int bLen = 64;
    u_char tlvBuf[bLen];
    u_char *ptr = tlvBuf;
    unsigned int remain = bLen;
    int len = -1;

    clearBuf(tlvBuf, bLen);

    /*** Test factory configuration util **************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing ServiceFactory: Default @@@@@@");
    const char *argvt4[3] = {"cla-tcp-v4", "addr=128.64.128.64", "port=80"};
    const char *argvu4[3] = {"cla-udp-v4", "addr=128.64.128.64", "port=81"};

    TcpV4ClaService *t4 = (TcpV4ClaService*)IPNDServiceFactory::instance()->configureService(3, argvt4);
    CHECK(t4 != NULL);
    UdpV4ClaService *u4 = (UdpV4ClaService*)IPNDServiceFactory::instance()->configureService(3, argvu4);
    CHECK(u4 != NULL);

    len = t4->write((const u_char**)&ptr, remain);
    ptr += len; remain -= len;
    len += u4->write((const u_char**)&ptr, remain);

    oasys::HexDumpBuffer dump((unsigned int)len);
    dump.append(tlvBuf, (unsigned int)len);
    log_always_p("ipnd-tlv/test", "Raw TLV Buffer:\n%s", dump.hexify().c_str());

    /*** Test factory buffer read util ****************************************/
    std::stringstream ss;
    ss.write((char*)tlvBuf, len);
    int result;
    std::list<IPNDService*> svcs =
            IPNDServiceFactory::instance()->readServices(2, ss, &result);
    CHECK(result > 0);
    CHECK_EQUAL((int)svcs.size(), 2);

    std::list<IPNDService*>::const_iterator iter = svcs.begin();
    TcpV4ClaService *t4r = (TcpV4ClaService*)(*iter++);
    UdpV4ClaService *u4r = (UdpV4ClaService*)(*iter++);

    CHECK_EQUAL((int)t4->getIpAddr(), (int)t4r->getIpAddr());
    CHECK_EQUAL((int)t4->getPort(), (int)t4r->getPort());
    CHECK_EQUAL((int)u4->getIpAddr(), (int)u4r->getIpAddr());
    CHECK_EQUAL((int)u4->getPort(), (int)u4r->getPort());

    delete t4;
    delete u4;
    delete t4r;
    delete u4r;
    svcs.clear();
    clearBuf(tlvBuf, bLen);
    ptr = tlvBuf;
    remain = bLen;
    dump.clear();

    /*** Test custom service impl *********************************************/
    log_always_p("ipnd-tlv/test", "@@@@@@ Testing ServiceFactory: Custom @@@@@@");
    dtn::IPNDServiceFactory::instance()->addPlugin(new FooPlugin());
    const char *argvcsi[1] = {"foo-svc"};
    FooService *fs = (FooService*)IPNDServiceFactory::instance()->configureService(1, argvcsi);
    CHECK(fs != NULL);

    len = fs->write((const u_char**)&ptr, remain);
    oasys::HexDumpBuffer dump2((unsigned int)len);
    dump2.append(tlvBuf, len);
    log_always_p("ipnd-tlv/test", "Raw TLV Buffer:\n%s", dump2.hexify().c_str());

    std::stringstream ss2;
    ss2.write((char*)tlvBuf, len);
    svcs = IPNDServiceFactory::instance()->readServices(1, ss2, &result);
    CHECK(result > 0);
    CHECK_EQUAL((int)svcs.size(), 1);

    iter = svcs.begin();

    FooService *fsr = (FooService*)(*iter);
    CHECK(fs->getKey() == fsr->getKey());

    delete fs;
    delete fsr;
    svcs.clear();
    dump2.clear();
    clearBuf(tlvBuf, bLen);

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(IPNDSDTLVTest) {
    ADD_TEST(PrimEncDec);
    ADD_TEST(ConstrEncDec);
    ADD_TEST(MultiEncDec);
    ADD_TEST(ErrHandling);
    ADD_TEST(ServiceFactory);
}

DECLARE_TEST_FILE(IPNDSDTLVTest, "ipnd-sb-tlv test");
