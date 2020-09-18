#ifndef SECURITY_POLICY_H
#define SECURITY_POLICY_H
#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include "oasys/serialize/SerializableVector.h"

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif


#ifdef BSP_ENABLED
#include "SecurityConfig.h"

using namespace std;
using namespace dtn;

class BundleSecurityConfig : public oasys::SerializableObject{
  public:
    oasys::SerializableVector<OutgoingRule> rules;

    BundleSecurityConfig();
    // the default assignment operator actually suffices for this class!
    BundleSecurityConfig(const SecurityConfig *in);
    BundleSecurityConfig(const BundleSecurityConfig &in);
    static BundleProtocol::bundle_block_type_t get_block_type(int csnum);

    bool is_policy_consistant(dtn::EndpointID &src, dtn::EndpointID &dest, dtn::EndpointID &lastsecdestp, dtn::EndpointID &lastsecdeste) const;
    vector<pair<int, EndpointID> > get_outgoing_rules(dtn::EndpointID &src, dtn::EndpointID &dest, dtn::EndpointID &lastsecdestp, dtn::EndpointID &lastsecdeste) const;

    void serialize(oasys::SerializeAction* a);
};
#endif
#endif
