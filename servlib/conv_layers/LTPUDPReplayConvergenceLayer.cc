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


#include <time.h>
#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include <naming/EndpointID.h>
#include <naming/IPNScheme.h>
#include <oasys/util/HexDumpBuffer.h>

#include "LTPUDPReplayConvergenceLayer.h"
#include "LTPUDPCommon.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTimestamp.h"
#include "bundling/SDNV.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"

namespace dtn {

class LTPUDPReplayConvergenceLayer::Params LTPUDPReplayConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    int temp = 0;

    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("local_port", &local_port_);
    a->process("ttl", &ttl_);
    a->process("dest", &dest_pattern_);
    a->process("remote_port", &remote_port_);
    a->process("bucket_type", &temp);
    a->process("rate", &rate_);
    a->process("inact_intvl", &inactivity_intvl_);
    a->process("bucket_depth", &bucket_depth_);
    a->process("wait_till_sent", &wait_till_sent_);
    a->process("icipher_suite", &inbound_cipher_suite_);
    a->process("icipher_keyid", &inbound_cipher_key_id_);
    a->process("icipher_engine",  &inbound_cipher_engine_);

}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::LTPUDPReplayConvergenceLayer()
    : IPConvergenceLayer("LTPUDPReplayConvergenceLayer", "ltpudpreplay")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = LTPUDPREPLAYCL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = 0;
    defaults_.bucket_type_              = (oasys::RateLimitedSocket::BUCKET_TYPE) 0;
    defaults_.rate_                     = 0; // unlimited
    defaults_.bucket_depth_             = 0; // default
    defaults_.wait_till_sent_           = false;
    defaults_.ttl_                      = 259200;
    defaults_.inactivity_intvl_         = 3600;
    defaults_.dest_pattern_             = EndpointID();
    defaults_.no_filter_                = true;
    defaults_.retran_intvl_             = 60;
    defaults_.retran_retries_           = 0;
    defaults_.inbound_cipher_suite_     = -1;
    defaults_.inbound_cipher_key_id_    = -1;
    defaults_.inbound_cipher_engine_    = "";

    next_hop_addr_                      = INADDR_NONE;
    next_hop_port_                      = 0;
    next_hop_flags_                     = 0;

    timer_processor_                    = NULL;
}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::~LTPUDPReplayConvergenceLayer()
{
    if (timer_processor_ != NULL) {
        timer_processor_->set_should_stop();
    }
}
//----------------------------------------------------------------------
CLInfo*
LTPUDPReplayConvergenceLayer::new_link_params()
{
    return new LTPUDPReplayConvergenceLayer::Params(LTPUDPReplayConvergenceLayer::defaults_);
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_params(&LTPUDPReplayConvergenceLayer::defaults_, argc, argv, invalidp);
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    oasys::OptParser p;
    int temp                     = 0;
    string      icipher_eng_str  = "";
    std::string dest_filter_str  = "";

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::IntOpt("bucket_type", &temp));
    p.addopt(new oasys::IntOpt("retran_retries", &params->retran_retries_));
    p.addopt(new oasys::IntOpt("retran_intvl", &params->retran_intvl_));
    p.addopt(new oasys::UInt64Opt("ttl", &params->ttl_));
    p.addopt(new oasys::UInt64Opt("rate", &params->rate_));
    p.addopt(new oasys::UInt64Opt("bucket_depth", &params->bucket_depth_));
    p.addopt(new oasys::BoolOpt("wait_till_sent",&params->wait_till_sent_));
    p.addopt(new oasys::UIntOpt("inact_intvl", &params->inactivity_intvl_));
    p.addopt(new oasys::StringOpt("dest", &dest_filter_str));
    p.addopt(new oasys::IntOpt("icipher_suite", &params->inbound_cipher_suite_));
    p.addopt(new oasys::IntOpt("icipher_keyid", &params->inbound_cipher_key_id_));
    p.addopt(new oasys::StringOpt("icipher_engine", &icipher_eng_str));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    if(icipher_eng_str.length() > 0) {
        params->inbound_cipher_engine_ = icipher_eng_str;
    }

    if(dest_filter_str.length() > 0)
    {
        params->no_filter_ = false;
        params->dest_pattern_.assign(dest_filter_str);
    }

    params->bucket_type_ = (oasys::RateLimitedSocket::BUCKET_TYPE) temp;

    return true;
};
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    
    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = LTPUDPReplayConvergenceLayer::defaults_;
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
    receiver->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
    
    if (receiver->bind(params.local_addr_, params.local_port_) != 0) {
        return false; // error log already emitted
    }
    
    // check if the user specified a remote addr/port to connect to
    if (params.remote_addr_ != INADDR_NONE) {
        if (receiver->connect(params.remote_addr_, params.remote_port_) != 0) {
            return false; // error log already emitted
        }
    }
   
    receiver_ = receiver;

    if (timer_processor_) {
        delete timer_processor_;
        timer_processor_ = NULL;
    } 
    timer_processor_                    = new TimerProcessor();


    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    return true;
}

//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::interface_activate(Interface* iface)
{
    // start listening and then start the thread to loop calling accept()
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->start();
}

//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::interface_down(Interface* iface)
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
LTPUDPReplayConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Receiver* rcvr = (Receiver*)iface->cl_info();

    //Params* params = &((Receiver*)iface->cl_info())->params_;
    Params* params = &rcvr->params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
    buf->appendf("\tttl: %ld\n", params->ttl_);
    buf->appendf("\tinactivity_intvl: %d\n", params->inactivity_intvl_);
    buf->appendf("\tdest: %s\n", params->dest_pattern_.c_str());
    
    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tbound to remote_addr: %s remote_port: %d\n",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tlistening\n");
    }

    buf->appendf("\tUDP packets processed: %" PRIu64 "\n",  
                     rcvr->udp_packets_received());
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::init_link(const LinkRef& link,
                               int argc, const char* argv[])
{
    (void) link;
    (void) argc;
    (void) argv;

    log_crit("LTPUDPReplayConvergenceLayer::init_contact called! - Use as Interface not as Link");
    return false;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("LTPUDPReplayConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_),  params->local_port_);

    buf->appendf("\tremote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);
    buf->appendf("inact_intvl: %u\n",         params->inactivity_intvl_);
    buf->appendf("rate: %" PRIu64 "\n",                params->rate_);
    buf->appendf("bucket_depth: %" PRIu64 "\n",        params->bucket_depth_);
}
//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::open_contact(const ContactRef& contact)
{
    (void) contact;

    log_crit("LTPUDPReplayConvergenceLayer::open_contact called! - Use as Interface not as Link");
    return false;
}

//----------------------------------------------------------------------
bool
LTPUDPReplayConvergenceLayer::close_contact(const ContactRef& contact)
{
    log_info("close_contact *%p", contact.object());
    return true;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void) link;

    log_crit("bundle_queue called -  Bundle ref %s\n", bundle->dest().c_str());   
}
//----------------------------------------------------------------------
u_int32_t
LTPUDPReplayConvergenceLayer::Inactivity_Interval()
{
     return session_params_.inactivity_intvl_;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Cleanup_Replay_Session_Receiver(string session_key,int force)
{
    receiver_->Cleanup_Session(session_key,force);
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Post_Timer_To_Process(oasys::Timer* event)
{
    timer_processor_->post(event);
}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::Receiver::Receiver(LTPUDPReplayConvergenceLayer::Params* params, LTPUDPCallbackIF * parent)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/udp/receiver")),
      UDPClient("/dtn/cl/ltpudpreplay/receiver"),
      Thread("LTPUDPReplayConvergenceLayer::Receiver"),
      rcvr_lock_()
{
    logfd_     = false;
    params_    = *params;
    parent_    = parent;

    udp_packets_received_ = 0;
    ltp_session_canceled_ = 0;
}
//----------------------------------------------------------------------
void 
LTPUDPReplayConvergenceLayer::Receiver::track_session(u_int64_t engine_id, u_int64_t session_id)
{
    char key[45];
    sprintf(key,"%20.20ld:%20.20ld",engine_id,session_id);
    string insert_key = key;
    ltpudpreplay_session_iterator_ = ltpudpreplay_sessions_.find(insert_key);
    if(ltpudpreplay_session_iterator_ == ltpudpreplay_sessions_.end())
    {
        LTPUDPSession * new_session = new LTPUDPSession(engine_id, session_id, parent_, PARENT_RECEIVER);
        ltpudpreplay_sessions_.insert(std::pair<std::string, LTPUDPSession*>(insert_key, new_session));
        ltpudpreplay_session_iterator_ = ltpudpreplay_sessions_.find(insert_key);
    }
    return;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Receiver::cleanup_session(string session_key, int force)
{ // lock should've occured before this routine is hit

    SESS_MAP::iterator cleanup_iterator = ltpudpreplay_sessions_.find(session_key);

    if(cleanup_iterator != ltpudpreplay_sessions_.end())
    {
        if(!force)
        {
            LTPUDPSession * ltpsession = cleanup_iterator->second;
            time_t time_left = ltpsession->Last_Packet_Time() + (params_.inactivity_intvl_);
            time_left = time_left - time(NULL);
            if(time_left > 0)
            {
                ltpsession->Start_Inactivity_Timer(time_left);
                return;
            }
        } 

        LTPUDPSession * ltpsession = cleanup_iterator->second;
        ltpsession->Cancel_Inactivity_Timer();
        delete ltpsession;   // calls the session destructor that calls the segments destructor...
        ltpudpreplay_sessions_.erase(cleanup_iterator); // cleanup session!!!
    }
    return;
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Receiver::process_data(u_char* bp, size_t len)
{
    int bytes_to_process = 0; 

    LTPUDPSession * ltpudpreplay_session;

    LTPUDPSegment * new_segment = LTPUDPSegment::DecodeBuffer(bp,len);
  
    if(new_segment == NULL)return; // didn't find a valid segment

    if (new_segment->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_DS     && 
        new_segment->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_CS_BS  &&   
        new_segment->Segment_Type() != LTPUDPSegment::LTPUDP_SEGMENT_CS_BR) { // clean up session if a cancel flag is seen...
       delete new_segment;
       return;
    }

#ifdef LTPUDP_AUTH_ENABLED
    if(new_segment->IsSecurityEnabled()) {
        if(!new_segment->IsAuthenticated(params_.inbound_cipher_suite_, 
                                         params_.inbound_cipher_key_id_,
                                         params_.inbound_cipher_engine_)) { 
            log_warn("process_data: invalid segment doesn't authenticate.... type:%s length:%d",
                 LTPUDPSegment::Get_Segment_Type(new_segment->Segment_Type()),(int) len);
            delete new_segment;
            return;
        }
    }
#endif

    oasys::ScopeLock l(&rcvr_lock_,"RCVRprocess_data");  // lock here since we reuse something set in track session!!!

    track_session(new_segment->Engine_ID(),new_segment->Session_ID());  // this guy sets the ltpudpreplay_session_iterator...

    if (new_segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BS || 
        new_segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_CS_BR)  // clean up session if a cancel flag is seen...
    {
        ++ltp_session_canceled_;

        delete new_segment;
        ltpudpreplay_session = ltpudpreplay_session_iterator_->second;
        ltpudpreplay_session->Set_Cleanup();
        cleanup_session(ltpudpreplay_session_iterator_->first, 1);
        return;
    } 

    // create a map entry unless one exists and add the segment to either red or green segments
    // we know it's a data segment....
    // create key for segment  (Range of numbers)????  will be done base off new_segment

    if(new_segment->Segment_Type() == LTPUDPSegment::LTPUDP_SEGMENT_DS) { // only handle data segments right now!!!

        if(new_segment->Service_ID() != 1 &&
           new_segment->Service_ID() != 2)
        {
             log_err("Service ID != 1 | 2 (%d)",new_segment->Service_ID());
             delete new_segment;
             return;
        }

        ltpudpreplay_session = ltpudpreplay_session_iterator_->second;

        ltpudpreplay_session->AddSegment((LTPUDPDataSegment*)new_segment);

        if (new_segment->IsEndofblock()) {
            ltpudpreplay_session->Set_EOB((LTPUDPDataSegment*)new_segment);
        }

        bytes_to_process = ltpudpreplay_session->IsRedFull();
        if(bytes_to_process > 0) {
            process_all_data(ltpudpreplay_session->GetAllRedData(),
                             (size_t) ltpudpreplay_session->Expected_Red_Bytes(), 
                             new_segment->Service_ID() == 2 ? true : false);
        }

        bytes_to_process = ltpudpreplay_session->IsGreenFull();
        if(bytes_to_process > 0) {
            process_all_data(ltpudpreplay_session->GetAllGreenData(),
                             (size_t)ltpudpreplay_session->Expected_Green_Bytes(), 
                             new_segment->Service_ID() == 2 ? true : false);
        }

        if(ltpudpreplay_session->DataProcessed())
        {
            ltpudpreplay_session->Set_Cleanup();
            cleanup_session(ltpudpreplay_session_iterator_->first,1);
        }
    }
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Receiver::process_all_data(u_char * bp, size_t len, bool multi_bundle)
{
    u_int32_t check_client_service = 0;
    // the payload should contain a full bundle
    int all_data = (int) len;
    u_char * current_bp = bp;

    if(bp == (u_char *) NULL)
    {
        log_err("process_all_data: no LTPUDPReplay bundle protocol error");
    }


    while(all_data > 0)
    {
        Bundle* bundle = new Bundle();


        if (multi_bundle) {
            // Multi bundle buffer... must make sure a 1 precedes all packets
            int bytes_used = SDNV::decode(current_bp, all_data, &check_client_service);
            if(check_client_service != 1)
            {
                log_err("process_all_data: LTPReplay SDA - invalid Service ID: %d",check_client_service);
                delete bundle;
                break; // so we free the bp
            }
            current_bp += bytes_used;
            all_data   -= bytes_used;
        }
 
        bool complete = false;
        int cc = BundleProtocol::consume(bundle, current_bp, len, &complete);

        if (cc < 0) {
            log_err("process_all_data: bundle protocol error");
            delete bundle;
            break;
        }

        if (!complete) {
            log_err("process_all_data: incomplete bundle");
            delete bundle;
            break;
        }

        // add filter for destination and adjust lifetime data based on params
        all_data   -= cc;
        current_bp += cc;

        if(params_.no_filter_ || params_.dest_pattern_.match(bundle->dest())) {

            int now =  BundleTimestamp::get_current_time();
            bundle->set_expiration((now-bundle->creation_ts().seconds_)+params_.ttl_); // update expiration time

            BundleDaemon::post(
                new BundleReceivedEvent(bundle, EVENTSRC_PEER, len, EndpointID::NULL_EID()));
        }

        else
        {
            //log_debug("process_all_data: deleting filtered bundle: %s  ID: %" PRIbid, bundle->gbofid_str().c_str(), bundle->bundleid());
            delete bundle;
        }
    }
    if(bp != (u_char *) NULL)free(bp);
}
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::Receiver::run()
{
    char threadname[16] = "LtpReplayRecvr";
    pthread_setname_np(pthread_self(), threadname);
   

    int ret;
    in_addr_t addr;
    u_int16_t port;
    u_char buf[MAX_UDP_PACKET];

    while (1) {

        if (should_stop())break;
        
        ret = recvfrom((char*)buf, MAX_UDP_PACKET, 0, &addr, &port);
        if (ret <= 0) {   
            if (errno == EINTR) {
                continue;
            }
            log_err("error in recvfrom(): %d %s",
                    errno, strerror(errno));
            close();
            break;
        }
        
        ++udp_packets_received_;

        process_data(buf, ret);
    }
}

//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::TimerProcessor::TimerProcessor()
    : Thread("LTPUDPReplayConvergenceLayer::TimerProcessor"),
      Logger("LTPUDPReplayConvergenceLayer::TimerProcessor",
             "/dtn/cl/ltpudp/timerproc/%p", this)
{
    // we always delete the thread object when we exit
    Thread::set_flag(Thread::DELETE_ON_EXIT);

    eventq_ = new oasys::MsgQueue< oasys::Timer* >(logpath_);
    start();
}
//----------------------------------------------------------------------
LTPUDPReplayConvergenceLayer::TimerProcessor::~TimerProcessor()
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
LTPUDPReplayConvergenceLayer::TimerProcessor::post(oasys::Timer* event)
{
    eventq_->push_back(event);
}
/// TimerProcessor main loop
//----------------------------------------------------------------------
void
LTPUDPReplayConvergenceLayer::TimerProcessor::run() 
{
    char threadname[16] = "LtpReplayTimerP";
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
