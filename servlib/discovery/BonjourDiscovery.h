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

#ifndef _BONJOUR_DISCOVERY_H_
#define _BONJOUR_DISCOVERY_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#ifdef OASYS_BONJOUR_ENABLED

#include <dns_sd.h>
#include <vector>

#include <oasys/thread/Notifier.h>
#include <oasys/thread/Thread.h>
#include "Discovery.h"

namespace dtn {

/**
 * A Discovery instance that uses Mac OS X Bonjour (i.e. the Multicast
 * DNS protocol).
 */
class BonjourDiscovery : public Discovery, public oasys::Thread {
public:
    /// Constructor
    BonjourDiscovery(const std::string& name);
    virtual ~BonjourDiscovery();

    /// @{ Virtual from Discovery
    bool configure(int argc, const char* argv[]);
    void shutdown();
    /// @}

    /// Main Thread run function
    void run();

private:
    oasys::Notifier notifier_;	///< Notifier used for shutdown
    bool shutdown_;		///< Bit used for shutdown
    typedef std::vector<DNSServiceRef> SvcVector;
    SvcVector svc_vec_;

    int ttl_;			///< TTL for announcement

    ///< Callback for registration reply
    static void register_reply_callback(DNSServiceRef sdRef,
                                        DNSServiceFlags flags,
                                        DNSServiceErrorType errorCode,
                                        const char *name,
                                        const char *regtype,
                                        const char *domain,
                                        void *context) 
    {
        ((BonjourDiscovery*)context)->
            handle_register_reply(sdRef, flags, errorCode, name,
                                  regtype, domain);
    }

    /// Registration reply handler
    void handle_register_reply(DNSServiceRef sdRef,
                               DNSServiceFlags flags,
                               DNSServiceErrorType errorCode,
                               const char *name,
                               const char *regtype,
                               const char *domain);

    ///< Callback for browse
    static void browse_callback(DNSServiceRef sdRef, 
                                DNSServiceFlags flags, 
                                uint32_t interfaceIndex, 
                                DNSServiceErrorType errorCode, 
                                const char *serviceName, 
                                const char *regtype, 
                                const char *replyDomain, 
                                void *context)
    {
        ((BonjourDiscovery*)context)->
            handle_browse(sdRef, flags, interfaceIndex, errorCode, 
                          serviceName, regtype, replyDomain);
    }

    /// Browse handler
    void handle_browse(DNSServiceRef sdRef,
                       DNSServiceFlags flags, 
                       uint32_t interfaceIndex, 
                       DNSServiceErrorType errorCode, 
                       const char *serviceName, 
                       const char *regtype, 
                       const char *replyDomain);

    ///< Callback for resolve
    static void resolve_callback(DNSServiceRef sdRef, 
                                 DNSServiceFlags flags, 
                                 uint32_t interfaceIndex, 
                                 DNSServiceErrorType errorCode, 
                                 const char *fullname, 
                                 const char *hosttarget,
                                 uint16_t port,
                                 uint16_t txtlen,
                                 const char* txtRecord,
                                 void *context)
    {
        ((BonjourDiscovery*)context)->
            handle_resolve(sdRef, flags, interfaceIndex, errorCode, 
                           fullname, hosttarget, port, txtlen, txtRecord);
    }

    /// Resolve handler
    void handle_resolve(DNSServiceRef sdRef, 
                        DNSServiceFlags flags, 
                        uint32_t interfaceIndex, 
                        DNSServiceErrorType errorCode, 
                        const char *fullname, 
                        const char *hosttarget,
                        uint16_t port,
                        uint16_t txtlen,
                        const char* txtRecord);

    /**
     * For some lame reason, the dns service doesn't define a strerror
     * analog
     */
    static const char* dns_service_strerror(DNSServiceErrorType err);

    /**
     * Remove the given DNSServiceRef from the vector.
     */
    void remove_svc(DNSServiceRef sdRef);
};

} // namespace dtn

#endif /* OASYS_BONJOUR_ENABLED */

#endif /* _BONJOUR_DISCOVERY_H_ */
