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
 * IPNDAnnouncement.cc
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "IPNDAnnouncement.h"
#include "IPNDService.h"
#include "ipnd_srvc/IPNDServiceFactory.h"

#include <oasys/debug/Log.h>

#include <netinet/in.h>
#include <typeinfo>
#include <iostream>
#include <sstream>
#include <string>
#include <list>
#include <cstring>

namespace dtn {

IPNDAnnouncement::IPNDAnnouncement(const DiscoveryVersion version,
        dtn::EndpointID eid, const bool remote)
    : Logger("IPNDAnnouncement","/dtn/discovery/ipnd/beacon"),
      remote_(remote), version_(version), canonical_eid_(eid),
      beacon_period_(0) {
}

IPNDAnnouncement::~IPNDAnnouncement() {
    clear_services();
}

dtn::EndpointID IPNDAnnouncement::get_endpoint_id() const
{
    return canonical_eid_;
}

const std::list<IPNDService*> IPNDAnnouncement::get_services() const
{
    return services_;
}

u_int8_t IPNDAnnouncement::get_flags() const
{
    return flags_;
}

u_int8_t IPNDAnnouncement::get_version() const {
    return version_;
}

void IPNDAnnouncement::clear_services() {
    if(remote_) {
        // free service resources since they're not attached to this node
        ServicesIter iter;
        for(iter = services_.begin(); iter != services_.end(); iter++) {
            delete (*iter);
        }
    }

    services_.clear();
}

void IPNDAnnouncement::add_service(IPNDService *service)
{
    services_.push_back(service);
}

void IPNDAnnouncement::set_sequence_number(u_int16_t sequence)
{
    sequence_num_ = sequence;
}

void IPNDAnnouncement::set_beacon_period(u_int64_t period)
{
    beacon_period_ = period;
}

u_int64_t IPNDAnnouncement::get_beacon_period() const
{
    return beacon_period_;
}

/**
 * Format as defined in draft-irtf-dtnrg-ipnd-02
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |    Version    |     Flags     |     Beacon Sequence Number    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          EID Len  (*)         |   Canonical EID (variable)    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * /                    Service Block (optional)                   /
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Beacon Period (*) (optional) |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */
size_t IPNDAnnouncement::format(u_char* bp, size_t len)
{
    std::stringstream buf;
    size_t length = 0;

    buf << version_;
    length += sizeof(version_);

    // compute the correct flags based on content of announcement
    u_int8_t flags = 0;
    if (canonical_eid_ != EndpointID())
    {
        flags |= IPNDAnnouncement::BEACON_CONTAINS_EID;
    }
    if (!services_.empty())
    {
        flags |= IPNDAnnouncement::BEACON_SERVICE_BLOCK;
    }
    // only include the period if it is non-zero.
    if (beacon_period_ > 0 && version_ > IPND_VERSION_01)
    {
        flags |= IPNDAnnouncement::BEACON_CONTAINS_PERIOD;
    }

    log_debug("IPND beacon flags [format]: 0x%02x", flags);

    buf << flags;
    length += sizeof(flags);

    // sequence number
    u_int16_t sn = htons(sequence_num_);
    buf.write((char*)&sn, 2);
    length += 2;

    // Endpoint ID
    if ( flags & IPNDAnnouncement::BEACON_CONTAINS_EID )
    {
        length += writeStringWithSDNV(buf, canonical_eid_.str());
    }

    // service block
    // XXX (agladd): Since version 0x04 introduced such major changes, we're
    // currently only implementing the new service block and are not
    // backward compatible with previous versions
    if(version_ >= IPND_VERSION_04 &&
            (flags & IPNDAnnouncement::BEACON_SERVICE_BLOCK)) {
        length += writeSDNV(buf, services_.size());

        ServicesIter iter;
        for(iter = services_.begin(); iter != services_.end(); iter++) {
            IPNDService *svc = (*iter);
            if(!svc->isFilled()) {
                log_info("Service indicates 'not filled'; skipping");
                continue;
            }
            // else write

            unsigned int req_len = svc->getLength();
            u_char *s_buf = new u_char[req_len];
            memset(s_buf, 0, req_len);
            int result = svc->write((const u_char**)&s_buf, req_len);

            if(result < 0) {
                log_err("Failure writing service definition to raw buffer; "
                        "skipping! [%i]", result);
                delete s_buf;
                continue;
            } else if((unsigned int)result != req_len) {
                log_err("Raw written size of service does not match expected;"
                        "skipping! [%i!=%u]", result, req_len);
                delete s_buf;
                continue;
            } else {
                // good
                log_debug("Writing raw service to buffer [len=%u]", req_len);
                buf.write((const char*)s_buf, req_len);
                length += req_len;
                delete s_buf;
            }
        }
    }

    // period in seconds
    if ( flags & IPNDAnnouncement::BEACON_CONTAINS_PERIOD )
    {
        log_debug("IPND beacon period [format]: %llu", beacon_period_);
        length += writeSDNV(buf, beacon_period_);
    }

    if (length <= len)
    {
        buf.seekg(0, std::ios::beg);
        buf.read((char*)bp, length);
//        memcpy(bp, buf.str().c_str(), length);
        log_debug("Formatted beacon into %u bytes", length);
        return length;
    }
    else
    {
        log_err("buffer too small in format");
        return 0;
    }
}

size_t IPNDAnnouncement::writeStringWithSDNV(std::ostream &out, const std::string & value) {

    size_t length = 0;
    char* sdnv;
    size_t encodeLen = SDNV::encoding_len(value.length());

    sdnv = new char[encodeLen];
    SDNV::encode(value.length(), sdnv, encodeLen);
    out.write(sdnv, encodeLen);
    delete sdnv;
    length += encodeLen;

    out << value.data();
    length += value.length();

    return length;
}

size_t IPNDAnnouncement::writeSDNV(std::ostream &out, const u_int64_t & value) {

    char* sdnv;
    size_t encodeLen = SDNV::encoding_len(value);

    sdnv = new char[encodeLen];
    SDNV::encode(value, sdnv, encodeLen);

    out.write(sdnv, encodeLen);
    delete sdnv;

    return encodeLen;
}

bool IPNDAnnouncement::parse(u_char* bp, size_t len)
{
    std::stringstream buf;
    buf.write((char*)bp, len);

    u_int8_t version = (u_int8_t)buf.get();
    version_ = version;

    log_debug("[Parse Beacon] Length: %d Version: 0x%02x", len, (int)version);

    if (version_ >= IPNDAnnouncement::IPND_VERSION_01)
    {
        u_int8_t flags = (u_int8_t)buf.get();
        flags_ = flags;

        log_debug("IPND beacon flags: 0x%02x", (int)flags);

        u_int16_t sn = 0;
        buf.read((char*)&sn, 2);

        // convert from network byte order
        sequence_num_ = ntohs(sn);
        log_debug("IPND beacon sequence number: %d", sequence_num_);

        if (flags & IPNDAnnouncement::BEACON_CONTAINS_EID)
        {
            std::string eid;
            if (readStringWithSDNV(buf, eid) <= 0) {
                log_err("Error reading Endpoint ID in IPND beacon, failed to "
                        "decode length");
                return false;
            }
            canonical_eid_.assign(eid);

            log_debug("IPND beacon from: %s", canonical_eid_.c_str());
        }

        // service block
        // XXX (agladd): Since version 0x04 introduced such major changes, we're
        // currently only implementing the new service block and are not
        // backward compatible with previous versions
        if(version_ >= IPND_VERSION_04 &&
                (flags & IPNDAnnouncement::BEACON_SERVICE_BLOCK)) {
             // read the number of services
             u_int64_t service_count = 0;
             if (readSDNV(buf, &service_count) <= 0) {
                 log_err("Error reading Service Block in IPND beacon, failed to"
                         " decode service count");
                 return false;
             }
             log_debug("IPND beacon contains %llu service block entries",
                     service_count);

             // clear any existing services
             clear_services();

             if(service_count > 0) {
                 // try to read services
                 int result;
                 services_ = IPNDServiceFactory::instance()->readServices(
                         (unsigned int)service_count, buf, &result);
                 if(result < 0) {
                     // problems
                     log_err("Unrecoverable failure reading services from "
                             "buffer! [%i]", result);
                     return false;
                 } else if(result == 0) {
                     log_err("Continuity error: service factory read zero "
                             "bytes!");
                     return false;
                 } else {
                     // all good
                     log_debug("Processed %i bytes of services successfully and "
                             "recognized %u services", result,
                             services_.size());
                 }
             }
             // else nothing to read
        }

        if (flags & IPNDAnnouncement::BEACON_BLOOMFILTER)
        {
            // TODO: make sure we have bloomfilter services
        }

        if (version >= IPND_VERSION_02)
        {
            if (flags & IPNDAnnouncement::BEACON_CONTAINS_PERIOD)
            {
                u_int64_t beacon_period = 0;
                if (readSDNV(buf, &beacon_period) <= 0) {
                    log_err("Error reading period in IPND beacon");
                    return false;
                }
                beacon_period_ = beacon_period;
                log_debug("IPND beacon period: %llu", beacon_period);
            }
        }
    }
    else
    {
        log_warn("Unsupported IPND version in beacon message: %d", version);
        return false;
    }
    return true;
}

size_t IPNDAnnouncement::readSDNV(std::istream &in, u_int64_t* value) {

    char sdnv[SDNV::MAX_LENGTH];
    char *sdnv_buf = sdnv;
    size_t sdnv_length = 0;

    in.read(sdnv_buf, sizeof(char));
    sdnv_length++;

    while (*sdnv_buf & 0x80) {
        sdnv_buf++;
        in.read(sdnv_buf, sizeof(char));
        sdnv_length++;
    }

    size_t decodedBytes = SDNV::decode(sdnv, sdnv_length, value);
    if (decodedBytes != sdnv_length) {
        return -1;
    } else {
        return sdnv_length;
    }
}

size_t IPNDAnnouncement::readStringWithSDNV(std::istream &in, std::string & value) {

    u_int64_t str_len = 0;
    size_t sdnv_len = readSDNV(in, &str_len);
    if (sdnv_len <= 0) {
        return -1;
    }

    char data[str_len];
    in.read(data, str_len);
    value.assign(data, str_len);
    return sdnv_len + str_len;
}

std::string IPNDAnnouncement::toString() const
{
    // TODO: improve!
    return canonical_eid_.str();
}

} // namespace dtn
