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
 *    Modifications made to this file by the patch file dtn2_mfs-33289-1.patch
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

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#ifdef __FreeBSD__
/* Needed for PATH_MAX, Linux doesn't need it */
#include <sys/syslimits.h>
#endif

#ifndef PATH_MAX
/* A conservative fallback */
#define PATH_MAX 1024
#endif

#include <vector>

#include "dtn_api.h"
#include "sdnv-c.h"
#include <sys/types.h>

static volatile int user_abort = 0;
u_int32_t TIMEVAL_CONVERSION_SECS = 946684800;

char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = (char*) "127.0.0.1";
short api_port = 5010;

// global options
int copies              = 1;    // the number of copies to send
int verbose             = 0;
int sleep_time          = 0;
int wait_for_report     = 0;    // wait for bundle status reports
int age                 = 0;
int bp_version          = 0;
bool simulate_bpdriver  = false;

// bundle options
int expiration                 = 3600; // expiration timer (default one hour)
int delivery_options           = 0;    // bundle delivery option bit vector
dtn_bundle_priority_t priority = COS_NORMAL; // bundle priority

// payload options
dtn_bundle_payload_location_t
payload_type        = DTN_PAYLOAD_FILE; // type of payload
char * data_source  = NULL;             // file or message, depending on type
char date_buf[256];                     // buffer for date payloads

// extension/metatdata block information
class ExtBlock {
public:
    ExtBlock(u_int type = 0): metadata_(false) {
        block_.type          = type;
        block_.flags         = 0;
        block_.data.data_len = 0;
        block_.data.data_val = NULL;
    }
    ~ExtBlock() {
        if (block_.data.data_val != NULL) {
            free(block_.data.data_val);
            block_.data.data_val = NULL;
            block_.data.data_len = 0;
        }
    }

    ExtBlock(const ExtBlock& o)
    {
        metadata_            = o.metadata_;
        block_.type          = o.block_.type;
        block_.flags         = o.block_.flags;
        block_.data.data_len = o.block_.data.data_len;
        block_.data.data_val = (char*)malloc(block_.data.data_len);
        memcpy(block_.data.data_val, o.block_.data.data_val,
               block_.data.data_len);
    }
    
    bool        metadata() const { return metadata_; }
    void        set_metadata()   { metadata_ = true; }

    dtn_extension_block_t & block() { return block_; }
    void set_block_buf(char * buf, u_int len) {
        if (block_.data.data_val != NULL) {
            free(block_.data.data_val);
            block_.data.data_val = NULL;
            block_.data.data_len = 0;
        }
        block_.data.data_val = buf;
        block_.data.data_len = len;
    }

    static unsigned int   num_meta_blocks_;

private:
    bool                  metadata_;
    dtn_extension_block_t block_;
};
unsigned int ExtBlock::num_meta_blocks_ = 0;

std::vector<ExtBlock> ext_blocks;

// specified options for bundle eids
char * arg_replyto      = NULL;
char * arg_source       = NULL;
char * arg_dest         = NULL;

dtn_reg_id_t regid      = DTN_REGID_NONE;


void parse_options(int, char**);
dtn_endpoint_id_t * parse_eid(dtn_handle_t handle,
                              dtn_endpoint_id_t * eid,
                              char * str);
void print_usage();
void print_eid(const char * label, dtn_endpoint_id_t * eid);
void fill_payload(dtn_bundle_payload_t* payload);
void log_bpv6_status_report(dtn_bundle_status_report_t* rpt);
void log_bpv7_status_report(dtn_bundle_status_report_t* rpt);

void CtrlCTrap(int dummy)
{
    (void) dummy;
    user_abort = 1;
}

int
main(int argc, char** argv)
{
    int i;
    int ret;
    
    dtn_handle_t handle;
    dtn_reg_info_t reginfo;
    dtn_bundle_spec_t bundle_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_id_t bundle_id;
    dtn_bundle_payload_t send_payload;
    dtn_bundle_payload_t reply_payload;
    struct timeval start, end;
    
    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    parse_options(argc, argv);


    signal(SIGINT, CtrlCTrap);


    // open the ipc handle
    if (verbose) fprintf(stdout, "Opening connection to local DTN daemon\n");
    
    int err = 0;

    if (api_IP_set || api_port != 5010)
        err = dtn_open_with_IP(api_IP,api_port,&handle);
    else  
        err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }

    // initialize bundle spec
    memset(&bundle_spec, 0, sizeof(bundle_spec));

    bundle_spec.bp_version = bp_version;

    // initialize/parse bundle src/dest/replyto eids
    if (verbose) fprintf(stdout, "Destination: %s\n", arg_dest);
    parse_eid(handle, &bundle_spec.dest, arg_dest);

    if (verbose) fprintf(stdout, "Source: %s\n", arg_source);
    parse_eid(handle, &bundle_spec.source, arg_source);
    if (arg_replyto == NULL) 
    {
        if (verbose) fprintf(stdout, "Reply To: same as source\n");
        dtn_copy_eid(&bundle_spec.replyto, &bundle_spec.source);
    }
    else
    {
        if (verbose) fprintf(stdout, "Reply To: %s\n", arg_replyto);
        parse_eid(handle, &bundle_spec.replyto, arg_replyto);
    }

    if (verbose)
    {
        print_eid("source_eid", &bundle_spec.source);
        print_eid("replyto_eid", &bundle_spec.replyto);
        print_eid("dest_eid", &bundle_spec.dest);
    }

    if (wait_for_report)
    {
        // create a new dtn registration to receive bundle status reports
        memset(&reginfo, 0, sizeof(reginfo));
        dtn_copy_eid(&reginfo.endpoint, &bundle_spec.replyto);
        reginfo.flags = DTN_REG_DROP;
        reginfo.regid = regid;
        reginfo.expiration = 60 * 60;
        if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
            fprintf(stderr, "error creating registration (id=%d): %d (%s)\n",
                    regid, ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }
        
        if (verbose) printf("dtn_register succeeded, regid 0x%x\n", regid);
    }
    
    // set the dtn options
    bundle_spec.expiration = expiration;
    bundle_spec.dopts      = delivery_options;
    bundle_spec.priority   = priority;

    // set extension block information
    unsigned int num_ext_blocks = ext_blocks.size() - ExtBlock::num_meta_blocks_;
    unsigned int num_meta_blocks = ExtBlock::num_meta_blocks_;

    if (num_ext_blocks > 0) {
        void* buf = malloc(num_ext_blocks * sizeof(dtn_extension_block_t));
        memset(buf, 0, num_ext_blocks * sizeof(dtn_extension_block_t));

        dtn_extension_block_t * bp = (dtn_extension_block_t *)buf;
        for (unsigned int i = 0; i < ext_blocks.size(); ++i) {
            if (ext_blocks[i].metadata()) {
                continue;
            }

            bp->type          = ext_blocks[i].block().type;
            bp->flags         = ext_blocks[i].block().flags;
            bp->data.data_len = ext_blocks[i].block().data.data_len;
            bp->data.data_val = ext_blocks[i].block().data.data_val;
            bp++;
        }

        bundle_spec.blocks.blocks_len = num_ext_blocks;
        bundle_spec.blocks.blocks_val = (dtn_extension_block_t *)buf;
    }

    if (num_meta_blocks > 0) {
        void* buf = malloc(num_meta_blocks * sizeof(dtn_extension_block_t));
        memset(buf, 0, num_ext_blocks * sizeof(dtn_extension_block_t));

        dtn_extension_block_t * bp = (dtn_extension_block_t *)buf;
        for (unsigned int i = 0; i < ext_blocks.size(); ++i) {
            if (!ext_blocks[i].metadata()) {
                continue;
            }

            bp->type          = ext_blocks[i].block().type;
            bp->flags         = ext_blocks[i].block().flags;
            bp->data.data_len = ext_blocks[i].block().data.data_len;
            bp->data.data_val = ext_blocks[i].block().data.data_val;
            bp++;
        }

        bundle_spec.metadata.metadata_len = num_meta_blocks;
        bundle_spec.metadata.metadata_val = (dtn_extension_block_t *)buf;
    }
    if (simulate_bpdriver) {
        char pilot_data[4];
        strncpy(pilot_data, ".Go", sizeof(pilot_data));
        dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, (char*) pilot_data, 3);
        memset(&bundle_id, 0, sizeof(bundle_id));
        if ((ret = dtn_send(handle, regid, &bundle_spec, &send_payload,
                            &bundle_id)) != 0)
        {
            fprintf(stderr, "error sending bpdriver simulated pilot bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        } else {
            fprintf(stdout, "simulated bpdriver pilot bundle sent\n");
        }
    }

    // loop, sending sends and getting replies.
    for (i = 0; i < copies; ++i) {
        gettimeofday(&start, NULL);

        fill_payload(&send_payload);
        
        memset(&bundle_id, 0, sizeof(bundle_id));
       
        ret = dtn_send(handle, regid, &bundle_spec, &send_payload,
                       &bundle_id);

        bool output_msg = true;
        while (ret != DTN_SUCCESS) {
            if (dtn_errno(handle) == DTN_ENOSPACE) {
                if (output_msg) {
                    output_msg = false;
                    fprintf(stderr, "unable to send bundle due to storage depletion - retrying every 30 seconds...\n");
                }
                int ctr = 0;
                while (ctr++ < 300) {
                    usleep(100000);
                }

            } else {
                fprintf(stderr, "error sending bundle: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
                exit(1);
            }

            ret = dtn_send(handle, regid, &bundle_spec, &send_payload,
                            &bundle_id);
        }

        if (verbose) fprintf(stdout, "bundle sent successfully: id %s,%" PRIu64 ".%" PRIu64 "\n",
                             bundle_id.source.uri,
                             bundle_id.creation_ts.secs_or_millisecs,
                             bundle_id.creation_ts.seqno);

        if (wait_for_report)
        {
            memset(&reply_spec, 0, sizeof(reply_spec));
            memset(&reply_payload, 0, sizeof(reply_payload));
            
            // now we block waiting for any replies
            if ((ret = dtn_recv(handle, &reply_spec,
                                DTN_PAYLOAD_MEM, &reply_payload,
                                DTN_TIMEOUT_INF)) < 0)
            {
                fprintf(stderr, "error getting reply: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
                exit(1);
            }
            gettimeofday(&end, NULL);

            printf("got %d byte report from [%s]: time=%.1f ms\n",
                   reply_payload.buf.buf_len,
                   reply_spec.source.uri,
                   ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                    (double)(end.tv_usec - start.tv_usec)/1000.0));

            if (reply_payload.status_report != NULL) {
                if (reply_spec.bp_version == 6) {
                    log_bpv6_status_report(reply_payload.status_report);
                } else {
                    log_bpv7_status_report(reply_payload.status_report);
                }
            }
        }

        if (sleep_time != 0) {
            usleep(sleep_time * 1000);
        }
    }

    if (wait_for_report)
    {
        printf("Waiting for status reports... press <ctrl-c> to exit\n");

        while (user_abort != 1) {
            memset(&reply_spec, 0, sizeof(reply_spec));
            memset(&reply_payload, 0, sizeof(reply_payload));
            
            int timeout = 1000;
        
            if ((ret = dtn_recv(handle, &reply_spec,
                                DTN_PAYLOAD_MEM, &reply_payload, timeout)) < 0)
            {
                if (dtn_errno(handle) == DTN_ETIMEOUT) {
                    continue;
                }
            
                fprintf(stderr, "error getting reply: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
                exit(1);
            }

            gettimeofday(&end, NULL);

            printf("got %d byte report from [%s]: time=%.1f ms\n",
                   reply_payload.buf.buf_len,
                   reply_spec.source.uri,
                   ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                    (double)(end.tv_usec - start.tv_usec)/1000.0));

            if (reply_payload.status_report != NULL) {
                if (reply_spec.bp_version == 6) {
                    log_bpv6_status_report(reply_payload.status_report);
                } else {
                    log_bpv7_status_report(reply_payload.status_report);
                }
            }
        }
    }

    dtn_close(handle);
 
    if (num_ext_blocks > 0) {
        assert(bundle_spec.blocks.blocks_val != NULL);
        free(bundle_spec.blocks.blocks_val);
        bundle_spec.blocks.blocks_val = NULL;
        bundle_spec.blocks.blocks_len = 0;
    }

    if (num_meta_blocks > 0) {
        assert(bundle_spec.metadata.metadata_val != NULL);
        free(bundle_spec.metadata.metadata_val);
        bundle_spec.metadata.metadata_val = NULL;
        bundle_spec.metadata.metadata_len = 0;
    }

    return 0;
}

void print_usage()
{
    fprintf(stderr, "usage: %s [opts] "
            "-s <source_eid> -d <dest_eid> -t <type> -p <payload>\n",
            progname);
    fprintf(stderr, "options:\n");   
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -h help\n");
    fprintf(stderr, " -A daemon api IP address\n");
    fprintf(stderr, " -B daemon api IP port\n");
    fprintf(stderr, " -s <eid|demux_string> source eid)\n");
    fprintf(stderr, " -d <eid|demux_string> destination eid)\n");
    fprintf(stderr, " -r <eid|demux_string> reply to eid)\n");
    fprintf(stderr, " -t <f|t|m|d> payload type: file, tmpfile, message, or date\n");
    fprintf(stderr, " -p <filename|string> payload data\n");
    fprintf(stderr, " -e <time> expiration time in seconds (default: one hour)\n");
    fprintf(stderr, " -P <priority> one of bulk, normal, expedited, or reserved\n");
    fprintf(stderr, " -i <regid> registration id for reply to\n");
    fprintf(stderr, " -n <int> copies of the bundle to send\n");
    fprintf(stderr, " -z <time> msecs to sleep between transmissions\n");
    fprintf(stderr, " -c request custody transfer\n");
    fprintf(stderr, " -C request custody transfer receipts\n");
    fprintf(stderr, " -D request for end-to-end delivery receipt\n");
    fprintf(stderr, " -X request for deletion receipt\n");
    fprintf(stderr, " -R request for bundle reception receipts\n");
    fprintf(stderr, " -F request for bundle forwarding receipts\n");
    fprintf(stderr, " -1 assert destination endpoint is a singleton\n");
    fprintf(stderr, " -N assert destination endpoint is not a singleton\n");
    fprintf(stderr, " -W set the do not fragment option\n");
    fprintf(stderr, " -w wait for bundle status reports\n");
    fprintf(stderr, " -E <int> include extension block and specify type\n");
    fprintf(stderr, " -M <int> include metadata block and specify type\n");
    fprintf(stderr, " -O <int> flags to include in extension/metadata block\n");
    fprintf(stderr, " -S <string> extension/metadata block content\n");
    fprintf(stderr, " -V <int> Bundle Protocol Version (6, 7, or 0 = DTNME default)\n");
	fprintf(stderr, " -Y simulate bpdriver sending a 3 byte pilot bundle\n");
    exit(1);
}

void parse_options(int argc, char**argv)
{
    int c, done = 0;
    char arg_type = 0;

    progname = argv[0];

    while (!done)
    {
        c = getopt(argc, argv, "A:B:vhHr:s:d:e:P:n:woDXFRcC1NWt:p:i:z:E:M:O:S:V:Y");
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
            print_usage();
            exit(0);
            return;
        case 'r':
            arg_replyto = optarg;
            break;
        case 's':
            arg_source = optarg;
            break;
        case 'd':
            arg_dest = optarg;
            break;
        case 'e':
            expiration = atoi(optarg);
            break;
        case 'P':
            if (!strcasecmp(optarg, "bulk"))   {
                priority = COS_BULK;
            } else if (!strcasecmp(optarg, "normal")) {
                priority = COS_NORMAL;
            } else if (!strcasecmp(optarg, "expedited")) {
                priority = COS_EXPEDITED;
            } else if (!strcasecmp(optarg, "reserved")) {
                priority = COS_RESERVED;
            } else {
                fprintf(stderr, "invalid priority value %s\n", optarg);
                exit(1);
            }
            break;
        case 'n':
            copies = atoi(optarg);
            break;
        case 'w':
            wait_for_report = 1;
            break;
        case 'D':
            delivery_options |= DOPTS_DELIVERY_RCPT;
            break;
        case 'X':
            delivery_options |= DOPTS_DELETE_RCPT;
            break;
        case 'F':
            delivery_options |= DOPTS_FORWARD_RCPT;
            break;
        case 'R':
            delivery_options |= DOPTS_RECEIVE_RCPT;
            break;
        case 'c':
            delivery_options |= DOPTS_CUSTODY;
            break;
        case 'C':
            delivery_options |= DOPTS_CUSTODY_RCPT;
            break;
        case '1':
            delivery_options |= DOPTS_SINGLETON_DEST;
            break;
        case 'N':
            delivery_options |= DOPTS_MULTINODE_DEST;
            break;
        case 'W':
            delivery_options |= DOPTS_DO_NOT_FRAGMENT;
            break;
        case 't':
            arg_type = optarg[0];
            break;
        case 'p':
            data_source = strdup(optarg);
            break;
        case 'i':
            regid = atoi(optarg);
            break;
        case 'z':
            sleep_time = atoi(optarg);
            break;
        case 'E':
            ext_blocks.push_back(ExtBlock(atoi(optarg)));
            break;
        case 'M':
            ext_blocks.push_back(ExtBlock(atoi(optarg)));
            ext_blocks.back().set_metadata();
            ExtBlock::num_meta_blocks_++;
            break;
        case 'O':
            if (ext_blocks.size() > 0) {
                ext_blocks.back().block().flags = atoi(optarg);
            }
            break;
        case 'S':
            if (ext_blocks.size() > 0) {
                char * block_buf = strdup(optarg);
                ext_blocks.back().set_block_buf(block_buf, strlen(block_buf));
            }
            break;
        case 'V':
            bp_version = atoi(optarg);
            if (bp_version == 0) {
                printf("\nBundle Protocol Version specified as zero: default configured in the DTNME server will be used\n");
                fflush(stdout);
            } else if (bp_version == 6) {
                printf("\nBundle Protocol Version 6 specified by user\n");
                fflush(stdout);
            } else if (bp_version == 7) {
                printf("\nBundle Protocol Version 7 specified by user\n");
                fflush(stdout);
            } else {
                printf("\nInvalid Bundle Protocol Version. must be 6 or 7 or 0 to use default configured in the DTNME server\n");
                fflush(stdout);
                exit(1);
            }
            break;
			
        case 'Y':
            simulate_bpdriver = true;
            break;
			

        case -1:
            done = 1;
            break;
        default:
            // getopt already prints an error message for unknown
            // option characters
            print_usage();
            exit(1);
        }
    }

#define CHECK_SET(_arg, _what)                                          \
    if (_arg == 0) {                                                    \
        fprintf(stderr, "send_me: %s must be specified\n", _what);      \
        print_usage();                                                  \
        exit(1);                                                        \
    }
    
    CHECK_SET(arg_source,   "source eid");
    CHECK_SET(arg_dest,     "destination eid");
    CHECK_SET(arg_type,     "payload type");
    if (arg_type != 'd') {
        CHECK_SET(data_source,  "payload data");
    }

    switch (arg_type)
    {
    case 'f': payload_type = DTN_PAYLOAD_FILE; break;
    case 't': payload_type = DTN_PAYLOAD_TEMP_FILE; break;
    case 'm': payload_type = DTN_PAYLOAD_MEM; break;
    case 'd': 
        payload_type = DTN_PAYLOAD_MEM;
        data_source = date_buf;
        break;
    default:
        fprintf(stderr, "send_me: type argument '%d' invalid\n", arg_type);
        print_usage();
        exit(1);
    }

    // dtnd needs full pathnames.
    if ( arg_type == 'f' && data_source[0] != '/' )
    {
        char fullpath[PATH_MAX];

        if ( getcwd(fullpath, PATH_MAX) == NULL )
        {
            perror("cwd");
            exit(1);
        }

        strncat(fullpath, "/", PATH_MAX - strlen(fullpath) - 1 );
        strncat(fullpath, data_source, PATH_MAX - strlen(fullpath) - 1);

        free(data_source);

        data_source = (char*)malloc(sizeof(char)*(strlen(fullpath) + 1));
        memcpy(data_source, fullpath, sizeof(char)*(strlen(fullpath) + 1));
    }
}

dtn_endpoint_id_t * parse_eid(dtn_handle_t handle, 
                              dtn_endpoint_id_t* eid, char * str)
{
    // try the string as an actual dtn eid
    if (!dtn_parse_eid_string(eid, str)) 
    {
        if (verbose) fprintf(stdout, "%s (literal)\n", str);
        return eid;
    }
    // build a local eid based on the configuration of our dtn
    // router plus the str as demux string
    else if (!dtn_build_local_eid(handle, eid, str))
    {
        if (verbose) fprintf(stdout, "%s (local)\n", str);
        return eid;
    }
    else
    {
        fprintf(stderr, "invalid eid string '%s'\n", str);
        exit(1);
    }
}

void print_eid(const char *  label, dtn_endpoint_id_t * eid)
{
    printf("%s [%s]\n", label, eid->uri);
}

void fill_payload(dtn_bundle_payload_t* payload)
{
    memset(payload, 0, sizeof(*payload));
    
    if (data_source == date_buf) {
        time_t current = time(NULL);
        strcpy(date_buf, ctime(&current));
    }

    // if we're sending multiple copies of the file using hard links,
    // the daemon will remove the file once we send it, so we need to
    // make a temp link for the daemon itself to use
    if (copies != 1 && payload_type == DTN_PAYLOAD_TEMP_FILE) {
        char tmp[PATH_MAX];
        snprintf(tmp, PATH_MAX, "%s.tmplink", data_source);

        if (link(data_source, tmp) != 0) {
            fprintf(stderr, "error creating hard link %s -> %s: %s",
                    data_source, tmp, strerror(errno));
            exit(1);
        }
        
        dtn_set_payload(payload, payload_type, tmp, strlen(tmp));
    } else {
        dtn_set_payload(payload, payload_type, data_source, strlen(data_source));
    }
}

const char* reason_to_str(dtn_status_report_reason_t reason)
{
    switch (reason) {
	    case REASON_NO_ADDTL_INFO: return "No additional info";
	    case REASON_LIFETIME_EXPIRED: return "Lifetime expired";
	    case REASON_FORWARDED_UNIDIR_LINK: return "Forwarded";
	    case REASON_TRANSMISSION_CANCELLED: return "Transmission cancelled";
	    case REASON_DEPLETED_STORAGE: return "Depleted storage";
	    case REASON_ENDPOINT_ID_UNINTELLIGIBLE: return "Endpoint ID unintelligible";
	    case REASON_NO_ROUTE_TO_DEST: return "No route to destination";
	    case REASON_NO_TIMELY_CONTACT: return "No timely contact";
	    case REASON_BLOCK_UNINTELLIGIBLE: return "Block unintelligible";
        default: return "Unrecognized reason code";
    }
};

void log_bpv6_event(const char* event, dtn_timestamp_t& timestamp, int& first_event)
{
    // timestamp is in seconds
    const char *fmtstr="%Y-%m-%d %H:%M:%S";
    struct tm tmval_buf;
    struct tm *tmval;
    char date_str[64];


    if (first_event) {
        fprintf(stdout, "        ");
        first_event = false;
    }

    time_t tmp_secs = timestamp.secs_or_millisecs + TIMEVAL_CONVERSION_SECS;
    tmval = gmtime_r(&tmp_secs, &tmval_buf);
    strftime(date_str, 64, fmtstr, tmval);
    fprintf(stdout, "%s: %s  ", event, date_str);
}

void log_bpv6_status_report(dtn_bundle_status_report_t* rpt)
{
    int first_event = 1;

    // output the original bundle info and the reason associated with the event(s)
    // and then output details on the event(s)
    fprintf(stdout, "    Bundle ID: %s,%" PRIu64 ".%" PRIu64 "  Flags: %u  Reason: %s\n",
                         rpt->bundle_id.source.uri,
                         rpt->bundle_id.creation_ts.secs_or_millisecs,
                         rpt->bundle_id.creation_ts.seqno,
                         uint32_t(rpt->flags),
                         reason_to_str(rpt->reason));

    if (rpt->flags & STATUS_RECEIVED) {
        log_bpv6_event("RECEIVED", rpt->receipt_ts, first_event);
    }
    if (rpt->flags & STATUS_CUSTODY_ACCEPTED) {
        log_bpv6_event("CUSTODY ACCEPTED", rpt->custody_ts, first_event);
    }
    if (rpt->flags & STATUS_FORWARDED) {
        log_bpv6_event("FORWARDED", rpt->forwarding_ts, first_event);
    }
    if (rpt->flags & STATUS_DELIVERED) {
        log_bpv6_event("DELIVERED", rpt->delivery_ts, first_event);
    }
    if (rpt->flags & STATUS_DELETED) {
        log_bpv6_event("DELETED", rpt->deletion_ts, first_event);
    }
    if (rpt->flags & STATUS_ACKED_BY_APP) {
        log_bpv6_event("APP ACKED", rpt->ack_by_app_ts, first_event);
    }
    if (first_event != 1) {
        fprintf(stdout, "\n");
    }
}

void log_bpv7_event(const char* event, dtn_timestamp_t& timestamp, int& first_event)
{
    // timestamp is in milliseconds

    const char *fmtstr="%Y-%m-%d %H:%M:%S";
    struct tm tmval_buf;
    struct tm *tmval;
    char date_str[64];

    if (first_event) {
        fprintf(stdout, "        ");
        first_event = false;
    }

    time_t tmp_millis = timestamp.secs_or_millisecs % 1000;
    time_t tmp_secs = (timestamp.secs_or_millisecs / 1000) + TIMEVAL_CONVERSION_SECS;
    tmval = gmtime_r(&tmp_secs, &tmval_buf);
    strftime(date_str, 64, fmtstr, tmval);
    fprintf(stdout, "%s: %s.%3.3zu  ", event, date_str, tmp_millis);
}

void log_bpv7_status_report(dtn_bundle_status_report_t* rpt)
{
    int first_event = 1;

    // output the original bundle info and the reason associated with the event(s)
    // and then output details on the event(s)
    fprintf(stdout, "    Bundle ID: %s,%" PRIu64 ".%" PRIu64 "  Flags: %u  Reason: %s\n",
                         rpt->bundle_id.source.uri,
                         rpt->bundle_id.creation_ts.secs_or_millisecs,
                         rpt->bundle_id.creation_ts.seqno,
                         uint32_t(rpt->flags),
                         reason_to_str(rpt->reason));

    if (rpt->flags & STATUS_RECEIVED) {
        log_bpv7_event("RECEIVED", rpt->receipt_ts, first_event);
    }
    if (rpt->flags & STATUS_CUSTODY_ACCEPTED) {
        log_bpv7_event("CUSTODY ACCEPTED", rpt->custody_ts, first_event);
    }
    if (rpt->flags & STATUS_FORWARDED) {
        log_bpv7_event("FORWARDED", rpt->forwarding_ts, first_event);
    }
    if (rpt->flags & STATUS_DELIVERED) {
        log_bpv7_event("DELIVERED", rpt->delivery_ts, first_event);
    }
    if (rpt->flags & STATUS_DELETED) {
        log_bpv7_event("DELETED", rpt->deletion_ts, first_event);
    }
    if (rpt->flags & STATUS_ACKED_BY_APP) {
        log_bpv7_event("APP ACKED", rpt->ack_by_app_ts, first_event);
    }
    if (first_event != 1) {
        fprintf(stdout, "\n");
    }
}


