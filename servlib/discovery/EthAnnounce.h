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

#ifndef _ETH_ANNOUNCE_H_
#define _ETH_ANNOUNCE_H_

#ifndef DTN_CONFIG_STATE
#error "MUST INCLUDE dtn-config.h before including this file"
#endif

#include "Announce.h"
#include "EthDiscovery.h"

namespace dtn {

class EthAnnounce : public Announce
{
public:

    size_t format_advertisement(u_char*, size_t);

    virtual ~EthAnnounce() {}

protected:

    friend class Announce;

    // Some stuff from EthConvergenceLayer here
    typedef EthDiscovery::EthBeaconHeader EthBeaconHeader;
    static const u_int8_t  ETHCL_VERSION = 0x01;
    static const u_int16_t ETHERTYPE_DTN = 0xd710;
    static const u_int8_t  ETHCL_BEACON = 0x01;


    EthAnnounce();

    bool configure(const std::string& name, ConvergenceLayer* cl,
                   int argc, const char *argv[]);
};

} // dtn

#endif
