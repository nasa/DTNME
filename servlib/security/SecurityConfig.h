#ifndef SECURITY_CONFIG_H
#define SECURITY_CONFIG_H

#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <vector>
#include <set>
#include <string>
#include <map>
#include <set>
#include <iostream>
#include <sstream>
#include <naming/EndpointID.h>
#include <bundling/BundleProtocol.h>
#include <oasys/serialize/Serialize.h>

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif


#ifdef BSP_ENABLED

using namespace std;
using namespace dtn;
namespace dtn {


class EndpointIDPatternNULL: public oasys::SerializableObject {
    public:
    bool isnull;
    EndpointIDPattern pat;
    EndpointIDPatternNULL();
    EndpointIDPatternNULL(const oasys::Builder b);
    EndpointIDPatternNULL(const string s);
    void serialize(oasys::SerializeAction* a);
    string str();
};
ostream& operator<< (ostream &out, EndpointIDPatternNULL &foo);
class EndpointIDNULL: public oasys::SerializableObject {
    public:
    bool isnull;
    EndpointID pat;
    EndpointIDNULL();
    EndpointIDNULL(const oasys::Builder b);
    EndpointIDNULL(const string s);
    void serialize(oasys::SerializeAction* a);
    string str();
};
ostream& operator<< (ostream &out, EndpointIDNULL &foo);
class IncomingRule {
    public:
      dtn::EndpointIDPattern src;
      dtn::EndpointIDPattern dest;
      EndpointIDPatternNULL secsrc;
      EndpointIDPatternNULL secdest;
    set<int> csnums;
    IncomingRule();
};

ostream& operator<< (ostream &out, IncomingRule &rule);
class OutgoingRule: public oasys::SerializableObject {
    public:
      dtn::EndpointIDPattern src;
      dtn::EndpointIDPattern dest;
      EndpointIDNULL secdest;
    int csnum;
    OutgoingRule(const oasys::Builder b);
    OutgoingRule();
    void serialize(oasys::SerializeAction* a);
};

ostream& operator<< (ostream &out, OutgoingRule &rule);

class SecurityConfig {
  public:
    string privdir;
    string certdir;
    map<int, string> pub_keys_enc;
    map<int, string> pub_keys_ver;
    map<int, string> priv_keys_sig;
    map<int, string> priv_keys_dec;
    oasys::SerializableVector<OutgoingRule> outgoing;
    vector<IncomingRule> incoming;

    // In all of the following, we use the convention that 0 = no ciphersuite.
    SecurityConfig();

    static BundleProtocol::bundle_block_type_t get_block_type(int csnum);
 
    string list_policy();
    string list_maps();

    string replace_stuff(string input, const string node);

    string get_pub_key_enc(const string dest, int cs=3);
    string get_pub_key_ver(const string src, const int csnum);
    string get_priv_key_sig(const string src, const int csnum);
    string get_priv_key_dec(const string dest, const int csnum);


    /**
     * Add the security blocks required by security policy for the
     * given outbound bundle.
     */
    static int prepare_out_blocks(const Bundle* bundle,
                                   const LinkRef& link,
                                   BlockInfoVec* xmit_blocks);

    /**
     * Check whether the processed BSP blocks
     * meet the security policy for this bundle.
     */
    static bool verify_in_policy(const Bundle* bundle);

private:
    static bool verify_one_ciphersuite(set<int> cs, const Bundle *bundle, const BlockInfoVec *recv_blocks,EndpointIDPatternNULL &src, EndpointIDPatternNULL &dest);


};

};
#endif
#endif
