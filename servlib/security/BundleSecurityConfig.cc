#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif


#ifdef BSP_ENABLED
#include "BundleSecurityConfig.h"

using namespace std;
using namespace dtn;
BundleSecurityConfig::BundleSecurityConfig() {
    }
BundleSecurityConfig::BundleSecurityConfig(const SecurityConfig *in):
       rules(oasys::SerializableVector<OutgoingRule>(in->outgoing))
    {
    }
BundleSecurityConfig::BundleSecurityConfig(const BundleSecurityConfig &in): rules(in.rules)
    {
    }
BundleProtocol::bundle_block_type_t BundleSecurityConfig::get_block_type(int csnum) {
        return SecurityConfig::get_block_type(csnum);
        
    }


bool BundleSecurityConfig::is_policy_consistant(dtn::EndpointID &src, dtn::EndpointID &dest, dtn::EndpointID &lastsecdestp, dtn::EndpointID &lastsecdeste) const {
    // What are we checking here?
    // We get a list of security dests we need to add.
    // We have a dest, and a lastsecdest
    // It needs to look like:
    // <list of new sec dests> -> lastsecdest -> dest
    // Our list of new sec dests can't be longer than 2 elements,
    // and if it has two elements, one must be the lastsecdest
    // If dest is in our list, lastsecdest must be dest
    vector<OutgoingRule>::const_iterator it;
    set<pair<EndpointID, int> > pcbdests;
    set<EndpointID> pdests;
    set<EndpointID> edests;
    for(it = rules.begin(); it != rules.end(); it++) {
        if(it->src.match(src) && it->dest.match(dest)) {
            EndpointID secdest;
            if(it->secdest.isnull) {
                secdest = dest;
            } else {
                secdest = it->secdest.pat;
            }
            if(get_block_type(it->csnum) == dtn::BundleProtocol::CONFIDENTIALITY_BLOCK) {
                pcbdests.insert(pair<EndpointID, int>(secdest, it->csnum));
            }
            if(get_block_type(it->csnum) == dtn::BundleProtocol::PAYLOAD_SECURITY_BLOCK ||get_block_type(it->csnum) == dtn::BundleProtocol::CONFIDENTIALITY_BLOCK) {
                pdests.insert(secdest);
            }
            if(get_block_type(it->csnum) == dtn::BundleProtocol::EXTENSION_SECURITY_BLOCK) {
                edests.insert(secdest);
            }
        }
    }
    if(pcbdests.size() > 1) {
        // We can't encrypt to different destinations, because this
        // would require two BEKs.
        return false;
    }
    //  We can handle two security dests if one of them is the
    //  dest
       
    if((pdests.count(lastsecdestp) ==0 && pdests.size() > 1 )
            || pdests.size() > 2) {
        set<EndpointID>::iterator idit;
        for(idit=pdests.begin();idit!=pdests.end();idit++) {
            cout<< idit->str() << "|" <<endl;
        }
        return false;
    }
    if(pdests.count(dest) && lastsecdestp != dest) {
        return false;
    }
    if((edests.count(lastsecdeste) == 0 && edests.size() >1)
            || edests.size() > 2) {
        return false;
    }
    if(edests.count(dest) && lastsecdeste != dest) {
        return false;
    }

    return true;
}


vector<pair<int, EndpointID> > BundleSecurityConfig::get_outgoing_rules(dtn::EndpointID &src, dtn::EndpointID &dest, dtn::EndpointID &lastsecdestp, dtn::EndpointID &lastsecdeste) const {
    // What are the constraints on ordering?
    // BAB happens last
    // ESB/EIB have an order
    // PIB/PCB have an order
    // For PIB/PCB order:
    //    We need to make a list of security destinations.  If
    //    there is one, we add PIB followed by PCB for that dest.
    //    If there is more than one, but one is the last security dest,
    //    we add PIB and PCB for the dest followed by PIB and
    //    PCB for the sec dest
    // For ESB/EIB we do the same.
    vector<OutgoingRule>::const_iterator it;
    vector<pair<int, EndpointID> > matches;
    vector<pair<int, EndpointID> >::iterator mit;
    vector<pair<int, EndpointID> > results;

    for(it = rules.begin(); it != rules.end(); it++) {
        if(it->src.match(src) && it->dest.match(dest)) {
            EndpointID secdest;
            if(it->secdest.isnull) {
                if(get_block_type(it->csnum) == dtn::BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
                    secdest = string("");
                } else {
                    secdest = dest;
                }
            } else {
                secdest = it->secdest.pat;
            }
            matches.push_back(pair<int, dtn::EndpointID>(it->csnum, secdest));
        }
    }
    for(mit = matches.begin(); mit != matches.end(); mit++) {
        if(get_block_type(mit->first) == dtn::BundleProtocol::PAYLOAD_SECURITY_BLOCK
                && mit->second == lastsecdestp) {
            results.push_back(*mit);
        }
    }
    for(mit = matches.begin(); mit != matches.end(); mit++) {
        if(get_block_type(mit->first) == dtn::BundleProtocol::CONFIDENTIALITY_BLOCK
                && mit->second == lastsecdestp) {
            results.push_back(*mit);
        }
    }
    for(mit = matches.begin(); mit != matches.end(); mit++) {
        if(get_block_type(mit->first) == dtn::BundleProtocol::PAYLOAD_SECURITY_BLOCK
                && mit->second != lastsecdestp) {
            results.push_back(*mit);
        }
    }
    for(mit = matches.begin(); mit != matches.end(); mit++) {
        if(get_block_type(mit->first) == dtn::BundleProtocol::CONFIDENTIALITY_BLOCK
                && mit->second != lastsecdestp) {
            results.push_back(*mit);
        }
    }
   for(mit = matches.begin(); mit != matches.end(); mit++) {
        if(get_block_type(mit->first) == dtn::BundleProtocol::EXTENSION_SECURITY_BLOCK
                && mit->second == lastsecdeste) {
            results.push_back(*mit);
        }
    }
   for(mit = matches.begin(); mit != matches.end(); mit++) {
        if(get_block_type(mit->first) == dtn::BundleProtocol::EXTENSION_SECURITY_BLOCK
                && mit->second != lastsecdeste) {
            results.push_back(*mit);
        }
    }
    for(mit = matches.begin(); mit != matches.end(); mit++) {
        if(get_block_type(mit->first) == dtn::BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
            results.push_back(*mit);
        }
    }

    return results;
}
void BundleSecurityConfig::serialize(oasys::SerializeAction* a) { 
        a->process("rules", &rules);
    }


#endif
