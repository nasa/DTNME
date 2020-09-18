/*
 *    Copyright 2004-2006 Intel Corporation
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

#include <oasys/util/StringUtils.h>

#include "LoggingRegistration.h"
#include "Registration.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "storage/GlobalStore.h"

namespace dtn {

LoggingRegistration::LoggingRegistration(const EndpointIDPattern& endpoint)
    : Registration(GlobalStore::instance()->next_regid(), endpoint,
                   Registration::DEFER, Registration::NEW, 0, 0)
{
    logpathf("/dtn/reg/logging/%d", regid_);
    set_active(true);
    
    log_info("new logging registration on endpoint %s", endpoint.c_str());
}

void
LoggingRegistration::deliver_bundle(Bundle* b)
{
    // use the bundle's builtin verbose formatting function and
    // generate the log output for all the header info
    oasys::StringBuffer buf;
    b->format_verbose(&buf);
    log_multiline(oasys::LOG_ALWAYS, buf.c_str());

    // now dump a short chunk of the payload data, either in ascii or
    // hexified string output
    size_t len = 128;
    size_t payload_len = b->payload().length();
    if (payload_len < len) {
        len = payload_len;
    }

    u_char payload_buf[payload_len];
    const u_char* data = b->payload().read_data(0, len, payload_buf);

    if (oasys::str_isascii(data, len)) {
        log_always("        payload (ascii): length %zu '%.*s'",
                   payload_len, (int)len, data);
    } else {
        std::string hex;
        oasys::hex2str(&hex, data, len);
        len *= 2;
        if (len > 128)
            len = 128;
        log_always("        payload (binary): length %zu %.*s",
                 payload_len, (int)len, hex.data());
    }

    // post the transmitted event
    BundleDaemon::post(new BundleDeliveredEvent(b, this));
}

} // namespace dtn
