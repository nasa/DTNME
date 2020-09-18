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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <inttypes.h>
#include <sstream>

#include "GbofId.h"

// GBOFID -- Global Bundle Or Fragment ID

namespace oasys {

//----------------------------------------------------------------------
template<>
const char*
InlineFormatter<dtn::GbofId>
::format(const dtn::GbofId& id)
{
    if (! id.is_fragment_) {
        buf_.appendf("<%s, %" PRIu64 ".%" PRIu64 ">",
                     id.source_.c_str(),
                     id.creation_ts_.seconds_,
                     id.creation_ts_.seqno_);
    } else {
        buf_.appendf("<%s, %" PRIu64 ".%" PRIu64 ", FRAG len %u offset %u>",
                     id.source_.c_str(),
                     id.creation_ts_.seconds_,
                     id.creation_ts_.seqno_,
                     id.frag_length_, id.frag_offset_);
    }
    return buf_.c_str();
}
} // namespace oasys

namespace dtn {

//----------------------------------------------------------------------
GbofId::GbofId()
    : source_(EndpointID::NULL_EID()),
      is_fragment_(false),
      frag_length_(0),
      frag_offset_(0),
      is_gbofid_str_set_(false)
{
    gbofid_str_.clear();
}

//----------------------------------------------------------------------
GbofId::GbofId(EndpointID      source,
               BundleTimestamp creation_ts,
               bool            is_fragment,
               u_int32_t       frag_length,
               u_int32_t       frag_offset)
    : source_(source),
      creation_ts_(creation_ts),
      is_fragment_(is_fragment),
      frag_length_(frag_length),
      frag_offset_(frag_offset),
      is_gbofid_str_set_(false)
{
    set_gbofid_str();
}

//----------------------------------------------------------------------
GbofId::GbofId (const GbofId& other)
    : source_(other.source_),
      creation_ts_(other.creation_ts_),
      is_fragment_(other.is_fragment_),
      frag_length_(other.frag_length_),
      frag_offset_(other.frag_offset_),
      is_gbofid_str_set_(false)
{
    set_gbofid_str();
}

//----------------------------------------------------------------------
GbofId::~GbofId()
{
}

//----------------------------------------------------------------------
void
GbofId::init(EndpointID      source,
             BundleTimestamp creation_ts,
             bool            is_fragment,
             u_int32_t       frag_length,
             u_int32_t       frag_offset)
{
    source_ = source;
    creation_ts_ = creation_ts;
    is_fragment_ = is_fragment;
    frag_length_ = frag_length;
    frag_offset_ = frag_offset;
    is_gbofid_str_set_ = false;
    set_gbofid_str();
}

//----------------------------------------------------------------------
bool
GbofId::equals(const GbofId& id) const
{
    return (0 == str().compare(id.str()));
/*
    //TODO: dz - gbofid_str_ string compare faster?
    if (creation_ts_.seconds_ == id.creation_ts_.seconds_ &&
        creation_ts_.seqno_   == id.creation_ts_.seqno_ &&
        is_fragment_          == id.is_fragment_ &&
        (!is_fragment_ || 
         (frag_length_ == id.frag_length_ && frag_offset_ == id.frag_offset_)) &&
        source_.equals(id.source_)) 
    {
        return true;
    } else {
        return false;
    }
*/
}

//----------------------------------------------------------------------
bool
GbofId::operator<(const GbofId& other) const
{
    return (-1 == str().compare(other.str()));
/*
    //TODO: dz - gbofid_str string compare safe here?
    //      string may not compare to the same order but okay for our purposes
    if (source_ < other.source_) return true;
    if (other.source_ < source_) return false;

    if (creation_ts_ < other.creation_ts_) return true;
    if (creation_ts_ > other.creation_ts_) return false;

    if (is_fragment_  && !other.is_fragment_) return true;
    if (!is_fragment_ && other.is_fragment_) return false;
    
    if (is_fragment_) {
        if (frag_length_ < other.frag_length_) return true;
        if (other.frag_length_ < frag_length_) return false;

        if (frag_offset_ < other.frag_offset_) return true;
        if (other.frag_offset_ < frag_offset_) return false;
    }

    return false; // all equal
*/
}

//----------------------------------------------------------------------
bool
GbofId::equals(EndpointID source,
               BundleTimestamp creation_ts,
               bool is_fragment,
               u_int32_t frag_length,
               u_int32_t frag_offset) const
{
    if (creation_ts_.seconds_ == creation_ts.seconds_ &&
	creation_ts_.seqno_ == creation_ts.seqno_ &&
	is_fragment_ == is_fragment &&
	(!is_fragment || 
	 (frag_length_ == frag_length && frag_offset_ == frag_offset)) &&
        source_.equals(source))
    {
        return true;
    } else {
        return false;
    }
}

//----------------------------------------------------------------------
std::string
GbofId::str() const
{
    if (!is_gbofid_str_set_) {
        GbofId* tmp = (GbofId*) this;
        tmp->set_gbofid_str();
    }
    return gbofid_str_;
}

//----------------------------------------------------------------------
void
GbofId::set_gbofid_str()
{
/* XXX/dz - Need this format in addition to the InlineFormatter?

    std::ostringstream oss;

    oss << source_.str() << ","
        << creation_ts_.seconds_ << "," 
        << creation_ts_.seqno_ << ","
        << is_fragment_ << ","
        << frag_length_ << ","
        << frag_offset_;

    gbofid_str_ = oss.str();
*/

    gbofid_str_ = oasys::InlineFormatter<GbofId>().format(*this);
    is_gbofid_str_set_ = true;
}

} // namespace dtn
