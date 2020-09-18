/*
 *    Copyright 2006 Intel Corporation
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
#  include <dtn-config.h>
#endif

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include "BundleRouter.h"
#include "RouteEntry.h"
#include "naming/EndpointIDOpt.h"

namespace dtn {

//----------------------------------------------------------------------
RouteEntry::RouteEntry(const EndpointIDPattern& dest_pattern,
                       const LinkRef& link)
    : dest_pattern_(dest_pattern),
      source_pattern_(EndpointIDPattern::WILDCARD_EID()),
      bundle_cos_((1 << Bundle::COS_BULK) |
                  (1 << Bundle::COS_NORMAL) |
                  (1 << Bundle::COS_EXPEDITED)),
      priority_(BundleRouter::config_.default_priority_),
      link_(link.object(), "RouteEntry"),
      route_to_(),
      action_(ForwardingInfo::FORWARD_ACTION),
      custody_spec_(),
      info_(NULL)
{
    // give precedence to ALWAYSON over OPPORTUNISTIC links
    if (link_->type() == Link::ALWAYSON) {
        priority_ = 1;
    }
}

//----------------------------------------------------------------------
RouteEntry::RouteEntry(const EndpointIDPattern& dest_pattern,
                       const EndpointIDPattern& route_to)
    : dest_pattern_(dest_pattern),
      source_pattern_(EndpointIDPattern::WILDCARD_EID()),
      bundle_cos_((1 << Bundle::COS_BULK) |
                  (1 << Bundle::COS_NORMAL) |
                  (1 << Bundle::COS_EXPEDITED)),
      priority_(BundleRouter::config_.default_priority_),
      link_("RouteEntry"),
      route_to_(route_to),
      action_(ForwardingInfo::FORWARD_ACTION),
      custody_spec_(),
      info_(NULL)
{
}

//----------------------------------------------------------------------
RouteEntry::~RouteEntry()
{
    if (info_)
        delete info_;
}

//----------------------------------------------------------------------
int
RouteEntry::parse_options(int argc, const char** argv, const char** invalidp)
{
    int num = custody_spec_.parse_options(argc, argv, invalidp);
    if (num == -1) {
        return -1;
    }
    
    argc -= num;
    
    oasys::OptParser p;

    p.addopt(new EndpointIDOpt("source_eid", &source_pattern_));
    p.addopt(new oasys::UIntOpt("route_priority", &priority_));
    p.addopt(new oasys::UIntOpt("cos_flags", &bundle_cos_));
    oasys::EnumOpt::Case fwdopts[] = {
        {"forward", ForwardingInfo::FORWARD_ACTION},
        {"copy",    ForwardingInfo::COPY_ACTION},
        {0, 0}
    };

    // default is to not change the action
    int action = action_;
    p.addopt(new oasys::EnumOpt("action", fwdopts, &action));

    int num2 = p.parse_and_shift(argc, argv, invalidp);
    if (num2 == -1) {
        return -1;
    }

    // this is paranoid, but action_ needs to be a u_int32_t since it's
    // serialized, but EnumOpt takes an int*
    action_ = action;
    
    if ((bundle_cos_ == 0) || (bundle_cos_ >= (1 << 3))) {
        static const char* s = "invalid cos flags";
        invalidp = &s;
        return -1;
    }

    return num + num2;
}

//----------------------------------------------------------------------
int
RouteEntry::format(char* bp, size_t sz) const
{
    // XXX/demmer when the route table is serialized, add an integer
    // id for the route entry and include it here.
    return snprintf(bp, sz, "%s -> %s (%s)",
                    dest_pattern().c_str(),
                    next_hop_str().c_str(),
                    ForwardingInfo::action_to_str(action()));
}

static const char* DASHES =
    "---------------------------------------------"
    "---------------------------------------------"
    "---------------------------------------------"
    "---------------------------------------------"
    "---------------------------------------------"
    "---------------------------------------------";

//----------------------------------------------------------------------
void
RouteEntry::dump_header(oasys::StringBuffer* buf,
                        int dest_eid_width,
                        int source_eid_width,
                        int next_hop_width)
{
    // though this is a less efficient way of appending this data, it
    // makes it much easier to copy the format string to the ::dump
    // method, and given that it's only used for diagnostics, the
    // performance impact is negligible
    buf->appendf("%-*.*s %-*.*s %3s    %-*.*s %-7s %5s [%-15s]\n"
                 "%-*.*s %-*.*s %3s    %-*.*s %-7s %5s [%-5s %-3s %-5s]\n"
                 "%.*s\n",
                 dest_eid_width, dest_eid_width, "destination",
                 source_eid_width, source_eid_width, "source",
                 "COS",
                 next_hop_width, next_hop_width, "next hop",
                 " fwd  ",
                 "route",
                 "custody timeout",
                 
                 dest_eid_width, dest_eid_width, "endpoint id",
                 source_eid_width, source_eid_width, " eid",
                 "BNE",
                 next_hop_width, next_hop_width, "link/eid",
                 "action",
                 "prio",
                 "min",
                 "pct",
                 "  max",
                 
                 dest_eid_width + source_eid_width + next_hop_width + 51,
                 DASHES);
}

//----------------------------------------------------------------------
void
RouteEntry::append_long_string(oasys::StringBuffer* buf,
                               oasys::StringVector* long_strings,
                               int width, const std::string& str)
{
    size_t tmplen;
    char tmp[64];
    
    if (str.length() <= (size_t)width) {
        buf->appendf("%-*.*s ", width, width, str.c_str());
    } else {
        size_t index;
        for (index = 0; index < long_strings->size(); ++index) {
            if ((*long_strings)[index] == str) break;
        }
        if (index == long_strings->size()) {
            long_strings->push_back(str);
        }
            
        tmplen = snprintf(tmp, sizeof(tmp), "[%zu] ", index);
        buf->appendf("%.*s... %s",
                     width - 3 - (int)tmplen, str.c_str(), tmp);

    }
}

//----------------------------------------------------------------------
void
RouteEntry::dump(oasys::StringBuffer* buf,
                 oasys::StringVector* long_strings,
                 int dest_eid_width,
                 int source_eid_width,
                 int next_hop_width) const
{
    append_long_string(buf, long_strings, dest_eid_width, dest_pattern().str());
    append_long_string(buf, long_strings, source_eid_width, source_pattern().str());

    buf->appendf("%c%c%c -> ",
                 (bundle_cos_ & (1 << Bundle::COS_BULK))      ? '1' : '0',
                 (bundle_cos_ & (1 << Bundle::COS_NORMAL))    ? '1' : '0',
                 (bundle_cos_ & (1 << Bundle::COS_EXPEDITED)) ? '1' : '0');

    append_long_string(buf, long_strings, next_hop_width, next_hop_str());
    
    buf->appendf("%7s %5d [%-5d %3d %5d]\n",
                 ForwardingInfo::action_to_str(action()),
                 priority(),
                 custody_spec().min_,
                 custody_spec().lifetime_pct_,
                 custody_spec().max_);
}

//----------------------------------------------------------------------
void
RouteEntry::serialize(oasys::SerializeAction *a)
{
    a->process("dest_pattern", &dest_pattern_);
    a->process("source_pattern", &source_pattern_);
    a->process("bundle_cos", &bundle_cos_);
    a->process("priority", &priority_);
    a->process("link", const_cast<std::string *>(&link_->name_str()));
    a->process("route_to", &route_to_);
    a->process("action", &action_);
    a->process("custody_spec", &custody_spec_);

    // XXX/demmer handle serialization of info
}

} // namespace dtn
