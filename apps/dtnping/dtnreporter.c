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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "dtn_api.h"
#include "dtnping.h"

const char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;


void
usage()
{
    fprintf(stderr, "usage: %s [-a api_IP] [-b api_port] eid\n",
            progname);
    exit(1);
}

void doOptions(int argc, const char **argv);

int expiration = 30;
int wait_after_done = 0;
char register_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char replyto_eid_str[DTN_MAX_ENDPOINT_ID] = "";


int
main(int argc, const char** argv)
{
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t register_eid;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t ping_spec;
    dtn_bundle_spec_t reply_spec;
    // dtn_bundle_payload_t ping_payload;
    // ping_payload_t payload_contents;
    // ping_payload_t recv_contents;
    dtn_bundle_payload_t reply_payload;
    dtn_bundle_status_report_t* sr_data;
    // dtn_bundle_id_t bundle_id;
    int debug = 1;
    char demux[64];
    // int dest_len = 0;
    // struct timeval send_time, now;
    struct timeval now;
    //u_int32_t nonce;
    //int done;
    time_t clock;
    struct tm* tm_buf;
    
    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    doOptions(argc, argv);

    memset(&ping_spec, 0, sizeof(ping_spec));

    gettimeofday(&now, 0);
    srand(now.tv_sec);
    //nonce = rand();
    
    // open the ipc handle
    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }

    // if the user specified a registration eid, register on it.
    // otherwise, build a local eid based on the configuration of
    // our dtn router plus the demux string
    snprintf(demux, sizeof(demux), "/dtnReportLogger");
    if (register_eid_str[0] != '\0') {
        if (dtn_parse_eid_string(&register_eid, register_eid_str)) {
            fprintf(stderr, "invalid eid string '%s'\n",
                    register_eid_str);
            exit(1);
        }
    } else {
        dtn_build_local_eid(handle, &register_eid, demux);
    }

    // now create a new registration
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &register_eid);
    reginfo.flags = DTN_REG_DROP;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.expiration = 0;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }    
    if (debug) printf("dtn_register %s succeeded, regid %d\n",
        register_eid_str, regid);

    memset(&reply_spec, 0, sizeof(reply_spec));
    memset(&reply_payload, 0, sizeof(reply_payload));

    // now loop waiting for replies / status reports until we're done
    //done = 0;
    while (1) {
        // int timeout = done ? wait_after_done * 1000 : -1;
        int timeout = 5000;
        if ((ret = dtn_recv(handle, &reply_spec,
                            DTN_PAYLOAD_MEM, &reply_payload, timeout)) < 0)
        {
            if ( dtn_errno(handle) == DTN_ETIMEOUT) {
	        printf("dtn_recv timed out, going again.\n");
                continue;
            }
            fprintf(stderr, "error getting ping reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        gettimeofday(&now, 0);
            
        if (reply_payload.status_report != NULL)
        {
            sr_data = reply_payload.status_report;


            if (sr_data->flags & STATUS_RECEIVED)
            {
                clock = sr_data->receipt_ts.secs + DTNTIME_OFFSET;
                tm_buf = gmtime(&clock);
                printf("%s: received bundle %s at %.*s UTC\n",
                       reply_spec.source.uri,
		       "FOO",
		       24, asctime(tm_buf));
            }
            if (sr_data->flags & STATUS_FORWARDED)
            {
                clock = sr_data->forwarding_ts.secs + DTNTIME_OFFSET;
                tm_buf = gmtime(&clock);
                printf("%s: forwarded at %.*s UTC\n",
                       reply_spec.source.uri, 24, asctime(tm_buf));
            }
            if (sr_data->flags & STATUS_DELIVERED)
            {
		dtn_endpoint_id_t *theSource = &(sr_data->bundle_id.source);
		dtn_timestamp_t *theCreationTimestamp = &(sr_data->bundle_id.creation_ts);

                clock = sr_data->delivery_ts.secs + DTNTIME_OFFSET;
                tm_buf = gmtime(&clock);
                printf("%s: delivered bundle (%s %"PRIu64".%"PRIu64") at %.*s UTC\n",
                       reply_spec.source.uri,
		       theSource->uri, theCreationTimestamp->secs, theCreationTimestamp->seqno,
		       24, asctime(tm_buf));
            }
            if (sr_data->flags & STATUS_DELETED)
            {
                clock = sr_data->deletion_ts.secs + DTNTIME_OFFSET;
                tm_buf = gmtime(&clock);
                printf("%s: deleted at %.*s UTC\n",
                       reply_spec.source.uri, 24, asctime(tm_buf));
                break;
            }
        }
        else {
	    // Got something else
	    continue;
        }
        
        dtn_free_payload(&reply_payload);
    }
    
    dtn_close(handle);
    
    return 0;
}

void
doOptions(int argc, const char **argv)
{
    int c;

    progname = argv[0];

    // Default value

    while ( (c=getopt(argc, (char **) argv, "A:B:he:d:s:r:w:")) !=EOF ) {
        switch (c) {
        case 'A':
            api_IP_set = 1;
            api_IP = optarg;
            break;
        case 'B':
            api_IP_set = 1;
            api_port = atoi(optarg);
            break;    
        case 'e':
            expiration = atoi(optarg);
            break;
        case 'd':
            strcpy(register_eid_str, optarg);
            break;
        case 'h':
            usage();
            break;
        case 'w':
            wait_after_done = atoi(optarg);
            break;
        default:
            break;
        }
    }

    if ((optind < argc) && (strlen(register_eid_str) == 0)) {
        strcpy(register_eid_str, argv[optind++]);
    }

    if (optind < argc) {
        fprintf(stderr, "unsupported argument '%s'\n", argv[optind]);
        exit(1);
    }

    if ( 0 && register_eid_str[0] == '\0') {
        fprintf(stderr, "must supply a destination eid (or 'localhost')\n");
        exit(1);
    }
}

