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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "dtnecho.h"
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
    //dz debug fprintf(stderr, "usage: %s [-A api_IP] [-B api_port][-c count] [-e expiration] [-s source eid] \n",
    fprintf(stderr, "usage: %s [options] \n", progname);
    fprintf(stderr, "    options: [-A api_IP] [-B api_port] [-c count] \n");
    fprintf(stderr, "             [-p max payload size] [-s source eid]\n");
    exit(1);
}

void doOptions(int argc, const char **argv);

int count = 0;
char my_eid_str[DTN_MAX_ENDPOINT_ID] = "";
unsigned int max_payload_size = 1024;

// ECOS parameters
int ecos_enabled = 0;
int ecos_flags = 0;
int ecos_ordinal = 0;
int ecos_flow_label = 0;


int
main(int argc, const char** argv)
{
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t my_eid;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t echo_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t echo_payload;
    dtn_bundle_payload_t reply_payload;
    dtn_bundle_id_t bundle_id;

    int debug = 1;

    int keep_going   = TRUE;
    int bundles_sent = 0;
    int bundles_rcvd = 0;
    int num_errors   = 0;
    int timeout;

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    doOptions(argc, argv);

    memset(&echo_spec, 0, sizeof(echo_spec));
    memset(&echo_payload, 0, sizeof(echo_payload));

    
    // open the ipc handle
    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }

    
    memset(&echo_spec, 0, sizeof(reply_spec));
    memset(&echo_payload, 0, sizeof(reply_payload));
    memset(&reply_spec, 0, sizeof(reply_spec));
    memset(&reply_payload, 0, sizeof(reply_payload));

    if (dtn_parse_eid_string(&my_eid, my_eid_str)) {
        fprintf(stderr, "invalid source eid string '%s'\n",
                my_eid_str);
        exit(1);
    }

    // now create a new registration based on the source
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &my_eid);

    reginfo.flags = DTN_REG_DEFER;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.expiration = 0;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }    
    if (debug) printf("dtn_register succeeded, regid %d\n", regid);


    do {
        timeout = 100;  

        if ((ret = dtn_recv(handle, &echo_spec,
             DTN_PAYLOAD_MEM, &echo_payload, timeout)) < 0)
        {
            if (dtn_errno(handle) == DTN_ETIMEOUT) {
                continue;
            }
                
            fprintf(stderr, "error getting bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));

            keep_going = FALSE;
            break;
        }
            
        ++bundles_rcvd;

        printf("READ %d bytes from [%s]: '%.*s' (sent: %d rcvd: %d errs: %d)\n",
               echo_payload.buf.buf_len,
               echo_spec.source.uri,
               (u_int)strlen(ECHO_STR),
               echo_payload.buf.buf_val,
               bundles_sent, bundles_rcvd, num_errors);
        fflush(stdout);

        memset(&reply_spec,    0, sizeof(reply_spec));
        memset(&reply_payload, 0, sizeof(reply_payload));
        memset(&bundle_id,     0, sizeof(bundle_id));

        dtn_copy_eid(&reply_spec.dest,   &echo_spec.source);
        dtn_copy_eid(&reply_spec.source, &echo_spec.dest);
        reply_spec.expiration = echo_spec.expiration;

        // only return up to max configured payload size
        int reply_payload_size = echo_payload.buf.buf_len < max_payload_size ? echo_payload.buf.buf_len : max_payload_size;
        reply_payload.buf.buf_val = echo_payload.buf.buf_val;
        reply_payload.buf.buf_len = reply_payload_size;
        reply_payload.location    = DTN_PAYLOAD_MEM;

        if ((ret = dtn_send(handle, regid, &reply_spec, &reply_payload,
                            &bundle_id)) != 0) {
            fprintf(stderr, "error sending bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            keep_going = FALSE;
            break;
        } else {
            printf("Echo bundle sent to [%s] size: %d\n", reply_spec.dest.uri, reply_payload_size);
        }

        dtn_free_payload(&echo_payload);
        ++bundles_sent;


        if(count > 0)
        {
            if(bundles_rcvd == count) 
            {
               printf("processed count met %d\n",bundles_rcvd);
               keep_going = FALSE;
               break;
            }
        }

    } while(keep_going);
    
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
        case 'c':
            count = atoi(optarg);
            break;
        case 'h':
            usage();
            break;
        case 's':
            strcpy(my_eid_str,optarg);
            break;
        case 'p':
            tmp = atoi(optarg);
            if (tmp > 0) {
                max_payload_size = tmp;
            } else {
                printf("\nInvalid max payload size.\n");
                fflush(stdout);
                exit(1);
            }
            break;    

        default:
            break;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "unsupported argument '%s'\n", argv[optind]);
        exit(1);
    }
}
