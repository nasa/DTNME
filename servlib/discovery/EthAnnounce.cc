/*
 *    Copyright 2013 Lana Black <sickmind@lavabit.com>
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

#include <net/ethernet.h>

#include <oasys/util/OptParser.h>
#include <bundling/BundleDaemon.h>
#include "EthAnnounce.h"

namespace dtn {

EthAnnounce::EthAnnounce()
{
}

size_t EthAnnounce::format_advertisement(u_char *bp, size_t len)
{
    const EndpointID& eid = BundleDaemon::instance()->local_eid();

    if(len < sizeof(EthBeaconHeader) + eid.length()) {
        log_warn("Not enough room to fill");
        return 0;
    }

    EthBeaconHeader *beacon= reinterpret_cast<EthBeaconHeader*>(bp);

    memset(bp, 0, len);

    beacon->version = ETHCL_VERSION;
    beacon->type = ETHCL_BEACON;
    beacon->interval = interval_ / 100;     // 100ms units
    beacon->eid_len = htons(eid.length());
    memcpy(beacon->sender_eid, eid.c_str(), eid.length());

    ::gettimeofday(&data_sent_,0);

    return sizeof(*beacon) + eid.length();
}

bool EthAnnounce::configure(const std::string &name, ConvergenceLayer *cl, int argc, const char *argv[])
{
    cl_ = cl;
    name_ = name;
    type_.assign(cl->name());

    if(strncmp(cl_->name(), "eth", 3)) {
        log_err("ethernet announce does not support cl type %s", cl_->name());
        return false;
    }

    oasys::OptParser p;

    p.addopt(new oasys::UIntOpt("interval", &interval_));

    const char* invalid;
    if(!p.parse(argc, argv, &invalid)) {
        log_err("bad parameter %s", invalid);
        return false;
    }

    if (interval_ == 0)
    {
        log_err("interval must be greater than 0");
        return false;
    }

    // convert from seconds to ms
    interval_ *= 1000;

    return true;
}

} // dtn
