/*
 *    Copyright 2004-2006 Intel Corporation
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

/*
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#include <third_party/oasys/util/OptParser.h>

#include "Link.h"
#include "ContactManager.h"
#include "AlwaysOnLink.h"
#include "OndemandLink.h"
#include "ScheduledLink.h"
#include "OpportunisticLink.h"

#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"
#include "conv_layers/ConvergenceLayer.h"
#include "naming/EndpointIDOpt.h"
#include "routing/RouterInfo.h"
#include "storage/LinkStore.h"

namespace dtn {

//----------------------------------------------------------------------
/// Default parameters, values overridden in ParamCommand
Link::Params::Params()
    : mtu_(0),
      min_retry_interval_(5),
      max_retry_interval_(10 * 60),
      idle_close_time_(0),
      potential_downtime_(30),
      prevhop_hdr_(true),
      cost_(100),
      qlimit_enabled_(false),
      qlimit_bundles_high_(10),
      qlimit_bytes_high_(1024*1024), // 1M
      qlimit_bundles_low_(5),
      qlimit_bytes_low_(512*1024) // 512K
{}

Link::Params Link::default_params_;

//----------------------------------------------------------------------
LinkRef
Link::create_link(const std::string& name, link_type_t type,
                  ConvergenceLayer* cl, const char* nexthop,
                  int argc, const char* argv[],
                  const char** invalid_argp)
{
    LinkRef link("Link::create_link: return value");
    switch(type) {
    case ALWAYSON:      link = new AlwaysOnLink(name, cl, nexthop); break;
    case ONDEMAND:      link = new OndemandLink(name, cl, nexthop); break;
    case SCHEDULED:     link = new ScheduledLink(name, cl, nexthop); break;
    case OPPORTUNISTIC: link = new OpportunisticLink(name, cl, nexthop); break;
    default:            PANIC("bogus link_type_t");
    }

    // hook for the link subclass that parses any arguments and shifts
    // argv appropriately
    int count = link->parse_args(argc, argv, invalid_argp);
    if (count == -1) {
        link->deleted_ = true;
        link = nullptr;
        return link;
    }

    argc -= count;

    // XXX/demmer need to pass invalid_argp to the convergence layer

    // notify the convergence layer, which parses the rest of the
    // arguments
    ASSERT(link->clayer_);
    link->cl_name_ = std::string(link->clayer_->name());
    if (!link->clayer_->init_link(link, argc, argv)) {
        link->deleted_ = true;
        link = nullptr;
        return link;
    }
    
    link->logf(oasys::LOG_INFO, "new link *%p", link.object());

    // now dispatch to the subclass for any initial state events that
    // need to be posted. this needs to be done after all the above is
    // completed to avoid potential race conditions if the core of the
    // system tries to use the link before its completely created
    // MOVED to ContactManager::handle_link_created()
    // link->set_initial_state();

    return link;
}

//----------------------------------------------------------------------
Link::Link(const std::string& name, link_type_t type,
           ConvergenceLayer* cl, const char* nexthop)
    : RefCountedObject("/dtn/link/refs"),
      Logger("Link", "/dtn/link/%s", name.c_str()),
      type_(type),
      state_(UNAVAILABLE),
      deleted_(false),
      create_pending_(false),
      usable_(true),
      nexthop_(nexthop),
      name_(name),
      reliable_(false),
      lock_(),
      queue_(name + ":queue", &lock_),
      inflight_(name + ":inflight", &lock_),
      bundles_queued_(0),
      bytes_queued_(0),
      bundles_inflight_(0),
      bytes_inflight_(0),
      contact_("Link"),
      clayer_(cl),
      cl_name_(cl->name()),
      cl_info_(nullptr),
      router_info_(nullptr),
      remote_eid_(EndpointID::NULL_EID()),
      reincarnated_(false),
      used_in_fwdlog_(false),
      in_datastore_(false),
      deferred_bundle_count_(0),
      deferred_timer_(nullptr),
      contact_timer_(nullptr),
      contact_state_(0)
{
    ASSERT(clayer_);
    params_         = default_params_;

    retry_interval_ = 0; // set in ContactManager

    memset(&stats_, 0, sizeof(Stats));
}

//----------------------------------------------------------------------
Link::Link(const oasys::Builder&)
    : RefCountedObject("/dtn/link/refs"),
      Logger("Link", "/dtn/link/UNKNOWN!!!"),
      type_(LINK_INVALID),
      state_(UNAVAILABLE),
      deleted_(false),
      create_pending_(false),
      usable_(false),
      nexthop_(""),
      name_(""),
      reliable_(false),
      lock_(),
      queue_("", &lock_),
      inflight_("", &lock_),
      bundles_queued_(0),
      bytes_queued_(0),
      bundles_inflight_(0),
      bytes_inflight_(0),
      contact_("Link"),
      clayer_(nullptr),
      cl_name_(""),
      cl_info_(nullptr),
      router_info_(nullptr),
      remote_eid_(EndpointID::NULL_EID()),
      reincarnated_(false),
      used_in_fwdlog_(false),
      in_datastore_(false)
{
}

//----------------------------------------------------------------------
void
Link::delete_link()
{
    oasys::ScopeLock l(&lock_, "Link::delete_link");

    ASSERT(!isdeleted());
    ASSERT(clayer_ != nullptr);

    clayer_->delete_link(LinkRef(this, "Link::delete_link"));
    deleted_ = true;
}

//----------------------------------------------------------------------
void
Link::cancel_all_bundles()
{
    oasys::ScopeLock l(&lock_, "Link::cancel_all_bundles");

    size_t payload_len;
    LinkRef lref = LinkRef(this, "Link::clear_bundle");


    // clear inflight bundles
    BundleRef bref = inflight_.pop_front();
    while (bref != nullptr) {
        //log_debug("Attempting to Clear Queue for Bundle %" PRIbid, bref->bundleid());

        ASSERT(bundles_inflight_ > 0);
        bundles_inflight_--;

        payload_len = bref->payload().length();
    
        // sanity checks
        if (bytes_inflight_ >= payload_len) {
            bytes_inflight_ -= payload_len;
    
        } else {
            log_err("cancel_all_bundles: *%p bytes_inflight %zu < payload_len %zu",
                    bref.object(), bytes_inflight_, payload_len);
        }

        BundleDaemon::post(new BundleSendCancelledEvent(bref.object(), lref));
        bref = inflight_.pop_front();
    }


    // clear queued bundles
    bref = queue_.pop_front();

    while (bref != nullptr) {
        log_info("Attempting to Clear Queue for Bundle %" PRIbid, bref->bundleid());

        ASSERT(bundles_queued_ > 0);
        bundles_queued_--;
    
        payload_len = bref->payload().length();

        // sanity checks
//        ASSERT(payload_len != 0);
        if (bytes_queued_ >= payload_len) {
            bytes_queued_ -= payload_len;
    
        } else {
            log_err("cancel_all_bundles: *%p bytes_queued %zu < payload_len %zu",
                    bref.object(), bytes_queued_, payload_len);
        }
    
        //BundleDaemon::post(new BundleTransmittedEvent(bref.object(), contact(), lref, 0, 0, false));
        BundleDaemon::post(new BundleSendCancelledEvent(bref.object(), lref));
        bref = queue_.pop_front();
    }
}

//----------------------------------------------------------------------
bool
Link::isdeleted() const
{
    oasys::ScopeLock l(&lock_, "Link::delete_link");
    return deleted_;
}

//----------------------------------------------------------------------
void
Link::set_used_in_fwdlog()
{
    if (used_in_fwdlog_)
    {
        return;
    }

    oasys::ScopeLock l(&lock_, "Link::set_used_in_fwdlog");

    // Update persistent storage if changing value
    if (!used_in_fwdlog_)
    {
        oasys::DurableStore *store = oasys::DurableStore::instance();
        if (!store->is_transaction_open())
        {
        	store->begin_transaction();
        }

        used_in_fwdlog_ = true;

        // Add (or update) this Link to the persistent store
        BundleDaemon::post(new StoreLinkUpdateEvent(this));
    }

}

//----------------------------------------------------------------------
bool
Link::reconfigure_link(int argc, const char* argv[])
{
    oasys::ScopeLock l(&lock_, "Link::reconfigure_link");

    if (isdeleted()) {
        log_debug("Link::reconfigure_link: "
                  "cannot reconfigure deleted link %s", name());
        return false;
    }

    ASSERT(clayer_ != nullptr);
    return clayer_->reconfigure_link(LinkRef(this, "Link::reconfigure_link"),
                                     argc, argv);
}

//----------------------------------------------------------------------
void
Link::reconfigure_link(AttributeVector& params)
{
    oasys::ScopeLock l(&lock_, "Link::reconfigure_link");

    if (isdeleted()) {
        log_debug("Link::reconfigure_link: "
                  "cannot reconfigure deleted link %s", name());
        return;
    }

    AttributeVector::iterator iter;
    for (iter = params.begin(); iter != params.end(); ) {
        if (iter->name() == "is_usable") {
            if (iter->bool_val()) {
                set_usable(true);
            } else {
                set_usable(false);
            }
            ++iter;

        } else if (iter->name() == "nexthop") {
            set_nexthop(iter->string_val());
            ++iter;

        // Following are DTNME parameters not listed in the DP interface.
        } else if (iter->name() == "min_retry_interval") {
            params_.min_retry_interval_ = iter->u_int_val();
            iter = params.erase(iter);

        } else if (iter->name() == "max_retry_interval") {
            params_.max_retry_interval_ = iter->u_int_val();
            iter = params.erase(iter);

        } else if (iter->name() == "idle_close_time") {
            params_.idle_close_time_ = iter->u_int_val();
            iter = params.erase(iter);
            
        } else if (iter->name() == "potential_downtime") {
            params_.potential_downtime_ = iter->u_int_val();
            iter = params.erase(iter);

        } else {
            ++iter;
        }
    }
    
    ASSERT(clayer_ != nullptr);
    return clayer_->reconfigure_link(
                        LinkRef(this, "Link::reconfigure_link"), params);
}

//----------------------------------------------------------------------
void
Link::serialize(oasys::SerializeAction* a)
{
    //std::string cl_name;
    std::string type_str;

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        a->process("type",     &type_str);
        type_ = str_to_link_type(type_str.c_str());
        ASSERT(type_ != LINK_INVALID);
    } else {
        type_str = link_type_to_str(type());
        a->process("type",     &type_str);
    }
    
    a->process("nexthop",  &nexthop_);
    a->process("name",     &name_);
    a->process("state",    &state_);
    a->process("deleted",  &deleted_);
    a->process("usable",   &usable_);
    a->process("reliable", &reliable_);
    a->process("used_in_fwdlog", &used_in_fwdlog_);


    /**
     * The persistent store just records the name of the convergence layer.
     * Initially we are just using this for checking manually created links
     * for consistency.  If the convergence layer is not available when
     * restarting we just leave the clayer nullptr.  the cl_info needs more
     * work because the connection convergence layer doesn't have a serialize
     * routine for the link parameters (yet).
     */
    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        a->process("clayer", &cl_name_);
        clayer_ = ConvergenceLayer::find_clayer(cl_name_.c_str());
        ASSERT(clayer_);
        cl_info_ = clayer_->new_link_params();
        a->process("cl_info", cl_info_);
    } else {
        a->process("clayer", &cl_name_);
        a->process("cl_info", cl_info_);
#if 0
        if (state_ == OPEN)
            a->process("clinfo", contact_->cl_info());
#endif
    }

    // XXX/demmer router_info_??

    a->process("remote_eid",         &remote_eid_);
    a->process("min_retry_interval", &params_.min_retry_interval_);
    a->process("max_retry_interval", &params_.max_retry_interval_);
    a->process("idle_close_time",    &params_.idle_close_time_);
    a->process("potential_downtime", &params_.potential_downtime_);
    a->process("cost",               &params_.cost_);
    a->process("bp6_redirect",       &params_.bp6_redirect_);
    a->process("bp7_redirect",       &params_.bp7_redirect_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        logpathf("/dtn/link/%s", name_.c_str());
    }
}

//----------------------------------------------------------------------
int
Link::parse_args(int argc, const char* argv[], const char** invalidp)
{
    oasys::OptParser p;

    p.addopt(new dtn::EndpointIDOpt("remote_eid", &remote_eid_));
    p.addopt(new oasys::BoolOpt("reliable", &reliable_));
    p.addopt(new oasys::StringOpt("nexthop", &nexthop_));
    p.addopt(new oasys::UInt64Opt("mtu", &params_.mtu_));
    p.addopt(new oasys::UIntOpt("min_retry_interval",
                                &params_.min_retry_interval_));
    p.addopt(new oasys::UIntOpt("max_retry_interval",
                                &params_.max_retry_interval_));
    p.addopt(new oasys::UIntOpt("idle_close_time",
                                &params_.idle_close_time_));
    p.addopt(new oasys::UIntOpt("potential_downtime",
                                &params_.potential_downtime_));
    p.addopt(new oasys::BoolOpt("prevhop_hdr", &params_.prevhop_hdr_));
    p.addopt(new oasys::UIntOpt("cost", &params_.cost_));
    p.addopt(new oasys::BoolOpt("qlimit_enabled", &params_.qlimit_enabled_));
    p.addopt(new oasys::SizeOpt("qlimit_bundles_high",
                                &params_.qlimit_bundles_high_));
    p.addopt(new oasys::SizeOpt("qlimit_bytes_high",
                                &params_.qlimit_bytes_high_));
    p.addopt(new oasys::SizeOpt("qlimit_bundles_low",
                                &params_.qlimit_bundles_low_));
    p.addopt(new oasys::SizeOpt("qlimit_bytes_low",
                                &params_.qlimit_bytes_low_));

    p.addopt(new oasys::SizeOpt("qlimit_bytes_low",
                                &params_.qlimit_bytes_low_));
    
    p.addopt(new oasys::StringOpt("bp6_redirect",
                                &params_.bp6_redirect_));
    p.addopt(new oasys::StringOpt("bp7_redirect",
                                &params_.bp7_redirect_));
    
    int ret = p.parse_and_shift(argc, argv, invalidp);
    if (ret == -1) {
        return -1;
    }

    if (params_.min_retry_interval_ == 0 ||
        params_.max_retry_interval_ == 0)
    {
        *invalidp = "invalid retry interval";
        return -1;
    }
    
    if (params_.idle_close_time_ != 0 && type_ == ALWAYSON)
    {
        *invalidp = "idle_close_time must be zero for always on link";
        return -1;
    }
    
    return ret;
}

//----------------------------------------------------------------------
void
Link::set_initial_state()
{
}

//----------------------------------------------------------------------
Link::~Link()
{
    if (!BundleDaemon::shutting_down()) {
        // occasional segfault while shutting down trying to delete cl_info_
        
        ASSERT(!isopen());

        if(cl_info_ != nullptr) {
            delete cl_info_;
            cl_info_ = nullptr;
        }

        if (router_info_ != nullptr) {
            delete router_info_;
            router_info_ = nullptr;
        }
    }
}

//----------------------------------------------------------------------
void
Link::set_state(state_t new_state)
{
    log_debug("set_state %s -> %s",
              state_to_str(state()),
              state_to_str(new_state));

#define ASSERT_STATE(condition)                             \
    if (!(condition)) {                                     \
        log_err("set_state %s -> %s: expected %s",          \
                state_to_str(state()), \
                state_to_str(new_state),                    \
                #condition);                                \
    }

    oasys::ScopeLock l(&lock_, "Link::set_state");

    switch(new_state) {
    case UNAVAILABLE:
        break; // any old state is valid

    case AVAILABLE:
        ASSERT_STATE(state_ == OPEN || state_ == UNAVAILABLE);
        break;

    case OPENING:
        ASSERT_STATE(state_ == AVAILABLE || state_ == UNAVAILABLE);
        break;
        
    case OPEN:
        ASSERT_STATE(state_ == OPENING ||
                     state_ == UNAVAILABLE /* for opportunistic links */);
        break;

    default:
        NOTREACHED;
    }
#undef ASSERT_STATE

    state_ = new_state;
}

//----------------------------------------------------------------------
void
Link::open()
{
    oasys::ScopeLock l(&lock_, __func__);

    ASSERT(!isdeleted());

    if (state_ != AVAILABLE) {
        log_crit("Link::open: in state %s: expected state AVAILABLE",
                 state_to_str(state()));
        return;
    }

    set_state(OPENING);

    // tell the convergence layer to establish a new session however
    // it needs to, it will set the Link state to OPEN and post a
    // ContactUpEvent when it has done the deed
    ASSERT(contact_ == nullptr);

    Contact* contact = new Contact(LinkRef(this, "Link::open"));
    set_contact(contact);

    clayer()->open_contact(contact_);

    stats_.contact_attempts_++;

    log_debug("Link::open: *%p new contact %p", this, contact_.object());
}
    
//----------------------------------------------------------------------
void
Link::close()
{
    log_debug("Link::close");


    do {  // limit scope of lock
        oasys::ScopeLock l(&lock_, __func__);

        // we should always be open, therefore we must have a contact
        if (contact_ == nullptr) {
           log_err("Link::close with no contact");
           return;
       }
    } while (false);  // release lock before cloes_contact
    
    // Kick the convergence layer to close the contact and make sure
    // it cleaned up its state
    clayer()->close_contact(contact_);


    // get the lock again
    oasys::ScopeLock l(&lock_, __func__);

    ASSERT(contact_->cl_info() == nullptr);

    // Remove the reference from the link, which will clean up the
    // object eventually
    contact_ = nullptr;

    log_debug("Link::close complete");
}

//----------------------------------------------------------------------
bool
Link::queue_is_full() const
{
    oasys::ScopeLock l(&lock_, __func__);

    if (!params_.qlimit_enabled_) {
        return false;
    } else {
        return ( (bundles_queued_ > params_.qlimit_bundles_high_) ||
                 (bytes_queued_   > params_.qlimit_bytes_high_) ) ;
    }
}

//----------------------------------------------------------------------
bool
Link::queue_has_space() const
{
    oasys::ScopeLock l(&lock_, __func__);

    if (!params_.qlimit_enabled_) {
        return true;
    } else {
        return ( (bundles_queued_ < params_.qlimit_bundles_low_) &&
                 (bytes_queued_   < params_.qlimit_bytes_low_) );
    }
}

//----------------------------------------------------------------------
bool
Link::queue_limits_active () const
{
    return params_.qlimit_enabled_;
}

//----------------------------------------------------------------------
bool
Link::add_to_queue(const BundleRef& bundle)
{
    oasys::ScopeLock l(&lock_, "Link::add_to_queue");

    if ((state_ == UNAVAILABLE) && is_opportunistic()) {
        if (BundleDaemon::params_.clear_bundles_when_opp_link_unavailable_) {
            log_debug("attempt to queue bundle to opportunistic link while it is unavailable");

            LinkRef lref = LinkRef(this, "Link::add_to_queue");
            BundleDaemon::post(new BundleSendCancelledEvent(bundle.object(), lref));
            return false;
        }
    }

    
    if (queue_.contains(bundle)) {
        log_err("add_to_queue: bundle *%p already in queue for link %s",
                bundle.object(), name_.c_str());
        return false;
    }
    
    log_debug("adding *%p to queue (length %u)",
              bundle.object(), bundles_queued_);
    bundles_queued_++;
    bytes_queued_ += bundle->payload().length();
    queue_.push_back(bundle);

    // finally, kick the convergence layer
    if ((contact_ != nullptr) && (clayer_ != nullptr))
    {
        LinkRef lref = LinkRef(this, "Link::add_to_queue");
        clayer_->bundle_queued(lref, bundle);
    }

    return true;
}

//----------------------------------------------------------------------
bool
Link::del_from_queue(const BundleRef& bundle)
{
    oasys::ScopeLock l(&lock_, "Link::del_from_queue");
    
    if (! queue_.erase(bundle)) {
        return false;
    }

    ASSERT(bundles_queued_ > 0);
    bundles_queued_--;
   
    size_t payload_len = bundle->payload().length();
 
    // sanity checks
//    ASSERT(payload_len != 0);
    if (bytes_queued_ >= payload_len) {
        bytes_queued_ -= payload_len;

    } else {
        log_err("del_from_queue: *%p bytes_queued %zu < payload_len %zu",
                bundle.object(), bytes_queued_, payload_len);
    }
    
    log_debug("removed *%p from queue (length %zu)",
              bundle.object(), bundles_queued_);
    return true;
}
//----------------------------------------------------------------------
bool
Link::add_to_inflight(const BundleRef& bundle)
{
    oasys::ScopeLock l(&lock_, "Link::add_to_inflight");

    if (bundle->is_queued_on(&inflight_)) {
        log_err("bundle *%p already in flight for link %s",
                bundle.object(), name_.c_str());
        return false;
    }
    
    log_debug("adding *%p to in flight list for link %s",
              bundle.object(), name_.c_str());
    
    inflight_.push_back(bundle.object());

    bundles_inflight_++;
    bytes_inflight_ += bundle->payload().length();

    return true;
}

//----------------------------------------------------------------------
bool
Link::del_from_inflight(const BundleRef& bundle)
{
    oasys::ScopeLock l(&lock_, "Link::del_from_inflight");

    if (! inflight_.erase(bundle)) {
        return false;
    }

    ASSERT(bundles_inflight_ > 0);
    bundles_inflight_--;
    
    size_t payload_len = bundle->payload().length();

    // sanity checks
//    ASSERT(payload_len != 0);
    if (bytes_inflight_ >= payload_len) {
        bytes_inflight_ -= payload_len;

    } else {
        log_err("del_from_inflight: *%p bytes_inflight %zu < payload_len %zu",
                bundle.object(), bytes_inflight_, payload_len);
    }
    
    log_debug("removed *%p from inflight list (length %u)",
              bundle.object(), bundles_inflight_);
    return true;
}

//----------------------------------------------------------------------
int
Link::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz, "%s [%s %s %s %s state=%s]",
                    name(), nexthop(), remote_eid_.c_str(),
                    link_type_to_str(type()),
                    clayer()->name(),
                    state_to_str(state()));
}

//----------------------------------------------------------------------
void
Link::dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, "Link::dump");

    if (isdeleted()) {
        log_debug("Link::dump: cannot dump deleted link %s", name());
        return;
    }

    buf->appendf("Link %s:\n"
                 "clayer: %s\n"
                 "type: %s\n"
                 "state: %s\n"
                 "bp6_redirect: %s\n"
                 "bp7_redirect: %s\n"
                 "deleted: %s\n"
                 "nexthop: %s\n"
                 "remote eid: %s\n"
                 "mtu: %" PRIu64 "\n"
                 "min_retry_interval: %u\n"
                 "max_retry_interval: %u\n"
                 "idle_close_time: %u\n"
                 "potential_downtime: %u\n"
                 "prevhop_hdr: %s\n"
                 "reincarnated: %s\n"
                 "used in fwdlog: %s\n",
                 name(),
                 (clayer_!=nullptr) ? clayer_->name(): "(blank)",
                 link_type_to_str(type()),
                 state_to_str(state()),
                 params_.bp6_redirect_.c_str(),
                 params_.bp7_redirect_.c_str(),
                 (deleted_? "true" : "false"),
                 nexthop(),
                 remote_eid_.c_str(),
                 params_.mtu_,
                 params_.min_retry_interval_,
                 params_.max_retry_interval_,
                 params_.idle_close_time_,
                 params_.potential_downtime_,
                 params_.prevhop_hdr_ ? "true" : "false",
                 reincarnated_ ? "true" : "false",
                 used_in_fwdlog_ ? "true" : "false");


    if( !params_.qlimit_enabled_ ) {
        buf->appendf("queue limits disabled\n");
    } else {
        buf->appendf("qlimit_bundles_high: %zu\n"
                     "qlimit_bundles_low: %zu\n"
                     "qlimit_bytes_high: %zu\n"
                     "qlimit_bytes_low: %zu\n",
                     params_.qlimit_bundles_high_,
                     params_.qlimit_bundles_low_,
                     params_.qlimit_bytes_high_,
                     params_.qlimit_bytes_low_ );
    }

    buf->append("\n");
    if (contact_ != nullptr) {
        buf->appendf("contact: connected\n");
        buf->appendf("contact duration: %u\n", contact_->duration());
        buf->appendf("contact elapsed: %lu\n", contact_->start_time().elapsed_ms()/1000);
    } else {
        buf->appendf("contact: not connected\n");
    }
    buf->appendf("contact planned state: %s\n\n", contact_state_ ? "active" : "not active");

    ASSERT(clayer_ != nullptr);
    if (cl_info_ !=nullptr)
    	clayer_->dump_link(LinkRef(this, "Link::dump"), buf);
}

//----------------------------------------------------------------------
void
Link::get_cla_stats(oasys::StringBuffer& buf)
{
    oasys::ScopeLock l(&lock_, "Link::dump_stats");

    if (contact_ != nullptr) {
       contact_->get_cla_stats(buf);
    }
}

//----------------------------------------------------------------------
void
Link::dump_stats(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, "Link::dump_stats");

    if (isdeleted()) {
        log_debug("Link::dump_stats: "
                  "cannot dump stats for deleted link %s", name());
        return;
    }

    size_t uptime = stats_.uptime_;
    if (contact_ != nullptr) {
        uptime += (contact_->start_time().elapsed_ms() / 1000);
    }

    size_t throughput = 0;
    if (uptime != 0) {
        throughput = (stats_.bytes_transmitted_ * 8) / uptime;
    }
        
    buf->appendf("%zu contact_attempts -- "
                 "%zu contacts -- "
                 "%zu bundles_transmitted -- "
                 "%zu bytes_transmitted -- "
                 "%zu bundles_queued -- "
                 "%zu bytes_queued -- "
                 "%zu bundles_inflight -- "
                 "%zu bytes_inflight -- "
                 "%zu bundles_cancelled -- "
                 "%zu uptime -- "
                 "%zu throughput_bps",
                 stats_.contact_attempts_,
                 stats_.contacts_,
                 stats_.bundles_transmitted_,
                 stats_.bytes_transmitted_,
                 bundles_queued_,
                 bytes_queued_,
                 bundles_inflight_,
                 bytes_inflight_,
                 stats_.bundles_cancelled_,
                 uptime,
                 throughput);

    if (router_info_) {
        router_info_->dump_stats(buf);
    }
    if(planner_contact_!=NULL){	
      time_t tmp_start = (time_t)planner_contact_->start_time().in_seconds();
      struct tm* tmp_info;
      tmp_info = localtime(&tmp_start);
      char time_buf[18];
      strftime(time_buf,18,"%Y:%j:%H:%M:%S",tmp_info);
      std::string start_time(time_buf);
      std::stringstream ss_duration;
      ss_duration << planner_contact_->duration();
      std::string duration;
      ss_duration >> duration;

      buf->appendf("\n\nContact Info");
      buf->appendf("\nStart Time: ");
      buf->appendf(start_time.c_str());
      buf->appendf(" - Duration: ");
      buf->appendf(duration.c_str());

    } else if(contact_!=NULL){	
      time_t tmp_start = (time_t)contact_->start_time().in_seconds();
      struct tm* tmp_info;
      tmp_info = localtime(&tmp_start);
      char time_buf[18];
      strftime(time_buf,18,"%Y:%j:%H:%M:%S",tmp_info);
      std::string start_time(time_buf);
      std::stringstream ss_duration;
      ss_duration << contact_->duration();
      std::string duration;
      ss_duration >> duration;

      buf->appendf("\n\nContact Info");
      buf->appendf("\nStart Time: ");
      buf->appendf(start_time.c_str());
      buf->appendf(" - Duration: ");
      buf->appendf(duration.c_str());
    }

    if (contact_ != nullptr) {
        buf->appendf(" - Elapsed: %lu", contact_->start_time().elapsed_ms()/1000);
    }
    buf->appendf(" - Planned State:: %s", contact_state_ ? "active" : "not active");
    buf->appendf("\n");
}


//----------------------------------------------------------------------    
void 
Link::increment_deferred_count()
{
    ++deferred_bundle_count_;
    if (nullptr == deferred_timer_) {
        deferred_timer_ = std::make_shared<LinkDeferredTimer>(this);
        oasys::SPtr_Timer base_sptr = deferred_timer_;
        oasys::SharedTimerSystem::instance()->schedule_in(1000, base_sptr);
    }
}

//----------------------------------------------------------------------    
void
Link::decrement_deferred_count()
{
    --deferred_bundle_count_;
}

//----------------------------------------------------------------------    
void
Link::check_deferred_bundles()
{
    deferred_timer_ = nullptr;

    if (deferred_bundle_count_ > 0) {
        // timer expired - issue event and start a new timer
        BundleDaemon::post(new LinkCheckDeferredEvent(this));

        deferred_timer_ = std::make_shared<LinkDeferredTimer>(this);
        oasys::SPtr_Timer base_sptr = deferred_timer_;
        oasys::SharedTimerSystem::instance()->schedule_in(1000, base_sptr);
    }
}

//----------------------------------------------------------------------
bool
Link::get_contact_state()
{
    oasys::ScopeLock l(&lock_, __func__);
    return contact_state_;
}

//----------------------------------------------------------------------
void
Link::set_contact_state(bool state)
{
    oasys::ScopeLock l(&lock_, __func__);
    contact_state_ = state;
}

//----------------------------------------------------------------------
void
Link::set_contact_state(bool state, Contact* contact)
{
    oasys::ScopeLock l(&lock_, __func__);
    contact_timer_ = nullptr;
    int duration = (int)contact->duration();
    if(duration > 0)
    {
      contact_timer_ = std::make_shared<ContactTimer>(this);
      oasys::SPtr_Timer base_sptr = contact_timer_;
      oasys::SharedTimerSystem::instance()->schedule_in(duration, base_sptr);
    }
    contact_state_ = state;
    planner_contact_ = contact;
}

//----------------------------------------------------------------------
void 
Link::set_contact(Contact* contact)
{
    oasys::ScopeLock l(&lock_, __func__);

    // XXX/demmer check this invariant
    ASSERT(contact_ == NULL);
    contact_ = contact;
    int duration = (int)contact_->duration();
    if(duration > 0)
    {
      contact_timer_ = nullptr;
      contact_timer_ = std::make_shared<ContactTimer>(this);
      oasys::SPtr_Timer base_sptr = contact_timer_;
      oasys::SharedTimerSystem::instance()->schedule_in(duration, base_sptr);
    }

    set_contact_state(true);
}

//----------------------------------------------------------------------
void
Link::ContactTimer::timeout(const struct timeval& now)
{
    (void)now;
    linkref_->set_contact_state(false);
    usleep(1);
    delete this;
}


//----------------------------------------------------------------------    
void
Link::LinkDeferredTimer::timeout(
        const struct timeval &now)
{
    (void)now;
    linkref_->check_deferred_bundles();
}


} // namespace dtn
