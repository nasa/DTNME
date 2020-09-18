/*
 *    Copyright 2010 - 2012 Trinity College Dublin
 *    Copyright 2012 Folly Consulting Ltd
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
#include "dtn_api.h"
#include "sdnv-c.h"

#define BUFSIZE 16
#define BLOCKSIZE 8192
#define COUNTER_MAX_DIGITS 9

// todo: move these out to a header file
#define DTN_BPQ_LITERAL 1
#define DTN_BPQ_BASE64 2
#define DTN_BPQ_FILE 3

// Find the maximum commandline length
#ifdef __FreeBSD__
/* Needed for PATH_MAX, Linux doesn't need it */
#include <sys/syslimits.h>
#endif

#ifndef PATH_MAX
/* A conservative fallback */
#define PATH_MAX 1024
#endif

//global variables
const char* progname;
int api_IP_set = 0;
char  api_IP[40] = "127.0.0.1";
short api_port = 5010;


/*******************************************************************************
* usage:
* display cmd line options to user.
*******************************************************************************/
int
usage()
{
    fprintf(stderr, "usage: %s -s < src endpoint > -d < dest endpoint >\n"
					"                -p <payload file> -n <name for payload file> "
                    "[opts]\n", progname);
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -r  < eid > reply to endpoint\n");
    fprintf(stderr, " -p  < filename > payload filename\n");
    fprintf(stderr, " -n  < cache name > typically ni: name for payload\n");
    fprintf(stderr, " -o  < seconds > report receiver timeout\n");
    fprintf(stderr, " -e  < seconds > bundle expiry time\n");
    fprintf(stderr, " -a  <number of reports expected to wait for>\n");
    fprintf(stderr, " -P  < bulk | normal | expedited | reserved > priority\n");
    fprintf(stderr, " -D  request end-to-end delivery receipt\n");
    fprintf(stderr, " -X  request deletion receipt\n");
    fprintf(stderr, " -A  api_IP optional (default is localhost)\n");
    fprintf(stderr, " -B  api_PORT optional (default is 5010)\n");
    fprintf(stderr, " -F  request bundle forwarding receipts\n");
    fprintf(stderr, " -R  request bundle reception receipts\n");
    fprintf(stderr, " -c  request custody transfer\n");
    fprintf(stderr, " -C  request custody transfer receipts\n");
    fprintf(stderr, " -1  assert destination endpoint is a singleton\n");
    fprintf(stderr, " -N  assert destination endpoint is not a singleton\n");
    fprintf(stderr, " -W  set the do not fragment option\n");
    fprintf(stderr, " -w  wait for bundle status reports\n");
    fprintf(stderr, " -h  help\n");
    fprintf(stderr, " -v  verbose\n");

    return DTN_SUCCESS;
}

/*******************************************************************************
* parse options:
* set internal variables based on cmd line args.
* calls parse matching file if required.
* returns success or exits on failure.
*******************************************************************************/
int
parse_options(int argc, char** argv,
    char * src_eid_name,                // s
    char * dest_eid_name,               // d
    char * reply_eid_name,              // r
    char * payload_file,                // p
    char * cache_name,                  // n
    dtn_timeval_t * timeout,            // o
    dtn_timeval_t * bundle_expiry,      // e
    int * num_reports,                  // a
    dtn_bundle_priority_t * priority,   // P
    int * delivery_options,             // D X F R c C 1 N W
    int * wait_for_report,              // w
    int * verbose)                      // v
{
    int c, done = 0;

    progname = argv[0];

    //initialize strings
    memset(src_eid_name,    0, sizeof(char) * PATH_MAX);
    memset(dest_eid_name,   0, sizeof(char) * PATH_MAX);
    memset(reply_eid_name,  0, sizeof(char) * PATH_MAX);
    memset(payload_file,    0, sizeof(char) * PATH_MAX);
    memset(cache_name,      0, sizeof(char) * PATH_MAX);

    while( !done )
    {
        c = getopt(argc, argv, "A:B:s:d:r:p:n:o:e:a:P:DXFRcC1NWwhHv");
        switch(c)
        {
        case 'A':
            api_IP_set = 1;
            strcpy(api_IP,optarg);
            break;
        case 'B':
            api_IP_set = 1;
            api_port = (short) atoi(optarg);
            break;
        case 's':            
            strncpy(src_eid_name, optarg, PATH_MAX);
            break;
        case 'd':    
            strncpy(dest_eid_name, optarg, PATH_MAX);
            break;
        case 'r':
            strncpy(reply_eid_name, optarg, PATH_MAX);
            break;
        case 'p':
            strncpy(payload_file, optarg, PATH_MAX);
            break;
        case 'n':
            strncpy(cache_name, optarg, PATH_MAX);
            break;
        case 'o':
            *timeout = atoi(optarg);
            break;
        case 'e':
            *bundle_expiry = atoi(optarg);  
            break;
        case 'a':
            *num_reports = atoi(optarg);
            break;
         case 'P':
            if (!strcasecmp(optarg, "bulk"))   {
                *priority = COS_BULK;
            } else if (!strcasecmp(optarg, "normal")) {
                *priority = COS_NORMAL;
            } else if (!strcasecmp(optarg, "expedited")) {
                *priority = COS_EXPEDITED;
            } else if (!strcasecmp(optarg, "reserved")) {
                *priority = COS_RESERVED;
            } else {
                fprintf(stderr, "invalid priority value %s\n", optarg);
                usage();
                exit(1);
            }
            break;
        case 'D':
            *delivery_options |= DOPTS_DELIVERY_RCPT;
            break;
        case 'X':
            *delivery_options |= DOPTS_DELETE_RCPT;
            break;
        case 'F':
            *delivery_options |= DOPTS_FORWARD_RCPT;
            break;
        case 'R':
            *delivery_options |= DOPTS_RECEIVE_RCPT;
            break;
        case 'c':
            *delivery_options |= DOPTS_CUSTODY;
            break;
        case 'C':
            *delivery_options |= DOPTS_CUSTODY_RCPT;
            break;
        case '1':
            *delivery_options |= DOPTS_SINGLETON_DEST;
            break;
        case 'N':
            *delivery_options |= DOPTS_MULTINODE_DEST;
            break;
        case 'W':
            *delivery_options |= DOPTS_DO_NOT_FRAGMENT;
            break;
        case 'w':
        	*wait_for_report = 1;
        	break;
        case 'v':
            *verbose = 1;
            break;
        case 'h':
        case 'H':
            usage();

            exit(0);
        case -1:
            done = 1;
            break;
        default:
            // getopt already prints error message for unknown option characters
            usage();
            exit(1);
        }
        
    }

    // if no reply-to eid set, use the src eid
    if (*reply_eid_name == 0)
        strncpy(reply_eid_name, src_eid_name, PATH_MAX);
    
    return DTN_SUCCESS;
}

/*******************************************************************************
* validate options:
* as there are different requirements depending on the mode,
* the validation will differ accordingly.
* returns success or exits on failure
*******************************************************************************/
int
validate_options(const char * src_eid_name,
    const char * dest_eid_name,
    const char * payload_file,
    const char * cache_name,
    dtn_timeval_t timeout,
    dtn_timeval_t bundle_expiry,
    int num_reports)
{
#define REQUIRE(test, err_msg) \
    if(!(test)) { \
        fprintf(stderr, err_msg); \
        usage(); \
        exit(1); \
    }

	REQUIRE(strlen(src_eid_name) > 0, "-s <src eid> required\n");
	REQUIRE(strlen(dest_eid_name) > 0, "-d <dest eid> required\n");
	REQUIRE(strlen(payload_file) > 0, "-p <payload filename> required\n");
	REQUIRE(strlen(cache_name) > 0, "-n <cache name> required\n");
	REQUIRE(timeout > 0, "-o <timeout> must be a positive integer\n");
	REQUIRE(bundle_expiry > 0, "-e <expiry> must be a positive integer\n");
	REQUIRE(num_reports > 0, "-a <number of reports expected> must be a positive integer\n");

    return DTN_SUCCESS;
#undef REQUIRE
}

/*******************************************************************************
* register with dtn: only needed for receipt of reports -
* sending a bundle doesn't need a registration
* 
*******************************************************************************/
int
register_with_dtn(dtn_handle_t handle,
    dtn_endpoint_id_t * reply_eid,
    dtn_reg_id_t *regid_rep,
    int wait_for_report,
    int verbose)
{
    int ret;
    dtn_reg_info_t reginfo;

    *regid_rep = DTN_REGID_NONE;

    if (wait_for_report)
    {
        // create a new dtn registration to receive bundle status reports

        memset(&reginfo, 0, sizeof(reginfo));
        dtn_copy_eid(&reginfo.endpoint, reply_eid);
        reginfo.flags = DTN_REG_DROP;
        reginfo.regid = *regid_rep;
        reginfo.expiration = 60 * 60; /* 1 hour */
        if ((ret = dtn_register(handle, &reginfo, regid_rep)) != 0) {
            fprintf(stderr, "error creating registration (id=%d): %d (%s)\n",
                    *regid_rep, ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        if (verbose) printf("dtn_register for reports succeeded, regid 0x%x\n", *regid_rep);
    }


    return DTN_SUCCESS;
}

/*******************************************************************************
* parse eid:
*  
* code lifted from dtnsend
*******************************************************************************/
dtn_endpoint_id_t *
parse_eid(dtn_handle_t handle,
    dtn_endpoint_id_t * eid,
    const char * str,
    int verbose)
{
    // try the string as an actual dtn eid
    if (dtn_parse_eid_string(eid, str) == DTN_SUCCESS) {
        if (verbose) fprintf(stdout, "%s (literal)\n", str);
        return eid;
    }

    // build a local eid based on the configuration of our dtn
    // router plus the str as demux string
    else if (dtn_build_local_eid(handle, eid, str) == DTN_SUCCESS) {
        if (verbose) fprintf(stdout, "%s (local)\n", str);
        return eid;
    }
    else {
        fprintf(stderr, "invalid eid string '%s'\n", str);
        exit(1);
    }
}


/*******************************************************************************
* bpq to char array
* encode the following information:
*
*   BPQ-kind             		1-byte
*   Matching rule 				1-byte
*
*   Creation time-stamp	sec		SDNV
*   Creation time-stamp	seq		SDNV
*   Source EID length			SDNV
*   Source EID 					n-bytes
*
*   Query value length     		SDNV
*   Query value            		n-bytes
*
*   Number of fragments  		SDNV
*   Fragment offsets    		SDNV
*   Fragment lengths     		SDNV
*
* @return The number of bytes or -1 on error
*******************************************************************************/
int
bpq_to_char_array(const dtn_bpq_extension_block_data_t * bpq,
    char* buf,
    size_t buf_len,
    int verbose)
{
    int j=0, q_encoding_len, f_encoding_len, encoding_len;
    u_int i=0, k=0;
    u_char encoding[PATH_MAX];

    memset(buf, 0, buf_len);

    // BPQ-kind             1-byte
    if (i < buf_len)    buf[i++] = (char) bpq->kind;

    // matching rule type   1-byte
    if (i < buf_len)    buf[i++] = (char) bpq->matching_rule;

    // Timestamp secs (SDNV)
	if ( (q_encoding_len = sdnv_encode (bpq->original_id.creation_ts.secs,
										encoding, PATH_MAX)) == -1 ) {
		fprintf (stderr, "Error encoding creation timestamp secs\n");
		return -1;
	} else {
		for (j=0; i<buf_len && j<q_encoding_len; ++j)
	        buf[i++] = encoding[j];
	}

    // Timestamp seqno (SDNV)
	if ( (q_encoding_len = sdnv_encode (bpq->original_id.creation_ts.seqno,
										encoding, PATH_MAX)) == -1 ) {
		fprintf (stderr, "Error encoding creation timestamp seqno\n");
		return -1;
	} else {
		for (j=0; i<buf_len && j<q_encoding_len; ++j)
	        buf[i++] = encoding[j];
	}

    // Source EID len (SDNV)
	if ( (q_encoding_len = sdnv_encode (bpq->original_id.source_len,
										encoding, PATH_MAX)) == -1 ) {
		fprintf (stderr, "Error encoding source EID len\n");
		return -1;
	} else {
		for (j=0; i<buf_len && j<q_encoding_len; ++j)
	        buf[i++] = encoding[j];
	}

    // Source EID n-bytes
	 if ( (i + bpq->original_id.source_len) < buf_len ) {
		memcpy(&(buf[i]), bpq->original_id.source.uri, bpq->original_id.source_len);
		i += bpq->original_id.source_len;
	} else {
		fprintf (stderr, "Error encoding query value\n");
		return -1;
	}


    // Query length (SDNV)
    if ( (q_encoding_len = sdnv_encode (bpq->query.query_len,
    									encoding, PATH_MAX)) == -1 ) {
    	fprintf (stderr, "Error encoding query len\n");
        return -1;
    } else {
    	for (j=0; i<buf_len && j<q_encoding_len; ++j)
    		buf[i++] = encoding[j];
    }

    // Query value n-bytes
    if ( (i + bpq->query.query_len) < buf_len ) {
    	memcpy(&(buf[i]), bpq->query.query_val, bpq->query.query_len);
    	i += bpq->query.query_len;
    } else {
    	fprintf (stderr, "Error encoding query value\n");
    	return -1;
    }


    // number of fragments  SDNV
    if ( (f_encoding_len = sdnv_encode (bpq->fragments.num_frag_returned,
    									encoding, PATH_MAX)) == -1 ){
    	fprintf (stderr, "Error encoding number of fragments\n");
        return -1;
    } else {
    	for (j=0; i<buf_len && j<f_encoding_len; ++j)
    		buf[i++] = encoding[j];
    }

    for (k=0; k<bpq->fragments.num_frag_returned; ++k) {

        // fragment offsets     SDNV
        if ( (encoding_len = sdnv_encode (bpq->fragments.frag_offsets[k],
        								  encoding, PATH_MAX)) == -1 ) {
        	fprintf (stderr, "Error encoding fragment offset[%d]\n", k);
        	return -1;
        } else {
        	for (j=0; i<buf_len && j<encoding_len; ++j)
        		buf[i++] = encoding[j];
        }

        // fragment lengths     SDNV
        if ( (encoding_len = sdnv_encode (bpq->fragments.frag_lenghts[k],
        								  encoding, PATH_MAX)) == -1 ) {
        	fprintf (stderr, "Error encoding fragment length[%d]\n", k);
        	return -1;
        } else {
        	for (j=0; i<buf_len && j<encoding_len; ++j)
        		buf[i++] = encoding[j];
        }
    }

    if (verbose) {
        fprintf (stdout, "\nbpq_to_char_array (buf_len:%zu, i:%d):\n",buf_len,i);
        fprintf (stdout, "             kind: %d\n", (int) buf[0]);
        fprintf (stdout, "    matching rule: %d\n", (int) buf[1]);

        fprintf (stdout, "  creation ts sec: %d\n",
        		 (int) bpq->original_id.creation_ts.secs);
        fprintf (stdout, "  creation ts seq: %d\n",
        		 (int) bpq->original_id.creation_ts.seqno);
        fprintf (stdout, "   source eid len: %d\n",
        		 (int) bpq->original_id.source_len);
        fprintf (stdout, "       source eid: %s\n",
        		       bpq->original_id.source.uri);

        fprintf (stdout, "        query len: %d\n", bpq->query.query_len);
        fprintf (stdout, "   q_encoding_len: %d\n", q_encoding_len);
        fprintf (stdout, "        query val: %s\n", bpq->query.query_val);

        fprintf (stdout, "     fragment len: %d\n", bpq->fragments.num_frag_returned);
        fprintf (stdout, "   f_encoding_len: %d\n\n", f_encoding_len);
    }

    return i;
}

/*******************************************************************************
* char array to bpq
* decode the following information:
*
*   BPQ-kind             		1-byte
*   Matching rule 				1-byte
*
*   Creation time-stamp	sec		SDNV
*   Creation time-stamp	seq		SDNV
*   Source EID length			SDNV
*   Source EID 					n-bytes
*
*   Query value length     		SDNV
*   Query value            		n-bytes
*
*   Number of fragments  		SDNV
*   Fragment offsets    		SDNV
*   Fragment lengths     		SDNV
*
* @return The number of bytes or -1 on error
*******************************************************************************/
int
char_array_to_bpq(const u_char* buf,
    size_t buf_len,
    dtn_bpq_extension_block_data_t * bpq,
    int verbose)
{
    u_int i=0, j=0;
    u_int64_t uval64;
    int q_decoding_len, f_decoding_len, decoding_len;

    // BPQ-kind             1-byte
    if (i<buf_len) bpq->kind = (u_int) buf[i++];

    // matching rule type   1-byte
    if (i<buf_len) bpq->matching_rule = (u_int) buf[i++];



    // Creation time-stamp sec     SDNV
    if ( (q_decoding_len = sdnv_decode (&(buf[i]),
    									buf_len - i,
    									&(bpq->original_id.creation_ts.secs))) == -1 ) {
        fprintf (stderr, "Error decoding creation time-stamp sec\n");
        return -1;
    }
    i += q_decoding_len;

    // Creation time-stamp seq     SDNV
    if ( (q_decoding_len = sdnv_decode (&(buf[i]),
    									buf_len - i,
    									&(bpq->original_id.creation_ts.seqno))) == -1 ) {
        fprintf (stderr, "Error decoding creation time-stamp seq\n");
        return -1;
    }
    i += q_decoding_len;

    // Source EID length     SDNV
    q_decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
    if ( q_decoding_len == -1 ) {
        fprintf (stderr, "Error decoding source EID length\n");
        return -1;
    }
    bpq->original_id.source_len = (u_int) uval64;
    i += q_decoding_len;

    // Source EID            n-bytes
    if (i<buf_len && bpq->original_id.source_len <= DTN_MAX_ENDPOINT_ID) {
    	strncpy(bpq->original_id.source.uri, (char*)&(buf[i]), bpq->original_id.source_len);
    	i += bpq->original_id.source_len;
    } else {
    	fprintf (stderr, "Error copying source EID\n");
		return -1;
    }

    // BPQ-value-length     SDNV
    q_decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
    if ( q_decoding_len == -1 ) {
        fprintf (stderr, "Error decoding BPQ-value-length\n");
        return -1;
    }
    bpq->query.query_len = (u_int) uval64;
    i += q_decoding_len;

    // BPQ-value            n-bytes
    if (i<buf_len) bpq->query.query_val = (char*)&(buf[i]);
    	i += bpq->query.query_len;


    // number of fragments  SDNV
    f_decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
    if ( f_decoding_len == -1 ) {
        fprintf (stderr, "Error decoding number of fragments\n");
        return -1;
    }
    bpq->fragments.num_frag_returned = (u_int) uval64;
    i += f_decoding_len;

    for (j=0; i<buf_len && j<bpq->fragments.num_frag_returned; ++j) {

        // fragment offsets     SDNV
        decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
        if ( decoding_len == -1 ) {
            fprintf (stderr, "Error decoding fragment[%d] offset\n", j);
            return -1;
        }
        bpq->fragments.frag_offsets[j] = (u_int) uval64;
        i += decoding_len;

        // fragment lengths     SDNV
        decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
        if ( decoding_len == -1 ) {
            fprintf (stderr, "Error decoding fragment[%d] length\n", j);
            return -1;
        }
        bpq->fragments.frag_lenghts[j] = (u_int) uval64;
        i += decoding_len;
    }

    if (i != buf_len)
        return -1;

    if (verbose) {
        fprintf (stdout, "\nchar_array_to_bpq (buf_len:%zu, i:%d):\n",buf_len, i);
        fprintf (stdout, "             kind: %d\n", (int) buf[0]);
        fprintf (stdout, "    matching rule: %d\n", (int) buf[1]);

        fprintf (stdout, "  creation ts sec: %d\n",
        		 (int) bpq->original_id.creation_ts.secs);
        fprintf (stdout, "  creation ts seq: %d\n",
        		 (int) bpq->original_id.creation_ts.seqno);
        fprintf (stdout, "   source eid len: %d\n",
        		 (int) bpq->original_id.source_len);
        fprintf (stdout, "       source eid: %s\n",
        		 bpq->original_id.source.uri);

        fprintf (stdout, "        query len: %d\n", bpq->query.query_len);
        fprintf (stdout, "   q_decoding_len: %d\n", q_decoding_len);
        fprintf (stdout, "        query val: %s\n", bpq->query.query_val);

        fprintf (stdout, "     fragment len: %d\n", bpq->fragments.num_frag_returned);
        fprintf (stdout, "   f_decoding_len: %d\n\n", f_decoding_len);
    }

    return DTN_SUCCESS;
}

/*******************************************************************************
* send bpq:
* build a bundle with a BPQ extension block conatining the name to be associated
* with the file given as a payload and send it to the destination (which may be
* totally arbitrary - the intention is to publish the file and have it cached
* where it passes.
*******************************************************************************/
int
send_bpq(dtn_handle_t handle,
    const dtn_endpoint_id_t * src_eid,
    const dtn_endpoint_id_t * dest_eid,
    const dtn_endpoint_id_t * reply_eid,
    char * cache_name,
    char * payload_file,
    int bundle_expiry,
    dtn_bundle_priority_t  priority,
    int delivery_options,
    int verbose)
{
    int  ret = 0;
    char buf [PATH_MAX];
    size_t buf_len = 0;
    dtn_bundle_id_t                 bundle_id;
    dtn_bundle_spec_t               bundle_spec;
    dtn_extension_block_t           bpq_block;
    dtn_bpq_extension_block_data_t  bpq_block_data;
    dtn_bundle_payload_t            payload;

    memset(buf,             0, PATH_MAX);
    memset(&bundle_spec,    0, sizeof(dtn_bundle_spec_t));
    memset(&bpq_block,      0, sizeof(dtn_extension_block_t));
    memset(&bpq_block_data, 0, sizeof(dtn_bpq_extension_block_data_t));
    memset(&payload,        0, sizeof(dtn_bundle_payload_t));
    
    // set the bpq block data
    bpq_block_data.kind = BPQ_BLOCK_KIND_PUBLISH;
    bpq_block_data.matching_rule = BPQ_MATCHING_RULE_EXACT;

    bpq_block_data.original_id.creation_ts.secs = 0;
    bpq_block_data.original_id.creation_ts.seqno = 0;
    bpq_block_data.original_id.source = *src_eid;
    bpq_block_data.original_id.source_len = strlen(src_eid->uri);

    bpq_block_data.query.query_len = strlen(cache_name) + 1;     // include the null char at the end
    bpq_block_data.query.query_val = cache_name;
    bpq_block_data.fragments.num_frag_returned = 0;
    bpq_block_data.fragments.frag_offsets = NULL;
    bpq_block_data.fragments.frag_lenghts = NULL;

    buf_len = bpq_to_char_array(&bpq_block_data, buf, PATH_MAX, verbose);

    // set the bpq block
    bpq_block.type = DTN_BPQ_BLOCK_TYPE;
    bpq_block.flags = BLOCK_FLAG_REPLICATE;
    bpq_block.data.data_len = buf_len;
    bpq_block.data.data_val = buf;
    
    // set the payload (expected to be file path)
    dtn_set_payload(&payload, DTN_PAYLOAD_FILE, payload_file, strlen(payload_file));

    // set the bundle spec eids
    if (verbose) fprintf(stdout, "Source: %s\n", src_eid->uri);
    if (verbose) fprintf(stdout, "Destination: %s\n", dest_eid->uri);
    if (verbose) fprintf(stdout, "Reply-To: %s\n", reply_eid->uri);
    bundle_spec.source = *src_eid;
    bundle_spec.dest = *dest_eid;
    bundle_spec.replyto = *reply_eid;

    // set the bundle spec dtn options
    bundle_spec.expiration = bundle_expiry;
    bundle_spec.dopts = delivery_options;
    bundle_spec.priority = priority;

    // set the bundle extension
    bundle_spec.blocks.blocks_len = 1;
    bundle_spec.blocks.blocks_val = &bpq_block;

    // send the bundle, bpq extension and empty payload
    if (verbose) fprintf(stdout, "Sending bundle to: %s (options %x)\n",
					     dest_eid->uri, delivery_options);
    ret = dtn_send(handle, DTN_REGID_NONE, &bundle_spec, &payload, &bundle_id);
    if (ret != DTN_SUCCESS) {
        fprintf(stderr, "error sending bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
    } else if (verbose) {
        fprintf(stdout, "bundle sent successfully: id %s,%"PRIu64".%"PRIu64"\n",
                    bundle_id.source.uri,
                    bundle_id.creation_ts.secs,
                    bundle_id.creation_ts.seqno); 
    }
    return ret;   
}

/*******************************************************************************
* report_type_to_str:  convert status report type flag to string
*
*******************************************************************************/
char *
report_type_to_str(dtn_status_report_flags_t status_type)
{
	switch (status_type) {
		case STATUS_RECEIVED:
			return "bundle received";
			break;
		case STATUS_CUSTODY_ACCEPTED:
			return "custody accepted";
			break;
		case STATUS_FORWARDED:
			return "bundle forwarded";
			break;
		case STATUS_DELIVERED:
			return "bundle delivered";
			break;
		case STATUS_DELETED:
			return "bundle deleted";
			break;
		case STATUS_ACKED_BY_APP:
			return "acknowledged by app";
			break;
		default:
			return "unrecgnized status report";
			break;
	}
}
/*******************************************************************************
* await_reports:  if request specified reports wanted and we ask to wait for
* them - loop waiting for reports to arrive up to timeout.
*******************************************************************************/
#define TIMEVAL_DIFF_MSEC(t1, t2) \
    ((unsigned long int)(((t1).tv_sec  - (t2).tv_sec)  * 1000) + \
     (((t1).tv_usec - (t2).tv_usec) / 1000))
int
await_reports(
	    dtn_handle_t handle,
		dtn_timeval_t timeout,
		int num_reports)
{
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t reply_payload;
    dtn_bundle_status_report_t* sr_data;
    struct timeval start, end;
    int ret = DTN_SUCCESS;

    memset(&reply_spec, 0, sizeof(reply_spec));
    memset(&reply_payload, 0, sizeof(reply_payload));

    gettimeofday(&start, NULL);
    // now we block waiting for any replies
    while (num_reports-- > 0) {
        if ((ret = dtn_recv(handle, &reply_spec,
                            DTN_PAYLOAD_MEM, &reply_payload,
                            timeout)) < 0)
        {
            fprintf(stderr, "error getting reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            return ret;
        }
        gettimeofday(&end, NULL);
        if ((sr_data = reply_payload.status_report) != NULL){
            printf("report of %s from [%s] (%s): time=%ld ms\n",
            	   report_type_to_str(sr_data->flags),
                   reply_spec.source.uri,
                   dtn_status_report_reason_to_str(sr_data->reason),
                   TIMEVAL_DIFF_MSEC(end, start));

        } else {
        	fprintf(stderr, "unexpected non-report bundle received: %d bytes from [%s]: time %.1f ms\n",
                    reply_payload.buf.buf_len,
                    reply_spec.source.uri,
                    ((double)(end.tv_sec - start.tv_sec) * 1000.0 +
                     (double)(end.tv_usec - start.tv_usec)/1000.0));
        }

    }

    return ret;
}

/*******************************************************************************
* main:
*******************************************************************************/
int
main(int argc, char** argv)
{
    dtn_endpoint_id_t reg_eid;
    dtn_endpoint_id_t src_eid;
    dtn_endpoint_id_t dest_eid;
    dtn_endpoint_id_t reply_eid;
    char reg_eid_name[PATH_MAX];
    char src_eid_name[PATH_MAX];
    char dest_eid_name[PATH_MAX];
    char reply_eid_name[PATH_MAX];
    char payload_file[PATH_MAX];
    char cache_name[PATH_MAX];
    dtn_timeval_t timeout = DTN_TIMEOUT_INF;    //forever
    dtn_timeval_t bundle_expiry = 3600;         //one hour
    int num_reports = 1;
    dtn_reg_id_t regid_rep = DTN_REGID_NONE;
    dtn_bundle_priority_t priority = COS_NORMAL; 
    int delivery_options = 0;
    int wait_for_report = 0;
    int verbose = 0;
    int err = 0;
    dtn_handle_t handle;

    memset( reg_eid_name,   0, sizeof(char) * PATH_MAX );
    memset( src_eid_name,   0, sizeof(char) * PATH_MAX );
    memset( dest_eid_name,  0, sizeof(char) * PATH_MAX );
    memset( reply_eid_name, 0, sizeof(char) * PATH_MAX );
    memset( payload_file,   0, sizeof(char) * PATH_MAX );
    memset( cache_name,     0, sizeof(char) * PATH_MAX );

    parse_options(argc, argv,
        src_eid_name,
        dest_eid_name,
        reply_eid_name,
        payload_file,
        cache_name,
        &timeout,
        &bundle_expiry,
        &num_reports,
        &priority,
        &delivery_options,
        &wait_for_report,
        &verbose);

	snprintf(&(reg_eid_name[0]), PATH_MAX, "%s", src_eid_name);

    validate_options(src_eid_name,
        dest_eid_name,
        payload_file,
        cache_name,
        timeout,
        bundle_expiry,
        num_reports);

    // open the ipc handle
    if (verbose) printf("opening connection to dtn router...\n");
    if (api_IP_set || api_port != 5010)
    {
        if((err = dtn_open_with_IP(api_IP,api_port, &handle)) != DTN_SUCCESS) 
        {
            fprintf(stderr, "fatal error opening dtn handle(%s:%d) : %s\n",
                    api_IP,api_port,dtn_strerror(err));
            exit(1);
        }

    } else if ((err = dtn_open(&handle)) != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }
    if (verbose) fprintf(stdout, "opened connection to dtn router...\n");

    // parse eids
    parse_eid(handle, &reg_eid, reg_eid_name, verbose);
    parse_eid(handle, &src_eid, src_eid_name, verbose);
    parse_eid(handle, &dest_eid, dest_eid_name, verbose);
    parse_eid(handle, &reply_eid, reply_eid_name, verbose);
    if (verbose) fprintf(stdout, "parsed reg_eid: %s\n", reg_eid.uri);
    if (verbose) fprintf(stdout, "parsed src_eid: %s\n", src_eid.uri);
    if (verbose) fprintf(stdout, "parsed dest_eid: %s\n", dest_eid.uri);
    if (verbose) fprintf(stdout, "parsed reply_eid: %s\n", reply_eid.uri);

    // get dtn registration
    if (verbose) fprintf(stdout, "registering with dtn...\n");
    register_with_dtn(handle,
        &reply_eid,
        &regid_rep,
        wait_for_report,
        verbose);
    if (verbose) {
    	if (wait_for_report) {
    		fprintf(stdout, "registered to receive reports with dtn, "
                             "regid: %d local eid: %s\n",
                             regid_rep, reply_eid.uri);
    	} else {
    		fprintf(stdout, "No registration required - not waiting for reports\n");
    	}
    }
    
    //get to work
#define TRY(fn, err_msg) \
    if (fn != DTN_SUCCESS) { \
        fprintf(stderr, err_msg); \
        dtn_close(handle); \
        exit(1); \
    }
    TRY( send_bpq(handle, &src_eid, &dest_eid, &reply_eid, cache_name,
                      payload_file, bundle_expiry, priority,
                      delivery_options, verbose), "error publishing file\n" );
    if (wait_for_report) {
    	TRY( await_reports(handle, timeout, num_reports),
    			"error receiving expected reports");
    }


#undef TRY

    // close the ipc handle
    if (verbose) fprintf(stdout, "closing connection to dtn router...\n");
    dtn_close(handle);
    if (verbose) fprintf(stdout, "closed connection to dtn router...\n");

    return DTN_SUCCESS;
}

