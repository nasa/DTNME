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
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif


#include <inttypes.h>
#include <stdint.h>
#include <limits.h>

#include <third_party/oasys/util/ScratchBuffer.h>

#include "AggregateCustodySignal.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocolVersion6.h"
#include "BundleProtocolVersion7.h"
#include "storage/GlobalStore.h"
#include "SDNV.h"

#define SET_FLDNAMES(fld) \
    cborutil.set_fld_name(fld);

namespace dtn {

//----------------------------------------------------------------------
void
AggregateCustodySignal::create_aggregate_custody_signal(Bundle*           bundle,
                                                        PendingAcs*       pacs, 
                                                        const SPtr_EID&   sptr_source_eid)
{
    log_debug_p("/dtn/acs", "Create ACS (%s) to: %s    pacs_id(%d)",
                (pacs->succeeded() ? "success" : "fail"),
                pacs->custody_eid().c_str(), pacs->pacs_id());
    
    bundle->set_source(sptr_source_eid);
    if (pacs->custody_eid() == BD_NULL_EID()) {
        log_err_p("/dtn/acs", "create_custody_signal: "
                  "aggregate custody signal cannot be generated to null eid");
        return;
    }


    bundle->mutable_dest() = pacs->custody_eid();

    bundle->mutable_replyto() = BD_MAKE_EID_NULL();
    bundle->mutable_custodian() = BD_MAKE_EID_NULL();
    bundle->set_is_admin(true);

    // use the expiration time from the original bundles
    // XXX/demmer maybe something more clever??
    // XXX/dz snag expiration from first bundle received and pass in here?
    bundle->set_expiration_secs(86400);

    int sdnv_encoding_len1 = 0;
    int sdnv_encoding_len2 = 0;
    int signal_len = 0;


    // we generally don't expect the AggregateCustody Signal length to be > 2048 bytes
    oasys::ScratchBuffer<u_char*, 2048> scratch;
    
    // format of aggregate custody signals:
    //
    // 1 byte admin payload type and flags
    // 1 byte status code
    // SDNV   [Left edge of first fill]
    // SDNV   [Length of first fill]
    // SDNV   [Difference between right of previous fill]
    // SDNV   [Length of fill]
    // SDNV   [Difference between right of previous fill]
    // SDNV   [Length of fill]
    // etc. for as many entries as needed

    //
    // first calculate the length:
    //    worst case length is 2 fixed bytes plus pacs->acs_payload_length_
    //    * SDNV length of Differences between fills will be 
    //      less than or equal to the SDNV length of the Left edges 
    //      of the fills
    //
    signal_len =  1 + 1 + pacs->acs_payload_length();

    
    //
    // now format the buffer
    //
    u_char* bp = scratch.buf(signal_len);
    int len = signal_len;
    int actual_len = 0;
    
    // Admin Payload Type and flags
    *bp = (BundleProtocolVersion6::ADMIN_AGGREGATE_CUSTODY_SIGNAL << 4);
    bp++;
    len--;
    actual_len++;
    
    // Success flag and reason code
    *bp++ = ((pacs->succeeded() ? 1 : 0) << 7) | (pacs->reason() & 0x7f);
    len--;
    actual_len++;
   
    // loop through the ACS entries and add them to the buffer
    AcsEntry* entry;
    int entry_num = 0;
    uint64_t right_edge = 0;
    uint64_t diff; 
    AcsEntryIterator del_itr;
    AcsEntryIterator ae_itr = pacs->acs_entry_map()->begin();
    while ( ae_itr != pacs->acs_entry_map()->end() ) {
        entry = ae_itr->second;

        log_debug_p("/dtn/acs", "pacs_id(%d) Entry: left: %" PRIu64 " length: %" PRIu64,
                    pacs->pacs_id(), entry->left_edge_, entry->length_of_fill_);
    

        // Add the Left edge of first fill or the difference between fills
        // NOTE: diff with initial right_edge of 0 gives us the left edge for the first fill
        diff = entry->left_edge_ - right_edge;

        ASSERT(diff == entry->diff_to_prev_right_edge_);

        sdnv_encoding_len1 = SDNV::encode(diff, bp, len);
        ASSERT(sdnv_encoding_len1 > 0);

        len -= sdnv_encoding_len1;
        bp  += sdnv_encoding_len1;
        actual_len += sdnv_encoding_len1;

        // Add the length of the fill
        sdnv_encoding_len2 = SDNV::encode(entry->length_of_fill_, bp, len);
        ASSERT(sdnv_encoding_len2 > 0);
        ASSERT((sdnv_encoding_len1 + sdnv_encoding_len2) == entry->sdnv_length_);
        len -= sdnv_encoding_len2;
        bp  += sdnv_encoding_len2;
        actual_len += sdnv_encoding_len2;

        // calculate the new right edge
        right_edge = entry->left_edge_ + (entry->length_of_fill_ - 1);

        // save a copy of the iterator so that we can remove it from the map
        del_itr = ae_itr;

        // move to the next entry
        ++ae_itr;
        ++entry_num;

        // clean up
        delete entry;
        pacs->acs_entry_map()->erase(del_itr);
    }

    // 
    // Finished generating the payload
    //
    bundle->mutable_payload()->set_data(scratch.buf(), actual_len);

    log_debug_p("/dtn/acs", "Finished ACS pacs_id(%d)",
                pacs->pacs_id());
    

    // clear out the Pending ACS for re-use
    pacs->pacs_id_ = GlobalStore::instance()->next_pacsid();
    pacs->acs_payload_length_ = 0;
    pacs->num_custody_ids_ = 0;
    pacs->acs_entry_map()->clear();

}

//----------------------------------------------------------------------
bool
AggregateCustodySignal::parse_aggregate_custody_signal(data_t* data,
                                                       const u_char* bp, uint32_t len)
{
    // create an acs entry map
    data->acs_entry_map_ = new AcsEntryMap(); 

    // 1 byte Admin Payload Type + Flags:
    if (len < 1) { return false; }
    data->admin_type_  = (*bp >> 4);
    data->admin_flags_ = *bp & 0xf;
    bp++;
    len--;

    // validate the admin type
    if (data->admin_type_ != BundleProtocolVersion6::ADMIN_AGGREGATE_CUSTODY_SIGNAL) {
        return false;
    }

    // Success flag and reason code
    if (len < 1) { return false; }
    data->succeeded_ = (*bp >> 7);
    data->reason_    = (*bp & 0x7f);
    bp++;
    len--;
    
    data->redundant_ = (data->reason_ == BundleProtocol::CUSTODY_REDUNDANT_RECEPTION);


    // parse out the entries to fill the acs entry map
    uint64_t fill_len = 0;
    uint64_t right_edge = 0;
    uint64_t diff;
    int num_bytes;
    while ( len > 0 ) {
        // read the diff between previous right edge and this left edge
        num_bytes = SDNV::decode(bp, len, &diff);
        if (num_bytes == -1) { return false; }
        bp  += num_bytes;
        len -= num_bytes;

        // read the length of the fill
        num_bytes = SDNV::decode(bp, len, &fill_len);
        if (num_bytes == -1) { return false; }
        bp  += num_bytes;
        len -= num_bytes;

        // add this entry to the acs entry map
        AcsEntry* entry = new AcsEntry();
        entry->left_edge_ = right_edge + diff;
        entry->length_of_fill_ = fill_len;

        // our implementation does not issue 0 as a valid custody id so we
        // know this is a bogus or erroneous ACS
        if (0 == entry->left_edge_) {
            log_crit_p("/dtn/acs/", "Received ACS with a Custody ID of zero "
                       "which is invalid - abort");
            return false;
        }

        if (entry->left_edge_ > 0) { 
            // check for a 64 bit overflow 
            if (fill_len > (UINT64_MAX - entry->left_edge_ + 1)) { 
                log_crit_p("/dtn/acs/", "Received ACS with Length of Fill "
                           "which overflows 64 bits - abort");
                return false; 
            }
        }

        log_debug_p("/dtn/acs", "parse - add entry - left: %" PRIu64 "  len: %" PRIu64,
                  entry->left_edge_, entry->length_of_fill_);

        data->acs_entry_map_->insert(AcsEntryPair(entry->left_edge_, entry));

        // parenthesized to prevent uint overflow
        right_edge = entry->left_edge_ + (entry->length_of_fill_ - 1);
    }

    return (data->acs_entry_map_->size() > 0);
}


//----------------------------------------------------------------------
const char*
AggregateCustodySignal::reason_to_str(uint8_t reason)
{
    switch (reason) {
    case BundleProtocol::CUSTODY_NO_ADDTL_INFO:
        return "no additional info";
        
    case BundleProtocol::CUSTODY_REDUNDANT_RECEPTION:
        return "redundant reception";
        
    case BundleProtocol::CUSTODY_DEPLETED_STORAGE:
        return "depleted storage";
        
    case BundleProtocol::CUSTODY_ENDPOINT_ID_UNINTELLIGIBLE:
        return "eid unintelligible";
        
    case BundleProtocol::CUSTODY_NO_ROUTE_TO_DEST:
        return "no route to dest";
        
    case BundleProtocol::CUSTODY_NO_TIMELY_CONTACT:
        return "no timely contact";
        
    case BundleProtocol::CUSTODY_BLOCK_UNINTELLIGIBLE:
        return "block unintelligible";
    }

    static char buf[64];
    snprintf(buf, 64, "unknown reason %d", reason);
    return buf;
}



//----------------------------------------------------------------------
void
AggregateCustodySignal::create_bibe_custody_signal(Bundle*           bundle,
                                                   PendingAcs*       pacs, 
                                                   const SPtr_EID&   sptr_source_eid)
{
    log_debug_p("/dtn/bibe", "Create BIBE/ACS (%s) to: %s    pacs_id(%d)",
                (pacs->succeeded() ? "success" : "fail"),
                pacs->custody_eid().c_str(), pacs->pacs_id());
    
    if (pacs->custody_eid() == BD_NULL_EID()) {
        log_err_p("/dtn/bibe/acs", "create_custody_signal: "
                    "BIBE aggregate custody signal cannot be generated to null eid");
        return;
    }

    bundle->set_source(sptr_source_eid);
    bundle->mutable_dest() = pacs->custody_eid();
    bundle->mutable_replyto() = BD_MAKE_EID_NULL();
    bundle->mutable_custodian() = BD_MAKE_EID_NULL();
    bundle->set_is_admin(true);

    // use the expiration time from the original bundles
    // XXX/demmer maybe something more clever??
    // XXX/dz snag expiration from first bundle received and pass in here?
    bundle->set_expiration_secs(86400);


    // Calculate the max bytes needed for the admin rec & BIBE overhead 
    //   1 byte for BP7 admin record CBOR array of length 2
    //   1 byte for CBOR UInt value 4 representing admin record type 4
    //   1 byte for BIBE Custody Signal CBOR array of length 2
    //   9 bytes for max possible disposition scope sequence CBOR array header
    //   1 + 9 + 9 bytes for each entry in the disposition scope sequence array which is a CBOR array of length 2
    //                   so 1 bytes for the CBOR array header and 9 bytes for each of the two UInt values 
    uint64_t max_size = 3 + 9 + (pacs->acs_entry_map()->size() * 19);

    oasys::ScratchBuffer<uint8_t*, 0> scratch;
    scratch.reserve(max_size);

    CborUtil cborutil;

    // encode the BP7 admin record array size
    int64_t encoded_len = 0;
    uint64_t uval64 = 2;
    int64_t need_bytes = cborutil.encode_array_header(scratch.end(), scratch.nfree(), 
                                               0, uval64, encoded_len);
    ASSERT(need_bytes == 0);
    scratch.incr_len(encoded_len);
    
    // encode the BP7 admin record type 3
    uval64 = BundleProtocolVersion7::ADMIN_CUSTODY_SIGNAL;
    need_bytes = cborutil.encode_uint64(scratch.end(), scratch.nfree(), 
                                         0, uval64, encoded_len);
    ASSERT(need_bytes == 0);
    scratch.incr_len(encoded_len);
    
    // encode the BIBE Custody Signal content array size
    uval64 = 2;
    need_bytes = cborutil.encode_array_header(scratch.end(), scratch.nfree(), 
                                               0, uval64, encoded_len);
    ASSERT(need_bytes == 0);
    scratch.incr_len(encoded_len);
  
    // First entry in the array is the disposition code
    uval64 = pacs->reason();
    need_bytes = cborutil.encode_uint64(scratch.end(), scratch.nfree(), 
                                        0, uval64, encoded_len);
    ASSERT(need_bytes == 0);
    scratch.incr_len(encoded_len);
   
    // second entry is an array of disposition scope sequences
    uval64 = pacs->acs_entry_map()->size(); 
    need_bytes = cborutil.encode_array_header(scratch.end(), scratch.nfree(), 
                                               0, uval64, encoded_len);
    ASSERT(need_bytes == 0);
    scratch.incr_len(encoded_len);

    // loop through the ACS entries and add the disposition scope sequences
    AcsEntry* entry;
    AcsEntryIterator del_itr;
    AcsEntryIterator ae_itr = pacs->acs_entry_map()->begin();
    while ( ae_itr != pacs->acs_entry_map()->end() ) {
        entry = ae_itr->second;

        //log_debug_p("/dtn/bibe/acs", "pacs_id(%d) Entry: left: %" PRIu64 " length: %" PRIu64,
        //            pacs->pacs_id(), entry->left_edge_, entry->length_of_fill_);
    

        // encode the scope sequence array size
        uval64 = 2;
        need_bytes = cborutil.encode_array_header(scratch.end(), scratch.nfree(), 
                                                   0, uval64, encoded_len);
        ASSERT(need_bytes == 0);
        scratch.incr_len(encoded_len);

        // first entry in this array is the first transmission ID in the sequence (or "left edge")
        need_bytes = cborutil.encode_uint64(scratch.end(), scratch.nfree(), 
                                             0, entry->left_edge_, encoded_len);
        ASSERT(need_bytes == 0);
        scratch.incr_len(encoded_len);
    
        // second entry in this array is the number of transmission IDs in this sequence ("length of fill")
        need_bytes = cborutil.encode_uint64(scratch.end(), scratch.nfree(), 
                                             0, entry->length_of_fill_, encoded_len);
        ASSERT(need_bytes == 0);
        scratch.incr_len(encoded_len);

        // save a copy of the iterator so that we can remove it from the map
        del_itr = ae_itr;

        // move to the next entry
        ++ae_itr;

        // clean up
        delete entry;
        pacs->acs_entry_map()->erase(del_itr);
    }

    // 
    // Finished generating the payload
    //
    bundle->mutable_payload()->set_data(scratch.buf(), scratch.len());


    // clear out the Pending ACS for re-use
    pacs->pacs_id_ = GlobalStore::instance()->next_pacsid();
    pacs->acs_payload_length_ = 0;
    pacs->num_custody_ids_ = 0;
    pacs->acs_entry_map()->clear();

}

//----------------------------------------------------------------------
bool
AggregateCustodySignal::parse_bibe_custody_signal(CborValue& cvAdminElements, CborUtil& cborutil, data_t* data)
{

    int status;
    CborError err;
    CborValue cvCustodySignalArray;
    CborValue cvScopeReportArray;
    CborValue cvScopeSequenceArray;

    uint64_t tmp_num_elements = 0;
    uint64_t num_sequences = 0;

    // 2nd element of admin array is the BIBE custody signal array
    SET_FLDNAMES("BIBECustodySignal-Array")
    status = cborutil.validate_cbor_fixed_array_length(cvAdminElements, 2, 2, tmp_num_elements);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe/acs", "Error - CBOR BIBE Custody Signal array must contain 2 elements");
        return false;
    }

    err = cbor_value_enter_container(&cvAdminElements, &cvCustodySignalArray);
    if (err != CborNoError) {
        log_err_p("/bibe/acs", "Error entering CBOR BIBE Custody Signal array");
        return false;
    }

    
    // 1st element of custody signal array is the disposition code
    uint64_t disposition_code = 0;
    SET_FLDNAMES("BIBECustodySignal-DispositionCode")
    status = cborutil.decode_uint(cvCustodySignalArray, disposition_code);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe/acs", "Error decoding CBOR BIBE Custody Signal disposition code");
        return false;
    }
    data->succeeded_ = (disposition_code == BundleProtocolVersion7::CUSTODY_TRANSFER_DISPOSITION_ACCEPTED);
    data->reason_    = disposition_code;

    data->redundant_ = (data->reason_ == BundleProtocolVersion7::CUSTODY_TRANSFER_DISPOSITION_REDUNDANT);

    // 2nd element is an array of disposition scope sequences
    SET_FLDNAMES("BIBECustodySignal-ScopeArray")
    status = cborutil.validate_cbor_fixed_array_length(cvCustodySignalArray, 0, UINT32_MAX, num_sequences);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe/acs", "Error - CBOR BIBE Custody Signal disposition scope sequences array must be fixed length");
        return false;
    }

    err = cbor_value_enter_container(&cvCustodySignalArray, &cvScopeReportArray);
    if (err != CborNoError) {
        log_err_p("/bibe/acs", "Error entering CBOR BIBE Custody Signal array");
        return false;
    }

    // create an acs entry map
    data->acs_entry_map_ = new AcsEntryMap(); 

    // loop processing the scope sequences
    uint64_t transmission_id = 0;
    uint64_t sequence_len = 0;
    uint64_t count = 0;
    while (count < num_sequences) {
        // Each sequence is an array with 2 elements
        SET_FLDNAMES("BIBECustodySignal-ScopeEntryArray")
        status = cborutil.validate_cbor_fixed_array_length(cvScopeReportArray, 2, 2, tmp_num_elements);
        if (status != CBORUTIL_SUCCESS) {
            log_err_p("/bibe/acs", "Error - Each CBOR BIBE dispositon scope sequence must contain 2 elements");
            return false;
        }

        err = cbor_value_enter_container(&cvScopeReportArray, &cvScopeSequenceArray);
        if (err != CborNoError) {
            log_err_p("/bibe/acs", "Error entering CBOR BIBE Custody Signal disposition scope sequence array #%" PRIu64, count+1);
            return false;
        }

        // 1st element is a starting transmission ID
        SET_FLDNAMES("BIBECustodySignal-ScopeEntry-TransmissionID")
        status = cborutil.decode_uint(cvScopeSequenceArray, transmission_id);
        if (status != CBORUTIL_SUCCESS) {
            log_err_p("/bibe/acs", "Error decoding CBOR BIBE Custody Signal disposition sequence transmisison ID #%" PRIu64, count+1);
            return false;
        }

        // 2nd element is the number of consecutive IDs
        SET_FLDNAMES("BIBECustodySignal-ScopeEntry-SequenceLen")
        status = cborutil.decode_uint(cvScopeSequenceArray, sequence_len);
        if (status != CBORUTIL_SUCCESS) {
            log_err_p("/bibe/acs", "Error decoding CBOR BIBE Custody Signal disposition sequence length #%" PRIu64, count+1);
            return false;
        }

        err = cbor_value_leave_container(&cvScopeReportArray, &cvScopeSequenceArray);
        if (err != CborNoError) {
            log_err_p("/bibe/acs", "Error entering CBOR BIBE Custody Signal disposition scope sequence array #%" PRIu64, count+1);
            return false;
        }

        if (0 == transmission_id) {
            log_crit_p("/bibe/acs/", "Received BIBE Custody Signal with a Custody ID of zero "
                       "which is invalid - abort");
            return false;
        }

        if (sequence_len > (UINT64_MAX - transmission_id + 1)) {
            log_crit_p("/bibe/acs/", "Received ACS with Length of Fill "
                       "which overflows 64 bits - abort");
            return false; 
        }


        // add this entry to the acs entry map
        AcsEntry* entry = new AcsEntry();
        entry->left_edge_ = transmission_id;
        entry->length_of_fill_ = sequence_len;

        log_debug_p("/dtn/acs", "parse - add entry - left: %" PRIu64 "  len: %" PRIu64,
                  entry->left_edge_, entry->length_of_fill_);

        data->acs_entry_map_->insert(AcsEntryPair(entry->left_edge_, entry));

        ++count;
    }

    return (data->acs_entry_map_->size() > 0);
}



//----------------------------------------------------------------------
AcsEntry::AcsEntry() 
    : left_edge_(0),
      diff_to_prev_right_edge_(0),
      length_of_fill_(0),
      sdnv_length_(0)
{}


//----------------------------------------------------------------------
AcsEntry::AcsEntry(const oasys::Builder&)
    : left_edge_(0),
      diff_to_prev_right_edge_(0),
      length_of_fill_(0),
      sdnv_length_(0)
{}


//----------------------------------------------------------------------
void
AcsEntry::serialize(oasys::SerializeAction* a)
{
    a->process("left_edge", &left_edge_);
    a->process("diff", &diff_to_prev_right_edge_);
    a->process("fill", &length_of_fill_);
    a->process("sdnv_len", &sdnv_length_);
}


//----------------------------------------------------------------------
PendingAcs::PendingAcs(std::string key) 
    : key_(key),
      pacs_id_(0),
      succeeded_(false),
      reason_(BundleProtocol::CUSTODY_NO_ADDTL_INFO),
      acs_payload_length_(0),
      num_custody_ids_(0),
      acs_entry_map_(nullptr),
      acs_expiration_timer_(nullptr),
      in_datastore_(false),
      queued_for_datastore_(false),
      params_revision_(0),
      acs_delay_(0),
      acs_size_(0),
      bp_version_(-1)
{
    sptr_custody_eid_ = BD_MAKE_EID_NULL();
}


//----------------------------------------------------------------------
PendingAcs::PendingAcs(const oasys::Builder&)
    : key_(""),
      pacs_id_(0),
      succeeded_(false),
      reason_(BundleProtocol::CUSTODY_NO_ADDTL_INFO),
      acs_payload_length_(0),
      num_custody_ids_(0),
      acs_entry_map_(nullptr),
      acs_expiration_timer_(nullptr),
      in_datastore_(false),
      queued_for_datastore_(false),
      params_revision_(0),
      acs_delay_(0),
      acs_size_(0),
      bp_version_(-1)
{
    sptr_custody_eid_ = BD_MAKE_EID_NULL();
}

//----------------------------------------------------------------------
PendingAcs::~PendingAcs () {
    if (nullptr != acs_expiration_timer_) {
        oasys::SPtr_Timer sptr = acs_expiration_timer_;
        acs_expiration_timer_->cancel_timer(sptr);
        acs_expiration_timer_ = nullptr;
    }

    if (nullptr != acs_entry_map_) {
        AcsEntryIterator ae_itr = acs_entry_map_->begin();
        while ( ae_itr != acs_entry_map_->end() ) {
            AcsEntry* entry = ae_itr->second;
            delete entry;
            acs_entry_map_->erase(ae_itr);

            ae_itr = acs_entry_map_->begin();
        }
        delete acs_entry_map_;
        acs_entry_map_ = nullptr;
    }
}


//----------------------------------------------------------------------
void
PendingAcs::serialize(oasys::SerializeAction* a)
{
    uint32_t reason = (uint32_t) reason_;

   std::string tmp_eid = sptr_custody_eid_->str();

    a->process("key", &key_);
    a->process("pacs_id", &pacs_id_);
    a->process("bp_version", &bp_version_);
    a->process("custody_eid", &tmp_eid);
    a->process("succeeded", &succeeded_);
    a->process("reason", &reason);
    a->process("payload_len", &acs_payload_length_);
    a->process("num_custody_ids", &num_custody_ids_);

    // process the acs_entry_map_
    AcsEntry* entry;
    uint32_t sz = 0;
    if (a->action_code() == oasys::Serialize::MARSHAL || \
        a->action_code() == oasys::Serialize::INFO) {
        if (nullptr == acs_entry_map_) {
            a->process("map_size", &sz);
        } else {
            sz = acs_entry_map_->size();

            a->process("map_size", &sz);

            AcsEntryIterator ae_itr = acs_entry_map_->begin();
            while ( ae_itr != acs_entry_map_->end() ) {
                entry = ae_itr->second;

                entry->serialize(a);

                ++ae_itr;
            }
        }
    }

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        reason_ = (BundleProtocol::custody_signal_reason_t) (reason & 0x000f);

        a->process("map_size", &sz);
        if (sz > 0) {
            ASSERT(nullptr == acs_entry_map_);
            acs_entry_map_ = new AcsEntryMap();

            for ( uint32_t i=0; i<sz; i++ ) {
                entry = new AcsEntry();

                entry->serialize(a);

                acs_entry_map_->insert(AcsEntryPair(entry->left_edge_, entry));
            }
        }
    }

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        sptr_custody_eid_ = BD_MAKE_EID(tmp_eid);
    }
}


} // namespace dtn

