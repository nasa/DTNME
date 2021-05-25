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

#include "Registration.h"
#include "RegistrationInitialLoadThread.h"

#include "contacts/ContactManager.h"


namespace dtn {


RegistrationInitialLoadThread::RegistrationInitialLoadThread(Registration* registration)
    : Thread("RegistrationInitialLoadThread", DELETE_ON_EXIT )
{
    reg_ = registration;
    reg_->set_initial_load_thread(this);
}

RegistrationInitialLoadThread::~RegistrationInitialLoadThread()
{
}

void 
RegistrationInitialLoadThread::run()
{
    char threadname[16] = "RegLoader";
    pthread_setname_np(pthread_self(), threadname);
   
    //log_always_p("/dtn/reginitload", "run for Reg: *%p", reg_);

    scan_pending_bundles_map();

}


void
RegistrationInitialLoadThread::scan_pending_bundles_map()
{
    // Loop through the pending bundles without keeping it locked to deliver 
    // the appropriate ones to the registration
    BundleDaemon* bd = BundleDaemon::instance();
    pending_bundles_t* pending_bundles = bd->pending_bundles();
    BundleRef bref(__FUNCTION__);
    bundleid_t bundleid = 0;
    u_int32_t count = 0;
    u_int32_t processed = 0;

    log_debug_p("/dtn/reginitload", "scan_pending_bundles_map: begin - pending bundles: %zu", pending_bundles->size());

    pending_bundles->lock()->lock("RegistrationInitialLoadThread::scan_pending_bundles_map");
    pending_bundles_t::iterator iter = pending_bundles->begin();
    if ( iter == pending_bundles->end() ) {
        set_should_stop();
    } else {
        bref = iter->second;
        bref->lock()->lock(__FUNCTION__);
        bundleid = bref->bundleid();
    }
    pending_bundles->lock()->unlock();

    while ( !should_stop() ) {
        ++processed;

        try {
            if (!bref->is_fragment() &&
                reg_->endpoint().match(bref->dest())) {

                /*
                 * Mark the bundle as needing delivery to the registration.
                 * Marking is durable and should be transactionalized together
                 * with storing the bundle payload and metadata to disk if
                 * the storage mechanism supports it (i.e. if transactions are
                 * supported, we should be in one).
                 */
                bref->fwdlog()->add_entry(reg_,
                                            ForwardingInfo::FORWARD_ACTION,
                                            ForwardingInfo::PENDING_DELIVERY);

                bd->deliver_to_registration(bref.object(), reg_, true);
                ++count;
            }

            bref->lock()->unlock();
            bref.release();
        } catch (std::exception& e) {
            log_err_p("/dtn/reginitload/", "Error while scanning bundles to deliver to registration: %s", e.what());
        }

        bref = pending_bundles->find_next(bundleid);

        if (bref != NULL) {
            bref->lock()->lock(__FUNCTION__);
            bundleid = bref->bundleid();
        } else {
            set_should_stop();
        } 
    }

    if (bref != NULL) {
        bref->lock()->unlock();
        bref.release();
    }

    reg_->clear_initial_load_thread(this);

    log_debug_p("/dtn/reginitload", "run: exiting for Reg: *%p - processed: %u  delivered: %u", 
                 reg_, processed, count);
}


} // namespace dtn
