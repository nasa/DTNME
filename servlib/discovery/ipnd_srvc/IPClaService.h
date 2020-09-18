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
 * IPClaService.h
 *
 * Implements some standard Internet Protocol CLA services. These services
 * generally have an address (IP or hostname) and a transport port number.
 */

#ifndef IPCLASERVICE_H_
#define IPCLASERVICE_H_

#include "discovery/IPNDService.h"

#include <netinet/in.h>
#include <string>

using namespace ipndtlv;

namespace dtn {

class IPClaService: public IPNDService {
public:
    virtual ~IPClaService();

    /**
     * Gets the port number for the CLA represented by the service
     */
    uint16_t getPort() const;

    /**
     * Gets the port number for the CLA as a string
     */
    std::string getPortStr() const;

    /**
     * Minimum structure sizes for the various types
     */
    static const unsigned int MIN_LEN_v4 = 10;
    static const unsigned int MIN_LEN_v6 = 23;
    static const unsigned int MIN_LEN_HNAME = 8;

protected:
    /**
     * Constructors should only be accessed by subclasses.
     * Port number defaults to zero if not specified.
     */
    IPClaService(const uint8_t tag_value, const std::string &name);
    IPClaService(const uint8_t tag_value, const std::string &name,
            uint16_t port);
};

// XXX (agladd): Should really make two additional "base" classes here for less
// code duplication: one for a generic IPv4 CLA and another for a generic IPv6
// CLA.

/**
 * Defines the cla-tcp-v4 service as described in draft-irtf-dtnrg-ipnd-02
 */
class TcpV4ClaService: public IPClaService {
public:
    TcpV4ClaService();
    TcpV4ClaService(in_addr_t ip_addr, uint16_t port);

    /**
     * Gets the IP address for the CLA represented by the service
     */
    in_addr_t getIpAddr() const;

    /**
     * Gets the IP address for the CLA as a string
     */
    std::string getIpAddrStr() const;

    /**
     * The minimum length of an IPv4 CLA service
     */
    unsigned int getMinLength() const;
};

/**
 * Defines the cla-udp-v4 service as described in draft-irtf-dtnrg-ipnd-02
 */
class UdpV4ClaService: public IPClaService {
public:
    UdpV4ClaService();
    UdpV4ClaService(in_addr_t ip_addr, uint16_t port);

    /**
     * Gets the IP address for the CLA represented by the service
     */
    in_addr_t getIpAddr() const;

    /**
     * Gets the IP address for the CLA as a string
     */
    std::string getIpAddrStr() const;

    /**
     * The minimum length of an IPv4 CLA service
     */
    unsigned int getMinLength() const;
};

/**
 * Defines the cla-tcp-v6 service as described in draft-irtf-dtnrg-ipnd-02
 */
class TcpV6ClaService: public IPClaService {
public:
    TcpV6ClaService();
    TcpV6ClaService(in6_addr ip_addr, uint16_t port);

    /**
     * Gets the IP address for the CLA represented by the service
     */
    in6_addr getIpAddr() const;

    /**
     * Gets the IP address for the CLA as a string
     */
    std::string getIpAddrStr() const;

    /**
     * The minimum length of an IPv6 CLA service
     */
    unsigned int getMinLength() const;
};

/**
 * Defines the cla-udp-v6 service as described in draft-irtf-dtnrg-ipnd-02
 */
class UdpV6ClaService: public IPClaService {
public:
    UdpV6ClaService();
    UdpV6ClaService(in6_addr ip_addr, uint16_t port);

    /**
     * Gets the IP address for the CLA represented by the service
     */
    in6_addr getIpAddr() const;

    /**
     * Gets the IP address for the CLA as a string
     */
    std::string getIpAddrStr() const;

    /**
     * The minimum length of an IPv6 CLA service
     */
    unsigned int getMinLength() const;
};

// TODO (agladd): Hostname CLA service

}

#endif /* IPCLASERVICE_H_ */
