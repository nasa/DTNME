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

#ifndef LTPUDPCOMMON_H
#define LTPUDPCOMMON_H

#ifdef HAVE_CONFIG_H
#    include <dtn-config.h>
#endif

#ifdef LTPUDP_ENABLED


#include <map>
#include <stdio.h>
#include <stdlib.h>     /* malloc, free, rand */
#include <string>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <oasys/thread/Timer.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/Time.h>
#include <oasys/util/ScratchBuffer.h>


#include "bundling/SDNV.h" 
#include "bundling/BundleList.h"
#include "contacts/Contact.h"
#include "bundling/Bundle.h"

#ifdef LTPUDP_AUTH_ENABLED
#    include "security/KeyDB.h"
#    include <openssl/hmac.h>
#    include <openssl/bio.h>
#    include <openssl/pem.h>
#    include <openssl/evp.h>
#    include <openssl/err.h>
#    include <openssl/rsa.h>
#    include <openssl/sha.h>
#    include <openssl/rand.h>
#endif

using namespace std;

#define PARENT_SENDER   1
#define PARENT_RECEIVER 2

#define LTPUDP_SECURITY_DEFAULT 0
#define LTPUDP_SECURITY_HEADER  1
#define LTPUDP_SECURITY_TRAILER 2

#define UNKNOWN_SEGMENT   0
#define RED_SEGMENT       1
#define GREEN_SEGMENT     2

#define ADJUSTED_SEGMENT  1
#define DELETED_SEGMENT   2
#define DUPLICATE_SEGMENT 3
#define LOOKUP_COMPLETE   4
#define LTPUDP_14BIT_RAND (rand() % 0x3FFF)  // 1 - 2**32 so start it at 14 bits since it must start 2**14
#define LTPUDPSEGMENT_CS_BS_BYTE  ((u_char) 0x0C)
#define LTPUDPSEGMENT_CAS_BS_BYTE ((u_char) 0x0D)
#define LTPUDPSEGMENT_CS_BR_BYTE  ((u_char) 0x0E)
#define LTPUDPSEGMENT_CAS_BR_BYTE ((u_char) 0x0F)

namespace dtn
{

class LTPUDPSession;
class LTPUDPSegment;
class LTPUDPReportSegment;
class LTPUDPConvergenceLayer;
struct MySendObject;


typedef std::map<std::string, LTPUDPReportSegment*>  RS_MAP;
typedef std::pair<std::string, LTPUDPReportSegment*> RS_PAIR;

typedef struct MySendObject MySendObject;
  

class LTPUDPCallbackIF
{
public:

    virtual           ~LTPUDPCallbackIF(){}

    virtual u_int32_t Inactivity_Interval() { return 0; }
    virtual u_int64_t AOS_Counter() { return 0; }
    virtual int       Retran_Interval() { return 0; }
    virtual int       Retran_Retries() { return 0; }
    virtual u_int64_t Base_Checkpoint() { return 0; }
    virtual void      ProcessSessionEvent(MySendObject * event) { (void) event; return; }
    virtual void      Process_Sender_Segment(LTPUDPSegment * segment) {(void) segment; return; }
    virtual void      Receiver_Generate_Checkpoint(string session_key, LTPUDPSegment * seg) { 
                         (void) seg;
                          printf("Receiver_Generate_Checkpoint not implemented(%s)", 
                                 session_key.c_str()); 
                          return; 
                       }
    virtual void      Receiver_Resend_RS(LTPUDPReportSegment * seg) { 
                          (void) seg;
                          printf("Receiver_Resend_RS not implemented"); 
                          return; 
                       }
    virtual void      Subtract_Timers(string session_key,int parent_type) { 
                          printf("Not implemented Subtract_Timers(%s) parent(%d)", session_key.c_str(), parent_type); return; 
                       }
    virtual void      Resend_Sender_Checkpoint(string session_key) { 
                          printf("Not implemented Resnd_Sender_Checkpoint(%s)", session_key.c_str()); return; 
                       }
    virtual void      Send_Sender_Remaining_Segments(string session_key) { 
                          printf("Not implemented Send_Sender_Remaining_Segments(%s)", session_key.c_str()); return; 
                       }

    virtual const char   *  Get_Session_State() { printf("Get_Session_State is not implemented"); return "Not Implemented"; }
    virtual const char   *  Get_Segment_Type() { printf("Get_Segment_Type is not implemented"); return NULL; }
    virtual void      Set_BaseCheckpoint(u_int64_t new_value) { printf("Set_BaseCheckpoint not implemented %" PRIu64, new_value); }
    virtual void      Cleanup_Session_Sender(string session_key, string segment_key, int segment_type) {
                           printf("Cleanup_Session_Sender not implemented Session:%s Segment:%s segment_type:%d",
                                  session_key.c_str(), segment_key.c_str(), segment_type);
                       }
    virtual void      Cleanup_Session_Receiver(string session_key, string segment_key, int segment_type) {
                           (void) segment_type;
                           printf("Cleanup_Session_Receiver not implemented %s - %s",
                                  session_key.c_str(), segment_key.c_str());
                       }
    virtual void      Cleanup_RS_Receiver(string session, u_int64_t report_key, int segment_type) { 
                           (void) segment_type;
                           printf("Cleanup_RS_Receiver session %s not implemented %20.20" PRIu64,
                                  session.c_str(),report_key);  
                       }
    virtual void      Send(LTPUDPSegment * segment,int parent_type) { 
                           printf("Send(Segment) is not implemented %p (%d)", segment, parent_type); return; 
                       }
    virtual void      Send_RS_Sender(LTPUDPSegment * segment,int parent_type) { 
                           printf("Send_RS_Sender is not implemented %p (%d)", segment,parent_type);  
                       }
    virtual void      Build_CS_Receiver_Segment(LTPUDPSession * session, int segment_type) { 
                           printf("Send_CS_Sender is not implemented %p segment_type %d",
                                  session,segment_type); 
                       }
    virtual void      bundle_queued(const LinkRef& link, const BundleRef& bundle) { 
                           printf("bundle_queued link %s and bundle %" PRIbid, link->type_str(),  bundle.object()->bundleid());
                       }
    virtual bool      pop_queued_bundle(const LinkRef& link) { 
                           printf("pop_queued_bundle link %s",link->type_str());
                           return false;
                       }

    virtual u_int32_t  Outbound_Cipher_Suite()  {  printf("Outbound_Cipher_Suite() not implemented...\n");  return -1;}
    virtual u_int32_t  Outbound_Cipher_Key_Id() {  printf("Outbound_Cipher_Key_Id() not implemented...\n"); return -1; }
    virtual string     Outbound_Cipher_Engine() {  printf("Outbound_Cipher_Engine() not implemented...\n"); return ""; }

    virtual LinkRef* link_ref() = 0;

    // XXX/dz pure virtual - force descendants to declare it 
    //        -- probably should make all of these pure virtual 
    //           so you know it is either defined or not needed in which case you can delete it
    virtual void      Post_Timer_To_Process(oasys::Timer* event) = 0;
};
//
// end of LTPUDPCallbackIF class.......
//



class LTPSessionKey
{
public:
    LTPSessionKey(uint64_t engine_id, uint64_t session_id)
        : engine_id_(engine_id),
          session_id_(session_id)
    {}

    bool operator< (const LTPSessionKey& other) const
    {
        return ( (engine_id_ < other.engine_id_) ||
                 ( (engine_id_ == other.engine_id_) &&
                   (session_id_ < other.session_id_) ) );
    }

   uint64_t engine_id_;
   uint64_t session_id_;
};



class LTPUDPSegment : public oasys::Logger
{

public:

    LTPUDPSegment();
    LTPUDPSegment(LTPUDPSession * session);
    LTPUDPSegment(LTPUDPSegment& from);

    virtual ~LTPUDPSegment();

    enum {
        LTPUDP_SEGMENT_UNKNOWN,       // 0
        LTPUDP_SEGMENT_DS,            // 1
        LTPUDP_SEGMENT_RS,            // 2
        LTPUDP_SEGMENT_RAS,           // 3
        LTPUDP_SEGMENT_CS_BR,         // 4
        LTPUDP_SEGMENT_CAS_BR,        // 5
        LTPUDP_SEGMENT_CS_BS,         // 6
        LTPUDP_SEGMENT_CAS_BS,        // 7
        LTPUDP_SEGMENT_CTRL1,         // 8
        LTPUDP_SEGMENT_CTRL2,         // 9
        LTPUDP_SEGMENT_UNDEFINED,     // 10
        LTPUDP_SEGMENT_DS_TO,         // 11
        LTPUDP_SEGMENT_RS_TO,         // 12
        LTPUDP_SEGMENT_CS_TO,         // 13
        LTPUDP_SEGMENT_COMPLETED,     // 15
        LTPUDP_SEGMENT_END            // 16
    };
  
    string    Key()                { return key_;               }  // for the map that contains this entry
    string    Session_Key()        { return session_key_;       }
    int       Segment_Type()       { return segment_type_;      }
    int       Red_Green()          { return red_green_none_;    }
    size_t    Packet_Len()         { return packet_len_;        }
    int       Retrans_Retries()    { return retrans_retries_;   }


#ifdef LTPUDP_AUTH_ENABLED
    void      Create_Trailer();
    int       Parse_Trailer(u_char * bp, size_t current_byte, size_t length);
#endif

    bool       IsAuthenticated      (u_int32_t suite, u_int32_t key_id, string engine);

    bool       IsValid()            { return is_valid_;          }
    bool       IsCheckpoint()       { return is_checkpoint_;     }
    bool       IsEndofblock()       { return (is_redendofblock_ | is_greenendofblock_); }
    bool       IsRedEndofblock()    { return is_redendofblock_;  }
    bool       IsGreenEndofblock()  { return is_greenendofblock_;}
    bool       IsGreen()            { return (red_green_none_ == GREEN_SEGMENT ? true : false); }
    bool       IsRed()              { return (red_green_none_ == RED_SEGMENT   ? true : false); }
    bool       IsSecurityEnabled()  { return is_security_enabled_;   }
    u_int32_t  Security_Mask()      { return security_mask_;         }
    u_int32_t  Cipher_Suite()       { return cipher_suite_;          }
    u_int32_t  Cipher_Key_Id()      { return cipher_key_id_;         }
    string     Cipher_Engine()      { return cipher_engine_;         }
 
    u_int64_t Session_ID()         { return session_id_;        }
    u_int64_t Engine_ID()          { return engine_id_;         }
    u_int64_t Checkpoint_ID()      { return checkpoint_id_;     }
    u_int64_t Serial_ID()          { return serial_number_;     }
    u_int32_t Service_ID()         { return client_service_id_; }
 
    const char *    Get_Segment_Type();
    static const char *    Get_Segment_Type(int st);
    void      Build_Header_ExtTag();
    void      Create_LTP_Header();
    void      Dump_Current_Buffer(char * location);
    void      Encode_Field(u_int32_t infield);
    void      Encode_Field(u_int64_t infield);
    void      Increment_Retries();
    void      Set_Checkpoint_ID(u_int64_t new_chkpt);
    void      Set_Cipher_Suite(int new_val);
    void      Set_Cipher_Key_Id(int new_key);
    void      Set_Cipher_Engine(string new_engine);
    void      Set_Client_Service_ID(u_int32_t in_client_service_id);       
    void      SetCheckpoint();
    void      Set_Parent(LTPUDPCallbackIF * parent);
    void      Set_Report_Serial_Number(u_int64_t new_rsn);
    void      Set_Retrans_Retries(int new_counter);
    void      Set_RGN(int new_color);
    void      Set_Security_Mask(u_int32_t add_val);
    void      Set_Session_Key();
    void      UnsetCheckpoint();

    LTPUDPCallbackIF * Parent()     { return parent_; };

    oasys::ScratchBuffer<u_char*, 0> * RawPacket() { return (oasys::ScratchBuffer<u_char*, 0> *) &raw_packet_; }
    oasys::ScratchBuffer<u_char*, 0> * Packet() { return (oasys::ScratchBuffer<u_char*, 0> *) &packet_; }

    string  Serial_ID_Str() { return serial_number_str_; }
 
    int     suite_result_len(int suite);
    int     Parse_Buffer(u_char * bp, size_t length, size_t * offset);

    static  LTPUDPSegment * DecodeBuffer(u_char * bp, size_t length);

public:

    class RetransmitTimer : public oasys::SafeTimer,
                            public oasys::Logger 
    {
    public:
        RetransmitTimer(int seconds, LTPUDPCallbackIF *parent, int parent_type, 
                        int segment_type, string session_key, u_int64_t report_serial_number, 
                        void** timer_ptr, LTPUDPSegment* seg_ptr)
                : Logger("LTPUDPReportSegment::RetransmitTimer",
                         "/dtn/cl/ltpudpreport/retransmit/Timer"),
                  parent_(parent),
                  seconds_(seconds),
                  parent_type_(parent_type),
                  segment_type_(segment_type),
                  segment_key_(""),
                  session_key_(session_key),
                  report_serial_number_(report_serial_number),
                  target_counter_(0),
                  first_call_(true),
                  seg_ptr_(seg_ptr),
                  timer_ptr_(timer_ptr) {  }  

        RetransmitTimer(int seconds, LTPUDPCallbackIF *parent,int parent_type, 
                        int segment_type, string session_key, string segment_key, 
                        void** timer_ptr, LTPUDPSegment* seg_ptr)
                 : Logger("LTPUDPReportSegment::RetransmitTimer",
                             "/dtn/cl/ltpudpreport/retransmit/Timer"),
                   parent_(parent),
                   seconds_(seconds),
                   parent_type_(parent_type),
                   segment_type_(segment_type),
                   segment_key_(segment_key),
                   session_key_(session_key),
                   report_serial_number_(0),
                   target_counter_(0),
                   first_call_(true),
                   seg_ptr_(seg_ptr),
                   timer_ptr_(timer_ptr) {  }  

        void SetSeconds(int seconds) { 
            seconds_ = seconds; 
            target_counter_ = seconds + parent_->AOS_Counter();
        }

        /**
         * Timer callback function
         */
        void timeout(const struct timeval &now);

    public:

        LTPUDPCallbackIF *parent_;
        int               seconds_;
        int               parent_type_;
        int               segment_type_;
        string            segment_key_;      
        string            session_key_;          // uses one or the other depending on SegmentType
        u_int64_t         report_serial_number_;
        u_int64_t         target_counter_;

        bool              first_call_; // flag to control processing in timeout()

        LTPUDPSegment* seg_ptr_;  //dz debug - temporary for tracking down issues
        void** timer_ptr_;
    };
    //
    // end of RetransmitTimer class.......
    //

    void Create_RS_Timer(int seconds, int parent_type, int segment_type, u_int64_t report_serial_number);
    void Create_Retransmission_Timer(int seconds, int parent_type, int segment_type);
    void Start_Retransmission_Timer(RetransmitTimer* timer);
    void Cancel_Retransmission_Timer();

    RetransmitTimer *timer_;
    oasys::SafeTimerStateRef retran_timer_state_ref_;

protected:
    LTPUDPSegment& operator= ( LTPUDPSegment& ) { return *this; }

#ifdef LTPUDP_AUTH_ENABLED
    int sign_it(const u_char* msg, size_t mlen, u_char** sig, size_t* slen, EVP_PKEY* pkey);
    int verify_it(const u_char* msg, size_t mlen, const u_char* sig, size_t slen, EVP_PKEY* pkey);
#endif

protected:
    string    session_key_;

    int       retrans_retries_;
      
    // session_id, engine_id... etc....

    LTPUDPCallbackIF * parent_;
 
    string    key_;   //  <string version of offset and length used as key in segments map>

    int       version_number_;
    int       control_flags_;
    int       header_exts_;
    int       trailer_exts_;
 
    oasys::ScratchBuffer<u_char*, 0> packet_;
    oasys::ScratchBuffer<u_char*, 0> raw_packet_;

    size_t       packet_len_;

    u_int32_t security_mask_;
    u_int64_t engine_id_;
    u_int64_t session_id_;
    u_int64_t serial_number_;
    u_int64_t checkpoint_id_;

    u_int32_t client_service_id_;

    // security detail
    size_t        cipher_applicable_length_;
    size_t        cipher_trailer_offset_;
    u_int32_t  cipher_suite_;
    u_int32_t  cipher_key_id_;
    string     cipher_engine_;
  
    string    serial_number_str_;

    bool      is_security_enabled_;
    bool      is_checkpoint_;
    bool      is_redendofblock_;
    bool      is_greenendofblock_;
    bool      is_valid_;

    int       segment_type_;    // DS, RS, CS...
    int       red_green_none_;
};
//
// end of LTPUDPSegment class...
//


class LTPUDPDataSegment : public LTPUDPSegment 
{

public:

    LTPUDPDataSegment();
    LTPUDPDataSegment(LTPUDPDataSegment& from);
    LTPUDPDataSegment(LTPUDPSession * session);

    virtual ~LTPUDPDataSegment();

    LTPUDPDataSegment(u_char * bp, size_t length);

    size_t Offset()     { return offset_;     }       

    size_t Payload_Length () { return payload_length_; }

    size_t Start_Byte() { return start_byte_; }
    size_t Stop_Byte()  { return stop_byte_;  }

    oasys::ScratchBuffer<u_char *, 0> * Payload() { return (oasys::ScratchBuffer<u_char *, 0> *) &payload_; }

    void Set_Key ();
    void Encode_All();
    void Encode_Buffer();
    void Encode_Buffer(u_char * data, size_t length, size_t start_byte, bool checkpoint);

    int  Parse_Buffer(u_char * bp, size_t len);
    void Set_Endofblock();
    void Unset_Endofblock();
    void Set_Checkpoint();
    void Set_Segment_Color_Flags();
    void Unset_Checkpoint();
    void Set_Start_Byte(size_t start_byte);
    void Set_Stop_Byte(size_t stop_byte);
    void Set_Length(size_t length);
    void Set_Offset(size_t offset);

protected:

    oasys::ScratchBuffer<u_char *, 0>  payload_;

    size_t payload_length_;
    size_t offset_;

    size_t       start_byte_;
    size_t       stop_byte_;
};

typedef std::map<std::string, LTPUDPDataSegment *>   DS_MAP;
//
// end of LTPUDPDataSegment class...
//



class LTPUDPReportClaim {

public:

    LTPUDPReportClaim();
    LTPUDPReportClaim(LTPUDPReportClaim& from);
    LTPUDPReportClaim(size_t offset, size_t len);
 
    virtual ~LTPUDPReportClaim();

    string     Key()     { return key_;     }
    size_t        Length()  { return length_;  }
    size_t        Offset()  { return offset_;  }

protected:

    string    key_;
    size_t       length_;
    size_t       offset_;
}; 

typedef std::map<std::string, LTPUDPReportClaim *>   RC_MAP;
typedef std::pair<std::string, LTPUDPReportClaim*>   RC_PAIR;
//
// end of LTPUDPReportClaims class....
//



class LTPUDPReportSegment : public LTPUDPSegment 
{

public:

    LTPUDPReportSegment();
    LTPUDPReportSegment(LTPUDPReportSegment& from);
    LTPUDPReportSegment(u_char * bp, size_t len);

    LTPUDPReportSegment(LTPUDPSession * session, u_int64_t checkpoint_id, 
                        size_t lower_bounds, size_t upper_bounds, RC_MAP* claim_map);

    LTPUDPReportSegment(LTPUDPSession * session, int red_green, 
                        size_t lower_bounds_start, size_t chkpt_upper_bounds, int32_t segsize);

    virtual ~LTPUDPReportSegment();

    int        Parse_Buffer(u_char * bp, size_t len);

    u_int64_t  Report_Serial_Number()  { return report_serial_number_;      }
    size_t     UpperBounds()           { return upper_bounds_;              }
    size_t     LowerBounds()           { return lower_bounds_;              }
    bool       ReportAcked()           { return report_acked_;              }
    u_int32_t  ClaimCount()            { return claims_;                    }
    RC_MAP &   Claims()                { return claim_map_;                 }
 
    void       SetReportAcked()        { report_acked_ = true;              }
    void       Set_Key();
    void       Encode_All();
    void       Encode_All(RC_MAP* claim_map);
    void       Encode_Counters();
    void       Build_RS_Data(LTPUDPSession * session, int red_green,
                             size_t lower_bounds_start, size_t chkpt_upper_bounds, int32_t segsize);
    time_t     Last_Packet_Time();

protected:

    u_int64_t report_serial_number_;
    size_t upper_bounds_;
    size_t lower_bounds_;
    u_int32_t claims_;
    time_t    last_packet_time_;
    RC_MAP    claim_map_;
    bool      report_acked_;
};

//
// end of LTPUDPReportSegment class.......
//



class LTPUDPReportAckSegment : public LTPUDPSegment 
{
public:

    LTPUDPReportAckSegment();
    LTPUDPReportAckSegment(LTPUDPReportAckSegment& from);
    LTPUDPReportAckSegment(LTPUDPReportSegment * from, LTPUDPSession * session);
    LTPUDPReportAckSegment(u_char * bp, size_t len);

    virtual ~LTPUDPReportAckSegment();

    LTPUDPReportAckSegment(LTPUDPSession * session, LTPUDPCallbackIF * parent);

    int        Parse_Buffer(u_char * bp, size_t len);
    u_int64_t  Report_Serial_Number()          { return report_serial_number_; }
    int        Retries()                       { return retransmit_ctr_;       }
    void       Set_Retries(int new_value)      { retransmit_ctr_ = new_value; }
    void       Reset_Retrans_Timer();
    time_t     Last_Packet_Time();

protected:

    LTPUDPCallbackIF *parent_;
  
    time_t    last_packet_time_;
    u_int64_t report_serial_number_;
    int       retransmit_ctr_;
    string    str_serial_number_;
};
//
// end of LTPUDPReportAckSegment class.......
//



class LTPUDPCancelSegment : public LTPUDPSegment 
{
public:

    enum {
        LTPUDP_CS_REASON_USR_CANCLED,     // 0  Client Service cancelled session
        LTPUDP_CS_REASON_UNREACHABLE,     // 1  Unreachable client service
        LTPUDP_CS_REASON_RLEXC,           // 2  Retransmission limit exceeded.
        LTPUDP_CS_REASON_MISCOLORED,      // 3  Miscolored segment
        LTPUDP_CS_REASON_SYS_CNCLD,       // 4  A system error condition caused Unexpected session termination
        LTPUDP_CS_REASON_RXMTCYCEX,       // 5  Exceeded the Retransmission Cycles Limit
    };

    LTPUDPCancelSegment();
    LTPUDPCancelSegment(LTPUDPCancelSegment& from);
    LTPUDPCancelSegment(u_char * bp, size_t len, int segment_type);
    LTPUDPCancelSegment(LTPUDPSession * session, int segment_type, u_char reason_code, LTPUDPCallbackIF * parent);
 
    virtual ~LTPUDPCancelSegment();

    int  Parse_Buffer(u_char * bp, size_t len, int segment_type);

    const char *     Get_Reason_Code(int reason_code);

protected:

    LTPUDPCallbackIF *parent_;
  
    u_char    reason_code_;
};
//
// end of LTPUDPCancelSegment class.......
//


class LTPUDPCancelAckSegment : public LTPUDPSegment 
{
public:

    LTPUDPCancelAckSegment();
    LTPUDPCancelAckSegment(u_char * bp, size_t len, int segment_type);
    LTPUDPCancelAckSegment(LTPUDPCancelSegment * from, u_char control_byte, LTPUDPSession * session);
    LTPUDPCancelAckSegment(LTPUDPCancelAckSegment& from);

    virtual ~LTPUDPCancelAckSegment();

    int  Parse_Buffer(u_char * bp, size_t len, int segment_type);
};
//
// end of LTPUDPCancelAckSegment class.......
//


class LTPUDPSession : public oasys::Logger 
{
public:
    LTPUDPSession(u_int64_t engine_id, u_int64_t session_id, LTPUDPCallbackIF * parent, int parent_type);

    virtual ~LTPUDPSession();
   
    /**
     *     Enum for messages from the daemon thread to the sender
     *     thread.
     */
    typedef enum SessionState_t {
        LTPUDP_SESSION_STATE_UNKNOWN,      // 0 
        LTPUDP_SESSION_STATE_RS,           // 1
        LTPUDP_SESSION_STATE_CS,           // 2
        LTPUDP_SESSION_STATE_DS,           // 3
        LTPUDP_SESSION_STATE_CREATED,      // 4
        LTPUDP_SESSION_STATE_UNDEFINED     // 5
    } SessionState_t;
 
    enum {
        LTPUDP_SESSION_STATUS_UNKNOWN,          // 0
        LTPUDP_SESSION_STATUS_IN_USE,           // 1
        LTPUDP_SESSION_STATUS_CLEANUP,          // 2
        LTPUDP_SESSION_STATUS_INITIALIZED,      // 3
        LTPUDP_SESSION_STATUS_NOT_INITIALIZED,  // 3
        LTPUDP_SESSION_STATUS_UNDEFINED         // 4
    };
 
    void       AddToBundleList (BundleRef bundle,size_t bundle_size);
    int        AddSegment     (LTPUDPDataSegment * sop, bool check_for_overlap=true);

    void       SetAggTime     ();

    int        RemoveSegment  (DS_MAP * segments, size_t reception_offset, size_t reception_length);
    bool       CheckAndTrim   (LTPUDPDataSegment * sop);

    bool       HaveReportSegmentsToSend(LTPUDPDataSegment * dataseg, int32_t segsize);
    void       GenerateReportSegments(u_int64_t chkpt, size_t lower_bounds_start, size_t chkpt_upper_bounds, int32_t segsize);
    void       clear_claim_map(RC_MAP* claim_map);

    int        Check_Overlap  (DS_MAP * segments, LTPUDPDataSegment * sop);
    size_t        IsGreenFull    ();
    size_t        IsRedFull      ();
    u_int64_t  Increment_Checkpoint_ID();
    u_int64_t  Increment_Report_Serial_Number();

    u_char *  GetAllRedData();
    u_char *  GetAllGreenData();

    string                 Key()                    { return session_key_;              }
    BundleList *           Bundle_List()            { return bundle_list_;              }
    size_t                 Expected_Red_Bytes()     { return expected_red_bytes_;       }
    size_t                 Expected_Green_Bytes()   { return expected_green_bytes_;     }
    size_t                 Red_Bytes_Received()     { return red_bytes_received_;       }
    size_t                 Green_Bytes_Received()   { return green_bytes_received_;     }
    size_t                 Session_Size()           { return session_size_;             }
    u_int64_t              Engine_ID()              { return engine_id_;                }
    u_int64_t              Session_ID()             { return session_id_;               }
    u_int64_t              Checkpoint_ID()          { return checkpoint_id_;            }
    u_int64_t              Report_Serial_Number()   { return report_serial_number_;     }
    int                    reports_sent()           { return reports_sent_;             }
    SessionState_t         Session_State()          { return session_state_;            }
    int                    Session_Status()         { return session_status_;           }
    LTPUDPCancelSegment*   Cancel_Segment()         { return cancel_segment_;           }
    LTPUDPCallbackIF *     Parent()                 { return parent_;                   }
    u_int32_t              Inbound_Cipher_Suite()   { return inbound_cipher_suite_;     }
    u_int32_t              Inbound_Cipher_Key_Id()  { return inbound_cipher_key_id_;    }
    string                 Inbound_Cipher_Engine()  { return inbound_cipher_engine_;    }
    u_int32_t              Outbound_Cipher_Suite()  { return outbound_cipher_suite_;    }
    u_int32_t              Outbound_Cipher_Key_Id() { return outbound_cipher_key_id_;   }
    string                 Outbound_Cipher_Engine() { return outbound_cipher_engine_;   }

    bool                   IsSecurityEnabled()      { return is_security_enabled_;      }
    bool                   GreenProcessed()         { return green_processed_;          }
    bool                   RedProcessed()           { return red_processed_;            }
    bool                   DataProcessed()          { return red_processed_ | green_processed_; }
    bool                   Has_Inactivity_Timer()   { return NULL != inactivity_timer_state_ref_.object(); }
    bool                   IsGreen()                { return is_green_; }

    DS_MAP&  Red_Segments()    { return red_segments_;    }
    DS_MAP&  Green_Segments()  { return green_segments_;  }
    RS_MAP&  Report_Segments() { return report_segments_; }

    const char *    Get_Session_State(); 

    bool      TimeToSendIt(uint32_t milliseconds);

    void      RemoveSegments();
    void      Set_Checkpoint_ID(u_int64_t in_checkpoint);       
    void      Set_Report_Serial_Number(u_int64_t in_report_serial);       
    void      Set_Client_Service_ID(u_int32_t in_client_service_id);       
    void      Set_Cleanup();
    void      Set_Session_State(SessionState_t new_session_state);
    void      Set_EOB(LTPUDPDataSegment *sop);
    void      Set_Cancel_Segment(LTPUDPCancelSegment * cancel_segment);
    void      Set_Inbound_Cipher_Suite(int new_val);
    void      Set_Inbound_Cipher_Key_Id(int new_key);
    void      Set_Inbound_Cipher_Engine(string new_engine);
    void      Set_Outbound_Cipher_Suite(int new_val);
    void      Set_Outbound_Cipher_Key_Id(int new_key);
    void      Set_Outbound_Cipher_Engine(string new_engine);
    void      Start_Inactivity_Timer(time_t time_left);
    void      Cancel_Inactivity_Timer();
    void      Clear_Timer(oasys::Timer* triggered_timer);
    void      Set_Is_Green(bool t) { is_green_ = t; }

    void      inc_reports_sent() { ++reports_sent_; }

    time_t    Last_Packet_Time();

    oasys::SpinLock segmap_lock_;

protected:
    // disallow copy constructor
    LTPUDPSession(LTPUDPSession&)
          : Logger("LTPUDPSession", "/dtn/cl/ltpudp/session")
    {}

    LTPUDPSession& operator= ( LTPUDPSession& ) { return *this; }


    virtual void init();

protected:
    /**
     * Helper class for timing session inactivity
     */
    class InactivityTimer : public oasys::SafeTimer,
                            public oasys::Logger 
    {
    public:
        InactivityTimer( LTPUDPCallbackIF *parent, string session_key, int parent_type)
                : Logger("LTPUDPSession::InactivityTimer",
                         "/dtn/cl/ltpudp/session/Timer"),
                  parent_(parent),
                  session_key_(session_key),
                  parent_type_(parent_type),
                  seconds_(0),
                  target_counter_(0),
                  first_call_(true) { }
  
        virtual ~InactivityTimer() { }

        void SetSeconds(int seconds) { 
            seconds_ = seconds; 
            target_counter_ = seconds + parent_->AOS_Counter();
        }

        int64_t Target_Count()  {  return target_counter_;   }

        /**
         * Timer callback function
         */
        void timeout(const struct timeval &now);

    public:
        LTPUDPCallbackIF * parent_;
        string             session_key_;
        int                parent_type_;
        int                seconds_;
        int64_t            target_counter_;

        bool               first_call_; // flag to control processing in timeout()
    };


    /***
     * manage InactivityTimer
     */
    InactivityTimer     * inactivity_timer_;  
    oasys::SafeTimerStateRef inactivity_timer_state_ref_;  

    LTPUDPCallbackIF    * parent_;
    LTPUDPCancelSegment * cancel_segment_;

    string session_key_;   //  <string version of engine and session used as key in sessions map>

    time_t    last_packet_time_;

    u_int64_t engine_id_;
    u_int64_t session_id_;
    u_int64_t checkpoint_id_;
    u_int64_t report_serial_number_;

    oasys::Time agg_start_time_;

    bool red_complete_;   //  if red completes first ship all that we have
    bool green_complete_; //  
    bool red_processed_;
    bool green_processed_;
    bool is_green_;


    size_t red_highest_offset_;
    size_t green_highest_offset_;
    size_t  red_bytes_received_;
    size_t  green_bytes_received_;

    size_t  expected_red_bytes_;
    size_t  expected_green_bytes_;


    size_t  session_size_;
    int  reports_sent_;

    DS_MAP  red_segments_;
    DS_MAP  green_segments_;
    RS_MAP  report_segments_;

    BundleList * bundle_list_;

    int  parent_type_;
    SessionState_t session_state_;
    int  session_status_;
 
    // security enabled detail
    u_int32_t inbound_cipher_suite_;
    u_int32_t inbound_cipher_key_id_;
    string    inbound_cipher_engine_;

    u_int32_t outbound_cipher_suite_;
    u_int32_t outbound_cipher_key_id_;
    string    outbound_cipher_engine_;

    bool      is_security_enabled_;
};
//
// end of LTPUDPSession class...
//

struct MySendObject {
    std::string      * str_data_;
    std::string        session_;
    std::string        segment_;
    int                segment_type_;
    int                parent_type_;
    LTPUDPSegment::RetransmitTimer*   timer_;
};
 

 
}

#endif  //LTPUDP_ENABLED

#endif // COMMONOBJECT_H
