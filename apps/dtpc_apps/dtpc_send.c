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
#include <sys/time.h>
#include <signal.h>

#ifdef DTPC_ENABLED

#include "dtn_api.h"

const char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;
int profile = 0;
int profile_set = 0;
int topic = 0;
int topic_set = 0;

void
usage()
{
    fprintf(stderr, "usage: %s [-A api_IP][-B api_port][-c count][-i interval (ms)] -p <profile> -t <topic> eid\n",
            progname);
    exit(1);
}

void doOptions(int argc, const char **argv);

int interval = 1000; // default sleep is 1 second
int count = 15;
char dest_eid_str[DTPC_MAX_ENDPOINT_ID] = "";
int user_exit = 0;

static void
sigint(int sig)
{
    (void)sig;
    user_exit = 1;
}


int
main(int argc, const char** argv)
{
    int ret;
    dtn_handle_t handle;

    struct timeval now, last_status;
    int data_items_sent = 0;
    u_int32_t total_bytes = 0;


    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    doOptions(argc, argv);

    if (!profile_set) {
        fprintf(stderr, "fatal error - profile not specified\n");
        usage();
        exit(1);
    }

    if (!topic_set) {
        fprintf(stderr, "fatal error - topic not specified\n");
        usage();
        exit(1);
    }

    // open the ipc handle
    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }

    //CTRL+C handling
    signal(SIGINT, &sigint);

    // convert the interval from millisecs to microsecs
    interval *= 1000;

    // send a data item
    dtpc_data_item_t data_item;
    dtpc_profile_id_t profile_id = profile;
    dtpc_topic_id_t topic_id = topic;
    dtpc_endpoint_id_t dest_eid;
    memset(dest_eid.uri, 0, sizeof(dest_eid.uri));
    strncpy(dest_eid.uri, dest_eid_str, sizeof(dest_eid.uri)-1);

    int ix = 0;
    char sample_data_item[4096];
    memset(sample_data_item, 0, sizeof(sample_data_item));
    char letter = 'A';

    gettimeofday(&last_status, 0);

    for (ix=0; (!user_exit && (count == 0 || ix < count)); ++ix) {
        memset(sample_data_item, letter++, 99);
        if (letter > 'z') letter = 'A';
        data_item.buf.buf_len = 100;
        data_item.buf.buf_val = sample_data_item;
        topic_id = topic;

        if ((ret = dtpc_send(handle, &data_item, &topic_id, &dest_eid, profile_id)) != 0) {
            fprintf(stderr, "error in send Data Item for topic: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            break;
        }
        ++data_items_sent;
        total_bytes += data_item.buf.buf_len;

        gettimeofday(&now, 0);
        if ((now.tv_sec - last_status.tv_sec) >= 5) {
            printf("\rADIs sent: %d        ", ix);
            fflush(stdout);
            last_status.tv_sec = now.tv_sec;
        }
        usleep(interval);
    }

    dtn_close(handle);

    printf("\n\n%s (pid %d) exiting: %d ADIs transmitted, %"PRIu32" total bytes\n\n",
           progname, getpid(), data_items_sent, total_bytes);

    return 0;
}

void
doOptions(int argc, const char **argv)
{
    int c;

    progname = argv[0];

    while ( (c=getopt(argc, (char **) argv, "A:B:hc:i:d:p:t:")) !=EOF ) {
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
        case 'i':
            interval = atoi(optarg);
            break;
        case 'd':
            strcpy(dest_eid_str, optarg);
            break;
        case 'p':
            profile = atoi(optarg);
            profile_set = 1;
            break;
        case 't':
            topic = atoi(optarg);
            topic_set = 1;
            break;
        case 'h':
            usage();
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

#else // DTPC_ENABLED

int
main(int argc, const char** argv)
{
    (void) argc;
    (void) argv;
    printf("DTPC was not enabled for this build. Configure using the option --with-dtpc\n");
    return 0;
}
#endif // DTPC_ENABLED

