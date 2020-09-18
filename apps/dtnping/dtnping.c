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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "dtnping.h"
#include "dtn_api.h"
#include "dtn_types.h"

const char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;

void
usage()
{
    //dz debug fprintf(stderr, "usage: %s [-A api_IP] [-B api_port][-c count] [-i interval] [-e expiration] [-s source eid] eid\n",
    fprintf(stderr, "usage: %s [options] <destination eid>\n", progname);
    fprintf(stderr, "    options: [-A api_IP] [-B api_port][-c count] [-i interval (ms)] [-e expiration]\n");
    fprintf(stderr, "             [-t (custody transfer)] [-p payload size] [-s source eid]\n");
    fprintf(stderr, "             [-C (ECOS critical)] [-S (ECOS streaming)] [-R (ECOS reliable)] [-O <ECOS ordinal>] [-F <ECOS flow label>]\n\n");
    exit(1);
}

void doOptions(int argc, const char **argv);

int interval = 1 * 1000;  //interval in millisecs
int count = 0;
int reply_count = 0;
int expiration = 30;
char dest_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char source_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char replyto_eid_str[DTN_MAX_ENDPOINT_ID] = "";
int custody_transfer = 0;
size_t payload_size = sizeof(ping_payload_t);

// ECOS parameters
int ecos_enabled = 0;
int ecos_flags = 0;
int ecos_ordinal = 0;
int ecos_flow_label = 0;


#define MAX_PINGS_IN_FLIGHT 1000000

int
main(int argc, const char** argv)
{
    int i;
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t source_eid;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t ping_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t ping_payload;
    ping_payload_t* payload_contents;
    ping_payload_t* recv_contents;
    dtn_bundle_payload_t reply_payload;
    dtn_bundle_status_report_t* sr_data;
    dtn_bundle_id_t bundle_id;
    int debug = 1;
    char demux[64];
    int dest_len = 0;
    struct timeval* send_times;
    dtn_timestamp_t* creation_times;
    struct timeval now, recv_start, recv_end;
    u_int32_t nonce;
    u_int32_t seqno = 0;
    int timeout;

    int bundles_sent = 0;
    int bundles_rcvd = 0;
    int num_errors = 0;
    struct timeval start_time;
    struct timeval all_sent_time;
    struct timeval end_time;
    double sent_secs = 0.0;
    double sent_bundles_per_sec = 0.0;
    double recv_secs = 0.0;
    double recv_bundles_per_sec = 0.0;

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    doOptions(argc, argv);

    int inflight = MAX_PINGS_IN_FLIGHT;
    if (count > 0 && count < MAX_PINGS_IN_FLIGHT) inflight = count;
    send_times = (struct timeval*) calloc(inflight, sizeof(struct timeval));
    creation_times = (dtn_timestamp_t*) calloc(inflight, sizeof(dtn_timestamp_t));


    memset(&ping_spec, 0, sizeof(ping_spec));

    payload_contents = (ping_payload_t*) malloc(payload_size);
    recv_contents = (ping_payload_t*) malloc(payload_size);
    memset(payload_contents, ' ', payload_size);
    memset(recv_contents, ' ', payload_size);
    char* ascii_ctr = 0;
    if (payload_size >= (sizeof(ping_payload_t) + 10)) {
        // reserve first 10 extra bytes for the counter and fill the rest with ABC
        ascii_ctr = (char*) payload_contents; // use the ptr temporarily to load the fill
        size_t ix = sizeof(ping_payload_t) + 10;
        char fill = 'A';
        while (ix < payload_size) {
            ascii_ctr[ix++] = fill++;
            if (fill > 'Z') fill = 'A';
        }
        // now point to where the counter will be loaded
        ascii_ctr += sizeof(ping_payload_t);
    }


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
    snprintf(demux, sizeof(demux), "/ping.%d", getpid());
    if (source_eid_str[0] != '\0') {
        if (dtn_parse_eid_string(&source_eid, source_eid_str)) {
            fprintf(stderr, "invalid source eid string '%s'\n",
                    source_eid_str);
            exit(1);
        }
    } else {
        dtn_build_local_eid(handle, &source_eid, demux);
    }

    // set the source and replyto eids in the bundle spec
    if (debug) printf("source_eid [%s]\n", source_eid.uri);
    dtn_copy_eid(&ping_spec.source, &source_eid);
    dtn_copy_eid(&ping_spec.replyto, &source_eid);

    if (custody_transfer) {
        ping_spec.dopts |= DOPTS_CUSTODY;
    }    

    // now create a new registration based on the source
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &source_eid);
    reginfo.flags = DTN_REG_DEFER;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.expiration = 0;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }    
    if (debug) printf("dtn_register succeeded, regid %d\n", regid);

    // ECOS parameters
    ping_spec.ecos_enabled = ecos_enabled;
    ping_spec.ecos_flags = ecos_flags;
    ping_spec.ecos_ordinal = ecos_ordinal;
    ping_spec.ecos_flow_label = ecos_flow_label;

    // set the expiration time 
    ping_spec.expiration = expiration;

    printf("PING [%s] (expiration %u)...\n", ping_spec.dest.uri, expiration);
    if (interval == 0) {
        printf("WARNING: zero second interval will result in flooding pings!!\n");
    }
    
    gettimeofday(&start_time, NULL);

    // loop, sending pings and polling for activity
    for (i = 0; count == 0 || i < count; ++i) {
        gettimeofday(&send_times[seqno % inflight], NULL);
        
        // fill in a short payload string, a nonce, and a sequence number
        // to verify the echo feature and make sure we're not getting
        // duplicate responses or ping responses from another app
        memcpy(payload_contents->ping, PING_STR, 8);
        payload_contents->seqno = seqno;
        payload_contents->nonce = nonce;
        payload_contents->time = send_times[seqno % inflight].tv_sec;

        if (0 != ascii_ctr) {
            snprintf(ascii_ctr, 10, "%9.9u", seqno);
        }
        
        memset(&ping_payload, 0, sizeof(ping_payload));
        dtn_set_payload(&ping_payload, DTN_PAYLOAD_MEM,
                        (char*)payload_contents, payload_size);
        
        memset(&bundle_id, 0, sizeof(bundle_id));
        if ((ret = dtn_send(handle, regid, &ping_spec, &ping_payload,
                            &bundle_id)) != 0) {
            fprintf(stderr, "error sending bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }
        
        creation_times[seqno % inflight] = bundle_id.creation_ts;

        ++bundles_sent;

        memset(&reply_spec, 0, sizeof(reply_spec));
        memset(&reply_payload, 0, sizeof(reply_payload));

        // now loop waiting for replies / status reports until it's
        // time to send again, adding twice the expiration time if we
        // just sent the last ping
        //timeout = interval * 1000;
        timeout = interval;   // specify interval in millisecs
        if (i == count - 1) {
            timeout += (expiration * 2 * 1000);  // times 1000 since now using millis

            gettimeofday(&all_sent_time, NULL);
            sent_secs = all_sent_time.tv_sec - start_time.tv_sec;
            if (0.0 == sent_secs) sent_secs = 1.0;
            sent_bundles_per_sec = count / sent_secs;
            fprintf(stderr, "\nAll bundles \"sent\": %d bundles in %.0f secs  (avg: %.2f bundles/sec)\n",
                    count, sent_secs, sent_bundles_per_sec);
            fflush(stderr);
        }
        
        do {
            gettimeofday(&recv_start, 0);
            if ((ret = dtn_recv(handle, &reply_spec,
                                DTN_PAYLOAD_MEM, &reply_payload, timeout)) < 0)
            {
                if (dtn_errno(handle) == DTN_ETIMEOUT) {
                    break; // time to send again
                }
                
                fprintf(stderr, "error getting ping reply: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
                exit(1);
            }
            gettimeofday(&recv_end, 0);
            
            if (reply_payload.status_report != NULL)
            {
                sr_data = reply_payload.status_report;
                if (sr_data->flags != STATUS_DELETED) {
                    fprintf(stderr, "(bad status report from %s: flags 0x%x)\n",
                            reply_spec.source.uri, sr_data->flags);
                    goto next;
                }

                // find the seqno corresponding to the original
                // transmission time in the status report
                int j = 0;
                for (j = 0; j < inflight; ++j) {
                    if (creation_times[j].secs  ==
                            sr_data->bundle_id.creation_ts.secs &&
                        creation_times[j].seqno ==
                            sr_data->bundle_id.creation_ts.seqno % inflight)
                    {
                        printf("bundle deleted at [%s] (%s): seqno=%d, time=%ld ms   (sent: %d rcvd: %d errs: %d)\n",
                               reply_spec.source.uri,
                               dtn_status_report_reason_to_str(sr_data->reason),
                               j, TIMEVAL_DIFF_MSEC(recv_end, send_times[j]),
                               bundles_sent, bundles_rcvd, num_errors);
                        goto next;
                    }
                }

                ++num_errors;
                printf("bundle deleted at [%s] (%s): ERROR: can't find seqno   (sent: %d rcvd: %d errs: %d)\n",
                       reply_spec.source.uri, 
                       dtn_status_report_reason_to_str(sr_data->reason),
                       bundles_sent, bundles_rcvd, num_errors);
            }
            else {
                if (reply_payload.buf.buf_len != payload_size)
                {
                    ++num_errors;
                    printf("%d bytes from [%s]: ERROR: length != %zu   (sent: %d rcvd: %d errs: %d)\n",
                           reply_payload.buf.buf_len,
                           reply_spec.source.uri,
                           payload_size,
                           bundles_sent, bundles_rcvd, num_errors);
                    goto next;
                }

                memcpy(recv_contents, reply_payload.buf.buf_val, payload_size);
                
//dz deug                if (recv_contents.seqno > count)
//dz deug                {
//dz deug                    printf("%d bytes from [%s]: ERROR: invalid seqno %d\n",
//dz deug                           reply_payload.buf.buf_len,
//dz deug                           reply_spec.source.uri,
//dz deug                           recv_contents.seqno);
//dz deug                    goto next;
//dz deug                }

                if (recv_contents->nonce != nonce)
                {
                    ++num_errors;
                    printf("%d bytes from [%s]: ERROR: invalid nonce %u != %u  seq: %u (sent: %d rcvd: %d errs: %d)\n",
                           reply_payload.buf.buf_len,
                           reply_spec.source.uri,
                           recv_contents->seqno,
                           recv_contents->nonce, nonce,
                           bundles_sent, bundles_rcvd, num_errors);
                    goto next;
                }

                if (recv_contents->time != (u_int32_t)send_times[recv_contents->seqno % inflight].tv_sec)
                {
                    ++num_errors;
                    printf("%d bytes from [%s]: ERROR: time mismatch -- "
                           "seqno %u reply time %u != send time %lu   (sent: %d rcvd: %d errs: %d)\n",
                           reply_payload.buf.buf_len,
                           reply_spec.source.uri,
                           recv_contents->seqno,
                           recv_contents->time,
                           (long unsigned int)send_times[recv_contents->seqno % inflight].tv_sec,
                           bundles_sent, bundles_rcvd, num_errors);
                    goto next;
                }
                
                ++bundles_rcvd;

                printf("%d bytes from [%s]: '%.*s' seqno=%d, time=%ld ms   (sent: %d rcvd: %d errs: %d)\n",
                       reply_payload.buf.buf_len,
                       reply_spec.source.uri,
                       (u_int)strlen(PING_STR),
                       reply_payload.buf.buf_val,
                       recv_contents->seqno,
                       TIMEVAL_DIFF_MSEC(recv_end,
                                         send_times[recv_contents->seqno % inflight]),
                       bundles_sent, bundles_rcvd, num_errors);
                fflush(stdout);
            }
next:
            dtn_free_payload(&reply_payload);
            timeout -= TIMEVAL_DIFF_MSEC(recv_end, recv_start);


            // once we get status from all the pings we're supposed to
            // send, we're done
            reply_count++;
            if (count != 0 && reply_count == count) {
                break;
            }

        } while (timeout > 0);
        
        seqno++;
        //seqno %= inflight;
    }

    gettimeofday(&end_time, NULL);
    recv_secs = end_time.tv_sec - start_time.tv_sec;
    if (0.0 == recv_secs) recv_secs = 1.0;
    recv_bundles_per_sec = reply_count / recv_secs;
    fprintf(stderr, "\nReceived %d of %d replies in %.0f secs  (avg: %.2f bundles/sec)\n",
            reply_count, count, recv_secs, recv_bundles_per_sec);
    fflush(stderr);



    dtn_close(handle);

    return 0;
}

void
doOptions(int argc, const char **argv)
{
    int c;
    size_t tmp;

    progname = argv[0];

    while ( (c=getopt(argc, (char **) argv, "A:B:htc:i:e:d:s:r:p:CSRO:F:")) !=EOF ) {
        switch (c) {
        case 'A':
            api_IP_set = 1;
            api_IP = optarg;
            break;
        case 'B':
            api_IP_set = 1;
            api_port = atoi(optarg);
            break;    

        // ECOS parameters
        case 'C':
            ecos_enabled = 1;
            ecos_flags |= ECOS_FLAG_CRITICAL;
            break;
        case 'S':
            ecos_enabled = 1;
            ecos_flags |= ECOS_FLAG_STREAMING;
            break;
        case 'R':
            ecos_enabled = 1;
            ecos_flags |= ECOS_FLAG_RELIABLE;
            break;
        case 'O':
            ecos_enabled = 1;
            ecos_ordinal = atoi(optarg);
            if ( ecos_ordinal < 0 || ecos_ordinal > 254 )
            {
              fprintf(stderr, "Invalid ECOS ordinal value specified - must be 0 to 254\n");
              exit(1);
            }
            break;
        case 'F':
            ecos_enabled = 1;
            ecos_flags |= ECOS_FLAG_FLOW_LABEL;
            ecos_flow_label = atoi(optarg);
            if ( ecos_ordinal < 0 )
            {
              fprintf(stderr, "Invalid ECOS flow label value specified - must be greater than or equal to 0\n");
              exit(1);
            }
            break;
            

        case 'c':
            count = atoi(optarg);
            break;
        case 'i':
            interval = atoi(optarg);
            break;
        case 'e':
            expiration = atoi(optarg);
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
        case 't':
            custody_transfer = 1;
            break;

        case 'p':
            tmp = atoi(optarg);
            if (tmp >= sizeof(ping_payload_t)) {
                payload_size = tmp;
            } else {
                printf("\nInvalid payload size. Minimum is %zu\n", sizeof(ping_payload_t));
                fflush(stdout);
                exit(1);
            }
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

