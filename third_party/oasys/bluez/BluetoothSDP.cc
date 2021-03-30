/*
 *    Copyright 2006 Baylor University
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
#  include <oasys-config.h>
#endif

#ifdef OASYS_BLUETOOTH_ENABLED

#include "io/IO.h"

#include "Bluetooth.h"
#include "BluetoothSDP.h"

#include <cerrno>
extern int errno;

namespace oasys {

BluetoothServiceDiscoveryClient::
BluetoothServiceDiscoveryClient(const char* logpath) :
    Logger("BluetoothServiceDiscoveryClient",logpath)
{
    Bluetooth::hci_get_bdaddr(&local_addr_);
    channel_ = 0;
}

BluetoothServiceDiscoveryClient::
~BluetoothServiceDiscoveryClient()
{
}

bool
BluetoothServiceDiscoveryClient::
is_dtn_router(bdaddr_t remote)
{
    // connect to the SDP server running on the remote machine
    sdp_session_t* sess =
        sdp_connect(&local_addr_, /* bind to specified local adapter */
                    &remote,
                    0); // formerly SDP_RETRY_IF_BUSY);

    if (!sess) {
        log_debug("Failed to connect to SDP server on %s: %s\n",
                  bd2str(remote),strerror(errno));
        return false;
    }

    // specify Universally Unique Identifier of service to query for
    const uint32_t svc_uuid_int[] = OASYS_BLUETOOTH_SDP_UUID;
    uuid_t svc_uuid;

    // hi-order (0x0000) specifies start of search range;
    // lo-order (0xffff) specifies end of range;
    // 0x0000ffff specifies a search of full range of attributes
    uint32_t range = 0x0000ffff;

    // specify UUID of the application we're searching for
    sdp_uuid128_create(&svc_uuid,&svc_uuid_int);

    // initialize the linked list with UUID to limit SDP
    // search scope on remote host
    sdp_list_t *search = sdp_list_append(NULL,&svc_uuid);

    // search all attributes by specifying full range
    sdp_list_t *attrid = sdp_list_append(NULL,&range);

    // list of service records returned by search request
    sdp_list_t *seq = NULL;

    // get a list of service records that have matching UUID
    int err = sdp_service_search_attr_req(
                        sess,                /* open session handle        */
                        search,              /* define the search (UUID)   */
                        SDP_ATTR_REQ_RANGE,  /* type of search             */
                        attrid,              /* search mask for attributes */
                        &seq);               /* linked list of responses   */

    // manage the malloc()'s flying around like crazy in BlueZ
    sdp_list_free(attrid,0);
    sdp_list_free(search,0);

    if (err != 0) {
        if(sess)
            sdp_close(sess);
        log_debug("Service Search failed: %s\n",
                  strerror(errno));
        return false;
    }

    int found = 0;
    // similar to bluez-utils-2.25/tools/sdptool.c:2896
    sdp_list_t *next;
    for (; seq; seq = next) {

        sdp_record_t *record = (sdp_record_t*) seq->data;

        // pull out name, desc, provider of this record
        sdp_data_t *d = sdp_data_get(record,SDP_ATTR_SVCNAME_PRIMARY);
        if(d)
            remote_eid_.assign(d->val.str,strlen(d->val.str));

        // success returns 0
        sdp_list_t *proto; 
        if (sdp_get_access_protos(record,&proto) == 0) {

            sdp_list_t* next_proto;
            // iterate over the elements of proto_list
            // sdptool.c:1095 - sdp_list_foreach(proto, print_access_protos, 0);
            for (; proto; proto = next_proto) {

                sdp_list_t* proto_desc = (sdp_list_t*) proto->data;
                sdp_list_t* next_desc;
                // sdptool.c:1061
                // sdp_list_foreach(protDescSeq, print_service_desc, 0);
                for (; proto_desc; proto_desc = next_desc) {

                    sdp_data_t *nextp, *p =
                        (sdp_data_t*) proto_desc->data;

                    int proto = 0;

                    for (; p; p = nextp) {
                        switch(p->dtd) {
                            case SDP_UUID16:
                            case SDP_UUID32:
                            case SDP_UUID128:
                                proto = sdp_uuid_to_proto(
                                            &p->val.uuid);
                                if (proto == RFCOMM_UUID) {
                                    found++;
                                }
                                break;
                            case SDP_UINT8:
                                if (proto == RFCOMM_UUID)
                                    channel_ = p->val.uint8;
                                break;
                            default:
                                break;
                        } // switch

                        nextp = p->next;

                    } // p

                    // advance to next element
                    next_desc = proto_desc->next;
                    // free each element after visiting
                    //sdp_list_free(proto_desc,0);

                } // proto_desc

                next_proto = proto->next;
                // free each element after visiting
                //sdp_list_free(proto,0);
                // advance to next element in linked list
                proto = next_proto;

            } // proto

            next = seq->next;
            //free(seq);
            sdp_record_free(record);

        } else {
            // error, bail!
            next = 0;
            found = -1;
        } // sdp_get_access_protos

    } // for()

    sdp_close(sess);
    return (found>0);
}

BluetoothServiceRegistration::
BluetoothServiceRegistration(const char* name,
                             const char* logpath) :
    Logger("BluetoothServiceRegistration",logpath),
    sess_(NULL)
{
    Bluetooth::hci_get_bdaddr(&local_addr_);
    status_ = register_service(name);
}

BluetoothServiceRegistration::
~BluetoothServiceRegistration()
{
    if (sess_) 
        sdp_close(sess_);
}

bool
BluetoothServiceRegistration::
register_service(const char *service_name)
{
    uint32_t service_uuid_int[] = OASYS_BLUETOOTH_SDP_UUID;
    uint8_t rfcomm_channel = 10;

    uuid_t root_uuid,
           l2cap_uuid,
           rfcomm_uuid,
           svc_uuid;
    sdp_list_t *l2cap_list = 0,
               *rfcomm_list = 0,
               *root_list = 0,
               *proto_list = 0,
               *access_proto_list = 0;
    sdp_data_t *channel = 0;
    int err = -1;

    // memset(0) happens within sdp_record_alloc()
    sdp_record_t *record = sdp_record_alloc();

    // set the general service ID
    sdp_uuid128_create(&svc_uuid,&service_uuid_int);
    sdp_set_service_id(record,svc_uuid);

    // make the service record publicly browsable
    sdp_uuid16_create(&root_uuid,PUBLIC_BROWSE_GROUP);
    root_list = sdp_list_append(0,&root_uuid);
    sdp_set_browse_groups(record,root_list);

    // set l2cap information
    sdp_uuid16_create(&l2cap_uuid,L2CAP_UUID);
    l2cap_list = sdp_list_append(0,&l2cap_uuid);
    proto_list = sdp_list_append(0,l2cap_list);

    // set rfcomm information
    sdp_uuid16_create(&rfcomm_uuid,RFCOMM_UUID);
    channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
    rfcomm_list = sdp_list_append(0,&rfcomm_uuid);
    sdp_list_append(rfcomm_list, channel);
    sdp_list_append(proto_list,rfcomm_list);

    // attach protocol information to service record
    access_proto_list = sdp_list_append(0,proto_list);
    sdp_set_access_protos(record,access_proto_list);

    // set the name, provider, and description
    sdp_set_info_attr(record,service_name,0,0);

    // connect to the local SDP server, register the service record, and
    // disconnect
    sess_ = sdp_connect(&local_addr_, /* bind to specified adapter */
                        BDADDR_LOCAL, /* connect to local server */
                        SDP_RETRY_IF_BUSY);
    if (sess_ != NULL)
        err = sdp_record_register(sess_,record,0);
    else
        log_err("Failed to connect to SDP service: %s (%d)",
                strerror(errno),errno);

    // cleanup
    sdp_data_free(channel);
    sdp_list_free(l2cap_list,0);
    sdp_list_free(rfcomm_list,0);
    sdp_list_free(root_list,0);
    sdp_list_free(proto_list,0);
    sdp_list_free(access_proto_list,0);
    sdp_record_free(record);

    return (err == 0);
}

} // namespace oasys
#endif // OASYS_BLUETOOTH_ENABLED
