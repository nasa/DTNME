
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <third_party/oasys/debug/Log.h>

#include "BIBEExtractor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleEvent.h"
#include "BundleProtocol.h"
#include "BundleProtocolVersion6.h"
#include "BundleProtocolVersion7.h"
#include "contacts/ContactManager.h"
#include "reg/Registration.h"
#include "storage/BundleStore.h"



namespace dtn {

//----------------------------------------------------------------------
BIBEExtractor::BIBEExtractor()
    : Thread("BIBEExtractor"),
      Logger("BIBEExtractor", "/dtn/bibextract"),
      eventq_("/dtn/bibextract"),
      cborutil_("BibeExtractor")
{
    start();
}


//----------------------------------------------------------------------
BIBEExtractor::~BIBEExtractor()
{
    BibeEvent* event;

    while (eventq_.try_pop(&event)) {
        event->bref_ = nullptr;
        delete event;
    }
}

//----------------------------------------------------------------------
void
BIBEExtractor::post(Bundle* bundle, Registration* reg)
{
    BibeEvent* event = new BibeEvent();
    event->bref_ = bundle;
    event->reg_  = reg;

    eventq_.push_back(event);
}

//----------------------------------------------------------------------
void
BIBEExtractor::run() 
{
    char threadname[16] = "BIBEextractor";
    pthread_setname_np(pthread_self(), threadname);
   
    BibeEvent* event;

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
            if (eventq_.try_pop(&event)) {
                ASSERT(event != nullptr)

                process_event(event);


                event->bref_ = nullptr;
                event->reg_= nullptr;
                delete event;
            }
        }
    }
}

//----------------------------------------------------------------------
void
BIBEExtractor::process_event(BibeEvent* event)
{
    if (extract_bundle(event->bref_.object())) {
        event->bref_->fwdlog()->update( event->reg_, ForwardingInfo::DELIVERED);

        if (event->bref_->is_bpv6()) {
            BundleDaemon::instance()->inc_bibe6_extractions();
        } else {
            BundleDaemon::instance()->inc_bibe7_extractions();
        }
    } else {
        log_err("malformed BP6 Bundle In Bundle Encapsulation: *%p", event->bref_.object());

        if (event->bref_->is_bpv6()) {
            BundleDaemon::instance()->inc_bibe6_extraction_errors();
        } else {
            BundleDaemon::instance()->inc_bibe7_extraction_errors();
        }
    }

    BundleDaemon::post(new BundleDeliveredEvent(event->bref_.object(), event->reg_));
}

//----------------------------------------------------------------------
bool
BIBEExtractor::extract_bundle(Bundle* bibe_bundle)
{
    uint64_t block_size = 4 * 1024 * 1024; 
    recvbuf_.clear();
    recvbuf_.reserve(block_size);

    uint64_t payload_len = bibe_bundle->payload().length();
    uint64_t bytes_consumed = 0;

    uint64_t chunk_size;

    // Calculate the max bytes needed for the admin rec & BIBE overhead 
    //   1 byte for BP6 compatibility admin record of type 3 (for BP6 BIBE only - not part of RFC-5050)
    //   1 byte for BP7 admin record CBOR array of length 2  (used with BP6 BIBE also)
    //   1 byte for CBOR UInt value 3 representing admin record type 3
    //   1 byte for BIBE BPDU CBOR array of length 3
    //   9 bytes for CBOR UInt max value representing the transmission ID
    //   9 bytes for CBOR UInt max value representing the retransmission time
    //   + 9 bytes for the byte string header with length
    uint64_t admin_header_max_len = 22 + 9;


    // read in up to the max header bytes from the payload
    chunk_size = std::min(payload_len, admin_header_max_len);
    bibe_bundle->payload().read_data(0, chunk_size, (u_char*) recvbuf_.end());
    recvbuf_.fill(chunk_size);
    


    // start decoding the BIBE payload
    if (bibe_bundle->is_bpv6()) {
        // verify the first byte indicates our pseudo BPv6 BIBE 
        if (((*recvbuf_.start()) >> 4) != BundleProtocolVersion6::ADMIN_BUNDLE_IN_BUNDLE_ENCAP) {
            log_err_p("/bibe", "BIBE extraction error - BPv6 variant has incorrect admin record type");
            return false;
        }
        recvbuf_.consume(1);
        bytes_consumed += 1;
    }

    uint64_t transmission_id = 0;
    uint64_t retransmit_time = 0;
    uint8_t  reason = BundleProtocolVersion7::CUSTODY_TRANSFER_DISPOSITION_BLOCK_UNINTELLIGIBLE;

    // now ready to begin CBOR decoding of the header data
    int status;
    CborError err;
    CborParser parser;
    CborValue cvAdminArray;
    CborValue cvAdminElements;
    CborValue cvBibeArray;

    err = cbor_parser_init((const uint8_t*)recvbuf_.start(), recvbuf_.fullbytes(), 0, &parser, &cvAdminArray);
    if (err != CborNoError) {
        log_err_p("/bibe", "BIBE extraction error initializing CBOR parser");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    uint64_t num_elements;
    status = cborutil_.validate_cbor_fixed_array_length(cvAdminArray, 2, 2, num_elements);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe", "BIBE extraction error - CBOR admin array must contain 2 elements");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    err = cbor_value_enter_container(&cvAdminArray, &cvAdminElements);
    if (err != CborNoError) {
        log_err_p("/bibe", "BIBE extraction error entering CBOR admin array container");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    // 1st element of admin array is the record type
    uint64_t admin_rec_type = 0;
    status = cborutil_.decode_uint(cvAdminElements, admin_rec_type);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe", "BIBE extraction error - error decoding CBOR admin record type");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }
    if (admin_rec_type != BundleProtocolVersion7::ADMIN_BUNDLE_IN_BUNDLE_ENCAP) {
        log_err_p("/bibe", "BIBE extraction error - incorrect admin record type");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    // 2nd element of admin array is the BIBE array
    status = cborutil_.validate_cbor_fixed_array_length(cvAdminElements, 3, 3, num_elements);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe", "BIBE extraction error - CBOR BIBE array must contain 3 elements");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    err = cbor_value_enter_container(&cvAdminElements, &cvBibeArray);
    if (err != CborNoError) {
        log_err_p("/bibe", "BIBE extraction error entering CBOR admin array container");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    
    // 1st element of BIBE array is the transmission ID
    status = cborutil_.decode_uint(cvBibeArray, transmission_id);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe", "BIBE extraction error - error decoding CBOR transmission ID");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    // 2nd element of BIBE array is the retransmit time
    status = cborutil_.decode_uint(cvBibeArray, retransmit_time);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe", "BIBE extraction error - error decoding CBOR retransmit time");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }


    const uint8_t* next_byte = cbor_value_get_next_byte(&cvBibeArray);
    uint64_t bytes_before_bibe_byte_string = (char*) next_byte - recvbuf_.start();
    bytes_consumed += bytes_before_bibe_byte_string;


    // 3rd element of BIBE array is a byte string containing the encapsulated bundle
    // -- determine the byte string size
    uint64_t bstr_header_len = 0;
    uint64_t formatted_len = 0;
    status = cborutil_.decode_length_of_fixed_string(cvBibeArray, formatted_len, bstr_header_len);
    if (status != CBORUTIL_SUCCESS) {
        log_err_p("/bibe", "BIBE extraction error - error decoding CBOR byte string length of encapsulated bundle");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    bytes_consumed += bstr_header_len;  
    
    ASSERT(payload_len == (bytes_consumed + formatted_len));

    // now have the offset into the payload to where the encapsulaed bundle starts
    // - feed chunk size blocks to the BP7 consumer...
    recvbuf_.clear();

    std::unique_ptr<Bundle> bptr ( new Bundle(BundleProtocol::BP_VERSION_UNKNOWN) );

    int cc;
    bool complete = false;
    while (formatted_len > 0) {
        chunk_size = std::min(formatted_len, std::min(block_size, recvbuf_.tailbytes()));
        bibe_bundle->payload().read_data(bytes_consumed, chunk_size, (u_char*) recvbuf_.end());
        recvbuf_.fill(chunk_size);

        cc = BundleProtocol::consume(bptr.get(), (u_char*)recvbuf_.start(), recvbuf_.fullbytes(), &complete);

        if (cc < 0) {
            log_err_p("/bibe", "BIBE extraction error in BundleProtocol::consume()");
            return handle_custody_transfer(bibe_bundle, transmission_id, reason);
        }
        if (cc == 0) {
            log_always_p("/bibe", "BIBE extraction error in BundleProtocol::consume() returned zero ??");
        }

        bytes_consumed += cc;
        formatted_len -= cc;
        recvbuf_.consume(cc);

        if (formatted_len == 0) {
            if (!complete) {
                log_err_p("/bibe", "BIBE extraction error - BundleProtocol::consume() did not return complete as true");
                return handle_custody_transfer(bibe_bundle, transmission_id, reason);
            } else {
                break;
            }
        } else {
            if (!complete) {
                log_err_p("/bibe", "BIBE extraction error - BundleProtocol::consume() returned complete as true with unprocessed data left");
                return handle_custody_transfer(bibe_bundle, transmission_id, reason);
            }
        }

        // force buffer to move any left over bytes to the front of the buffer
        recvbuf_.reserve(recvbuf_.size() - recvbuf_.fullbytes());
    }

    
    BundleProtocol::status_report_reason_t reception_reason;
    BundleProtocol::status_report_reason_t deletion_reason;
    bool valid = BundleProtocol::validate(bptr.get(), &reception_reason, &deletion_reason);
    if (!valid) {
        log_err_p("/bibe", "BIBE extraction error - rejecting encapsulated bundle due to Block Unintelligible");
        return handle_custody_transfer(bibe_bundle, transmission_id, reason);
    }

    // have a valid bundle - transfer the payload reserved space to the new bundle
    BundleStore* bs = BundleStore::instance();
    bs->reserve_payload_space(bptr.get());

    bs->release_payload_space(bibe_bundle->durable_size() - 1);
    // and release the disk space - keeping 1 byte for BDStorage to flag
    // whether the bundle is in the database or not which uses + or - the size
    bibe_bundle->mutable_payload()->truncate(1);


    std::string link_name;
    EndpointID remote_eid;
    ForwardingInfo info;
    bibe_bundle->fwdlog()->get_latest_entry(ForwardingInfo::RECEIVED, &info);
    link_name = info.link_name();
    remote_eid.assign(info.remote_eid());
    
    LinkRef link("BibeExtractor");
    if (!link_name.empty()) {
        link = BundleDaemon::instance()->contactmgr()->find_link(link_name.c_str());
    }


    bptr->mutable_prevhop()->assign(bibe_bundle->prevhop());


    BundleDaemon::post( new BundleReceivedEvent(bptr.release(), EVENTSRC_PEER, 
                                                payload_len, remote_eid, link.object()) );

    reason = BundleProtocolVersion7::CUSTODY_TRANSFER_DISPOSITION_ACCEPTED;

    return handle_custody_transfer(bibe_bundle, transmission_id, reason);
}

//----------------------------------------------------------------------
bool
BIBEExtractor::handle_custody_transfer(Bundle* bibe_bundle, uint64_t transmission_id, uint8_t reason)
{
    bool success = (reason == BundleProtocolVersion7::CUSTODY_TRANSFER_DISPOSITION_ACCEPTED);

    if (transmission_id > 0) {

        // only BPv7 gets in here
        BundleDaemon::instance()->add_bibe_bundle_to_acs(bibe_bundle->bp_version(), bibe_bundle->source(), 
                                                         transmission_id, success, reason);
    }

    return success;
}





} // namespace dtn
