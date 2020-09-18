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
 * IPNDAnnouncement.h
 *
 * IPNDAnnouncement captures the content of the IP neighbor discovery (IPND)
 * beacon packet as defined in draft-irtf-dtnrg-ipnd-02.
 */

#ifndef IPNDANNOUNCEMENT_H_
#define IPNDANNOUNCEMENT_H_

#include <oasys/debug/Log.h>

#include "bundling/SDNV.h"
#include "naming/EndpointID.h"
#include "IPNDService.h"
#include <string>
#include <list>

namespace dtn {

class IPNDAnnouncement : public oasys::Logger {
public:
    enum BeaconFlags
    {
        BEACON_CONTAINS_EID    = 0x01,
        BEACON_SERVICE_BLOCK   = 0x02,
        BEACON_BLOOMFILTER     = 0x04,
        BEACON_CONTAINS_PERIOD = 0x08
    };

    enum DiscoveryVersion
    {
        IPND_VERSION_00 = 0x01,
        IPND_VERSION_01 = 0x02,
        IPND_VERSION_02 = 0x03,
        // XXX (agladd): I'm going to stop some possible confusion here and
        // align the enum identifiers with the actual value.
        IPND_VERSION_04 = 0x04
    };

    /**
     * Constructor
     *
     * @param version Set the version implemented by the beacon
     * @param eid The source endpoint ID
     * @param remote Whether this beacon is from a remote node
     */
    IPNDAnnouncement(const DiscoveryVersion version = IPND_VERSION_04,
            dtn::EndpointID eid = dtn::EndpointID(), const bool remote = true);

    virtual ~IPNDAnnouncement();

    /**
     * Retrieve list of services out of the beacon
     */
    const std::list<IPNDService*> get_services() const;

    /**
     * Clears the current list of services in the beacon. If the beacon was
     * created with remote=true, then the service resources are freed.
     */
    void clear_services();

    /**
     * Adds a service definition to the service block of the beacon.
     */
    void add_service(IPNDService *service);

    /**
     * Get string representation of the beacon
     */
    std::string toString() const;

    /**
     * Get the IPND version number of the beacon
     */
    u_int8_t get_version() const;

    /**
     * Retrieve the value of the flags field.
     *
     * @return flags
     */
    u_int8_t get_flags() const;

    /**
     * Set the sequence number of the beacon
     */
    void set_sequence_number(u_int16_t sequence);

    /**
     * Gets the source endpoint ID of the beacon
     */
    dtn::EndpointID get_endpoint_id() const;

    /**
     * Set the period of the beacon in seconds.
     */
    void set_beacon_period(u_int64_t period);

    /**
     * Retrieve the beacon period declared in this announcement in seconds.
     * A value of "0" should be interpreted as "undefined".
     */
    u_int64_t get_beacon_period() const;

    /**
     * Parses a beacon out of the given buffer. Returns true on success; false
     * on any failure.
     */
    bool parse(u_char* bp, size_t len);

    /**
     * Formats the beacon as raw data and writes it to the given buffer.
     *
     * @return The number of bytes written to the buffer or zero on error
     */
    size_t format(u_char* bp, size_t len);

    /**
     * Write the specified string to the specified output stream
     * and prepend it with its length represented as a SDNV.
     *
     * @return size_t the length of the SDNV and string written
     */
    static size_t writeStringWithSDNV(std::ostream &out, const std::string & value);

    /**
     * Write the specified self-delimiting numeric value to the
     * specified output stream.
     *
     * @param[in] out the output stream to write to
     * @param[in] value the SDNV to write
     * @return the number of bytes required to represent the SDNV
     */
    static size_t writeSDNV(std::ostream &out, const u_int64_t & value);

    /**
     * Read a string from the specified input stream.  The length of the string
     * precedes the string and is specified as a self-delimiting numeric value.
     *
     * @param[in] in the input stream to consume
     * @param[out] value the string to assign the read content to
     * @return size_t the length of the SDNV and string retrieved in bytes
     */
    static size_t readStringWithSDNV(std::istream &in, std::string & value);

    /**
     * Read a self-delimiting numeric value (SDNV) from the specified input
     * stream.  Store the value in the output parameter.
     *
     * @param[in] in the input stream
     * @param[out] value the value that was stored as a SDNV
     * @return the number of bytes read/consumed from the input stream
     */
    static size_t readSDNV(std::istream &in, u_int64_t* value);

private:

    bool remote_;            ///< whether this beacon was from a remote node
    u_int8_t version_;       ///< version of IPND defining this announcement
    u_int8_t flags_;         ///< flags field of IPND beacon
    u_int16_t sequence_num_; ///< beacon sequence number

    static const int HEADER_LENGTH = 32; ///< total length of fixed beacon header fields

    dtn::EndpointID canonical_eid_;
    std::list<dtn::IPNDService*> services_;
    u_int64_t beacon_period_;

    /**
     * Convenience typedef
     */
    typedef std::list<dtn::IPNDService*>::const_iterator ServicesIter;
};

} // namespace dtn

#endif /* IPNDANNOUNCEMENT_H_ */
