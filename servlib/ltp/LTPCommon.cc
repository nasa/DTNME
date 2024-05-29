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

#include <inttypes.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>



#include <third_party/oasys/util/HexDumpBuffer.h>

#include "LTPCommon.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"


#define FILEMODE_ALL (S_IRWXU | S_IRWXG | S_IRWXO )


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
LTPTimer::LTPTimer(SPtr_LTPNodeRcvrSndrIF sptr_rsif, int event_type, uint32_t seconds)
        : Logger("LTPTimer", "/ltp/timer"),
          wptr_rsif_(sptr_rsif),
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
    oasys::ScopeLock scoplok(&lock_, __func__);

    seconds_ = seconds; 
    SPtr_LTPNodeRcvrSndrIF sptr_rsif = wptr_rsif_.lock();

    if (sptr_rsif != nullptr) {
        target_counter_ = seconds + sptr_rsif->RSIF_aos_counter();
    }

    sptr_rsif.reset();
}

//--------------------------------------------------------------------------
void    
LTPTimer::start()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    SPtr_LTPNodeRcvrSndrIF sptr_rsif = wptr_rsif_.lock();

    if ((sptr_rsif != nullptr) && (sptr_to_self_ != nullptr)) {
        set_seconds(seconds_);
        schedule_in(seconds_*1000 + 100, sptr_to_self_);
    }
    // else canceled or already triggered and a new timer would have been created if needed
}

//--------------------------------------------------------------------------
bool    
LTPTimer::cancel()
{
    bool result = false;

    if (lock_.try_lock(__func__) == 0) {
        if (sptr_to_self_ != nullptr) {
            result = oasys::SharedTimer::cancel_timer(sptr_to_self_);
            sptr_to_self_.reset();
        }

        wptr_rsif_.reset();

        lock_.unlock();

    } else {
        // either the timeout processing is currently taking place
        // or another thread is already trying to cancel this timer
    }

    return result;
}

//--------------------------------------------------------------------------
void    
LTPTimer::timeout(const struct timeval &now)
{
    (void)now;

    oasys::ScopeLock scoplok(&lock_, __func__);

    if (cancelled() || (sptr_to_self_ == nullptr)) {
        // clear the internal reference so this object will be deleted
        sptr_to_self_.reset();
    } else {
        // quickly post this for processing
        // in order to release the TimerSystem to go back to its job

        SPtr_LTPNodeRcvrSndrIF my_rsif = wptr_rsif_.lock();

        if (my_rsif != nullptr)
            // NOTE: keeping the lock to prevent pointers
            //       from being modified/whacked by another
            //       thread before returning from the processing
            my_rsif->RSIF_post_timer_to_process(sptr_to_self_);
        else {
            // clear the internal reference so this object will be deleted
            sptr_to_self_.reset();
        }
    }
}





//--------------------------------------------------------------------------
LTPRetransmitSegTimer::LTPRetransmitSegTimer(SPtr_LTPNodeRcvrSndrIF sptr_rsif, int event_type, uint32_t seconds,
                                             SPtr_LTPSegment sptr_retran_seg)
        : LTPTimer(sptr_rsif, event_type, seconds),
          wptr_retran_seg_(sptr_retran_seg)
{
    BundleDaemon::instance()->inc_ltp_retran_timers_created();
}

//--------------------------------------------------------------------------
LTPRetransmitSegTimer::~LTPRetransmitSegTimer()
{
    BundleDaemon::instance()->inc_ltp_retran_timers_deleted();
}

//--------------------------------------------------------------------------
void    
LTPRetransmitSegTimer::do_timeout_processing()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    if (!cancelled() && (sptr_to_self_ != nullptr)) {

        // get a safe-to-use copy of the RSIF pointer
        SPtr_LTPNodeRcvrSndrIF my_rsif = wptr_rsif_.lock();
        SPtr_LTPSegment        my_seg = wptr_retran_seg_.lock();

        if ((my_rsif != nullptr) && (my_seg != nullptr)) {

            uint64_t right_now = my_rsif->RSIF_aos_counter();

            if (right_now >= target_counter_)
            {
                // no need to reschedule this timer so clear the internal reference 
                // so this object can be destroyed
                sptr_to_self_.reset();

                // NOTE: keeping the lock to prevent pointers
                //       from being modified/whacked by another
                //       thread before returning from the processing
                my_rsif->RSIF_retransmit_timer_triggered(event_type_, my_seg);
            }
            else 
            {
                my_seg.reset();
                my_rsif.reset();

                int seconds = (int) (target_counter_ - right_now);

                 // invoke the timer again
                schedule_in(seconds*1000, sptr_to_self_);
            }
        }
        else
        {
            sptr_to_self_.reset();
            wptr_rsif_.reset();
            wptr_retran_seg_.reset();
        }
    } else {
        sptr_to_self_.reset();
        wptr_rsif_.reset();
        wptr_retran_seg_.reset();
    }
}



//--------------------------------------------------------------------------
LTPCloseoutTimer::LTPCloseoutTimer(SPtr_LTPNodeRcvrSndrIF rsif, 
                                   uint64_t engine_id, uint64_t session_id)
        : LTPTimer(rsif, LTP_EVENT_CLOSEOUT_TIMEOUT, 30)
{
    BundleDaemon::instance()->inc_ltp_closeout_timers_created();

    engine_id_ = engine_id;
    session_id_ = session_id;
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

    if (!cancelled() && (sptr_to_self_ != nullptr)) {

        // get a safe-to-use copy of the RSIF pointer
        SPtr_LTPNodeRcvrSndrIF my_rsif = wptr_rsif_.lock();

        if (my_rsif != nullptr) {

            uint64_t right_now = my_rsif->RSIF_aos_counter();

            if (right_now >= target_counter_)
            {
                // no need to reschedule this timer so clear the internal reference 
                // so this object will be destroyed
                sptr_to_self_.reset();

                my_rsif->RSIF_closeout_timer_triggered(engine_id_, session_id_);
            }
            else
            {
                my_rsif.reset();

                int seconds = (int) (target_counter_ - right_now);

                 // invoke the timer again
                schedule_in(seconds*1000, sptr_to_self_);
            }
        }
        else
        {
            // clear the internal reference so this object will be destroyed
            sptr_to_self_.reset();
            wptr_rsif_.reset();
        }

        my_rsif.reset();

    } else {
        // clear the internal reference so this object will be destroyed
        sptr_to_self_.reset();
        wptr_rsif_.reset();
    }
}


//
//   LTPSession ......................
//
//----------------------------------------------------------------------    
LTPSession::LTPSession(uint64_t engine_id, uint64_t session_id, bool is_counted_session)
        : LTPSessionKey(engine_id, session_id),
          oasys::Logger("LTPSession","/ltp/session/%zu:%zu", engine_id, session_id),
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
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    if (is_counted_session_) {
        BundleDaemon::instance()->inc_ltpsession_deleted();
    }

    if (work_buf_) {
        free(work_buf_);
        work_buf_ = nullptr;
    }

    Cancel_Inactivity_Timer();

    checkpoints_.clear();
    RemoveSegments();
    delete bundle_list_;

    if (sptr_file_) {
        if (sptr_file_->data_file_fd_ > 0) {
            ::close(sptr_file_->data_file_fd_);
            ::unlink(file_path_.c_str());
        }
    }
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
     session_size_           = 0;
     report_serial_number_   = LTP_14BIT_RAND; // 1 through (2**14)-1=0x3FFF
     red_bytes_received_     = 0;
     inactivity_timer_       = nullptr;
     last_packet_time_       = 0;
     checkpoint_id_          = LTP_14BIT_RAND; // 1 through (2**14)-1=0x3FFF
     bundle_list_            = new BundleList("LTP_" + key_str());
     session_state_          = LTP_SESSION_STATE_CREATED;
     send_green_             = false;
     reports_sent_           = 0;
     first_contiguous_bytes_ = 0;
     inbound_cipher_suite_   = -1;
     inbound_cipher_key_id_  = -1;
     inbound_cipher_engine_  = "";
     outbound_cipher_suite_  = -1;
     outbound_cipher_key_id_ = -1;
     outbound_cipher_engine_ = "";
     is_security_enabled_    = false;

    red_segments_ = std::make_shared<LTP_DS_MAP>();
    green_segments_ = std::make_shared<LTP_DS_MAP>();
}

//----------------------------------------------------------------------    
std::string
LTPSession::key_str() const
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
LTPSession::TimeToSendIt(uint64_t agg_milliseconds)
{
    return agg_start_time_.elapsed_ms() >= agg_milliseconds;
}

//----------------------------------------------------------------------    
uint64_t
LTPSession::MicrosRemainingToSendIt(uint64_t agg_milliseconds)
{
    uint64_t agg_micros = agg_milliseconds * 1000;
    uint64_t elapsed_micros = agg_start_time_.elapsed_us();
    
    return (elapsed_micros >= agg_micros) ? 0 : (agg_micros - elapsed_micros);
}


//----------------------------------------------------------------------    
void 
LTPSession::RemoveSegments()
{
    LTP_DS_MAP::iterator seg_iter;
    SPtr_LTPDataSegment sptr_ds_seg;

    LTP_RS_MAP::iterator rs_iter;
    SPtr_LTPReportSegment rs_seg;

    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    checkpoints_.clear();

    // remove all segments from the red_segments_ list
    seg_iter = red_segments_->begin();
    while (seg_iter != red_segments_->end())  {
        sptr_ds_seg = seg_iter->second;
 
        sptr_ds_seg->Set_Deleted();

        red_segments_->erase(seg_iter);
        sptr_ds_seg.reset();

        seg_iter = red_segments_->begin();
    }

    // no timers associated with green segments so just erase them all
    green_segments_->clear();


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
LTPSession::Set_Cancel_Segment(SPtr_LTPCancelSegment new_segment)
{
    // basically a map of one element :)
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    if (cancel_segment_ != nullptr)
    {
        cancel_segment_->Set_Deleted();
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
        cancel_segment_ = nullptr;
    }
}

//----------------------------------------------------------------------    
void 
LTPSession::Set_Checkpoint_ID(uint64_t in_checkpoint)
{
    oasys::ScopeLock scoplok(&ids_lock_, __func__);

    checkpoint_id_  = in_checkpoint;
}

//----------------------------------------------------------------------    
uint64_t 
LTPSession::Get_Next_Checkpoint_ID()
{
    oasys::ScopeLock scoplok(&ids_lock_, __func__);

    return ++checkpoint_id_;
}

//----------------------------------------------------------------------    
uint64_t 
LTPSession::Get_Next_Report_Serial_Number()
{
    oasys::ScopeLock scoplok(&ids_lock_, __func__);

    return ++report_serial_number_;
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
ssize_t
LTPSession::First_RedSeg_Offset()
{
    oasys::ScopeLock scoplok (&segmap_lock_, __func__);

    ssize_t result = -1;

    LTP_DS_MAP::iterator seg_iter;
    seg_iter = red_segments_->begin();
    if (seg_iter != red_segments_->end()) {
        SPtr_LTPDataSegment sptr_ds_seg = seg_iter->second;
        result = sptr_ds_seg->Offset();
    }
    return result;
}

//----------------------------------------------------------------------    
ssize_t
LTPSession::Find_Checkpoint_Stop_Byte(uint64_t checkpoint_id)
{
    ssize_t stop_byte = -1;

    oasys::ScopeLock scoplok (&segmap_lock_, __func__);

    SPtr_LTPDataSegment sptr_ds_seg;
    LTP_CHECKPOINT_MAP::iterator ckpt_iter = checkpoints_.find(checkpoint_id);
    if (ckpt_iter != checkpoints_.end()) {
        stop_byte = ckpt_iter->second;
    }

    return stop_byte;
}

//----------------------------------------------------------------------    
void
LTPSession::RS_Claimed_Entire_Session()
{
    oasys::ScopeLock scoplok (&segmap_lock_, __func__);

    checkpoints_.clear();

    LTP_DS_MAP::iterator seg_iter = red_segments_->begin();

    // remove all segments from the red_segments_ list
    while (seg_iter != red_segments_->end())  {
        SPtr_LTPDataSegment sptr_ds_seg = seg_iter->second;

        sptr_ds_seg->Set_Deleted();

        red_segments_->erase(seg_iter);
        sptr_ds_seg.reset();

        seg_iter = red_segments_->begin();
    }
}

//----------------------------------------------------------------------    
size_t
LTPSession::RemoveClaimedSegments(uint64_t checkpoint_id, size_t lower_bounds, 
                                  size_t claim_offset, size_t upper_bounds,
                                  LTP_DS_MAP* retransmit_dsmap_ptr)
{
    ASSERT(segmap_lock_.is_locked_by_me());

    ASSERT(retransmit_dsmap_ptr != nullptr);

    size_t segs_claimed = 0;

    SPtr_LTPDataSegment sptr_ds_seg;
    LTP_DS_MAP::iterator seg_iter;
    LTP_DS_MAP::iterator del_seg_iter;
    seg_iter = red_segments_->lower_bound(lower_bounds);
    if (seg_iter != red_segments_->begin()) {
        --seg_iter;
    }

    while (seg_iter != red_segments_->end())
    {
        sptr_ds_seg = seg_iter->second;

        if (sptr_ds_seg->IsDeleted())
        {
            del_seg_iter = seg_iter;
            ++seg_iter; // increment before we remove!

            // remove the segment
            red_segments_->erase(del_seg_iter);

        }
        else if (sptr_ds_seg->Stop_Byte() < lower_bounds) 
        {
            // this segment is not covered by this claim
            ++seg_iter;
        }
        else if ((sptr_ds_seg->Offset() >= upper_bounds)) 
        {
            // this segment is past the end of this claim
            break;
        } 
        else if (sptr_ds_seg->Stop_Byte() < claim_offset)
        {
            if (sptr_ds_seg->IsCheckpoint() && (sptr_ds_seg->Checkpoint_ID() != checkpoint_id)) {
                // The checkpoint DS is not being claimed but it already has a retransmission timer
                // to force it to be resent so not adding it to the retransmit list
                //log_always("dzdebug - %s-%zu - not resending unclaimed inner chkpt %zu;  rpt chkpt: %zu",
                //           sptr_ds_seg->session_key_str().c_str(), sptr_ds_seg->Offset(), sptr_ds_seg->Checkpoint_ID(),
                //           checkpoint_id);

                ++seg_iter;

            } else {
                // this segment was not claimed and needs to be resent
                (*retransmit_dsmap_ptr)[sptr_ds_seg->Start_Byte()] = sptr_ds_seg;

                ++seg_iter;
            }

        }
        else if ((sptr_ds_seg->Offset() >= claim_offset) && 
                 (sptr_ds_seg->Stop_Byte() < upper_bounds))
        {
            if (sptr_ds_seg->IsCheckpoint() && (sptr_ds_seg->Checkpoint_ID() != checkpoint_id)) {
                // The chcekpoint DS is being claimed but the RS for it has not been received
                // so not marking claimed/deleted yet 
                //log_always("dzdebug - %s-%zu - not releasing claimed inner chkpt %zu;  rpt chkpt: %zu",
                //           sptr_ds_seg->session_key_str().c_str(), sptr_ds_seg->Offset(), sptr_ds_seg->Checkpoint_ID(),
                //           checkpoint_id);

                ++seg_iter;

            } else {
                ++segs_claimed;

                sptr_ds_seg->Set_Deleted();

                del_seg_iter = seg_iter;
                ++seg_iter; // increment before we remove!

                // remove the segment
                red_segments_->erase(del_seg_iter);
            }

        } 
        else 
        {
            log_always("%s : got to final else!!  RptSeg Off: %zu StopByte: %zu   lower: %zu  claim offset: %zu upper: %zu", 
                     __func__, sptr_ds_seg->Offset(), sptr_ds_seg->Stop_Byte(),
                     lower_bounds, claim_offset, upper_bounds);

            // this segment was not claimed and needs to be resent
            (*retransmit_dsmap_ptr)[sptr_ds_seg->Start_Byte()] = sptr_ds_seg;

            ++seg_iter;
        }

        sptr_ds_seg.reset();
    }

    return segs_claimed;
}

//----------------------------------------------------------------------    
int 
LTPSession::AddSegment_incoming(SPtr_LTPDataSegment sptr_ds_seg, bool check_for_overlap)
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);


    if (red_processed_) {
        return -1;
    }

    int return_status = -1;
    bool duplicate = false;

    LTP_DS_MAP::iterator seg_iter;

    if ( sptr_ds_seg->IsRedEndofblock() )  red_eob_received_ = true;

    switch(sptr_ds_seg->Red_Green()) {

        case RED_SEGMENT:
            if (check_for_overlap) {
                check_for_overlap = check_red_contig_blocks(sptr_ds_seg->Start_Byte(), 
                                                            sptr_ds_seg->Stop_Byte(), 
                                                            duplicate);
                if (duplicate) {
                    return -1;
                }
            }

            if (!check_for_overlap || CheckAndTrim(sptr_ds_seg.get())) { // make sure there are no collisions...
                if (expected_red_bytes_ < (sptr_ds_seg->Stop_Byte() + 1))
                {
                    expected_red_bytes_ = sptr_ds_seg->Stop_Byte() + 1;
                }
                if (sptr_ds_seg->IsEndofblock()) {
                    red_complete_ = true;
                }

                ++total_red_segments_;
                red_bytes_received_ += sptr_ds_seg->Payload_Length();


                if (red_bytes_received_ > expected_red_bytes_) {
                    log_crit("Received more bytes than expected!! ds_seg.start: %zu .stop: %zu  overlap: %s",
                             sptr_ds_seg->Start_Byte(), sptr_ds_seg->Stop_Byte(), check_for_overlap?"true":"false");
                    dump_red_contiguous_blocks();
                }

                if (use_file_) {
                    if (sptr_ds_seg->IsEndofblock() && (sptr_ds_seg->Offset() == 0)) {
                        // only use a file if more than one segment
                        use_file_ = false;
                    } else if (!store_data_in_file(sptr_ds_seg.get())) {
                        return -99;  // cancel the session
                    }
                }

                (*red_segments_)[sptr_ds_seg->Start_Byte()] = sptr_ds_seg;

                return_status = 0; 
            }
            break;

        case GREEN_SEGMENT:

            if (CheckAndTrim(sptr_ds_seg.get())) { // make sure there are no collisions...
                if (expected_green_bytes_ < (sptr_ds_seg->Stop_Byte()+1)) {
                    expected_green_bytes_ = (sptr_ds_seg->Stop_Byte()+1);
                }

                (*green_segments_)[sptr_ds_seg->Start_Byte()] = sptr_ds_seg;

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
LTPSession::AddSegment_outgoing(SPtr_LTPDataSegment sptr_ds_seg)
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    int return_status = -1;

    LTP_DS_MAP::iterator seg_iter;

    if ( sptr_ds_seg->IsRedEndofblock() )  red_eob_received_ = true;

    switch(sptr_ds_seg->Red_Green()) {

        case RED_SEGMENT:
            if (expected_red_bytes_ < (sptr_ds_seg->Stop_Byte() + 1))
            {
                expected_red_bytes_ = sptr_ds_seg->Stop_Byte() + 1;
            }
            if (sptr_ds_seg->IsEndofblock()) {
                red_complete_ = true;
            }

            ++total_red_segments_;
            red_bytes_received_ += sptr_ds_seg->Payload_Length();

            (*red_segments_)[sptr_ds_seg->Start_Byte()] = sptr_ds_seg;

            return_status = 0; 
            break;

        case GREEN_SEGMENT:

            if (expected_green_bytes_ < (sptr_ds_seg->Stop_Byte()+1)) {
                expected_green_bytes_ = (sptr_ds_seg->Stop_Byte()+1);
            }

            (*green_segments_)[sptr_ds_seg->Start_Byte()] = sptr_ds_seg;

            return_status = 0; 
            break;

        default:
            // invalid red/green segment indication
            break;
    }

    return return_status;
}

//----------------------------------------------------------------------    
bool 
LTPSession::check_red_contig_blocks(uint64_t start_byte, uint64_t stop_byte, bool& duplicate)
{
    ASSERT(segmap_lock_.is_locked_by_me());

    duplicate = false;

    bool overlap = false;
    uint64_t contig_start_byte = start_byte;
    uint64_t contig_stop_byte = stop_byte;

    LTP_U64_MAP::iterator prev_iter;
    LTP_U64_MAP::iterator bound_iter = red_contig_blocks_.lower_bound(start_byte);
    if (bound_iter == red_contig_blocks_.end()) {
        if (bound_iter != red_contig_blocks_.begin()) {
            prev_iter = bound_iter;
            --prev_iter;

            if (prev_iter->second >= stop_byte) {
                ASSERT(prev_iter->first <= start_byte);
                duplicate = true;
                overlap = true;

            } else if ((prev_iter->second == start_byte) || ((prev_iter->second + 1) == start_byte)) {
                if (prev_iter->second == start_byte) {
                    overlap = true;
                }

                // this segment extends this contiguous block
                prev_iter->second = stop_byte;

                contig_start_byte = prev_iter->first;
                contig_stop_byte = stop_byte;



            } else {
                // nothing equal or higher - this is the start of the highest known contiguous block
                red_contig_blocks_[start_byte] = stop_byte;
            }
        } else {
            // first entry in list
            red_contig_blocks_[start_byte] = stop_byte;
        }
    } else if (bound_iter->first == start_byte) {
        // already received this start byte
        ASSERT(bound_iter->second >= stop_byte);
        duplicate = true;
        overlap = true;

    } else {
        ASSERT(bound_iter->first > start_byte);

        if (bound_iter != red_contig_blocks_.begin()) {
            prev_iter = bound_iter;
            --prev_iter;

            ASSERT(prev_iter->first < start_byte);

            if (prev_iter->second >= stop_byte) {
                duplicate = true;
                overlap = true;

            } else if (prev_iter->second >= start_byte) {
                overlap = true;

                if (stop_byte > prev_iter->second) {
                    prev_iter->second = stop_byte;

                    // does it reach the block above it?
                    if ((stop_byte == bound_iter->first) || ((stop_byte + 1) == bound_iter->first)) {
                        // yes - extend this lower block
                        prev_iter->second = bound_iter->second;

                        // and delete the other
                        red_contig_blocks_.erase(bound_iter);
                    }
                } else {
                    duplicate = true;
                }

                contig_start_byte = prev_iter->first;
                contig_stop_byte = prev_iter->second;

            } else if ((prev_iter->second + 1) == start_byte) {
                // this segment extends this contigusous block
                prev_iter->second = stop_byte;

                // does it reach the one above it?
                if ((stop_byte == bound_iter->first) || ((stop_byte + 1) == bound_iter->first)) {
                    if (stop_byte == bound_iter->first) {
                        overlap = true;
                    }

                    // yes - extend this lower block
                    prev_iter->second = bound_iter->second;

                    // and delete the other
                    red_contig_blocks_.erase(bound_iter);
                }

                contig_start_byte = prev_iter->first;
                contig_stop_byte = prev_iter->second;


            } else { 
                // this is a new contiguous block
                // is it contiguous with the lower end of the bounding block?
                if ((stop_byte == bound_iter->first) || ((stop_byte + 1) == bound_iter->first)) {
                    if (stop_byte == bound_iter->first) {
                        overlap = true;
                    }

                    // yes, add this new block to include the bounding block
                    red_contig_blocks_[start_byte] = bound_iter->second;

                    contig_stop_byte = bound_iter->second;

                    // and delete the other
                    red_contig_blocks_.erase(bound_iter);

                } else {
                    // no, jsut a new block
                    red_contig_blocks_[start_byte] = stop_byte;

                }
            }
        } else {

            if (bound_iter->first <= stop_byte) {
                overlap = true;

                // add this new block to include the bounding block
                if (bound_iter->second > stop_byte) {
                    red_contig_blocks_[start_byte] = bound_iter->second;
                    contig_stop_byte = bound_iter->second;
                } else {
                    red_contig_blocks_[start_byte] = stop_byte;
                    contig_stop_byte = stop_byte;
                }

                // and delete the other
                red_contig_blocks_.erase(bound_iter);

            } else {
                // just a new stand-alone contiguous block 
                red_contig_blocks_[start_byte] = stop_byte;

                if ((stop_byte == bound_iter->first) || ((stop_byte + 1) == bound_iter->first)) {
                    if (stop_byte == bound_iter->first) {
                        overlap = true;
                    }

                    // yes, add this new block to include the bounding block
                    if (bound_iter->second > stop_byte) {
                        red_contig_blocks_[start_byte] = bound_iter->second;
                        contig_stop_byte = bound_iter->second;
                    } else {
                        red_contig_blocks_[start_byte] = stop_byte;
                        contig_stop_byte = stop_byte;
                    }

                    // and delete the other
                    red_contig_blocks_.erase(bound_iter);
                }
            }
        }
    }

    if (!duplicate && (contig_start_byte == 0)) {
        first_contiguous_bytes_ = contig_stop_byte + 1;
    }

    return overlap;
}


//----------------------------------------------------------------------    
bool 
LTPSession::CheckAndTrim(LTPDataSegment * ds_seg)
{
    ASSERT(segmap_lock_.is_locked_by_me());

    LTP_DS_MAP::iterator seg_iter;
 
    bool return_stats = false;  // don't insert this segment    
    bool checking     = true; 
    int lookup_status = UNKNOWN_SEGMENT;

    switch(ds_seg->Red_Green())
    {
        case RED_SEGMENT: 

          seg_iter = red_segments_->find(ds_seg->Start_Byte()); // if we find an exact match ignore this one
          if (seg_iter == red_segments_->end()) {  // we didn't find one...
              while (checking)
              {
                  lookup_status = Check_Overlap(red_segments_.get(), ds_seg); // keep looking until we have pruned the data
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

          seg_iter = green_segments_->find(ds_seg->Start_Byte());   // if we find an exact match ignore this one
          if (seg_iter == green_segments_->end()) {  // we didn't find one...
              while (checking)
              { 
                  lookup_status = Check_Overlap(green_segments_.get(), ds_seg); // keep looking until we have pruned the data
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
LTPSession::Check_Overlap(LTP_DS_MAP* segments, LTPDataSegment* ds_seg)
{
    ASSERT(segmap_lock_.is_locked_by_me());

    ssize_t adjust        = 0;
    int return_status = UNKNOWN_SEGMENT;

    if (segments->empty()) return LOOKUP_COMPLETE;

    LTP_DS_MAP::iterator seg_iter = segments->upper_bound(ds_seg->Start_Byte());

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

        if ((ds_seg->Start_Byte() > this_segment->Stop_Byte())) {
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
            segments->erase(this_segment->Start_Byte());

            red_bytes_received_ -= this_segment->Payload_Length();
            return DELETED_SEGMENT;
        }

        if (left_overlap) 
        {
            adjust = this_segment->Stop_Byte()-ds_seg->Offset();
            memmove(ds_seg->Payload(), ds_seg->Payload()+adjust, ds_seg->Payload_Length()-adjust); // move the data!!!
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
            ds_seg->Set_Stop_Byte(ds_seg->Start_Byte() + (ds_seg->Payload_Length()-1));
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
void
LTPSession::Add_Checkpoint_Seg(LTPDataSegment* ds_seg)
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    checkpoints_[ds_seg->Checkpoint_ID()] = ds_seg->Stop_Byte();
}

//----------------------------------------------------------------------
void
LTPSession::Flag_Checkpoint_As_Claimed(uint64_t checkpoint_id)
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    checkpoints_[checkpoint_id] = -99;
}

//----------------------------------------------------------------------
bool
LTPSession::HaveReportSegmentsToSend(LTPDataSegment * ds_seg, int32_t segsize, size_t& bytes_claimed)
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    bytes_claimed = 0;

    bool result = false;
    bool found_checkpoint = false;
    size_t lower_bounds = 0;
    size_t upper_bounds = ds_seg->Stop_Byte() + 1;

    LTP_U64_MAP::iterator offset_iter = ds_offset_to_upperbounds_.lower_bound(ds_seg->Offset());
    if (offset_iter != ds_offset_to_upperbounds_.begin()) {
        // backup to the entry that would come before this DS offset
        --offset_iter;

        // start this lower bound at the previous entry's upper bound
        lower_bounds = offset_iter->second;
    }
    // else start with zero for the lower bounds

    // Add this offset and upper bounds to the list for the next checkpoint
    ds_offset_to_upperbounds_[ds_seg->Offset()] = upper_bounds;


    LTP_RS_MAP::iterator rs_iter;
    SPtr_LTPReportSegment rptseg;

    rs_iter = report_segments_.begin();
    while (rs_iter != report_segments_.end())
    {
        rptseg = rs_iter->second;

        if (rptseg->Checkpoint_ID() == ds_seg->Checkpoint_ID())
        {
            // already generated and ready to send
            found_checkpoint = true;
            bytes_claimed = rptseg->ClaimedBytes();

            if (!rptseg->IsDeleted())
            {
                result = true;
            }
            break;
        }
        else if (rptseg->Report_Serial_Number() == ds_seg->Serial_ID())
        {
            // generate new reports based on the bounds of the original report
            lower_bounds = rptseg->LowerBounds();
            break;
        }
        else
        {
            ++rs_iter;
        }
    }

    if (!found_checkpoint)
    {

        bytes_claimed = GenerateReportSegments(ds_seg->Checkpoint_ID(), lower_bounds, upper_bounds, segsize);

        result = true;
    }

    return result;

}

//----------------------------------------------------------------------
size_t
LTPSession::GenerateReportSegments(uint64_t chkpt, size_t lower_bounds, size_t chkpt_upper_bounds, int32_t segsize)
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    size_t total_bytes_claimed = 0;
    size_t seg_bytes_claimed = 0;

    LTP_RC_MAP claim_map;
    LTP_RC_MAP::iterator rc_iter;
    SPtr_LTPReportClaim rc;
    SPtr_LTPReportSegment rptseg;

    if (red_complete_ && (red_bytes_received_ == expected_red_bytes_)) {
        // generate simple "got it all" report segment
        lower_bounds = 0;
        rc = std::make_shared<LTPReportClaim>(lower_bounds, red_bytes_received_);
        claim_map[rc->Offset()] = rc;

        seg_bytes_claimed = red_bytes_received_;
        total_bytes_claimed = seg_bytes_claimed;

        rptseg = std::make_shared<LTPReportSegment>(this, chkpt, lower_bounds, 
                                                    red_bytes_received_, seg_bytes_claimed, claim_map);
        report_segments_[rptseg->Report_Serial_Number()] = rptseg;

    } else if (first_contiguous_bytes_ >= chkpt_upper_bounds) {
        // generate simple "got all to this point" report segment
        lower_bounds = 0;
        rc = std::make_shared<LTPReportClaim>(lower_bounds, chkpt_upper_bounds);
        claim_map[rc->Offset()] = rc;

        seg_bytes_claimed = chkpt_upper_bounds - lower_bounds;
        total_bytes_claimed = seg_bytes_claimed;

        rptseg = std::make_shared<LTPReportSegment>(this, chkpt, lower_bounds, 
                                                    chkpt_upper_bounds, seg_bytes_claimed, claim_map);
        report_segments_[rptseg->Report_Serial_Number()] = rptseg;

    }
    else
    {
        bool      new_claim        = true;

        size_t    claim_start      = 0;
        size_t    claim_length     = 0;
        size_t    next_claim_start = 0;
        size_t    offset;
        int32_t   sdnv_size_of_claim;


        // reserve 40 bytes overhead for the non-claims info if there is room
        // otherwise it will probably be a bit larger than the preferred seg size
        int32_t est_seg_size  = (segsize > 60) ? 40 : 0;


        size_t    find_lower_bound = lower_bounds;

        size_t    rpt_lower_bounds = lower_bounds;
        size_t    rpt_upper_bounds = lower_bounds;

        size_t    block_start_byte = 0;
        size_t    block_stop_byte  = 0;
        size_t    block_len        = 0;

        if (first_contiguous_bytes_ > lower_bounds) {
            // The segments for the known first block of contiguous bytes can be skipped
            find_lower_bound = first_contiguous_bytes_;
            rpt_upper_bounds = first_contiguous_bytes_;

            // and start the first claim
            new_claim = false;
            claim_start = lower_bounds;
            claim_length = first_contiguous_bytes_ - lower_bounds;
            next_claim_start = claim_start + claim_length;
            seg_bytes_claimed = claim_length;

        }

        SPtr_LTPReportSegment rptseg;

        LTP_U64_MAP::iterator iter;

        oasys::ScopeLock scoplok(&segmap_lock_, __func__);

        iter = red_contig_blocks_.lower_bound(find_lower_bound);
        if (iter != red_contig_blocks_.begin()) {
            // back up one since lower_bound find first start_byte not less than 
            // but its stop byte can still extend past the our start/stop bytes
            --iter;
        }

        while (iter != red_contig_blocks_.end())
        {
            block_start_byte = iter->first;
            block_stop_byte = iter->second;
            block_len = block_stop_byte - block_start_byte + 1;
            ++iter;

            if (block_stop_byte < rpt_lower_bounds)
            {
                // skip segments until we get to the starting lower bounds -- shouldn't happen
                continue;
            }


            if (new_claim)
            {
                if (block_start_byte < chkpt_upper_bounds)
                {
                    new_claim = false;
                    if (block_start_byte < rpt_lower_bounds) {
                        claim_start = rpt_lower_bounds;
                        claim_length = block_len - (rpt_lower_bounds - block_start_byte);
                    } else {
                        claim_start = block_start_byte;
                        claim_length = block_len;
                    }
                    seg_bytes_claimed += block_len;
                    next_claim_start = claim_start + claim_length;
                    rpt_upper_bounds = next_claim_start;
                } else {
                    break;
                }
            }
            else if (block_start_byte >= chkpt_upper_bounds)
            {
                // close out the in-progress claim
                break;
            }
            else if (next_claim_start == block_start_byte)
            {
                // contiguouos - extend this claim
                claim_length += block_len;
                seg_bytes_claimed += block_len;
                next_claim_start = claim_start + claim_length;
                rpt_upper_bounds = next_claim_start;
            }
            else
            {
                // hit a gap - save off this claim and start a new one
                rc = std::make_shared<LTPReportClaim>(claim_start, claim_length);
                claim_map[rc->Offset()] = rc;


                // add this claim to the estimated segment length
                offset = claim_start - rpt_lower_bounds;  // use offset from lower bounds to determine SDNV length
                sdnv_size_of_claim = SDNV::encoding_len(offset) + SDNV::encoding_len(claim_length);
                est_seg_size += sdnv_size_of_claim;

                if (est_seg_size >= segsize)
                {
                    // this claim tips us over the "soft" max size - close out the report

                    if (chkpt_upper_bounds < rpt_upper_bounds) {
                        seg_bytes_claimed -= (rpt_upper_bounds - chkpt_upper_bounds);
                        rpt_upper_bounds = chkpt_upper_bounds;
                    }
                    total_bytes_claimed += seg_bytes_claimed;


                    // create this report segment
                    rptseg = std::make_shared<LTPReportSegment>(this, chkpt, rpt_lower_bounds, 
                                                                rpt_upper_bounds, seg_bytes_claimed, claim_map);
                    report_segments_[rptseg->Report_Serial_Number()] = rptseg;

                    // init for next report segment
                    claim_map.clear();

                    rpt_lower_bounds = rpt_upper_bounds;

                    // reserve 40 bytes overhead for the non-claims info if there is room
                    // otherwise it will probably be a bit larger than the preferred seg size
                    est_seg_size  = (segsize > 60) ? 40 : 0;


                    if (chkpt_upper_bounds == rpt_upper_bounds) {
                        // completed this RS
                        new_claim = true;  // no claim in-progress
                        break;
                    }
                    
                }

                // back up the iterator and configure to start a new claim
                --iter;
                new_claim        = true;
                claim_start      = 0;
                claim_length     = 0;
                next_claim_start = 0;
           }
        }

        if (!new_claim)
        {
            if (chkpt_upper_bounds < rpt_upper_bounds) {
                size_t overage = rpt_upper_bounds - chkpt_upper_bounds;
                seg_bytes_claimed -= overage;

                ASSERT(claim_length > overage);
                claim_length -= overage;

                rpt_upper_bounds = chkpt_upper_bounds;
            }
            total_bytes_claimed += seg_bytes_claimed;

            // create the final report segment
            rc = std::make_shared<LTPReportClaim>(claim_start, claim_length);
            claim_map[rc->Offset()] = rc;


            // create this report segment
            rptseg = std::make_shared<LTPReportSegment>(this, chkpt, rpt_lower_bounds, 
                                                        rpt_upper_bounds, seg_bytes_claimed, claim_map);
            report_segments_[rptseg->Report_Serial_Number()] = rptseg;
        }
    }

    return total_bytes_claimed;
}

//----------------------------------------------------------------------    
size_t
LTPSession::get_num_red_contiguous_bloks()
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);
    return red_contig_blocks_.size();
}

//----------------------------------------------------------------------    
void
LTPSession::dump_red_contiguous_blocks()
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    log_always("   List of contiguous blocks  (expected bytes: %zu - received: %zu):",
               expected_red_bytes_, red_bytes_received_);

    ssize_t last_stop = -1;
    size_t gap = 0;

    LTP_U64_MAP::iterator iter;
    iter = red_contig_blocks_.begin();

    while (iter != red_contig_blocks_.end())
    {
        if (last_stop == -1) {
            log_always("        Start: %12zu    Stop: %12zu", iter->first, iter->second);
        } else {
            gap = iter->first - last_stop - 1;
            log_always("        Start: %12zu    Stop: %12zu   gap: %12zu", iter->first, iter->second, gap);
        }
        last_stop = iter->second;

        ++iter;
    }

}


//----------------------------------------------------------------------    
void
LTPSession::dump_red_segments(oasys::StringBuffer* buf)
{
    LTP_DS_MAP::iterator seg_iter = red_segments_->begin();

    while (seg_iter != red_segments_->end()) {
        SPtr_LTPDataSegment sptr_ds_seg = seg_iter->second;

        buf->appendf("        Seg- Start: %zu  Stop: %zu  (Len: %zu) - Chkpt: %zu RptSerial: %zu\n",
                     sptr_ds_seg->Start_Byte(), sptr_ds_seg->Stop_Byte(), sptr_ds_seg->Payload_Length(),
                     sptr_ds_seg->Checkpoint_ID(), sptr_ds_seg->Serial_ID());

        ++seg_iter;
    }

}


//----------------------------------------------------------------------    
size_t
LTPSession::IsRedFull() 
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

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
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    if (!green_complete_ || green_processed_) return false; // we haven't seen the EOB packet or we've already sent this.

    size_t total_bytes = 0;

    for (LTP_DS_MAP::iterator seg_iter=green_segments_->begin(); seg_iter!=green_segments_->end(); ++seg_iter) {

         SPtr_LTPDataSegment this_segment = seg_iter->second;
         total_bytes                     += this_segment->Payload_Length(); 
    }        

    if (total_bytes == expected_green_bytes_) return total_bytes;
    return 0; // we aren't complete yet.
}

//----------------------------------------------------------------------    
SPtr_LTP_DS_MAP
LTPSession::GetAllRedData()
{
    SPtr_LTP_DS_MAP copy_to_map = std::make_shared<LTP_DS_MAP>();

    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    // it is assumed that a call to IsRedFull() was made first and returned true - need to add check here?
    ASSERT(IsRedFull() > 0);

    LTP_DS_MAP::iterator seg_iter;
    for (seg_iter=red_segments_->begin(); seg_iter!=red_segments_->end(); ++seg_iter) {
        (*copy_to_map)[seg_iter->first] = seg_iter->second;
    }        

    red_processed_ = true;

    return copy_to_map;
}

//----------------------------------------------------------------------    
void
LTPSession::GetRedDataFile(std::string& file_path)
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    // only using the file path - bundle extractor will open the file itself
    ::close(sptr_file_->data_file_fd_);

    if (work_buf_) {
        free(work_buf_);
        work_buf_ = nullptr;
    }

    sptr_file_.reset();

    file_path = file_path_;

    red_processed_ = true;
}

//----------------------------------------------------------------------    
SPtr_LTP_DS_MAP
LTPSession::GetAllGreenData()
{
    SPtr_LTP_DS_MAP copy_to_map = std::make_shared<LTP_DS_MAP>();

    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    // it is assumed that a call to IsGreenFull() was made first and returned true - need to add check here?

    LTP_DS_MAP::iterator seg_iter;
    for (seg_iter=green_segments_->begin(); seg_iter!=green_segments_->end(); ++seg_iter) {
        (*copy_to_map)[seg_iter->first] = seg_iter->second;
    }        

    green_processed_ = true;

    return copy_to_map;
}

//----------------------------------------------------------------------    
void
LTPSession::Start_Inactivity_Timer(SPtr_LTPNodeRcvrSndrIF rsif, time_t time_left)
{
    // basically a map of one element :)
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    inactivity_timer_ = std::make_shared<InactivityTimer>(rsif, engine_id_, session_id_);
    inactivity_timer_->set_sptr_to_self(inactivity_timer_);
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
LTPSession::Start_Closeout_Timer(SPtr_LTPNodeRcvrSndrIF rsif, time_t time_left)
{
    oasys::ScopeLock scoplok(&segmap_lock_, __func__);

    if (time_left < 10) time_left = 10;

    // we don't want this timer to be cancelled when the session is deleted so using a temp pointer here
    SPtr_LTPTimer closeout_timer = std::make_shared<LTPCloseoutTimer>(rsif, engine_id_, session_id_);

    closeout_timer->set_sptr_to_self(closeout_timer);
    closeout_timer->set_seconds(time_left);
    closeout_timer->start();
    closeout_timer.reset();
}

//--------------------------------------------------------------------------
LTPSession::InactivityTimer::InactivityTimer(SPtr_LTPNodeRcvrSndrIF rsif, 
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

    if (!cancelled() && (sptr_to_self_ != nullptr)) {

        // get a safe-to-use copy of the RSIF pointer
        SPtr_LTPNodeRcvrSndrIF my_rsif = wptr_rsif_.lock();

        if (my_rsif != nullptr) {

            uint64_t right_now = my_rsif->RSIF_aos_counter();

            if (right_now >= target_counter_)
            {
                // no need to reschedule this timer so clear the internal reference 
                // so this object will be destroyed
                sptr_to_self_.reset();

                my_rsif->RSIF_inactivity_timer_triggered(engine_id_, session_id_);

                wptr_rsif_.reset();
            }
            else
            {
                my_rsif.reset();

                int seconds = (int) (target_counter_ - right_now);

                // invoke a new timer!
                schedule_in(seconds*1000, sptr_to_self_);
            }
        }
        else
        {
            // clear the internal reference so this object will be destroyed
            sptr_to_self_.reset();
            wptr_rsif_.reset();
        }
    } else {
        // clear the internal reference so this object will be destroyed
        sptr_to_self_.reset();
        wptr_rsif_.reset();
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

//----------------------------------------------------------------------
void
LTPSession::set_file_usage(std::string& dir_path, bool run_with_disk_io_kludge)
{
    use_file_ = true;

    dir_base_path_ = dir_path;
    if (dir_base_path_.length() == 0) {
        dir_base_path_ = ".";  // default to the current working directory
    }

    use_disk_io_kludges_ = run_with_disk_io_kludge;

}

//----------------------------------------------------------------------
bool
LTPSession::store_data_in_file(LTPDataSegment* ds_seg)
{
    bool result = false;

    if (use_file_) {
        if (open_file_write()) {
            result = write_ds_to_file(ds_seg);
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
LTPSession::open_file_write()
{
    if (sptr_file_ && (sptr_file_->data_file_fd_ > 0)) {
        return true;
    }

    bool result = false;

    if (dir_path_.length() == 0) {
        oasys::StringBuffer sb;
        sb.appendf("%s/tmp_ltp_%zu", dir_base_path_.c_str(), engine_id_);
        dir_path_ = sb.c_str();

        if (!create_storage_path(dir_path_)) {
            use_file_ = false;
            disk_io_error_ = true;
        } else {
            // directory structure is good
            // create the file path
            time_t now;
            struct tm *timeinfo;
            char time_str[80];
            time(&now);
            timeinfo = localtime(&now);
            strftime(time_str, 80, "%Y-%m-%d_%H-%M-%S", timeinfo);

            sb.appendf("/ltpsession_%zu-%zu__%s.dat", engine_id_, session_id_, time_str);

            file_path_ = sb.c_str();

            sptr_file_ = std::make_shared<LTPDataFile>();
        }
    }

    if (!disk_io_error_ && sptr_file_) {
        oasys::ScopeLock(&sptr_file_->lock_, __func__);

        if (sptr_file_->data_file_fd_ < 0) {
            sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0777);
            //sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0777);

            if (sptr_file_->data_file_fd_ < 0) {
                if (errno == EEXIST) {
                    // file already exists - adjust the name and try again....
                    char rand_str[16];
                    snprintf(rand_str, sizeof(rand_str), "_%u", LTP_14BIT_RAND);
                    file_path_.append(rand_str);

                    log_err("LTP Session %s: file already exists - trying new name: %s",
                            key_str().c_str(), file_path_.c_str());


                    sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0777);
                }

                if (sptr_file_->data_file_fd_ < 0) {
                    ASSERT(bytes_written_to_disk_ == 0);

                    log_err("error creating LTP data file: %s - %s - no data written yet so using memory",
                             file_path_.c_str(), strerror(errno));

                    use_file_ = false;
                    disk_io_error_ = true;
                }
            } else {
                result = true;
            }
        } else {
            result = true;
        }
    }

    return result;
}

//----------------------------------------------------------------------
bool
LTPSession::open_file_read()
{
    if (sptr_file_ && (sptr_file_->data_file_fd_ > 0)) {
        log_always("%s - file already open?? Should not happen!", __func__);
        return true;
    }

    bool result = false;

    if (dir_path_.length() == 0) {
        oasys::StringBuffer sb;
        sb.appendf("%s/tmp_ltp_%zu", dir_base_path_.c_str(), engine_id_);
        dir_path_ = sb.c_str();

        if (!create_storage_path(dir_path_)) {
            use_file_ = false;
            disk_io_error_ = true;
        } else {
            // directgory structure is good
            // create the file path
            sb.appendf("/ltpsession_%zu-%zu.dat", engine_id_, session_id_);

            file_path_ = sb.c_str();

            sptr_file_ = std::make_shared<LTPDataFile>();
        }
    }

    if (!disk_io_error_ && sptr_file_) {
        oasys::ScopeLock(&sptr_file_->lock_, __func__);

        if (sptr_file_->data_file_fd_ < 0) {
            sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_RDONLY);

            if (sptr_file_->data_file_fd_ < 0) {
                log_err("error opening LTP data file for reading: %s - %s",
                         file_path_.c_str(), strerror(errno));

                use_file_ = false;
                disk_io_error_ = true;

            } else {
                result = true;
            }
        } else {
            result = true;
        }
    }

    return result;
}

//----------------------------------------------------------------------
SPtr_LTPDataFile
LTPSession::open_session_file_write(std::string& file_path)
{
  open_file_write();

  file_path = file_path_;

  return sptr_file_;
}

//----------------------------------------------------------------------
SPtr_LTPDataFile
LTPSession::open_session_file_read(std::string& file_path)
{
  open_file_read();

  file_path = file_path_;

  return sptr_file_;
}

//----------------------------------------------------------------------
bool
LTPSession::create_storage_path(std::string& path)
{

    bool result = false;

    errno = 0;
    int retval = mkdir(path.c_str(), FILEMODE_ALL);

    if ((retval == 0) || (errno == EEXIST)) {
        // success
        result = true;
    } else if (errno != ENOENT) {
        // unable to create a directory - fall through to return false
        log_err("Error creating storage path: %s - (%d) %s", path.c_str(), errno, strerror(errno));
    } else {
        // ENOENT - higher level directory does not exist yet
        size_t pos = path.find_last_of('/');
        
        if (pos != std::string::npos) {
            // try to create the next higher directory
            std::string higher_path = path.substr(0, pos);

            result = create_storage_path(higher_path);

            if (result) {
                result = false;

                errno = 0;
                retval = mkdir(path.c_str(), FILEMODE_ALL);

                if (retval == 0) {
                    // success
                    result = true;
                } else {
                    log_err("Error creating storage path after higher directory was created: %s - (%d) %s", 
                            path.c_str(), errno, strerror(errno));
                }
            } else {
                log_err("Error creating higher directory: %s - aborting", path.c_str());
            }
        }
    }

    return result;
}


//----------------------------------------------------------------------
/**
 * Buffers contiguous data segments to perform larger disk write operations 
 * to improve performance when possible
 *
 */
bool
LTPSession::write_ds_to_file(LTPDataSegment* ds_seg)
{
    if (use_disk_io_kludges_) {
        return write_ds_to_file_with_kludges(ds_seg);
    }


    bool result = true;

    off64_t cur_ds_offset = ds_seg->Offset();
    off64_t cur_ds_len = ds_seg->Payload_Length();


    bool is_eob = ds_seg->IsEndofblock();
    bool last_seg = red_complete_ && (red_bytes_received_ == expected_red_bytes_);


    if (work_buf_idx_ != 0) {
        ASSERT(prev_seg_payload_ == nullptr);

        if (cur_ds_offset == (work_buf_file_offset_ + work_buf_idx_)) {
            // contiguous data to that in the work_buf
            if (cur_ds_len <= work_buf_available_) {
                // add it to the work_buf
                memcpy(work_buf_ + work_buf_idx_, ds_seg->Payload(), cur_ds_len);

                work_buf_idx_ += cur_ds_len;
                work_buf_available_ -= cur_ds_len;

                // delete the DS Payload
                free(ds_seg->Release_Payload());
            } else {
                // write the work_buf to disk (empty it)
                result = write_work_buf_to_file();

                // but just save off this payload until we see if the next
                // payload is contiguous to it
                prev_seg_payload_offset_ = cur_ds_offset;
                prev_seg_payload_len_ = cur_ds_len;
                prev_seg_payload_ = ds_seg->Release_Payload();
            }
        } else {
            // this payload is not contiguous
            // write the work_buf to disk
            result = write_work_buf_to_file();

            // but just save off this payload until we see if the next
            // payload is contiguous to it
            prev_seg_payload_offset_ = cur_ds_offset;
            prev_seg_payload_len_ = cur_ds_len;
            prev_seg_payload_ = ds_seg->Release_Payload();
        }
    } else if (prev_seg_payload_len_ != 0) {

        if (cur_ds_offset == (prev_seg_payload_offset_ + prev_seg_payload_len_)) {
            // kind of assuming that the work_buf allocation isze is greater than a data segment or two
            // contiguous data to that of the prev_seg
            // - hope for the best and combine these two segs into the work_buf
            if (work_buf_ == nullptr) {
                if ((prev_seg_payload_len_ + cur_ds_len) < work_buf_alloc_size_) {
                    work_buf_ = (u_char*) malloc(work_buf_alloc_size_);
                    work_buf_size_ = work_buf_alloc_size_;
                    work_buf_idx_ = 0;
                    work_buf_available_ = work_buf_size_;
                }
            }

            if ((prev_seg_payload_len_ + cur_ds_len) < work_buf_available_) {
                // copy the previous seg into the work_buf,
                ASSERT(work_buf_idx_ == 0);
                memcpy(work_buf_ + work_buf_idx_, prev_seg_payload_, prev_seg_payload_len_);

                work_buf_file_offset_ = prev_seg_payload_offset_;  //only for first seg in the work_buf
                work_buf_idx_ += prev_seg_payload_len_;
                work_buf_available_ -= prev_seg_payload_len_;

                // clear the prev_seg
                prev_seg_payload_offset_ = 0;
                prev_seg_payload_len_ = 0;
                free(prev_seg_payload_);  // finished with the prev seg payload
                prev_seg_payload_ = nullptr;

                // and copy the new seg into the work buf
                memcpy(work_buf_ + work_buf_idx_, ds_seg->Payload(), cur_ds_len);

                work_buf_idx_ += cur_ds_len;
                work_buf_available_ -= cur_ds_len;

                // delete the DS Payload
                free(ds_seg->Release_Payload());
            } else {
                // write the previous seg to file
                result = write_prev_seg_to_file();
                ASSERT(prev_seg_payload_ == nullptr);

                // and save the new seg as the previous seg for the next one
                // (maybe the previous seg was extra large and there will be 
                //  savings with future ones)
                // NOTE: not assuming that UDP will be the only source of segments in the future
                prev_seg_payload_offset_ = cur_ds_offset;
                prev_seg_payload_len_ = cur_ds_len;
                prev_seg_payload_ = ds_seg->Release_Payload();
            }
        } else {
            // this payload is not contiguous

            // write the prev seg to disk
            result = write_prev_seg_to_file();
            ASSERT(prev_seg_payload_ == nullptr);

            // but just save off this payload until we see if the next
            // payload is contiguous to it
            prev_seg_payload_offset_ = cur_ds_offset;
            prev_seg_payload_len_ = cur_ds_len;
            prev_seg_payload_ = ds_seg->Release_Payload();
        }
    } else {

        // this is the first segment - save it as the previous seg
        ASSERT(prev_seg_payload_ == nullptr);
        prev_seg_payload_offset_ = cur_ds_offset;
        prev_seg_payload_len_ = cur_ds_len;
        prev_seg_payload_ = ds_seg->Release_Payload();
    }

    // force write to disk if last seg
    if (is_eob || (last_seg && result)) {
        if (work_buf_idx_ != 0) {
            // write the work_buf to disk
            result = write_work_buf_to_file();
        } else if (prev_seg_payload_len_ != 0) {
            result = write_prev_seg_to_file();
        }
    }


    return result;
}


//----------------------------------------------------------------------
bool
LTPSession::write_work_buf_to_file()
{
    bool result = true;

    oasys::ScopeLock(&sptr_file_->lock_, __func__);

    // seek to the correct position in the file
    off64_t new_offset = ::lseek64(sptr_file_->data_file_fd_, work_buf_file_offset_, SEEK_SET);

    if (new_offset < 0) {
        log_err("%s - error seeking to new position (%zu) in file: %s - %s", 
                __func__, prev_seg_payload_offset_, 
                file_path_.c_str(), strerror(errno));

        disk_io_error_ = true;
        result = false;
    } else if (new_offset != work_buf_file_offset_) {
        log_err("Error -  seeking to offset (%zd) returned different value (%zd) in file: %s - %s",
                prev_seg_payload_offset_, new_offset,
                file_path_.c_str(), strerror(errno));

        disk_io_error_ = true;
        result = false;
    }

    if (result) {
        // write the work_buf to disk
        errno = 0;
        ssize_t cc = ::write(sptr_file_->data_file_fd_, work_buf_, work_buf_idx_);

        if (cc < 0) {
            log_err("%s - error writing to file: %s - %s", __func__, file_path_.c_str(), strerror(errno));
            disk_io_error_ = true;
            result = false;
        } else {
            bytes_written_to_disk_ += cc;
        }
    }

    // refresh the work_buf for use no matter the result
    work_buf_idx_ = 0;
    work_buf_file_offset_ = 0;
    work_buf_available_ = work_buf_size_;

    return result;
}

//----------------------------------------------------------------------
bool
LTPSession::write_prev_seg_to_file()
{
    bool result = true;

    oasys::ScopeLock(&sptr_file_->lock_, __func__);

    // seek to the correct position in the file
    off64_t new_offset = ::lseek64(sptr_file_->data_file_fd_, prev_seg_payload_offset_, SEEK_SET);


    if (new_offset < 0) {
        log_err("%s - error seeking to new position (%zu) in file: %s - %s", 
                __func__, prev_seg_payload_offset_, 
                file_path_.c_str(), strerror(errno));

        disk_io_error_ = true;
        result = false;
    } else if (new_offset != prev_seg_payload_offset_) {
        log_err("Error -  seeking to offset (%zd) returned different value (%zd) in file: %s - %s",
                prev_seg_payload_offset_, new_offset,
                file_path_.c_str(), strerror(errno));

        disk_io_error_ = true;
        result = false;
    }

    if (result) {
        // write the work_buf to disk
        errno = 0;
        ssize_t cc = ::write(sptr_file_->data_file_fd_, prev_seg_payload_, prev_seg_payload_len_);

        if (cc < 0) {
            log_err("%s - error writing to file: %s - %s", __func__, file_path_.c_str(), strerror(errno));
            disk_io_error_ = true;
            result = false;
        } else {
            bytes_written_to_disk_ += cc;
        }
    }

    // clear the prev_seg no matter what the result
    prev_seg_payload_offset_ = 0;
    prev_seg_payload_len_ = 0;
    free(prev_seg_payload_);  // finished with the prev seg payload
    prev_seg_payload_ = nullptr;

    return result;
}

//----------------------------------------------------------------------
bool
LTPSession::write_ds_to_file_with_kludges(LTPDataSegment* ds_seg)
{
    // kind of assuming that the work_buf allocation isze is greater than a data segment or two
    bool result = true;

    off64_t cur_ds_offset = ds_seg->Offset();
    off64_t cur_ds_len = ds_seg->Payload_Length();


    bool is_eob = ds_seg->IsEndofblock();  // End Of Block received in this DS - last extent of the file/TLP data
    bool last_seg = red_complete_ && (red_bytes_received_ == expected_red_bytes_);  // this DS fills in final "hole" in the file/LTP data

    if (work_buf_idx_ != 0) {
        ASSERT(prev_seg_payload_ == nullptr);

        if (cur_ds_offset == (work_buf_file_offset_ + work_buf_idx_)) {
            // contiguous data to that in the work_buf
            if (cur_ds_len <= work_buf_available_) {
                // add it to the work_buf
                memcpy(work_buf_ + work_buf_idx_, ds_seg->Payload(), cur_ds_len);

                work_buf_idx_ += cur_ds_len;
                work_buf_available_ -= cur_ds_len;

                // delete the DS Payload
                free(ds_seg->Release_Payload());

            } else {
                // write the work_buf to disk (empty it)
                // - false == does not include the final extent of the file
                result = write_work_buf_to_file_with_kludges(false);

                // but just save off this payload until we see if the next
                // payload is contiguous to it
                prev_seg_payload_offset_ = cur_ds_offset;
                prev_seg_payload_len_ = cur_ds_len;
                prev_seg_payload_ = ds_seg->Release_Payload();
            }
        } else {
            // this payload is not contiguous
            // write the work_buf to disk
            // - false == does not include the final extent of the file
            result = write_work_buf_to_file_with_kludges(false);

            // but just save off this payload until we see if the next
            // payload is contiguous to it
            prev_seg_payload_offset_ = cur_ds_offset;
            prev_seg_payload_len_ = cur_ds_len;
            prev_seg_payload_ = ds_seg->Release_Payload();
        }
    } else if (prev_seg_payload_len_ != 0) {

        if (cur_ds_offset == (prev_seg_payload_offset_ + prev_seg_payload_len_)) {
            // contiguous data to that of the prev_seg
            // - hope for the best and combine these two segs into the work_buf
            if (work_buf_ == nullptr) {
                if ((prev_seg_payload_len_ + cur_ds_len) < work_buf_alloc_size_) {
                    work_buf_ = (u_char*) malloc(work_buf_alloc_size_);
                    work_buf_size_ = work_buf_alloc_size_;
                    work_buf_idx_ = 0;
                    work_buf_available_ = work_buf_size_;
                }
            }

            if ((prev_seg_payload_len_ + cur_ds_len) < work_buf_available_) {
                // copy the previous seg into the work_buf,
                ASSERT(work_buf_idx_ == 0);
                memcpy(work_buf_ + work_buf_idx_, prev_seg_payload_, prev_seg_payload_len_);

                work_buf_file_offset_ = prev_seg_payload_offset_;  //only for first seg in the work_buf
                work_buf_idx_ += prev_seg_payload_len_;
                work_buf_available_ -= prev_seg_payload_len_;

                // clear the prev_seg
                prev_seg_payload_offset_ = 0;
                prev_seg_payload_len_ = 0;
                free(prev_seg_payload_);  // finished with the prev seg payload
                prev_seg_payload_ = nullptr;

                // and copy the new seg into the work buf
                memcpy(work_buf_ + work_buf_idx_, ds_seg->Payload(), cur_ds_len);

                work_buf_idx_ += cur_ds_len;
                work_buf_available_ -= cur_ds_len;

                // delete the DS Payload
                free(ds_seg->Release_Payload());
            } else {
                // write the previous seg to file
                // - false == does not include the final extent of the file
                result = write_prev_seg_to_file_with_kludges(false);
                ASSERT(prev_seg_payload_ == nullptr);

                // and save the new seg as the previous seg for the next one
                // (maybe the previous seg was extra large and there will be 
                //  savings with future ones)
                // NOTE: not assuming that UDP will be the only source of segments in the future
                prev_seg_payload_offset_ = cur_ds_offset;
                prev_seg_payload_len_ = cur_ds_len;
                prev_seg_payload_ = ds_seg->Release_Payload();
            }
        } else {
            // this payload is not contiguous

            // write the prev seg to disk
            // - false == does not include the final extent of the file
            result = write_prev_seg_to_file_with_kludges(false);
            ASSERT(prev_seg_payload_ == nullptr);

            // but just save off this payload until we see if the next
            // payload is contiguous to it
            prev_seg_payload_offset_ = cur_ds_offset;
            prev_seg_payload_len_ = cur_ds_len;
            prev_seg_payload_ = ds_seg->Release_Payload();
        }
    } else {

        // this is the first segment - save it as the previous seg
        ASSERT(prev_seg_payload_ == nullptr);
        prev_seg_payload_offset_ = cur_ds_offset;
        prev_seg_payload_len_ = cur_ds_len;
        prev_seg_payload_ = ds_seg->Release_Payload();
    }

    if (is_eob || (last_seg && result)) {
        if (work_buf_idx_ != 0) {
            // write the work_buf to disk
            result = write_work_buf_to_file_with_kludges(is_eob);
        } else if (prev_seg_payload_len_ != 0) {
            result = write_prev_seg_to_file_with_kludges(is_eob);
        }
    }


    return result;
}


//----------------------------------------------------------------------
bool
LTPSession::write_ds_to_file_with_kludges_prev_seg_only(LTPDataSegment* ds_seg)
{
    // kind of assuming that the work_buf allocation isze is greater than a data segment or two
    bool result = true;

    bool is_eob = ds_seg->IsEndofblock();

    prev_seg_payload_offset_ = ds_seg->Offset();
    prev_seg_payload_len_ = ds_seg->Payload_Length();
    prev_seg_payload_ = ds_seg->Release_Payload();

    result = write_prev_seg_to_file_with_kludges(is_eob);

    return result;
}


//----------------------------------------------------------------------
bool
LTPSession::write_prev_seg_to_file_with_kludges(bool is_eob)
{
    bool result = true;

    oasys::ScopeLock(&sptr_file_->lock_, __func__);

    // seek to the correct position in the file
    off64_t cur_offset = ::lseek64(sptr_file_->data_file_fd_, 0, SEEK_CUR);
    off64_t adjust = prev_seg_payload_offset_- cur_offset;
    off64_t new_offset = -1;

    if (adjust < 0) {
        result = reopen_file_and_seek_kludge(prev_seg_payload_offset_);
        adjust = 0;
    }

    new_offset = ::lseek64(sptr_file_->data_file_fd_, adjust, SEEK_CUR);

    if (new_offset < 0) {
        result = reopen_file_and_seek_kludge(prev_seg_payload_offset_);
        if (result) {
            new_offset = prev_seg_payload_offset_;
        }
    } else if (new_offset != prev_seg_payload_offset_) {
        log_err("Error -  seeking to offset (%zu) returned different value (%zd) - try close and reopen...",
                prev_seg_payload_offset_, new_offset);
        result = reopen_file_and_seek_kludge(prev_seg_payload_offset_);
        if (result) {
            new_offset = prev_seg_payload_offset_;
        }
    }

    if (result) {
        // write the prev_seg to disk
        errno = 0;
        ssize_t cc = ::write(sptr_file_->data_file_fd_, prev_seg_payload_, prev_seg_payload_len_);

        if (cc < 0) {
            // error - try one more time
            result = reopen_file_and_seek_kludge(prev_seg_payload_offset_);

            if (result) {
                errno = 0;
                cc = ::write(sptr_file_->data_file_fd_, prev_seg_payload_, prev_seg_payload_len_);

                if (cc < 0) {
                    log_err("%s - error writing to file: %s - %s", __func__, file_path_.c_str(), strerror(errno));
                    disk_io_error_ = true;
                    result = false;
                } else {
                    bytes_written_to_disk_ += cc;
                }
            }
        } else {
            bytes_written_to_disk_ += cc;
        }
    }

    if (is_eob && result) {
        // kludge: close the file; reopen it and verify it is the expected length
        // and rewrite the segment if necessary
        ::close(sptr_file_->data_file_fd_);
        sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_WRONLY);
        off64_t cur_offset = ::lseek64(sptr_file_->data_file_fd_, 0, SEEK_END);

        if (cur_offset == prev_seg_payload_offset_) {
            // the write did not take - try again
            log_err("Got EOB but file length == DS offset - trying to write it again");
            if (::write(sptr_file_->data_file_fd_, prev_seg_payload_, prev_seg_payload_len_) < 0) {
                log_err("%s - error writing to file: %s - %s", __func__, file_path_.c_str(), strerror(errno));
            }
            ::close(sptr_file_->data_file_fd_);
            sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_WRONLY | O_APPEND);
            off64_t cur_offset = ::lseek64(sptr_file_->data_file_fd_, 0, SEEK_END);

            if (cur_offset != (prev_seg_payload_offset_ + prev_seg_payload_len_)) {
                log_err("Got EOB (#2) but file length %zd) does not match expected: %zu -- not deleting file: %s",
                         cur_offset, (prev_seg_payload_offset_ + prev_seg_payload_len_), file_path_.c_str());

                //dzdebug - keeping bad file to look at it
                ::close(sptr_file_->data_file_fd_);
                sptr_file_->data_file_fd_ = -1;


                disk_io_error_ = true;
                result = false;
            }
        } else if (cur_offset != (prev_seg_payload_offset_ + prev_seg_payload_len_)) {
            log_err("Got EOB but file length %zd) does not match expected: %zu - not deleting file: %s",
                     cur_offset, (prev_seg_payload_offset_ + prev_seg_payload_len_),
                     file_path_.c_str());

            //dzdebug - keeping bad file to look at it
            ::close(sptr_file_->data_file_fd_);
            sptr_file_->data_file_fd_ = -1;

            disk_io_error_ = true;
            result = false;
        }
    }


    // clear the prev_seg no matter what the result
    prev_seg_payload_offset_ = 0;
    prev_seg_payload_len_ = 0;
    free(prev_seg_payload_);  // finished with the prev seg payload
    prev_seg_payload_ = nullptr;

    return result;
}

//----------------------------------------------------------------------
bool
LTPSession::write_work_buf_to_file_with_kludges(bool is_eob)
{
    bool result = true;

    oasys::ScopeLock(&sptr_file_->lock_, __func__);

    // seek to the correct position in the file
    off64_t cur_offset = ::lseek64(sptr_file_->data_file_fd_, 0, SEEK_CUR);
    off64_t adjust = work_buf_file_offset_- cur_offset;
    off64_t new_offset = -1;

    if (adjust < 0) {
        result = reopen_file_and_seek_kludge(work_buf_file_offset_);
        adjust = 0;
    }

    new_offset = ::lseek64(sptr_file_->data_file_fd_, adjust, SEEK_CUR);

    if (new_offset < 0) {
        result = reopen_file_and_seek_kludge(work_buf_file_offset_);
        if (result) {
            new_offset = work_buf_file_offset_;
        }
    } else if (new_offset != work_buf_file_offset_) {
        log_err("Error -  seeking to offset (%zu) returned different value (%zd) - try close and reopen...",
                work_buf_file_offset_, new_offset);
        result = reopen_file_and_seek_kludge(work_buf_file_offset_);
        if (result) {
            new_offset = work_buf_file_offset_;
        }
    }

    if (result) {
        // write the work_buf to disk
        errno = 0;
        ssize_t cc = ::write(sptr_file_->data_file_fd_, work_buf_, work_buf_idx_);

        if (cc < 0) {
            // error - try one more time
            result = reopen_file_and_seek_kludge(work_buf_file_offset_);

            if (result) {
                errno = 0;
                cc = ::write(sptr_file_->data_file_fd_, work_buf_, work_buf_idx_);

                if (cc < 0) {
                    log_err("%s - error writing to file: %s - %s", __func__, file_path_.c_str(), strerror(errno));
                    disk_io_error_ = true;
                    result = false;
                } else {
                    bytes_written_to_disk_ += cc;
                }
            }
        } else {
            bytes_written_to_disk_ += cc;
        }
    }

    if (is_eob && result) {
        // kludge: close the file; reopen it and verify it is the expected length
        // and rewrite the segment if necessary
        ::close(sptr_file_->data_file_fd_);
        sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_WRONLY);
        off64_t cur_offset = ::lseek64(sptr_file_->data_file_fd_, 0, SEEK_END);

        if (cur_offset == work_buf_file_offset_) {
            // the write did not take - try again
            log_err("Got EOB but file length == DS offset - trying to write it again");
            if (::write(sptr_file_->data_file_fd_, work_buf_, work_buf_idx_) < 0) {
                log_err("%s - error writing to file: %s - %s", __func__, file_path_.c_str(), strerror(errno));
            }
            ::close(sptr_file_->data_file_fd_);
            sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_WRONLY | O_APPEND);
            off64_t cur_offset = ::lseek64(sptr_file_->data_file_fd_, 0, SEEK_END);

            if (cur_offset != (work_buf_file_offset_ + work_buf_idx_)) {
                log_err("Got EOB (#2) but file length %zd) does not match expected: %zu -- not deleting file: %s",
                         cur_offset, (work_buf_file_offset_ + work_buf_idx_), file_path_.c_str());

                //dzdebug - keeping bad file to look at it
                ::close(sptr_file_->data_file_fd_);
                sptr_file_->data_file_fd_ = -1;


                disk_io_error_ = true;
                result = false;
            }
        } else if (cur_offset != (work_buf_file_offset_ + work_buf_idx_)) {
            log_err("Got EOB but file length %zd) does not match expected: %zu - not deleting file: %s",
                     cur_offset, (work_buf_file_offset_ + work_buf_idx_),
                     file_path_.c_str());

            //dzdebug - keeping bad file to look at it
            ::close(sptr_file_->data_file_fd_);
            sptr_file_->data_file_fd_ = -1;

            disk_io_error_ = true;
            result = false;
        }
    }


    // clear the work_buf no matter what the result
    work_buf_idx_ = 0;
    work_buf_file_offset_ = 0;
    work_buf_available_ = work_buf_size_;

    return result;
}

//----------------------------------------------------------------------
bool
LTPSession::reopen_file_and_seek_kludge(off64_t offset)
{
    bool result = true;

    ::close(sptr_file_->data_file_fd_);
    sptr_file_->data_file_fd_ = ::open(file_path_.c_str(), O_WRONLY);

    if (sptr_file_->data_file_fd_ < 0) {
        log_err("%s - error re-opening file: %s - %s", 
            __func__, file_path_.c_str(), strerror(errno));
        
        disk_io_error_ = true;
        result = false;
    } else {
        off64_t cur_offset = ::lseek64(sptr_file_->data_file_fd_, 0, SEEK_CUR);
        off64_t adjust = offset - cur_offset;
        off64_t new_offset = ::lseek64(sptr_file_->data_file_fd_, adjust, SEEK_CUR);
        if (new_offset != offset) {
            log_err("%s - error seeking to original position (%zu -pos: %zd) in file: %s - %s", 
                    __func__, offset, new_offset,
                    file_path_.c_str(), strerror(errno));

            disk_io_error_ = true;
            result = false;
        }
    }

    return result;
}



//
//   LTPSegment ......................
//
LTPSegment::LTPSegment() :
        oasys::Logger("LTPSegment",
                      "/ltp/seg/%p", this)
{
    BundleDaemon::instance()->inc_ltpseg_created();

    segment_type_             = 0;
    red_green_none_           = 0;
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
LTPSegment::LTPSegment(const LTPSegment& from) :
        oasys::Logger("LTPSegment",
                      "/ltp/seg/%p", this)

{
    BundleDaemon::instance()->inc_ltpseg_created();

    segment_type_          = from.segment_type_;
    engine_id_             = from.engine_id_;
    session_id_            = from.session_id_;
    serial_number_         = from.serial_number_;
    checkpoint_id_         = from.checkpoint_id_;
    control_flags_         = from.control_flags_;
    retrans_retries_       = from.retrans_retries_;
    sptr_timer_            = from.sptr_timer_;
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

    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);
}

//----------------------------------------------------------------------
LTPSegment::LTPSegment(const LTPSession* session) :
        oasys::Logger("LTPSegment",
                      "/ltp/seg/%p", this)
{
    BundleDaemon::instance()->inc_ltpseg_created();

    packet_len_         = 0;
    version_number_     = 0; 
    engine_id_          = session->Engine_ID();
    session_id_         = session->Session_ID();
    serial_number_      = 0;  // this is a new Segment so RPT should be 0. 
    checkpoint_id_      = 0;
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
    security_mask_       = LTP_SECURITY_DEFAULT;
    cipher_key_id_       = session->Outbound_Cipher_Key_Id();
    cipher_engine_       = session->Outbound_Cipher_Engine();
    cipher_suite_        = session->Outbound_Cipher_Suite();
    is_security_enabled_ = ((int) cipher_suite_ != -1);
    cipher_applicable_length_ = 0;
    cipher_trailer_offset_ = 0;

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
      log_err("Error SDNV encoding U64 field: value: %" PRIu64 " est len: %d", infield, estimated_len);
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
      log_err("Error SDNV encoding U32 field: value: %" PRIu32 " est len: %d", infield, estimated_len);
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
    is_checkpoint_ = (new_chkpt > 0);
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

//----------------------------------------------------------------------
const char* LTPSegment::Get_Seg_Str()
{
    return Get_Seg_Str(segment_type_);
}

//----------------------------------------------------------------------
const char* LTPSegment::Get_Seg_Str(int st)
{
    switch(st)
    {
        case LTP_SEGMENT_DS:  return "DS";
        case LTP_SEGMENT_RS:  return "RS";
        case LTP_SEGMENT_RAS: return "RAS";
        case LTP_SEGMENT_CS_BR: return "CS-BR";
        case LTP_SEGMENT_CAS_BR: return "CAS-BR";
        case LTP_SEGMENT_CS_BS: return "CS-BS";
        case LTP_SEGMENT_CAS_BS: return "CAS-BS";
        default: return "UNK";
    }
}

//--------------------------------------------------------------------------
void
LTPSegment::Set_Deleted()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    is_deleted_ = true;
    Cancel_Retransmission_Timer();
}

//--------------------------------------------------------------------------
SPtr_LTPTimer
LTPSegment::Create_Retransmission_Timer(SPtr_LTPNodeRcvrSndrIF rsif, int event_type, uint32_t seconds,
                                        SPtr_LTPSegment sptr_retran_seg)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    // only create a timer if this is the first time in this function
    // - after it has been created once the timer will handle subsequent retransmits
    // -- prevents a race condition n ReportSegs if timer kicks off at same time  
    // -- as processing receipt of a re-sent DS checkpoint
    if (timer_created_) {
        SPtr_LTPTimer dummy;
        return dummy;
    }

    if (sptr_timer_ != nullptr) {
        sptr_timer_->cancel();
        sptr_timer_.reset();
    }

    if (is_deleted_) {
        //log_always("not creating retransmit timer for seg that is already deleted: %s",
        //           session_key_str().c_str());
    } else {
        sptr_timer_ = std::make_shared<LTPRetransmitSegTimer>(rsif, event_type, seconds, sptr_retran_seg);

        sptr_timer_->set_sptr_to_self(sptr_timer_);

        timer_created_ = true;
    }

    return sptr_timer_;
}

//--------------------------------------------------------------------------
SPtr_LTPTimer
LTPSegment::Renew_Retransmission_Timer(SPtr_LTPNodeRcvrSndrIF rsif, int event_type, uint32_t seconds,
                                       SPtr_LTPSegment sptr_retran_seg)
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    // verify that the timer was previouosly created
    ASSERT(timer_created_);

    if (sptr_timer_ != nullptr) {
        sptr_timer_->cancel();
        sptr_timer_.reset();
    }

    if (is_deleted_) {
        //log_always("dzdebug - not creating retransmit timer for seg that is already deleted: %s",
        //           session_key_str().c_str());
    } else {
        sptr_timer_ = std::make_shared<LTPRetransmitSegTimer>(rsif, event_type, seconds, sptr_retran_seg);

        sptr_timer_->set_sptr_to_self(sptr_timer_);

        timer_created_ = true;
    }

    return sptr_timer_;
}

//--------------------------------------------------------------------------
void    
LTPSegment::Cancel_Retransmission_Timer()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    if (sptr_timer_ != nullptr) {
        sptr_timer_->cancel();
        sptr_timer_.reset();
    }
}

//--------------------------------------------------------------------------
bool
LTPSegment::Start_Retransmission_Timer()
{
    bool result = false;
    oasys::ScopeLock scoplok(&lock_, __func__);

    if (sptr_timer_ != nullptr) {
        if (!sptr_timer_->pending()) {
            sptr_timer_->start();
            result = true;
        }
    }

    return result;
}




//----------------------------------------------------------------------
void
LTPSegment::Dump_Current_Buffer(char * location)
{
    (void) location; // prevent warning when debug dissabled

    char hold_byte[4];
    char * output_str = (char *) malloc(packet_len_ * 3);
    if (output_str != nullptr)
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
    BundleDaemon::instance()->inc_ltp_ds_created();

    payload_length_ = 0;
    offset_ = 0;

    if (Parse_Buffer(bp, len) != 0) 
        is_valid_ = false;
}

//--------------------------------------------------------------------------
LTPDataSegment::LTPDataSegment(const LTPSession* session) 
    : LTPDataSegKey(0, 0),
      LTPSegment(session)
{
    BundleDaemon::instance()->inc_ltp_ds_created();

    segment_type_      = LTP_SEGMENT_DS;
    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);

    is_checkpoint_     = false;
    offset_            = 0;
    payload_length_    = 0;
    control_flags_     = 0;
}

//--------------------------------------------------------------------------
LTPDataSegment::~LTPDataSegment()
{
    BundleDaemon::instance()->inc_ltp_ds_deleted();

    if (payload_) {
        free(payload_);
        payload_ = nullptr;
    }
}

//--------------------------------------------------------------------------
u_char*
LTPDataSegment::Release_Payload()
{
    u_char* result = payload_;

    payload_ = nullptr;

    return result;
}

//--------------------------------------------------------------------------
void
LTPDataSegment::Set_Payload(u_char* payload)
{
    ASSERT(payload_ == nullptr);
    payload_ = payload;
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
void LTPDataSegment::Encode_Checkpoint_Bit()
{
    is_checkpoint_ = true;
    u_char control_byte = control_flags_;

    if (packet_.len() == 0) {
        memcpy(packet_.buf(1), &control_byte, 1);
        packet_.incr_len(1);
    }
    
    control_byte |= (u_char) 0x01; // add in checkpoint  only needs one bit if red!!
    memcpy(packet_.buf(), &control_byte, 1); // set checkpoint flags
    control_flags_ = control_byte;
}

//--------------------------------------------------------------------------
void
LTPDataSegment::Set_Deleted()
{
    oasys::ScopeLock scoplok(&lock_, __func__);

    LTPSegment::Set_Deleted();
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
        control_byte &= 0x0b; //  don't need this on if red
    }
    memcpy(packet_.buf(), &control_byte, 1);
    control_flags_ = control_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Start_Byte(size_t start_byte)
{
    start_byte_ = start_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Stop_Byte(size_t stop_byte)
{
    stop_byte_ = stop_byte;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Length(size_t len)
{
    payload_length_ = len;
}

//-----------------------------------------------------------------
void LTPDataSegment::Set_Offset(size_t offset)
{
    offset_ = offset;
}

//-----------------------------------------------------------------
void
LTPDataSegment::Set_File_Usage(SPtr_LTPDataFile sptr_file, const std::string& file_path,
                               size_t start_byte, size_t length, bool eob)
{
    sptr_file_ = sptr_file;
    file_path_ = file_path;

    offset_         = start_byte;
    start_byte_     = start_byte;
    stop_byte_      = start_byte+length-1; 
    payload_length_ = length;

    if (IsRed()) {
       is_redendofblock_  = eob;
    } else if (IsGreen()) {
       is_greenendofblock_ = eob;
    }
}

//-----------------------------------------------------------------
void LTPDataSegment::Encode_LTP_DS_Header()
{
    Set_Segment_Color_Flags();

    if (IsRed() && is_checkpoint_) {
        Encode_Checkpoint_Bit();
    }

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
void LTPDataSegment::Encode_Buffer(u_char * data, size_t length, size_t start_byte, bool checkpoint, bool eob)
{
    payload_length_ = length;

    if (IsRed()) {
       is_redendofblock_  = eob;
       is_checkpoint_     = checkpoint;
    } else if (IsGreen()) {
       is_checkpoint_      = false;
       is_greenendofblock_ = eob;
    }
    offset_         = start_byte;
    start_byte_     = start_byte;
    stop_byte_      = start_byte+length-1; 

    Encode_LTP_DS_Header();

    payload_ = (u_char*) realloc(payload_, length);

    memcpy(payload_, data, length);
}

//-----------------------------------------------------------------
void LTPDataSegment::Encode_All()
{
    // called when a checkpoint was added to the segment and the header needs to be rebuilt
    packet_.clear();
    packet_len_ = 0;
    Encode_LTP_DS_Header();
}

//-----------------------------------------------------------------
std::string* 
LTPDataSegment::asNewString()
{
    // start with the LTP Segment header info
    std::string* result = new std::string((const char*)packet_.buf(), packet_len_);

    if (sptr_file_) {
        // read the payload data in from the file
        payload_ = (u_char*) realloc(payload_, payload_length_);
        if (read_seg_from_file(payload_, payload_length_)) {
            // append the payload data
            result->append((const char*)payload_, payload_length_);
            
        } else {
            log_err("LTP Session %s-%zu: Error reading DS payload from file; "
                    "forcing cancel by sender",
                    session_key_str().c_str(), offset_);
            delete result;
            result = nullptr;
        }

        free(payload_);
        payload_ = nullptr;
    } else {
        // append the payload data from memory
        result->append((const char*)payload_, payload_length_);
    }

    return result;
}


//----------------------------------------------------------------------
bool
LTPDataSegment::read_seg_from_file(u_char* buf, size_t buf_len)
{
    bool result = true;

    oasys::ScopeLock(&sptr_file_->lock_, __func__);

    // seek to the correct position in the file
    errno = 0;
    off64_t cur_offset = ::lseek64(sptr_file_->data_file_fd_, 0, SEEK_CUR);
    off64_t adjust = (off64_t) offset_ - cur_offset;
    off64_t new_offset = ::lseek64(sptr_file_->data_file_fd_, adjust, SEEK_CUR);
    if (new_offset != (off64_t) offset_) {
        log_err("%s - error seeking to position (%zu) in file(%d): %s - %s", 
                __func__, offset_, sptr_file_->data_file_fd_, file_path_.c_str(), strerror(errno));
        result = false;
    } else {

        ssize_t cc = ::read(sptr_file_->data_file_fd_, buf, buf_len);

        if (cc < 0) {
            log_err("%s - error reading file(%d): %s - %s", 
                    __func__, sptr_file_->data_file_fd_, file_path_.c_str(), strerror(errno));
            result = false;
        }
    }

    return result;
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
        stop_byte_  = offset_ + payload_length_ - 1;

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
            payload_ = (u_char*) realloc(payload_, payload_length_);
            memcpy(payload_, bp+current_byte, payload_length_); // only copy the payload

        } else {
            is_valid_ = false;
        }
    } else {
        is_valid_ = false;
    }
    return is_valid_ ? 0 : 1;
}

//-----------------------------------------------------------------
bool
LTPDataSegment::Queued_To_Send()
{
    ASSERT(lock_.is_locked_by_me());

    return queued_to_send_;
}

//-----------------------------------------------------------------
void
LTPDataSegment::Set_Queued_To_Send(bool queued)
{
    ASSERT(lock_.is_locked_by_me());

    queued_to_send_ = queued;
}    






//
//  LTPReportSegment .................................
//
//----------------------------------------------------------------------
LTPReportSegment::LTPReportSegment()
{
    BundleDaemon::instance()->inc_ltp_rs_created();

    retrans_retries_  = 0;
}

//----------------------------------------------------------------------
LTPReportSegment::LTPReportSegment(LTPSession* session, uint64_t checkpoint_id,
                                   size_t lower_bounds, size_t upper_bounds,
                                   size_t claimed_bytes, LTP_RC_MAP& claim_map)
        : LTPSegment(session)
{
    BundleDaemon::instance()->inc_ltp_rs_created();

    segment_type_ = LTP_SEGMENT_RS;
    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);

    checkpoint_id_            = checkpoint_id;

    control_flags_ = 0x08; // this is a ReportSegment

    LTPSegment::Create_LTP_Header();

    memset(packet_.buf(),0x08,1);

    serial_number_ = session->Get_Next_Report_Serial_Number();

    retrans_retries_          = 0;

    lower_bounds_    = lower_bounds;
    upper_bounds_    = upper_bounds;
    claims_          = claim_map.size();
    claimed_bytes_   = claimed_bytes;

    Encode_All(claim_map);
}

//----------------------------------------------------------------------
LTPReportSegment::LTPReportSegment(uint64_t engine_id, uint64_t session_id, uint64_t checkpoint_id,
                                   size_t lower_bounds, size_t upper_bounds,
                                   size_t claimed_bytes, LTP_RC_MAP& claim_map)
{
    BundleDaemon::instance()->inc_ltp_rs_created();

    engine_id_ = engine_id;
    session_id_ = session_id;

    segment_type_ = LTP_SEGMENT_RS;
    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);

    checkpoint_id_            = checkpoint_id;

    control_flags_ = 0x08; // this is a ReportSegment

    LTPSegment::Create_LTP_Header();

    memset(packet_.buf(),0x08,1);

    serial_number_ = LTP_14BIT_RAND;

    retrans_retries_          = 0;

    lower_bounds_    = lower_bounds;
    upper_bounds_    = upper_bounds;
    claims_          = claim_map.size();
    claimed_bytes_   = claimed_bytes;

    Encode_All(claim_map);
}

//----------------------------------------------------------------------
LTPReportSegment::LTPReportSegment(u_char * bp, size_t len)
{
    BundleDaemon::instance()->inc_ltp_rs_created();

    upper_bounds_ = 0;
    lower_bounds_ = 0;
    claims_ = 0;

    if (Parse_Buffer(bp,len) != 0)is_valid_ = false;
}

//----------------------------------------------------------------------
LTPReportSegment::~LTPReportSegment()
{
    BundleDaemon::instance()->inc_ltp_rs_deleted();

    Cancel_Retransmission_Timer();

    claim_map_.clear();
}

//----------------------------------------------------------------------
int LTPReportSegment::Parse_Buffer(u_char * bp, size_t length)
{
    claimed_bytes_ = 0;

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
        logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);


        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &serial_number_);
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

            claimed_bytes_ += reception_length;
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
}

//--------------------------------------------------------------------------
void 
LTPReportSegment::Encode_Counters()
{
    LTPSegment::Encode_Field(serial_number_);
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
        logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);

        decode_status = SDNV::decode(bp+current_byte, length-current_byte, &serial_number_);

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
LTPReportAckSegment::LTPReportAckSegment(const LTPReportSegment * rs, const LTPSession* session) 
         : LTPSegment(*rs)
{
    segment_type_         = LTP_SEGMENT_RAS;
    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);


    // HMAC context create and decode if security enabled
    control_flags_   = 0x09;
    serial_number_   = rs->Serial_ID();
    cipher_suite_  = session->Outbound_Cipher_Suite();
    cipher_key_id_ = session->Outbound_Cipher_Key_Id();
    cipher_engine_ = session->Outbound_Cipher_Engine();
    is_security_enabled_ = ((int) cipher_suite_ != -1) ? true : false;
    LTPSegment::Create_LTP_Header();
    serial_number_ = rs->Report_Serial_Number();

    Encode_Field(serial_number_);
    memset(packet_.buf(),0x09,1);
} 

//--------------------------------------------------------------------------
LTPReportAckSegment::LTPReportAckSegment()
{
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
LTPCancelSegment::LTPCancelSegment(const LTPSession* session, int segment_type, u_char reason_code) 
        : LTPSegment(session)
{
    segment_type_   = segment_type;
    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);

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
        logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);

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
LTPCancelSegment::LTPCancelSegment(LTPSegment* from, int segment_type, u_char reason_code) 
    : LTPSegment(*from)
{
    segment_type_   = segment_type;
    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);

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
    packet_len_++;
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
    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);

    return 0;
}

//-----------------------------------------------------------------
LTPCancelAckSegment::LTPCancelAckSegment()
{
    packet_len_   = 0;
}

//-----------------------------------------------------------------
LTPCancelAckSegment::LTPCancelAckSegment(LTPCancelSegment * from, u_char control_byte, const LTPSession* session)
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

    logpathf("/ltp/seg/%s/%lu-%lu", Get_Seg_Str(), engine_id_, session_id_);
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


