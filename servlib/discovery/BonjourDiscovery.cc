/*
 *    Copyright 2007 Intel Corporation
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

#ifdef OASYS_BONJOUR_ENABLED

#include "BonjourDiscovery.h"
#include "bundling/BundleDaemon.h"
#include "conv_layers/TCPConvergenceLayer.h"

#define ADDRESS_KEY "local_eid"

namespace dtn {

//----------------------------------------------------------------------
BonjourDiscovery::BonjourDiscovery(const std::string& name)
    : Discovery(name, "bonjour"),
      oasys::Thread("BonjourDiscovery", DELETE_ON_EXIT),
      notifier_("/dtn/discovery/bonjour"),
      shutdown_(false)
{
}

//----------------------------------------------------------------------
BonjourDiscovery::~BonjourDiscovery()
{
    // XXX/demmer call DNSServiceRefDeallocate??
}

//----------------------------------------------------------------------
bool
BonjourDiscovery::configure(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;

    start();
    return true;
}

//----------------------------------------------------------------------
void
BonjourDiscovery::shutdown()
{
    shutdown_ = true;
    notifier_.notify();
}

//----------------------------------------------------------------------
void
BonjourDiscovery::run()
{
    DNSServiceRef register_svc, browse_svc;
    DNSServiceErrorType err;

    const EndpointID& local_eid = BundleDaemon::instance()->local_eid();
    char txt[255];
    TXTRecordRef record;
    TXTRecordCreate(&record, 255, &txt);
    err = TXTRecordSetValue(&record, ADDRESS_KEY, local_eid.length(), local_eid.data());

    if (err != kDNSServiceErr_NoError) {
        log_err("KURTIS error in DNSServiceRegister: %s", dns_service_strerror(err));
        return;
    }

    // call DNSServiceRegister to announce the tcp service listening
    // on the default port
    err = DNSServiceRegister(&register_svc,
                             0 /* interface */,
                             0 /* flags */,
                             NULL /* name */,
                             "_dtn._tcp" /* regtype */,
                             NULL /* domain */,
                             NULL /* host */,
                             htons(TCPConvergenceLayer::TCPCL_DEFAULT_PORT),
                             TXTRecordGetLength(&record) /* txtLen */,
                             TXTRecordGetBytesPtr(&record) /* txtRecord */,
                             register_reply_callback /* callback */,
                             this /* context */);

    TXTRecordDeallocate(&record);

    if (err != kDNSServiceErr_NoError) {
        log_err("error in DNSServiceRegister: %s", dns_service_strerror(err));
        return;
    }

    log_notice("DNSServiceRegister succeeded");
    svc_vec_.push_back(register_svc);

    // kick off a browse for other services on the local network
    err = DNSServiceBrowse(&browse_svc,
                           0 /* flags */, 
                           0 /* interface */,
                           "_dtn._tcp" /* regtype */, 
                           NULL /* domain */,
                           browse_callback /* callback */,
                           this /* context */);

    if (err != kDNSServiceErr_NoError) {
        log_err("error in DNSServiceBrowse: %s", dns_service_strerror(err));
        return;
    }

    log_notice("DNSServiceBrowse succeeded");
    svc_vec_.push_back(browse_svc);

    int notifier_fd = notifier_.read_fd();

    while (1) {
retry:
        int num_pollfds = svc_vec_.size() + 1;
        struct pollfd pollfds[num_pollfds];

        for (int i = 0; i < num_pollfds - 1; ++i) {
            pollfds[i].fd = DNSServiceRefSockFD(svc_vec_[i]);
            if (pollfds[i].fd == -1) {
                log_crit("DNSServiceRefSockFD failed -- removing svc %d!!", i);
                svc_vec_.erase(svc_vec_.begin() + i);
                goto retry;
            }
            pollfds[i].events  = POLLIN;
            pollfds[i].revents = 0;
        }
        
        pollfds[num_pollfds - 1].fd      = notifier_fd;
        pollfds[num_pollfds - 1].events  = POLLIN;
        pollfds[num_pollfds - 1].revents = 0;
        
        int cc = oasys::IO::poll_multiple(pollfds, num_pollfds, -1, NULL,
                                          logpath_);
        if (cc <= 0) {
            log_err("unexpected return from poll_multiple: %d", cc);
            return;
        }

        if (shutdown_) {
            log_debug("shutdown_ bit set, exiting");
            break;
        }

        for (int i = 0; i < num_pollfds - 1; ++i) {
            if (pollfds[i].revents != 0) {
                log_debug("calling DNSServiceProcessResult for svc %d (fd %d)",
                          i, pollfds[i].fd);
                DNSServiceProcessResult(svc_vec_[i]);
            }
        }
    }
}

//----------------------------------------------------------------------
const char*
BonjourDiscovery::dns_service_strerror(DNSServiceErrorType err)
{
    switch(err) {
    case kDNSServiceErr_NoError: return "kDNSServiceErr_NoError";
    case kDNSServiceErr_Unknown: return "kDNSServiceErr_Unknown";
    case kDNSServiceErr_NoSuchName: return "kDNSServiceErr_NoSuchName";
    case kDNSServiceErr_NoMemory: return "kDNSServiceErr_NoMemory";
    case kDNSServiceErr_BadParam: return "kDNSServiceErr_BadParam";
    case kDNSServiceErr_BadReference: return "kDNSServiceErr_BadReference";
    case kDNSServiceErr_BadState: return "kDNSServiceErr_BadState";
    case kDNSServiceErr_BadFlags: return "kDNSServiceErr_BadFlags";
    case kDNSServiceErr_Unsupported: return "kDNSServiceErr_Unsupported";
    case kDNSServiceErr_NotInitialized: return "kDNSServiceErr_NotInitialized";
    case kDNSServiceErr_AlreadyRegistered: return "kDNSServiceErr_AlreadyRegistered";
    case kDNSServiceErr_NameConflict: return "kDNSServiceErr_NameConflict";
    case kDNSServiceErr_Invalid: return "kDNSServiceErr_Invalid";
    case kDNSServiceErr_Firewall: return "kDNSServiceErr_Firewall";
    case kDNSServiceErr_Incompatible: return "kDNSServiceErr_Incompatible";
    case kDNSServiceErr_BadInterfaceIndex: return "kDNSServiceErr_BadInterfaceIndex";
    case kDNSServiceErr_Refused: return "kDNSServiceErr_Refused";
    case kDNSServiceErr_NoSuchRecord: return "kDNSServiceErr_NoSuchRecord";
    case kDNSServiceErr_NoAuth: return "kDNSServiceErr_NoAuth";
    case kDNSServiceErr_NoSuchKey: return "kDNSServiceErr_NoSuchKey";
    case kDNSServiceErr_NATTraversal: return "kDNSServiceErr_NATTraversal";
    case kDNSServiceErr_DoubleNAT: return "kDNSServiceErr_DoubleNAT";
    case kDNSServiceErr_BadTime: return "kDNSServiceErr_BadTime";
    default:
        static char buf[32];
        snprintf(buf, sizeof(buf), "%d", err);
        return buf;
    }
}

//----------------------------------------------------------------------
void
BonjourDiscovery::remove_svc(DNSServiceRef sdRef)
{
    SvcVector::iterator iter;
    for (iter = svc_vec_.begin(); iter != svc_vec_.end(); ++iter) {
        if (*iter == sdRef) {
            svc_vec_.erase(iter);
            return;
        }
    }
    
    log_err("remove_svc: can't find sdRef %p in vector!!", sdRef);
}

//----------------------------------------------------------------------
void
BonjourDiscovery::handle_register_reply(DNSServiceRef sdRef,
                                        DNSServiceFlags flags,
                                        DNSServiceErrorType errorCode,
                                        const char *name,
                                        const char *regtype,
                                        const char *domain)
{
    (void)flags;
    (void)errorCode;
    (void)name;
    (void)regtype;
    (void)domain;
    
    log_debug("handle_register_reply(%s, %s, %s): %s",
              name, regtype, domain, dns_service_strerror(errorCode));

    remove_svc(sdRef);
}

//----------------------------------------------------------------------
void
BonjourDiscovery::handle_browse(DNSServiceRef sdRef,
                                DNSServiceFlags flags, 
                                uint32_t interfaceIndex, 
                                DNSServiceErrorType errorCode, 
                                const char *name, 
                                const char *regtype, 
                                const char *domain)
{
    (void)sdRef;
    (void)interfaceIndex;

    if (errorCode != kDNSServiceErr_NoError) {
        log_warn("handle_browse(%s, %s, %s): error %s",
                 name, regtype, domain, dns_service_strerror(errorCode));
        return;
    }

    if (flags & kDNSServiceFlagsAdd) {
        log_info("browse found new entry: %s.%s.%s (if %d) -- setting up resolver",
                 name, regtype, domain, interfaceIndex);
        DNSServiceRef svc;
        DNSServiceErrorType err;

        err = DNSServiceResolve(&svc, 0, interfaceIndex, name, regtype, domain,
                                (DNSServiceResolveReply)resolve_callback, this);
        if (err != kDNSServiceErr_NoError) {
            log_err("error in DNSServiceResolve: %s",
                    dns_service_strerror(err));
            return;
        }
        
        svc_vec_.push_back(svc);
    } else {
        log_info("browse found old entry: %s.%s.%s",
                 name, regtype, domain);
    }
}

//----------------------------------------------------------------------
void
BonjourDiscovery::handle_resolve(DNSServiceRef sdRef, 
                                 DNSServiceFlags flags, 
                                 uint32_t interfaceIndex, 
                                 DNSServiceErrorType errorCode, 
                                 const char *fullname, 
                                 const char *hosttarget,
                                 uint16_t port,
                                 uint16_t txtlen,
                                 const char* txtRecord)
{
    EndpointID remote_eid;
    oasys::StaticStringBuffer<64> buf;
    unsigned char value_len;
    char* value;
        
    (void)sdRef;
    (void)flags;
    (void)interfaceIndex;
    
    if (errorCode != kDNSServiceErr_NoError) {
        log_warn("handle_resolve(%s, %s): error %s",
                 fullname, hosttarget, dns_service_strerror(errorCode));
        goto done;
    }

    if (txtlen == 0) {
        log_warn("handle_resolve(%s, %s): zero-length txt record",
	       fullname, hosttarget);
        goto done;
    }

    if (!TXTRecordContainsKey(txtlen, txtRecord, ADDRESS_KEY)){
        log_warn("handle_resolve(%s, %s): no ADDRESS_KEY field found in txt record",
		 fullname, hosttarget);
	goto done;
    }

    value = (char*) TXTRecordGetValuePtr(txtlen, txtRecord, ADDRESS_KEY, &value_len);

    remote_eid.assign(value, static_cast<size_t>(value_len));
    if (!remote_eid.valid()) {
        log_warn("handle_resolve(%s, %s): %s not a valid eid",
                 fullname, hosttarget, remote_eid.c_str());
        goto done;
    }

    if (remote_eid.equals(BundleDaemon::instance()->local_eid())) {
        log_info("handle_resolve(%s, %s): ignoring resolution of local eid %s",
                 fullname, hosttarget, remote_eid.c_str());
        goto done;
    }
    
    log_debug("handle_resolve: name %s host %s port %u txt %s if %d: err %s",
              fullname, hosttarget, ntohs(port), remote_eid.c_str(),
              interfaceIndex, dns_service_strerror(errorCode));

    buf.appendf("%s:%u", hosttarget, htons(port));
    
    // XXX/demmer in the future this should check for udp as well
    log_debug("calling handle_neighbor_discovered for next hop %s", buf.c_str());
    handle_neighbor_discovered("tcp", buf.c_str(), remote_eid);
    
done:
    remove_svc(sdRef);
}

} // namespace dtn

#endif /* OASYS_BONJOUR_ENABLED */
