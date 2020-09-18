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
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef DTPC_ENABLED 

#include "dtn_api.h"

const char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;
int topic = 0;
int topic_set = 0;
int use_elision_func = 0;
int elision_func_calls = 0;


void
usage()
{
    fprintf(stderr, "usage: %s [-A api_IP] [-B api_port] [-c count] [-e =use elision func] [-q =quiet] -t <topic>\n",
            progname);
    exit(1);
}

void doOptions(int argc, const char **argv);

int quiet = 0;
int count = 0;
int user_exit = 0;

#define BUFSIZE 16
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

static void 
elision_func(dtpc_data_item_list_t* data_item_list, bool_t* modified)
{
    ++elision_func_calls;

    static int insert_val = 1;
    u_int num_adis = data_item_list->data_items.data_items_len;

    if (0 == (num_adis % 4)) {
        // Example adding a new ADI to the end of the list
        ++num_adis;
        // reallocate the list of ADI pointers to the new increased size
        data_item_list->data_items.data_items_len = num_adis;
        data_item_list->data_items.data_items_val = realloc(data_item_list->data_items.data_items_val, 
                                                            sizeof(dtpc_data_item_t)*num_adis);

        // allocate the space for the ADI and load the new pointer entry
        data_item_list->data_items.data_items_val[num_adis-1].buf.buf_len = 32;
        data_item_list->data_items.data_items_val[num_adis-1].buf.buf_val = calloc(32, sizeof(char));
        snprintf(data_item_list->data_items.data_items_val[num_adis-1].buf.buf_val, 31,
                 "Elision Insertion: %d", insert_val++);
    } else if (7 == num_adis) {
        // Example reducing the number of ADIs by keeping approximately the first 1/10th of the 
        // next to last ADI and appending the last ADI data to it and then dropping the last ADI
        int orig_len = data_item_list->data_items.data_items_val[num_adis-2].buf.buf_len;
        int len_from_next_to_last_adi = (orig_len / 10) + (orig_len % 10);
        int len_from_last_adi = data_item_list->data_items.data_items_val[num_adis-1].buf.buf_len;
        int new_len = len_from_next_to_last_adi + len_from_last_adi;
        // reallocate the space for the next to last ADI to be modified
        data_item_list->data_items.data_items_val[num_adis-2].buf.buf_len = new_len;
        data_item_list->data_items.data_items_val[num_adis-2].buf.buf_val =
                realloc(data_item_list->data_items.data_items_val[num_adis-2].buf.buf_val, new_len);
        // move the data from the last ADI to the next to last ADI
        memcpy(&data_item_list->data_items.data_items_val[num_adis-2].buf.buf_val[len_from_next_to_last_adi],
               data_item_list->data_items.data_items_val[num_adis-1].buf.buf_val, len_from_last_adi);

        // free up the memory from the last ADI
        free(data_item_list->data_items.data_items_val[num_adis-1].buf.buf_val);
        data_item_list->data_items.data_items_val[num_adis-1].buf.buf_len = 0;
        data_item_list->data_items.data_items_val[num_adis-1].buf.buf_val = NULL;
        // Note that you do not need to reallocate the list of ADI pointers

        // reduce the number of ADIs in the structure
        --data_item_list->data_items.data_items_len;
        --num_adis;
    }

    printf("%s - elision function - topic: %"PRIu32", dest: %s, profile: %"PRIu32", "
           "num data items: %"PRIu32"\n", 
           progname, data_item_list->topic_id, data_item_list->dest_eid.uri,
           data_item_list->profile_id, num_adis);

    u_int ix=0;
    for (ix=0; ix<num_adis; ++ix) {
        printf("      ADI[%d] len = %u data = %-30.30s\n",
               ix+1, data_item_list->data_items.data_items_val[ix].buf.buf_len,
               data_item_list->data_items.data_items_val[ix].buf.buf_val);
    }
    fflush(stdout);

    *modified = 1;
}


static void
sigint(int sig)
{
    (void)sig;
    user_exit = 1;
}


int
main(int argc, const char** argv)
{
    int i;
    int ret;
    dtn_handle_t handle;

    dtpc_endpoint_id_t src_eid;
    dtpc_topic_id_t topic_id;
    dtpc_data_item_t data_item;
    dtpc_timeval_t timeout = 1000;

    int data_items_received = 0;
    u_int32_t total_bytes = 0;

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    doOptions(argc, argv);

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



    // DTPC testing
    // create a new DTPC Topic registration
    dtpc_reg_info_t dtpcreginfo;
    memset(&dtpcreginfo, 0, sizeof(dtpcreginfo));
    dtpcreginfo.topic_id = topic;
    dtpcreginfo.has_elision_func = use_elision_func;
    if ((ret = dtpc_register(handle, &dtpcreginfo)) != 0) {
        fprintf(stderr, "error creating DTPC topic registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        dtn_close(handle);
        exit(1);
    }    

    //CTRL+C handling
    signal(SIGINT, &sigint);

    printf("dtpc_register succeeded - wait for application data items...\n");

    // loop waiting for bundles
    if (count == 0) {
        printf("looping forever to receive Application Data Items\n");
    } else {
        printf("looping to receive %d Application Data Items\n", count);
    }
    for (i = 0; !user_exit && ((count == 0) || (i < count)); ++i) {
        memset(&src_eid, 0, sizeof(src_eid));
        memset(&data_item, 0, sizeof(data_item));

        if ((ret = dtpc_recv(handle, &data_item, &topic_id, &src_eid,
                             timeout, &elision_func)) < 0)
        {
            if (DTN_ETIMEOUT != dtn_errno(handle)) {
                fprintf(stderr, "error getting dtpc_recv reply: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
                break;
            }

            continue; // timeout so we can check for user exit
        }

        ++data_items_received;
        total_bytes += data_item.buf.buf_len;

        printf("ADI #%d - Topic %"PRIu32" from %s  - %d bytes: \n",
               data_items_received, topic_id, src_eid.uri, data_item.buf.buf_len);
        
        if (!quiet) {
            // XXX - you may want to cap the number of bytes printed
            print_data(data_item.buf.buf_val, data_item.buf.buf_len);
        }
        printf("\n");
    }

    printf("\n\n%s (pid %d) exiting: %d ADIs received, %"PRIu32" total bytes - Elision function calls: %d\n\n",
           progname, getpid(), data_items_received, total_bytes, elision_func_calls);

    if ((ret = dtpc_unregister(handle, topic)) != 0) {
        fprintf(stderr, "error unregistering from DTPC topic: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        dtn_close(handle);
        exit(1);
    }    

    printf("dtpc_unregister succeeded - exiting\n");
    dtn_close(handle);

    return 0;
}

void
doOptions(int argc, const char **argv)
{
    int c;

    progname = argv[0];

    while ( (c=getopt(argc, (char **) argv, "A:B:heqc:t:")) !=EOF ) {
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
        case 't':
            topic = atoi(optarg);
            topic_set = 1;
            break;
        case 'e':
            use_elision_func = 1;
            break;
        case 'h':
            usage();
            break;
        case 'q':
            quiet = 1;
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
