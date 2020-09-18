/*
 *    Copyright 1996-2006 Intel Corporation
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
 * moteproxy: a DTN <-> mote proxying app, intended for use with GSK
 *   for MICA-2, use 57600 baud rate
 */

/* Modified to recognize Mote-PC protocol -- Mark Thomas (23/06/04) 
 * Now uses serialsource.c to communicate with serial port 
 * Files: crc16.c, misc.c, mote_io.c and mote_io.h are not used anymore */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include "serialsource.h"

#include <strings.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <oasys/compat/inttypes.h>

#include "dtn_api.h"
 
#define	dout	stderr

#include <ctype.h>

char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;

static char *msgs[] = {
  "unknown_packet_type",
  "ack_timeout"	,
  "sync"	,
  "too_long"	,
  "too_short"	,
  "bad_sync"	,
  "bad_crc"	,
  "closed"	,
  "no_memory"	,
  "unix_error"
};

typedef struct data_packet
{
    // MultiHop Header 
    u_int16_t source_mote_id;
    u_int16_t origin_mote_id;
    u_int16_t seq_no;
    u_int8_t hop_cnt;

    // Surge Sensor Header 
    u_int8_t surge_pkt_type;
    u_int16_t surge_reading;
    u_int16_t surge_parent_addr;
    u_int32_t surge_seq_no;
    u_int8_t light;
    u_int8_t temp;
    u_int8_t magx;
    u_int8_t magy;
    u_int8_t accelx;
    u_int8_t accely;

}DATAPACKET;

#define DATAPACKET_SIZE 22
#define SURGE_PKT	0x11
#define DEBUG_PKT	0x03

void parse_options(int, char**);
dtn_endpoint_id_t * parse_eid(dtn_handle_t handle, dtn_endpoint_id_t * eid, 
                          char * str);
void print_usage();
void print_eid(char * label, dtn_endpoint_id_t * eid);
void init_motes();
void stderr_msg(serial_source_msg problem);
void usage(char *str1, char *str2);
void readCommandLineArgs(int argc, char **argv);
void hexdump();
void read_packet_file(char* filename);

// specified options
char arg_dest[128];
char arg_target[128];

char devicename[128] = "/dev/ttyS0";
char baud[128] = "57600";
char directory[128]="send";
u_int32_t debug = 0;             // higher values cause more info to print
serial_source_ptr src;

int g_argc;
char **g_argv;

int
main(int argc, char **argv)
{
    /* save in case of crash */
    g_argc = argc;
    g_argv = argv;

    readCommandLineArgs(argc, argv);
    init_motes();

    // NOTREACHED 
    return (EXIT_FAILURE);      // should never get here 
}

void stderr_msg(serial_source_msg problem)
{
  fprintf(stderr, "Note: %s\n", msgs[problem]);
}

int read_packet(char *buf, int *n)
{
    const char *buff;
    int i;
    
    if (debug > 0) fprintf(stdout, "Reading packet:\n");
    
    if(!(buff = read_serial_packet(src, n)))
	return 0;

    if (debug > 0) fprintf(stdout, " ==> : ");
    for (i = 0; i < buff[4]; i++)
        printf(" %02x", buff[i]);
    putchar('\n');

   
    // strip TOS header & copy to buf 
    memset(buf,0,BUFSIZ);
    memcpy(buf,buff,buff[4]+5);
    *n=buff[4] + 5;
    if(buff[2]==SURGE_PKT || buff[2]==DEBUG_PKT) return buff[2];
    return -1;
}

int
reader_thread(void *p)
{
    (void) p;

    // loop reading from motes, writing to directory

    static int tcnt=0;
    DATAPACKET *dataPacket;

    // dtn api variables
    int ret;
    dtn_handle_t handle;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid = DTN_REGID_NONE;
    dtn_bundle_spec_t bundle_spec;
    dtn_bundle_payload_t send_payload;
    dtn_bundle_id_t bundle_id;
    char demux[4096];

    //p = NULL;

    // open the ipc handle
    if (debug > 0) fprintf(stdout, "Opening connection to local DTN daemon\n");

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
    sprintf(demux, "%s/dtnmoteproxy/recv", arg_dest);
    parse_eid(handle, &bundle_spec.dest, demux);

    // source is local eid with file path as demux string
    sprintf(demux, "/dtnmoteproxy/send");
    parse_eid(handle, &bundle_spec.source, demux);

    // reply to is the same as the source
    dtn_copy_eid(&bundle_spec.replyto, &bundle_spec.source);


    if (debug > 2)
    {
        print_eid("source_eid", &bundle_spec.source);
        print_eid("replyto_eid", &bundle_spec.replyto);
        print_eid("dest_eid", &bundle_spec.dest);
    }

    // set the return receipt option
    bundle_spec.dopts |= DOPTS_DELIVERY_RCPT;

    // send file and wait for reply

    // create a new dtn registration to receive bundle status reports
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &bundle_spec.replyto);
    reginfo.flags = DTN_REG_DEFER;
    reginfo.regid = regid;
    reginfo.expiration = 0;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration (id=%d): %d (%s)\n",
                regid, ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    
    if (debug > 3) printf("dtn_register succeeded, regid 0x%x\n", regid);

    while (1) {
        static unsigned char motedata[BUFSIZ];
	int length;
	int ret;

        if (debug > 1) fprintf(dout, "about to read from motes...\n");

	while((ret=read_packet((char *) motedata, (int *) &length))){
	    if(ret==DEBUG_PKT)
		continue;
            if (debug > 0) {
                fprintf(dout, "\nreader loop... got [%d] bytes from motes\n", 
                        length);
                if (debug > 1) hexdump(motedata, length);
            }
	   
            // the extra cast to void* is needed to circumvent gcc warnings
            // about unsafe casting 
	    dataPacket=(DATAPACKET *)((void*)motedata);
	    
	    // skip packets from base mote 
	    if(dataPacket->origin_mote_id == 0) continue;

            // set a default expiration time of one hour
            bundle_spec.expiration = 3600;
            
            // fill in a payload
            memset(&send_payload, 0, sizeof(send_payload));

            dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM,
                            (char *) motedata, length);
 
            memset(&bundle_id, 0, sizeof(bundle_id));
                        
            if ((ret = dtn_send(handle, regid, &bundle_spec, &send_payload,
                                &bundle_id)) != 0)
            {
                fprintf(stderr, "error sending bundle: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
            }   
            else fprintf(stderr, "motedata bundle sent");

	    printf("Mote ID = %u\n",dataPacket->origin_mote_id);
	    printf("Source Mote ID = %u\n",dataPacket->source_mote_id);
	    printf("Hop Count = %u\n",dataPacket->hop_cnt);
	    printf("Packet Type = %u\n",dataPacket->surge_pkt_type);
	    printf("Parent Address = %u\n",dataPacket->surge_parent_addr);
	    printf("Sequence Number = %u\n", (u_int)dataPacket->surge_seq_no);
	    printf("Light = %u\n",dataPacket->light);
	    printf("Temperature = %u\n\n",dataPacket->temp);	    

	    tcnt=(tcnt+1)%10000;
	  
        }
        if (debug > 0)
            fprintf(dout, "reader loop.... nothing to do? [shouldn't happen]\n");
    }

    // if this was ever changed to gracefully shutdown, it would be good to call:
    dtn_close(handle);
    
    return (1);
    // NOTREACHED 
}


void
readCommandLineArgs(int argc, char **argv)
{
    int c;

    while ((c = getopt(argc, argv, "A:B:hr:d:b:D:t:")) != EOF) {
        switch (c) {
        case 'A':
            api_IP_set = 1;
            api_IP = optarg;
            break;
        case 'B':
            api_IP_set = 1;
            api_port = atoi(optarg);
            break;    
        case 'h':
            usage("moteproxy", "");
            exit(0);
            break;
        case 'r':
            read_packet_file(optarg);
            exit(0);
        case 'd':
            debug = atoi(optarg);
            break;
        case 'b':
            strcpy(baud, optarg);
            break;
        case 't':
            strcpy(devicename, optarg);
            break;
	case 'D':
	    strcpy(arg_dest,optarg);
	    break;		
        default:
            fprintf(stderr, "mfproxy: unknown option: '%c'\n", (char) c);
            usage("moteproxy", "");
            exit(EXIT_FAILURE);
        }
    }
}

void
usage(char *str1, char *str2)
{
    (void)str2;
    
    fprintf(stderr, "usage: %s\n", str1);
    fprintf(stderr, " -A daemon api IP address\n");
    fprintf(stderr, " -B daemon api IP port\n");
    fprintf(stderr, "  [-b baudrate]     - baud rate\n");
    fprintf(stderr, "  [-t devicename]      - name of mote network dev tty\n");
    fprintf(stderr, "  [-d debugValue]\n");
    fprintf(stderr, "  [-D directory]\n");
    fprintf(stderr, "  [-h]              - print this message.\n");
    fprintf(stderr, "\n");
}


// initialize the motes
void
init_motes()
{
    src = open_serial_source(devicename, atoi(baud), 0, stderr_msg);

    if(reader_thread(NULL) == 1) {
        fprintf(stderr, "couldn't start reader on mote network\n");
        exit(EXIT_FAILURE);
    }
    return;
}

void read_packet_file(char* filename)
{
    int fd = open(filename, O_RDONLY);
    static unsigned char buf[BUFSIZ];
    int n = read(fd, buf, BUFSIZ);
    hexdump(buf, n);
}

void
hexdump(unsigned char *buf, int n)
{
    int i;
    unsigned char *p = buf;

    fprintf(dout,"Packet contains %d:\n",n);
    for (i = 0; i < n; i++) {
        fprintf(dout,"%02x ", *p++);
        if ((i & 0x7) == 0x7)
            fprintf(dout,"\n");
    }
    printf("\n\n");
    fflush(stdout);
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
        fprintf(stderr, "invalid eid string '%s'\n", str);
        exit(1);
    }
}

void print_eid(char* label, dtn_endpoint_id_t* eid)
{
    printf("%s [%s]\n", label, eid->uri);
}
    
