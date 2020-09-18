/*
 * Copyright 2008 The MITRE Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * The US Government will not be charged any license fee and/or royalties
 * related to this software. Neither name of The MITRE Corporation; nor the
 * names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 */

/*
 * This product includes software written and developed 
 * by Brian Adamson and Joe Macker of the Naval Research 
 * Laboratory (NRL).
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if defined(NORM_ENABLED)

#include <sys/poll.h>
#include <stdlib.h>
#include <normApi.h>

#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/Random.h>
#include <oasys/util/OptParser.h>

#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "contacts/ContactManager.h"
#include "NORMConvergenceLayer.h"
#include "NORMSender.h"
#include "NORMReceiver.h"
#include "NORMSessionManager.h"

namespace dtn {

NORMParameters NORMConvergenceLayer::defaults_;

//----------------------------------------------------------------------
bool
NORMParameters::parse_link_params(NORMParameters *params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    ASSERT(params != 0);
    oasys::OptParser p;

    // first see if a link type was requested
    oasys::EnumOpt::Case link_type_opts[] = {
        {"eplrs4hop",  NORMParameters::EPLRS4HOP},
        {"eplrs1hop",  NORMParameters::EPLRS1HOP},
        {0, 0}
    };
    int type = 0;
    p.addopt(new oasys::EnumOpt("link_type", link_type_opts, &type));

    int res = p.parse_and_shift(argc, argv, invalidp);

    switch (type) {
        case EPLRS4HOP:   params->eplrs4hop(); break;
        case EPLRS1HOP:   params->eplrs1hop(); break;
        default: break;
    }

    argc -= res;
    *invalidp = 0;

    // now check for all other settings/overrides
    p.addopt(new oasys::StringOpt("multicast_interface", &params->multicast_interface_));
    p.addopt(new oasys::InAddrOpt("nodeid", &params->nodeid_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("group_addr", &params->group_addr_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::BoolOpt("cc", &params->cc_));
    p.addopt(new oasys::BoolOpt("ecn", &params->ecn_));
    p.addopt(new oasys::UInt16Opt("segment_size", &params->segment_size_));
    p.addopt(new oasys::UInt64Opt("fec_buf_size", &params->fec_buf_size_));
    p.addopt(new oasys::UInt8Opt("block_size", &params->block_size_));
    p.addopt(new oasys::UInt8Opt("num_parity", &params->num_parity_));
    p.addopt(new oasys::UInt8Opt("auto_parity", &params->auto_parity_));
    p.addopt(new oasys::DoubleOpt("backoff_factor", &params->backoff_factor_));
    p.addopt(new oasys::UIntOpt("group_size", &params->group_size_));
    p.addopt(new oasys::UInt64Opt("tx_cache_size_max",
             reinterpret_cast<u_int64_t*>(&params->tx_cache_size_max_)));
    p.addopt(new oasys::UIntOpt("tx_cache_count_min", &params->tx_cache_count_min_));
    p.addopt(new oasys::UIntOpt("tx_cache_count_max", &params->tx_cache_count_max_));
    p.addopt(new oasys::UIntOpt("rx_buf_size",&params->rx_buf_size_));
    p.addopt(new oasys::UIntOpt("tx_robust_factor",&params->tx_robust_factor_));
    p.addopt(new oasys::UIntOpt("rx_robust_factor",&params->rx_robust_factor_));
    p.addopt(new oasys::UIntOpt("keepalive_intvl", &params->keepalive_intvl_));
    p.addopt(new oasys::UInt8Opt("tos", &params->tos_));
    p.addopt(new oasys::BoolOpt("ack", &params->ack_));
    p.addopt(new oasys::StringOpt("acking_list", &params->acking_list_));
    p.addopt(new oasys::BoolOpt("silent", &params->silent_));
    p.addopt(new oasys::DoubleOpt("rate", &params->rate_));
    p.addopt(new oasys::UIntOpt("object_size",&params->object_size_));
    p.addopt(new oasys::UIntOpt("tx_spacer",&params->tx_spacer_));

    // parse options
    if (! p.parse(argc, argv, invalidp))
        return false;

    params->pause_time();

    return true;
};

//----------------------------------------------------------------------
NORMParameters::NORMParameters()
    : multicast_interface_(),
      nodeid_(INADDR_ANY),
      local_port_(NORMCL_DEFAULT_MPORT),
      group_addr_(INADDR_ANY),
      remote_addr_(INADDR_ANY),
      remote_port_(NORMCL_DEFAULT_PORT),
      cc_(false), ecn_(false), segment_size_(1400),
      fec_buf_size_(1024*1024), block_size_(64),
      num_parity_(16), auto_parity_(0),
      backoff_factor_(0.0),
      group_size_(1000), tx_cache_size_max_(20971520),
      tx_cache_count_min_(8), tx_cache_count_max_(1024),
      rx_buf_size_(300000), tx_robust_factor_(20),
      rx_robust_factor_(20), keepalive_intvl_(5000),
      tos_(0), ack_(false), acking_list_(),
      silent_(false), rate_(64000), object_size_(0),
      tx_spacer_(0), inter_object_pause_(0),
      multicast_dest_(false),
      norm_session_mode_(NEGATIVE_ACKING),
      norm_session_(NORM_SESSION_INVALID),
      norm_sender_(0), norm_receiver_(0)
{
}

//----------------------------------------------------------------------
NORMParameters::NORMParameters(const NORMParameters &params)
    : CLInfo(),
      multicast_interface_(params.multicast_interface_),
      nodeid_(params.nodeid_),
      local_port_(params.local_port_),
      group_addr_(params.group_addr_),
      remote_addr_(params.remote_addr_),
      remote_port_(params.remote_port_),
      cc_(params.cc_), ecn_(params.ecn_),
      segment_size_(params.segment_size_),
      fec_buf_size_(params.fec_buf_size_),
      block_size_(params.block_size_),
      num_parity_(params.num_parity_),
      auto_parity_(params.auto_parity_),
      backoff_factor_(params.backoff_factor_),
      group_size_(params.group_size_),
      tx_cache_size_max_(params.tx_cache_size_max_),
      tx_cache_count_min_(params.tx_cache_count_min_),
      tx_cache_count_max_(params.tx_cache_count_max_),
      rx_buf_size_(params.rx_buf_size_),
      tx_robust_factor_(params.tx_robust_factor_),
      rx_robust_factor_(params.rx_robust_factor_),
      keepalive_intvl_(params.keepalive_intvl_),
      tos_(params.tos_),
      ack_(params.ack_),
      acking_list_(params.acking_list_),
      silent_(params.silent_),
      rate_(params.rate_),
      object_size_(params.object_size_),
      tx_spacer_(params.tx_spacer_),
      inter_object_pause_(params.inter_object_pause_),
      multicast_dest_(params.multicast_dest_),
      norm_session_mode_(params.norm_session_mode_),
      norm_session_(NORM_SESSION_INVALID),
      norm_sender_(0), norm_receiver_(0)
{
}

//----------------------------------------------------------------------
void
NORMParameters::serialize(oasys::SerializeAction *a)
{
    a->process("nodeid", oasys::InAddrPtr(&nodeid_));
    a->process("local_port", &local_port_);
    a->process("group_addr", oasys::InAddrPtr(&group_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("remote_port", &remote_port_);
    a->process("cc", &cc_);
    a->process("ecn", &ecn_);
    a->process("rate", (int64_t*)&rate_); // XXX fix me
    a->process("segment_size", &segment_size_);
    a->process("fec_buf_size", &fec_buf_size_);
    a->process("block_size", &block_size_);
    a->process("num_parity", &num_parity_);
    a->process("auto_parity", &auto_parity_);
    a->process("backoff_factor", (int64_t*)&backoff_factor_); // XXX fix me
    a->process("group_size", &group_size_);
    a->process("tx_cache_size_max", &tx_cache_size_max_);
    a->process("tx_cache_count_min", &tx_cache_count_min_);
    a->process("tx_cache_count_max", &tx_cache_count_max_);
    a->process("rx_buf_size", &rx_buf_size_);
    a->process("tx_robust_factor", &tx_robust_factor_);
    a->process("rx_robust_factor", &rx_robust_factor_);
    a->process("keepalive_intvl", &keepalive_intvl_);
    a->process("object_size", &object_size_);
    a->process("inter_object_pause", &inter_object_pause_);
    a->process("tx_spacer", &tx_spacer_);
    a->process("tos", &tos_);
    a->process("ack", &ack_);
}

//----------------------------------------------------------------------
void 
NORMParameters::eplrs4hop()
{
    cc_ = false;
    rate_ = 57600; // bps
    keepalive_intvl_ = 5000;
    object_size_ = 7200; // one second to transmit
    rx_buf_size_ = 64000;
    tx_spacer_ = 200;
}

//----------------------------------------------------------------------
void
NORMParameters::eplrs1hop()
{
    cc_ = false;
    rate_ = 230400; // bps
    keepalive_intvl_ = 5000;
    object_size_ = 28800; // one second to transmit
    rx_buf_size_ = 256000;
    tx_spacer_ = 200;
}

//----------------------------------------------------------------------
void
NORMParameters::pause_time()
{
    // the only way we'll calculate is if cc is disabled
    // and an object_size has been specified
    if ((! cc_) && (object_size_ > 0)) {
        u_int32_t frag_size_bps = object_size_ * 8;
        inter_object_pause_ =
            (u_int32_t)(((double)frag_size_bps / rate_) * 1000) +
            tx_spacer_;
    }
}

//----------------------------------------------------------------------
NORMConvergenceLayer::BundleInfo::BundleInfo(
    u_int32_t seconds, u_int32_t seqno, u_int32_t frag_offset,
    u_int32_t total_length, u_int32_t payload_offset,
    u_int32_t length, u_int32_t object_size, u_int16_t chunk)
    : seconds_(seconds), seqno_(seqno), frag_offset_(frag_offset),
      total_length_(total_length), payload_offset_(payload_offset),
      length_(length), object_size_(object_size), chunk_(chunk)
{
}

//----------------------------------------------------------------------
NORMConvergenceLayer::NORMConvergenceLayer()
    : IPConvergenceLayer("NORMConvergenceLayer", "norm"),
      lock_(logpath_)
{
}

//----------------------------------------------------------------------
CLInfo*
NORMConvergenceLayer::new_link_params()
{
    return new NORMParameters(defaults_);
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                        const char** invalidp)
{
    return NORMParameters::parse_link_params(&defaults_, argc,
                                             argv, invalidp);
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::interface_up(Interface* iface, int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());

    // Create a new parameters interface structure
    const char* invalid;
    NORMParameters *interface_params = new NORMParameters(defaults_);
    if (! NORMParameters::parse_link_params(interface_params, argc, argv, &invalid)) {
        delete interface_params;
        log_err("error parsing interface options: invalid option '%s'", invalid);
        return false;
    }

    if (! interface_params->group_addr()) {
        delete interface_params;
        log_err("error parsing interface options: group_addr required");
        return false;
    }

    interface_params->set_session_mode(NORMParameters::RECEIVE_ONLY);

    // check for a multicast group to join,
    if (multicast_addr(interface_params->group_addr())) {
        log_info("interface %s joining multicast group %s",
                 iface->name().c_str(), intoa(interface_params->group_addr()));
        interface_params->set_multicast_dest(true);
    } else {
        delete interface_params;
        log_err("error parsing interface options: group_addr is not a multicast address");
        return false;
    }

    // start *the* norm engine instance
    NORMSessionManager::instance()->init();

    NormSessionHandle interface_session;
    create_session(&interface_session, interface_params->nodeid(),
                   interface_params->group_addr(),
                   interface_params->local_port());

    interface_params->set_norm_session(interface_session);

    // set the multicast interface for the join
    if (interface_params->multicast_interface().empty()) {
        log_info("no network interface specified for %s, using default",
                 iface->name().c_str());
    } else {
        NormSetMulticastInterface(interface_session,
                                  interface_params->multicast_interface_c_str());
    }

    // create a new receiver thread for the interface
    // that listens for events from the norm engine
    NORMReceiver *interface_receiver =
        new NORMReceiver(interface_params, new ReceiveOnly());
    //dzdebug interface_receiver->start();

    // register the receiver thread with
    // the Norm session manager
    NORMSessionManager::instance()->register_receiver(interface_receiver);

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(interface_receiver);

    return true;
}
    
//----------------------------------------------------------------------
void
NORMConvergenceLayer::interface_activate(Interface* iface)
{
    NORMReceiver *interface_receiver = (NORMReceiver*)iface->cl_info();
    interface_receiver->start();
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object and set a flag for the thread to stop
    NORMReceiver *interface_receiver = (NORMReceiver*)iface->cl_info();

    // set a flag for the receiver thread to stop
    interface_receiver->set_should_stop();

    // unregister the receiver from the session manager
    NORMSessionManager::instance()->
        remove_receiver(interface_receiver);

    NormDestroySession(interface_receiver->norm_session());

    // free norm receiver thread
    delete interface_receiver;

    return true;
}

//----------------------------------------------------------------------
void
NORMConvergenceLayer::dump_interface(Interface *iface,
                                     oasys::StringBuffer *buf)
{
    NORMReceiver* receiver = dynamic_cast<NORMReceiver*>(iface->cl_info());
    ASSERT(receiver);

    buf->appendf("\tnodeid: %s local_port: %hu",
                 intoa(receiver->link_params()->nodeid()),
                 receiver->link_params()->local_port());

    if (receiver->link_params()->multicast_dest()) {
        buf->appendf(" group_addr: %s\n",
                 intoa(receiver->link_params()->group_addr()));
    } else {
        buf->appendf("\n");
    }
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::init_link(const LinkRef& link,
                                int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);
    
    if (! (link->type() == Link::ALWAYSON ||
           link->type() == Link::OPPORTUNISTIC)) {
        log_warn("link type not supported");
        return false;
    }

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    const char* invalid;
    NORMParameters *params = new NORMParameters(defaults_);
    if (! NORMParameters::parse_link_params(params, argc, argv, &invalid)) {
        delete params;
        log_err("error parsing link options: invalid option '%s'", invalid);
        return false;
    }

    // start *the* norm engine instance (noop if already started)
    NORMSessionManager::instance()->init();

    if (params->norm_session_mode() != NORMParameters::POSITIVE_ACKING) {
        link->set_reliable(true);
    }

    link->set_cl_info(params);

    return true;
}

//----------------------------------------------------------------------
void
NORMConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("NORMConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
//XXX Close the session!!
}

//----------------------------------------------------------------------
void
NORMConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    NORMParameters *params =
        dynamic_cast<NORMParameters*>(link->cl_info());

    if (params == 0) {
        log_err("can't access %s link parameters", link->name());
        return ;
    }

    buf->appendf("group_addr: %s\n",
                 intoa(params->group_addr()));
    buf->appendf("remote_addr: %s:%d\n",
                 intoa(params->remote_addr()), params->remote_port());
    buf->appendf("congestion control: %s\n",
                 params->cc() ? "enabled" : "disabled");
    buf->appendf("rate (bps): %f\n", params->rate());
    buf->appendf("segment size: %u\n", params->segment_size());
    buf->appendf("fec buffer size: %llu\n", params->fec_buf_size());
    buf->appendf("block size: %u\n", params->block_size());
    buf->appendf("parity blocks: %u\n", params->num_parity());
    buf->appendf("proactive parity blocks: %u\n", params->auto_parity());
    buf->appendf("backoff factor: %f\n", params->backoff_factor());
    buf->appendf("group size: %u\n", params->group_size());
    buf->appendf("transmit cache size max: %llu\n",
                 params->tx_cache_size_max());
    buf->appendf("transmit cache count min: %u\n",
                 params->tx_cache_count_min());
    buf->appendf("transmit cache count max: %u\n",
                 params->tx_cache_count_max());
    buf->appendf("tx robust factor: %u\n", params->tx_robust_factor());
    buf->appendf("rx robust factor: %u\n", params->rx_robust_factor());
    buf->appendf("keepalive interval (ms): %u\n", params->keepalive_intvl());
    buf->appendf("session type: %s\n",
        NORMParameters::session_mode_to_str(params->norm_session_mode()));
    if (params->tos() == 0) {
        buf->appendf("tos: 0\n");
    } else {
        buf->appendf("tos: %c\n", params->tos());
    }

    if (params->norm_session_mode() != NORMParameters::RECEIVE_ONLY) {
        buf->appendf("norm object size: %u\n", params->object_size());
        buf->appendf("inter object pause (ms): %u\n", params->inter_object_pause());
    }
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    LinkRef link = contact->link();

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    // grab the link parameters
    NORMParameters *params = dynamic_cast<NORMParameters*>(link->cl_info());
    if (params == 0) {
        log_err("can't access %s link parameters", link->name());
        open_contact_abort(link);
        return false;
    }

    // first check for an existing norm session on this link
    if (params->norm_sender() && params->norm_receiver()) {
        NORMSender *sender = params->norm_sender();

        if (! sender->closing_session()) {
            log_debug("reopening link %s with existing norm session %p",
                      link->name(), params->norm_session());
    
            sender->contact_ = contact;
            contact->set_cl_info(sender);

            NormSetGrttProbingMode(sender->norm_session(), NORM_PROBE_ACTIVE);

            BundleDaemon::post(new ContactUpEvent(link->contact()));
            sender->contact_up_ = true;

            // reissue bundle queued messages to sender if needed
            issue_bundle_queued(link, sender);
    
            return true;
        }
    }

    // we don't have an existing norm session
    log_debug("NORMConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());

    // parse out the address / port from the nexthop address
    if (! parse_nexthop(link->nexthop(), &addr, &port)) {
        log_err("invalid next hop address '%s'", link->nexthop());
        open_contact_abort(link);
        return false;
    }
    
    // make sure it's really a valid address
    if (addr == INADDR_ANY || addr == INADDR_NONE) {
        log_err("can't lookup hostname in next hop address '%s'",
                link->nexthop());
        open_contact_abort(link);
        return false;
    }
    
    // if the port wasn't specified, use the default
    if (port == 0) {
        port = NORMParameters::NORMCL_DEFAULT_PORT;
    }

    params->set_remote_addr(addr);
    params->set_remote_port(port);

    if (multicast_addr(addr)) {
        params->set_multicast_dest(true);
    }

    // create a new norm session
    NormSessionHandle session;
    if (! create_session(&session, params->nodeid(), addr, port)) {
        log_err("failed to create NORM session");
        open_contact_abort(link);
        return false;
    } else {
        log_info("new norm session %p on link %s",
                 session, link->name());
    }

    params->set_norm_session(session);

    if (params->multicast_dest()) {
        // set the multicast interface for the join
        if (params->multicast_interface().empty()) {
            log_warn("no transmit network interface specified for link %s, using default",
                     link->name());
        } else {
            NormSetMulticastInterface(session,
                                      params->multicast_interface_c_str());
        }

        params->set_backoff_factor(4.0); // per RFC for multicast nack

        if (! params->acking_list().empty()) {
            params->set_session_mode(NORMParameters::POSITIVE_ACKING);
        }

    } else {
        // for unicast links, no backoff factor,
        // smallest group size possible, and watermarking
        params->set_backoff_factor(0.0);
        params->set_group_size(10);

        if (params->ack()) {
            params->set_acking_list(intoa(addr));
            params->set_session_mode(NORMParameters::POSITIVE_ACKING);
        }
    }

    // create and initialize new receiver and sender instances
    NORMReceiver *receiver = 0;
    NORMSender *sender = 0;

    // this is where various NORMSender and NORMReceiver combinations
    // are paired according to the send_mode
    switch(params->norm_session_mode()) {
        case NORMParameters::POSITIVE_ACKING: {
            SendReliable *reliable_strategy = new SendReliable();
            sender = new NORMSender(params, contact, reliable_strategy);

            // if we're here, there's an acking list
            SendReliable *reliable_sender = dynamic_cast<SendReliable*>(sender->strategy());
            ASSERT(reliable_sender);
            reliable_sender->push_acking_nodes(sender);

            if (! params->multicast_dest()) {
                NormSetDefaultUnicastNack(session, true);
            }

            receiver = new NORMReceiver(params, link,
                                        new ReceiveWatermark(reliable_strategy));
            break;
        }
        case NORMParameters::NEGATIVE_ACKING:
        default: {
            SendBestEffort *best_effort_strategy = new SendBestEffort();
            sender = new NORMSender(params, contact, best_effort_strategy);
            receiver = new NORMReceiver(params, link, new ReceiveOnly());
            break;
        }
    }
        
    params->set_norm_sender(sender);
    params->set_norm_receiver(receiver);

    receiver->start();
    NORMSessionManager::instance()->register_receiver(receiver);

    if (! sender->init()) {
        log_err("error initializing contact");
        open_contact_abort(link, params, session);
        return false;
    }

    contact->set_cl_info(sender);
    BundleDaemon::post(new ContactUpEvent(link->contact()));
    sender->contact_up_ = true;
    sender->start();

    // if bundles are already queued on the link,
    // notify the new sender
    issue_bundle_queued(link, sender);
    
    return true;
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::close_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    NORMSender* sender = (NORMSender*)contact->cl_info();
    ASSERT(sender);

    sender->commandq_->push_back(
        NORMSender::CLMsg(NORMSender::CLMSG_BREAK_CONTACT));
    contact->set_cl_info(NULL);
    return true;
}

//----------------------------------------------------------------------
void
NORMConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void)bundle;
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    
    const ContactRef contact = link->contact();
    if (contact == NULL) {
        log_debug("bundle queued, but link %s is down",
                  link->name());
        return;
    }

    NORMSender* sender = (NORMSender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no NORMSender!!",
                 contact.object());
        return;
    }

    ASSERT(contact == sender->contact());

    sender->commandq_->push_back(
        NORMSender::CLMsg(NORMSender::CLMSG_BUNDLE_QUEUED));
}

//----------------------------------------------------------------------
void
NORMConvergenceLayer::cancel_bundle(const LinkRef& link, const BundleRef& bundle)
{
    // the bundle should be on the inflight queue for cancel_bundle to
    // be called
    if (! bundle->is_queued_on(link->inflight())) {
        log_warn("cancel_bundle *%p not on link %s inflight queue",
                 bundle.object(), link->name());
        return;
    }

    if (! link->isopen()) {
        // See note on ConnectionConvergenceLayer::cancel_bundle
        log_warn("cancel_bundle *%p but link *%p isn't open!!",
                 bundle.object(), link.object());
        BundleDaemon::post(new BundleSendCancelledEvent(bundle.object(), link));
        return;
    }

    NORMParameters *params = dynamic_cast<NORMParameters*>(link->cl_info());
    if (params == 0) {
        log_err("can't access %s link parameters", link->name());
        return;
    }

    NORMSender *sender = params->norm_sender();
    ASSERT(sender);
    sender->commandq_->push_back(
        NORMSender::CLMsg(NORMSender::CLMSG_CANCEL_BUNDLE, bundle));
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::is_queued(const LinkRef& link, Bundle* bundle)
{
    if (link->queue()->contains(bundle)) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::create_session(NormSessionHandle *session, NormNodeId nodeid,
                                     in_addr_t addr, u_int16_t port)
{
    // create a new Norm session
    *session = NormCreateSession(NORMSessionManager::instance()->norm_instance(),
                                 intoa(addr), port,
                                 nodeid ? ntohl(nodeid) : NORM_NODE_ANY);

    if (*session == NORM_SESSION_INVALID) {
        return false;
    }

    // allow multiple norm sessions on the same receive port
    NormSetRxPortReuse(*session, true);

    // force the use of an ephemeral tx port
    NormSetTxPort(*session, 0);

    return true;
}

//----------------------------------------------------------------------
bool
NORMConvergenceLayer::multicast_addr(in_addr_t addr)
{
    struct in_addr multicast_addr;
    multicast_addr.s_addr = addr;
    int first_octet = atoi(inet_ntoa(multicast_addr));

    if (first_octet >= 224 && first_octet <= 239) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------
void
NORMConvergenceLayer::issue_bundle_queued(const LinkRef &link,
                                          NORMSender *sender)
{
    for (size_t i = 0; i < link->queue()->size(); ++i) {
        sender->commandq_->push_back(
            NORMSender::CLMsg(NORMSender::CLMSG_BUNDLE_QUEUED));
    }
}

//----------------------------------------------------------------------
void
NORMConvergenceLayer::open_contact_abort(const LinkRef &link,
                                         NORMParameters *params,
                                         NormSessionHandle session)
{
    if (session != NORM_SESSION_INVALID) {
        ASSERT(params);
        params->set_norm_session(NORM_SESSION_INVALID);
        NormDestroySession(session);
    }

    BundleDaemon::post(
        new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                   ContactEvent::NO_INFO));
}

} // namespace dtn
#endif // NORM_ENABLED
