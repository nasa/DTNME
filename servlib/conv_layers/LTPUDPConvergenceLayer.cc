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
 *    results, designs or products rsulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef LTPUDP_ENABLED

#include <iostream>
#include <sys/timeb.h>
#include <sys/resource.h>
#include <climits>

#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/HexDumpBuffer.h>
#include <oasys/util/Time.h>
#include <oasys/io/IO.h>
#include <naming/EndpointID.h>
#include <naming/IPNScheme.h>

#include "LTPUDPConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTimestamp.h"
#include "bundling/SDNV.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "security/KeyDB.h"
#include "storage/BundleStore.h"




// enable or disable debug level logging in this file
#undef LTPUDPCL_LOG_DEBUG_ENABLED
//#define LTPUDPCL_LOG_DEBUG_ENABLED 1


// enable or disable debug timing in SenderGenerateDataSegsOffload
#undef LTPUDPCL_DEBUG_TIMING_ENABLED
//#define LTPUDPCL_DEBUG_TIMING_ENABLED 1



namespace dtn {

class LTPUDPConvergenceLayer::Params LTPUDPConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    int temp = (int) bucket_type_;

    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("remote_port", &remote_port_);
    a->process("bucket_type", &temp);
    a->process("rate", &rate_);
    a->process("bucket_depth", &bucket_depth_);
    a->process("inact_intvl", &inactivity_intvl_);
    a->process("retran_intvl", &retran_intvl_);
    a->process("retran_retries", &retran_retries_);
    a->process("engine_id", &engine_id_);
    a->process("wait_till_sent", &wait_till_sent_);
    a->process("hexdump", &hexdump_);
    a->process("comm_aos", &comm_aos_);
    a->process("ion_comp",&ion_comp_);
    a->process("max_sessions", &max_sessions_);
    a->process("seg_size", &seg_size_);
    a->process("agg_size", &agg_size_);
    a->process("agg_time", &agg_time_);
    a->process("sendbuf", &sendbuf_);
    a->process("recvbuf", &recvbuf_);
    // clear_stats_ not needed here
    a->process("icipher_suite", &inbound_cipher_suite_);
    a->process("icipher_keyid", &inbound_cipher_key_id_);
    a->process("icipher_engine",  &inbound_cipher_engine_);
    a->process("ocipher_suite", &outbound_cipher_suite_);
    a->process("ocipher_keyid", &outbound_cipher_key_id_);
    a->process("ocipher_engine",  &outbound_cipher_engine_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;
    }
}


//----------------------------------------------------------------------
bool
LTPSessionList::add_session(LTPSessionKey& key, LTPUDPSession* session)
{
    bool result = false;

    oasys::ScopeLock slock(&list_lock_,"add_session");  

    LIST_ITER iter = list_.find(key);

    if (iter == list_.end())
    {
        list_[key] = session;
        result = true;
    }

    return result;
}


//----------------------------------------------------------------------
LTPUDPSession*
LTPSessionList:: remove_session(LTPSessionKey& key)
{
    LTPUDPSession* result = NULL;

    oasys::ScopeLock slock(&list_lock_,"add_session");  

    LIST_ITER iter = list_.find(key);

    if (iter != list_.end())
    {
        result = iter->second;
        list_.erase(iter);
    }

    return result;
}

//----------------------------------------------------------------------
LTPUDPSession*
LTPSessionList::find_session(LTPSessionKey& key)
{
    LTPUDPSession* result = NULL;

    oasys::ScopeLock slock(&list_lock_,"add_session");  

    LIST_ITER iter = list_.find(key);

    if (iter != list_.end())
    {
        result = iter->second;
    }

    return result;
}

//----------------------------------------------------------------------
size_t
LTPSessionList::size()
{
    return list_.size();
}






//----------------------------------------------------------------------
LTPUDPConvergenceLayer::LTPUDPConvergenceLayer()
    : IPConvergenceLayer("LTPUDPConvergenceLayer", "ltpudp"),
      Thread("LTPUDPConvergenceLayer")
//      Thread("LTPUDPConvergenceLayer", Thread::DELETE_ON_EXIT)
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = LTPUDPCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = LTPUDPCL_DEFAULT_PORT;
    defaults_.bucket_type_              = oasys::RateLimitedSocket::STANDARD;
    defaults_.rate_                     = 0;         // unlimited
    defaults_.bucket_depth_             = 65535 * 8; // default for standard bucket
    defaults_.inactivity_intvl_         = 30;
    defaults_.retran_intvl_             = 3;
    defaults_.retran_retries_           = 3;
    defaults_.engine_id_                = 0;
    defaults_.wait_till_sent_           = true;
    defaults_.hexdump_                  = false;
    defaults_.comm_aos_                 = true;
    defaults_.ion_comp_                 = true;
    defaults_.max_sessions_             = 100;
    defaults_.seg_size_                 = 1400;
    defaults_.agg_size_                 = 100000;
    defaults_.agg_time_                 = 1000;  // milliseconds (1 sec)
    defaults_.recvbuf_                  = 0;
    defaults_.sendbuf_                  = 0;
    defaults_.clear_stats_              = false;
    defaults_.inbound_cipher_suite_     = -1;
    defaults_.inbound_cipher_key_id_    = -1;
    defaults_.inbound_cipher_engine_    = "";
    defaults_.outbound_cipher_suite_    = -1;
    defaults_.outbound_cipher_key_id_   = -1;
    defaults_.outbound_cipher_engine_   = "";

    next_hop_addr_                      = INADDR_NONE;
    next_hop_port_                      = 0;
    next_hop_flags_                     = 0;
    sender_                             = NULL;
    receiver_                           = NULL;

    timer_processor_                    = NULL;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::~LTPUDPConvergenceLayer()
{
    shutdown();

    if (timer_processor_ != NULL) {
        wait_for_stopped(timer_processor_);
        delete timer_processor_;
        timer_processor_ = NULL;
    }
    if (receiver_ != NULL) {
        wait_for_stopped(receiver_);
        delete receiver_;
        receiver_ = NULL;
    }
    if (sender_   != NULL) {
        wait_for_stopped(sender_);
        delete sender_;
        sender_ = NULL;
    }

    wait_for_stopped(this);
}

void
LTPUDPConvergenceLayer::wait_for_stopped(oasys::Thread* thread)
{
    int ctr = 0;
    while (!thread->is_stopped() && (ctr++ < 10))
    {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::shutdown()
{
    set_should_stop();

    if (timer_processor_ != NULL) {
        timer_processor_->set_should_stop();
    }
    if (receiver_ != NULL) {
        receiver_->do_shutdown();
    }
    if (sender_   != NULL) {
        sender_->do_shutdown();
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::run()
{ 
    char threadname[16] = "LtpAosCounter";
    pthread_setname_np(pthread_self(), threadname);
   

    aos_counter_ = 0;
    log_debug("CounterThread run invoked");
    while (!should_stop()) {
        sleep(1);
        if (session_params_.comm_aos_) aos_counter_++;
    }        
}

//----------------------------------------------------------------------
CLInfo*
LTPUDPConvergenceLayer::new_link_params()
{
    return new LTPUDPConvergenceLayer::Params(LTPUDPConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&LTPUDPConvergenceLayer::defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    char icipher_from_engine[50];
    char ocipher_from_engine[50];
    oasys::OptParser p;
    int temp = 0;

    string ocipher_engine_str = "";
    string icipher_engine_str = "";

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::IntOpt("bucket_type", &temp));
    p.addopt(new oasys::UInt64Opt("rate", &params->rate_));
    p.addopt(new oasys::UInt64Opt("bucket_depth", &params->bucket_depth_));
    p.addopt(new oasys::IntOpt("inact_intvl", &params->inactivity_intvl_));
    p.addopt(new oasys::IntOpt("retran_intvl", &params->retran_intvl_));
    p.addopt(new oasys::IntOpt("retran_retries", &params->retran_retries_));
    p.addopt(new oasys::UInt64Opt("engine_id", &params->engine_id_));
    p.addopt(new oasys::BoolOpt("wait_till_sent",&params->wait_till_sent_));
    p.addopt(new oasys::BoolOpt("hexdump",&params->hexdump_));
    p.addopt(new oasys::BoolOpt("comm_aos",&params->comm_aos_));
    p.addopt(new oasys::BoolOpt("ion_comp",&params->ion_comp_));
    p.addopt(new oasys::UIntOpt("max_sessions", &params->max_sessions_));
    p.addopt(new oasys::IntOpt("seg_size", &params->seg_size_));
    p.addopt(new oasys::IntOpt("agg_size", &params->agg_size_));
    p.addopt(new oasys::IntOpt("agg_time", &params->agg_time_));
    p.addopt(new oasys::UIntOpt("recvbuf", &params->recvbuf_));
    p.addopt(new oasys::UIntOpt("sendbuf", &params->sendbuf_));
    p.addopt(new oasys::BoolOpt("clear_stats", &params->clear_stats_));

    p.addopt(new oasys::IntOpt("ocipher_suite", &params->outbound_cipher_suite_));
    p.addopt(new oasys::IntOpt("ocipher_keyid", &params->outbound_cipher_key_id_));
    p.addopt(new oasys::StringOpt("ocipher_engine", &ocipher_engine_str));
    p.addopt(new oasys::IntOpt("icipher_suite", &params->inbound_cipher_suite_));
    p.addopt(new oasys::IntOpt("icipher_keyid", &params->inbound_cipher_key_id_));
    p.addopt(new oasys::StringOpt("icipher_engine", &icipher_engine_str));

#ifdef LTPUDPCL_DEBUG_TIMING_ENABLED
    p.addopt(new oasys::BoolOpt("dbg_timing", &params->dbg_timing_));
#endif

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }


    params->bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;

    if (icipher_engine_str.length() > 0) {
        params->inbound_cipher_engine_.assign(icipher_engine_str);
    } else {
        sprintf(icipher_from_engine, "%" PRIu64, params->engine_id_);
        params->inbound_cipher_engine_ = icipher_from_engine;
    } 
    if (ocipher_engine_str.length() > 0) {
        params->outbound_cipher_engine_.assign(ocipher_engine_str);
    } else {
        sprintf(ocipher_from_engine, "%" PRIu64, params->engine_id_);
        params->outbound_cipher_engine_ = ocipher_from_engine;
    } 

    if (params->outbound_cipher_suite_ == 0 || params->outbound_cipher_suite_ == 1 ) {
         if (params->outbound_cipher_key_id_ == -1)return false;
    } 
    if (params->inbound_cipher_suite_ == 0 || params->inbound_cipher_suite_ == 1 ) {
         if (params->inbound_cipher_key_id_ == -1)return false;
    } 

    return true;
};

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("LTPUDPCOnvergenceLayer::interface_up: %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = LTPUDPConvergenceLayer::defaults_;
    const char* invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // check that the local interface / port are valid
    if (params.local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0");
        return false;
    }

    if (params.local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }

    session_params_ = params;
 
    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(&params,this);
    receiver->logpathf("%s/iface/receiver/%s", logpath_, iface->name().c_str());
    
    if (receiver->bind(params.local_addr_, params.local_port_) != 0) {
        return false; // error log already emitted
    }
    
    // check if the user specified a remote addr/port to connect to
    if (params.remote_addr_ != INADDR_NONE) {
        if (receiver->connect(params.remote_addr_, params.remote_port_) != 0) {
            return false; // error log already emitted
        }
    }
   
    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::interface_activate(Interface* iface)
{
    log_debug("activating interface %s", iface->name().c_str());

    // start listening and then start the thread to loop calling accept()
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->start();
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->set_should_stop();
    receiver->interrupt_from_io();
    
    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }

    delete receiver;
    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params * params = &((Receiver*)iface->cl_info())->cla_params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d",
                 intoa(params->local_addr_), params->local_port_);
    buf->appendf("\tinactivity_intvl: %d", params->inactivity_intvl_);
    buf->appendf("\tretran_intvl: %d", params->retran_intvl_);
    buf->appendf("\tretran_retries: %d", params->retran_retries_);

    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tnot connected");
    }


    //buf->appendf("Sender Sessions: size() (first 5): ");
    buf->appendf("Sender Sessions: size(): ");
    if (NULL != sender_) 
        sender_->Dump_Sessions(buf);
    else
        buf->appendf("\nsender_ not initialized\n");

    //buf->appendf("\nReceiver Sessions: size() (first 5): ");
    buf->appendf("\nReceiver Sessions: size(): ");
    if (NULL != receiver_)
        receiver_->Dump_Sessions(buf);
    else
        buf->appendf("\nreceiver not initialized\n");

    
    buf->appendf("\nStatistics:\n");
    buf->appendf("              Sessions     Sessns      Data Segments        Report Segments    RptAck   Cancel By Sendr  Cancel By Recvr  CanAck  Bundles   Bundles   CanBRcv\n");
    buf->appendf("           Total /  Max    DS R-X     Total  / ReXmit       Total / ReXmit     Total    Total / ReXmit   Total / ReXmit   Total   Success   Failure   But Got\n");
    buf->appendf("         ----------------  -------  --------------------  ------------------  --------  ---------------  ---------------  ------  --------  --------  -------\n");
    if (NULL != sender_) sender_->Dump_Statistics(buf);
    if (NULL != receiver_) receiver_->Dump_Statistics(buf);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port = 0;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);
    
    log_debug("LTPUDPConvergenceLayer::init_link: %s link %s", link->type_str(), link->nexthop());

    // Parse the nexthop address but don't bail if the parsing fails,
    // since the remote host may not be resolvable at initialization
    // time and we retry in open_contact
    parse_nexthop(link->nexthop(), &addr, &port);

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = new Params(defaults_);
    params->local_addr_ = INADDR_NONE;
    params->local_port_ = 0;

    const char* invalid;
    if (! parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    session_params_ = *params;

    this->start();  // start the TimerCounter thread...

    if (link->params().mtu_ > MAX_BUNDLE_LEN) {
        log_err("error parsing link options: mtu %d > maximum %d",
                link->params().mtu_, MAX_BUNDLE_LEN);
        delete params;
        return false;
    }

    link->set_cl_info(&session_params_);
    delete params;

    link_ref_ = link;

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("LTPUDPConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::reconfigure_link(const LinkRef& link,
                                       int argc, const char* argv[]) 
{
    (void) link;

    const char* invalid;
    if (! parse_params(&session_params_, argc, argv, &invalid)) {
        log_err("reconfigure_link: invalid parameter %s", invalid);
        return false;
    }

    // inform the sender of the reconfigure
    if (NULL != sender_) {
        sender_->reconfigure();
    }

    if (session_params_.clear_stats_) {
        if (NULL != sender_) sender_->clear_statistics();
        if (NULL != receiver_) receiver_->clear_statistics();
        session_params_.clear_stats_ = false;
    }

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::reconfigure_link(const LinkRef& link,
                                         AttributeVector& params)
{
    (void) link;

    AttributeVector::iterator iter;
    for (iter = params.begin(); iter != params.end(); iter++) {
        if (iter->name().compare("comm_aos") == 0) { 
            session_params_.comm_aos_ = iter->bool_val();
            log_debug("reconfigure_link - new comm_aos = %s", session_params_.comm_aos_ ? "true": "false");
        } else if (iter->name().compare("rate") == 0) { 
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                session_params_.rate_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                session_params_.rate_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                session_params_.rate_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                session_params_.rate_ = iter->int64_val();
            }
            log_debug("reconfigure_link - new rate = %" PRIu64, session_params_.rate_);
        } else if (iter->name().compare("bucket_depth") == 0) { 
            if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                session_params_.bucket_depth_ = iter->u_int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INTEGER) {
                session_params_.bucket_depth_ = iter->int_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_UNSIGNED_INT64) {
                session_params_.bucket_depth_ = iter->uint64_val();
            } else if (iter->type() == NamedAttribute::ATTRIBUTE_TYPE_INT64) {
                session_params_.bucket_depth_ = iter->int64_val();
            }
            log_debug("reconfigure_link - new bucket depth = %" PRIu64, session_params_.bucket_depth_);
        } else if (iter->name().compare("bucket_type") == 0) { 
            session_params_.bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) iter->int_val();
            log_debug("reconfigure_link - new bucket type = %d", (int) session_params_.bucket_type_);
        } else if (iter->name().compare("clear_stats") == 0) { 
            session_params_.clear_stats_ = iter->bool_val();
        }
        // any others to change on the fly through the External Router interface?

        else {
            log_debug("reconfigure_link - unknown parameter: %s", iter->name().c_str());
        }
    }    

    // inform the sender of the reconfigure
    if (NULL != sender_) {
        sender_->reconfigure();
    }

    if (session_params_.clear_stats_) {
        if (NULL != sender_) sender_->clear_statistics();
        if (NULL != receiver_) receiver_->clear_statistics();
        session_params_.clear_stats_ = false;
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    Params* params = (Params*)link->cl_info();
    ASSERT(params == &session_params_);

    buf->appendf("local_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
    buf->appendf("remote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);

    if (NULL == sender_ ) {
        buf->appendf("transmit rate: %" PRIu64 "\n", params->rate_);
        if (0 == params->bucket_type_) 
            buf->appendf("bucket_type: TokenBucket\n");
        else
            buf->appendf("bucket_type: TokenBucketLeaky\n");
        buf->appendf("bucket_depth: %" PRIu64 "\n", params->bucket_depth_);
        buf->appendf("wait_till_sent: %d\n",(int) params->wait_till_sent_);
    } else {
        sender_->dump_link(buf);
    }

    buf->appendf("inactivity_intvl: %d\n", params->inactivity_intvl_);
    buf->appendf("retran_intvl: %d\n", params->retran_intvl_);
    buf->appendf("retran_retries: %d\n", params->retran_retries_);
    buf->appendf("agg_size: %d (bytes)\n",params->agg_size_);
    buf->appendf("agg_time: %d (milliseconds)\n",params->agg_time_);
    buf->appendf("seg_size: %d (bytes)\n",params->seg_size_);
    buf->appendf("Max Sessions: %u\n",params->max_sessions_);
    buf->appendf("Comm_AOS: %d\n",(int) params->comm_aos_);
    buf->appendf("ION_Compatible: %d\n",(int) params->ion_comp_);
    buf->appendf("hexdump: %d\n",(int) params->hexdump_);
#ifdef LTPUDPCL_DEBUG_TIMING_ENABLED
    buf->appendf("dbg_timing: %s (debug timing in LtpSndrGenDS\n", params->dbg_timing_ ? "true" : "false");
#endif

    if (params->outbound_cipher_suite_ != -1) {
        buf->appendf("outbound_cipher_suite: %d\n",(int) params->outbound_cipher_suite_);
        if (params->outbound_cipher_suite_ != 255) {
            buf->appendf("outbound_cipher_key_id: %d\n",(int) params->outbound_cipher_key_id_);
            buf->appendf("outbound_cipher_engine_: %s\n", params->outbound_cipher_engine_.c_str());
        }
    }

    if (params->inbound_cipher_suite_ != -1) {
        buf->appendf("inbound_cipher_suite: %d\n",(int) params->inbound_cipher_suite_);
        if (params->inbound_cipher_suite_ != 255) {
            buf->appendf("inbound_cipher_key_id_: %d\n",(int) params->inbound_cipher_key_id_);
            buf->appendf("inbound_cipher_engine_: %s\n", params->inbound_cipher_engine_.c_str());
        }
    }


    //buf->appendf("Sender Sessions: size() (first 5): ");
    buf->appendf("Sender Sessions: size(): ");
    if (NULL != sender_) 
        sender_->Dump_Sessions(buf);
    else
        buf->appendf("\nsender_ not initialized\n");

    //buf->appendf("\nReceiver Sessions: size() (first 5): ");
    buf->appendf("\nReceiver Sessions: size(): ");
    if (NULL != receiver_) {
        receiver_->Dump_Sessions(buf);
        buf->appendf("\nReceiver: 10M Buffers: %d  Raw data bytes queued: %zu  Bundle extraction bytes queued: %zu\n", 
                     receiver_->buffers_allocated(), receiver_->process_data_bytes_queued(), 
                     receiver_->process_bundles_bytes_queued());
    } else {
        buf->appendf("\nreceiver not initialized\n");
    }

    if (receiver_ != NULL)
        receiver_->Dump_Bundle_Processor(buf);

    if (sender_ != NULL)
        sender_->dump_queue_sizes(buf);

    buf->appendf("\n");

    buf->appendf("\nStatistics:\n");
    buf->appendf("              Sessions     Sessns      Data Segments        Report Segments    RptAck   Cancel By Sendr  Cancel By Recvr  CanAck  Bundles   Bundles   CanBRcv\n");
    buf->appendf("           Total /  Max    DS R-X     Total  / ReXmit       Total / ReXmit     Total    Total / ReXmit   Total / ReXmit   Total   Success   Failure   But Got\n");
    buf->appendf("         ----------------  -------  --------------------  ------------------  --------  ---------------  ---------------  ------  --------  --------  -------\n");
    if (NULL != sender_) sender_->Dump_Statistics(buf);
    if (NULL != receiver_) receiver_->Dump_Statistics(buf);

    buf->appendf("\n");

}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::open_contact(const ContactRef& contact)
{
    in_addr_t addr;
    u_int16_t port;

    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
    
    log_debug("LTPUDPConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());


    // Initialize and start the Sender thread
    
    // parse out the address / port from the nexthop address
    if (! parse_nexthop(link->nexthop(), &addr, &port)) {
        log_err("invalid next hop address '%s'", link->nexthop());
        return false;
    }

    // make sure it's really a valid address
    if (addr == INADDR_ANY || addr == INADDR_NONE) {
        log_err("can't lookup hostname in next hop address '%s'",
                link->nexthop());
        return false;
    }

    // if the port wasn't specified, use the default
    if (port == 0) {
        port = LTPUDPCL_DEFAULT_PORT;
    }

    next_hop_addr_  = addr;
    next_hop_port_  = port;
    next_hop_flags_ = MSG_DONTWAIT;

    Params* params = (Params*)link->cl_info();
   
    session_params_ = *params;
 
    // create a new sender structure
    Sender* sender = new Sender(link->contact(),this);
  
    if (!sender->init(&session_params_, addr, port)) {

        log_err("error initializing contact");
        BundleDaemon::post(
            new LinkStateChangeRequest(link, Link::UNAVAILABLE,
                                       ContactEvent::NO_INFO));
        delete sender;
        return false;
    }
        
    contact->set_cl_info(sender);
    BundleDaemon::post(new ContactUpEvent(link->contact()));

    sender_ = sender;    
    sender->start();


    // Initialize and start the Receive rthread

    // check that the local interface / port are valid
    if (session_params_.local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0"); 
        return false;
    }

    if (session_params_.local_port_ == 0) { 
        log_err("invalid local port setting of 0"); 
        return false;
    }

    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(&session_params_,this);
    receiver->logpathf("%s/link/receiver/%s", logpath_, link->name());
    
    if (receiver->bind(session_params_.local_addr_, session_params_.local_port_) != 0) { 
        log_err("error binding to %s:%d: %s",
                 intoa(session_params_.local_addr_), session_params_.local_port_,
                 strerror(errno));
        return false; // error log already emitted 
    } else {
        log_info("socket bind occured %s:%d",
                 intoa(session_params_.local_addr_), session_params_.local_port_);
    }

    // start the thread which automatically listens for data
    receiver_ = receiver; 
    receiver->start();


    // Initialize and start the thread that processes timer events
    timer_processor_ = new TimerProcessor();

    return true;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::close_contact(const ContactRef& contact)
{
    Sender* sender = (Sender*)contact->cl_info();

    log_info("close_contact *%p", contact.object());
    log_crit("close_contact *%p", contact.object()); //dzdebug

    if (timer_processor_) {
        timer_processor_->set_should_stop();
        timer_processor_ = NULL;
    }

    if (receiver_ != NULL) {
        receiver_->do_shutdown();
        wait_for_stopped(receiver_);
        delete receiver_;
        receiver_ = NULL;
    }

    if (sender_   != NULL) {
        if (sender != sender_) {
            log_crit("%s - contact sender (%p) does not equal expected sender_ (%p)",
                     __FUNCTION__, sender, sender_);
        }
    log_crit("close_contact *%p - stopping sender", contact.object()); //dzdebug
        sender_->do_shutdown();
        wait_for_stopped(sender_);
        delete sender_;
        sender_ = NULL;
    }
    log_crit("close_contact *%p - clearing contact->cl_info", contact.object()); //dzdebug
    contact->set_cl_info(NULL);

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void) link;
    (void) bundle;
    return;
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::pop_queued_bundle(const LinkRef& link)
{
    bool result = false;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    if (!session_params_.comm_aos_)return result;
     
    const ContactRef contact = link->contact();

    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return result;
    }

    BundleRef lbundle = link->queue()->front();

    //XXX/dz - TODO: check to see if bundle has been deleted...


    if (lbundle != NULL) {
        result = true;
        sender->send_bundle(lbundle, next_hop_addr_, next_hop_port_);
    }

    return result;
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Process_Sender_Segment(LTPUDPSegment * seg)
{
    if (!should_stop() && (sender_ != NULL)) {
        sender_->Post_Segment_to_Process(seg);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Send(LTPUDPSegment * send_segment,int parent_type)
{
    if (!should_stop() && (sender_ != NULL)) {
        sender_->Send(send_segment,parent_type);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Send_Low_Priority(LTPUDPSegment * send_segment,int parent_type)
{
    if (!should_stop() && (sender_ != NULL)) {
        sender_->Send_Low_Priority(send_segment, parent_type);
    }
}

//----------------------------------------------------------------------
u_int32_t 
LTPUDPConvergenceLayer::Inactivity_Interval()
{
     return session_params_.inactivity_intvl_;
}

//----------------------------------------------------------------------
int 
LTPUDPConvergenceLayer::Retran_Interval()
{
     return session_params_.retran_intvl_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Cleanup_Session_Receiver(string session_key, string segment_key, int segment_type)
{
    if (!should_stop() && (receiver_ != NULL)) {
        oasys::ScopeLock l(receiver_->session_lock(), __FUNCTION__);
        receiver_->cleanup_receiver_session(session_key, segment_key, segment_type);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Cleanup_RS_Receiver(string session_key, u_int64_t rs_key, int segment_type)
{
    if (!should_stop() && (receiver_ != NULL)) {
        oasys::ScopeLock l(receiver_->session_lock(), __FUNCTION__);
        char temp[24];
        snprintf(temp, sizeof(temp), "%20.20" PRIu64, rs_key);
        string str_key = temp;
        receiver_->cleanup_RS(session_key, str_key, segment_type);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Cleanup_Session_Sender(string session_key, string segment_key, int segment_type)
{
    if (!should_stop() && (sender_ != NULL)) {
        oasys::ScopeLock l(sender_->session_lock(), __FUNCTION__);
        sender_->cleanup_sender_session(session_key, segment_key, segment_type);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Post_Timer_To_Process(oasys::Timer* event)
{
    if (! should_stop()) {
        if (timer_processor_) {
            timer_processor_->post(event);
        } else {
            delete event;
        }
    }
    else
    {
        delete event;
    }
}

//----------------------------------------------------------------------
u_int32_t LTPUDPConvergenceLayer::Receiver::Outbound_Cipher_Suite()
{
    return cla_params_.outbound_cipher_suite_;
}
//----------------------------------------------------------------------
u_int32_t LTPUDPConvergenceLayer::Receiver::Outbound_Cipher_Key_Id()
{
    return cla_params_.outbound_cipher_key_id_;
}
//----------------------------------------------------------------------
string LTPUDPConvergenceLayer::Receiver::Outbound_Cipher_Engine()
{
    return cla_params_.outbound_cipher_engine_;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::Receiver(LTPUDPConvergenceLayer::Params* params, LTPUDPCallbackIF * parent)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/ltpudp/receiver")),
      UDPClient("/dtn/cl/ltpudp/receiver"),
      Thread("LTPUDPConvergenceLayer::Receiver")
{
    logfd_         = false;
    cla_params_    = *params;
    parent_        = parent;

    memset(&stats_, 0, sizeof(stats_));

    sessions_state_ds = 0;
    sessions_state_rs = 0;
    sessions_state_cs = 0;


    // bump up the receive buffer size
    params_.recv_bufsize_ = cla_params_.recvbuf_;

    bundle_processor_ = new RecvBundleProcessor(this, parent->link_ref());
    bundle_processor_->start();

    data_process_offload_ = new RecvDataProcessOffload(this);
    data_process_offload_->start();

    blank_session_    = new LTPUDPSession(0UL, 0UL, parent_,  PARENT_RECEIVER);
    blank_session_->Set_Outbound_Cipher_Suite(cla_params_.outbound_cipher_suite_);
    blank_session_->Set_Outbound_Cipher_Key_Id(cla_params_.outbound_cipher_key_id_);
    blank_session_->Set_Outbound_Cipher_Engine(cla_params_.outbound_cipher_engine_);

    // create an initial pool of DataProcObejcts that may grow as needed
    buffers_allocated_ = 0; 
    for (int ix=0; ix<100; ++ix) {
        DataProcObject* obj = new DataProcObject();
        dpo_pool_.push_back(obj);
    }
    // force creating first buffer
    get_recv_buffer();

}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::~Receiver()
{
    do_shutdown();

    if (bundle_processor_ != NULL) {
        delete bundle_processor_;
        bundle_processor_ = NULL;
    }

    if (data_process_offload_ != NULL) {
        delete data_process_offload_;
        data_process_offload_ = NULL;
    }


    DataProcObject* obj;
    while (!dpo_pool_.empty()) {
        obj = dpo_pool_.front();
        dpo_pool_.pop_front();
        delete obj;
    }

    BufferObject* bo;
    while (!buffer_pool_.empty()) {
        bo = buffer_pool_.front();
        buffer_pool_.pop_front();
        free(bo->buffer_);
        delete bo;
    }

    while (!used_buffer_pool_.empty()) {
        bo = used_buffer_pool_.front();
        used_buffer_pool_.pop_front();
        free(bo->buffer_);
        delete bo;
    }

    delete blank_session_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::do_shutdown()
{
    set_should_stop();

    //TODO - wait for processing to finish...

    if (data_process_offload_ != NULL) {
        data_process_offload_->set_should_stop();

        while (!data_process_offload_->is_stopped() ) {
            usleep(10000);
        }
    }

    if (bundle_processor_ != NULL) {
        bundle_processor_->set_should_stop();

        while (!bundle_processor_->is_stopped() ) {
            usleep(10000);
        }
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::Dump_Sessions(oasys::StringBuffer* buf)
{
    buf->appendf("%zu :  DS: %zu RS: %zu CS: %zu\n",
                 ltpudp_rcvr_sessions_.size(), 
                 sessions_state_ds, sessions_state_rs, sessions_state_cs);

//dzdebug - need to get the session lock if going to list some of the sessions in detail
/*
    DS_MAP::iterator segment_iter;
    SESS_MAP::iterator session_iter;
    int count = 0;
    int num_segs = 0;
    int max_segs = 0;
    size_t last_stop_byte = 0;
    
    for(session_iter =  ltpudp_rcvr_sessions_.begin(); 
        session_iter != ltpudp_rcvr_sessions_.end(); 
        session_iter ++)
    {
        LTPUDPSession * session = session_iter->second;

        buf->appendf("    Session: %s   %s\n",session_iter->first.c_str(), session->Get_Session_State());
        if (session->Red_Segments().size() > 0) {
            num_segs = 0;
            last_stop_byte = 0;
            max_segs = session->Red_Segments().size() - 1;
            buf->appendf("        Red Segments: %d  (first 5, gaps & last)\n",(int) session->Red_Segments().size());
            if ((int) session->Inbound_Cipher_Suite() != -1)
                buf->appendf("             Inbound_Cipher_Suite: %d\n",session->Inbound_Cipher_Suite());
            for(segment_iter =  session->Red_Segments().begin(); 
                segment_iter != session->Red_Segments().end(); 
                segment_iter ++)
            {
                LTPUDPDataSegment * segment = segment_iter->second;
                if (segment->Start_Byte() != last_stop_byte) {
                    buf->appendf("             gap: %zu - %zu\n",
                                 last_stop_byte, segment->Start_Byte());
                }
                if (num_segs < 5 || num_segs == max_segs) {
                    if (num_segs > 5) {
                        buf->appendf("             ...\n");
                    }
                    buf->appendf("             key[%d]: %s\n", 
                                 num_segs, segment->Key().c_str()); 
                }
                last_stop_byte = segment->Stop_Byte() + 1;
                ++num_segs;
            }
        } else {
            buf->appendf("        Red Segments: 0\n");
        }
        if (session->Green_Segments().size() > 0) {
            num_segs = 0;
            last_stop_byte = 0;
            max_segs = session->Green_Segments().size() - 1;
            buf->appendf("      Green Segments: %d  (first 5, gaps & last)\n",(int) session->Green_Segments().size());
            if ((int) session->Inbound_Cipher_Suite() != -1)
                buf->appendf("             Inbound_Cipher_Suite: %d\n",session->Inbound_Cipher_Suite());
            for(segment_iter =  session->Green_Segments().begin(); 
                segment_iter != session->Green_Segments().end(); 
                segment_iter ++)
            {
                LTPUDPDataSegment * segment = segment_iter->second;
                if (segment->Start_Byte() != last_stop_byte) {
                    buf->appendf("             gap: %zu - %zu\n",
                                 last_stop_byte, segment->Start_Byte());
                }
                if (num_segs < 5 || num_segs == max_segs) {
                    if (num_segs > 5) {
                        buf->appendf("             ...\n");
                    }
                    buf->appendf("             key[%d]: %s\n", 
                                 num_segs, segment->Key().c_str()); 
                }
                last_stop_byte = segment->Stop_Byte() + 1;
                ++num_segs;
            }
        } else {
            buf->appendf("      Green Segments: 0\n");
           
        }
        count++;
        if (count >= 5)break;
    }
*/

}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::Dump_Statistics(oasys::StringBuffer* buf)
{
//              Sessions     Sessns      Data Segments        Report Segments    RptAck   Cancel By Sendr  Cancel By Recvr  CanAck  Bundles   Bundles   CanBRcv
//           Total /  Max    DS R-X     Total  / ReXmit       Total / ReXmit     Total    Total / ReXmit   Total / ReXmit   Total   Success   Failure   But Got
//         ----------------  -------  --------------------  ------------------  --------  ---------------  ---------------  ------  --------  --------  -------
//Sender   12345678 / 12345  1234567  123456789 / 12345678  12345678 / 1234567  12345678  123456 / 123456  123456 / 123456  123456  12345678  12345678  1234567
//Receiver 12345678 / 12345  1234567  123456789 / 12345678  12345678 / 1234567  12345678  123456 / 123456  123456 / 123456  123456  12345678  12345678  1234567

    buf->appendf("Receiver %8" PRIu64 " / %5" PRIu64 "  %7" PRIu64 "  %9" PRIu64 " / %8" PRIu64 "  %8" PRIu64 " / %7" PRIu64
                 "  %8" PRIu64 "  %6" PRIu64 " / %6" PRIu64 "  %6" PRIu64 " / %6" PRIu64 "  %6" PRIu64 "  %8" PRIu64 "  %8" PRIu64 "  %7" PRIu64 "\n",
                 stats_.total_sessions_, stats_.max_sessions_,
                 stats_.ds_session_resends_, stats_.total_rcv_ds_, stats_.ds_segment_resends_,
                 stats_.total_snt_rs_, stats_.rs_session_resends_, stats_.total_rcv_ra_,
                 stats_.total_rcv_css_, uint64_t(0), stats_.total_snt_csr_, stats_.cs_session_resends_,
                 stats_.total_snt_ca_,
                 stats_.bundles_success_, uint64_t(0), stats_.cs_session_cancel_by_receiver_but_got_it_);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::Dump_Bundle_Processor(oasys::StringBuffer* buf)
{
    if (!should_stop()) {
        buf->appendf("Bundle Processor Queue Size: %zu\n", bundle_processor_->queue_size());
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::clear_statistics()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Receiver::build_CS_segment(LTPUDPSession * session, int segment_type, u_char reason_code)
{
    LTPUDPCancelSegment * cancel_segment = new LTPUDPCancelSegment(session, segment_type, reason_code, parent_);

    update_session_counts(session, LTPUDPSession::LTPUDP_SESSION_STATE_CS);

    cancel_segment->Set_Retrans_Retries(0);
    cancel_segment->Create_Retransmission_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER,
                                                LTPUDPSegment::LTPUDP_SEGMENT_CS_TO);

    session->Set_Cancel_Segment(cancel_segment);

    if (session->RedProcessed()) {
      ++stats_.cs_session_cancel_by_receiver_but_got_it_;
    } else {
      ++stats_.cs_session_cancel_by_receiver_;
    }
    ++stats_.total_snt_csr_;

    parent_->Send(cancel_segment,PARENT_RECEIVER);
}

//----------------------------------------------------------------------
RS_MAP::iterator 
LTPUDPConvergenceLayer::Receiver::track_RS(LTPUDPSession * session, LTPUDPReportSegment * this_segment)
{
    RS_MAP::iterator report_segment_iterator = session->Report_Segments().find(this_segment->Key().c_str());
    if (report_segment_iterator == session->Report_Segments().end())
    {
        session->Report_Segments().insert(std::pair<std::string, LTPUDPReportSegment *>(this_segment->Key(), this_segment));
        report_segment_iterator = session->Report_Segments().find(this_segment->Key().c_str());
    }
    return report_segment_iterator;
}

//----------------------------------------------------------------------
LTPUDPReportSegment * 
LTPUDPConvergenceLayer::Receiver::scan_RS_list(LTPUDPSession * this_session)
{
    LTPUDPReportSegment * report_segment = (LTPUDPReportSegment *) NULL;

    RS_MAP::iterator report_seg_iterator;

    for(report_seg_iterator =  this_session->Report_Segments().begin(); 
        report_seg_iterator != this_session->Report_Segments().end(); 
        report_seg_iterator++)
    {
        report_segment  = report_seg_iterator->second;
        if (this_session->Checkpoint_ID() == report_segment->Checkpoint_ID())return report_segment;
    }    
    return (LTPUDPReportSegment *) NULL;
}

//----------------------------------------------------------------------
SESS_MAP::iterator
LTPUDPConvergenceLayer::Receiver::track_receiver_session(u_int64_t engine_id, u_int64_t session_id, bool create_flag)
{
    SESS_MAP::iterator session_iterator; 
    char key[45];
    sprintf(key,"%20.20" PRIu64 ":%20.20" PRIu64,engine_id,session_id);
    string insert_key = key;

    session_iterator = ltpudp_rcvr_sessions_.find(insert_key);
    if (session_iterator == ltpudp_rcvr_sessions_.end())
    {
        if (create_flag) {
            LTPUDPSession * new_session = new LTPUDPSession(engine_id, session_id, parent_,  PARENT_RECEIVER);
            new_session->Set_Inbound_Cipher_Suite(cla_params_.inbound_cipher_suite_);
            new_session->Set_Inbound_Cipher_Key_Id(cla_params_.inbound_cipher_key_id_);
            new_session->Set_Inbound_Cipher_Engine(cla_params_.inbound_cipher_engine_);
            new_session->Set_Outbound_Cipher_Suite(cla_params_.outbound_cipher_suite_);
            new_session->Set_Outbound_Cipher_Key_Id(cla_params_.outbound_cipher_key_id_);
            new_session->Set_Outbound_Cipher_Engine(cla_params_.outbound_cipher_engine_);
            ltpudp_rcvr_sessions_.insert(std::pair<std::string, LTPUDPSession*>(insert_key, new_session));
            session_iterator = ltpudp_rcvr_sessions_.find(insert_key);
            if (ltpudp_rcvr_sessions_.size() > stats_.max_sessions_) {
                stats_.max_sessions_ = ltpudp_rcvr_sessions_.size();
            }
            ++stats_.total_sessions_;
        } else {
        }
    } else {
    }
    return session_iterator;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::cleanup_receiver_session(string session_key, string segment_key, 
                                                           int segment_type)
{ 
    (void) segment_key;

    // lock should've occured before this routine is hit
    ASSERT(session_lock_.is_locked_by_me());

    time_t time_left;

    LTPUDPCancelSegment * cs_segment;

    SESS_MAP::iterator cleanup_iterator = ltpudp_rcvr_sessions_.find(session_key);

    DS_MAP::iterator ds_segment_iterator;
    RS_MAP::iterator rs_segment_iterator;

    if (cleanup_iterator != ltpudp_rcvr_sessions_.end())
    {
        LTPUDPSession * ltpsession = cleanup_iterator->second;
        
        ltpsession->Cancel_Inactivity_Timer();

        switch(segment_type)
        {
            case LTPUDPSegment::LTPUDP_SEGMENT_UNKNOWN: // treat as a DS since the Inactivity timer sends 0
            case LTPUDPSegment::LTPUDP_SEGMENT_DS:

                time_left = ltpsession->Last_Packet_Time() + cla_params_.inactivity_intvl_;
                time_left = time_left - parent_->AOS_Counter();

                if (time_left > 0) /* timeout occured but retries are not exhausted */
                {
                    ltpsession->Start_Inactivity_Timer(time_left); // catch the remaining seconds...
                } else {
                    ltpsession->RemoveSegments();   
                    build_CS_segment(ltpsession,LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                                     LTPUDPCancelSegment::LTPUDP_CS_REASON_RXMTCYCEX);
                }
                break;

            case LTPUDPSegment::LTPUDP_SEGMENT_CS_TO:

                cs_segment = ltpsession->Cancel_Segment();
                cs_segment->Cancel_Retransmission_Timer();
                cs_segment->Increment_Retries();

                if (cs_segment->Retrans_Retries() <= cla_params_.retran_retries_) {
                    cs_segment->Create_Retransmission_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER, 
                                                            LTPUDPSegment::LTPUDP_SEGMENT_CS_TO);

                    ++stats_.cs_session_resends_;

                    parent_->Send(cs_segment,PARENT_RECEIVER);
                    return; 
                } else {
                    update_session_counts(ltpsession, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
                    delete ltpsession;
                    ltpudp_rcvr_sessions_.erase(cleanup_iterator);
                }

                break;

            case LTPUDPSegment::LTPUDP_SEGMENT_CAS_BR:
                update_session_counts(ltpsession, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
                delete ltpsession;
                ltpudp_rcvr_sessions_.erase(cleanup_iterator);
                break;

            default:
                log_err("cleanup_receiver_session - unhandled segment type: %d", segment_type);
                break;
        }
    }

    return;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::cleanup_RS(string session_key, string segment_key, 
                                             int segment_type)
{ 
    // lock should've occured before this routine is hit
    ASSERT(session_lock_.is_locked_by_me());

    SESS_MAP::iterator session_iterator = ltpudp_rcvr_sessions_.find(session_key);

    if (session_iterator != ltpudp_rcvr_sessions_.end()) {

        LTPUDPSession * session = session_iterator->second;

        RS_MAP::iterator rs_iterator = session->Report_Segments().find(segment_key);

        if (rs_iterator != session->Report_Segments().end())
        {
            LTPUDPReportSegment * ltpreportsegment = rs_iterator->second;

            ltpreportsegment->Cancel_Retransmission_Timer();

            if (segment_type == (int) LTPUDPSegment::LTPUDP_SEGMENT_RS_TO)
            { 
                LTPUDPCancelSegment * cancel_segment = session->Cancel_Segment();

                if (cancel_segment != NULL) {
#ifdef LTPUDPCL_LOG_DEBUG_ENABLED
                    log_debug("cleanup_RS: Session: %s - Ignoring RS resend while in Cancelling State", 
                              session_key.c_str());
#endif
                } else {
                    ltpreportsegment->Increment_Retries();
                    if (ltpreportsegment->Retrans_Retries() <= cla_params_.retran_retries_) {
                        Resend_RS(session, ltpreportsegment);
                    } else {
                        // Exceeded retries - start cancelling
                        build_CS_segment(session,LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                                         LTPUDPCancelSegment::LTPUDP_CS_REASON_RLEXC);
                    }
                } 

                return;
            }

            ltpreportsegment->Cancel_Retransmission_Timer();
            delete ltpreportsegment;
            session->Report_Segments().erase(rs_iterator);

        } else {
        }
    } else {
    }
}
                         
//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::Resend_RS(LTPUDPSession * report_session, LTPUDPReportSegment * report_segment)
{
    (void) report_session;

    report_segment->Create_RS_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER, 
                                    LTPUDPSegment::LTPUDP_SEGMENT_RS_TO, 
                                   report_segment->Report_Serial_Number());

    ++stats_.rs_session_resends_;
    ++stats_.total_snt_rs_;

    parent_->Send(report_segment,PARENT_RECEIVER);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::generate_RS(LTPUDPSession * report_session, int red_green, size_t chkpt_upper_bounds)
{
    int32_t segsize = cla_params_.seg_size_;
    u_int64_t lower_bound_start = 0;



    LTPUDPReportSegment * report_segment = (LTPUDPReportSegment *) NULL;

    report_segment = scan_RS_list(report_session);

    if (report_segment == (LTPUDPReportSegment *) NULL) {

        // loop generating max seg sized reports
        while (lower_bound_start < chkpt_upper_bounds)
        {
            report_segment = new LTPUDPReportSegment( report_session, red_green, lower_bound_start, chkpt_upper_bounds, segsize );

            track_RS(report_session, report_segment);

            report_segment->Create_RS_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER, 
                                            LTPUDPSegment::LTPUDP_SEGMENT_RS_TO, 
                                            report_segment->Report_Serial_Number());

            ++stats_.total_snt_rs_;
            parent_->Send(report_segment,PARENT_RECEIVER);

            lower_bound_start = report_segment->UpperBounds();
        }

        report_session->inc_reports_sent();
    } 
    else
    {
        // resend previous report -- XXX/TODO - loop sending all associated reports
        report_segment->Create_RS_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER, 
                                        LTPUDPSegment::LTPUDP_SEGMENT_RS_TO, 
                                        report_segment->Report_Serial_Number());

        ++stats_.total_snt_rs_;
        parent_->Send(report_segment,PARENT_RECEIVER);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::generate_RS(LTPUDPSession * report_session, LTPUDPDataSegment* dataseg)
{
    int32_t segsize = cla_params_.seg_size_;

    if (report_session->HaveReportSegmentsToSend(dataseg, segsize))
    {
        u_int64_t chkpt = dataseg->Checkpoint_ID();

        LTPUDPReportSegment* rptseg;

        RS_MAP::iterator iter;

        for(iter =  report_session->Report_Segments().begin(); 
            iter != report_session->Report_Segments().end(); 
            iter++)
        {
            rptseg = iter->second;

            if (chkpt == rptseg->Checkpoint_ID())
            {
                // send or resend this report segment
                rptseg->Create_RS_Timer(cla_params_.retran_intvl_,PARENT_RECEIVER, 
                                        LTPUDPSegment::LTPUDP_SEGMENT_RS_TO, 
                                        rptseg->Report_Serial_Number());

                ++stats_.total_snt_rs_;
                parent_->Send(rptseg, PARENT_RECEIVER);
                report_session->inc_reports_sent();
            }
            else
            {
                //log_debug("generateRS - skip ReportSeg for Checkpoint ID: %lu", rptseg->Checkpoint_ID());
            }
        }
    }    
    else
    {
        //log_debug("generateRS - no report segments to send");
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    size_t green_bytes_to_process = 0; 
    size_t red_bytes_to_process   = 0; 
   
    bool create_flag = false;

    LTPUDPSession * ltpudp_session;
    SESS_MAP::iterator session_iterator;

    if (cla_params_.hexdump_) {
        oasys::HexDumpBuffer hex;
        int plen = len > 30 ? 30 : len;
        hex.append(bp, plen);
        log_always("process_data: Buffer (%d of %zu):", plen, len);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }

    LTPUDPSegment * seg = LTPUDPSegment::DecodeBuffer(bp, len);

    if (should_stop()) {
        delete seg;
        return;
    }

    if (!seg->IsValid()){
        log_warn("process_data: invalid segment.... type:%s length:%d", 
                 LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()),(int) len);
        delete seg;
        return;
    }

#ifdef LTPUDP_AUTH_ENABLED
    if (cla_params_.inbound_cipher_suite_ != -1) {
        if (!seg->IsAuthenticated(cla_params_.inbound_cipher_suite_, 
                                 cla_params_.inbound_cipher_key_id_, 
                                 cla_params_.inbound_cipher_engine_)) {
            log_warn("process_data: invalid segment doesn't authenticate.... type:%s length:%d", 
                     LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()),(int) len);
            delete seg;
            return;
        }
    }
#endif

    string temp_string = "";
 
    // after then set the parent....
    seg->Set_Parent(parent_);
 
    //
    // Send over to Sender if it's any of these three SEGMENT TYPES
    //
    if (seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_RS     || 
        seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BR  ||
        seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CAS_BS) { 
        parent_->Process_Sender_Segment(seg);
        return;
    }

    oasys::ScopeLock my_lock(&session_lock_,"Receiver::process_data");  


    //XXX/dz  TODO: if DS then check payload quota to see if we have room to accept this /session/bundle

    create_flag = (seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_DS);

    // get current session from rcvr_sessions...
    session_iterator = track_receiver_session(seg->Engine_ID(),seg->Session_ID(),create_flag);  

    if (!create_flag && session_iterator == ltpudp_rcvr_sessions_.end())
    {
        if ((int) seg->Segment_Type() != 6) {
            //log_debug("process_data: Session: %" PRIu64 "-%" PRIu64 " - %s received but session not found",
            //          seg->Engine_ID(), seg->Session_ID(), LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()));
        } else {
            LTPUDPCancelAckSegment * cas_seg = new LTPUDPCancelAckSegment((LTPUDPCancelSegment *) seg, 
                                                                           LTPUDPSEGMENT_CAS_BS_BYTE, blank_session_); 
            ++stats_.total_snt_ca_;
            parent_->Send(cas_seg,PARENT_RECEIVER); 
            delete cas_seg;
        }
        delete seg; // ignore this segment since the session is already deleted.
        return; 
    }
       
    ltpudp_session = session_iterator->second;

    if (ltpudp_session->Session_State() == LTPUDPSession::LTPUDP_SESSION_STATE_CS) {
        if ((seg->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_CAS_BR) &&
           (seg->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_CS_BS)) {

              //log_debug("process_data: Session: %s - Ignoring %s while Session in CS state",
              //          ltpudp_session->Key().c_str(), LTPUDPSegment::Get_Segment_Type(seg->Segment_Type()));

              delete seg;
              return;
        }
    }

    //
    //   CTRL EXC Flag 1 Flag 0 Code  Nature of segment
    //   ---- --- ------ ------ ----  ---------------------------------------
    //     0   0     0      0     0   Red data, NOT {Checkpoint, EORP or EOB}
    //     0   0     0      1     1   Red data, Checkpoint, NOT {EORP or EOB}
    //     0   0     1      0     2   Red data, Checkpoint, EORP, NOT EOB
    //     0   0     1      1     3   Red data, Checkpoint, EORP, EOB
    //
    //     0   1     0      0     4   Green data, NOT EOB
    //     0   1     0      1     5   Green data, undefined
    //     0   1     1      0     6   Green data, undefined
    //     0   1     1      1     7   Green data, EOB
    //
    
    if (seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_DS) {  // data segments
        LTPUDPDataSegment* dataseg = (LTPUDPDataSegment*) seg;

        ++stats_.total_rcv_ds_;
        if (dataseg->Session_ID() > ULONG_MAX)
        {
            build_CS_segment(ltpudp_session,LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                             LTPUDPCancelSegment::LTPUDP_CS_REASON_SYS_CNCLD);
            delete dataseg;
            return;
        }

        if (dataseg->Service_ID() != 1 && dataseg->Service_ID() != 2)
        {
            if (dataseg->IsRed()) {
                log_warn("process_data: received %s DS with invalid Service ID: %d",
                          ltpudp_session->Key().c_str(), dataseg->Service_ID());

                // send cancel stuff
                build_CS_segment(ltpudp_session,LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                                 LTPUDPCancelSegment::LTPUDP_CS_REASON_UNREACHABLE);
            } else {
                log_warn("process_data: received %s Green DS with invalid Service ID: %d", 
                         ltpudp_session->Key().c_str(),dataseg->Service_ID());
                update_session_counts(ltpudp_session, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
                ltpudp_rcvr_sessions_.erase(session_iterator);
                my_lock.unlock();
                delete ltpudp_session;
            }

            delete dataseg;
            return; 
        }

        // if already in a cancelling state then abort processing
        if (ltpudp_session->Session_State() == LTPUDPSession::LTPUDP_SESSION_STATE_CS)
        {
            delete dataseg;
            return; 
        }


        update_session_counts(ltpudp_session, LTPUDPSession::LTPUDP_SESSION_STATE_DS);

        if (-1 == ltpudp_session->AddSegment(dataseg)) {
            delete dataseg;
            return;
        }

        if (!ltpudp_session->Has_Inactivity_Timer()) {
            ltpudp_session->Start_Inactivity_Timer(cla_params_.inactivity_intvl_);
        }

        if (ltpudp_session->reports_sent() > 0) {
            // RS has already been sent so this is a resend of the DS
            ++stats_.ds_segment_resends_;
        }

        if (dataseg->IsEndofblock()) {
            ltpudp_session->Set_EOB(dataseg);
        }

        red_bytes_to_process = ltpudp_session->IsRedFull();
        bool ok_to_accept_bundle = true;
        if (red_bytes_to_process > 0) {
            // check payload quota to see if we can accept the bundle(s)
            BundleStore* bs = BundleStore::instance();
            if (!bs->try_reserve_payload_space(red_bytes_to_process)) {
                ok_to_accept_bundle = false;
                // rejecting bundle due to storage depletion
                log_warn("LTPUDP::Receiver: rejecting bundle due to storage depletion - session: %s bytes: %zu",
                          ltpudp_session->Key().c_str(), red_bytes_to_process);
            }
            // else space is reserved and must be managed in the bundle extraction process
         }

        if (!ok_to_accept_bundle)
        {
            // Cancel Session due to storage depletion
            build_CS_segment(ltpudp_session, LTPUDPSegment::LTPUDP_SEGMENT_CS_BR, 
                             LTPUDPCancelSegment::LTPUDP_CS_REASON_SYS_CNCLD);
            return;
        }
        else if (dataseg->IsCheckpoint())
        {
            ++stats_.total_rcv_ds_checkpoints_;
            update_session_counts(ltpudp_session, LTPUDPSession::LTPUDP_SESSION_STATE_RS);

            ltpudp_session->Set_Checkpoint_ID(dataseg->Checkpoint_ID());
            generate_RS(ltpudp_session, dataseg);
            dataseg->Set_Retrans_Retries(0);
        }

        if (red_bytes_to_process > 0) {
            bundle_processor_->post(ltpudp_session->GetAllRedData(), red_bytes_to_process,
                                    dataseg->Service_ID() == 2 ? true : false);
        } else if (dataseg->IsCheckpoint()) {
            ++stats_.ds_session_resends_;
        }
 

        green_bytes_to_process = ltpudp_session->IsGreenFull();
        if (green_bytes_to_process > 0) {
            bundle_processor_->post(ltpudp_session->GetAllGreenData(), green_bytes_to_process, 
                                    dataseg->Service_ID() == 2 ? true : false);

            // CCSDS BP Specification - LTP Sessions must be all red or all green
            if (ltpudp_session->DataProcessed()) {
                update_session_counts(ltpudp_session, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
                ltpudp_rcvr_sessions_.erase(session_iterator);
                my_lock.unlock();
                delete ltpudp_session;
            }
        }

    } else if (seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_RAS)  { 
         // Report Ack Segment
         ++stats_.total_rcv_ra_;

        LTPUDPReportAckSegment * look = (LTPUDPReportAckSegment *) seg;
        // cleanup the Report Segment list so we don't keep transmitting.
        char temp[24];
        snprintf(temp, sizeof(temp), "%20.20" PRIu64, look->Report_Serial_Number());
        string str_key = temp;
        cleanup_RS(session_iterator->first, str_key, LTPUDPSegment::LTPUDP_SEGMENT_RAS); 
        delete seg;

        if (ltpudp_session->DataProcessed()) {
            update_session_counts(ltpudp_session, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
            ltpudp_rcvr_sessions_.erase(session_iterator);
            my_lock.unlock();
            delete ltpudp_session;
        } else {
            ltpudp_session->Start_Inactivity_Timer(cla_params_.inactivity_intvl_);
        }
    } else if (seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BS) { 
        // Cancel Segment from Block Sender
        ++stats_.total_rcv_css_;

        //dz debug log_debug("process_data: Session: %s - Cancel by Sender Segment received - delete session",
        log_warn("process_data: Session: %s - Cancel by Sender Segment received - delete session",
                  ltpudp_session->Key().c_str());

        update_session_counts(ltpudp_session, LTPUDPSession::LTPUDP_SESSION_STATE_CS);

        ltpudp_session->Set_Cleanup();
        LTPUDPCancelAckSegment * cas_seg = new LTPUDPCancelAckSegment((LTPUDPCancelSegment *) seg, 
                                                                       LTPUDPSEGMENT_CAS_BS_BYTE, blank_session_); 
        ++stats_.total_snt_ca_;
        parent_->Send(cas_seg,PARENT_RECEIVER); 
        delete seg;
        delete cas_seg;

        // clean up the session
        update_session_counts(ltpudp_session, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
        ltpudp_rcvr_sessions_.erase(session_iterator);
        my_lock.unlock();
        delete ltpudp_session;

    } else if (seg->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CAS_BR) { 
        // Cancel-acknowledgment Segment from Block Receiver
        ++stats_.total_rcv_ca_;

        ltpudp_session->Set_Cleanup();
        update_session_counts(ltpudp_session, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
        ltpudp_rcvr_sessions_.erase(session_iterator); // cleanup session!!!
        my_lock.unlock();
        delete ltpudp_session;
        delete seg;
    } else {
        log_err("process_data - Unhandled segment type received: %d", seg->Segment_Type());
        delete seg;
    }
}


//----------------------------------------------------------------------
char*
LTPUDPConvergenceLayer::Receiver::get_recv_buffer()
{
    oasys::ScopeLock l(&dpo_lock_, "Receiver::return_dpo");

    BufferObject* bo;
    if (buffer_pool_.empty()) {
        ++buffers_allocated_; 
        bo = new BufferObject();
        bo->buffer_ = (u_char*) malloc(10000000);
        bo->len_ = 10000000;
        bo->used_ = 0;
        bo->last_ptr_ = NULL;
        buffer_pool_.push_back(bo);
    }

    bo = buffer_pool_.front();
    if ((bo->used_ + MAX_UDP_PACKET) > (bo->len_ - bo->used_)) {
        // this buffer is full - move to used pool
        buffer_pool_.pop_front();
        used_buffer_pool_.push_back(bo);

        return get_recv_buffer();
    }

    return (char*) (bo->buffer_ + bo->used_);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::DataProcObject* 
LTPUDPConvergenceLayer::Receiver::get_dpo(int len)
{
    oasys::ScopeLock l(&dpo_lock_, "Receiver::return_dpo");

    DataProcObject* dpo;
    if (dpo_pool_.empty()) {
        dpo = new DataProcObject();
    } else {
        dpo = dpo_pool_.front();
        dpo_pool_.pop_front();
    }

    BufferObject* bo = buffer_pool_.front();    
    dpo->data_ = &bo->buffer_[bo->used_];
    dpo->len_ = len;

    bo->used_ += len;
    ASSERT(bo->used_ <= bo->len_);
    bo->last_ptr_ = dpo;

    return dpo;
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Receiver::return_dpo(DataProcObject* obj)
{
    oasys::ScopeLock l(&dpo_lock_, "Receiver::return_dpo");

    if (!used_buffer_pool_.empty()) {
        BufferObject* bo = used_buffer_pool_.front();
        if (bo->last_ptr_ == obj) {
            // final block of data in the buffer has been processed
            used_buffer_pool_.pop_front();

            // buffer can now be re-used
            bo->used_ = 0;
            buffer_pool_.push_back(bo);
        }
    }

    dpo_pool_.push_back(obj);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::run()
{
    char threadname[16] = "LtpReceiver";
    pthread_setname_np(pthread_self(), threadname);
   

    int read_len;
    in_addr_t addr;
    u_int16_t port;

    int bufsize = 0; 
    socklen_t optlen = sizeof(bufsize);
    if (::getsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &bufsize, &optlen) < 0) {
        log_warn("error getting SO_RCVBUF: %s", strerror(errno));
    } else {
        log_always("LTPUDPConvergenceLayer::Receiver socket recv_buffer size = %d", bufsize);
    }


    while (1) {
        if (should_stop())
            break;

        read_len = poll_sockfd(POLLIN, NULL, 10);

        if (should_stop())
            break;

        if (1 == read_len) {
            read_len = recvfrom(get_recv_buffer(), MAX_UDP_PACKET, 0, &addr, &port);

            if (read_len <= 0) {
                if (errno == EINTR) {
                    continue;
                }
                log_err("error in recvfrom(): %d %s",
                        errno, strerror(errno));
                close();
                break;
            }
        
            if (should_stop())
                break;

            data_process_offload_->post(get_dpo(read_len));
        }
    }
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Receiver::update_session_counts(LTPUDPSession* session, LTPUDPSession::SessionState_t new_state)
{
    LTPUDPSession::SessionState_t old_state = session->Session_State();

    if (old_state != new_state) {
        switch (old_state)
        {
            case LTPUDPSession::LTPUDP_SESSION_STATE_RS: 
                --sessions_state_rs;
                break;
            case LTPUDPSession::LTPUDP_SESSION_STATE_CS:
                --sessions_state_cs;
                break;
            case LTPUDPSession::LTPUDP_SESSION_STATE_DS:
                --sessions_state_ds;
                break;
            default:
              ;
        }

        switch (new_state)
        {
            case LTPUDPSession::LTPUDP_SESSION_STATE_RS: 
                ++sessions_state_rs;
                break;
            case LTPUDPSession::LTPUDP_SESSION_STATE_CS:
                ++sessions_state_cs;
                break;
            case LTPUDPSession::LTPUDP_SESSION_STATE_DS:
                ++sessions_state_ds;
                break;
            default:
              ;
        }

        session->Set_Session_State(new_state); 
    }
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::RecvBundleProcessor(Receiver* parent, LinkRef* link)
    : Logger("LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor",
             "/dtn/cl/ltpudp/receiver/bundleproc/%p", this),
      Thread("LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor")
{
    parent_ = parent;
    link_ = link->object();

    bytes_queued_ = 0;
    max_bytes_queued_ = 100000000; // 100 MB

    eventq_ = new oasys::MsgQueue< EventObj* >(logpath_);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::~RecvBundleProcessor()
{

    set_should_stop();

    int ctr = 0;
    while (!is_stopped() && (ctr++ < 10))
    {
        usleep(100000);
    }

    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

    EventObj* event;
    while (eventq_->try_pop(&event)) {
        free(event->data_);
        delete event;
    }
    delete eventq_;
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::queue_size()
{
    return eventq_->size();
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::bytes_queued()
{
    return bytes_queued_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::post(u_char* data, size_t len, bool multi_bundle)
{
//dzdebug - lockup if bundle length is longer than max!!!    while (not should_stop() && ((bytes_queued_ + len) > max_bytes_queued_)) {
    while (!should_stop() && (bytes_queued_ > max_bytes_queued_)) {
        usleep(100);
    }
    if (should_stop()) return;

 
    EventObj* event = new EventObj();
    event->data_ = data;
    event->len_ = len;
    event->multi_bundle_ = multi_bundle;

    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);
    if (should_stop()) 
    {
        delete event;
    }
    else
    {
        bytes_queued_ += len;
        eventq_->push_back(event);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::run()
{
    char threadname[16] = "LtpRcvrBndlProc";
    pthread_setname_np(pthread_self(), threadname);
   

    EventObj* event;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (true) {

        // finish processing the the eventq before stoppping
        if (should_stop()) {
            if (0 == eventq_->size()) {
                 break; 
            }
        }


        ret = oasys::IO::poll_multiple(pollfds, 1, 10, NULL);

        if (ret == oasys::IOINTR) {
            log_err("RecvBundleProcessor interrupted - aborting");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("RecvBundleProcessor IO Error - aborting");
            set_should_stop();
            continue;
        }

        if (should_stop()) continue;

        // check for an event
        if (event_poll->revents & POLLIN) {

            event = NULL;

            queue_lock_.lock("RecvBundleProcessor:run");

            if (should_stop()) continue;

            if (eventq_->try_pop(&event)) {
                ASSERT(event != NULL)
                bytes_queued_ -= event->len_;
            }

            queue_lock_.unlock();

            if (event) { 
                extract_bundles_from_data(event->data_, event->len_, event->multi_bundle_);
                delete event;
            }
        }
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvBundleProcessor::extract_bundles_from_data(u_char * bp, size_t len, bool multi_bundle)
{
    size_t temp_payload_bytes_reserved   = len;
    BundleStore* bs = BundleStore::instance();

    if (bp == (u_char *) NULL)
    {
        bs->release_payload_space(temp_payload_bytes_reserved, 0);
        log_err("extract_bundles_from_data: no data to process?");
        return;
    }


    u_int32_t check_client_service = 0;
    // the payload should contain a full bundle
    int64_t all_data = (int64_t) len;
    u_char * current_bp = bp;

//  if (multi_bundle) {
//      oasys::HexDumpBuffer hex;
//      hex.append(bp, len);
//      log_debug("Extract: (%d):",(int) len);
//      log_multiline(oasys::LOG_DEBUG, hex.hexify().c_str());
//  }

    while (!should_stop() && (all_data > 0))
    {
        Bundle* bundle = new Bundle();
    
        bool complete = false;
        if (multi_bundle) {
            // Multi bundle buffer... must make sure a 1 precedes all packets
            int bytes_used = SDNV::decode(current_bp, all_data, &check_client_service);
            if (check_client_service != 1)
            {
                log_err("extract_bundles_from_data: LTP SDA - invalid Service ID: %d",check_client_service);
                delete bundle;
                break; // so we free the bp
            }
            current_bp += bytes_used;
            all_data   -= bytes_used;
        }
             
        int64_t  cc = BundleProtocol::consume(bundle, current_bp, all_data, &complete);
        if (cc < 0)  {
            log_err("extract_bundles_from_data: bundle protocol error");
            delete bundle;
            break; // so we free the bp
        }

        if (!complete) {
            log_err("extract_bundles_from_data: incomplete bundle");
            delete bundle;
            break;  // so we free the bp
        }

        all_data   -= cc;
        current_bp += cc;

        parent_->inc_bundles_received();

        // bundle already approved to use payload space
        BundleStore::instance()->reserve_payload_space(bundle);

        BundleDaemon::post(
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, cc, EndpointID::NULL_EID(), link_.object()));
    }

    if (bp != (u_char *) NULL)free(bp);

    // release the temp reserved space since the bundles now have the actually space reserved
    bs->release_payload_space(temp_payload_bytes_reserved, 0);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::RecvDataProcessOffload(Receiver* parent)
    : Logger("LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload",
             "/dtn/cl/ltpudp/receiver/dataproc/%p", this),
      Thread("LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload")
{
    parent_ = parent;
    bytes_queued_ = 0;
    max_bytes_queued_ = 100000000; // 100 MB

    eventq_ = new oasys::MsgQueue<DataProcObject*>(logpath_);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::~RecvDataProcessOffload()
{
    set_should_stop();

    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

    DataProcObject* obj;
    while (eventq_->try_pop(&obj)) {
        delete obj;
    }
    delete eventq_;
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::queue_size()
{
    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

    return eventq_->size();
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::bytes_queued()
{
    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

    return bytes_queued_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::post(DataProcObject* obj)
{
    while (not should_stop() && ((bytes_queued_ + obj->len_) > max_bytes_queued_)) {
        usleep(100);
    }
    if (should_stop()) return;

    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

    bytes_queued_ += obj->len_;
 
    eventq_->push_back(obj);
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Receiver::RecvDataProcessOffload::run()
{
    char threadname[16] = "LtpRcvrProcOfld";
    pthread_setname_np(pthread_self(), threadname);
   

    DataProcObject* obj;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (true) {

        // finish processing the the eventq before stoppping
        if (should_stop()) {
            if (0 == eventq_->size()) {
                 break; 
            }
        }

        ret = oasys::IO::poll_multiple(pollfds, 1, 10, NULL);

        if (ret == oasys::IOINTR) {
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("RecvDataProcessOffload IO Error - aborting");
            set_should_stop();
            continue;
        }

        // check for an obj
        if (event_poll->revents & POLLIN) {
            obj = NULL;
            queue_lock_.lock(__FUNCTION__);

            if (eventq_->try_pop(&obj)) {
                ASSERT(obj != NULL)
                bytes_queued_ -= obj->len_;
            }
 
            queue_lock_.unlock();

            if (obj) {
                parent_->process_data(obj->data_, obj->len_);

                parent_->return_dpo(obj);
            }
        }
    }
}














//----------------------------------------------------------------------
u_int32_t LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Suite()
{
    return params_->outbound_cipher_suite_;
}
//----------------------------------------------------------------------
u_int32_t LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Key_Id()
{
    return params_->outbound_cipher_key_id_;
}
//----------------------------------------------------------------------
string    LTPUDPConvergenceLayer::Sender::Outbound_Cipher_Engine()
{
    return params_->outbound_cipher_engine_;
}
//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::Sender(const ContactRef& contact,LTPUDPCallbackIF * parent)
    : 
      Logger("LTPUDPConvergenceLayer::Sender",
             "/dtn/cl/ltpudp/sender/%p", this),
      Thread("LTPUDPConvergenceLayer::Sender"),
      socket_(logpath_),
      session_ctr_(1),
      rate_socket_(0),
      contact_(contact.object(), "LTPUDPCovergenceLayer::Sender")
{
    char extra_logpath[200];
    strcpy(extra_logpath,logpath_);
    strcat(extra_logpath,"2");
    loading_session_          = (LTPUDPSession *) NULL;
    poller_                   = NULL;
    parent_                   = parent;
    admin_sendq_              = new oasys::MsgQueue< MySendObject* >(logpath_);
    high_eventq_              = new oasys::MsgQueue< MySendObject* >(logpath_);
    low_eventq_               = new oasys::MsgQueue< MySendObject* >(extra_logpath);

    memset(&stats_, 0, sizeof(stats_));

    sender_process_offload_ = new SenderProcessOffload(this);
    sender_process_offload_->start();

    sender_generate_datasegs_offload_ = nullptr;

    max_adminq_size_ = 0;
    max_highq_size_ = 0;
    max_lowq_size_ = 0;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::~Sender()
{
    this->set_should_stop();

    if (sender_process_offload_ != NULL ) {
        sender_process_offload_->set_should_stop();
        delete sender_process_offload_;
        sender_process_offload_ = NULL;
    }

    if (sender_generate_datasegs_offload_ != NULL ) {
        sender_generate_datasegs_offload_->set_should_stop();
        delete sender_generate_datasegs_offload_;
        sender_generate_datasegs_offload_ = NULL;
    }



    if (poller_ != NULL) {
        poller_->set_should_stop();
        delete poller_;
        poller_ = NULL;
    }

    MySendObject *event;

    sleep(1);

    while (admin_sendq_->try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }

    while (high_eventq_->try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }

    while (low_eventq_->try_pop(&event)) {
        delete event->str_data_;
        delete event;
    }

    delete admin_sendq_;
    delete high_eventq_;
    delete low_eventq_;

    if (rate_socket_) {
        delete rate_socket_;
    }
    delete blank_session_;
}

void
LTPUDPConvergenceLayer::Sender::do_shutdown()
{
    this->set_should_stop();

    if (poller_ != NULL) {
        poller_->set_should_stop();

        while (!poller_->is_stopped() ) {
            usleep(10000);
        }
    }

    if (sender_process_offload_ != NULL ) {
        sender_process_offload_->set_should_stop();

        while (!sender_process_offload_->is_stopped() ) {
            usleep(10000);
        }
    }

    if (sender_generate_datasegs_offload_ != NULL ) {
        sender_generate_datasegs_offload_->set_should_stop();

        while (!sender_generate_datasegs_offload_->is_stopped() ) {
            usleep(10000);
        }
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::init(Params* params, in_addr_t addr, u_int16_t port)
{
    log_debug("init entered");

    params_ = params;
    
    socket_.logpathf("%s/conn/%s:%d", logpath_, intoa(addr), port);
    socket_.set_logfd(false);
    socket_.params_.send_bufsize_ = params_->sendbuf_;
    socket_.init_socket();

    int bufsize = 0; 
    socklen_t optlen = sizeof(bufsize);
    if (::getsockopt(socket_.fd(), SOL_SOCKET, SO_SNDBUF, &bufsize, &optlen) < 0) {
        log_warn("error getting SO_SNDBUF: %s", strerror(errno));
    } else {
        log_always("LTPUDPConvergenceLayer::Sender socket send_buffer size = %d", bufsize);
    }

    // do not bind or connect the socket
    if (params->rate_ != 0) {

        rate_socket_ = new oasys::RateLimitedSocket(logpath_, 0, 0, params->bucket_type_);
 
        rate_socket_->set_socket((oasys::IPSocket *) &socket_);
        rate_socket_->bucket()->set_rate(params->rate_);

        if (params->bucket_depth_ != 0) {
            rate_socket_->bucket()->set_depth(params->bucket_depth_);
        }
 
        log_debug("init: Sender rate controller after initialization: rate %" PRIu64 " depth %" PRIu64,
                  rate_socket_->bucket()->rate(),
                  rate_socket_->bucket()->depth());
    }

    blank_session_ = new LTPUDPSession(0UL, 0UL, parent_,  PARENT_RECEIVER);
    blank_session_->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
    blank_session_->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
    blank_session_->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);

    return true;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::run()
{
    char threadname[16] = "LtpSender";
    pthread_setname_np(pthread_self(), threadname);
   
    // set thread niceness
    setpriority( PRIO_PROCESS, 0, -9);


    sessions_state_ds = 0;
    sessions_state_rs = 0;
    sessions_state_cs = 0;

    struct timespec sleep_time = { 0, 100000000 }; // 1 tenth of a second

    LTPUDPSession* loaded_session = NULL;
    int ret;

    MySendObject *hevent; 
    MySendObject *levent; 
    int cc = 0;
    // block on input from the socket and
    // on input from the bundle event list

    poller_ = new SendPoller(this, contact_->link());
    poller_->start();

    sender_generate_datasegs_offload_ = new SenderGenerateDataSegsOffload(this);
    sender_generate_datasegs_offload_->init(params_);
    sender_generate_datasegs_offload_->start();

    struct pollfd pollfds[3];

    struct pollfd* adminq_poll = &pollfds[0];

    adminq_poll->fd = admin_sendq_->read_fd();
    adminq_poll->events = POLLIN; 
    adminq_poll->revents = 0;

    struct pollfd* high_event_poll = &pollfds[1];

    high_event_poll->fd = high_eventq_->read_fd();
    high_event_poll->events = POLLIN; 
    high_event_poll->revents = 0;

    struct pollfd* low_event_poll = &pollfds[2];

    low_event_poll->fd = low_eventq_->read_fd();
    low_event_poll->events = POLLIN; 
    low_event_poll->revents = 0;

    while (!should_stop()) {

        // only need to check loading_session_ if not NULL and the lock is immediately
        // available since send_bundle now checks for agg time expiration also
        if ((NULL != loading_session_) && 
            (0 == loading_session_lock_.try_lock("Sender::run()"))) {

            loaded_session = NULL;

            // loading_session_ may have been cleared out between the two if checks above
            if (NULL != loading_session_ && loading_session_->TimeToSendIt(params_->agg_time_))
            {
                 loaded_session = loading_session_;
                 loading_session_ = NULL;
            }

            // release the lock as quick as possible
            loading_session_lock_.unlock();

            if (NULL != loaded_session) {
                 if (params_->outbound_cipher_suite_ != -1)
                 {
                     loaded_session->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
                     loaded_session->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
                     loaded_session->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);
                 }

                 //post for offload DS generation
                 sender_generate_datasegs_offload_->post(loaded_session);

                 loaded_session = NULL;
            }
        }


        ret = oasys::IO::poll_multiple(pollfds, 3, 1, NULL);

        if (should_stop()) continue; 

        if (ret == oasys::IOINTR) {
            log_err("run(): Sender interrupted");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("run(): Sender I/O error");
            set_should_stop();
            continue;
        }

        if (!params_->comm_aos_)
        {
            // not currently in contact so sleep and try again later
            nanosleep(&sleep_time,NULL);
            continue; 
        }

        // check for an event
        if (adminq_poll->revents & POLLIN) {
            if (admin_sendq_->try_pop(&hevent)) {
                ASSERT(hevent != NULL)
        
                // oasys::HexDumpBuffer hex;
                // hex.append((u_char *) hevent->str_data_->data(),(int) hevent->str_data_->size());
                // log_always("high_event: Buffer (%d):",(int) hevent->str_data_->size());
                // log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
             
                if (params_->rate_ > 0) {
                    cc = rate_socket_->sendto(const_cast< char * >(hevent->str_data_->data()),
                                              hevent->str_data_->size(), 0, 
                                              params_->remote_addr_, 
                                              params_->remote_port_, 
                                              params_->wait_till_sent_);
                } else {
  
                    cc = socket_.sendto(const_cast< char * > (hevent->str_data_->data()), 
                                        hevent->str_data_->size(), 0, 
                                        params_->remote_addr_, 
                                        params_->remote_port_);
                }

                if (hevent->timer_) {
                    hevent->timer_->SetSeconds(hevent->timer_->seconds_); // re-figure the target_counter
                    hevent->timer_->schedule_in(hevent->timer_->seconds_*1000); // it needs milliseconds
                }
               
                if (cc == (int)hevent->str_data_->size()) {
                } else {
                    log_err("Send: error sending administrative segment to %s:%d (wrote %d/%zu): %s",
                            intoa(params_->remote_addr_), params_->remote_port_,
                            cc, hevent->str_data_->size(), strerror(errno));
                }

                delete hevent->str_data_;
                delete hevent;
            }

            // loop around to see if there is another high priority segment
            // to transmit before checking for low priority segments 
            continue; 
        }

        if (high_event_poll->revents & POLLIN) {
            if (high_eventq_->try_pop(&hevent)) {
                ASSERT(hevent != NULL)
        
                // oasys::HexDumpBuffer hex;
                // hex.append((u_char *) hevent->str_data_->data(),(int) hevent->str_data_->size());
                // log_always("high_event: Buffer (%d):",(int) hevent->str_data_->size());
                // log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
             
                if (params_->rate_ > 0) {
                    cc = rate_socket_->sendto(const_cast< char * >(hevent->str_data_->data()),
                                              hevent->str_data_->size(), 0, 
                                              params_->remote_addr_, 
                                              params_->remote_port_, 
                                              params_->wait_till_sent_);
                } else {
  
                    cc = socket_.sendto(const_cast< char * > (hevent->str_data_->data()), 
                                        hevent->str_data_->size(), 0, 
                                        params_->remote_addr_, 
                                        params_->remote_port_);
                }

                if (hevent->timer_) {
                    hevent->timer_->SetSeconds(hevent->timer_->seconds_); // re-figure the target_counter
                    hevent->timer_->schedule_in(hevent->timer_->seconds_*1000); // it needs milliseconds
                }
               
                if (cc == (int)hevent->str_data_->size()) {
                } else {
                    log_err("Send: error sending high priority segment to %s:%d (wrote %d/%zu): %s",
                            intoa(params_->remote_addr_), params_->remote_port_,
                            cc, hevent->str_data_->size(), strerror(errno));
                }

                delete hevent->str_data_;
                delete hevent;
            }

            // loop around to see if there is another high priority segment
            // to transmit before checking for low priority segments 
            continue; 
        }


        // check for a low priority event (new Data Segment)
        if (low_event_poll->revents & POLLIN) {
            if (low_eventq_->try_pop(&levent)) {

                ASSERT(levent != NULL)

                // oasys::HexDumpBuffer hex;
                // hex.append((u_char *)levent->str_data_->data(),(int) levent->str_data_->size());
                // log_always("low_event: Buffer (%d):", (int) levent->str_data_->size());
                // log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());

                if (params_->rate_ > 0) {
                    cc = rate_socket_->sendto(const_cast< char * >(levent->str_data_->data()),
                                              levent->str_data_->size(), 0, 
                                              params_->remote_addr_, 
                                              params_->remote_port_, 
                                              params_->wait_till_sent_);
                } else {
                    cc = socket_.sendto(const_cast< char * > (levent->str_data_->data()), 
                                        levent->str_data_->size(), 0, 
                                        params_->remote_addr_, 
                                        params_->remote_port_);
                }

                if (levent->timer_) {
                    levent->timer_->SetSeconds(levent->timer_->seconds_); // re-figure the target_counter
                    levent->timer_->schedule_in(levent->timer_->seconds_*1000); // it needs milliseconds
                }
               
                if (cc == (int)levent->str_data_->size()) {
                } else {
                    log_err("Send: error sending low priority segment to %s:%d (wrote %d/%zu): %s",
                            intoa(params_->remote_addr_), params_->remote_port_,
                            cc, levent->str_data_->size(), strerror(errno));
                }

                delete levent->str_data_;
                delete levent;
            }
        }
    }
}
 

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::PostTransmitProcessing(LTPUDPSession * session, bool success)
{
    LinkRef link = contact_->link();

    BundleRef bundle;

    while ((bundle = session->Bundle_List()->pop_back()) != NULL)
    {
        if (success) {

            ++stats_.bundles_success_;

            if (session->Expected_Red_Bytes() > 0)
            {
                BundleDaemon::post(
                    new BundleTransmittedEvent(bundle.object(), contact_, 
                                               link, session->Expected_Red_Bytes(), 
                                               session->Expected_Red_Bytes(), success));
            } else if (session->Expected_Green_Bytes() > 0) { 
                BundleDaemon::post(
                    new BundleTransmittedEvent(bundle.object(), contact_, 
                                               link, session->Expected_Green_Bytes(), 
                                               session->Expected_Green_Bytes(), success));
            }
        } else {
            ++stats_.bundles_fail_;

            // signaling failure so External Router can reroute the bundle
            BundleDaemon::post(
                new BundleTransmittedEvent(bundle.object(), contact_, 
                                           link, 0, 0, success));
        }
    }
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::Process_Segment(LTPUDPSegment * segment)
{
    if (segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_RS    ||
        segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BR ||
        segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CAS_BS) {

        switch(segment->Segment_Type()) {

              case LTPUDPSegment::LTPUDP_SEGMENT_RS:
                   process_RS((LTPUDPReportSegment *) segment);
                   break;

              case LTPUDPSegment::LTPUDP_SEGMENT_CS_BR:
                   process_CS((LTPUDPCancelSegment *) segment);
                   break;

              case LTPUDPSegment::LTPUDP_SEGMENT_CAS_BS:
                   process_CAS((LTPUDPCancelAckSegment *) segment);
                   break;

              default:
                   log_err("Unknown Segment Type for Sender::Process_Segment(%s)",
                           segment->Get_Segment_Type()); 
                   break;
        }
    }
    return;
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::update_session_counts(LTPUDPSession* session, LTPUDPSession::SessionState_t new_state)
{
    LTPUDPSession::SessionState_t old_state = session->Session_State();

    if (old_state != new_state) {
        switch (old_state)
        {
            case LTPUDPSession::LTPUDP_SESSION_STATE_RS: 
                --sessions_state_rs;
                break;
            case LTPUDPSession::LTPUDP_SESSION_STATE_CS:
                --sessions_state_cs;
                break;
            case LTPUDPSession::LTPUDP_SESSION_STATE_DS:
                --sessions_state_ds;
                break;
            default:
              ;
        }

        switch (new_state)
        {
            case LTPUDPSession::LTPUDP_SESSION_STATE_RS: 
                ++sessions_state_rs;
                break;
            case LTPUDPSession::LTPUDP_SESSION_STATE_CS:
                ++sessions_state_cs;
                break;
            case LTPUDPSession::LTPUDP_SESSION_STATE_DS:
                ++sessions_state_ds;
                break;
            default:
              ;
        }

        session->Set_Session_State(new_state); 
    }
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::build_CS_segment(LTPUDPSession * session, int segment_type, u_char reason_code)
{
    LTPUDPCancelSegment * cancel_segment = new LTPUDPCancelSegment(session, segment_type, reason_code, parent_);

    update_session_counts(session, LTPUDPSession::LTPUDP_SESSION_STATE_CS);

    cancel_segment->Set_Retrans_Retries(0);
    cancel_segment->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                                LTPUDPSegment::LTPUDP_SEGMENT_CS_TO);

    session->Set_Cancel_Segment(cancel_segment);

    ++stats_.cs_session_cancel_by_sender_;
    ++stats_.total_snt_css_;

    Send(cancel_segment,PARENT_SENDER);
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::dump_queue_sizes(oasys::StringBuffer* buf)
{
    buf->appendf("Sender Queues/max: Admin: %zu/%zu ResendDS: %zu/%zu DataSegs: %zu/%zu\n", 
                 admin_sendq_->size(), max_adminq_size_, high_eventq_->size(), max_highq_size_, low_eventq_->size(), max_lowq_size_);
    buf->appendf("Sender GenerateDataSegs Queue size: %zu\n", sender_generate_datasegs_offload_->queue_size());
    buf->appendf("Sender ProcessLTPSegsOffload Queue size: %zu\n", sender_process_offload_->queue_size());
}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::Dump_Sessions(oasys::StringBuffer* buf)
{
    buf->appendf("%zu :  DS: %zu RS: %zu CS: %zu\n",
                 ltpudp_send_sessions_.size(), 
                 sessions_state_ds, sessions_state_rs, sessions_state_cs);

//dzdebug - need to get the session lock if going to list some of the sessions in detail
/*
    int count = 0;
    int num_segs = 0;
    int max_segs = 0;
    DS_MAP::iterator segment_iter;
    SESS_MAP::iterator session_iter;

    for(session_iter =  ltpudp_send_sessions_.begin(); 
        session_iter != ltpudp_send_sessions_.end(); 
        session_iter ++)
    {
        LTPUDPSession * session = session_iter->second;

        buf->appendf("    Session: %s   %s\n",session_iter->first.c_str(), session->Get_Session_State());
        num_segs = 0;
        max_segs = session->Red_Segments().size() - 1;
        buf->appendf("        Red Segments: %zu\n",session->Red_Segments().size());
        if ((int) session->Outbound_Cipher_Suite() != -1)
            buf->appendf("             Outbound_Cipher_Suite: %d\n",session->Outbound_Cipher_Suite());
        
        for(segment_iter =  session->Red_Segments().begin(); 
            segment_iter != session->Red_Segments().end(); 
            segment_iter ++)
        {
            LTPUDPDataSegment * segment = segment_iter->second;
            if (num_segs < 5 || num_segs == max_segs) {
                if (num_segs > 5) {
                    buf->appendf("             ...\n");
                }
                buf->appendf("             key[%d]: %s\n", 
                             num_segs, segment->Key().c_str()); 
            }
            ++num_segs;
        }
        num_segs = 0;
        max_segs = session->Green_Segments().size() - 1;
        buf->appendf("      Green Segments: %zu\n",session->Green_Segments().size());
        if ((int) session->Outbound_Cipher_Suite() != -1)
            buf->appendf("             Outbound_Cipher_Suite: %d\n",session->Outbound_Cipher_Suite());
        for(segment_iter =  session->Green_Segments().begin(); 
            segment_iter != session->Green_Segments().end(); 
            segment_iter ++)
        {
            LTPUDPDataSegment * segment = segment_iter->second;
            if (num_segs < 5 || num_segs == max_segs) {
                if (num_segs > 5) {
                    buf->appendf("             ...\n");
                }
                buf->appendf("             key[%d]: %s\n", 
                             num_segs, segment->Key().c_str()); 
            }
            ++num_segs;
        }
        count++;
        if (count >= 5)break;
    }
*/


}

//----------------------------------------------------------------------
void 
LTPUDPConvergenceLayer::Sender::Dump_Statistics(oasys::StringBuffer* buf)
{
//              Sessions     Sessns      Data Segments        Report Segments    RptAck   Cancel By Sendr  Cancel By Recvr  CanAck  Bundles   Bundles   CanBRcv
//           Total /  Max    DS R-X     Total  / ReXmit       Total / ReXmit     Total    Total / ReXmit   Total / ReXmit   Total   Success   Failure   But Got
//         ----------------  -------  --------------------  ------------------  --------  ---------------  ---------------  ------  --------  --------  -------
//Sender   12345678 / 12345  1234567  123456789 / 12345678  12345678 / 1234567  12345678  123456 / 123456  123456 / 123456  123456  12345678  12345678  1234567
//Receiver 12345678 / 12345  1234567  123456789 / 12345678  12345678 / 1234567  12345678  123456 / 123456  123456 / 123456  123456  12345678  12345678  1234567

    buf->appendf("Sender   %8" PRIu64 " / %5" PRIu64 "  %7" PRIu64 "  %9" PRIu64 " / %8" PRIu64 "  %8" PRIu64 " / %7" PRIu64 
                 "  %8" PRIu64 "  %6" PRIu64 " / %6" PRIu64 "  %6" PRIu64 " / %6" PRIu64 "  %6" PRIu64 "  %8" PRIu64 "  %8" PRIu64 "\n",
                 stats_.total_sessions_, stats_.max_sessions_,
                 stats_.ds_session_resends_, stats_.total_snt_ds_, stats_.ds_segment_resends_,
                 stats_.total_rcv_rs_, uint64_t(0), stats_.total_snt_ra_,
                 stats_.total_snt_css_, stats_.cs_session_resends_, stats_.total_rcv_csr_, uint64_t(0),
                 stats_.total_rcv_ca_,
                 stats_.bundles_success_, stats_.bundles_fail_ );
                 // Cancel By Recever but Got not applicable to Sender
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::clear_statistics()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send(LTPUDPSegment * send_segment,int parent_type)
{
    oasys::HexDumpBuffer hex;

    if (!should_stop()) {
        MySendObject* obj = new MySendObject();

        obj->str_data_     = new std::string((char *) send_segment->Packet()->buf(), send_segment->Packet_Len());
        obj->session_      = send_segment->Session_Key();
        obj->segment_      = send_segment->Key();
        obj->segment_type_ = send_segment->Segment_Type();
        obj->parent_type_  = parent_type;
        obj->timer_        = send_segment->timer_;

        if (params_->hexdump_) {
            int dump_length = 20;
            if (send_segment->Packet_Len() < 20)dump_length = send_segment->Packet_Len();
            hex.append(send_segment->Packet()->buf(), dump_length);
            log_always("Send(segment): Buffer:%d type:%s", dump_length,LTPUDPSegment::Get_Segment_Type(send_segment->Segment_Type()));
		    log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
        }

	if (send_segment->IsRed()) {
	    high_eventq_->push_back(obj);

            size_t qsize = high_eventq_->size();
            if (qsize > max_highq_size_) {
                max_highq_size_ = qsize;
            }
        } else {
	    admin_sendq_->push_back(obj);

            size_t qsize = admin_sendq_->size();
            if (qsize > max_adminq_size_) {
                max_adminq_size_ = qsize;
            }
        }
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send_Low_Priority(LTPUDPSegment * send_segment,int parent_type)
{
    if (!should_stop()) {

        MySendObject* obj = new MySendObject();

        obj->str_data_ = new std::string((char *) send_segment->Packet()->buf(), send_segment->Packet_Len());
	obj->session_      = send_segment->Session_Key();
	obj->segment_      = send_segment->Key();
	obj->segment_type_ = send_segment->Segment_Type();
	obj->parent_type_  = parent_type;
	obj->timer_        = send_segment->timer_;

	low_eventq_->push_back(obj);

        size_t qsize = low_eventq_->size();
        if (qsize > max_lowq_size_) {
            max_lowq_size_ = qsize;
        }
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::process_CS(LTPUDPCancelSegment * cs_segment)
{
    ++stats_.total_rcv_csr_;
    oasys::ScopeLock mylock(&session_lock_,"Sender::process_CS");  

    SESS_MAP::iterator session_iterator = track_sender_session(cs_segment->Engine_ID(), cs_segment->Session_ID(),false);

    if (session_iterator != ltpudp_send_sessions_.end()) {
	++stats_.total_snt_ca_;
	cleanup_sender_session(cs_segment->Session_Key(), cs_segment->Key(), cs_segment->Segment_Type());
    } else {
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::process_CAS(LTPUDPCancelAckSegment * cas_segment)
{
    ++stats_.total_rcv_ca_;

    // lock session since we potentially alter sender_session 
    oasys::ScopeLock mylock(&session_lock_,"Sender::process_CAS");
    SESS_MAP::iterator session_iterator = track_sender_session(cas_segment->Engine_ID(), cas_segment->Session_ID(),false);

    if (session_iterator != ltpudp_send_sessions_.end()) {
	LTPUDPSession * session = session_iterator->second;

	if (session->Cancel_Segment() != NULL) {
	    session->Cancel_Segment()->Cancel_Retransmission_Timer();
        }

        update_session_counts(session, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
	delete session;
	ltpudp_send_sessions_.erase(session_iterator);
    } else {
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::process_RS(LTPUDPReportSegment * report_segment)
{
    ++stats_.total_rcv_rs_;

    // lock sessions for sender since we potentially change sender sessions MAP
    oasys::ScopeLock mylock(&session_lock_,"Sender::process_RS");  

    SESS_MAP::iterator session_iterator = 
		 track_sender_session(report_segment->Engine_ID(), report_segment->Session_ID(),false);

    if (session_iterator == ltpudp_send_sessions_.end()) {
	return;
    }

    report_segment->Set_Parent(parent_);

    ++stats_.total_snt_ra_;

    int checkpoint_removed = 0;

    if (session_iterator != ltpudp_send_sessions_.end()) {
    
	LTPUDPSession * session = session_iterator->second;

	RC_MAP::iterator loop_iterator;

	for (loop_iterator  = report_segment->Claims().begin(); 
	     loop_iterator != report_segment->Claims().end(); 
	     loop_iterator++) 
	{
	    LTPUDPReportClaim * rc = loop_iterator->second;     
	    checkpoint_removed = session->RemoveSegment(&session->Red_Segments(),
							//XXX/dz - ION 3.3.0 - multiple Report Segments if
							//  too many claims for one message (max = 20)
							// - other RS's have non-zero lower bound
							report_segment->LowerBounds()+rc->Offset(),
							rc->Length()); 
	}

	if (checkpoint_removed) {
	    Send_Remaining_Segments(session);
	}
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send_Remaining_Segments(LTPUDPSession * session)
{
    //
    // ONLY DEALS WITH RED SEGMENTS
    //
    LTPUDPDataSegment * segment;
    DS_MAP::iterator loop_iterator;

    if (session->Red_Segments().size() > 0)
    {
	loop_iterator = --session->Red_Segments().end();
	segment       = loop_iterator->second;

	if (!segment->IsCheckpoint())
	{
	    segment->Set_Checkpoint_ID(session->Increment_Checkpoint_ID());
	    segment->SetCheckpoint();
	    segment->Encode_All();
            segment->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                                LTPUDPSegment::LTPUDP_SEGMENT_DS_TO);
        }

        ++stats_.ds_session_resends_;

        for(loop_iterator = session->Red_Segments().begin(); loop_iterator != session->Red_Segments().end(); loop_iterator++)
        {
            segment = loop_iterator->second;

            ++stats_.ds_segment_resends_;
            ++stats_.total_snt_ds_;
            if (segment->IsCheckpoint()) ++stats_.total_snt_ds_checkpoints_;

            Send(segment,PARENT_SENDER);
        }

    } else {
        cleanup_sender_session(session->Key(), session->Key(), LTPUDPSegment::LTPUDP_SEGMENT_COMPLETED);
    }
    return; 
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::reconfigure()
{
    // NOTE: params_ have already been updated with the new values by the parent

    if (params_->rate_ != 0) {
        if (NULL == rate_socket_) {
            rate_socket_ = new oasys::RateLimitedSocket(logpath_, 0, 0, params_->bucket_type_);
            rate_socket_->set_socket((oasys::IPSocket *) &socket_);
        }

        // allow updating the rate and the bucket depth on the fly but not the bucket type
        // >> set rate to zero and then reconfigure the bucket type to change it

        rate_socket_->bucket()->set_rate(params_->rate_);

        if (params_->bucket_depth_ != 0) {
            rate_socket_->bucket()->set_depth(params_->bucket_depth_);
        }
 
#ifdef LTPUDPCL_LOG_DEBUG_ENABLED
        log_debug("reconfigure: new rate controller values: rate %llu depth %llu",
                  U64FMT(rate_socket_->bucket()->rate()),
                  U64FMT(rate_socket_->bucket()->depth()));
#endif
    } else {
        if (NULL != rate_socket_) {
            delete rate_socket_;
            rate_socket_ = NULL;
        }
    }

    return true;
}

//----------------------------------------------------------------------
int
LTPUDPConvergenceLayer::Sender::send_bundle(const BundleRef& bundle, in_addr_t next_hop_addr, u_int16_t next_hop_port)
{
    (void) next_hop_addr;
    (void) next_hop_port;

    if (should_stop()) return -1;


    LTPUDPSession* loaded_session = NULL;

    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    size_t total_len        = BundleProtocol::total_length(blocks);

    if (bundle->expired() || bundle->manually_deleting())
    {    
        // do not add to the LTP session
        contact_->link()->del_from_queue(bundle, total_len); 
        BundleDaemon::instance()->post_at_head(
            new BundleExpiredEvent(bundle.object()));
        log_warn("send_bundle: ignoring expired/deleted bundle" ); 
        return -1;
    }    
 



#ifdef ECOS_ENABLED
    bool green           = bundle->ecos_enabled() && bundle->ecos_streaming();
#else
    bool green           = false;
#endif //ECOS_ENABLED

    if (green) {

        LTPUDPSession * green_session = new LTPUDPSession(params_->engine_id_, session_ctr_, parent_, PARENT_SENDER);
        green_session->SetAggTime();
        green_session->Set_Session_State(LTPUDPSession::LTPUDP_SESSION_STATE_DS);
        green_session->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
        green_session->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
        green_session->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);

        green_session->AddToBundleList(bundle, total_len);

        green_session->Set_Is_Green(true);
        // post for offload DS generation
        sender_generate_datasegs_offload_->post(green_session);

        PostTransmitProcessing(green_session);
        delete green_session; // remove it after sent...
        session_ctr_++;  
        if (session_ctr_ > ULONG_MAX) {
            session_ctr_ = LTPUDP_14BIT_RAND;
        }

    } else {  // define scope for the ScopeLock

        // prevent run method from accessing the loading_session_ while working with it
        oasys::ScopeLock loadlock(&loading_session_lock_,"Sender::send_bundle");  

        if (loading_session_ == NULL) {
            oasys::ScopeLock mylock(&session_lock_,"Sender::send_bundle");  // dz debug

            SESS_MAP::iterator session_iterator = track_sender_session(params_->engine_id_,session_ctr_,true);
            if (session_iterator == ltpudp_send_sessions_.end()) {
                return -1;
            }
            loading_session_ = session_iterator->second;
            session_ctr_++; 
            if (session_ctr_ > ULONG_MAX) {
                    session_ctr_ = LTPUDP_14BIT_RAND;
            }
        }
        // use the loading sesssion and add the bundle to it.
 
        update_session_counts(loading_session_, LTPUDPSession::LTPUDP_SESSION_STATE_DS);


        loading_session_->AddToBundleList(bundle, total_len);

        // check the aggregation size and time while we have the loading session
        if ((int) loading_session_->Session_Size() >= params_->agg_size_) {
            loaded_session = loading_session_;
            loading_session_ = NULL;
        } else if (loading_session_->TimeToSendIt(params_->agg_time_)) {
            loaded_session = loading_session_;
            loading_session_ = NULL;
        } else {
        }
    }

    // do link housekeeping now that the lock is released
    contact_->link()->del_from_queue(bundle, total_len); 

    if (!green) {
        contact_->link()->add_to_inflight(bundle, total_len);
    }

    if (loaded_session != NULL && !green) {
        loaded_session->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
        loaded_session->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
        loaded_session->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);
        // post for offload DS generation
        sender_generate_datasegs_offload_->post(loaded_session);
    }

    return total_len;
}
//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Sender::process_bundles(LTPUDPSession* loaded_session,bool is_green)
{
#ifdef LTPUDPCL_LOG_DEBUG_ENABLED
    static size_t max_produce_time = 0;
    static size_t max_gen_segs_time = 0;
    static size_t this_time = 0;
    static oasys::Time otimer;
    otimer.get_time();
#endif


    size_t current_data     = 0;
    size_t segments_created = 0;
    size_t data_len      = loaded_session->Session_Size();
    size_t total_bytes   = loaded_session->Session_Size();
    size_t max_segment_size = params_->seg_size_;
    size_t bundle_ctr       = 0;
    size_t bytes_produced   = 0;
    int client_service_id = 1;

    size_t num_bundles = loaded_session->Bundle_List()->size();

    size_t extra_bytes = ((!params_->ion_comp_ && num_bundles > 1) ? num_bundles : 0);

    u_char * buffer = (u_char *) malloc(total_bytes+extra_bytes);
   
    Bundle * bundle = (Bundle *) NULL;
    BundleRef br("Sender::process_bundles()");  
    BundleList::iterator bundle_iter; 



    if (!params_->ion_comp_) {
        client_service_id = (extra_bytes > 0 ? 2 : 1);
    }

    oasys::ScopeLock bl(loaded_session->Bundle_List()->lock(),"process_bundles()"); 

    for(bundle_iter =  loaded_session->Bundle_List()->begin(); 
        bundle_iter != loaded_session->Bundle_List()->end(); 
        bundle_iter ++)
    {
        bool complete = false;

        bundle_ctr++;

        bundle = *bundle_iter;
        br     = bundle;

        BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());

        ASSERT(blocks != NULL);

        // check for expired/deleted bundle
        if (bundle->expired() || bundle->manually_deleting())
        {
            // do not add to the LTP session
            size_t total_len        = BundleProtocol::total_length(blocks);
            contact_->link()->del_from_queue(br, total_len); 
            BundleDaemon::instance()->post_at_head(
                new BundleExpiredEvent(bundle));
            continue;
        }



   
        if (extra_bytes > 0) {
           *(buffer+bytes_produced) = 1;
           bytes_produced ++;
        }
        size_t total_len = BundleProtocol::produce(bundle, blocks,
                                                   buffer+bytes_produced, 0, 
                                                   (size_t) total_bytes, &complete);
        if (!complete) {
            size_t formatted_len = BundleProtocol::total_length(blocks);
            log_err("process_bundle: bundle too big (%zu > %u) bundle_ctr:%zu",
                    formatted_len, LTPUDPConvergenceLayer::MAX_BUNDLE_LEN,bundle_ctr);
            if (buffer != (u_char *) NULL)free(buffer);

            // signal the BundleDeamon that these bundles failed
            PostTransmitProcessing(loaded_session, false);

            return -1;
        }

        total_bytes    -= total_len;
        bytes_produced += total_len;
    }

#ifdef LTPUDPCL_LOG_DEBUG_ENABLED
     //dzdebug
    this_time = otimer.elapsed_ms();
    if (this_time > max_produce_time) {
        max_produce_time = this_time;
        log_always("dzdebug - new max time to produce: %zu ms  bundles: %zu", max_produce_time, num_bundles);
    }
    otimer.get_time();
#endif


    data_len   = loaded_session->Session_Size()+extra_bytes;

    while( data_len > 0)
    {
        bool checkpoint = false;

        LTPUDPDataSegment * ds = new LTPUDPDataSegment(loaded_session);

        segments_created ++;

        ds->Set_Client_Service_ID(client_service_id); 

        if (is_green)
            ds->Set_RGN(GREEN_SEGMENT);
        else
            ds->Set_RGN(RED_SEGMENT);

        //
        // always have header and trailer....  
        //
        //if (current_data == 0)
        //{
            ds->Set_Security_Mask(LTPUDP_SECURITY_HEADER | LTPUDP_SECURITY_TRAILER);
        //} else {
        //    ds->Set_Security_Mask(LTPUDP_SECURITY_TRAILER);
        //}
       /* 
        * see if it's a full packet or not
        */ 
        if (data_len  > max_segment_size) {
           ds->Encode_Buffer(&buffer[current_data], max_segment_size, current_data, checkpoint);
           data_len         -= max_segment_size;
           current_data     += max_segment_size; 
        } else {
           ds->Encode_Buffer(&buffer[current_data], data_len, current_data, true);
           data_len         -= data_len;
           current_data     += data_len; 
        }

        // generating the new segments so no possible overlap
        loaded_session->AddSegment((LTPUDPDataSegment*) ds, false);  //false = no check for overlap
                
        if (!is_green) {
            if (ds->IsCheckpoint()) {
                ds->Set_Retrans_Retries(0);
                ds->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                                LTPUDPSegment::LTPUDP_SEGMENT_DS_TO);

                ++stats_.total_snt_ds_checkpoints_;
            }
        }

        ++stats_.total_snt_ds_;

        Send_Low_Priority(ds,PARENT_SENDER); 

    }

#ifdef LTPUDPCL_LOG_DEBUG_ENABLED
     //dzdebug
    this_time = otimer.elapsed_ms();
    if (this_time > max_gen_segs_time) {
        max_gen_segs_time = this_time;
        log_always("dzdebug - new max time to generate data segs: %zu ms  bytes: %zu", max_gen_segs_time, bytes_produced);
    }
#endif

    if (buffer != NULL) {
        free(buffer);
    }
    return bytes_produced; 
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Send_Remaining_Segments(string session_key)
{ 
    SESS_MAP::iterator remaining_iterator = ltpudp_send_sessions_.find(session_key);

    if (remaining_iterator != ltpudp_send_sessions_.end())
    {
        LTPUDPSession * ltpsession = remaining_iterator->second;
        Send_Remaining_Segments(ltpsession); 
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::cleanup_sender_session(string session_key, string segment, 
                                                       int segment_type)
{ 
    //
    // ONLY DEALS WITH RED SEGMENTS
    //
    ASSERT(session_lock_.is_locked_by_me());

    LTPUDPDataSegment   * ds         = (LTPUDPDataSegment   *) NULL; 
    LTPUDPCancelSegment * cs         = (LTPUDPCancelSegment *) NULL; 
    LTPUDPSession       * ltpsession = (LTPUDPSession       *) NULL;

    DS_MAP::iterator   cleanup_segment_iterator;

    SESS_MAP::iterator cleanup_session_iterator = ltpudp_send_sessions_.find(session_key);

    if (cleanup_session_iterator != ltpudp_send_sessions_.end())
    {
        ltpsession = cleanup_session_iterator->second;

        ltpsession->Cancel_Inactivity_Timer();

        // lookup the segment and resubmit if it was a timeout
        switch(segment_type)
        { 
            case LTPUDPSegment::LTPUDP_SEGMENT_DS_TO:
            case LTPUDPSegment::LTPUDP_SEGMENT_RS_TO:
                 cleanup_segment_iterator = ltpsession->Red_Segments().find(segment);
                 if (cleanup_segment_iterator != ltpsession->Red_Segments().end())
                 {
                     ds = cleanup_segment_iterator->second;
                     ds->Cancel_Retransmission_Timer();

                     ds->Increment_Retries();
                     if (ds->Retrans_Retries() <= params_->retran_retries_) {
                         ++stats_.ds_session_checkpoint_resends_;
                         ++stats_.ds_segment_resends_;
                         update_session_counts(ltpsession, LTPUDPSession::LTPUDP_SESSION_STATE_DS);
                         Resend_Checkpoint(ltpsession);
                     } else {
                         build_CS_segment(ltpsession,LTPUDPSegment::LTPUDP_SEGMENT_CS_BS, 
                                          LTPUDPCancelSegment::LTPUDP_CS_REASON_RXMTCYCEX);

                         //signal DTN that transmit failed for the bundle(s)
                         PostTransmitProcessing(ltpsession, false);
                     }
                 } else {
                 }
                 return;
                 break;

            case LTPUDPSegment::LTPUDP_SEGMENT_CS_TO:
                 cs = ltpsession->Cancel_Segment();
                 cs->Cancel_Retransmission_Timer();
                 cs->Increment_Retries();

                 if (cs->Retrans_Retries() <= params_->retran_retries_) {
                     cs->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                                     LTPUDPSegment::LTPUDP_SEGMENT_CS_TO);

                     ++stats_.cs_session_resends_;

                     Send(cs,PARENT_SENDER);
                     return; 
                 } else {
                 }
                 break;

            case LTPUDPSegment::LTPUDP_SEGMENT_CS_BR:
                 ++stats_.cs_session_cancel_by_receiver_;

                 // signal DTN that transmit failed for the bundle(s)
                 PostTransmitProcessing(ltpsession, false);
                 break;

            case LTPUDPSegment::LTPUDP_SEGMENT_COMPLETED: 
                 PostTransmitProcessing(ltpsession);
                 break;

            default: // all other cases don't lookup a segment...
                 return;
                 break;
        }

        update_session_counts(ltpsession, LTPUDPSession::LTPUDP_SESSION_STATE_UNDEFINED);
        delete ltpsession;
        ltpudp_send_sessions_.erase(cleanup_session_iterator);

    } else {
    }

    return;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Resend_Checkpoint(string session_key)
{
    SESS_MAP::iterator session_iterator = ltpudp_send_sessions_.find(session_key);
    if (session_iterator != ltpudp_send_sessions_.end())
    {
        LTPUDPSession * session = session_iterator->second;
        Resend_Checkpoint(session);
    } else {
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Resend_Checkpoint(LTPUDPSession * session)
{
    //
    // ONLY DEALS WITH RED SEGMENTS
    //
    DS_MAP::iterator ltpudp_segment_iterator = --session->Red_Segments().end(); // get the last one!

    if (ltpudp_segment_iterator != session->Red_Segments().end()) {
        LTPUDPDataSegment * segment = ltpudp_segment_iterator->second; 
        if (!segment->IsCheckpoint())
        {
            segment->Set_Report_Serial_Number(session->Increment_Report_Serial_Number());
            if (session->Report_Serial_Number() > ULONG_MAX)
            {
                build_CS_segment(session,LTPUDPSegment::LTPUDP_SEGMENT_CS_BS, 
                                 LTPUDPCancelSegment::LTPUDP_CS_REASON_SYS_CNCLD);
                session->Set_Report_Serial_Number(LTPUDP_14BIT_RAND); // restart the random ctr
                return;
            }
            segment->Set_Checkpoint_ID(session->Increment_Checkpoint_ID());
            segment->SetCheckpoint(); // turn on the checkpoint bits in the control byte
            segment->Encode_All();
            segment->Set_Retrans_Retries(0);
        } else {
        }

        segment->Create_Retransmission_Timer(params_->retran_intvl_,PARENT_SENDER, 
                                             LTPUDPSegment::LTPUDP_SEGMENT_DS_TO);

        ++stats_.total_snt_ds_;
        ++stats_.total_snt_ds_checkpoints_;

        Send(segment,PARENT_SENDER);
    } else {
    }
}

//----------------------------------------------------------------------
SESS_MAP::iterator 
LTPUDPConvergenceLayer::Sender::track_sender_session(u_int64_t engine_id,u_int64_t session_ctr,bool create_flag)
{
    SESS_MAP::iterator session_iterator;
    char key[45];
    sprintf(key,"%20.20" PRIu64 ":%20.20" PRIu64,engine_id,session_ctr);
    string insert_key = key;

    session_iterator = ltpudp_send_sessions_.find(insert_key);

    if (session_iterator == ltpudp_send_sessions_.end())
    {
        if (create_flag) {
            LTPUDPSession * new_session = new LTPUDPSession(engine_id, session_ctr, parent_, PARENT_SENDER);

            new_session->SetAggTime();
            new_session->Set_Outbound_Cipher_Key_Id(params_->outbound_cipher_key_id_);
            new_session->Set_Outbound_Cipher_Suite(params_->outbound_cipher_suite_);
            new_session->Set_Outbound_Cipher_Engine(params_->outbound_cipher_engine_);


//dzdebug -  get session_iterator from insert   no find needed/
            ltpudp_send_sessions_.insert(std::pair<std::string, LTPUDPSession*>(insert_key, new_session));
            session_iterator = ltpudp_send_sessions_.find(insert_key);

            if (ltpudp_send_sessions_.size() > stats_.max_sessions_) {
                stats_.max_sessions_ = ltpudp_send_sessions_.size();
            }
            ++stats_.total_sessions_;
        } else {
        }
    } else {
    }
    
    return session_iterator;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::dump_link(oasys::StringBuffer* buf)
{
    if (NULL == rate_socket_) {
        buf->appendf("transmit rate: unlimited\n");
    } else {
        buf->appendf("transmit rate: %" PRIu64 "\n", rate_socket_->bucket()->rate());
        if (0 == params_->bucket_type_) 
            buf->appendf("bucket_type: Standard\n");
        else
            buf->appendf("bucket_type: Leaky\n");
        buf->appendf("bucket_depth: %" PRIu64 "\n", rate_socket_->bucket()->depth());
        buf->appendf("wait_till_sent: %d\n",(int) params_->wait_till_sent_);
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::check_ready_for_bundle()
{
    bool result = true;
    while (!should_stop() && result && 
           (loading_session_ != NULL || ltpudp_send_sessions_.size() < params_->max_sessions_)) {
        result = parent_->pop_queued_bundle(contact_->link());
    }
    return result;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::Post_Segment_to_Process(LTPUDPSegment* seg)
{
    if (!should_stop())
    {
        sender_process_offload_->post(seg);
    }
    else
    {
        delete seg;
    }
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SendPoller::SendPoller(Sender* parent, const LinkRef& link)
    : Logger("LTPUDPConvergenceLayer::Sender::SendPoller",
             "/dtn/cl/ltpudp/sender/poller/%p", this),
      Thread("LTPUDPConvergenceLayer::Sender::SendPoller")
{
    parent_ = parent;
    link_ = link;
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SendPoller::~SendPoller()
{
    set_should_stop();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SendPoller::run()
{
    char threadname[16] = "LtpSndrPollr";
    pthread_setname_np(pthread_self(), threadname);
   
    // set thread niceness
    setpriority( PRIO_PROCESS, 0, -7);
    
    while (!should_stop()) {
        parent_->check_ready_for_bundle();
        usleep(1);
    }
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SenderProcessOffload::SenderProcessOffload(Sender* parent)
    : Logger("LTPUDPConvergenceLayer::Sender::SenderProcessOffload",
             "/dtn/cl/ltpudp/receiver/dataproc/%p", this),
      Thread("LTPUDPConvergenceLayer::Sender::SenderProcessOffload")
{
    parent_ = parent;

    eventq_ = new oasys::MsgQueue<LTPUDPSegment*>(logpath_);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SenderProcessOffload::~SenderProcessOffload()
{
    set_should_stop();

    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

    LTPUDPSegment* obj;
    while (eventq_->try_pop(&obj)) {
        delete obj;
    }
    delete eventq_;
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Sender::SenderProcessOffload::queue_size()
{
    return eventq_->size();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SenderProcessOffload::post(LTPUDPSegment* seg)
{
    // send an ACK and then queue for further processing
    if (generateACK(seg))
    {
        oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

        if (!should_stop())
        {
            eventq_->push_back(seg);
        }
        else
        {
            delete seg;
        }
    }
}

//----------------------------------------------------------------------
bool
LTPUDPConvergenceLayer::Sender::SenderProcessOffload::generateACK(LTPUDPSegment* seg)
{
    bool result = true;
    LTPUDPReportAckSegment * ras;
    LTPUDPCancelAckSegment * cas_seg;

    switch(seg->Segment_Type()) {

          case LTPUDPSegment::LTPUDP_SEGMENT_RS:
               ras = new LTPUDPReportAckSegment((LTPUDPReportSegment*)seg, parent_->Blank_Session());
               parent_->Send(ras, PARENT_SENDER);
               delete ras;
               break;

          case LTPUDPSegment::LTPUDP_SEGMENT_CS_BR:
               cas_seg = new LTPUDPCancelAckSegment((LTPUDPCancelSegment *) seg, 
									      LTPUDPSEGMENT_CAS_BR_BYTE, parent_->Blank_Session()); 
               parent_->Send(cas_seg, PARENT_SENDER); 
               delete cas_seg;
               break;

          case LTPUDPSegment::LTPUDP_SEGMENT_CAS_BS:
               // nothing to send - just pass through to process
               break;

          default:
               result = false;
               log_err("Unknown Segment Type for SenderProcessOffload::generateACK(%s)",
                       seg->Get_Segment_Type()); 
               delete seg;
               break;
    }

    return result;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SenderProcessOffload::run()
{
    char threadname[16] = "LtpSndrProcOfld";
    pthread_setname_np(pthread_self(), threadname);
   
    // set thread niceness
    setpriority( PRIO_PROCESS, 0, -11);
    

    LTPUDPSegment* seg;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (!should_stop()) {

        ret = oasys::IO::poll_multiple(pollfds, 1, 10, NULL);

        if (ret == oasys::IOINTR) {
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("SenderProcessOffload IO Error - aborting");
            set_should_stop();
            continue;
        }

        // check for a seg to process
        if (event_poll->revents & POLLIN) {
            seg = NULL;

            queue_lock_.lock( __FUNCTION__);

            if (eventq_->try_pop(&seg)) {
                ASSERT(seg != NULL)
            }

            queue_lock_.unlock();

            if (seg) {
                if (!should_stop()) {
                    parent_->Process_Segment(seg);
                }
                delete seg;
            }
        }
    }
}




//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SenderGenerateDataSegsOffload::SenderGenerateDataSegsOffload(Sender* parent)
    : Logger("LTPUDPConvergenceLayer::Sender::SenderGenerateDataSegsOffload",
             "/dtn/cl/ltpudp/sender/gendatasegs/%p", this),
      Thread("LTPUDPConvergenceLayer::Sender::SenderGenerateDataSegsOffload")
{
    parent_ = parent;

    eventq_ = new oasys::MsgQueue<LTPUDPSession *>(logpath_);
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::Sender::SenderGenerateDataSegsOffload::~SenderGenerateDataSegsOffload()
{
    set_should_stop();

    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

    LTPUDPSession* obj;
    while (eventq_->try_pop(&obj)) {
        //delete obj;
    }
    delete eventq_;
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SenderGenerateDataSegsOffload::init(Params* params)
{
    params_ = params;
}

//----------------------------------------------------------------------
size_t
LTPUDPConvergenceLayer::Sender::SenderGenerateDataSegsOffload::queue_size()
{
    return eventq_->size();
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SenderGenerateDataSegsOffload::post(LTPUDPSession* session)
{
    oasys::ScopeLock l(&queue_lock_, __FUNCTION__);

    if (!should_stop())
    {
        eventq_->push_back(session);
    }
}

//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::Sender::SenderGenerateDataSegsOffload::run()
{
    char threadname[16] = "LtpSndrGenDS";
    pthread_setname_np(pthread_self(), threadname);

    // set thread niceness
    setpriority( PRIO_PROCESS, 0, -10);
    
    // set thread priority
/*
    int oldpolicy = -1;
    sched_param priority;
    sched_param new_param;
    pthread_getschedparam(pthread_self(), &oldpolicy, &priority);
    log_always("old priority = %d", priority.sched_priority);
    priority.sched_priority = 1;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &priority);
*/
   
    LTPUDPSession* session;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

#ifdef LTPUDPCL_DEBUG_TIMING_ENABLED
    size_t sessions_processed = 0;
    size_t total_work_time = 0;
    size_t avg_time = 0;
    size_t max_time = 0;
    size_t this_time = 0;
    oasys::Time proc_timer;
    oasys::Time report_timer;
    report_timer.get_time();
#endif

    while (!should_stop()) {

        ret = oasys::IO::poll_multiple(pollfds, 1, 10, NULL);

        if (ret == oasys::IOINTR) {
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("SenderGenerateDataSegsOffload IO Error - aborting");
            set_should_stop();
            continue;
        }

        // check for a seg to process
        if (event_poll->revents & POLLIN) {
            session = NULL;

            queue_lock_.lock( __FUNCTION__);

            if (eventq_->try_pop(&session)) {
                ASSERT(session != NULL)
            }

            queue_lock_.unlock();

            if (session) {
                if (!should_stop()) {
#ifdef LTPUDPCL_DEBUG_TIMING_ENABLED
                    if (params_->dbg_timing_)
                    {
                        proc_timer.get_time();
                    }
#endif

                    parent_->process_bundles(session, session->IsGreen());

#ifdef LTPUDPCL_DEBUG_TIMING_ENABLED
                    if (params_->dbg_timing_)
                    {
                        this_time = proc_timer.elapsed_ms();
                        total_work_time += this_time;
                        if (this_time > max_time) {
                            max_time = this_time;
                        }
                        ++sessions_processed;
                    } else {
                        max_time = 0;
                   }
#endif
                }
            }
        }

#ifdef LTPUDPCL_DEBUG_TIMING_ENABLED
        if (params_->dbg_timing_)
        {
            if (report_timer.elapsed_ms() >= 1000) {
                if (sessions_processed > 0) {
                    avg_time = total_work_time / sessions_processed;
                    log_always("dzdebug ltp sessions generated: %zu  avg: %zu max: %zu ms   queue size: %zu", 
                               sessions_processed, avg_time, max_time, eventq_->size());
                }
                report_timer.get_time();
                sessions_processed = 0;
                total_work_time = 0;
            }
        }
#endif
    }
}



//----------------------------------------------------------------------
LTPUDPConvergenceLayer::TimerProcessor::TimerProcessor()
    : Thread("LTPUDPConvergenceLayer::TimerProcessor"),
      Logger("LTPUDPConvergenceLayer::TimerProcessor",
             "/dtn/cl/ltpudp/timerproc/%p", this)
{
    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    eventq_ = new oasys::MsgQueue< oasys::Timer* >(logpath_);
    start();
}

//----------------------------------------------------------------------
LTPUDPConvergenceLayer::TimerProcessor::~TimerProcessor()
{
    // free all pending events
    oasys::Timer* event;
    while (eventq_->try_pop(&event))
        delete event;

    delete eventq_;
}

/// Post a Timer to trigger
//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::TimerProcessor::post(oasys::Timer* event)
{
    eventq_->push_back(event);
}

/// TimerProcessor main loop
//----------------------------------------------------------------------
void
LTPUDPConvergenceLayer::TimerProcessor::run() 
{
    char threadname[16] = "LtpTimerProc";
    pthread_setname_np(pthread_self(), threadname);
   

    struct timeval dummy_time;
    ::gettimeofday(&dummy_time, 0);

    // block on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq_->read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    while (!should_stop()) {
        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 10);

        if (ret == oasys::IOINTR) {
            log_err("timer processor interrupted");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            oasys::Timer* event;
            if (eventq_->try_pop(&event)) {
                ASSERT(event != NULL)

                event->timeout(dummy_time);
                // NOTE: these timers delete themselves
            }
        }
    }
}

} // namespace dtn

#endif // LTPUDP_ENABLED

#undef LTPUDPCL_LOG_DEBUG_ENABLED

