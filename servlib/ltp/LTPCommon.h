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

#ifndef LTP_COMMON_H
#define LTP_COMMON_H

#ifdef HAVE_CONFIG_H
#    include <dtn-config.h>
#endif


#include <atomic>
#include <fstream>
#include <map>
#include <stdio.h>
#include <stdlib.h>     /* malloc, free, rand */
#include <string>
#include <time.h>
#include <sys/time.h>

#include <third_party/oasys/io/FileIOClient.h>
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

typedef std::shared_ptr<LTPSegment> SPtr_LTPSegment;
typedef std::weak_ptr<LTPSegment> WPtr_LTPSegment;

typedef std::shared_ptr<LTPDataSegment> SPtr_LTPDataSegment;
typedef std::weak_ptr<LTPDataSegment> WPtr_LTPDataSegment;
typedef std::shared_ptr<LTPCancelSegment> SPtr_LTPCancelSegment;
typedef std::shared_ptr<LTPReportSegment> SPtr_LTPReportSegment;

// map of DataSegs using Start_Byte as a  key
typedef std::map<size_t, SPtr_LTPDataSegment> LTP_DS_MAP;
typedef std::shared_ptr<LTP_DS_MAP> SPtr_LTP_DS_MAP;
typedef std::unique_ptr<LTP_DS_MAP> QPtr_LTP_DS_MAP;


typedef std::map<uint64_t, SPtr_LTPReportSegment> LTP_RS_MAP;

typedef std::map<uint64_t, uint64_t> LTP_U64_MAP;
typedef LTP_U64_MAP LTP_DS_OFFSET_TO_UPPER_BOUNDS_MAP;
typedef LTP_U64_MAP LTP_CONTIG_BLOCKS_MAP;
typedef std::map<uint64_t, ssize_t> LTP_CHECKPOINT_MAP;



struct LTPDataFile {
    oasys::SpinLock lock_;
    //std::fstream data_file_;
    int data_file_fd_ = -1;         ///< Handle for the data file if in use
};

typedef std::shared_ptr<LTPDataFile> SPtr_LTPDataFile;

class LTPSessionKey
{
public:
    LTPSessionKey(uint64_t engine_id, uint64_t session_id)
        : engine_id_(engine_id),
          session_id_(session_id)
    {}

    virtual ~LTPSessionKey() {}

    uint64_t Engine_ID()  const { return engine_id_;  }
    uint64_t Session_ID() const { return session_id_; }

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



// Interface to the LTPNode::Receiver or LTPNode::Sender used by timers
class LTPNodeRcvrSndrIF;
typedef std::shared_ptr<LTPNodeRcvrSndrIF> SPtr_LTPNodeRcvrSndrIF;
typedef std::weak_ptr<LTPNodeRcvrSndrIF> WPtr_LTPNodeRcvrSndrIF;

class LTPNodeRcvrSndrIF
{
public:
    virtual ~LTPNodeRcvrSndrIF(){}

    virtual void     RSIF_post_timer_to_process(oasys::SPtr_Timer event) = 0;
    virtual uint64_t RSIF_aos_counter() = 0;
    virtual void     RSIF_retransmit_timer_triggered(int event_type, SPtr_LTPSegment retran_seg) = 0;
    virtual void     RSIF_inactivity_timer_triggered(uint64_t engine_id, uint64_t session_id) = 0;
    virtual void     RSIF_closeout_timer_triggered(uint64_t engine_id, uint64_t session_id) = 0;
    virtual bool     RSIF_shutting_down() = 0;
};


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
// set_sptr_to_self method so that it has reference to itself for interacting
// with the oasys::TiemrSystem.

class LTPTimer;
typedef std::shared_ptr<LTPTimer> SPtr_LTPTimer;
typedef std::weak_ptr<LTPTimer> WPtr_LTPTimer;

class LTPTimer : public oasys::SharedTimer,
                 public oasys::Logger 
{
public:
    LTPTimer(SPtr_LTPNodeRcvrSndrIF rsif, int event_type, uint32_t seconds);

    virtual ~LTPTimer();

    void set_sptr_to_self(SPtr_LTPTimer sptr) { sptr_to_self_ = sptr; }

    void set_seconds(int seconds);

    virtual void start();

    virtual bool cancel();

    /// this method overrides the oasys::Timer method to post itself 
    /// to the LTP TimerProcessor thread
    virtual void timeout(const struct timeval &now) override;

    /// override this method for the actual processing to be done when the timer triggers
    virtual void do_timeout_processing() = 0;

protected:

    WPtr_LTPNodeRcvrSndrIF wptr_rsif_;

    int         event_type_ = 0;
    uint32_t    seconds_ = 0;
    uint64_t    target_counter_ = 0;

    oasys::SPtr_Timer sptr_to_self_;

    oasys::SpinLock lock_;
};
//
// end of LTPTimer (base) class.......
//


class LTPRetransmitSegTimer : public LTPTimer
{
public:
    LTPRetransmitSegTimer(SPtr_LTPNodeRcvrSndrIF sptr_rsif, int event_type, uint32_t seconds,
                          SPtr_LTPSegment sptr_retran_seg);

    virtual ~LTPRetransmitSegTimer();

    void do_timeout_processing() override;

protected:

    WPtr_LTPSegment wptr_retran_seg_;
};
//
// end of LTPRetransmitSegTimer class.......
//



class LTPCloseoutTimer;
typedef std::shared_ptr<LTPCloseoutTimer> SPtr_LTPCloseoutTimer;

class LTPCloseoutTimer : public LTPTimer
{
public:
    LTPCloseoutTimer(SPtr_LTPNodeRcvrSndrIF sptr_rsif, uint64_t engine_id, uint64_t session_id);

    virtual ~LTPCloseoutTimer();

    void do_timeout_processing() override;

protected:
    uint64_t engine_id_ = 0;
    uint64_t session_id_ = 0;

};
//
// end of LTPCloseoutTimer class.......
//
    

class LTPSegment : public oasys::Logger
{

public:

    LTPSegment();
    LTPSegment(const LTPSegment& from);
    LTPSegment(const LTPSession* session);

    virtual ~LTPSegment();

    std::string     session_key_str();
    int       Segment_Type()          const { return segment_type_;      }
    int       Red_Green()             const { return red_green_none_;    }
    size_t    Packet_Len()            const { return packet_len_;        }
    uint32_t  Retrans_Retries()       const { return retrans_retries_;   }

    virtual std::string* asNewString()      { return new std::string((const char*)packet_.buf(), packet_len_); }

    bool       IsAuthenticated      (uint32_t suite, uint32_t key_id, std::string& engine);

    bool       IsValid()               const { return is_valid_;          }
    bool       IsCheckpoint()          const { return is_checkpoint_;     }
    bool       IsEndofblock()          const { return (is_redendofblock_ | is_greenendofblock_); }
    bool       IsRedEndofblock()       const { return is_redendofblock_;  }
    bool       IsGreenEndofblock()     const { return is_greenendofblock_;}
    bool       IsGreen()               const { return (red_green_none_ == GREEN_SEGMENT); }
    bool       IsRed()                 const { return (red_green_none_ == RED_SEGMENT); }
    bool       IsSecurityEnabled()     const { return is_security_enabled_;   }
    bool       IsDeleted()             const { return is_deleted_;            }
    uint32_t   Security_Mask()         const { return security_mask_;         }
    uint32_t   Cipher_Suite()          const { return cipher_suite_;          }
    uint32_t   Cipher_Key_Id()         const { return cipher_key_id_;         }
    std::string Cipher_Engine()        const { return cipher_engine_;       }
 
    uint64_t Session_ID()              const { return session_id_;        }
    uint64_t Engine_ID()               const { return engine_id_;         }
    uint64_t Checkpoint_ID()           const { return checkpoint_id_;     }
    uint64_t Serial_ID()               const { return serial_number_;     }
    uint32_t Service_ID()              const { return client_service_id_; }
 
    const char*        Get_Segment_Type();
    static const char* Get_Segment_Type(int st);

    const char*        Get_Seg_Str();
    static const char* Get_Seg_Str(int st);

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
    void      Set_Report_Serial_Number(uint64_t new_rsn);
    void      Set_RGN(int new_color);
    void      Set_Security_Mask(uint32_t add_val);

    /**
     * Set flag indicating the segment is considered deleted and cancel the retransmission  timer
     */
    virtual void Set_Deleted();

     /**
      * provide access to the segment's lock
      */
    oasys::Lock* lock()  { return &lock_; }


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

    virtual SPtr_LTPTimer Create_Retransmission_Timer(SPtr_LTPNodeRcvrSndrIF rsif, int event_type, 
                                                      uint32_t seconds, SPtr_LTPSegment sptr_retran_seg);
    virtual SPtr_LTPTimer Renew_Retransmission_Timer(SPtr_LTPNodeRcvrSndrIF rsif, int event_type, 
                                                      uint32_t seconds, SPtr_LTPSegment sptr_retran_seg);
    bool Start_Retransmission_Timer();
    virtual void Cancel_Retransmission_Timer();

    void* Retransmission_Timer_Raw_Ptr() { return sptr_timer_.get(); }
protected:
    LTPSegment(LTPSegment&& from) = delete;

    LTPSegment& operator= ( LTPSegment& ) = delete;

protected:

    uint32_t  retrans_retries_;
      
    // session_id, engine_id... etc....

    bool timer_created_ = false;
    SPtr_LTPTimer sptr_timer_; ///< shared pointer to the retransmit timer


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
    size_t       cipher_applicable_length_;
    size_t       cipher_trailer_offset_;
    uint32_t     cipher_suite_;
    uint32_t     cipher_key_id_;
    std::string  cipher_engine_;
  
    bool      is_security_enabled_;
    bool      is_checkpoint_;
    bool      is_redendofblock_;
    bool      is_greenendofblock_;
    bool      is_valid_;
    bool      is_deleted_ = false;   ///< flag to let timer know the segment was deleted due to ACK or Cancel

    int       segment_type_;    ///< DS, RS, CS...
    int       red_green_none_;

    oasys::SpinLock lock_;
};
//
// end of LTPSegment class...
//


class LTPDataSegment : public LTPDataSegKey,
                       public LTPSegment 
{

public:

    LTPDataSegment() = delete;
    LTPDataSegment(const LTPSession* session);
    LTPDataSegment(u_char * bp, size_t length);

    virtual ~LTPDataSegment();

    virtual void Set_Deleted() override;

    size_t Offset()     { return offset_;     }       

    size_t Payload_Length () { return payload_length_; }
    size_t Transmit_Length() { return packet_len_ + payload_length_; }

    /**
     * Generates a string of bytes to be transmitted for this segment.
     * If there is an error reading from an LTP session file then it genreates
     * a minimal DS with a Client Seervice ID of 9999 to force the receiver to
     * abort the session.
     */
    std::string* asNewString() override; ///< return then packet header plus payload as a string for transmission

    u_char* Payload() { return payload_; }      ///< return a pointer to the payload data
    u_char* Release_Payload();                  ///< release the payload to the caller to lifetime manage
    void    Set_Payload(u_char* payload);       ///< take over lifetime management of the provided payload


    void Encode_All();
    void Encode_Checkpoint_Bit();
    void Encode_LTP_DS_Header();
    void Encode_Buffer(u_char * data, size_t length, size_t start_byte, bool checkpoint, bool eob);

    int  Parse_Buffer(u_char * bp, size_t len);
    void Set_Endofblock();
    void Unset_Endofblock();

    void Set_Segment_Color_Flags();
    void Set_Start_Byte(size_t start_byte);
    void Set_Stop_Byte(size_t stop_byte);
    void Set_Length(size_t length);
    void Set_Offset(size_t offset);

    /**
     * Configure an outgoing LTP DS for accessing its payload data from the
     * provide file rather than in memory
     * @param sptr_file Shread pointer to the LTP Session's data file
     * @param start_byte Starting byte within the LTP Session for this segment
     * @param length Length of this data segment
     * @param eob Whether or not this is the last segment of the LTP Sesssion
     */
    void Set_File_Usage(SPtr_LTPDataFile sptr_file, const std::string& file_path, 
                        size_t start_byte, size_t length, bool eob);


    /**
     * Flag indicating if the DS is queued to be transmitted
     */
    bool Queued_To_Send();
    /*
     * Set the flag indicating if the DS is queued to be transmitted
     * @param queued Value to set the flag indicating if the DS is queued to be transmitted
     */
    void Set_Queued_To_Send(bool queued);

    /**
     * Set the file handle to use if the incoming LTP payload is to be written to disk
     * @param handle The handle to the already open file ready for writing
     */
    void Set_LTP_Payload_Handle(int handle);

protected:
    LTPDataSegment(LTPDataSegment&& from) = delete;

    LTPDataSegment& operator= ( LTPDataSegment& ) = delete;

    bool read_seg_from_file(u_char* buf, size_t buf_len);

protected:

    bool queued_to_send_ = false;       ///< Whether this segment is already queued to be sent

    u_char* payload_ = nullptr;         ///< The data portion of the segment

    size_t payload_length_ = 0;         ///< The length of the data portion of the segment
    size_t offset_ = 0;                 ///< The offset within the Session's data of this payload

    std::string file_path_;             ///< path to the file containing the payload of the LTP session
    SPtr_LTPDataFile sptr_file_;        ///< Shared pointer to the Session's data file if in use 
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
    LTPReportSegment(u_char* bp, size_t len);

    LTPReportSegment(LTPSession* session, uint64_t checkpoint_id,
                     size_t lower_bounds, size_t upper_bounds, 
                     size_t claimed_bytes, LTP_RC_MAP& claim_map);

    LTPReportSegment(uint64_t engine_id, uint64_t session_id, uint64_t checkpoint_id,
                     size_t lower_bounds, size_t upper_bounds, 
                     size_t claimed_bytes, LTP_RC_MAP& claim_map);

    virtual ~LTPReportSegment();

    int         Parse_Buffer(u_char * bp, size_t len);

    uint64_t    Report_Serial_Number() const  { return serial_number_; }
    size_t      UpperBounds()          const  { return upper_bounds_;  }
    size_t      LowerBounds()          const  { return lower_bounds_;  }
    size_t      ClaimCount()           const  { return claims_;        }
    size_t      ClaimedBytes()         const  { return claimed_bytes_; }
    LTP_RC_MAP& Claims()                      { return claim_map_;     }
 
    void        Encode_All(LTP_RC_MAP& claim_map);
    void        Encode_Counters();

protected:
    LTPReportSegment(LTPReportSegment& from) = delete;
    LTPReportSegment(LTPReportSegment&& from) = delete;

    LTPReportSegment& operator= ( LTPReportSegment& ) = delete;

protected:
    //uint64_t  report_serial_number_ = 0;
    size_t    upper_bounds_ = 0;
    size_t    lower_bounds_ = 0;
    size_t    claims_ = 0;
    size_t    claimed_bytes_ = 0;

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
    LTPReportAckSegment(const LTPReportSegment * from, const LTPSession* session);
    LTPReportAckSegment(u_char * bp, size_t len);

    virtual ~LTPReportAckSegment();

    LTPReportAckSegment(const LTPSession* session);

    int        Parse_Buffer(u_char * bp, size_t len);
    uint64_t   Report_Serial_Number() const    { return serial_number_;       }
    int        Retries() const                 { return retransmit_ctr_;      }
    void       Set_Retries(int new_value)      { retransmit_ctr_ = new_value; }
    void       Reset_Retrans_Timer();

protected:
    LTPReportAckSegment(LTPReportAckSegment& from) = delete;
    LTPReportAckSegment(LTPReportAckSegment&& from) = delete;

    LTPReportAckSegment& operator= ( LTPReportAckSegment& ) = delete;

protected:
    std::string key_; // hold the key for the timer... matches Report Key..

    int          retransmit_ctr_;
    std::string  str_serial_number_;
};
//
// end of LTPReportAckSegment class.......
//



class LTPCancelSegment : public LTPSegment 
{
public:

    enum {
        LTP_CS_REASON_USR_CANCLED,     // 0  Client Service cancelled session
        LTP_CS_REASON_UNREACHABLE,     // 1  Unreachable client service
        LTP_CS_REASON_RLEXC,           // 2  Retransmission limit exceeded.
        LTP_CS_REASON_MISCOLORED,      // 3  Miscolored segment
        LTP_CS_REASON_SYS_CNCLD,       // 4  A system error condition caused Unexpected session termination
        LTP_CS_REASON_RXMTCYCEX,       // 5  Exceeded the Retransmission Cycles Limit
    };

    LTPCancelSegment();
    LTPCancelSegment(LTPSegment* from, int segment_type, u_char reason_code) ;
    LTPCancelSegment(u_char * bp, size_t len, int segment_type);
    LTPCancelSegment(const LTPSession* session, int segment_type, u_char reason_code);
 
    virtual ~LTPCancelSegment();

    int  Parse_Buffer(u_char * bp, size_t len, int segment_type);

    const char *     Get_Reason_Code(int reason_code);

protected:
    LTPCancelSegment(LTPCancelSegment& from) = delete;
    LTPCancelSegment(LTPCancelSegment&& from) = delete;

    LTPCancelSegment& operator= ( LTPCancelSegment& ) = delete;

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
    LTPCancelAckSegment(LTPCancelSegment * from, u_char control_byte, const LTPSession* session);

    virtual ~LTPCancelAckSegment();

    int  Parse_Buffer(u_char * bp, size_t len, int segment_type);
protected:
    LTPCancelAckSegment(LTPCancelAckSegment& from) = delete;
    LTPCancelAckSegment(LTPCancelAckSegment&& from) = delete;

    LTPCancelAckSegment& operator= ( LTPCancelAckSegment& ) = delete;

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

    /**
     * Attempt to add a Data Segment to the list of received red or green segments
     * @param sptr_ds Shared pointer to the newly received Data Segment
     * @param check_for_overlap flag indicating whether a check for overlap needs to be done or if the check can be bypassed 
     * @return 0 = segment was added; -1 = segment not added because it was a duplicate; -99 = disk I/O error requires aborting the session
     */
    int        AddSegment_incoming (SPtr_LTPDataSegment sptr_ds, bool check_for_overlap=true);

    /**
     * Add a Data Segment to the list of received red or green segments to be transmitted. Since these are being generated
     * properly there is no need to check for overlap.
     * @param sptr_ds Shared pointer to the newly created Data Segment to be added to the session
     * @return 0 = segment was added; -1 = error which should not happen
     */
    int        AddSegment_outgoing (SPtr_LTPDataSegment sptr_ds);

    void       Add_Checkpoint_Seg(LTPDataSegment* ds_seg);
    void       Flag_Checkpoint_As_Claimed(uint64_t checkpoint_id);
    ssize_t    Find_Checkpoint_Stop_Byte(uint64_t checkpoint_id);

    void       SetAggTime     ();

    void       RS_Claimed_Entire_Session();
    size_t     RemoveClaimedSegments(uint64_t checkpoint_id, size_t lower_bounds, 
                                     size_t claim_offset, size_t upper_bounds,
                                     LTP_DS_MAP* retransmit_dsmap_ptr);

    bool       CheckAndTrim   (LTPDataSegment* ds_seg);

    bool       HaveReportSegmentsToSend(LTPDataSegment * ds_seg, int32_t segsize, size_t& bytes_claimed);
    size_t     GenerateReportSegments(uint64_t chkpt, size_t lower_bounds_start, size_t chkpt_upper_bounds, int32_t segsize);
    void       clear_claim_map(LTP_RC_MAP* claim_map);

    int        Check_Overlap  (LTP_DS_MAP* segments, LTPDataSegment* ds_seg);
    size_t     IsGreenFull    ();
    size_t     IsRedFull      ();
    uint64_t   Get_Next_Checkpoint_ID();
    uint64_t   Get_Next_Report_Serial_Number();

    SPtr_LTP_DS_MAP GetAllRedData();
    void GetRedDataFile(std::string& file_path);
    SPtr_LTP_DS_MAP GetAllGreenData();

    std::string         key_str() const;
    BundleList *        Bundle_List()            const { return bundle_list_;              }
    size_t              Expected_Red_Bytes()     const { return expected_red_bytes_;       }
    size_t              Expected_Green_Bytes()   const { return expected_green_bytes_;     }
    size_t              Red_Bytes_Received()     const { return red_bytes_received_;       }
    size_t              Total_Red_Segments()     const { return total_red_segments_;       }
    size_t              Session_Size()           const { return session_size_;             }
    int                 reports_sent()           const { return reports_sent_;             }
    int                 Session_State()          const { return session_state_;            }
    SPtr_LTPCancelSegment   Cancel_Segment()           { return cancel_segment_;           }
    uint32_t            Inbound_Cipher_Suite()   const { return inbound_cipher_suite_;     }
    uint32_t            Inbound_Cipher_Key_Id()  const { return inbound_cipher_key_id_;    }
    std::string         Inbound_Cipher_Engine()  const { return inbound_cipher_engine_;    }
    uint32_t            Outbound_Cipher_Suite()  const { return outbound_cipher_suite_;    }
    uint32_t            Outbound_Cipher_Key_Id() const { return outbound_cipher_key_id_;   }
    std::string         Outbound_Cipher_Engine() const { return outbound_cipher_engine_;   }

    bool                IsSecurityEnabled()      const { return is_security_enabled_;      }
    bool                GreenProcessed()         const { return green_processed_;          }
    bool                RedProcessed()           const { return red_processed_;            }
    bool                DataProcessed()          const { return red_processed_ | green_processed_; }
    bool                Has_Inactivity_Timer()   const { return inactivity_timer_ != nullptr; }
    bool                Is_LTP_Cancelled()       const { return session_state_ == LTP_SESSION_STATE_CS; }

    bool                Send_Green()             const { return send_green_;      }
    bool                Send_Red()               const { return !send_green_;     }
    bool                Is_EOB_Defined()         const { return red_complete_;    }

    SPtr_LTP_DS_MAP     Red_Segments()           const { return red_segments_;    }
    SPtr_LTP_DS_MAP     Green_Segments()         const { return green_segments_;  }
    LTP_RS_MAP&         Report_Segments()              { return report_segments_; }

    const char *    Get_Session_State(); 

    ssize_t   First_RedSeg_Offset();

    bool      TimeToSendIt(uint32_t milliseconds);
    void      Set_Send_Green(bool new_val)       { send_green_ = new_val; }

    void      RemoveSegments();
    void      Set_Checkpoint_ID(uint64_t in_checkpoint);       
    void      Set_Report_Serial_Number(uint64_t in_report_serial);       
    void      Set_Client_Service_ID(uint32_t in_client_service_id);       
    void      Set_Session_State(int new_session_state);
    void      Set_EOB(LTPDataSegment *sop);
    void      Set_Cancel_Segment(SPtr_LTPCancelSegment cancel_segment);
    void      Clear_Cancel_Segment();
    void      Set_Inbound_Cipher_Suite(int new_val);
    void      Set_Inbound_Cipher_Key_Id(int new_key);
    void      Set_Inbound_Cipher_Engine(std::string& new_engine);
    void      Set_Outbound_Cipher_Suite(int new_val);
    void      Set_Outbound_Cipher_Key_Id(int new_key);
    void      Set_Outbound_Cipher_Engine(std::string& new_engine);
    void      Start_Inactivity_Timer(SPtr_LTPNodeRcvrSndrIF rsif, time_t time_left);
    void      Cancel_Inactivity_Timer();
    void      Start_Closeout_Timer(SPtr_LTPNodeRcvrSndrIF rsif, time_t time_left);

    void      inc_reports_sent() { ++reports_sent_; }

    time_t    Last_Packet_Time() { return last_packet_time_; }
    void      Set_Last_Packet_Time(time_t pt) { last_packet_time_ = pt; }

    oasys::Lock* SegMap_Lock() { return &segmap_lock_; }

    bool      using_file() { return use_file_; }
    void      set_file_usage(std::string& dir_path, bool run_with_disk_io_kludge=false);

    int       write_to_file(char* buf, size_t offset, size_t len);
    bool      abort_due_to_disk_io_error() { return use_file_ && disk_io_error_; }

    SPtr_LTPDataFile open_session_file_write(std::string& file_path);
    SPtr_LTPDataFile open_session_file_read(std::string& file_path);

public:
    bool      has_ds_resends() { return has_ds_resends_; }
    void      set_has_ds_resends() { has_ds_resends_ = true; }

protected:
    // disallow copy constructor
    LTPSession(LTPSession& other) = delete;
    LTPSession(LTPSession&& other) = delete;

    LTPSession& operator= ( LTPSession& )  = delete;


    virtual void init();

    /**
     * maintains a map of the contiguous blocks that the received Data Segements represent
     * which is faster to spin through than the individual segments for generating reports
     * and cehcking for overlaps
     *
     * @param start_byte the starting byte offset of a received DS
     * @param stop_byte the stop byte offset of a received DS
     * @param duplciate returns whether the DS is a duplicae reception
     * @return whether only a portion of the DS overlaps previous receptions
     */
    bool check_red_contig_blocks(uint64_t start_byte, uint64_t stop_byte, bool& duplicate);

    bool open_file_write();
    bool open_file_read();
    bool create_storage_path(std::string& path);

    /**
     * Open an LTP Data file if needed and then write out the
     * Data Segment's data
     */
    bool store_data_in_file(LTPDataSegment* ds_seg);

    /**
     * Buffers contiguous data segments to perform larger disk write operations 
     * to improve performance when possible
     */
    bool write_ds_to_file(LTPDataSegment* ds_seg);

    /**
     * Write the previous Data Segment's data to the LTP data file
     * because the just received DS is not contiguous to it
     */
    bool write_prev_seg_to_file();

    /**
     * Write a larger work buffer to the LTP data file
     */
    bool write_work_buf_to_file();

    /**
     * Buffers contiguous data segments to perform larger disk write operations 
     * to improve performance when possible
     *
     * XXX/dz  This is a major kludge. The development system went through a phase for
     *         several days where about 5% of the time, anything written to the file
     *         after having to seek back to an earlier position in the file did not take.
     *         The workaround for that was to close and reopen the file anytime that scenario
     *         arose but then there was the very last segment not getting ssaved to disk
     *         about 2% of the time. So the final kludge that seems to make it work 
     *         consistently was to close and reopen the file after writing the last segment
     *         and write it again if the file was short.
     *
     *         - I had the same issues using oasys::FileIOClient and ofstream/ifstream/fstream
     *
     *         - I also tried using a Read/Write file and reading in data before and after
     *           writing to segment that required a seek backward. The reads were null 
     *           characters before the write and the expected characters after the write
     *           but when the file was closed/reopened or rewound to read in for processing
     *           the data in those segments was bad.
     *
     *         - The issue miraculously cleared up so I reverted to the original code and
     *           left this in for now just in case.
     */
    bool write_ds_to_file_with_kludges(LTPDataSegment* ds_seg);

    /**
     * This version does not attempt to use a work_buf to write out larger
     * chunks of data to the file. 
     */
    bool write_ds_to_file_with_kludges_prev_seg_only(LTPDataSegment* ds_seg);

    /**
     * Write the previous Data Segment's data to the LTP data file
     * because the just received DS is not contiguous to it
     */
    bool write_prev_seg_to_file_with_kludges(bool is_eob);

    /**
     * Write a larger work buffer to the LTP data file
     */
    bool write_work_buf_to_file_with_kludges(bool is_eob);

    /**
     * Close and reopen and seek to the desired location in the LTP
     * data file for a write as a workaround for an issue after a seek backwards 
     * with writing the end of block buffer.
     */
    bool reopen_file_and_seek_kludge(off64_t offset);

public:
    void dump_red_contiguous_blocks();
    size_t get_num_red_contiguous_bloks();
    void dump_red_segments(oasys::StringBuffer* buf);

protected:
    oasys::SpinLock segmap_lock_;
    oasys::SpinLock ids_lock_;

    /**
     * Helper class for timing session inactivity
     */
    class InactivityTimer : public LTPTimer
    {
    public:
        InactivityTimer(SPtr_LTPNodeRcvrSndrIF rsif, uint64_t engine_id, uint64_t session_id);

        virtual ~InactivityTimer();

        void do_timeout_processing();

    protected:

        uint64_t engine_id_ = 0;
        uint64_t session_id_ = 0;
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

    bool red_eob_received_;   ///<  once we receive the Red EOB then we truly know how many bytes to expect
    bool red_complete_;
    bool green_complete_;     //  
    bool red_processed_;
    bool green_processed_;

    size_t red_bytes_received_;

    size_t  expected_red_bytes_;
    size_t  expected_green_bytes_;
    size_t  session_size_;
    int  reports_sent_;

    size_t total_red_segments_ = 0;
    size_t first_contiguous_bytes_ = 0;

    bool send_green_ = false;  ///< Whether this outgoing LTP Session is red or green

    SPtr_LTP_DS_MAP     red_segments_;      ///< List of "red" LTP Data Segments for this session
    SPtr_LTP_DS_MAP     green_segments_;    ///< List of "green" LTP Data Segments for this session
    LTP_CHECKPOINT_MAP  checkpoints_;       ///< Mapping of sent checkpoints and the associated stop byte
    LTP_RS_MAP          report_segments_;   ///< List of LTP Report Segments for this session

    LTP_U64_MAP  ds_offset_to_upperbounds_;  ///< mapping of DS checkpoint offsets to their upper bounds 
                                             ///< used to determine the lower bounds of the next checkpoint
    LTP_U64_MAP  red_contig_blocks_;         ///< mapping of start byte and length of received contiguous blocks of DS segments

    BundleList * bundle_list_;               ///< List of Bundles being sent via this LTP session

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
    bool has_ds_resends_     = false;


    bool use_file_ = false;                   ///< whether to write LTP session data to a file
    bool disk_io_error_ = false;              ///< flag for LTPEngine to check to see if there was a disk I/O error so it can cancel the session
    bool use_disk_io_kludges_ = false;        ///< Whether to use disk I/O kludges that have been necessary on the 
                                              ///< development system to ensure reliable writes after backward seeks and for the
                                              ///< final segment written to the file. opefully not needed on all systems.

    std::string dir_base_path_;               ///< user configured path to store LTP files in
    std::string dir_path_;                    ///< user configured path plus the engine ID as a subdirectory
    std::string file_path_;                   ///< path to the file containing the payload of the LTP session
    SPtr_LTPDataFile sptr_file_;              ///< shared pointer to the LTP data file
    size_t bytes_written_to_disk_ = 0;        ///< Track number of bytes written to the file

    off64_t work_buf_alloc_size_ = 10000000;   ///< size of the allocaed work_buf (10 MB when allocated)
    u_char* work_buf_ = nullptr;
    off64_t work_buf_size_ = 0;                ///< size of the allocaed work_buf (10 MB when allocated)
    off64_t work_buf_idx_ = 0;                 ///< current index/offset into the work_buf (how much is filled)
    off64_t work_buf_available_ = 0;           ///< bytes available in the work_buf
    off64_t work_buf_file_offset_ = 0;         ///< starting offset into the file for this work_buf

    u_char* prev_seg_payload_ = nullptr;       ///< payload from the previously received data degement; held temporarily
                                               ///< hoping the next one will be consecutive and they can be combined 
                                               ///< before writing them to the session file
    off64_t prev_seg_payload_offset_ = 0;      ///< the offset into the session file where the prev data seg needs to be written
    off64_t prev_seg_payload_len_ = 0;         ///< the length of the previous segment's payload


};
//
// end of LTPSession class...
//

}


#endif // LTP_COMMON_H
