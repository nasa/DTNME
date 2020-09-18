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
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "dtn_api.h"

char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;

int verbose             = 1;

char data_source[1024]; // filename or message, depending on type

// specified options
char * arg_dest         = NULL;
char * arg_target       = NULL;

int    expiration_time  = 60 * 60; // default is 1 hour
int delivery_receipts   = 0;    // request end to end delivery receipts

void parse_options(int, char**);
dtn_endpoint_id_t * parse_eid(dtn_handle_t handle, dtn_endpoint_id_t * eid, 
                          char * str);
void print_usage();
void print_eid(char * label, dtn_endpoint_id_t * eid);

int
main(int argc, char** argv)
{
    int ret;
    dtn_handle_t handle;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid = DTN_REGID_NONE;
    dtn_bundle_spec_t bundle_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t send_payload;
    dtn_bundle_payload_t reply_payload;
    dtn_bundle_id_t bundle_id;
    char demux[4096];
    struct timeval start, end;

/*     FILE * file; */
/*     //struct stat finfo; */
/*     char buffer[4096]; // max filesize to send is 4096 (temp) */
/*     int bufsize = 0; */

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    parse_options(argc, argv);

    // open the ipc handle
    if (verbose) fprintf(stdout, "Opening connection to local DTN daemon\n");

    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }


    // ----------------------------------------------------
    // initialize bundle spec with src/dest/replyto
    // ----------------------------------------------------

    // initialize bundle spec
    memset(&bundle_spec, 0, sizeof(bundle_spec));

    // destination host is specified at run time, demux is hardcoded
    sprintf(demux, "%s/dtncp/recv?file=%s", arg_dest, arg_target);
    parse_eid(handle, &bundle_spec.dest, demux);

    // source is local eid with file path as demux string
    sprintf(demux, "/dtncp/send?source=%s", data_source);
    parse_eid(handle, &bundle_spec.source, demux);

    if (verbose)
    {
        print_eid("source_eid", &bundle_spec.source);
        print_eid("dest_eid", &bundle_spec.dest);
    }

    // set the expiration time (one hour)
    bundle_spec.expiration = expiration_time;
    
    if (delivery_receipts)
    {
        // set the delivery receipt option
        bundle_spec.dopts |= DOPTS_DELIVERY_RCPT;
    }

    // fill in a payload
    memset(&send_payload, 0, sizeof(send_payload));

    dtn_set_payload(&send_payload, DTN_PAYLOAD_FILE,
        data_source, strlen(data_source));
     
    // send file and wait for reply

    // create a new dtn registration to receive bundle status reports
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &bundle_spec.source);
    reginfo.flags = DTN_REG_DEFER;
    reginfo.regid = regid;
    reginfo.expiration = 0;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration (id=%d): %d (%s)\n",
                regid, ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    
    if (verbose) printf("dtn_register succeeded, regid 0x%x\n", regid);

    gettimeofday(&start, NULL); // timer

    memset(&bundle_id, 0, sizeof(bundle_id));
                
    if ((ret = dtn_send(handle, regid, &bundle_spec, &send_payload,
                        &bundle_id)) != 0) {
        fprintf(stderr, "error sending file bundle: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }

    if (delivery_receipts)
      {
	memset(&reply_spec, 0, sizeof(reply_spec));
	memset(&reply_payload, 0, sizeof(reply_payload));
	
	// now we block waiting for the echo reply
	if ((ret = dtn_recv(handle, &reply_spec,
			    DTN_PAYLOAD_MEM, &reply_payload, -1)) < 0)
	  {
	    fprintf(stderr, "error getting reply: %d (%s)\n",
		    ret, dtn_strerror(dtn_errno(handle)));
	    exit(1);
	  }
	gettimeofday(&end, NULL);


	printf("file sent successfully to [%s]: time=%.1f ms\n",
	       reply_spec.source.uri,
	       ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
		(double)(end.tv_usec - start.tv_usec)/1000.0));
	
	dtn_free_payload(&reply_payload);
      } 
    else 
      {
	printf("file sent to [%s]\n",
	       bundle_spec.dest.uri);
      }

    dtn_close(handle);
    
    return 0;
}

void print_usage()
{
    fprintf(stderr,
            "usage: %s [-A api IP address] [-B api port] [-D] [--expiration sec] <filename> <destination_eid> <remote-name>\n", 
            progname);
    fprintf(stderr,
            "    Remote filename is optional; defaults to the "
            "local filename.\n\n"
	    "-D disables acknowledgements\n"
            "Bundle expiration time is in seconds.\n");
    
    exit(1);
}

void parse_options(int argc, char**argv)
{
    progname = argv[0];

    if (argc < 2)
        goto bail;
    if (strcmp(argv[1], "-A") == 0)  
    {      
        argv++;
        argc--;        

        if (argc < 2)
            goto bail;

        api_IP=argv[1];
        api_IP_set = 1;
        argv++;
        argc--;        
    }        

    if (argc < 2)
        goto bail;
    if (strcmp(argv[1], "-B") == 0)        
    {   
        argv++;
        argc--;        

        if (argc < 2)
            goto bail;

        api_IP_set = 1;
        api_port=atoi(argv[1]);
        argv++;
        argc--;        
    }        
        
    
    // expiration time in seconds
    if (argc < 2)
        goto bail;
    if (strcmp(argv[1], "--expiration") == 0)
    {
        argv++;
        argc--;
        
        if (argc < 2)
            goto bail;
        
        expiration_time = atoi(argv[1]);
        if (expiration_time == 0)
        {
            fprintf(stderr, 
                    "Expiration time must be > 0\n");
	    exit(1);
	    }

        argv++;
        argc--;
    }

    // no reply
    if (argc < 2)
        goto bail;
    if (strcmp(argv[1], "-D") == 0)
      {
	delivery_receipts = 1;
	
        argv++;
        argc--;
      }

    // parse the normal arguments
    if (argc < 3)
        goto bail;

    if (argv[1][0] == '/') sprintf(data_source, "%s", argv[1]);
    else sprintf(data_source, "%s/%s", getenv("PWD"), argv[1]);

    arg_dest = argv[2];
    if (argc > 3)
    {
        arg_target = argv[3];
    }
    else 
    {
        arg_target = strrchr(data_source, '/');
        if (arg_target == 0) arg_target = data_source;
    }

    return;

  bail:
        print_usage();
        exit(1);
}

dtn_endpoint_id_t * parse_eid(dtn_handle_t handle, 
                          dtn_endpoint_id_t * eid, char * str)
{
    
    // try the string as an actual dtn eid
    if (!dtn_parse_eid_string(eid, str)) 
    {
        return eid;
    }
    // build a local eid based on the configuration of our dtn
    // router plus the str as demux string
    else if (!dtn_build_local_eid(handle, eid, str))
    {
        return eid;
    }
    else
    {
        fprintf(stderr, "invalid endpoint id string '%s'\n", str);
        exit(1);
    }
}

void print_eid(char *  label, dtn_endpoint_id_t * eid)
{
    printf("%s [%s]\n", label, eid->uri);
}
    



