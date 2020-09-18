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
    fprintf(stderr, "usage: %s [-A api_IP] [-B api_port] [-c count] [-i interval] [-e expiration] [-w waittime] eid\n",
            progname);
    exit(1);
}

void doOptions(int argc, const char **argv);

int expiration = 30;
int wait_after_done = 0;
char dest_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char source_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char replyto_eid_str[DTN_MAX_ENDPOINT_ID] = "";


int
main(int argc, const char** argv)
{
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t source_eid;
    dtn_endpoint_id_t replyto_eid;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t ping_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t ping_payload;
    ping_payload_t payload_contents;
    ping_payload_t recv_contents;
    dtn_bundle_payload_t reply_payload;
    dtn_bundle_status_report_t* sr_data;
    dtn_bundle_id_t bundle_id;
    int debug = 1;
    char demux[64];
    int dest_len = 0;
    struct timeval send_time, now;
    u_int32_t nonce;
    int done;
    time_t clock;
    struct tm* tm_buf;
    
    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    doOptions(argc, argv);

    memset(&ping_spec, 0, sizeof(ping_spec));

    gettimeofday(&now, 0);
    srand(now.tv_sec);
    nonce = rand();
    
    // open the ipc handle
    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }

    // make sure they supplied a valid destination eid or
    // "localhost", in which case we just use the local daemon
    if (strcmp(dest_eid_str, "localhost") == 0) {
        dtn_build_local_eid(handle, &ping_spec.dest, "ping");
        
    } else {
        if (dtn_parse_eid_string(&ping_spec.dest, dest_eid_str)) {
            fprintf(stderr, "invalid destination eid string '%s'\n",
                    dest_eid_str);
            exit(1);
        }
    }

    dest_len = strlen(ping_spec.dest.uri);
    if ((dest_len < 4) ||
        (strcmp(ping_spec.dest.uri + dest_len - 4, "ping") != 0))
    {
        fprintf(stderr, "\nWARNING: ping destination does not end in \"ping\"\n\n");
    }
    
    // if the user specified a source eid, register on it.
    // otherwise, build a local eid based on the configuration of
    // our dtn router plus the demux string
    snprintf(demux, sizeof(demux), "/traceroute.%d", getpid());
    if (source_eid_str[0] != '\0') {
        if (dtn_parse_eid_string(&source_eid, source_eid_str)) {
            fprintf(stderr, "invalid source eid string '%s'\n",
                    source_eid_str);
            exit(1);
        }
    } else {
        dtn_build_local_eid(handle, &source_eid, demux);
    }

    if (debug) printf("source_eid [%s]\n", source_eid.uri);
    dtn_copy_eid(&ping_spec.source, &source_eid);
    
    // Make the replyto EID
    if ( replyto_eid_str[0]!=0 ) {
        if (dtn_parse_eid_string(&replyto_eid, replyto_eid_str)) {
            fprintf(stderr, "invalid replyto eid string '%s'\n",
                    replyto_eid_str);
            exit(1);
        }
        dtn_copy_eid(&ping_spec.replyto, &replyto_eid);
        printf("using user-supplied replyto\n");
    } else {
        dtn_copy_eid(&ping_spec.replyto, &source_eid);
        printf("using default replyto\n");
    }

    // now create a new registration based on the source
    // if the replyto EID is unspecified or the same as the
    // source EID then we'll get status reports, otherwise
    // they'll go somewhere else.
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &source_eid);
    reginfo.flags = DTN_REG_DROP;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.expiration = 0;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }    
    if (debug) printf("dtn_register succeeded, regid %d\n", regid);

    // set the expiration time and request a bunch of status reports
    ping_spec.expiration = expiration;
    ping_spec.dopts = DOPTS_DELETE_RCPT |
                      DOPTS_RECEIVE_RCPT |
                      DOPTS_FORWARD_RCPT |
                      DOPTS_DELIVERY_RCPT;

    // loop, sending pings and polling for activity
    gettimeofday(&send_time, NULL);
        
    // fill in a short payload string, a nonce, and a sequence number
    // to verify the echo feature and make sure we're not getting
    // duplicate responses or ping responses from another app
    memcpy(&payload_contents.ping, PING_STR, 8);
    payload_contents.seqno = 0;
    payload_contents.nonce = nonce;
    payload_contents.time  = send_time.tv_sec;
        
    memset(&ping_payload, 0, sizeof(ping_payload));
    dtn_set_payload(&ping_payload, DTN_PAYLOAD_MEM,
                    (char*)&payload_contents, sizeof(payload_contents));
        
    memset(&bundle_id, 0, sizeof(bundle_id));
    if ((ret = dtn_send(handle, regid, &ping_spec, &ping_payload, &bundle_id)) != 0) {
        fprintf(stderr, "error sending bundle: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
        
    memset(&reply_spec, 0, sizeof(reply_spec));
    memset(&reply_payload, 0, sizeof(reply_payload));

    clock = time(&clock);
    tm_buf = gmtime(&clock);
    printf("%s: sent at %.*s UTC\n",
           ping_spec.source.uri, 24, asctime(tm_buf));
    
    // now loop waiting for replies / status reports until we're done
    done = 0;
    while (1) {
        int timeout = done ? wait_after_done * 1000 : -1;
        if ((ret = dtn_recv(handle, &reply_spec,
                            DTN_PAYLOAD_MEM, &reply_payload, timeout)) < 0)
        {
            if (done && dtn_errno(handle) == DTN_ETIMEOUT) {
                break;
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
                printf("%s: received at %.*s UTC (%ld ms rtt)\n",
                       reply_spec.source.uri, 24, asctime(tm_buf),
                       TIMEVAL_DIFF_MSEC(now, send_time));
            }
            if (sr_data->flags & STATUS_FORWARDED)
            {
                clock = sr_data->forwarding_ts.secs + DTNTIME_OFFSET;
                tm_buf = gmtime(&clock);
                printf("%s: forwarded at %.*s UTC (%ld ms rtt)\n",
                       reply_spec.source.uri, 24, asctime(tm_buf),
                       TIMEVAL_DIFF_MSEC(now, send_time));
            }
            if (sr_data->flags & STATUS_DELIVERED)
            {
                clock = sr_data->delivery_ts.secs + DTNTIME_OFFSET;
                tm_buf = gmtime(&clock);
                printf("%s: delivered at %.*s UTC (%ld ms rtt)\n",
                       reply_spec.source.uri, 24, asctime(tm_buf),
                       TIMEVAL_DIFF_MSEC(now, send_time));
            }
            if (sr_data->flags & STATUS_DELETED)
            {
                clock = sr_data->deletion_ts.secs + DTNTIME_OFFSET;
                tm_buf = gmtime(&clock);
                printf("%s: deleted at %.*s UTC (%s) (%ld ms rtt)\n",
                       reply_spec.source.uri, 24, asctime(tm_buf),
                       dtn_status_report_reason_to_str(sr_data->reason),
                       TIMEVAL_DIFF_MSEC(now, send_time));
                break;
            }
        }
        else {
            if (reply_payload.buf.buf_len != sizeof(ping_payload_t))
            {
                printf("%d bytes from [%s]: ERROR: length != %zu\n",
                       reply_payload.buf.buf_len,
                       reply_spec.source.uri,
                       sizeof(ping_payload_t));
                break;
            }

            memcpy(&recv_contents, reply_payload.buf.buf_val,
                   sizeof(recv_contents));
                
            if (recv_contents.seqno != 0)
            {
                printf("%d bytes from [%s]: ERROR: invalid seqno %d\n",
                       reply_payload.buf.buf_len,
                       reply_spec.source.uri,
                       recv_contents.seqno);
                break;
            }

            if (recv_contents.nonce != nonce)
            {
                printf("%d bytes from [%s]: ERROR: invalid nonce %u != %u\n",
                       reply_payload.buf.buf_len,
                       reply_spec.source.uri,
                       recv_contents.nonce, nonce);
                break;
            }

            if ((int)recv_contents.time != (int)send_time.tv_sec)
            {
                printf("%d bytes from [%s]: ERROR: time mismatch %u != %lu\n",
                       reply_payload.buf.buf_len,
                       reply_spec.source.uri,
                       recv_contents.time,
                       (long unsigned int)send_time.tv_sec);
            }
                
            clock = reply_spec.creation_ts.secs + DTNTIME_OFFSET;
            tm_buf = gmtime(&clock);
            printf("%s: echo reply at %.*s UTC (%ld ms rtt)\n",
                   reply_spec.source.uri, 24, asctime(tm_buf),
                   TIMEVAL_DIFF_MSEC(now, send_time));
            done = 1;
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
        case 'r':
            strcpy(replyto_eid_str, optarg);
	    printf("Setting replyto_eid_str to: %s\n", replyto_eid_str);
            break;
        case 'd':
            strcpy(dest_eid_str, optarg);
            break;
        case 's':
            strcpy(source_eid_str, optarg);
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

    if ((optind < argc) && (strlen(dest_eid_str) == 0)) {
        strcpy(dest_eid_str, argv[optind++]);
    }

    if (optind < argc) {
        fprintf(stderr, "unsupported argument '%s'\n", argv[optind]);
        exit(1);
    }

    if (dest_eid_str[0] == '\0') {
        fprintf(stderr, "must supply a destination eid (or 'localhost')\n");
        exit(1);
    }
}

