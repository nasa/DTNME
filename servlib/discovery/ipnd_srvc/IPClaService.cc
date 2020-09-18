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
 * IPClaService.cc
 */

#include "IPClaService.h"

#include "discovery/ipnd_sd_tlv/Primitives.h"

#include <arpa/inet.h>
#include <endian.h>
#include <sstream>

namespace dtn {

IPClaService::IPClaService(const uint8_t tag_value, const std::string &name)
    : IPNDService(tag_value, name) {
    addChild(new Primitives::Fixed16());
}

IPClaService::IPClaService(const uint8_t tag_value, const std::string &name,
        uint16_t port)
    : IPNDService(tag_value, name) {
    addChild(new Primitives::Fixed16(port));
}

IPClaService::~IPClaService() {}

uint16_t IPClaService::getPort() const {
    Primitives::Fixed16 *port =
            (Primitives::Fixed16*)getChild(IPNDSDTLV::P_FIXED16);

    return port->getValue();
}

std::string IPClaService::getPortStr() const {
    std::stringstream ss;

    ss << (unsigned int)getPort();

    return ss.str();
}

/*** CLA-TCP-v4 ***************************************************************/

TcpV4ClaService::TcpV4ClaService()
    : IPClaService(IPNDSDTLV::C_CLA_TCPv4, "cla-tcp-v4") {
    addChild(new Primitives::Fixed32());
}

TcpV4ClaService::TcpV4ClaService(in_addr_t ip_addr, uint16_t port)
    : IPClaService(IPNDSDTLV::C_CLA_TCPv4, "cla-tcp-v4", port) {
    // XXX (agladd): calling ntohl on the ip address here because the Fixed32
    // datatype expects incoming values to be in host byte order, where an
    // in_addr_t is in network byte order (see inet(3)). This doesn't matter as
    // far as functionality goes within DTNME, but could matter for
    // interoperability with other implementations.
    addChild(new Primitives::Fixed32(ntohl((uint32_t)ip_addr)));
    setFilled();
}

in_addr_t TcpV4ClaService::getIpAddr() const {
    Primitives::Fixed32 *addr =
            (Primitives::Fixed32*)getChild(IPNDSDTLV::P_FIXED32);

    // XXX (agladd): See comment in TcpV4ClaService::TcpV4ClaService(...)
    return (in_addr_t)htonl(addr->getValue());
}

std::string TcpV4ClaService::getIpAddrStr() const {
    in_addr_t addr = getIpAddr();
    in_addr addrS;
    addrS.s_addr = addr;
    char ip_buf[INET_ADDRSTRLEN] = {'\0'};

    if(inet_ntop(AF_INET, &addrS, ip_buf, INET_ADDRSTRLEN) == NULL) {
        log_err("Could not convert IPv4 address to string!");
        return std::string("");
    } else {
        return std::string(ip_buf);
    }
}

unsigned int TcpV4ClaService::getMinLength() const {
    return MIN_LEN_v4;
}

/*** CLA-UDP-v4 ***************************************************************/

UdpV4ClaService::UdpV4ClaService()
    : IPClaService(IPNDSDTLV::C_CLA_UDPv4, "cla-udp-v4") {
    addChild(new Primitives::Fixed32());
}

UdpV4ClaService::UdpV4ClaService(in_addr_t ip_addr, uint16_t port)
    : IPClaService(IPNDSDTLV::C_CLA_UDPv4, "cla-udp-v4", port) {
    // XXX (agladd): See comment in TcpV4ClaService::TcpV4ClaService(...)
    addChild(new Primitives::Fixed32(ntohl((uint32_t)ip_addr)));
    setFilled();
}

in_addr_t UdpV4ClaService::getIpAddr() const {
    Primitives::Fixed32 *addr =
            (Primitives::Fixed32*)getChild(IPNDSDTLV::P_FIXED32);

    // XXX (agladd): See comment in TcpV4ClaService::TcpV4ClaService(...)
    return (in_addr_t)htonl(addr->getValue());
}

std::string UdpV4ClaService::getIpAddrStr() const {
    in_addr_t addr = getIpAddr();
    in_addr addrS;
    addrS.s_addr = addr;
    char ip_buf[INET_ADDRSTRLEN] = {'\0'};

    if(inet_ntop(AF_INET, &addrS, ip_buf, INET_ADDRSTRLEN) == NULL) {
        log_err("Could not convert IPv4 address to string!");
        return std::string("");
    } else {
        return std::string(ip_buf);
    }
}

unsigned int UdpV4ClaService::getMinLength() const {
    return MIN_LEN_v4;
}

/*** CLA-TCP-v6 ***************************************************************/

TcpV6ClaService::TcpV6ClaService()
    : IPClaService(IPNDSDTLV::C_CLA_TCPv6, "cla-tcp-v6") {
    addChild(new Primitives::Bytes());
}

TcpV6ClaService::TcpV6ClaService(in6_addr ip_addr, uint16_t port)
    : IPClaService(IPNDSDTLV::C_CLA_TCPv6, "cla-tcp-v6", port) {
    // XXX (agladd): The Bytes datatype doesn't change byte ordering so we're
    // good to go with IPv6 addresses.
    addChild(new Primitives::Bytes((u_char*)ip_addr.s6_addr, 16));
    setFilled();
}

in6_addr TcpV6ClaService::getIpAddr() const {
    Primitives::Bytes *addr =
            (Primitives::Bytes*)getChild(IPNDSDTLV::P_BYTES);

    // check for mischief
    if(addr->getValueLen() != 16) {
        log_err("IPv6 address in structure is not defined with len=16! [%u]",
                addr->getValueLen());

        // return the "any" addr ("::")
        return in6addr_any;
    } else {
        in6_addr addr6;
        memcpy(addr6.s6_addr, addr->getValue(), addr->getValueLen());

        return addr6;
    }
}

std::string TcpV6ClaService::getIpAddrStr() const {
    in6_addr addr = getIpAddr();
    char ip_buf[INET6_ADDRSTRLEN] = {'\0'};

    if(inet_ntop(AF_INET6, &addr, ip_buf, INET6_ADDRSTRLEN) == NULL) {
        log_err("Could not convert IPv6 address to string!");
        return std::string("");
    } else {
        return std::string(ip_buf);
    }
}

unsigned int TcpV6ClaService::getMinLength() const {
    return MIN_LEN_v6;
}

/*** CLA-UDP-v6 ***************************************************************/

UdpV6ClaService::UdpV6ClaService()
    : IPClaService(IPNDSDTLV::C_CLA_UDPv6, "cla-udp-v6") {
    addChild(new Primitives::Bytes());
}

UdpV6ClaService::UdpV6ClaService(in6_addr ip_addr, uint16_t port)
    : IPClaService(IPNDSDTLV::C_CLA_UDPv6, "cla-udp-v6", port) {
    addChild(new Primitives::Bytes((u_char*)ip_addr.s6_addr, 16));
    setFilled();
}

in6_addr UdpV6ClaService::getIpAddr() const {
    Primitives::Bytes *addr =
            (Primitives::Bytes*)getChild(IPNDSDTLV::P_BYTES);

    // check for mischief
    if(addr->getValueLen() != 16) {
        log_err("IPv6 address in structure is not defined with len=16! [%u]",
                addr->getValueLen());

        // return the "any" addr ("::")
        return in6addr_any;
    } else {
        in6_addr addr6;
        memcpy(addr6.s6_addr, addr->getValue(), addr->getValueLen());

        return addr6;
    }
}

std::string UdpV6ClaService::getIpAddrStr() const {
    in6_addr addr = getIpAddr();
    char ip_buf[INET6_ADDRSTRLEN] = {'\0'};

    if(inet_ntop(AF_INET6, &addr, ip_buf, INET6_ADDRSTRLEN) == NULL) {
        log_err("Could not convert IPv6 address to string!");
        return std::string("");
    } else {
        return std::string(ip_buf);
    }
}

unsigned int UdpV6ClaService::getMinLength() const {
    return MIN_LEN_v6;
}

// TODO (agladd): Hostname CLA service

}
