/*
 *    Copyright 2012 Raytheon BBN Technologies
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
 * IPNDServiceFactory.cc
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "IPNDServiceFactory.h"

#include "discovery/IPNDAnnouncement.h"
#include "discovery/ipnd_sd_tlv/IPNDSDTLV.h"
#include "IPClaService.h"
#include <oasys/util/OptParser.h>
#include <oasys/util/Options.h>

#include <cstring>

namespace dtn {

/*** IPND SERVICE FACTORY *****************************************************/

template<>
IPNDServiceFactory* oasys::Singleton<IPNDServiceFactory,true>::instance_ = NULL;

IPNDServiceFactory::IPNDServiceFactory()
    : Logger("IPNDServiceFactory", "/ipnd-srvc/factory") {
    // always add the default plugin first
    addPlugin(new DefaultIpndServicePlugin());
}

IPNDServiceFactory::~IPNDServiceFactory() {
    PluginIter iter;
    for(iter = plugins_.begin(); iter != plugins_.end(); iter++) {
        delete (*iter);
    }

    plugins_.clear();
}

IPNDService *IPNDServiceFactory::configureService(const int argc,
        const char *argv[]) const {
    // must have no less than one parameter (the type name)
    if(argc < 1) {
        log_err("Need at least one parameter (service type) to configure "
                "service!");
        return NULL;
    }
    // else good enough

    std::string sType(argv[0]);

    // simply iterate through plugins until we get a hit
    IPNDService *srvc = NULL;
    PluginIter iter = plugins_.begin();
    while(iter != plugins_.end()) {
        Plugin *cur = (*iter);
        srvc = cur->configureService(sType, (argc - 1), (argv + 1));

        if(srvc != NULL) {
            break;
        }

        iter++;
    }

    if(srvc == NULL) {
        log_info("Plugins did not recognize service configuration request");
    } else {
        log_info("New service recognized and configured");
    }

    return srvc;
}

std::list<IPNDService*> IPNDServiceFactory::readServices(
        const unsigned int expected, std::istream &in, int *num_read) const {
    // our return value
    std::list<IPNDService*> services;

    // first figure out our remaining length
    unsigned int len_remain = 0, found = 0;
    int curPos = in.tellg(), result = -1, totalRead = 0;
    in.seekg(0, std::ios::end);
    len_remain = (unsigned int)((int)in.tellg() - curPos);

    // do initial length check
    if(len_remain < IPNDService::MIN_LEN) {
        log_err("Stream does not contain enough data for an IPNDService! [%u]",
                len_remain);
        *num_read = ipndtlv::IPNDSDTLV::ERR_LENGTH;
        return services;
    }
    // else good for now

    uint8_t tag;
    uint64_t len = 0;
    in.seekg(curPos);

    // loop until we get all expected or run out of length
    while(found < expected && len_remain >= IPNDService::MIN_LEN) {
        // record current pos
        curPos = in.tellg();
        // peek at the tag
        in.read((char*)&tag, 1);
        // peek at the length
        result = IPNDAnnouncement::readSDNV(in, (u_int64_t*)&len);

        if(result <= 0) {
            // couldn't read sdnv
            log_err("Failure reading SDNV length value from stream! [%i]",
                    result);
            *num_read = ipndtlv::IPNDSDTLV::ERR_PROCESSING;
            break;
        } else if((1 + result + len) > len_remain) {
            // not enough space left; something's fishy
            log_err("Structure indicates len=%llu but stream only has %u bytes "
                    "remaining", len, len_remain);
            *num_read = ipndtlv::IPNDSDTLV::ERR_LENGTH;
            break;
        } else {
            // try to read a service
            // total size = (tag size) + (sdnv len size) + len
            unsigned int structureLen = 1 + result + (unsigned int)len;
            in.seekg(curPos);
            u_char *buf = new u_char[structureLen];
            memset(buf, 0, structureLen);
            in.read((char*)buf, structureLen);

            // iterate through plugins to extract service
            IPNDService *srvc = NULL;
            PluginIter iter = plugins_.begin();
            while(iter != plugins_.end()) {
                Plugin *curPlug = (*iter);
                srvc = curPlug->readService(tag, (const u_char**)&buf,
                        structureLen, &result);

                if(result != 0) {
                    // plugin recognized service and extracted or failed
                    log_debug("Plugin recognized service");
                    break;
                }
                // else plugin didn't recognize; continue

                iter++;
            }

            // check result
            if(srvc == NULL && result < 0) {
                log_err("Failed to extract service; probable malformed service"
                        " definition! [%i]", result);
                *num_read = result;
                break;
            } else if(srvc == NULL && result == 0) {
                log_info("Service definition not recognized by plugins; "
                        "skipping");
                len_remain -= structureLen;
                totalRead += structureLen;
                found++;
            } else if(srvc != NULL && result > 0) {
                if((unsigned int)result == structureLen) {
                    // everything good
                    log_info("Successfully read service definition");
                    services.push_back(srvc);
                    len_remain -= structureLen;
                    totalRead += structureLen;
                    found++;
                } else {
                    // wonky
                    log_err("Continuity error: expected to read %u bytes, but "
                            "read %u!", structureLen, (unsigned int)result);
                    *num_read = ipndtlv::IPNDSDTLV::ERR_PROCESSING;
                    break;
                }
            } else {
                // should never get here; means srvc != NULL && result <= 0
                log_err("Continuity error: service is defined but reported "
                        "length <= 0! [%i]", result);
                *num_read = ipndtlv::IPNDSDTLV::ERR_PROCESSING;
                break;
            }
        }
    } // end top-level while

    // handle final result
    if(found != expected) {
        // something went wrong along the way; return what we have
        // num_read should be set to some error code already
        log_err("Failed to read the expected number of services! [%u!=%u]",
                found, expected);
        return services;
    } else {
        // should be good
        log_info("Successfully read the expected number of services");
        *num_read = totalRead;
        return services;
    }
}

void IPNDServiceFactory::addPlugin(IPNDServiceFactory::Plugin *plugin) {
    // add to end of list
    plugins_.push_back(plugin);
}

/*** PLUGIN *******************************************************************/

IPNDServiceFactory::Plugin::Plugin(const std::string name)
    : Logger("IPNDServiceFactory::Plugin", "/ipnd-srvc/factory/plugin/%s",
            (name.size() == 0)?"undef":name.c_str()) {
    if(name.size() == 0) {
        log_warn("Constructing IPNDServiceFactory Plugin with undefined name");
    }
}

IPNDServiceFactory::Plugin::~Plugin() {}

/*** DEFAULT PLUGIN ***********************************************************/

DefaultIpndServicePlugin::DefaultIpndServicePlugin()
    : IPNDServiceFactory::Plugin("default") {
    // nop
}

DefaultIpndServicePlugin::~DefaultIpndServicePlugin() {}

IPNDService *DefaultIpndServicePlugin::configureService(const std::string &type,
        const int argc, const char *argv[]) const {
    if(argc < 0 || (unsigned int)argc < MIN_PARAMS) {
        log_info("Not enough parameters to parse [%i<%u]", argc, MIN_PARAMS);
        return NULL;
    }
    // else worth a try

    in_addr_t addr;
    uint16_t port;

    if(type == "cla-tcp-v4" && parseOptions(argc, argv, &addr,
            &port)) {
        return new TcpV4ClaService(addr, port);
    } else if(type == "cla-udp-v4" && parseOptions(argc, argv, &addr,
            &port)) {
        return new UdpV4ClaService(addr, port);
    } else {
        return NULL;
    }
}

IPNDService *DefaultIpndServicePlugin::readService(const uint8_t tag,
        const u_char **buf, const unsigned int len_remain,
        int *num_read) const {

    IPNDService *result = NULL;

    switch(tag) {
    case ipndtlv::IPNDSDTLV::C_CLA_TCPv4:
        result = (IPNDService*)new TcpV4ClaService();
        break;
    case ipndtlv::IPNDSDTLV::C_CLA_UDPv4:
        result = (IPNDService*)new UdpV4ClaService();
        break;
    default:
        *num_read = 0;
        return NULL;
    }

    *num_read = result->read(buf, len_remain);
    if(*num_read < 0) {
        delete result;
        result = NULL;
    } else if(*num_read == 0) {
        delete result;
        result = NULL;
        *num_read = ipndtlv::IPNDSDTLV::ERR_PROCESSING;
    }

    return result;
}

bool DefaultIpndServicePlugin::parseOptions(const int argc, const char *argv[],
        in_addr_t *addr, uint16_t *port) const {

    bool gotPort = false, gotAddr = false, result = true;
    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("addr", addr, "", "", &gotAddr));
    p.addopt(new oasys::UInt16Opt("port", (u_int16_t*)port, "", "", &gotPort));

    char *invalid = NULL;
    if(!p.parse(argc, argv, (const char**)&invalid)) {
        log_err("Failed to parse options on '%s'", invalid);
    }

    if(!gotAddr) {
        log_err("Did not find addr parameter!");
        result = false;
    }

    if(!gotPort) {
        log_err("Did not find port parameter!");
        result = false;
    }

    return result;
}

}
