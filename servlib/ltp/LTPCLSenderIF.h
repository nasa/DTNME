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

#ifndef _LTP_CL_SENDER_IF_H_
#define _LTP_CL_SENDER_IF_H_

#include "bundling/Bundle.h"
#include "bundling/BlockInfo.h"
#include <third_party/oasys/thread/Timer.h>

namespace dtn {

class LTPCLSenderIF;
class LTPTimer;
class LTPDataSegment;


typedef std::shared_ptr<LTPCLSenderIF> SPtr_LTPCLSenderIF;
//typedef std::unique_ptr<LTPCLSenderIF> QPtr_LTPCLSenderIF;

#ifndef SPtr_LTPTimer
    typedef std::shared_ptr<LTPTimer> SPtr_LTPTimer;
#endif

#ifndef SPtr_LTPDataSegment
    typedef std::shared_ptr<LTPDataSegment> SPtr_LTPDataSegment;
#endif

class LTPCLSenderIF
{
public:
    LTPCLSenderIF() {}

    virtual ~LTPCLSenderIF(){}

    virtual uint64_t           Remote_Engine_ID() = 0;

    virtual void               Set_Ready_For_Bundles(bool input_flag) = 0;
    virtual void               Send_Admin_Seg_Highest_Priority (std::string * send_data, SPtr_LTPTimer timer, bool back) = 0;
    virtual void               Send_DataSeg_Higher_Priority(SPtr_LTPDataSegment ds_seg, SPtr_LTPTimer timer) = 0;
    virtual void               Send_DataSeg_Low_Priority(SPtr_LTPDataSegment ds_seg, SPtr_LTPTimer timer) = 0;
    virtual uint32_t           Retran_Intvl() = 0;
    virtual uint32_t           Retran_Retries() = 0;
    virtual uint32_t           Inactivity_Intvl() = 0;
    virtual void               Add_To_Inflight(const BundleRef& bref) = 0;
    virtual void               Del_From_Queue(const BundleRef&  bref) = 0;
    virtual size_t             Get_Bundles_Queued_Count() = 0;
    virtual size_t             Get_Bundles_InFlight_Count() = 0;
    virtual bool               Del_From_InFlight_Queue(const BundleRef&  bref) = 0;
    virtual void               Delete_Xmit_Blocks(const BundleRef&  bref) = 0;
    virtual uint32_t           Max_Sessions() = 0;
    virtual uint32_t           Agg_Time() = 0;
    virtual uint64_t           Agg_Size() = 0;
    virtual uint32_t           Seg_Size() = 0;
    virtual bool               CCSDS_Compatible() = 0;
    virtual size_t             Ltp_Queued_Bytes_Quota() = 0;
    virtual size_t             Bytes_Per_CheckPoint() = 0;
    virtual bool               Use_Files_Xmit() = 0;
    virtual bool               Use_Files_Recv() = 0;
    virtual bool               Keep_Aborted_Files() = 0;
    virtual bool               Use_DiskIO_Kludge() = 0;
    virtual int32_t            Recv_Test() = 0;
    virtual std::string&       Dir_Path() = 0;

    virtual void               PostTransmitProcessing(BundleRef& bref, bool red, uint64_t expected_bytes, bool success) = 0;
    virtual SPtr_BlockInfoVec  GetBlockInfoVec(Bundle *bref) = 0;
    virtual bool               Dump_Sessions() = 0;
    virtual bool               Dump_Segs() = 0;
    virtual bool               Hex_Dump() = 0;
    virtual bool               AOS() = 0;
    virtual uint32_t           Inbound_Cipher_Suite()  = 0;
    virtual uint32_t           Inbound_Cipher_Key_Id() = 0;
    virtual std::string&       Inbound_Cipher_Engine() = 0;
    virtual uint32_t           Outbound_Cipher_Suite( ) = 0;
    virtual uint32_t           Outbound_Cipher_Key_Id() = 0;
    virtual std::string&       Outbound_Cipher_Engine() = 0;

};

}    
#endif 
