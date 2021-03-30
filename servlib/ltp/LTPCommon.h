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

#ifndef LTPCOMMON_H
#define LTPCOMMON_H

#ifdef HAVE_CONFIG_H
#    include <dtn-config.h>
#endif


#include <map>
#include <stdio.h>
#include <stdlib.h>     /* malloc, free, rand */
#include <string>
#include <time.h>
#include <sys/time.h>

#include <third_party/oasys/thread/Timer.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/util/Time.h>
#include <third_party/oasys/util/ScratchBuffer.h>


#include "bundling/SDNV.h" 
#include "bundling/BundleList.h"
#include "contacts/Contact.h"
#include "bundling/Bundle.h"

#define PARENT_SENDER   1
#define PARENT_RECEIVER 2

#define LTP_SECURITY_DEFAULT 0
#define LTP_SECURITY_HEADER  1
#define LTP_SECURITY_TRAILER 2

#define UNKNOWN_SEGMENT   0
#define RED_SEGMENT       1
#define GREEN_SEGMENT     2

#define ADJUSTED_SEGMENT  1
#define DELETED_SEGMENT   2
#define DUPLICATE_SEGMENT 3
#define LOOKUP_COMPLETE   4
#define LTP_14BIT_RAND (rand() % 0x3FFF)  // 1 - 2**32 so start it at 14 bits since it must start 2**14
#define LTPSEGMENT_CS_BS_BYTE  ((u_char) 0x0C)
#define LTPSEGMENT_CAS_BS_BYTE ((u_char) 0x0D)
#define LTPSEGMENT_CS_BR_BYTE  ((u_char) 0x0E)
#define LTPSEGMENT_CAS_BR_BYTE ((u_char) 0x0F)

namespace dtn
{
class LTPDataSegment;
class LTPDataSegKey;
class LTPConvergenceLayer;
class LTPSegment;
class LTPCancelSegment;
class LTPReportSegment;
class LTPSession;
class LTPSessionKey;
struct LTPDataSegKeyCompare;

typedef std::shared_ptr<LTPSegment> SPtr_LTPSegment;

typedef std::shared_ptr<LTPDataSegment> SPtr_LTPDataSegment;
typedef std::shared_ptr<LTPCancelSegment> SPtr_LTPCancelSegment;
typedef std::shared_ptr<LTPReportSegment> SPtr_LTPReportSegment;

typedef std::map<LTPDataSegKey*, SPtr_LTPDataSegment, LTPDataSegKeyCompare> LTP_DS_MAP;
typedef std::map<uint64_t, SPtr_LTPReportSegment> LTP_RS_MAP;


class LTPSessionKey
{
public:
    LTPSessionKey(uint64_t engine_id, uint64_t session_id)
        : engine_id_(engine_id),
          session_id_(session_id)
    {}

    virtual ~LTPSessionKey() {}

    uint64_t Engine_ID()  { return engine_id_;  }
    uint64_t Session_ID() { return session_id_; }

protected:
    uint64_t engine_id_;
    uint64_t session_id_;
};

typedef std::unique_ptr<LTPSessionKey> QPtr_LTPSessionKey;


class LTPDataSegKey
{
public:
    LTPDataSegKey(size_t start_byte, size_t stop_byte)
        : start_byte_(start_byte),
          stop_byte_(stop_byte)
    {}

    virtual ~LTPDataSegKey() {}

    size_t Start_Byte() { return start_byte_; }
    size_t Stop_Byte()  { return stop_byte_;  }

protected:
    size_t start_byte_;
    size_t stop_byte_;
};

// compare function for use in containers
struct LTPDataSegKeyCompare {
    bool operator()(LTPDataSegKey* key1, LTPDataSegKey* key2) {
        return ((key1->Start_Byte() < key2->Start_Byte()) || (key1->Stop_Byte() < key2->Stop_Byte()));
    }
};




// Interface to the LTPNode::Receiver or LTPNode::Sender used by timers
class LTPNodeRcvrSndrIF;
typedef std::shared_ptr<LTPNodeRcvrSndrIF> SPtr_LTPNodeRcvrSndrIF;

class LTPNodeRcvrSndrIF
{
public:
    virtual ~LTPNodeRcvrSndrIF(){}

    virtual void     RSIF_post_timer_to_process(oasys::SPtr_Timer& event) = 0;
    virtual uint64_t RSIF_aos_counter() = 0;
    virtual void     RSIF_retransmit_timer_triggered(int event_type, SPtr_LTPSegment& retran_seg) = 0;
    virtual void     RSIF_inactivity_timer_triggered(uint64_t engine_id, uint64_t session_id) = 0;
    virtual void     RSIF_closeout_timer_triggered(uint64_t engine_id, uint64_t session_id) = 0;
};


// Interface to the LTPNode  --- still needed???
//class LTPNodeIF
//{
//public:
//
//    virtual           ~LTPNodeIF(){}
//
//    virtual uint64_t  AOS_Counter() = 0;
//    virtual void      Process_Sender_Segment(LTPSegment * segment, bool closed, bool cancelled) = 0;
//
//    virtual void      Post_Timer_To_Process(oasys::SPtr_Timer& event) = 0;
////};
//
// end of LTPNodeIF class.......
//



// NOTE: These values do not correspond to the Segment Type Flag values!!
enum : int {
    LTP_SEGMENT_UNKNOWN  = 0,  // 0
    LTP_SEGMENT_DS,            // 1
    LTP_SEGMENT_RS,            // 2
    LTP_SEGMENT_RAS,           // 3
    LTP_SEGMENT_CS_BR,         // 4
    LTP_SEGMENT_CAS_BR,        // 5
    LTP_SEGMENT_CS_BS,         // 6
    LTP_SEGMENT_CAS_BS,        // 7



    LTP_EVENT_SESSION_COMPLETED  = 100,            // 100
    LTP_EVENT_ABORTED_DUE_TO_EXPIRED_BUNDLES,      // 101
    LTP_EVENT_DS_TO,                               // 102
    LTP_EVENT_RS_TO,                               // 103
    LTP_EVENT_CS_TO,                               // 104
    LTP_EVENT_RAS_RECEIVED,                        // 105
    LTP_EVENT_INACTIVITY_TIMEOUT,                  // 106
    LTP_EVENT_CLOSEOUT_TIMEOUT,                    // 107
};


// Base class for timers that offload processing from the system-wide
// TimerThread to a dedicated thread. The TimerThread timeout trigger
// just posts to the dedicated thread which then uses the do_timeout_processing
// method to do the real work
//
// NOTE:
// after creating the timer into a SPtr_LTPTimer, you must pass the
// SPtr_LTPTimer back into the newly created timer using the 
// set_sptr mehtod so that it has reference to itself for interacting
// with the oasys::TiemrSystem.

class LTPTimer;
typedef std::shared_ptr<LTPTimer> SPtr_LTPTimer;

class LTPTimer : public oasys::SharedTimer,
                 public oasys::Logger 
{
public:
    LTPTimer(SPtr_LTPNodeRcvrSndrIF& rsif, int event_type, uint32_t seconds);

    virtual ~LTPTimer();

    void set_sptr(SPtr_LTPTimer& sptr) { sptr_ = sptr; }

    void set_seconds(int seconds);

    virtual void start();

    virtual bool cancel();

    // this method overrides the oasys::Timer method to post itself 
    // to the LTP TimerProcessor thread
    virtual void timeout(const struct timeval &now) override;

    // override this method for the actual processing to be done when the timer triggers
    virtual void do_timeout_processing() = 0;

protected:

    SPtr_LTPNodeRcvrSndrIF rsif_;

    int         event_type_;
    uint32_t    seconds_ = 0;
    uint64_t    target_counter_ = 0;

    oasys::SPtr_Timer sptr_;

    oasys::SpinLock lock_;
};
//
// end of LTPTimer (base) class.......
//


class LTPRetransmitSegTimer : public LTPTimer
{
public:
    LTPRetransmitSegTimer(SPtr_LTPNodeRcvrSndrIF& rsif, int event_type, uint32_t seconds,
                          SPtr_LTPSegment& retransmit_seg);

    virtual ~LTPRetransmitSegTimer();

    virtual bool cancel() override;

    void do_timeout_processing() override;

protected:

    SPtr_LTPSegment retransmit_seg_;
};
//
// end of LTPRetransmitSegTimer class.......
//

class LTPCloseoutTimer;
typedef std::shared_ptr<LTPCloseoutTimer> SPtr_LTPCloseoutTimer;

class LTPCloseoutTimer : public LTPTimer
{
public:
    LTPCloseoutTimer(SPtr_LTPNodeRcvrSndrIF& rsif, uint64_t engine_id, uint64_t session_id);

    virtual ~LTPCloseoutTimer();

    void do_timeout_processing() override;

protected:

    uint64_t engine_id_;
    uint64_t session_id_;
};
//
// end of LTPCloseoutTimer class.......
//
    

class LTPSegment : public oasys::Logger
{

public:

    LTPSegment();
    LTPSegment(LTPSession * session);
    LTPSegment(LTPSegment& from);

    virtual ~LTPSegment();

    std::string     session_key_str();
    int       Segment_Type()       { return segment_type_;      }
    int       Red_Green()          { return red_green_none_;    }
    size_t    Packet_Len()         { return packet_len_;        }
    uint32_t  Retrans_Retries()    { return retrans_retries_;   }

    std::string* asNewString()     { return new std::string((const char*)packet_.buf(), packet_len_); }

    bool       IsAuthenticated      (uint32_t suite, uint32_t key_id, std::string& engine);

    bool       IsValid()            { return is_valid_;          }
    bool       IsCheckpoint()       { return is_checkpoint_;     }
    bool       IsEndofblock()       { return (is_redendofblock_ | is_greenendofblock_); }
    bool       IsRedEndofblock()    { return is_redendofblock_;  }
    bool       IsGreenEndofblock()  { return is_greenendofblock_;}
    bool       IsGreen()            { return (red_green_none_ == GREEN_SEGMENT ? true : false); }
    bool       IsRed()              { return (red_green_none_ == RED_SEGMENT   ? true : false); }
    bool       IsSecurityEnabled()  { return is_security_enabled_;   }
    bool       IsDeleted()          { return is_deleted_;            }
    uint32_t   Security_Mask()      { return security_mask_;         }
    uint32_t   Cipher_Suite()       { return cipher_suite_;          }
    uint32_t   Cipher_Key_Id()      { return cipher_key_id_;         }
    std::string& Cipher_Engine()      { return cipher_engine_;       }
 
    uint64_t Session_ID()         { return session_id_;        }
    uint64_t Engine_ID()          { return engine_id_;         }
    uint64_t Checkpoint_ID()      { return checkpoint_id_;     }
    uint64_t Serial_ID()          { return serial_number_;     }
    uint32_t Service_ID()         { return client_service_id_; }
 
    const char*        Get_Segment_Type();
    static const char* Get_Segment_Type(int st);
    static const char* Get_Event_Type(int et) { return Get_Segment_Type(et); }

    void      Build_Header_ExtTag();
    void      Create_LTP_Header();
    void      Dump_Current_Buffer(char * location);
    void      Encode_Field(uint32_t infield);
    void      Encode_Field(uint64_t infield);
    void      Increment_Retries();
    void      Set_Checkpoint_ID(uint64_t new_chkpt);
    void      Set_Cipher_Suite(int new_val);
    void      Set_Cipher_Key_Id(int new_key);
    void      Set_Cipher_Engine(std::string& new_engine);
    void      Set_Client_Service_ID(uint32_t in_client_service_id);       
    void      SetCheckpoint();
    void      Set_Report_Serial_Number(uint64_t new_rsn);
    void      xxSet_Retrans_Retries(int new_counter);
    void      Set_RGN(int new_color);
    void      Set_Security_Mask(uint32_t add_val);
    void      UnsetCheckpoint();
    void      Set_Deleted()      { is_deleted_ = true; }

    oasys::ScratchBuffer<u_char*, 0> * RawPacket() { return (oasys::ScratchBuffer<u_char*, 0> *) &raw_packet_; }
    oasys::ScratchBuffer<u_char*, 0> * Packet() { return (oasys::ScratchBuffer<u_char*, 0> *) &packet_; }

    int     suite_result_len(int suite);
    int     Parse_Buffer(u_char * bp, size_t length, size_t * offset);

    static  LTPSegment* DecodeBuffer(u_char * bp, size_t length);
    static  uint64_t    Parse_Engine(u_char * bp, size_t length);
    static  uint64_t    Parse_Session(u_char * bp, size_t length);
    static  bool        Parse_Engine_and_Session(u_char * bp, size_t length, 
                                                 uint64_t& engine_id, uint64_t& session_id, int& seg_type);
    static  int         SegTypeFlags_to_SegType(uint8_t seg_type_flags);


public:

//    virtual SPtr_LTPRetransmitSegTimer Create_Retransmission_Timer(SPtr_LTPNodeRcvrSndrIF& rsif, int event_type, 
    virtual SPtr_LTPTimer Create_Retransmission_Timer(SPtr_LTPNodeRcvrSndrIF& rsif, int event_type, 
                                                      uint32_t seconds, SPtr_LTPSegment& retransmit_seg);
    void Cancel_Retransmission_Timer();

protected:
    LTPSegment& operator= ( LTPSegment& ) = delete;

protected:

    uint32_t  retrans_retries_;
      
    // session_id, engine_id... etc....

    SPtr_LTPTimer timer_;


    int       version_number_;
    int       control_flags_;
    int       header_exts_;
    int       trailer_exts_;
 
    oasys::ScratchBuffer<u_char*, 0> packet_;
    oasys::ScratchBuffer<u_char*, 0> raw_packet_;

    size_t    packet_len_;

    uint32_t security_mask_;
    uint64_t engine_id_;
    uint64_t session_id_;
    uint64_t serial_number_;
    uint64_t checkpoint_id_;

    uint32_t client_service_id_;

    // security detail
    size_t     cipher_applicable_length_;
    size_t     cipher_trailer_offset_;
    uint32_t  cipher_suite_;
    uint32_t  cipher_key_id_;
    std::string     cipher_engine_;
  
    bool      is_security_enabled_;
    bool      is_checkpoint_;
    bool      is_redendofblock_;
    bool      is_greenendofblock_;
    bool      is_valid_;
    bool      is_deleted_ = false;   // flag to let timer know the segment was deleted due to ACK or Cancel

    int       segment_type_;    // DS, RS, CS...
    int       red_green_none_;

    oasys::SpinLock timer_lock_;
};
//
// end of LTPSegment class...
//


class LTPDataSegment : public LTPDataSegKey,
                       public LTPSegment 
{

public:

    LTPDataSegment();
    LTPDataSegment(LTPDataSegment& from);
    LTPDataSegment(LTPSession * session);

    virtual ~LTPDataSegment();

    LTPDataSegment(u_char * bp, size_t length);

    size_t Offset()     { return offset_;     }       

    size_t Payload_Length () { return payload_length_; }

    oasys::ScratchBuffer<u_char *, 0> * Payload() { return (oasys::ScratchBuffer<u_char *, 0> *) &payload_; }

    void Encode_All();
    void Encode_Buffer();
    void Encode_Buffer(u_char * data, size_t length, size_t start_byte, bool checkpoint);

    int  Parse_Buffer(u_char * bp, size_t len);
    void Set_Endofblock();
    void Unset_Endofblock();
    void Set_Checkpoint();
    void Set_Segment_Color_Flags();
    void Unset_Checkpoint();
    void Set_Start_Byte(int start_byte);
    void Set_Stop_Byte(int stop_byte);
    void Set_Length(int length);
    void Set_Offset(size_t offset);

protected:

    oasys::ScratchBuffer<u_char *, 0>  payload_;

    size_t payload_length_;
    size_t offset_;

};

//
// end of LTPDataSegment class...
//



class LTPReportClaim {

public:

    LTPReportClaim();
    LTPReportClaim(LTPReportClaim& from);
    LTPReportClaim(size_t offset, size_t len);
 
    virtual ~LTPReportClaim();

    std::string&     Key()     { return key_;     }
    size_t        Length()  { return length_;  }
    size_t        Offset()  { return offset_;  }

protected:

    std::string    key_;
    size_t       length_;
    size_t       offset_;
}; 

typedef std::shared_ptr<LTPReportClaim> SPtr_LTPReportClaim;

typedef std::map<size_t, SPtr_LTPReportClaim>   LTP_RC_MAP;
//
// end of LTPReportClaims class....
//



class LTPReportSegment : public LTPSegment 
{

public:

    LTPReportSegment();
    LTPReportSegment(u_char * bp, size_t len);

    LTPReportSegment(LTPSession * session, uint64_t checkpoint_id,
                        size_t lower_bounds, size_t upper_bounds, LTP_RC_MAP& claim_map);

    virtual ~LTPReportSegment();

    //
    // LTPReportSegment(LTPSession * session, int red_green);
    //

    int         Parse_Buffer(u_char * bp, size_t len);

    uint64_t    Report_Serial_Number()  { return report_serial_number_;      }
    size_t      UpperBounds()           { return upper_bounds_;              }
    size_t      LowerBounds()           { return lower_bounds_;              }
    size_t      ClaimCount()            { return claims_;                    }
    LTP_RC_MAP& Claims()                { return claim_map_;                 }
 
    void        Encode_All(LTP_RC_MAP& claim_map);
    void        Encode_Counters();

protected:
    LTPReportSegment(LTPReportSegment& from) = delete;
    LTPReportSegment(LTPReportSegment&& from) = delete;

    uint64_t  report_serial_number_;
    size_t    upper_bounds_;
    size_t    lower_bounds_;
    size_t    claims_;

    // used for received RS
    LTP_RC_MAP    claim_map_;
};

//
// end of LTPReportSegment class.......
//



class LTPReportAckSegment : public LTPSegment 
{
public:

    LTPReportAckSegment();
    LTPReportAckSegment(LTPReportSegment * from, LTPSession * session);
    LTPReportAckSegment(u_char * bp, size_t len);

    virtual ~LTPReportAckSegment();

    LTPReportAckSegment(LTPSession * session);

    int        Parse_Buffer(u_char * bp, size_t len);
    uint64_t   Report_Serial_Number()          { return report_serial_number_; }
    int        Retries()                       { return retransmit_ctr_;       }
    void       Set_Retries(int new_value)      { retransmit_ctr_ = new_value; }
    void       Reset_Retrans_Timer();

protected:
    LTPReportAckSegment(LTPReportAckSegment& from) = delete;

    std::string key_; // hold the key for the timer... matches Report Key..

    uint64_t report_serial_number_;
    int       retransmit_ctr_;
    std::string    str_serial_number_;
};
//
// end of LTPReportAckSegment class.......
//



class LTPCancelSegment : public LTPSegment 
{
public:

    enum {
        LTP_CS_REASON_USR_CANCLED,     // 0  Clien Service cancelled session
        LTP_CS_REASON_UNREACHABLE,     // 1  Unreachable client service
        LTP_CS_REASON_RLEXC,           // 2  Retransmission limit exceeded.
        LTP_CS_REASON_MISCOLORED,      // 3  Miscolored segment
        LTP_CS_REASON_SYS_CNCLD,       // 4  A system error condition caused Unexpected session termination
        LTP_CS_REASON_RXMTCYCEX,       // 5  Exceeded the Retransmission Cycles Limit
    };

    LTPCancelSegment();
    LTPCancelSegment(LTPCancelSegment& from);
    LTPCancelSegment(u_char * bp, size_t len, int segment_type);
    LTPCancelSegment(LTPSession * session, int segment_type, u_char reason_code);
 
    virtual ~LTPCancelSegment();

    int  Parse_Buffer(u_char * bp, size_t len, int segment_type);

    const char *     Get_Reason_Code(int reason_code);

protected:

    u_char    reason_code_;
};
//
// end of LTPCancelSegment class.......
//


class LTPCancelAckSegment : public LTPSegment 
{
public:

    LTPCancelAckSegment();
    LTPCancelAckSegment(u_char * bp, size_t len, int segment_type);
    LTPCancelAckSegment(LTPCancelSegment * from, u_char control_byte, LTPSession * session);
    LTPCancelAckSegment(LTPCancelAckSegment& from);

    virtual ~LTPCancelAckSegment();

    int  Parse_Buffer(u_char * bp, size_t len, int segment_type);
};
//
// end of LTPCancelAckSegment class.......
//


class LTPSession : public LTPSessionKey,
                   public oasys::Logger
{
public:
    LTPSession(uint64_t engine_id, uint64_t session_id, bool is_counted_session=true);

    virtual ~LTPSession();

    /**
     *     Enum for messages from the daemon thread to the sender
     *     thread.
     */
    enum {
        LTP_SESSION_STATE_UNKNOWN,      // 0 
        LTP_SESSION_STATE_RS,           // 1
        LTP_SESSION_STATE_CS,           // 2
        LTP_SESSION_STATE_DS,           // 3
        LTP_SESSION_STATE_CREATED,      // 4
        LTP_SESSION_STATE_UNDEFINED     // 5
    };
 
    void       AddToBundleList (BundleRef bundle,size_t bundle_size);
    int        AddSegment     (LTPDataSegment * sop, bool check_for_overlap=true);
    int        AddSegment     (SPtr_LTPDataSegment& ds_seg, bool check_for_overlap=true);

    void       SetAggTime     ();

    int        RemoveClaimedSegments  (LTP_DS_MAP * segments, size_t reception_offset, size_t reception_length);
    bool       CheckAndTrim   (LTPDataSegment * sop);
    bool       CheckAndTrim   (SPtr_LTPDataSegment& ds_seg);

    bool       HaveReportSegmentsToSend(LTPDataSegment * dataseg, int32_t segsize, int64_t& bytes_claimed);
    int64_t    GenerateReportSegments(uint64_t chkpt, size_t lower_bounds_start, size_t chkpt_upper_bounds, int32_t segsize);
    void       clear_claim_map(LTP_RC_MAP* claim_map);

    int        Check_Overlap  (LTP_DS_MAP * segments, LTPDataSegment * sop);
    int        Check_Overlap  (LTP_DS_MAP * segments, SPtr_LTPDataSegment& ds_seg);
    size_t     IsGreenFull    ();
    size_t     IsRedFull      ();
    uint64_t   Increment_Checkpoint_ID();
    uint64_t   Increment_Report_Serial_Number();

    u_char *  GetAllRedData();
    u_char *  GetAllGreenData();

    std::string         key_str();
    BundleList *        Bundle_List()            { return bundle_list_;              }
    size_t              Expected_Red_Bytes()     { return expected_red_bytes_;       }
    size_t              Expected_Green_Bytes()   { return expected_green_bytes_;     }
    size_t              Red_Bytes_Received()     { return red_bytes_received_;       }
    size_t              Session_Size()           { return session_size_;             }
    uint64_t            Checkpoint_ID()          { return checkpoint_id_;            }
    uint64_t            Report_Serial_Number()   { return report_serial_number_;     }
    int                 reports_sent()           { return reports_sent_;             }
    int                 Session_State()          { return session_state_;            }
    SPtr_LTPCancelSegment&   Cancel_Segment()    { return cancel_segment_;           }
    uint32_t            Inbound_Cipher_Suite()   { return inbound_cipher_suite_;     }
    uint32_t            Inbound_Cipher_Key_Id()  { return inbound_cipher_key_id_;    }
    std::string&        Inbound_Cipher_Engine()  { return inbound_cipher_engine_;    }
    uint32_t            Outbound_Cipher_Suite()  { return outbound_cipher_suite_;    }
    uint32_t            Outbound_Cipher_Key_Id() { return outbound_cipher_key_id_;   }
    std::string&        Outbound_Cipher_Engine() { return outbound_cipher_engine_;   }

    bool                IsSecurityEnabled()      { return is_security_enabled_;      }
    bool                GreenProcessed()         { return green_processed_;          }
    bool                RedProcessed()           { return red_processed_;            }
    bool                DataProcessed()          { return red_processed_ | green_processed_; }
    bool                Has_Inactivity_Timer()   { return inactivity_timer_ != nullptr; }
    bool                Is_LTP_Cancelled()       { return session_state_ == LTP_SESSION_STATE_CS; }

    LTP_DS_MAP&  Red_Segments()    { return red_segments_;    }
    LTP_DS_MAP&  Green_Segments()  { return green_segments_;  }
    LTP_RS_MAP&  Report_Segments() { return report_segments_; }

    const char *    Get_Session_State(); 

    bool      TimeToSendIt(uint32_t milliseconds);

    void      RemoveSegments();
    void      Set_Checkpoint_ID(uint64_t in_checkpoint);       
    void      Set_Report_Serial_Number(uint64_t in_report_serial);       
    void      Set_Client_Service_ID(uint32_t in_client_service_id);       
    void      Set_Session_State(int new_session_state);
    void      Set_EOB(LTPDataSegment *sop);
    void      Set_Cancel_Segment(SPtr_LTPCancelSegment& cancel_segment);
    void      Clear_Cancel_Segment();
    void      Set_Inbound_Cipher_Suite(int new_val);
    void      Set_Inbound_Cipher_Key_Id(int new_key);
    void      Set_Inbound_Cipher_Engine(std::string& new_engine);
    void      Set_Outbound_Cipher_Suite(int new_val);
    void      Set_Outbound_Cipher_Key_Id(int new_key);
    void      Set_Outbound_Cipher_Engine(std::string& new_engine);
    void      Start_Inactivity_Timer(SPtr_LTPNodeRcvrSndrIF& rsif, time_t time_left);
    void      Cancel_Inactivity_Timer();
    void      Start_Closeout_Timer(SPtr_LTPNodeRcvrSndrIF& rsif, time_t time_left);

    void      inc_reports_sent() { ++reports_sent_; }

    time_t    Last_Packet_Time() { return last_packet_time_; }
    void      Set_Last_Packet_Time(time_t pt) { last_packet_time_ = pt; }

    oasys::SpinLock segmap_lock_;

protected:
    // disallow copy constructor
    LTPSession(LTPSession& other) = delete;

    LTPSession& operator= ( LTPSession& )  = delete;


    virtual void init();

protected:
    /**
     * Helper class for timing session inactivity
     */
    class InactivityTimer : public LTPTimer
    {
    public:
        InactivityTimer(SPtr_LTPNodeRcvrSndrIF& rsif, uint64_t engine_id, uint64_t session_id);

        virtual ~InactivityTimer();

        void do_timeout_processing();

    protected:

        uint64_t engine_id_;
        uint64_t session_id_;
    };
    //
    // end of InactivityTimer class.......
    //
   
    /***
     * manage InactivityTimer
     */
    SPtr_LTPTimer     inactivity_timer_;  

    SPtr_LTPCancelSegment cancel_segment_;

    time_t    last_packet_time_;

    uint64_t checkpoint_id_;
    uint64_t report_serial_number_;

    oasys::Time agg_start_time_;

    bool red_eob_received_;   //  once we receive the Red EOB then we truly know how many bytes to expect
    bool red_complete_;       //  if red completes first ship all that we have
    bool green_complete_;     //  
    bool red_processed_;
    bool green_processed_;

    size_t red_highest_offset_;
    size_t red_bytes_received_;

    size_t  expected_red_bytes_;
    size_t  expected_green_bytes_;
    size_t  session_size_;
    int  reports_sent_;

    LTP_DS_MAP  red_segments_;
    LTP_DS_MAP  green_segments_;
    LTP_RS_MAP  report_segments_;

    BundleList * bundle_list_;

    int  session_state_;
 
    // security enabled detail
    bool        is_security_enabled_;

    uint32_t    inbound_cipher_suite_;
    uint32_t    inbound_cipher_key_id_;
    std::string inbound_cipher_engine_;

    uint32_t    outbound_cipher_suite_;
    uint32_t    outbound_cipher_key_id_;
    std::string outbound_cipher_engine_;

    bool is_counted_session_ = false;

};
//
// end of LTPSession class...
//

}


#endif // COMMONOBJECT_H
