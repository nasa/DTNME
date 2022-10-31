/* Copyright 2004-2006 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * $Id$
 */

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _GBOFID_H_
#define _GBOFID_H_

#include "naming/EndpointID.h"
#include "bundling/BundleTimestamp.h"

namespace dtn {

/**
 * Class definition for a GBOF ID (Global Bundle Or Fragment ID)
 */
class GbofId
{
public:
    GbofId();
    GbofId(EndpointID      source,
           BundleTimestamp creation_ts,
           bool            is_fragment,
           size_t       frag_length,
           size_t       frag_offset);
    ~GbofId();

    /**
     * Copy constructor
     */
    GbofId (const GbofId& other);

    /**
     * (Re)Initialization using provided values
     */
    void init(EndpointID      source,
              BundleTimestamp creation_ts,
              bool            is_fragment,
              size_t       frag_length,
              size_t       frag_offset);

    /**
     * Compares if GBOF IDs are the same.
     */
    bool equals(const GbofId& id) const;

    /** 
     * Compares if fields match those of this GBOF ID
     */
    bool equals(EndpointID,
                BundleTimestamp,
                bool,
                size_t,
                size_t) const;

    /**
     * Equality operator.
     */
    bool operator==(const GbofId& id) const {
        return equals(id);
    }
    
    /**
     * Comparison operator.
     */
    bool operator<(const GbofId& other) const;
    
    /**
     * Returns a string version of the gbof
     */
    std::string str() const { return gbofid_str_; }

    /// @{ Accessors
    const EndpointID& source()           const { return source_; }
    const BundleTimestamp& creation_ts() const { return creation_ts_; }
    bool is_fragment()                   const { return is_fragment_; }
    size_t frag_length()              const { return frag_length_; }
    size_t frag_offset()              const { return frag_offset_; }
    /// @}

    /// @{ Setters and mutable accessors
    void set_source(const EndpointID& eid);
    void set_source(std::string& eid);
    void set_creation_ts(const BundleTimestamp& ts);
    void set_creation_ts(uint64_t secs_or_millisecs, uint64_t seqno);
    void set_fragment(bool t, size_t offset=0, size_t length=0);
    void set_is_fragment(bool t);
    void set_frag_offset(size_t offset);
    void set_frag_length(size_t length);
    /// @}

private:
    void set_gbofid_str();

    EndpointID source_;		  ///< Source eid
    BundleTimestamp creation_ts_; ///< Creation timestamp
    bool is_fragment_;		  ///< Fragmentary Bundle
    size_t frag_length_;  	  ///< Length of original bundle
    size_t frag_offset_;	  ///< Offset of fragment in original bundle
    std::string gbofid_str_;      ///< String version of the gbof

};

} // namespace dtn

#endif /* _GBOFID_H_ */
