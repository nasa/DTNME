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


#include <iostream>
#include <string>
#include <sys/timeb.h>
#include <climits>

#include <third_party/oasys/io/NetUtils.h>
#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/util/OptParser.h>
#include <third_party/oasys/util/StringBuffer.h>
#include <third_party/oasys/util/HexDumpBuffer.h>
#include <third_party/oasys/io/IO.h>
#include <naming/EndpointID.h>
#include <naming/IPNScheme.h>

#include "LTPEngine.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTimestamp.h"
#include "bundling/SDNV.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "storage/BundleStore.h"

namespace dtn {


//-----------------------------------------------------------------------
LTPEngineReg::LTPEngineReg(SPtr_LTPCLSenderIF& clsender)
{
    remote_engine_id_ = clsender->Remote_Engine_ID();
    clsender_  = clsender;
    ltpnode_   = nullptr;
}

//-----------------------------------------------------------------------
LTPEngineReg::~LTPEngineReg()
{
}



//----------------------------------------------------------------------
LTPEngine::LTPEngine(uint64_t local_engine_id) 
     : Logger("LTPEngine::", "/dtn/ltpeng")
{
    local_engine_id_ = local_engine_id;
    session_id_ = 0;

    data_processor_ = std::unique_ptr<RecvDataProcessor>(new RecvDataProcessor());
    data_processor_->start();
}

//----------------------------------------------------------------------
void
LTPEngine::unregister_engine(uint64_t engine)
{
     oasys::ScopeLock my_lock(&enginereg_map_lock_,"unregister_engine");  

     ENGINE_MAP::iterator reg_iter = registered_engines_.find(engine);

     if (reg_iter != registered_engines_.end()) {

         SPtr_LTPEngineReg reg_entry = reg_iter->second;
         if (reg_entry != nullptr)
         {
             SPtr_LTPNode node_ptr = reg_entry->LTP_Node();
             registered_engines_.erase(reg_iter); // cleanup map

             node_ptr->shutdown();
         }

     } else {
         //log_debug("LTPEngine unregister.. %" PRIu64 " not found." ,engine);
     } 
} 

//----------------------------------------------------------------------
bool
LTPEngine::register_engine(SPtr_LTPCLSenderIF& clsender)
{
    oasys::ScopeLock my_lock(&enginereg_map_lock_,"register_engine");  

    ENGINE_MAP::iterator node_iter = registered_engines_.find(clsender->Remote_Engine_ID());

    if (node_iter == registered_engines_.end()) {
        SPtr_LTPEngineReg regptr= std::make_shared<LTPEngineReg>(clsender);

        SPtr_LTPEngine ltp_engine_sptr = BundleDaemon::instance()->ltp_engine();
        SPtr_LTPNode node_ptr = std::make_shared<LTPNode>(ltp_engine_sptr, clsender);

        regptr->Set_LTPNode(node_ptr);
        registered_engines_[regptr->Remote_Engine_ID()] = regptr;
        node_ptr->start();
        return true;
    }

    return false;
} 


LTPEngine::~LTPEngine()
{
    shutdown();
}

//----------------------------------------------------------------------
void
LTPEngine::shutdown()
{
    if (data_processor_ != nullptr) {
        data_processor_->shutdown();
        data_processor_ = nullptr;
    }

    ENGINE_MAP::iterator reg_iter = registered_engines_.begin();

    while (reg_iter != registered_engines_.end()) {
        SPtr_LTPEngineReg regptr = reg_iter->second;
        SPtr_LTPNode node_ptr = regptr->LTP_Node();
        node_ptr->shutdown();

        registered_engines_.erase(reg_iter);

        reg_iter = registered_engines_.begin();
    }

}
    
//-------------------------------------------------------------3---------
void
LTPEngine::post_data(u_char * buffer, size_t length)
{
    data_processor_->post_data(buffer, length);
}

//-------------------------------------------------------------3---------
uint64_t
LTPEngine::new_session_id(uint64_t remote_engine_id)
{ 
    oasys::ScopeLock scoplok(&session_id_lock_, __func__);

    ++session_id_;
    if (session_id_ > ULONG_MAX) {
        session_id_ = LTP_14BIT_RAND;
    }

    session_id_to_engine_map_[session_id_] = remote_engine_id;

    return session_id_; 
}

//----------------------------------------------------------------------
void
LTPEngine::closed_session(uint64_t session_id, uint64_t engine_id, bool cancelled)
{
    oasys::ScopeLock scoplok(&session_id_lock_, __func__);
    
    struct ClosedSession rec;
    rec.engine_id_ = engine_id;
    rec.cancelled_ = cancelled;
    closed_session_id_to_engine_map_[session_id] = rec;

    session_id_to_engine_map_.erase(session_id);
}

//----------------------------------------------------------------------
void
LTPEngine::delete_closed_session(uint64_t session_id)
{
    oasys::ScopeLock scoplok(&session_id_lock_, __func__);
    
    closed_session_id_to_engine_map_.erase(session_id);
}

//----------------------------------------------------------------------
size_t
LTPEngine::get_closed_session_count()
{
    oasys::ScopeLock scoplok(&session_id_lock_, __func__);
    
    return closed_session_id_to_engine_map_.size();
}

//----------------------------------------------------------------------
SPtr_LTPEngineReg
LTPEngine::lookup_engine(uint64_t engine)
{
   SPtr_LTPEngineReg engine_entry;

   oasys::ScopeLock my_lock(&enginereg_map_lock_,"lookup_engine");  

   ENGINE_MAP::iterator engine_iter = registered_engines_.find(engine);

   if (engine_iter != registered_engines_.end()) {
       engine_entry = engine_iter->second;
   }
   return engine_entry;
}

//----------------------------------------------------------------------
SPtr_LTPEngineReg
LTPEngine::lookup_engine(uint64_t engine, uint64_t session, bool& closed, bool& cancelled)
{
    closed = false;
    cancelled = false;

    if (engine == local_engine_id_)
    {
        // If this is a local session then we need to determine
        // the remote engine to which it is being sent
        oasys::ScopeLock scoplok(&session_id_lock_, __func__);

        SESS_CTR_TO_ENGINE_MAP::iterator iter = session_id_to_engine_map_.find(session);
        if (iter == session_id_to_engine_map_.end()) {
            // session ID not found in list of current sessions
            // try to find it in the closed sessions list
            CLOSED_SESS_CTR_TO_ENGINE_MAP::iterator closed_iter = closed_session_id_to_engine_map_.find(session);
            if (closed_iter == closed_session_id_to_engine_map_.end()) {
                SPtr_LTPEngineReg empty_engptr;
                    return empty_engptr;
            } else {
                closed = true;
                cancelled = closed_iter->second.cancelled_;

                // change to lookup the correct remote engine id
                engine = closed_iter->second.engine_id_;
            }

        } else {
            // change to lookup the correct remote engine id
            engine = iter->second;
        }
    }

    return lookup_engine(engine);
}

//----------------------------------------------------------------------
void
LTPEngine::link_reconfigured(uint64_t engine)
{

     SPtr_LTPEngineReg engine_entry = lookup_engine(engine);

     if (engine_entry != nullptr) {
         engine_entry->LTP_Node()->reconfigured();
     }
} 

//----------------------------------------------------------------------
void
LTPEngine::dump_link_stats(uint64_t engine, oasys::StringBuffer* buf)
{

     SPtr_LTPEngineReg engine_entry = lookup_engine(engine);

     if (engine_entry == nullptr)
     {
         buf->appendf("LTPEngine::egine %ld\n not found ", engine);
     } else {
         engine_entry->LTP_Node()->dump_link_stats(buf);
     }
} 

//----------------------------------------------------------------------
void
LTPEngine::clear_stats(uint64_t engine)
{

     SPtr_LTPEngineReg engine_entry = lookup_engine(engine);

     if (engine_entry != nullptr)
     {
         engine_entry->LTP_Node()->clear_stats();
     }
} 

//----------------------------------------------------------------------
int64_t
LTPEngine::send_bundle(const BundleRef& bundle, uint64_t engine)
{
    bool result = false;

    SPtr_LTPEngineReg engine_entry = lookup_engine(engine);

    if (engine_entry != nullptr)
    {
        result = engine_entry->LTP_Node()->send_bundle(bundle);
    }

    return result;
}

//----------------------------------------------------------------------
LTPEngine::RecvDataProcessor::RecvDataProcessor()
    : Logger("LTPEngine::RecvDataProcessor",
             "/dtn/ltp/dataproc/%p", this),
      Thread("LTPEngine::RecvDataProcessor"),
      eventq_(logpath_)
{
}

//----------------------------------------------------------------------
LTPEngine::RecvDataProcessor::~RecvDataProcessor()
{
    shutdown();

    LTPDataReceivedEvent* event;

    while (eventq_.try_pop(&event)) {
        free(event->data_);
        delete event;
    }

    ltp_engine_sptr_ = nullptr;
}


//----------------------------------------------------------------------
void
LTPEngine::RecvDataProcessor::shutdown()
{
    set_should_stop();
    while (! is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
LTPEngine::RecvDataProcessor::post_data(u_char* data, size_t len)
{
    if (should_stop()) {
        free(data);
        return;
    }

    LTPDataReceivedEvent* event = new LTPDataReceivedEvent();
    event->data_  = (u_char *) malloc(len);
    if (event->data_ == nullptr) {
       log_err("Out of Memory for LTP::RecvDataProcessor...");
       delete event;
       return;
    }
    memcpy(event->data_, data, len);
    event->len_ = len;


    //dzdebug
    oasys::ScopeLock l(&eventq_lock_, __func__);

    eventq_.push_back(event);

    eventq_bytes_ += len;
    if (eventq_bytes_ > eventq_bytes_max_)
    {
        eventq_bytes_max_ = eventq_bytes_;

        //dzdebug
        if (eventq_bytes_max_ > eventq_max_next_size_to_log_) {
            eventq_max_next_size_to_log_+= 10000000;
            log_always("LTP.RecvDataProcessor - new max queued (%zu) allocation size = %zu", 
                       eventq_.size(), eventq_bytes_max_);
        }
    }
}

//----------------------------------------------------------------------
void
LTPEngine::RecvDataProcessor::run()
{
    char threadname[16] = "LTPRcvData";
    pthread_setname_np(pthread_self(), threadname);

    usleep(100000); // give the BD::SPtr_LTPEngine time to get set
    ltp_engine_sptr_ = BundleDaemon::instance()->ltp_engine();
    while (!should_stop() && (ltp_engine_sptr_ == nullptr)) {
        ltp_engine_sptr_ = BundleDaemon::instance()->ltp_engine();
    }
   
    LTPDataReceivedEvent* event;
    int ret;
    int seg_type = 0;
    uint64_t engine_id = 0;
    uint64_t session_id = 0;
    uint64_t last_unknown_engine_id = UINT64_MAX;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_.read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (1) {

        if (should_stop()) return; 

        ret = oasys::IO::poll_multiple(pollfds, 1, 10, nullptr);

        if (ret == oasys::IOINTR) {
            log_err("RecvDataProcessor interrupted - aborting");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("RecvDataProcessor IO Error - aborting");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {

            if (eventq_.try_pop(&event)) {
                ASSERT(event != nullptr)

                do {
                    oasys::ScopeLock l(&eventq_lock_, __func__);

                    eventq_bytes_ -= event->len_;
                } while (false);  // just limiting the scopelock


                SPtr_LTPEngineReg engptr;

                if (event->data_[0] & 0xf0) {
                    log_err("Unsupported LTP version: %u", (event->data_[0] & 0xf0)>>4);

                    free(event->data_);
                    delete event;
                    continue;
                }

                engine_id = 0;
                if (LTPSegment::Parse_Engine_and_Session(event->data_, event->len_,
                                                           engine_id, session_id, seg_type)) {

                    if (seg_type == -1) {
                        log_err("Received Invalid LTP Segment Type - first byte = %2.2x", event->data_[0]);
                        free(event->data_);
                        delete event;
                        continue;
                    }

                    engptr = ltp_engine_sptr_->lookup_engine(engine_id, session_id, event->closed_, event->cancelled_);
                }

                if (engptr != nullptr) {
                    // pass the received data on to the correct LTPNode
                    event->seg_type_ = seg_type;
                    engptr->LTP_Node()->post_data(event);
                } else {
                    if (engine_id == ltp_engine_sptr_->local_engine_id()) {
                        //log_err("Received LTP segments for local Engine ID with unknown Session ID");
                    } else if (last_unknown_engine_id != engine_id) {
                        log_err("Ignoring LTP segments from unknown Engine ID: %" PRIu64, engine_id);
                        last_unknown_engine_id = engine_id;
                    }

                    free(event->data_);
                    delete event;
                }
            }
        }
    }
}





//----------------------------------------------------------------------
LTPNode::LTPNode(SPtr_LTPEngine& ltp_engine_sptr, SPtr_LTPCLSenderIF& clsender)
     : Logger("LTPNode::", "/dtn//ltp/node/%p", this),
       Thread("LTPNode")
{
    ltp_engine_sptr_ = ltp_engine_sptr;
    clsender_ = clsender;

    sender_sptr_   = std::make_shared<Sender>(ltp_engine_sptr_, this, clsender_);
    sender_sptr_->start();

    receiver_sptr_ = std::make_shared<Receiver>(this, clsender_);
    receiver_sptr_->start();

    timer_processor_ = std::unique_ptr<TimerProcessor>(new TimerProcessor(clsender_->Remote_Engine_ID()));
}

//----------------------------------------------------------------------
LTPNode::~LTPNode()
{
    set_should_stop();
    shutdown();

    while (!is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
LTPNode::run()
{
    char threadname[16] = "LTPNode";
    pthread_setname_np(pthread_self(), threadname);

    int sleep_intervals = 0;

    aos_counter_ = 0;
    while(!should_stop())
    {
        usleep(100000);  //  0.10 seconds

        if (should_stop()) break;

        if (++sleep_intervals == 10) {
            sleep_intervals = 0;

            if (clsender_->AOS()) {
                aos_counter_++;
            }
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::shutdown()
{
    if (shutting_down_) {
        return;
    }

    shutting_down_ = true;

    sender_sptr_->shutdown();
    receiver_sptr_->shutdown();
    timer_processor_->set_should_stop();
}

//----------------------------------------------------------------------
void
LTPNode::reconfigured()
{
    sender_sptr_->reconfigured();
    receiver_sptr_->reconfigured();
}

//----------------------------------------------------------------------
SPtr_LTPNodeRcvrSndrIF
LTPNode::sender_rsif_sptr()
{
  SPtr_LTPNodeRcvrSndrIF rsif_sptr = sender_sptr_;
  return rsif_sptr;
}

//----------------------------------------------------------------------
SPtr_LTPNodeRcvrSndrIF
LTPNode::receiver_rsif_sptr()
{
  SPtr_LTPNodeRcvrSndrIF rsif_sptr = receiver_sptr_;
  return rsif_sptr;
}

//----------------------------------------------------------------------
void
LTPNode::post_data(LTPDataReceivedEvent* event)
{
    if (!shutting_down_) {
        receiver_sptr_->post_data(event);
    } else {
        free(event->data_);
        delete event;
    }
}

//----------------------------------------------------------------------
void 
LTPNode::Process_Sender_Segment(LTPSegment * seg, bool closed, bool cancelled)
{
    sender_sptr_->Process_Segment(seg, closed, cancelled);
}

//----------------------------------------------------------------------
int64_t
LTPNode::send_bundle(const BundleRef& bundle)
{
    return sender_sptr_->send_bundle(bundle);
}

//----------------------------------------------------------------------
uint64_t
LTPNode::AOS_Counter()
{
    return aos_counter_;
}

//----------------------------------------------------------------------
void
LTPNode::dump_link_stats(oasys::StringBuffer* buf)
{
    buf->appendf("\n");
    BundleDaemon::instance()->get_ltp_object_stats(buf);
    buf->appendf("\n");

    sender_sptr_->dump_sessions(buf);
    receiver_sptr_->dump_sessions(buf);

    buf->appendf("\nStatistics:\n");
    buf->appendf("                Sessions    Sessions        Data Segments           Report Segments      Rpt Acks    Bundles   \n");
    buf->appendf("             Total  /  Max   DS RXmt      Total    /  ReXmit        Total   /  ReXmit      Total     Succes    \n");
    buf->appendf("         -----------------  --------  -------------------------  ---------------------  ----------  ---------  \n");

    sender_sptr_->dump_success_stats(buf);
    receiver_sptr_->dump_success_stats(buf);


    buf->appendf("\n\n");
    buf->appendf("             Cancel By Sender       Cancel By Receiver     Cancl Acks  Cancelled   No Rpt Ack   Bundles     Bundles   \n");
    buf->appendf("             Total  /   ReXmit         Total / ReXmit      Rcvd Total   But Got     But Got    Expird inQ   Failure   \n");
    buf->appendf("         -----------------------  -----------------------  ----------  ----------  ----------  ----------  ---------- \n");

    sender_sptr_->dump_cancel_stats(buf);
    receiver_sptr_->dump_cancel_stats(buf);
}

//----------------------------------------------------------------------
void
LTPNode::clear_stats()
{
    receiver_sptr_->clear_statistics();
    sender_sptr_->clear_statistics();
}

//----------------------------------------------------------------------
void
LTPNode::Post_Timer_To_Process(oasys::SPtr_Timer& timer)
{
    if (! should_stop()) {
        if (timer_processor_) {
            timer_processor_->post(timer);
        }
    }
}

//----------------------------------------------------------------------
LTPNode::Receiver::Receiver(LTPNode* parent_node, SPtr_LTPCLSenderIF& clsender) :
      Logger("LTPNode::Receiver::Receiver",
             "/dtn/ltp/node/receiver/%p", this),
      Thread("LTPNode::Receiver"),
      clsender_(clsender),
      parent_node_(parent_node),
      eventq_(logpath_)
{
    memset(&stats_, 0, sizeof(stats_));

    seg_processor_ = QPtr_RecvSegProcessor(new RecvSegProcessor(this));
    seg_processor_->start();

    bundle_processor_ = QPtr_RecvBundleProcessor(new RecvBundleProcessor(this));
    bundle_processor_->start();

    blank_session_ = std::make_shared<LTPSession>(0UL, 0UL, false);
    blank_session_->Set_Inbound_Cipher_Suite(clsender_->Inbound_Cipher_Suite());
    blank_session_->Set_Inbound_Cipher_Key_Id(clsender_->Inbound_Cipher_Key_Id());
    blank_session_->Set_Inbound_Cipher_Engine(clsender_->Inbound_Cipher_Engine());
    blank_session_->Set_Outbound_Cipher_Suite(clsender_->Outbound_Cipher_Suite());
    blank_session_->Set_Outbound_Cipher_Key_Id(clsender_->Outbound_Cipher_Key_Id());
    blank_session_->Set_Outbound_Cipher_Engine(clsender_->Outbound_Cipher_Engine());

    reconfigured();
}

//----------------------------------------------------------------------
LTPNode::Receiver::~Receiver()
{
    shutdown();

    /* ---- cleanup sessions -----*/
    incoming_sessions_.clear();
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::shutdown()
{
    if (shutting_down_) {
        return;
    }

    // these flags prevent procesing of any new incoming data 
    shutting_down_ = true;
    set_should_stop();

    // do not accept/process any new segments
    seg_processor_->shutdown();
    while (!seg_processor_->is_stopped()) {
        usleep(100000);
    }

    // wait for the bundle processing thread to complete queued up data
    bundle_processor_->shutdown();
    while (!bundle_processor_->is_stopped()) {
        usleep(100000);
    }

    while (! is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::reconfigured()
{
    seg_size_            = clsender_->Seg_Size();
    retran_retries_      = clsender_->Retran_Retries();
    retran_interval_     = clsender_->Retran_Intvl();
    inactivity_interval_ = clsender_->Inactivity_Intvl();

    //seg_processor_->reconfigured();
    bundle_processor_->reconfigured();
}


//----------------------------------------------------------------------
void
LTPNode::Receiver::post_data(LTPDataReceivedEvent* event)
{
    if (should_stop()) {
        free(event->data_);
        delete event;
        return;
    }


    switch (event->seg_type_) {
        case LTP_SEGMENT_DS:
            seg_processor_->post_ds(event);
            break;

        case LTP_SEGMENT_RAS:
        case LTP_SEGMENT_CS_BS:
        case LTP_SEGMENT_CAS_BR:
            seg_processor_->post_admin(event);
            break;

        default:
            // all other segments are in response to the Sender 
            // which are processed here and passed over to it

            oasys::ScopeLock l(&eventq_lock_, __func__);

            eventq_.push_back(event);

            eventq_bytes_ += event->len_;
            if (eventq_bytes_ > eventq_bytes_max_)
            {
                eventq_bytes_max_ = eventq_bytes_;

                //dzdebug
                if (eventq_bytes_max_ > eventq_max_next_size_to_log_) {
                    eventq_max_next_size_to_log_+= 10000000;
                    log_always("LTPNode.Receiver - new max queued (%zu) allocation size = %zu", 
                               eventq_.size(), eventq_bytes_max_);
                }
            }
            break;
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::run()
{
    char threadname[16] = "LTPNodeRcvr";
    pthread_setname_np(pthread_self(), threadname);
   
    LTPDataReceivedEvent* event;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_.read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (1) {

        if (should_stop()) return; 

        ret = oasys::IO::poll_multiple(pollfds, 1, 100, nullptr);

        if (ret == oasys::IOINTR) {
            log_err("RecvDataProcessor interrupted - aborting");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("RecvDataProcessor IO Error - aborting");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {

            if (eventq_.try_pop(&event)) {
                ASSERT(event != nullptr)

                // Kind of strange but the Receiver actually processes the segments
                // arriving in response to outgoing bundles/sessions.
                // Segments related to incoming sessions are offloaded to the RecvSegProcessor thread.
                process_data_for_sender(event->data_, event->len_, event->closed_, event->cancelled_);

                oasys::ScopeLock l(&eventq_lock_, __func__);

                eventq_bytes_ -= event->len_;

                free(event->data_);
                delete event;
            }
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RSIF_post_timer_to_process(oasys::SPtr_Timer& event)
{
    parent_node_->Post_Timer_To_Process(event);
}

//----------------------------------------------------------------------
uint64_t
LTPNode::Receiver::RSIF_aos_counter()
{
    return parent_node_->AOS_Counter();
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RSIF_retransmit_timer_triggered(int event_type, SPtr_LTPSegment& retran_seg)
{
    if (event_type == LTP_EVENT_RS_TO) {
        report_seg_timeout(retran_seg);
    } else if (event_type == LTP_EVENT_CS_TO) {
        cancel_seg_timeout(retran_seg);
    } else {
        log_err("Unexpected RetransmitSegTimer triggerd: %s", LTPSegment::Get_Event_Type(event_type));
    }
}


//----------------------------------------------------------------------
void
LTPNode::Receiver::RSIF_inactivity_timer_triggered(uint64_t engine_id, uint64_t session_id)
{
    SPtr_LTPSession session_sptr = find_incoming_session(engine_id, session_id);

    if (session_sptr != nullptr) {
        if (session_sptr->Session_State() == LTPSession::LTP_SESSION_STATE_DS) {
            time_t time_left = session_sptr->Last_Packet_Time() +  inactivity_interval_;
            time_left = time_left - parent_node_->AOS_Counter();

            if (time_left > 0) /* timeout occured but retries are not exhausted */
            {
                SPtr_LTPNodeRcvrSndrIF rcvr_rsif = parent_node_->receiver_rsif_sptr();
                session_sptr->Start_Inactivity_Timer(rcvr_rsif, time_left); // catch the remaining seconds...
            } else {
                session_sptr->RemoveSegments();   
                build_CS_segment(session_sptr, LTP_SEGMENT_CS_BR, 
                                 LTPCancelSegment::LTP_CS_REASON_RXMTCYCEX);
            }
        } else {
            //log_debug("%s): %s - Inactivity Timeout ignored - session no longer in DS state - state = %s red processed = %s",
            //           __func__, session_sptr->key_str().c_str(), session_sptr->Get_Session_State(),
            //           session_sptr->RedProcessed()?"true":"false");
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RSIF_closeout_timer_triggered(uint64_t engine_id, uint64_t session_id)
{
    //(void) engine_id;
    ASSERT(engine_id == clsender_->Remote_Engine_ID());

    // remove from closed sessions
    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    closed_session_map_.erase(session_id);
}


//----------------------------------------------------------------------
void
LTPNode::Receiver::report_seg_timeout(SPtr_LTPSegment& seg)
{ 
    if (!seg->IsDeleted()) {
        // Okay to resend segment if retries not exceeded

        SPtr_LTPSession session_sptr = find_incoming_session(seg->Engine_ID(), seg->Session_ID());

        if (session_sptr != nullptr) {
            // for logging
            std::string key_str = session_sptr->key_str();

            SPtr_LTPReportSegment rs_seg = std::dynamic_pointer_cast<LTPReportSegment>(seg);

            if (rs_seg != nullptr) {

                rs_seg->Increment_Retries();

                if (rs_seg->Retrans_Retries() <= retran_retries_) {
                    //dzdebug
                    //log_always("RS timeout: Session: %s - Resending Report Segment",
                    //          key_str.c_str());

                    SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();

                    SPtr_LTPTimer timer;
                    timer = rs_seg->Create_Retransmission_Timer(rcvr_if, LTP_EVENT_RS_TO, retran_interval_, seg);

                    ++stats_.rs_segment_resends_;

                    clsender_->Send(rs_seg->asNewString(), timer, false);
                } else {
                    //dzdebug
                    //log_debug("RS timeout: Session: %s - Resend Report Segment exceeds max - giving up",
                    //          key_str.c_str());

                    rs_seg->Set_Deleted();

                    if (session_sptr->DataProcessed()) {
                        // No RAS received but we did extract the bundles so we can just delete the session
                        // - peer will either cancel on their side or they sent the RAS and it did not make it here
                        if (session_sptr->RedProcessed()) {
                            ++stats_.RAS_not_received_but_got_bundles_;
                        }
                        erase_incoming_session(session_sptr);
                    } else {
                        // Exceeded retries - start cancelling
                        build_CS_segment(session_sptr, LTP_SEGMENT_CS_BR, 
                                         LTPCancelSegment::LTP_CS_REASON_RLEXC);
                    }
                }
            } else {
                log_err("RS timeout - error casting seg to rs_seg!");
            }
        } else {
            //log_always("RS timeout - ignoring - session not found");
        }
    } else {
        //log_always("RS timeout - Session: %s - ignoring since flagged as deleted", 
        //           seg->session_key_str().c_str());
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::cancel_seg_timeout(SPtr_LTPSegment& seg)
{ 
    if (!seg->IsDeleted()) {
        // Okay to resend segment if retries not exceeded

        SPtr_LTPSession session_sptr = find_incoming_session(seg->Engine_ID(), seg->Session_ID());

        if (session_sptr != nullptr ) {
            // for logging
            std::string key_str = session_sptr->key_str();

            SPtr_LTPCancelSegment cs_seg = std::dynamic_pointer_cast<LTPCancelSegment>(seg);

            if (cs_seg != nullptr) {

                cs_seg->Increment_Retries();

                if (cs_seg->Retrans_Retries() <= retran_retries_) {
                    //dzdebug
                    //log_always("CS timeout: Session: %s - Resending Cancel By Receiver Segment",
                    //          key_str.c_str());

                    SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();

                    SPtr_LTPTimer timer;
                    timer = cs_seg->Create_Retransmission_Timer(rcvr_if, LTP_EVENT_CS_TO, retran_interval_, seg);

                    ++stats_.csr_segment_resends_;

                    clsender_->Send(cs_seg->asNewString(), timer, false);
                } else {
                    //dzdebug
                    //log_debug("CS timeout: Session: %s - Resend Cancel By Receiver Segment exceeds max - giving up",
                    //          key_str.c_str());

                    cs_seg->Set_Deleted();

                    erase_incoming_session(session_sptr);
                }
            } else {
                log_always("CS timeout - error casting seg to cs_seg!");
            }
        } else {
            //log_always("CS timeout - ignoring - session not found");
        }
    } else {
        //log_always("CS timeout - Session: %s - ignoring since flagged as deleted", seg->session_key_str().c_str());
    }
}




//----------------------------------------------------------------------
void
LTPNode::Receiver::dump_sessions(oasys::StringBuffer* buf)
{
    if (shutting_down_) {
        return;
    }

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    buf->appendf("\nReceiver Sessions: Total: %zu  (States- DS: %zu  RS: %zu  CS: %zu - Closing: %zu)\n",
                 incoming_sessions_.size(), sessions_state_ds_, sessions_state_rs_, sessions_state_cs_,
                 closed_session_map_.size());

    uint64_t seg_queue_size = 0;
    uint64_t seg_bytes_queued = 0;
    uint64_t seg_bytes_queued_max = 0;

    uint64_t bundle_queue_size = 0;
    uint64_t bundle_bytes_queued = 0;
    uint64_t bundle_bytes_queued_max = 0;
    uint64_t bundle_quota = 0;

    seg_processor_->get_queue_stats(seg_queue_size, seg_bytes_queued, seg_bytes_queued_max);
    bundle_processor_->get_queue_stats(bundle_queue_size, bundle_bytes_queued, bundle_bytes_queued_max, bundle_quota);

    buf->appendf("Receiver threads: SegProcessor: queued: %zu  bytes: %zu  max bytes: %zu\n"
                 "               BundleProcessor: queued: %zu  bytes: %zu  max bytes: %zu  quota: %zu\n",
                 seg_queue_size, seg_bytes_queued, seg_bytes_queued_max,
                 bundle_queue_size, bundle_bytes_queued, bundle_bytes_queued_max,
                 bundle_quota);


}

//----------------------------------------------------------------------
void
LTPNode::Receiver::dump_success_stats(oasys::StringBuffer* buf)
{
    if (shutting_down_) {
        return;
    }

               //                Sessions    Sessions        Data Segments           Report Segments      Rpt Acks    Bundles   
               //             Total  /  Max   DS RXmt      Total    /  ReXmit        Total   /  ReXmit      Total     Success
               //         -----------------  --------  -------------------------  ---------------------  ----------  ----------
               //Sender   1234567890 / 1234  12345678  123456789012 / 1234567890  1234567890 / 12345678  1234567890  1234567890
               //Receiver 1234567890 / 1234  12345678  123456789012 / 1234567890  1234567890 / 12345678  1234567890  1234567890

    buf->appendf("Receiver %10" PRIu64 " / %4" PRIu64 "  %8" PRIu64 "  %12" PRIu64 " / %10" PRIu64 "  %10" PRIu64 " / %8" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "\n", 
                 stats_.total_sessions_, stats_.max_sessions_,
                 stats_.ds_sessions_with_resends_, stats_.total_rcv_ds_, stats_.ds_segment_resends_,
                 stats_.total_rs_segs_generated_, stats_.rs_segment_resends_, stats_.total_rcv_ra_,
                 stats_.bundles_success_);
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::dump_cancel_stats(oasys::StringBuffer* buf)
{
    if (shutting_down_) {
        return;
    }

               //             Cancel By Sender       Cancel By Receiver     Cancl Acks  Cancelled   No Rpt Ack   Bundles     Bundles   
               //             Total  /   ReXmit         Total / ReXmit      Rcvd Total   But Got     But Got    Expird inQ   Failure
               //         -----------------------  -----------------------  ----------  ----------  ----------  ----------  ----------
               //Sender   1234567890 / 1234567890  1234567890 / 1234567890  1234567890  1234567890  1234567890  1234567890  1234567890
               //Receiver 1234567890 / 1234567890  1234567890 / 1234567890  1234567890  1234567890  1234567890  1234567890  1234567890
    buf->appendf("Receiver %10" PRIu64 " / %10" PRIu64 "  %10" PRIu64 " / %10" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "\n",
                 stats_.total_rcv_css_, stats_.rcv_css_resends_, 
                 stats_.total_snt_csr_, stats_.csr_segment_resends_, stats_.total_sent_and_rcvd_ca_, 
                 stats_.session_cancelled_but_got_it_, stats_.RAS_not_received_but_got_bundles_,
                 stats_.bundles_expired_in_queue_, stats_.bundles_failed_ );
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::clear_statistics()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void 
LTPNode::Receiver::update_session_counts(SPtr_LTPSession& session, int new_state)
{
    int old_state = session->Session_State();

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    if (old_state != new_state) {
        switch (old_state)
        {
            case LTPSession::LTP_SESSION_STATE_RS: 
                --sessions_state_rs_;
                break;
            case LTPSession::LTP_SESSION_STATE_CS:
                --sessions_state_cs_;
                break;
            case LTPSession::LTP_SESSION_STATE_DS:
                --sessions_state_ds_;
                break;
            default:
              ;
        }

        switch (new_state)
        {
            case LTPSession::LTP_SESSION_STATE_RS: 
                ++sessions_state_rs_;
                break;
            case LTPSession::LTP_SESSION_STATE_CS:
                ++sessions_state_cs_;
                break;
            case LTPSession::LTP_SESSION_STATE_DS:
                ++sessions_state_ds_;
                break;
            default:
              ;
        }

        session->Set_Session_State(new_state); 
    }
}

//----------------------------------------------------------------------
void 
LTPNode::Receiver::build_CS_segment(SPtr_LTPSession& session_sptr, int segment_type, u_char reason_code)
{
    SPtr_LTPCancelSegment cancel_segment = std::make_shared<LTPCancelSegment>(session_sptr.get(), segment_type, reason_code);

    update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_CS); 

    SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();

    SPtr_LTPSegment seg = cancel_segment;
    SPtr_LTPTimer timer;
    timer = cancel_segment->Create_Retransmission_Timer(rcvr_if, LTP_EVENT_CS_TO, retran_interval_, seg);

    session_sptr->Set_Cancel_Segment(cancel_segment);

    if (session_sptr->RedProcessed()) {
      ++stats_.session_cancelled_but_got_it_;
    }
    ++stats_.total_snt_csr_;

    clsender_->Send(cancel_segment->asNewString(), timer, false);
    
}

//----------------------------------------------------------------------
void 
LTPNode::Receiver::build_CAS_for_CSS(LTPCancelSegment* seg)
{
    LTPCancelAckSegment* cas_seg = new LTPCancelAckSegment(seg, LTPSEGMENT_CAS_BS_BYTE, blank_session_.get()); 

    SPtr_LTPTimer dummy_timer;   // no retransmits
    clsender_->Send(cas_seg->asNewString(), dummy_timer, false);
    delete cas_seg;

    ++stats_.total_sent_and_rcvd_ca_;
}

//----------------------------------------------------------------------
SPtr_LTPSession
LTPNode::Receiver::find_incoming_session(LTPSegment* seg, bool create_flag)
{
    SPtr_LTPSession session_sptr;

    session_sptr = find_incoming_session(seg->Engine_ID(), seg->Session_ID());

    if (session_sptr == nullptr) {
        if (create_flag) {
            session_sptr = std::make_shared<LTPSession>(seg->Engine_ID(), seg->Session_ID());

            session_sptr->Set_Inbound_Cipher_Suite(clsender_->Inbound_Cipher_Suite());
            session_sptr->Set_Inbound_Cipher_Key_Id(clsender_->Inbound_Cipher_Key_Id());
            session_sptr->Set_Inbound_Cipher_Engine(clsender_->Inbound_Cipher_Engine());
            session_sptr->Set_Outbound_Cipher_Suite(clsender_->Outbound_Cipher_Suite());
            session_sptr->Set_Outbound_Cipher_Key_Id(clsender_->Outbound_Cipher_Key_Id());
            session_sptr->Set_Outbound_Cipher_Engine(clsender_->Outbound_Cipher_Engine());

            oasys::ScopeLock scoplok(&session_list_lock_, __func__);

            incoming_sessions_[session_sptr.get()] = session_sptr;

            if (incoming_sessions_.size() > stats_.max_sessions_) {
                stats_.max_sessions_ = incoming_sessions_.size();
            }
            ++stats_.total_sessions_;
        }
    }

    return session_sptr;
}

//----------------------------------------------------------------------
SPtr_LTPSession
LTPNode::Receiver::find_incoming_session(uint64_t engine_id, uint64_t session_id)
{
    SPtr_LTPSession session_sptr;

    QPtr_LTPSessionKey qkey = QPtr_LTPSessionKey(new LTPSessionKey(engine_id, session_id));

    SESS_SPTR_MAP::iterator iter; 

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    iter = incoming_sessions_.find(qkey.get());
    if (iter != incoming_sessions_.end())
    {
        session_sptr = iter->second;
    }

    return session_sptr;
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::erase_incoming_session(SPtr_LTPSession& session_sptr)
{

    SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();
    session_sptr->Start_Closeout_Timer(rcvr_if, inactivity_interval_);

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    incoming_sessions_.erase(session_sptr.get());

    update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_UNDEFINED); 

    closed_session_map_[session_sptr->Session_ID()] = session_sptr->Session_ID();
    if (closed_session_map_.size() > max_closed_sessions_) {
        max_closed_sessions_ = closed_session_map_.size();
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::cleanup_RS(uint64_t engine_id, uint64_t session_id, uint64_t report_serial_number, 
                                             int event_type)
{ 
    //log_always("dzdebug - cleanup_RS triggered by event (%s) for session_id: %zu", 
    //           LTPSegment::Get_Segment_Type(event_type), session_id);

    SPtr_LTPSession session_sptr = find_incoming_session(engine_id, session_id);

    if (session_sptr != nullptr) {

        // for logging
        std::string key_str = session_sptr->key_str();

        LTP_RS_MAP::iterator rs_iterator = session_sptr->Report_Segments().find(report_serial_number);

        if (rs_iterator != session_sptr->Report_Segments().end())
        {
            SPtr_LTPReportSegment rptseg = rs_iterator->second;

            rptseg->Cancel_Retransmission_Timer();

            if (event_type != LTP_EVENT_RAS_RECEIVED) {
                log_err("cleanup_RS: Session: %s - Unexpected event type; %d",
                           key_str.c_str(), event_type);
                return;
            }

            // else if RAS received then delete the report segment
            // and flag it as deleted in case a timer has a reference to it
            rs_iterator->second->Set_Deleted();

            session_sptr->Report_Segments().erase(rs_iterator);

        } else {
            //log_debug("cleanup_RS: Session: %zu - Report Segment not found: %zu",
            //          session_id, report_serial_number);
        }
    } else {
        //log_debug("cleanup_RS: Session: %zu - not found", session_id);
    }
}
                         

//----------------------------------------------------------------------
void
LTPNode::Receiver::generate_RS(SPtr_LTPSession& session_sptr, LTPDataSegment* dataseg)
{
    int32_t segsize = seg_size_;
    int64_t bytes_claimed = 0;

    if (session_sptr->HaveReportSegmentsToSend(dataseg, segsize, bytes_claimed))
    {
        uint64_t chkpt = dataseg->Checkpoint_ID();

        SPtr_LTPReportSegment rptseg;

        LTP_RS_MAP::iterator iter;

        for (iter =  session_sptr->Report_Segments().begin();
            iter != session_sptr->Report_Segments().end();
            iter++)
        {
            rptseg = iter->second;

            if (chkpt == rptseg->Checkpoint_ID())
            {
                update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_RS); 

                // send or resend this report segment
                SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();

                SPtr_LTPSegment seg = rptseg;

                SPtr_LTPTimer timer;
                timer = rptseg->Create_Retransmission_Timer(rcvr_if, LTP_EVENT_RS_TO, retran_interval_, seg);

                ++stats_.total_rs_segs_generated_;
                clsender_->Send(rptseg->asNewString(), timer, false);
                session_sptr->inc_reports_sent();
            }
            else
            {
                //log_always("generateRS - Session: %s skip ReportSeg for Checkpoint ID: %lu", 
                //           session_sptr->key_str().c_str(), rptseg->Checkpoint_ID());
            }
        }
    }
    else
    {
        //log_always("generateRS - Session: %s no report segments to send",
        //           session_sptr->key_str().c_str());
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::process_data_for_sender(u_char* bp, size_t len, bool closed, bool cancelled)
{
    if (shutting_down_) {
        return;
    }

    if (clsender_->Hex_Dump()) {
        oasys::HexDumpBuffer hex;
        int plen = len > 30 ? 30 : len;
        hex.append(bp, plen);
        log_always("process_data: Buffer (%d of %zu):", plen, len);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }

    LTPSegment * seg = LTPSegment::DecodeBuffer(bp, len);

    if (!seg->IsValid()){
        log_err("process_data: invalid segment.... type:%s length:%d", 
                 LTPSegment::Get_Segment_Type(seg->Segment_Type()),(int) len);
        delete seg;
        return;
    }

    //log_debug("process_data: Session: %" PRIu64 "-%" PRIu64 " - %s received",
    //          seg->Engine_ID(), seg->Session_ID(), LTPSegment::Get_Segment_Type(seg->Segment_Type()));

    //
    // Send over to Sender if it's any of these three SEGMENT TYPES
    //
    if (seg->Segment_Type() == LTP_SEGMENT_RS     || 
        seg->Segment_Type() == LTP_SEGMENT_CS_BR  ||
        seg->Segment_Type() == LTP_SEGMENT_CAS_BS) { 

        parent_node_->Process_Sender_Segment(seg, closed, cancelled);

    } else {
        log_err("process_data_for_sender: received unexpected segment type: %s", seg->Get_Segment_Type());
    }

    delete seg; // we are done with this segment
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::process_data_for_receiver(LTPDataReceivedEvent* event)
{
    if (shutting_down_) {
        return;
    }

    u_char* bp = event->data_;
    size_t len = event->len_;

    if (clsender_->Hex_Dump()) {
        oasys::HexDumpBuffer hex;
        int plen = len > 30 ? 30 : len;
        hex.append(bp, plen);
        log_always("process_data: Buffer (%d of %zu):", plen, len);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }

    LTPSegment * seg = LTPSegment::DecodeBuffer(bp, len);

    if (!seg->IsValid()){
        log_err("process_data: invalid segment.... type:%s length:%d", 
                 LTPSegment::Get_Segment_Type(seg->Segment_Type()),(int) len);
        delete seg;
        return;
    }

    //log_debug("process_data: Session: %" PRIu64 "-%" PRIu64 " - %s received",
    //          seg->Engine_ID(), seg->Session_ID(), LTPSegment::Get_Segment_Type(seg->Segment_Type()));

    //
    // Should not receive any segment types meant for the Sender side
    //
    if (seg->Segment_Type() == LTP_SEGMENT_RS     || 
        seg->Segment_Type() == LTP_SEGMENT_CS_BR  ||
        seg->Segment_Type() == LTP_SEGMENT_CAS_BS) { 

        log_err("process_data_for_receiver: received unexpected segment type: %s  event seg type=%d", seg->Get_Segment_Type(), event->seg_type_);

        delete seg; // we are done with this segment
        return;
    }



    oasys::ScopeLock scoplok(&session_list_lock_, __func__);
    CLOSED_SESSION_MAP::iterator closed_iter = closed_session_map_.find(seg->Session_ID());
    bool closed = (closed_iter != closed_session_map_.end());
    scoplok.unlock();


    if (closed) {
        if (seg->Segment_Type() == LTP_SEGMENT_CS_BS) {
            build_CAS_for_CSS((LTPCancelSegment*) seg);
        }
        // else
        // LTP_SEGMENT_CAS_BR - no reply needed
        // LTP_SEGMENT_DS - ignore since already exceeded RS retransmits and possibly CSS retransmits

        delete seg;
        return;
    }


    bool create_flag = (seg->Segment_Type() == LTP_SEGMENT_DS);

    // get current session from rcvr_sessions...
    SPtr_LTPSession session_sptr = find_incoming_session(seg, create_flag);  

    if (session_sptr == nullptr) {
        // probably started up while peer was sending cancel by sedner segments
        if (seg->Segment_Type() == LTP_SEGMENT_CS_BS) {
            build_CAS_for_CSS((LTPCancelSegment*) seg);
        }

        delete seg;
        return;
    }

    if (session_sptr->Is_LTP_Cancelled()) {
        if (seg->Segment_Type() == LTP_SEGMENT_CS_BS) {
            ++stats_.rcv_css_resends_;
            build_CAS_for_CSS((LTPCancelSegment*) seg);
        } else if (seg->Segment_Type() != LTP_SEGMENT_CAS_BR) {
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
    
    if (seg->Segment_Type() == LTP_SEGMENT_DS) {
        process_incoming_data_segment(session_sptr, seg);

    } else if (seg->Segment_Type() == LTP_SEGMENT_RAS)  { 
         // Report Ack Segment
         ++stats_.total_rcv_ra_;

        LTPReportAckSegment* ras_seg = (LTPReportAckSegment *) seg;
        // cleanup the Report Segment list so we don't keep transmitting.
        cleanup_RS(seg->Engine_ID(), seg->Session_ID(), ras_seg->Report_Serial_Number(), LTP_EVENT_RAS_RECEIVED); 
        delete seg;

        if (session_sptr->DataProcessed()) {
            erase_incoming_session(session_sptr);
        } else {
            // got the RAS so now waiting for the missing DSs
            update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_DS); 
            session_sptr->Set_Last_Packet_Time(parent_node_->AOS_Counter());
            if (!session_sptr->Has_Inactivity_Timer()) {
                SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();
                session_sptr->Start_Inactivity_Timer(rcvr_if, inactivity_interval_);
            }
        }

    } else if (seg->Segment_Type() == LTP_SEGMENT_CS_BS) { 
        // Cancel Segment from Block Sender
        ++stats_.total_rcv_css_;

        update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_CS); 

        build_CAS_for_CSS((LTPCancelSegment*) seg);
        delete seg;

        // clean up the session
        session_sptr->RemoveSegments();
        erase_incoming_session(session_sptr);

    } else if (seg->Segment_Type() == LTP_SEGMENT_CAS_BR) { 
        // Cancel-acknowledgment Segment from Block Receiver
        ++stats_.total_sent_and_rcvd_ca_;

        session_sptr->Clear_Cancel_Segment();
        erase_incoming_session(session_sptr);
        delete seg;
    } else {
        log_err("process_data - Unhandled segment type received: %d", seg->Segment_Type());
        delete seg;
    }
}


//----------------------------------------------------------------------
void
LTPNode::Receiver::process_incoming_data_segment(SPtr_LTPSession& session_sptr, LTPSegment* seg)
{
    ++stats_.total_rcv_ds_;

    if (seg->Session_ID() > ULONG_MAX)
    {
        log_err("%s: received %zu:%zu DS with invalid Session ID - exceeds ULONG_MAX", 
                  __func__, session_sptr->Engine_ID(), session_sptr->Session_ID());

        build_CS_segment(session_sptr,LTP_SEGMENT_CS_BR, 
                         LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
        delete seg;
        return;
    }

    if (seg->Service_ID() != 1 && seg->Service_ID() != 2)
    {
        if (seg->IsRed()) {
            log_err("%s: calling build_CS_Segment received %zu:%zu DS with invalid Service ID: %d",
                      __func__, session_sptr->Engine_ID(), session_sptr->Session_ID(), seg->Service_ID());

            // cancel this session
            build_CS_segment(session_sptr, LTP_SEGMENT_CS_BR, 
                             LTPCancelSegment::LTP_CS_REASON_UNREACHABLE);
        } else {
            log_err("%s: received %zu:%zu Green DS with invalid Service ID: %d", 
                    __func__, session_sptr->Engine_ID(), session_sptr->Session_ID(), seg->Service_ID());
            erase_incoming_session(session_sptr);
        }

        delete seg;
        return; 
    }


    update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_DS); 

    session_sptr->Set_Last_Packet_Time(parent_node_->AOS_Counter());

    if (!session_sptr->Has_Inactivity_Timer()) {
        SPtr_LTPNodeRcvrSndrIF rcvr_rsif = parent_node_->receiver_rsif_sptr();
        session_sptr->Start_Inactivity_Timer(rcvr_rsif, inactivity_interval_);
    }

    if (session_sptr->reports_sent() > 0) {
        // RS has already been sent so this is a resend of the DS
        // XXX?dz what about optional intermediate check points? 
        ++stats_.ds_segment_resends_;
    }

    if (session_sptr->DataProcessed() || (-1 == session_sptr->AddSegment((LTPDataSegment*)seg))) {
                                         // not an "error" - just duplicate data
        if (seg->IsCheckpoint())
        {
            session_sptr->Set_Checkpoint_ID(seg->Checkpoint_ID());

            //dzdebug
            //log_always("LTPNode::Receiver: Session: %s got DS Checkpoint(%zu) -DataProcessed: %s - generate RS",
            //           session_sptr->key_str().c_str(), seg->Checkpoint_ID(), session_sptr->DataProcessed()?"true":"false");

            generate_RS(session_sptr, static_cast<LTPDataSegment*>(seg));
        }

        delete seg;
        return;
    }




    if (seg->IsEndofblock()) {
        session_sptr->Set_EOB((LTPDataSegment*) seg);
    }

    size_t red_bytes_to_process = session_sptr->IsRedFull();
    bool ok_to_accept_bundle = true;

    if (red_bytes_to_process > 0) {
        if (!bundle_processor_->okay_to_queue()) {
            ok_to_accept_bundle = false;
        } else {
            //check payload quota to see if we can accept the bundle(s)
            BundleStore* bs = BundleStore::instance();
            if (!bs->try_reserve_payload_space(red_bytes_to_process)) {
                ok_to_accept_bundle = false;
                // rejecting bundle due to storage depletion
                log_warn("LTPNode::Receiver: rejecting bundle due to storage depletion - session: %zu:%zu bytes: %zu",
                         session_sptr->Engine_ID(), session_sptr->Session_ID(), red_bytes_to_process);
            }
            // else space is reserved and must be managed in the bundle extraction process
        }
    }

    if (!ok_to_accept_bundle)
    {
        // Cancel Session due to storage depletion
        build_CS_segment(session_sptr, LTP_SEGMENT_CS_BR,
                         LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
        return;
    }
    else if (seg->IsCheckpoint())
    {
        session_sptr->Set_Checkpoint_ID(seg->Checkpoint_ID());


            //dzdebug
            //log_always("LTPNode::Receiver: Session: %s got DS Checkpoint(%zu) -DataProcessed: %s - generate RS",
            //           session_sptr->key_str().c_str(), seg->Checkpoint_ID(), session_sptr->DataProcessed()?"true":"false");

        generate_RS(session_sptr, static_cast<LTPDataSegment*>(seg));
    }


    if (red_bytes_to_process > 0) {
        u_char* data = session_sptr->GetAllRedData();
        int sid = seg->Service_ID();
        bundle_processor_->post(data, red_bytes_to_process, ((sid == 2) ? true : false) );
    } else if (seg->IsCheckpoint()) {
        ++stats_.ds_sessions_with_resends_;   // always true??
    }


    size_t green_bytes_to_process = session_sptr->IsGreenFull();
    if (green_bytes_to_process > 0) {
        //check payload quota to see if we can accept the bundle(s)
        BundleStore* bs = BundleStore::instance();
        if (bs->try_reserve_payload_space(green_bytes_to_process)) {
            u_char* data = session_sptr->GetAllGreenData();
            int sid = seg->Service_ID();
            bundle_processor_->post(data, green_bytes_to_process, ((sid == 2) ? true : false) );

        } else {
            // rejecting bundle due to storage depletion
            log_warn("LTPNode::Receiver: rejecting green bundle due to storage depletion - session: %zu:%zu bytes: %zu",
                     session_sptr->Engine_ID(), session_sptr->Session_ID(), green_bytes_to_process);
        }

        // CCSDS BP Specification - LTP Sessions must be all red or all green so we can now delete this session
        erase_incoming_session(session_sptr);
    }

}





//----------------------------------------------------------------------
LTPNode::Receiver::RecvBundleProcessor::RecvBundleProcessor(Receiver* parent_rcvr)
    : Logger("LTPNode::Receiver::RecvBundleProcessor",
             "/dtn/ltp/rcvr/bundleproc/%p", this),
      Thread("LTPNode::Receiver::RecvBundleProcessor"),
      eventq_(logpath_)
{
    parent_rcvr_ = parent_rcvr;
}

//----------------------------------------------------------------------
LTPNode::Receiver::RecvBundleProcessor::~RecvBundleProcessor()
{
    shutdown();

    if (eventq_.size() > 0) {
        log_always("RcvBundleProcessor::destructor - Warning eventq_ contains %zu events",
                   eventq_.size());

        EventObj* event;
        while (eventq_.try_pop(&event)) {
            free(event->data_);
            delete event;
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::shutdown()
{
    // this flag prevents procesing of any new incoming data 
    shutting_down_ = true;

    int ctr = 0;
    while ((eventq_.size() > 0) && !should_stop()) {
        usleep(100000);
        if (++ctr >= 10) {
            ctr = 0;
            log_always("RcvBundleProcessor delaying shutdown until all data processed - eventq size: %zu", 
                       eventq_.size());
        }
    }

    set_should_stop();
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::reconfigured()
{
    queued_bytes_quota_  = parent_rcvr_->ltp_queued_bytes_quota();
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::get_queue_stats(size_t& queue_size, 
                              uint64_t& bytes_queued, uint64_t& bytes_queued_max,
                              uint64_t& bytes_quota)
{
    oasys::ScopeLock scoplok(&lock_, __func__);
    queue_size = eventq_.size();
    bytes_queued = eventq_bytes_;
    bytes_queued_max = eventq_bytes_max_;
    bytes_quota = queued_bytes_quota_;
}

//----------------------------------------------------------------------
bool
LTPNode::Receiver::RecvBundleProcessor::okay_to_queue()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    return (eventq_bytes_ < queued_bytes_quota_);
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::post(u_char* data, size_t len, bool multi_bundle)
{
    // Receiver goes into shutdown mode before the RecvBundleProcessor so
    // continue to accept new posts from it even if our shutting_down_ flag is set

    EventObj* event = new EventObj();
    event->data_ = data;
    event->len_ = len;
    event->multi_bundle_ = multi_bundle;

    oasys::ScopeLock scoplok(&lock_, __func__);

    eventq_.push_back(event);

    eventq_bytes_ += len;
    if (eventq_bytes_ > eventq_bytes_max_) {
        eventq_bytes_max_ = eventq_bytes_;

        //dzdebug
        if (eventq_bytes_max_ > eventq_max_next_size_to_log_) {
            eventq_max_next_size_to_log_+= 100000000;
            log_always("LTPNode.RecvBundleProcessor - new max queued (%zu) allocation size = %zu", 
                       eventq_.size(), eventq_bytes_max_);
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::run()
{
    char threadname[16] = "LTPRcvBndlProc";
    pthread_setname_np(pthread_self(), threadname);

    EventObj* event;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_.read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (!should_stop()) {

        if (should_stop()) return; 

        ret = oasys::IO::poll_multiple(pollfds, 1, 100, nullptr);

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

        // check for an event
        if (event_poll->revents & POLLIN) {
            lock_.lock(__func__);

            eventq_.try_pop(&event);

            if (event) {
                eventq_bytes_ -= event->len_;
                lock_.unlock();

                extract_bundles_from_data(event->data_, event->len_, event->multi_bundle_);
                free(event->data_);
                delete event;
            } else {
                lock_.unlock();
            }
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::extract_bundles_from_data(u_char * bp, size_t len, bool multi_bundle)
{
    size_t temp_payload_bytes_reserved   = len;
    BundleStore* bs = BundleStore::instance();

    if (bp == nullptr)
    {
        bs->release_payload_space(temp_payload_bytes_reserved);
        log_err("extract_bundles_from_data: no data to process?");
        return;
    }

    uint32_t check_client_service = 0;
    // the payload should contain a full bundle
    size_t all_data = len;

    u_char * current_bp = bp;

    while (!should_stop() && (all_data > 0))
    {
        Bundle* bundle = new Bundle(BundleProtocol::BP_VERSION_UNKNOWN);
    
        bool complete = false;
        if (multi_bundle) {
            // Multi bundle buffer... must make sure a 1 precedes all packets
            int bytes_used = SDNV::decode(current_bp, all_data, &check_client_service);
            if (check_client_service != 1)
            {
                log_err("extract_bundles_from_data: LTP SDA - invalid Service ID: %d",check_client_service);
                parent_rcvr_->inc_bundles_failed();
                delete bundle;
                break; // so we free the reserved payload quota
            }
            current_bp += bytes_used;
            all_data   -= bytes_used;
        }
             
        int64_t cc = BundleProtocol::consume(bundle, current_bp, all_data, &complete);
        if (cc < 0)  {
            log_err("extract_bundles_from_data: bundle protocol error");
            parent_rcvr_->inc_bundles_failed();
            delete bundle; break; // so we free the reserved payload quota
        }

        if (!complete) {
            log_err("extract_bundles_from_data: incomplete bundle");
            parent_rcvr_->inc_bundles_failed();
            delete bundle;
            break;  // so we free the reserved payload quota
        }

        all_data   -= cc;
        current_bp += cc;

        //log_debug("extract_bundles_from_data: new Bundle %" PRIbid"  (payload %zu)",
        //          bundle->bundleid(), bundle->payload().length());

        // bundle already approved to use payload space
        bs->reserve_payload_space(bundle);

        parent_rcvr_->inc_bundles_success();


        BundleDaemon::post(
            new BundleReceivedEvent(bundle, EVENTSRC_PEER, cc, EndpointID::NULL_EID(), nullptr));
    }

    // release the temp reserved space since the bundles now have the actually space reserved
    bs->release_payload_space(temp_payload_bytes_reserved);
}







//----------------------------------------------------------------------
LTPNode::Receiver::RecvSegProcessor::RecvSegProcessor(Receiver* parent_rcvr)
    : Logger("LTPNode::Receiver::RecvSegProcessor",
             "/dtn/ltp/rcvr/segproc/%p", this),
      Thread("LTPNode::Receiver::RecvSegProcessor"),
      eventq_admin_(logpath_),
      eventq_ds_(logpath_)
{
    parent_rcvr_ = parent_rcvr;
}

//----------------------------------------------------------------------
LTPNode::Receiver::RecvSegProcessor::~RecvSegProcessor()
{
    shutdown();


    LTPDataReceivedEvent* event;

    while (eventq_ds_.try_pop(&event)) {
        free(event->data_);
        delete event;
    }

    while (eventq_admin_.try_pop(&event)) {
        free(event->data_);
        delete event;
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::shutdown()
{
    // this flag prevents procesing of any new incoming data 
    shutting_down_ = true;

    set_should_stop();

    while (!is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::get_queue_stats(size_t& queue_size, 
                              uint64_t& bytes_queued, uint64_t& bytes_queued_max)
{
    oasys::ScopeLock scoplok(&eventq_ds_lock_, __func__);
    queue_size = eventq_ds_.size();
    bytes_queued = eventq_ds_bytes_;
    bytes_queued_max = eventq_ds_bytes_max_;
}

//----------------------------------------------------------------------
//bool
//LTPNode::Receiver::RecvSegProcessor::okay_to_queue()
//{
//    return true;
//
//    oasys::ScopeLock scoplok(&lock_, __func__);
//
//    return (bytes_queued_ < queued_bytes_quota_);
//}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::post_admin(LTPDataReceivedEvent* event)
{
    if (should_stop()) {
        free(event->data_);
        delete event;
        return;
    }

    eventq_admin_.push_back(event);
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::post_ds(LTPDataReceivedEvent* event)
{
    if (shutting_down_) {
        free(event->data_);
        delete event;
        return;
    }

    oasys::ScopeLock scoplok(&eventq_ds_lock_, __func__);

    eventq_ds_.push_back(event);

    eventq_ds_bytes_ += event->len_;
    if (eventq_ds_bytes_ > eventq_ds_bytes_max_)
    {
        eventq_ds_bytes_max_ = eventq_ds_bytes_;

        //dzdebug
        if (eventq_ds_bytes_max_ > eventq_ds_max_next_size_to_log_) {
            eventq_ds_max_next_size_to_log_+= 10000000;
            log_always("LTPNode.RecvSegProcessor(ds) - new max queued (%zu) allocation size = %zu", 
                       eventq_ds_.size(), eventq_ds_bytes_max_);
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::run()
{
    char threadname[16] = "LTPRcvSegProc";
    pthread_setname_np(pthread_self(), threadname);

    LTPDataReceivedEvent* event;
    int ret;

    struct pollfd pollfds[2];

    struct pollfd* event_admin_poll = &pollfds[0];
    struct pollfd* event_ds_poll = &pollfds[1];

    event_admin_poll->fd = eventq_admin_.read_fd();
    event_admin_poll->events = POLLIN; 
    event_admin_poll->revents = 0;

    event_ds_poll->fd = eventq_ds_.read_fd();
    event_ds_poll->events = POLLIN; 
    event_ds_poll->revents = 0;


    while (!should_stop()) {

        if (should_stop()) return; 

        ret = oasys::IO::poll_multiple(pollfds, 2, 100, nullptr);

        if (ret == oasys::IOINTR) {
            log_err("RecvSegProcessor interrupted - aborting");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("RecvSegProcessor IO Error - aborting");
            set_should_stop();
            continue;
        }

        // process all queued admin events
        do {
            event = nullptr;
            eventq_admin_.try_pop(&event);

            if (event) {
                //bytes_queued_ -= event->len_;

                parent_rcvr_->process_data_for_receiver(event);
                free(event->data_);
                delete event;
            } else {
                break;
            }
        } while (true);


        // process one queued DS event
        event = nullptr;

        eventq_ds_lock_.lock(__func__);

        eventq_ds_.try_pop(&event);

        if (event) {
            eventq_ds_bytes_ -= event->len_;

            eventq_ds_lock_.unlock();

            parent_rcvr_->process_data_for_receiver(event);
            free(event->data_);
            delete event;
        } else {
            eventq_ds_lock_.unlock();
        }
    }
}







//----------------------------------------------------------------------
LTPNode::Sender::Sender(SPtr_LTPEngine& ltp_engine_sptr, LTPNode* parent_node, SPtr_LTPCLSenderIF& clsender)
    : 
      Logger("LTPNode::Sender", "/dtn/ltpnode/sender/%zu", clsender->Remote_Engine_ID()),
      Thread("LTPNode::Sender"),
      clsender_(clsender),
      parent_node_(parent_node)
{
    ltp_engine_sptr_ = ltp_engine_sptr;
    local_engine_id_ = ltp_engine_sptr_->local_engine_id();
    remote_engine_id_ = clsender->Remote_Engine_ID();

    bundle_processor_ = QPtr_SndrBundleProcessor(new SndrBundleProcessor(this));
    bundle_processor_->start();

    char extra_logpath[200];
    strcpy(extra_logpath,logpath_);
    strcat(extra_logpath,"2");
    loading_session_ = nullptr;
    memset(&stats_, 0, sizeof(stats_));
    blank_session_ = std::make_shared<LTPSession>(0UL, 0UL, false);
    blank_session_->Set_Outbound_Cipher_Suite(clsender_->Outbound_Cipher_Suite());
    blank_session_->Set_Outbound_Cipher_Key_Id(clsender_->Outbound_Cipher_Key_Id());
    blank_session_->Set_Outbound_Cipher_Engine(clsender_->Outbound_Cipher_Engine());

    reconfigured();
}

//----------------------------------------------------------------------
LTPNode::Sender::~Sender()
{

    /* ---- cleanup sessions -----*/
    send_sessions_.clear(); 
}

//----------------------------------------------------------------------
void
LTPNode::Sender::run()
{
    char threadname[16] = "LTPSender";
    pthread_setname_np(pthread_self(), threadname);

    SPtr_LTPSession loaded_session;

    // set the initial ready for bundles flag
    bool is_ready = (send_sessions_.size() < max_sessions_);
    clsender_->Set_Ready_For_Bundles(is_ready);

    while (!should_stop()) {

        if (shutting_down_) {
            clsender_->Set_Ready_For_Bundles(false);
            usleep(10000);

            // if loading_session_ is in progress then it will never 
            // be processed which is okay 
            // - basically just waiting for should_stop
            continue;
        }



        // only need to check loading_session_ if not nullptr and the lock is immediately
        // available since send_bundle now checks for agg time expiration also
        if ((loading_session_ != nullptr) && 
            (0 == loading_session_lock_.try_lock("Sender::run()"))) {

            loaded_session = nullptr;

            // loading_session_ may have been cleared out by the time we get the lcok here
            if ((nullptr != loading_session_)  && loading_session_->TimeToSendIt(agg_time_))  
            {
                 loaded_session = loading_session_;
                 loading_session_ = nullptr;
            }

            // release the lock as quick as possible
            loading_session_lock_.unlock();

            if (nullptr != loaded_session) {
                 bundle_processor_->post(loaded_session, false);
                 loaded_session = nullptr;
            }
        }
        usleep(10000);
    }
}

//---------------------------------------------------------------------------
void
LTPNode::Sender::shutdown()
{
    shutting_down_ = true;

    set_should_stop();
    while (!is_stopped()) {
        usleep(100000);
    }

    bundle_processor_->shutdown();
}

//----------------------------------------------------------------------
void
LTPNode::Sender::PostTransmitProcessing(SPtr_LTPSession& session_sptr, bool success)
{
    BundleRef bref;

    while ((bref = session_sptr->Bundle_List()->pop_back()) != nullptr)
    {
        if (bref->expired_in_link_queue()) {
            //log_debug("PostTransmitProcessing not processing bundle that expired in Q: %" PRIbid,
            //         bref->bundleid());
        } else if (success) {
            if (session_sptr->Expected_Red_Bytes() > 0)
            {
                clsender_->PostTransmitProcessing(bref, true,  session_sptr->Expected_Red_Bytes(), success);
            } else if (session_sptr->Expected_Green_Bytes() > 0) { 
                clsender_->PostTransmitProcessing(bref, false,  session_sptr->Expected_Green_Bytes(), success);
            } else {
                log_always("PostTransmitProcessing - Skipping success bundle because expected bytes is 0?? : %" PRIbid,
                         bref->bundleid());

            }

            ++stats_.bundles_success_;
        }
        else
        {
            //log_crit("PostTransmitProcessing - processing bundle failure: %" PRIbid,
            //         bref->bundleid());

            clsender_->PostTransmitProcessing(bref, false, 0, success);
            ++stats_.bundles_failed_;
        }
    }
}

//----------------------------------------------------------------------
void 
LTPNode::Sender::Process_Segment(LTPSegment * segment, bool closed, bool cancelled)
{
    if (shutting_down_) {
        return;
    }

    
    switch(segment->Segment_Type()) {

        case LTP_SEGMENT_RS:
             process_RS((LTPReportSegment *) segment, closed, cancelled);
             break;

        case LTP_SEGMENT_CS_BR:
             process_CS((LTPCancelSegment *) segment);
             break;

        case LTP_SEGMENT_CAS_BS:
             process_CAS((LTPCancelAckSegment *) segment);
             break;

        default:
             log_err("Unknown Segment Type for Sender::Process_Segment(%s)",
                     segment->Get_Segment_Type()); 
             break;
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::process_RS(LTPReportSegment * report_segment, bool closed, bool cancelled)
{
    ++stats_.total_rcv_rs_;

    // was the session previously closed and possibly cancelled?
    if (closed) {
        ++stats_.rs_segment_resends_;

        if (!cancelled) {
            // send RS ACK
            LTPReportAckSegment * ras = new LTPReportAckSegment(report_segment, blank_session_.get());

            ++stats_.total_snt_ra_;

            SPtr_LTPTimer dummy_timer;
            clsender_->Send(ras->asNewString(), dummy_timer, false);
            delete ras;
        } // else ignore
        return;
    }

    SPtr_LTPSession session_sptr = find_sender_session(report_segment);


    if (session_sptr == nullptr) {
        // no tin closed list or current list so it must be pretty old
        ++stats_.rs_segment_resends_;
        return;
    }

    session_sptr->Set_Last_Packet_Time(parent_node_->AOS_Counter());

    //dzdebug
    //log_always("process_RS[%" PRIi64 "]: Session: %s Send RAS(%zu) and process RS - claims: %zu", 
    //          remote_engine_id_, session_sptr->key_str().c_str(), report_segment->Checkpoint_ID(), report_segment->Claims().size());

    LTPReportAckSegment * ras = new LTPReportAckSegment(report_segment, blank_session_.get());

    ++stats_.total_snt_ra_;

    SPtr_LTPTimer dummy_timer;
    clsender_->Send(ras->asNewString(), dummy_timer, false);
    delete ras;

    SPtr_LTPReportClaim rc;
    LTP_RC_MAP::iterator loop_iterator;

    for (loop_iterator  = report_segment->Claims().begin(); 
         loop_iterator != report_segment->Claims().end(); 
         loop_iterator++) 
    {
        rc = loop_iterator->second;     
        session_sptr->RemoveClaimedSegments(&session_sptr->Red_Segments(),
                            //XXX/dz - ION 3.3.0 - multiple Report Segments if
                            //  too many claims for one message (max = 20)
                            // - other RS's have non-zero lower bound
                            report_segment->LowerBounds()+rc->Offset(),
                            rc->Length()); 
    }

    Send_Remaining_Segments(session_sptr);
    
    session_sptr->Set_Last_Packet_Time(parent_node_->AOS_Counter());
}

//----------------------------------------------------------------------
void
LTPNode::Sender::process_CS(LTPCancelSegment * cs_segment)
{
    ++stats_.total_rcv_csr_;
    ++stats_.total_sent_and_rcvd_ca_;

    // send an ACK even if we don't have the session so the peer can close out its session
    LTPCancelAckSegment * cas_seg = new LTPCancelAckSegment(cs_segment, 
                                                            LTPSEGMENT_CAS_BR_BYTE, blank_session_.get()); 


    SPtr_LTPTimer dummy_timer;
    clsender_->Send(cas_seg->asNewString(), dummy_timer, false);
    delete cas_seg;

    // now try to find the session and close it out
    SPtr_LTPSession session_sptr = find_sender_session(cs_segment);

    if (session_sptr != nullptr) {
        session_sptr->RemoveSegments();
        cleanup_sender_session(cs_segment->Engine_ID(), cs_segment->Session_ID(), 0, 0, cs_segment->Segment_Type());
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::process_CAS(LTPCancelAckSegment * cas_segment)
{
    ++stats_.total_sent_and_rcvd_ca_;

    SPtr_LTPSession session_sptr = find_sender_session(cas_segment);

    if (session_sptr != nullptr) {
        session_sptr->Clear_Cancel_Segment();

        erase_send_session(session_sptr, true);
    }
}

//----------------------------------------------------------------------
void 
LTPNode::Sender::build_CS_segment(SPtr_LTPSession& session_sptr, int segment_type, u_char reason_code)
{
    session_sptr->RemoveSegments();

    SPtr_LTPCancelSegment cancel_segment = std::make_shared<LTPCancelSegment>(session_sptr.get(), segment_type, reason_code);

    update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_CS); 

    SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

    SPtr_LTPSegment seg = cancel_segment;

    SPtr_LTPTimer timer;
    timer = cancel_segment->Create_Retransmission_Timer(sndr_if, LTP_EVENT_CS_TO, retran_interval_, seg);

    session_sptr->Set_Cancel_Segment(cancel_segment);

    ++stats_.total_snt_css_;

    clsender_->Send(cancel_segment->asNewString(), timer, false);

    //signal DTN that transmit failed for the bundle(s)
    PostTransmitProcessing(session_sptr, false);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::dump_sessions(oasys::StringBuffer* buf)
{
    if (shutting_down_) {
        return;
    }

    SESS_SPTR_MAP::iterator iter; 
    SPtr_LTPSession session_sptr;
    size_t ctr_ds = 0;
    size_t ctr_rs = 0;
    size_t ctr_cs = 0;

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    // validate the counts while we have the lock
    iter = send_sessions_.begin();
    while (iter != send_sessions_.end())
    {
        session_sptr = iter->second;

        switch (session_sptr->Session_State())
        {
            case LTPSession::LTP_SESSION_STATE_RS: 
                ++ctr_rs;
                break;
            case LTPSession::LTP_SESSION_STATE_CS:
                ++ctr_cs;
                break;
            case LTPSession::LTP_SESSION_STATE_DS:
                ++ctr_ds;
                break;
            default:
              ;
        }
        ++iter;
    }

    if (ctr_ds != sessions_state_ds_) {
        buf->appendf("detected incorrect DS count - changing from %zu to %zu",
                     sessions_state_ds_, ctr_ds);
        sessions_state_ds_ = ctr_ds;
    }
    if (ctr_rs != sessions_state_rs_) {
        buf->appendf("detected incorrect DS count - changing from %zu to %zu",
                     sessions_state_rs_, ctr_rs);
        sessions_state_rs_ = ctr_rs;
    }
    if (ctr_cs != sessions_state_cs_) {
        buf->appendf("detected incorrect DS count - changing from %zu to %zu",
                     sessions_state_cs_, ctr_cs);
        sessions_state_cs_ = ctr_cs;
    }

    buf->appendf("\nSender Sessions: Total: %zu  (States- DS: %zu  RS: %zu  CS: %zu - Closing: %zu)\n",
                 send_sessions_.size(), sessions_state_ds_, sessions_state_rs_, sessions_state_cs_,
                 ltp_engine_sptr_->get_closed_session_count());
    buf->appendf("Sender Bundles: Queued: %zu InFlight: %zu\n",
                 clsender_->Get_Bundles_Queued_Count(), clsender_->Get_Bundles_InFlight_Count());

}

//----------------------------------------------------------------------
void
LTPNode::Sender::dump_success_stats(oasys::StringBuffer* buf)
{
    if (shutting_down_) {
        return;
    }

               //                Sessions    Sessions        Data Segments           Report Segments      Rpt Acks    Bundles   
               //             Total  /  Max   DS RXmt      Total    /  ReXmit        Total   /  ReXmit      Total     Success
               //         -----------------  --------  -------------------------  ---------------------  ----------  ----------
               //Sender   1234567890 / 1234  12345678  123456789012 / 1234567890  1234567890 / 12345678  1234567890  1234567890
               //Receiver 1234567890 / 1234  12345678  123456789012 / 1234567890  1234567890 / 12345678  1234567890  1234567890

    buf->appendf("Sender   %10" PRIu64 " / %4" PRIu64 "  %8" PRIu64 "  %12" PRIu64 " / %10" PRIu64 "  %10" PRIu64 " / %8" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "\n", 
                 stats_.total_sessions_, stats_.max_sessions_,
                 stats_.ds_sessions_with_resends_, stats_.total_snt_ds_, stats_.ds_segment_resends_,
                 stats_.total_rcv_rs_, stats_.rs_segment_resends_, stats_.total_snt_ra_,
                 stats_.bundles_success_);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::dump_cancel_stats(oasys::StringBuffer* buf)
{
    if (shutting_down_) {
        return;
    }

               //             Cancel By Sender       Cancel By Receiver     Cancl Acks  Cancelled   No Rpt Ack   Bundles     Bundles   
               //             Total  /   ReXmit         Total / ReXmit      Rcvd Total   But Got     But Got    Expird inQ   Failure
               //         -----------------------  -----------------------  ----------  ----------  ----------  ----------  ----------
               //Sender   1234567890 / 1234567890  1234567890 / 1234567890  1234567890  1234567890  1234567890  1234567890  1234567890
               //Receiver 1234567890 / 1234567890  1234567890 / 1234567890  1234567890  1234567890  1234567890  1234567890  1234567890
    buf->appendf("Sender   %10" PRIu64 " / %10" PRIu64 "  %10" PRIu64 " / %10" PRIu64 "  %10" PRIu64 "  %10d  %10d  %10" PRIu64 "  %10" PRIu64 "\n",
                 stats_.total_snt_css_, stats_.css_segment_resends_, 
                 stats_.total_rcv_csr_, stats_.csr_segment_resends_, stats_.total_sent_and_rcvd_ca_,
                 0, 0,
                 stats_.bundles_expired_in_queue_, stats_.bundles_failed_);
}


//----------------------------------------------------------------------
void
LTPNode::Sender::clear_statistics()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void 
LTPNode::Sender::update_session_counts(SPtr_LTPSession& session, int new_state)
{
    int old_state = session->Session_State();

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    if (old_state != new_state) {
        switch (old_state)
        {
            case LTPSession::LTP_SESSION_STATE_RS: 
                --sessions_state_rs_;
                break;
            case LTPSession::LTP_SESSION_STATE_CS:
                --sessions_state_cs_;
                break;
            case LTPSession::LTP_SESSION_STATE_DS:
                --sessions_state_ds_;
                break;
            default:
              ;
        }

        switch (new_state)
        {
            case LTPSession::LTP_SESSION_STATE_RS: 
                ++sessions_state_rs_;
                break;
            case LTPSession::LTP_SESSION_STATE_CS:
                ++sessions_state_cs_;
                break;
            case LTPSession::LTP_SESSION_STATE_DS:
                ++sessions_state_ds_;
                break;
            default:
              ;
        }

        session->Set_Session_State(new_state); 
    }

    // less than or equal here for case where last session is currently being loaded
    bool is_ready = !shutting_down_ && 
                    ((send_sessions_.size() - sessions_state_cs_) <= max_sessions_);
    clsender_->Set_Ready_For_Bundles(is_ready);
}

//----------------------------------------------------------------------
size_t
LTPNode::Sender::Send_Remaining_Segments(SPtr_LTPSession& session_sptr)
{
    size_t result = 0;

    if (shutting_down_) {
        return result;
    }

    //
    // ONLY DEALS WITH RED SEGMENTS
    //

    if (session_sptr->Red_Segments().size() > 0)
    {
        SPtr_LTPTimer timer;
        SPtr_LTPDataSegment ds_seg;
        LTP_DS_MAP::iterator iter;

        //dzdebug
        //log_always("Send_Remaining_Segments: Session: %s - resending %zu segments",
        //         session_sptr->key_str().c_str(), session_sptr->Red_Segments().size());

        iter     = --session_sptr->Red_Segments().end();
        ds_seg   = iter->second;

        if (!ds_seg->IsCheckpoint())
        {
            ds_seg->Set_Checkpoint_ID(session_sptr->Increment_Checkpoint_ID());
            ds_seg->SetCheckpoint();
            ds_seg->Encode_All();
        }
        // XXX/dz what about sessions that require multiple reports to get all of the DS?
        //        flag session as already had resends?
        ++stats_.ds_sessions_with_resends_;

        for (iter = session_sptr->Red_Segments().begin(); iter != session_sptr->Red_Segments().end(); iter++)
        {
            ds_seg = iter->second;

            ++stats_.ds_segment_resends_;
            //++stats_.total_snt_ds_;
            if (ds_seg->IsCheckpoint())
            {
                SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

                SPtr_LTPSegment seg = ds_seg;

                timer = ds_seg->Create_Retransmission_Timer(sndr_if, LTP_EVENT_DS_TO, retran_interval_, seg);
            } else {
                timer = nullptr;
            }

            clsender_->Send(ds_seg->asNewString(), timer, true);

            ++result;
        }

    } else {
        cleanup_sender_session(session_sptr->Engine_ID(), session_sptr->Session_ID(), 0, 0, LTP_EVENT_SESSION_COMPLETED);
    }
    return result; 
}

//----------------------------------------------------------------------
int64_t
LTPNode::Sender::send_bundle(const BundleRef& bref)
{
    if (shutting_down_) {
        return -1;
    }

    int64_t total_len = 0;

    Bundle * bundle_ptr  = (Bundle *) bref.object();

    //dzdebug
    do {  // just to limit the scope of the lock
        oasys::ScopeLock l(bref->lock(), __func__);

        SPtr_BlockInfoVec blocks = clsender_->GetBlockInfoVec(bundle_ptr);

        if (blocks == nullptr) {
            //dzdebug - only seen when bundle has expired
            //log_always("dzdebug - LTPEngine::send_bundle got null blocks for bundle *%p - expired = %s",
            //           bundle_ptr, bref->expired()?"true":"false");
        } else {
            total_len = BundleProtocol::total_length(bundle_ptr, blocks);
        }
    } while (false); // end of scope



    if (bref->expired() || bref->manually_deleting())
    {
        bref->set_expired_in_link_queue();
        ++stats_.bundles_expired_in_queue_;

        // do not add to the LTP session
        clsender_->Del_From_Queue(bref);
        log_warn("send_bundle: ignoring expired/deleted bundle" ); 
        return -1;
    }


#ifdef ECOS_ENABLED
    bool green           = bref->ecos_enabled() && bref->ecos_streaming();
#else
    bool green           = false;
#endif //ECOS_ENABLED

    SPtr_LTPSession loaded_session;

    if (green) {

        SPtr_LTPSession green_session = std::make_shared<LTPSession>(local_engine_id_, 
                                                                     ltp_engine_sptr_->new_session_id(remote_engine_id_));
        // not part of the send_sessions_ list so do not update_session_counts
        green_session->Set_Session_State(LTPSession::LTP_SESSION_STATE_DS);
        green_session->Set_Outbound_Cipher_Key_Id(clsender_->Outbound_Cipher_Key_Id());
        green_session->Set_Outbound_Cipher_Suite(clsender_->Outbound_Cipher_Suite());
        green_session->Set_Outbound_Cipher_Engine(clsender_->Outbound_Cipher_Engine());

        green_session->AddToBundleList(bref, total_len);

        bundle_processor_->post(green_session, true);


    } else {  // define scope for the ScopeLock

        // prevent run method from accessing the loading_session_ while working with it
        oasys::ScopeLock loadlock(&loading_session_lock_,"Sender::send_bundle");  

        if (loading_session_ == nullptr) {
            // sets loading_session_ if it true
            if (!create_sender_session()) {
                return -1;
            }
        }
        // use the loading sesssion and add the bundle to it.
 
        update_session_counts(loading_session_, LTPSession::LTP_SESSION_STATE_DS); 


        loading_session_->AddToBundleList(bref, total_len);

        // check the aggregation size and time while we have the loading session
        if (loading_session_->Session_Size() >= agg_size_) {
            loaded_session = loading_session_;
            loading_session_ = nullptr;
        } else if (loading_session_->TimeToSendIt(agg_time_)) {
            loaded_session = loading_session_;
            loading_session_ = nullptr;
        }
    }

    // do link housekeeping now that the lock is released
    if (!green) {
        clsender_->Add_To_Inflight(bref);
    }
    clsender_->Del_From_Queue(bref);

    if (loaded_session != nullptr && !green) {
        bundle_processor_->post(loaded_session, false);
    }

    return total_len;
}

//----------------------------------------------------------------------
void
LTPNode::Sender::process_bundles(SPtr_LTPSession& loaded_session, bool is_green)
{
    if (shutting_down_) {
        return;
    }


    size_t  current_data      = 0;
    size_t  segments_created  = 0;
    size_t  data_len          = loaded_session->Session_Size();
    ssize_t total_bytes       = loaded_session->Session_Size();
    size_t  max_segment_size  = seg_size_;
    int     bundle_ctr        = 0;
    size_t  bytes_produced    = 0;
    int     client_service_id = 1;   // 1 = Bundle Protocol (single bundle for CCSDS or multiple for ION)
    ssize_t extra_bytes       = 0;

    int num_bundles = loaded_session->Bundle_List()->size();

    if (ccsds_compatible_) {
        if (num_bundles > 0) {
            client_service_id = 2;  // 2 = LTP Service Data Aggregation
            extra_bytes = num_bundles;
        }
    }


    u_char* buffer = (u_char *) malloc(total_bytes+extra_bytes);

    if (buffer == nullptr) {
       log_crit("Error allocating memory of size %zd - bundle too large - use mtu setting on link to fragment bundle", 
                total_bytes+extra_bytes);

        // signal the BundleDeamon that these bundles failed
        PostTransmitProcessing(loaded_session, false);
        cleanup_sender_session(loaded_session->Engine_ID(), loaded_session->Session_ID(), 0, 0, 
                               LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES);
        return;
    }


    Bundle * bundle = nullptr;
    BundleRef br("Sender::process_bundles()");  
    BundleList::iterator bundle_iter; 
    BundleList::iterator deletion_iter;

    oasys::ScopeLock bl(loaded_session->Bundle_List()->lock(), "process_bundles()"); 

    
    deletion_iter = loaded_session->Bundle_List()->end(); 

    for (bundle_iter =  loaded_session->Bundle_List()->begin(); 
        bundle_iter != loaded_session->Bundle_List()->end(); 
        bundle_iter ++)
    {
        if (deletion_iter != loaded_session->Bundle_List()->end()) {
            loaded_session->Bundle_List()->erase(deletion_iter);
            deletion_iter = loaded_session->Bundle_List()->end();
        }

        bool complete = false;
        bool skip_expired_bundle = false;
        size_t total_len = 0;

        bundle = *bundle_iter;
        br     = bundle;


        do {   // just to limit the scope of the lock
            if (shutting_down_) {
                return;
            }

            oasys::ScopeLock scoplok(br->lock(), __func__);

            // check for expired/deleted bundle
            if (bundle->expired() || bundle->manually_deleting())
            {
                // do not add to the LTP session
                // must unlock before deleting from the inflight queue
                scoplok.unlock();

                if (!is_green) {
                    if (!clsender_->Del_From_InFlight_Queue(br)) {
                        log_err("Failed to delete from InFLight Queue expired bundle *%p", bundle);
                    }
                }
                deletion_iter = bundle_iter;

                bundle->set_expired_in_link_queue();
                ++stats_.bundles_expired_in_queue_;
                skip_expired_bundle = true;
                
                break;
            }

            SPtr_BlockInfoVec blocks = clsender_->GetBlockInfoVec(bundle);

            ASSERT(blocks != nullptr);
   
            bundle_ctr++;

            if (extra_bytes > 0) {
               *(buffer+bytes_produced) = 1;
               bytes_produced ++;
            }
            total_len = BundleProtocol::produce(bundle, blocks,
                                                buffer+bytes_produced, 0, 
                                                total_bytes, &complete);
        } while (false);  // end of lock scope


        // finished with the xmit blocks so we can now free up that memory
        clsender_->Delete_Xmit_Blocks(br);

        if (!skip_expired_bundle) {
            if (!complete) {
                log_err("process_bundle: aborting due to error in BP::produce on bundle *%p - expired = %s  LTPSession: %zu:%zu",
                        bundle, bundle->expired()?"true":"false", loaded_session->Engine_ID(), loaded_session->Session_ID());

                // force cancellation of this session
                bundle_ctr = 0;
                break;
            }

            total_bytes    -= total_len;
            bytes_produced += total_len;
        }


    }

    if (deletion_iter != loaded_session->Bundle_List()->end()) {
        loaded_session->Bundle_List()->erase(deletion_iter);
        deletion_iter = loaded_session->Bundle_List()->end();
    }

    if (shutting_down_) {
        return;
    }

    bl.unlock();

    if (bundle_ctr == 0) {
        if (buffer != nullptr) free(buffer);
        // signal the BundleDeamon that these bundles failed
        PostTransmitProcessing(loaded_session, false);
        cleanup_sender_session(loaded_session->Engine_ID(), loaded_session->Session_ID(), 0, 0, 
                               LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES);
        return;
    }

    if (loaded_session->Bundle_List()->size() == 0) {
        log_crit("%s: transmitting LTP session %s with zero bundles!",
                 __func__, loaded_session->key_str().c_str());
    }

    bool created_retran_timer = false;

    data_len   = bytes_produced;

    while ((data_len > 0) && !shutting_down_)
    {
        SPtr_LTPDataSegment ds_seg = std::make_shared<LTPDataSegment>(loaded_session.get());


        segments_created ++;

        ds_seg->Set_Client_Service_ID(client_service_id); 

        if (is_green)
            ds_seg->Set_RGN(GREEN_SEGMENT);
        else
            ds_seg->Set_RGN(RED_SEGMENT);

        //
        // always have header and trailer....  
        //
        //if (current_data == 0)
        //{
            ds_seg->Set_Security_Mask(LTP_SECURITY_HEADER | LTP_SECURITY_TRAILER);
        //} else {
        //    ds_seg->Set_Security_Mask(LTP_SECURITY_TRAILER);
        //}
       /* 
        * see if it's a full packet or not
        */ 
        if (data_len  > max_segment_size) {
                                                                   // not a checkpoint (false)
           ds_seg->Encode_Buffer(&buffer[current_data], max_segment_size, current_data, false);
           data_len         -= max_segment_size;
           current_data     += max_segment_size; 
        } else {
                                                   // last seg is a checkpoint (true)
           ds_seg->Encode_Buffer(&buffer[current_data], data_len, current_data, true);
           data_len         -= data_len;
           current_data     += data_len; 
        }


        // generating the new segments so no possible overlap
        loaded_session->AddSegment(ds_seg, false);  //false = no check for overlap needed

        SPtr_LTPTimer timer;
        if (!is_green) {
            if (ds_seg->IsCheckpoint()) {
                SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

                SPtr_LTPSegment seg = ds_seg;

                timer = ds_seg->Create_Retransmission_Timer(sndr_if, LTP_EVENT_DS_TO, retran_interval_, seg);

                created_retran_timer = true;
            }
        }

        ++stats_.total_snt_ds_;

        clsender_->Send_Low_Priority(ds_seg->asNewString(), timer);

    }

    loaded_session->Set_Last_Packet_Time(parent_node_->AOS_Counter());
    SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();
    loaded_session->Start_Inactivity_Timer(sndr_if, inactivity_interval_);

    if (!created_retran_timer) {
        log_crit("Sent Session: %s but not retransmit timer was created!", loaded_session->key_str().c_str());
    }

    if (buffer != nullptr) free(buffer);


    // if sending green we can declare these bundles transmitted
    if (is_green) {
        PostTransmitProcessing(loaded_session);
    }

    return;
}

//----------------------------------------------------------------------
void
LTPNode::Sender::RSIF_post_timer_to_process(oasys::SPtr_Timer& event)
{
    parent_node_->Post_Timer_To_Process(event);
}

//----------------------------------------------------------------------
uint64_t
LTPNode::Sender::RSIF_aos_counter()
{
    return parent_node_->AOS_Counter();
}

//----------------------------------------------------------------------
void
LTPNode::Sender::RSIF_retransmit_timer_triggered(int event_type, SPtr_LTPSegment& retran_seg)
{
    if (event_type == LTP_EVENT_DS_TO) {
        data_seg_timeout(retran_seg);
    } else if (event_type == LTP_EVENT_CS_TO) {
        cancel_seg_timeout(retran_seg);
    } else {
        log_err("Unexpected RetransmitSegTimer triggerd: %s", LTPSegment::Get_Event_Type(event_type));
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::RSIF_inactivity_timer_triggered(uint64_t engine_id, uint64_t session_id)
{
    SPtr_LTPSession session_sptr = find_sender_session(engine_id, session_id);

    if (session_sptr != nullptr) {
        if (session_sptr->Session_State() == LTPSession::LTP_SESSION_STATE_DS) {
            time_t time_left = session_sptr->Last_Packet_Time() +  inactivity_interval_;
            time_left = time_left - parent_node_->AOS_Counter();

            if (time_left > 0) /* timeout occured but retries are not exhausted */
            {
                SPtr_LTPNodeRcvrSndrIF sndr_rsif = parent_node_->sender_rsif_sptr();
                session_sptr->Start_Inactivity_Timer(sndr_rsif, time_left); // catch the remaining seconds...
            } else {
                build_CS_segment(session_sptr, LTP_SEGMENT_CS_BS, 
                                 LTPCancelSegment::LTP_CS_REASON_RXMTCYCEX);
            }
        } else {
            //log_always("%s): %s - Inactivity Timeout ignored - session no longer in DS state - state = %s ",
            //           __func__, session_sptr->key_str().c_str(), session_sptr->Get_Session_State());
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::RSIF_closeout_timer_triggered(uint64_t engine_id, uint64_t session_id)
{
    //(void) engine_id;
    ASSERT(engine_id == local_engine_id_);

    ltp_engine_sptr_->delete_closed_session(session_id);
}



//----------------------------------------------------------------------
void
LTPNode::Sender::data_seg_timeout(SPtr_LTPSegment& seg)
{ 
    if (!seg->IsDeleted()) {
        // Okay to resend segment if retries not exceeded

        SPtr_LTPSession session_sptr = find_sender_session(seg->Engine_ID(), seg->Session_ID());

        if (session_sptr != nullptr) {
            // for logging
            std::string key_str = session_sptr->key_str();

            if (!session_sptr->Is_LTP_Cancelled()) {
                SPtr_LTPDataSegment ds_seg = std::dynamic_pointer_cast<LTPDataSegment>(seg);

                if (ds_seg != nullptr) {

                    ds_seg->Increment_Retries();

                    if (ds_seg->Retrans_Retries() <= retran_retries_) {
                        //dzdebug
                        //log_always("DS timeout: Session: %s - Resending (Checkpoint) Data Segment",
                        //          key_str.c_str());

                        ++stats_.ds_segment_resends_;
                        update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_DS); 
                    
                        SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

                        SPtr_LTPTimer timer;
                        timer = ds_seg->Create_Retransmission_Timer(sndr_if, LTP_EVENT_DS_TO, retran_interval_, seg);

                        clsender_->Send(ds_seg->asNewString(), timer, false);

                        session_sptr->Set_Last_Packet_Time(parent_node_->AOS_Counter());

                    } else {
                        //dzdebug
                        //log_always("DS timeout: Session: %s - Resend Data Segment exceeds max - giving up",
                        //          key_str.c_str());

                        ds_seg->Set_Deleted();

                        build_CS_segment(session_sptr,LTP_SEGMENT_CS_BS, 
                                         LTPCancelSegment::LTP_CS_REASON_RXMTCYCEX);

                    }
                } else {
                    //log_always("DS timeout - Session: %s - ignoring because session already in Cancel state", key_str.c_str());
                }
            } else {
                log_always("DS timeout - error casting seg to ds_seg!");
            }
        } else {
            //log_always("DS timeout - ignoring - session not found");
        }
    } else {
        //SPtr_LTPSession session_sptr = find_sender_session(seg->Engine_ID(), seg->Session_ID());
        //if (session_sptr != nullptr) {
            //log_always("DS timeout - Session: %s (%s) - ignoring since flagged as deleted", 
            //           seg->session_key_str().c_str(), 
            //           (session_sptr == nullptr) ? "not found":"foound");
        //}
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::cancel_seg_timeout(SPtr_LTPSegment& seg)
{ 
    if (!seg->IsDeleted()) {
        // Okay to resend segment if retries not exceeded

        SPtr_LTPSession session_sptr = find_sender_session(seg->Engine_ID(), seg->Session_ID());

        if (session_sptr != nullptr) {
            // for logging
            std::string key_str = session_sptr->key_str();

            SPtr_LTPCancelSegment cs_seg = std::dynamic_pointer_cast<LTPCancelSegment>(seg);

            if (cs_seg != nullptr) {

                cs_seg->Increment_Retries();

                if (cs_seg->Retrans_Retries() <= retran_retries_) {
                    //dzdebug
                    //log_always("CS timeout: Session: %s - Resending Cancel Segment",
                    //          key_str.c_str());

                    SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

                    SPtr_LTPTimer timer;
                    timer = cs_seg->Create_Retransmission_Timer(sndr_if, LTP_EVENT_CS_TO, retran_interval_, seg);

                    ++stats_.css_segment_resends_;

                    clsender_->Send(cs_seg->asNewString(), timer, false);
                } else {
                    //dzdebug
                    //log_always("CS timeout: Session: %s - Resend Cancel Segment exceeds max - giving up",
                    //          key_str.c_str());

                    cs_seg->Set_Deleted();

                    erase_send_session(session_sptr, true);
                }
            } else {
                log_always("CS timeout - error casting seg to cs_seg!");
            }
        } else {
            //log_always("CS timeout - ignoring - session not found");
        }
    } else {
        //log_always("CS timeout - Session: %s - ignoring since flagged as deleted", seg->session_key_str().c_str());
    }
}


//----------------------------------------------------------------------
void
LTPNode::Sender::cleanup_sender_session(uint64_t engine_id, uint64_t session_id, uint64_t ds_start_byte, uint64_t ds_stop_byte,
                                                       int event_type)
{ 
    (void) ds_start_byte;
    (void) ds_stop_byte;

    //
    // only deals with red segments
    //


    bool cancelled = true;

    SPtr_LTPDataSegment ds;
    LTP_DS_MAP::iterator cleanup_segment_iterator;


    SPtr_LTPSession session_sptr = find_sender_session(engine_id, session_id);

    if (session_sptr != nullptr) {

        session_sptr->Cancel_Inactivity_Timer();

        switch(event_type)
        { 
            case LTP_SEGMENT_CS_BR:

                 //log_debug("cleanup_sender_session(CS_BR): Session: %s - Cancel by Receiver received - delete session",
                 //          session_sptr->key_str().c_str());

                 // signal DTN that transmit failed for the bundle(s)
                 PostTransmitProcessing(session_sptr, false);
                 break;

            case LTP_EVENT_SESSION_COMPLETED: 
                 //log_debug("cleanup_sender_session(COMPLETED): Session: %s - delete session",
                 //          session_sptr->key_str().c_str());

                 cancelled = false;
                 PostTransmitProcessing(session_sptr);
                 break;

            case LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES:
                 // all bundles were expired - pass through and delete the session
                 PostTransmitProcessing(session_sptr, false);
                 break;


            default: // all other cases don't lookup a segment...
                 
                 log_err("cleanup_sender_session: unknown event type: %d", event_type);
                 return;
        }

        erase_send_session(session_sptr, cancelled);

    } else {
        log_debug("cleanup_sender_session(CS_BR): Session: %zzu:zu - not found",
                  engine_id, session_id);
    }

    return;
}

//----------------------------------------------------------------------
SPtr_LTPSession
LTPNode::Sender::find_sender_session(LTPSegment* seg)
{
    return find_sender_session(seg->Engine_ID(), seg->Session_ID());
}

//----------------------------------------------------------------------
SPtr_LTPSession
LTPNode::Sender::find_sender_session(uint64_t engine_id, uint64_t session_id)
{
    SPtr_LTPSession session_sptr;

    QPtr_LTPSessionKey qkey = QPtr_LTPSessionKey(new LTPSessionKey(engine_id, session_id));

    SESS_SPTR_MAP::iterator iter; 

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    iter = send_sessions_.find(qkey.get());
    if (iter != send_sessions_.end())
    {
        session_sptr = iter->second;
    }

    return session_sptr;
}

//----------------------------------------------------------------------
bool
LTPNode::Sender::create_sender_session()
{
    bool result = false;

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    bool is_ready = !shutting_down_ && 
                    ((send_sessions_.size() - sessions_state_cs_) < max_sessions_);

    if (is_ready) {
        result = true;

        uint64_t session_id = ltp_engine_sptr_->new_session_id(remote_engine_id_);

        loading_session_ = std::make_shared<LTPSession>(local_engine_id_, session_id);
        loading_session_->SetAggTime();
        loading_session_->Set_Outbound_Cipher_Key_Id(clsender_->Outbound_Cipher_Key_Id());
        loading_session_->Set_Outbound_Cipher_Suite(clsender_->Outbound_Cipher_Suite());
        loading_session_->Set_Outbound_Cipher_Engine(clsender_->Outbound_Cipher_Engine());

        send_sessions_[loading_session_.get()] = loading_session_;

    
        if (send_sessions_.size() > stats_.max_sessions_) {
            stats_.max_sessions_ = send_sessions_.size();
        }

        ++stats_.total_sessions_;
    }
    
    clsender_->Set_Ready_For_Bundles(is_ready);

    return result;
}

//----------------------------------------------------------------------
void
LTPNode::Sender::erase_send_session(SPtr_LTPSession& session_sptr, bool cancelled)
{
    ltp_engine_sptr_->closed_session(session_sptr->Session_ID(), remote_engine_id_, cancelled);

    // start an InactivityTimer to eventually delete the closed out session from Engine's list
    SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();
    session_sptr->Start_Closeout_Timer(sndr_if, inactivity_interval_);

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    send_sessions_.erase(session_sptr.get());

    update_session_counts(session_sptr, LTPSession::LTP_SESSION_STATE_UNDEFINED); 

    // less than or equal here for case where last session is currently being loaded
    bool is_ready = !shutting_down_ && 
                    ((send_sessions_.size() - sessions_state_cs_) <= max_sessions_);
    clsender_->Set_Ready_For_Bundles(is_ready);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::reconfigured()
{
    max_sessions_ = clsender_->Max_Sessions();
    agg_time_     = clsender_->Agg_Time();
    agg_size_     = clsender_->Agg_Size();
    seg_size_     = clsender_->Seg_Size();
    ccsds_compatible_    = clsender_->CCSDS_Compatible();
    retran_retries_      = clsender_->Retran_Retries();
    retran_interval_     = clsender_->Retran_Intvl();
    inactivity_interval_ = clsender_->Inactivity_Intvl();

    bundle_processor_->reconfigured();
}


//----------------------------------------------------------------------
LTPNode::Sender::SndrBundleProcessor::SndrBundleProcessor(Sender* parent_sndr)
    : Logger("LTPNode::Sender::SndrBundleProcessor",
             "/dtn/ltp/sndr/bundleproc/%p", this),
      Thread("LTPNode::Sender::SndrBundleProcessor"),
      eventq_(logpath_)
{
    parent_sndr_ = parent_sndr;

    queued_bytes_quota_ = parent_sndr_->ltp_queued_bytes_quota();
    
}

//----------------------------------------------------------------------
LTPNode::Sender::SndrBundleProcessor::~SndrBundleProcessor()
{
    shutdown();

    if (eventq_.size() > 0) {
        EventObj* event;
        while (eventq_.try_pop(&event)) {
            delete event;
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::shutdown()
{
    set_should_stop();
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::reconfigured()
{
    queued_bytes_quota_ = parent_sndr_->ltp_queued_bytes_quota();
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::get_queue_stats(size_t& queue_size, 
                              uint64_t& bytes_queued, uint64_t& bytes_queued_max)
{
    oasys::ScopeLock scoplok(&lock_, __func__);
    queue_size = eventq_.size();
    bytes_queued = bytes_queued_;
    bytes_queued_max = bytes_queued_max_;
}

//----------------------------------------------------------------------
bool
LTPNode::Sender::SndrBundleProcessor::okay_to_queue()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    return (bytes_queued_ < queued_bytes_quota_);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::post(SPtr_LTPSession& session_sptr, bool is_green)
{
    // Sender goes into shutdown mode before the SndrBundleProcessor so
    // continue to accept new posts from it even if our shutting_down_ flag is set

    EventObj* event = new EventObj();
    event->session_sptr_ = session_sptr;
    event->is_green_ = is_green;

//    oasys::ScopeLock scoplok(&lock_, __func__);

    eventq_.push_back(event);

//    bytes_queued_ += len;
//    if (bytes_queued_ > bytes_queued_max_) {
//        bytes_queued_max_ = bytes_queued_;

        //dzdebug
//        if (bytes_queued_max_ > 10000000) {
//            log_always("LTPNode.SndrBundleProcessor - new max queued (%zu) allocation size = %zu", 
//                       eventq_.size(), bytes_queued_max_);
//        }
//    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::run()
{
    char threadname[16] = "LTPSndBndlProc";
    pthread_setname_np(pthread_self(), threadname);

    EventObj* event;
    int ret;

    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];

    event_poll->fd = eventq_.read_fd();
    event_poll->events = POLLIN; 
    event_poll->revents = 0;

    while (!should_stop()) {

        if (should_stop()) return; 

        ret = oasys::IO::poll_multiple(pollfds, 1, 100, nullptr);

        if (ret == oasys::IOINTR) {
            log_err("SndrBundleProcessor interrupted - aborting");
            set_should_stop();
            continue;
        }

        if (ret == oasys::IOERROR) {
            log_err("SndrBundleProcessor IO Error - aborting");
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            eventq_.try_pop(&event);

            // XXX/dz experienced a POLLIN indication that resulted in a nullptr after the try_pop
            //        so no ASSERT and checking the event before use

            if (event) {
                parent_sndr_->process_bundles(event->session_sptr_, event->is_green_);
                event->session_sptr_ = nullptr;
                delete event;
            } else {
            }
        }
    }
}







//----------------------------------------------------------------------
LTPNode::TimerProcessor::TimerProcessor(uint64_t remote_engine_id)
    : Logger("LTPNode::TimerProcessor",
             "/dtn/ltp/node/timerproc/%p", this),
      Thread("LTPNode::TimerProcessor"),
      eventq_(logpath_)
{
    remote_engine_id_ = remote_engine_id;
    start();
}

LTPNode::TimerProcessor::~TimerProcessor()
{
    set_should_stop();

    while (! is_stopped()) {
        usleep(100000);
    }
}

/// Post a Timer to trigger
void
LTPNode::TimerProcessor::post(oasys::SPtr_Timer& event)
{
    eventq_.push_back(event);
}

/// TimerProcessor main loop
void
LTPNode::TimerProcessor::run() 
{
    char threadname[16] = "LTPSndTimerProc";
    pthread_setname_np(pthread_self(), threadname);
   

    oasys::SPtr_Timer event;
    SPtr_LTPTimer ltp_timer;

    // block on input from the bundle event list
    struct pollfd pollfds[1];

    struct pollfd* event_poll = &pollfds[0];
    event_poll->fd = eventq_.read_fd();
    event_poll->events = POLLIN;
    event_poll->revents = 0;

    while (!should_stop()) {

        // block waiting...
        int ret = oasys::IO::poll_multiple(pollfds, 1, 100);

        if (ret == oasys::IOINTR) {
            set_should_stop();
            continue;
        }

        // check for an event
        if (event_poll->revents & POLLIN) {
            event = nullptr;
            if (eventq_.try_pop(&event)) {
                if (event != nullptr) {
                    ltp_timer = std::dynamic_pointer_cast<LTPTimer>(event);
                    ASSERT(ltp_timer!= nullptr);

                    event = nullptr;

                    ltp_timer->do_timeout_processing();
                    // NOTE: these timers delete themselves when the shared_ptrs release

                    ltp_timer = nullptr;
                }
            }
        }
    }
}

} // namespace dtn

