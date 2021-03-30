/*
 *    Copyright 2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <third_party/oasys/util/OptParser.h>
#include "BIBEConvergenceLayer.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocolVersion7.h"
#include "storage/BundleStore.h"

namespace dtn {

//----------------------------------------------------------------------
void
BIBEConvergenceLayer::Params::serialize(oasys::SerializeAction* a)
{
    a->process("bp_version", &bp_version_);
    a->process("dest_eid", &dest_eid_);
    a->process("src_eid", &src_eid_);
    a->process("custody_transfer", &custody_transfer_);
}

//----------------------------------------------------------------------
BIBEConvergenceLayer::BIBEConvergenceLayer()
  : ConvergenceLayer("BIBEConvergenceLayer", "bibe")
{
}

//----------------------------------------------------------------------
bool
BIBEConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{
    (void) iface;
    (void) argc;
    (void) argv;

    log_err("interface_up: BIBEConvergenceLayer should only be used as a Link and not as an Interface - aborting");
    return false;
}

//----------------------------------------------------------------------
CLInfo*
BIBEConvergenceLayer::new_link_params()
{
    return new BIBEConvergenceLayer::Params(defaults_);
}

//----------------------------------------------------------------------
bool
BIBEConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_link_params(&defaults_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
BIBEConvergenceLayer::parse_link_params(Params* params,
                                        int argc, const char** argv,
                                        const char** invalidp)
{
    oasys::OptParser p;
    p.addopt(new oasys::UIntOpt("bp_version", &params->bp_version_));
    p.addopt(new oasys::BoolOpt("custody_transfer", &params->custody_transfer_));
    p.addopt(new oasys::StringOpt("src_eid", &params->src_eid_));
    return p.parse(argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
BIBEConvergenceLayer::init_link(const LinkRef& link,
                                int argc, const char* argv[])
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == nullptr);
   
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // verify that the nexthop is a valid EID
    EndpointID eid(link->nexthop());
    if (! eid.valid()) {
        log_err("error parsing link options: nexthop (%s) must be specified as a valid Endpoint ID", 
                link->nexthop());
        return false;
    }

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = new Params(defaults_);

    const char* invalid;
    if (! parse_link_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    if ((params->bp_version_ != 6) && (params->bp_version_ != 7)) {
        log_err("Invalid parameter; bp_version must be 6 or 7");
        delete params;
        return false;
    }

    params->dest_eid_ = link->nexthop();

    if (params->src_eid_.empty()) {
        BundleDaemon* bd = BundleDaemon::instance();
        if(!bd->params_.announce_ipn_)
        {
            params->src_eid_ = bd->local_eid().c_str();
        } else {
            params->src_eid_ = bd->local_eid_ipn().c_str();
        }
    }
    eid.assign(params->src_eid_);
    if (! eid.valid()) {
        log_err("error parsing link options: src_eid (%s) must be specified as a valid Endpoint ID", 
                params->src_eid_.c_str());
        return false;
    }


    link->set_cl_info(params);
    return true;
}

//----------------------------------------------------------------------
bool
BIBEConvergenceLayer::reconfigure_link(const LinkRef& link,
                                       int argc, const char* argv[])
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != nullptr);
    
    Params* params = dynamic_cast<Params*>(link->cl_info());
    ASSERT(params != nullptr);
    
    Params* tmp_params = new Params(*params);


    const char* invalid;
    if (! parse_link_params(tmp_params, argc, argv, &invalid)) {
        log_err("%s: invalid parameter %s", __func__, invalid);
        delete tmp_params;
        return false;
    }

    if ((tmp_params->bp_version_ != 6) && (tmp_params->bp_version_ != 7)) {
        log_err("%s: Invalid parameter; bp_version must be 6 or 7", __func__);
        delete params;
        return false;
    }

    *params = *tmp_params;
    delete tmp_params;

    return true;
}

//----------------------------------------------------------------------
void
BIBEConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != nullptr);

    Params* params = dynamic_cast<Params*>(link->cl_info());
    ASSERT(params != nullptr);

    buf->appendf("---\n");
    buf->appendf("dest_eid: %s\n", params->dest_eid_.c_str());
    buf->appendf("src_eid: %s\n", params->src_eid_.c_str());
    buf->appendf("bp_version : %u\n", params->bp_version_);
    buf->appendf("custody_transfer: %s\n", params->custody_transfer_ ? "true" : "false");
}

//----------------------------------------------------------------------
void
BIBEConvergenceLayer::list_link_opts(oasys::StringBuffer& buf)
{
    buf.appendf("Bundle In Bundle Encapsulation Convergence Layer [%s] - valid Link options:\n\n", name());
    buf.appendf("<nexthop> format for \"link add\" command is the administrative Endpoint ID of the node to do the extraction\n\n");
    buf.appendf("CLA specific options:\n");

    buf.appendf("    bp_version <6 or 7>                - BP version to use when generating the encapsulating bundle (must be 6 or 7) \n");
    buf.appendf("    custody_transfer <Bool>            - Whether to use custody transfer (default: false)\n");
    buf.appendf("    src_eid <Endpoint ID>              - Endpoint ID to use as the source of the encapsualted bundle\n");
    buf.appendf("                                             (local administrative EID will be used if not specified)\n");
    buf.appendf("\n");
    buf.appendf("Example:\n");
    buf.appendf("link add bibe6_62 ipn:62.0 ALWAYSON bibe bp_version=6 custody_transfer=false \n");
    buf.appendf("    (create a link named \"bibe6_62\" that will encapsulate a bundle within a BP version 6 bundle with a destination of ipn:62.0\n");
    buf.appendf("     and do not flag the bundle as requesitng custody transfer)\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
void
BIBEConvergenceLayer::list_interface_opts(oasys::StringBuffer& buf)
{
    buf.appendf("Bundle In Bundle Encapsulation Convergence Layer [%s] valid Interface options:\n\n", name());
    buf.appendf("  The BIBE Convergence Layer should only be used as a Link and not as an Interface\n\n");
    buf.appendf("\n");
}

//----------------------------------------------------------------------
void
BIBEConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != nullptr);

    log_debug("deleting link %s", link->name());
    
    delete link->cl_info();
    link->set_cl_info(nullptr);
}

//----------------------------------------------------------------------
bool
BIBEConvergenceLayer::open_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());

    Params* params = dynamic_cast<Params*>(link->cl_info());
    ASSERT(params != nullptr);


    BIBE* bibe = new BIBE(contact, params);
    contact->set_cl_info(bibe);
    bibe->start();


    BundleDaemon::post(new ContactUpEvent(contact));
    return true;
}

//----------------------------------------------------------------------
bool
BIBEConvergenceLayer::close_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != nullptr);
    ASSERT(!link->isdeleted());

    BIBE* bibe  = dynamic_cast<BIBE*>(contact->cl_info());
    ASSERT(bibe != nullptr);

    bibe->set_should_stop();

    contact->set_cl_info(nullptr);

    return true;
}

//----------------------------------------------------------------------
void
BIBEConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bundle)
{
    (void) link;
    (void) bundle;
    return;
}

//----------------------------------------------------------------------
void
BIBEConvergenceLayer::cancel_bundle(const LinkRef& link, const BundleRef& bundle)
{
    // if configured to not sent bundles, and if the bundle in
    // question is still on the link queue, then it can be cancelled
    if (link->queue()->contains(bundle)) {
        log_debug("BIBEConvergenceLayer::cancel_bundle: "
                  "cancelling bundle *%p on *%p", bundle.object(), link.object());
        BundleDaemon::post(new BundleSendCancelledEvent(bundle.object(), link));
        return;
    } else {
        log_debug("BIBEConvergenceLayer::cancel_bundle: "
                  "not cancelling bundle *%p on *%p since !is_queued()",
                  bundle.object(), link.object());
    }
}




//----------------------------------------------------------------------
BIBEConvergenceLayer::BIBE::BIBE(const ContactRef& contact, Params* params)
    : Logger("BIBEConvergenceLayer::BIBE",
             "/dtn/cl/bibe/%p", this),
      Thread("BIBEConvergenceLayer::BIBE", Thread::DELETE_ON_EXIT),
      contact_(contact.object(), "BIBECovergenceLayer::BIBE"),
      link_(contact->link()),
      params_(params)
{
}

BIBEConvergenceLayer::BIBE::~BIBE()
{
}

//----------------------------------------------------------------------
void
BIBEConvergenceLayer::BIBE::run()
{
    char threadname[16] = "BIBE";
    pthread_setname_np(pthread_self(), threadname);
   
    BundleRef bref("BIBE");

    while (!should_stop()) {
        bref = link_->queue()->front();

        if (bref != nullptr) {
            encapsulate_bundle(bref);
        } else {
            usleep(100000);
        }

        bref = nullptr;
    }
}


//----------------------------------------------------------------------
void
BIBEConvergenceLayer::BIBE::encapsulate_bundle(BundleRef& bref)
{
    uint64_t block_size = 4 * 1024 * 1024; 
    uint64_t chunk_size;
    uint64_t ret;
    int64_t encoded_len = 0;
    int64_t need_bytes = 0;
    bool complete = false;

    scratch_.clear();
    scratch_.reserve(block_size);

    oasys::ScopeLock scoplok(bref->lock(), __func__);

    SPtr_BlockInfoVec blocks = bref->xmit_blocks()->find_blocks(contact_->link());

    scoplok.unlock();

    if (blocks == nullptr) {
        //dzdebug log_debug("BlockInfoVec deleted before *%p could be processed - nothing to do", bref.object());
        log_always("BlockInfoVec deleted before *%p could be processed - nothing to do", bref.object());

        BundleDaemon::post(new BundleSendCancelledEvent(bref.object(), link_));
        link_->del_from_queue(bref);
        return;
    }

    //XXX/dz TOOD: test deleting blocks from  bundle to make sure no impact to processing


    // Calculate the max bytes needed for the admin rec & BIBE overhead 
    //   1 byte for BP6 compatibility admin record of type 3 (for BP6 BIBE only - not part of RFC-5050)
    //   1 byte for BP7 admin record CBOR array of length 2  (used with BP6 BIBE also)
    //   1 byte for CBOR UInt value 3 representing admin record type 3
    //   1 byte for BIBE BPDU CBOR array of length 3
    //   9 bytes for CBOR UInt max value representing the transmission ID
    //   9 bytes for CBOR UInt max value representing the retransmission time
    uint64_t admin_header_max_len = 22;

    uint64_t formatted_len = BundleProtocol::total_length(bref.object(), blocks);
    uint64_t bstr_header_len = CborUtil::uint64_encoding_len(formatted_len);

    uint64_t max_total_len =  admin_header_max_len + formatted_len + bstr_header_len;


    // pause until able to reserve payload space for the new bundle
    BundleStore* bs = BundleStore::instance();

    params_->reserving_space_ = true;

    while (!should_stop())
    {
        if (bs->try_reserve_payload_space(max_total_len)) {
            break;
        } else {
            usleep(100000);
        }
    }
    params_->reserving_space_ = false;
        
    if (should_stop()) {
        link_->del_from_queue(bref);
        return;
    }
 

    Bundle* bibe_bundle = new Bundle(params_->bp_version_);
    BundlePayload* bibe_payload = bibe_bundle->mutable_payload();

    bibe_bundle->set_source(params_->src_eid_);
    bibe_bundle->mutable_dest()->assign(params_->dest_eid_);
    bibe_bundle->mutable_replyto()->assign(EndpointID::NULL_EID());
    bibe_bundle->mutable_custodian()->assign(EndpointID::NULL_EID());
    bibe_bundle->set_expiration_millis(bref->time_to_expiration_millis());

    bibe_bundle->set_is_admin(true);

    // Take a reference which will add the bundle to the all_bundles list
    BundleRef bibe_bref (bibe_bundle, "BIBE");

    if (params_->custody_transfer_) {
        // accept custody now so we can determine the transmission ID 
        if (bibe_bundle->is_bpv6()) {
            // If BIBE bundle is BP6 then take custody of the BIBE bundle

            ASSERT(bibe_bundle->custody_dest() == "bpv6");
            bibe_bundle->set_custody_requested(true);
            bibe_bundle->set_bibe_custody(true);
        } else {
            // if BIBE bundle is BP7 then take custody of original bundle
            // preventing it from being deleted after transmission

            // if encapsulated bundle is in local custody but the custody_dest
            // does not match this dest_eid then a new Custody ID needs to be
            // assigned
            // then we need to change the custody ID which is type "bpv6"
            // to a new Custody ID appropriate for the destination EID
            if (bref->local_custody()) {
                if (bref->custody_dest() != params_->dest_eid_) {
                    BundleDaemon::instance()->switch_bp6_to_bibe_custody(bref.object(), params_->dest_eid_);
                }
            } else {
                BundleDaemon::instance()->accept_bibe_custody(bref.object(), params_->dest_eid_);
            }
        }
    }


    // start encoding the BIBE payload
    if (bibe_bundle->is_bpv6()) {
         *scratch_.end() = (BundleProtocolVersion6::ADMIN_BUNDLE_IN_BUNDLE_ENCAP << 4);
         scratch_.incr_len(1);
    }

    // encode the BP7 admin record array size
    uint64_t uval64 = 2;
    need_bytes = cborutil_.encode_array_header(scratch_.end(), scratch_.nfree(), 
                                               0, uval64, encoded_len);
    ASSERT(need_bytes == 0);
    scratch_.incr_len(encoded_len);
    
    // encode the BP7 admin record type 3
    uval64 = BundleProtocolVersion7::ADMIN_BUNDLE_IN_BUNDLE_ENCAP;
    need_bytes = cborutil_.encode_uint64(scratch_.end(), scratch_.nfree(), 
                                         0, uval64, encoded_len);
    ASSERT(need_bytes == 0);
    scratch_.incr_len(encoded_len);
    
    // encode the BIBE BPDU array size
    uval64 = 3;
    need_bytes = cborutil_.encode_array_header(scratch_.end(), scratch_.nfree(), 
                                               0, uval64, encoded_len);
    ASSERT(need_bytes == 0);
    scratch_.incr_len(encoded_len);
    
    // encode the transmission ID and retransmission time
    uint64_t transmission_id = 0;
    uint64_t retransmit_time = 0;
    if (params_->custody_transfer_) {
        if (bibe_bundle->is_bpv6()) {
            transmission_id = 0; // BPv6 ACS handled separately
        } else{
            transmission_id = bref->custodyid();
        }

        //XXX/dz - hard coded to 15 minutes for now
        //         TODO: better determine the retransmit time
        retransmit_time = BundleTimestamp::get_current_time() + (15 * 60);
    }
    need_bytes = cborutil_.encode_uint64(scratch_.end(), scratch_.nfree(), 
                                         0, transmission_id, encoded_len);
    ASSERT(need_bytes == 0);
    scratch_.incr_len(encoded_len);

    need_bytes = cborutil_.encode_uint64(scratch_.end(), scratch_.nfree(), 
                                         0, retransmit_time, encoded_len);
    ASSERT(need_bytes == 0);
    scratch_.incr_len(encoded_len);
    
    // Now ready to add the cbor encoded bundle as the 3rd element of the BIBE BDU array

    // encode the CBOR byte string length
    need_bytes = cborutil_.encode_byte_string_header(scratch_.end(), bstr_header_len, 
                                                     0, formatted_len, encoded_len);
    ASSERT(need_bytes == 0);
    ASSERT((uint64_t)encoded_len == bstr_header_len);

    scratch_.incr_len(bstr_header_len);

    // loop adding formatted bytes to the payload in block_size chunks
    encoded_len = 0;
    while (formatted_len > 0) {
        chunk_size = block_size - scratch_.len();

        chunk_size = std::min(chunk_size, formatted_len);

        ret = BundleProtocol::produce(bref.object(), blocks, scratch_.end(), 
                                      encoded_len, chunk_size, &complete);

        ASSERT(ret == chunk_size);

        scratch_.incr_len(ret);
        encoded_len += ret;
        formatted_len -= ret;

        bibe_payload->append_data(scratch_.buf(), scratch_.len());

        scratch_.clear();
    }
    ASSERT(complete == true);
    ASSERT(max_total_len >= bibe_bundle->payload().length());

    // bundle already approved to use payload space
    bs->reserve_payload_space(bibe_bundle);

    // release the temp reserved space since the bundle now has the actually space reserved
    bs->release_payload_space(max_total_len);


    BundleDaemon::post(new BundleReceivedEvent(bibe_bundle, EVENTSRC_ADMIN));

    link_->del_from_queue(bref);
    link_->add_to_inflight(bref);

    BundleDaemon::post(new BundleTransmittedEvent(bref.object(), contact_, link_, encoded_len, encoded_len, true, true));

    if (params_->bp_version_ == 6) {
        BundleDaemon::instance()->inc_bibe6_encapsulations();
    } else {
        BundleDaemon::instance()->inc_bibe7_encapsulations();
    }
}


} // namespace dtn
