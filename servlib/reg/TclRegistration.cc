/*
 *    Copyright 2004-2006 Intel Corporation
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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#include <climits>
#include <oasys/serialize/TclListSerialize.h>
#include <oasys/util/ScratchBuffer.h>

#include "TclRegistration.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleStatusReport.h"
#include "bundling/CustodySignal.h"
#include "storage/GlobalStore.h"

namespace dtn {

TclRegistration::TclRegistration(const EndpointIDPattern& endpoint,
                                 Tcl_Interp* interp)
    
    : Registration(GlobalStore::instance()->next_regid(),
                   endpoint, Registration::DEFER, 0, 0) // XXX/demmer expiration??
{
    (void)interp;
    
    logpathf("/dtn/registration/tcl/%d", regid_);
    set_active(true);

    log_info("new tcl registration on endpoint %s", endpoint.c_str());

    bundle_list_ = new BlockingBundleList(logpath_);
    int fd = bundle_list_->notifier()->read_fd();
    intptr_t fd_as_ptr_size = (intptr_t) fd;
    notifier_channel_ = oasys::TclCommandInterp::instance()->
                        register_file_channel((ClientData)fd_as_ptr_size, TCL_READABLE);
    log_debug("notifier_channel_ is %p", notifier_channel_);
}

void
TclRegistration::deliver_bundle(Bundle* bundle)
{
    bundle_list_->push_back(bundle);
}

int
TclRegistration::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    oasys::TclCommandInterp* cmdinterp = oasys::TclCommandInterp::instance();
    if (argc < 1) {
        cmdinterp->wrong_num_args(argc, argv, 0, 1, INT_MAX);
        return TCL_ERROR;
    }
    const char* op = argv[0];

    if (strcmp(op, "get_list_channel") == 0)
    {
        return get_list_channel(interp);
    }
    else if (strcmp(op, "get_bundle_data") == 0)
    {
        return get_bundle_data(interp);
    }
    else
    {
        cmdinterp->resultf("invalid operation '%s'", op);
        return TCL_ERROR;
    }
}

int
TclRegistration::get_list_channel(Tcl_Interp* interp)
{
    (void)interp;
    oasys::TclCommandInterp* cmdinterp = oasys::TclCommandInterp::instance();
    cmdinterp->set_result(Tcl_GetChannelName(notifier_channel_));
    return TCL_OK;
}


int
TclRegistration::get_bundle_data(Tcl_Interp* interp)
{
    oasys::TclCommandInterp* cmdinterp = oasys::TclCommandInterp::instance();
    BundleRef b("TclRegistration::get_bundle_data temporary");
    b = bundle_list_->pop_front();
    if (b == NULL) {
        cmdinterp->set_objresult(Tcl_NewListObj(0, 0));
        return TCL_OK; // empty list
    }

    Tcl_Obj* result;
    int ok = parse_bundle_data(interp, b, &result);
    cmdinterp->set_objresult(result);

    if (ok != TCL_OK) {
        return ok;
    }
    
    BundleDaemon::post(new BundleDeliveredEvent(b.object(), this));
    return TCL_OK;
    
}

/**
 * Return a Tcl list key-value pairs detailing bundle contents to a
 * registered procedure each time a bundle arrives. The returned TCL
 * list is suitable for assigning to an array, e.g.
 *     array set b $bundle_info
 *
 * Using the TclListSerialize class, all fields that are serialized in
 * Bundle::serialize will be present in the returned tcl list.
 *
 * ADMIN-BUNDLE-ONLY KEY-VALUE PAIRS:
 *
 * admin_type       : the Admin Type (the following pairs are only defined
 *                    if the admin_type = "Stauts Report")
 * reason_code      : Reason Code string
 * orig_creation_ts : creation timestamp of original bundle
 * orig_source      : EID of the original bundle's source
 *
 * ADMIN-BUNDLE-ONLY OPTIONAL KEY-VALUE PAIRS:
 *
 * orig_frag_offset : Offset of fragment
 * orig_frag_length : Length of original bundle
 *
 * STATUS-REPORT-ONLY OPTIONAL KEY-VALUE PAIRS
 *
 * (Note that the presence of timestamp keys implies a corresponding
 * flag has been set true. For example if forwarded_time is returned
 * the Bundle Forwarded status flag was set; if frag_offset and
 * frag_length are returned the ACK'ed bundle was fragmented.)
 *
 * sr_reason                  : status report reason code
 * sr_received_time           : bundle reception timestamp
 * sr_custody_time            : bundle custody transfer timestamp
 * sr_forwarded_time          : bundle forwarding timestamp
 * sr_delivered_time          : bundle delivery timestamp
 * sr_deletion_time           : bundle deletion timestamp
 *
 * CUSTODY-SIGNAL-ONLY OPTIONAL KEY-VALUE PAIRS
 *
 * custody_succeeded          : boolean if custody transfer succeeded
 * custody_reason             : reason information
 * custody_signal_time        : custody transfer time
 */
int
TclRegistration::parse_bundle_data(Tcl_Interp* interp,
                                   const BundleRef& b,
                                   Tcl_Obj** result)
{
    // Using the tcl based serializer, grab all the serialized
    // metadata fields into a new list object
    Tcl_Obj* objv = Tcl_NewListObj(0, 0);
    oasys::TclListSerialize s(interp, objv,
                              oasys::Serialize::CONTEXT_LOCAL,
                              0);
    b->serialize(&s);

    // read in all the payload data (XXX/demmer this will not be nice
    // for big bundles)
    size_t payload_len = b->payload().length();
    oasys::ScratchBuffer<u_char*> payload_buf;
    const u_char* payload_data = (const u_char*)"";
    if (payload_len != 0) {
        payload_data = b->payload().read_data(0, payload_len, 
                                              payload_buf.buf(payload_len));
    }

    char tmp_buf[128];              // used for sprintf strings

#define addElement(e)                                                      \
    if (Tcl_ListObjAppendElement(interp, objv, (e)) != TCL_OK) {           \
        *result = Tcl_NewStringObj("Tcl_ListObjAppendElement failed", -1); \
        return TCL_ERROR;                                                  \
    }
    
    // stick in the payload
    addElement(Tcl_NewStringObj("payload_len", -1));
    addElement(Tcl_NewIntObj(payload_len));
    
    addElement(Tcl_NewStringObj("payload_data", -1));
    addElement(Tcl_NewByteArrayObj(const_cast<u_char*>(payload_data), 
                                   payload_len));

    // and a pretty formatted creation timestamp
    addElement(Tcl_NewStringObj("creation_ts", -1));
    sprintf(tmp_buf, "%" PRIu64 ".%" PRIu64, b->creation_ts().seconds_, b->creation_ts().seqno_);
    addElement(Tcl_NewStringObj(tmp_buf, -1));

    // If we're not an admin bundle, we're done
    if (!b->is_admin()) {
        goto done;
    }

    // Admin Type:
    addElement(Tcl_NewStringObj("admin_type", -1));
    BundleProtocol::admin_record_type_t admin_type;
    if (!BundleProtocol::get_admin_type(b.object(), &admin_type)) {
        goto done;
    }

    // Now for each type of admin bundle, first append the string to
    // define that type, then all the relevant fields of the type
    switch (admin_type) {
    case BundleProtocol::ADMIN_STATUS_REPORT:
    {
        addElement(Tcl_NewStringObj("Status Report", -1));

        BundleStatusReport::data_t sr;
        if (!BundleStatusReport::parse_status_report(&sr, payload_data,
                                                     payload_len)) {
            *result =
                Tcl_NewStringObj("Admin Bundle Status Report parsing failed", -1);
            return TCL_ERROR;
        }

        // Fragment fields
        if (sr.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT) {
            addElement(Tcl_NewStringObj("orig_frag_offset", -1));
            addElement(Tcl_NewLongObj(sr.orig_frag_offset_));
            addElement(Tcl_NewStringObj("orig_frag_length", -1));
            addElement(Tcl_NewLongObj(sr.orig_frag_length_));
        }

        // Status fields with timestamps:
#define APPEND_TIMESTAMP(_flag, _what, _field)                          \
        if (sr.status_flags_ & BundleStatusReport::_flag) {             \
            addElement(Tcl_NewStringObj(_what, -1));                    \
            sprintf(tmp_buf, "%" PRIu64 ".%" PRIu64,                               \
                    sr._field.seconds_, sr._field.seqno_);              \
            addElement(Tcl_NewStringObj(tmp_buf, -1));                  \
        }

        APPEND_TIMESTAMP(STATUS_RECEIVED,
                         "sr_received_time", receipt_tv_);
        APPEND_TIMESTAMP(STATUS_CUSTODY_ACCEPTED,
                         "sr_custody_time",      custody_tv_);
        APPEND_TIMESTAMP(STATUS_FORWARDED,
                         "sr_forwarded_time",    forwarding_tv_);
        APPEND_TIMESTAMP(STATUS_DELIVERED,
                         "sr_delivered_time",    delivery_tv_);
        APPEND_TIMESTAMP(STATUS_DELETED,
                         "sr_deleted_time",      deletion_tv_);
        APPEND_TIMESTAMP(STATUS_ACKED_BY_APP,
                         "sr_acked_by_app_time", ack_by_app_tv_);
#undef APPEND_TIMESTAMP
        
        // Reason Code:
        addElement(Tcl_NewStringObj("sr_reason", -1));
        addElement(Tcl_NewStringObj(BundleStatusReport::reason_to_str(sr.reason_code_), -1));
        
        // Bundle creation timestamp
        addElement(Tcl_NewStringObj("orig_creation_ts", -1));
        sprintf(tmp_buf, "%" PRIu64 ".%" PRIu64,
                sr.orig_creation_tv_.seconds_,
                sr.orig_creation_tv_.seqno_);
        addElement(Tcl_NewStringObj(tmp_buf, -1));

        // Status Report's Source EID:
        addElement(Tcl_NewStringObj("orig_source", -1));
        addElement(Tcl_NewStringObj(sr.orig_source_eid_.data(),
                                    sr.orig_source_eid_.length()));
        break;
    }

    //-------------------------------------------

    case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
    {
        addElement(Tcl_NewStringObj("Custody Signal", -1));

        CustodySignal::data_t cs;
        if (!CustodySignal::parse_custody_signal(&cs, payload_data,
                                                 payload_len))
        {
            *result = Tcl_NewStringObj("Admin Custody Signal parsing failed", -1);
            return TCL_ERROR;
        }

        // Fragment fields
        if (cs.admin_flags_ & BundleProtocol::ADMIN_IS_FRAGMENT) {
            addElement(Tcl_NewStringObj("orig_frag_offset", -1));
            addElement(Tcl_NewLongObj(cs.orig_frag_offset_));
            addElement(Tcl_NewStringObj("orig_frag_length", -1));
            addElement(Tcl_NewLongObj(cs.orig_frag_length_));
        }
        
        addElement(Tcl_NewStringObj("custody_succeeded", -1));
        addElement(Tcl_NewBooleanObj(cs.succeeded_));

        addElement(Tcl_NewStringObj("custody_reason", -1));
        switch(cs.reason_) {
        case BundleProtocol::CUSTODY_NO_ADDTL_INFO:
            addElement(Tcl_NewStringObj("No additional information.", -1));
            break;
            
        case BundleProtocol::CUSTODY_REDUNDANT_RECEPTION:
            addElement(Tcl_NewStringObj("Redundant bundle reception.", -1));
            break;
            
        case BundleProtocol::CUSTODY_DEPLETED_STORAGE:
            addElement(Tcl_NewStringObj("Depleted Storage.", -1));
            break;
            
        case BundleProtocol::CUSTODY_ENDPOINT_ID_UNINTELLIGIBLE:
            addElement(Tcl_NewStringObj("Destination endpoint ID unintelligible.", -1));
            break;
            
        case BundleProtocol::CUSTODY_NO_ROUTE_TO_DEST:
            addElement(Tcl_NewStringObj("No known route to destination from here", -1));
            break;
            
        case BundleProtocol::CUSTODY_NO_TIMELY_CONTACT:
            addElement(Tcl_NewStringObj("No timely contact with next node en route.", -1));
            break;
            
        case BundleProtocol::CUSTODY_BLOCK_UNINTELLIGIBLE:
            addElement(Tcl_NewStringObj("Block unintelligible.", -1));
            break;
            
        default:
            sprintf(tmp_buf, "Error: Unknown Custody Signal Reason Code 0x%x",
                    cs.reason_);
            addElement(Tcl_NewStringObj(tmp_buf, -1));
            break;
        }

        // Custody signal timestamp
        addElement(Tcl_NewStringObj("custody_signal_time", -1));
        sprintf(tmp_buf, "%" PRIu64 ".%" PRIu64,
                cs.custody_signal_tv_.seconds_,
                cs.custody_signal_tv_.seqno_);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
        
        // Bundle creation timestamp
        addElement(Tcl_NewStringObj("orig_creation_ts", -1));
        sprintf(tmp_buf, "%" PRIu64 ".%" PRIu64,
                cs.orig_creation_tv_.seconds_,
                cs.orig_creation_tv_.seqno_);
        addElement(Tcl_NewStringObj(tmp_buf, -1));

        // Original source eid
        addElement(Tcl_NewStringObj("orig_source", -1));
        addElement(Tcl_NewStringObj(cs.orig_source_eid_.data(),
                                    cs.orig_source_eid_.length()));
        break;
    }

    //-------------------------------------------

    default:
        sprintf(tmp_buf,
                "Error: Unknown Status Report Type 0x%x", admin_type);
        addElement(Tcl_NewStringObj(tmp_buf, -1));
        break;
    }

    // all done
 done:
    *result = objv;
    return TCL_OK;
}

} // namespace dtn
