/*
 *    Copyright 2006-2007 The MITRE Corporation
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
 *    The US Government will not be charged any license fee and/or royalties
 *    related to this software. Neither name of The MITRE Corporation; nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _ROUTER_CUSTOM_H_
#define _ROUTER_CUSTOM_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include "router.h"
#include "RouteEntry.h"
#include <bundling/Bundle.h>
#include <bundling/CustodySignal.h>
#include <contacts/Link.h>
#include <reg/Registration.h>

namespace dtn {
namespace rtrmessage {

class linkType : public linkType_base
{
public:
    linkType (const remote_eid::type&,
              const link_id::type&,
              const type::type_&,
              const nexthop::type&,
              const state::type&,
              const is_reachable::type& e,
              const is_usable::type& f,
              const how_reliable::type& g,
              const how_available::type& h,
              const clayer::type&,
              const min_retry_interval::type&,
              const max_retry_interval::type&,
              const idle_close_time::type&);
  
    linkType (const ::xercesc::DOMElement&,
              ::xml_schema::flags = 0,
              ::xml_schema::type* = 0);

    linkType (const linkType&,
              ::xml_schema::flags = 0,
              ::xml_schema::type* = 0);

    linkType (Link*);

    virtual linkType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class bundleType : public bundleType_base
{
public:
    bundleType (const source::type&,
                const dest::type&,
                const custodian::type&,
                const replyto::type&,
                const prevhop::type&,
                const length::type&,
                const location::type&,
                const bundleid::type&,
                const is_fragment::type&,
                const is_admin::type&,
                const do_not_fragment::type&,
                const priority::type&,
                const custody_requested::type&,
                const local_custody::type&,
                const singleton_dest::type&,
                const custody_rcpt::type&,
                const receive_rcpt::type&,
                const forward_rcpt::type&,
                const delivery_rcpt::type&,
                const deletion_rcpt::type&,
                const app_acked_rcpt::type&,
                const creation_ts_seconds::type&,
                const creation_ts_seqno::type&,
                const expiration::type&,
                const orig_length::type&,
                const frag_offset::type&,
                const owner::type&,
                const custodyid::type&,
                const ecos_flags::type&,
                const ecos_ordinal::type&);

    bundleType (const ::xercesc::DOMElement&,
                ::xml_schema::flags = 0,
                ::xml_schema::type* = 0);

    bundleType (const bundleType&,
                ::xml_schema::flags = 0,
                ::xml_schema::type* = 0);

    bundleType (Bundle*);

    virtual bundleType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;

    static const char * location_to_str(int location);
};

class contactType : public contactType_base
{
public:
    contactType (const link_attr::type&,
                 const start_time_sec::type&,
                 const start_time_usec::type&,
                 const duration::type&,
                 const bps::type&,
                 const latency::type&,
                 const pkt_loss_prob::type&);

    contactType (const ::xercesc::DOMElement&,
                 ::xml_schema::flags = 0,
                 ::xml_schema::type* = 0);

    contactType (const contactType&,
                 ::xml_schema::flags = 0,
                 ::xml_schema::type* = 0);

    contactType (Contact*);

    virtual contactType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class eidType : public eidType_base
{
public:
    eidType (const uri::type&);

    eidType (const ::xercesc::DOMElement&,
                ::xml_schema::flags = 0,
                ::xml_schema::type* = 0);

    eidType (const eidType&,
                ::xml_schema::flags = 0,
                ::xml_schema::type* = 0);

    eidType (const dtn::EndpointID&);

    eidType (const std::string&);

    virtual eidType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class gbofIdType : public gbofIdType_base
{
public:
    gbofIdType (const source::type&,
                const creation_ts::type&,
                const is_fragment::type&,
                const frag_length::type&,
                const frag_offset::type&);

    gbofIdType (const ::xercesc::DOMElement&,
                ::xml_schema::flags = 0,
                ::xml_schema::type* = 0);

    gbofIdType (const gbofIdType&,
                ::xml_schema::flags = 0,
                ::xml_schema::type* = 0);

    gbofIdType (const dtn::Bundle*);

    gbofIdType (dtn::CustodySignal::data_t);

    virtual gbofIdType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class key_value_pair : public key_value_pair_base
{
public:
    key_value_pair (const name::type&,
                    const bool_value::type&);

    key_value_pair (const name::type&,
                    const u_int_value::type&);

    key_value_pair (const name::type&,
                    const int_value::type&);

    key_value_pair (const name::type&,
                    const str_value::type&);

    key_value_pair (const ::xercesc::DOMElement&,
                 ::xml_schema::flags = 0,
                 ::xml_schema::type* = 0);

    key_value_pair (const key_value_pair&,
                 ::xml_schema::flags = 0,
                 ::xml_schema::type* = 0);

    key_value_pair (const dtn::NamedAttribute&);

    virtual key_value_pair*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class routeEntryType : public routeEntryType_base
{
public:
    routeEntryType (const dest_pattern::type&,
                    const source_pattern::type&,
                    const route_priority::type&,
                    const action::type&,
                    const link::type&);

    routeEntryType (const ::xercesc::DOMElement&,
                    ::xml_schema::flags = 0,
                    ::xml_schema::type* = 0);

    routeEntryType (const routeEntryType&,
                    ::xml_schema::flags = 0,
                    ::xml_schema::type* = 0);

    routeEntryType (RouteEntry*);

    virtual routeEntryType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};


// registrationType

class registrationType : public registrationType_base
{
public:
    registrationType (const endpoint::type&,
                      const regid::type&,
                      const action::type&,
                      const script::type&,
                      const expiration::type&);

    registrationType (const ::xercesc::DOMElement&,
                      ::xml_schema::flags = 0,
                      ::xml_schema::type* = 0);

    registrationType (const registrationType&,
                      ::xml_schema::flags = 0,
                      ::xml_schema::type* = 0);

    registrationType (Registration*);

    virtual registrationType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class to_lower
{
public:
    char operator() (char c) const
    {
        return tolower(c);
    }
};

std::string lowercase(const char *c_str);

#define bundle_ts_to_long(ts) (((::xml_schema::long_ ) (ts).seconds_ << 32) | (ts).seqno_)

} // namespace rtrmessage
} // namespace dtn

#endif // XERCES_C_ENABLED && EXTERNAL_DP_ENABLED
#endif // _ROUTER_CUSTOM_H_
