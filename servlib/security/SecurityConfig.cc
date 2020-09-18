
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "SecurityConfig.h"
#ifdef BSP_ENABLED
#include "Ciphersuite.h"
#include "BP_Local_CS.h"
#include "bundling/Bundle.h"

using namespace std;

namespace dtn {

static const char * log = "/dtn/bundle/security";

ostream& operator<< (ostream &out, EndpointIDPatternNULL &foo) {
    if(foo.isnull) {
        out << "NULL";
    } else {
        out << foo.pat.str();
    }
    return out;
}
ostream& operator<< (ostream &out, EndpointIDNULL &foo) {
    if(foo.isnull) {
        out << "NULL";
    } else {
        out << foo.pat.str();
    }
    return out;
}

ostream& operator<< (ostream &out, IncomingRule &rule) {
    out << rule.src.str() << " " << rule.dest.str() << " " << rule.secsrc << " " << rule.secdest<< " ";
    set<int>::iterator it;
    for(it=rule.csnums.begin();it!= rule.csnums.end();it++) {
        out << *it << " ";
    }
    return out;
}

ostream& operator<< (ostream &out, OutgoingRule &rule) {
    out << rule.src.str() << " " << rule.dest.str() << " " << rule.secdest<< " " << rule.csnum;
    return out;
}

EndpointIDPatternNULL::EndpointIDPatternNULL() {
        isnull = true;
}
EndpointIDPatternNULL::EndpointIDPatternNULL(const oasys::Builder b) {
        (void) b;
    }
EndpointIDPatternNULL::EndpointIDPatternNULL(const string s) {
        if(s == "NULL") {
            isnull = true;
        } else {
            pat = EndpointIDPattern(s);
            isnull = false;
        }
    }
void EndpointIDPatternNULL::serialize(oasys::SerializeAction* a) {
        a->process("isnull", &isnull);
        a->process("pat", &pat);
    }
string EndpointIDPatternNULL::str() {
         if(isnull) {
             return string("NULL");
         } else {
             return pat.str();
         }
    }

EndpointIDNULL::EndpointIDNULL() {
        isnull = true;
    }
EndpointIDNULL::EndpointIDNULL(const oasys::Builder b) {
        (void) b;
    }
EndpointIDNULL::EndpointIDNULL(const string s) {
        if(s == "NULL") {
            isnull = true;
        } else {
            pat = EndpointID(s);
            isnull = false;
        }
    }
void EndpointIDNULL::serialize(oasys::SerializeAction* a) {
        a->process("isnull", &isnull);
        a->process("pat", &pat);
    }
string EndpointIDNULL::str() {
        if(isnull) {
            return string("NULL");
        } else {
            return pat.str();
        }
    }

IncomingRule::IncomingRule() {
}

OutgoingRule::OutgoingRule(const oasys::Builder b) {
        (void) b;
    }
OutgoingRule::OutgoingRule() {
    }
void OutgoingRule::serialize(oasys::SerializeAction* a) {
        a->process("src", &src);
        a->process("dest", &dest);
        a->process("secdest", &secdest);
        a->process("csnum", &csnum);
    }


SecurityConfig::SecurityConfig(): privdir("/usr/local/ssl/private/"), certdir("/usr/local/ssl/certs/"){
        // define which keys are used with each ciphersuite
        // PIB-RSA-SHA256: ciphersuite value: 0x02
        pub_keys_ver[2] = string("RSA_sig_cert_<NODE>.pem");
        priv_keys_sig[2] = string("RSA_sig_priv_key.pem");

        // PCB-RSA-AES128-PAYLOAD-PIB-PCB: ciphersuite value: 0x03
        pub_keys_enc[3] = string("RSA_enc_cert_<NODE>.pem");
        priv_keys_dec[3] = string("RSA_enc_priv_key.pem");

        // ESB-RSA-AES128-EXT: ciphersuite value: 0x04
        pub_keys_enc[4] = string("RSA_enc_cert_<NODE>.pem");
        priv_keys_dec[4] = string("RSA_enc_priv_key.pem");

        // PIB-ECDSA-SHA256: ciphersuite value: 0x06
        pub_keys_ver[6] = string("EC_sig_cert1_<NODE>.pem");
        priv_keys_sig[6] = string("EC_sig_priv_key1.pem");

        // PCB-ECDH-SHA256-AES128: ciphersuite value: 0x07
        pub_keys_enc[7] = string("EC_enc_cert1_<NODE>.pem");
        priv_keys_dec[7] = string("EC_enc_priv_key1.pem");

        // ESB-ECDH-SHA256-AES128: ciphersuite value: 0x08
        pub_keys_enc[8] = string("EC_enc_cert1_<NODE>.pem");
        priv_keys_dec[8] = string("EC_enc_priv_key1.pem");

        // PIB-ECDSA-SHA384: ciphersuite value: 0x0A
        pub_keys_ver[10] = string("EC_sig_cert2_<NODE>.pem");
        priv_keys_sig[10] = string("EC_sig_priv_key2.pem");

        // PCB-ECDH-SHA384-AES256: ciphersuite value: 0x0B
        pub_keys_enc[11] = string("EC_enc_cert2_<NODE>.pem");
        priv_keys_dec[11] = string("EC_enc_priv_key2.pem");

        // ESB-ECDH-SHA384-AES256: ciphersuite value: 0x0C
        pub_keys_enc[12] = string("EC_enc_cert2_<NODE>.pem");
        priv_keys_dec[12] = string("EC_enc_priv_key2.pem");

        // EIB-RSA-SHA256:  ciphersuite value: 0x11
        pub_keys_ver[17] = string("RSA_sig_cert_<NODE>.pem");
        priv_keys_sig[17] = string("RSA_sig_priv_key.pem");
    };

BundleProtocol::bundle_block_type_t SecurityConfig::get_block_type(int csnum) {
        
        if(csnum == 1 || csnum == 5 || csnum == 9) {
            return dtn::BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK;
        } else if(csnum == 2 || csnum == 6 || csnum == 10) {
            return dtn::BundleProtocol::PAYLOAD_SECURITY_BLOCK;
        } else if(csnum == 3 || csnum == 7 || csnum == 11) {
            return dtn::BundleProtocol::CONFIDENTIALITY_BLOCK;
        } else if(csnum == 4 || csnum == 8 || csnum == 12) {
            return dtn::BundleProtocol::EXTENSION_SECURITY_BLOCK;
        }
        return dtn::BundleProtocol::UNKNOWN_BLOCK;
    }
 
string SecurityConfig::list_policy() {
        stringstream foo(stringstream::out);
        vector<IncomingRule>::iterator it;
        vector<OutgoingRule>::iterator it2;
        foo << "Incoming rules" << endl;
        foo << "<rulenum> <src pattern> <dest pattern> <secsrc pattern|NULL> <secdest pattern|NULL> <csnums>" <<endl;
        int i=0;
        for(it = incoming.begin();it!=incoming.end();it++) {
            foo << i << " " << *it << endl;
            i++;
        }
        i=0;
        foo << "Outgoing rules" <<endl;
        foo << "<rulenum> <src pattern> <dest pattern> <secdest|NULL> <csnum>"<<endl;
        for(it2 = outgoing.begin();it2!=outgoing.end();it2++) {
            foo <<i << " " << *it2 <<endl;
            i++;
        }
        

        return foo.str();
    }
string SecurityConfig::list_maps() {
       stringstream foo(stringstream::out);
       map<int, string>::iterator it;

       foo << "certdir = " << certdir << endl;
       foo << "privdir = " << privdir << endl << endl;

       foo << "pub_keys_enc:" <<endl;
       for(it = pub_keys_enc.begin(); it != pub_keys_enc.end();it++) {
           foo << "(cs num " << (*it).first << ")" << " => " << (*it).second << endl;
       }
       foo << endl;

       foo << "priv_keys_dec:" <<endl;
       for(it = priv_keys_dec.begin(); it != priv_keys_dec.end();it++) {
           foo << "(cs num " << (*it).first << ")" << " => " << (*it).second << endl;
       }
       foo << endl;

       foo << "pub_keys_ver:" << endl;
       for(it = pub_keys_ver.begin(); it != pub_keys_ver.end();it++) {
           foo << "(cs num " << (*it).first << ")"  << " => " << (*it).second << endl;
       }
       foo << endl;

       foo << "priv_keys_sig:" << endl;
       for(it = priv_keys_sig.begin(); it != priv_keys_sig.end();it++) {
           foo << "(cs num " << (*it).first <<  ")" << " => " << (*it).second << endl;
       }
       foo << endl;

       return foo.str();
    }
string SecurityConfig::replace_stuff(string input, const string node) {
        string n = string("<NODE>");
        if(input.find(n) != string::npos) {
            input.replace(input.find(n),n.length(),node);
        }
        return input;
    }



string SecurityConfig::get_pub_key_enc(const string dest, int cs) {
            return certdir + replace_stuff(pub_keys_enc[cs], dest);
    }
string SecurityConfig::get_pub_key_ver(const string src, const int csnum) {
            return certdir + replace_stuff(pub_keys_ver[csnum], src);
    }
string SecurityConfig::get_priv_key_sig(const string src, const int csnum) {
            return privdir + replace_stuff(priv_keys_sig[csnum], src);
    }
string SecurityConfig::get_priv_key_dec(const string dest, const int csnum) {
            return privdir + replace_stuff(priv_keys_dec[csnum], dest);
    }


int
SecurityConfig::prepare_out_blocks(const Bundle* bundle, const LinkRef& link,
                    BlockInfoVec* xmit_blocks)
{
    std::string bundle_src_str = bundle->source().uri().scheme() + "://" +
                                 bundle->source().uri().host();
    EndpointID        src_node(bundle_src_str);
    std::string bundle_dest_str = bundle->dest().uri().scheme() + "://" +
                                 bundle->dest().uri().host();
    EndpointID        dest_node(bundle_dest_str);
    vector<pair<int, EndpointID> > bps;
    vector<pair<int, EndpointID> >::iterator it;
    Ciphersuite *bp;

	log_debug_p(log, "SecurityConfig::prepare_out_blocks() begin");
    EndpointID lastsecdeste = Ciphersuite::find_last_secdeste(bundle);
    EndpointID lastsecdestp = Ciphersuite::find_last_secdestp(bundle);
    if(!bundle->security_config().is_policy_consistant(src_node, dest_node, lastsecdestp, lastsecdeste)) {
        log_err_p(log, "SecurityConfig::prepare_out_blocks dropping bundle because policy is inconsistant");
        goto fail;
    } else {
        log_debug_p(log, "SecurityConfig::prepare_out_blocks security policy is consistant");
    }
    bps  = bundle->security_config().get_outgoing_rules(src_node, dest_node, lastsecdestp, lastsecdeste);
    for(it=bps.begin();it!=bps.end();it++) {

        bp = Ciphersuite::find_suite(it->first);
        if(bp == NULL) {
            log_err_p(log, "SecurityConfig::prepare_out_blocks() couldn't find ciphersuite %d which our current policy requires.  Therefore, we are not going to send this bundle.", it->first);
            goto fail;
        }
        // Tricky, tricky.  We must make a fake source to fool the
        // block processor into using our security dest
        log_debug_p(log, "SecurityConfig::prepare_out_blocks calling prepare on %d with security_dest=%s",  bp->cs_num(), (it->second).str().c_str());
        BlockInfo temp(BundleProtocol::find_processor(Ciphersuite::config->get_block_type(it->first)));
        bp->init_locals(&temp);
        dynamic_cast<BP_Local_CS*>(temp.locals())->set_security_dest((it->second).str());
        if(BP_FAIL == bp->prepare(bundle, xmit_blocks, &temp, link,
                    BlockInfo::LIST_NONE)) {
            log_err_p(log, "SecurityConfig::prepare_out_blocks() Ciphersuite number %d->prepare returned BP_FAIL", bp->cs_num());
            goto fail;
        }
    }

	log_debug_p(log, "SecurityConfig::prepare_out_blocks() done");
    return BP_SUCCESS;
fail:
    return BP_FAIL;
}

bool SecurityConfig::verify_one_ciphersuite(set<int> cs, const Bundle *bundle, const BlockInfoVec *recv_blocks,EndpointIDPatternNULL &src, EndpointIDPatternNULL &dest) {
    set<int>::iterator it;
  if(cs.count(0) > 0) {
        // Why is this here anymore?
        log_debug_p(log, "Allowing bundle because 0 is in the list of allowed ciphersuites");
        return true;
    } else {
        for(it=cs.begin();it!=cs.end();it++) {
            if(Ciphersuite::check_validation(bundle, recv_blocks, *it,src, dest)) {
                log_debug_p(log, "Allowing bundle because cs=%d say's it is ok", *it);
                return true;
            } else {
                log_debug_p(log, "cs=%d won't ok the bundle", *it);
            }
        }
    }
    return false;
}


bool
SecurityConfig::verify_in_policy(const Bundle* bundle)
{
    std::string bundle_dest_str = bundle->dest().uri().scheme() + "://" +
                                 bundle->dest().uri().host();
    EndpointID        dest_node(bundle_dest_str);
    std::string bundle_src_str = bundle->source().uri().scheme() + "://" +
                                 bundle->source().uri().host();
    EndpointID        src_node(bundle_src_str);
    const BlockInfoVec* recv_blocks = &bundle->recv_blocks();
    vector<IncomingRule>::iterator it;
    for(it= Ciphersuite::config->incoming.begin();it != Ciphersuite::config->incoming.end();it++) {
        if(it->src.match(src_node) && it->dest.match(dest_node)) {
            std::stringstream foo(stringstream::out);
            foo << *it << endl;
            log_debug_p(log, "Considering rule %s", foo.str().c_str());
            if(!verify_one_ciphersuite(it->csnums, bundle, recv_blocks, it->secsrc, it->secdest)) {
                return false;
            }
        } else {
            log_debug_p(log, "Ignoring rule because bundle src/dest doesn't match");
        }
    }

    return true;
}

};

#endif
