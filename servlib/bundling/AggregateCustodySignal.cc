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

#ifdef ACS_ENABLED

#include <inttypes.h>
#include <stdint.h>
#include <limits.h>

#include <oasys/util/ScratchBuffer.h>

#include "AggregateCustodySignal.h"
#include "Bundle.h"
#include "storage/GlobalStore.h"
#include "SDNV.h"

namespace dtn {

//----------------------------------------------------------------------
void
AggregateCustodySignal::create_aggregate_custody_signal(Bundle*           bundle,
                                                        PendingAcs*       pacs, 
                                                        const EndpointID& source_eid)
{
    log_debug_p("/dtn/acs", "Create ACS (%s) to: %s    pacs_id(%d)",
                (pacs->succeeded() ? "success" : "fail"),
                pacs->custody_eid().c_str(), pacs->pacs_id());
    
    bundle->mutable_source()->assign(source_eid);
    if (pacs->custody_eid().equals(EndpointID::NULL_EID())) {
        PANIC("create_custody_signal: "
              "aggregate custody signal cannot be generated to null eid");
    }
    bundle->mutable_dest()->assign(pacs->custody_eid());
    bundle->mutable_replyto()->assign(EndpointID::NULL_EID());
    bundle->mutable_custodian()->assign(EndpointID::NULL_EID());
    bundle->set_is_admin(true);

    // use the expiration time from the original bundles
    // XXX/demmer maybe something more clever??
    // XXX/dz snag expiration from first bundle received and pass in here?
    bundle->set_expiration(86400);

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
    *bp = (BundleProtocol::ADMIN_AGGREGATE_CUSTODY_SIGNAL << 4);
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
    pacs->pacs_id() = GlobalStore::instance()->next_pacsid();
    pacs->acs_payload_length() = 0;
    pacs->num_custody_ids() = 0;
    pacs->acs_entry_map()->clear();

}

//----------------------------------------------------------------------
bool
AggregateCustodySignal::parse_aggregate_custody_signal(data_t* data,
                                                       const u_char* bp, u_int len)
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
    if (data->admin_type_ != BundleProtocol::ADMIN_AGGREGATE_CUSTODY_SIGNAL) {
        return false;
    }

    // Success flag and reason code
    if (len < 1) { return false; }
    data->succeeded_ = (*bp >> 7);
    data->reason_    = (*bp & 0x7f);
    bp++;
    len--;
    
    u_int64_t last_custody_id = GlobalStore::instance()->last_custodyid();

    // parse out the entries to fill the acs entry map
    u_int64_t fill_len = 0;
    u_int64_t right_edge = 0;
    u_int64_t diff;
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

            // A malicious attack could take the form of a single entry 
            // specifying left edge=0 and length_of_fill=0xffffffffffffffff 
            // which would wipe out all custody bundles and tie up the thread 
            // in a near infinite loop. We can check that the last custody id 
            // in the ACS is not greater than the last issued Custody ID and 
            // kick it out if it is.
            if ( (entry->left_edge_ > last_custody_id) ||
                 (fill_len > (last_custody_id - entry->left_edge_ + 1)) ) { 
                log_crit_p("/dtn/acs/", "Received ACS attempting to "
                           "acknowledge bundles beyond those issued - "
                           "abort");
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
AggregateCustodySignal::reason_to_str(u_int8_t reason)
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
      custody_eid_(),
      succeeded_(false),
      reason_(BundleProtocol::CUSTODY_NO_ADDTL_INFO),
      acs_payload_length_(0),
      num_custody_ids_(0),
      acs_entry_map_(NULL),
      acs_expiration_timer_(NULL),
      in_datastore_(false),
      queued_for_datastore_(false),
      params_revision_(0),
      acs_delay_(0),
      acs_size_(0)
{}


//----------------------------------------------------------------------
PendingAcs::PendingAcs(const oasys::Builder&)
    : key_(""),
      pacs_id_(0),
      custody_eid_(),
      succeeded_(false),
      reason_(BundleProtocol::CUSTODY_NO_ADDTL_INFO),
      acs_payload_length_(0),
      num_custody_ids_(0),
      acs_entry_map_(NULL),
      acs_expiration_timer_(NULL),
      in_datastore_(false),
      queued_for_datastore_(false),
      params_revision_(0),
      acs_delay_(0),
      acs_size_(0)
{}

//----------------------------------------------------------------------
PendingAcs::~PendingAcs () {
    if (NULL != acs_expiration_timer_) {
        acs_expiration_timer_->cancel();
        acs_expiration_timer_ = NULL;
    }

    if (NULL != acs_entry_map_) {
        AcsEntryIterator ae_itr = acs_entry_map_->begin();
        while ( ae_itr != acs_entry_map_->end() ) {
            AcsEntry* entry = ae_itr->second;
            delete entry;
            acs_entry_map_->erase(ae_itr);

            ae_itr = acs_entry_map_->begin();
        }
        delete acs_entry_map_;
        acs_entry_map_ = NULL;
    }
}


//----------------------------------------------------------------------
void
PendingAcs::serialize(oasys::SerializeAction* a)
{
    u_int32_t reason = (u_int32_t) reason_;

    a->process("key", &key_);
    a->process("pacs_id", &pacs_id_);
    a->process("custody_eid", &custody_eid_);
    a->process("succeeded", &succeeded_);
    a->process("reason", &reason);
    a->process("payload_len", &acs_payload_length_);
    a->process("num_custody_ids", &num_custody_ids_);

    // process the acs_entry_map_
    AcsEntry* entry;
    u_int sz = 0;
    if (a->action_code() == oasys::Serialize::MARSHAL || \
        a->action_code() == oasys::Serialize::INFO) {
        if (NULL == acs_entry_map_) {
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
            ASSERT(NULL == acs_entry_map_);
            acs_entry_map_ = new AcsEntryMap();

            for ( u_int i=0; i<sz; i++ ) {
                entry = new AcsEntry();

                entry->serialize(a);

                acs_entry_map_->insert(AcsEntryPair(entry->left_edge_, entry));
            }
        }
    }
}


} // namespace dtn

#endif // ACS_ENABLED
