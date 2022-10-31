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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <inttypes.h>
#include <sstream>

#include "GbofId.h"

// GBOFID -- Global Bundle Or Fragment ID


namespace dtn {

//----------------------------------------------------------------------
GbofId::GbofId()
    : source_(EndpointID::NULL_EID()),
      is_fragment_(false),
      frag_length_(0),
      frag_offset_(0)
{
    gbofid_str_.clear();
}

//----------------------------------------------------------------------
GbofId::GbofId(EndpointID      source,
               BundleTimestamp creation_ts,
               bool            is_fragment,
               size_t       frag_length,
               size_t       frag_offset)
    : source_(source),
      creation_ts_(creation_ts),
      is_fragment_(is_fragment),
      frag_length_(frag_length),
      frag_offset_(frag_offset)
{
    set_gbofid_str();
}

//----------------------------------------------------------------------
GbofId::GbofId (const GbofId& other)
    : source_(other.source_),
      creation_ts_(other.creation_ts_),
      is_fragment_(other.is_fragment_),
      frag_length_(other.frag_length_),
      frag_offset_(other.frag_offset_)
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
             size_t       frag_length,
             size_t       frag_offset)
{
    source_ = source;
    creation_ts_ = creation_ts;
    is_fragment_ = is_fragment;
    frag_length_ = frag_length;
    frag_offset_ = frag_offset;
    set_gbofid_str();
}

//----------------------------------------------------------------------
bool
GbofId::equals(const GbofId& id) const
{
    return (0 == str().compare(id.str()));
}

//----------------------------------------------------------------------
bool
GbofId::operator<(const GbofId& other) const
{
    return (-1 == str().compare(other.str()));
}

//----------------------------------------------------------------------
bool
GbofId::equals(EndpointID source,
               BundleTimestamp creation_ts,
               bool is_fragment,
               size_t frag_length,
               size_t frag_offset) const
{
    if ((creation_ts_.secs_or_millisecs_ == creation_ts.secs_or_millisecs_) &&
        (creation_ts_.seqno_ == creation_ts.seqno_) &&
        (is_fragment_ == is_fragment) &&
        (!is_fragment || 
            ((frag_length_ == frag_length) && 
             (frag_offset_ == frag_offset))) &&
        source_.equals(source))
    {
        return true;
    } else {
        return false;
    }
}

//----------------------------------------------------------------------
void
GbofId::set_gbofid_str()
{
    char buf[128];

    if (!is_fragment_) {
        snprintf(buf, sizeof(buf), "<%s, %" PRIu64 ".%" PRIu64 ">",
                     source_.c_str(),
                     creation_ts_.secs_or_millisecs_,
                     creation_ts_.seqno_);
    } else {
        snprintf(buf, sizeof(buf), "<%s, %" PRIu64 ".%" PRIu64 ", FRAG len %zu offset %zu>",
                     source_.c_str(),
                     creation_ts_.secs_or_millisecs_,
                     creation_ts_.seqno_,
                     frag_length_,
                     frag_offset_);
    }

    buf[sizeof(buf)-1] = '\0';
    gbofid_str_ = buf;
}

//----------------------------------------------------------------------
void
GbofId::set_source(const EndpointID& eid)
{
    source_.assign(eid);
    set_gbofid_str();
}

//----------------------------------------------------------------------
void
GbofId::set_source(std::string& eid)
{
    source_.assign(eid);
    set_gbofid_str();
}

//----------------------------------------------------------------------
void
GbofId::set_creation_ts(uint64_t secs_or_millisecs, uint64_t seqno)
{
    creation_ts_.secs_or_millisecs_ = secs_or_millisecs;
    creation_ts_.seqno_ = seqno;
    set_gbofid_str();
}

//----------------------------------------------------------------------
void
GbofId::set_creation_ts(const BundleTimestamp& ts)
{
    creation_ts_ = ts; 
    set_gbofid_str();
}

//----------------------------------------------------------------------
void
GbofId::set_fragment(bool t, size_t offset, size_t length)
{ 
    is_fragment_ = t; 
    frag_offset_ = offset;
    frag_length_ = length;
    set_gbofid_str();
}

//----------------------------------------------------------------------
void
GbofId::set_is_fragment(bool t)
{ 
    is_fragment_ = t; 
    set_gbofid_str();
}

//----------------------------------------------------------------------
void
GbofId::set_frag_offset(size_t offset)
{ 
    frag_offset_ = offset;
    set_gbofid_str();
}

//----------------------------------------------------------------------
void
GbofId::set_frag_length(size_t length)
{ 
    frag_length_ = length;
    set_gbofid_str();
}




} // namespace dtn
