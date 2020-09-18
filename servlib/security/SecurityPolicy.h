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


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif


#ifdef BSP_ENABLED
#include "SecurityConfig.h"

using namespace std;

class SecurityPolicy : public oasys::SerializableObject{
  public:
    // In all of the following, we use the convention that 0=no ciphersuite.
    int pcb_out;
    int pib_out;
    int bab_out;
    int esb_out;

    SecurityPolicy() {
        pcb_out = 0;
        pib_out = 0;
        bab_out = 0;
        esb_out = 0;
    };
    // Note: astonishingly enough, the default assignment operator
    // actually suffices for this class!
    SecurityPolicy(const SecurityConfig *in): pcb_out(in->pcb_out), pib_out(in->pib_out), bab_out(in->bab_out), esb_out(in->esb_out) {
    };
    SecurityPolicy(const SecurityPolicy &in): pcb_out(in.pcb_out), pib_out(in.pib_out), bab_out(in.bab_out), esb_out(in.esb_out) {
    };

    void serialize(oasys::SerializeAction* a) {
        a->process("pcb_out", &pcb_out);
        a->process("pib_out", &pib_out);
        a->process("bab_out", &bab_out);
        a->process("esb_out", &esb_out);
    };

    vector<int> get_out_bps() const {
       vector<int> res;
       if(pib_out != 0) {
           res.push_back(pib_out);
       }
       if(pcb_out != 0) {
           res.push_back(pcb_out);
       }
       if(esb_out != 0) {
           res.push_back(esb_out);
       }
       if(bab_out != 0) {
           res.push_back(bab_out);
       }
       return res;
    };
};
#endif
#endif
