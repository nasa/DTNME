/*
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "SPD.h"
#include "bundling/BundleDaemon.h"
#include "Ciphersuite.h"
#include "Ciphersuite_BA1.h"
#include "Ciphersuite_PI2.h"
#include "Ciphersuite_PC3.h"
#include "Ciphersuite_ES4.h"
#include "SecurityPolicy.h"

template <>
dtn::SPD* oasys::Singleton<dtn::SPD, false>::instance_ = NULL;

namespace dtn {

static const char * log = "/dtn/bundle/security";

SPD::SPD() {
}

SPD::~SPD()
{
}

void
SPD::init()
{       
    if (instance_ != NULL) 
    {
        PANIC("SPD already initialized");
    }
    
    instance_ = new SPD();
	log_debug_p(log, "SPD::init() done");
}

int
SPD::prepare_out_blocks(const Bundle* bundle, const LinkRef& link,
                    BlockInfoVec* xmit_blocks)
{
    std::string bundle_src_str = bundle->source().uri().scheme() + "://" +
                                 bundle->source().uri().host();
    EndpointID        src_node(bundle_src_str);
    vector<int> bps = bundle->security_policy().get_out_bps();
    vector<int>::iterator it;
    Ciphersuite *bp;


    for(it=bps.begin();it!=bps.end();it++) {

        if(src_node != BundleDaemon::instance()->local_eid() && *it != 1) {
            log_debug_p(log, "SPD::prepare_out_blocks() not adding security block %d to forwarded node", *it);
        } else {
        bp = Ciphersuite::find_suite(*it);
        if(bp == NULL) {
            log_err_p(log, "SPD::prepare_out_blocks() couldn't find ciphersuite %d which our current policy requires.  Therefore, we are not going to send this bundle.", *it);
            goto fail;
        }

        if(BP_FAIL == bp->prepare(bundle, xmit_blocks, NULL, link,
                    BlockInfo::LIST_NONE)) {
            log_err_p(log, "SPD::prepare_out_blocks() Ciphersuite number %d->prepare returned BP_FAIL", bp->cs_num());
            goto fail;
        }
        }
    }

	log_debug_p(log, "SPD::prepare_out_blocks() done");
    return BP_SUCCESS;
fail:
    return BP_FAIL;
}

bool SPD::verify_one_ciphersuite(set<int> *cs, const Bundle *bundle, const BlockInfoVec *recv_blocks) {
    set<int>::iterator it;
  if(cs->count(0) > 0) {
        log_debug_p(log, "Allowing bundle because 0 is in the list of allowed ciphersuites");
        return true;
    } else {
        for(it=cs->begin();it!=cs->end();it++) {
            if(Ciphersuite::check_validation(bundle, recv_blocks, *it)) {
                log_debug_p(log, "Allowing bundle because cs=%d say's it is ok", *it);
                return true;
            }
        }
    }
    return false;
}


bool
SPD::verify_in_policy(const Bundle* bundle)
{
    std::string bundle_dest_str = bundle->dest().uri().scheme() + "://" +
                                 bundle->dest().uri().host();
    EndpointID        dest_node(bundle_dest_str);
    set<int> *cs;
    const BlockInfoVec* recv_blocks = &bundle->recv_blocks();
    cs = Ciphersuite::config->get_allowed_babs();
    if(!verify_one_ciphersuite(cs,bundle, recv_blocks)) {
        log_debug_p(log, "BAB disallowing bundle");
        return false;
    }

    if(dest_node != BundleDaemon::instance()->local_eid()) {
        log_debug_p(log, "SPD::verify_in_policy() quiting after BAB check because we're just forwarding");
        return true;
    }
    cs = Ciphersuite::config->get_allowed_pibs();
    if(!verify_one_ciphersuite(cs, bundle, recv_blocks)) {
        log_debug_p(log, "PIB disallowing bundle");
        return false;
    }
    cs = Ciphersuite::config->get_allowed_pcbs();
    if(!verify_one_ciphersuite(cs, bundle, recv_blocks)) {
        log_debug_p(log, "PCB disallowing bundle");
        return false;
    }

    return true;
}

} // namespace dtn

#endif  /* BSP_ENABLED */
