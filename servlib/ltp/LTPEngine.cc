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

#include <inttypes.h>

#include <errno.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

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
#include "bundling/FormatUtils.h"
#include "storage/BundleStore.h"

namespace dtn {


//-----------------------------------------------------------------------
LTPEngineReg::LTPEngineReg(SPtr_LTPCLSenderIF sptr_clsender)
{
    remote_engine_id_ = sptr_clsender->Remote_Engine_ID();
    sptr_clsender_  = sptr_clsender;
    sptr_ltpnode_   = nullptr;
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
LTPEngine::register_engine(SPtr_LTPCLSenderIF sptr_clsender)
{
    oasys::ScopeLock my_lock(&enginereg_map_lock_,"register_engine");  

    ENGINE_MAP::iterator node_iter = registered_engines_.find(sptr_clsender->Remote_Engine_ID());

    if (node_iter == registered_engines_.end()) {
        SPtr_LTPEngineReg regptr= std::make_shared<LTPEngineReg>(sptr_clsender);

        SPtr_LTPEngine ltp_engine_sptr = BundleDaemon::instance()->ltp_engine();
        SPtr_LTPNode node_ptr = std::make_shared<LTPNode>(ltp_engine_sptr, sptr_clsender);

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
LTPEngine::start_shutdown()
{
    // inform the LTP Nodes to start shutting down
    //   * start discarding all incoming packets
    //   * stop trasnmitting DS sewgs
    //   * cancel all outgoing sessions
    //   * cancel all incoming sessions

    ENGINE_MAP::iterator reg_iter = registered_engines_.begin();
    while (reg_iter != registered_engines_.end()) {
        SPtr_LTPEngineReg regptr = reg_iter->second;
        SPtr_LTPNode node_ptr = regptr->LTP_Node();

        node_ptr->start_shutdown();

        ++reg_iter;
    }
}
    

//----------------------------------------------------------------------
void
LTPEngine::shutdown()
{
    // stop the incoming data processor
    // (interface receiving packets should have already been shutdown)
    if (data_processor_ != nullptr) {
        data_processor_->shutdown();
        data_processor_ = nullptr;
    }


    // shutdown processing for each remote node
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
LTPEngine::post_data(u_char* buffer, size_t length)
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
void
LTPEngine::force_cancel_by_sender(LTPDataSegment* ds_seg)
{
     SPtr_LTPEngineReg engine_entry = lookup_engine(ds_seg->Engine_ID());

     if (engine_entry != nullptr) {
         engine_entry->LTP_Node()->force_cancel_by_sender(ds_seg);
     }
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
             "/dtn/ltp/rcvdata"),
      Thread("LTPEngine::RecvDataProcessor")
{
}

//----------------------------------------------------------------------
LTPEngine::RecvDataProcessor::~RecvDataProcessor()
{
    shutdown();

    LTPDataReceivedEvent* event;

    while (me_eventq_.try_pop(&event)) {
        free(event->data_);
        delete event;
    }
}


//----------------------------------------------------------------------
void
LTPEngine::RecvDataProcessor::shutdown()
{
    // * interface receiving packets should have already been shutdown
    // * any queued events would be deleted anyway so no need to wait for the queue to empty
    set_should_stop();
    while (! is_stopped()) {
        usleep(100000);
    }

    ltp_engine_sptr_.reset();
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


    oasys::ScopeLock l(&eventq_lock_, __func__);

    me_eventq_.push_back(event);

    eventq_bytes_ += len;
    if (eventq_bytes_ > eventq_bytes_max_)
    {
        eventq_bytes_max_ = eventq_bytes_;
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
    int seg_type = 0;
    uint64_t engine_id = 0;
    uint64_t session_id = 0;
    uint64_t last_unknown_engine_id = UINT64_MAX;

    SPtr_LTPEngineReg engptr;

    while (!should_stop()) {
        if (me_eventq_.size() > 0) {
            me_eventq_.try_pop(&event);

            ASSERT(event != nullptr)

            do {
                oasys::ScopeLock l(&eventq_lock_, __func__);

                eventq_bytes_ -= event->len_;
            } while (false);  // just limiting the scopelock



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

                // LTP_Node will delete the event...
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
            engptr.reset();
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }
}





//----------------------------------------------------------------------
LTPNode::LTPNode(SPtr_LTPEngine ltp_engine_sptr, SPtr_LTPCLSenderIF sptr_clsender)
     : Logger("LTPNode", "/dtn/ltp/node/%lu", sptr_clsender->Remote_Engine_ID()),
       Thread("LTPNode")
{
    ltp_engine_sptr_ = ltp_engine_sptr;
    sptr_clsender_ = sptr_clsender;

    sender_sptr_   = std::make_shared<Sender>(ltp_engine_sptr_, this, sptr_clsender_);
    sender_sptr_->start();

    receiver_sptr_ = std::make_shared<Receiver>(this, sptr_clsender_);
    receiver_sptr_->start();

    timer_processor_ = std::unique_ptr<TimerProcessor>(new TimerProcessor(sptr_clsender_->Remote_Engine_ID()));
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

    std::mutex dummy_lock;
    std::condition_variable dummy_cond_var;

    std::unique_lock<std::mutex> qlok(dummy_lock);
    qlok.unlock();


    aos_counter_ = 0;
    while(!should_stop())
    {
        // sleep 1/10th sec
        dummy_cond_var.wait_for(qlok, std::chrono::milliseconds(100));

        if (should_stop()) break;

        if (++sleep_intervals == 10) {
            sleep_intervals = 0;

            if (sptr_clsender_->AOS()) {
                aos_counter_++;
            }
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::start_shutdown()
{
    //   * start discarding all incoming packets
    start_shutting_down_ = true;

    //   * stop trasnmitting DS sewgs
    //   * cancel all outgoing sessions
    sender_sptr_->start_shutdown();

    //   * cancel all incoming sessions
    receiver_sptr_->start_shutdown();
}

//----------------------------------------------------------------------
void
LTPNode::shutdown()
{
    log_always("LTPNode::shutdown called");

    if (shutting_down_) {
        return;
    }

    // output the final LTP statistics
    oasys::StringBuffer buf;
    buf.appendf("\n\nFinal LTP statistics for remote Engine ID: %zu\n",
               sptr_clsender_->Remote_Engine_ID());
    dump_link_stats(&buf);
    buf.appendf("\n\n");
    log_multiline(oasys::LOG_ALWAYS, buf.c_str());

    start_shutting_down_= true;
    shutting_down_ = true;

    sender_sptr_->shutdown();
    receiver_sptr_->shutdown();
    timer_processor_->set_should_stop();


    set_should_stop();
    while (!is_stopped()) {
        usleep(100000);
    }

    ltp_engine_sptr_.reset();
    sender_sptr_.reset();
    receiver_sptr_.reset();
    timer_processor_.reset();
}

//----------------------------------------------------------------------
void
LTPNode::reconfigured()
{
    sender_sptr_->reconfigured();
    receiver_sptr_->reconfigured();
}

//----------------------------------------------------------------------
void
LTPNode::force_cancel_by_sender(LTPDataSegment* ds_seg)
{
    sender_sptr_->force_cancel_by_sender(ds_seg);
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
LTPNode::Process_Sender_Segment(LTPSegment* seg_ptr, bool closed, bool cancelled)
{
    sender_sptr_->Process_Segment(seg_ptr, closed, cancelled);
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
    buf->appendf("          Sessions  /  Segments    Sessions  /  Segments   Rcvd Total   But Got     But Got    Expird inQ   Failure   \n");
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
LTPNode::Post_Timer_To_Process(oasys::SPtr_Timer timer)
{
    if (! should_stop()) {
        if (timer_processor_) {
            timer_processor_->post(timer);
        }
    }
}

//----------------------------------------------------------------------
LTPNode::Receiver::Receiver(LTPNode* parent_node, SPtr_LTPCLSenderIF sptr_clsender) :
      Logger("LTPNode::Receiver",
             "/dtn/ltp/node/%lu/rcvr", sptr_clsender->Remote_Engine_ID()),
      Thread("LTPNode::Receiver"),
      sptr_clsender_(sptr_clsender),
      parent_node_(parent_node)
{
    memset(&stats_, 0, sizeof(stats_));

    seg_processor_ = QPtr_RecvSegProcessor(new RecvSegProcessor(this, sptr_clsender_->Remote_Engine_ID()));
    seg_processor_->start();

    bundle_processor_ = QPtr_RecvBundleProcessor(new RecvBundleProcessor(this, sptr_clsender_->Remote_Engine_ID()));
    bundle_processor_->start();

    sptr_blank_session_ = std::make_shared<LTPSession>(0UL, 0UL, false);
    sptr_blank_session_->Set_Inbound_Cipher_Suite(sptr_clsender_->Inbound_Cipher_Suite());
    sptr_blank_session_->Set_Inbound_Cipher_Key_Id(sptr_clsender_->Inbound_Cipher_Key_Id());
    sptr_blank_session_->Set_Inbound_Cipher_Engine(sptr_clsender_->Inbound_Cipher_Engine());
    sptr_blank_session_->Set_Outbound_Cipher_Suite(sptr_clsender_->Outbound_Cipher_Suite());
    sptr_blank_session_->Set_Outbound_Cipher_Key_Id(sptr_clsender_->Outbound_Cipher_Key_Id());
    sptr_blank_session_->Set_Outbound_Cipher_Engine(sptr_clsender_->Outbound_Cipher_Engine());

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
LTPNode::Receiver::start_shutdown()
{
    // signal to start discarding all segments
    start_shutting_down_ = true;

    // signal seg processor to start discarding queued segments
    seg_processor_->start_shutdown();

    cancel_all_sessions();

    // leave the bundle processor chugging away at this point
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::shutdown()
{
    if (shutting_down_) {
        return;
    }

    // these flags prevent procesing of any new incoming data 
    start_shutting_down_ = true;
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

    seg_processor_.reset();
    bundle_processor_.reset();
    sptr_clsender_.reset();
    sptr_blank_session_.reset();
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::reconfigured()
{
    seg_size_            = sptr_clsender_->Seg_Size();
    retran_retries_      = sptr_clsender_->Retran_Retries();
    retran_interval_     = sptr_clsender_->Retran_Intvl();
    inactivity_interval_ = sptr_clsender_->Inactivity_Intvl();

    seg_processor_->reconfigured();
    bundle_processor_->reconfigured();
}


//----------------------------------------------------------------------
void
LTPNode::Receiver::post_data(LTPDataReceivedEvent* event)
{
    if (start_shutting_down_) {
        free(event->data_);
        delete event;
        return;
    }

    int32_t recv_test = sptr_clsender_->Recv_Test();
    bool okay_to_process = true;

    if (recv_test > 0) {
        okay_to_process = ((packets_received_ % (uint32_t)recv_test) != (uint32_t)(recv_test - 1));
    }
    ++packets_received_;


    if (okay_to_process) {
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

                me_eventq_.push_back(event);

                eventq_bytes_ += event->len_;
                if (eventq_bytes_ > eventq_bytes_max_)
                {
                    eventq_bytes_max_ = eventq_bytes_;
                }
                break;
        }

    } else {
        ++packets_dropped_for_recv_test_;
        free(event->data_);
        delete event;
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::run()
{
    char threadname[16] = "LTPNodeRcvr";
    pthread_setname_np(pthread_self(), threadname);

    LTPDataReceivedEvent* event;

    while (!should_stop()) {
        if (me_eventq_.size() > 0) {
            me_eventq_.try_pop(&event);

            ASSERT(event != nullptr)

            process_segments_for_sender(event->data_, event->len_, 
                                        event->closed_, event->cancelled_);

            oasys::ScopeLock l(&eventq_lock_, __func__);

            eventq_bytes_ -= event->len_;

            free(event->data_);
            delete event;
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }
}

//----------------------------------------------------------------------
bool
LTPNode::Receiver::RSIF_shutting_down()
{
    return shutting_down_;
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RSIF_post_timer_to_process(oasys::SPtr_Timer event)
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
LTPNode::Receiver::RSIF_retransmit_timer_triggered(int event_type, SPtr_LTPSegment sptr_retran_seg)
{
    if (event_type == LTP_EVENT_RS_TO) {
        report_seg_timeout(sptr_retran_seg);
    } else if (event_type == LTP_EVENT_CS_TO) {
        cancel_seg_timeout(sptr_retran_seg);
    } else {
        log_err("Unexpected RetransmitSegTimer triggerd: %s", LTPSegment::Get_Event_Type(event_type));
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RSIF_inactivity_timer_triggered(uint64_t engine_id, uint64_t session_id)
{
    SPtr_LTPSession sptr_session = find_incoming_session(engine_id, session_id);

    if (sptr_session != nullptr) {
        if (sptr_session->Session_State() == LTPSession::LTP_SESSION_STATE_DS) {
            time_t time_left = sptr_session->Last_Packet_Time() +  inactivity_interval_;
            time_left = time_left - parent_node_->AOS_Counter();

            if (time_left > 0)
            {
                SPtr_LTPNodeRcvrSndrIF rcvr_rsif = parent_node_->receiver_rsif_sptr();
                sptr_session->Start_Inactivity_Timer(rcvr_rsif, time_left);
            } else {
                //log_debug("%s - Inactivity Timeout cancelling session",
                //           sptr_session->key_str().c_str());

                sptr_session->RemoveSegments();   
                build_CS_segment(sptr_session.get(), LTP_SEGMENT_CS_BR, 
                                 LTPCancelSegment::LTP_CS_REASON_RXMTCYCEX);
            }
        } else {
            //log_debug("%s): %s - Inactivity Timeout ignored - session no longer in DS state - state = %s red processed = %s",
            //           __func__, sptr_session->key_str().c_str(), sptr_session->Get_Session_State(),
            //           sptr_session->RedProcessed()?"true":"false");
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RSIF_closeout_timer_triggered(uint64_t engine_id, uint64_t session_id)
{
    //(void) engine_id;
    ASSERT(engine_id == sptr_clsender_->Remote_Engine_ID());

    // remove from closed sessions
    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    closed_session_map_.erase(session_id);
}


//----------------------------------------------------------------------
void
LTPNode::Receiver::report_seg_timeout(SPtr_LTPSegment sptr_ltp_seg)
{ 
    if (!sptr_ltp_seg->IsDeleted()) {
        // Okay to resend segment if retries not exceeded

        SPtr_LTPSession sptr_session = find_incoming_session(sptr_ltp_seg->Engine_ID(), sptr_ltp_seg->Session_ID());

        if (sptr_session && !sptr_session->Is_LTP_Cancelled())  {
            // for logging
            std::string key_str = sptr_session->key_str();

            SPtr_LTPReportSegment rs_seg = std::dynamic_pointer_cast<LTPReportSegment>(sptr_ltp_seg);

            if (rs_seg != nullptr) {

                rs_seg->Increment_Retries();

                if (rs_seg->Retrans_Retries() <= retran_retries_) {
                    SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();

                    SPtr_LTPTimer timer;
                    timer = rs_seg->Renew_Retransmission_Timer(rcvr_if, LTP_EVENT_RS_TO, retran_interval_, sptr_ltp_seg);

                    ++stats_.rs_segment_resends_;

                    sptr_clsender_->Send_Admin_Seg_Highest_Priority(rs_seg->asNewString(), timer, true);
                } else {
                    rs_seg->Set_Deleted();

                    if (sptr_session->DataProcessed()) {
                        // No RAS received but we did extract the bundles so we can just delete the session
                        // - peer will either cancel on their side or they sent the RAS and it did not make it here
                        if (sptr_session->RedProcessed()) {
                            ++stats_.RAS_not_received_but_got_bundles_;
                        }
                        erase_incoming_session(sptr_session.get());
                    } else if (!sptr_session->Is_LTP_Cancelled()) {
                        // Exceeded retries - start cancelling
                        //log_debug("Cancelling by receiver - LTP Session: %s because RS retransmission retries exceeded",
                        //         sptr_session->key_str().c_str());

                        build_CS_segment(sptr_session.get(), LTP_SEGMENT_CS_BR, 
                                         LTPCancelSegment::LTP_CS_REASON_RLEXC);
                    }
                }
            } else {
                log_err("RS timeout - error casting seg to rs_seg!");
            }
        } else {
            //log_debug("RS timeout - ignoring - session not found");
        }
    } else {
        //log_debug("RS timeout - Session: %s - ignoring since flagged as deleted", 
        //           sptr_ltp_seg->session_key_str().c_str());
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::cancel_seg_timeout(SPtr_LTPSegment sptr_ltp_seg)
{ 
    if (!sptr_ltp_seg->IsDeleted()) {
        // Okay to resend segment if retries not exceeded

        SPtr_LTPSession sptr_session = find_incoming_session(sptr_ltp_seg->Engine_ID(), sptr_ltp_seg->Session_ID());

        if (sptr_session != nullptr ) {
            // for logging
            std::string key_str = sptr_session->key_str();

            SPtr_LTPCancelSegment cs_seg = std::dynamic_pointer_cast<LTPCancelSegment>(sptr_ltp_seg);

            if (cs_seg != nullptr) {

                cs_seg->Increment_Retries();

                if (cs_seg->Retrans_Retries() <= retran_retries_) {
                    SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();

                    SPtr_LTPTimer timer;
                    timer = cs_seg->Renew_Retransmission_Timer(rcvr_if, LTP_EVENT_CS_TO, retran_interval_, sptr_ltp_seg);

                    ++stats_.cancel_by_rcvr_segs_;

                    sptr_clsender_->Send_Admin_Seg_Highest_Priority(cs_seg->asNewString(), timer, false);
                } else {
                    cs_seg->Set_Deleted();

                    erase_incoming_session(sptr_session.get());
                }
            } else {
                log_err("CS timeout - error casting seg to cs_seg!");
            }
        } else {
            //log_debug("CS timeout - ignoring - session not found");
        }
    } else {
        //log_debug("CS timeout - Session: %s - ignoring since flagged as deleted", sptr_ltp_seg->session_key_str().c_str());
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


    if (sptr_clsender_->Dump_Sessions() && (incoming_sessions_.size() > 0)) {
        buf->append("Receiver Session List:\n");

        SPtr_LTPSession sptr_session;
        SESS_SPTR_MAP::iterator iter = incoming_sessions_.begin();

        while (iter != incoming_sessions_.end())
        {
            sptr_session = iter->second;

            buf->appendf("    %s [%s]  red segs: %zu  rcvd bytes: %zu of %zu  EOB rcvd: %s  contig blocks: %zu\n",
                         sptr_session->key_str().c_str(), sptr_session->Get_Session_State(),
                         sptr_session->Red_Segments()->size(),
                         sptr_session->Red_Bytes_Received(),
                         sptr_session->Expected_Red_Bytes(),
                         sptr_session->Is_EOB_Defined()?"true":"false",
                         sptr_session->get_num_red_contiguous_bloks());

            ++iter;
        }
        buf->append("\n");
    }


    size_t seg_queue_size = 0;
    size_t seg_bytes_queued = 0;
    size_t seg_bytes_queued_max = 0;
    size_t seg_bytes_quota = 0;
    size_t seg_ds_discards = 0;

    size_t bundle_queue_size = 0;
    size_t bundle_bytes_queued = 0;
    size_t bundle_bytes_queued_max = 0;
    size_t bundle_quota = 0;

    seg_processor_->get_queue_stats(seg_queue_size, seg_bytes_queued, seg_bytes_queued_max, seg_bytes_quota, seg_ds_discards);
    bundle_processor_->get_queue_stats(bundle_queue_size, bundle_bytes_queued, bundle_bytes_queued_max, bundle_quota);

    buf->appendf("Receiver threads: SegProcessor: queued: %zu  bytes: %zu (%s)  max bytes: %zu (%s)   quota: %zu (%s)  DS discards: %zu\n"
                 "               BundleProcessor: queued: %zu  bytes: %zu (%s)  max bytes: %zu (%s)\n",
                 seg_queue_size, 
                 seg_bytes_queued, 
                 FORMAT_WITH_MAG(seg_bytes_queued).c_str(),
                 seg_bytes_queued_max,
                 FORMAT_WITH_MAG(seg_bytes_queued_max).c_str(),
                 seg_bytes_quota, FORMAT_WITH_MAG(seg_bytes_quota).c_str(), seg_ds_discards,

                 bundle_queue_size, 
                 bundle_bytes_queued, 
                 FORMAT_WITH_MAG(bundle_bytes_queued).c_str(),
                 bundle_bytes_queued_max,
                 FORMAT_WITH_MAG(bundle_bytes_queued_max).c_str());


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
                 stats_.ds_sessions_with_resends_, stats_.total_ds_unique_, stats_.total_ds_duplicate_,
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
               //          Sessions  /  Segments    Sessions  /  Segments   Rcvd Total   But Got     But Got    Expird inQ   Failure
               //         -----------------------  -----------------------  ----------  ----------  ----------  ----------  ----------
               //Sender   1234567890 / 1234567890  1234567890 / 1234567890  1234567890  1234567890  1234567890  1234567890  1234567890
               //Receiver 1234567890 / 1234567890  1234567890 / 1234567890  1234567890  1234567890  1234567890  1234567890  1234567890
    buf->appendf("Receiver %10" PRIu64 " / %10" PRIu64 "  %10" PRIu64 " / %10" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "  %10" PRIu64 "\n",
                 stats_.cancel_by_sndr_sessions_, stats_.cancel_by_sndr_segs_, 
                 stats_.cancel_by_rcvr_sessions_, stats_.cancel_by_rcvr_segs_, stats_.total_sent_and_rcvd_ca_, 
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
LTPNode::Receiver::update_session_counts(LTPSession* session_ptr, int new_state)
{
    int old_state = session_ptr->Session_State();

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

        session_ptr->Set_Session_State(new_state); 
    }
}

//----------------------------------------------------------------------
void 
LTPNode::Receiver::build_CS_segment(LTPSession* session_ptr, int segment_type, u_char reason_code)
{
    SPtr_LTPCancelSegment cancel_segment = std::make_shared<LTPCancelSegment>(session_ptr, segment_type, reason_code);

    if (!session_ptr->Is_LTP_Cancelled()) {
        if (session_ptr->RedProcessed()) {
          ++stats_.session_cancelled_but_got_it_;
        }
        ++stats_.cancel_by_rcvr_sessions_;

        update_session_counts(session_ptr, LTPSession::LTP_SESSION_STATE_CS); 

        SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();

        SPtr_LTPSegment sptr_ltp_seg = cancel_segment;
        SPtr_LTPTimer timer;
        timer = cancel_segment->Create_Retransmission_Timer(rcvr_if, LTP_EVENT_CS_TO, retran_interval_, sptr_ltp_seg);

        session_ptr->Set_Cancel_Segment(cancel_segment);

        ++stats_.cancel_by_rcvr_segs_;

        sptr_clsender_->Send_Admin_Seg_Highest_Priority(cancel_segment->asNewString(), timer, false);
    }
    // else already in the process of sending the cancel segment
}

//----------------------------------------------------------------------
void 
LTPNode::Receiver::build_CS_segment(LTPSegment* seg_ptr, int segment_type, u_char reason_code)
{
    // this is a one shot cancel after receving a message for an unknown session
    SPtr_LTPCancelSegment cancel_segment = std::make_shared<LTPCancelSegment>(seg_ptr, segment_type, reason_code);

    ++stats_.cancel_by_rcvr_segs_;

    SPtr_LTPTimer dummy_timer;
    sptr_clsender_->Send_Admin_Seg_Highest_Priority(cancel_segment->asNewString(), dummy_timer, false);
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::cancel_all_sessions()
{
    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    SPtr_LTPSession sptr_session;
    SESS_SPTR_MAP::iterator iter = incoming_sessions_.begin();

    size_t num_cancelled = 0;

    while (iter != incoming_sessions_.end())
    {
        sptr_session = iter->second;

        if (!sptr_session->Is_LTP_Cancelled()) {
            build_CS_segment(sptr_session.get(), LTP_SEGMENT_CS_BR, 
                             LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
            ++num_cancelled;
        }

        ++iter;
    }

    log_always("LTPEngine::Receiver(%zu) cancelled %zu sesssions while shutting down",
               Remote_Engine_ID(), num_cancelled);
}

//----------------------------------------------------------------------
void 
LTPNode::Receiver::build_CAS_for_CSS(LTPCancelSegment* seg_ptr)
{
    LTPCancelAckSegment* cas_seg = new LTPCancelAckSegment(seg_ptr, LTPSEGMENT_CAS_BS_BYTE, sptr_blank_session_.get()); 

    SPtr_LTPTimer dummy_timer;   // no retransmits
    sptr_clsender_->Send_Admin_Seg_Highest_Priority(cas_seg->asNewString(), dummy_timer, false);
    delete cas_seg;

    ++stats_.total_sent_and_rcvd_ca_;
}

//----------------------------------------------------------------------
SPtr_LTPSession
LTPNode::Receiver::find_incoming_session(LTPSegment* seg_ptr, bool create_flag, 
                                         bool& closed, size_t& closed_session_size, bool& cancelled)
{
    closed = false;
    cancelled = false;
    closed_session_size = 0;

    SPtr_LTPSession sptr_session;

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    CLOSED_SESSION_MAP::iterator closed_iter = closed_session_map_.find(seg_ptr->Session_ID());
    if (closed_iter != closed_session_map_.end()) {
        closed = true;
        closed_session_size = closed_iter->second;
        cancelled = (closed_session_size == 0);
    }

    if (!closed) {
        sptr_session = find_incoming_session(seg_ptr->Engine_ID(), seg_ptr->Session_ID());

        if (sptr_session == nullptr) {
            if (create_flag) {
                sptr_session = std::make_shared<LTPSession>(seg_ptr->Engine_ID(), seg_ptr->Session_ID());

                sptr_session->Set_Inbound_Cipher_Suite(sptr_clsender_->Inbound_Cipher_Suite());
                sptr_session->Set_Inbound_Cipher_Key_Id(sptr_clsender_->Inbound_Cipher_Key_Id());
                sptr_session->Set_Inbound_Cipher_Engine(sptr_clsender_->Inbound_Cipher_Engine());
                sptr_session->Set_Outbound_Cipher_Suite(sptr_clsender_->Outbound_Cipher_Suite());
                sptr_session->Set_Outbound_Cipher_Key_Id(sptr_clsender_->Outbound_Cipher_Key_Id());
                sptr_session->Set_Outbound_Cipher_Engine(sptr_clsender_->Outbound_Cipher_Engine());

                if (sptr_clsender_->Use_Files_Recv()) {
                    bool run_with_disk_io_kludges = sptr_clsender_->Use_DiskIO_Kludge();

                    sptr_session->set_file_usage(sptr_clsender_->Dir_Path(), run_with_disk_io_kludges);
                }

                incoming_sessions_[sptr_session.get()] = sptr_session;

                if (incoming_sessions_.size() > stats_.max_sessions_) {
                    stats_.max_sessions_ = incoming_sessions_.size();
                }
                ++stats_.total_sessions_;
            }
        }
    }

    return sptr_session;
}

//----------------------------------------------------------------------
SPtr_LTPSession
LTPNode::Receiver::find_incoming_session(uint64_t engine_id, uint64_t session_id)
{
    SPtr_LTPSession sptr_session;

    QPtr_LTPSessionKey qkey = QPtr_LTPSessionKey(new LTPSessionKey(engine_id, session_id));

    SESS_SPTR_MAP::iterator iter; 

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    iter = incoming_sessions_.find(qkey.get());
    if (iter != incoming_sessions_.end())
    {
        sptr_session = iter->second;
    }

    return sptr_session;
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::erase_incoming_session(LTPSession* session_ptr)
{

    SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();
    session_ptr->Start_Closeout_Timer(rcvr_if, inactivity_interval_);

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    incoming_sessions_.erase(session_ptr);

    uint64_t session_size = 0;  // 0 indicates session was cancelled
    if (!session_ptr->Is_LTP_Cancelled()) {
        session_size = session_ptr->Red_Bytes_Received();
    }

    // this changes the session state so it must be after checking for cancelled
    update_session_counts(session_ptr, LTPSession::LTP_SESSION_STATE_UNDEFINED); 

    closed_session_map_[session_ptr->Session_ID()] = session_size;

    if (closed_session_map_.size() > max_closed_sessions_) {
        max_closed_sessions_ = closed_session_map_.size();
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::cleanup_RS(uint64_t engine_id, uint64_t session_id, uint64_t report_serial_number, 
                                             int event_type)
{ 
    SPtr_LTPSession sptr_session = find_incoming_session(engine_id, session_id);

    if (sptr_session != nullptr) {

        // for logging
        std::string key_str = sptr_session->key_str();

        LTP_RS_MAP::iterator rs_iterator = sptr_session->Report_Segments().find(report_serial_number);

        if (rs_iterator != sptr_session->Report_Segments().end())
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

            sptr_session->Report_Segments().erase(rs_iterator);

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
LTPNode::Receiver::generate_RS(LTPSession* session_ptr, LTPDataSegment* ds_seg)
{
    int32_t segsize = seg_size_;
    size_t bytes_claimed = 0;

    if (session_ptr->HaveReportSegmentsToSend(ds_seg, segsize, bytes_claimed))
    {
        uint64_t chkpt = ds_seg->Checkpoint_ID();

        SPtr_LTPReportSegment rptseg;

        LTP_RS_MAP::iterator iter;

        for (iter =  session_ptr->Report_Segments().begin();
            iter != session_ptr->Report_Segments().end();
            iter++)
        {
            rptseg = iter->second;

            if (chkpt == rptseg->Checkpoint_ID())
            {
                update_session_counts(session_ptr, LTPSession::LTP_SESSION_STATE_RS); 

                // send or resend this report segment
                SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();

                SPtr_LTPSegment sptr_ltp_seg = rptseg;

                if (bytes_claimed < rptseg->UpperBounds()) {
                    if (!session_ptr->has_ds_resends()) {
                        ++stats_.ds_sessions_with_resends_;
                        session_ptr->set_has_ds_resends();
                    }
                }

                // if this is actually a resend of the report seg due to receiving a duplicate
                // DS checkpoint then a timer will not be created again
                SPtr_LTPTimer timer;
                timer = rptseg->Create_Retransmission_Timer(rcvr_if, LTP_EVENT_RS_TO, retran_interval_, sptr_ltp_seg);


                ++stats_.total_rs_segs_generated_;
                sptr_clsender_->Send_Admin_Seg_Highest_Priority(rptseg->asNewString(), timer, true);
                session_ptr->inc_reports_sent();
            }
            else
            {
                //log_debug("generateRS - Session: %s skip ReportSeg for Checkpoint ID: %lu", 
                //           session_ptr->key_str().c_str(), rptseg->Checkpoint_ID());
            }
        }
    }
    else
    {
        //log_debug("No RS to send for DS: %s-%zu  Ckpt ID: %zu", 
        //           ds_seg->session_key_str().c_str(), ds_seg->Offset(),
        //           ds_seg->Checkpoint_ID());
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::process_segments_for_sender(u_char* bp, size_t len, bool closed, bool cancelled)
{
    if (shutting_down_) {
        return;
    }

    if (sptr_clsender_->Hex_Dump()) {
        oasys::HexDumpBuffer hex;
        int plen = len > 30 ? 30 : len;
        hex.append(bp, plen);
        log_always("%s: Buffer (%d of %zu):", __func__, plen, len);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }

    LTPSegment* seg_ptr = LTPSegment::DecodeBuffer(bp, len);

    if (!seg_ptr->IsValid()){
        log_err("%s: invalid segment.... type:%s length:%d", 
                 __func__, LTPSegment::Get_Segment_Type(seg_ptr->Segment_Type()),(int) len);
        delete seg_ptr;
        return;
    }

    //
    // Send over to Sender if it's any of these three SEGMENT TYPES
    //
    if (seg_ptr->Segment_Type() == LTP_SEGMENT_RS     || 
        seg_ptr->Segment_Type() == LTP_SEGMENT_CS_BR  ||
        seg_ptr->Segment_Type() == LTP_SEGMENT_CAS_BS) { 

        parent_node_->Process_Sender_Segment(seg_ptr, closed, cancelled);

    } else {
        log_err("%s: received unexpected segment type: %s", __func__, seg_ptr->Get_Segment_Type());
    }

    delete seg_ptr; // we are done with this segment
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

    if (sptr_clsender_->Hex_Dump()) {
        oasys::HexDumpBuffer hex;
        int plen = len > 30 ? 30 : len;
        hex.append(bp, plen);
        log_always("process_data: Buffer (%d of %zu):", plen, len);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }

    LTPSegment* seg_ptr = LTPSegment::DecodeBuffer(bp, len);

    if (!seg_ptr->IsValid()){
        log_err("process_data: invalid segment.... type:%s length:%d", 
                 LTPSegment::Get_Segment_Type(seg_ptr->Segment_Type()),(int) len);
        delete seg_ptr;
        return;
    }

    //log_debug("process_data: Session: %" PRIu64 "-%" PRIu64 " - %s received",
    //          seg_ptr->Engine_ID(), seg_ptr->Session_ID(), LTPSegment::Get_Segment_Type(seg_ptr->Segment_Type()));

    // try to find the current session from rcvr_sessions...
    bool closed = false;
    bool cancelled = false;
    size_t closed_session_size = 0;
    bool create_flag = (seg_ptr->Segment_Type() == LTP_SEGMENT_DS);
    SPtr_LTPSession sptr_session = find_incoming_session(seg_ptr, create_flag, closed, closed_session_size, cancelled);

    if (closed) {
        // this session was already closed and possibly cancelled
        // - send a reply to let the sender know
        if (seg_ptr->Segment_Type() == LTP_SEGMENT_CS_BS) {
            ++stats_.cancel_by_sndr_segs_;
            build_CAS_for_CSS((LTPCancelSegment*) seg_ptr);
        }
        else if (seg_ptr->Segment_Type() == LTP_SEGMENT_DS) {
            if (cancelled) {
                build_CS_segment(seg_ptr, LTP_SEGMENT_CS_BR, 
                                 LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
            } else {
                LTPDataSegment* ds_seg = (LTPDataSegment*)seg_ptr;

                if (ds_seg->IsCheckpoint()) {
                    // build a simple report segment with 1 claim for the entire session
                    // generate simple "got it all" report segment
                    LTP_RC_MAP claim_map;
                    size_t lower_bounds = 0;

                    SPtr_LTPReportClaim rc;
                    rc = std::make_shared<LTPReportClaim>(lower_bounds, closed_session_size);
                    claim_map[rc->Offset()] = rc;

                    SPtr_LTPReportSegment sptr_rs;
                    sptr_rs = std::make_shared<LTPReportSegment>(ds_seg->Engine_ID(), ds_seg->Session_ID(), 
                                                                 ds_seg->Checkpoint_ID(), lower_bounds, 
                                                                 closed_session_size, closed_session_size, claim_map);

                    SPtr_LTPTimer timer;
                    ++stats_.total_rs_segs_generated_;
                    sptr_clsender_->Send_Admin_Seg_Highest_Priority(sptr_rs->asNewString(), timer, true);
                    sptr_rs.reset();
                }
            }
        }
        // else
        // LTP_SEGMENT_RAS - no reply needed
        // LTP_SEGMENT_CAS_BR - no reply needed
        // LTP_SEGMENT_DS - ignore since already exceeded RS retransmits and possibly CSS retransmits
        
        delete seg_ptr;
        return;
    }

    if (sptr_session == nullptr) {
        // probably started up while peer was sending cancel by sender segments
        if (seg_ptr->Segment_Type() == LTP_SEGMENT_CS_BS) {
            build_CAS_for_CSS((LTPCancelSegment*) seg_ptr);
        }

        delete seg_ptr;
        return;
    }

    if (sptr_session->Is_LTP_Cancelled()) {
        // this session is already in a cancellin state
        if (seg_ptr->Segment_Type() == LTP_SEGMENT_CS_BS) {
            ++stats_.cancel_by_sndr_segs_;
            build_CAS_for_CSS((LTPCancelSegment*) seg_ptr);
        } else if (seg_ptr->Segment_Type() != LTP_SEGMENT_CAS_BR) {
              ++stats_.total_sent_and_rcvd_ca_;
              delete seg_ptr;
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
    
    if (seg_ptr->Segment_Type() == LTP_SEGMENT_DS) {
        process_incoming_data_segment(sptr_session.get(), seg_ptr);

    } else if (seg_ptr->Segment_Type() == LTP_SEGMENT_RAS)  { 
         // Report Ack Segment
         ++stats_.total_rcv_ra_;

        LTPReportAckSegment* ras_seg = (LTPReportAckSegment *) seg_ptr;

        // cleanup the Report Segment list so we don't keep transmitting.
        cleanup_RS(seg_ptr->Engine_ID(), seg_ptr->Session_ID(), ras_seg->Report_Serial_Number(), LTP_EVENT_RAS_RECEIVED); 
        delete seg_ptr;

        if (sptr_session->DataProcessed()) {
            erase_incoming_session(sptr_session.get());
        } else {
            // got the RAS so now waiting for the missing DSs
            update_session_counts(sptr_session.get(), LTPSession::LTP_SESSION_STATE_DS); 
            sptr_session->Set_Last_Packet_Time(parent_node_->AOS_Counter());
            if (!sptr_session->Has_Inactivity_Timer()) {
                SPtr_LTPNodeRcvrSndrIF rcvr_if = parent_node_->receiver_rsif_sptr();
                sptr_session->Start_Inactivity_Timer(rcvr_if, inactivity_interval_);
            }
        }

    } else if (seg_ptr->Segment_Type() == LTP_SEGMENT_CS_BS) { 
        // Cancel Segment from Block Sender
        ++stats_.cancel_by_sndr_sessions_;
        ++stats_.cancel_by_sndr_segs_;

        log_debug("Received cancel by sender - LTP Session: %s",
                  seg_ptr->session_key_str().c_str());

        update_session_counts(sptr_session.get(), LTPSession::LTP_SESSION_STATE_CS); 

        build_CAS_for_CSS((LTPCancelSegment*) seg_ptr);
        delete seg_ptr;

        // clean up the session
        sptr_session->RemoveSegments();
        erase_incoming_session(sptr_session.get());

    } else if (seg_ptr->Segment_Type() == LTP_SEGMENT_CAS_BR) { 
        // Cancel-acknowledgment Segment from Block Receiver
        ++stats_.total_sent_and_rcvd_ca_;

        sptr_session->Clear_Cancel_Segment();
        erase_incoming_session(sptr_session.get());
        delete seg_ptr;
    } else {
        log_err("process_data - Unhandled segment type received: %d", seg_ptr->Segment_Type());
        delete seg_ptr;
    }
}


//----------------------------------------------------------------------
void
LTPNode::Receiver::process_incoming_data_segment(LTPSession* session_ptr, LTPSegment* seg_ptr)
{
    ++stats_.total_rcv_ds_;

//dzdebug -- allow HDTN to use full 64-bit session IDs
//dzdebug
//dzdebug    if (seg_ptr->Session_ID() > ULONG_MAX)
//dzdebug    {
//dzdebug        log_err("%s: received %zu:%zu DS with invalid Session ID - exceeds ULONG_MAX", 
//dzdebug                  __func__, session_ptr->Engine_ID(), session_ptr->Session_ID());
//dzdebug
//dzdebug        build_CS_segment(session_ptr, LTP_SEGMENT_CS_BR, 
//dzdebug                         LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
//dzdebug        delete seg_ptr;
//dzdebug        return;
//dzdebug    }

    if (seg_ptr->Service_ID() != 1 && seg_ptr->Service_ID() != 2)
    {
        if (seg_ptr->IsRed()) {
            log_err("%s: calling build_CS_Segment received %zu:%zu DS with invalid Service ID: %d",
                      __func__, session_ptr->Engine_ID(), session_ptr->Session_ID(), seg_ptr->Service_ID());

            // cancel this session
            build_CS_segment(session_ptr, LTP_SEGMENT_CS_BR, 
                             LTPCancelSegment::LTP_CS_REASON_UNREACHABLE);
        } else {
            log_err("%s: received %zu:%zu Green DS with invalid Service ID: %d", 
                    __func__, session_ptr->Engine_ID(), session_ptr->Session_ID(), seg_ptr->Service_ID());
            erase_incoming_session(session_ptr);
        }

        delete seg_ptr;
        return; 
    }


    update_session_counts(session_ptr, LTPSession::LTP_SESSION_STATE_DS); 

    session_ptr->Set_Last_Packet_Time(parent_node_->AOS_Counter());

    if (!session_ptr->Has_Inactivity_Timer()) {
        SPtr_LTPNodeRcvrSndrIF rcvr_rsif = parent_node_->receiver_rsif_sptr();
        session_ptr->Start_Inactivity_Timer(rcvr_rsif, inactivity_interval_);
    }

    LTPDataSegment* ds_seg = static_cast<LTPDataSegment*>(seg_ptr);

    if (session_ptr->DataProcessed()) {
        ++stats_.total_ds_duplicate_;

        if (ds_seg->IsCheckpoint())
        {
            session_ptr->Set_Checkpoint_ID(ds_seg->Checkpoint_ID());
            generate_RS(session_ptr, ds_seg);
        }

        delete seg_ptr;
        return;
    }

    SPtr_LTPDataSegment sptr_ds_seg(ds_seg);  // sptr_ds_seg now controls lifetime of the "seg"

    int add_result = session_ptr->AddSegment_incoming(sptr_ds_seg);

    if (-1 == add_result) {
       // not an "error" - just duplicate data

        ++stats_.total_ds_duplicate_;

        if (ds_seg->IsCheckpoint())
        {
            session_ptr->Set_Checkpoint_ID(ds_seg->Checkpoint_ID());
            generate_RS(session_ptr, ds_seg);
        }

        return;
    }

    // Must check for disk error after calling AddSegment
    // to determine if the session needs to be cancelled 
    // -99 indicates there was a non-recoverable error
    if ((-99 == add_result) && session_ptr->abort_due_to_disk_io_error()) {
        log_crit("Cancelling session %s-%zu due to disk I/O error", 
                 session_ptr->key_str().c_str(), ds_seg->Offset());

        build_CS_segment(session_ptr, LTP_SEGMENT_CS_BR,
                         LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
        return;
    }

    ++stats_.total_ds_unique_;


    if (ds_seg->IsEndofblock()) {
        session_ptr->Set_EOB(ds_seg);
    }

    size_t red_bytes_to_process = session_ptr->IsRedFull();
    bool ok_to_accept_bundle = true;

    if (red_bytes_to_process > 0) {
        if (!bundle_processor_->okay_to_queue()) {
            log_err("Cancelling LTP Session: %s because BundleProcessor Queue is over quota",
                    session_ptr->key_str().c_str());

            ok_to_accept_bundle = false;
        } else {
            //check payload quota to see if we can accept the bundle(s)
            BundleStore* bs = BundleStore::instance();
            if (!bs->try_reserve_payload_space(red_bytes_to_process)) {
                ok_to_accept_bundle = false;
                // rejecting bundle due to storage depletion
                log_err("LTPNode::Receiver: rejecting bundle due to storage depletion - session: %zu:%zu bytes: %zu",
                         session_ptr->Engine_ID(), session_ptr->Session_ID(), red_bytes_to_process);
            }
            // else space is reserved and must be managed in the bundle extraction process
        }
    }

    if (!ok_to_accept_bundle)
    {
        // Cancel Session due to storage depletion
        build_CS_segment(session_ptr, LTP_SEGMENT_CS_BR,
                         LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
        return;
    }
    else if (ds_seg->IsCheckpoint())
    {
        session_ptr->Set_Checkpoint_ID(ds_seg->Checkpoint_ID());

        generate_RS(session_ptr, ds_seg);
    }


    if (red_bytes_to_process > 0) {
                //log_always("LTPNode::Receiver: got all data - session: %zu:%zu bytes: %zu",
                //         session_ptr->Engine_ID(), session_ptr->Session_ID(), red_bytes_to_process);

        if (session_ptr->using_file()) {
            std::string file_path;
            session_ptr->GetRedDataFile(file_path);
            int sid = ds_seg->Service_ID();
            bundle_processor_->post(file_path, red_bytes_to_process, ((sid == 2) ? true : false) );
        } else {
            SPtr_LTP_DS_MAP red_segs = session_ptr->GetAllRedData();
            int sid = ds_seg->Service_ID();
            bundle_processor_->post(red_segs, red_bytes_to_process, ((sid == 2) ? true : false) );
        }
    }


    size_t green_bytes_to_process = session_ptr->IsGreenFull();
    if (green_bytes_to_process > 0) {
        //check payload quota to see if we can accept the bundle(s)
        BundleStore* bs = BundleStore::instance();
        if (bs->try_reserve_payload_space(green_bytes_to_process)) {
            SPtr_LTP_DS_MAP green_segs = session_ptr->GetAllGreenData();
            int sid = ds_seg->Service_ID();
            bundle_processor_->post(green_segs, green_bytes_to_process, ((sid == 2) ? true : false) );

        } else {
            // rejecting bundle due to storage depletion
            log_warn("LTPNode::Receiver: rejecting green bundle due to storage depletion - session: %zu:%zu bytes: %zu",
                     session_ptr->Engine_ID(), session_ptr->Session_ID(), green_bytes_to_process);
        }

        // CCSDS BP Specification - LTP Sessions must be all red or all green so we can now delete this session
        erase_incoming_session(session_ptr);
    }

}





//----------------------------------------------------------------------
LTPNode::Receiver::RecvBundleProcessor::RecvBundleProcessor(Receiver* parent_rcvr, uint64_t engine_id)
    : Logger("LTPNode::Receiver::RecvBundleProcessor",
             "/dtn/ltp/node/%lu/rcvr/bundleproc", engine_id),
      Thread("LTPNode::Receiver::RecvBundleProcessor")
{
    parent_rcvr_ = parent_rcvr;
}

//----------------------------------------------------------------------
LTPNode::Receiver::RecvBundleProcessor::~RecvBundleProcessor()
{
    shutdown();

    if (me_eventq_.size() > 0) {
        log_always("RcvBundleProcessor::destructor - Warning me_eventq_ contains %zu events",
                   me_eventq_.size());

        BundleProcessorEventObj* event;
        while (me_eventq_.try_pop(&event)) {
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

    lock_.lock(__func__);

    if (me_eventq_.size() > 0) {
        log_always("LTPEngine::Receiver(%zu) - Extracting bundles from %zu received sessions before shutting down",
                   parent_rcvr_->Remote_Engine_ID(), me_eventq_.size());
    }
    lock_.unlock();



    int ctr = 0;
    while ((me_eventq_.size() > 0) && !should_stop()) {
        usleep(100000);
        if (++ctr >= 10) {
            ctr = 0;
            log_always("RcvBundleProcessor delaying shutdown until all data processed - eventq size: %zu", 
                       me_eventq_.size());
        }
    }

    set_should_stop();
    while (!is_stopped()) {
        usleep(100000);
    }
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
                              size_t& bytes_queued, size_t& bytes_queued_max,
                              size_t& bytes_quota)
{
    oasys::ScopeLock scoplok(&lock_, __func__);
    queue_size = me_eventq_.size();
    bytes_queued = eventq_bytes_;
    bytes_queued_max = eventq_bytes_max_;
    bytes_quota = queued_bytes_quota_;
}

//----------------------------------------------------------------------
bool
LTPNode::Receiver::RecvBundleProcessor::okay_to_queue()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    //XXX?dz no longer allocating memory for this so not so much an issue
    //return (queued_bytes_quota_ == 0) || (eventq_bytes_ < queued_bytes_quota_);

    return true;
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::post(std::string& file_path, 
                                             size_t len, bool multi_bundle)
{
    // Receiver goes into shutdown mode before the RecvBundleProcessor so
    // continue to accept new posts from it even if our shutting_down_ flag is set

    BundleProcessorEventObj* event = new BundleProcessorEventObj();
    event->file_path_ = file_path;
    event->len_ = len;
    event->multi_bundle_ = multi_bundle;

    oasys::ScopeLock scoplok(&lock_, __func__);

    me_eventq_.push_back(event);

    eventq_bytes_ += len;
    if (eventq_bytes_ > eventq_bytes_max_) {
        eventq_bytes_max_ = eventq_bytes_;
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::post(SPtr_LTP_DS_MAP seg_list, size_t len, bool multi_bundle)
{
    // Receiver goes into shutdown mode before the RecvBundleProcessor so
    // continue to accept new posts from it even if our shutting_down_ flag is set

    BundleProcessorEventObj* event = new BundleProcessorEventObj();
    event->sptr_seg_list_ = seg_list;
    event->len_ = len;
    event->multi_bundle_ = multi_bundle;

    oasys::ScopeLock scoplok(&lock_, __func__);

    me_eventq_.push_back(event);

    eventq_bytes_ += len;
    if (eventq_bytes_ > eventq_bytes_max_) {
        eventq_bytes_max_ = eventq_bytes_;
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::run()
{
    char threadname[16] = "LTPRcvBndlProc";
    pthread_setname_np(pthread_self(), threadname);

    BundleProcessorEventObj* event;

    while (!should_stop()) {
        if (me_eventq_.size() > 0) {
            lock_.lock(__func__);

            event = nullptr;

            me_eventq_.try_pop(&event);

            if (event) {
                eventq_bytes_ -= event->len_;
                lock_.unlock();

                if (event->sptr_seg_list_) {
                    extract_bundles_from_data(event->sptr_seg_list_.get(), event->len_, event->multi_bundle_);
                } else {
                    extract_bundles_from_data_file(event->file_path_, event->len_, event->multi_bundle_);
                }

                delete event;
            } else {
                lock_.unlock();
            }
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::extract_bundles_from_data(LTP_DS_MAP* ds_map_ptr, size_t len, bool multi_bundle)
{
    size_t temp_payload_bytes_reserved   = len;
    BundleDaemon* bdaemon = BundleDaemon::instance();
    BundleStore* bs = BundleStore::instance();

    if (ds_map_ptr->size() == 0)
    {
        bs->release_payload_space(temp_payload_bytes_reserved);
        log_err("extract_bundles_from_data: no data to process?");
        return;
    }

    uint32_t check_client_service = 0;
    // the payload should contain one or more full bundles

    Bundle* bundle = nullptr;
    bool bundle_complete = false;
    size_t bundle_on_wire_len = 0;
    size_t num_bundles_extracted = 0;

    u_char* tmp_buf = nullptr;
    size_t tmp_buf_size = 0;
    size_t new_tmp_buf_size = 0;

    u_char* buf_ptr = nullptr;
    size_t  buf_len = 0;

    SPtr_LTPDataSegment sptr_ds_seg;
    LTP_DS_MAP::iterator seg_iter = ds_map_ptr->begin();

    bool abort_processing_segments = false;
    while (!abort_processing_segments && (seg_iter != ds_map_ptr->end())) {
        sptr_ds_seg = seg_iter->second;

        if (buf_len == 0) {
            buf_ptr = sptr_ds_seg->Payload();
            buf_len = sptr_ds_seg->Payload_Length();
        } else {
            // there was not enough data left from the last segment to process a CBOR element so
            // add this segment's data to the left over and try again
            new_tmp_buf_size = buf_len + sptr_ds_seg->Payload_Length();
            if (new_tmp_buf_size > tmp_buf_size) {
                tmp_buf_size = new_tmp_buf_size + 8;
                tmp_buf = (u_char*) realloc(tmp_buf, tmp_buf_size);
            }

            memcpy(tmp_buf+buf_len, sptr_ds_seg->Payload(), sptr_ds_seg->Payload_Length());
            buf_ptr = tmp_buf;  // update in case of a newly allocated buffer
            buf_len += sptr_ds_seg->Payload_Length();
        }

        while (buf_len > 0) {
            if (bundle == nullptr) {
                if (multi_bundle) {
                    // Multi bundle buffer... must make sure a 1 precedes all packets
                    int bytes_used = SDNV::decode(buf_ptr, buf_len, &check_client_service);
                    if (check_client_service != 1)
                    {
                        log_err("extract_bundles_from_data: LTP SDA - invalid Service ID: %d", check_client_service);
                        parent_rcvr_->inc_bundles_failed();
                        break; // so we free the reserved payload quota
                    }
                    buf_ptr += bytes_used;
                    buf_len -= bytes_used;
                }

                // begin extraction of a bundle
                bundle = new Bundle(BundleProtocol::BP_VERSION_UNKNOWN);
                bundle_complete = false;
                bundle_on_wire_len = 0;
            }


            ssize_t cc = BundleProtocol::consume(bundle, buf_ptr, buf_len, &bundle_complete);

            if (cc < 0)  {
                log_err("%s: LTP Session: %s - bundle protocol error",
                        __func__, sptr_ds_seg->session_key_str().c_str());

                parent_rcvr_->inc_bundles_failed();
                abort_processing_segments = true;
                break;
            } else if (cc == 0) {
                // need more data to parse a bundle block
                // Move left over data to the temp buffer before deleting the DS segment
                if (buf_len > tmp_buf_size) {
                    tmp_buf_size = new_tmp_buf_size + 8;
                    tmp_buf = (u_char*) realloc(tmp_buf, tmp_buf_size);
                }

                // buf_ptr is still good even though the sptr_ds_seg has been changed
                memmove(tmp_buf, buf_ptr, buf_len);
                buf_ptr = tmp_buf;  // switch to use our temp buffer
                break;
            } else {
                buf_ptr += cc;
                buf_len -= cc;

                bundle_on_wire_len += cc;

                if (bundle_complete) {
                    ++num_bundles_extracted;

                    // reserve payload space
                    bool bundle_deleted = false;
                    bool payload_space_reserved = false; // an output not an input
                    uint64_t dummy_prev_reserved_space = 0;
                    if (! bdaemon->query_accept_bundle_based_on_quotas(bundle, payload_space_reserved, dummy_prev_reserved_space)) {
                        if ((bundle->custody_requested() && !bundle->local_custody()) ||
                            (bundle->is_admin() && bundle->is_bpv7() && is_bibe_with_custody_transfer(bundle))) {

                            // safe to delete this bundle
                            delete bundle;
                            bundle = nullptr;
                            bundle_deleted = true;
                            // do not break - this is just rejecting one bundle
                            // due to quotas and not a protocol error
                        }
                    }

                    if (!bundle_deleted) {
                        // bundle already approved to use payload space
                        bs->reserve_payload_space(bundle);

                        parent_rcvr_->inc_bundles_success();

                        SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
                        BundleReceivedEvent* event_to_post;
                        event_to_post = new BundleReceivedEvent(bundle, EVENTSRC_PEER, 
                                                                bundle_on_wire_len, 
                                                                sptr_dummy_prevhop, nullptr);
                        SPtr_BundleEvent sptr_event_to_post(event_to_post);
                        BundleDaemon::post(sptr_event_to_post);

                        bundle = nullptr;
                    }
                }
            }
        }

        // this DS is no longer needed so delete it
        ds_map_ptr->erase(seg_iter);

        seg_iter = ds_map_ptr->begin();
    }

    // release the temp reserved space since the bundles now have the actually space reserved
    bs->release_payload_space(temp_payload_bytes_reserved);


    if (tmp_buf != nullptr) {
        free(tmp_buf);
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvBundleProcessor::extract_bundles_from_data_file(std::string& file_path, 
                                                                       size_t len, bool multi_bundle)
{
    size_t temp_payload_bytes_reserved   = len;
    BundleDaemon* bdaemon = BundleDaemon::instance();
    BundleStore* bs = BundleStore::instance();

    int data_file_fd = ::open(file_path.c_str(), O_RDONLY);

    if (data_file_fd < 0) {
        bs->release_payload_space(temp_payload_bytes_reserved);
        log_err("extract_bundles_from_data_file: error opening file (%s): %s", 
                file_path.c_str(), strerror(errno));
        return;
    }

    uint32_t check_client_service = 0;
    // the payload should contain one or more full bundles

    Bundle* bundle = nullptr;
    bool bundle_complete = false;
    size_t bundle_on_wire_len = 0;
    size_t num_bundles_extracted = 0;

    oasys::StreamBuffer work_buf;
    if (len >= 10000000) {
        work_buf.reserve(10000000);
    } else {
        work_buf.reserve(len);
    }

    // rewind the file back to its start
    errno = 0;

    size_t bytes_processed = 0;
    size_t bytes_remaining = len;
    ssize_t to_read;
    ssize_t cc;
    bool abort_processing = false;

    to_read = std::min(work_buf.tailbytes(), bytes_remaining);
    cc = ::read(data_file_fd, work_buf.end(), to_read);
    if (cc != to_read) {
        log_err("%s - error reading from file: %s - %s - closing and reopening to try again", 
                __func__, file_path.c_str(), strerror(errno));

        ::close(data_file_fd);
        data_file_fd = ::open(file_path.c_str(), O_RDONLY);

        if (data_file_fd < 0) {
            log_err("%s - error re-opening file: %s - %s - abort...", 
                    __func__, file_path.c_str(), strerror(errno));
            abort_processing = true;
        } else {
            cc = ::read(data_file_fd, work_buf.end(), to_read);
            if (cc != to_read) {
                log_err("%s - error reading 2nd time from file: %s - %s - abort...", 
                        __func__, file_path.c_str(), strerror(errno));
                abort_processing = true;
            } else {
                work_buf.fill(to_read);
            }
        }
    } else  {
        work_buf.fill(to_read);
    }

    while (!abort_processing && (bytes_remaining  > 0)) {
        while (work_buf.fullbytes() > 0) {
            if (bundle == nullptr) {
                if (multi_bundle) {
                    // Multi bundle buffer... must make sure a 1 precedes all packets
                    int bytes_used = SDNV::decode(work_buf.start(), work_buf.fullbytes(), &check_client_service);
                    if (check_client_service != 1)
                    {
                        abort_processing = true;
                        log_err("extract_bundles_from_data_file: %s - LTP SDA - invalid Service ID: %d", 
                                file_path.c_str(), check_client_service);
                        parent_rcvr_->inc_bundles_failed();
                        break; // so we free the reserved payload quota
                    }
                    work_buf.consume(bytes_used);
                    bytes_remaining -= bytes_used;
                    bytes_processed += bytes_used;
                }

                // begin extraction of a bundle
                bundle = new Bundle(BundleProtocol::BP_VERSION_UNKNOWN);
                bundle_complete = false;
                bundle_on_wire_len = 0;
            }


            cc = BundleProtocol::consume(bundle, (u_char*) work_buf.start(), work_buf.fullbytes(), &bundle_complete);

            if (cc < 0)  {
                log_err("%s: LTP Session file: %s - bundle protocol error",
                        __func__, file_path.c_str());

                parent_rcvr_->inc_bundles_failed();
                abort_processing = true;
                break;
            } else if (cc == 0) {
                // need more data to parse a bundle block
                break;
            } else {
                work_buf.consume(cc);
                bytes_remaining -= cc;
                bytes_processed += cc;

                bundle_on_wire_len += cc;

                if (bundle_complete) {
                    ++num_bundles_extracted;

                    // reserve payload space
                    bool bundle_deleted = false;
                    if ((bundle->custody_requested() && !bundle->local_custody()) ||
                        (bundle->is_admin() && bundle->is_bpv7() && is_bibe_with_custody_transfer(bundle))) {

                        bool payload_space_reserved = false; // an output not an input
                        uint64_t dummy_prev_reserved_space = 0;
                        if (! bdaemon->query_accept_bundle_based_on_quotas(bundle, payload_space_reserved, 
                                                                       dummy_prev_reserved_space)) {
                            // safe to delete this bundle
                            delete bundle;
                            bundle = nullptr;
                            bundle_deleted = true;
                            // do not break - this is just rejecting one bundle
                            // due to quotas and not a protocol error
                        }
                    }

                    if (!bundle_deleted) {
                        // bundle already approved to use payload space
                        bs->reserve_payload_space(bundle);

                        parent_rcvr_->inc_bundles_success();

                        //log_always("%s - file:%s - got bundle: *%p", 
                        //           __func__, file_path.c_str(), bundle);

                        SPtr_EID sptr_dummy_prevhop = BD_MAKE_EID_NULL();
                        BundleReceivedEvent* event_to_post;
                        event_to_post = new BundleReceivedEvent(bundle, EVENTSRC_PEER, 
                                                                bundle_on_wire_len, 
                                                                sptr_dummy_prevhop, nullptr);
                        SPtr_BundleEvent sptr_event_to_post(event_to_post);
                        BundleDaemon::post(sptr_event_to_post);

                        bundle = nullptr;
                    }
                }
            }
        }

        if (!abort_processing) {
            if (bytes_remaining > 0) {
                work_buf.compact();

                to_read = std::min(work_buf.tailbytes(), bytes_remaining);

                cc = ::read(data_file_fd, work_buf.end(), to_read);

                if (cc != to_read) {
                    errno = 0;
                    log_err("%s - error (cc=%zd) in loop reading %zd bytes at offset %zu (session_size: %zu) from file: %s - %s -- try to close and reopen file...", 
                            __func__, cc, to_read, bytes_processed, len, file_path.c_str(), strerror(errno));

                    // try to close and reopen the file 
                    ::close(data_file_fd);
                    errno = 0;
                    data_file_fd = ::open(file_path.c_str(), O_RDONLY);

                    if (data_file_fd < 0) {
                        log_err("%s - error re-opening from file: %s - %s -- abort...", 
                                __func__, file_path.c_str(), strerror(errno));
                        abort_processing = true;
                    } else {
                        // seek to the previous position
                        off64_t cur_offset = ::lseek64(data_file_fd, 0, SEEK_CUR);
                        off64_t adjust = (off64_t) (off64_t) bytes_processed - cur_offset;
                        off64_t new_offset = ::lseek64(data_file_fd, adjust, SEEK_CUR);
                        if (new_offset != (off64_t) bytes_processed) {
                            log_err("%s - error seeking to original position of file: %s - %s - abort...", 
                                    __func__, file_path.c_str(), strerror(errno));
                            abort_processing = true;
                        } else {
                            errno = 0;
                            cc = ::read(data_file_fd, work_buf.end(), to_read);
                            if (cc != to_read) {
                                log_err("%s - error (cc=%zd) in 2nd attempt reading from file: %s - %s -- abort...",
                                        __func__, cc, file_path.c_str(), strerror(errno));
                                abort_processing = true;
                            } else {
                                work_buf.fill(to_read);
                            }
                        }
                    }
                } else  {
                    work_buf.fill(to_read);
                }
            }
        }
    }

    // release the temp reserved space since the bundles now have the actual space reserved
    bs->release_payload_space(temp_payload_bytes_reserved);

    // close and delete the file
    ::close(data_file_fd);

    if (abort_processing && parent_rcvr_->keep_aborted_files()) {
        log_always("%s: keeping aborted LTP Session file for analysis: %s",
                        __func__, file_path.c_str());

    } else {
        ::unlink(file_path.c_str());
    }
}


//----------------------------------------------------------------------
bool
LTPNode::Receiver::RecvBundleProcessor::is_bibe_with_custody_transfer(Bundle* bundle)
{
    (void) bundle;

    //TODO:  requires a fairly deep-dive into parsing the payload if implemented
    return false;
}





//----------------------------------------------------------------------
LTPNode::Receiver::RecvSegProcessor::RecvSegProcessor(Receiver* parent_rcvr, uint64_t engine_id)
    : Logger("LTPNode::Receiver::RecvSegProcessor",
             "/dtn/ltp/node/%lu/rcvr/segproc", engine_id),
      Thread("LTPNode::Receiver::RecvSegProcessor"),
      eventq_admin_(logpath_),
      eventq_ds_(logpath_)
{
    parent_rcvr_ = parent_rcvr;
    queued_bytes_quota_  = parent_rcvr_->ltp_queued_bytes_quota();
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
LTPNode::Receiver::RecvSegProcessor::start_shutdown()
{
    // this flag prevents procesing of any new incoming data 
    start_shutting_down_ = true;
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::shutdown()
{
    // this flag prevents procesing of any new incoming data 
    start_shutting_down_ = true;
    shutting_down_ = true;

    set_should_stop();

    while (!is_stopped()) {
        usleep(100000);
    }
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::reconfigured()
{
    queued_bytes_quota_  = parent_rcvr_->ltp_queued_bytes_quota();
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::get_queue_stats(size_t& queue_size, 
                              size_t& bytes_queued, size_t& bytes_queued_max,
                              size_t& bytes_quota, size_t& segs_discards)
{
    oasys::ScopeLock scoplok(&eventq_ds_lock_, __func__);
    queue_size = eventq_ds_.size();
    bytes_queued = eventq_ds_bytes_;
    bytes_queued_max = eventq_ds_bytes_max_;
    bytes_quota = queued_bytes_quota_;
    segs_discards = stats_ds_segs_discarded_;
}

//----------------------------------------------------------------------
void
LTPNode::Receiver::RecvSegProcessor::post_admin(LTPDataReceivedEvent* event)
{
    if (start_shutting_down_) {
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
    if (start_shutting_down_) {
        free(event->data_);
        delete event;
        return;
    }

    oasys::ScopeLock scoplok(&eventq_ds_lock_, __func__);

    if ((queued_bytes_quota_ > 0) && (eventq_ds_bytes_ >= queued_bytes_quota_)){
        // must discard this DS  (it should be resent if it is red LTP else unreliable anyway)
        ++stats_ds_segs_discarded_;
        free(event->data_);
        delete event;
        return;
    }

    eventq_ds_.push_back(event);

    eventq_ds_bytes_ += event->len_;
    if (eventq_ds_bytes_ > eventq_ds_bytes_max_)
    {
        eventq_ds_bytes_max_ = eventq_ds_bytes_;
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
LTPNode::Sender::Sender(SPtr_LTPEngine ltp_engine_sptr, LTPNode* parent_node, SPtr_LTPCLSenderIF sptr_clsender)
    : 
      Logger("LTPNode::Sender", "/dtn/ltp/node/%zu/sender", sptr_clsender->Remote_Engine_ID()),
      Thread("LTPNode::Sender"),
      sptr_clsender_(sptr_clsender),
      parent_node_(parent_node)
{
    ltp_engine_sptr_ = ltp_engine_sptr;
    local_engine_id_ = ltp_engine_sptr_->local_engine_id();
    remote_engine_id_ = sptr_clsender->Remote_Engine_ID();

    bundle_processor_ = QPtr_SndrBundleProcessor(new SndrBundleProcessor(this, sptr_clsender_->Remote_Engine_ID()));
    bundle_processor_->start();

    sptr_loading_session_ = nullptr;
    memset(&stats_, 0, sizeof(stats_));
    sptr_blank_session_ = std::make_shared<LTPSession>(0UL, 0UL, false);
    sptr_blank_session_->Set_Outbound_Cipher_Suite(sptr_clsender_->Outbound_Cipher_Suite());
    sptr_blank_session_->Set_Outbound_Cipher_Key_Id(sptr_clsender_->Outbound_Cipher_Key_Id());
    sptr_blank_session_->Set_Outbound_Cipher_Engine(sptr_clsender_->Outbound_Cipher_Engine());

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
    sptr_clsender_->Set_Ready_For_Bundles(is_ready);

    uint64_t tmp_agg_time_millis = 0;
    uint64_t micros_to_wait = 0;

    while (!should_stop()) {

        if (start_shutting_down_) {
            sptr_clsender_->Set_Ready_For_Bundles(false);
            usleep(10000);

            // if sptr_loading_session_ is in progress then it will never 
            // be processed which is okay 
            // - basically just waiting for should_stop
            continue;
        }

        agg_time_lock_.lock();

        if (agg_time_millis_ == 0) {
            // No time based aggregation so no processing needed here
            // - just sleep a 1/10th before checking to see if
            // an aggreagation time has been set or if time to stop
            micros_to_wait = 100000;

            agg_time_lock_.unlock();
        } else {
            // snag the current agg time while we have the lock
            tmp_agg_time_millis = agg_time_millis_;

            // must release agg time lock before getting loading session lock
            agg_time_lock_.unlock();

            // max wait will be the agg time which is milliseconds
            micros_to_wait = tmp_agg_time_millis * 1000;

            // only need to check sptr_loading_session_ if not nullptr and the lock is immediately
            // available since send_bundle now checks for agg time expiration also
            if ((sptr_loading_session_ != nullptr) && 
                (0 == loading_session_lock_.try_lock("Sender::run()"))) {

                loaded_session = nullptr;

                // sptr_loading_session_ may have been cleared out by the time we get the lcok here
                if (nullptr != sptr_loading_session_) {
                    micros_to_wait = sptr_loading_session_->MicrosRemainingToSendIt(agg_time_millis_);

                    if (micros_to_wait == 0) {
                        // time to close the LTP session and send it
                        loaded_session = sptr_loading_session_;
                        sptr_loading_session_ = nullptr;

                        // no need to check again until the agg time expires
                        micros_to_wait = tmp_agg_time_millis * 1000;
                    }
                }

                // release the lock as quick as possible
                loading_session_lock_.unlock();

                if (nullptr != loaded_session) {
                     bundle_processor_->post(loaded_session);
                     loaded_session = nullptr;
                }
            }
        }
        if (micros_to_wait > 100000) {
            // force max wait to be 1/10th second
            micros_to_wait = 100000;
        }

        if (micros_to_wait > 10) {
            // wait for time to expire or the agg time to be changed
            std::unique_lock<std::mutex> qlok(agg_time_lock_);

            cond_var_agg_time_changed_.wait_for(qlok, std::chrono::microseconds(micros_to_wait));
        }
    }
}

//---------------------------------------------------------------------------
void
LTPNode::Sender::start_shutdown()
{
    //   * stop trasnmitting DS sewgs
    start_shutting_down_ = true;

    // stop processing queued bundles to be sent
    bundle_processor_->start_shutdown();

    //   * cancel all outgoing sessions
    cancel_all_sessions();
}

//---------------------------------------------------------------------------
void
LTPNode::Sender::shutdown()
{
    start_shutting_down_ = true;
    shutting_down_ = true;

    set_should_stop();
    while (!is_stopped()) {
        usleep(100000);
    }

    bundle_processor_->shutdown();

    ltp_engine_sptr_.reset();
    bundle_processor_.reset();
    sptr_clsender_.reset();
    sptr_blank_session_.reset();
    sptr_loading_session_.reset();
}

//----------------------------------------------------------------------
void
LTPNode::Sender::PostTransmitProcessing(LTPSession* session_ptr, bool success)
{
    BundleRef bref;

    while ((bref = session_ptr->Bundle_List()->pop_back()) != nullptr)
    {
        if (bref->expired_in_link_queue()) {
            //log_debug("PostTransmitProcessing not processing bundle that expired in Q: %" PRIbid,
            //         bref->bundleid());
        } else if (success) {
            if (session_ptr->Expected_Red_Bytes() > 0)
            {
                sptr_clsender_->PostTransmitProcessing(bref, true,  session_ptr->Expected_Red_Bytes(), success);
            } else if (session_ptr->Expected_Green_Bytes() > 0) { 
                sptr_clsender_->PostTransmitProcessing(bref, false,  session_ptr->Expected_Green_Bytes(), success);
            } else {
                log_always("PostTransmitProcessing - Skipping xmit success bundle because expected bytes is 0?? : %" PRIbid,
                         bref->bundleid());

            }

            ++stats_.bundles_success_;
        }
        else
        {
            sptr_clsender_->PostTransmitProcessing(bref, false, 0, success);
            ++stats_.bundles_failed_;
        }
    }
}

//----------------------------------------------------------------------
void 
LTPNode::Sender::Process_Segment(LTPSegment* seg_ptr, bool closed, bool cancelled)
{
    if (shutting_down_) {
        return;
    }

    
    switch(seg_ptr->Segment_Type()) {

        case LTP_SEGMENT_RS:
             process_RS((LTPReportSegment *) seg_ptr, closed, cancelled);
             break;

        case LTP_SEGMENT_CS_BR:
             process_CS((LTPCancelSegment *) seg_ptr, closed, cancelled);
             break;

        case LTP_SEGMENT_CAS_BS:
             process_CAS((LTPCancelAckSegment *) seg_ptr);
             break;

        default:
             log_err("Unknown Segment Type for Sender::Process_Segment(%s)",
                     seg_ptr->Get_Segment_Type()); 
             break;
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::process_RS(LTPReportSegment* rpt_seg, bool closed, bool cancelled)
{
    ++stats_.total_rcv_rs_;

    SPtr_LTPSession sptr_session;


    if (!closed) {

        sptr_session = find_sender_session(rpt_seg);

        if (sptr_session == nullptr) {
            // was it closed while this RS retransmit was queued to be processed?

            SPtr_LTPEngineReg engptr = ltp_engine_sptr_->lookup_engine(rpt_seg->Engine_ID(),
                                                                       rpt_seg->Session_ID(),
                                                                       closed, cancelled);
    
            if ((engptr == nullptr) || !closed) {
                build_CS_segment(rpt_seg, LTP_SEGMENT_CS_BS, 
                                 LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);

                // not in closed list or current list so it must be pretty old
                ++stats_.rs_segment_resends_;
                return;

            } else {
                // closed session will be handled below
            }
        }
    }

    // was the session previously closed and possibly cancelled?
    if (closed) {
        ++stats_.rs_segment_resends_;

        if (!cancelled) {
            // send RS ACK
            LTPReportAckSegment* ras = new LTPReportAckSegment(rpt_seg, sptr_blank_session_.get());

            ++stats_.total_snt_ra_;

            SPtr_LTPTimer dummy_timer;
            sptr_clsender_->Send_Admin_Seg_Highest_Priority(ras->asNewString(), dummy_timer, false);
            delete ras;

        } else {
            // received an RS for cancelled session so try to inform it again of the cancel
            build_CS_segment(rpt_seg, LTP_SEGMENT_CS_BS, 
                             LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
       }

       return;
    }


    ASSERT(sptr_session != nullptr);


    sptr_session->Set_Last_Packet_Time(parent_node_->AOS_Counter());

    // send back an ACK
    LTPReportAckSegment* ras = new LTPReportAckSegment(rpt_seg, sptr_blank_session_.get());

    ++stats_.total_snt_ra_;

    SPtr_LTPTimer dummy_timer;
    sptr_clsender_->Send_Admin_Seg_Highest_Priority(ras->asNewString(), dummy_timer, true);
    delete ras;

    // check for RS claiming the entire session
    if (sptr_session->Is_EOB_Defined() && 
        (rpt_seg->LowerBounds() == 0) &&
        (rpt_seg->Claims().size() == 1) &&
        (rpt_seg->ClaimedBytes() == sptr_session->Expected_Red_Bytes())) {

//        log_always("RS claimed entire LTP session: %s Chkpt: %zu Bytes: %zu", 
//                   sptr_session->key_str().c_str(), rpt_seg->Checkpoint_ID(), rpt_seg->ClaimedBytes());

        sptr_session->RS_Claimed_Entire_Session();
        cleanup_sender_session(sptr_session->Engine_ID(), sptr_session->Session_ID(), 0, 0, LTP_EVENT_SESSION_COMPLETED);
        return;
    }

    // process the report - could be multiple Report Segemnts for a Checkpoint

    uint64_t checkpoint_id = rpt_seg->Checkpoint_ID();

    // returns -99 if the Checkpoint ID DS has already been claimed
    ssize_t ckpt_stop_byte = sptr_session->Find_Checkpoint_Stop_Byte(checkpoint_id);

    if (ckpt_stop_byte < 0) {
        if (ckpt_stop_byte == -99) {
            // already processed this duplicate RS so the RAS just sent is sufficient
        } else {
            // probably aleady closed out so the RAS just sent is sufficient
            log_warn("Session: %s  Ckpt ID: %zu  #Claims: %zu Bytes: %zu  - Received Report Segment with unknown Checkpoint ID",
                       rpt_seg->session_key_str().c_str(), 
                       rpt_seg->Checkpoint_ID(),
                       rpt_seg->Claims().size(),
                       rpt_seg->ClaimedBytes());
        }
        return;
    } else if ((size_t)(ckpt_stop_byte + 1) == rpt_seg->UpperBounds()) {
        // this report claims the DS Checkpoint itself so can now flag it as claimed by setting the stop byte to -99
        sptr_session->Flag_Checkpoint_As_Claimed(checkpoint_id);
    }


    // Create a DS Map to hold the list of data segs that need to be retransmitted as a 
    // result of processing this report seg
    QPtr_LTP_DS_MAP qptr_retransmit_dsmap(new LTP_DS_MAP());

    size_t segs_claimed = 0;

    // lower bounds starts with the report's lower bounds and increases after each claim
    size_t rpt_lower_bounds = rpt_seg->LowerBounds();
    size_t claim_lower_bounds = rpt_lower_bounds;
    // upper bounds is set to the upper bounds of each claim
    size_t claim_upper_bounds = claim_lower_bounds;
    size_t claim_offset;

    SPtr_LTPReportClaim rc;
    LTP_RC_MAP::iterator rc_iterator;

    oasys::ScopeLock scoplok (sptr_session->SegMap_Lock(), __func__);

    rc_iterator= rpt_seg->Claims().begin(); 
    while (rc_iterator != rpt_seg->Claims().end())
    {
        rc = rc_iterator->second;     

        claim_offset = rpt_lower_bounds + rc->Offset();
        claim_upper_bounds = rpt_lower_bounds + rc->Offset() + rc->Length();

        segs_claimed += sptr_session->RemoveClaimedSegments(checkpoint_id, 
                                                            claim_lower_bounds, 
                                                            claim_offset, 
                                                            claim_upper_bounds, 
                                                            qptr_retransmit_dsmap.get()); 

        // update the lower bounds for use with the next claim
        claim_lower_bounds = claim_upper_bounds;

        rc_iterator++;
    }

    // check for a gap between the last claim and the RS upper bounds
    if (rpt_seg->UpperBounds() > claim_upper_bounds) {
        // claim_lower_bounds already seet to the previous claim upper bounds
        claim_upper_bounds = rpt_seg->UpperBounds();
        claim_offset = claim_upper_bounds;

        segs_claimed += sptr_session->RemoveClaimedSegments(checkpoint_id, 
                                                            claim_lower_bounds, 
                                                            claim_offset, 
                                                            claim_upper_bounds, 
                                                            qptr_retransmit_dsmap.get()); 
    }

    size_t report_serial_num = rpt_seg->Report_Serial_Number();
        
    // only send segments from the new map which is a subset of the red_segs DS Map
    Send_Remaining_Segments(sptr_session.get(), qptr_retransmit_dsmap.get(), report_serial_num);
    
    sptr_session->Set_Last_Packet_Time(parent_node_->AOS_Counter());

    if (sptr_session->Is_EOB_Defined() && (sptr_session->Red_Segments()->size() == 0)) {
        // XXX/dz Receiver can respond so fast to intermediate checkpoints that red_segments can drop to zero
        //        before the sender has generated all of the segments -- must check for EOB defined!!

        scoplok.unlock();

        //log_always("dzdebug - all segs claimed %s - bounds.lower: %zu .upper: %zu",
        //           sptr_session->key_str().c_str(), rpt_seg->LowerBounds(), rpt_seg->UpperBounds());

        cleanup_sender_session(sptr_session->Engine_ID(), sptr_session->Session_ID(), 0, 0, LTP_EVENT_SESSION_COMPLETED);
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::Send_Remaining_Segments(LTPSession* session_ptr, LTP_DS_MAP* retransmit_dsmap_ptr,
                                         size_t report_serial_num)
{
    ASSERT(session_ptr->SegMap_Lock()->is_locked_by_me());

    if (start_shutting_down_) {
        return;
    }

    if (retransmit_dsmap_ptr->size() > 0)
    {
        SPtr_LTPTimer sptr_timer;
        SPtr_LTPDataSegment sptr_ds_seg;
        LTP_DS_MAP::iterator iter;


        if (!session_ptr->has_ds_resends()) {
            ++stats_.ds_sessions_with_resends_;
            session_ptr->set_has_ds_resends();
        }

        bool last_seg_remaining;

        iter = retransmit_dsmap_ptr->begin();
        while (iter != retransmit_dsmap_ptr->end())
        {
            sptr_ds_seg = iter->second;

            // Is this the last segment to be sent? If so then it needs to be a new checkpoint
            last_seg_remaining = (retransmit_dsmap_ptr->size() == 1);

            sptr_timer.reset();

            // prevent [ltpudp] CL from sending this segment if it is already queued
            // until we can determine if it needs to be changed to a checkpoint
            oasys::ScopeLock ds_scoplok(sptr_ds_seg->lock(), __func__);

            ASSERT(!sptr_ds_seg->IsCheckpoint());

             if (last_seg_remaining) {
                sptr_ds_seg->Set_Checkpoint_ID(session_ptr->Get_Next_Checkpoint_ID());
                sptr_ds_seg->Set_Report_Serial_Number(report_serial_num);

                // Add this Data Segment to the Session's list of checkpoint DSs
                session_ptr->Add_Checkpoint_Seg(sptr_ds_seg.get());

                sptr_ds_seg->Encode_All();

                SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();
                SPtr_LTPSegment sptr_ltp_seg = sptr_ds_seg;

                sptr_timer = sptr_ds_seg->Create_Retransmission_Timer(sndr_if, LTP_EVENT_DS_TO, retran_interval_, sptr_ltp_seg);
            }

            if (!sptr_ds_seg->Queued_To_Send()) {
                ++stats_.ds_segment_resends_;

                sptr_ds_seg->Set_Queued_To_Send(true);

                sptr_clsender_->Send_DataSeg_Higher_Priority(sptr_ds_seg, sptr_timer);
            }


            // can now delete the DS from the retransmit list
            retransmit_dsmap_ptr->erase(iter);
            sptr_timer.reset();
            sptr_ds_seg.reset();

            // get the next segment to transmit
            iter = retransmit_dsmap_ptr->begin();
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::process_CS(LTPCancelSegment* cs_segment, bool closed, bool cancelled)
{
    if (cancelled) {
        ++stats_.cancel_by_rcvr_segs_;
    } else if (closed) {
        // already received a good RS indicating the session was received??
        log_warn("%s - Received cancel by receiver after receiving a full RS indicating success?",
                 cs_segment->session_key_str().c_str());
        ++stats_.cancel_by_rcvr_segs_;
    } else {
        // now try to find the session and close it out
        SPtr_LTPSession sptr_session = find_sender_session(cs_segment);

        if (sptr_session != nullptr) {
            ++stats_.cancel_by_rcvr_sessions_;

            log_debug("Received cancel by receiver - LTP Session: %s",
                    sptr_session->key_str().c_str());

            sptr_session->RemoveSegments();
            cleanup_sender_session(cs_segment->Engine_ID(), cs_segment->Session_ID(), 0, 0, cs_segment->Segment_Type());

        } else {
            log_debug("Received cancel by receiver - unknown LTP Session: %s",
                    cs_segment->session_key_str().c_str());
        }
    }

    ++stats_.total_sent_and_rcvd_ca_;

    // send an ACK even if we don't have the session so the peer can close out its session
    LTPCancelAckSegment* cas_seg = new LTPCancelAckSegment(cs_segment, 
                                                            LTPSEGMENT_CAS_BR_BYTE, sptr_blank_session_.get()); 

    SPtr_LTPTimer dummy_timer;
    sptr_clsender_->Send_Admin_Seg_Highest_Priority(cas_seg->asNewString(), dummy_timer, true);
    delete cas_seg;

}

//----------------------------------------------------------------------
void
LTPNode::Sender::process_CAS(LTPCancelAckSegment* cas_segment)
{
    ++stats_.total_sent_and_rcvd_ca_;

    SPtr_LTPSession sptr_session = find_sender_session(cas_segment);

    if (sptr_session != nullptr) {
        sptr_session->Clear_Cancel_Segment();

        erase_send_session(sptr_session.get(), true);
    }
}

//----------------------------------------------------------------------
void 
LTPNode::Sender::build_CS_segment(LTPSession* session_ptr, int segment_type, u_char reason_code)
{
    if (!session_ptr->Is_LTP_Cancelled()) {
        ++stats_.cancel_by_sndr_sessions_;

        session_ptr->RemoveSegments();

        SPtr_LTPCancelSegment cancel_segment = std::make_shared<LTPCancelSegment>(session_ptr, segment_type, reason_code);

        update_session_counts(session_ptr, LTPSession::LTP_SESSION_STATE_CS); 

        SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

        SPtr_LTPSegment sptr_ltp_seg = cancel_segment;

        SPtr_LTPTimer timer;
        timer = cancel_segment->Create_Retransmission_Timer(sndr_if, LTP_EVENT_CS_TO, retran_interval_, sptr_ltp_seg);

        session_ptr->Set_Cancel_Segment(cancel_segment);

        ++stats_.cancel_by_sndr_segs_;

        sptr_clsender_->Send_Admin_Seg_Highest_Priority(cancel_segment->asNewString(), timer, false);

        //signal DTN that transmit failed for the bundle(s)
        PostTransmitProcessing(session_ptr, false);
    }
}

//----------------------------------------------------------------------
void 
LTPNode::Sender::build_CS_segment(LTPSegment* seg_ptr, int segment_type, u_char reason_code)
{
    // this is a one shot cancel after receving a message for an unknown session
    SPtr_LTPCancelSegment cancel_segment = std::make_shared<LTPCancelSegment>(seg_ptr, segment_type, reason_code);

    ++stats_.cancel_by_sndr_segs_;

    SPtr_LTPTimer dummy_timer;
    sptr_clsender_->Send_Admin_Seg_Highest_Priority(cancel_segment->asNewString(), dummy_timer, false);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::cancel_all_sessions()
{
    SESS_SPTR_MAP::iterator iter; 
    SPtr_LTPSession sptr_session;
    size_t num_cancelled = 0;

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    // validate the counts while we have the lock
    iter = send_sessions_.begin();
    while (iter != send_sessions_.end())
    {
        sptr_session = iter->second;

        switch (sptr_session->Session_State())
        {
            case LTPSession::LTP_SESSION_STATE_DS:
            case LTPSession::LTP_SESSION_STATE_RS: 
                ++num_cancelled;
                build_CS_segment(sptr_session.get(), LTP_SEGMENT_CS_BS, 
                                 LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);

                break;

            default:
              ;
        }
        ++iter;
    }

    log_always("LTPEngine::Sender(%zu) cancelled %zu sesssions while shutting down",
               remote_engine_id_, num_cancelled);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::dump_sessions(oasys::StringBuffer* buf)
{
    if (shutting_down_) {
        return;
    }

    SESS_SPTR_MAP::iterator iter; 
    SPtr_LTPSession sptr_session;
    size_t ctr_ds = 0;
    size_t ctr_rs = 0;
    size_t ctr_cs = 0;

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    // validate the counts while we have the lock
    iter = send_sessions_.begin();
    while (iter != send_sessions_.end())
    {
        sptr_session = iter->second;

        switch (sptr_session->Session_State())
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
                 sptr_clsender_->Get_Bundles_Queued_Count(), sptr_clsender_->Get_Bundles_InFlight_Count());

    if (sptr_clsender_->Dump_Sessions() && (send_sessions_.size() > 0)) {
        buf->append("Sender Session List:\n");

        SPtr_LTPSession sptr_session;
        SESS_SPTR_MAP::iterator iter = send_sessions_.begin();

        while (iter != send_sessions_.end())
        {
            sptr_session = iter->second;

            buf->appendf("    %s [%s]  red segs unclaimed: %zu  of %zu  bytes: %zu  EOB set: %s\n",
                         sptr_session->key_str().c_str(), sptr_session->Get_Session_State(),
                         sptr_session->Red_Segments()->size(), sptr_session->Total_Red_Segments(),
                         sptr_session->Expected_Red_Bytes(),
                         sptr_session->Is_EOB_Defined()?"true":"false");

            if (sptr_clsender_->Dump_Segs()) {
                sptr_session->dump_red_segments(buf);
            }


            ++iter;
        }
        buf->append("\n");
    }

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
               //          Sessions  /  Segments    Sessions  /  Segments   Rcvd Total   But Got     But Got    Expird inQ   Failure
               //         -----------------------  -----------------------  ----------  ----------  ----------  ----------  ----------
               //Sender   1234567890 / 1234567890  1234567890 / 1234567890  1234567890  1234567890  1234567890  1234567890  1234567890
               //Receiver 1234567890 / 1234567890  1234567890 / 1234567890  1234567890  1234567890  1234567890  1234567890  1234567890
    buf->appendf("Sender   %10" PRIu64 " / %10" PRIu64 "  %10" PRIu64 " / %10" PRIu64 "  %10" PRIu64 "  %10d  %10d  %10" PRIu64 "  %10" PRIu64 "\n",
                 stats_.cancel_by_sndr_sessions_, stats_.cancel_by_sndr_segs_, 
                 stats_.cancel_by_rcvr_sessions_, stats_.cancel_by_rcvr_segs_, stats_.total_sent_and_rcvd_ca_,
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
LTPNode::Sender::update_session_counts(LTPSession* session_ptr, int new_state)
{
    int old_state = session_ptr->Session_State();

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

        session_ptr->Set_Session_State(new_state); 
    }

    // less than or equal here for case where last session is currently being loaded
    bool is_ready = !shutting_down_ && 
                    ((send_sessions_.size() - sessions_state_cs_) <= max_sessions_);
    sptr_clsender_->Set_Ready_For_Bundles(is_ready);
}

//----------------------------------------------------------------------
int64_t
LTPNode::Sender::send_bundle(const BundleRef& bref)
{
    if (start_shutting_down_) {
        return -1;
    }

    int64_t total_len = 0;

    Bundle* bundle_ptr  = (Bundle *) bref.object();

    do {  // just to limit the scope of the lock
        oasys::ScopeLock l(bref->lock(), __func__);

        SPtr_BlockInfoVec sptr_blocks = sptr_clsender_->GetBlockInfoVec(bundle_ptr);

        if (sptr_blocks == nullptr) {
            //XXX/dz only seen when bundle has expired
            //log_always("dzdebug - LTPEngine::send_bundle got null blocks for bundle *%p - expired = %s",
            //           bundle_ptr, bref->expired()?"true":"false");
        } else {
            total_len = BundleProtocol::total_length(bundle_ptr, sptr_blocks.get());
        }
    } while (false); // end of scope



    if (bref->expired() || bref->manually_deleting())
    {
        bref->set_expired_in_link_queue();
        ++stats_.bundles_expired_in_queue_;

        // do not add to the LTP session
        sptr_clsender_->Del_From_Queue(bref);
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
        green_session->Set_Send_Green(green);

        // not part of the send_sessions_ list so do not update_session_counts
        green_session->Set_Session_State(LTPSession::LTP_SESSION_STATE_DS);
        green_session->Set_Outbound_Cipher_Key_Id(sptr_clsender_->Outbound_Cipher_Key_Id());
        green_session->Set_Outbound_Cipher_Suite(sptr_clsender_->Outbound_Cipher_Suite());
        green_session->Set_Outbound_Cipher_Engine(sptr_clsender_->Outbound_Cipher_Engine());

        green_session->AddToBundleList(bref, total_len);

        bundle_processor_->post(green_session);


    } else {  // define scope for the ScopeLock

        // prevent run method from accessing the sptr_loading_session_ while working with it
        oasys::ScopeLock loadlock(&loading_session_lock_,"Sender::send_bundle");  

        if (sptr_loading_session_ == nullptr) {
            // sets sptr_loading_session_ if it true
            if (!create_sender_session()) {
                return -1;
            }
        }
        // use the loading sesssion and add the bundle to it.
 
        update_session_counts(sptr_loading_session_.get(), LTPSession::LTP_SESSION_STATE_DS); 


        sptr_loading_session_->AddToBundleList(bref, total_len);

        // check the aggregation size and time while we have the loading session
        if (sptr_loading_session_->Session_Size() >= agg_size_) {
            loaded_session = sptr_loading_session_;
            sptr_loading_session_ = nullptr;
        } else {
            std::lock_guard<std::mutex> scoplok(agg_time_lock_);

            if (sptr_loading_session_->TimeToSendIt(agg_time_millis_)) {
                loaded_session = sptr_loading_session_;
                sptr_loading_session_ = nullptr;
            }
        }
    }

    // do link housekeeping now that the lock is released
    if (!green) {
        sptr_clsender_->Add_To_Inflight(bref);
    }
    sptr_clsender_->Del_From_Queue(bref);

    if (loaded_session != nullptr && !green) {
        bundle_processor_->post(loaded_session);
    }

    return total_len;
}

//----------------------------------------------------------------------
void
LTPNode::Sender::process_bundles(LTPSession* session_ptr)
{
    if (start_shutting_down_) {
        return;
    }

    if (sptr_clsender_->Use_Files_Xmit() && session_ptr->Session_Size() >= 10000000) {

        bool result = process_bundles_using_a_file(session_ptr);
        if (result) {
            // success - finished
            return;
        } else {
            log_err("Error creating LTP data file so reverting to using memory");
        }
    }

    oasys::ScopeLock bl(session_ptr->Bundle_List()->lock(), "process_bundles()"); 

    
    remove_expired_bundles(session_ptr);

    size_t  num_bundles = session_ptr->Bundle_List()->size();

    if (num_bundles == 0) {
        // all bundles expired so remove this session
        cleanup_sender_session(session_ptr->Engine_ID(), session_ptr->Session_ID(), 0, 0, 
                               LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES);
        return;
    }


    Bundle* bundle = nullptr;
    BundleRef bref("Sender::process_bundles()");  
    BundleList::iterator bundle_iter; 


    size_t bundles_processed = 0;
    size_t bytes_produced    = 0;

    size_t session_offset    = 0;
    int    client_service_id = 1;   // 1 = Bundle Protocol (single bundle for CCSDS or multiple for ION)

    if (ccsds_compatible_) {
        if (num_bundles > 0) {
            client_service_id = 2;  // 2 = LTP Service Data Aggregation indicates multiple bundles for CCSDS
        }
    }


    // use a copy of the seg_size_ in case it gets changed while processing this session
    size_t my_seg_size = seg_size_;
    size_t buf_idx = 0;
    size_t buf_unused = my_seg_size;
    u_char* buffer = (u_char*) malloc(my_seg_size);

    if (buffer == nullptr) {
       log_crit("Error allocating LTP Sender temp buffer of size %zu", my_seg_size);

        // signal the BundleDeamon that these bundles failed
        PostTransmitProcessing(session_ptr, false);
        cleanup_sender_session(session_ptr->Engine_ID(), session_ptr->Session_ID(), 0, 0, 
                               LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES);
        return;
    }


    // loop through the bundles generating LTP Segments


    bool is_checkpoint = false;
    size_t bytes_per_checkpoint = sptr_clsender_->Bytes_Per_CheckPoint();
    size_t bytes_produced_toward_checkpoint = 0;

    // track segements associated with each intermediate checkpoint if needed
    bool use_intermediate_checkpoints = (bytes_per_checkpoint > 0) && 
                                        (session_ptr->Session_Size() > bytes_per_checkpoint);

    bool last_seg = false;  // is this the last segment for the LTP Session?

    bundle_iter = session_ptr->Bundle_List()->begin(); 
    while (!start_shutting_down_ && (bundle_iter != session_ptr->Bundle_List()->end()))
    {
        bundles_processed += 1;

        bool complete = false;

        bundle = *bundle_iter;
        bref     = bundle;


        oasys::ScopeLock scoplok(bref->lock(), __func__);

        SPtr_BlockInfoVec sptr_blocks = sptr_clsender_->GetBlockInfoVec(bundle);

        ASSERT(sptr_blocks != nullptr);


        if (client_service_id == 2) {
            buffer[buf_idx] = 1;

            buf_idx += 1;
            buf_unused -= 1;
            bytes_produced_toward_checkpoint += 1;
        }

        bool force_send_seg = false;
        if (use_intermediate_checkpoints) {
            is_checkpoint = (bytes_produced_toward_checkpoint > bytes_per_checkpoint);
        }

        if (check_send_segment(buffer, buf_idx, buf_unused, 
                               my_seg_size, session_ptr, 
                               client_service_id, session_offset,
                               is_checkpoint, last_seg, force_send_seg))
        {
            // checkpoint segment was sent so reset the byte count and start a new segment list
            bytes_produced_toward_checkpoint = 0;
        }

        size_t bundle_offset = 0;
        while (!shutting_down_ && !complete) {

            bytes_produced = BundleProtocol::produce(bundle, sptr_blocks.get(),
                                                     buffer+buf_idx, bundle_offset, 
                                                     buf_unused, &complete);
            buf_idx += bytes_produced;
            buf_unused -= bytes_produced;

            bundle_offset += bytes_produced;

            bytes_produced_toward_checkpoint += bytes_produced;
            if (use_intermediate_checkpoints) {
                is_checkpoint = (bytes_produced_toward_checkpoint >= bytes_per_checkpoint);
            }

            // shouldn't happen but CYA to prevent infinite loop if not enough space in buffer to start 
            force_send_seg = !complete && (bytes_produced == 0) && (buf_idx > 0);

            // did this complete production of the last bundle?
            last_seg = complete && (bundles_processed == num_bundles);
            if (last_seg) {
                is_checkpoint = true;  // force checkpoint on last segment
            }

            if (check_send_segment(buffer, buf_idx, buf_unused, 
                                   my_seg_size, session_ptr, 
                                   client_service_id, session_offset,
                                   is_checkpoint, last_seg, force_send_seg))
            {
                if (is_checkpoint) {
                    // checkpoint segment was sent so reset the byte count
                   bytes_produced_toward_checkpoint = 0;
               }
            }
        }

        // finished with the xmit blocks so we can now free up that memory
        sptr_clsender_->Delete_Xmit_Blocks(bref);

        ++bundle_iter;
    }

    if (!shutting_down_) {
        // should always have sent the last segment and be at the end of the bundle list
        ASSERT(last_seg);
        ASSERT(bundle_iter == session_ptr->Bundle_List()->end());

        session_ptr->Set_Last_Packet_Time(parent_node_->AOS_Counter());

        // if sending green we can declare these bundles transmitted
        if (session_ptr->Send_Green()) {
            PostTransmitProcessing(session_ptr);
        }
    }

    free(buffer);
}

//----------------------------------------------------------------------
bool
LTPNode::Sender::check_send_segment(u_char* buffer, size_t& buf_used, size_t& buf_unused, 
                                    size_t my_seg_size, LTPSession* session_ptr, 
                                    int client_service_id, size_t& session_offset,
                                    bool is_checkpoint, bool last_seg, bool force_send_seg)
{
    bool ds_sent = false;

    // check to see if the segment (buffer) needs to be closed out and sent
    if (!last_seg && !force_send_seg && (buf_unused != 0)) {
        return ds_sent; // still room in the buffer
    }

    SPtr_LTPTimer retransmit_timer;


    // send this buffer as a segment (sets the DS checkpoint ID to the current value
    SPtr_LTPDataSegment sptr_ds_seg = std::make_shared<LTPDataSegment>(session_ptr);

    if (session_ptr->Send_Green()) {
        sptr_ds_seg->Set_RGN(GREEN_SEGMENT);
        is_checkpoint = false;   // checkpoints are not valid for Green mode
    } else {
        sptr_ds_seg->Set_RGN(RED_SEGMENT);
    }

    if (is_checkpoint) {
        sptr_ds_seg->Set_Checkpoint_ID(session_ptr->Get_Next_Checkpoint_ID());
    }

    sptr_ds_seg->Set_Client_Service_ID(client_service_id); 

    sptr_ds_seg->Set_Security_Mask(LTP_SECURITY_HEADER | LTP_SECURITY_TRAILER);

    // Encode the LTP header for transmission
    sptr_ds_seg->Encode_Buffer(buffer, buf_used, session_offset, is_checkpoint, last_seg);

    // the new data segment is not fully defined until after calling Encode_Buffer
    // - it can now be properly added to LTP_DS_MAPs

    // generating the new segments so no possible overlap
    session_ptr->AddSegment_outgoing(sptr_ds_seg);

    // update the session offset for the next segment
    session_offset += buf_used;

    if (is_checkpoint) {
        // Add this Data Segment to the Session's list of checkpoint DSs
        session_ptr->Add_Checkpoint_Seg(sptr_ds_seg.get());

        // create a retransmit timer
        SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();
        SPtr_LTPSegment sptr_ltp_seg = sptr_ds_seg;

        retransmit_timer = sptr_ds_seg->Create_Retransmission_Timer(sndr_if, LTP_EVENT_DS_TO, retran_interval_, sptr_ltp_seg);
    }

    ++stats_.total_snt_ds_;

    oasys::ScopeLock scoplok(sptr_ds_seg->lock(), __func__);
    if (!sptr_ds_seg->Queued_To_Send()) {
        sptr_ds_seg->Set_Queued_To_Send(true);
        sptr_clsender_->Send_DataSeg_Low_Priority(sptr_ds_seg, retransmit_timer);
        ds_sent = true;
    }

    // reset the buffer parameters so it can be resued
    buf_used = 0;
    buf_unused = my_seg_size;

    return ds_sent;
}


//----------------------------------------------------------------------
bool
LTPNode::Sender::process_bundles_using_a_file(LTPSession* session_ptr)
{
    // return false if error opening or writing to the data file so that the session can be sent
    // using memory segments

    if (start_shutting_down_) {
        return true;
    }


    oasys::ScopeLock bl(session_ptr->Bundle_List()->lock(), "process_bundles()"); 

    
    remove_expired_bundles(session_ptr);

    size_t  num_bundles = session_ptr->Bundle_List()->size();

    if (num_bundles == 0) {
        // all bundles expired so remove this session
        cleanup_sender_session(session_ptr->Engine_ID(), session_ptr->Session_ID(), 0, 0, 
                               LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES);
        return true;
    }


    Bundle* bundle = nullptr;
    BundleRef bref("Sender::process_bundles()");  
    BundleList::iterator bundle_iter; 


    size_t bundles_processed    = 0;
    size_t bytes_produced       = 0;
    size_t session_size         = 0;

    int    client_service_id = 1;   // 1 = Bundle Protocol (single bundle for CCSDS or multiple for ION)

    if (ccsds_compatible_) {
        if (num_bundles > 0) {
            client_service_id = 2;  // 2 = LTP Service Data Aggregation
        }
    }



    // init the working buffer (10MB)
    work_buf_.clear();
    work_buf_.reserve(10000000);

    std::string file_path;
    session_ptr->set_file_usage(sptr_clsender_->Dir_Path());
    SPtr_LTPDataFile sptr_file = session_ptr->open_session_file_write(file_path);

    if (!sptr_file || (sptr_file->data_file_fd_ <= 0)) {
        return false;  // fall back to generating segments in memory
    }

    bool force_write = false;


    // generate the session data into the file
    bundle_iter = session_ptr->Bundle_List()->begin(); 
    while (!start_shutting_down_ && (bundle_iter != session_ptr->Bundle_List()->end()))
    {
        bundles_processed += 1;

        bool complete = false;

        bundle = *bundle_iter;
        bref     = bundle;

        oasys::ScopeLock scoplok(bref->lock(), __func__);

        SPtr_BlockInfoVec sptr_blocks = sptr_clsender_->GetBlockInfoVec(bundle);

        ASSERT(sptr_blocks != nullptr);


        if (client_service_id == 2) {
            *(work_buf_.end()) = 1;
            work_buf_.fill(1);


            if (!check_write_to_file(sptr_file.get(), file_path, session_size, force_write)) {
                log_err("error writing to outgoing LTP session file: %s - reverting to using memory",
                        file_path.c_str());
                return false;
            }
        }

        size_t bundle_offset = 0;
        while (!shutting_down_ && !complete) {

            bytes_produced = BundleProtocol::produce(bundle, sptr_blocks.get(),
                                                     (u_char*)work_buf_.end(), bundle_offset, 
                                                     work_buf_.tailbytes(), &complete);
            bundle_offset += bytes_produced;

            work_buf_.fill(bytes_produced);
            if (!check_write_to_file(sptr_file.get(), file_path, session_size, force_write)) {
                log_err("error writing to outgoing LTP session file: %s - reverting to using memory",
                        file_path.c_str());
                return false;
            }
        }

        // finished with the xmit blocks so we can now free up that memory
        sptr_clsender_->Delete_Xmit_Blocks(bref);

        ++bundle_iter;
    }

    force_write = true;
    if (!check_write_to_file(sptr_file.get(), file_path, session_size, force_write)) {
        log_err("error writing to outgoing LTP session file: %s - reverting to using memory",
                file_path.c_str());
        return false;
    }

    // generate the segments providing each with an offset into the data file and the 
    // length of for the segment

    // use a copy of the seg_size_ in case it gets changed while processing this session
    size_t my_seg_size = seg_size_;
    size_t session_offset    = 0;
    size_t data_size = 0;
    size_t bytes_remaining = session_size;

    bool is_checkpoint = false;
    size_t bytes_per_checkpoint = sptr_clsender_->Bytes_Per_CheckPoint();
    size_t bytes_produced_toward_checkpoint = 0;

    bool use_intermediate_checkpoints = (bytes_per_checkpoint > 0) && (session_size > bytes_per_checkpoint);

    ::close(sptr_file->data_file_fd_);
    sptr_file->data_file_fd_ = -1;
    sptr_file.reset();

    sptr_file = session_ptr->open_session_file_read(file_path);

    int  num_segs = 0;
    bool last_seg = false;  // is this the last segment for the LTP Session?

    while (!start_shutting_down_ && (bytes_remaining > 0))
    {
        if (bytes_remaining > my_seg_size) {
            data_size = my_seg_size;
        } else {
            data_size = bytes_remaining;
            last_seg = true;
        }

        if (use_intermediate_checkpoints) {
            bytes_produced_toward_checkpoint += data_size;
            is_checkpoint = (bytes_produced_toward_checkpoint >= bytes_per_checkpoint);
        }

        if (last_seg) {
            is_checkpoint = true;
        }
        
        ++num_segs;

        // session_offset updated in this call
        send_segment_using_file(sptr_file, file_path, session_offset, data_size, session_ptr, 
                                client_service_id, is_checkpoint, last_seg);


        if (is_checkpoint) {
            // checkpoint segment was sent so reset the byte count
            bytes_produced_toward_checkpoint = 0;
        }

        bytes_remaining -= data_size;
    }

    if (!shutting_down_) {
        session_ptr->Set_Last_Packet_Time(parent_node_->AOS_Counter());

        // if sending green we can declare these bundles transmitted
        if (session_ptr->Send_Green()) {
            PostTransmitProcessing(session_ptr);
        }
    }

    return true;
}

//----------------------------------------------------------------------
bool
LTPNode::Sender::check_write_to_file(LTPDataFile* file_ptr, std::string& file_path, size_t& session_size, bool force_write)
{
    bool result = true;

    if (force_write || (work_buf_.tailbytes() == 0)) {
        ssize_t cc = ::write(file_ptr->data_file_fd_, work_buf_.start(), work_buf_.fullbytes());
        if (cc != (ssize_t) work_buf_.fullbytes()) {
            result = false;
            log_err("Error writing %zu bytes to outgoing LTP data file: %s",
                    work_buf_.fullbytes(), file_path.c_str());
        } else {
            session_size += work_buf_.fullbytes();
            work_buf_.consume(work_buf_.fullbytes());
        }

        work_buf_.compact();
    }

    return result;
}


//----------------------------------------------------------------------
void
LTPNode::Sender::send_segment_using_file(SPtr_LTPDataFile sptr_file, std::string& file_path, size_t& session_offset, size_t data_size,
                                    LTPSession* session_ptr, int client_service_id,
                                    bool is_checkpoint, bool last_seg)
{
    SPtr_LTPTimer retransmit_timer;


    // send this buffer as a segment (sets the DS checkpoint ID to the current value
    SPtr_LTPDataSegment sptr_ds_seg = std::make_shared<LTPDataSegment>(session_ptr);

    if (session_ptr->Send_Green()) {
        sptr_ds_seg->Set_RGN(GREEN_SEGMENT);
        is_checkpoint = false;   // checkpoints are not valid for Green mode
    } else {
        sptr_ds_seg->Set_RGN(RED_SEGMENT);
    }

    if (is_checkpoint) {
        sptr_ds_seg->Set_Checkpoint_ID(session_ptr->Get_Next_Checkpoint_ID());
    }

    sptr_ds_seg->Set_Client_Service_ID(client_service_id); 

    sptr_ds_seg->Set_Security_Mask(LTP_SECURITY_HEADER | LTP_SECURITY_TRAILER);

    sptr_ds_seg->Set_File_Usage(sptr_file, file_path, session_offset, data_size, last_seg);

    // Encode the LTP header for transmission
    sptr_ds_seg->Encode_LTP_DS_Header();

    // the new data segment is not fully defined until after calling Encode_Buffer
    // - it can now be properly added to LTP_DS_MAPs

    // generating the new segments so no possible overlap
    session_ptr->AddSegment_outgoing(sptr_ds_seg);

    // update the session offset for the next segment
    session_offset += data_size;

    if (is_checkpoint) {
        // Add this Data Segment to the Session's list of checkpoint DSs
        session_ptr->Add_Checkpoint_Seg(sptr_ds_seg.get());

        // create a retransmit timer
        SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();
        SPtr_LTPSegment sptr_ltp_seg = sptr_ds_seg;

        retransmit_timer = sptr_ds_seg->Create_Retransmission_Timer(sndr_if, LTP_EVENT_DS_TO, retran_interval_, sptr_ltp_seg);
    }

    ++stats_.total_snt_ds_;

    oasys::ScopeLock scoplok(sptr_ds_seg->lock(), __func__);
    if (!sptr_ds_seg->Queued_To_Send()) {
        sptr_ds_seg->Set_Queued_To_Send(true);
        sptr_clsender_->Send_DataSeg_Low_Priority(sptr_ds_seg, retransmit_timer);
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::remove_expired_bundles(LTPSession* session_ptr)
{
    Bundle* bundle = nullptr;
    BundleList::iterator bundle_iter; 
    BundleList::iterator deletion_iter;

    deletion_iter = session_ptr->Bundle_List()->end(); 

    // make a quick pass through the list of bundles to see if any expired while in the queue
    for (bundle_iter =  session_ptr->Bundle_List()->begin(); 
        bundle_iter != session_ptr->Bundle_List()->end(); 
        ++bundle_iter)
    {
        if (deletion_iter != session_ptr->Bundle_List()->end()) {
            session_ptr->Bundle_List()->erase(deletion_iter);
            deletion_iter = session_ptr->Bundle_List()->end();
        }

        if (shutting_down_) {
            return;
        }

        bundle = *bundle_iter;
        oasys::ScopeLock scoplok(bundle->lock(), __func__);

        // check for expired/deleted bundle
        if (bundle->expired() || bundle->manually_deleting())
        {
            // do not add to the LTP session
            // must unlock before deleting from the inflight queue
            scoplok.unlock();

            if (session_ptr->Send_Red()) {
                BundleRef bref(__func__);
                bref = bundle;

                if (!sptr_clsender_->Del_From_InFlight_Queue(bref)) {
                    log_err("Failed to delete from InFLight Queue expired bundle *%p", bundle);
                }
            }
            deletion_iter = bundle_iter;

            bundle->set_expired_in_link_queue();
            ++stats_.bundles_expired_in_queue_;
        }
    }

    if (deletion_iter != session_ptr->Bundle_List()->end()) {
        session_ptr->Bundle_List()->erase(deletion_iter);
    }
}


//----------------------------------------------------------------------
bool
LTPNode::Sender::RSIF_shutting_down()
{
    return shutting_down_;
}

//----------------------------------------------------------------------
void
LTPNode::Sender::RSIF_post_timer_to_process(oasys::SPtr_Timer event)
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
LTPNode::Sender::RSIF_retransmit_timer_triggered(int event_type, SPtr_LTPSegment sptr_retran_seg)
{
    if (event_type == LTP_EVENT_DS_TO) {
        data_seg_timeout(sptr_retran_seg);
    } else if (event_type == LTP_EVENT_CS_TO) {
        cancel_seg_timeout(sptr_retran_seg);
    } else {
        log_err("Unexpected RetransmitSegTimer triggerd: %s", LTPSegment::Get_Event_Type(event_type));
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::RSIF_inactivity_timer_triggered(uint64_t engine_id, uint64_t session_id)
{
    //XXX/dz Sender no longer uses an Inactivity Timer

    SPtr_LTPSession sptr_session = find_sender_session(engine_id, session_id);

    if (sptr_session) {
        if (sptr_session->Session_State() == LTPSession::LTP_SESSION_STATE_DS) {
            time_t time_left = sptr_session->Last_Packet_Time() +  inactivity_interval_;
            time_left = time_left - parent_node_->AOS_Counter();

            if (time_left > 0)
            {
                SPtr_LTPNodeRcvrSndrIF sndr_rsif = parent_node_->sender_rsif_sptr();
                sptr_session->Start_Inactivity_Timer(sndr_rsif, time_left); // catch the remaining seconds...
            } else {
                //log_debug("Sender cancelling session due to inactivity timer");

                build_CS_segment(sptr_session.get(), LTP_SEGMENT_CS_BS, 
                                 LTPCancelSegment::LTP_CS_REASON_RXMTCYCEX);
            }
        } else {
            //log_always("%s): %s - Inactivity Timeout ignored - session no longer in DS state - state = %s ",
            //           __func__, sptr_session->key_str().c_str(), sptr_session->Get_Session_State());
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
LTPNode::Sender::data_seg_timeout(SPtr_LTPSegment sptr_ltp_seg)
{ 
    if (!sptr_ltp_seg->IsDeleted()) {
        // Okay to resend segment if retries not exceeded

        SPtr_LTPSession sptr_session = find_sender_session(sptr_ltp_seg->Engine_ID(), sptr_ltp_seg->Session_ID());

        if (sptr_session != nullptr) {
            // for logging
            std::string key_str = sptr_session->key_str();

            oasys::ScopeLock scoplok_session(sptr_session->SegMap_Lock(), __func__);

            if (!sptr_session->Is_LTP_Cancelled() && !sptr_ltp_seg->IsDeleted()) {
                SPtr_LTPDataSegment sptr_ds_seg = std::dynamic_pointer_cast<LTPDataSegment>(sptr_ltp_seg);

                if (sptr_ds_seg != nullptr) {

                    oasys::ScopeLock scoplok(sptr_ds_seg->lock(), __func__);

                    if (sptr_ds_seg->Queued_To_Send()) {
                        // about to be transmitted so create a new timer for the CLA to start
                        SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();
                        SPtr_LTPTimer timer;
                        timer = sptr_ds_seg->Renew_Retransmission_Timer(sndr_if, LTP_EVENT_DS_TO, retran_interval_, sptr_ltp_seg);

                    } else {
                        sptr_ds_seg->Increment_Retries();

                        if (sptr_ds_seg->Retrans_Retries() <= retran_retries_) {
                            update_session_counts(sptr_session.get(), LTPSession::LTP_SESSION_STATE_DS); 
                    
                            SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

                            SPtr_LTPTimer timer;
                            timer = sptr_ds_seg->Renew_Retransmission_Timer(sndr_if, LTP_EVENT_DS_TO, retran_interval_, sptr_ltp_seg);

                            ++stats_.ds_segment_resends_;

                            sptr_ds_seg->Set_Queued_To_Send(true);

                            sptr_clsender_->Send_DataSeg_Higher_Priority(sptr_ds_seg, timer);

                            sptr_session->Set_Last_Packet_Time(parent_node_->AOS_Counter());
                        } else {
                            sptr_ds_seg->Set_Deleted();

                            log_warn("Cancelling by sender - LTP Session: %s because DS retransmission retries exceeded",
                                    sptr_session->key_str().c_str());

                            build_CS_segment(sptr_session.get(), LTP_SEGMENT_CS_BS, 
                                             LTPCancelSegment::LTP_CS_REASON_RXMTCYCEX);

                        }
                    }
                } else {
                    log_err("DS timeout - error casting seg to ds_seg!");
                }
            } else {
                //if (!sptr_session->Is_LTP_Cancelled() && sptr_ds_seg->IsDeleted()) {
                //    log_always("DS timeout - Session: %s - ignoring because DS is deleted", key_str.c_str());
                //}
            }
        } else {
            //log_always("DS timeout - ignoring - session not found");
        }
    } else {
        //SPtr_LTPSession sptr_session = find_sender_session(sptr_ltp_seg->Engine_ID(), sptr_ltp_seg->Session_ID());
        //if (sptr_session != nullptr) {
            //log_always("DS timeout - Session: %s (%s) - ignoring since flagged as deleted", 
            //           sptr_ltp_seg->session_key_str().c_str(), 
            //           (sptr_session == nullptr) ? "not found":"foound");
        //}
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::cancel_seg_timeout(SPtr_LTPSegment sptr_ltp_seg)
{ 
    if (!sptr_ltp_seg->IsDeleted()) {
        // Okay to resend segment if retries not exceeded

        SPtr_LTPSession sptr_session = find_sender_session(sptr_ltp_seg->Engine_ID(), sptr_ltp_seg->Session_ID());

        if (sptr_session != nullptr) {
            // for logging
            std::string key_str = sptr_session->key_str();

            SPtr_LTPCancelSegment cs_seg = std::dynamic_pointer_cast<LTPCancelSegment>(sptr_ltp_seg);

            if (cs_seg != nullptr) {

                cs_seg->Increment_Retries();

                if (cs_seg->Retrans_Retries() <= retran_retries_) {
                    SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

                    SPtr_LTPTimer timer;
                    timer = cs_seg->Renew_Retransmission_Timer(sndr_if, LTP_EVENT_CS_TO, retran_interval_, sptr_ltp_seg);

                    ++stats_.cancel_by_sndr_segs_;

                    //log_debug("resend cancel by sender for %s", sptr_ltp_seg->session_key_str().c_str());


                    sptr_clsender_->Send_Admin_Seg_Highest_Priority(cs_seg->asNewString(), timer, false);
                } else {
                    cs_seg->Set_Deleted();

                    erase_send_session(sptr_session.get(), true);
                }
            } else {
                log_err("CS timeout - error casting seg to cs_seg!");
            }
        } else {
            //log_always("CS timeout - ignoring - session not found");
        }
    } else {
        //log_always("CS timeout - Session: %s - ignoring since flagged as deleted", sptr_ltp_seg->session_key_str().c_str());
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

    LTP_DS_MAP::iterator cleanup_segment_iterator;


    SPtr_LTPSession sptr_session = find_sender_session(engine_id, session_id);

    if (sptr_session != nullptr) {

        sptr_session->Cancel_Inactivity_Timer();

        switch(event_type)
        { 
            case LTP_SEGMENT_CS_BR:

                 //log_debug("cleanup_sender_session(CS_BR): Session: %s - Cancel by Receiver received - delete session",
                 //          sptr_session->key_str().c_str());

                 // signal DTN that transmit failed for the bundle(s)
                 PostTransmitProcessing(sptr_session.get(), false);
                 break;

            case LTP_EVENT_SESSION_COMPLETED: 
                 //log_debug("cleanup_sender_session(COMPLETED): Session: %s - delete session",
                 //          sptr_session->key_str().c_str());

                 cancelled = false;
                 PostTransmitProcessing(sptr_session.get());
                 break;

            case LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES:
                 // all bundles were expired - pass through and delete the session
                 PostTransmitProcessing(sptr_session.get(), false);
                 break;


            default: // all other cases don't lookup a segment...
                 
                 log_err("cleanup_sender_session: unknown event type: %d", event_type);
                 return;
        }

        erase_send_session(sptr_session.get(), cancelled);

    } else {
        log_debug("cleanup_sender_session(CS_BR): Session: %zzu:zu - not found",
                  engine_id, session_id);
    }

    return;
}

//----------------------------------------------------------------------
SPtr_LTPSession
LTPNode::Sender::find_sender_session(LTPSegment* seg_ptr)
{
    return find_sender_session(seg_ptr->Engine_ID(), seg_ptr->Session_ID());
}

//----------------------------------------------------------------------
SPtr_LTPSession
LTPNode::Sender::find_sender_session(uint64_t engine_id, uint64_t session_id)
{
    SPtr_LTPSession sptr_session;

    QPtr_LTPSessionKey qkey = QPtr_LTPSessionKey(new LTPSessionKey(engine_id, session_id));

    SESS_SPTR_MAP::iterator iter; 

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    iter = send_sessions_.find(qkey.get());
    if (iter != send_sessions_.end())
    {
        sptr_session = iter->second;
    }

    return sptr_session;
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

        sptr_loading_session_ = std::make_shared<LTPSession>(local_engine_id_, session_id);
        sptr_loading_session_->SetAggTime();
        sptr_loading_session_->Set_Outbound_Cipher_Key_Id(sptr_clsender_->Outbound_Cipher_Key_Id());
        sptr_loading_session_->Set_Outbound_Cipher_Suite(sptr_clsender_->Outbound_Cipher_Suite());
        sptr_loading_session_->Set_Outbound_Cipher_Engine(sptr_clsender_->Outbound_Cipher_Engine());

        send_sessions_[sptr_loading_session_.get()] = sptr_loading_session_;

    
        if (send_sessions_.size() > stats_.max_sessions_) {
            stats_.max_sessions_ = send_sessions_.size();
        }

        ++stats_.total_sessions_;
    }
    
    sptr_clsender_->Set_Ready_For_Bundles(is_ready);

    return result;
}

//----------------------------------------------------------------------
void
LTPNode::Sender::erase_send_session(LTPSession* session_ptr, bool cancelled)
{
    ltp_engine_sptr_->closed_session(session_ptr->Session_ID(), remote_engine_id_, cancelled);

    // start an InactivityTimer to eventually delete the closed out session from Engine's list
    SPtr_LTPNodeRcvrSndrIF sndr_if = parent_node_->sender_rsif_sptr();

    // allow plenty of time to handle late RS's if the other side gets backed up
    uint32_t closeout_secs = std::max(inactivity_interval_, 3600U);
    session_ptr->Start_Closeout_Timer(sndr_if, closeout_secs);

    oasys::ScopeLock scoplok(&session_list_lock_, __func__);

    send_sessions_.erase(session_ptr);

    update_session_counts(session_ptr, LTPSession::LTP_SESSION_STATE_UNDEFINED); 

    // less than or equal here for case where last session is currently being loaded
    bool is_ready = !shutting_down_ && 
                    ((send_sessions_.size() - sessions_state_cs_) <= max_sessions_);
    sptr_clsender_->Set_Ready_For_Bundles(is_ready);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::force_cancel_by_sender(LTPDataSegment* ds_seg)
{
    SPtr_LTPSession sptr_session = find_sender_session(ds_seg);

    if (sptr_session != nullptr) {
        ds_seg->Set_Deleted();

        build_CS_segment(sptr_session.get(), LTP_SEGMENT_CS_BS, 
                         LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD);
    } else {
        log_err("%s: unable to find session: %s", __func__, ds_seg->session_key_str().c_str());
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::reconfigured()
{
    max_sessions_ = sptr_clsender_->Max_Sessions();
    agg_size_     = sptr_clsender_->Agg_Size();
    seg_size_     = sptr_clsender_->Seg_Size();
    ccsds_compatible_    = sptr_clsender_->CCSDS_Compatible();
    retran_retries_      = sptr_clsender_->Retran_Retries();
    retran_interval_     = sptr_clsender_->Retran_Intvl();
    inactivity_interval_ = sptr_clsender_->Inactivity_Intvl();

    if (agg_time_millis_ != sptr_clsender_->Agg_Time()) {
        // wake up the Sender thread if the agg_time has changed
        std::lock_guard<std::mutex> scoplok(agg_time_lock_);

        agg_time_millis_ = sptr_clsender_->Agg_Time();

        cond_var_agg_time_changed_.notify_all();
    }

    bundle_processor_->reconfigured();
}


//----------------------------------------------------------------------
LTPNode::Sender::SndrBundleProcessor::SndrBundleProcessor(Sender* parent_sndr, uint64_t engine_id)
    : Logger("LTPNode::Sender::SndrBundleProcessor",
             "/dtn/ltp/node/%zu/sender/bundleproc", engine_id),
      Thread("LTPNode::Sender::SndrBundleProcessor")
{
    parent_sndr_ = parent_sndr;

    queued_bytes_quota_ = parent_sndr_->ltp_queued_bytes_quota();
    
}

//----------------------------------------------------------------------
LTPNode::Sender::SndrBundleProcessor::~SndrBundleProcessor()
{
    shutdown();

    if (me_eventq_.size() > 0) {
        EventObj* event;
        while (me_eventq_.try_pop(&event)) {
            delete event;
        }
    }
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::start_shutdown()
{
    start_shutting_down_ = true;
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::shutdown()
{
    start_shutting_down_ = true;
    shutting_down_ = true;

    set_should_stop();
    while (!is_stopped()) {
        usleep(100000);
    }
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
    queue_size = me_eventq_.size();
    bytes_queued = bytes_queued_;
    bytes_queued_max = bytes_queued_max_;
}

//----------------------------------------------------------------------
bool
LTPNode::Sender::SndrBundleProcessor::okay_to_queue()
{
    //XXX/dz not currently called...


    oasys::ScopeLock scoplok(&lock_, __func__);

    return (queued_bytes_quota_ == 0) || (bytes_queued_ < queued_bytes_quota_);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::post(SPtr_LTPSession sptr_session)
{
    // Sender goes into shutdown mode before the SndrBundleProcessor so
    // continue to accept new posts from it even if our shutting_down_ flag is set

    EventObj* event = new EventObj();
    event->sptr_session_ = sptr_session;

    me_eventq_.push_back(event);
}

//----------------------------------------------------------------------
void
LTPNode::Sender::SndrBundleProcessor::run()
{
    char threadname[16] = "LTPSndBndlProc";
    pthread_setname_np(pthread_self(), threadname);

    EventObj* event;
    bool ok;

    while (!should_stop()) {
        if (me_eventq_.size() > 0) {
            ok = me_eventq_.try_pop(&event);
            ASSERT(ok);

            if (!start_shutting_down_) {
                parent_sndr_->process_bundles(event->sptr_session_.get());
            }

            event->sptr_session_.reset();
            delete event;
            
        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }
}







//----------------------------------------------------------------------
LTPNode::TimerProcessor::TimerProcessor(uint64_t remote_engine_id)
    : Logger("LTPNode::TimerProcessor",
             "/dtn/ltp/node/%zu/timerproc", remote_engine_id),
      Thread("LTPNode::TimerProcessor")
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
LTPNode::TimerProcessor::post(oasys::SPtr_Timer sptr_event)
{
    me_eventq_.push_back(sptr_event);
}

/// TimerProcessor main loop
void
LTPNode::TimerProcessor::run() 
{
    char threadname[16] = "LTPSndTimerProc";
    pthread_setname_np(pthread_self(), threadname);
   

    oasys::SPtr_Timer sptr_event;
    SPtr_LTPTimer sptr_ltp_timer;


    while (!should_stop()) {
        if (me_eventq_.size() > 0) {
            me_eventq_.try_pop(&sptr_event);

            if (sptr_event) {
                sptr_ltp_timer = std::dynamic_pointer_cast<LTPTimer>(sptr_event);

                if (sptr_ltp_timer) {
                    sptr_ltp_timer->do_timeout_processing();

                    // NOTE: these timers delete themselves when the shared_ptrs release
                    sptr_ltp_timer.reset();
                }
            }
            sptr_event.reset();

        } else {
            me_eventq_.wait_for_millisecs(100); // millisecs to wait
        }
    }
}

} // namespace dtn

