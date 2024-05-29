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

#ifdef DTPC_ENABLED

#include "bundling/BundleDaemon.h"
#include "bundling/SDNV.h"
#include "storage/GlobalStore.h"

#include "DtpcDaemon.h"
#include "DtpcReceiver.h"

namespace dtn {


//----------------------------------------------------------------------
DtpcReceiver::DtpcReceiver()
    : Thread("DtpcReceiver", DELETE_ON_EXIT),
      Logger("DtpcReceiver", "/dtpc/receiver"),
      bindings_(nullptr),
      notifier_("/dtpc/receiver/notifier")
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
DtpcReceiver::~DtpcReceiver()
{
    SPtr_Registration sptr_reg; 
    while (! bindings_->empty()) {
        sptr_reg = bindings_->front();
        bindings_->pop_front();
    }    

    delete bindings_;
}

//----------------------------------------------------------------------
void
DtpcReceiver::do_init()
{
    bindings_ = new RegistrationList();
}

//----------------------------------------------------------------------
void
DtpcReceiver::get_stats(oasys::StringBuffer* buf)
{
    (void) buf;
}

//----------------------------------------------------------------------
void
DtpcReceiver::reset_stats()
{
    memset(&stats_, 0, sizeof(stats_));
}

//----------------------------------------------------------------------
void
DtpcReceiver::run()
{
    char threadname[16] = "DtpcReceiver";
    pthread_setname_np(pthread_self(), threadname);
   
    // XXX/dz check for prior existence of a registration? (on reload) 
    // Move to a bind method for on the fly [un]binds?
    u_int32_t regid = GlobalStore::instance()->next_regid(); 
    SPtr_EIDPattern sptr_pattern = BD_MAKE_PATTERN(DtpcDaemon::instance()->local_eid()->str());
    APIRegistration* api_reg = new APIRegistration(regid, sptr_pattern,
                                               Registration::DEFER, Registration::NEW, 
                                               0, 0, false, 0, "");
    api_reg->set_reg_list_type_str("DtpcReceiverReg");

    SPtr_Registration sptr_reg(api_reg);
    bindings_->push_back(sptr_reg);
    api_reg->set_active(true);

    RegistrationAddedEvent* event_to_post;
    event_to_post = new RegistrationAddedEvent(sptr_reg, EVENTSRC_APP);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post_and_wait(sptr_event_to_post, &notifier_);

    u_int32_t  timeout = 1000;

    while (1) {
        if (should_stop()) {
            log_debug("DtpcReceiver: stopping");
            break;
        }

        int err = wait_for_notify("run-recv", timeout, sptr_reg);
        if (0 != err) {
            continue;
        }

        // must have a registration with a bundle ready
        ASSERT(sptr_reg != nullptr);
        receive_bundle(sptr_reg);
    }
}

//----------------------------------------------------------------------
int
DtpcReceiver::wait_for_notify(const char*       operation,
                              u_int32_t         dtn_timeout,
                              SPtr_Registration& sptr_recv_ready_reg)
{
    APIRegistration* api_reg;

    if (bindings_->empty()) {
        log_err("wait_for_notify(%s): no bound registrations", operation);
        return -2;
    }

    int timeout = (int)dtn_timeout;
    if (timeout < -1) {
        log_err("wait_for_notify(%s): "
                "invalid timeout value %d", operation, timeout);
        return -2;
    }

    // try to optimize by using a statically sized pollfds array,
    // otherwise we need to malloc the array.
    //
    // XXX/demmer this would be cleaner by tweaking the
    // StaticScratchBuffer class to be handle arrays of arbitrary
    // sized structs
    struct pollfd static_pollfds[64];
    struct pollfd* pollfds;
    oasys::ScopeMalloc pollfd_malloc;
    size_t npollfds = 0;
    npollfds += bindings_->size();
    
    if (npollfds <= 64) {
        pollfds = &static_pollfds[0];
    } else {
        pollfds = (struct pollfd*)malloc(npollfds * sizeof(struct pollfd));
        pollfd_malloc = pollfds;
    }
    
    // loop through all the registrations -- if one has bundles on its
    // list, we don't need to poll, just return it immediately.
    // otherwise we'll need to poll it
    RegistrationList::iterator iter;
    unsigned int i = 0;

    SPtr_Registration tmp_sptr_reg;

    for (iter = bindings_->begin(); iter != bindings_->end(); ++iter) {
        tmp_sptr_reg = *iter;
        api_reg = dynamic_cast<APIRegistration*>(tmp_sptr_reg.get());
        if (api_reg == nullptr) {
            log_err("Error casting registration as an APIRegistration");
            return -2;
        }

        
        if (api_reg && !api_reg->bundle_list()->empty()) {
            //log_debug("wait_for_notify(%s): "
            //          "immediately returning bundle for reg %d",
            //          operation, api_reg->regid());
            sptr_recv_ready_reg = tmp_sptr_reg;
            return 0;
        }
    
        pollfds[i].fd = api_reg->bundle_list()->notifier()->read_fd();
        pollfds[i].events = POLLIN;
        pollfds[i].revents = 0;
        ++i;
        ASSERT(i <= npollfds);
    }

    if (timeout == 0) {
        //log_debug("wait_for_notify(%s): "
        //          "no ready registrations and timeout=%d, returning immediately",
        //          operation, timeout);
        return -1;
    }
    
    //log_debug("wait_for_notify(%s): "
    //          "blocking to get events from %zu sources (timeout %d)",
    //          operation, npollfds, timeout);
    int nready = oasys::IO::poll_multiple(&pollfds[0], npollfds, timeout,
                                          nullptr, logpath_);

    if (nready == oasys::IOTIMEOUT) {
        //log_debug("wait_for_notify(%s): timeout waiting for events",
        //          operation);
        return -1;

    } else if (nready <= 0) {
        log_err("wait_for_notify(%s): unexpected error polling for events",
                operation);
        return -3;
    }

    // otherwise, there should be data on one (or more) bundle lists, so
    // scan the list to find the first one.
    //log_debug("wait_for_notify: looking for data on bundle lists: recv_ready_reg(%p)\n",
    //          recv_ready_reg);
    for (iter = bindings_->begin(); iter != bindings_->end(); ++iter) {
        tmp_sptr_reg = *iter;
        api_reg = dynamic_cast<APIRegistration*>(tmp_sptr_reg.get());

        if (api_reg && !api_reg->bundle_list()->empty()) {
            //log_debug("wait_for_notify: found one %p", api_reg);
            sptr_recv_ready_reg = tmp_sptr_reg;
            break;
        }
    }

    if (!sptr_recv_ready_reg)
    {
        log_err("wait_for_notify(%s): error -- no events",
                operation);
        return -3;
    }
    
    return 0;
}

//----------------------------------------------------------------------
void
DtpcReceiver::receive_bundle(SPtr_Registration& sptr_reg)
{
    BundleRef bref("receive_bundle");

    APIRegistration* api_reg = dynamic_cast<APIRegistration*>(sptr_reg.get());
    if (api_reg == nullptr) {
        log_err("Error casting registration as an APIRegistration");
        return;
    }

    bref = api_reg->deliver_front();
    Bundle* bundle = bref.object();
    ASSERT(bundle != nullptr);

    bool is_valid = false;
    DtpcProtocolDataUnit* pdu = nullptr;
    size_t payload_len = bundle->payload().length();

    do {
        if (0 == payload_len) {
            // no payload - abort processing
            log_err("Bundle (*%p) has zero length payload "
                    "- abort DTPC processing", bundle);
            break;
        } 

        //XXX/dz - deletion report is received if a bundle is still in local custody
        //when it expires - can we determine the bundle is not a DTPC bundle?
        // --- administrative bit set??


        // assume that this is a data PDU so we will instantiate a PDU to
        // get an in-memory copy of the bundle payload in case it is in a file
        DtpcPayloadBuffer* buf = new DtpcPayloadBuffer(payload_len);
    
        const u_char* payload_buf =
            bundle->payload().read_data(0, payload_len, (u_char*)buf->tail_buf(payload_len));
        buf->incr_len(payload_len);
    
        pdu = new DtpcProtocolDataUnit(bundle->source(), 0, 0);
        pdu->set_buf(buf);

        // First byte of the payload should be the flags field:
        //     Version number    2 bits   - expect 00
        //     Reserved          5 bits   - should be zeros - not checked
        //     PDU Type          1 bit    - 0 = data  1 = ACK
        size_t payload_idx = 0;
        int version = (payload_buf[payload_idx] & DtpcProtocolDataUnit::DTPC_VERSION_MASK) >> 
                         DtpcProtocolDataUnit::DTPC_VERSION_BIT_SHIFT;
        if (version != DtpcProtocolDataUnit::DTPC_VERSION_NUMBER) {
            log_warn("Bundle (*%p) has an unsupported DTPC version (%d) "
                     "- abort DTPC processing", bundle, version);
            break;
        }
        bool is_ack = (DtpcProtocolDataUnit::DTPC_PDU_TYPE_ACK ==
                          ((payload_buf[payload_idx] & DtpcProtocolDataUnit::DTPC_PDU_TYPE_ACK)));
        ++payload_idx;
        --payload_len;

        // next field is the Profile ID as an SDNV
        int num_bytes;
        u_int32_t profile_id;
        num_bytes = SDNV::decode(&payload_buf[payload_idx], payload_len, &profile_id);
        if (-1 == num_bytes) {
            log_err("Bundle (*%p) has invalid Profile ID SDNV "
                    "- abort DTPC processing", bundle);
            break;
        }
        payload_idx += num_bytes;
        payload_len -= num_bytes;

        // next field is the Payload Sequence Number as an SDNV
        u_int64_t seq_ctr;
        num_bytes = SDNV::decode(&payload_buf[payload_idx], payload_len, &seq_ctr);
        if (-1 == num_bytes) {
            log_err("Bundle (*%p) has invalid Payload Sequence Number SDNV "
                    "- abort DTPC processing", bundle);
            break;
        }
        payload_idx += num_bytes;
        payload_len -= num_bytes;

        if (is_ack) {
            // if this is an ACK then the bundle payload should be fully consumed
            if (payload_len > 0) {
                log_err("Bundle (*%p) is a DTPC ACK with data after the header fields "
                        "- abort DTPC processing", bundle);
                break;
            }

            log_debug("posting DtpcAckReceivedEvent - EID: %s, Profile: %" PRIu32 " SeqCtr: %" PRIu64,
                      bundle->source().c_str(), profile_id, seq_ctr);

            // post a DTPC ACK received event and clean up
            is_valid = true;
            pdu->set_profile_id(profile_id);
            pdu->set_seq_ctr(seq_ctr);
            DtpcAckReceivedEvent* event = new DtpcAckReceivedEvent(pdu);
            SPtr_BundleEvent sptr_event(event);
            DtpcDaemon::post_at_head(sptr_event);
            break;
        } 

        // Data PDU - make a pass through the remaining data to validate structure
        pdu->set_topic_block_index(payload_idx); // save the pointer to the first Topic Block

        is_valid = true;
        // loop through the topics
        while (is_valid && payload_len > 0) {
            // make sure we have a valid Topic ID SDNV but we won't check the actual value here
            u_int32_t topic_id;
            num_bytes = SDNV::decode(&payload_buf[payload_idx], payload_len, &topic_id);
            if (-1 == num_bytes) {
                log_err("Bundle (*%p) has invalid Topic ID SDNV "
                        "- abort DTPC processing", bundle);
                is_valid = false;
                break;
            }
            payload_idx += num_bytes;
            payload_len -= num_bytes;

            // the Payload Record Count is an SDNV defining the number of 
            // Application Data Items that follow
            u_int64_t rec_count;
            num_bytes = SDNV::decode(&payload_buf[payload_idx], payload_len, &rec_count);
            if (-1 == num_bytes) {
                log_err("Bundle (*%p) has invalid Payload Record Count SDNV "
                        "- abort DTPC processing", bundle);
                is_valid = false;
                break;
            }
            if (0 == rec_count) {
                log_err("Bundle (*%p) has invalid Topic Block with zero records "
                        "- abort DTPC processing", bundle);
                is_valid = false;
                break;
            }
            payload_idx += num_bytes;
            payload_len -= num_bytes;

            // loop through the Application Data Items for this topic
            while (rec_count > 0) {
                size_t adi_len;
                num_bytes = SDNV::decode(&payload_buf[payload_idx], payload_len, &adi_len);
                if (-1 == num_bytes) {
                    log_err("Bundle (*%p) has invalid Application Data Item Length SDNV "
                            "- abort DTPC processing", bundle);
                    is_valid = false;
                    break;
                }
                payload_idx += num_bytes;
                payload_len -= num_bytes;

                if (adi_len > payload_len) {
                    log_err("Bundle (*%p) has invalid Application Data Item structure "
                            "(ADI Len: %zu, Payload Remaining: %zu) "
                            "- abort DTPC processing", bundle, adi_len, payload_len);
                    is_valid = false;
                    break;
                } else {
                    payload_idx += adi_len;
                    payload_len -= adi_len;
                    --rec_count;
                }
            }
        }

        if (is_valid) {
            // pass on the valid DPDU structure for further processing
            pdu->set_profile_id(profile_id);
            pdu->set_seq_ctr(seq_ctr);
            pdu->set_local_eid(bundle->dest());
            pdu->set_app_ack(seq_ctr > 0); // CCSDS 734.2-R3 uses seq ctr instead of App Ack flag
            pdu->set_creation_ts(bundle->creation_time_secs());
            u_int64_t exp = BundleTimestamp::TIMEVAL_CONVERSION_SECS + 
                            bundle->creation_time_secs() + 
                            bundle->expiration_secs();
            pdu->set_expiration_ts(exp);  // actual expiration time

            DtpcDataReceivedEvent* event = new DtpcDataReceivedEvent(pdu, bref);
            SPtr_BundleEvent sptr_event(event);
            DtpcDaemon::post(sptr_event);
        }
    } while (false);

    // clean up
    if (!is_valid) {
        delete pdu;
    }

    BundleDeliveredEvent* event_to_post;
    event_to_post = new BundleDeliveredEvent(bundle, sptr_reg);
    SPtr_BundleEvent sptr_event_to_post(event_to_post);
    BundleDaemon::post(sptr_event_to_post);
}

} // namespace dtn

#endif // DTPC_ENABLED
