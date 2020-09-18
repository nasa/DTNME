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

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "dtn_api.h"

#define BUFSIZE 16
#define BLOCKSIZE 8192
#define COUNTER_MAX_DIGITS 9

// Find the maximum commandline length
#ifdef __FreeBSD__
/* Needed for PATH_MAX, Linux doesn't need it */
#include <sys/syslimits.h>
#endif

#ifndef PATH_MAX
/* A conservative fallback */
#define PATH_MAX 1024
#endif

const char *progname;
// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;

int   verbose           = 0;    	// verbose output
char* endpoint		= NULL; 	// endpoint for registration
dtn_reg_id_t regid	= DTN_REGID_NONE;// registration id
int   expiration	= 30; 		// registration expiration time
int   count             = 0;            // exit after count bundles received
int   failure_action	= DTN_REG_DEFER;// registration delivery failure action
char* failure_script	= "";	 	// script to exec
int   register_only	= 0;    	// register and quit
int   change		= 0;    	// change existing registration 
int   unregister	= 0;    	// remove existing registration 
int   no_find_reg	= 0;    	// omit call to dtn_find_registration
char filename[PATH_MAX];		// Destination filename for file xfers
dtn_bundle_payload_location_t bundletype = DTN_PAYLOAD_MEM;
int   promiscuous       = 0;	        // accept any bundles, ignore content
int   dots              = 0;            // show progress using dots


#define RECV_TIMEOUT 10000    	/* timeout to dtn_recv call (10s) */
int max_startup_retries = 10;	// how many times to spin on first bundle

void
usage()
{
    fprintf(stderr, "usage: %s [opts] -n <number of bundles to expect> "
	    "<endpoint> \n", progname);
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -h help\n");
    fprintf(stderr, " -A daemon api IP address\n");
    fprintf(stderr, " -B daemon api IP port\n");
    fprintf(stderr, " -d <eid|demux_string> endpoint id\n");
    fprintf(stderr, " -r <regid> use existing registration regid\n");
    fprintf(stderr, " -e <time> registration expiration time in seconds "
            "(default: one hour)\n");
    fprintf(stderr, " -f <defer|drop|exec> failure action\n");
    fprintf(stderr, " -F <script> failure script for exec action\n");
    fprintf(stderr, " -x call dtn_register and immediately exit\n");
    fprintf(stderr, " -c call dtn_change_registration and immediately exit\n");
    fprintf(stderr, " -u call dtn_unregister and immediately exit\n");
    fprintf(stderr, " -N don't try to find an existing registration\n");
    fprintf(stderr, " -p operate in promiscuous mode "
	    "(accept n bundles, ignore contents\n");
    fprintf(stderr, " -s show progress using dots (promiscuous mode)\n");
    fprintf(stderr, " -t <retries> default is 10 for 100 seconds\n");
}

void
parse_options(int argc, char**argv)
{
    int c, done = 0;

    progname = argv[0];

    memset(filename, 0, sizeof(char) * PATH_MAX);

    while (!done)
    {
        c = getopt(argc, argv, "A:B:vhHd:r:e:f:F:xn:cuNpst:");
        switch (c)
        {
        case 'A':
            api_IP_set = 1;
            api_IP = optarg;
            break;
        case 'B':
            api_IP_set = 1;
            api_port = atoi(optarg);
            break;    
        case 'v':
            verbose = 1;
            break;
        case 'h':
        case 'H':
            usage();
            exit(0);
            return;
        case 'r':
            regid = atoi(optarg);
            break;
        case 'e':
            expiration = atoi(optarg);
            break;
        case 'f':
            if (!strcasecmp(optarg, "defer")) {
                failure_action = DTN_REG_DEFER;

            } else if (!strcasecmp(optarg, "drop")) {
                failure_action = DTN_REG_DROP;

            } else if (!strcasecmp(optarg, "exec")) {
                failure_action = DTN_REG_EXEC;

            } else {
                fprintf(stderr, "invalid failure action '%s'\n", optarg);
                usage();
                exit(1);
            }
        case 'F':
            failure_script = optarg;
            break;
        case 'x':
            register_only = 1;
            break;
        case 'n':
            count = atoi(optarg);
            break;
        case 'c':
            change = 1;
            break;
        case 'u':
            unregister = 1;
            break;
        case 'N':
            no_find_reg = 1;
            break;
        case 'o':
            strncpy(filename, optarg, PATH_MAX);
            break;
	case 'p':
	    promiscuous = 1;
	    break;
        case 's':
            dots = 1;
            break;
        case 't':
            max_startup_retries = atoi(optarg);
            break;
        case -1:
            done = 1;
            break;
        default:
            // getopt already prints an error message for unknown
            // option characters
            usage();
            exit(1);
        }
    }

    if (count < 1) {
	fprintf(stderr, "must specify (positive) number of bundles expected\n");
	usage();
	exit(1);
    }

    endpoint = argv[optind];
    if (!endpoint && (regid == DTN_REGID_NONE)) {
        fprintf(stderr, "must specify either an endpoint or a regid\n");
        usage();
        exit(1);
    }

    if ((change || unregister) && (regid == DTN_REGID_NONE)) {
        fprintf(stderr, "must specify regid when using -%c option\n",
                change ? 'c' : 'u');
        usage();
        exit(1);
    }

    if (failure_action == DTN_REG_EXEC && failure_script == NULL) {
        fprintf(stderr, "failure action EXEC must supply script\n");
        usage();
        exit(1);
    }

    // the default is to use memory transfer mode, but if someone specifies a
    // filename then we need to tell the API to expect a file
    if ( filename[0] != '\0' )
        bundletype = DTN_PAYLOAD_FILE;

}


/* ----------------------------------------------------------------------- */
/* File transfers suffer considerably from the inability to safely send 
 * metadata on the same channel as the file transfer in DTN.  Perhaps we 
 * should work around this?
 */
int
handle_file_transfer(dtn_bundle_payload_t payload, uint32_t *size, uint32_t *which)
{
    int tempdes;
    struct stat fileinfo;

    // Copy the file into place
    tempdes = open(payload.filename.filename_val, O_RDONLY);
    if ( tempdes < 0 ) {
	fprintf(stderr, "While opening the temporary file for reading '%s': %s\n",
		payload.filename.filename_val, strerror(errno));
	return 0;
    }

    if (fstat(tempdes, &fileinfo) != 0) {
	fprintf(stderr, "While stat'ing the temp file '%s': %s\n",
		payload.filename.filename_val, strerror(errno));
	close(tempdes);
	return -1;
    }

    if (read(tempdes, which, sizeof(*which)) != sizeof(*which)) {
	fprintf(stderr, "While reading bundle number from temp file '%s': %s\n",
		payload.filename.filename_val, strerror(errno));
	close(tempdes);
	return -1;
    }
    close(tempdes);
    unlink(payload.filename.filename_val);

    *size = fileinfo.st_size;
    *which = (uint32_t) ntohl(*(uint32_t *)which);

    return 0;
}

int
main(int argc, char** argv)
{
    int i;
    int errs = 0;
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t local_eid;
    dtn_reg_info_t reginfo;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    int call_bind;
    time_t now;
    int bundles_received = 0;

    struct timespec first_bundle_receipt_time;
    struct timespec last_bundle_receipt_time;
    struct timespec interim_time;
    struct timespec delta;
    uint64_t bytes_received = 0;
    uint64_t interim_bytes_received = 0;
    double rate = 0.0;

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    progname = argv[0];

    parse_options(argc, argv);

    printf("dtnsink starting up -- waiting for %u bundles\n", count);

    // open the ipc handle
    if (verbose) printf("opening connection to dtn router...\n");

    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }
    if (verbose) printf("opened connection to dtn router...\n");

    // if we're not given a regid, or we're in change mode, we need to
    // build up the reginfo
    memset(&reginfo, 0, sizeof(reginfo));

    if ((regid == DTN_REGID_NONE) || change)
    {
        // if the specified eid starts with '/', then build a local
        // eid based on the configuration of our dtn router plus the
        // demux string. otherwise make sure it's a valid one
        if (endpoint[0] == '/') {
            if (verbose) printf("calling dtn_build_local_eid.\n");
            dtn_build_local_eid(handle, &local_eid, (char *) endpoint);
            if (verbose) printf("local_eid [%s]\n", local_eid.uri);
        } else {
            if (verbose) printf("calling parse_eid_string\n");
            if (dtn_parse_eid_string(&local_eid, endpoint)) {
                fprintf(stderr, "invalid destination endpoint '%s'\n",
                        endpoint);
                goto err;
            }
        }

        // create a new registration based on this eid
        dtn_copy_eid(&reginfo.endpoint, &local_eid);
        reginfo.regid             = regid;
        reginfo.expiration        = expiration;
        reginfo.flags             = failure_action;
        reginfo.script.script_val = failure_script;
        reginfo.script.script_len = strlen(failure_script) + 1;
    }

    if (change) {
        if ((ret = dtn_change_registration(handle, regid, &reginfo)) != 0) {
            fprintf(stderr, "error changing registration: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            goto err;
        }
        printf("change registration succeeded, regid %d\n", regid);
        goto done;
    }
    
    if (unregister) {
        if (dtn_unregister(handle, regid) != 0) {
            fprintf(stderr, "error in unregister regid %d: %s\n",
                    regid, dtn_strerror(dtn_errno(handle)));
            goto err;
        }
        
        printf("unregister succeeded, regid %d\n", regid);
        goto done;
    }
    
    // try to see if there is an existing registration that matches
    // the given endpoint, in which case we'll use that one.
    if (regid == DTN_REGID_NONE && ! no_find_reg) {
        if (dtn_find_registration(handle, &local_eid, &regid) != 0) {
            if (dtn_errno(handle) != DTN_ENOTFOUND) {
                fprintf(stderr, "error in find_registration: %s\n",
                        dtn_strerror(dtn_errno(handle)));
                goto err;
            }
        }
        printf("find registration succeeded, regid %d\n", regid);
        call_bind = 1;
    }
    
    // if the user didn't give us a registration to use, get a new one
    if (regid == DTN_REGID_NONE) {
        if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
            fprintf(stderr, "error creating registration: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            goto err;
        }

        printf("register succeeded, regid %d\n", regid);
        call_bind = 0;
    } else {
        call_bind = 1;
    }
    
    if (register_only) {
        goto done;
    }

    if (call_bind) {
        // bind the current handle to the found registration
        printf("binding to regid %d\n", regid);
        if (dtn_bind(handle, regid) != 0) {
            fprintf(stderr, "error binding to registration: %s\n",
                    dtn_strerror(dtn_errno(handle)));
            goto err;
        }
    }

    // keep track of what we've seen
    char *received = (char *)malloc(count + 1);
    memset(received, '\0', count);

    int percnt1 = count / 100;

    // loop waiting for bundles
    fprintf(stderr, "waiting %d seconds for first bundle...\n",
	    (max_startup_retries)*RECV_TIMEOUT/1000);
    i = 1;
    while (i <= count) {
	int tries;
	uint32_t which;
	uint32_t size;

        memset(&spec, 0, sizeof(spec));
        memset(&payload, 0, sizeof(payload));

	/* 
	 * this is a little tricky. We want dtn_recv to time out after
	 * RECV_TIMEOUT ms, so we don't wait a long time for a bundle
	 * if something is broken and no bundle is coming.  But we
	 * want to be friendly and wait patiently for the first
	 * bundle, in case dtnsource is slow in getting off the mark.
	 * 
	 * So we loop at most max_startup_retries times
	 */
	tries = 0;
	while ((ret = dtn_recv(handle, &spec, bundletype, &payload, 
			       RECV_TIMEOUT)) < 0) {
	    /* if waiting for the first bundle and we timed out be patient */
	    if (dtn_errno(handle) == DTN_ETIMEOUT) {
		if (i == 1 && ++tries < max_startup_retries) {
		    fprintf(stderr, "waiting %d seconds for first bundle...\n",
			    (max_startup_retries-tries)*RECV_TIMEOUT/1000);
		} else if (++tries < max_startup_retries) {
		    fprintf(stderr, "waiting %d seconds for bundle %d...\n",
			    (max_startup_retries-tries)*RECV_TIMEOUT/1000, i);
                } else {
		    /* timed out waiting, something got dropped */
		    fprintf(stderr, "timeout waiting for bundle %d\n", i);
		    goto bail;
		}
	    } else {
	        /* a bad thing has happend in recv, or we've lost patience */
		fprintf(stderr, "error in dtn_recv: %d (%d, %s)\n", ret, 
			dtn_errno(handle), dtn_strerror(dtn_errno(handle)));
		goto bail;
	    }
	}

        tries = 0;
	if (i == 1) {
            clock_gettime(CLOCK_MONOTONIC, &first_bundle_receipt_time);
            last_bundle_receipt_time = first_bundle_receipt_time;
            interim_time = first_bundle_receipt_time;
	} else {
            clock_gettime(CLOCK_MONOTONIC, &last_bundle_receipt_time);
        }



        // Files need to be handled differently than memory transfers
        if (payload.location == DTN_PAYLOAD_FILE) {
            if (handle_file_transfer(payload, &size, &which) < 0) {
                dtn_free_payload(&payload);
                continue;
            }
        } else {
            size = payload.buf.buf_len;

            if (3 == size && 0 == strncmp(&payload.buf.buf_val[1], "Go", 2)) {
                printf("Received bpdriver start packet\n");
                continue;
            }

            memcpy(&which, payload.buf.buf_val, sizeof(uint32_t));
            which = ntohl(which);
        }
	    
	if (i == 1) {
            now = time(0);
	    printf("received first bundle at %s\n", ctime(&now));
        }

        ++bundles_received;
        bytes_received += size;

	if (!promiscuous) {
	    /* check to see which bundle this is */
	    if (which > (uint32_t) count) {
		// note that the above cast is safe as count always >= 0
		fprintf(stderr, "-- expecting %d bundles, saw bundle %u\n", 
			count, which);
	    }
	    else if (which <= 0) { /* because I am paranoid -DJE */
		fprintf(stderr, "-- didn't expect bundle %u\n", which);
	    }
	    else {
	        ++received[which];
	    }
	} else if (!verbose) {
            if (dots) {
                printf(".");
                fflush(stdout);
            } else if (bundles_received % percnt1 == 0) {
                delta.tv_sec = last_bundle_receipt_time.tv_sec - interim_time.tv_sec;
                delta.tv_nsec = last_bundle_receipt_time.tv_nsec - interim_time.tv_nsec;
                while (delta.tv_nsec >= 1000000000) {
                    delta.tv_sec++;
                    delta.tv_nsec -= 1000000000;
                }
                while (delta.tv_nsec < 0) {
                    delta.tv_sec--;
                    delta.tv_nsec += 1000000000;
                }
                double delta_us = delta.tv_sec * 1000000.0 + delta.tv_nsec / 1000.0;
                rate = 0.0;
                if (delta_us > 0.0) {
                    rate = ((bytes_received - interim_bytes_received) * 8.0) / delta_us;
                }

                printf("Bundles received: %d   rate: %.3f Mbps\n", bundles_received, rate);

                interim_time = last_bundle_receipt_time;
                interim_bytes_received = bytes_received;
            }
        }

	if (verbose) {
	    printf("bundle %d received successfully: id %s,%"PRIu64".%"PRIu64"  payload len: %"PRIu32"\n",
		   i,
		   spec.source.uri,
		   spec.creation_ts.secs,
		   spec.creation_ts.seqno,
           size
           );
	}

	// XXX should verify size here...

	/* all done, get next one */
	dtn_free_payload(&payload);

        ++i;
    }

bail:
    printf("\n");
	if (!promiscuous) {
        for (i = 1; i <= count; ++i) {
	        if (received[i] == 0) {
	            int j = i + 1;
	            while (j <= count && received[j] == 0)
		            ++j;
	            if (j == i + 1)
		            printf("bundle %d: dropped\n", i);
	            else
		            printf("bundles %d-%d dropped\n", i, j - 1);
	            errs += (j - i);
	            i += (j - i - 1);
	        } else if (received[i] > 1) {
	            printf("bundle %d: received %d copies\n", i, received[i]);
	            ++errs;
	        }
        }
        if (errs == 0) {
	        printf("all %d bundles received correctly\n", count);
        }
    }

    delta.tv_sec = last_bundle_receipt_time.tv_sec - first_bundle_receipt_time.tv_sec;
    delta.tv_nsec = last_bundle_receipt_time.tv_nsec - first_bundle_receipt_time.tv_nsec;
    while (delta.tv_nsec >= 1000000000) {
        delta.tv_sec++;
        delta.tv_nsec -= 1000000000;
    }
    while (delta.tv_nsec < 0) {
        delta.tv_sec--;
        delta.tv_nsec += 1000000000;
    }
    double delta_us = delta.tv_sec * 1000000.0 + delta.tv_nsec / 1000.0;
    rate = 0.0;
    if (delta_us > 0.0) {
        rate = (bytes_received * 8.0) / delta_us;
    }

    free(received);
    now = time(0);
    printf("terminating at %s\n\n", ctime(&now));

    printf("Total bundles received: %d  in %.3f secs - payload bytes: %"PRIu64"  rate: %.3f Mbps\n", 
           bundles_received, delta_us/1000000.0, bytes_received, rate);


done:
    dtn_close(handle);
    return 0;

err:
    dtn_close(handle);
    return 1;
}
