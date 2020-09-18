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
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
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

#include <queue>

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/TokenBucket.h>

#include "SimConvergenceLayer.h"
#include "Connectivity.h"
#include "Node.h"
#include "Simulator.h"
#include "Topology.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleList.h"
#include "contacts/ContactManager.h"

namespace dtnsim {

class InFlightBundle;

//----------------------------------------------------------------------
class SimLink : public CLInfo,
                public oasys::Logger {
	// Note although this is derived from CLInfo (and hence SerializableObject
	// we don't define serialize and it defaults to the empty routine in CLInfo.
	// Since the simulator uses the memory 'persistent' data store, there is never
	// anything to read back in on restart, so it is irrelevant what is serialized
	// the cl_info field of any Links.
public:
    struct Params;
    
    SimLink(const LinkRef& link,
            const SimLink::Params& params)
        : Logger("SimLink", "/dtn/cl/sim/%s", link->name()),
          link_(link.object(), "SimLink"),
          params_(params),
          tb_(((std::string)logpath_ + "/tb").c_str(),
              params_.capacity_,
              0xffffffff  /* unlimited rate -- overridden by Connectivity */),
          inflight_timer_(this, PendingEventTimer::INFLIGHT),
          arrival_timer_(this, PendingEventTimer::ARRIVAL),
          transmitted_timer_(this, PendingEventTimer::TRANSMITTED)
    {
    }

    ~SimLink() {};

    void start_next_bundle();
    void timeout(const oasys::Time& now);
    void handle_pending_inflight(const oasys::Time& now);
    void handle_arrival_events(const oasys::Time& now);
    void handle_transmitted_events(const oasys::Time& now);
    void reschedule_timers();

    /// The dtn Link
    LinkRef link_;
    
    struct Params {
        /// if contact closes in the middle of a transmission, deliver
        /// the partially received bytes to the router.
        bool deliver_partial_;

        /// for bundles sent over the link, signal to the router
        /// whether or not they were delivered reliably by the
        /// convergence layer
        bool reliable_;

        /// burst capacity of the link (default 0)
        u_int capacity_;

        /// automatically infer the remote eid when the link connects
        bool set_remote_eid_;
        
        /// set the previous hop when bundles arrive
        bool set_prevhop_;
        
    } params_;

    /// The sending node
    Node* src_node_;

    /// The receiving node
    Node* peer_node_;	

    /// Token bucket to track the link rate
    oasys::TokenBucket tb_;

    /// Temp buffer
    u_char buf_[65536];
    
    /// Helper class to track bundle transmission or reception events
    /// that need to be delivered in the future
    struct PendingEvent {
        PendingEvent(Bundle*            bundle,
                     size_t             total_len,
                     const oasys::Time& time)
            : bundle_(bundle, "SimCL::PendingEvent"),
              total_len_(total_len),
              time_(time) {}

        BundleRef   bundle_;
        size_t      total_len_;
        oasys::Time time_;
    };

    /// Pending event (at most one) to put the next bundle in flight
    PendingEvent* pending_inflight_;

    /// Pending bundle arrival events
    std::queue<PendingEvent*> arrival_events_;

    /// Pending bundle transmitted events
    std::queue<PendingEvent*> transmitted_events_;
    
    /// Timer class to manage pending events
    class PendingEventTimer : public oasys::Timer {
    public:
        typedef enum { INFLIGHT, ARRIVAL, TRANSMITTED } type_t;
        
        PendingEventTimer(SimLink* link, type_t type)
            : link_(link), type_(type) {}
        
        void timeout(const timeval& now);
        
    protected:
        SimLink* link_;
        type_t   type_;
    };

    /// @{ Three timer instances to independently schedule the timers,
    /// though each class can itself be managed with a FIFO queue.
    PendingEventTimer inflight_timer_;
    PendingEventTimer arrival_timer_;
    PendingEventTimer transmitted_timer_;
    /// @}
};

//----------------------------------------------------------------------
void
SimLink::start_next_bundle()
{
    ASSERT(!link_->queue()->empty());
    ASSERT(pending_inflight_ == NULL);
    
    // Remember the node that was active on entry
    // Have to control the active node so that operations are done
    // on either the source or destination node as appropriate.
    // Since this routine is called via the (single) timer system,
    // there is no guarantee what node is active when it is called.
    // and we have to force the active node first to the link source
    // and later to the link destination.  The active node is set back
    // to the same as on entry at the end.
    // These considerations also apply to handle_arrival_events and
    // handle_transmitted_events
    Node * orig_node = Node::active_node();

    src_node_->set_active();

    const ConnState* cs = Connectivity::instance()->lookup(src_node_, peer_node_);
    ASSERT(cs);

    BundleRef src_bundle("SimLink::start_next_bundle");
    src_bundle = link_->queue()->front();
    
    BlockInfoVec* blocks = src_bundle->xmit_blocks()->find_blocks(link_);
    ASSERT(blocks != NULL);

    // since we don't really have any payload to send, we find the
    // payload block and overwrite the data_length to be zero, then
    // adjust the payload_ on the new bundle
    if (src_bundle->payload().location() == BundlePayload::NODATA) {
        BlockInfo* payload = const_cast<BlockInfo*>(
            blocks->find_block(BundleProtocol::PAYLOAD_BLOCK));
        ASSERT(payload != NULL);
        payload->set_data_length(0);
    }
    
    bool complete = false;
    size_t len = BundleProtocol::produce(src_bundle.object(), blocks,
                                         buf_, 0, sizeof(buf_),
                                         &complete);
    ASSERTF(complete, "BundleProtocol non-payload blocks must fit in "
            "65 K buffer size");

    size_t total_len = len;

    if (src_bundle->payload().location() == BundlePayload::NODATA)
        total_len += src_bundle->payload().length();

    complete = false;
    peer_node_->set_active();
    Bundle* dst_bundle = new Bundle(src_bundle->payload().location());
    int cc = BundleProtocol::consume(dst_bundle, buf_, len, &complete);
    ASSERT(cc == (int)len);
    ASSERT(complete);

    if (src_bundle->payload().location() == BundlePayload::NODATA) {
        dst_bundle->mutable_payload()->set_length(src_bundle->payload().length());
    }
            
    tb_.drain(total_len * 8);
    
    oasys::Time bw_delay = tb_.time_to_level(0);
    oasys::Time inflight_time = oasys::Time(Simulator::time()) + bw_delay;
    oasys::Time arrival_time = inflight_time + cs->latency_;
    oasys::Time transmitted_time;

    // the transmitted event either occurs after the "ack" comes back
    // (when in reliable mode) or immediately after we send the bundle
    if (params_.reliable_) {
        transmitted_time = inflight_time + (cs->latency_ * 2);
    } else {
        transmitted_time = inflight_time;
    }
    
    log_debug("send_bundle src %" PRIbid " dst %" PRIbid ": total len %zu, "
              "inflight_time %u.%u arrival_time %u.%u transmitted_time %u.%u",
              src_bundle->bundleid(), dst_bundle->bundleid(), total_len,
              inflight_time.sec_, inflight_time.usec_,
              arrival_time.sec_, arrival_time.usec_,
              transmitted_time.sec_, transmitted_time.usec_);

    pending_inflight_ = new PendingEvent(src_bundle.object(), total_len, inflight_time);
    arrival_events_.push(new PendingEvent(dst_bundle, total_len, arrival_time));
    transmitted_events_.push(new PendingEvent(src_bundle.object(), total_len, transmitted_time));

    reschedule_timers();
    orig_node->set_active();
}

//----------------------------------------------------------------------
void
SimLink::reschedule_timers()
{
    // if the timer is already pending, there's no need to reschedule
    // since the channel is FIFO and latency changes don't take effect
    // mid-flight

    if (! inflight_timer_.pending() && pending_inflight_ != NULL)
    {
        inflight_timer_.schedule_at(pending_inflight_->time_);
    }

    if (! arrival_timer_.pending() && !arrival_events_.empty())
    {
        arrival_timer_.schedule_at(arrival_events_.front()->time_);
    }

    if (! transmitted_timer_.pending() && !transmitted_events_.empty())
    {
        transmitted_timer_.schedule_at(transmitted_events_.front()->time_);
    }
}

//----------------------------------------------------------------------
void
SimLink::PendingEventTimer::timeout(const timeval& tv)
{
    oasys::Time now(tv.tv_sec, tv.tv_usec);
    switch (type_) {
    case INFLIGHT:
        link_->handle_pending_inflight(now);
        break;
    case ARRIVAL:
        link_->handle_arrival_events(now);
        break;
    case TRANSMITTED:
        link_->handle_transmitted_events(now);
        break;
    default:
        NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
SimLink::handle_pending_inflight(const oasys::Time& now)
{
    ASSERT(pending_inflight_ != NULL);
    
    // deliver any bundles that have arrived
    if (pending_inflight_->time_ <= now) {
        const BundleRef& bundle = pending_inflight_->bundle_;

        log_debug("putting *%p in flight", bundle.object());
        link_->add_to_inflight(bundle, pending_inflight_->total_len_);
        link_->del_from_queue(bundle, pending_inflight_->total_len_);
            
        // XXX/demmer maybe there should be an event for this??
        
        delete pending_inflight_;
        pending_inflight_ = NULL;

        if (! link_->queue()->empty()) {
            start_next_bundle();
        }
    }
    
    reschedule_timers();
}

//----------------------------------------------------------------------
void
SimLink::handle_arrival_events(const oasys::Time& now)
{
    ASSERT(! arrival_events_.empty());
    
    Node * orig_node = Node::active_node();

    // deliver any bundles that have arrived
	peer_node_->set_active();
    while (! arrival_events_.empty()) {
        PendingEvent* next = arrival_events_.front();
        if (next->time_ <= now) {
            const BundleRef& bundle = next->bundle_;
            arrival_events_.pop();

            log_debug("*%p arrived", bundle.object());
            
            BundleReceivedEvent* rcv_event =
                new BundleReceivedEvent(bundle.object(),
                                        EVENTSRC_PEER,
                                        next->total_len_,
                                        params_.set_prevhop_ ?
                                        Node::active_node()->local_eid() :
                                        EndpointID::NULL_EID());
            peer_node_->post_event(rcv_event);

            delete next;
            
        } else {
            break;
        }
    }

    reschedule_timers();
    orig_node->set_active();
}

//----------------------------------------------------------------------
void
SimLink::handle_transmitted_events(const oasys::Time& now)
{
    ASSERT(! transmitted_events_.empty());
    
    Node * orig_node = Node::active_node();

    // deliver any bundles that have arrived
	src_node_->set_active();
    while (! transmitted_events_.empty()) {
        PendingEvent* next = transmitted_events_.front();
        if (next->time_ <= now) {
            const BundleRef& bundle = next->bundle_;
            transmitted_events_.pop();
            
            log_debug("*%p transmitted", bundle.object());

            ASSERT(link_->contact() != NULL);
            
            BundleTransmittedEvent* xmit_event =
                new BundleTransmittedEvent(bundle.object(), link_->contact(), link_,
                                           next->total_len_,
                                           params_.reliable_ ? next->total_len_ : 0);
            BundleDaemon::post(xmit_event);
            
            delete next;
        } else {
            break;
        }
    }
    
    reschedule_timers();
    orig_node->set_active();
}

//----------------------------------------------------------------------
SimConvergenceLayer* SimConvergenceLayer::instance_;

SimConvergenceLayer::SimConvergenceLayer()
    : ConvergenceLayer("SimConvergenceLayer", "sim")
{
}

//----------------------------------------------------------------------
bool
SimConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    oasys::OptParser p;
    SimLink::Params params;

    params.deliver_partial_ = true;
    params.reliable_        = true;
    params.capacity_        = 0;
    params.set_remote_eid_  = true;
    params.set_prevhop_     = true;
    
    p.addopt(new oasys::BoolOpt("deliver_partial", &params.deliver_partial_));
    p.addopt(new oasys::BoolOpt("reliable", &params.reliable_));
    p.addopt(new oasys::UIntOpt("capacity", &params.capacity_));
    p.addopt(new oasys::BoolOpt("set_remote_eid", &params.set_remote_eid_));
    p.addopt(new oasys::BoolOpt("set_prevhop", &params.set_prevhop_));

    const char* invalid;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option %s", invalid);
        return false;
    }

    SimLink* sl = new SimLink(link, params);
    sl->src_node_ = Node::active_node();
    sl->peer_node_ = Topology::find_node(link->nexthop());

    ASSERT (sl->peer_node_);
    ASSERT (sl->src_node_ != sl->peer_node_);

    link->set_cl_info(sl);

    return true;
}

//----------------------------------------------------------------------
void
SimConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("SimConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
bool
SimConvergenceLayer::open_contact(const ContactRef& contact)
{
    log_debug("opening contact for link [*%p]", contact.object());


    SimLink* sl = (SimLink*)contact->link()->cl_info();
    ASSERT(sl);
    
    const ConnState* cs = Connectivity::instance()->
                          lookup(Node::active_node(), sl->peer_node_);
    if (cs != NULL && cs->open_) {
        log_debug("opening contact");
        if (sl->params_.set_remote_eid_) {
            contact->link()->set_remote_eid(sl->peer_node_->local_eid());
        }
        update_connectivity(Node::active_node(), sl->peer_node_, *cs);
        BundleDaemon::post(new ContactUpEvent(contact));

        // if there is a queued bundle on the link, start sending it
        if (! contact->link()->queue()->empty()) {
            sl->start_next_bundle();
        }
        
    } else {
        log_debug("connectivity is down when trying to open contact");
        BundleDaemon::post(
            new LinkStateChangeRequest(contact->link(),
                                       Link::CLOSED,
                                       ContactEvent::BROKEN));
    }
	
    return true;
}

//----------------------------------------------------------------------
void 
SimConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void)bundle;
    
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("bundle_queued *%p on link *%p", bundle.object(), link.object());

    SimLink* sl = (SimLink*)link->cl_info();
    ASSERT(sl);

    if (link->isopen() && (sl->pending_inflight_ == NULL)) {
        sl->start_next_bundle();
    }
}

//----------------------------------------------------------------------
void
SimConvergenceLayer::update_connectivity(Node* n1, Node* n2, const ConnState& cs)
{
    ASSERT(n1 != NULL);
    ASSERT(n2 != NULL);

    n1->set_active();
    
    ContactManager* cm = n1->contactmgr();;

    oasys::ScopeLock l(cm->lock(), "SimConvergenceLayer::update_connectivity");
    const LinkSet* links = cm->links();
    
    for (LinkSet::iterator iter = links->begin();
         iter != links->end();
         ++iter)
    {
        LinkRef link = *iter;
        SimLink* sl = (SimLink*)link->cl_info();
        ASSERT(sl);

        // update the token bucket
        sl->tb_.set_rate(cs.bw_);
        
        if (sl->peer_node_ != n2)
            continue;
        
        log_debug("update_connectivity: checking node %s link %s",
                  n1->name(), link->name());
        
        if (cs.open_ == false && link->state() == Link::OPEN) {
            log_debug("update_connectivity: closing link %s", link->name());
            n1->post_event(
                new LinkStateChangeRequest(link, Link::CLOSED,
                                           ContactEvent::BROKEN));
        }
    }
}


} // namespace dtnsim
