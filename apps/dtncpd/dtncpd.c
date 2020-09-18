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
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "dtn_api.h"

#define BUFSIZE 16
#define BUNDLE_DIR_DEFAULT INSTALL_LOCALSTATEDIR "/dtn/dtncpd-incoming"

static const char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;

void
usage()
{
    fprintf(stderr, "usage: %s [-A api_IP_address] [-B api_port] [ directory ]\n", progname);
    fprintf(stderr, "    optional directory parameter is where incoming "
                    "files will get put\n");
    fprintf(stderr, "    (defaults to: %s)\n", BUNDLE_DIR_DEFAULT);
    exit(1);
}

int
main(int argc, const char** argv)
{
    int i;
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t local_eid;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t bundle;
    dtn_bundle_payload_t payload;
    const char* endpoint_demux;
    int debug = 1;

    char * bundle_dir = 0;

    char host[PATH_MAX];
    int host_len;
    char * dirpath;
    char * filename;
    char filepath[PATH_MAX];
    time_t current;

    char * buffer;
    char s_buffer[BUFSIZE + 1];

    int bufsize, marker, maxwrite;

    FILE * target;

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    s_buffer[BUFSIZE] = '\0';

    struct stat st;

    progname = argv[0];
    
    if (argc > 6) usage();

    if (argc > 1 && strcmp(argv[1], "-A") == 0)  
    {      
        argv++;
        argc--;        
        if (argc < 2) usage();
        api_IP=(char*)argv[1];
        api_IP_set = 1;
        argv++;
        argc--;        
    }        

    if (argc > 1 && strcmp(argv[1], "-B") == 0)        
    {   
        argv++;
        argc--;        
        if (argc < 2) usage();
        api_port=atoi(argv[1]);
        argv++;
        argc--;        
    }  

    if (argc > 1 && argv[1] != NULL) {
        bundle_dir = (char *) argv[1];
    }
    else
    {
        bundle_dir = BUNDLE_DIR_DEFAULT;
    }

    if (access(bundle_dir, W_OK | X_OK) == -1) {
        fprintf(stderr, "can't access directory '%s': %s\n",
                bundle_dir, strerror(errno));
        usage();
    }

    if (stat(bundle_dir, &st) == -1) { 
        fprintf(stderr, "can't stat directory '%s': %s\n",
                bundle_dir, strerror(errno));
        usage();
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "path '%s' is not a directory\n",
                bundle_dir);
        usage();
    }

    // open the ipc handle
    if (debug) printf("opening connection to dtn router...\n");
 
    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }

    // build a local eid based on the configuration of our dtn
    // router plus the demux string
    endpoint_demux = "/dtncp/recv?file=*";
    dtn_build_local_eid(handle, &local_eid, endpoint_demux);
    if (debug) printf("local_eid [%s]\n", local_eid.uri);

    // try to find an existin registration, or create a new
    // registration based on this eid
    ret = dtn_find_registration(handle, &local_eid, &regid);
    if (ret == 0) {
        if (debug) printf("dtn_find_registration succeeded, regid 0x%x\n",
                          regid);
        
        // bind the current handle to the new registration
        dtn_bind(handle, regid);
        
    } else if (dtn_errno(handle) == DTN_ENOTFOUND) {
        memset(&reginfo, 0, sizeof(reginfo));
        dtn_copy_eid(&reginfo.endpoint, &local_eid);
        reginfo.flags = DTN_REG_DEFER;
        reginfo.regid = DTN_REGID_NONE;
        reginfo.expiration = 60 * 60;
        if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
            fprintf(stderr, "error creating registration: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }
    
        if (debug) printf("dtn_register succeeded, regid 0x%x\n", regid);

    } else {
        fprintf(stderr, "error in dtn_find_registration: %s",
                dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    
    // loop waiting for bundles
    while(1)
    {
        // change this to _MEM here to receive into memory then write the file
        // ourselves. (So this code shows both ways to do it.)
        // XXX/demmer better would be to have a cmd line option
        dtn_bundle_payload_location_t file_or_mem = DTN_PAYLOAD_FILE;

        memset(&bundle, 0, sizeof(bundle));
        memset(&payload, 0, sizeof(payload));
        memset(&dirpath, 0, sizeof(dirpath));
        memset(&filename, 0, sizeof(filename));
        memset(&filepath, 0, sizeof(filepath));
        memset(&host, 0, sizeof(host));
        memset(&st, 0, sizeof(st));
        
        printf("dtn_recv [%s]...\n", local_eid.uri);
    
        if ((ret = dtn_recv(handle, &bundle,
                            file_or_mem, &payload, -1)) < 0)
        {
            fprintf(stderr, "error getting recv reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        // mark time received
        current = time(NULL);

        if (strncmp(bundle.source.uri, "dtn://", 6) != 0)
        {
            fprintf(stderr, "bundle source uri '%s' must be a dtn:// uri\n",
                    bundle.source.uri);
            exit(1);
        }

        // grab the sending authority and service tag (i.e. the path)
        host_len = strchr(&bundle.source.uri[6], '/') - &bundle.source.uri[6];
        strncpy(host, &bundle.source.uri[6], host_len);
        
        // extract directory from destination path (everything
        // following std demux, except the '*')
        endpoint_demux = "/dtncp/recv?file=";
        dirpath = strstr(bundle.dest.uri, endpoint_demux);
        if (!dirpath) {
            fprintf(stderr, "can't find %s demux in uri '%s'\n",
                    endpoint_demux, bundle.dest.uri);
            exit(1);
        }
        
        dirpath += strlen(endpoint_demux); // skip std demux
        if (dirpath[0] == '/') dirpath++; // skip leading slash

        // filename is everything following last /
        filename = strrchr(dirpath, '/');
        if (filename == 0)
        {
            filename = dirpath;
            dirpath = "";
        }
        else
        {
            filename[0] = '\0'; // null terminate path
            filename++; // next char;
        }

        // recursively create full directory path
        // XXX/demmer system -- yuck!
        sprintf(filepath, "mkdir -p %s/%s/%s", bundle_dir, host, dirpath);
        system(filepath);
        
        // create file name
        sprintf(filepath, "%s/%s/%s/%s", bundle_dir, host, dirpath, filename);
        
        // bundle name is the name of the bundle payload file
        buffer = payload.filename.filename_val;
        // bufsize is the length of the payload file (not what we want to print)
        bufsize = payload.filename.filename_len;
        // st contains (among other things) size of payload file
        ret = stat(buffer, &st);

        printf ("======================================\n");
        printf (" File Received at %s\n", ctime(&current));
        printf ("   host   : %s\n", host);
        printf ("   path   : %s\n", dirpath);
        printf ("   file   : %s\n", filename);
        printf ("   loc    : %s\n", filepath);
        printf ("   size   : %d bytes\n", (int) st.st_size);
        

        if (file_or_mem == DTN_PAYLOAD_FILE) {
            int cmdlen = 5 + strlen(buffer) + strlen(filepath);
            char *cmd = malloc(cmdlen);

            if (cmd) {
                snprintf(cmd, cmdlen, "mv %.*s %s", bufsize, buffer,
                         filepath);
                system(cmd);
                printf("Moving payload to final filename: '%s'\n", cmd);
                free(cmd);
            } else {
                printf("Out of memory. Find file in %*s.\n", bufsize,
                        buffer);
            }
        } else {

            target = fopen(filepath, "w");

            if (target == NULL)
            {
                fprintf(stderr, "Error opening file for writing %s\n",
                         filepath);
                continue;
            }
            if (debug) printf ("--------------------------------------\n");
        
            marker = 0;
            while (marker < bufsize)
            {
                // write 256 bytes at a time
                i=0;
                maxwrite = (marker + 256) > bufsize? bufsize-marker : 256;
                while (i < maxwrite)
                {
                    i += fwrite(buffer + marker + i, 1, maxwrite - i, target);
                }
            
                if (debug)
                {
                    for (i=0; i < maxwrite; i++)
                    {
                        if (buffer[marker] >= ' ' && buffer[marker] <= '~')
                            s_buffer[marker%BUFSIZE] = buffer[i];
                        else
                            s_buffer[marker%BUFSIZE] = '.';
                    
                        if (marker%BUFSIZE == 0) // new line every 16 bytes
                        {
                            printf("%07x ", marker);
                        }
                        else if (marker%2 == 0)
                        {
                            printf(" "); // space every 2 bytes
                        }
                    
                        printf("%02x", buffer[i] & 0xff);
                    
                        // print character summary (a la emacs hexl-mode)
                        if (marker%BUFSIZE == BUFSIZE-1)
                        {
                            printf(" |  %s\n", s_buffer);
                        }
                        marker ++;
                    }
                }
                else
                {
                    marker += maxwrite;
                }
            }
    
            fclose(target);
    
            // round off last line
            if (debug && marker % BUFSIZE != 0)
            {
                while (marker % BUFSIZE !=0)
                {
                    s_buffer[marker%BUFSIZE] = ' ';
    
                    if (marker%2 == 0)
                    {
                        printf(" "); // space every 2 bytes
                    }
                        
                    printf("  ");
                        
                    // print character summary (a la emacs hexl-mode)
                    if (marker%BUFSIZE == BUFSIZE-1)
                    {
                        printf(" |  %s\n", s_buffer);
                    }
                    marker ++;
                }
            }
            printf ("   size   : %d bytes\n", bufsize);
        }
    
        printf ("======================================\n");
        dtn_free_payload(&payload);
    }

    dtn_close(handle);
    
    return 0;
}
