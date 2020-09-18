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
#include <sys/stat.h>
#include <fcntl.h>
#include "dtn_api.h"

#include <time.h>

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
int   doAppAcks         = 0;            // Application ack of received bundles
int   quiet             = 0;    	// quiet output
char* endpoint		= NULL; 	// endpoint for registration
dtn_reg_id_t regid	= DTN_REGID_NONE;// registration id
int   expiration	= 30; 		// registration expiration time
int   count             = 0;            // exit after count bundles received
int   failure_action	= DTN_REG_DEFER;// registration delivery failure action
char* failure_script	= "";	 	// script to exec
int   replay_action	= DTN_REPLAY_NEW;// registration delivery replay semantics
int   register_only	= 0;    	// register and quit
int   change		= 0;    	// change existing registration 
int   unregister	= 0;    	// remove existing registration 
int   recv_timeout	= -1;    	// timeout to dtn_recv call
int   no_find_reg	= 0;    	// omit call to dtn_find_registration
int   do_stats      = 0;        // statistics frequency (bundle count)
char filename[PATH_MAX];		// Destination filename for file xfers
dtn_bundle_payload_location_t bundletype = DTN_PAYLOAD_MEM;

void
usage()
{
    fprintf(stderr, "usage: %s [opts] <endpoint> \n", progname);
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -A daemon api IP address\n");
    fprintf(stderr, " -B daemon api IP port\n");
    fprintf(stderr, " -a Application acknowledgement of received bundles (to daemon)\n");
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -q quiet\n");
    fprintf(stderr, " -s show statistics (implies quiet & no timeout)\n");
    fprintf(stderr, " -h help\n");
    fprintf(stderr, " -d <eid|demux_string> endpoint id\n");
    fprintf(stderr, " -r <regid> use existing registration regid\n");
    fprintf(stderr, " -n <count> exit after count bundles received\n");
    fprintf(stderr, " -e <time> registration expiration time in seconds "
            "(default: %d)\n", expiration);
    fprintf(stderr, " -f <defer|drop|exec> failure action\n");
    fprintf(stderr, " -R <new|none|all> replay action\n");
    fprintf(stderr, " -F <script> failure script for exec action\n");
    fprintf(stderr, " -x call dtn_register and immediately exit\n");
    fprintf(stderr, " -c call dtn_change_registration and immediately exit\n");
    fprintf(stderr, " -u call dtn_unregister and immediately exit\n");
    fprintf(stderr, " -N don't try to find an existing registration\n");
    fprintf(stderr, " -t <timeout> timeout value for call to dtn_recv\n");
    fprintf(stderr, " -o <template> Write out transfers to files using this template (# chars are\n"
            "replaced with a counter). Example: f##.bin goes to f00.bin, f01.bin, etc...\n");
}

void
parse_options(int argc, char**argv)
{
    int c, done = 0;

    progname = argv[0];

    memset(filename, 0, sizeof(char) * PATH_MAX);

    while (!done)
    {
        c = getopt(argc, argv, "aA:B:vqhHsd:r:e:f:R:F:xn:cuNt:o:s");
        switch (c)
        {
        case 'a':
            doAppAcks = 1;
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
        case 'q':
            quiet = 1;
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
        case 'R':
            if (!strcasecmp(optarg, "new")) {
                replay_action = DTN_REPLAY_NEW;

            } else if (!strcasecmp(optarg, "none")) {
                replay_action = DTN_REPLAY_NONE;

            } else if (!strcasecmp(optarg, "all")) {
                replay_action = DTN_REPLAY_ALL;

            } else {
                fprintf(stderr, "invalid replay action '%s'\n", optarg);
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
        case 't':
            recv_timeout = atoi(optarg);
            break;
        case 'o':
            strncpy(filename, optarg, PATH_MAX);
            break;
        case -1:
            done = 1;
            break;
        case 's':
            do_stats = 1;
            quiet = 1;
            recv_timeout = -1;
            break;    
        default:
            // getopt already prints an error message for unknown
            // option characters
            usage();
            exit(1);
        }
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


static void
print_data(char* buffer, u_int length)
{
    u_int k;
    char s_buffer[BUFSIZE];
    for (k=0; k < length; k++)
    {
        if (buffer[k] >= ' ' && buffer[k] <= '~')
            s_buffer[k%BUFSIZE] = buffer[k];
        else
            s_buffer[k%BUFSIZE] = '.';

        if (k%BUFSIZE == 0) // new line every 16 bytes
        {
            printf("%07x ", k);
        }
        else if (k%2 == 0)
        {
            printf(" "); // space every 2 bytes
        }
                    
        printf("%02x", buffer[k] & 0xff);
                    
            // print character summary (a la emacs hexl-mode)
        if (k%BUFSIZE == BUFSIZE-1)
        {
            printf(" |  %.*s\n", BUFSIZE, s_buffer);
        }
    }
    
    // handle leftovers
    if ((length % BUFSIZE) != 0) {
        while (k%BUFSIZE != BUFSIZE-1) {
            if (k%2 == 0) {
                printf(" ");
            }
            printf("  ");
            k++;
        }
        printf("   |  %.*s\n",
               (int)length%BUFSIZE, 
               s_buffer);
    }
    printf("\n");
}

/* ----------------------------------------------------------------------- */
/* Builds the temporary file based on the template given and an integer
 * counter value
 */
int buildfilename(char* template, char* newfilename, int counter )
{
    char counterasstring[COUNTER_MAX_DIGITS];
    char formatstring[10];
    char* startloc;
    char* endloc;
    int templatelen;

    strcpy(newfilename, template);

    endloc = rindex(newfilename, '#');
    /* No template in the filename, just copy it as is */
    if ( endloc == NULL )
        return 0;
        
    /* Search backwards for the start of the template */
    for ( startloc = endloc; *startloc == '#' && startloc != template; 
          startloc -= 1 );

    startloc += 1;

    templatelen = endloc - startloc + 1;
    if ( templatelen > COUNTER_MAX_DIGITS )
        templatelen = COUNTER_MAX_DIGITS;

    sprintf(formatstring, "%%0%dd", templatelen);
    sprintf(counterasstring, formatstring, counter);

    if ( strlen(counterasstring) > (unsigned int)templatelen )
        fprintf(stderr, "Warning: Filename template not large enough "
                "to support counter value %d\n", counter);

    memcpy(startloc, counterasstring, sizeof(char) * templatelen);

    return 0;
}

/* ----------------------------------------------------------------------- */
/* File transfers suffer considerably from the inability to safely send 
 * metadata on the same channel as the file transfer in DTN.  Perhaps we 
 * should work around this?
 */
int
handle_file_transfer(dtn_bundle_spec_t spec, dtn_bundle_payload_t payload,
                     uint64_t* total_bytes, int counter)
{
    int tempdes;
    int destdes;
    char block[BLOCKSIZE];
    ssize_t bytesread;
    struct stat fileinfo;
    char currentfile[PATH_MAX];

    // Create the filename by searching for ### characters in the given
    // filename and replacing that with an incrementing counter.  
    buildfilename(filename, currentfile, counter);

    // Try to rename the old file to the new name to avoid unnecessary copying
    if (rename(filename, currentfile) == 0) {
        // success!

    } else {
        // Copy the file into place
        tempdes = open(payload.filename.filename_val, O_RDONLY);
        
        if ( tempdes < 0 )
        {
            fprintf(stderr, "While opening the temporary file for reading '%s': %s\n",
                    payload.filename.filename_val, strerror(errno));
            exit(1);
        }

        destdes = creat(currentfile, 0644);
        
        if ( destdes < 0 )
        {
            fprintf(stderr, "While opening output file for writing '%s': %s\n",
                    filename, strerror(errno));
            exit(1);
        }

        // Duplicate the file
        while ( (bytesread = read(tempdes, block, sizeof(block))) > 0 )
            write(destdes, block, bytesread);

        close(tempdes);
        close(destdes);
        
        unlink(payload.filename.filename_val);
    }

    if ( stat(currentfile, &fileinfo) == -1 )
    {
        fprintf(stderr, "Unable to stat destination file '%s': %s\n",
                currentfile, strerror(errno));
        return 1;
    }

    printf("%d byte file from [%s]: transit time=%d ms, written to '%s'\n",
           (int)fileinfo.st_size, spec.source.uri, 0, currentfile);        

    *total_bytes += fileinfo.st_size;

    return 0;
}

int
main(int argc, char** argv)
{
    int bundle_count;
    int bundles_this_sec;
    u_int k;
    int ret;
    uint64_t total_bytes = 0;
    dtn_handle_t handle;
    dtn_endpoint_id_t local_eid;
    dtn_reg_info_t reginfo;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    int call_bind;
    time_t now = time(0);
    time_t first_pkt_time = time(0);
    time_t elapsed;
    time_t last_rpt = time(0);;
    int last_bundle_count = 0;
    int bundles_per_sec = 0;
    double Mbps = 0.0;

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    progname = argv[0];

    parse_options(argc, argv);

    if (count == 0) { 
        printf("dtnrecv (pid %d) starting up\n", getpid());
    } else {
        printf("dtnrecv (pid %d) starting up -- count %u\n", getpid(), count);
    }

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
        reginfo.replay_flags      = replay_action;
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
        dtn_parse_eid_string(&local_eid, "dtn://" "??" "/" "?");
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

    // loop waiting for bundles
    if (count == 0) {
        printf("looping forever to receive bundles\n");
    } else {
        printf("looping to receive %d bundles\n", count);
    }
    bundle_count = 0;
    bundles_this_sec = 0;
    while ((count == 0) || (bundle_count < count)) {
        memset(&spec, 0, sizeof(spec));
        memset(&payload, 0, sizeof(payload));

        if (!quiet) {
            printf("dtn_recv [%s]...\n", local_eid.uri);
        }
   
        if (do_stats)
        {
            if ((ret = dtn_recv(handle, &spec, bundletype, &payload, 
                                                            250)) < 0)
            {
                if (DTN_ETIMEOUT !=  dtn_errno(handle)) {
                    fprintf(stderr, "error getting recv reply: %d (%s)\n",
                            ret, dtn_strerror(dtn_errno(handle)));
                    goto err;
                } else if (bundle_count != last_bundle_count) {
                    now = time(0);
                    elapsed = now - last_rpt;
                    if ( elapsed > 1) {
                        // output a summary if new bundle not received within a second
                        last_rpt = now;
                        last_bundle_count = bundle_count;
                        elapsed = now - first_pkt_time;
                        if (elapsed > 0) {
                            bundles_per_sec = bundle_count / elapsed;
                            Mbps = (total_bytes * 8.0) / 1000000.0 / elapsed;
                        }
                        printf("Summary: rcvd %d bundles  1sec: %d - elapsed: %lds  Avg Bundles/sec: %d  Data Mbps: %.3f\n", 
                               bundle_count, bundles_this_sec, elapsed, bundles_per_sec, Mbps);
                        fflush(stdout);
                        bundles_this_sec = 0;
                    }
                }
                continue;
            }

        } else  if ((ret = dtn_recv(handle, &spec, bundletype, &payload, 
                                                        recv_timeout)) < 0)
        {
            fprintf(stderr, "error getting recv reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            goto err;
        }


        // Files need to be handled differently than memory transfers
        if ( bundletype == DTN_PAYLOAD_FILE )
        {
                handle_file_transfer(spec, payload, &total_bytes, bundle_count);
                dtn_free_payload(&payload);
                bundle_count++;
                continue;
        }

        total_bytes += payload.buf.buf_len;

        if (quiet) {
            now = time(0);
 
            //dzdebug
            if (do_stats) {
                if ( 0 == bundle_count ) {
                    first_pkt_time = now;
                    last_rpt = now;
                }
                if ((0 == bundle_count) || (now > last_rpt)) {
                    last_rpt = now;
                    elapsed = now - first_pkt_time;
                    if (elapsed > 0) {
                        bundles_per_sec = bundle_count / elapsed;
                        Mbps = (total_bytes * 8.0) / 1000000.0 / elapsed;
                    }
                    printf("rcvd bundle #%d   1sec: %d - elapsed: %lds  Avg Bundles/sec: %d  Data Mbps: %.3f\n", 
                           bundle_count, bundles_this_sec, elapsed, bundles_per_sec, Mbps);
                    fflush(stdout);
                    bundles_this_sec = 0;
                }
            }

            dtn_free_payload(&payload);
            if (spec.blocks.blocks_len > 0) {
                free(spec.blocks.blocks_val);
                spec.blocks.blocks_val = NULL;
                spec.blocks.blocks_len = 0;
            }
            if (spec.metadata.metadata_len > 0) {
                free(spec.metadata.metadata_val);
                spec.metadata.metadata_val = NULL;
                spec.metadata.metadata_len = 0;
            }
            bundle_count++;
            bundles_this_sec++;
            continue;
        }

        printf("bundle spec at 0x%p\n", &spec);
        //printf("bundle spec at 0x%08X\n", &spec);

        printf("\n%d extension blocks from [%s]: transit time=%d ms\n",
               spec.blocks.blocks_len, spec.source.uri, 0);
        dtn_extension_block_t *block = spec.blocks.blocks_val;
        for (k = 0; k < spec.blocks.blocks_len; k++) {
            printf("Extension Block %i:\n", k);
            printf("\ttype = %i\n\tflags = %i\n",
                   block[k].type, block[k].flags);
            print_data(block[k].data.data_val, block[k].data.data_len);
        }

        printf("\n%d metadata blocks from [%s]: transit time=%d ms\n",
               spec.metadata.metadata_len, spec.source.uri, 0);
        dtn_extension_block_t *meta = spec.metadata.metadata_val;
        for (k = 0; k < spec.metadata.metadata_len; k++) {
            printf("Metadata Extension Block %i:\n", k);
            printf("\ttype = %i\n\tflags = %i\n",
                   meta[k].type, meta[k].flags);
            print_data(meta[k].data.data_val, meta[k].data.data_len);
        }

        printf("%d payload bytes from [%s]: transit time=%d ms\n",
               payload.buf.buf_len,
               spec.source.uri, 0);
        
        print_data(payload.buf.buf_val, payload.buf.buf_len);
        printf("\n");

        if (doAppAcks) {
            dtn_bundle_id_t id;
            printf("Acknowledging receipt of bundle to daemon.\n");
            dtn_ack(handle, &spec, &id);
        }

        dtn_free_payload(&payload);
        if (spec.blocks.blocks_len > 0) {
        	size_t i=0;
        	for ( i=0; i<spec.blocks.blocks_len; i++ ) {
        		//printf("Freeing extension block [%zu].data at 0x%08X\n",
        		printf("Freeing extension block [%zu].data at %p\n",
        			   i, &spec.blocks.blocks_val[i].data.data_val);
        	    free(spec.blocks.blocks_val[i].data.data_val);
        	}
            free(spec.blocks.blocks_val);
            spec.blocks.blocks_val = NULL;
            spec.blocks.blocks_len = 0;
        }
        if (spec.metadata.metadata_len > 0) {
        	size_t i=0;
        	for ( i=0; i<spec.metadata.metadata_len; i++ ) {
        		//printf("Freeing metadata block [%zu].data at 0x%08X\n",
        		printf("Freeing metadata block [%zu].data at %p\n",
        			   i, &spec.metadata.metadata_val[i].data.data_val);
        	    free(spec.metadata.metadata_val[i].data.data_val);
        	}
            free(spec.metadata.metadata_val);
            spec.metadata.metadata_val = NULL;
            spec.metadata.metadata_len = 0;
        }

        bundle_count++;
    }

    if (do_stats) {
        elapsed = now - first_pkt_time;
        if (elapsed > 0) {
            bundles_per_sec = bundle_count / elapsed;
            Mbps = (total_bytes * 8.0) / 1000000.0 / elapsed;
        }
        printf("Final rcvd bundle #%d 1sec: %d -  elapsed: %lds  Avg Bundles/sec: %d  Data Mbps: %.3f\n\n", 
               bundle_count, bundles_this_sec, elapsed, bundles_per_sec, Mbps);
        fflush(stdout);
    }

    printf("dtnrecv (pid %d) exiting: %d bundles received, %"PRIu64" total bytes\n\n",
           getpid(), bundle_count, total_bytes);

done:
    dtn_close(handle);
    return 0;

err:
    dtn_close(handle);
    return 1;
}
