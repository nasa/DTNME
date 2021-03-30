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
#    include <dtn-config.h>
#endif

#include <sys/time.h>
#include <errno.h>



#include <third_party/oasys/util/HexDumpBuffer.h>

#include "LTPCommon.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"

#define CHECK_DECODE_STATUS  \
if (decode_status > 0) { \
    current_byte += decode_status;\
} else { \
    is_valid_=false;\
    return -1; \
}

namespace dtn
{

//--------------------------------------------------------------------------
LTPTimer::LTPTimer(SPtr_LTPNodeRcvrSndrIF& rsif, int event_type, uint32_t seconds)
        : Logger("LTPTimer", "/dtn/ltp/timer"),
          rsif_(rsif),
          event_type_(event_type)
{  
    set_seconds(seconds);
}  

//--------------------------------------------------------------------------
LTPTimer::~LTPTimer()
{
}

//--------------------------------------------------------------------------
void    
LTPTimer::set_seconds(int seconds)
{
    seconds_ = seconds; 
    if (rsif_ != nullptr) {
        target_counter_ = seconds + rsif_->RSIF_aos_counter();
    }
}

//--------------------------------------------------------------------------
void    
LTPTimer::start()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    if ((sptr_ != nullptr) && (rsif_ != nullptr)) {
        set_seconds(seconds_);
        schedule_in(seconds_*1000 + 100, sptr_);
    } else if (!cancelled()) {
        log_err("Attempt to start LTPTimer(%p) without a SPtr_Timer defined and is not cancelled", this);
        sptr_ = nullptr;
        rsif_ = nullptr;
    }
}

//--------------------------------------------------------------------------
bool    
LTPTimer::cancel()
{
    bool result = false;

    oasys::ScopeLock scoplok(&lock_, __func__);

    if (sptr_ != nullptr) {
        result = oasys::SharedTimer::cancel(sptr_);
        sptr_ = nullptr;
    }

    rsif_ = nullptr;

    return result;
}

//--------------------------------------------------------------------------
void    
LTPTimer::timeout(const struct timeval &now)
{
    (void)now;

    oasys::ScopeLock scoplok(&lock_, __func__);

    if (cancelled() || (sptr_ == nullptr)) {
        // clear the internal reference so this object will be deleted
        sptr_ = nullptr;
    } else {
        // quickly post this for processing
        // in order to release the TimerSystem to go back to its job

        SPtr_LTPNodeRcvrSndrIF my_rsif = rsif_;

        // release the lock now that we hava a safe pointer to use
        scoplok.unlock();

        if (my_rsif != nullptr)
            my_rsif->RSIF_post_timer_to_process(sptr_);
        else {
            // clear the internal reference so this object will be deleted
            sptr_ = nullptr;
        }
    }
}







//--------------------------------------------------------------------------
LTPRetransmitSegTimer::LTPRetransmitSegTimer(SPtr_LTPNodeRcvrSndrIF& rsif, int event_type, uint32_t seconds,
                                             SPtr_LTPSegment& retransmit_seg)
        : LTPTimer(rsif, event_type, seconds),
          retransmit_seg_(retransmit_seg)
{
    BundleDaemon::instance()->inc_ltp_retran_timers_created();
}

//--------------------------------------------------------------------------
LTPRetransmitSegTimer::~LTPRetransmitSegTimer()
{
    BundleDaemon::instance()->inc_ltp_retran_timers_deleted();
}

//--------------------------------------------------------------------------
bool    
LTPRetransmitSegTimer::cancel()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    retransmit_seg_ = nullptr;

    return LTPTimer::cancel();
}

//--------------------------------------------------------------------------
void    
LTPRetransmitSegTimer::do_timeout_processing()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    if (!cancelled() && (sptr_ != nullptr)) {

        // get a safe-to-use copy of the RSIF pointer
        SPtr_LTPNodeRcvrSndrIF my_rsif = rsif_;
        SPtr_LTPSegment        my_seg = retransmit_seg_;

        if ((my_rsif != nullptr) && (my_seg != nullptr)) {

            uint64_t right_now = my_rsif->RSIF_aos_counter();

            if (right_now >= target_counter_)
            {
                // no need to reschedule this timer so clear the internal reference 
                // so this object will be destroyed
                sptr_ = nullptr;
                scoplok.unlock();

                my_rsif->RSIF_retransmit_timer_triggered(event_type_, my_seg);

                retransmit_seg_ = nullptr;
                rsif_ = nullptr;
            }
            else
            {  // invoke a new timer!
                int seconds = (int) (target_counter_ - right_now);

                schedule_in(seconds*1000, sptr_);
            }
        }
        else
        {
            sptr_ = nullptr;
            rsif_ = nullptr;
            retransmit_seg_ = nullptr;
        }
    } else {
        sptr_ = nullptr;
        rsif_ = nullptr;
        retransmit_seg_ = nullptr;
    }
}


//--------------------------------------------------------------------------
LTPCloseoutTimer::LTPCloseoutTimer(SPtr_LTPNodeRcvrSndrIF& rsif, 
                                   uint64_t engine_id, uint64_t session_id)
        : LTPTimer(rsif, LTP_EVENT_CLOSEOUT_TIMEOUT, 30),
          engine_id_(engine_id),
          session_id_(session_id)
{
    BundleDaemon::instance()->inc_ltp_closeout_timers_created();

    //log_always("dzdebug - create closeout timer for Session: %zu:%zu", engine_id_, session_id_);
}

//--------------------------------------------------------------------------
LTPCloseoutTimer::~LTPCloseoutTimer()
{
    BundleDaemon::instance()->inc_ltp_closeout_timers_deleted();
}

//--------------------------------------------------------------------------
void    
LTPCloseoutTimer::do_timeout_processing()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    //if (sptr_ != nullptr) {
    //    log_always("dzdebug - closeout timer triggered for Session: %zu:%zu - sptr refs: %ld", 
    //               engine_id_, session_id_, sptr_.use_count());
    //}

    if (!cancelled() && (sptr_ != nullptr)) {

        // get a safe-to-use copy of the RSIF pointer
        SPtr_LTPNodeRcvrSndrIF my_rsif = rsif_;

        if (my_rsif != nullptr) {

            uint64_t right_now = my_rsif->RSIF_aos_counter();

            if (right_now >= target_counter_)
            {
                // no need to reschedule this timer so clear the internal reference 
                // so this object will be destroyed
                sptr_ = nullptr;
                scoplok.unlock();

                my_rsif->RSIF_closeout_timer_triggered(engine_id_, session_id_);

                rsif_ = nullptr;
            }
            else
            {  // invoke a new timer!
                int seconds = (int) (target_counter_ - right_now);

                schedule_in(seconds*1000, sptr_);
            }
        }
        else
        {
            // clear the internal reference so this object will be destroyed
            sptr_ = nullptr;
            rsif_ =  nullptr;
        }
    } else {
        // clear the internal reference so this object will be destroyed
        sptr_ = nullptr;
        rsif_ =  nullptr;
    }
}


//
//   LTPSession ......................
//
//----------------------------------------------------------------------    
LTPSession::LTPSession(uint64_t engine_id, uint64_t session_id, bool is_counted_session)
        : LTPSessionKey(engine_id, session_id),
          oasys::Logger("LTPSession","/dtn/ltp/session/%zu", session_id),
          is_counted_session_(is_counted_session)
{
    if (is_counted_session_) {
        BundleDaemon::instance()->inc_ltpsession_created();
    }

    init();
}

//----------------------------------------------------------------------    
LTPSession::~LTPSession()
{
    if (is_counted_session_) {
        BundleDaemon::instance()->inc_ltpsession_deleted();
    }

    Cancel_Inactivity_Timer();
    RemoveSegments();
    delete bundle_list_;
}

//----------------------------------------------------------------------    
void
LTPSession::init()
{
     red_eob_received_       = false;
     red_complete_           = false;
     green_complete_         = false;
     red_processed_          = false;
     green_processed_        = false;
     expected_red_bytes_     = 0;
     expected_green_bytes_   = 0;
     red_highest_offset_     = 0;
     session_size_           = 0;
     report_serial_number_   = 0;
     red_bytes_received_     = 0;
     inactivity_timer_       = nullptr;
     last_packet_time_       = 0;
     checkpoint_id_          = LTP_14BIT_RAND; // 1 through (2**14)-1=0x3FFF
     bundle_list_            = new BundleList(key_str());
     session_state_          = LTP_SESSION_STATE_CREATED;
     reports_sent_           = 0;
     inbound_cipher_suite_   = -1;
     inbound_cipher_key_id_  = -1;
     inbound_cipher_engine_  = "";
     outbound_cipher_suite_  = -1;
     outbound_cipher_key_id_ = -1;
     outbound_cipher_engine_ = "";
     is_security_enabled_    = false;
}

//----------------------------------------------------------------------    
std::string
LTPSession::key_str()
{
    char key[48];
    snprintf(key, sizeof(key), "%" PRIu64 ":%" PRIu64, engine_id_, session_id_);
    std::string result(key);

    return result;
}

//----------------------------------------------------------------------
void 
LTPSession::Set_Inbound_Cipher_Suite(int new_val)
{
    is_security_enabled_ = (new_val != -1) ? true : false;
    inbound_cipher_suite_ = new_val;
}
//----------------------------------------------------------------------
void
LTPSession::Set_Inbound_Cipher_Key_Id(int new_key)
{
    inbound_cipher_key_id_ = new_key;
}
//----------------------------------------------------------------------
void 
LTPSession::Set_Inbound_Cipher_Engine(std::string& new_engine)
{
    inbound_cipher_engine_ = new_engine;
}
//----------------------------------------------------------------------
void 
LTPSession::Set_Outbound_Cipher_Suite(int new_val)
{
    is_security_enabled_ = (new_val != -1) ? true : false;
    outbound_cipher_suite_ = new_val;
}
//----------------------------------------------------------------------
void
LTPSession::Set_Outbound_Cipher_Key_Id(int new_key)
{
    outbound_cipher_key_id_ = new_key;
}
//----------------------------------------------------------------------
void 
LTPSession::Set_Outbound_Cipher_Engine(std::string& new_engine)
{
    outbound_cipher_engine_ = new_engine;
}
//----------------------------------------------------------------------    
void 
LTPSession::SetAggTime()
{
    agg_start_time_.get_time();
}

//----------------------------------------------------------------------    
bool 
LTPSession::TimeToSendIt(uint32_t milliseconds)
{
    return agg_start_time_.elapsed_ms() >= milliseconds;
}


//----------------------------------------------------------------------    
void 
LTPSession::RemoveSegments()
{
    LTP_DS_MAP::iterator iter;
    SPtr_LTPDataSegment ds_seg;

    LTP_RS_MAP::iterator rs_iter;
    SPtr_LTPReportSegment rs_seg;

    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    // flag all segments as deleted for timer info and then erase them
    iter = red_segments_.begin();
    while (iter != red_segments_.end()) {
        iter->second->Set_Deleted();
        ++iter;
    }
    red_segments_.clear();

    // no timers associated with green segments so just erase them all
    green_segments_.clear();


    // flag all segments as deleted for timer info and then erase them
    rs_iter = report_segments_.begin();
    while (rs_iter != report_segments_.end()) {
        rs_iter->second->Set_Deleted();
        ++rs_iter;
    }
    report_segments_.clear();

    // clear the cancel segment
    Clear_Cancel_Segment();

    Cancel_Inactivity_Timer();
}

//----------------------------------------------------------------------    
void
LTPSession::Set_Cancel_Segment(SPtr_LTPCancelSegment& new_segment)
{
    // basically a map of one element :)
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    if (cancel_segment_ != nullptr)
    {
        cancel_segment_->Set_Deleted();
        cancel_segment_->Cancel_Retransmission_Timer();
        cancel_segment_ = nullptr;
    }

     cancel_segment_ = new_segment;
}

//----------------------------------------------------------------------    
void
LTPSession::Clear_Cancel_Segment()
{
    // basically a map of one element :)
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    if (cancel_segment_ != nullptr)
    {
        cancel_segment_->Set_Deleted();
        cancel_segment_->Cancel_Retransmission_Timer();
        cancel_segment_ = nullptr;
    }
}

//----------------------------------------------------------------------    
void 
LTPSession::Set_Checkpoint_ID(uint64_t in_checkpoint)
{
     checkpoint_id_  = in_checkpoint;
}

//----------------------------------------------------------------------    
uint64_t 
LTPSession::Increment_Checkpoint_ID()
{
     checkpoint_id_++;
     return checkpoint_id_;
}

//----------------------------------------------------------------------    
uint64_t 
LTPSession::Increment_Report_Serial_Number()
{
     report_serial_number_++;
     return report_serial_number_;
}

//----------------------------------------------------------------------
void
LTPSession::AddToBundleList(BundleRef bundle, size_t bundle_size)
{
     session_size_ += bundle_size;
     bundle_list_->push_back(bundle);
}

//----------------------------------------------------------------------    
void 
LTPSession::Set_Session_State(int new_session_state)
{
     session_state_  = new_session_state;
}

//----------------------------------------------------------------------    
void 
LTPSession::Set_Report_Serial_Number(uint64_t in_report_serial)
{ 
     report_serial_number_  = in_report_serial;
}

//----------------------------------------------------------------------    
void 
LTPSession::Set_EOB(LTPDataSegment * sop)
{
     if (sop->IsRed()) {
         expected_red_bytes_ = sop->Stop_Byte()+1;
         red_complete_ = true;
         sop->Set_Endofblock();
     } else if (sop->IsGreen()) {
         green_complete_ = true;
         expected_green_bytes_ = sop->Stop_Byte()+1;
         sop->Set_Endofblock();
     } else {
         log_err("Set_EOB: Session: %s - No Red or Green Data?",
                 key_str().c_str());
     }
}


//----------------------------------------------------------------------    
int 
LTPSession::RemoveClaimedSegments(LTP_DS_MAP * segments, size_t reception_offset, size_t reception_length)
{
    // return 1 to indicate that a checkpoint segment was claimed else zero
    // XXX/dz ION 3.3.0 limits report segments to 20 claims so it can take multiple
    //        RS to complete a report and we want to let the caller know if this
    //        claim completes the report
    int checkpoint_claimed = 0;
    if (segments->size() > 0) {
        LTP_DS_MAP::iterator seg_iter;
        LTP_DS_MAP::iterator del_seg_iter;

        for(seg_iter=segments->begin(); seg_iter!=segments->end(); ) 
        {
            SPtr_LTPDataSegment this_segment = seg_iter->second;
 
            if (this_segment->Offset() == reception_offset && this_segment->Payload_Length() == reception_length)
            {
                checkpoint_claimed = this_segment->IsCheckpoint() ? 1 : checkpoint_claimed;
                seg_iter->second->Set_Deleted();
                seg_iter->second->Cancel_Retransmission_Timer();

                segments->erase(seg_iter);
                break;
            }
            else if ((this_segment->Offset() >= reception_offset) && 
                     ((this_segment->Offset() + this_segment->Payload_Length() - 1) < reception_offset+reception_length))
            {
                checkpoint_claimed = this_segment->IsCheckpoint() ? 1 : checkpoint_claimed;
                seg_iter->second->Set_Deleted();
                seg_iter->second->Cancel_Retransmission_Timer();

                del_seg_iter = seg_iter;
                seg_iter++; // increment before we remove!
                segments->erase(del_seg_iter);
            } else {
                seg_iter++; // prevent endless loop ;)
            }
        }
    }

    return checkpoint_claimed;
}

//----------------------------------------------------------------------    
int 
LTPSession::AddSegment(LTPDataSegment* sop, bool check_for_overlap)
{
    int return_status = -1;

    //log_always("Session: %s: add segment - chkptr: %s  start: %zu  stop: %zu  length: %zu",
    //           sop->session_key_str().c_str(), sop->IsCheckpoint()?"true":"false",
    //           sop->Start_Byte(), sop->Stop_Byte(), (sop->Stop_Byte() - sop->Start_Byte() + 1));

    LTP_DS_MAP::iterator seg_iter;

    if ( sop->IsRedEndofblock() )  red_eob_received_ = true;

    switch(sop->Red_Green()) {

        case RED_SEGMENT:

          if (!check_for_overlap || CheckAndTrim(sop)) { // make sure there are no collisions...
              if (expected_red_bytes_ < (sop->Stop_Byte() + 1))
              {
                  expected_red_bytes_ = sop->Stop_Byte() + 1;
              }

              if ((sop->Stop_Byte() + 1) > red_highest_offset_)
              {
                  red_highest_offset_ = sop->Stop_Byte() + 1; 
              }

              red_bytes_received_ += sop->Payload_Length();

              SPtr_LTPDataSegment ds_sptr(sop);
              Red_Segments()[sop] = ds_sptr;

              return_status = 0; 
          }
          break;

        case GREEN_SEGMENT:

          if (CheckAndTrim(sop)) { // make sure there are no collisions...
              if (expected_green_bytes_ < (sop->Stop_Byte()+1)) {
                  expected_green_bytes_ = (sop->Stop_Byte()+1);
              }

              SPtr_LTPDataSegment ds_sptr(sop);
              Green_Segments()[sop] = ds_sptr;

              return_status = 0; 
          }
          break;

        default:
          // invalid red/green segment indication
          break;
    }

    return return_status;
}

//----------------------------------------------------------------------    
int 
LTPSession::AddSegment(SPtr_LTPDataSegment& ds_seg, bool check_for_overlap)
{
    int return_status = -1;

    LTP_DS_MAP::iterator seg_iter;

    if ( ds_seg->IsRedEndofblock() )  red_eob_received_ = true;

    switch(ds_seg->Red_Green()) {

        case RED_SEGMENT:

          if (!check_for_overlap || CheckAndTrim(ds_seg)) { // make sure there are no collisions...
              if (expected_red_bytes_ < (ds_seg->Stop_Byte() + 1))
              {
                  expected_red_bytes_ = ds_seg->Stop_Byte() + 1;
              }

              if ((ds_seg->Stop_Byte() + 1) > red_highest_offset_)
              {
                  red_highest_offset_ = ds_seg->Stop_Byte() + 1; 
              }

              red_bytes_received_ += ds_seg->Payload_Length();

              Red_Segments()[ds_seg.get()] = ds_seg;

              return_status = 0; 
          }
          break;

        case GREEN_SEGMENT:

          if (CheckAndTrim(ds_seg)) { // make sure there are no collisions...
              if (expected_green_bytes_ < (ds_seg->Stop_Byte()+1)) {
                  expected_green_bytes_ = (ds_seg->Stop_Byte()+1);
              }

              Green_Segments()[ds_seg.get()] = ds_seg;

              return_status = 0; 
          }
          break;

        default:
          // invalid red/green segment indication
          break;
    }

    return return_status;
}

//----------------------------------------------------------------------    
bool 
LTPSession::CheckAndTrim(LTPDataSegment * sop)
{
    LTP_DS_MAP::iterator ltpudp_segment_iterator_;
 
    bool return_stats = false;  // don't insert this segment    
    bool checking     = true; 
    int lookup_status = UNKNOWN_SEGMENT;

    switch(sop->Red_Green())
    {
        case RED_SEGMENT: 

          ltpudp_segment_iterator_ = Red_Segments().find(sop); // if we find an exact match ignore this one
          if (ltpudp_segment_iterator_ == Red_Segments().end()) {  // we didn't find one...
              while (checking)
              {
                  lookup_status = Check_Overlap(&Red_Segments(), sop); // keep looking until we have pruned the data
                  if (lookup_status == LOOKUP_COMPLETE)
                  {
                      checking = false;
                  }
                  if (lookup_status == DUPLICATE_SEGMENT)
                  {
                      break;
                  }
                  // if ADJUSTED simply keep looking...
              }
              if (lookup_status == LOOKUP_COMPLETE) return_stats = true; // we checked the list...
          } else {
              // exact match for an existing record - no need to insert this copy
          }
          break;

        case GREEN_SEGMENT:

          ltpudp_segment_iterator_ = Green_Segments().find(sop);   // if we find an exact match ignore this one
          if (ltpudp_segment_iterator_ == Green_Segments().end()) {  // we didn't find one...
              while (checking)
              { 
                  lookup_status =Check_Overlap(&Green_Segments(), sop); // keep looking until we have pruned the data
                  if (lookup_status == LOOKUP_COMPLETE)
                  {
                      checking = false;
                  }
                  if (lookup_status == DUPLICATE_SEGMENT)
                  {
                      break;
                  }
                  // if ADJUSTED simply keep looking...
              }
              if (lookup_status == LOOKUP_COMPLETE)return_stats = true; // we checked the list...
          }
          break;
    }
    return return_stats;
}  

//----------------------------------------------------------------------    
bool 
LTPSession::CheckAndTrim(SPtr_LTPDataSegment& ds_seg)
{
    LTP_DS_MAP::iterator ltpudp_segment_iterator_;
 
    bool return_stats = false;  // don't insert this segment    
    bool checking     = true; 
    int lookup_status = UNKNOWN_SEGMENT;

    switch(ds_seg->Red_Green())
    {
        case RED_SEGMENT: 

          ltpudp_segment_iterator_ = Red_Segments().find(ds_seg.get()); // if we find an exact match ignore this one
          if (ltpudp_segment_iterator_ == Red_Segments().end()) {  // we didn't find one...
              while (checking)
              {
                  lookup_status = Check_Overlap(&Red_Segments(), ds_seg); // keep looking until we have pruned the data
                  if (lookup_status == LOOKUP_COMPLETE)
                  {
                      checking = false;
                  }
                  if (lookup_status == DUPLICATE_SEGMENT)
                  {
                      break;
                  }
                  // if ADJUSTED simply keep looking...
              }
              if (lookup_status == LOOKUP_COMPLETE) return_stats = true; // we checked the list...
          } else {
              // exact match for an existing record - no need to insert this copy
          }
          break;

        case GREEN_SEGMENT:

          ltpudp_segment_iterator_ = Green_Segments().find(ds_seg.get());   // if we find an exact match ignore this one
          if (ltpudp_segment_iterator_ == Green_Segments().end()) {  // we didn't find one...
              while (checking)
              { 
                  lookup_status =Check_Overlap(&Green_Segments(), ds_seg); // keep looking until we have pruned the data
                  if (lookup_status == LOOKUP_COMPLETE)
                  {
                      checking = false;
                  }
                  if (lookup_status == DUPLICATE_SEGMENT)
                  {
                      break;
                  }
                  // if ADJUSTED simply keep looking...
              }
              if (lookup_status == LOOKUP_COMPLETE)return_stats = true; // we checked the list...
          }
          break;
    }
    return return_stats;
}  

//----------------------------------------------------------------------    
int 
LTPSession::Check_Overlap(LTP_DS_MAP * segments, LTPDataSegment * sop)
{
    ssize_t adjust        = 0;
    int return_status = UNKNOWN_SEGMENT;

    if (segments->empty()) return LOOKUP_COMPLETE;

    LTP_DS_MAP::iterator seg_iter = segments->upper_bound(sop);

    if (seg_iter == segments->end()) {
        --seg_iter;
    }

    while (true) {

        bool left_overlap = false;
        bool right_overlap = false;

        SPtr_LTPDataSegment this_segment = seg_iter->second;

        if (this_segment == nullptr) {
            return LOOKUP_COMPLETE;
        }

        if ((sop->Start_Byte() > this_segment->Stop_Byte())) {
            return LOOKUP_COMPLETE;
        }

        if (sop->Start_Byte() <= this_segment->Stop_Byte() &&
            sop->Start_Byte() >= this_segment->Start_Byte()) {
            // left is within
            left_overlap = true;
        }

        if (sop->Stop_Byte() <= this_segment->Stop_Byte() &&
            sop->Stop_Byte() >= this_segment->Start_Byte()) {
            // right is within
            right_overlap = true;
        }

        if (left_overlap && right_overlap)
        {
            return DUPLICATE_SEGMENT;  // we already have this data
        }

        if (sop->Start_Byte() <= this_segment->Start_Byte() &&
            sop->Stop_Byte() >= this_segment->Stop_Byte())
        {
            // toss out the smaller segment 
            segments->erase(this_segment.get());

            red_bytes_received_ -= this_segment->Payload_Length();
            return DELETED_SEGMENT;
        }

        if (left_overlap) 
        {
   
  //dzdebug
  log_always("left overlap - move data");

            adjust = this_segment->Stop_Byte()-sop->Offset();
            memmove(sop->Payload()->buf(), sop->Payload()->buf()+adjust, sop->Payload_Length()-adjust); // move the data!!!
            sop->Set_Offset(sop->Offset() + adjust);
            sop->Set_Length(sop->Payload_Length() - adjust);
            sop->Set_Start_Byte(sop->Offset());
            sop->Set_Stop_Byte(sop->Offset()+sop->Payload_Length()-1);
            return_status    = ADJUSTED_SEGMENT; 
        }

        if (right_overlap) 
        {
            adjust          = (this_segment->Start_Byte()-1)-sop->Start_Byte();
            // truncate the length  don't bother removing the data since it won't be used
            sop->Set_Length(sop->Payload_Length()-adjust); 
            sop->Set_Stop_Byte(sop->Start_Byte()+(sop->Payload_Length()-1));
            return_status   = ADJUSTED_SEGMENT; 
        }

        if (return_status != UNKNOWN_SEGMENT) break;


        if (seg_iter == segments->begin()) {
            return LOOKUP_COMPLETE;
        }

        --seg_iter;
    }

    // fell through not finding any conflicts
    if (return_status == UNKNOWN_SEGMENT) return_status = LOOKUP_COMPLETE;

    return return_status;
}

//----------------------------------------------------------------------    
int 
LTPSession::Check_Overlap(LTP_DS_MAP * segments, SPtr_LTPDataSegment& ds_seg)
{
    int adjust        = 0;
    int return_status = UNKNOWN_SEGMENT;

    if (segments->empty())return LOOKUP_COMPLETE;

    for(LTP_DS_MAP::reverse_iterator seg_iter=segments->rbegin(); seg_iter!=segments->rend(); ++seg_iter) {

        bool left_overlap = false;
        bool right_overlap = false;

        SPtr_LTPDataSegment this_segment = seg_iter->second;

        if (ds_seg->Start_Byte() > this_segment->Stop_Byte()) {
            return LOOKUP_COMPLETE;
        }

        if (ds_seg->Start_Byte() <= this_segment->Stop_Byte() &&
            ds_seg->Start_Byte() >= this_segment->Start_Byte()) {
            // left is within
            left_overlap = true;
        }

        if (ds_seg->Stop_Byte() <= this_segment->Stop_Byte() &&
            ds_seg->Stop_Byte() >= this_segment->Start_Byte()) {
            // right is within
            right_overlap = true;
        }

        if (left_overlap && right_overlap)
        {
            return DUPLICATE_SEGMENT;  // we already have this data
        }

        if (ds_seg->Start_Byte() <= this_segment->Start_Byte() &&
            ds_seg->Stop_Byte() >= this_segment->Stop_Byte())
        {
            // toss out the smaller segment 
            segments->erase(this_segment.get());

            red_bytes_received_ -= this_segment->Payload_Length();
            return DELETED_SEGMENT;
        }

        if (left_overlap) 
        {
            adjust = this_segment->Stop_Byte()-ds_seg->Offset();
            memmove(ds_seg->Payload()->buf(), ds_seg->Payload()->buf()+adjust, ds_seg->Payload_Length()-adjust); // move the data!!!
            ds_seg->Set_Offset(ds_seg->Offset() + adjust);
            ds_seg->Set_Length(ds_seg->Payload_Length() - adjust);
            ds_seg->Set_Start_Byte(ds_seg->Offset());
            ds_seg->Set_Stop_Byte(ds_seg->Offset()+ds_seg->Payload_Length()-1);
            return_status    = ADJUSTED_SEGMENT; 
        }

        if (right_overlap) 
        {
            adjust          = (this_segment->Start_Byte()-1)-ds_seg->Start_Byte();
            // truncate the length  don't bother removing the data since it won't be used
            ds_seg->Set_Length(ds_seg->Payload_Length()-adjust); 
            ds_seg->Set_Stop_Byte(ds_seg->Start_Byte()+(ds_seg->Payload_Length()-1));
            return_status   = ADJUSTED_SEGMENT; 
        }

        if (return_status != UNKNOWN_SEGMENT) break;
    }

    // fell through not finding any conflicts
    if (return_status == UNKNOWN_SEGMENT) return_status = LOOKUP_COMPLETE;

    return return_status;
}

//----------------------------------------------------------------------
bool
LTPSession::HaveReportSegmentsToSend(LTPDataSegment * dataseg, int32_t segsize, int64_t& bytes_claimed)
{
    bytes_claimed = 0;

    bool result = false;
    bool found_checkpoint = false;
    size_t lower_bounds_start = 0;

    // scan through list of report segments looking for match on checkpoint serial number or report serial number

    LTP_RS_MAP::iterator iter;
    SPtr_LTPReportSegment rptseg;

    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    iter = report_segments_.begin();
    while (iter != report_segments_.end())
    {
        rptseg = iter->second;

        if (rptseg->Checkpoint_ID() == dataseg->Checkpoint_ID())
        {
            // already generated and ready to send
            found_checkpoint = true;
            if (!rptseg->IsDeleted())
            {
                result = true;
                break;
            }
        }
        else if (rptseg->Report_Serial_Number() == dataseg->Serial_ID())
        {
            // generate new reports based on the bounds of the original report
            lower_bounds_start = rptseg->LowerBounds();
            break;
        }

        ++iter;
    }

    if (!found_checkpoint)
    {
        size_t upper_bounds = dataseg->Stop_Byte() + 1;

        bytes_claimed = GenerateReportSegments(dataseg->Checkpoint_ID(), lower_bounds_start, upper_bounds, segsize);

        result = true;
    }

    return result;

}

//----------------------------------------------------------------------
int64_t
LTPSession::GenerateReportSegments(uint64_t chkpt, size_t lower_bounds_start, size_t chkpt_upper_bounds, int32_t segsize)
{
    size_t bytes_claimed = 0;

    LTP_RC_MAP claim_map;
    LTP_RC_MAP::iterator rc_iter;
    SPtr_LTPReportClaim rc;
    SPtr_LTPReportSegment rptseg;

    if (red_complete_ && (red_bytes_received_ == expected_red_bytes_))
    {
        // generate simple "got it all" report segment
        lower_bounds_start = 0;
        rc = std::make_shared<LTPReportClaim>(lower_bounds_start, red_bytes_received_);
        claim_map[rc->Offset()] = rc;

        //dzdebug - negative means simple rpt
        bytes_claimed = -red_bytes_received_;

        rptseg = std::make_shared<LTPReportSegment>(this, chkpt, lower_bounds_start, 
                                                    red_bytes_received_, claim_map);
        report_segments_[rptseg->Report_Serial_Number()] = rptseg;

                    //dzdebug
                    //log_always("%s: Session: %s generated simple complete RS with %zu claims for %zu bytes", 
                    // __func__, key_str().c_str(), claim_map.size(), -bytes_claimed);

    }
    else
    {
        bool      new_claim        = true;

        size_t    claim_start      = 0;
        size_t    claim_length     = 0;
        size_t    next_claim_start = 0;
        size_t    offset;
        int32_t   sdnv_size_of_claim;
        int32_t   est_seg_size  = segsize / 2;  // allow plenty of overhead for headers and trailers


        LTP_DS_MAP::iterator iter;
        SPtr_LTPDataSegment dataseg;
        SPtr_LTPReportSegment rptseg;

        if (0 == lower_bounds_start)
        {
            iter = red_segments_.begin();
        }
        else
        {
            LTPDataSegKey key(lower_bounds_start, 0);
            iter = red_segments_.lower_bound(&key);
        }


        size_t    rpt_lower_bounds = lower_bounds_start;
        size_t    rpt_upper_bounds = lower_bounds_start;

        while (iter != red_segments_.end())
        {
            dataseg = iter->second;
            ++iter;

            if (dataseg->Offset() < rpt_lower_bounds)
            {
                // skip segments until we get to the starting lower bounds -- shouldn't happen
                continue;
            }

            if (new_claim)
            {
                new_claim = false;
                claim_start = dataseg->Offset();
                claim_length = dataseg->Payload_Length();
                next_claim_start = claim_start + claim_length;
            }
            else if (next_claim_start >= chkpt_upper_bounds)
            {
                bytes_claimed += claim_length;

                rc = std::make_shared<LTPReportClaim>(claim_start, claim_length);
                claim_map[rc->Offset()] = rc;

                rpt_upper_bounds = next_claim_start;
                break;
            }
            else if (next_claim_start == dataseg->Offset())
            {
                claim_length += dataseg->Payload_Length();
                next_claim_start = claim_start + claim_length;
            }
            else
            {
                bytes_claimed += claim_length;

                // hit a gap - save off this claim and start a new one
                rc = std::make_shared<LTPReportClaim>(claim_start, claim_length);
                claim_map[rc->Offset()] = rc;


                // add this claim to the estimated segment length
                offset = claim_start - rpt_lower_bounds;  // use offset from lower bounds to determine SDNV length
                sdnv_size_of_claim = SDNV::encoding_len(offset) + SDNV::encoding_len(claim_length);
                est_seg_size += sdnv_size_of_claim;

                if (est_seg_size >= segsize)
                {
                    // this claim tips us over the "soft" segsize - close out the report

                    // upper bound is the start of the next claim to signal the gap at the tail end
                    rpt_upper_bounds = dataseg->Offset();


                    // create this report segment
                    rptseg = std::make_shared<LTPReportSegment>(this, chkpt, rpt_lower_bounds, 
                                                                rpt_upper_bounds, claim_map);
                    report_segments_[rptseg->Report_Serial_Number()] = rptseg;


                    //dzdebug
                    //log_always("%s: Session: %s created chkpt after loop with %zu claims for %zu bytes", 
                    // __func__, key_str().c_str(), claim_map.size(), bytes_claimed);

                    // init for next report segment
                    claim_map.clear();
                    rpt_lower_bounds = rpt_upper_bounds;

                    est_seg_size = segsize / 2;
                }

                // start the new claim
                claim_start      = dataseg->Offset();
                claim_length     = dataseg->Payload_Length();
                next_claim_start = claim_start + claim_length;
           }
        }

        if (!new_claim)
        {
            // create the final report segment

            bytes_claimed = +claim_length;

            rc = std::make_shared<LTPReportClaim>(claim_start, claim_length);
            claim_map[rc->Offset()] = rc;

            rpt_upper_bounds = claim_start + claim_length;

            // create this report segment
            rptseg = std::make_shared<LTPReportSegment>(this, chkpt, rpt_lower_bounds, 
                                      rpt_upper_bounds, claim_map);
            report_segments_[rptseg->Report_Serial_Number()] = rptseg;

            //dzdebug
            //log_always("%s: Session: %s created chkpt after loop with %zu claims for %zu bytes", 
            //         __func__, key_str().c_str(), claim_map.size(), bytes_claimed);

        }
    }

    return bytes_claimed;
}

//----------------------------------------------------------------------    
size_t
LTPSession::IsRedFull() 
{
    size_t result = 0;

    if (red_eob_received_ && red_complete_ && !red_processed_)
    {
        if (red_bytes_received_ == expected_red_bytes_)
        {
            result = red_bytes_received_;
        }

    }

    return result;
}

//----------------------------------------------------------------------    
size_t
LTPSession::IsGreenFull() 
{
    if (!green_complete_ || green_processed_) return false; // we haven't seen the EOB packet or we've already sent this.

    size_t total_bytes = 0;

    for (LTP_DS_MAP::iterator seg_iter_=Green_Segments().begin(); seg_iter_!=Green_Segments().end(); ++seg_iter_) {

         SPtr_LTPDataSegment this_segment = seg_iter_->second;
         total_bytes                     += this_segment->Payload_Length(); 
    }        

    if (total_bytes == expected_green_bytes_) return total_bytes;
    return 0; // we aren't complete yet.
}

//----------------------------------------------------------------------    
u_char * 
LTPSession::GetAllRedData()
{
    // it is assumed that a call to IsRedFull() was made first and returned true - need to add check here?
    size_t bytes_loaded = 0;

    u_char * buffer = (u_char *) malloc(expected_red_bytes_); 

    if (buffer != (u_char *) NULL) {
        for(LTP_DS_MAP::iterator seg_iter_=Red_Segments().begin(); seg_iter_!=Red_Segments().end(); ++seg_iter_) {
            SPtr_LTPDataSegment this_segment = seg_iter_->second;
            memcpy(&buffer[this_segment->Offset()], this_segment->Payload()->buf(), this_segment->Payload_Length());
            bytes_loaded += this_segment->Payload_Length();
        }        
        ASSERT(bytes_loaded == expected_red_bytes_);
        red_processed_ = true;
    }       

    return buffer;
}

//----------------------------------------------------------------------    
u_char * 
LTPSession::GetAllGreenData()
{
    size_t bytes_loaded = 0;
    u_char * buffer = (u_char *) malloc(expected_green_bytes_); 
    if (buffer != (u_char *) NULL) {
        for(LTP_DS_MAP::iterator seg_iter_=Green_Segments().begin(); seg_iter_!=Green_Segments().end(); ++seg_iter_) {
            SPtr_LTPDataSegment this_segment = seg_iter_->second;
            memcpy(&buffer[this_segment->Offset()], this_segment->Payload()->buf(), this_segment->Payload_Length());
            bytes_loaded += this_segment->Payload_Length();
        }        
        ASSERT(bytes_loaded == expected_green_bytes_);
        green_processed_ = true;
    }

    return buffer;
}

//----------------------------------------------------------------------    
void
LTPSession::Start_Inactivity_Timer(SPtr_LTPNodeRcvrSndrIF& rsif, time_t time_left)
{
    // basically a map of one element :)
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    inactivity_timer_ = std::make_shared<InactivityTimer>(rsif, engine_id_, session_id_);
    inactivity_timer_->set_sptr(inactivity_timer_);
    inactivity_timer_->set_seconds(time_left);
    inactivity_timer_->start();
}

//----------------------------------------------------------------------    
void
LTPSession::Cancel_Inactivity_Timer()
{
    // basically a map of one element :)
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    if (inactivity_timer_ != nullptr)
    {    
        inactivity_timer_->cancel();
        inactivity_timer_ = nullptr;
    }
}

//----------------------------------------------------------------------    
void
LTPSession::Start_Closeout_Timer(SPtr_LTPNodeRcvrSndrIF& rsif, time_t time_left)
{
    if (time_left < 10) time_left = 10;

    // we don't want this timer to be cancelled when the session is deleted so using a temp pointer here
    SPtr_LTPTimer closeout_timer = std::make_shared<LTPCloseoutTimer>(rsif, engine_id_, session_id_);

    closeout_timer->set_sptr(closeout_timer);
    closeout_timer->set_seconds(time_left);
    closeout_timer->start();
    closeout_timer = nullptr;
}

//--------------------------------------------------------------------------
LTPSession::InactivityTimer::InactivityTimer(SPtr_LTPNodeRcvrSndrIF& rsif, 
                                   uint64_t engine_id, uint64_t session_id)
        : LTPTimer(rsif, LTP_EVENT_INACTIVITY_TIMEOUT, 30),
          engine_id_(engine_id),
          session_id_(session_id)
{
    BundleDaemon::instance()->inc_ltp_inactivity_timers_created();
}

//--------------------------------------------------------------------------
LTPSession::InactivityTimer::~InactivityTimer()
{
    BundleDaemon::instance()->inc_ltp_inactivity_timers_deleted();
}

//--------------------------------------------------------------------------
void    
LTPSession::InactivityTimer::do_timeout_processing()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    if (!cancelled() && (sptr_ != nullptr)) {

        // get a safe-to-use copy of the RSIF pointer
        SPtr_LTPNodeRcvrSndrIF my_rsif = rsif_;

        if (my_rsif != nullptr) {

            uint64_t right_now = my_rsif->RSIF_aos_counter();

            if (right_now >= target_counter_)
            {
                // no need to reschedule this timer so clear the internal reference 
                // so this object will be destroyed
                sptr_ = nullptr;
                scoplok.unlock();

                my_rsif->RSIF_inactivity_timer_triggered(engine_id_, session_id_);

                rsif_ = nullptr;
            }
            else
            {  // invoke a new timer!
                int seconds = (int) (target_counter_ - right_now);

                schedule_in(seconds*1000, sptr_);
            }
        }
        else
        {
            // clear the internal reference so this object will be destroyed
            sptr_ = nullptr;
            rsif_ = nullptr;
        }
    } else {
        // clear the internal reference so this object will be destroyed
        sptr_ = nullptr;
        rsif_ = nullptr;
    }
}

//----------------------------------------------------------------------
const char *
LTPSession::Get_Session_State()
{
    const char * return_var = "";
    static char   temp[20];

    switch(session_state_)
    {
        case LTPSession::LTP_SESSION_STATE_RS:
            return_var = "Session State RS";
            break;

        case LTPSession::LTP_SESSION_STATE_CS:
            return_var = "Session State CS";
            break;

        case LTPSession::LTP_SESSION_STATE_DS:
            return_var = "Session State DS";
            break;

        case LTPSession::LTP_SESSION_STATE_UNKNOWN:
            return_var = "Session State UNKNOWN";
            break;

        case LTPSession::LTP_SESSION_STATE_UNDEFINED:
            return_var = "Session State UNDEFINED";
            break;

        default:
            sprintf(temp,"SessState?? (%d)",session_state_);
            return_var = temp; 
            break;
    }
    return return_var;
}

//
//   LTPSegment ......................
//
LTPSegment::LTPSegment() :
        oasys::Logger("LTPSegment",
                      "/dtn/cl/ltpudp/segment/%p", this)
{
    BundleDaemon::instance()->inc_ltpseg_created();

    segment_type_             = 0;
    red_green_none_           = 0;
    timer_                    = NULL;
    is_checkpoint_            = false;
    is_redendofblock_         = false;
    is_greenendofblock_       = false;
    retrans_retries_          = 0;
    is_security_enabled_      = false;
    security_mask_            = LTP_SECURITY_DEFAULT;
    cipher_trailer_offset_    = 0;
    cipher_applicable_length_ = 0;
    cipher_key_id_            = -1;
    cipher_suite_             = -1;
    cipher_engine_            = "";

    //dzdebug
    version_number_           = 0;
    control_flags_            = 0;
    header_exts_              = 0;
    trailer_exts_             = 0;
    packet_len_               = 0;
    engine_id_                = 0;
    session_id_               = 0;
    serial_number_            = 0;
    checkpoint_id_            = 0;
    client_service_id_        = 0;
    is_valid_                 = true;
}

//----------------------------------------------------------------------
LTPSegment::LTPSegment(LTPSegment& from) :
        oasys::Logger("LTPSegment",
                      "/dtn/cl/ltpudp/segment/%p", this)

{
    BundleDaemon::instance()->inc_ltpseg_created();

    segment_type_          = from.segment_type_;
    engine_id_             = from.engine_id_;
    session_id_            = from.session_id_;
    serial_number_         = from.serial_number_;
    checkpoint_id_         = from.checkpoint_id_;
    control_flags_         = from.control_flags_;
    retrans_retries_       = from.retrans_retries_;
    timer_                 = from.timer_;
    security_mask_         = from.security_mask_;
    is_checkpoint_         = from.is_checkpoint_;
    is_redendofblock_      = from.is_redendofblock_;
    is_greenendofblock_    = from.is_greenendofblock_;

    is_security_enabled_      = from.is_security_enabled_;

    version_number_             = from.version_number_;
    header_exts_                = from.header_exts_;
    trailer_exts_               = from.trailer_exts_;
    packet_len_                 = from.packet_len_;
    client_service_id_          = from.client_service_id_;
    cipher_applicable_length_   = from.cipher_applicable_length_;
    cipher_trailer_offset_      = from.cipher_trailer_offset_;
    cipher_suite_               = from.cipher_suite_;
    cipher_key_id_              = from.cipher_key_id_;
    cipher_engine_              = from.cipher_engine_;
    is_valid_                   = from.is_valid_;
    red_green_none_             = from.red_green_none_;
}

//----------------------------------------------------------------------
LTPSegment::LTPSegment(LTPSession * session) :
        oasys::Logger("LTPSegment",
                      "/dtn/cl/ltpudp/segment/%p", this)
{
    BundleDaemon::instance()->inc_ltpseg_created();

    packet_len_         = 0;
    version_number_     = 0; 
    engine_id_          = session->Engine_ID();
    session_id_         = session->Session_ID();
    serial_number_      = 0;  // this is a new Segment so RPT should be 0. 
    checkpoint_id_      = session->Checkpoint_ID();
    control_flags_      = 0;
    segment_type_       = 0;
    client_service_id_  = 1;
    red_green_none_     = 0;
    is_checkpoint_      = false;
    is_redendofblock_   = false;
    is_greenendofblock_ = false;
    is_valid_           = true;
    retrans_retries_    = 0;
    memset(packet_.buf(1),0,1); /* Control Byte  initialize to zero and we'll change before we transmit/buffer */
    packet_.incr_len(1);
    timer_              = NULL;
    security_mask_       = LTP_SECURITY_DEFAULT;
    cipher_key_id_       = session->Outbound_Cipher_Key_Id();
    cipher_engine_       = session->Outbound_Cipher_Engine();
    cipher_suite_        = session->Outbound_Cipher_Suite();
    is_security_enabled_ = ((int) cipher_suite_ != -1);
    cipher_applicable_length_ = 0;
    cipher_trailer_offset_ = 0;

    //dzdebug
    header_exts_ = 0;
    trailer_exts_ = 0;
}

//----------------------------------------------------------------------
LTPSegment::~LTPSegment()
{
    BundleDaemon::instance()->inc_ltpseg_deleted();

     Cancel_Retransmission_Timer();
     raw_packet_.clear();
     packet_.clear();
}

//----------------------------------------------------------------------    
std::string
LTPSegment::session_key_str()
{
    char key[48];
    snprintf(key, sizeof(key), "%." PRIu64 ":%." PRIu64, engine_id_, session_id_);
    std::string result(key);

    return result;
}

//----------------------------------------------------------------------
void
LTPSegment::Set_Security_Mask(u_int32_t new_val)
{
    security_mask_ = new_val;
}
//----------------------------------------------------------------------
void 
LTPSegment::Set_Cipher_Suite(int new_val)
{
    cipher_suite_ = new_val;
    is_security_enabled_ = (new_val != -1) ? true : false;
}
//----------------------------------------------------------------------
void
LTPSegment::Set_Cipher_Key_Id(int new_key)
{
    cipher_key_id_ = new_key;
}
//----------------------------------------------------------------------
void 
LTPSegment::Set_Cipher_Engine(std::string& new_engine)
{
    cipher_engine_ = new_engine;
}

//----------------------------------------------------------------------
void
LTPSegment::Encode_Field(uint64_t infield)
{
    int estimated_len = SDNV::encoding_len(infield); 
    int result = 0;

    result  = SDNV::encode(infield,packet_.tail_buf(estimated_len), sizeof(infield));
    if (result < 1)
    {
      log_err("Error SDNV encoding field: value: %" PRIu64 " est len: %d", infield, estimated_len);
    }
    else
    {
      packet_len_  += result;
      packet_.incr_len(result);
    }
} 

//----------------------------------------------------------------------
void
LTPSegment::Encode_Field(u_int32_t infield)
{
    int estimated_len = SDNV::encoding_len(infield); 
    int result = 0;

    result  = SDNV::encode(infield,packet_.tail_buf(estimated_len), sizeof(infield));
    if (result < 1)
    {
      log_err("Error SDNV encoding field: value: %" PRIu32 " est len: %d", infield, estimated_len);
    }
    else
    {
      packet_len_  += result;
      packet_.incr_len(result);
    }

} 

//----------------------------------------------------------------------
void 
LTPSegment::Set_Client_Service_ID(u_int32_t in_client_service_id)
{
     client_service_id_  = in_client_service_id;
}

//----------------------------------------------------------------------
int LTPSegment::Parse_Buffer(u_char * bp, size_t length, size_t * byte_offset)
{
    u_int32_t test_tag  = 0;
    u_int32_t test_len  = 0;
    u_int32_t seg_len   = 0;

    size_t read_len  = 0;
    size_t current_byte    = 1;
    int decode_status   = 0;
    bool header_found   = false;

    control_flags_      = (int) (*bp & 0x0F);

    is_redendofblock_   = false;
    is_greenendofblock_ = false;
    is_checkpoint_      = false;
    is_valid_           = true;
    red_green_none_     = 0;

    timer_              = NULL;

    bool ctr_flag = (control_flags_ & 0x08) > 0 ? true : false;
    bool exc_flag = (control_flags_ & 0x04) > 0 ? true : false;
    bool flag_1   = (control_flags_ & 0x02) > 0 ? true : false;
    bool flag_0   = (control_flags_ & 0x01) > 0 ? true : false;
   
    // Written from PAGE 11 of LTP Spec RFC 5326  
    if (!ctr_flag) { // Data Segment
        if (!exc_flag) { // Red Data Segment
            red_green_none_ = RED_SEGMENT;
            if (flag_1) {
                is_checkpoint_      = true;
                if (flag_0) {
                   is_redendofblock_  = true;
                }
            } else { // not end of red-part
                if (flag_0)is_checkpoint_ = true;
            }
        } else { // Green Data Segment
            red_green_none_ = GREEN_SEGMENT;
            if (flag_1) {
                is_checkpoint_      = true;
                if (flag_0) {
                    is_greenendofblock_ = true;
                }
            }
        }
    }

    version_number_    = (int) (*bp & 0xF0) >> 4;

    // this will probably need to create an LTP thing....
    // Or at least parse off the front end to get the session and source. 
    //  Then add it to an existing session map or create a new one then add.

    // this should pull the engine id from and returns bytes used
    decode_status = SDNV::decode(bp+current_byte, length-current_byte, &engine_id_);

    CHECK_DECODE_STATUS

    // this should pull the session id from the buffer
    decode_status = SDNV::decode(bp+current_byte, length-current_byte, &session_id_); 

    CHECK_DECODE_STATUS

    // pull off each nibble and make int.
    //
    char one_byte = (char) * (bp+current_byte);

    if (one_byte > 0) { // may have only a trailer too
        is_security_enabled_ = true;
    }

    header_exts_  = (int) (one_byte & 0xF0) >> 4;
    trailer_exts_ = (int) (one_byte & 0x0F);
 
    is_security_enabled_ = (one_byte > 0x00);
    current_byte += 1;  // we just processed a byte

    for(int ctr = 0 ; ctr < header_exts_ ; ctr++) {
        test_tag = bp[current_byte];
        current_byte += 1; // segtag   
        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &test_len);
        CHECK_DECODE_STATUS
        if (test_tag == 0x00)
        {
            if (!header_found) {
                seg_len              = test_len;
                cipher_suite_        = bp[current_byte];
                cipher_key_id_       = -1;
                current_byte        += 1; // segtag   
                read_len             = seg_len - 1;
                is_security_enabled_ = true;
                header_found         = true;
                if (read_len > 0)
                {
                    decode_status = SDNV::decode(bp+current_byte, length-current_byte, &cipher_key_id_); 
                    CHECK_DECODE_STATUS
                }
            } else {
                current_byte += seg_len; // skip the data
            }
        } else {
            current_byte += seg_len; // skip the data
        }
    }

    *byte_offset = current_byte;

    if (current_byte > length)is_valid_ = false;
    return 0;
}

//----------------------------------------------------------------------
void 
LTPSegment::Set_RGN(int new_color)
{
    if (new_color == GREEN_SEGMENT) {
        control_flags_ = control_flags_ | 0x04;
    } else if (new_color == RED_SEGMENT) {
        control_flags_ = control_flags_ & 0x0B;
    }
    red_green_none_ = new_color;
}

//----------------------------------------------------------------------
void 
LTPSegment::Set_Report_Serial_Number(uint64_t new_rsn)
{
    serial_number_ = new_rsn;
}

//----------------------------------------------------------------------
void
LTPSegment::Set_Checkpoint_ID(uint64_t new_chkpt)
{
    checkpoint_id_ = new_chkpt;
}

//----------------------------------------------------------------------
void
LTPSegment::SetCheckpoint()
{
    u_char control_byte = (u_char) (control_flags_ & 0xFF);
    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    } else {
        memset(packet_.buf(), control_byte, 1); // allocated by Constructor... 
    }
    is_checkpoint_ = true;
}

//----------------------------------------------------------------------
void
LTPSegment::UnsetCheckpoint()   //XXX/dz - what is the difference with SetCheckpoint? Are these used??
{
    u_char control_byte = (u_char) (control_flags_ & 0xFF);
    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    } else {
        memset(packet_.buf(), control_byte, 1); // allocated by Constructor... 
    }
    is_checkpoint_ = false;
}

//----------------------------------------------------------------------
void
LTPSegment::xxSet_Retrans_Retries(int new_count)
{
    retrans_retries_ = new_count;
}

//----------------------------------------------------------------------
void
LTPSegment::Increment_Retries()
{
    retrans_retries_++;
}

//----------------------------------------------------------------------
void
LTPSegment::Create_LTP_Header()
{
    u_char control_byte = (u_char) (control_flags_ & 0xFF);
    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    } else {
        memset(packet_.buf(), control_byte, 1); // allocated by Constructor... 
        packet_.set_len(1);
    }

    /*
     * stuff the session_ctr into the buffer
     */
    packet_len_ = 1;

    Encode_Field(engine_id_);

    Encode_Field(session_id_);

    /*
     *  HDR / TRLR ext we don't have to set to 0x00;
     */
    memset(packet_.tail_buf(1),0,1); // plain old no hdr and no trlr

    packet_.incr_len(1);
    packet_len_ ++;

    return;
}
void
LTPSegment::Build_Header_ExtTag()
{
    int estimated_len = 0;
    memset(packet_.tail_buf(1),0,1); // LTP AUTH
    packet_.incr_len(1);
    packet_len_ ++;
   
    if ((int) cipher_suite_ == 0) {
        estimated_len = SDNV::encoding_len(cipher_key_id_); 
    }

    memset(packet_.tail_buf(1),((estimated_len+1) & 0xFF),1); // Ciphersuite
    packet_.incr_len(1);
    packet_len_ ++;

    memset(packet_.tail_buf(1),(cipher_suite_ & 0xFF),1); // Ciphersuite
    packet_.incr_len(1);
    packet_len_ ++;

    if (estimated_len > 0)
    {
        LTPSegment::Encode_Field((u_int32_t)cipher_key_id_); // SDNV the key_id
    }
}
//----------------------------------------------------------------------
uint64_t LTPSegment::Parse_Engine(u_char * bp, size_t length)
{
    uint64_t engine_id = 0;
    int current_byte    = 1;
    SDNV::decode(bp+current_byte, length-current_byte, &engine_id);
    return engine_id;
}

//----------------------------------------------------------------------
uint64_t LTPSegment::Parse_Session(u_char * bp, size_t length)
{
    uint64_t engine_id  = 0;
    uint64_t session_id = 0;
    int decode_status    = 0;
    int current_byte     = 1;
    decode_status = SDNV::decode(bp+current_byte, length-current_byte, &engine_id);
    if (decode_status > 0)
        current_byte += decode_status;
    else
        return 0;
    decode_status = SDNV::decode(bp+current_byte, length-current_byte, &session_id);
    return session_id;
}

//----------------------------------------------------------------------
bool LTPSegment::Parse_Engine_and_Session(u_char * bp, size_t length, uint64_t& engine_id, uint64_t& session_id, 
                                          int& seg_type)
{
    engine_id  = 0;
    session_id = 0;
    int decode_status    = 0;
    int current_byte     = 1;

    uint8_t seg_type_flags = bp[0];
    seg_type = LTPSegment::SegTypeFlags_to_SegType(seg_type_flags);

    decode_status = SDNV::decode(bp+current_byte, length-current_byte, &engine_id);
    if (decode_status > 0)
        current_byte += decode_status;
    else
        return false;

    decode_status = SDNV::decode(bp+current_byte, length-current_byte, &session_id);
    return (decode_status > 0);
}

//----------------------------------------------------------------------
int
LTPSegment::SegTypeFlags_to_SegType(uint8_t seg_type_flags)
{
    int result = -1;

    bool ctrl_flag = (seg_type_flags & 0x08) != 0;
    bool exc_flag = (seg_type_flags & 0x04) != 0;
    bool flag_1   = (seg_type_flags & 0x02) != 0;
    bool flag_0   = (seg_type_flags & 0x01) != 0;

    if (!ctrl_flag)
    {
        result = LTP_SEGMENT_DS;
    } else {
        if (!exc_flag) {
            if (!flag_1) {
                result = flag_0 ? LTP_SEGMENT_RAS : LTP_SEGMENT_RS;
            }
            // else undefined type
        } else {
            if (!flag_1) {
                result = flag_0 ? LTP_SEGMENT_CAS_BS : LTP_SEGMENT_CS_BS;
            } else {
                result = flag_0 ? LTP_SEGMENT_CAS_BR : LTP_SEGMENT_CS_BR;
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------
LTPSegment * LTPSegment::DecodeBuffer(u_char * bp, size_t length)
{
    int control_flags    = (int) (*bp & 0x0f);

    bool ctrl_flag = (control_flags & 0x08) != 0;
    bool exc_flag = (control_flags & 0x04) != 0;
    bool flag_1   = (control_flags & 0x02) != 0;
    bool flag_0   = (control_flags & 0x01) != 0;

    if (!ctrl_flag)  // 0 * * *
    {
       return new LTPDataSegment(bp,(int) length);
    }
    else // REPORT, CANCEL segment   1 * * *
    {
       if (!exc_flag) {       // exc_flag == 0  REPORT SEGMENT     1 0 * *
           if (!flag_1) {     // flag_1   == 0                     1 0 0 *
               if (!flag_0) { // flag_0   == 0                     1 0 0 0
                   return new LTPReportSegment(bp,(int) length);
               } else {      // flag_0   == 1                     1 0 0 1
                   return new LTPReportAckSegment(bp,(int) length);
               }
           } else {
               // not implemented yet....
           }
       } else {               // exc_flag == 1                    1 1 * *
           if (flag_1) {      // flag_1   == 1                    1 1 1 *
               if (!flag_0) {  // flag_0   == 0                    1 1 1 0 CS from block RCVR
                   return new LTPCancelSegment(bp,(int) length, LTP_SEGMENT_CS_BR);
               } else {       // flag_0   == 1  //                1 1 1 1 CAS to Block RCVR
                   return new LTPCancelAckSegment(bp,(int) length,  LTP_SEGMENT_CAS_BR);
               }
           } else {           // flag_1 == 0                      1 1 0 *
               if (!flag_0) {   // flag_0 == 0  CS to block sender  1 1 0 0
                   return new LTPCancelSegment(bp,(int) length,  LTP_SEGMENT_CS_BS);
               } else {      // flag_0 == 1                       1 1 0 1    CAS from block sender 
                   return new LTPCancelAckSegment(bp,(int) length,  LTP_SEGMENT_CAS_BS);
               }
           }
       }
    }
    return NULL;
}
//----------------------------------------------------------------------
const char * LTPSegment::Get_Segment_Type()
{
    return LTPSegment::Get_Segment_Type(segment_type_);
}

//----------------------------------------------------------------------
const char * LTPSegment::Get_Segment_Type(int st)
{
    const char * return_var = "";
    static char temp[20];
 
    switch(st)
    {
        case LTP_SEGMENT_UNKNOWN:
            return_var = "Segment Type UNKNOWN(0)";
            break;

        case LTP_SEGMENT_DS:
            return_var = "Segment Type DS(1)";
            break;
    
        case LTP_SEGMENT_RS:
            return_var = "Segment Type RS(2)";
            break;
    
        case LTP_SEGMENT_RAS:
            return_var = "Segment Type RAS(3)";
            break;
    
        case LTP_SEGMENT_CS_BR:
            return_var = "Segment Type CS_BR(4)"; 
            break;
    
        case LTP_SEGMENT_CAS_BR:
            return_var = "Segment Type CAS_BR(5)";
            break;

        case LTP_SEGMENT_CS_BS:
            return_var = "Segment Type CS_BS(6)"; 
            break;

        case LTP_SEGMENT_CAS_BS:
            return_var = "Segment Type CAS_BS(7)";
            break;

        case LTP_EVENT_SESSION_COMPLETED:
            return_var = "Event Type COMPLETED(101)"; 
            break;

        case LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES:
            return_var = "Event Type INTERNAL_ABORT(101)"; 
            break;

        case LTP_EVENT_DS_TO:
            return_var = "Event Type DS(102) timeout";
            break;

        case LTP_EVENT_RS_TO:
            return_var = "Event Type RS(103) timeout";
            break;

        case LTP_EVENT_CS_TO:
            return_var = "Event Type CS(104) timeout"; 
            break;

        case LTP_EVENT_RAS_RECEIVED:
            return_var = "Event Type RAS Recevied(105)"; 
            break;

        case LTP_EVENT_INACTIVITY_TIMEOUT:
            return_var = "Event Type Inactivity(106) timeout"; 
            break;

        case LTP_EVENT_CLOSEOUT_TIMEOUT:
            return_var = "Event Type Closeout(107) timeout"; 
            break;

        default:
            snprintf(temp, sizeof(temp), "Unknown Type? (%d)", st);
            return_var = temp;
            break;
    }
    return return_var;
}

//--------------------------------------------------------------------------
SPtr_LTPTimer
LTPSegment::Create_Retransmission_Timer(SPtr_LTPNodeRcvrSndrIF& rsif, int event_type, uint32_t seconds,
                                        SPtr_LTPSegment& retransmit_seg)
{
    oasys::ScopeLock scoplok(&timer_lock_, __func__);

    if (timer_ != nullptr) {
        timer_->cancel();
        timer_ = nullptr;
    }

    timer_ = std::make_shared<LTPRetransmitSegTimer>(rsif, event_type, seconds, retransmit_seg);

    timer_->set_sptr(timer_);

    return timer_;
}

//--------------------------------------------------------------------------
void    
LTPSegment::Cancel_Retransmission_Timer()
{
    oasys::ScopeLock scoplok(&timer_lock_, __func__);

    if (timer_ != nullptr) {
        timer_->cancel();
        timer_ = nullptr;
    }
}


//----------------------------------------------------------------------
void
LTPSegment::Dump_Current_Buffer(char * location)
{
    (void) location; // prevent warning when debug dissabled

    char hold_byte[4];
    char * output_str = (char *) malloc(packet_len_ * 3);
    if (output_str != (char *) NULL)
    {
        memset(output_str,0,packet_len_*3);
        for(size_t counter = 0 ; counter < packet_len_ ; counter++) {
            sprintf(hold_byte,"%02X ",(u_char) *(packet_.buf()+counter));
            strcat(output_str,hold_byte);
        }
        free(output_str);
    }
}



//
//  LTPDataSegment.........................
//
//--------------------------------------------------------------------------
LTPDataSegment::LTPDataSegment(u_char * bp, size_t len)
    : LTPDataSegKey(0, 0)
{
    payload_length_ = 0;
    offset_ = 0;

    if (Parse_Buffer(bp, len) != 0) 
        is_valid_ = false;
}

//--------------------------------------------------------------------------
LTPDataSegment::LTPDataSegment(LTPSession * session) 
    : LTPDataSegKey(0, 0),
      LTPSegment(session)
{
    //log_debug("Creating DS..");
    segment_type_      = LTP_SEGMENT_DS;
    is_checkpoint_     = false;
    offset_            = 0;
    payload_length_    = 0;
    control_flags_     = 0;
}

//--------------------------------------------------------------------------
LTPDataSegment::~LTPDataSegment()
{
    Cancel_Retransmission_Timer();
    payload_.clear();
}

//--------------------------------------------------------------------------
void LTPDataSegment::Set_Endofblock()
{
    if (IsRed())
    {
        is_redendofblock_   = true;
    } else if (IsGreen()) {
        is_greenendofblock_ = true;
    }
    u_char control_byte = control_flags_;
    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    }
    
    control_byte |= 0x03; // add in checkpoint && EOB
    memcpy(packet_.buf(), &control_byte, 1); // set checkpoint flags
    control_flags_ = control_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Unset_Endofblock()
{
    if (IsRed())
    {
        is_redendofblock_    = false;
    } else if (IsGreen()) {
        is_greenendofblock_  = false;
    }
    u_char control_byte = control_flags_;
    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    }
    control_byte &= 0x0C; // remove checkpoint
    memset(packet_.buf(), control_byte, 1); // clear buffer 
    control_flags_ = control_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Checkpoint()
{
    is_checkpoint_ = true;
    u_char control_byte = control_flags_;

    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    }
    
    control_byte |= (u_char) 0x01; // add in checkpoint  onlhy needs one bit if red!!
    memcpy(packet_.buf(), &control_byte, 1); // set checkpoint flags
    control_flags_ = control_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Segment_Color_Flags()
{
    u_char control_byte = control_flags_;

    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    }
    
    if (IsGreen()) { 
        control_byte |= 0x04; //  if green then turn on the exec put
    } else  {
        control_byte &= 0x04; //  don't need this on if red
    }
    memcpy(packet_.buf(), &control_byte, 1); // set checkpoint flags
    control_flags_ = control_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Unset_Checkpoint()
{
    is_checkpoint_ = false;
    u_char control_byte = control_flags_;
    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    }
    control_byte &= 0x0E; // remove checkpoint
    memset(packet_.buf(), control_byte, 1); // set control byte
    control_flags_ = control_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Start_Byte(int start_byte)
{
    start_byte_ = start_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Stop_Byte(int stop_byte)
{
    stop_byte_ = stop_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Length(int len)
{
    payload_length_ = len;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Offset(size_t offset)
{
    offset_ = offset;
}

//-----------------------------------------------------------------
void LTPDataSegment::Encode_Buffer()
{
    if (packet_.len() > 0) {
        control_flags_ = *packet_.buf();
    }
    
    Set_Segment_Color_Flags();

    if (is_checkpoint_) Set_Checkpoint();

    if (is_redendofblock_ || is_greenendofblock_) Set_Endofblock();

    LTPSegment::Create_LTP_Header();

    /*
     *  Client Service ID 
     */ 
    LTPSegment::Encode_Field(client_service_id_);
    
    /*
     *  Offset 
     */ 
    LTPSegment::Encode_Field(offset_);
  
    /*
     *  Length 
     */ 
    LTPSegment::Encode_Field(payload_length_);

    if (IsRed()) {

        if (is_checkpoint_)
        {
            LTPSegment::Encode_Field(checkpoint_id_);
            LTPSegment::Encode_Field(serial_number_);
        }
    }
}

//-----------------------------------------------------------------
void LTPDataSegment::Encode_Buffer(u_char * data, size_t length, size_t start_byte, bool checkpoint)
{
    payload_length_ = length;
    if (IsRed()) {
       is_redendofblock_  = checkpoint;
       is_checkpoint_     = checkpoint;
    } else if (IsGreen()) {
       is_checkpoint_      = false;
       is_greenendofblock_ = checkpoint;
    }
    offset_         = start_byte;
    start_byte_     = start_byte;
    stop_byte_      = start_byte+length-1; 

    Encode_Buffer();
    payload_.clear();

    memcpy(payload_.tail_buf(length),data,length);
    payload_.incr_len(length);
    segment_type_   = LTP_SEGMENT_DS;
    payload_length_ = length;

    memcpy(packet_.tail_buf(payload_length_),payload_.buf(),payload_length_);
    packet_.incr_len(payload_length_);
    packet_len_    += payload_length_;
}

//-----------------------------------------------------------------
void LTPDataSegment::Encode_All()
{
    packet_.clear();
    packet_len_ = 0;
    Encode_Buffer();
  
    memcpy(packet_.tail_buf(payload_length_),payload_.buf(),payload_length_);
    packet_.incr_len(payload_length_);
    packet_len_ += payload_length_;
}

//-----------------------------------------------------------------
int LTPDataSegment::Parse_Buffer(u_char * bp, size_t length)
{
    size_t current_byte = 0;
    int decode_status  = 0;
    
    if (LTPSegment::Parse_Buffer(bp, length, &current_byte) == 0)
    {
        segment_type_ = LTP_SEGMENT_DS;

        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &client_service_id_);
        CHECK_DECODE_STATUS

        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &offset_);
        CHECK_DECODE_STATUS

        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &payload_length_);
        CHECK_DECODE_STATUS

        start_byte_ = offset_;
        stop_byte_  = offset_+payload_length_-1;

        if (IsRed()) {

            if (is_checkpoint_)  // this is a checkpoint segment so it should contain these two entries.
            {
                decode_status = SDNV::decode(bp+current_byte, length-current_byte, &checkpoint_id_);
                CHECK_DECODE_STATUS

                decode_status = SDNV::decode(bp+current_byte, length-current_byte, &serial_number_);
                CHECK_DECODE_STATUS
            }
        }
        
        if (payload_length_ > 0) {
            if ((int) (current_byte + payload_length_) > (int) length) {
                 is_valid_ = false;
                 return -1;
            }
            memcpy(payload_.buf(payload_length_),bp+current_byte,payload_length_); // only copy the payload
            payload_.incr_len(payload_length_);

        } else {
            is_valid_ = false;
        }
    } else {
        is_valid_ = false;
    }
    return is_valid_ ? 0 : 1;
}

//
//  LTPReportSegment .................................
//
//----------------------------------------------------------------------
LTPReportSegment::LTPReportSegment()
{
    report_serial_number_    = 0;
    timer_            = NULL;
    retrans_retries_  = 0;
}

//----------------------------------------------------------------------
LTPReportSegment::LTPReportSegment(LTPSession * session, uint64_t checkpoint_id,
                                         size_t lower_bounds, size_t upper_bounds, LTP_RC_MAP& claim_map) 
        : LTPSegment(session)
{
    if (session->Report_Serial_Number() == 0)
    {
       session->Set_Report_Serial_Number(LTP_14BIT_RAND);
    } else {
       session->Set_Report_Serial_Number(session->Report_Serial_Number()+1);
    }

    checkpoint_id_            = checkpoint_id;

    control_flags_ = 0x08; // this is a ReportSegment

    LTPSegment::Create_LTP_Header();

    segment_type_ = LTP_SEGMENT_RS;

    memset(packet_.buf(),0x08,1);

    report_serial_number_ = session->Report_Serial_Number();

    timer_                    = NULL;
    retrans_retries_          = 0;

    claims_          = claim_map.size();
    lower_bounds_    = lower_bounds;
    upper_bounds_    = upper_bounds;

    Encode_All(claim_map);
}

//----------------------------------------------------------------------
LTPReportSegment::~LTPReportSegment()
{
    Cancel_Retransmission_Timer();

    claim_map_.clear();
}

//----------------------------------------------------------------------
LTPReportSegment::LTPReportSegment(u_char * bp, size_t len)
{
    report_serial_number_ = 0;
    upper_bounds_ = 0;
    lower_bounds_ = 0;
    claims_ = 0;

    if (Parse_Buffer(bp,len) != 0)is_valid_ = false;
}

//----------------------------------------------------------------------
int LTPReportSegment::Parse_Buffer(u_char * bp, size_t length)
{
    size_t reception_offset = 0;
    size_t reception_length = 0;

    // HMAC context create and decode if security enabled

    int ctr       = 0;  // declare this to make sure all data was there.
    size_t current_byte = 0;
    int decode_status  = 0;

    if (LTPSegment::Parse_Buffer(bp, length, &current_byte) == 0)
    {
        if (!is_valid_)return -1;

        segment_type_ = LTP_SEGMENT_RS; 

        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &report_serial_number_);
        CHECK_DECODE_STATUS

        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &checkpoint_id_);
        CHECK_DECODE_STATUS
        
        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &upper_bounds_);
        CHECK_DECODE_STATUS
        
        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &lower_bounds_);
        CHECK_DECODE_STATUS
        
        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &claims_);
        CHECK_DECODE_STATUS

        SPtr_LTPReportClaim rc;
        for(ctr = 0 ; ctr < (int) claims_ ; ctr++) {// step through the claims

            decode_status = SDNV::decode(bp+current_byte, length-current_byte, &reception_offset);
            CHECK_DECODE_STATUS
            
            decode_status = SDNV::decode(bp+current_byte, length-current_byte, &reception_length);
            CHECK_DECODE_STATUS

            rc = std::make_shared<LTPReportClaim>(reception_offset,reception_length);
            claim_map_[rc->Offset()] = rc;
        }

        if (ctr == (int) claims_)is_valid_ = true;

        if (current_byte > length)
        {
            is_valid_ = false;
        }
    } else {
        is_valid_ = false;
    }

    return is_valid_ ? 0 : 1;
}

//----------------------------------------------------------------------
void
LTPReportSegment::Encode_All(LTP_RC_MAP& claim_map)
{
    static char buf[128];
    int buflen = sizeof(buf);
    int bufidx = 0;
    int ctr = 0;
    int strsize;
    memset(buf, 0, sizeof(buf));

    size_t val;
    LTP_RC_MAP::iterator loop_iterator;

    SPtr_LTPReportClaim rc;

    packet_.clear();

    LTPSegment::Create_LTP_Header();

    Encode_Counters();

    for(loop_iterator = claim_map.begin(); loop_iterator != claim_map.end(); loop_iterator++)
    {
        rc = loop_iterator->second;

        if (ctr++ < 3)
        {
            strsize = snprintf(buf+bufidx, buflen, "%lu-%lu ", rc->Offset(), (rc->Offset() + rc->Length() - 1));
            bufidx += strsize;
            buflen -= strsize;
        }

        val = rc->Offset() - lower_bounds_;
        LTPSegment::Encode_Field(val);

        val = rc->Length();
        LTPSegment::Encode_Field(val);
    }

    //dzdebug
    //log_always("RptSeg[%lu] lower: %lu upper: %lu claims: %zu  First3: %s",
    //           report_serial_number_, lower_bounds_, upper_bounds_, claim_map.size(), buf);

}

//--------------------------------------------------------------------------
void 
LTPReportSegment::Encode_Counters()
{
    LTPSegment::Encode_Field(report_serial_number_);
    LTPSegment::Encode_Field(checkpoint_id_);
    LTPSegment::Encode_Field(upper_bounds_);
    LTPSegment::Encode_Field(lower_bounds_);
    LTPSegment::Encode_Field(claims_);
}

//
//  LTPReportAckSegment .................................
//
//----------------------------------------------------------------------    
LTPReportAckSegment::LTPReportAckSegment(u_char * bp, size_t len)
{
    if (Parse_Buffer(bp,len) != 0)is_valid_ = false;
}

//--------------------------------------------------------------------------
int LTPReportAckSegment::Parse_Buffer(u_char * bp, size_t length)
{
    size_t current_byte = 0;
    int decode_status  = 0;
    // HMAC context create and decode if security enabled

    if (LTPSegment::Parse_Buffer(bp, length, &current_byte) == 0)
    {
        if (!is_valid_)return -1;

        segment_type_ = LTP_SEGMENT_RAS; 

        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &report_serial_number_);

        CHECK_DECODE_STATUS

        if (current_byte > length)
        {
            is_valid_ = false;
        }
    } else {
        return -1;
    }
    return is_valid_ ? 0 : 1;
}

//--------------------------------------------------------------------------
LTPReportAckSegment::LTPReportAckSegment(LTPReportSegment * rs, LTPSession * session) 
         : LTPSegment(*rs)
{
    // HMAC context create and decode if security enabled
    control_flags_   = 0x09;
    serial_number_   = rs->Serial_ID();
    cipher_suite_  = session->Outbound_Cipher_Suite();
    cipher_key_id_ = session->Outbound_Cipher_Key_Id();
    cipher_engine_ = session->Outbound_Cipher_Engine();
    is_security_enabled_ = ((int) cipher_suite_ != -1) ? true : false;
    LTPSegment::Create_LTP_Header();
    report_serial_number_ = rs->Report_Serial_Number();

    segment_type_         = LTP_SEGMENT_RAS;
    Encode_Field(report_serial_number_);
    memset(packet_.buf(),0x09,1);
} 

//--------------------------------------------------------------------------
LTPReportAckSegment::LTPReportAckSegment(LTPSession * session) 
        : LTPSegment(session)
{
    report_serial_number_ = session->Report_Serial_Number();
    segment_type_         = LTP_SEGMENT_RAS;

    cipher_suite_  = session->Outbound_Cipher_Suite();
    cipher_key_id_ = session->Outbound_Cipher_Key_Id();
    cipher_engine_ = session->Outbound_Cipher_Engine();
    is_security_enabled_ = ((int) cipher_suite_ != -1) ? true : false;

    LTPSegment::Create_LTP_Header();
    LTPSegment::Encode_Field(report_serial_number_);
    memset(packet_.buf(),0x09,1);
}

//--------------------------------------------------------------------------
LTPReportAckSegment::LTPReportAckSegment()
{
    report_serial_number_     = 0;
}

//--------------------------------------------------------------------------
LTPReportAckSegment::~LTPReportAckSegment()
{
}


//
//  LTPCancelSegment .................................
//
//----------------------------------------------------------------------    
LTPCancelSegment::LTPCancelSegment(u_char * bp, size_t len, int segment_type)
{
    if (Parse_Buffer(bp,len, segment_type) != 0)is_valid_ = false;
}

//-----------------------------------------------------------------
LTPCancelSegment::LTPCancelSegment(LTPSession * session, int segment_type, u_char reason_code) 
        : LTPSegment(session)
{
    cipher_suite_        = session->Outbound_Cipher_Suite();
    cipher_key_id_       = session->Outbound_Cipher_Key_Id();
    cipher_engine_       = session->Outbound_Cipher_Engine();
    is_security_enabled_ = ((int) cipher_suite_ != -1) ? true : false;

    LTPSegment::Create_LTP_Header();

    memset(packet_.tail_buf(1),reason_code,1);

    packet_.incr_len(1);

    switch(segment_type)
    {
        case LTP_SEGMENT_CS_BS:
            memset(packet_.buf(),0x0C,1); // 12 Cancel Segment from Block Sender
            break;

        case LTP_SEGMENT_CS_BR:
            memset(packet_.buf(),0x0E,1); // 14 Cancel segment from Block Receiver
            break;
   
        default:
            memset(packet_.buf(),0x0C,1); //  just make it a valid one but complain
            break;

    }

    reason_code_    = reason_code;
    segment_type_   = segment_type;
    packet_len_++;
} 

//-----------------------------------------------------------------
int LTPCancelSegment::Parse_Buffer(u_char * bp, size_t length, int segment_type)
{
    size_t current_byte = 0;
    // HMAC context create and decode if security enabled

    if (LTPSegment::Parse_Buffer(bp, length, &current_byte) == 0)
    {       
        if (!is_valid_)return -1;

        segment_type_ = segment_type; 
        reason_code_  = *(bp+current_byte);
        current_byte += 1;
    } else {
        return -1;
    }

    if (current_byte > length)
    {
        is_valid_ = false;
        return -1;
    }

    return 0;
}

//-----------------------------------------------------------------
LTPCancelSegment::LTPCancelSegment()
{
    reason_code_  = 0x00;
    packet_len_   = 0;
}

//-----------------------------------------------------------------
LTPCancelSegment::LTPCancelSegment(LTPCancelSegment& from)  : LTPSegment(from)
{
    reason_code_  = from.reason_code_;
    packet_len_   = from.packet_len_;
    packet_       = from.packet_;

}

//-----------------------------------------------------------------
LTPCancelSegment::~LTPCancelSegment()
{
    Cancel_Retransmission_Timer();
}


//
//  LTPCancelAckSegment .................................
//
//----------------------------------------------------------------------    
 LTPCancelAckSegment::LTPCancelAckSegment(u_char * bp, size_t len, int segment_type)
{
    if (Parse_Buffer(bp,len, segment_type) != 0)is_valid_ = false;
}

//-----------------------------------------------------------------
int LTPCancelAckSegment::Parse_Buffer(u_char * bp, size_t length, int segment_type)
{
    size_t  current_byte = 0;

    if (LTPSegment::Parse_Buffer(bp, length, &current_byte) != 0)is_valid_ = false;

    if (!is_valid_)return -1;

    segment_type_ = segment_type;

    return 0;
}

//-----------------------------------------------------------------
LTPCancelAckSegment::LTPCancelAckSegment()
{
    packet_len_   = 0;
}

//-----------------------------------------------------------------
LTPCancelAckSegment::LTPCancelAckSegment(LTPCancelSegment * from, u_char control_byte, LTPSession * session)
{
    packet_len_          = 0;
    engine_id_           = from->Engine_ID();
    session_id_          = from->Session_ID();;
    control_flags_       = control_byte;
    is_security_enabled_ = session->IsSecurityEnabled();
    cipher_suite_        = session->Outbound_Cipher_Suite();
    cipher_key_id_       = session->Outbound_Cipher_Key_Id();
    cipher_engine_       = session->Outbound_Cipher_Engine();
    is_security_enabled_ = ((int) cipher_suite_ != -1) ? true : false;

    LTPSegment::Create_LTP_Header();
    if (control_byte == LTPSEGMENT_CAS_BR_BYTE)
        segment_type_      = LTP_SEGMENT_CAS_BR;
    else
        segment_type_      = LTP_SEGMENT_CAS_BS;
}

//-----------------------------------------------------------------
LTPCancelAckSegment::LTPCancelAckSegment(LTPCancelAckSegment& from)  : LTPSegment(from)
{
    packet_len_   = from.packet_len_;
}

//-----------------------------------------------------------------
LTPCancelAckSegment::~LTPCancelAckSegment()
{
}

//-----------------------------------------------------------------
const char * LTPCancelSegment::Get_Reason_Code(int reason_code)
{
    const char * return_var = "";
    static char temp[30];
    switch(reason_code)
    {
        case LTPCancelSegment::LTP_CS_REASON_USR_CANCLED:
            return_var = "Client Service Cancelled session";
            break;
        case LTPCancelSegment::LTP_CS_REASON_UNREACHABLE:
            return_var = "Unreashable client service";
            break;
        case LTPCancelSegment::LTP_CS_REASON_RLEXC:
            return_var = "Retransmission limit exceeded";
            break;
        case LTPCancelSegment::LTP_CS_REASON_MISCOLORED:
            return_var = "Miscolored segment";
            break;
        case LTPCancelSegment::LTP_CS_REASON_SYS_CNCLD:
            return_var = "A system error condition cuased Unexpected session termination";
            break;
        case LTPCancelSegment::LTP_CS_REASON_RXMTCYCEX:
            return_var = "Exceede the retransmission cycles limit";
            break;
        default:
            sprintf(temp,"ReasonCode Reserved (%02X)",(reason_code & 0xFF));
            return_var = temp;
    }
    return return_var;
}


//
//  LTPReportClaims .................................
//
//----------------------------------------------------------------------    
LTPReportClaim::LTPReportClaim(size_t offset, size_t len)
{
    char temp[24];
    sprintf(temp,"%10.10zu:%10.10zu",offset,len);
    offset_      = offset;
    length_      = len;
    key_         = temp;
}

//-----------------------------------------------------------------
LTPReportClaim::LTPReportClaim()
{
    length_       = 0;
    offset_       = 0;
}

//-----------------------------------------------------------------
LTPReportClaim::LTPReportClaim(LTPReportClaim& from)
{
    length_       = from.length_;
    offset_       = from.offset_;
}

//-----------------------------------------------------------------
LTPReportClaim::~LTPReportClaim()
{

}
}


