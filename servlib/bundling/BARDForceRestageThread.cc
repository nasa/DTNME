/*
 *    Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BARD_ENABLED

#include "BARDForceRestageThread.h"
#include "BundleDaemon.h"
#include "naming/DTNScheme.h"
#include "naming/EndpointID.h"
#include "naming/IMCScheme.h"
#include "naming/IPNScheme.h"

namespace dtn {


BARDForceRestageThread::BARDForceRestageThread(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme,
                                             std::string& nodename, uint64_t node_number,
                                             size_t bundles_to_restage, size_t bytes_to_restage,
                                             std::string restage_link_name)
    : Thread("BARDForceRestageThread", DELETE_ON_EXIT ),
      quota_type_(quota_type),
      scheme_(scheme),
      nodename_(nodename),
      node_number_(node_number),
      bundles_to_restage_(bundles_to_restage),
      bytes_to_restage_(bytes_to_restage),
      restage_link_name_(restage_link_name)
{
}

BARDForceRestageThread::~BARDForceRestageThread()
{
}

void 
BARDForceRestageThread::run()
{
    char threadname[16] = "ForceRestage";
    pthread_setname_np(pthread_self(), threadname);
   
    scan_pending_bundles_map();
}


void
BARDForceRestageThread::scan_pending_bundles_map()
{
    BundleDaemon* bd = BundleDaemon::instance();

    size_t num_restaged = 0;
    size_t bytes_restaged = 0;
    size_t bundles_processed = 0;

    // Loop through the pending bundles without keeping it locked to 
    // find bundles that need to be restaged
    int action = ForwardingInfo::FORWARD_ACTION;
    BundleRef bref(__func__);
    bundleid_t bundleid = UINT64_MAX;

    pending_bundles_t* pending_bundles = bd->pending_bundles();

    bref = pending_bundles->find_prev(bundleid);

    while (!bd->shutting_down() && (bref != nullptr) && 
           ((num_restaged < bundles_to_restage_) || (bytes_restaged < bytes_to_restage_))) {

        ++bundles_processed;

        bref->lock()->lock(__func__);

        bundleid = bref->bundleid();

        if (is_bundle_to_restage(bref)) {

            BundleSendRequest* event_to_post;
            event_to_post = new BundleSendRequest(bref, restage_link_name_, action, true);
            SPtr_BundleEvent sptr_event_to_post(event_to_post);
            BundleDaemon::post(sptr_event_to_post);

            num_restaged   += 1;
            bytes_restaged += bref->payload().length();
        }

        bref->lock()->unlock();

        bref = pending_bundles->find_prev(bundleid);
    }

    log_info_p("/bard/forcerestage", "restaging %s %s %s -  processed: %zu  restaged: %zu bundles with %zu bytes", 
                 bard_quota_type_to_str(quota_type_), bard_naming_scheme_to_str(scheme_), nodename_.c_str(),
                 bundles_processed, num_restaged, bytes_restaged);
}

bool
BARDForceRestageThread::is_bundle_to_restage(BundleRef& bref)
{
    bool do_restage = false;
    bool restage_by_src = false;

    if (quota_type_ == BARD_QUOTA_TYPE_DST) {
        do_restage = is_bundle_to_restage(bref->dest());
    } else if (quota_type_ == BARD_QUOTA_TYPE_SRC) {
        restage_by_src = true;
        do_restage = is_bundle_to_restage(bref->source());
    }


    if (do_restage) {
        // set the restage info in the bundle
        bref->set_bard_restage_by_src(restage_by_src);
        bref->set_bard_restage_link_name(restage_link_name_);
    }

    return do_restage;
}


bool
BARDForceRestageThread::is_bundle_to_restage(const SPtr_EID& sptr_eid)
{
    bool do_restage = false;

    switch (scheme_) {
        case BARD_QUOTA_NAMING_SCHEME_IPN:
            do_restage = (sptr_eid->valid() && sptr_eid->is_ipn_scheme());
            if (do_restage) {
                do_restage = (sptr_eid->node_num() == node_number_);
            }
            break;

        case BARD_QUOTA_NAMING_SCHEME_DTN:
            do_restage = (sptr_eid->scheme() == DTNScheme::instance());
            if (do_restage) {
                do_restage = (nodename_.compare(sptr_eid->ssp()) == 0);
            }
            break;

        case BARD_QUOTA_NAMING_SCHEME_IMC:
            do_restage = (sptr_eid->valid() && sptr_eid->is_imc_scheme());
            if (do_restage) {
                do_restage = (sptr_eid->node_num() == node_number_);
            }
            break;

        default:
            break;
    }

    return do_restage;
}




} // namespace dtn


#endif // BARD_ENABLED
